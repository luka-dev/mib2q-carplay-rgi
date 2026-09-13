#ifndef CR_PREVIEW_FIXTURES_H
#define CR_PREVIEW_FIXTURES_H
#include "../../maneuver_render/scene/lane_model.h"
#include "maneuver.h"
#define CASES 33
static const char *names[CASES]={"Right exit / 4 lanes", "Two recommended / 5 lanes", "Fourth from real left edge", "Right exit / 8 lanes", "Left turn / 5 lanes", "Unknown lane direction", "No recommended lane", "Separated recommendations", "Two left-turn lanes / 5 lanes", "Three left-turn lanes / 5 lanes", "Three left / middle lane primary", "Three left / right lane primary", "Straight + right / shared lane", "Safety: crossing a through lane", "Safety: unknown outer lane", "Safety: incomplete lane snapshot", "Safety: missing lane position", "Safety: different maneuver action",
"No lanes: follow street", "No lanes: right / side streets", "No lanes: sharp left",
"No lanes: sharp right", "No lanes: U-turn left", "No lanes: U-turn right",
"No lanes: full roundabout RHT", "No lanes: full roundabout LHT", "No lanes: highway exit",
"No lanes: merge", "No lanes: lane change", "No lanes: arrived", "No lanes: roundabout departure",
"No maneuver: clear", "No lanes: slight right"};
static void sample(lr_state *s,int c) {
    int i;
    if(c>=18){memset(s,0,sizeof(*s));return;}
    if(c>=13) {
        sample(s,5);
        if(c==13){s->lane[4].primary=0;s->lane[4].angles[0]=0;}
        if(c==14){s->lane[4].primary=1000;s->lane[4].count=0;}
        if(c==15)s->complete=0;
        if(c==16)s->lane[4].position=7;
        lr_normalize(s);return;
    }
    memset(s,0,sizeof(*s));
    s->complete=1; /* Complete ordered synthetic snapshot, not inferred live. */
    s->count=c==0?4:c==2||c==3||c==7?8:5;
    for(i=0;i<s->count;++i) {
        s->lane[i].position=i;s->lane[i].primary=0;s->lane[i].count=1;
    }
    if(c==0 || c==3) {i=s->count-1;s->lane[i].primary=45;s->lane[i].status=2;s->lane[i].angles[0]=45;}
    if(c==1) {s->lane[3].status=1;s->lane[4].status=2;s->lane[3].primary=s->lane[4].primary=45;s->lane[3].count=2;s->lane[3].angles[1]=45;s->lane[4].angles[0]=45;}
    if(c==2) {s->lane[3].status=2;s->lane[4].status=1;}
    if(c==4) {s->lane[0].status=2;s->lane[0].primary=-90;s->lane[0].angles[0]=-90;}
    if(c==5) {
        s->lane[0].primary=1000;s->lane[0].count=0;
        s->lane[1].primary=-1000;s->lane[1].count=0;
        s->lane[3].status=2;s->lane[3].primary=45;s->lane[3].count=2;s->lane[3].angles[1]=45;
        /* The fixture's outer lane shares the exit, but is not recommended.
         * Leaving its default straight-only direction made the selected
         * inner lane appear to turn across a through lane. This is synthetic
         * input, not permission to infer missing directions in real data. */
        s->lane[4].primary=45;s->lane[4].angles[0]=45;
    }
    if(c==7) {s->lane[0].status=s->lane[7].status=2;}
    if(c>=8 && c<=11)for(i=0;i<(c==8?2:3);++i) {
        int primary=c==10?1:c==11?2:0;
        s->lane[i].status=i==primary?2:1;s->lane[i].primary=-90;s->lane[i].angles[0]=-90;
    }
    if(c==12){s->count=4;s->lane[3].status=2;s->lane[3].primary=45;s->lane[3].count=2;s->lane[3].angles[1]=45;}
    lr_normalize(s);
}
static maneuver_state_t sample_maneuver(int c) {
    /* Independent maneuver input: never change it to match a lane record. */
    maneuver_state_t m;memset(&m,0,sizeof(m));m.icon=ICON_TURN;
    if(c==0 || c==1 || c==3 || c==5 || c==12 || (c>=13 && c<=16))m.exit_angle=45;
    if(c==4 || (c>=8 && c<=11) || c==17)m.exit_angle=-90;
    if(c>=18) {
        static const int icons[]={ICON_APPROACH,ICON_TURN,ICON_TURN,ICON_TURN,
            ICON_UTURN,ICON_UTURN,ICON_ROUNDABOUT,ICON_ROUNDABOUT,ICON_EXIT,
            ICON_MERGE,ICON_LANE_CHANGE,ICON_ARRIVED,ICON_ROUNDABOUT_EXIT,ICON_NONE,ICON_TURN};
        static const float angles[]={0,90,-160,160,0,0,135,-135,0,0,0,0,0,0,45};
        m.icon=icons[c-18];m.exit_angle=angles[c-18];m.bap_geometry=1;
        m.driving_side=c==23 || c==25;
        m.direction=c==22 || c==27 || c==28?-1:c==23 || c==26?1:0;
        if(c==19){m.junction_angle_count=2;m.junction_angles[0]=-90;m.junction_angles[1]=0;}
        if(c==24 || c==25) {
            m.junction_angle_count=4;
            m.junction_angles[0]=-90;m.junction_angles[1]=0;m.junction_angles[2]=90;m.junction_angles[3]=m.exit_angle;
        }
    }
    return m;
}


#endif
