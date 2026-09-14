#include "engine.h"
#include <string.h>
#define HOLD_MS 900
#define FADE_MS 350

void cursor_engine_reset(cursor_engine *e) { memset(e, 0, sizeof(*e)); }
static double fade(uint64_t elapsed) {
    if (elapsed >= FADE_MS) return 0;
    double t = (double)elapsed / FADE_MS;
    return 1 - t*t*(3-2*t);
}
static unsigned opacity(const cursor_engine *e, uint64_t now) {
    uint64_t elapsed = now >= e->activity_at ? now - e->activity_at : 0;
    if (!e->shown || elapsed >= HOLD_MS + FADE_MS) return 0;
    unsigned a = elapsed <= HOLD_MS ? 255 : (unsigned)(255*fade(elapsed-HOLD_MS)+.5);
    if (e->dismissing) {
        unsigned graceful = (unsigned)(e->dismiss_alpha*fade(now-e->dismiss_at)+.5);
        if (graceful < a) a = graceful;
    }
    return a;
}
static void motion_step(cursor_engine *e, uint64_t now) {
    uint64_t previous=e->stepped, hold_until=e->motion_at+90;
    if (now<=previous) return;
    // Continue between input packets, then settle to neutral without more input.
    if (previous<hold_until) {
        uint64_t end=now<hold_until ? now : hold_until;
        double k=1-exp(-(double)(end-previous)/75.0);
        e->motion_x+=(e->target_x-e->motion_x)*k;
        e->motion_y+=(e->target_y-e->motion_y)*k;
        previous=end;
    }
    if (now>previous) {
        double k=exp(-(double)(now-previous)/140.0);
        e->motion_x*=k; e->motion_y*=k;
    }
    e->stepped=now;
}
void cursor_engine_frame(cursor_engine *e, uint64_t now, cursor_frame *f) {
    *f = e->command;
    if (!e->initialized) { f->mode = CURSOR_HIDE; f->alpha = 0; return; }
    if (f->mode == CURSOR_LOADER) {
        handoff_step(&e->handoff, now > e->stepped ? (now-e->stepped)/1000.0 : 0);
        e->stepped = now;
        unsigned elapsed = (unsigned)(now-e->started);
        f->x = elapsed & 65535; f->y = elapsed >> 16;
        f->vx = f->vy = 0; f->vw = f->width; f->vh = f->height;
        f->alpha = (unsigned)(255 * (1-e->handoff.position) + .5);
        if (e->handoff.target && !f->alpha) e->done = 1;
        if (e->done) f->alpha = 0;
    } else {
        motion_step(e, now);
        // Wrapped native phase keeps GLES mediump precision stable on long sessions.
        // Input/heartbeat updates do not restart the travelling light.
        f->light_phase=(float)((now-e->started)%6000)*(6.28318530718f/6000.0f);
        f->alpha=opacity(e, now); f->motion_x=e->motion_x; f->motion_y=e->motion_y;
    }
}
void cursor_engine_apply(cursor_engine *e, const cursor_frame *c, uint64_t now) {
    cursor_frame ignored;
    cursor_engine_frame(e, now, &ignored); // Advance old target before retargeting.
    int changed = !e->initialized || e->command.mode != c->mode
        || e->command.width != c->width || e->command.height != c->height
        || e->command.vx != c->vx || e->command.vy != c->vy
        || e->command.vw != c->vw || e->command.vh != c->vh;
    if (changed) { cursor_engine_reset(e); e->started = e->stepped = now; }
    if (c->mode == CURSOR_LOADER) {
        if (!changed && e->command.revision != c->revision) {
            /* A new video configuration invalidates any old aperture. */
            memset(&e->handoff, 0, sizeof(e->handoff)); e->done = 0;
        }
        e->handoff.target = c->flags != 0;
        if (!c->flags) e->done = 0;
    } else {
        if (c->flags == CURSOR_INPUT_FORCE_HIDE) {
            e->shown=0; e->dismissing=0;
            e->motion_x=e->motion_y=e->target_x=e->target_y=0;
        } else if (c->flags == CURSOR_INPUT_DISMISS) {
            if (!e->dismissing) {
                e->dismiss_alpha=opacity(e,now); e->dismiss_at=now; e->dismissing=1;
            }
        } else {
            if (c->activity != e->activity) {
                double dt=fmax(12.0,fmin(200.0,(double)(now-e->motion_at)));
                double vx=changed ? 0 : ((double)c->x-e->command.x)*1000.0/dt/900.0;
                double vy=changed ? 0 : ((double)c->y-e->command.y)*1000.0/dt/900.0;
                double limit=fmax(1.0,sqrt(vx*vx+vy*vy));
                e->target_x=vx/limit; e->target_y=vy/limit; e->motion_at=now;
                e->shown=1; e->activity_at=now; e->dismissing=0;
            }
            /* A late finger-up must not bring back an already faded cursor. */
            if (e->touching && c->flags == CURSOR_INPUT_RELEASED && opacity(e, now))
                e->activity_at = now;
        }
        e->activity = c->activity;
        e->touching = c->flags == CURSOR_INPUT_TOUCH;
    }
    e->command = *c; e->initialized = 1;
}
unsigned cursor_engine_click_ttl(const cursor_engine *e, uint64_t now) {
    if (!e->initialized || e->command.mode != CURSOR_SHOW
            || e->command.flags >= CURSOR_INPUT_DISMISS || !opacity(e, now)) return 0;
    /* End before alpha rounds to zero. The caller also bounds this by its lease. */
    uint64_t end = e->activity_at + HOLD_MS + FADE_MS - 15;
    return now >= end ? 0 : end-now > CURSOR_LEASE_MS ? CURSOR_LEASE_MS : (unsigned)(end-now);
}
