/*
 * CarPlay Cursor Hook
 *
 * Overlays a 32×32 white crosshair on the CarPlay video stream, driven by
 * CMD_CURSOR_POS / CMD_CURSOR_HIDE commands from Java over the bus.
 *
 * Pipeline intercept:
 *
 *   iPhone H.264 → OMX HW decoder → onFillBufferDoneCallback()  ← hook here
 *                                          ↓ (sprite blitted in place on NV12)
 *                                 CScreenRender::render()
 *                                          ↓
 *                                 screen_post_window()             (untouched)
 *
 * Dimension discovery: we snoop screen_create_window_group to find the
 * CarPlay Screen window, then query SCREEN_PROPERTY_SIZE to learn the
 * decoded-frame resolution.  Once we know it, we publish EVT_SCREEN_INFO
 * (sticky) so Java clamps cursor coordinates correctly.
 *
 * Buffer layout: Qualcomm Adreno HW decoder on MHI2Q outputs NV12 in either
 * linear (width not a multiple of 64) or 64×32 tiles (width aligned to 64).
 * Tiles are grouped in 2×2 super-blocks with zigzag sub-tile order; layout
 * reverse-engineered in carplay_vc/carplay_vc_frame_hook.c.  We dispatch
 * blit_cursor_nv12_linear() vs blit_cursor_nv12_tiled() based on the
 * detected width, so the cursor sprite lands correctly on both 800×480
 * (linear) and 1280×480 / 1920×720 (tiled) style displays.  Set env
 * CP_CURSOR_LINEAR=1 to force linear for debug.
 */

#include "../framework/common.h"
#include "../framework/logging.h"
#include "../framework/bus.h"

#include <dlfcn.h>

DEFINE_LOG_MODULE(CURSOR);

/* ============================================================
 * OMX & Screen ABI -- minimal copies of the structs we touch
 * ============================================================ */

/* screen_window_t is an opaque pointer in QNX Screen API. */
typedef void* screen_window_t;

/* Window properties we query (magic numbers from QNX Screen headers). */
#define SCREEN_PROPERTY_SIZE  40

/* Minimal OMX buffer header (ARM 32-bit, matches OMX IL 1.1). */
typedef struct {
    uint32_t nSize;
    uint32_t nVersion;
    uint8_t* pBuffer;           /* decoded NV12, CPU pointer */
    uint32_t nAllocLen;
    uint32_t nFilledLen;
    uint32_t nOffset;
    void*    pAppPrivate;
    void*    pPlatformPrivate;
    void*    pInputPortPrivate;
    void*    pOutputPortPrivate;
} omx_buf_hdr_t;

/* Resolved originals (populated lazily via dlsym). */
static int  (*real_screen_create_window_group)(screen_window_t, const char*) = NULL;
static int  (*real_screen_get_window_property_iv)(screen_window_t, int, int*) = NULL;
static void (*real_onFillBufferDoneCallback)(void*, void*, omx_buf_hdr_t*) = NULL;

static pthread_once_t g_resolved_once = PTHREAD_ONCE_INIT;
static void resolve_originals(void) {
    real_screen_create_window_group =
        dlsym(RTLD_NEXT, "screen_create_window_group");
    real_screen_get_window_property_iv =
        dlsym(RTLD_NEXT, "screen_get_window_property_iv");
    real_onFillBufferDoneCallback = (void (*)(void*, void*, omx_buf_hdr_t*))dlsym(
        RTLD_NEXT,
        "_ZN3dio16COMXVideoDecoder24onFillBufferDoneCallbackEPvP20OMX_BUFFERHEADERTYPE");
}
static void ensure_resolved(void) {
    pthread_once(&g_resolved_once, resolve_originals);
}

/* ============================================================
 * Cursor state
 *
 * Writers: bus reader thread (on_cursor_pos / on_cursor_hide) and OMX
 *   callback thread (fade tick).
 * Readers: OMX callback thread (blit path).
 *
 * Plain pthread_mutex — QNX 6.5 GCC lacks C11 __atomic_*.  Critical
 * sections are tiny, no contention in practice.
 * ============================================================ */
#define FADE_DURATION_MS  300

