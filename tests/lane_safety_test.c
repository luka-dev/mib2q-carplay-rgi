#include "../maneuver_render/lane_safety.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    cr_lane_safety_lane_t lanes[8],before[8];int checked=0;
    const int angles[]={-90,-45,45,90};
    for(int n=1;n<=8;++n)for(int anchor=0;anchor<n;++anchor)
        for(int a=0;a<4;++a) {
            int angle=angles[a],step=angle>0?1:-1;
            memset(lanes,0,sizeof(lanes));
            for(int i=0;i<n;++i) {
                lanes[i].position=i;lanes[i].count=1;
                if((i-anchor)*step>=0)lanes[i].primary=lanes[i].angles[0]=angle;
            }
            lanes[anchor].status=2;
            memcpy(before,lanes,sizeof(lanes));
            assert(cr_lane_road_check(lanes,n,1,anchor,1,angle)==CR_LANE_ROAD_OK);
            assert(!memcmp(before,lanes,sizeof(lanes)));
            assert(cr_lane_road_check(lanes,n,0,anchor,1,angle)==CR_LANE_ROAD_INCOMPLETE);
            assert(cr_lane_road_check(lanes,n,1,anchor,1,-angle)==CR_LANE_ROAD_ACTION_MISMATCH);
            for(int i=anchor+step;i>=0 && i<n;i+=step) {
                cr_lane_safety_lane_t saved=lanes[i];
                /* Status zero is not a prohibition: its explicit turn permits
                 * the connected surface. A straight-only/unknown lane does not. */
                assert(lanes[i].status==0);
                lanes[i].primary=lanes[i].angles[0]=0;
                assert(cr_lane_road_check(lanes,n,1,anchor,1,angle)==CR_LANE_ROAD_UNCONFIRMED_EXIT);
                lanes[i].count=0;lanes[i].primary=1000;
                assert(cr_lane_road_check(lanes,n,1,anchor,1,angle)==CR_LANE_ROAD_UNCONFIRMED_EXIT);
                lanes[i].primary=-1000;
                assert(cr_lane_road_check(lanes,n,1,anchor,1,angle)==CR_LANE_ROAD_UNCONFIRMED_EXIT);
                lanes[i]=saved;
            }
            lanes[anchor].position=n+1;
            assert(cr_lane_road_check(lanes,n,1,anchor,1,angle)==CR_LANE_ROAD_BAD_RECORDS);
            memcpy(lanes,before,sizeof(lanes));lanes[anchor].count=17;
            assert(cr_lane_road_check(lanes,n,1,anchor,1,angle)==CR_LANE_ROAD_BAD_RECORDS);
            memcpy(lanes,before,sizeof(lanes));lanes[anchor].status=0;
            assert(cr_lane_road_check(lanes,n,1,anchor,1,angle)==CR_LANE_ROAD_NO_ANCHOR);
            ++checked;
        }
    memset(lanes,0,sizeof(lanes));lanes[0].status=2;
    assert(cr_lane_road_check(lanes,1,1,0,1,0)==CR_LANE_ROAD_OK);
    assert(cr_lane_road_check(lanes,1,1,0,1,1000)==CR_LANE_ROAD_BAD_MANEUVER);
    assert(cr_lane_road_check(lanes,1,1,0,1,-1000)==CR_LANE_ROAD_BAD_MANEUVER);
    assert(cr_lane_road_check(lanes,1,1,0,1,NAN)==CR_LANE_ROAD_BAD_MANEUVER);
    assert(cr_lane_road_check(lanes,1,1,0,1,INFINITY)==CR_LANE_ROAD_BAD_MANEUVER);
    assert(cr_lane_road_check(lanes,1,1,0,0,0)==CR_LANE_ROAD_UNSUPPORTED);
    lanes[0].primary=150;
    assert(cr_lane_road_check(lanes,1,1,0,1,150)==CR_LANE_ROAD_UNSUPPORTED);
    lanes[0].primary=45;
    assert(cr_lane_road_check(lanes,1,1,0,1,45.5f)==CR_LANE_ROAD_OK);
    assert(cr_lane_road_check(lanes,1,1,0,1,50.f)==CR_LANE_ROAD_ACTION_MISMATCH);
    assert(cr_lane_road_check(NULL,9,1,0,1,0)==CR_LANE_ROAD_BAD_RECORDS);
    assert(cr_lane_road_check(NULL,0,1,0,1,0)==CR_LANE_ROAD_BAD_RECORDS);
    memset(lanes,0,sizeof(lanes));lanes[1].position=1;
    lanes[0].primary=-90;lanes[1].primary=45;lanes[1].status=2;
    assert(cr_lane_road_check(lanes,2,1,1,1,45)==CR_LANE_ROAD_UNSUPPORTED);
    lanes[0].primary=1000;
    assert(cr_lane_road_check(lanes,2,1,1,1,45)==CR_LANE_ROAD_OK); /* unknown inner-side slot stays empty */
    lanes[1].position=0;
    assert(cr_lane_road_check(lanes,2,1,1,1,45)==CR_LANE_ROAD_BAD_RECORDS);
    printf("lane safety: %d mirrored layouts, blocked/unknown exits, complete snapshots, independent actions and no input mutation PASS\n",checked);
    return 0;
}
