/*
 * rt_upmix_guard.h -- deadline policy for the realtime (type-100) upmix.
 *
 * The realtime path upmixes one QSA period inside the stock audio thread. If
 * that takes too long the rears are not worth an underrun, so the upmix gives
 * up; but giving up for the rest of the track over one bad moment is worse than
 * trying again, and the periods right after a start or a timeline reset run on
 * cold caches and a cold controller worker, which is not evidence about the
 * steady state. Keeping the policy pure makes those three rules testable
 * without QSA, threads or a head unit.
 *
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */
#ifndef RT_UPMIX_GUARD_H
#define RT_UPMIX_GUARD_H

#include <stdint.h>

/* Half of the 16 ms period was a guess made before the cost was measured; the
 * DSP is about a quarter cheaper since, and the guard is a safety net rather
 * than a deadline, so it sits at 10 ms and leaves 6 ms for the rest of the
 * thread. Four consecutive periods over it, not one, mean the unit cannot keep
 * up. */
#define SPA_RT_UPMIX_BUDGET_NS  UINT64_C(10000000)
#define SPA_RT_UPMIX_SLOW_LIMIT 4u
#define SPA_RT_UPMIX_WARMUP_PERIODS 16u
/* Coming back in the middle of a track means the rears appear under the music,
 * which is more noticeable than their absence, and a failed attempt costs about
 * 320 ms of periods at the limit before it gives up again.  So the upmix waits
 * for a real gap in the stream instead of a timer: playback paused, the track
 * or the app changed, the phone stopped writing.  Realtime periods arrive every
 * 16 ms, so anything beyond a few hundred milliseconds is a break, not jitter.
 * If it fails again after the break it simply waits for the next one. */
#define SPA_RT_UPMIX_PAUSE_NS UINT64_C(500000000)

typedef struct {
    int disabled;            /* the DSP is skipped while this is set */
    uint32_t slowPeriods;    /* consecutive periods over the budget */
    uint32_t warmupLeft;     /* periods still excused from the guard */
    uint32_t disables;       /* how many times the guard tripped this session */
    uint32_t revivals;       /* how many times a break brought it back */
    uint64_t worstNs;        /* worst judged period */
    uint64_t coldWorstNs;    /* worst excused period, reported separately */
} spa_rt_upmix_guard_t;

/* A fresh sink: no history at all, and the first periods are excused.  This
 * clears the session counters too, so a caller never reports a previous
 * session's worst or disable count. */
static inline void spa_rt_upmix_guard_arm(spa_rt_upmix_guard_t *guard)
{
    guard->disabled = 0;
    guard->slowPeriods = 0;
    guard->warmupLeft = SPA_RT_UPMIX_WARMUP_PERIODS;
    guard->disables = 0;
    guard->revivals = 0;
    guard->worstNs = 0;
    guard->coldWorstNs = 0;
}

/* The DSP state was reset (stock restarted its timeline, or the upmix resumes
 * after periods it did not own), so the next periods are cold again. */
static inline void spa_rt_upmix_guard_cold(spa_rt_upmix_guard_t *guard)
{
    guard->warmupLeft = SPA_RT_UPMIX_WARMUP_PERIODS;
    guard->slowPeriods = 0;
}

/* Once per realtime period, with the gap since the previous one.  A gap that
 * long is a break in the stream, so the upmix may try again as a cold start.
 * 1 = it just came back. */
static inline int spa_rt_upmix_guard_gap(spa_rt_upmix_guard_t *guard,
                                         uint64_t gap_ns)
{
    if (gap_ns < SPA_RT_UPMIX_PAUSE_NS) return 0;
    spa_rt_upmix_guard_cold(guard);   /* the DSP is cold after any break */
    if (!guard->disabled) return 0;
    guard->disabled = 0;
    ++guard->revivals;
    return 1;
}

/* One measured period.  1 = the guard tripped on this one. */
static inline int spa_rt_upmix_guard_period(spa_rt_upmix_guard_t *guard,
                                            uint64_t elapsed_ns)
{
    if (guard->warmupLeft) {
        --guard->warmupLeft;
        if (elapsed_ns > guard->coldWorstNs) guard->coldWorstNs = elapsed_ns;
        return 0;
    }
    if (elapsed_ns > guard->worstNs) guard->worstNs = elapsed_ns;
    if (elapsed_ns <= SPA_RT_UPMIX_BUDGET_NS) {
        guard->slowPeriods = 0;
        return 0;
    }
    if (++guard->slowPeriods < SPA_RT_UPMIX_SLOW_LIMIT) return 0;
    guard->disabled = 1;
    ++guard->disables;
    return 1;
}

#endif /* RT_UPMIX_GUARD_H */
