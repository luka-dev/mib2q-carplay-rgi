/* Stock-210 receiver scheduling parity; no process-wide priority change. */
#ifndef ALT_RECEIVER_RUNTIME_H
#define ALT_RECEIVER_RUNTIME_H

#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <stdlib.h>

typedef int (*alt_receiver_set_priority_t)(int);

typedef struct {
    int requested, result;
    int before_error, before_policy, before_priority;
    int after_error, after_policy, after_priority;
} alt_receiver_schedule_t;

static inline alt_receiver_schedule_t
alt_receiver_schedule(alt_receiver_set_priority_t set_priority)
{
    alt_receiver_schedule_t state;
    struct sched_param parameter = {0};
    const char *override = getenv("SCREEN_RECEIVER_THREAD_PRIO");

    /* libairplay 210.81 _ScreenThread @0x2425c..0x2429c uses exactly
     * this existing environment override/default, then calls its exported
     * SetCurrentThreadPriority. That helper selects FIFO on this QNX ABI.
     * Calling the same helper avoids inventing a second policy mapping. */
    state.requested = override ? atoi(override) : 17;
    state.before_policy = state.after_policy = -1;
    state.before_priority = state.after_priority = -1;
    state.before_error = pthread_getschedparam(pthread_self(),
                                              &state.before_policy, &parameter);
    if (!state.before_error) state.before_priority = parameter.sched_priority;
    state.result = set_priority ? set_priority(state.requested) : ENOSYS;
    state.after_error = pthread_getschedparam(pthread_self(),
                                             &state.after_policy, &parameter);
    if (!state.after_error) state.after_priority = parameter.sched_priority;
    return state;
}

#endif
