#include "../altscreen_render/queue_policy.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
    alt_frame_meta_t meta = {0};
    alt_queue_view_t view = {0};
    uint64_t now = 5000;
    meta.version = ALT_WIRE_VERSION;
    meta.received_us = 3100000;
    assert(alt_queue_origin_ms(&meta, 4900, now) == 3100);
    meta.received_us = 6000000;
    assert(alt_queue_origin_ms(&meta, 4900, now) == 4900);
    meta.received_us = 0;
    assert(alt_queue_origin_ms(&meta, 4900, now) == 4900);
    view.ready = 1;
    view.latest_origin_ms = 4750;
    view.pending_count = 3;
    for (unsigned i = 0; i < 3; ++i)
        view.pending_origin_ms[i] = 4750;
    assert(alt_queue_can_enter_steady(&view, now));
    view.pending_origin_ms[1] = 4749;
    assert(!alt_queue_can_enter_steady(&view, now));
    view.pending_origin_ms[1] = 4750;
    view.busy = 1;
    view.busy_origin_ms = 4749;
    assert(!alt_queue_can_enter_steady(&view, now));
    view.busy_origin_ms = 4750;
    assert(alt_queue_can_enter_steady(&view, now));
    view.pending_count = 5;
    assert(!alt_queue_can_enter_steady(&view, now));
    view.pending_count = 0;
    view.latest_origin_ms = 3100;
    assert(!alt_queue_can_enter_steady(&view, now));
    view.latest_origin_ms = now;
    view.ready = 0;
    assert(!alt_queue_can_enter_steady(&view, now));
    assert(!alt_queue_has_backlog(0, 3, 3100, now));
    assert(!alt_queue_has_backlog(1, 1, 3100, now));
    assert(!alt_queue_has_backlog(1, 2, 4000, now));
    assert(alt_queue_has_backlog(1, 2, 3999, now));
    assert(!alt_queue_has_backlog(1, 2, 0, now));
    assert(!alt_queue_has_backlog(1, 2, 6000, now));
    puts("altscreen_policy_test: pure origin/freshness boundaries, readiness/startup, "
         "middle/in-flight AU, capacity and invalid clock safeguards PASS");
    return 0;
}
