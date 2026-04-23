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
    int (*create_window)(screen_window_t*, screen_context_t);
    int (*destroy_window)(screen_window_t);
    int (*set_window_iv)(screen_window_t, int, const int*);
    int (*set_window_cv)(screen_window_t, int, int, const char*);
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

static void resolve_screen(void) {
    if (S.resolved) return;
    S.create_window         = dlsym(RTLD_NEXT, "screen_create_window");
    S.destroy_window        = dlsym(RTLD_NEXT, "screen_destroy_window");
    S.set_window_iv         = dlsym(RTLD_NEXT, "screen_set_window_property_iv");
    S.set_window_cv         = dlsym(RTLD_NEXT, "screen_set_window_property_cv");
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
        "screen ABI resolved: create=%p set_iv=%p post=%p join=%p",
        (void*)S.create_window, (void*)S.set_window_iv,
        (void*)S.post_window, (void*)S.join_window_group);
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
    screen_context_t parent_ctx;
    screen_window_t  win;
    screen_buffer_t  buf;             /* single-buffered for test */
    void*            ptr;             /* CPU view of buf */
    int              stride;          /* bytes per row */
    int              w, h;
} O = { .lock = PTHREAD_MUTEX_INITIALIZER };

/* --- Creation (single-shot) ----------------------------------------- */

static int overlay_do_create(screen_window_t carplay_win, int w, int h) {
    if (!S.create_window || !S.set_window_iv) {
        LOG_ERROR(LOG_MODULE, "screen ABI not available");
        return -1;
    }
    if (!carplay_win) {
        LOG_ERROR(LOG_MODULE, "no CarPlay parent window tracked");
        return -1;
    }

    /* Get the CarPlay window's screen_context_t so our overlay lands
     * on the same compositor instance + display. */
    screen_context_t ctx = NULL;
    int rc = S.get_window_pv(carplay_win, PROP_CONTEXT, (void**)&ctx);
    if (rc != 0 || !ctx) {
        LOG_ERROR(LOG_MODULE, "get parent CONTEXT rc=%d ctx=%p", rc, ctx);
        return -1;
    }

    screen_window_t win = NULL;
    rc = S.create_window(&win, ctx);
    if (rc != 0 || !win) {
        LOG_ERROR(LOG_MODULE, "screen_create_window rc=%d win=%p", rc, win);
        return -1;
    }

    /* "graphics" class so compositor treats this as a normal RGBA
     * window (not a YUV media surface). */
    S.set_window_cv(win, PROP_CLASS, 8, "graphics");
    S.set_window_cv(win, PROP_ID_STRING, 7, "overlay");

    int format    = FMT_RGBA8888;        S.set_window_iv(win, PROP_FORMAT,       &format);
    int transp    = TRANS_SOURCE_OVER;   S.set_window_iv(win, PROP_TRANSPARENCY, &transp);
    int usage     = USAGE_NATIVE | USAGE_WRITE;
                                         S.set_window_iv(win, PROP_USAGE,        &usage);
    int sz[2]     = { w, h };            S.set_window_iv(win, PROP_SIZE,         sz);
    int pos[2]    = { 0, 0 };            S.set_window_iv(win, PROP_POSITION,     pos);
    int zorder    = 100;                 S.set_window_iv(win, PROP_ZORDER,       &zorder);
    int ga        = 255;                 S.set_window_iv(win, PROP_GLOBAL_ALPHA, &ga);
    int visible   = 1;                   S.set_window_iv(win, PROP_VISIBLE,      &visible);

    /* Join CarPlay's window group — compositor sees us as a sibling
     * of the video window rather than a detached top-level. */
    if (S.join_window_group) {
        int jrc = S.join_window_group(win, OVERLAY_WINDOW_GROUP);
        LOG_INFO(LOG_MODULE, "join_window_group('%s') rc=%d",
                 OVERLAY_WINDOW_GROUP, jrc);
    }

    rc = S.create_window_buffers(win, OVERLAY_BUFFER_COUNT);
    if (rc != 0) {
        LOG_ERROR(LOG_MODULE, "create_window_buffers rc=%d", rc);
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

    O.parent_ctx = ctx;
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
                              int w, int h) {
    resolve_screen();
    pthread_mutex_lock(&O.lock);
    if (O.create_attempted) {
        int done = O.created ? 0 : -1;
        pthread_mutex_unlock(&O.lock);
        return done;
    }
    O.create_attempted = true;
    int rc = overlay_do_create((screen_window_t)carplay_win, w, h);
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