static pthread_mutex_t g_cursor_lock = PTHREAD_MUTEX_INITIALIZER;
static bool            g_visible = false;
static int32_t         g_x = 0;
static int32_t         g_y = 0;
static int             g_alpha_request = 255;  /* from last CMD_CURSOR_POS */
static bool            g_fading = false;
static uint64_t        g_fade_start_ms = 0;

/* Dimension discovery state. */
static screen_window_t g_carplay_window = NULL;
static int             g_screen_w = 0;
static int             g_screen_h = 0;
static int             g_screen_published_w = 0;
static int             g_screen_published_h = 0;

/* Sprite: 128×128, per-pixel {Y_target, alpha}, computed once at module
 * load into g_sprite via 4×4 supersampled anti-aliasing.
 *
 * Shape (the "soft glowing ring"):
 *   - Inner hole r ≤ R_HOLE                         : transparent
 *   - Black gradient R_HOLE < r < R_BLACK           : black, alpha smoothstep
 *                                                     0 → 1 outward
 *   - White halo gradient R_BLACK ≤ r < R_WHITE     : white, alpha smoothstep
 *                                                     1 → 0 outward
 *   - Outside r ≥ R_WHITE                            : transparent
 *
 * Peak total density is at r = R_BLACK (black and white each hit alpha=1
 * from their respective sides at this boundary).  No sharp edges — all
 * transitions are smooth, so the result is a "glowing target reticle"
 * rather than a technical crosshair.
 *
 * Anti-aliasing: every output pixel is the alpha-weighted mean of a 4×4
 * grid of subpixel samples (16 samples).  Smooth edges, no staircase,
 * computed once in the constructor — zero per-frame cost.
 *
 * Chroma: both colours target U=V=128 (neutral grey-axis).  Edge pixels
 * with mixed coverage average Y linearly, giving intermediate grey
 * values — the UV blend toward 128 is the same regardless of which
 * colour zone we averaged from.
 *
 * All math is integer; no libm dependency.
 */
#define SPRITE_SIZE     128
#define SPRITE_CX       (SPRITE_SIZE / 2)
#define SPRITE_CY       (SPRITE_SIZE / 2)
#define SPRITE_SS       4        /* 4x4 = 16 subpixel samples per pixel */
#define SPRITE_SUB      8        /* fixed-point scale: 1 px = 8 sub-units */

/* Effective outer radius of the sprite — anything beyond this is
 * guaranteed alpha==0.  Must match R_WHITE in sprite_generate() so the
 * blit bounding box is tight. */
#define SPRITE_R_OUTER  48
#define SPRITE_BB_LO    (SPRITE_CX - SPRITE_R_OUTER)   /* inclusive */
#define SPRITE_BB_HI    (SPRITE_CX + SPRITE_R_OUTER)   /* inclusive */

#define Y_BLACK         16       /* BT.601 video-range limits */
#define Y_WHITE         235

typedef struct {
    uint8_t y_target;   /* 0 if alpha==0 */
    uint8_t alpha;      /* 0..255, 0 = transparent */
} sprite_px_t;

static sprite_px_t g_sprite[SPRITE_SIZE][SPRITE_SIZE];

/* Integer sqrt via Newton's method — converges in log2(n) iterations.
 * Only called from sprite_generate (~2000 calls at module load), so
 * simplicity trumps performance. */
static int isqrt_i(int n) {
    if (n <= 0) return 0;
    int r = n;
    int r1 = (r + n / r) / 2;
    while (r1 < r) {
        r = r1;
        if (r == 0) return 0;
        r1 = (r + n / r) / 2;
    }
    return r;
}

/* Integer smoothstep: input t ∈ [0..256] (fixed-point, 256 = 1.0),
 * output ∈ [0..256] following the S-curve 3t² − 2t³.
 * Maps t==0 → 0 and t==256 → 256 exactly. */
static int smoothstep_fx(int t) {
    if (t <= 0)   return 0;
    if (t >= 256) return 256;
    /* (3t² - 2t³)  normalized by 256².  Max intermediate: 256²*256 ≈ 16M, fits int32. */
    return (t * t * (3 * 256 - 2 * t)) / (256 * 256);
}

