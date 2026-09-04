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

#endif /* PCM_PERIOD_H */
