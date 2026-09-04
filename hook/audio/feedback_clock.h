/*
 * feedback_clock.h — measured clock for hook-owned detached QSA playback.
 *
 * Type-100 normally supplies AirPlay's measured `sr` feedback.  When iOS
 * tears type-100 down but leaves buffered type-103 playing, the APAP thread
 * becomes the QSA writer and can measure the actual sink cadence itself.
 *
 * Ignore the first few queued writes, measure over a long window, and reject
 * a window containing a scheduler-sized gap.  This keeps a handoff transient
 * from becoming the clock reference for the rest of a song.
 *
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */
#ifndef FEEDBACK_CLOCK_H
#define FEEDBACK_CLOCK_H

#include <stdint.h>

#define SPA_FEEDBACK_CLOCK_WARMUP_WRITES 256u
#define SPA_FEEDBACK_CLOCK_WINDOW_WRITES 1024u
#define SPA_FEEDBACK_CLOCK_MAX_GAP_NS UINT64_C(100000000)
#define SPA_FEEDBACK_CLOCK_MIN_RATE 40000.0
#define SPA_FEEDBACK_CLOCK_MAX_RATE 60000.0

typedef struct {
    uint64_t warmupWrites;
    uint64_t windowWrites;
    uint64_t windowFrames;
    uint64_t windowStartNs;
    uint64_t lastWriteNs;
    double rateHz;
    int valid;
} spa_feedback_clock_t;

static inline void spa_feedback_clock_reset(spa_feedback_clock_t *clock)
{
    if (!clock) return;
    clock->warmupWrites = 0;
    clock->windowWrites = 0;
    clock->windowFrames = 0;
    clock->windowStartNs = 0;
    clock->lastWriteNs = 0;
    clock->rateHz = 0.0;
    clock->valid = 0;
}

/* Returns 1 when a new rate was accepted, 0 while accumulating, and -1 when
 * a recovery, long gap, invalid timestamp, or implausible window reset the
 * measurement.  If the reset point is itself a successful timestamped write,
 * that write becomes warmup #1. */
static inline int spa_feedback_clock_note_write(spa_feedback_clock_t *clock,
                                                uint64_t nowNs,
                                                uint64_t frames,
                                                int discontinuity)
{
    uint64_t elapsedNs;
    double rate;

    if (!clock) return -1;
    if (!nowNs || !frames) {
        spa_feedback_clock_reset(clock);
        return -1;
    }
    if (discontinuity ||
        (clock->lastWriteNs &&
         (nowNs <= clock->lastWriteNs ||
          nowNs - clock->lastWriteNs > SPA_FEEDBACK_CLOCK_MAX_GAP_NS))) {
        spa_feedback_clock_reset(clock);
        clock->lastWriteNs = nowNs;
        clock->warmupWrites = 1;
        return -1;
    }

    clock->lastWriteNs = nowNs;
    if (clock->warmupWrites < SPA_FEEDBACK_CLOCK_WARMUP_WRITES) {
        clock->warmupWrites++;
        if (clock->warmupWrites == SPA_FEEDBACK_CLOCK_WARMUP_WRITES)
            clock->windowStartNs = nowNs;
        return 0;
    }

    if (!clock->windowStartNs) {
        clock->windowStartNs = nowNs;
        return 0;
    }
    clock->windowWrites++;
    clock->windowFrames += frames;
    if (clock->windowWrites < SPA_FEEDBACK_CLOCK_WINDOW_WRITES) return 0;

    elapsedNs = nowNs - clock->windowStartNs;
    rate = elapsedNs ?
        ((double)clock->windowFrames * 1000000000.0) / (double)elapsedNs : 0.0;
    clock->windowStartNs = nowNs;
    clock->windowWrites = 0;
    clock->windowFrames = 0;
    if (rate < SPA_FEEDBACK_CLOCK_MIN_RATE ||
        rate > SPA_FEEDBACK_CLOCK_MAX_RATE || rate != rate) {
        spa_feedback_clock_reset(clock);
        clock->lastWriteNs = nowNs;
        clock->warmupWrites = 1;
        return -1;
    }

    clock->rateHz = rate;
    clock->valid = 1;
    return 1;
}

static inline double spa_feedback_clock_get(const spa_feedback_clock_t *clock,
                                            int *valid)
{
    int haveRate = clock && clock->valid;

    if (valid) *valid = haveRate;
    return haveRate ? clock->rateHz : 0.0;
}

#endif /* FEEDBACK_CLOCK_H */