static void sprite_generate(void) {
    /* Tunables.  With 128-px canvas, keep R_WHITE ≤ 63.
     *   R_HOLE    radius of the transparent core (user sees UI through it)
     *   R_BLACK   peak-density radius (black alpha hits 1 here)
     *   R_WHITE   outer edge of halo (alpha returns to 0 here)
     * Ring "thickness" visually = R_BLACK - R_HOLE pixels of black gradient.
     * Halo "width" visually = R_WHITE - R_BLACK pixels of white gradient. */
    const int R_HOLE  = 20;
    const int R_BLACK = 32;
    const int R_WHITE = SPRITE_R_OUTER;   /* must match the blit bbox */

    /* Same radii in sub-pixel units for supersampled distance checks. */
    const int R_HOLE_SUB  = R_HOLE  * SPRITE_SUB;
    const int R_BLACK_SUB = R_BLACK * SPRITE_SUB;
    const int R_WHITE_SUB = R_WHITE * SPRITE_SUB;
    const int R_HOLE_SUB2  = R_HOLE_SUB  * R_HOLE_SUB;
    const int R_BLACK_SUB2 = R_BLACK_SUB * R_BLACK_SUB;
    const int R_WHITE_SUB2 = R_WHITE_SUB * R_WHITE_SUB;

    const int CX_SUB = SPRITE_CX * SPRITE_SUB;
    const int CY_SUB = SPRITE_CY * SPRITE_SUB;
    const int SUB_STEP = SPRITE_SUB / SPRITE_SS;  /* sub-units between samples */

    int x, y;
    for (y = 0; y < SPRITE_SIZE; y++) {
        for (x = 0; x < SPRITE_SIZE; x++) {
            int a_sum = 0;    /* sum of per-sample alpha (0..256 each), max 16*256 */
            int y_num = 0;    /* sum of y_target * alpha */
            int sj, si;
            for (sj = 0; sj < SPRITE_SS; sj++) {
                for (si = 0; si < SPRITE_SS; si++) {
                    /* Subpixel centre at (x + (si+0.5)/SS, y + (sj+0.5)/SS). */
                    int fx_sub = x * SPRITE_SUB + si * SUB_STEP + SUB_STEP / 2;
                    int fy_sub = y * SPRITE_SUB + sj * SUB_STEP + SUB_STEP / 2;
                    int dx = fx_sub - CX_SUB;
                    int dy = fy_sub - CY_SUB;
                    int d2 = dx * dx + dy * dy;

                    int sample_alpha = 0;
                    int sample_y = 0;

                    if (d2 <= R_HOLE_SUB2) {
                        continue;                 /* transparent sub-sample */
                    } else if (d2 < R_BLACK_SUB2) {
                        /* Black gradient: alpha 0→1 outward */
                        int r_sub = isqrt_i(d2);
                        int t = ((r_sub - R_HOLE_SUB) * 256) / (R_BLACK_SUB - R_HOLE_SUB);
                        sample_alpha = smoothstep_fx(t);
                        sample_y = Y_BLACK;
                    } else if (d2 < R_WHITE_SUB2) {
                        /* White halo: alpha 1→0 outward */
                        int r_sub = isqrt_i(d2);
                        int t = ((R_WHITE_SUB - r_sub) * 256) / (R_WHITE_SUB - R_BLACK_SUB);
                        sample_alpha = smoothstep_fx(t);
                        sample_y = Y_WHITE;
                    } else {
                        continue;
                    }
                    a_sum += sample_alpha;
                    y_num += sample_y * sample_alpha;
                }
            }

            if (a_sum <= 0) {
                g_sprite[y][x].y_target = 0;
                g_sprite[y][x].alpha    = 0;
                continue;
            }
            /* Pixel-level alpha = mean over 16 samples, scaled 0..255. */
            int alpha_px = (a_sum * 255) / (SPRITE_SS * SPRITE_SS * 256);
            if (alpha_px < 0)   alpha_px = 0;
            if (alpha_px > 255) alpha_px = 255;
            /* Colour = alpha-weighted mean Y of the contributing samples. */
            int y_px = y_num / a_sum;
            if (y_px < 0)   y_px = 0;
            if (y_px > 255) y_px = 255;

            g_sprite[y][x].y_target = (uint8_t)y_px;
            g_sprite[y][x].alpha    = (uint8_t)alpha_px;
        }
    }
}

/* ============================================================
 * Public: state snapshot + fade tick
 * ============================================================ */

/* Returns the effective alpha (0..255) after fade animation, and sets
 * *out_vis=false once alpha has drained fully. */
