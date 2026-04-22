/*
 * CarPlay Cursor Hook
 *
 * Receives CMD_CURSOR_POS / CMD_CURSOR_HIDE over the bus, keeps an atomic
 * cursor state, and blends a small sprite onto the decoded NV12 frame
 * going to the CarPlay window before screen_post_window().
 *
 * Status: scaffolding.  NV12 blend + OMX hook point are TODO — flipped on
 * once the overlay sprite + OMX interception are in place.  For now this
 * just records state and logs transitions so the command path end-to-end
 * can be validated on device.
 */

#include "../framework/common.h"
#include "../framework/logging.h"
#include "../framework/bus.h"

DEFINE_LOG_MODULE(CURSOR);

/* ============================================================
 * Cursor state.
 *
 * Writers: bus reader thread (on_cursor_pos / on_cursor_hide).
 * Readers: OMX callback thread (cursor_state_snapshot, future NV12 blit).
 *
 * Protected by a plain pthread_mutex — the QNX 6.5 GCC toolchain does
 * not support C11 __atomic_* builtins.  Critical section is a few words,
 * no contention in practice.
 * ============================================================ */
static pthread_mutex_t g_cursor_lock = PTHREAD_MUTEX_INITIALIZER;
static bool            g_visible = false;
static int32_t         g_x = 0;
static int32_t         g_y = 0;

/* Public accessor for the OMX blend path (future). */
void cursor_state_snapshot(int* visible, int* x, int* y) {
    pthread_mutex_lock(&g_cursor_lock);
    if (visible) *visible = g_visible ? 1 : 0;
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

    bool was_hidden;
    pthread_mutex_lock(&g_cursor_lock);
    was_hidden = !g_visible;
    g_visible = true;
    g_x = x;
    g_y = y;
    pthread_mutex_unlock(&g_cursor_lock);

    if (was_hidden) {
        LOG_INFO(LOG_MODULE, "SHOW at %d,%d", x, y);
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
    g_visible = false;
    pthread_mutex_unlock(&g_cursor_lock);

    if (was_visible) {
        LOG_INFO(LOG_MODULE, "HIDE (was at %d,%d)", ox, oy);
    }
}

/* ============================================================
 * Module init — registered at hook library load.
 * ============================================================ */
__attribute__((constructor))
static void cursor_module_init(void) {
    /* Register handlers before bus actually starts accepting clients;
     * the bus library is thread-safe on bus_on() regardless of order. */
    bus_on(CMD_CURSOR_POS,  on_cursor_pos,  NULL);
    bus_on(CMD_CURSOR_HIDE, on_cursor_hide, NULL);
}
