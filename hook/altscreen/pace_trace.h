/* Optional stream-111 telemetry. No media data or absolute sender timestamps.
 * Each receiver owns its accumulator; reconnects never mix sampling windows. */
#ifndef ALT_PACE_TRACE_H
#define ALT_PACE_TRACE_H

#include <stdint.h>
#include <string.h>

typedef struct {
    uint64_t sum, max;
    unsigned count, zero;
} alt_pace_metric_t;

typedef struct {
    uint64_t last_arrival_us, last_pts;
    unsigned frames, forwarded, pts_invalid;
    alt_pace_metric_t arrival, read, work, pts;
} alt_pace_trace_t;

static inline void alt_pace_add(alt_pace_metric_t *m, uint64_t us)
{
    m->sum += us;
    if (us > m->max) m->max = us;
    m->count++;
    if (!us) m->zero++;
}

static inline uint64_t alt_pace_mean(const alt_pace_metric_t *m)
{
    return m->count ? m->sum / m->count : 0;
}

static inline void alt_pace_record(alt_pace_trace_t *s, uint64_t arrival_us,
                                  uint64_t read_us, uint64_t work_us,
                                  uint64_t sender_pts, int forwarded)
{
    if (s->last_arrival_us && arrival_us >= s->last_arrival_us)
        alt_pace_add(&s->arrival, arrival_us - s->last_arrival_us);
    s->last_arrival_us = arrival_us;
    alt_pace_add(&s->read, read_us);
    alt_pace_add(&s->work, work_us);
    /* AirPlayScreenHeader.params[0] is NTP 32.32. Only report plausible
     * positive deltas; unsynchronised/unused timestamps are not sender FPS. */
    if (sender_pts && s->last_pts) {
        uint64_t delta = sender_pts - s->last_pts;
        if (delta && delta <= (UINT64_C(10) << 32)) {
            uint64_t us = (delta >> 32) * UINT64_C(1000000)
                + ((delta & UINT64_C(0xffffffff)) * UINT64_C(1000000) >> 32);
            alt_pace_add(&s->pts, us);
        } else s->pts_invalid++;
    } else s->pts_invalid++;
    s->last_pts = sender_pts;
    s->frames++;
    if (forwarded) s->forwarded++;
}

static inline void alt_pace_reset_window(alt_pace_trace_t *s)
{
    uint64_t arrival = s->last_arrival_us, pts = s->last_pts;
    memset(s, 0, sizeof(*s));
    s->last_arrival_us = arrival;
    s->last_pts = pts;
}

#endif
