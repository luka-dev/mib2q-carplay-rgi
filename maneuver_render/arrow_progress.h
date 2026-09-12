#ifndef CR_ARROW_PROGRESS_H
#define CR_ARROW_PROGRESS_H
#include <math.h>
/* The wire clock owns blink. Native animation only softens received targets. */
#define CR_PROGRESS_FLAG 0x20
#define CR_PROGRESS_OFF 0
#define CR_PROGRESS_FILL 1
#define CR_PROGRESS_BLINK_LOW 2
#define CR_PROGRESS_BLINK_HIGH 3
#define CR_PROGRESS_RETRACT_SECONDS .40f

typedef struct {
    int state, initialized, active;
    float fill, velocity, target; /* velocity is route lengths per second */
    double last_time;
    /* Brightness = path_weight * spatial_fill + glow. Two palette endpoints:
     * base and filled blue. Blink low/off share exactly zero brightness. */
    float path_weight, glow;
    float path_from, glow_from, path_target, glow_target;
    double tint_started, tint_last_time;
    float tint_duration;
    int tint_active;
    /* A route handoff retracts the OLD front. Incoming packets remain queued
     * until the engine commits the new maneuver, then fill rises from zero. */
    int handoff, handoff_released, incoming_level, incoming_state;
    float retract_from;
    double retract_started;
} cr_arrow_progress_t;

static inline int cr_progress_decode(int flags, int state, int mode) {
    /* Old level-only senders can fill the arrow. Their mode 2 carries no
     * phase: keep the quiet arrow instead of inventing a second blink clock. */
    if (!(flags & CR_PROGRESS_FLAG))
        return mode == 1 ? CR_PROGRESS_FILL : CR_PROGRESS_OFF;
    if (state > CR_PROGRESS_BLINK_HIGH || state < 0)
        return CR_PROGRESS_OFF;
    if ((state == CR_PROGRESS_OFF && mode != 0) || (state != CR_PROGRESS_OFF && mode != 1))
        return CR_PROGRESS_OFF;
    return state;
}

static inline void cr_progress_reset(cr_arrow_progress_t *p) {
    p->state = CR_PROGRESS_OFF;
    p->initialized = p->active = 0;
    p->fill = p->velocity = p->target = 0;
    p->last_time = 0;
    p->path_weight = p->glow = p->path_from = p->glow_from = 0;
    p->path_target = p->glow_target = 0;
    p->tint_started = p->tint_last_time = 0;
    p->tint_duration = 0;
    p->tint_active = 0;
    p->handoff = p->handoff_released = 0;
    p->incoming_level = 16; p->incoming_state = CR_PROGRESS_OFF;
    p->retract_from = 0; p->retract_started = 0;
}

static inline int cr_progress_tick_fill(cr_arrow_progress_t *p, double now) {
    if (!p->active || now <= p->last_time) return 0;
    double elapsed = now - p->last_time;
    p->last_time = now;
    if (elapsed >= 1.0) {
        /* Already visually settled after a long render stall. Also avoids
         * multiplying an unbounded dt by a vanishing exponential. */
        p->fill = p->target;
        p->velocity = 0;
        p->active = 0;
        return 1;
    }

    /* Exact critically damped motion for this fixed-target interval.
     * New packets only replace target: position AND velocity survive retargets.
     * Unlike restarting an ease curve, a stream of targets keeps moving.
     * omega=16: ~300 ms to cover 95% of a step; steady input lags ~125 ms.
     * Analytic integration keeps the response independent of frame rate. */
    const float omega = 16.0f;
    float dt = (float)elapsed;
    float offset = p->fill - p->target;
    float decay = expf(-omega * dt);
    float tangent = p->velocity + omega * offset;
    float next = p->target + (offset + tangent * dt) * decay;
    float velocity = (p->velocity - omega * tangent * dt) * decay;

    /* A target moved inside the stopping distance must not be overshot.
     * Reversal may initially brake in the old direction, within route bounds. */
    if ((offset <= 0 && next >= p->target) ||
        (offset >= 0 && next <= p->target) ||
        (fabsf(next - p->target) <= 0.0005f && fabsf(velocity) <= 0.005f)) {
        p->fill = p->target;
        p->velocity = 0;
        p->active = 0;
    } else {
        p->fill = next;
        p->velocity = velocity;
        if (p->fill < 0) { p->fill = 0; p->velocity = 0; }
        if (p->fill > 1) { p->fill = 1; p->velocity = 0; }
    }
    return 1;
}

static inline void cr_progress_set(cr_arrow_progress_t *p, int level, int state, double now);

