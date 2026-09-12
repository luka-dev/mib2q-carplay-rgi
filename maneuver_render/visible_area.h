#ifndef CR_VISIBLE_AREA_H
#define CR_VISIBLE_AREA_H

#include "protocol.h"

/* Visible-area animation and clipping helpers shared with the CPU regression tests.
 * Coordinates are source pixels, top-left origin, independent of Retina/SSAA. */
typedef struct { float x, y, w, h; } cr_rect_t;
typedef struct {
    cr_rect_t from, current, target;
    double started;
    int active;
} cr_rect_animation_t;

static inline int cr_rect_equal(cr_rect_t a, cr_rect_t b) {
    return a.x==b.x && a.y==b.y && a.w==b.w && a.h==b.h;
}

static inline void cr_rect_animate(cr_rect_animation_t *a, double now) {
    float t,e;
    if (!a->active) return;
    t=(float)((now-a->started)/0.250);
    if (t>=1) { a->current=a->target; a->active=0; return; }
    if (t<0) t=0;
    e=t*t*t*(t*(t*6-15)+10); /* zero velocity/acceleration at either end */
    a->current.x=a->from.x+(a->target.x-a->from.x)*e;
    a->current.y=a->from.y+(a->target.y-a->from.y)*e;
    a->current.w=a->from.w+(a->target.w-a->from.w)*e;
    a->current.h=a->from.h+(a->target.h-a->from.h)*e;
}

static inline int cr_rect_retarget(cr_rect_animation_t *a, cr_rect_t target, double now) {
    cr_rect_animate(a,now);
    if (cr_rect_equal(target,a->target)) return 0;
    a->from=a->current; a->target=target; a->started=now; a->active=1;
    return 1;
}
static inline cr_rect_t cr_visible_area(int x, int y, int w, int h) {
    cr_rect_t r;
    if (x < 0 || y < 0 || x >= CR_DEFAULT_WIDTH || y >= CR_DEFAULT_HEIGHT-1 || w <= 0 || h <= 0) {
        x=CR_POPUP_X; y=CR_POPUP_Y; w=CR_POPUP_W; h=CR_POPUP_H;
    }
    if (w > CR_DEFAULT_WIDTH-x) w=CR_DEFAULT_WIDTH-x;
    if (h > CR_DEFAULT_HEIGHT-1-y) h=CR_DEFAULT_HEIGHT-1-y;
    r.x=(float)x; r.y=(float)y; r.w=(float)w; r.h=(float)h;
    return r;
}

#endif
