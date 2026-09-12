/* Host lane prototype; live lane transport is not connected.
 * Uses the production route engine, progress controller, shaders and road FBO. */
#define _DEFAULT_SOURCE
#define GL_SILENCE_DEPRECATION
#include <GLFW/glfw3.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <unistd.h>
#include "platform.h"
#include "render.h"
#include "maneuver.h"
#include "arrow_progress.h"
#include "lane_layout.h"
#define PI 3.14159265358979323846f
#define CASES 18
#define EXIT_SOLID_REACH .20f
#define EXIT_FADE_LENGTH .30f
#define EXIT_ROAD_REACH (EXIT_SOLID_REACH+EXIT_FADE_LENGTH)
static const char *names[CASES]={"Right exit / 4 lanes", "Two recommended / 5 lanes", "Fourth from real left edge", "Right exit / 8 lanes", "Left turn / 5 lanes", "Unknown lane direction", "No recommended lane", "Separated recommendations", "Two left-turn lanes / 5 lanes", "Three left-turn lanes / 5 lanes", "Three left / middle lane primary", "Three left / right lane primary", "Straight + right / shared lane", "Safety: crossing a through lane", "Safety: unknown outer lane", "Safety: incomplete lane snapshot", "Safety: missing lane position", "Safety: different maneuver action"};
static route_path_t path, glyph;
static route_mesh_t mesh;
static float camera_x,camera_y;
static int framing_ok;
static float frame_bounds[4];
#define MAX_KEYS (RMESH_MAX_VERTS+2048)
static float points[MAX_KEYS][3];
static int point_count;
static int visible_count;
static int hidden_side;
static float scene_secondary_width;
static uint32_t scene_mesh_hash;
static int scene_exit_lanes;
static float scene_glyph_clearance;
static float scene_head_min,scene_head_max;
static uint32_t scene_glyph_hash[LR_MAX_LANES];
static unsigned scene_glyph_seen;
static int provider_masks_only,provider_transform_active;
static float provider_tx,provider_ty,provider_cos=1,provider_sin;
static float forward_fade_start=.25f;
extern float g_3d_offset_adjust;

static void key_point(float x,float h,float y) {
    if(point_count>=MAX_KEYS){fprintf(stderr,"Framing point capacity exceeded\n");exit(4);}
    points[point_count][0]=x;points[point_count][1]=h;points[point_count++][2]=y;
}
static void projected_bounds(const float m[16],float px,float py,float b[4]) {
    int i;b[0]=b[1]=1e6f;b[2]=b[3]=-1e6f;
    for(i=0;i<point_count;++i) {
        float x=points[i][0]-px,h=points[i][1],y=points[i][2]-py;
        float w=m[3]*x+m[7]*h+m[11]*y+m[15];
        float sx=(1+(m[0]*x+m[4]*h+m[8]*y+m[12])/w)*164;
        float sy=(1-(m[1]*x+m[5]*h+m[9]*y+m[13])/w)*90.5f;
        if(sx<b[0])b[0]=sx;
        if(sy<b[1])b[1]=sy;
        if(sx>b[2])b[2]=sx;
        if(sy>b[3])b[3]=sy;
    }
}

/* Pan only: production FOV/eye/tilt and route thickness stay unchanged.
 * Fit importance bounds in the existing source-pixel rectangle, with 8px inset.
 * Camera depth translation is capped at 0.08 world units (~6% of eye distance). */
static void frame_scene(float arrow_x,int compact) {
    float m[16],best=1e9f,lo_x=compact?67:8,lo_y=compact?35:8;
    float hi_x=compact?261:320,hi_y=172;int ix,iy;
    render_set_camera_pan(0,0);render_sync_camera();render_get_camera_matrix(m);
    for(iy=-4;iy<=4;++iy)for(ix=-50;ix<=50;++ix) {
        float px=ix*.01f,py=iy*.02f,b[4],cost,overflow=0;
        projected_bounds(m,px,py,b);
        if(b[0]<lo_x)overflow+=lo_x-b[0];
        if(b[1]<lo_y)overflow+=lo_y-b[1];
        if(b[2]>hi_x)overflow+=b[2]-hi_x;
        if(b[3]>hi_y)overflow+=b[3]-hi_y;
        /* At most a small (~6px) compromise on primary-lane centering. */
        if(fabsf(px-arrow_x)>.04f)overflow+=(fabsf(px-arrow_x)-.04f)*100;
        cost=overflow*10000+fabsf(px-arrow_x)*100+fabsf(py)*12;
        if(cost<best){best=cost;camera_x=px;camera_y=py;framing_ok=overflow<.01f;memcpy(frame_bounds,b,sizeof(frame_bounds));}
    }
    render_set_camera_pan(camera_x,camera_y);render_sync_camera();
}

static void sample(lr_state *s,int c) {
    int i;
    if(c>=13) {
        sample(s,5);
        if(c==13){s->lane[4].primary=0;s->lane[4].angles[0]=0;}
        if(c==14){s->lane[4].primary=1000;s->lane[4].count=0;}
        if(c==15)s->complete=0;
        if(c==16)s->lane[4].position=7;
        lr_normalize(s);return;
    }
    memset(s,0,sizeof(*s));
    s->complete=1; /* Complete ordered synthetic snapshot, not inferred live. */
    s->count=c==0?4:c==2||c==3||c==7?8:5;
    for(i=0;i<s->count;++i) {
        s->lane[i].position=i;s->lane[i].primary=0;s->lane[i].count=1;
    }
    if(c==0 || c==3) {i=s->count-1;s->lane[i].primary=45;s->lane[i].status=2;s->lane[i].angles[0]=45;}
    if(c==1) {s->lane[3].status=1;s->lane[4].status=2;s->lane[3].primary=s->lane[4].primary=45;s->lane[3].count=2;s->lane[3].angles[1]=45;s->lane[4].angles[0]=45;}
    if(c==2) {s->lane[3].status=2;s->lane[4].status=1;}
    if(c==4) {s->lane[0].status=2;s->lane[0].primary=-90;s->lane[0].angles[0]=-90;}
    if(c==5) {
        s->lane[0].primary=1000;s->lane[0].count=0;
        s->lane[1].primary=-1000;s->lane[1].count=0;
        s->lane[3].status=2;s->lane[3].primary=45;s->lane[3].count=2;s->lane[3].angles[1]=45;
        /* The fixture's outer lane shares the exit, but is not recommended.
         * Leaving its default straight-only direction made the selected
         * inner lane appear to turn across a through lane. This is synthetic
         * input, not permission to infer missing directions in real data. */
        s->lane[4].primary=45;s->lane[4].angles[0]=45;
    }
    if(c==7) {s->lane[0].status=s->lane[7].status=2;}
    if(c>=8 && c<=11)for(i=0;i<(c==8?2:3);++i) {
        int primary=c==10?1:c==11?2:0;
        s->lane[i].status=i==primary?2:1;s->lane[i].primary=-90;s->lane[i].angles[0]=-90;
    }
    if(c==12){s->count=4;s->lane[3].status=2;s->lane[3].primary=45;s->lane[3].count=2;s->lane[3].angles[1]=45;}
    lr_normalize(s);
}
static maneuver_state_t sample_maneuver(int c) {
    /* Independent maneuver input: never change it to match a lane record. */
    maneuver_state_t m;memset(&m,0,sizeof(m));m.icon=ICON_TURN;
    if(c==0 || c==1 || c==3 || c==5 || c==12 || (c>=13 && c<=16))m.exit_angle=45;
    if(c==4 || (c>=8 && c<=11) || c==17)m.exit_angle=-90;
    return m;
}

