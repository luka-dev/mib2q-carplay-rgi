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

static unsigned run_periods(spa_rt_upmix_guard_t *guard, unsigned count,
                            uint64_t elapsed_ns)
{
    unsigned trips = 0;
    unsigned i;

    for (i = 0; i < count; ++i) {
        spa_rt_upmix_guard_retry(guard);
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
    assert(guard.retryCountdown == SPA_RT_UPMIX_RETRY_PERIODS);
}

/* It comes back on its own, as a cold start, and the wait doubles with every
 * further failure until it caps. */
static void test_backoff_returns_and_caps(void)
{
    spa_rt_upmix_guard_t guard;
    uint32_t expected = SPA_RT_UPMIX_RETRY_PERIODS;
    unsigned attempt;

    spa_rt_upmix_guard_arm(&guard);
    guard.warmupLeft = 0;
    for (attempt = 0; attempt < 8u; ++attempt) {
        uint32_t wait = expected;
        unsigned i;

        assert(run_periods(&guard, SPA_RT_UPMIX_SLOW_LIMIT, OVER) == 1);
        assert(guard.disabled && guard.retryCountdown == wait);
        /* Still disabled one period before the backoff expires. */
        for (i = 0; i + 1u < wait; ++i) {
            assert(spa_rt_upmix_guard_retry(&guard) == 0);
            assert(guard.disabled);
        }
        assert(spa_rt_upmix_guard_retry(&guard) == 1);
        assert(!guard.disabled);
        /* Back as a cold start: the excused periods cannot trip it again. */
        assert(guard.warmupLeft == SPA_RT_UPMIX_WARMUP_PERIODS);
        assert(run_periods(&guard, SPA_RT_UPMIX_WARMUP_PERIODS, OVER) == 0);
        assert(!guard.disabled);
        expected = expected > SPA_RT_UPMIX_RETRY_MAX_PERIODS / 2u
            ? SPA_RT_UPMIX_RETRY_MAX_PERIODS : expected * 2u;
    }
    assert(guard.retryPeriods == SPA_RT_UPMIX_RETRY_MAX_PERIODS);
    assert(guard.disables == 8u);
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

int main(void)
{
    test_warmup_is_excused();
    test_four_consecutive_trip();
    test_backoff_returns_and_caps();
    test_cold_reset_and_healthy_stream();
    puts("rt_upmix_guard_test: OK (warmup grace, consecutive trip, backoff and return)");
    return 0;
}
