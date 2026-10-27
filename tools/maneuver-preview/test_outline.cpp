#include "../../maneuver_render/scene/scene_internal.hpp"
#include <cassert>
#include <new>
static int provider_calls;
static void route_probe(void *,const maneuver_state_t *,route_path_t *) {++provider_calls;}
static void paint_probe(void *,const maneuver_state_t *,float,float,float,float) {++provider_calls;}
int main() {
    auto *b=new (std::calloc(1,sizeof(navigation::Builder))) navigation::Builder();
    assert(b);
    maneuver_scene_provider_t provider={};provider.build_route=route_probe;provider.paint_masks=paint_probe;
    cr_scene_configure_provider(&provider);maneuver_set_scene_provider(&provider);
    float mx,my;maneuver_get_transition_mask_bounds(&mx,&my);
    assert(mx>=1 && my>=1 && !provider_calls);maneuver_set_scene_provider(nullptr);
    const float half=navigation::style::RoadWidth*.5f,curb=navigation::style::ShoulderWidth;
    const float offsets[]={-half-curb,-half,half,half+curb};
    const int angles[]={-90,-45,0,45,90};
    for(int angle:angles) {
        maneuver_state_t m={};m.icon=ICON_TURN;m.exit_angle=angle;
        b->build_maneuver_path(&b->path,&m,0,.90f,.05f);
        b->path.segs[0].y0=-1.4f;rpath_densify(&b->path);
        for(float offset:offsets) {
            float px,py;b->exit_point(0,offset,&px,&py);
            for(int i=1;i<=b->path.pt_count+1;++i) {
                float x,y;b->exit_point(i,offset,&x,&y);
                assert(std::isfinite(x) && std::isfinite(y));
                assert((x-px)*(angle>0?1:-1)>=-1e-5f && y>=py-1e-5f);
                px=x;py=y;
            }
        }
    }
    b->~Builder();std::free(b);
    std::puts("20 ordinary curb contours and transition mask allocation isolation PASS");
}
