/*
 * Cursor Overlay — implementation.
 *
 * See cursor_overlay.h for the rationale.  The window properties are
 * set up to match what io-winmgr-gles2 on MHI2Q expects for a normal
 * RGBA8888 application surface: USAGE = NATIVE | WRITE (CPU-writable
 * compositor-accessible buffer, *not* a hardware overlay plane), with
 * TRANSPARENCY = SOURCE_OVER so our alpha pixels blend over the
 * underlying "media" class CarPlay window.  Z-order is pushed higher
 * than the CarPlay window group's default, which we joined via
 * screen_join_window_group so the compositor sees us as a sibling
 * rather than a random top-level.
 */

#include "cursor_overlay.h"
#include "../framework/logging.h"

#include <dlfcn.h>
#include <pthread.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <errno.h>

DEFINE_LOG_MODULE(OVRLY);

/* =========================================================
 * QNX Screen ABI subset — we resolve via dlsym(RTLD_NEXT) to
 * avoid linking libscreen.so directly (keeps the hook portable
 * across QNX SDP header variants).
 * ========================================================= */

/* Property IDs — verified against libairplay.so's CScreenRender::config
 * decompile (cluster / cursor reverse sessions).  All values match the
 * QNX 6.5 screen.h we ship against on this HU. */
#define PROP_ALPHA_MODE        1
#define PROP_BUFFER_SIZE       6
#define PROP_CLASS             7
#define PROP_CONTEXT           12
#define PROP_DISPLAY           14
#define PROP_GLOBAL_ALPHA      16
#define PROP_ID_STRING         20
#define PROP_RENDER_BUFFERS    28
#define PROP_POINTER           34
#define PROP_POSITION          35
#define PROP_SIZE              40
#define PROP_STRIDE            41
#define PROP_VISIBLE           45
#define PROP_USAGE             46
#define PROP_FORMAT            48
#define PROP_TRANSPARENCY      51
#define PROP_ZORDER            79

/* Formats (subset). */
#define FMT_RGBA8888           9

/* Transparency modes. */
#define TRANS_NONE             0
#define TRANS_SOURCE_OVER      4

/* Usage bit flags. */
#define USAGE_DISPLAY          0x00000001
#define USAGE_READ             0x00000002
#define USAGE_WRITE            0x00000004
#define USAGE_NATIVE           0x00000008
#define USAGE_OPENGL_ES2       0x00000020

typedef void* screen_context_t;
typedef void* screen_window_t;
typedef void* screen_buffer_t;

/* libscreen entry points we need. */
static struct {
    int (*create_context)(screen_context_t*, int);
    int (*destroy_context)(screen_context_t);
    int (*get_context_pv)(screen_context_t, int, void**);
    int (*get_context_iv)(screen_context_t, int, int*);
    int (*get_display_pv)(void*, int, void**);
    int (*get_display_iv)(void*, int, int*);
    int (*create_window)(screen_window_t*, screen_context_t);
    int (*destroy_window)(screen_window_t);
    int (*set_window_iv)(screen_window_t, int, const int*);
    int (*set_window_cv)(screen_window_t, int, int, const char*);
    int (*set_window_pv)(screen_window_t, int, void**);
    int (*get_window_iv)(screen_window_t, int, int*);
    int (*get_window_pv)(screen_window_t, int, void**);
    int (*create_window_buffers)(screen_window_t, int);
    int (*get_buffer_iv)(screen_buffer_t, int, int*);
    int (*get_buffer_pv)(screen_buffer_t, int, void**);
    int (*post_window)(screen_window_t, screen_buffer_t, int, const int*, int);
    int (*join_window_group)(screen_window_t, const char*);
    int (*flush_context)(screen_context_t, int);
    bool resolved;
} S;

/* Context flag for screen_create_context(SCREEN_APPLICATION_CONTEXT). */
#define SCREEN_APPLICATION_CONTEXT 0