static int cursor_effective_alpha_locked(void) {
    if (!g_visible) return 0;
    int alpha = g_alpha_request;
    if (g_fading) {
        uint64_t now = get_timestamp_ms();
        uint64_t elapsed = (now >= g_fade_start_ms) ? (now - g_fade_start_ms) : 0;
        if (elapsed >= FADE_DURATION_MS) {
            /* Done fading */
            g_visible = false;
            g_fading = false;
            return 0;
        }
        /* Linear alpha interpolation from the request alpha to 0. */
        int scale = (int)(FADE_DURATION_MS - elapsed);
        alpha = (alpha * scale) / FADE_DURATION_MS;
        if (alpha < 0) alpha = 0;
    }
    return alpha;
}

/* Non-locking version for internal use. */
void cursor_state_snapshot(int* visible, int* x, int* y) {
    pthread_mutex_lock(&g_cursor_lock);
    int a = cursor_effective_alpha_locked();
    if (visible) *visible = (a > 0) ? 1 : 0;
    if (x)       *x = g_x;
    if (y)       *y = g_y;
    pthread_mutex_unlock(&g_cursor_lock);
}

/* ============================================================
 * Bus handlers
 * ============================================================ */
static void on_cursor_pos(uint16_t type, uint8_t flags,
                          const uint8_t* payload, uint32_t len, void* ctx) {
    (void)type; (void)flags; (void)ctx;
    if (len < 8 || !payload) {
        LOG_WARN(LOG_MODULE, "CURSOR_POS: short payload len=%u", len);
        return;
    }
    int32_t x = (int32_t)read_be32(payload);
    int32_t y = (int32_t)read_be32(payload + 4);
    int alpha = (len >= 12) ? (int)payload[8] : 255;
    if (alpha < 0)   alpha = 0;
    if (alpha > 255) alpha = 255;

    bool was_hidden;
    pthread_mutex_lock(&g_cursor_lock);
    was_hidden = !g_visible;
    g_visible = true;
    g_fading = false;                 /* incoming POS cancels any fade */
    g_x = x;
    g_y = y;
    g_alpha_request = alpha;
    pthread_mutex_unlock(&g_cursor_lock);

    if (was_hidden) {
        LOG_INFO(LOG_MODULE, "SHOW at %d,%d alpha=%d", x, y, alpha);
    }
}

static void on_cursor_hide(uint16_t type, uint8_t flags,
                           const uint8_t* payload, uint32_t len, void* ctx) {
    (void)type; (void)flags; (void)payload; (void)len; (void)ctx;

    bool was_visible;
    int32_t ox, oy;
    pthread_mutex_lock(&g_cursor_lock);
    was_visible = g_visible;
    ox = g_x; oy = g_y;
    if (g_visible && !g_fading) {
        g_fading = true;
        g_fade_start_ms = get_timestamp_ms();
    }
    pthread_mutex_unlock(&g_cursor_lock);

    if (was_visible) {
        LOG_INFO(LOG_MODULE, "HIDE requested (was at %d,%d) — fading", ox, oy);
    }
}

/* ============================================================
 * EVT_SCREEN_INFO publisher — fires once per size change
 * ============================================================ */
static void publish_screen_info_if_changed(int w, int h) {
    if (w <= 0 || h <= 0) return;
    if (w == g_screen_published_w && h == g_screen_published_h) return;

    g_screen_published_w = w;
    g_screen_published_h = h;

    bus_text_builder_t b;
    uint8_t scratch[128];
    bus_text_begin_with(&b, "screeninfo", scratch, sizeof(scratch));
    bus_text_int(&b, "width",  w);
    bus_text_int(&b, "height", h);
    bus_send_text(EVT_SCREEN_INFO, BUS_FLAG_STICKY, &b);

    LOG_INFO(LOG_MODULE, "EVT_SCREEN_INFO published W=%d H=%d", w, h);
}

