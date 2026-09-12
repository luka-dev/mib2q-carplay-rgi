/* Exercise the prototype's actual offset geometry without creating a window. */
#define main lane_preview_main
#include "preview.c"
#undef main

int main(void) {
    const int angles[] = {-90, -45, 45, 90};
    const float offsets[] = {
        -.672f, -.65f, -.412f, -.39f, -.152f, -.13f,
         .13f, .152f, .39f, .412f, .65f, .672f
    };
    int failed = 0;
    /* Allocation sizing must not send synthetic built-in states to a provider
     * whose immutable scene keys are current/next pointers only. */
    maneuver_scene_provider_t provider={NULL,provider_route,provider_paint,.002f,.008f,.85f,EXIT_ROAD_REACH};
    float mx,my;
    maneuver_set_scene_provider(&provider);
    maneuver_get_transition_mask_bounds(&mx,&my);
    if(mx<1 || my<1)++failed;
    maneuver_set_scene_provider(NULL);
    for (unsigned a = 0; a < sizeof(angles)/sizeof(*angles); ++a) {
        build_path(&path, angles[a], .26f, .90f, .05f);
        path.segs[0].y0 = -1.4f;
        rpath_densify(&path);
        for (unsigned o = 0; o < sizeof(offsets)/sizeof(*offsets); ++o) {
            float px, py;
            exit_point(0, offsets[o], &px, &py);
            for (int i = 1; i <= path.pt_count+1; ++i) {
                float x, y;
                exit_point(i, offsets[o], &x, &y);
                /* These turns travel monotonically forward, then left/right.
                 * Their offset boundaries must not reverse through the road. */
                if ((x-px)*(angles[a] > 0 ? 1 : -1) < -1e-5f || y < py-1e-5f) {
                    fprintf(stderr, "Backtracking contour: angle=%d offset=%.3f sample=%d\n",
                            angles[a], offsets[o], i);
                    ++failed;
                }
                px = x; py = y;
            }
        }
    }
    printf("48 left/right offset contours: %s (%d reversed segments)\n",
           failed ? "FAIL" : "PASS", failed);
    /* A shared fork's shoulder has matching left/right fillets, tangent to
     * both straight boundaries. Tangential turn-only departures need no cap. */
    for(int angle=45;angle<=90;angle+=45) {
        road_join_t mirrored[2];
        for(int side=-1;side<=1;side+=2) {
            build_path(&path,side*angle,side*.26f,.90f,.05f);
            road_join_t join=road_join(side*.39f,-side*.13f,side);
            mirrored[(side+1)/2]=join;
            if(!join.valid || join.radius<=.022f) {++failed;continue;}
            float start=side>0?PI:0,end=start+side*join.sweep;
            float x=join.cx+join.radius*cosf(end),y=join.cy+join.radius*sinf(end);
            float ex,ey;exit_point(path.pt_count,-side*.13f,&ex,&ey);
            float dx=cosf(path.arrow_angle),dy=sinf(path.arrow_angle);
            if(fabsf(join.cx+join.radius*cosf(start)-join.x)>1e-5f ||
               fabsf((x-ex)*dy-(y-ey)*dx)>1e-5f ||
               fabsf(cosf(end)*dx+sinf(end)*dy)>1e-5f)++failed;
            if(road_join(side*.13f,-side*.13f,side).valid)++failed;
        }
        if(fabsf(mirrored[0].cx+mirrored[1].cx)>1e-5f ||
           fabsf(mirrored[0].cy-mirrored[1].cy)>1e-5f ||
           fabsf(mirrored[0].radius-mirrored[1].radius)>1e-5f)++failed;
    }
    /* A common two/three-lane bend must keep even its innermost curb round. */
    for (unsigned a=0; a<sizeof(angles)/sizeof(*angles); ++a)
        for(int lanes=2;lanes<=3;++lanes)for(int primary=0;primary<lanes;++primary) {
            float lo=-.13f-primary*.26f,hi=.13f+(lanes-1-primary)*.26f;
            build_lane_path(angles[a],0,lanes,lo,hi);
            for(int i=0;i<path.seg_count;++i)if(path.segs[i].type==RSEG_ARC) {
                const route_seg_t *s=&path.segs[i];
                float sign=s->end_rad>=s->start_rad?1.f:-1.f;
                if(s->radius+sign*(lo-.022f)<.03f || s->radius+sign*(hi+.022f)<.03f)++failed;
            }
        }
    lr_state state;int first,last,lanes;
    sample(&state,11);lanes=lr_exit_group(&state,2,&first,&last);
    for(int i=0;i<5;++i)if(continues_forward(&state,i,first,last,lanes)!=(i>=3))++failed;
    sample(&state,0);lanes=lr_exit_group(&state,3,&first,&last);
    if(continues_forward(&state,3,first,last,lanes))++failed;
    sample(&state,12);lanes=lr_exit_group(&state,3,&first,&last);
    if(!continues_forward(&state,3,first,last,lanes))++failed;
    /* A turn-only exit exposes the neighboring through lane's new shoulder,
     * on both sides of the road. Shared straight+turn does not invent one. */
    const int edge_cases[]={0,3,4,9,11,12,1};
    const int edge_lane[] ={2,6,1,3, 3,-1,3};
    const int edge_side[] ={1,1,-1,-1,-1,0,1};
    for(unsigned c=0;c<sizeof(edge_cases)/sizeof(*edge_cases);++c) {
        sample(&state,edge_cases[c]);lr_window window=lr_choose(&state,3,-1);
        lanes=lr_exit_group(&state,window.anchor,&first,&last);
        for(int i=0;i<state.count;++i)for(int side=-1;side<=1;side+=2)
            if(through_branch_edge(&state,i,side,first,last,lanes)!=
               (i==edge_lane[c] && side==edge_side[c]))++failed;
    }
    sample(&state,1);lanes=lr_exit_group(&state,4,&first,&last);
    if(!recommended_exit(&state,3,first,last,lanes,45) ||
       !recommended_exit(&state,4,first,last,lanes,45))++failed;
    /* Permission for an optional turn is not a recommendation for that turn;
     * neither sentinel can acquire a tint through wrapped angle arithmetic. */
    const int other_primary[]={0,1000,-1000};
    for(unsigned i=0;i<sizeof(other_primary)/sizeof(*other_primary);++i) {
        state.lane[3].primary=other_primary[i];
        if(recommended_exit(&state,3,first,last,lanes,45))++failed;
    }
    state.lane[3].primary=45;state.lane[3].status=0;
    if(recommended_exit(&state,3,first,last,lanes,45))++failed;
    /* The unknown-angle fixture must still describe a consistent exit:
     * the outer lane also turns, without gaining recommendation status. */
    sample(&state,5);lanes=lr_exit_group(&state,3,&first,&last);
    if(lanes!=2 || first!=3 || last!=4 || state.lane[4].status!=0 ||
       !lr_has_direction(&state.lane[4],45) || lr_has_direction(&state.lane[4],0) ||
       lr_valid_angle(state.lane[0].primary) || lr_valid_angle(state.lane[1].primary))++failed;
    /* All fixtures using this simplified connected-road presentation have
     * a contiguous exit corridor to the matching physical edge. This is a
     * fixture sanity check, not a claim about every real road's topology. */
    for(int c=0;c<13;++c) {
        sample(&state,c);lr_window window=lr_choose(&state,3,-1);
        lanes=lr_exit_group(&state,window.anchor,&first,&last);
        if(lanes && ((state.lane[window.anchor].primary>0 && last!=state.count-1) ||
                     (state.lane[window.anchor].primary<0 && first!=0)))++failed;
    }
    const cr_lane_safety_result_t expected[]={CR_LANE_ROAD_UNCONFIRMED_EXIT,
        CR_LANE_ROAD_UNCONFIRMED_EXIT,CR_LANE_ROAD_INCOMPLETE,
        CR_LANE_ROAD_BAD_RECORDS,CR_LANE_ROAD_ACTION_MISMATCH};
    for(int c=13;c<CASES;++c) {
        sample(&state,c);lr_state before=state;lr_window window=lr_choose(&state,3,-1);
        maneuver_state_t m=sample_maneuver(c);
        if(lr_geometry_check(&state,window.anchor,1,m.exit_angle)!=expected[c-13] ||
           memcmp(&state,&before,sizeof(state)))++failed;
    }
    printf("Lane junctions: round inner curbs, turn-only termination and shared straight+turn %s\n",failed?"FAIL":"PASS");
    return failed ? 1 : 0;
}