static void resolve_screen(void) {
    if (S.resolved) return;
    S.create_context        = dlsym(RTLD_NEXT, "screen_create_context");
    S.destroy_context       = dlsym(RTLD_NEXT, "screen_destroy_context");
    S.get_context_pv        = dlsym(RTLD_NEXT, "screen_get_context_property_pv");
    S.get_context_iv        = dlsym(RTLD_NEXT, "screen_get_context_property_iv");
    S.get_display_pv        = dlsym(RTLD_NEXT, "screen_get_display_property_pv");
    S.get_display_iv        = dlsym(RTLD_NEXT, "screen_get_display_property_iv");
    S.create_window         = dlsym(RTLD_NEXT, "screen_create_window");
    S.destroy_window        = dlsym(RTLD_NEXT, "screen_destroy_window");
    S.set_window_iv         = dlsym(RTLD_NEXT, "screen_set_window_property_iv");
    S.set_window_cv         = dlsym(RTLD_NEXT, "screen_set_window_property_cv");
    S.set_window_pv         = dlsym(RTLD_NEXT, "screen_set_window_property_pv");
    S.get_window_iv         = dlsym(RTLD_NEXT, "screen_get_window_property_iv");
    S.get_window_pv         = dlsym(RTLD_NEXT, "screen_get_window_property_pv");
    S.create_window_buffers = dlsym(RTLD_NEXT, "screen_create_window_buffers");
    S.get_buffer_iv         = dlsym(RTLD_NEXT, "screen_get_buffer_property_iv");
    S.get_buffer_pv         = dlsym(RTLD_NEXT, "screen_get_buffer_property_pv");
    S.post_window           = dlsym(RTLD_NEXT, "screen_post_window");
    S.join_window_group     = dlsym(RTLD_NEXT, "screen_join_window_group");
    S.flush_context         = dlsym(RTLD_NEXT, "screen_flush_context");
    S.resolved = true;
    LOG_INFO(LOG_MODULE,
        "screen ABI resolved: create_ctx=%p create_win=%p set_iv=%p post=%p join=%p",
        (void*)S.create_context, (void*)S.create_window, (void*)S.set_window_iv,
        (void*)S.post_window, (void*)S.join_window_group);
}

/* PROP_DISPLAYS on context: 9 in QNX 6.5 screen.h. */
#define PROP_DISPLAYS           9
/* PROP_DISPLAY_COUNT on context: 38 in QNX 6.5 screen.h. */
#define PROP_DISPLAY_COUNT      38

/* Enumerate our context's displays and return the first one, or NULL.
 * The CarPlay window (from the dio_manager host context) lands on the
 * main LVDS display; our own context's display list is identical —
 * io-winmgr-gles2 exposes the same displays to every connected
 * context on this HU. */
static void* overlay_pick_first_display(screen_context_t ctx) {
    if (!S.get_context_iv || !S.get_context_pv) return NULL;
    int display_count = 0;
    int rc = S.get_context_iv(ctx, PROP_DISPLAY_COUNT, &display_count);
    LOG_INFO(LOG_MODULE, "ctx DISPLAY_COUNT rc=%d count=%d", rc, display_count);
    if (rc != 0 || display_count <= 0) return NULL;

    /* Cap to 4 — no HU we support has more than a handful. */
    if (display_count > 4) display_count = 4;
    void* displays[4] = {0};
    rc = S.get_context_pv(ctx, PROP_DISPLAYS, displays);
    LOG_INFO(LOG_MODULE, "ctx DISPLAYS rc=%d d0=%p d1=%p",
             rc, displays[0], displays[1]);
    if (rc != 0) return NULL;

    /* Log each display's size for operator insight. */
    if (S.get_display_iv) {
        for (int i = 0; i < display_count && displays[i]; i++) {
            int sz[2] = {0, 0};
            int srs = S.get_display_iv(displays[i], PROP_SIZE, sz);
            LOG_INFO(LOG_MODULE, "  display[%d]=%p size=%dx%d rc=%d",
                     i, displays[i], sz[0], sz[1], srs);
        }
    }
    return displays[0];
}