/* ============================================================
 * NV12 blit — alpha-blended 32×32 sprite, supports linear OR 64×32 tiled
 *
 * NV12 (linear):
 *   Plane 0 (Y) : w × h bytes
 *   Plane 1 (UV): w × (h/2) bytes, interleaved U,V pairs at 2×2 subsample
 *
 * NV12 (Qualcomm 64×32 tiled, w % 64 == 0 && w >= 64 && h >= 32):
 *   Each plane laid out as a sequence of 64×32 byte tiles.  Tiles are
 *   grouped into 2×2 super-blocks (128×64 pixels) in row-major super-
 *   block order.  Inside each super-block, sub-tile order zigzags by
 *   super-block x:
 *     even bx → [TL TR BL BR]         sub = sy*2 + sx
 *     odd  bx → [BL BR TL TR]         sub = (1-sy)*2 + sx
 *   Odd number of tile-rows (nty) compacts the last row to ntx tiles.
 *   Reverse-engineered in carplay_vc/carplay_vc_frame_hook.c.
 *
 * We blend toward white (Y=235, U=V=128).  For a single-colour sprite
 * the UV write pulls chroma toward neutral grey proportionally to alpha,
 * which reads as "white-ish" regardless of underlying colour.
 *
 * Env override:
 *   CP_CURSOR_LINEAR=1 → always use linear blit (for debugging tile
 *                        geometry on hardware).
 * ============================================================ */

#define NV12_TILE_W 64
#define NV12_TILE_H 32

static int g_force_linear = -1;   /* -1 = check env once; 0/1 = cached */

static bool cursor_should_tile(int w, int h) {
    if (g_force_linear < 0) {
        const char* e = getenv("CP_CURSOR_LINEAR");
        g_force_linear = (e && e[0] == '1') ? 1 : 0;
    }
    if (g_force_linear) return false;
    return (w >= NV12_TILE_W) && (h >= NV12_TILE_H) && ((w % NV12_TILE_W) == 0);
}

/* Byte offset within a tiled plane for linear pixel (x, y). */
static size_t tile_byte_offset(int x, int y, int plane_w, int plane_h) {
    const int tw = NV12_TILE_W, th = NV12_TILE_H;
    int ntx  = plane_w / tw;
    int nbx  = ntx / 2;
    int nty  = (plane_h + th - 1) / th;
    int full_pairs = nty / 2;
    int last_partial = (nty % 2);

    int tx = x / tw;
    int ty = y / th;
    int itx = x % tw;
    int ity = y % th;

    int tile_idx;
    if (last_partial && ty == nty - 1) {
        tile_idx = full_pairs * nbx * 4 + tx;
    } else {
        int bx = tx / 2;
        int by = ty / 2;
        int sx = tx % 2;
        int sy = ty % 2;
        int block_idx = by * nbx + bx;
        int sub_idx = (bx % 2 == 0) ? (sy * 2 + sx) : ((1 - sy) * 2 + sx);
        tile_idx = block_idx * 4 + sub_idx;
    }
    return (size_t)tile_idx * (size_t)(tw * th) + (size_t)ity * (size_t)tw + (size_t)itx;
}

static size_t tile_plane_size(int plane_w, int plane_h) {
    const int tw = NV12_TILE_W, th = NV12_TILE_H;
    int ntx = plane_w / tw;
    int nbx = ntx / 2;
    int nty = (plane_h + th - 1) / th;
    int full_pairs = nty / 2;
    size_t sz = (size_t)full_pairs * (size_t)nbx * 4 * (size_t)(tw * th);
    if (nty % 2) sz += (size_t)ntx * (size_t)(tw * th);
    return sz;
}

/* --- Linear blit ---
 *
 * Sprite iteration is clamped to the sprite's bounding box
 * [SPRITE_BB_LO..SPRITE_BB_HI] — everything outside is guaranteed
 * transparent.  For the default 128×128 canvas with R_WHITE=48 this
 * cuts the inner-loop count from 16384 to 9409 (~43% save). */
static void blit_cursor_nv12_linear(uint8_t* buf, int w, int h,
                                    int cx, int cy, int runtime_alpha) {
    const int stride = w;
    uint8_t* const y_plane  = buf;
    uint8_t* const uv_plane = buf + (size_t)stride * (size_t)h;

    const int x0 = cx - SPRITE_CX;
    const int y0 = cy - SPRITE_CY;

    int sy, sx;
    for (sy = SPRITE_BB_LO; sy <= SPRITE_BB_HI; sy++) {
        int dy = y0 + sy;
        if (dy < 0 || dy >= h) continue;
        for (sx = SPRITE_BB_LO; sx <= SPRITE_BB_HI; sx++) {
            int dx = x0 + sx;
            if (dx < 0 || dx >= w) continue;
            sprite_px_t sp = g_sprite[sy][sx];
            if (sp.alpha == 0) continue;
            int a = (sp.alpha * runtime_alpha) / 255;
            if (a <= 0) continue;
            int inv = 255 - a;

            uint8_t* py = y_plane + (size_t)dy * (size_t)stride + (size_t)dx;
            *py = (uint8_t)(((int)(*py) * inv + (int)sp.y_target * a) / 255);

            if (((sx | sy) & 1) == 0) {
                int uvx = (dx & ~1);
                int uvy = (dy >> 1);
                uint8_t* puv = uv_plane + (size_t)uvy * (size_t)stride + (size_t)uvx;
                puv[0] = (uint8_t)(((int)puv[0] * inv + 128 * a) / 255);
                puv[1] = (uint8_t)(((int)puv[1] * inv + 128 * a) / 255);
            }
        }
    }
}

