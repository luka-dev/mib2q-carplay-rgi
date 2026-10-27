/* Host preview of ordinary maneuver scenes.
 * Uses the production route engine, progress controller, shaders and road FBO. */
#define _DEFAULT_SOURCE
#define GL_SILENCE_DEPRECATION
#include <GLFW/glfw3.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <limits.h>
#include <unistd.h>
#include "platform.h"
#include "render.h"
#include "maneuver.h"
#include "arrow_progress.h"
#include "fixtures.h"
#include "lane_fixtures.h"
#include "../../maneuver_render/scene/scene.h"
static cr_scene_info_t capture_info;
extern float g_3d_offset_adjust;

typedef struct {
    int case_id,compact;
    maneuver_state_t maneuver;
    cr_scene_t *compiled;
} preview_scene_t;
static preview_scene_t current_scene,next_scene;
static int pending_case=-1,engine_phase; /* 0 idle, 1 push, 2 camera settle */
static cr_arrow_progress_t preview_progress;
static void prepare_scene(preview_scene_t *scene,int c,int compact) {
    scene->case_id=c;scene->compact=compact;scene->maneuver=sample_maneuver(c);
    if(!scene->compiled)scene->compiled=cr_scene_create();
    cr_scene_input_t input;memset(&input,0,sizeof(input));
    input.maneuver=scene->maneuver;input.generation=(uint64_t)c+1;
    cr_scene_view_t view;view.compact=compact;render_get_layout_matrix(view.projection);
    if(!cr_scene_prepare(scene->compiled,&input,&view)) {
        fprintf(stderr,"Scene preparation failed for fixture %d\n",c);exit(4);
    }
    capture_info=*cr_scene_info(scene->compiled);
}
static const preview_scene_t *provider_scene(const maneuver_state_t *m) {
    if(m==&current_scene.maneuver)return &current_scene;
    if(m==&next_scene.maneuver)return &next_scene;
    fprintf(stderr,"Unknown immutable scene snapshot\n");exit(9);
}
static void provider_route(void *context,const maneuver_state_t *m,route_path_t *out) {
    (void)context;*out=*cr_scene_route(provider_scene(m)->compiled);
}
static int provider_handles(void *context,const maneuver_state_t *m) {
    (void)context;return !cr_scene_is_native(provider_scene(m)->compiled);
}
static float provider_elevation(void *context,const maneuver_state_t *m) {
    (void)context;return cr_scene_info(provider_scene(m)->compiled)->route_elevation;
}
static void provider_paint(void *context,const maneuver_state_t *m,float tx,float ty,float cos_r,float sin_r) {
    (void)context;const preview_scene_t *scene=provider_scene(m);
    cr_scene_paint(scene->compiled,tx,ty,cos_r,sin_r);
    capture_info=*cr_scene_info(scene->compiled);
}
static int requested_case(void) {
    return pending_case>=0?pending_case:engine_phase==1?next_scene.case_id:current_scene.case_id;
}
static void request_case(int c,int compact) {
    if(engine_phase) {
        int destination=engine_phase==1?next_scene.case_id:current_scene.case_id;
        pending_case=c==destination?-1:c;return;
    }
    if(c==current_scene.case_id)return;
    prepare_scene(&next_scene,c,compact);
    cr_progress_begin_handoff(&preview_progress,glfwGetTime());
    maneuver_start_push();render_invalidate_masks();engine_phase=1;
}
static int tick_engine(int compact) {
    int changed=0;
    if(engine_phase==1 && !maneuver_is_pushing()) {
        preview_scene_t previous=current_scene;current_scene=next_scene;next_scene=previous;
        maneuver_commit_pushed_state(&current_scene.maneuver);
        cr_progress_finish_handoff(&preview_progress,glfwGetTime());
        render_invalidate_masks();engine_phase=2;changed=1;
    }
    if(engine_phase==2 && !maneuver_is_animating()) {
        engine_phase=0;
        if(pending_case>=0) {int c=pending_case;pending_case=-1;request_case(c,compact);}
    }
    return changed;
}
static void capture(const char *dir,int c,int compact,int width,int height) {
    char name[1024];unsigned char *p=malloc((size_t)width*height*4);int y,x,bad=0,excess=0;FILE *f;
    if(!p)exit(2);
    glReadPixels(0,0,width,height,GL_RGBA,GL_UNSIGNED_BYTE,p);
    /* The window compositor receives associated (premultiplied) RGBA, not
     * this script's RGB review sheet. Every color channel must be <= alpha.
     * Otherwise unassociation can amplify the fade into a bright fringe. */
    for(y=0;y<height;++y)for(x=0;x<width;++x) {
        unsigned char *q=&p[(y*width+x)*4];
        int peak=q[0]>q[1]?q[0]:q[1];if(q[2]>peak)peak=q[2];
        int over=peak-q[3];if(over>2)++bad;if(over>excess)excess=over;
    }
    printf("ALPHA %d %s invalid %d max_excess %d\n",c,compact?"crop":"wide",bad,excess);
    if(bad){fprintf(stderr,"Invalid premultiplied window pixels\n");free(p);exit(7);}
    snprintf(name,sizeof(name),"%s/%02d-%s.ppm",dir,c,compact?"crop":"wide");
    f=fopen(name,"wb");if(!f){perror(name);exit(2);}
    fprintf(f,"P6\n%d %d\n255\n",width,height);
    for(y=height-1;y>=0;--y)for(x=0;x<width;++x) {
        unsigned char rgb[3],*q=&p[(y*width+x)*4];int j;
        for(j=0;j<3;++j) {int v=q[j]+(21*(255-q[3]))/255;rgb[j]=(unsigned char)(v>255?255:v);}
        fwrite(rgb,1,3,f);
    }
    fclose(f);free(p);
}
int main(int argc,char **argv) {
    int width,height,c=0,compact=1,dirty=1,frame=0,animate=0,show_frame=1,show_lanes=1;
    int blink_frame=0;
    const char *out=argc==3 && !strcmp(argv[1],"--capture")?argv[2]:NULL;
    const char *lanes_out=argc==3 && !strcmp(argv[1],"--capture-lanes")?argv[2]:NULL;
    const char *reveal_out=argc==3 && !strcmp(argv[1],"--capture-lane-reveal")?argv[2]:NULL;
    int blink_with_lanes=argc==3 && !strcmp(argv[1],"--capture-blink-lanes");
    const char *blink_out=argc==3 && (!strcmp(argv[1],"--capture-blink") || blink_with_lanes)?argv[2]:NULL;
    int reveal_frame=0;
    int lane_case=-1;
    if(argc==3 && !strcmp(argv[1],"--lanes"))lane_case=atoi(argv[2]);
    if(lanes_out)lane_case=0;
    if(blink_with_lanes)lane_case=0;
    if(lane_case>=LANE_CASES || lane_case<-1)return 2;
    if(lane_case>=0)c=lane_maneuvers[lane_case];
    if(reveal_out)c=lane_maneuvers[7];
    cr_lane_panel_t *panel=cr_lane_panel_create();
    if(!panel)return 2;
    int lane_handoff=(argc==3 || argc==5) && !strcmp(argv[1],"--capture-lane-transition");
    int blink_handoff=(argc==3 || argc==5) && !strcmp(argv[1],"--capture-transition-blink");
    const char *handoff_out=(argc==3 || argc==5) && (!strcmp(argv[1],"--capture-transition") || lane_handoff || blink_handoff)?argv[2]:NULL;
    if(lane_handoff)lane_case=0;
    int handoff_current=argc==5?atoi(argv[3]):1,handoff_next=argc==5?atoi(argv[4]):2;
    int handoff_frame=0,handoff_settled_frames=0,handoff_warmup=0;
    double started;
    if(argc==3 && !strcmp(argv[1],"--case")){c=atoi(argv[2]);if(c<0 || c>=CASES)return 2;}
    if(blink_out){c=14;show_frame=0;show_lanes=blink_with_lanes;}
    if(handoff_out) {
        if(handoff_current<0 || handoff_current>=CASES || handoff_next<0 || handoff_next>=CASES)return 2;
        c=handoff_current;
    }
    if(platform_init(328,181))return 1;
    if(out || handoff_out || lanes_out || reveal_out || blink_out) {
        glfwHideWindow(glfwGetCurrentContext());
        /* Interactive preview keys must not abort a hidden capture midway. */
        glfwSetKeyCallback(glfwGetCurrentContext(),NULL);
    }
    platform_get_framebuffer_size(&width,&height);if(render_init(width,height))return 1;
    {char atlas[PATH_MAX];const char *slash=strrchr(argv[0],'/');
    if(slash)snprintf(atlas,sizeof(atlas),"%.*s/flag_atlas.rgba",(int)(slash-argv[0]),argv[0]);
    else snprintf(atlas,sizeof(atlas),"./flag_atlas.rgba");
    if(render_load_flag_atlas(atlas,128,128,14)) {
        fprintf(stderr,"Missing preview flag atlas: %s\n",atlas);return 2;
    }}
    maneuver_scene_provider_t provider={0};
    provider.build_route=provider_route;provider.paint_masks=provider_paint;provider.handles=provider_handles;provider.elevation=provider_elevation;
    cr_scene_configure_provider(&provider);
    maneuver_set_scene_provider(&provider);
    render_set_perspective(1);maneuver_set_slide(1);started=glfwGetTime();
    cr_progress_reset(&preview_progress);
    cr_progress_set(&preview_progress,4,CR_PROGRESS_FILL,started-1);
    cr_progress_tick(&preview_progress,started);
    while(!platform_should_close()) {
        platform_poll();
        double now=glfwGetTime();
        if(!out && !handoff_out && !lanes_out && !reveal_out && !blink_out) {
        if(platform_key_tap(CR_KEY_RIGHT))request_case((requested_case()+1)%CASES,compact);
        if(platform_key_tap(CR_KEY_LEFT))request_case((requested_case()+CASES-1)%CASES,compact);
        if(platform_key_tap(CR_KEY_UP)){compact=!compact;dirty=1;}
        if(platform_key_tap(CR_KEY_DOWN)) {
            lane_case=(lane_case+1)%LANE_CASES;
            request_case(lane_maneuvers[lane_case],compact);dirty=1;
        }
        if(platform_key_tap(CR_KEY_SPACE)){animate=!animate;started=glfwGetTime();}
        if(platform_key_tap(CR_KEY_D))show_frame=!show_frame;
        if(platform_key_tap(CR_KEY_P))show_lanes=!show_lanes;
        }
        if(tick_engine(compact)){c=current_scene.case_id;dirty=1;}
        if(dirty) {
            char title[256];
            /* Vertical lens shift: no change to focal length, tilt or distance. */
            g_3d_offset_adjust=-.16f;
            render_set_visible_area(compact?59:0,compact?27:0,compact?210:328,compact?153:180);
            snprintf(title,sizeof(title),"%s | %s | Up crop, Down lanes, P on/off",lane_case>=0?lane_names[lane_case]:fixtures[c].name,compact?"210x153":"328x180");
            glfwSetWindowTitle(glfwGetCurrentContext(),title);
            render_set_camera_pan(0,0);render_set_camera_rotation(0);render_set_light_rotation(0);
            prepare_scene(&current_scene,c,compact);
            if(engine_phase==1)prepare_scene(&next_scene,next_scene.case_id,compact);
            render_invalidate_masks();
            frame=0;
        }
        cr_lane_guidance_t lanes={0};
        if(reveal_out)lane_case=reveal_frame>=12 && reveal_frame<42?7:-1;
        if(lane_case>=0 && show_lanes)lanes=sample_lanes(lane_case);
        cr_rect_t panel_visible,panel_target;
        render_get_visible_area(NULL,&panel_target);
        cr_lane_panel_update(panel,&lanes,panel_target.w,now);
        float content_frame[3];
        cr_lane_panel_framing(panel,current_scene.compiled,engine_phase==1?next_scene.compiled:NULL,content_frame);
        render_set_content_framing(content_frame[0],content_frame[1],content_frame[2]);
        render_begin_frame();
        {int level=4,state=CR_PROGRESS_FILL;double t=fmod(now-started,8.0);
        /* Synthetic packets use the SAME target filter/handoff as production;
         * only this demo's packet source owns an illustrative blink clock. */
        if(animate) {
            level=16-(int)fmin(t*4,16);
            if(t>=4)state=fmod(t-4,1.2)<.6?CR_PROGRESS_BLINK_LOW:CR_PROGRESS_BLINK_HIGH;
        }
        if(blink_out) {
            static const int states[]={CR_PROGRESS_FILL,CR_PROGRESS_BLINK_HIGH,
                CR_PROGRESS_BLINK_LOW,CR_PROGRESS_BLINK_HIGH,CR_PROGRESS_OFF,CR_PROGRESS_FILL};
            int stage=blink_frame<12?0:1+(blink_frame-12)/8;
            state=states[stage>5?5:stage];level=4;
        }
        if(blink_handoff) {
            state=handoff_frame==0 && handoff_warmup>=5
                ?CR_PROGRESS_BLINK_HIGH:CR_PROGRESS_FILL;
            level=4;
        }
        cr_progress_set(&preview_progress,level,state,now);
        const maneuver_state_t *next=engine_phase==1?&next_scene.maneuver:NULL;
        render_set_route_progress(preview_progress.fill,preview_progress.path_weight,preview_progress.glow);
        maneuver_prepare_frame(&current_scene.maneuver,next);
        maneuver_draw(&current_scene.maneuver,next);}
        render_get_visible_area(&panel_visible,&panel_target);
        cr_lane_panel_draw(panel,panel_visible,now);
        if(show_frame)render_debug_visible_area();
        if(show_frame && lanes.showing && !compact)render_debug_small_area();
        render_end_frame();dirty=0;
        if(reveal_out) {
            float x,y,dolly;render_get_content_framing(&x,&y,&dolly);
            printf("FRAMING %d %.6f %.6f %.6f %u\n",reveal_frame,x,y,dolly,cr_scene_info(current_scene.compiled)->builds);
            capture(reveal_out,reveal_frame,1,width,height);
            if(++reveal_frame==60)break;
        }
        if(blink_out && blink_frame>=11) {
            int stage=blink_frame<12?0:1+(blink_frame-12)/8;
            printf("BLINK %d stage %d fill %.6f path %.6f glow %.6f\n",
                   blink_frame-11,stage,preview_progress.fill,
                   preview_progress.path_weight,preview_progress.glow);
            capture(blink_out,blink_frame-11,1,width,height);
        }
        if(blink_out && ++blink_frame==52)break;
        /* Match static captures' initial viewport/progress-map settling before
         * calling this the departure endpoint of a maneuver transition. */
        if(handoff_out && ++handoff_warmup>=12) {
            if(handoff_frame==0) {
                const route_path_t *route=cr_scene_route(current_scene.compiled);
                printf("EXIT_HEADING %.6f\n",route?remainderf(1.5707963268f-route->arrow_angle,6.2831853072f):0);
            }
            const cr_scene_info_t *a=cr_scene_info(current_scene.compiled),*b=cr_scene_info(next_scene.compiled);
            printf("CACHE %d builds %u %u paints %u %u\n",handoff_frame,a?a->builds:0,b?b->builds:0,a?a->paint_count:0,b?b->paint_count:0);
            float px,py,rot;render_get_camera_pose(&px,&py,&rot);
            printf("TRAVEL %d camera %.6f %.6f rotation %.6f phase %d fill %.6f handoff %d\n",handoff_frame,px,py,rot,engine_phase,preview_progress.fill,preview_progress.handoff);
            if(blink_handoff)
                printf("BLINK_TRAVEL %d path %.6f glow %.6f\n",handoff_frame,
                       preview_progress.path_weight,preview_progress.glow);
            capture(handoff_out,handoff_frame,1,width,height);
            if(handoff_frame==0)request_case(handoff_next,compact);
            else if(handoff_frame==2)request_case((handoff_next+1)%CASES,compact);
            else if(handoff_frame==3)request_case(handoff_next,compact);
            else if(!engine_phase && maneuver_is_presentationally_settled() &&
                    !render_is_animating() &&
                    !preview_progress.handoff && !preview_progress.active && !preview_progress.tint_active) {
                /* Keep the first fully idle draws too: the native light
                 * settle clears its timer after updating the current frame. */
                if(++handoff_settled_frames>=3) {
                    printf("HANDOFF frames %d current %d next %d engine native\n",handoff_frame+1,handoff_current,handoff_next);
                    break;
                }
            }
            if(++handoff_frame>900){fprintf(stderr,"Native transition did not settle\n");return 10;}
        }
        if(lanes_out && ++frame==12) {
            capture(lanes_out,lane_case,compact,width,height);
            if(compact){compact=0;dirty=1;}
            else {
                compact=1;dirty=1;
                if(++lane_case==LANE_CASES)break;
                c=lane_maneuvers[lane_case];
            }
        }
        if(out && ++frame==12) {
            capture(out,c,compact,width,height);
            printf("FRAME %d %s %s bounds %.2f %.2f %.2f %.2f camera %.2f %.2f mesh %08x\n",c,compact?"crop":"wide",capture_info.framed?"PASS":"FAIL",capture_info.bounds[0],capture_info.bounds[1],capture_info.bounds[2],capture_info.bounds[3],capture_info.camera_x,capture_info.camera_y,(unsigned)capture_info.mesh_hash);
            if(!capture_info.framed){fprintf(stderr,"Important geometry outside safe area\n");return 4;}
            if(compact){compact=0;dirty=1;}else{compact=1;dirty=1;if(++c==CASES)break;}
        }
        if(glGetError()!=GL_NO_ERROR){fprintf(stderr,"GL error\n");return 3;}
        platform_swap();
        /* Include drawing time in the 30Hz budget, especially while painting
         * two scenes. A fixed post-render sleep slowed handoffs unnecessarily. */
        double remaining=1.0/30.0-(glfwGetTime()-now);
        if(remaining>0)usleep((unsigned)(remaining*1000000));
    }
    cr_lane_panel_destroy(panel);
    maneuver_set_scene_provider(NULL);cr_scene_destroy(current_scene.compiled);cr_scene_destroy(next_scene.compiled);render_shutdown();platform_shutdown();return 0;
}
