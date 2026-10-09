/* Pure compressed-input policy. No clocks, locks, sockets or hardware calls. */
#ifndef ALTSCREEN_QUEUE_POLICY_H
#define ALTSCREEN_QUEUE_POLICY_H
#include "../common/alt_video_wire.h"
#define ALT_QUEUE_STARTUP_ITEMS 4u
#define ALT_QUEUE_FRESH_MS UINT64_C(250)
#define ALT_QUEUE_BACKLOG_MS UINT64_C(1000)
typedef struct {
    int ready, busy;
    unsigned pending_count;
    uint64_t latest_origin_ms, busy_origin_ms;
    uint64_t pending_origin_ms[ALT_QUEUE_STARTUP_ITEMS];
} alt_queue_view_t;
static inline uint64_t alt_queue_origin_ms(const alt_frame_meta_t *meta, uint64_t enqueued_ms,
                                           uint64_t now_ms) {
    uint64_t origin = meta && meta->version == ALT_WIRE_VERSION ? meta->received_us / 1000u : 0;
    return origin && origin <= now_ms && origin < enqueued_ms ? origin : enqueued_ms;
}
static inline int alt_queue_fresh(uint64_t origin_ms, uint64_t now_ms) {
    return origin_ms && now_ms >= origin_ms && now_ms - origin_ms <= ALT_QUEUE_FRESH_MS;
}
static inline int alt_queue_can_enter_steady(const alt_queue_view_t *view, uint64_t now_ms) {
    unsigned i;
    if (!view || !view->ready || view->pending_count > ALT_QUEUE_STARTUP_ITEMS ||
        !alt_queue_fresh(view->latest_origin_ms, now_ms) ||
        (view->busy && !alt_queue_fresh(view->busy_origin_ms, now_ms)))
        return 0;
    for (i = 0; i < view->pending_count; ++i)
        if (!alt_queue_fresh(view->pending_origin_ms[i], now_ms))
            return 0;
    return 1;
}
static inline int alt_queue_has_backlog(int steady, unsigned pending_count, uint64_t head_origin_ms,
                                        uint64_t now_ms) {
    /* An unavailable clock cannot prove backlog. A sole/final AU is protected. */
    return steady && pending_count > 1 && head_origin_ms && now_ms >= head_origin_ms &&
           now_ms - head_origin_ms > ALT_QUEUE_BACKLOG_MS;
}
#endif
