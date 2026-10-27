#define GL_SILENCE_DEPRECATION
#include <GLFW/glfw3.h>
#include "render.h"
#include "lane_panel.h"
extern "C" {
#include "platform.h"
}
#include <cassert>
#include <cstdio>
#include <cmath>
int main() {
    assert(!platform_init(328,181));glfwHideWindow(glfwGetCurrentContext());
    int w,h;platform_get_framebuffer_size(&w,&h);assert(!render_init(w,h));
    cr_rect_t full={0,0,328,180},row={70,148,188,32};
    const float quad[]={0,0,328,0,328,180,0,0,328,180,0,180};
    render_begin_frame();render_begin_overlay(full);
    render_overlay_mesh(quad,6,0,0,.8f,.4f,.2f,.75f);
    render_overlay_cutout(row,8,12,1);
    render_end_overlay();render_end_frame();
    auto pixel=[&](int x,int y,unsigned char *out) {
        glReadPixels(x*w/328,h-1-y*h/181,1,1,GL_RGBA,GL_UNSIGNED_BYTE,out);
    };
    unsigned char clear[4],solid[4],fade[4];
    pixel(164,164,clear);pixel(164,124,solid);pixel(164,142,fade);
    for(int i=0;i<4;++i)assert(clear[i]==0); // actual transparency, not a black plate
    assert(solid[3]>185 && solid[3]<197);
    assert(fade[3]>30 && fade[3]<solid[3]-30);
    for(int i=0;i<3;++i) {
        assert(fade[i]<=fade[3]);
        assert(std::fabs(float(fade[i])*solid[3]-float(solid[i])*fade[3])<400);
    }
    int previous=256,distinct=0;
    for(int y=135;y<=151;++y) {
        unsigned char p[4];pixel(164,y,p);
        assert(p[3]<=previous);if(p[3]!=previous)++distinct;previous=p[3];
    }
    assert(distinct>=8);

    /* The VC crop stops at y=180, but the source texture is 181px tall.
     * Mask the final source row too: it must not resurrect the arrow below
     * the lane glyphs after downsampling/compositing. One lane must erase the
     * full source width in either stage and throughout stage interpolation. */
    cr_lane_guidance_t lanes={};lanes.showing=1;lanes.count=1;
    lanes.lanes[0].primary=0;
    cr_lane_panel_t *panel=cr_lane_panel_create();assert(panel);
    const cr_rect_t areas[]={{59,27,210,153},{0,0,328,180},{29.5f,13.5f,269,166.5f}};
    const cr_rect_t source={0,0,328,181};
    const float source_quad[]={0,0,328,0,328,181,0,0,328,181,0,181};
    for(cr_rect_t area:areas) {
        render_begin_frame();render_begin_overlay(source);
        render_overlay_mesh(source_quad,6,0,0,.8f,.4f,.2f,.75f);
        render_end_overlay();
        cr_lane_panel_update(panel,&lanes,area.w,0);
        cr_lane_panel_draw(panel,area,1);render_end_frame();
        for(int y=178;y<181;++y)for(int x=0;x<328;++x) {
            unsigned char p[4];pixel(x,y,p);
            for(int i=0;i<4;++i)assert(p[i]==0);
        }
        // The feather is vertical and identical at the sides and the centre.
        pixel(164,142,fade);
        for(int x=0;x<328;++x) {
            unsigned char p[4];pixel(x,142,p);
            for(int i=0;i<4;++i)assert(p[i]==fade[i]);
        }
    }
    cr_lane_panel_destroy(panel);
    assert(glGetError()==GL_NO_ERROR);
    render_shutdown();platform_shutdown();
    std::puts("Lane mask GPU: one lane erases full source width through final row in both stages, uniform vertical feather, premultiplication retained, no opaque background PASS");
}
