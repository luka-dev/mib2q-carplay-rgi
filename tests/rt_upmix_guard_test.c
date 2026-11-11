/*
 * The realtime upmix deadline policy: warmup grace, four-in-a-row trip,
 * backoff that doubles and caps, and the return as a cold start.
 *
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */
#include "audio/rt_upmix_guard.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#define OVER  (SPA_RT_UPMIX_BUDGET_NS + UINT64_C(1))
#define UNDER (SPA_RT_UPMIX_BUDGET_NS - UINT64_C(1))

/* One period every 16 ms, which is never a break. */
#define STEADY_GAP UINT64_C(16000000)

static unsigned run_periods(spa_rt_upmix_guard_t *guard, unsigned count,
                            uint64_t elapsed_ns)
{
    unsigned trips = 0;
    unsigned i;

    for (i = 0; i < count; ++i) {
        assert(spa_rt_upmix_guard_gap(guard, STEADY_GAP) == 0);
        if (!guard->disabled) trips += (unsigned)spa_rt_upmix_guard_period(guard, elapsed_ns);
    }
    return trips;
}

/* A cold start is never evidence: the excused periods do not trip the guard
 * and they are accounted separately from the judged worst. */
static void test_warmup_is_excused(void)
{
    spa_rt_upmix_guard_t guard;

    spa_rt_upmix_guard_arm(&guard);
    assert(run_periods(&guard, SPA_RT_UPMIX_WARMUP_PERIODS, OVER) == 0);
    assert(!guard.disabled);
    assert(guard.coldWorstNs == OVER);
    assert(guard.worstNs == 0);
    assert(guard.warmupLeft == 0);
    /* The very next period is judged, but one is not four. */
    assert(run_periods(&guard, 1, OVER) == 0);
    assert(guard.worstNs == OVER && guard.slowPeriods == 1);
}

/* Four consecutive, and only consecutive, overruns disable it. */
static void test_four_consecutive_trip(void)
{
    spa_rt_upmix_guard_t guard;
    unsigned i;

    spa_rt_upmix_guard_arm(&guard);
    guard.warmupLeft = 0;
    for (i = 0; i < 20u; ++i) {
        assert(run_periods(&guard, SPA_RT_UPMIX_SLOW_LIMIT - 1u, OVER) == 0);
        assert(run_periods(&guard, 1, UNDER) == 0);
        assert(!guard.disabled && guard.slowPeriods == 0);
    }
    assert(run_periods(&guard, SPA_RT_UPMIX_SLOW_LIMIT, OVER) == 1);
    assert(guard.disabled && guard.disables == 1);
    /* And it stays off while the stream keeps flowing. */
    assert(run_periods(&guard, 1000u, UNDER) == 0);
    assert(guard.disabled && guard.revivals == 0);
}

/* It stays off through the rest of the track and comes back on the next break
 * in the stream, as a cold start.  A failure right after the break simply waits
 * for the following one. */
static void test_revives_only_on_a_break(void)
{
    spa_rt_upmix_guard_t guard;
    unsigned attempt;

    spa_rt_upmix_guard_arm(&guard);
    guard.warmupLeft = 0;
    for (attempt = 0; attempt < 4u; ++attempt) {
        assert(run_periods(&guard, SPA_RT_UPMIX_SLOW_LIMIT, OVER) == 1);
        assert(guard.disabled && guard.disables == attempt + 1u);
        /* Minutes of playing do not bring it back. */
        assert(run_periods(&guard, 60u * 1000u / 16u, UNDER) == 0);
        assert(guard.disabled);
        /* Just short of a break does not either. */
        assert(spa_rt_upmix_guard_gap(&guard, SPA_RT_UPMIX_PAUSE_NS - 1u) == 0);
        assert(guard.disabled);
        /* The break does, as a cold start. */
        assert(spa_rt_upmix_guard_gap(&guard, SPA_RT_UPMIX_PAUSE_NS) == 1);
        assert(!guard.disabled && guard.revivals == attempt + 1u);
        assert(guard.warmupLeft == SPA_RT_UPMIX_WARMUP_PERIODS);
        assert(run_periods(&guard, SPA_RT_UPMIX_WARMUP_PERIODS, OVER) == 0);
        assert(!guard.disabled);
    }
}

/* A break while it is healthy costs nothing but the warmup grace. */
static void test_break_while_healthy(void)
{
    spa_rt_upmix_guard_t guard;

    spa_rt_upmix_guard_arm(&guard);
    guard.warmupLeft = 0;
    assert(run_periods(&guard, 100u, UNDER) == 0);
    assert(spa_rt_upmix_guard_gap(&guard, 10u * SPA_RT_UPMIX_PAUSE_NS) == 0);
    assert(!guard.disabled && guard.revivals == 0);
    assert(guard.warmupLeft == SPA_RT_UPMIX_WARMUP_PERIODS);
}

