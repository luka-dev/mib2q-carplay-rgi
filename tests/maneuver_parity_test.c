/* Execute the production decoder, path builder and transition engine without
 * creating a window or calling main. GL/platform symbols are linked only. */
#define main renderer_application_main
#include "../maneuver_render/main.c"
#undef main
#include "../maneuver_render/maneuver.c"
#include <assert.h>
#include <math.h>
#include "../maneuver_render/visible_area.h"

static void build_settled_mesh(const maneuver_state_t *state, route_mesh_t *mesh) {
    route_path_t path;
    float end;
    maneuver_build_route(state,&path);
    rpath_extend(&path);
    rpath_densify(&path);
    if (state->icon==ICON_ARRIVED) {
        path.tip_blend=1.0f;
        path.bulb_radius=ARRIVE_INNER_R-OL_W;
    }
    end=path.total_length>1e-6f ? (path.total_length-ROUTE_EXTEND)/path.total_length : 0;
    rpath_set_ramp_restart(-1.0f);
    rpath_set_elevation(maneuver_elevation(state),maneuver_elevation(state));
    rpath_extrude(&path,mesh,SHAFT_T,ROUTE_BASE_Y,ROUTE_TOP_Y,0,end);
    rpath_set_elevation(0,0);
}

static void test_route_progress_projection(void) {
    cr_route_progress_map_t map;
    float identity[16]={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    cr_rect_t crop={0,0,328,180};
    cr_route_progress_point_t points[]={{0,0,-2,0},{1,0,0,0},{2,.8f,0,0}};
    assert(cr_route_progress_project(&map,points,3,identity,crop));
    /* 91.5 px of the lower extension are outside the content crop. */
    assert(fabsf(map.start-91.5f)<1e-4f);
    assert(fabsf(map.length-(89.5f+131.2f))<1e-4f);
    assert(fabsf(cr_route_progress_at(&map,91.5f/181))<1e-5f);
    assert(cr_route_progress_at(&map,0)<0);
    assert(fabsf(cr_route_progress_at(&map,2)-1)<1e-5f);
    /* Moving the crop bottom moves the origin, not the source animation state. */
    crop.y=20;crop.h=100;
    assert(cr_route_progress_project(&map,points,3,identity,crop));
    assert(fabsf(map.start-151.5f)<1e-4f);
    assert(fabsf(map.length-(29.5f+131.2f))<1e-4f);

    crop=(cr_rect_t){0,0,328,180};
    identity[7]=.3f; /* unequal clip-W at the ends of a long straight triangle */
    assert(cr_route_progress_project(&map,points,3,identity,crop));
    int i;
    for(i=0;i<=16;i++) {
        float world_t=i/16.0f;
        float world_y=-2+2*world_t;
        float screen_y=(1-world_y/(1+.3f*world_y))*181*.5f;
        float expected=(180-screen_y)/map.length;
        assert(fabsf(cr_route_progress_at(&map,world_t)-expected)<1e-5f);
    }
    /* Exact same screen point visited twice must have different progress. */
    cr_route_progress_point_t repeated[]={{0,0,-1,0},{1,0,.5f,0},{2,0,-1,0},{3,0,.5f,0}};
    identity[7]=0;
    assert(cr_route_progress_project(&map,repeated,4,identity,crop));
    assert(cr_route_progress_at(&map,2.5f)>cr_route_progress_at(&map,.5f)+.5f);
    crop=(cr_rect_t){0,0,1,1};
    assert(!cr_route_progress_project(&map,repeated,4,identity,crop));
    identity[15]=-1;
    assert(!cr_route_progress_project(&map,repeated,4,identity,crop));

    /* Numerically integrate the actual shader feather. Every 1/16 step adds
     * 1/16 visible centerline coverage, including zero and the complete tip. */
    for(i=0;i<=32;i++) {
        float fill=i/32.0f, front=cr_route_progress_front(fill), sum=0;
        int j;
        for(j=0;j<20000;j++) {
            float q=(j+.5f)/20000;
            float t=(q-front+CR_ROUTE_FEATHER)/(2*CR_ROUTE_FEATHER);
            if(t<0)t=0;if(t>1)t=1;
            sum+=1-t*t*(3-2*t);
        }
        assert(fabsf(sum/20000-fill)<2e-5f);
    }
    assert(cr_route_progress_front(0)==-CR_ROUTE_FEATHER);
    assert(cr_route_progress_front(1)==1+CR_ROUTE_FEATHER);
    puts("route progress: visible crop origin, perspective length, crossings and even feather coverage PASS");
}

static void test_arrow_progress(void) {
    cr_arrow_progress_t p;
    cr_progress_reset(&p);
    assert(p.state==CR_PROGRESS_OFF && !p.active);
    assert(cr_progress_decode(0,0,1)==CR_PROGRESS_FILL);
    assert(cr_progress_decode(0,255,1)==CR_PROGRESS_FILL); /* unmarked byte ignored */
    assert(cr_progress_decode(0,3,0)==CR_PROGRESS_OFF);
    assert(cr_progress_decode(0,3,2)==CR_PROGRESS_OFF); /* no independent blink clock */
    assert(cr_progress_decode(0,1,255)==CR_PROGRESS_OFF);
    assert(cr_progress_decode(32,4,1)==CR_PROGRESS_OFF);
    assert(cr_progress_decode(32,0,1)==CR_PROGRESS_OFF);
    assert(cr_progress_decode(32,2,2)==CR_PROGRESS_OFF);
    assert(cr_progress_decode(32,3,1)==3);
    cr_progress_set(&p,16,CR_PROGRESS_FILL,10);
    assert(p.fill==0 && !p.active && p.velocity==0);
    cr_progress_set(&p,8,CR_PROGRESS_FILL,10);
    cr_progress_tick(&p,10.08);
    assert(p.fill>0 && p.fill<.5f && p.velocity>0);
    float position=p.fill, velocity=p.velocity;
    cr_progress_set(&p,0,CR_PROGRESS_FILL,10.08);
    assert(p.fill==position && p.velocity==velocity); /* further target keeps momentum */
    cr_progress_tick(&p,10.10);
    assert(p.fill>position && p.velocity>velocity); /* accelerates instead of restarting */
    position=p.fill;velocity=p.velocity;
    cr_progress_set(&p,16,CR_PROGRESS_FILL,10.10);
    assert(p.fill==position && p.velocity==velocity); /* reversal brakes, not teleports */
    cr_progress_tick(&p,10.11);
    assert(p.fill>position && p.velocity<velocity);
    cr_progress_tick(&p,11);
    assert(p.fill==0 && p.velocity==0 && !p.active);

    /* Repeated targets at 200 Hz produce the same motion as a single target.
     * Compare 30/60 Hz and irregular frame intervals at an identical timestamp. */
    cr_arrow_progress_t single, repeated, fps30, fps60;
    cr_progress_reset(&single);cr_progress_set(&single,16,1,0);cr_progress_set(&single,0,1,0);
    repeated=fps30=fps60=single;
    int n;
    for(n=1;n<=40;n++) cr_progress_set(&repeated,0,1,n*.005);
    for(n=1;n<=6;n++) cr_progress_tick(&fps30,n/30.0);
    for(n=1;n<=12;n++) cr_progress_tick(&fps60,n/60.0);
    cr_progress_tick(&single,.017);cr_progress_tick(&single,.071);cr_progress_tick(&single,.2);
    assert(fabsf(single.fill-repeated.fill)<1e-5f && fabsf(single.velocity-repeated.velocity)<1e-5f);
    assert(fabsf(single.fill-fps30.fill)<1e-5f && fabsf(single.velocity-fps30.velocity)<1e-5f);
    assert(fabsf(single.fill-fps60.fill)<1e-5f && fabsf(single.velocity-fps60.velocity)<1e-5f);
    position=single.fill;velocity=single.velocity;
    cr_progress_tick(&single,.1);
    assert(single.fill==position && single.velocity==velocity && single.last_time==.2);

    /* Targets outrun rendering: retain velocity at each packet, remain bounded,
     * and reach the final target with no overshoot or perpetual idle redraw. */
    cr_progress_reset(&p);cr_progress_set(&p,16,1,0);
    for(n=1;n<=16;n++) {
        double now=n*.025;
        cr_progress_tick(&p,now);position=p.fill;velocity=p.velocity;
        cr_progress_set(&p,16-n,1,now);
        assert(p.fill==position && p.velocity==velocity);
        assert(p.fill>=0 && p.fill<=p.target && p.velocity>=0);
    }
    for(n=1;n<=60;n++) {
        position=p.fill;cr_progress_tick(&p,.4+n/60.0);
        assert(p.fill>=position && p.fill<=1);
    }
    assert(p.fill==1 && p.velocity==0 && !p.active);
    /* Alternating 0<>16 targets and a closer target while already moving. */
    for(n=1;n<=120;n++) {
        cr_progress_set(&p,(n%2)*16,1,2+n*.03);
        cr_progress_tick(&p,2+n*.03+.015);
        assert(isfinite(p.fill) && isfinite(p.velocity) && p.fill>=0 && p.fill<=1);
    }
    cr_progress_reset(&p);cr_progress_set(&p,16,1,0);cr_progress_set(&p,0,1,0);
    cr_progress_tick(&p,.1);assert(p.fill<.5f);
    cr_progress_set(&p,8,1,.1);cr_progress_tick(&p,.3);
    assert(p.fill==.5f && !p.active && p.velocity==0);

    cr_progress_set(&p,0,CR_PROGRESS_BLINK_LOW,11);
    assert(!p.active && p.velocity==0 && p.state==CR_PROGRESS_BLINK_LOW);
    cr_progress_tick(&p,100);assert(p.state==CR_PROGRESS_BLINK_LOW); /* no second clock */
    cr_progress_set(&p,16,CR_PROGRESS_BLINK_HIGH,100);
    assert(!p.active && p.state==CR_PROGRESS_BLINK_HIGH);
    cr_progress_set(&p,0,CR_PROGRESS_OFF,100);
    assert(!p.initialized && !p.active && p.state==CR_PROGRESS_OFF);
    cr_progress_set(&p,8,CR_PROGRESS_FILL,101);
    assert(p.fill==.5f && !p.active && p.velocity==0); /* no stale momentum after off */
    cr_progress_set(&p,0,CR_PROGRESS_FILL,101);cr_progress_tick(&p,101.05);
    cr_progress_reset(&p);
    assert(p.velocity==0 && !p.active && !p.initialized);

    static route_path_t path;
    static route_mesh_t mesh;
    maneuver_state_t s;
    int icon,side,i;
    for(icon=ICON_APPROACH;icon<ICON_COUNT;icon++) for(side=0;side<2;side++) {
        memset(&s,0,sizeof(s));s.icon=icon;s.driving_side=side;s.direction=side?1:-1;
        s.exit_angle=side?160:-160;s.bap_geometry=1;
        build_settled_mesh(&s,&mesh); assert(mesh.valid);
        assert(mesh.progress_end>mesh.progress_start);
        assert(mesh.progress_count>=2 && mesh.progress_count<=CR_ROUTE_PROGRESS_POINTS);
        int point;
        for(point=1;point<mesh.progress_count;point++)
            assert(mesh.progress_points[point].d>mesh.progress_points[point-1].d);
        float max=-1;
        for(i=0;i<mesh.vert_count;i++) {
            assert(isfinite(mesh.path_dist[i]));
            if(mesh.path_dist[i]>max)max=mesh.path_dist[i];
        }
        assert(fabsf(max-mesh.progress_end)<1e-4f);
    }
    /* Exact retracing path: spatial coordinates are identical in both passes,
     * but distance must retain traversal order. First 18 vertices per segment. */
    rpath_clear(&path);
    rpath_add_line(&path,0,0,1,0);rpath_add_line(&path,1,0,0,0);
    rpath_add_line(&path,0,0,1,0);rpath_densify(&path);
    rpath_extrude(&path,&mesh,.1f,.02f,.05f,0,1);
    /* Corners may miter the edges differently; centerline revisits x=0. */
    assert(fabsf(mesh.verts[0]-mesh.verts[36*6])<1e-5f);
    assert(mesh.path_dist[0]==0 && mesh.path_dist[36]==2);
    rpath_extrude_partial(&path,&mesh,.1f,.02f,.05f,.2f,.7f,0,0,0);
    assert(fabsf(mesh.path_dist[0]-.6f)<1e-5f); /* partial window doesn't rebase distance */
    puts("arrow progress: protocol, duplicate/retarget/off/blink and all settled meshes PASS");
}

static void test_progress_glow(void) {
    cr_arrow_progress_t p;
    cr_progress_reset(&p);
    cr_progress_set(&p,16,CR_PROGRESS_BLINK_HIGH,0);
    assert(p.glow==0 && p.tint_active);
    cr_progress_tick(&p,.05);
    assert(fabsf(p.glow-.5f)<1e-5f && p.path_weight==0);
    cr_arrow_progress_t repeated=p;
    cr_progress_set(&repeated,16,CR_PROGRESS_BLINK_HIGH,.05);
    assert(repeated.tint_started==0); /* duplicates do not prolong a phase edge */
    cr_progress_tick(&p,.1);cr_progress_tick(&repeated,.1);
    assert(p.glow==1 && !p.tint_active && repeated.glow==1);
    cr_progress_tick(&p,10);
    assert(p.glow==1 && p.state==CR_PROGRESS_BLINK_HIGH); /* no autonomous blinking */

    /* A maneuver command starts dimming with motion, while preserving the
     * displayed endpoint. It must finish long before a normal route push. */
    cr_progress_begin_maneuver(&p,10);
    cr_progress_set(&p,0,CR_PROGRESS_OFF,10);
    assert(p.glow==1 && p.tint_active);
    cr_progress_tick(&p,10.075);assert(fabsf(p.glow-.5f)<1e-4f);
    cr_progress_tick(&p,10.151);assert(p.glow==0 && !p.tint_active);
    cr_progress_set(&p,0,CR_PROGRESS_BLINK_LOW,11);
    assert(p.glow==0 && !p.tint_active && p.path_weight==0);
    cr_progress_set(&p,0,CR_PROGRESS_OFF,11);
    assert(p.glow==0 && !p.tint_active); /* low -> off has exactly the same color */

    cr_progress_set(&p,16,CR_PROGRESS_BLINK_HIGH,12);
    cr_progress_tick(&p,12.04);float glow=p.glow;
    cr_progress_set(&p,0,CR_PROGRESS_BLINK_LOW,12.04);
    assert(p.glow==glow); /* interrupted edge remains continuous */
    cr_progress_tick(&p,12.03);assert(p.glow==glow); /* monotonic clock */
    cr_progress_tick(&p,12.2);assert(p.glow==0 && !p.tint_active);
    cr_progress_set(&p,16,CR_PROGRESS_BLINK_HIGH,13);cr_progress_tick(&p,13.2);
    cr_progress_set(&p,8,CR_PROGRESS_FILL,14);
    assert(p.fill==.5f && p.glow==1 && p.path_weight==0);
    cr_progress_tick(&p,14.075);
    assert(fabsf(p.glow-.5f)<1e-4f && fabsf(p.path_weight-.5f)<1e-4f);
    cr_progress_tick(&p,14.2);assert(p.glow==0 && p.path_weight==1);

    cr_progress_set(&p,0,CR_PROGRESS_FILL,15);cr_progress_tick(&p,15.05);
    assert(p.velocity>0);float fill=p.fill;
    cr_progress_begin_maneuver(&p,15.05);
    assert(p.fill==fill && p.velocity==0 && !p.initialized);
    cr_progress_set(&p,16,CR_PROGRESS_FILL,15.05);
    assert(p.fill==fill && p.target==0 && p.velocity==0);
    /* Switch away/back before the spatial tint disappears. Keep its front. */
    cr_progress_set(&p,0,CR_PROGRESS_OFF,15.05);
    cr_progress_tick(&p,15.10);fill=p.fill;
    cr_progress_set(&p,0,CR_PROGRESS_FILL,15.10);
    assert(p.fill==fill && p.target==1 && p.velocity==0);

    /* Rapidly replaced modes remain inside the two-color palette; no stale
     * glow, overshoot or idle animation after the final off command. */
    int i;
    for(i=0;i<120;i++) {
        cr_progress_set(&p,i%17,i%4,16+i*.03);
        cr_progress_tick(&p,16+i*.03+.02);
        assert(p.path_weight>=0 && p.glow>=0 && p.path_weight+p.glow<=1.000001f);
    }
    cr_progress_set(&p,0,CR_PROGRESS_OFF,20);cr_progress_tick(&p,21);
    assert(p.glow==0 && p.path_weight==0 && !p.tint_active && !p.active);
    cr_progress_reset(&p);assert(p.glow==0 && p.path_weight==0 && !p.tint_active);
    puts("arrow glow: two endpoints, phase easing, maneuver fade, interruption and reset PASS");
}

static void test_progress_handoff(void) {
    cr_arrow_progress_t p;
    cr_progress_reset(&p);cr_progress_set(&p,0,CR_PROGRESS_FILL,0);cr_progress_tick(&p,1);
    cr_progress_begin_handoff(&p,1);
    assert(p.fill==1 && p.path_weight==1 && p.handoff);
    cr_progress_set(&p,8,CR_PROGRESS_FILL,1);
    cr_arrow_progress_t fps30=p,fps60=p;
    float previous=p.fill;
    for(int i=1;i<=24;i++) {
        cr_progress_set(&p,i<12?8:4,CR_PROGRESS_FILL,1+i/60.0);
        assert(p.fill<=previous && p.path_weight==1 && p.target==0);
        previous=p.fill;
    }
    cr_progress_tick(&p,1.5);assert(p.fill==0 && p.handoff);
    for(int i=1;i<=6;i++)cr_progress_tick(&fps30,1+i/30.0);
    for(int i=1;i<=12;i++)cr_progress_tick(&fps60,1+i/60.0);
    assert(fabsf(fps30.fill-.5f)<1e-5f && fabsf(fps30.fill-fps60.fill)<1e-5f);
    cr_progress_tick(&p,3);assert(p.fill==0); /* wait for route commit, not a timer */
    cr_progress_finish_handoff(&p,3);
    assert(!p.handoff && p.fill==0 && p.target==.75f && p.active);
    cr_progress_tick(&p,3.1);assert(p.fill>0 && p.fill<.75f && p.velocity>0);
    float fill=p.fill,velocity=p.velocity;
    cr_progress_set(&p,2,CR_PROGRESS_FILL,3.1);
    assert(p.fill==fill && p.velocity==velocity); /* live updates keep momentum */
    cr_progress_tick(&p,4.2);assert(p.fill==.875f && !p.active);

    /* Even an unusually early commit cannot turn the retreat into a snap. */
    cr_progress_begin_handoff(&p,5);cr_progress_set(&p,8,CR_PROGRESS_FILL,5);
    cr_progress_finish_handoff(&p,5.1);
    assert(p.handoff && p.fill>0);
    cr_progress_tick(&p,5.41);assert(!p.handoff && p.fill==0 && p.target==.5f);
    cr_progress_tick(&p,6.5);assert(p.fill==.5f);

    /* Blink fades during departure; the latest incoming phase wins at commit,
     * and blink->fill must also grow spatially from an empty route. */
    cr_progress_reset(&p);cr_progress_set(&p,0,CR_PROGRESS_BLINK_HIGH,7);cr_progress_tick(&p,8);
    cr_progress_begin_handoff(&p,8);cr_progress_set(&p,0,CR_PROGRESS_BLINK_HIGH,8);
    cr_progress_tick(&p,8.075);assert(fabsf(p.glow-.5f)<1e-4f);
    cr_progress_set(&p,0,CR_PROGRESS_BLINK_LOW,8.2);assert(p.glow==0);
    cr_progress_finish_handoff(&p,9);assert(p.state==CR_PROGRESS_BLINK_LOW && p.glow==0);
    cr_progress_begin_handoff(&p,10);cr_progress_set(&p,4,CR_PROGRESS_FILL,10);
    cr_progress_finish_handoff(&p,11);assert(p.fill==0 && p.target==.75f && p.active);
    cr_progress_tick(&p,11.1);assert(p.fill>0 && p.fill<.75f);
    cr_progress_reset(&p);assert(!p.handoff && !p.active && p.fill==0);

    /* Actual engine ownership: C's queued updates cannot color entering B. */
    memset(&g_engine,0,sizeof(g_engine));g_engine.has_current=1;
    g_engine.current.icon=ICON_TURN;g_engine.current.exit_angle=-90;
    maneuver_set_slide(1);cr_progress_reset(&g_arrow);
    cr_progress_set(&g_arrow,0,CR_PROGRESS_FILL,20);cr_progress_tick(&g_arrow,21);
    maneuver_state_t next=g_engine.current;next.exit_angle=45;
    engine_apply_maneuver(&next,21);engine_set_progress(8,CR_PROGRESS_FILL,21);
    double started=g_arrow.retract_started;
    next.exit_angle=90;engine_apply_maneuver(&next,21.1);
    engine_set_progress(2,CR_PROGRESS_FILL,21.1);
    engine_set_progress(4,CR_PROGRESS_FILL,21.2);
    engine_refresh_maneuver(&next,21.2);
    assert(g_arrow.incoming_level==8 && g_arrow.retract_started==started);
    assert(g_pending_progress_level==4 && g_engine.has_pending);
    cr_progress_tick(&g_arrow,22);g_route_animating=0;engine_tick(22);
    assert(g_engine.current.exit_angle==45 && g_engine.next.exit_angle==90);
    assert(g_arrow.handoff && g_arrow.incoming_level==4 && !g_engine.has_pending);
    cr_progress_tick(&g_arrow,23);g_route_animating=0;engine_tick(23);
    assert(g_engine.current.exit_angle==90 && !g_arrow.handoff && g_arrow.fill==0);
    cr_progress_tick(&g_arrow,24);assert(g_arrow.fill==.75f);
    maneuver_set_slide(1);cr_progress_reset(&g_arrow);
    memset(&g_engine,0,sizeof(g_engine));
    puts("arrow handoff: retreat, commit gate, refill, FPS invariance and queued target ownership PASS");
}

static void test_contact_shadow(void) {
    float source[12*6]={0},copy[12*6],out[CR_CONTACT_MAX_VERTS*6];
    const float corners[6][2]={{-1,-1},{1,-1},{1,1},{-1,-1},{1,1},{-1,1}};
    int i,layer;
    for(layer=0;layer<2;layer++)for(i=0;i<6;i++) {
        float *v=source+(layer*6+i)*6;
        v[0]=corners[i][0]*(layer?.5f:1);v[1]=layer?.05f:0;
        v[2]=corners[i][1]*(layer?.5f:1);v[4]=1;
    }
    memcpy(copy,source,sizeof(source));
    int count=cr_contact_build(source,12,.02f,out,CR_CONTACT_MAX_VERTS);
    assert(count>0 && count%3==0);
    assert(!memcmp(copy,source,sizeof(source)));
    for(i=0;i<count;i++) {
        const float *v=out+i*6;
        assert(v[1]==0); /* only the lower receiving plane */
        assert(fabsf(v[0])<=1.00001f && fabsf(v[2])<=1.00001f);
        assert(fmaxf(fabsf(v[0]),fabsf(v[2]))>=.4749f); /* no internal diagonal crease */
        assert(fabsf(v[3])<=1.00001f && fabsf(v[4]-.05f)<1e-5f);
    }
    assert(cr_contact_build(source,12,.02f,out,3)==-1); /* bounded output, no overwrite */
    /* A folded surface lifts continuously from a crease. Shadow must cover
     * its first 2%, including clearances below the old adjacency/gap cutoffs.
     * Width grows from zero to full width using one scale across the mesh. */
    for(i=6;i<12;i++)source[i*6+1]=.05f+.1f*source[i*6+2];
    count=cr_contact_build(source,12,.02f,out,CR_CONTACT_MAX_VERTS);
    int has_start=0,has_full=0;
    assert(count>0);
    for(i=0;i<count;i++) {
        const float *v=out+i*6;
        assert(v[5]>=0 && v[5]<=1.00001f && isfinite(v[5]));
        if(v[2]<-.49f && v[5]<.02f)has_start=1;
        /* Along either side of the sloping fold, normalized width is linear
         * in distance from contact, independently of the triangle split. */
        if(fabsf(v[0])>.4749f && fabsf(v[0])<.5251f && fabsf(v[2])<.45f)
            assert(fabsf(v[5]-(v[2]+.5f))<1e-4f);
        if(v[5]>.9999f)has_full=1;
    }
    assert(has_start && has_full);
    /* Nearby pieces of one sloping plane must not self-shadow. Their shared
     * height ramp has no clearance, even though min/max top heights differ. */
    for(i=0;i<12;i++)source[i*6+1]=.05f+.1f*source[i*6+2];
    assert(cr_contact_build(source,12,.02f,out,CR_CONTACT_MAX_VERTS)==0);
    /* A strip can be close to a lower face without actually overlying it. */
    memcpy(source,copy,sizeof(source));
    for(i=6;i<12;i++)source[i*6]+=1.51f;
    assert(cr_contact_build(source,12,.02f,out,CR_CONTACT_MAX_VERTS)==0);
    for(i=0;i<12;i++)source[i*6+1]=0;
    assert(cr_contact_build(source,12,.02f,out,CR_CONTACT_MAX_VERTS)==0); /* coplanar */
    assert(cr_contact_build(source,12,0,out,CR_CONTACT_MAX_VERTS)==0);

    static route_mesh_t mesh;
    maneuver_state_t state;
    memset(&state,0,sizeof(state));state.icon=ICON_TURN;state.bap_geometry=1;
    int angle,max_count=0;
    for(angle=-180;angle<=180;angle+=5) {
        state.exit_angle=angle;state.direction=angle<0?-1:1;
        build_settled_mesh(&state,&mesh);
        count=cr_contact_build(mesh.verts,mesh.vert_count,ROUTE_TOP_Y-ROUTE_BASE_Y,out,CR_CONTACT_MAX_VERTS);
        assert(count>=0 && count==mesh.contact_count);
        if(count>max_count)max_count=count;
        if(angle==90 || angle==-90 || angle==0)assert(count==0);
        if(angle==160 || angle==-160)assert(count>0);
        for(i=0;i<count*6;i++)assert(isfinite(out[i]));
    }
    state.exit_angle=160;build_settled_mesh(&state,&mesh);
    clock_t start=clock();
    for(i=0;i<500;i++)cr_contact_build(mesh.verts,mesh.vert_count,ROUTE_TOP_Y-ROUTE_BASE_Y,out,CR_CONTACT_MAX_VERTS);
    printf("contact AO: lower receivers, boundary edges, 73 angles, max %d verts; %.3f ms/build (host test) PASS\n",
           max_count,1000.0*(clock()-start)/CLOCKS_PER_SEC/500);
}

static void test_visible_area(void) {
    cr_rect_t areas[]={{0,0,328,180},{59,27,210,153}};
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

static void test_live_scene_ownership(void) {
    maneuver_scene_provider_t provider={0};
    provider.handles=scene_handles;provider.build_route=scene_route;
    provider.paint_masks=scene_paint;provider.elevation=scene_elevation;
    cr_scene_configure_provider(&provider);maneuver_set_scene_provider(&provider);
    memset(&g_engine,0,sizeof(g_engine));
    g_engine.current_scene=cr_scene_create();g_engine.next_scene=cr_scene_create();
    assert(g_engine.current_scene && g_engine.next_scene);
    maneuver_state_t a={0},b={0},c={0};
    a.icon=ICON_TURN;a.exit_angle=45;a.direction=1;a.bap_geometry=1;
    b=a;b.exit_angle=160;c.icon=ICON_APPROACH;
    engine_apply_maneuver(&a,40);prepare_engine_scenes();
    assert(scene_handles(NULL,&g_engine.current));
    engine_apply_maneuver(&b,41);prepare_engine_scenes();
    assert(g_engine.current.exit_angle==45 && g_engine.next.exit_angle==160);
    assert(!scene_handles(NULL,&g_engine.next));
    assert(maneuver_builtin_elevation(&g_engine.next)>.05f);
    cr_scene_t *arriving=g_engine.next_scene;
    engine_apply_maneuver(&c,41.1);
    c.icon=ICON_TURN; // Caller storage cannot change the pending snapshot.
    assert(g_engine.pending.icon==ICON_APPROACH);
    g_route_animating=0;engine_tick(42);prepare_engine_scenes();
    assert(g_engine.current_scene==arriving && g_engine.current.exit_angle==160);
    assert(g_engine.next.icon==ICON_APPROACH && !g_engine.has_pending);
    c.exit_angle=-45;
    engine_refresh_maneuver(&c,42.1);prepare_engine_scenes();
    assert(scene_handles(NULL,&g_engine.next));
    assert(g_engine.current.exit_angle==160);
    maneuver_set_scene_provider(NULL);
    cr_scene_destroy(g_engine.current_scene);cr_scene_destroy(g_engine.next_scene);
    g_engine.current_scene=g_engine.next_scene=NULL;
    puts("Live scene ownership: current/next/pending, commit, refresh and immutable maneuvers PASS");
}

int main(void) {
    test_route_progress_projection();
    test_arrow_progress();
    test_progress_glow();
    test_progress_handoff();
    test_contact_shadow();
    test_visible_area();
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
    engine_refresh_maneuver(&s,30);
    assert(g_engine.phase==ENGINE_IDLE && g_engine.current.junction_angle_count==1);
    s.exit_angle=45;engine_apply_maneuver(&s,30);
    assert(g_engine.phase==ENGINE_PUSHING && g_engine.has_next);
    s.junction_angles[0]=-90;engine_refresh_maneuver(&s,30);
    assert(g_engine.next.junction_angles[0]==-90 && g_engine.current.exit_angle==-90);
    assert(!g_engine.has_pending); /* refresh must not enqueue another maneuver */
    s.icon=ICON_LANE_CHANGE;engine_apply_maneuver(&s,30);
    assert(g_engine.has_pending);
    s.direction=-1;engine_refresh_maneuver(&s,30);
    assert(g_engine.pending.direction==-1 && g_engine.next.icon==ICON_TURN);
    s.icon=ICON_NONE;engine_apply_maneuver(&s,30);
    assert(g_engine.phase==ENGINE_IDLE && !g_engine.has_pending && !g_engine.has_next);
    g_persp_deferred=1;g_persp_deferred_value=1;
    engine_tick(30);
    assert(!g_persp_deferred);
    s.icon=ICON_TURN;engine_apply_maneuver(&s,30);
    assert(g_engine.phase==ENGINE_IDLE && g_engine.current.icon==ICON_TURN);
    test_live_scene_ownership();
    puts("maneuver_parity_test: decoder, half-degree paths, icon families, refresh/coalescing and NONE transitions PASS");
    return 0;
}
