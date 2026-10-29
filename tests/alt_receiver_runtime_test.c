/* Scheduling failures stay local; use the stock default and existing override. */
#include "../hook/altscreen/receiver_runtime.h"
#include <assert.h>
#include <stdio.h>

static int requested, calls, result;
static int set_priority(int value) { requested = value; ++calls; return result; }
int main(void)
{
    alt_receiver_schedule_t state;
    assert(!unsetenv("SCREEN_RECEIVER_THREAD_PRIO"));
    state = alt_receiver_schedule(set_priority);
    assert(state.requested == 17 && requested == 17 && !state.result && calls == 1);
    assert(!state.before_error && !state.after_error);
    assert(state.before_policy == state.after_policy && state.before_priority == state.after_priority);
    assert(!setenv("SCREEN_RECEIVER_THREAD_PRIO", "16", 1));
    result = EPERM;
    state = alt_receiver_schedule(set_priority);
    assert(state.requested == 16 && requested == 16 && state.result == EPERM && calls == 2);
    state = alt_receiver_schedule(NULL);
    assert(state.result == ENOSYS && calls == 2);
    assert(state.before_policy == state.after_policy && state.before_priority == state.after_priority);
    puts("alt_receiver_runtime: stock default/override, missing helper and denied priority PASS");
    return 0;
}
