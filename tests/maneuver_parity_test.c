/* Execute the production decoder, path builder and transition engine without
 * creating a window or calling main. GL/platform symbols are linked only. */
#define main renderer_application_main
#include "../maneuver_render/main.c"
#undef main
#include "../maneuver_render/maneuver.c"
#include <assert.h>
#include <math.h>

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
