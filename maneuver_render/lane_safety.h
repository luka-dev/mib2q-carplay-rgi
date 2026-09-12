#ifndef CR_LANE_SAFETY_H
#define CR_LANE_SAFETY_H
#include <math.h>

/* Eligibility for the schematic connected lane-road renderer. This validates
 * a complete current-event snapshot; it never repairs or infers permissions.
 * Transport must establish completeness/event ownership before setting complete.
 * Unsupported/ambiguous roads keep their ordinary maneuver presentation. */
#define CR_LANE_SAFETY_MAX_LANES 8
#define CR_LANE_SAFETY_MAX_ANGLES 16
typedef struct {
    int position, status, primary, count;
    int angles[CR_LANE_SAFETY_MAX_ANGLES];
} cr_lane_safety_lane_t;
typedef enum {
    CR_LANE_ROAD_OK = 0,
    CR_LANE_ROAD_INCOMPLETE,
    CR_LANE_ROAD_BAD_RECORDS,
    CR_LANE_ROAD_NO_ANCHOR,
    CR_LANE_ROAD_BAD_MANEUVER,
    CR_LANE_ROAD_ACTION_MISMATCH,
    CR_LANE_ROAD_UNSUPPORTED,
    CR_LANE_ROAD_UNCONFIRMED_EXIT
} cr_lane_safety_result_t;

static inline int cr_lane_angle_valid(float a) { return a>=-180 && a<=180; }
static inline float cr_lane_angle_difference(float a,float b) {
    float d=fabsf(a-b);
    return d>180?360-d:d;
}
static inline int cr_lane_allows(const cr_lane_safety_lane_t *lane,float a) {
    if(cr_lane_angle_valid(lane->primary) && cr_lane_angle_difference(lane->primary,a)<5)return 1;
    for(int i=0;i<lane->count;++i)
        if(cr_lane_angle_valid(lane->angles[i]) && cr_lane_angle_difference(lane->angles[i],a)<5)return 1;
    return 0;
}
static inline cr_lane_safety_result_t cr_lane_road_check(
    const cr_lane_safety_lane_t *lanes,int count,int complete,int anchor,
    int supported_maneuver,float maneuver_angle) {
    if(complete!=1)return CR_LANE_ROAD_INCOMPLETE;
    if(!lanes || count<1 || count>CR_LANE_SAFETY_MAX_LANES)return CR_LANE_ROAD_BAD_RECORDS;
    for(int i=0;i<count;++i) {
        const cr_lane_safety_lane_t *l=lanes+i;
        if(l->position!=i || l->count<0 || l->count>CR_LANE_SAFETY_MAX_ANGLES ||
           l->status<0 || l->status>2)return CR_LANE_ROAD_BAD_RECORDS;
    }
    if(anchor<0 || anchor>=count || lanes[anchor].status<1 ||
       !cr_lane_angle_valid(lanes[anchor].primary))return CR_LANE_ROAD_NO_ANCHOR;
    if(!cr_lane_angle_valid(maneuver_angle))return CR_LANE_ROAD_BAD_MANEUVER;
    if(cr_lane_angle_difference(lanes[anchor].primary,maneuver_angle)>=5)
        return CR_LANE_ROAD_ACTION_MISMATCH;
    /* Sharp/U-turn/roundabout actions stay supported by the NORMAL renderer;
     * this simple lane-road surface does not model their connectivity. */
    if(!supported_maneuver || maneuver_angle < -100 || maneuver_angle > 100)
        return CR_LANE_ROAD_UNSUPPORTED;
    int turning=maneuver_angle < -10 || maneuver_angle > 10;
    int first=anchor,last=anchor;
    if(turning) {
        int step=maneuver_angle>0?1:-1;
        for(int i=anchor+step;i>=0 && i<count;i+=step)
            if(!cr_lane_allows(lanes+i,lanes[anchor].primary))return CR_LANE_ROAD_UNCONFIRMED_EXIT;
        while(first>0 && cr_lane_allows(lanes+first-1,lanes[anchor].primary))--first;
        while(last+1<count && cr_lane_allows(lanes+last+1,lanes[anchor].primary))++last;
    }
    /* The remaining surface is an approach/through road. Do not redraw a
     * known turn-only lane outside this bundle as a straight continuation. */
    for(int i=0;i<count;++i)if(!turning || i<first || i>last) {
        int known=cr_lane_angle_valid(lanes[i].primary);
        for(int j=0;j<lanes[i].count;++j)known|=cr_lane_angle_valid(lanes[i].angles[j]);
        if(known && !cr_lane_allows(lanes+i,0))return CR_LANE_ROAD_UNSUPPORTED;
    }
    return CR_LANE_ROAD_OK;
}
#endif