/* Scale and translate existing TURN geometry, preserving its filleted shape. */
static void build_maneuver_path(route_path_t *p,const maneuver_state_t *m,float x,float scale,float y) {
    int i;
    maneuver_build_route(m,p);
    for(i=0;i<p->seg_count;++i) {
        route_seg_t *s=&p->segs[i];
        s->x0=s->x0*scale+x;s->x1=s->x1*scale+x;
        s->y0=s->y0*scale+y;s->y1=s->y1*scale+y;
        s->cx=s->cx*scale+x;s->cy=s->cy*scale+y;s->radius*=scale;
    }
    p->arrow_x=p->arrow_x*scale+x;p->arrow_y=p->arrow_y*scale+y;
    rpath_densify(p);
}
static void build_path(route_path_t *p,int angle,float x,float scale,float y) {
    maneuver_state_t m;memset(&m,0,sizeof(m));m.icon=ICON_TURN;m.exit_angle=angle;
    build_maneuver_path(p,&m,x,scale,y);
}
/* A lane bundle needs a common circular bend whose INNER curb still has a
 * positive radius. Offsetting the tiny standalone-arrow fillet made the
 * innermost lanes fold backwards or collapse into square corners. Keep the
 * approach tangent fixed; wider outer lanes finish their turn farther ahead. */
static void build_lane_path(int angle,float x,int lanes,float lo,float hi) {
    if(lanes<=1 || abs(angle)>100) {build_path(&path,angle,x,.90f,.05f);return;}
    float side=angle>0?1.f:-1.f,theta=abs(angle)*PI/180.f;
    float radius=.055f+(angle>0?hi:-lo),start_y=-.04f;
    float start=angle>0?PI:0,end=start-angle*PI/180.f;
    float cx=x+side*radius,ex=cx+radius*cosf(end),ey=start_y+radius*sinf(end);
    float leg=fmaxf(.04f,.45f-radius*tanf(theta*.5f));
    float heading=PI*.5f-angle*PI/180.f;
    rpath_clear(&path);
    rpath_add_line(&path,x,-1.4f,x,start_y);
    rpath_add_arc(&path,cx,start_y,radius,start,end);
    rpath_add_line(&path,ex,ey,ex+cosf(heading)*leg,ey+sinf(heading)*leg);
    rpath_set_arrow(&path,ex+cosf(heading)*leg,ey+sinf(heading)*leg,heading);
    rpath_densify(&path);
}
static int continues_forward(const lr_state *s,int index,int first,int last,int lanes) {
    return !lanes || index<first || index>last || lr_has_direction(&s->lane[index],0);
}
/* A departing turn-only neighbor exposes a new physical side of the through
 * branch. A shared straight+turn lane does not: its junction stays open. */
