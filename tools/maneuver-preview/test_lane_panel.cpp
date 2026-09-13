#include "lane_panel.h"
#include "lane_fixtures.h"
#include "fixtures.h"
#include "render.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <initializer_list>
int main() {
    cr_lane_panel_layout_t small,wide;
    cr_lane_guidance_t g=sample_lanes(0),original=g;
    cr_lane_panel_layout(&small,&g,210);cr_lane_panel_layout(&wide,&g,328);
    assert(small.capacity==5 && small.count==5 && small.cells[0].overflow==-1);
    assert(wide.capacity==8 && wide.count==8);
    assert(small.width<=210-8 && wide.width<=328-8);
    assert(!std::memcmp(&g,&original,sizeof(g)));
    for(int count=1;count<=8;++count)for(int best=0;best<count;++best) {
        g=sample_lanes(0);g.count=count;
        for(int i=0;i<count;++i)g.lanes[i].status=i==best?2:0;
        cr_lane_panel_layout(&small,&g,210);cr_lane_panel_layout(&wide,&g,328);
        int found=0,overflow=0;
        for(int i=0;i<small.count;++i) {
            if(small.cells[i].overflow)++overflow;
            else if(small.cells[i].source==best)++found;
        }
        assert(found==1 && overflow==(count>5));
        assert(wide.count==count);
        for(int i=0;i<count;++i)assert(wide.cells[i].source==i && !wide.cells[i].overflow);
    }
    g=sample_lanes(0);g.lanes[0].status=2;
    cr_lane_panel_layout(&small,&g,210);assert(small.cells[0].overflow==-1); // equal best edges: right
    g.lanes[7].status=0;g.lanes[0].status=1;
    cr_lane_panel_layout(&small,&g,210);assert(small.cells[0].overflow==-1); // recommended is not best
    for(int i=0;i<8;++i)g.lanes[i].position=7-i;
    cr_lane_panel_layout(&wide,&g,328);assert(wide.cells[0].source==7 && wide.cells[7].source==0);
    g.lanes[0].position=65535;
    cr_lane_panel_layout(&wide,&g,328);assert(wide.cells[0].source==0); // incomplete order: no guessing
    for(int status : {0,1,2,255}) {
        g=sample_lanes(4);g.lanes[2].status=status;
        cr_lane_panel_layout(&small,&g,210);
        assert(small.cells[2].count==2 && small.cells[2].recommended==(status==2));
    }
    for(int sentinel : {1000,-1000,181,-181,32767,-32768}) {
        g=sample_lanes(5);g.lanes[0].primary=sentinel;
        for(int i=0;i<3;++i)g.lanes[0].angles[i]=sentinel;
        cr_lane_panel_layout(&small,&g,210);
        assert(!small.cells[0].count && !small.cells[0].recommended);
        g.lanes[0].angles[0]=45;
        cr_lane_panel_layout(&small,&g,210);
        assert(small.cells[0].count==1 && !small.cells[0].recommended);
    }
    g=sample_lanes(0);
    for(int i=0;i<g.count;++i) {
        g.lanes[i].primary=i%2?1000:-1000;g.lanes[i].angle_count=0;
    }
    cr_lane_panel_layout(&small,&g,210);assert(!small.count);
    cr_lane_panel_layout(&wide,&g,328);assert(!wide.count);
    g.lanes[0].primary=45;g.lanes[0].status=2;
    cr_lane_panel_layout(&small,&g,210);
    assert(small.cells[4].overflow==1); // Unknown status=2 at right cannot win.
    cr_lane_panel_t *panel=cr_lane_panel_create();assert(panel);
    // Compile all combinations, including the densest three-branch U-turn mesh.
    for(int a=-180;a<=180;a+=45)for(int b=-180;b<=180;b+=45)for(int c=-180;c<=180;c+=45) {
        g=sample_lanes(0);
        for(int i=0;i<8;++i) {
            g.lanes[i].primary=a;g.lanes[i].angles[0]=b;g.lanes[i].angles[1]=c;
            g.lanes[i].angle_count=2;g.lanes[i].status=2;
        }
        cr_lane_panel_update(panel,&g,328,10);
        assert(!cr_lane_panel_update(panel,&g,328,10.01));
        assert(cr_lane_panel_animating(panel,10.01) && !cr_lane_panel_animating(panel,11));
    }
    g.showing=0;assert(cr_lane_panel_update(panel,&g,210,12));
    assert(!cr_lane_panel_animating(panel,12));
    cr_lane_panel_layout(&small,&g,210);assert(!small.count);
    g.showing=1;g.count=255;cr_lane_panel_layout(&small,&g,210);assert(!small.count);
    cr_lane_panel_layout(&small,&original,NAN);assert(!small.count);
    cr_lane_panel_layout(&small,&original,40);assert(!small.count);
    cr_lane_panel_clear(panel);cr_lane_panel_destroy(panel);
    extern float g_3d_offset_adjust;g_3d_offset_adjust=-.16f;
    cr_scene_t *scene=cr_scene_create();assert(scene);
    panel=cr_lane_panel_create();g=sample_lanes(0);
    cr_scene_view_t view={};view.compact=1;render_build_layout_matrix(view.projection,328.f/181);
    for(int c=0;c<CASES;++c) {
        cr_scene_input_t input={};input.maneuver=sample_maneuver(c);input.generation=c+1;
        assert(cr_scene_prepare(scene,&input,&view));
        cr_lane_panel_update(panel,&g,210,20);
        float frame[3],wide_frame[3],m[16],b[4],head[4];
        cr_lane_panel_framing(panel,scene,nullptr,frame);
        assert(frame[2]>=.12f && frame[2]<=.18001f);
        cr_lane_panel_update(panel,&g,328,20);
        cr_lane_panel_framing(panel,scene,nullptr,wide_frame);
        assert(!std::memcmp(frame,wide_frame,sizeof(frame))); // always use SMALL safety frame
        render_build_framing_matrix(m,328.f/181,frame[2]);
        if(cr_scene_project_bounds(scene,m,b,head)) {
            for(int axis=0;axis<2;++axis) {b[axis]+=frame[axis];b[axis+2]+=frame[axis];head[axis]+=frame[axis];head[axis+2]+=frame[axis];}
            bool fits=b[0]>=62.99f && b[1]>=30.99f && b[2]<=265.01f && b[3]<=133.01f;
            if(!fits)std::fprintf(stderr,"Panel frame FAIL %d %s bounds %.2f %.2f %.2f %.2f dolly %.2f\n",c,fixtures[c].name,b[0],b[1],b[2],b[3],frame[2]);
            assert(fits);
            assert(head[3]<=133.01f);
        }
    }
    g.showing=0;cr_lane_panel_update(panel,&g,210,30);
    float off[3];cr_lane_panel_framing(panel,scene,nullptr,off);assert(off[0]==0 && off[1]==0 && off[2]==0);
    cr_scene_destroy(scene);cr_lane_panel_destroy(panel);
    std::puts("Panel camera: always dollies with lanes; 32 maneuvers fit the SMALL safe frame above the fade in both stages; hide restores ordinary pose PASS");
    std::puts("Lane panel: 5/8 capacity, overflow retains best, right tie, order, unknowns, branch colours, 729 geometry combinations and reset PASS");
}
