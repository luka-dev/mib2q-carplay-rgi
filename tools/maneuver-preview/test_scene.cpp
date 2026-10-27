#include "../../maneuver_render/scene/scene_internal.hpp"
#include "render.h"
#include "fixtures.h"
#include <cassert>
#include <initializer_list>
extern float g_3d_offset_adjust;
static void same_route(const route_path_t *a,const route_path_t *b) {
    assert(a && b && !std::memcmp(a,b,sizeof(*a)));
}
int main() {
    g_3d_offset_adjust=-.16f;
    cr_scene_view_t small={},wide={};small.compact=1;
    render_build_layout_matrix(small.projection,328.f/181);wide=small;wide.compact=0;
    cr_scene_t *a=cr_scene_create(),*b=cr_scene_create();assert(a && b);
    unsigned peak=0;
    for(int c=0;c<CASES;++c) {
        cr_scene_input_t in={};in.maneuver=sample_maneuver(c);in.generation=c+1;
        cr_scene_input_t original=in;
        assert(cr_scene_prepare(a,&in,&small));
        cr_scene_info_t info=*cr_scene_info(a);
        route_path_t owned=*cr_scene_route(a);
        if(!info.framed || info.fallback)std::fprintf(stderr,"Case %d: kind %d bounds %.2f %.2f %.2f %.2f\n",c,info.kind,info.bounds[0],info.bounds[1],info.bounds[2],info.bounds[3]);
        assert(info.framed && !info.fallback);
        if(info.command_count>peak)peak=info.command_count;
        for(int repeat=0;repeat<1000;++repeat)assert(cr_scene_prepare(a,&in,&small));
        assert(cr_scene_info(a)->builds==info.builds);
        assert(!std::memcmp(&original,&in,sizeof(in)));
        std::memset(&in,0x55,sizeof(in));same_route(&owned,cr_scene_route(a));
        assert(cr_scene_prepare(b,&original,&wide));
        auto other=cr_scene_info(b);
        assert(info.camera_x==other->camera_x && info.camera_y==other->camera_y && info.mesh_hash==other->mesh_hash);
        same_route(cr_scene_route(a),cr_scene_route(b));
        if(info.kind==CR_SCENE_NATIVE) {
            route_path_t native={};maneuver_build_route(&original.maneuver,&native);rpath_densify(&native);
            same_route(&native,cr_scene_route(a));
        }
        // Unused array tails never change geometry or invalidate the cache.
        original.maneuver.junction_angles[MAX_JUNCTION_ANGLES-1]=123;
        assert(cr_scene_prepare(a,&original,&small));assert(cr_scene_info(a)->builds==info.builds);
        ++original.generation;assert(cr_scene_prepare(a,&original,&small));
        assert(cr_scene_info(a)->builds==info.builds+1);same_route(&owned,cr_scene_route(a));
    }
    cr_scene_input_t bad={};bad.maneuver=sample_maneuver(14);
    for(float angle : {1000.f,-1000.f,NAN,INFINITY}) {
        bad.maneuver.exit_angle=angle;assert(!cr_scene_prepare(a,&bad,&small));assert(!cr_scene_route(a));
    }
    bad.maneuver=sample_maneuver(14);bad.maneuver.junction_angle_count=MAX_JUNCTION_ANGLES+1;
    assert(!cr_scene_prepare(a,&bad,&small));bad.maneuver=sample_maneuver(14);
    wide.projection[3]=NAN;assert(!cr_scene_prepare(a,&bad,&wide));
    assert(!cr_scene_prepare(a,nullptr,&small));assert(!cr_scene_route(a));
    assert(cr_scene_prepare(a,&bad,&small));assert(cr_scene_route(a));
    cr_scene_destroy(a);cr_scene_destroy(b);
    std::printf("%d ordinary scenes, %d cache hits, immutable snapshots, small/wide parity, native family parity and invalid inputs PASS; peak %u commands\n",CASES,CASES*1000,peak);
}