/* --- Tiled blit --- */
static void blit_cursor_nv12_tiled(uint8_t* buf, int w, int h,
                                   int cx, int cy, int runtime_alpha) {
    const int uv_h = h / 2;
    const size_t y_plane_sz  = tile_plane_size(w, h);
    uint8_t* const y_plane  = buf;
    uint8_t* const uv_plane = buf + y_plane_sz;

    const int x0 = cx - SPRITE_CX;
    const int y0 = cy - SPRITE_CY;

    int sy, sx;
    for (sy = SPRITE_BB_LO; sy <= SPRITE_BB_HI; sy++) {
        int dy = y0 + sy;
        if (dy < 0 || dy >= h) continue;
        for (sx = SPRITE_BB_LO; sx <= SPRITE_BB_HI; sx++) {
            int dx = x0 + sx;
            if (dx < 0 || dx >= w) continue;
            sprite_px_t sp = g_sprite[sy][sx];
            if (sp.alpha == 0) continue;
            int a = (sp.alpha * runtime_alpha) / 255;
            if (a <= 0) continue;
            int inv = 255 - a;

            size_t y_off = tile_byte_offset(dx, dy, w, h);
            uint8_t* py = y_plane + y_off;
            *py = (uint8_t)(((int)(*py) * inv + (int)sp.y_target * a) / 255);

            if (((sx | sy) & 1) == 0) {
                int uvx = (dx & ~1);
                int uvy = (dy >> 1);
                if (uvy < uv_h) {
                    /* UV plane is a w × (h/2) tiled plane of interleaved
                     * U,V pairs.  Adjacent bytes (U, V) land contiguously
                     * only when uvx is even, which we enforced above. */
                    size_t uv_off = tile_byte_offset(uvx, uvy, w, uv_h);
                    uint8_t* puv = uv_plane + uv_off;
                    puv[0] = (uint8_t)(((int)puv[0] * inv + 128 * a) / 255);
                    puv[1] = (uint8_t)(((int)puv[1] * inv + 128 * a) / 255);
                }
            }
        }
    }
}

/* Required NV12 plane size (Y + UV) for an (w x h) frame in the
 * layout we're about to blit into.  Returns 0 if inputs are bogus.
 * UV plane in NV12 is w x (h/2) bytes (interleaved U,V pairs). */
static size_t nv12_required_size(int w, int h, bool tiled) {
    if (w <= 0 || h <= 0 || (h & 1)) return 0;
    if (tiled) {
        return tile_plane_size(w, h) + tile_plane_size(w, h / 2);
    }
    return (size_t)w * (size_t)h + (size_t)w * (size_t)(h / 2);
}

/* Dispatch — also verifies the target buffer is large enough.
 * Called from on_fill_buffer_done where we've already clamped
 * w/h to g_screen_w/g_screen_h, but the OMX decoder could have
 * renegotiated to a smaller buffer between our cache hit and
 * this call.  Out-of-bounds writes on the tiled path are
 * especially dangerous: tile_byte_offset can produce very
 * large offsets for the same (x,y) if plane_h changes. */
