/* Small condition wrapper for scheduling, independent of wall-clock changes. */
#ifndef ALT_MONOTONIC_CONDITION_H
#define ALT_MONOTONIC_CONDITION_H
#include <pthread.h>
#include <errno.h>
#include "alt_timing_stats.h"
typedef struct {
    pthread_cond_t fallback, *active;
#if !defined(__APPLE__)
    pthread_cond_t monotonic;
    int ready;
#endif
} alt_monotonic_condition_t;
static inline int alt_condition_init(alt_monotonic_condition_t *c) {
#if defined(__APPLE__)
    (void)c;
    return 0;
#else
    pthread_condattr_t attr;
    int rc;
    if (c->ready)
        return 0;
    rc = pthread_condattr_init(&attr);
    if (rc)
        return rc;
    rc = pthread_condattr_setclock(&attr, CLOCK_MONOTONIC);
    if (!rc)
        rc = pthread_cond_init(&c->monotonic, &attr);
    pthread_condattr_destroy(&attr);
    if (!rc) {
        c->active = &c->monotonic;
        c->ready = 1;
    }
    return rc;
#endif
}
static inline int alt_condition_wait_until(alt_monotonic_condition_t *c, pthread_mutex_t *lock,
                                           uint64_t deadline_us) {
    struct timespec deadline;
    uint64_t now = alt_timing_now_us();
    if (!now || now >= deadline_us)
        return ETIMEDOUT;
#if defined(__APPLE__)
    deadline_us -= now;
    deadline.tv_sec = (time_t)(deadline_us / 1000000u);
    deadline.tv_nsec = (long)(deadline_us % 1000000u) * 1000L;
    return pthread_cond_timedwait_relative_np(c->active, lock, &deadline);
#else
    deadline.tv_sec = (time_t)(deadline_us / 1000000u);
    deadline.tv_nsec = (long)(deadline_us % 1000000u) * 1000L;
    return pthread_cond_timedwait(c->active, lock, &deadline);
#endif
}
#endif
