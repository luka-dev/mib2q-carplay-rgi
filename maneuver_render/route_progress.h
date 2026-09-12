#ifndef CR_ROUTE_PROGRESS_H
#define CR_ROUTE_PROGRESS_H
#include <math.h>
#include "visible_area.h"

#define CR_ROUTE_PROGRESS_POINTS 481
#define CR_ROUTE_FEATHER 0.055f

typedef struct { float d, x, y, z; } cr_route_progress_point_t;
typedef struct {
    int count;
    float d[CR_ROUTE_PROGRESS_POINTS], w[CR_ROUTE_PROGRESS_POINTS];
    float s[CR_ROUTE_PROGRESS_POINTS]; /* projected cumulative length, source px */
    float start, length; /* first to last visible centerline point */
} cr_route_progress_map_t;

/* Liang-Barsky interval, including the lower edge of the actual VC crop. */
static inline int cr_route_clip_edge(float p, float q, float *lo, float *hi) {
    if (fabsf(p) < 1e-7f) return q >= 0;
    float t = q / p;
    if (p < 0) { if (t > *hi) return 0; if (t > *lo) *lo = t; }
    else { if (t < *lo) return 0; if (t < *hi) *hi = t; }
    return 1;
}

static inline int cr_route_progress_project(cr_route_progress_map_t *out,
        const cr_route_progress_point_t *points, int count, const float mvp[16], cr_rect_t crop) {
    float previous_x = 0, previous_y = 0, first = -1, last = -1;
    int i;
    out->count = 0;
    if (count < 2 || count > CR_ROUTE_PROGRESS_POINTS) return 0;
    for (i = 0; i < count; i++) {
        const cr_route_progress_point_t *p = &points[i];
        float w = mvp[3]*p->x + mvp[7]*p->y + mvp[11]*p->z + mvp[15];
        if (!isfinite(w) || w <= 1e-5f || (i && p->d <= points[i-1].d)) return 0;
        float x = (1 + (mvp[0]*p->x + mvp[4]*p->y + mvp[8]*p->z + mvp[12])/w) * CR_DEFAULT_WIDTH*.5f;
        float y = (1 - (mvp[1]*p->x + mvp[5]*p->y + mvp[9]*p->z + mvp[13])/w) * CR_DEFAULT_HEIGHT*.5f;
        if (!isfinite(x) || !isfinite(y)) return 0;
        out->d[i] = p->d; out->w[i] = w;
        out->s[i] = 0;
        if (i) {
            float dx = x - previous_x, dy = y - previous_y;
            float length = sqrtf(dx*dx + dy*dy), lo = 0, hi = 1;
            out->s[i] = out->s[i-1] + length;
            if (length > 1e-6f &&
                cr_route_clip_edge(-dx, previous_x-crop.x, &lo, &hi) &&
                cr_route_clip_edge(dx, crop.x+crop.w-previous_x, &lo, &hi) &&
                cr_route_clip_edge(-dy, previous_y-crop.y, &lo, &hi) &&
                cr_route_clip_edge(dy, crop.y+crop.h-previous_y, &lo, &hi) && hi > lo) {
                if (first < 0) first = out->s[i-1] + lo*length;
                last = out->s[i-1] + hi*length;
            }
        }
        previous_x = x; previous_y = y;
    }
    if (first < 0 || last-first < 1e-4f) return 0;
    out->start = first; out->length = last-first; out->count = count;
    return 1;
}

/* Keep traversal identity at crossings; never choose a nearest screen point.
 * Fraction along a world segment must be converted to its projected fraction. */
static inline float cr_route_progress_at(const cr_route_progress_map_t *p, float d) {
    int lo = 0, hi = p->count-1;
    if (hi < 1) return 0;
    while (hi-lo > 1) {
        int mid = (lo+hi)/2;
        if (d < p->d[mid]) hi = mid; else lo = mid;
    }
    float t = (d-p->d[lo])/(p->d[hi]-p->d[lo]);
    float w = (1-t)*p->w[lo] + t*p->w[hi];
    float screen_t = fabsf(w) > 1e-6f ? t*p->w[hi]/w : t;
    float s = p->s[lo] + screen_t*(p->s[hi]-p->s[lo]);
    return (s-p->start)/p->length; /* negatives retain the hidden entry extension */
}

/* Equal amounts of visible centerline fill, including the first/last step.
 * Move the feather completely behind the crop at zero, rather than switching
 * all negative path coordinates on at the first nonzero packet. For interior
 * values the feather center is exactly fill. Near endpoints invert the area
 * under smoothstep: integral = 2*h*(a^3 - a^4/2). */
static inline float cr_route_progress_front(float fill) {
    const float h = CR_ROUTE_FEATHER;
    if (fill <= 0) return -h;
    if (fill >= 1) return 1+h;
    if (fill >= h && fill <= 1-h) return fill;
    float area = fill < h ? fill : 1-fill;
    float lo = 0, hi = 1;
    int i;
    for (i = 0; i < 16; i++) {
        float a = (lo+hi)*.5f;
        float integral = h*a*a*a*(2-a);
        if (integral < area) lo = a; else hi = a;
    }
    float front = h*(lo+hi)-h;
    return fill < h ? front : 1-front;
}
#endif
