#ifndef CR_BARGRAPH_LAYOUT_H
#define CR_BARGRAPH_LAYOUT_H

#include <math.h>
#include "protocol.h"

/* Pure layout/projection helpers shared with the CPU regression tests.
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
typedef struct {
    cr_rect_t visible, bars[2]; /* 0=left, 1=right */
    float bar_h, gap, cost[2];
} cr_bargraph_layout_t;

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

static inline float cr_rect_overlap(cr_rect_t a, cr_rect_t b) {
    float w=fminf(a.x+a.w,b.x+b.w)-fmaxf(a.x,b.x);
    float h=fminf(a.y+a.h,b.y+b.h)-fmaxf(a.y,b.y);
    return w>0 && h>0 ? w*h : 0;
}

static inline void cr_bargraph_begin(cr_bargraph_layout_t *p, cr_rect_t visible) {
    float margin=fminf(10.0f,fminf(visible.w,visible.h)*0.12f);
    float height=visible.h-2*margin;
    float scale=height/(16*7.0f+15*2.0f);
    float width=fminf(14*scale,visible.w-2*margin);
    p->visible=visible;
    p->bar_h=7*scale; p->gap=2*scale;
    p->bars[0].x=visible.x+margin;
    p->bars[1].x=visible.x+visible.w-margin-width;
    p->bars[0].y=p->bars[1].y=visible.y+margin;
    p->bars[0].w=p->bars[1].w=width;
    p->bars[0].h=p->bars[1].h=height;
    p->cost[0]=p->cost[1]=0;
}

/* Only the exit arrow prism is submitted here. The route shaft, roundabout
 * ring, bulb and grey side streets never trigger an edge change. */
static inline cr_rect_t cr_bargraph_avoid(const cr_bargraph_layout_t *p, int side) {
    cr_rect_t avoid=p->bars[side];
    float right=fminf(avoid.x+avoid.w+3,p->visible.x+p->visible.w);
    float bottom=fminf(avoid.y+avoid.h+3,p->visible.y+p->visible.h);
    avoid.x=fmaxf(avoid.x-3,p->visible.x);
    avoid.y=fmaxf(avoid.y-3,p->visible.y);
    avoid.w=right-avoid.x; avoid.h=bottom-avoid.y;
    return avoid;
}

/* Clip the projected triangle to the bar's footprint (including shadow).
 * A diagonal tip's bounding box alone can overlap without the tip touching. */
static inline float cr_triangle_overlap(const float triangle[3][2], cr_rect_t r) {
    float poly[2][8][2];
    float edges[4]={r.x,r.x+r.w,r.y,r.y+r.h};
    int n=3,src=0,edge,i;
    for (i=0;i<3;i++) { poly[0][i][0]=triangle[i][0]; poly[0][i][1]=triangle[i][1]; }
    for (edge=0;edge<4 && n;edge++) {
        int dst=1-src,count=0,axis=edge/2;
        float sign=(edge%2)?-1.0f:1.0f;
        for (i=0;i<n;i++) {
            const float *a=poly[src][i], *b=poly[src][(i+1)%n];
            float da=sign*(a[axis]-edges[edge]),db=sign*(b[axis]-edges[edge]);
            if ((da>=0)!=(db>=0)) {
                float t=da/(da-db);
                poly[dst][count][0]=a[0]+t*(b[0]-a[0]);
                poly[dst][count++][1]=a[1]+t*(b[1]-a[1]);
            }
            if (db>=0) {
                poly[dst][count][0]=b[0]; poly[dst][count++][1]=b[1];
            }
        }
        n=count; src=dst;
    }
    float area=0;
    for (i=1;i+1<n;i++) {
        const float *a=poly[src][0], *b=poly[src][i], *c=poly[src][i+1];
        area+=(b[0]-a[0])*(c[1]-a[1])-(b[1]-a[1])*(c[0]-a[0]);
    }
    return fabsf(area)*0.5f;
}

static inline void cr_bargraph_mesh(cr_bargraph_layout_t *p, const float *mvp,
                                    const float *verts, int count) {
    int i,j;
    for (i=0;i+2<count;i+=3) {
        float triangle[3][2];
        int valid=1;
        for (j=0;j<3;j++) {
            const float *v=verts+(i+j)*6;
            float w=mvp[3]*v[0]+mvp[7]*v[1]+mvp[11]*v[2]+mvp[15];
            float x,y;
            if (!(w>0.00001f)) { valid=0; break; }
            x=(mvp[0]*v[0]+mvp[4]*v[1]+mvp[8]*v[2]+mvp[12])/w;
            y=(mvp[1]*v[0]+mvp[5]*v[1]+mvp[9]*v[2]+mvp[13])/w;
            x=(x+1)*0.5f*CR_DEFAULT_WIDTH;
            y=(1-y)*0.5f*CR_DEFAULT_HEIGHT;
            if (!isfinite(x) || !isfinite(y)) { valid=0; break; }
            triangle[j][0]=x; triangle[j][1]=y;
        }
        if (valid) {
            for (j=0;j<2;j++)
                p->cost[j]+=cr_triangle_overlap(triangle,cr_bargraph_avoid(p,j));
        }
    }
}

/* One decision per maneuver/layout event, independent of the previous side. */
static inline int cr_bargraph_side(const cr_bargraph_layout_t *p) {
    return p->cost[1]>0 && p->cost[0]<p->cost[1] ? 0 : 1;
}

#endif
