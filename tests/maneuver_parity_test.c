/* Execute the production decoder, path builder and transition engine without
 * creating a window or calling main. GL/platform symbols are linked only. */
#define main renderer_application_main
#include "../maneuver_render/main.c"
#undef main
#include "../maneuver_render/maneuver.c"
#include <assert.h>
#include <math.h>
#include "../maneuver_render/bargraph_layout.h"

static void test_bargraph_layout(void) {
    cr_bargraph_layout_t p;
    cr_rect_t areas[]={{0,0,328,180},{59,27,210,153}};
    int i,side;
    float widths[2];
    for (i=0;i<2;i++) {
        cr_bargraph_begin(&p,areas[i]); widths[i]=p.bars[0].w;
        for (side=0;side<2;side++) {
            cr_rect_t r=p.bars[side];
            assert(r.x>=areas[i].x && r.y>=areas[i].y);
            assert(r.x+r.w<=areas[i].x+areas[i].w && r.y+r.h<=areas[i].y+areas[i].h);
            assert(fabsf(16*p.bar_h+15*p.gap-r.h)<0.001f);
        }
        assert(cr_bargraph_side(&p)==1);
        p.cost[1]=100;
        assert(cr_bargraph_side(&p)==0); /* right-pointing arrow: use left edge */
        cr_bargraph_begin(&p,areas[i]);
        p.cost[0]=100;
        assert(cr_bargraph_side(&p)==1);
        cr_bargraph_begin(&p,areas[i]);
        assert(cr_bargraph_side(&p)==1); /* return home once the tip clears */
        p.cost[0]=95; p.cost[1]=100;
        assert(cr_bargraph_side(&p)==0); /* both occupied: smaller intersection */
        p.cost[0]=20;
        assert(cr_bargraph_side(&p)==0); /* both obstructed: substantial improvement */
    }
    assert(widths[0]>widths[1]); /* crop changes dimensions, not just x position */
    cr_rect_animation_t anim={areas[0],areas[0],areas[0],0,0};
    assert(cr_rect_retarget(&anim,areas[1],10.0));
    assert(cr_rect_equal(anim.current,areas[0])); /* no teleport on command */
    cr_rect_animate(&anim,10.125);
    assert(fabsf(anim.current.x-29.5f)<0.001f && anim.active);
    cr_rect_t halfway=anim.current;
    assert(cr_rect_retarget(&anim,areas[0],10.125));
    assert(cr_rect_equal(anim.current,halfway)); /* interrupted switch stays continuous */
    assert(!cr_rect_retarget(&anim,areas[0],10.125)); /* repeat does not restart */
    cr_rect_animate(&anim,10.375);
    assert(!anim.active && cr_rect_equal(anim.current,areas[0]));
    cr_rect_t clipped=cr_visible_area(59,27,65535,65535);
    assert(clipped.x+clipped.w==328 && clipped.y+clipped.h==180);
    clipped=cr_visible_area(0,0,0,0);
    assert(clipped.x==59 && clipped.y==27 && clipped.w==210 && clipped.h==153);
    /* Project an actual vertex triangle onto the right bar, rather than choosing
     * an edge from a maneuver enum. Covers camera/route-space coordinates. */
    cr_bargraph_begin(&p,areas[1]);
    float m[16]={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    float v[18]={0};
    for(i=0;i<3;i++) {
        float x=p.bars[1].x+(i==1?p.bars[1].w:0);
        float y=p.bars[1].y+(i==2?20:0);
        v[6*i]=2*x/328-1;v[6*i+1]=1-2*y/181;
    }
    cr_bargraph_mesh(&p,m,v,3);
    assert(p.cost[1]>0 && p.cost[0]==0 && cr_bargraph_side(&p)==0);
    /* A diagonal head's bounding box is not itself a collision. */
    const float triangle[3][2]={{0,0},{10,0},{0,10}};
    assert(cr_triangle_overlap(triangle,(cr_rect_t){8,8,1,1})==0);
    assert(fabsf(cr_triangle_overlap(triangle,(cr_rect_t){2,2,2,2})-4)<0.001f);
    assert(fabsf(cr_triangle_overlap(triangle,(cr_rect_t){-1,-1,12,12})-50)<0.001f);
}

static void test_bargraph_exit_tip(void) {
    route_path_t path;
    route_mesh_t mesh;
    cr_bargraph_layout_t p;
    const cr_rect_t area={59,27,210,153};
    /* Orthographic projection: path x/z become source x/y pixels. */
    const float m[16]={2.0f/328,0,0,0, 0,0,1,0, 0,-2.0f/181,0,0, -1,1,0,1};
    rpath_clear(&path);
    rpath_add_line(&path,260,70,100,70); rpath_densify(&path);
    rpath_extrude(&path,&mesh,5,0,1,0,1);
    assert(mesh.valid && mesh.arrow_count==24 && mesh.arrow_first>0);
    assert(mesh.arrow_first+mesh.arrow_count<=mesh.vert_count);
    cr_bargraph_begin(&p,area);
    cr_bargraph_mesh(&p,m,mesh.verts,mesh.vert_count);
    assert(p.cost[1]>0); /* shaft crosses right bar */
    cr_bargraph_begin(&p,area);
    cr_bargraph_mesh(&p,m,mesh.verts+6*mesh.arrow_first,mesh.arrow_count);
    assert(p.cost[1]==0 && cr_bargraph_side(&p)==1); /* tip is clear: right */

    rpath_clear(&path);
    rpath_add_line(&path,100,70,246,70); rpath_densify(&path);
    rpath_extrude(&path,&mesh,5,0,1,0,1);
    cr_bargraph_begin(&p,area);
    cr_bargraph_mesh(&p,m,mesh.verts+6*mesh.arrow_first,mesh.arrow_count);
    assert(p.cost[1]>0 && cr_bargraph_side(&p)==0); /* actual prism hits bar */
    rpath_extrude(&path,&mesh,5,0,1,0,0.5f);
    cr_bargraph_begin(&p,area);
    cr_bargraph_mesh(&p,m,mesh.verts+6*mesh.arrow_first,mesh.arrow_count);
    assert(p.cost[1]==0); /* follows the animated tip, not final path endpoint */
    rpath_extrude_partial(&path,&mesh,5,0,1,0,1,1,0,0);
    assert(mesh.valid && mesh.arrow_count==0); /* trailing piece has no tip */
    path.tip_blend=1; path.bulb_radius=4;
    rpath_extrude(&path,&mesh,5,0,1,0,1);
    assert(mesh.valid && mesh.arrow_count==0); /* round bulb is not an exit arrow */
    rpath_clear(&path); rpath_extrude(&path,&mesh,5,0,1,0,1);
    assert(!mesh.valid && mesh.arrow_first==0 && mesh.arrow_count==0);
}

static void test_bargraph_maneuver_selection(void) {
    maneuver_state_t left={0},right={0},arrived={0};
    route_path_t path;
    static route_mesh_t before,after,preview;
    int i;
    left.icon=right.icon=ICON_TURN; left.exit_angle=-90; right.exit_angle=90;
    arrived.icon=ICON_ARRIVED;
    render_set_visible_area(59,27,210,153);
    render_set_perspective(1);
    maneuver_select_bargraph(&right);
    assert(render_get_bargraph_side()==0); /* final right tip conflicts, before animation */
    for (i=0;i<120;i++) {
        /* Incoming geometry stays fixed while the live camera and slide move. */
        render_set_camera_pan((i%2)?0.7f:-0.7f,0.2f);
        render_set_camera_rotation((float)i*0.03f);
        maneuver_set_slide((float)i/60);
        assert(render_get_bargraph_side()==0);
    }
    render_set_visible_area(0,0,328,180);
    assert(render_get_bargraph_side()==1); /* final target crop, before its first frame */
    render_set_visible_area(59,27,210,153);
    assert(render_get_bargraph_side()==0);
    render_set_camera_pan(0,0); render_set_camera_rotation(0); maneuver_set_slide(1);
    maneuver_select_bargraph(&left);
    assert(render_get_bargraph_side()==1); /* independent decision for next maneuver */
    maneuver_select_bargraph(&arrived);
    assert(render_get_bargraph_side()==1); /* final bulb has no exit tip */

    /* Preview must preserve the live combined path's ramp/elevation byte for byte. */
    maneuver_build_route(&right,&path); rpath_extend(&path); rpath_densify(&path);
    rpath_set_ramp_restart(0.8f); rpath_set_elevation(0.02f,0.09f);
    rpath_extrude(&path,&before,SHAFT_T,ROUTE_BASE_Y,ROUTE_TOP_Y,0,0.8f);
    left.exit_angle=-170;
    maneuver_build_settled_mesh(&left,&preview);
    rpath_extrude(&path,&after,SHAFT_T,ROUTE_BASE_Y,ROUTE_TOP_Y,0,0.8f);
    assert(before.vert_count==after.vert_count);
    assert(!memcmp(before.verts,after.verts,before.vert_count*6*sizeof(float)));
    assert(preview.arrow_count==RPATH_ARROW_VERTS);
    rpath_set_ramp_restart(-1); rpath_set_elevation(0,0);

    /* Queueing a later maneuver must not steal the active incoming side. */
    left.exit_angle=-90;
    memset(&g_engine,0,sizeof(g_engine));
    g_engine.current=left; g_engine.has_current=1; maneuver_set_slide(1);
    engine_apply_maneuver(&right);
    assert(g_engine.phase==ENGINE_PUSHING && render_get_bargraph_side()==0);
    engine_apply_maneuver(&left);
    assert(g_engine.has_pending && render_get_bargraph_side()==0);
    left.junction_angle_count=1; left.junction_angles[0]=45;
    engine_refresh_maneuver(&left);
    assert(render_get_bargraph_side()==0);
    maneuver_set_slide(1); engine_tick();
    assert(g_engine.current.exit_angle==90 && g_engine.next.exit_angle==-90);
    assert(g_engine.phase==ENGINE_PUSHING && !g_engine.has_pending);
    assert(render_get_bargraph_side()==1); /* queued target selects only when promoted */
    /* A direct NONE transition replaces the incoming target immediately. */
    arrived.icon=ICON_NONE; engine_apply_maneuver(&arrived);
    assert(render_get_bargraph_side()==1);
}

static void put_angle(uint8_t *p, int value) {
    p[0] = (uint8_t)(value >> 8); p[1] = (uint8_t)value;
}
static int near(float a, float b) { return fabsf(a - b) < 0.0001f; }
static float max_height(const route_mesh_t *m) {
    float h = -1000;
    int i;
    assert(m->valid && m->vert_count > 0);
    for (i=0;i<m->vert_count;i++) {
        float y=m->verts[6*i+1];assert(isfinite(y));
        if (y>h) h=y;
    }
    return h;
}

int main(void) {
    test_bargraph_layout();
    test_bargraph_exit_tip();
    test_bargraph_maneuver_selection();
    cr_cmd_t cmd;
    maneuver_state_t s, left, right;
    route_path_t path;
    route_mesh_t mesh;
    int i;
    memset(&cmd, 0, sizeof(cmd));
    cmd.cmd = CMD_MANEUVER; cmd.flags = MAN_FLAG_BAP_GEOMETRY;
    cmd.payload[0] = ICON_ROUNDABOUT;
    cmd.payload[1] = 255; cmd.payload[4] = 1; cmd.payload[5] = 255;
    put_angle(cmd.payload + 2, -135);
    for (i = 0; i < MAX_JUNCTION_ANGLES; i++) put_angle(cmd.payload + 6 + 2*i, -45 + i*45);
    cr_decode_maneuver(&cmd, &s);
    assert(s.icon == ICON_ROUNDABOUT && s.direction == -1 && s.driving_side == 1);
    assert(s.bap_geometry && near(s.exit_angle, -67.5f));
    assert(s.junction_angle_count == 18 && near(s.junction_angles[0], -22.5f));
    assert(near(s.junction_angles[17], 360.0f));
    cmd.flags |= MAN_FLAG_SNAP_TO_ROAD;
    cr_decode_maneuver(&cmd, &s);
    assert(!s.bap_geometry && near(s.exit_angle, -67.5f)); /* units independent of snap */
    cmd.flags = 0; put_angle(cmd.payload + 2, -67); put_angle(cmd.payload + 6, -1);
    cr_decode_maneuver(&cmd, &s);
    assert(!s.bap_geometry && near(s.exit_angle, -67) && near(s.junction_angles[0], -1));
    cmd.payload[0] = 255; cmd.payload[4] = 255;
    cr_decode_maneuver(&cmd, &s);
    assert(s.icon == ICON_NONE && s.driving_side == 0);
    maneuver_build_route(&s, &path); assert(path.seg_count == 0);

    memset(&s, 0, sizeof(s)); s.icon = ICON_ROUNDABOUT; s.exit_angle = -67.5f;
    s.junction_angle_count = 1; s.junction_angles[0] = -90; s.bap_geometry = 1;
    maneuver_exit_t ex = maneuver_get_exit(&s);
    assert(near(ex.heading, 157.5f * (float)M_PI / 180));
    maneuver_build_route(&s, &path);
    assert(near(path.arrow_angle, ex.heading));
    s.bap_geometry = 0;
    assert(near(maneuver_get_exit(&s).heading, (float)M_PI)); /* legacy snap retained */

    /* Raw roads contain the active exit; BAP-filtered roads do not. The active
     * road must be chosen before an adjacent road, with a strict 30-degree cap. */
    s.exit_angle=-67;s.junction_angle_count=3;
    s.junction_angles[0]=-90;s.junction_angles[1]=-65;s.junction_angles[2]=0;
    rab_snap_t snap=snap_roundabout_exit(s.exit_angle,s.junction_angles,3,s.bap_geometry);
    assert(snap.snapped_idx==1 && near(snap.snapped_deg,-65));
    ex=maneuver_get_exit(&s);maneuver_build_route(&s,&path);
    assert(near(ex.heading,155*(float)M_PI/180) && near(path.arrow_angle,ex.heading));
    s.junction_angles[0]=-97;
    snap=snap_roundabout_exit(-67,s.junction_angles,1,0);
    assert(snap.snapped_idx==-1 && near(snap.snapped_deg,-67));
    s.junction_angles[0]=179;
    snap=snap_roundabout_exit(-179,s.junction_angles,1,0);
    assert(snap.snapped_idx==0 && near(snap.snapped_deg,179));
    s.junction_angles[0]=10;
    snap=snap_roundabout_exit(0,s.junction_angles,1,1);
    assert(snap.snapped_idx==-1 && near(snap.snapped_deg,0)); /* missing fallback */
    s.junction_angles[0]=157;
    snap=snap_roundabout_exit(180,s.junction_angles,1,1);
    assert(snap.snapped_idx==-1 && near(snap.snapped_deg,180)); /* typed U-turn */

    /* The exact >150-degree turn must still activate the production lift.
     * Compare the same mesh with and without that lift, rather than its code. */
    for (i=-1;i<=1;i+=2) {
        memset(&s,0,sizeof(s));s.icon=ICON_TURN;s.exit_angle=i*160.0f;
        maneuver_build_route(&s,&path);rpath_extend(&path);rpath_densify(&path);
        rpath_set_elevation(maneuver_elevation(&s),maneuver_elevation(&s));
        rpath_extrude(&path,&mesh,0.1f,0.02f,0.05f,0.0f,1.0f);
        float raised=max_height(&mesh);
        rpath_set_elevation(0,0);
        rpath_extrude(&path,&mesh,0.1f,0.02f,0.05f,0.0f,1.0f);
        printf("overlap %+.0f degrees: raised=%.4f flat=%.4f\n",s.exit_angle,raised,max_height(&mesh));
        assert(raised>max_height(&mesh)+0.02f);
    }

    for (i = ICON_APPROACH; i < ICON_COUNT; i++) {
        int side;
        for (side = 0; side < 2; side++) {
            memset(&s, 0, sizeof(s));s.icon=i;s.driving_side=side;s.direction=side?1:-1;
            s.exit_angle=side?67.5f:-67.5f;s.bap_geometry=1;
            maneuver_build_route(&s, &path);ex=maneuver_get_exit(&s);
            assert(path.seg_count>0 && near(path.arrow_x,ex.x) && near(path.arrow_y,ex.y));
            assert(near(path.arrow_angle,ex.heading));
            rpath_densify(&path);assert(isfinite(path.total_length) && path.total_length>0);
        }
    }
    memset(&left,0,sizeof(left));left.icon=ICON_ROUNDABOUT_EXIT;left.driving_side=1;
    right=left;right.driving_side=0;
    maneuver_exit_t l=maneuver_get_exit(&left),r=maneuver_get_exit(&right);
    assert(l.x<0 && r.x>0 && near(l.x,-r.x) && near(l.y,r.y));

    assert(!(cr_merge_maneuver_flags(1,4,12)&MAN_FLAG_REFRESH)); /* new + refresh */
    assert(!(cr_merge_maneuver_flags(1,12,4)&MAN_FLAG_REFRESH)); /* refresh + new */
    assert(cr_merge_maneuver_flags(1,12,12)&MAN_FLAG_REFRESH);
    assert(cr_merge_maneuver_flags(0,4,12)&MAN_FLAG_REFRESH); /* after CLEAR */
    /* Seed the engine without clear_maneuver's GL alpha update. */
    memset(&g_engine,0,sizeof(g_engine));
    g_engine.has_current=1;g_engine.current.icon=ICON_APPROACH;
    maneuver_set_slide(1.0f);
    s=right;s.icon=ICON_TURN;s.exit_angle=-90;
    g_engine.current=s;g_cleared=0;
    s.junction_angle_count=1;s.junction_angles[0]=90;
    engine_refresh_maneuver(&s);
    assert(g_engine.phase==ENGINE_IDLE && g_engine.current.junction_angle_count==1);
    s.exit_angle=45;engine_apply_maneuver(&s);
    assert(g_engine.phase==ENGINE_PUSHING && g_engine.has_next);
    s.junction_angles[0]=-90;engine_refresh_maneuver(&s);
    assert(g_engine.next.junction_angles[0]==-90 && g_engine.current.exit_angle==-90);
    assert(!g_engine.has_pending); /* refresh must not enqueue another maneuver */
    s.icon=ICON_LANE_CHANGE;engine_apply_maneuver(&s);
    assert(g_engine.has_pending);
    s.direction=-1;engine_refresh_maneuver(&s);
    assert(g_engine.pending.direction==-1 && g_engine.next.icon==ICON_TURN);
    s.icon=ICON_NONE;engine_apply_maneuver(&s);
    assert(g_engine.phase==ENGINE_IDLE && !g_engine.has_pending && !g_engine.has_next);
    g_persp_deferred=1;g_persp_deferred_value=1;
    g_bargraph_deferred=1;g_bargraph_deferred_level=7;g_bargraph_deferred_mode=1;
    engine_tick();
    assert(!g_persp_deferred && !g_bargraph_deferred && g_bargraph_level==7 && g_bargraph_on==1);
    s.icon=ICON_TURN;engine_apply_maneuver(&s);
    assert(g_engine.phase==ENGINE_IDLE && g_engine.current.icon==ICON_TURN);
    puts("maneuver_parity_test: decoder, half-degree paths, icon families, refresh/coalescing and NONE transitions PASS");
    return 0;
}