/* =========================================================
 * Overlay singleton state.  OMX callback serialises all our
 * entry points, but we keep a mutex anyway for destructor safety.
 * ========================================================= */
#define OVERLAY_WINDOW_GROUP    "dio_manager.airplay.qnx.screen_render"
#define OVERLAY_BUFFER_COUNT    1       /* prototype; upgrade to 2 once moving */

static struct {
    pthread_mutex_t  lock;
    bool             create_attempted;
    bool             created;
    screen_window_t  win;
    screen_buffer_t  buf;             /* single-buffered for test */
    void*            ptr;             /* CPU view of buf */
    int              stride;          /* bytes per row */
    int              w, h;
} O = { .lock = PTHREAD_MUTEX_INITIALIZER };

/* --- Creation (single-shot) ----------------------------------------- */

static int overlay_do_create(screen_window_t carplay_win,
                             screen_context_t parent_ctx,
                             int w, int h) {
    if (!S.create_window || !S.set_window_iv) {
        LOG_ERROR(LOG_MODULE, "screen ABI not available");
        return -1;
    }
    if (!carplay_win || !parent_ctx) {
        LOG_ERROR(LOG_MODULE, "missing carplay_win=%p parent_ctx=%p",
                  carplay_win, parent_ctx);
        return -1;
    }

    /* Use dio_manager's existing context — the one libairplay allocates
     * its CarPlay video window in.  Creating our own SCREEN_APPLICATION_
     * CONTEXT came up with zero displays and ENOTTY at buffer-alloc. */
    screen_context_t ctx = parent_ctx;
    LOG_INFO(LOG_MODULE, "using parent ctx=%p", ctx);

    screen_window_t win = NULL;
    int rc = S.create_window(&win, ctx);
    if (rc != 0 || !win) {
        LOG_ERROR(LOG_MODULE, "screen_create_window rc=%d win=%p", rc, win);
        return -1;
    }

    /* Set properties in the order create_window_buffers cares about,
     * and log each rc so we can pinpoint which property the compositor
     * rejects.  On QNX 6.5 io-winmgr, CLASS must be set before buffers
     * are created; USAGE + FORMAT + (SIZE or BUFFER_SIZE) determine
     * whether the allocator will grant the backing store.
     *
     * We intentionally skip CLASS: the default class accepts plain
     * CPU-written RGBA surfaces with USAGE_NATIVE|USAGE_WRITE.  Setting
     * CLASS="graphics" on this build rejects non-EGL usage. */
    /* Log the parent context's displays for operator insight — when
     * we use dio_manager's real ctx these should be populated. */
    (void)overlay_pick_first_display(ctx);

    /* No CLASS set — defaults to the CPU-accessible raw buffer path
     * on io-winmgr-gles2, which is exactly what an RGBA overlay wants.
     * Setting CLASS="media" gave ENOTTY (HW overlay pipelines already
     * taken by CarPlay); CLASS="graphics" gave EINVAL (requires EGL
     * usage).  Default path uses GPU-composition, sampling our CPU
     * buffer as a texture at compose time. */
    int r;
    int format    = FMT_RGBA8888;
    r = S.set_window_iv(win, PROP_FORMAT, &format);
    LOG_INFO(LOG_MODULE, "set FORMAT=%d rc=%d", format, r);

    int usage     = USAGE_WRITE;
    r = S.set_window_iv(win, PROP_USAGE, &usage);
    LOG_INFO(LOG_MODULE, "set USAGE=0x%x rc=%d", usage, r);

    int transp    = TRANS_SOURCE_OVER;
    r = S.set_window_iv(win, PROP_TRANSPARENCY, &transp);
    LOG_INFO(LOG_MODULE, "set TRANSPARENCY=%d rc=%d", transp, r);

    int sz[2]     = { w, h };
    r = S.set_window_iv(win, PROP_SIZE, sz);
    LOG_INFO(LOG_MODULE, "set SIZE=%dx%d rc=%d", w, h, r);

    int pos[2]    = { 0, 0 };
    r = S.set_window_iv(win, PROP_POSITION, pos);
    LOG_INFO(LOG_MODULE, "set POSITION=%d,%d rc=%d", pos[0], pos[1], r);

    int zorder    = 100;
    r = S.set_window_iv(win, PROP_ZORDER, &zorder);
    LOG_INFO(LOG_MODULE, "set ZORDER=%d rc=%d", zorder, r);

    int ga        = 255;
    r = S.set_window_iv(win, PROP_GLOBAL_ALPHA, &ga);
    LOG_INFO(LOG_MODULE, "set GLOBAL_ALPHA=%d rc=%d", ga, r);

    int visible   = 1;
    r = S.set_window_iv(win, PROP_VISIBLE, &visible);
    LOG_INFO(LOG_MODULE, "set VISIBLE=%d rc=%d", visible, r);

    r = S.set_window_cv(win, PROP_ID_STRING, 7, "overlay");
    LOG_INFO(LOG_MODULE, "set ID_STRING rc=%d", r);

    /* Join CarPlay's window group — compositor sees us as a sibling
     * of the video window rather than a detached top-level. */
    if (S.join_window_group) {
        int jrc = S.join_window_group(win, OVERLAY_WINDOW_GROUP);
        LOG_INFO(LOG_MODULE, "join_window_group('%s') rc=%d",
                 OVERLAY_WINDOW_GROUP, jrc);
    }

    errno = 0;
    rc = S.create_window_buffers(win, OVERLAY_BUFFER_COUNT);
    if (rc != 0) {
        int e = errno;
        LOG_ERROR(LOG_MODULE, "create_window_buffers rc=%d errno=%d (%s)",
                  rc, e, strerror(e));
        S.destroy_window(win);
        return -1;
    }

    /* QNX returns buffers via PROP_RENDER_BUFFERS as a pointer array;
     * we only allocated one slot. */
    screen_buffer_t bufs[2] = { NULL, NULL };
    rc = S.get_window_pv(win, PROP_RENDER_BUFFERS, (void**)bufs);
    if (rc != 0 || !bufs[0]) {
        LOG_ERROR(LOG_MODULE, "get RENDER_BUFFERS rc=%d buf0=%p", rc, bufs[0]);
        S.destroy_window(win);
        return -1;
    }

    void* ptr = NULL;
    int   stride = 0;
    rc = S.get_buffer_pv(bufs[0], PROP_POINTER, &ptr);
    if (rc != 0 || !ptr) {
        LOG_ERROR(LOG_MODULE, "get buffer POINTER rc=%d ptr=%p", rc, ptr);
        S.destroy_window(win);
        return -1;
    }
    rc = S.get_buffer_iv(bufs[0], PROP_STRIDE, &stride);
    if (rc != 0 || stride <= 0) {
        LOG_ERROR(LOG_MODULE, "get buffer STRIDE rc=%d stride=%d", rc, stride);
        S.destroy_window(win);
        return -1;
    }

    O.win        = win;
    O.buf        = bufs[0];
    O.ptr        = ptr;
    O.stride     = stride;
    O.w          = w;
    O.h          = h;

    LOG_INFO(LOG_MODULE,
        "overlay created ctx=%p win=%p buf=%p %dx%d stride=%d ptr=%p zorder=%d",
        ctx, win, bufs[0], w, h, stride, ptr, zorder);
    return 0;
}

