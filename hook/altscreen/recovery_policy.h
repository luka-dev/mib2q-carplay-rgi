/* Pure monotonic policy; protected by the forwarder's state mutex. Successful
 * IDR receipt/send alone never resets recovery. Renderer presentation health does. */
#ifndef ALT_RECOVERY_POLICY_H
#define ALT_RECOVERY_POLICY_H
#include <stdint.h>
#include <string.h>
enum { ALT_KEYFRAME_RECOVERY = 0, ALT_KEYFRAME_START = 1, ALT_KEYFRAME_UI = 2 };
typedef struct {
    uint32_t generation;
    unsigned requests, connect_failures, bootstrap_mask;
    uint64_t request_due_us, connect_due_us, healthy_since_us, last_ui_us;
    int active, health_seen, health_ready;
} alt_recovery_policy_t;
static inline void alt_recovery_reset(alt_recovery_policy_t *r, uint32_t gen) {
    memset(r, 0, sizeof(*r));
    r->generation = gen;
}
static inline void alt_recovery_fault(alt_recovery_policy_t *r, uint64_t now, int transport) {
    if (!r->active) {
        r->active = 1;
        r->requests = 0;
        r->request_due_us = now;
    }
    r->healthy_since_us = 0;
    r->health_ready = 0;
    if (transport) {
        unsigned shift = r->connect_failures < 2 ? r->connect_failures : 2;
        r->connect_due_us = now + (UINT64_C(250000) << shift);
        if (r->connect_failures < 32)
            ++r->connect_failures;
    }
}
static inline int alt_recovery_request(alt_recovery_policy_t *r, uint32_t gen, unsigned kind,
                                       uint64_t now) {
    if (!gen || gen != r->generation || !now || kind > ALT_KEYFRAME_UI)
        return 0;
    if (kind && !(r->bootstrap_mask & (1u << kind))) {
        r->bootstrap_mask |= 1u << kind;
        if (!r->active) {
            r->active = 1;
            r->request_due_us = now + UINT64_C(2000000);
        }
        if (kind == ALT_KEYFRAME_UI)
            r->last_ui_us = now;
        return 1;
    }
    if (kind == ALT_KEYFRAME_START)
        return 0;
    if (kind == ALT_KEYFRAME_UI && !r->active) {
        if (now < r->last_ui_us || now - r->last_ui_us < UINT64_C(500000))
            return 0;
        r->last_ui_us = now;
        return 1;
    }
    if (!r->active)
        alt_recovery_fault(r, now, 0);
    if (now < r->request_due_us)
        return 0;
    {
        unsigned shift = r->requests < 2 ? r->requests : 2;
        r->request_due_us = now + (UINT64_C(2000000) << shift);
    }
    if (r->requests < 32)
        ++r->requests;
    return 1;
}
static inline void alt_recovery_health(alt_recovery_policy_t *r, uint64_t now, int ready) {
    int previous = r->health_ready;
    r->health_seen = 1;
    r->health_ready = ready ? 1 : 0;
    if (!ready) {
        if (previous)
            alt_recovery_fault(r, now, 0);
        r->healthy_since_us = 0;
        return;
    }
    if (!r->healthy_since_us)
        r->healthy_since_us = now;
    if (now >= r->healthy_since_us && now - r->healthy_since_us >= UINT64_C(1000000)) {
        r->active = 0;
        r->requests = r->connect_failures = 0;
        r->request_due_us = r->connect_due_us = 0;
    }
}
#endif