static void blit_cursor_nv12(uint8_t* buf, size_t buf_filled, size_t buf_alloc,
                             int w, int h, int cx, int cy, int runtime_alpha) {
    if (!buf || w <= 0 || h <= 0 || runtime_alpha <= 0) return;

    bool tiled = cursor_should_tile(w, h);
    size_t need = nv12_required_size(w, h, tiled);
    if (need == 0) return;
    /* nFilledLen is decoder-reported payload size; nAllocLen is the
     * physical buffer capacity.  Either being smaller than the frame
     * plane size means our g_screen_w/h is stale — skip rather than
     * corrupt memory the decoder owns. */
    if (need > buf_alloc || need > buf_filled) {
        static bool warned = false;
        if (!warned) {
            warned = true;
            LOG_WARN(LOG_MODULE,
                "blit skipped: need=%zu filled=%zu alloc=%zu for %dx%d (tiled=%d)",
                need, buf_filled, buf_alloc, w, h, tiled ? 1 : 0);
        }
        return;
    }

    if (tiled) {
        blit_cursor_nv12_tiled(buf, w, h, cx, cy, runtime_alpha);
    } else {
        blit_cursor_nv12_linear(buf, w, h, cx, cy, runtime_alpha);
    }
}

/* ============================================================
 * LD_PRELOAD: screen_create_window_group — find the CarPlay window
 * ============================================================ */
int screen_create_window_group(screen_window_t win, const char* name) {
    ensure_resolved();
    int rc;
    if (real_screen_create_window_group) {
        rc = real_screen_create_window_group(win, name);
    } else {
        /* RTLD_NEXT lookup failed — libscreen.so chain is broken
         * or loaded late.  Returning 0 here would lie to the caller
         * that the group was created; it would then proceed with an
         * ungrouped window and silent compositor desync.  Log once
         * and surface the failure. */
        static bool warned = false;
        if (!warned) {
            warned = true;
            LOG_ERROR(LOG_MODULE,
                "screen_create_window_group: RTLD_NEXT unresolved — "
                "returning -1 (window group NOT created)");
        }
        rc = -1;
    }
    if (name && !g_carplay_window) {
        /* The CarPlay window group on MHI2 is
         * "dio_manager.airplay.qnx.screen_render" (observed). */
        if (strstr(name, "airplay") || strstr(name, "screen_render")) {
            g_carplay_window = win;
            LOG_INFO(LOG_MODULE, "CarPlay window tracked: %p group='%s'", win, name);
        }
    }
    return rc;
}

/* ============================================================
 * LD_PRELOAD: onFillBufferDoneCallback — blit here
 * ============================================================ */
void _ZN3dio16COMXVideoDecoder24onFillBufferDoneCallbackEPvP20OMX_BUFFERHEADERTYPE(
        void* self, void* appPriv, omx_buf_hdr_t* buf) {
    ensure_resolved();

    if (buf && buf->pBuffer && buf->nFilledLen > 0) {
        /* Lazy dimension discovery: query size of the CarPlay window
         * once we've tracked it. */
        if (g_screen_w == 0 && g_carplay_window && real_screen_get_window_property_iv) {
            int size[2] = {0, 0};
            if (real_screen_get_window_property_iv(g_carplay_window,
                                                   SCREEN_PROPERTY_SIZE, size) == 0
                && size[0] > 0 && size[1] > 0) {
                g_screen_w = size[0];
                g_screen_h = size[1];
                publish_screen_info_if_changed(g_screen_w, g_screen_h);
            }
        }

        /* Blit if visible and we have a sane size guess. */
        if (g_screen_w > 0 && g_screen_h > 0) {
            int vis, cx, cy, alpha;
            pthread_mutex_lock(&g_cursor_lock);
            alpha = cursor_effective_alpha_locked();
            vis = (alpha > 0) ? 1 : 0;
            cx = g_x; cy = g_y;
            pthread_mutex_unlock(&g_cursor_lock);

            if (vis) {
                blit_cursor_nv12(buf->pBuffer,
                                 (size_t)buf->nFilledLen, (size_t)buf->nAllocLen,
                                 g_screen_w, g_screen_h, cx, cy, alpha);
            }
        }
    }

    if (real_onFillBufferDoneCallback) {
        real_onFillBufferDoneCallback(self, appPriv, buf);
    }
}

/* ============================================================
 * Module init — registered at hook library load.
 * ============================================================ */
__attribute__((constructor))
static void cursor_module_init(void) {
    sprite_generate();
    /* Handlers registered before bus_init() runs — safe, g_types is BSS-
     * zeroed and bus_init() no longer memsets it. */
    bus_on(CMD_CURSOR_POS,  on_cursor_pos,  NULL);
    bus_on(CMD_CURSOR_HIDE, on_cursor_hide, NULL);
}