int cursor_overlay_try_create(cursor_overlay_window_handle carplay_win,
                              cursor_overlay_context_handle parent_ctx,
                              int w, int h) {
    resolve_screen();
    pthread_mutex_lock(&O.lock);
    if (O.create_attempted) {
        int done = O.created ? 0 : -1;
        pthread_mutex_unlock(&O.lock);
        return done;
    }
    O.create_attempted = true;
    int rc = overlay_do_create((screen_window_t)carplay_win,
                               (screen_context_t)parent_ctx, w, h);
    O.created = (rc == 0);
    bool just_created = O.created;
    pthread_mutex_unlock(&O.lock);

    /* Kick an initial paint outside the lock so the first visual
     * proof of the overlay is on screen without a second call. */
    if (just_created) cursor_overlay_render_test();
    return rc;
}

/* --- Rendering ------------------------------------------------------ */

/* RGBA8888 pixel packing.  On little-endian ARMv7 QNX 6.5 with stock
 * Adreno/MDP compositor the byte order in memory is R,G,B,A — so a
 * uint32 literal 0xAABBGGRR reads as R=0xRR, G=0xGG, B=0xBB, A=0xAA
 * on the GPU.  The test square is fully opaque red so transparency
 * mis-interpretation would still be obvious (blue square instead of
 * red). */
