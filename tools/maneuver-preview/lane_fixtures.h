#ifndef CR_PREVIEW_LANE_FIXTURES_H
#define CR_PREVIEW_LANE_FIXTURES_H
#include "lane_panel.h"
static const char *lane_names[]={
    "Right exit / 8 lanes", "Left turn / 8 lanes", "Fourth from left / 8 lanes",
    "Two best lanes / 6 lanes", "Straight + right / shared lane",
    "Three directions / one lane", "Unknowns preserved", "U-turn / 3 lanes",
    "Sharp turn / 4 lanes", "Roundabout / independent guidance", "Hidden guidance"
};
static const int lane_maneuvers[]={14,17,15,14,14,15,14,4,20,6,14};
#define LANE_CASES ((int)(sizeof(lane_names)/sizeof(lane_names[0])))
static cr_lane_guidance_t sample_lanes(int c) {
    cr_lane_guidance_t g={0};g.event_index=73+c;g.showing=1;g.complete=1;
    g.count=c<3?8:c==3?6:c==5?1:c==8?4:3;
    for(int i=0;i<g.count;++i) {
        cr_lane_record_t *l=&g.lanes[i];l->position=i;l->primary=0;l->angle_count=1;l->angles[0]=0;
    }
    int best=c==1?0:c==2?3:g.count-1;
    int angle=c==1?-90:c==2 || c==5?0:c==7?-180:c==8?135:45;
    g.lanes[best].primary=angle;g.lanes[best].angles[0]=angle;g.lanes[best].status=2;
    if(c==3) {g.lanes[best-1].primary=45;g.lanes[best-1].angles[0]=45;g.lanes[best-1].status=2;}
    if(c==4) {g.lanes[best].angle_count=2;g.lanes[best].angles[1]=0;}
    if(c==5) {g.lanes[0].angle_count=3;g.lanes[0].angles[1]=-90;g.lanes[0].angles[2]=90;}
    if(c==6) {
        g.complete=0;g.lanes[0].primary=1000;g.lanes[0].angles[0]=-1000;
        g.lanes[1].primary=-1000;g.lanes[1].angles[0]=45;g.lanes[1].status=2;
        g.lanes[2].status=1;
    }
    if(c==10)g.showing=0;
    return g;
}
#endif