static int through_branch_edge(const lr_state *s,int index,int side,int first,int last,int lanes) {
    int neighbor=index+side;
    return lanes && neighbor>=0 && neighbor<s->count &&
           continues_forward(s,index,first,last,lanes) &&
           !continues_forward(s,neighbor,first,last,lanes);
}
static int recommended_exit(const lr_state *s,int index,int first,int last,int lanes,int angle) {
    const lr_lane *l=&s->lane[index];
    return lanes && index>=first && index<=last && l->status>0 &&
           lr_valid_angle(l->primary) && lr_difference(l->primary,angle)<5;
}
static void paint_path(const route_path_t *p,float width,float r,float g,float b,float a,int head) {
    int i;
    for(i=1;i<p->pt_count;++i)
        render_thick_line(p->px[i-1],p->py[i-1],p->px[i],p->py[i],width,r,g,b,a);
    if(head)render_arrowhead(p->arrow_x,p->arrow_y,p->arrow_angle,width*1.9f,r,g,b,a);
}
static uint32_t glyph_hash_value(uint32_t hash,float value) {
    /* Ignore subpixel floating-point summation noise when adding context. */
    uint32_t v=(uint32_t)(int)roundf(value*10000.f);
    for(int i=0;i<4;++i){hash=(hash^(v&255))*16777619u;v>>=8;}
    return hash;
}
static void lane_glyphs(const lr_lane *l,int anchor,float x,float lane_width,int collect) {
    int j,k,iteration;
    float scale=.24f*fminf(1,lane_width/.26f),shift=0,lo=0,hi=0;
    /* The large arrow covers its approach lane. Put an alternative straight
     * cue beyond the fork, on the actual through continuation. */
    float glyph_y=anchor && lr_has_direction(l,0) && lr_difference(l->primary,0)>=10?.18f:-.18f;
    /* Fit the whole multi-direction glyph together, preserving its common
     * stem. Include actual arrowhead corners, not just the centerline. */
    for(iteration=0;iteration<40;++iteration) {
        lo=1e6f;hi=-1e6f;
        for(j=0;j<l->count+1;++j) {
            int a=j<l->count?l->angles[j]:l->primary,dup=0;
            if(!lr_valid_angle(a))continue;
            for(k=0;k<j && k<l->count;++k)if(lr_difference(a,l->angles[k])<5)dup=1;
            if(dup || (anchor && lr_difference(a,l->primary)<5))continue;
            int active=l->status>0 && lr_difference(a,l->primary)<5;
            float width=fminf(active?.040f:.034f,(lane_width-.044f)/3.8f);
            float size=width*1.9f,dx,dy;
            build_path(&glyph,a,x,scale,glyph_y);dx=cosf(glyph.arrow_angle);dy=sinf(glyph.arrow_angle);
            for(k=0;k<glyph.pt_count;++k){lo=fminf(lo,glyph.px[k]-width*.5f);hi=fmaxf(hi,glyph.px[k]+width*.5f);}
            lo=fminf(lo,fminf(glyph.arrow_x+dx*size,glyph.arrow_x-fabsf(dy)*size*.885f));
            hi=fmaxf(hi,fmaxf(glyph.arrow_x+dx*size,glyph.arrow_x+fabsf(dy)*size*.885f));
        }
        if(hi<lo)return;
        if(hi-lo<=lane_width-.044f)break;
        scale*=.9f;
    }
    shift=x-(lo+hi)*.5f;
    scene_glyph_clearance=fminf(scene_glyph_clearance,(lane_width-(hi-lo))*.5f);
    if(hi-lo>lane_width-.044f+1e-5f){fprintf(stderr,"Glyph exceeds lane inset\n");exit(5);}
    for(j=0;j<l->count+1;++j) {
        int a=j<l->count?l->angles[j]:l->primary,dup=0;
        if(!lr_valid_angle(a))continue;
        for(k=0;k<j && k<l->count;++k)if(lr_difference(a,l->angles[k])<5)dup=1;
        if(dup || (anchor && lr_difference(a,l->primary)<5))continue;
        int active=l->status>0 && lr_difference(a,l->primary)<5;
        float width=fminf(active?.040f:.034f,(lane_width-.044f)/3.8f);
        build_path(&glyph,a,x+shift,scale,glyph_y);
        if(collect) {
            float dx=cosf(glyph.arrow_angle),dy=sinf(glyph.arrow_angle),size=width*1.9f;
            for(k=0;k<glyph.pt_count;++k) {
                key_point(glyph.px[k]-width*.5f,0,glyph.py[k]-width*.5f);
                key_point(glyph.px[k]+width*.5f,0,glyph.py[k]+width*.5f);
            }
            key_point(glyph.arrow_x+dx*size,0,glyph.arrow_y+dy*size);
            key_point(glyph.arrow_x-dy*size*.885f,0,glyph.arrow_y+dx*size*.885f);
            key_point(glyph.arrow_x+dy*size*.885f,0,glyph.arrow_y-dx*size*.885f);
        } else {
            int id=l->position;
            if(id>=0 && id<LR_MAX_LANES) {
                uint32_t h=(scene_glyph_seen&(1u<<id))?scene_glyph_hash[id]:2166136261u;
                h=glyph_hash_value(h,width);h=glyph_hash_value(h,glyph.arrow_angle);
                for(k=0;k<glyph.pt_count;++k) {
                    h=glyph_hash_value(h,glyph.px[k]);h=glyph_hash_value(h,glyph.py[k]);
                }
                h=glyph_hash_value(h,glyph.arrow_x);h=glyph_hash_value(h,glyph.arrow_y);
                scene_glyph_hash[id]=h;scene_glyph_seen|=1u<<id;
            }
            paint_path(&glyph,width,active?.52f:.60f,active?.83f:.65f,active?1.0f:.73f,1,1);
        }
    }
}
static void road_rect_from(float left,float width,float start,float r,float g,float b) {
    int j;
    render_rect(left,start,width,forward_fade_start-start,r,g,b,1);
    for(j=0;j<18;++j) {
        float t=(j+.5f)/18,alpha=1-t*t*(3-2*t);
        render_rect(left,forward_fade_start+j*.024f,width,.0241f,r,g,b,alpha);
    }
}
static void road_rect(float left,float width,float r,float g,float b) {
    road_rect_from(left,width,-1.4f,r,g,b);
}
static void exit_point(int i,float offset,float *x,float *y) {
    int n=path.pt_count;
    float dx,dy,len;
    if(i>=n) {
        dx=cosf(path.arrow_angle);dy=sinf(path.arrow_angle);
        float reach=EXIT_SOLID_REACH+(i-n)*EXIT_FADE_LENGTH;
        *x=path.arrow_x+dx*reach;*y=path.arrow_y+dy*reach;
    } else {
        int before=i?i-1:0,after=i+1<n?i+1:n-1;
        dx=path.px[after]-path.px[before];dy=path.py[after]-path.py[before];
        len=sqrtf(dx*dx+dy*dy);if(len<1e-6f){dx=0;dy=1;}else{dx/=len;dy/=len;}
        *x=path.px[i];*y=path.py[i];
    }
    float center_x=*x,center_y=*y;
    *x+=dy*offset;*y-=dx*offset;
    /* A wide road's inner offset can exceed the arrow fillet radius. Collapse
     * that arc to the offset tangents' intersection. Also trim the outgoing
     * line against this corner: a short exit can end before the corner, and
     * connecting it backwards paints a white spur across the approach. */
    for(int k=0;k<path.seg_count;++k) {
        const route_seg_t *s=&path.segs[k];
        float sign=s->end_rad>=s->start_rad?1.f:-1.f;
        if(s->type==RSEG_ARC && s->radius+sign*offset<=0) {
            float tx0=-sinf(s->start_rad)*sign,ty0=cosf(s->start_rad)*sign;
            float tx1=-sinf(s->end_rad)*sign,ty1=cosf(s->end_rad)*sign;
            float ex=s->cx+s->radius*cosf(s->end_rad);
            float ey=s->cy+s->radius*sinf(s->end_rad);
            float a=s->cx+s->radius*cosf(s->start_rad)+ty0*offset;
            float b=s->cy+s->radius*sinf(s->start_rad)-tx0*offset;
            float c=ex+ty1*offset,d=ey-tx1*offset;
            float det=tx0*ty1-ty0*tx1,mx=s->cx,my=s->cy;
            if(fabsf(det)>1e-5f) {
                float t=((c-a)*ty1-(d-b)*tx1)/det;
                mx=a+t*tx0;my=b+t*ty0;
            }
            int on_arc=fabsf(hypotf(center_x-s->cx,center_y-s->cy)-s->radius)<1e-5f;
            int on_exit=fabsf((center_x-ex)*ty1-(center_y-ey)*tx1)<1e-5f &&
                        (center_x-ex)*tx1+(center_y-ey)*ty1>=-1e-5f;
            if(on_arc || (on_exit && (*x-mx)*tx1+(*y-my)*ty1<0)) {
                *x=mx;*y=my;return;
            }
        }
    }
}
static void exit_fade_begin(void) {
    float dx=cosf(path.arrow_angle),dy=sinf(path.arrow_angle);
    render_set_mask_direction_fade(path.arrow_x+dx*EXIT_ROAD_REACH,
                                  path.arrow_y+dy*EXIT_ROAD_REACH,-dx,-dy,EXIT_FADE_LENGTH);
}
static void exit_fade_end(void) {
    render_set_mask_entry_fade(-1.4f,provider_transform_active?.40f:0);
}
typedef struct {
    int valid,side;
    float x,y,cx,cy,radius,sweep;
} road_join_t;
/* Round only a real intersection of the through edge with the departing
 * outer edge. Tangential departures already join smoothly. No new lane or
 * barrier is introduced: the tiny patch lies in the outside corner. */
