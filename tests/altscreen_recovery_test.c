#include "../hook/altscreen/recovery_policy.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
    alt_recovery_policy_t r;
    uint64_t now = 1000000;
    unsigned granted = 0;
    alt_recovery_reset(&r, 1);
    assert(alt_recovery_request(&r, 1, ALT_KEYFRAME_START, now));
    assert(!alt_recovery_request(&r, 1, ALT_KEYFRAME_START, now + 1));
    assert(alt_recovery_request(&r, 1, ALT_KEYFRAME_UI, now + 2));
    for (unsigned i = 0; i < 20; ++i) {
        now += 250000;
        alt_recovery_fault(&r, now, 1);
        granted += alt_recovery_request(&r, 1, ALT_KEYFRAME_RECOVERY, now);
    }
    assert(granted <= 2 && r.active && r.requests <= 2);
    assert(!alt_recovery_request(&r, 2, ALT_KEYFRAME_RECOVERY, now));
    now = r.request_due_us;
    assert(alt_recovery_request(&r, 1, ALT_KEYFRAME_RECOVERY, now));
    assert(r.request_due_us == now + 8000000);
    alt_recovery_health(&r, now + 1, 1);
    assert(r.active);
    alt_recovery_health(&r, now + 1000001, 1);
    assert(!r.active && !r.requests && !r.connect_failures);
    alt_recovery_health(&r, now + 1000002, 0);
    assert(r.active);
    assert(alt_recovery_request(&r, 1, ALT_KEYFRAME_RECOVERY, now + 1000002));
    alt_recovery_reset(&r, 2);
    assert(!r.active && !r.bootstrap_mask);
    assert(!alt_recovery_request(&r, 1, ALT_KEYFRAME_START, now));
    assert(alt_recovery_request(&r, 2, ALT_KEYFRAME_UI, now));
    assert(!alt_recovery_request(&r, 2, ALT_KEYFRAME_UI, now + 1));
    alt_recovery_health(&r, now + 1, 1);
    alt_recovery_health(&r, now + 500001, 0); /* Interrupted readiness cannot clear recovery. */
    alt_recovery_health(&r, now + 500002, 1);
    alt_recovery_health(&r, now + 1000002, 1);
    assert(r.active);
    alt_recovery_health(&r, now + 1500002, 1);
    assert(!r.active);
    assert(alt_recovery_request(&r, 2, ALT_KEYFRAME_UI, now + 1500003));
    assert(!alt_recovery_request(&r, 2, ALT_KEYFRAME_UI, now + 1500004));
    assert(alt_recovery_request(&r, 2, ALT_KEYFRAME_UI, now + 2000003));
    alt_recovery_fault(&r, now + 2000004, 1);
    assert(r.connect_due_us == now + 2250004);
    alt_recovery_fault(&r, now + 2250004, 1);
    assert(r.connect_due_us == now + 2750004);
    alt_recovery_fault(&r, now + 2750004, 1);
    assert(r.connect_due_us == now + 3750004);
    alt_recovery_fault(&r, now + 3750004, 1);
    assert(r.connect_due_us == now + 4750004);
    puts("altscreen_recovery_test: shared episode, bounded retry, bootstrap/UI exceptions, health "
         "ACK and generation reset PASS");
    return 0;
}
