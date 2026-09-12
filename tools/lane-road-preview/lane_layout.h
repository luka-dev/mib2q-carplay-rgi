#ifndef LANE_ROAD_LAYOUT_H
#define LANE_ROAD_LAYOUT_H

/* Visual experiment. Raw iAP2 degrees, never BAP direction codes.
 * No vehicle lane is known. Anchor is a representative recommended lane. */
#include <string.h>
#include <math.h>
#include "../../maneuver_render/lane_safety.h"
#define LR_MAX_LANES 8
#define LR_MAX_ANGLES 16
typedef struct {
    int position, status, primary, count;
    int angles[LR_MAX_ANGLES];
} lr_lane;
typedef struct { int count, complete; lr_lane lane[LR_MAX_LANES]; } lr_state;
typedef struct {
    int first, count, anchor, hidden_left, hidden_right;
    int recommended_left, recommended_right;
} lr_window;

/* Ground-plane location with the same screen projection as a raised vertex.
 * Used to test visual lane containment, not just ground-space centerlines. */
static inline int lr_project_ground(const float m[16],float x,float h,float z,float *gx,float *gz) {
    float w=m[3]*x+m[7]*h+m[11]*z+m[15],u,v,a,b,c,d,e,f,det;
    if(!isfinite(w) || fabsf(w)<1e-6f)return 0;
    u=(m[0]*x+m[4]*h+m[8]*z+m[12])/w;
    v=(m[1]*x+m[5]*h+m[9]*z+m[13])/w;
    a=m[0]-u*m[3];b=m[8]-u*m[11];c=u*m[15]-m[12];
    d=m[1]-v*m[3];e=m[9]-v*m[11];f=v*m[15]-m[13];det=a*e-b*d;
    if(!isfinite(det) || fabsf(det)<1e-8f)return 0;
    *gx=(c*e-b*f)/det;*gz=(a*f-c*d)/det;
    return isfinite(*gx) && isfinite(*gz);
}

static inline int lr_valid_angle(int angle) {
    /* Both safety sentinels and every non-angle remain unknown. */
    return angle >= -180 && angle <= 180;
}
static inline int lr_difference(int a, int b) {
    int d = a > b ? a-b : b-a;
    return d > 180 ? 360-d : d;
}
static inline int lr_has_direction(const lr_lane *lane,int angle) {
    int i;
    if(!lr_valid_angle(angle))return 0;
    if(lr_valid_angle(lane->primary) && lr_difference(lane->primary,angle)<5)return 1;
    for(i=0;i<lane->count && i<LR_MAX_ANGLES;++i)
        if(lr_valid_angle(lane->angles[i]) && lr_difference(lane->angles[i],angle)<5)return 1;
    return 0;
}
/* Schematic continuation of contiguous incoming lanes with the SAME direction;
 * not a claim about the destination street's surveyed lane count. */
static inline int lr_exit_group(const lr_state *s,int anchor,int *first,int *last) {
    int a;
    if(anchor<0 || anchor>=s->count || s->count>LR_MAX_LANES)return 0;
    a=s->lane[anchor].primary;
    if(!lr_valid_angle(a) || (a>=-10 && a<=10))return 0;
    *first=*last=anchor;
    while(*first>0 && s->lane[*first].position==s->lane[*first-1].position+1 &&
          lr_has_direction(&s->lane[*first-1],a))--*first;
    while(*last+1<s->count && s->lane[*last+1].position==s->lane[*last].position+1 &&
          lr_has_direction(&s->lane[*last+1],a))++*last;
    return *last-*first+1;
}
static inline void lr_normalize(lr_state *s) {
    int i, j, k;
    if(s->count<1 || s->count>LR_MAX_LANES)s->complete=0;
    if (s->count < 0) s->count=0;
    if (s->count > LR_MAX_LANES) s->count=LR_MAX_LANES;
    for (i=0; i<s->count; ++i) {
        lr_lane *l=&s->lane[i];
        int n=0, count=l->count;
        if(count<0 || count>LR_MAX_ANGLES)s->complete=0;
        if (l->status<0 || l->status>2) l->status=0;
        if (!lr_valid_angle(l->primary)) l->primary=1000;
        if (count>LR_MAX_ANGLES) count=LR_MAX_ANGLES;
        for (j=0; j<count; ++j) {
            int a=l->angles[j], duplicate=0;
            if (!lr_valid_angle(a)) continue;
            for (k=0; k<n; ++k) if (lr_difference(a,l->angles[k])<5) duplicate=1;
            if (!duplicate) l->angles[n++]=a;
        }
        l->count=n;
        /* Unknown lanes retain their slot. Never collapse the road topology. */
    }
    for (i=1; i<s->count; ++i) {
        lr_lane v=s->lane[i];
        for (j=i; j>0 && s->lane[j-1].position>v.position; --j) s->lane[j]=s->lane[j-1];
        s->lane[j]=v;
    }
}
static inline cr_lane_safety_result_t lr_geometry_check(const lr_state *s,int anchor,
                                                       int supported,float angle) {
    cr_lane_safety_lane_t lanes[LR_MAX_LANES];
    if(s->count<1 || s->count>LR_MAX_LANES)
        return cr_lane_road_check(NULL,s->count,s->complete,anchor,supported,angle);
    for(int i=0;i<s->count;++i) {
        const lr_lane *src=s->lane+i;cr_lane_safety_lane_t *dst=lanes+i;
        dst->position=src->position;dst->status=src->status;
        dst->primary=src->primary;dst->count=src->count;
        memcpy(dst->angles,src->angles,sizeof(dst->angles));
    }
    return cr_lane_road_check(lanes,s->count,s->complete,anchor,supported,angle);
}
static inline lr_window lr_choose(const lr_state *s, int capacity, int previous_position) {
    lr_window w;
    int i, best_status=0;
    memset(&w,0,sizeof(w)); w.anchor=-1;
    if (s->count<=0 || s->count>LR_MAX_LANES) return w;
    if (capacity<1) capacity=1;
    if (capacity>LR_MAX_LANES) capacity=LR_MAX_LANES;
    w.count=s->count<capacity?s->count:capacity;
    for (i=0;i<s->count;++i)
        if (lr_valid_angle(s->lane[i].primary) && s->lane[i].status>best_status)
            best_status=s->lane[i].status;
    if (best_status) {
        int nearest=1000;
        for (i=0;i<s->count;++i) if (s->lane[i].status==best_status && lr_valid_angle(s->lane[i].primary)) {
            int d=2*i-(s->count-1); if(d<0)d=-d;
            if(s->lane[i].position==previous_position) d=-1;
            if(d<nearest) {nearest=d;w.anchor=i;}
        }
    }
    /* Capacity is a preference, not permission to hide both physical edges.
     * Retain EVERY lane between primary and the nearest real road edge.
     * Rendering may pack secondary lanes more tightly; never omit one in
     * the countable corridor. With <=8 inputs that corridor needs <=4 lanes. */
    if(w.anchor>=0) {
        if(w.anchor<=s->count-1-w.anchor) {
            if(w.count<w.anchor+1)w.count=w.anchor+1;
            w.first=0;
        } else {
            if(w.count<s->count-w.anchor)w.count=s->count-w.anchor;
            w.first=s->count-w.count;
        }
    }
    w.hidden_left=w.first;
    w.hidden_right=s->count-w.first-w.count;
    for(i=0;i<w.first;++i) if(s->lane[i].status>0)w.recommended_left=1;
    for(i=w.first+w.count;i<s->count;++i)if(s->lane[i].status>0)w.recommended_right=1;
    return w;
}
#endif
