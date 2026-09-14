/* Shared critically damped handoff. Retargeting preserves velocity. */
#ifndef PRISM_HANDOFF_H
#define PRISM_HANDOFF_H
#include <math.h>
typedef struct { double position, velocity; int target; } handoff_t;
static inline void handoff_step(handoff_t *h, double dt) {
    if (dt <= 0) return;
    const double speed=5.4;
    double delta=h->position-h->target;
    double c=h->velocity+speed*delta, decay=exp(-speed*dt);
    h->position=h->target+(delta+c*dt)*decay;
    h->velocity=(h->velocity-speed*c*dt)*decay;
    if (h->position<0) { h->position=0; h->velocity=0; }
    if (h->position>1) { h->position=1; h->velocity=0; }
    if (fabs(h->position-h->target)<0.00001 && fabs(h->velocity)<0.0001) {
        h->position=h->target; h->velocity=0;
    }
}
static inline handoff_t handoff_demo(double t) {
    handoff_t h={0,0,0};
    if(t>3.4) { h.target=1; handoff_step(&h,fmin(t,7.0)-3.4); }
    if(t>=7.0) { h.target=0; handoff_step(&h,t-7.0); }
    return h;
}
#endif