static inline int cr_progress_tick(cr_arrow_progress_t *p, double now) {
    int changed = 0;
    if(p->handoff) {
        if(now>p->last_time) {
            float t=(float)((now-p->retract_started)/CR_PROGRESS_RETRACT_SECONDS);
            if(t>1)t=1;
            float fill=p->retract_from*(1-t*t*(3-2*t));
            changed=fill!=p->fill;
            p->fill=fill;
            p->last_time=now;
        }
    } else changed=cr_progress_tick_fill(p,now);
    if (p->tint_active && now > p->tint_last_time) {
        float t = (float)((now-p->tint_started)/p->tint_duration);
        p->tint_last_time = now;
        if (t >= 1) {
            p->path_weight = p->path_target;
            p->glow = p->glow_target;
            p->tint_active = 0;
        } else {
            float eased = t*t*(3-2*t);
            p->path_weight = p->path_from+(p->path_target-p->path_from)*eased;
            p->glow = p->glow_from+(p->glow_target-p->glow_from)*eased;
        }
        changed = 1;
    }
    if(p->handoff && p->handoff_released && p->fill==0) {
        int level=p->incoming_level,state=p->incoming_state;
        p->handoff=p->handoff_released=0;
        /* Seed the normal velocity-preserving filter at zero, including when
         * coming from blink/off. Its ordinary first-packet snap is unsuitable
         * here: the new route is already visible at this exact color. */
        p->state=CR_PROGRESS_FILL;p->initialized=1;p->active=0;
        p->target=p->velocity=0;p->last_time=now;
        cr_progress_set(p,level,state,now);
        changed=1;
    }
    return changed;
}

/* New geometry gets its own fill target and no old velocity. Keep the visible
 * tint so its change starts with route motion, without a post-settle reveal. */
static inline void cr_progress_begin_maneuver(cr_arrow_progress_t *p, double now) {
    cr_progress_tick(p,now);
    p->initialized = p->active = 0;
    p->velocity = 0;
}

static inline void cr_progress_tint_weights(cr_arrow_progress_t *p, float path,
                                           float glow, float duration, double now) {
    if (path == p->path_target && glow == p->glow_target) return;
    /* Preserve the visible mixture when a phase changes mid-animation.
     * Convex interpolation keeps path_weight + glow <= 1 at every frame. */
    p->path_from = p->path_weight; p->glow_from = p->glow;
    p->path_target = path; p->glow_target = glow;
    p->tint_started = p->tint_last_time = now;
    p->tint_duration = duration;
    p->tint_active = p->path_weight != path || p->glow != glow;
}
static inline void cr_progress_tint_target(cr_arrow_progress_t *p, int state, double now) {
    cr_progress_tint_weights(p,state==CR_PROGRESS_FILL?1.0f:0.0f,
                            state==CR_PROGRESS_BLINK_HIGH?1.0f:0.0f,
                            state==CR_PROGRESS_BLINK_HIGH?.100f:.150f,now);
}

/* Call only when route motion actually starts, not when a later maneuver is
 * queued. Keep spatial brightness constant so its boundary visibly retreats;
 * any old blink glow fades during the departure, independently of the front. */
static inline void cr_progress_begin_handoff(cr_arrow_progress_t *p,double now) {
    if(p->handoff)return;
    cr_progress_begin_maneuver(p,now);
    p->handoff=1;p->handoff_released=0;
    p->incoming_level=16;p->incoming_state=CR_PROGRESS_OFF;
    p->retract_from=p->fill;p->retract_started=p->last_time=now;p->target=0;
    cr_progress_tint_weights(p,p->path_weight,0,.150f,now);
}
static inline void cr_progress_finish_handoff(cr_arrow_progress_t *p,double now) {
    if(!p->handoff)return;
    p->handoff_released=1;
    /* A very short/interrupted push still completes the retreat continuously.
     * Normal pushes have already reached zero and release immediately. */
    cr_progress_tick(p,now);
}

static inline void cr_progress_set(cr_arrow_progress_t *p, int level, int state, double now) {
    cr_progress_tick(p, now); /* reach the packet timestamp using the OLD target */
    if (level < 0) level = 0;
    if (level > 16) level = 16;
    if(p->handoff) {
        p->incoming_level=level;p->incoming_state=state;
        return;
    }
    float target = 1.0f - (float)level / 16.0f;
    int previous = p->state;
    p->state = state;
    cr_progress_tint_target(p,state,now);
    if (state != CR_PROGRESS_FILL) {
        p->active = p->initialized = 0;
        p->velocity = 0; /* off/blink/maneuver reset never inherit fill momentum */
        return;
    }
    if (!p->initialized || previous != CR_PROGRESS_FILL) {
        /* If a previous spatial fill is still fading out, preserve its
         * visible front when returning from off/blink before the fade ends. */
        if (p->path_weight == 0) p->fill = target;
        p->target = target;
        p->velocity = 0;
        p->last_time = now;
        p->initialized = 1;
        p->active = p->fill != target;
        return;
    }
    if (target == p->target) return; /* repeats never reset motion or its clock */
    if (!p->active && now > p->last_time) p->last_time = now;
    p->target = target;
    p->active = 1;
}
#endif