static inline uint32_t rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    return ((uint32_t)a << 24) | ((uint32_t)b << 16) | ((uint32_t)g << 8) | r;
}

void cursor_overlay_render_test(void) {
    pthread_mutex_lock(&O.lock);
    if (!O.created || !O.ptr || !S.post_window) {
        pthread_mutex_unlock(&O.lock);
        return;
    }

    /* Clear the whole back buffer to alpha=0 (fully transparent). */
    for (int y = 0; y < O.h; y++) {
        memset((uint8_t*)O.ptr + (size_t)y * (size_t)O.stride, 0,
               (size_t)O.w * 4);
    }

    /* Draw an opaque red 200x200 square at (100, 100). */
    const int sq_x = 100, sq_y = 100, sq_w = 200, sq_h = 200;
    const int ex = (sq_x + sq_w > O.w) ? O.w : (sq_x + sq_w);
    const int ey = (sq_y + sq_h > O.h) ? O.h : (sq_y + sq_h);
    const uint32_t red = rgba(0xFF, 0x00, 0x00, 0xFF);
    for (int y = sq_y; y < ey; y++) {
        uint32_t* row = (uint32_t*)((uint8_t*)O.ptr
                                    + (size_t)y * (size_t)O.stride);
        for (int x = sq_x; x < ex; x++) row[x] = red;
    }

    /* Post full-window dirty rect. */
    const int dirty[4] = { 0, 0, O.w, O.h };
    int rc = S.post_window(O.win, O.buf, 1, dirty, 0);
    LOG_INFO(LOG_MODULE,
        "render_test: posted red 200x200 @ 100,100 rc=%d", rc);

    pthread_mutex_unlock(&O.lock);
}

void cursor_overlay_draw_cursor(int cx, int cy, int alpha) {
    /* Not wired yet — prototype: render_test only.  Will replace
     * once the test pattern is visually confirmed over CarPlay. */
    (void)cx; (void)cy; (void)alpha;
}

void cursor_overlay_set_visible(int visible) {
    pthread_mutex_lock(&O.lock);
    if (O.created && S.set_window_iv) {
        int v = visible ? 1 : 0;
        S.set_window_iv(O.win, PROP_VISIBLE, &v);
    }
    pthread_mutex_unlock(&O.lock);
}

void cursor_overlay_destroy(void) {
    pthread_mutex_lock(&O.lock);
    if (O.win && S.destroy_window) {
        int rc = S.destroy_window(O.win);
        LOG_INFO(LOG_MODULE, "destroy_window rc=%d", rc);
    }
    /* Don't destroy parent ctx — it's dio_manager's real one, still
     * in use by CarPlay's video window. */
    O.win = NULL;
    O.buf = NULL;
    O.ptr = NULL;
    O.created = false;
    /* keep `create_attempted` true so we don't keep retrying. */
    pthread_mutex_unlock(&O.lock);
}

__attribute__((destructor))
static void cursor_overlay_fini(void) {
    cursor_overlay_destroy();
}