static road_join_t road_join(float edge,float exit_offset,int side) {
    road_join_t join={0};float ax,ay;
    exit_point(0,exit_offset,&ax,&ay);
    for(int i=1;i<=path.pt_count;++i) {
        float bx,by;exit_point(i,exit_offset,&bx,&by);
        float dx=bx-ax,dy=by-ay;
        if(side*dx>1e-6f && side*(edge-ax)>=-1e-6f && side*(bx-edge)>=-1e-6f) {
            float length=hypotf(dx,dy),theta=atan2f(side*dx,dy);
            if(theta<.55f || theta>1.8f)return join;
            float t=fminf(1,fmaxf(0,(edge-ax)/dx));
            float radius=fminf(.040f,.10f*tanf(theta*.5f));
            if(radius<.030f)return join;
            float tangent=radius/tanf(theta*.5f),ex,ey;
            float y=ay+t*dy;
            exit_point(path.pt_count,exit_offset,&ex,&ey);
            if(((ex-edge)*dx+(ey-y)*dy)/length<tangent+.022f)return join;
            join.valid=1;join.side=side;join.x=edge;join.y=y;
            join.cx=edge+side*radius;join.cy=y+tangent;
            join.radius=radius;join.sweep=PI-theta;
            return join;
        }
        ax=bx;ay=by;
    }
    return join;
}
static void paint_road_join(const road_join_t *join,int outline) {
    if(!join->valid)return;
    float start=join->side>0?PI:0;
    for(int i=0;i<20;++i) {
        float a=start+join->side*join->sweep*i/20.f;
        float b=start+join->side*join->sweep*(i+1)/20.f;
        float ax=join->cx+join->radius*cosf(a),ay=join->cy+join->radius*sinf(a);
        float bx=join->cx+join->radius*cosf(b),by=join->cy+join->radius*sinf(b);
        if(outline) {
            float r=join->radius-.022f;
            float cx=join->cx+r*cosf(a),cy=join->cy+r*sinf(a);
            float dx=join->cx+r*cosf(b),dy=join->cy+r*sinf(b);
            render_triangle(ax,ay,cx,cy,dx,dy,.86f,.91f,.97f,1);
            render_triangle(ax,ay,dx,dy,bx,by,.86f,.91f,.97f,1);
        } else render_triangle(join->x,join->y,ax,ay,bx,by,.20f,.22f,.27f,1);
    }
}
static void exit_strip(float lo,float hi,float r,float g,float b,int dashed) {
    int i;
    for(i=1;i<=path.pt_count+1;++i) {
        float ax,ay,bx,by,cx,cy,dx,dy;
        exit_point(i-1,lo,&ax,&ay);exit_point(i-1,hi,&bx,&by);
        exit_point(i,hi,&cx,&cy);exit_point(i,lo,&dx,&dy);
        /* Curbs belong to the departing curve/leg, never a solid barrier
         * down an internal approach lane before the junction. */
        if(dashed==2 && i<path.pt_count && fabsf(path.px[i]-path.px[i-1])<1e-6f)continue;
        /* Keep pavement under the arrow head solid, then fade the complete
         * cross-section along its exit heading. Curbs, tint and dividers use
         * exactly the same plane; the route shader is unaffected. */
        if(i==path.pt_count+1)exit_fade_begin();
        if(dashed!=1) {
            render_triangle(ax,ay,bx,by,cx,cy,r,g,b,1);
            render_triangle(ax,ay,cx,cy,dx,dy,r,g,b,1);
        } else {
            /* Dashes are distances, not a sample-index switch: long straight
             * exit segments must not turn into a single solid separator. */
            float start=i>path.pt_count?path.total_length+EXIT_SOLID_REACH:path.dist[i-1];
            float end=i>=path.pt_count?path.total_length+EXIT_SOLID_REACH+(i-path.pt_count)*EXIT_FADE_LENGTH:path.dist[i];
            float pos=start,length=end-start;
            while(length>1e-6f && pos<end-1e-6f) {
                int cell=(int)floorf((pos+1e-6f)/.105f);
                float next=fminf(end,(cell+1)*.105f),t0=(pos-start)/length,t1=(next-start)/length;
                if(!(cell%2)) {
                    float x0=ax+(dx-ax)*t0,y0=ay+(dy-ay)*t0;
                    float x1=bx+(cx-bx)*t0,y1=by+(cy-by)*t0;
                    float x2=bx+(cx-bx)*t1,y2=by+(cy-by)*t1;
                    float x3=ax+(dx-ax)*t1,y3=ay+(dy-ay)*t1;
                    render_triangle(x0,y0,x1,y1,x2,y2,r,g,b,1);
                    render_triangle(x0,y0,x2,y2,x3,y3,r,g,b,1);
                }
                pos=next;
            }
        }
    }
    exit_fade_end();
}
static void lane_tint(float x,float width,float end) {
    int j;
    render_rect(x-width*.45f,-1.4f,width*.90f,fminf(.05f,end)+1.4f,.22f,.30f,.37f,1);
    for(j=0;j<14;++j) {
        float t=(j+.5f)/14,a=1-t*t*(3-2*t);
        if(.05f+j*.014f>=end)break;
        render_rect(x-width*.45f,.05f+j*.014f,width*.90f,.0141f,
                    .20f+.02f*a,.22f+.08f*a,.27f+.10f*a,1);
    }
}
static void lane_divider(float x,float end) {
    int j;
    for(j=0;j<7;++j) {
        float y=-1.15f+j*.21f;
        if(y>=end)break;
        render_thick_line(x,y,x,fminf(y+.095f,end),.010f,.57f,.62f,.68f,1);
    }
}
static void continuation(const lr_state *s,lr_window w,float left,float right,int first,int last,int lanes,float junction,float lane_width) {
    if(w.hidden_left || w.hidden_right) {
        int side=w.hidden_left?-1:1;
        int index=side<0?w.first-1:w.first+w.count;
        /* A hidden neighbor uses the SAME packed lane geometry as it will
         * have when revealed. Only its opacity differs between stages. */
        float edge=side<0?left:right,span=lane_width*(.23f/.26f),center=edge+side*lane_width*.5f;
        int forward=continues_forward(s,index,first,last,lanes);
        float end=forward?1.f:junction;
        /* Show an actual adjacent lane, with its own status/directions, fading
         * across its width. It stays parallel: this is not an invented exit. */
        render_set_mask_side_fade(edge,side*span);
        if(forward)road_rect(side<0?edge-span:edge,span,.20f,.22f,.27f);
        else render_rect(side<0?edge-span:edge,-1.4f,span,junction+1.4f,.20f,.22f,.27f,1);
        if(s->lane[index].status>0)lane_tint(center,lane_width,end);
        lane_glyphs(&s->lane[index],0,center,lane_width,0);
        render_set_mask_side_fade(0,0);
        /* A lane separator, not a false solid shoulder at the window boundary. */
        lane_divider(edge,fminf(end,junction));
    }
}
static void provider_mask_begin(void) {
    if(provider_transform_active) {
        render_push_mask_transform(provider_tx-provider_cos*camera_x+provider_sin*camera_y,
                                   provider_ty-provider_sin*camera_x-provider_cos*camera_y,
                                   provider_cos,provider_sin);
        render_set_mask_entry_fade(-1.4f,.40f);
    }
}
static void provider_mask_end(void) {
    if(provider_transform_active) {
        render_pop_mask_transform();render_set_mask_entry_fade(0,0);
    }
}
static void present_scene(float fill,float blink) {
    if(provider_masks_only)return;
    render_set_camera_pan(camera_x,camera_y);render_sync_camera();
    render_composite();
    render_set_route_progress(fill,blink<0?1:0,blink<0?0:blink);
    rpath_draw(&mesh,0,.65f,1,1);
}
static void draw_scene(const lr_state *s,lr_window w,int compact,float fill,float blink,int mask_dirty,float secondary_width) {
    int i,exit_first=0,exit_last=0;float widths[LR_MAX_LANES],centers[LR_MAX_LANES],edges[LR_MAX_LANES+1];
    float total=0,left,ax=0;
    int exit_lanes=lr_exit_group(s,w.anchor,&exit_first,&exit_last);
    float exit_lo=-.13f+(exit_first-w.anchor)*secondary_width;
    float exit_hi=.13f+(exit_last-w.anchor)*secondary_width;
    for(i=0;i<w.count;++i) {widths[i]=w.first+i==w.anchor?.26f:secondary_width;total+=widths[i];}
    left=-total*.5f;edges[0]=left;
    for(i=0;i<w.count;++i) {
        centers[i]=edges[i]+widths[i]*.5f;edges[i+1]=edges[i]+widths[i];
        if(w.first+i==w.anchor)ax=centers[i];
    }
    float angle=w.anchor<0?0:(float)s->lane[w.anchor].primary;
    if(mask_dirty) {
        build_lane_path((int)angle,ax,exit_lanes,exit_lo,exit_hi);
        /* Extend the entry beyond the visible lower edge for even progress. */
        path.segs[0].y0=-1.4f;rpath_densify(&path);
        /* Lane overlay, not the old floating standalone maneuver. Its extra
         * .035 lift + .060 top projected the head into the adjacent exit lane.
         * Keep a shallow 3D prism, with unchanged lateral width and path. */
        rpath_set_elevation(0,0);rpath_set_ramp_restart(-1);
        rpath_extrude(&path,&mesh,.14f,.002f,.008f,0,1);
        point_count=0;
        scene_glyph_clearance=1;
        if(w.anchor>=0)for(i=0;i<mesh.vert_count;++i) {
            const float *v=mesh.verts+i*6;
            if(v[2]>=-.38f)key_point(v[0],v[1],v[2]);
        }
        for(i=0;i<w.count;++i) {
            float x=centers[i];
            key_point(x-.02f,0,-.18f);key_point(x+.02f,0,-.18f);
            lane_glyphs(&s->lane[w.first+i],w.first+i==w.anchor,x,widths[i],1);
        }
        /* The real edge must remain visible next to the countable lane glyphs. */
        if(!w.hidden_left){key_point(left-.022f,0,-.32f);key_point(left-.022f,0,.10f);}
        if(!w.hidden_right){key_point(edges[w.count]+.022f,0,-.32f);key_point(edges[w.count]+.022f,0,.10f);}
        /* The inner part of the continuation stays readable; only its outer
         * transparent tail is secondary context allowed beyond the crop. */
        if(w.hidden_left)key_point(left-.045f,0,-.18f);
        if(w.hidden_right)key_point(edges[w.count]+.045f,0,-.18f);
        /* The primary composition ALWAYS targets the small red crop. Stage
         * size never selects another camera, route scale or primary lane. */
        frame_scene(ax,1);
        /* Keep readable arrow/glyph sizes. Sacrifice secondary context before
         * considering a wider camera, which this preview never does. */
        if(!framing_ok && w.count>1) {
            lr_window narrower=lr_choose(s,w.count-1,w.anchor<0?-1:s->lane[w.anchor].position);
            if(narrower.count<w.count) {
                draw_scene(s,narrower,compact,fill,blink,1,secondary_width);
                return;
            }
            /* Never remove a lane between primary and the real edge. */
            if(secondary_width>.141f) {
                draw_scene(s,w,compact,fill,blink,1,secondary_width-.02f);
                return;
            }
        }
        if(framing_ok) {
            /* Use remaining room in EITHER viewport before fading a neighbor.
             * The small-frame fit above remains the canonical composition;
             * append/prepend context without moving or resizing any lane. */
            lr_window canonical=w,best=w;
            float canonical_left=left,m[16],best_left=left,best_bounds[4];
            int base_keys=point_count,requested;
            memcpy(best_bounds,frame_bounds,sizeof(best_bounds));
            render_get_camera_matrix(m);
            for(requested=w.count+1;requested<=s->count;++requested) {
                lr_window candidate=lr_choose(s,requested,w.anchor<0?-1:s->lane[w.anchor].position);
                float candidate_left=canonical_left-(canonical.first-candidate.first)*secondary_width;
                float edge=candidate_left,b[4];
                point_count=base_keys;
                for(i=0;i<candidate.count;++i) {
                    int index=candidate.first+i;
                    float width=index==candidate.anchor?.26f:secondary_width;
                    if(index<canonical.first || index>=canonical.first+canonical.count) {
                        key_point(edge+width*.5f,0,-.18f);
                        lane_glyphs(&s->lane[index],0,edge+width*.5f,width,1);
                    }
                    edge+=width;
                }
                if(!candidate.hidden_left)key_point(candidate_left-.022f,0,-.32f);
                if(!candidate.hidden_right)key_point(edge+.022f,0,-.32f);
                projected_bounds(m,0,0,b);
                if(b[0]<(compact?67:8) || b[1]<(compact?35:8) ||
                   b[2]>(compact?261:320) || b[3]>172)break;
                best=candidate;best_left=candidate_left;memcpy(best_bounds,b,sizeof(b));
            }
            w=best;left=best_left;total=0;edges[0]=left;
            for(i=0;i<w.count;++i) {
                widths[i]=w.first+i==w.anchor?.26f:secondary_width;
                centers[i]=edges[i]+widths[i]*.5f;edges[i+1]=edges[i]+widths[i];
                total+=widths[i];
            }
            memcpy(frame_bounds,best_bounds,sizeof(frame_bounds));
        }
        visible_count=w.count;scene_secondary_width=secondary_width;
        scene_exit_lanes=exit_lanes;
        scene_head_min=scene_head_max=0;
        if(exit_lanes) {
            float m[16];render_get_camera_matrix(m);
            for(i=0;i<mesh.vert_count;++i)if(mesh.path_dist[i]>=path.total_length-1e-5f) {
                const float *v=mesh.verts+i*6;float gx,gz,offset;
                if(!lr_project_ground(m,v[0],v[1],v[2],&gx,&gz))exit(6);
                offset=(gx-path.arrow_x)*sinf(path.arrow_angle)-(gz-path.arrow_y)*cosf(path.arrow_angle);
                scene_head_min=fminf(scene_head_min,offset);scene_head_max=fmaxf(scene_head_max,offset);
            }
            if(scene_head_min<-.1301f || scene_head_max>.1301f) {
                fprintf(stderr,"Projected head leaves its exit lane: %.5f %.5f\n",scene_head_min,scene_head_max);exit(6);
            }
        }
        {const unsigned char *bytes=(const unsigned char *)mesh.verts;size_t j;
        scene_mesh_hash=2166136261u;
        for(j=0;j<(size_t)mesh.vert_count*6*sizeof(float);++j)scene_mesh_hash=(scene_mesh_hash^bytes[j])*16777619u;}
        hidden_side=w.hidden_left?-1:w.hidden_right?1:0;
        float junction=exit_lanes?path.segs[0].y1:.25f;
        road_join_t join={0};forward_fade_start=.25f;
        if(exit_lanes) {
            int side=angle>0?1:-1,through=-1;
            for(i=0;i<w.count;++i)
                if(continues_forward(s,w.first+i,exit_first,exit_last,exit_lanes) &&
                   (through<0 || side>0))through=i;
            if(through>=0) {
                float edge=side>0?edges[through+1]:edges[through];
                join=road_join(edge,side>0?exit_lo:exit_hi,side);
                if(join.valid)forward_fade_start=fmaxf(.25f,join.cy+.040f);
            }
        }
        scene_glyph_seen=0;memset(scene_glyph_hash,0,sizeof(scene_glyph_hash));
        provider_mask_begin();
        render_begin_outline_mask();
        /* Outline the union, not the individual overlapping roads: thin
         * contours go FIRST, then every asphalt surface covers interior edges.
         * Never paint a whole white road-shaped backing surface. */
        if(exit_lanes) {
            exit_strip(exit_lo-.022f,exit_lo,.86f,.91f,.97f,2);
            exit_strip(exit_hi,exit_hi+.022f,.86f,.91f,.97f,2);
        }
        if(!w.hidden_left) {
            if(continues_forward(s,w.first,exit_first,exit_last,exit_lanes))
                road_rect(left-.022f,.022f,.86f,.91f,.97f);
            else render_rect(left-.022f,-1.4f,.022f,junction+1.4f,.86f,.91f,.97f,1);
        }
        if(!w.hidden_right) {
            if(continues_forward(s,w.first+w.count-1,exit_first,exit_last,exit_lanes))
                road_rect(edges[w.count],.022f,.86f,.91f,.97f);
            else render_rect(edges[w.count],-1.4f,.022f,junction+1.4f,.86f,.91f,.97f,1);
        }
        for(i=0;i<w.count;++i) {
            int index=w.first+i;
            if(through_branch_edge(s,index,-1,exit_first,exit_last,exit_lanes))
                road_rect_from(edges[i]-.022f,.022f,junction,.86f,.91f,.97f);
            if(through_branch_edge(s,index,1,exit_first,exit_last,exit_lanes))
                road_rect_from(edges[i+1],.022f,junction,.86f,.91f,.97f);
        }
        paint_road_join(&join,1);
        render_end_outline_mask();render_begin_fill_mask();
        if(exit_lanes)exit_strip(exit_lo,exit_hi,.20f,.22f,.27f,0);
        /* Turning separators also sit underneath any through-road pavement. */
        if(exit_lanes>1)for(i=exit_first+1;i<=exit_last;++i) {
            float offset=i<=w.anchor?-.13f+(i-w.anchor)*secondary_width:
                                      .13f+(i-w.anchor-1)*secondary_width;
            exit_strip(offset-.005f,offset+.005f,.57f,.62f,.68f,1);
        }
        /* Turn-only lanes end their straight approach at the common tangent.
         * A straight+turn lane legitimately contributes BOTH road surfaces. */
        render_rect(left,-1.4f,total,junction+1.4f,.20f,.22f,.27f,1);
        for(i=0;i<w.count;++i) {
            int index=w.first+i;
            if(continues_forward(s,index,exit_first,exit_last,exit_lanes))
                road_rect(edges[i],widths[i],.20f,.22f,.27f);
        }
        paint_road_join(&join,0);
        continuation(s,w,left,edges[w.count],exit_first,exit_last,exit_lanes,junction,secondary_width);
        for(i=0;i<w.count;++i) {
            int index=w.first+i;
            const lr_lane *l=&s->lane[index];
            int forward=continues_forward(s,index,exit_first,exit_last,exit_lanes);
            int selected_turn=recommended_exit(s,index,exit_first,exit_last,exit_lanes,(int)angle);
            if(l->status>0)lane_tint(centers[i],widths[i],forward && !selected_turn?1.f:junction);
        }
        /* Carry the recommended pavement through its actual bend to the end
         * of the exit. The old straight-only tint faded at y=.05 even when
         * the recommendation followed a departing lane. Paint inside each
         * lane, after the road union, before its direction glyphs. */
        for(i=0;i<w.count;++i) {
            int index=w.first+i;
            if(recommended_exit(s,index,exit_first,exit_last,exit_lanes,(int)angle)) {
                float offset=centers[i]-ax,half=widths[i]*.45f;
                exit_strip(offset-half,offset+half,.22f,.30f,.37f,0);
            }
        }
        for(i=0;i<w.count;++i) {
            int index=w.first+i;
            lane_glyphs(&s->lane[index],index==w.anchor,centers[i],widths[i],0);
        }
        for(i=1;i<w.count;++i) {
            int both=continues_forward(s,w.first+i-1,exit_first,exit_last,exit_lanes) &&
                     continues_forward(s,w.first+i,exit_first,exit_last,exit_lanes);
            lane_divider(edges[i],both?1.f:junction);
        }
        render_end_fill_mask();
        render_begin_route_mask();
        /* Empty route mask: use the same mesh/contact shading as the production
         * maneuver path. Its legacy mask shadow is not enabled there either. */
        render_end_route_mask();
        provider_mask_end();
    }
    present_scene(fill,blink);
}
static void fallback_road(const route_path_t *road,float width,float r,float g,float b) {
    paint_path(road,width,r,g,b,1,0);
    for(int i=1;i+1<road->pt_count;++i)
        render_disc(road->px[i],road->py[i],width*.5f,16,r,g,b,1);
    float dx=cosf(path.arrow_angle),dy=sinf(path.arrow_angle);
    exit_fade_begin();
    render_thick_line(path.arrow_x+dx*EXIT_SOLID_REACH,path.arrow_y+dy*EXIT_SOLID_REACH,
                      path.arrow_x+dx*EXIT_ROAD_REACH,path.arrow_y+dy*EXIT_ROAD_REACH,
                      width,r,g,b,1);
    exit_fade_end();
}
static void draw_fallback(const maneuver_state_t *maneuver,float fill,float blink,int dirty) {
    /* Use only the independent maneuver. No lane widths/counts/positions,
     * lane glyphs or inferred branch connectivity survive into this view. */
    if(dirty) {
        build_maneuver_path(&path,maneuver,0,.90f,.05f);
        path.segs[0].y0=-1.4f;rpath_densify(&path);
        rpath_set_elevation(0,0);rpath_set_ramp_restart(-1);
        rpath_extrude(&path,&mesh,.14f,.002f,.008f,0,1);
        point_count=0;
        for(int i=0;i<mesh.vert_count;++i) {
            const float *v=mesh.verts+i*6;
            if(v[2]>=-.38f)key_point(v[0],v[1],v[2]);
        }
        frame_scene(0,1);
        visible_count=hidden_side=scene_exit_lanes=0;scene_secondary_width=0;
        scene_glyph_seen=0;memset(scene_glyph_hash,0,sizeof(scene_glyph_hash));
        scene_glyph_clearance=1;scene_head_min=scene_head_max=0;
        const unsigned char *bytes=(const unsigned char *)mesh.verts;
        scene_mesh_hash=2166136261u;
        for(size_t i=0;i<(size_t)mesh.vert_count*6*sizeof(float);++i)
            scene_mesh_hash=(scene_mesh_hash^bytes[i])*16777619u;
        provider_mask_begin();
        render_begin_outline_mask();
        /* Generic maneuver road, with no lane markings or lane-count claim. */
        route_path_t road=path;
        rpath_add_line(&road,path.arrow_x,path.arrow_y,
                       path.arrow_x+EXIT_SOLID_REACH*cosf(path.arrow_angle),path.arrow_y+EXIT_SOLID_REACH*sinf(path.arrow_angle));
        rpath_densify(&road);
        fallback_road(&road,.27f,.86f,.91f,.97f);
        fallback_road(&road,.23f,.20f,.22f,.27f);
        render_end_outline_mask();
        render_begin_route_mask();render_end_route_mask();
        provider_mask_end();
    }
    present_scene(fill,blink);
}
static void draw_fixture(int c,int compact,float fill,float blink,int dirty) {
    lr_state s;sample(&s,c);lr_window w=lr_choose(&s,3,-1);
    maneuver_state_t m=sample_maneuver(c);
    if(lr_geometry_check(&s,w.anchor,m.icon==ICON_TURN,m.exit_angle)==CR_LANE_ROAD_OK)
        draw_scene(&s,w,compact,fill,blink,dirty,.26f);
    else draw_fallback(&m,fill,blink,dirty);
}
typedef struct {
    int case_id,compact;
    maneuver_state_t maneuver;
    route_path_t route;
} preview_scene_t;
static preview_scene_t current_scene,next_scene;
static int pending_case=-1,engine_phase; /* 0 idle, 1 push, 2 camera settle */
static cr_arrow_progress_t preview_progress;
static void prepare_scene(preview_scene_t *scene,int c,int compact) {
    scene->case_id=c;scene->compact=compact;scene->maneuver=sample_maneuver(c);
    provider_masks_only=1;
    render_set_camera_rotation(0);
    draw_fixture(c,compact,.72f,-1,1);
    scene->route=path;
    /* The engine adds its own pre/post animation extensions. Supply the
     * nominal maneuver entry, not the preview's offscreen -1.4 fill extension;
     * otherwise the next junction is placed an extra road-length away. */
    scene->route.segs[0].y0=-.55f;
    for(int i=0;i<scene->route.seg_count;++i) {
        route_seg_t *s=&scene->route.segs[i];
        s->x0-=camera_x;s->x1-=camera_x;s->cx-=camera_x;
        s->y0-=camera_y;s->y1-=camera_y;s->cy-=camera_y;
    }
    scene->route.arrow_x-=camera_x;scene->route.arrow_y-=camera_y;
    rpath_densify(&scene->route);
    provider_masks_only=0;
}
static const preview_scene_t *provider_scene(const maneuver_state_t *m) {
    if(m==&current_scene.maneuver)return &current_scene;
    if(m==&next_scene.maneuver)return &next_scene;
    fprintf(stderr,"Unknown immutable scene snapshot\n");exit(9);
}
static void provider_route(void *context,const maneuver_state_t *m,route_path_t *out) {
    (void)context;*out=provider_scene(m)->route;
}
static void provider_paint(void *context,const maneuver_state_t *m,float tx,float ty,float cos_r,float sin_r) {
    (void)context;const preview_scene_t *scene=provider_scene(m);
    provider_tx=tx;provider_ty=ty;provider_cos=cos_r;provider_sin=sin_r;
    provider_masks_only=provider_transform_active=1;
    render_set_camera_rotation(0);
    draw_fixture(scene->case_id,scene->compact,.72f,-1,1);
    provider_masks_only=provider_transform_active=0;
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
        current_scene=next_scene;
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
    lr_state s;lr_window w;int width,height,c=0,compact=1,dirty=1,frame=0,animate=0,show_frame=1;
    maneuver_state_t maneuver;cr_lane_safety_result_t safety=CR_LANE_ROAD_INCOMPLETE;
    const char *out=argc==3 && !strcmp(argv[1],"--capture")?argv[2]:NULL;
    const char *handoff_out=(argc==3 || argc==5) && !strcmp(argv[1],"--capture-transition")?argv[2]:NULL;
    int handoff_current=argc==5?atoi(argv[3]):1,handoff_next=argc==5?atoi(argv[4]):2;
    int handoff_frame=0,handoff_settled_frames=0;
    double started;
    if(argc==3 && !strcmp(argv[1],"--case")){c=atoi(argv[2]);if(c<0 || c>=CASES)return 2;}
    if(handoff_out) {
        if(handoff_current<0 || handoff_current>=CASES || handoff_next<0 || handoff_next>=CASES)return 2;
        c=handoff_current;
    }
    if(platform_init(328,181))return 1;
    if(out || handoff_out)glfwHideWindow(glfwGetCurrentContext());
    platform_get_framebuffer_size(&width,&height);if(render_init(width,height))return 1;
    maneuver_scene_provider_t provider={NULL,provider_route,provider_paint,.002f,.008f,.85f,EXIT_ROAD_REACH};
    maneuver_set_scene_provider(&provider);
    render_set_perspective(1);maneuver_set_slide(1);started=glfwGetTime();
    cr_progress_reset(&preview_progress);
    cr_progress_set(&preview_progress,4,CR_PROGRESS_FILL,started-1);
    cr_progress_tick(&preview_progress,started);
    while(!platform_should_close()) {
        platform_poll();
        double now=glfwGetTime();
        if(!out && !handoff_out) {
        if(platform_key_tap(CR_KEY_RIGHT))request_case((requested_case()+1)%CASES,compact);
        if(platform_key_tap(CR_KEY_LEFT))request_case((requested_case()+CASES-1)%CASES,compact);
        if(platform_key_tap(CR_KEY_UP)){compact=!compact;dirty=1;}
        if(platform_key_tap(CR_KEY_SPACE)){animate=!animate;started=glfwGetTime();}
        if(platform_key_tap(CR_KEY_D))show_frame=!show_frame;
        }
        if(tick_engine(compact)){c=current_scene.case_id;dirty=1;}
        if(dirty) {
            char title[256];sample(&s,c);w=lr_choose(&s,3,-1);
            maneuver=sample_maneuver(c);
            safety=lr_geometry_check(&s,w.anchor,maneuver.icon==ICON_TURN,maneuver.exit_angle);
            /* Vertical lens shift: no change to focal length, tilt or distance. */
            g_3d_offset_adjust=-.16f;
            render_set_visible_area(compact?59:0,compact?27:0,compact?210:328,compact?153:180);
            snprintf(title,sizeof(title),"Lane road: %s | %s | L/R case, Up crop, D frame",names[c],compact?"210x153":"328x180");
            glfwSetWindowTitle(glfwGetCurrentContext(),title);
            render_set_camera_pan(0,0);render_set_camera_rotation(0);render_set_light_rotation(0);
            prepare_scene(&current_scene,c,compact);
            if(engine_phase==1)prepare_scene(&next_scene,next_scene.case_id,compact);
            render_invalidate_masks();
            frame=0;
        }
        render_begin_frame();
        {int level=4,state=CR_PROGRESS_FILL;double t=fmod(now-started,8.0);
        /* Synthetic packets use the SAME target filter/handoff as production;
         * only this demo's packet source owns an illustrative blink clock. */
        if(animate) {
            level=16-(int)fmin(t*4,16);
            if(t>=4)state=fmod(t-4,1.2)<.6?CR_PROGRESS_BLINK_LOW:CR_PROGRESS_BLINK_HIGH;
        }
        cr_progress_set(&preview_progress,level,state,now);
        const maneuver_state_t *next=engine_phase==1?&next_scene.maneuver:NULL;
        render_set_route_progress(preview_progress.fill,preview_progress.path_weight,preview_progress.glow);
        maneuver_prepare_frame(&current_scene.maneuver,next);
        maneuver_draw(&current_scene.maneuver,next);}
        if(show_frame)render_debug_visible_area();
        render_end_frame();dirty=0;
        if(handoff_out) {
            float px,py,rot;render_get_camera_pose(&px,&py,&rot);
            printf("TRAVEL %d camera %.6f %.6f rotation %.6f phase %d fill %.6f handoff %d\n",handoff_frame,px,py,rot,engine_phase,preview_progress.fill,preview_progress.handoff);
            capture(handoff_out,handoff_frame,1,width,height);
            if(handoff_frame==0)request_case(handoff_next,compact);
            else if(handoff_frame==2)request_case((handoff_next+1)%CASES,compact);
            else if(handoff_frame==3)request_case(handoff_next,compact);
            else if(!engine_phase && !maneuver_needs_redraw() &&
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
        if(out && ++frame==12) {
            capture(out,c,compact,width,height);
            printf("FRAME %d %s %s bounds %.2f %.2f %.2f %.2f camera %.2f %.2f lanes %d secondary_width %.2f hidden_side %d mesh %08x\n",c,compact?"crop":"wide",framing_ok?"PASS":"FAIL",frame_bounds[0],frame_bounds[1],frame_bounds[2],frame_bounds[3],camera_x,camera_y,visible_count,scene_secondary_width,hidden_side,(unsigned)scene_mesh_hash);
            printf("EXIT %d %s lanes %d glyph_clearance %.5f\n",c,compact?"crop":"wide",scene_exit_lanes,scene_glyph_clearance);
            printf("HEAD %d %s min %.5f max %.5f\n",c,compact?"crop":"wide",scene_head_min,scene_head_max);
            printf("SAFETY %d %s reason %d\n",c,compact?"crop":"wide",(int)safety);
            for(int id=0;id<LR_MAX_LANES;++id)if(scene_glyph_seen&(1u<<id))
                printf("GLYPH %d %s lane %d hash %08x\n",c,compact?"crop":"wide",id,(unsigned)scene_glyph_hash[id]);
            if(!framing_ok){fprintf(stderr,"Important geometry outside safe area\n");return 4;}
            if(compact){compact=0;dirty=1;}else{compact=1;dirty=1;if(++c==CASES)break;}
        }
        if(glGetError()!=GL_NO_ERROR){fprintf(stderr,"GL error\n");return 3;}
        platform_swap();
        /* Include drawing time in the 30Hz budget, especially while painting
         * two scenes. A fixed post-render sleep slowed handoffs unnecessarily. */
        double remaining=1.0/30.0-(glfwGetTime()-now);
        if(remaining>0)usleep((unsigned)(remaining*1000000));
    }
    maneuver_set_scene_provider(NULL);render_shutdown();platform_shutdown();return 0;
}