/* A timeline reset excuses the next periods again without clearing the
 * session's history, and a healthy stream never trips. */
static void test_cold_reset_and_healthy_stream(void)
{
    spa_rt_upmix_guard_t guard;

    spa_rt_upmix_guard_arm(&guard);
    guard.warmupLeft = 0;
    assert(run_periods(&guard, SPA_RT_UPMIX_SLOW_LIMIT - 1u, OVER) == 0);
    spa_rt_upmix_guard_cold(&guard);
    assert(guard.slowPeriods == 0);
    assert(run_periods(&guard, SPA_RT_UPMIX_WARMUP_PERIODS + 1u, OVER) == 0);
    assert(!guard.disabled);

    spa_rt_upmix_guard_arm(&guard);
    assert(run_periods(&guard, 100000u, UNDER) == 0);
    assert(!guard.disabled && guard.disables == 0 && guard.worstNs == UNDER);
    /* Exactly at the budget is not over it. */
    assert(run_periods(&guard, 100u, SPA_RT_UPMIX_BUDGET_NS) == 0);
    assert(!guard.disabled);
}

/* Dropout bookkeeping: jitter is not a gap, the buckets split at 32/64/128/500
 * ms, only gaps over 100 ms are worth a WARN, and silence counts as a run only
 * once it is long enough to hear and only when it ends. */
static void test_dropout_bookkeeping(void)
{
    spa_rt_dropout_t d;

    memset(&d, 0, sizeof(d));
    assert(spa_rt_dropout_gap(&d, STEADY_GAP) == 0);
    assert(spa_rt_dropout_gap(&d, UINT64_C(31999999)) == 0);
    assert(d.gapHist[0] == 0);
    assert(spa_rt_dropout_gap(&d, UINT64_C(32000000)) == 0);
    assert(spa_rt_dropout_gap(&d, UINT64_C(64000000)) == 0);
    assert(spa_rt_dropout_gap(&d, UINT64_C(100000001)) == 1);
    assert(spa_rt_dropout_gap(&d, UINT64_C(128000000)) == 1);
    assert(spa_rt_dropout_gap(&d, SPA_RT_UPMIX_PAUSE_NS) == 1);
    assert(d.gapHist[0] == 1 && d.gapHist[1] == 2 && d.gapHist[2] == 1 && d.gapHist[3] == 1);

    /* Stock leads every stream with mute buffers: silence before the first
     * music is a start, however long, not a hole. */
    assert(spa_rt_dropout_period(&d, 1) == 0);
    assert(spa_rt_dropout_period(&d, 1) == 0);
    assert(spa_rt_dropout_period(&d, 1) == 0);
    assert(spa_rt_dropout_period(&d, 0) == 0);
    assert(d.zeroRuns == 0 && d.live);
    /* One silent period is dither or a quiet intro, not a run. */
    assert(spa_rt_dropout_period(&d, 1) == 0);
    assert(spa_rt_dropout_period(&d, 0) == 0);
    assert(d.zeroRuns == 0);
    /* Two in a row are, reported when the music comes back. */
    assert(spa_rt_dropout_period(&d, 1) == 0);
    assert(spa_rt_dropout_period(&d, 1) == 0);
    assert(d.zeroRuns == 0);
    assert(spa_rt_dropout_period(&d, 0) == 2);
    assert(d.zeroRuns == 1 && d.longestZeroRun == 2);
    assert(spa_rt_dropout_period(&d, 1) == 0);
    assert(spa_rt_dropout_period(&d, 1) == 0);
    assert(spa_rt_dropout_period(&d, 1) == 0);
    assert(spa_rt_dropout_period(&d, 0) == 3);
    assert(d.zeroRuns == 2 && d.longestZeroRun == 3 && d.zeroRun == 0);
    /* Closing at stop with nothing open changes nothing. */
    assert(spa_rt_dropout_period(&d, 0) == 0);
    assert(d.zeroRuns == 2);
    /* A break (pause, track or app change) starts over: the silence that was
     * open across it and the silence leading back in are not holes. */
    assert(spa_rt_dropout_period(&d, 1) == 0);
    assert(spa_rt_dropout_period(&d, 1) == 0);
    assert(spa_rt_dropout_gap(&d, SPA_RT_UPMIX_PAUSE_NS) == 1);
    assert(!d.live && d.zeroRun == 0);
    assert(spa_rt_dropout_period(&d, 1) == 0);
    assert(spa_rt_dropout_period(&d, 1) == 0);
    assert(spa_rt_dropout_period(&d, 0) == 0);
    assert(d.zeroRuns == 2);
}

int main(void)
{
    test_warmup_is_excused();
    test_four_consecutive_trip();
    test_revives_only_on_a_break();
    test_break_while_healthy();
    test_cold_reset_and_healthy_stream();
    test_dropout_bookkeeping();
    puts("rt_upmix_guard_test: OK (warmup grace, consecutive trip, revival on a break, dropout buckets)");
    return 0;
}
