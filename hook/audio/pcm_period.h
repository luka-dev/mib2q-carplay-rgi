/*
 * pcm_period.h — whole-period selection for the buffered CarPlay PCM rail.
 *
 * A QSA period must never contain a decoded prefix followed by synthesized
 * silence.  If the producer has fewer frames than the sink requests, leave
 * every buffered frame queued and use the realtime fallback for the entire
 * period.  Keeping this decision tiny and pure also makes the 1024-frame
 * codec / 768-frame sink cadence directly regression-testable.
 *
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */
#ifndef PCM_PERIOD_H
#define PCM_PERIOD_H

#include <stddef.h>

static inline size_t spa_pcm_whole_period_frames(size_t available,
                                                  size_t requested)
{
    return requested && available >= requested ? requested : 0;
}

static inline int spa_pcm_prime_ready(size_t available, size_t target)
{
    return target != 0 && available >= target;
}

/* The detached APAP receiver is both the source pump and the QSA render
 * clock.  Keep reading while the software cushion is below its high-water
 * mark, but never stop below one writable QSA period (important when someone
 * selects a very small AUDIO_HIGH_MS).  At/above high water, one blocking QSA
 * write is the whole loop iteration and TCP backpressure retains the source
 * data on the phone instead of overflowing this ring. */
static inline int spa_pcm_pump_should_read_source(int detached,
                                                   size_t available,
                                                   size_t period,
                                                   size_t high_water)
{
    if (!detached) return 1;
    if (!period || available < period) return 1;
    return !high_water || available < high_water;
}

/* Once a complete sink period is queued, polling the nonblocking APAP socket
 * must not delay rendering.  A zero-time poll opportunistically ingests one
 * source record; if none is ready, the blocking QSA write advances playback.
 * Below a period, wait for the producer instead of spinning. */
static inline int spa_pcm_pump_poll_timeout_ms(int detached,
                                               size_t available,
                                               size_t period,
                                               int source_wait_ms)
{
    return detached && period && available >= period ? 0 : source_wait_ms;
}

/* Stock-owned playback normally paces a backpressured APAP source at the
 * codec frame duration.  During FLUSHBUFFERED that would make the receiver
 * discard an already-queued stale item in real time: every 1024 old frames
 * would cost another 21.33 ms, and repeated seeks would move the boundary
 * faster than it is reached.  Flush draining is the one safe exception because
 * none of those authenticated records can be rendered. */
static inline int spa_pcm_source_should_pace(int backpressure,
                                             int detached,
                                             int flush_pending)
{
    return backpressure && !detached && !flush_pending;
}

enum spa_pcm_period_source {
    SPA_PCM_PERIOD_FROM_REALTIME = 0,
    SPA_PCM_PERIOD_FROM_BUFFERED = 1,
    SPA_PCM_PERIOD_FROM_SILENCE = 2
};

/* Type 100 and buffered type 103 are separate sender timelines.  The stock
 * source is a valid bridge only before/after buffered media ownership.  Once
 * type 103 owns the media rail, substituting type-100 PCM during a pause,
 * flush/re-prime, or underrun can play a different item under the current
 * metadata.  Keep QSA clocked with a whole silent period instead. */
static inline enum spa_pcm_period_source spa_pcm_period_source(
    int buffered_owns_media, size_t buffered_frames)
{
    if (!buffered_owns_media) return SPA_PCM_PERIOD_FROM_REALTIME;
    return buffered_frames ? SPA_PCM_PERIOD_FROM_BUFFERED
                           : SPA_PCM_PERIOD_FROM_SILENCE;
}

#endif /* PCM_PERIOD_H */
