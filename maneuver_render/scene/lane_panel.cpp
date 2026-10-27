/* Independent screen-space guidance. No road geometry or maneuver ownership. */
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <cassert>
#include <initializer_list>
#include "../lane_panel.h"
#include "../render.h"

// QNX 6.5 xtgmath float fmin/fmax templates recurse with GCC 8.5.
// Use the explicit libm float entry points throughout layout, framing and draw.
namespace {
// Lane cells follow the stock HUD (8W6919617 lg_dr_* assets, r0764/r0765 screens): 33 px pitch,
// 5 px shafts, 11 px heads, glyphs 26 px tall inside a 2 px divider dashed 8/4 over 32 px. The glyphs
// sit at y 3..29 (stock 4..30): the last source rows below y=178 must stay clear (test_lane_mask).
constexpr float Pitch=33, Height=CR_LANE_PANEL_HEIGHT, Padding=8, Bottom=0;
constexpr float Shaft=5, HeadHalf=5.5f, Base=29, Top=3;
constexpr double FadeSeconds=.25;
constexpr int Unknown=9, MaxVertices=4096;
struct Point {float x,y;};
struct Mesh {
    float xy[MaxVertices*2];
    int count;
    void triangle(Point a,Point b,Point c) {
        assert(count+3<=MaxVertices);
        if(count+3>MaxVertices)return;
        for(Point p : {a,b,c}) {xy[count*2]=p.x;xy[count*2+1]=p.y;++count;}
    }
    void rect(float x,float y,float w,float h) {
        triangle({x,y},{x+w,y},{x+w,y+h});triangle({x,y},{x+w,y+h},{x,y+h});
    }
    void disc(float x,float y,float r) {
        for(int i=0;i<12;++i) {
            float a=i*6.2831853f/12,b=(i+1)*6.2831853f/12;
            triangle({x,y},{x+r*std::cos(a),y+r*std::sin(a)},
                     {x+r*std::cos(b),y+r*std::sin(b)});
        }
    }
};
struct Geometry { Mesh gray,blue,dividers; };
int bucket(int raw) {
    if(raw < -180 || raw > 180)return Unknown; // includes BOTH ±1000 sentinels
    return raw>=0?(raw+22)/45:-((-raw+22)/45);
}
bool best(const cr_lane_record_t &lane) {
    return lane.status==2 && bucket(lane.primary)!=Unknown;
}
void add_direction(cr_lane_cell_t &cell,int direction) {
    if(direction==Unknown || cell.count==CR_LANE_PANEL_DIRECTIONS)return;
    for(int j=0;j<cell.count;++j)if(cell.directions[j]==direction)return;
    cell.directions[cell.count++]=direction;
}
Point unit(Point p) {
    float length=std::sqrt(p.x*p.x+p.y*p.y);
    return length>0?Point{p.x/length,p.y/length}:Point{0,-1};
}
/* Continuous joined strip: no alpha overlaps or cracks at path corners. */
void stroke(Mesh &mesh,const Point *p,int n,float width,float cx) {
    Point left[32],right[32];
    assert(n>=2 && n<=32);
    for(int i=0;i<n;++i) {
        Point before=unit(i?Point{p[i].x-p[i-1].x,p[i].y-p[i-1].y}:
                           Point{p[1].x-p[0].x,p[1].y-p[0].y});
        Point after=unit(i+1<n?Point{p[i+1].x-p[i].x,p[i+1].y-p[i].y}:before);
        Point normal=unit({-before.y-after.y,before.x+after.x});
        float scale=width*.5f/::fmaxf(.55f,normal.x*(-after.y)+normal.y*after.x);
        left[i]={cx+p[i].x+normal.x*scale,p[i].y+normal.y*scale};
        right[i]={cx+p[i].x-normal.x*scale,p[i].y-normal.y*scale};
    }
    for(int i=1;i<n;++i) {
        mesh.triangle(left[i-1],right[i-1],right[i]);
        mesh.triangle(left[i-1],right[i],left[i]);
    }
}
/* Shaft up to y0, then a bend of radius r turning by `angle` toward side s. */
int bend(Point *p,int n,float s,float y0,float r,float angle,int steps) {
    p[n++]={0,y0};
    for(int i=1;i<=steps;++i) {
        float a=i*angle/steps;p[n++]={s*(r-r*std::cos(a)),y0-r*std::sin(a)};
    }
    return n;
}
/* HUD lg_dr shapes: the shaft rises high before it turns, heads are 11 px wide and
 * 5.5 px long (7 px straight), so a 45/135 degree head is a right-angled corner.
 * `low` is the HUD's combined-cell variant: the branch leaves the shared shaft lower. */
void glyph(Mesh &mesh,int direction,float cx,bool low) {
    Point p[32],tip;int n=0;float s=direction<0?-1.f:1.f;
    int turn=direction<0?-direction:direction;
    p[n++]={0,Base};
    if(turn==0) {p[n++]={0,Top+7};tip={0,Top};}
    else if(turn==1) {
        n=bend(p,n,s,low?18:16,3,.78539816f,3);  // low: wider, clear of a straight head
        Point e=p[n-1];float k=(e.y-Top)*1.4142136f-HeadHalf;   // tip on y=Top
        p[n++]={e.x+s*k*.70710678f,e.y-k*.70710678f};
        tip={p[n-1].x+s*HeadHalf*.70710678f,Top};
    } else if(turn==2) {
        n=bend(p,n,s,13,2.5f,1.5707963f,4);
        p[n++]={s*6.5f,10.5f};tip={s*12,10.5f};
    } else if(turn==3) {
        n=bend(p,n,s,11,4,2.3561945f,9);
        Point e=p[n-1];
        p[n++]={e.x+s*3.5f*.70710678f,e.y+3.5f*.70710678f};
        tip={p[n-1].x+s*HeadHalf*.70710678f,p[n-1].y+HeadHalf*.70710678f};
    } else {
        n=bend(p,n,s,10,4.5f,3.1415927f,12);
        p[n++]={s*9,14};tip={s*9,19.5f};
    }
    static const float drop[]={0,0,6,8,9};   // 45 degrees widens instead (above)
    if(low) {for(int i=1;i<n;++i)p[i].y+=drop[turn];tip.y+=drop[turn];}
    stroke(mesh,p,n,Shaft,cx);
    Point end=p[n-1],d=unit({tip.x-end.x,tip.y-end.y});
    mesh.triangle({cx+tip.x,tip.y},{cx+end.x-d.y*HeadHalf,end.y+d.x*HeadHalf},
                  {cx+end.x+d.y*HeadHalf,end.y-d.x*HeadHalf});
}
/* Like the HUD assets, a cell's arrows are centred as a whole: a turn's shaft sits off-centre. */
void centre(Geometry &mesh,int gray,int blue,float cx) {
    float lo=1e9f,hi=-1e9f;
    Mesh *meshes[]={&mesh.gray,&mesh.blue};int first[]={gray,blue};
    for(int m=0;m<2;++m)for(int v=first[m];v<meshes[m]->count;++v) {
        lo=::fminf(lo,meshes[m]->xy[v*2]);hi=::fmaxf(hi,meshes[m]->xy[v*2]);
    }
    if(lo>hi)return;
    float shift=cx-(lo+hi)*.5f;
    for(int m=0;m<2;++m)for(int v=first[m];v<meshes[m]->count;++v)meshes[m]->xy[v*2]+=shift;
}
void compile(Geometry &mesh,const cr_lane_panel_layout_t &layout) {
    mesh.gray.count=mesh.blue.count=mesh.dividers.count=0;
    for(int i=0;i<layout.count;++i) {
        const cr_lane_cell_t &cell=layout.cells[i];
        float cx=(i-(layout.count-1)*.5f)*Pitch;
        if(cell.overflow) {
            for(int j=-1;j<=1;++j)mesh.gray.disc(cx+j*4.5f,20,1.15f);
        } else if(!cell.count) {
            // Unknown is a neutral dash, never a fabricated straight arrow.
            mesh.gray.rect(cx-3.5f,18,7,1.6f);
        } else {
            int gray=mesh.gray.count,blue=mesh.blue.count;
            for(int j=0;j<cell.count;++j) {
                int d=cell.directions[j];bool low=false;
                // Lowered when the cell also holds a smaller turn straight ahead or to the same side.
                for(int k=0;k<cell.count;++k) {
                    int e=cell.directions[k];
                    if(std::abs(e)<std::abs(d) && (e==0 || (e<0)==(d<0)))low=true;
                }
                glyph(cell.recommended && d==cell.primary?mesh.blue:mesh.gray,d,cx,low);
            }
            centre(mesh,gray,blue,cx);
        }
        if(i+1<layout.count)for(int j=0;j<3;++j)
            mesh.dividers.rect(cx+Pitch*.5f-1,j*12,2,8);
    }
}
float easing(float t) {return t*t*t*(t*(t*6-15)+10);}
}

struct cr_lane_panel {
    cr_lane_panel_layout_t current,previous;
    Geometry mesh,old_mesh;
    double changed;
    const cr_scene_t *frame_scenes[2];
    unsigned frame_builds[2];
    float framing[3];
    int frame_cached;
};

void cr_lane_panel_layout(cr_lane_panel_layout_t *out,const cr_lane_guidance_t *lanes,float w) {
    if(!out)return;
    std::memset(out,0,sizeof(*out));
    if(!lanes || !lanes->showing || !lanes->count || lanes->count>CR_LANE_CAPACITY ||
       !std::isfinite(w) || w<2*Pitch+Padding+8)return;
    bool drawable=false;
    for(int i=0;i<lanes->count;++i) {
        const cr_lane_record_t &lane=lanes->lanes[i];
        if(bucket(lane.primary)!=Unknown)drawable=true;
        for(unsigned j=0;j<lane.angle_count && j<CR_LANE_ANGLE_CAPACITY;++j)
            if(bucket(lane.angles[j])!=Unknown)drawable=true;
    }
    if(!drawable)return; // Like HUD: an entirely unknown event is not an active panel.
    int capacity=(int)std::floor((::fminf(w,328.f)-Padding-8)/Pitch);
    if(capacity>CR_LANE_CAPACITY)capacity=CR_LANE_CAPACITY;
    int n=lanes->count,order[CR_LANE_CAPACITY];bool positions=true;
    for(int i=0;i<n;++i) {
        order[i]=i;
        if(lanes->lanes[i].position==65535)positions=false;
        for(int j=0;j<i;++j)if(lanes->lanes[j].position==lanes->lanes[i].position)positions=false;
    }
    if(positions)for(int i=1;i<n;++i) {
        int key=order[i],j=i;
        while(j && lanes->lanes[order[j-1]].position>lanes->lanes[key].position) {
            order[j]=order[j-1];--j;
        }
        order[j]=key;
    }
    int first=0,visible=n;bool right=true;
    if(n>capacity) {
        // Raw CarPlay transport has no HUD laneDescription expansion flags.
        // Use the verified HUD fallback: nearest best lane; right wins ties.
        for(int i=0;i<n;++i) {
            if(best(lanes->lanes[order[n-1-i]])) {right=true;break;}
            if(best(lanes->lanes[order[i]])) {right=false;break;}
        }
        visible=capacity;first=right?n-visible:0;
    }
    out->capacity=capacity;out->count=visible;out->first=first;
    out->width=visible*Pitch+Padding;out->height=Height;
    for(int i=0;i<visible;++i) {
        cr_lane_cell_t &cell=out->cells[i];
        cell.source=order[first+i];cell.primary=Unknown;
        if(n>capacity && i==(right?0:visible-1)) {cell.overflow=right?-1:1;continue;}
        const cr_lane_record_t &lane=lanes->lanes[cell.source];
        cell.primary=bucket(lane.primary);
        cell.recommended=lane.status==2 && cell.primary!=Unknown;
        add_direction(cell,cell.primary);
        unsigned count=lane.angle_count;
        if(count>CR_LANE_ANGLE_CAPACITY)count=CR_LANE_ANGLE_CAPACITY;
        for(unsigned j=0;j<count;++j)add_direction(cell,bucket(lane.angles[j]));
    }
}

cr_lane_panel_t *cr_lane_panel_create(void) {
    return static_cast<cr_lane_panel_t *>(std::calloc(1,sizeof(cr_lane_panel_t)));
}
void cr_lane_panel_destroy(cr_lane_panel_t *p) {std::free(p);}
void cr_lane_panel_clear(cr_lane_panel_t *p) {
    if(!p)return;
    std::memset(&p->current,0,sizeof(p->current));
    std::memset(&p->previous,0,sizeof(p->previous));p->changed=0;p->frame_cached=0;
}
int cr_lane_panel_update(cr_lane_panel_t *p,const cr_lane_guidance_t *lanes,float w,double now) {
    if(!p)return 0;
    cr_lane_panel_layout_t next;
    cr_lane_panel_layout(&next,lanes,w);
    if(!std::memcmp(&next,&p->current,sizeof(next)))return 0;
    if(!next.count) {cr_lane_panel_clear(p);return 1;}
    // Rapid updates retain one departing snapshot, never a queue of stale events.
    p->previous=p->current;p->old_mesh=p->mesh;p->current=next;
    compile(p->mesh,next);p->changed=now;return 1;
}
int cr_lane_panel_animating(const cr_lane_panel_t *p,double now) {
    return p && p->current.count && now<p->changed+FadeSeconds;
}
void cr_lane_panel_draw(const cr_lane_panel_t *p,cr_rect_t visible,double now) {
    if(!p || !p->current.count || visible.h<Height+Bottom || visible.w<=0)return;
    float t=(float)((now-p->changed)/FadeSeconds);
    t=easing(::fmaxf(0.f,::fminf(1.f,t)));
    float opacity=p->previous.count?1:t;
    float cx=visible.x+visible.w*.5f,top=visible.y+visible.h-Bottom-Height;
    // The VC crop ends at y=180; the source texture has one further row.
    // Keep erasing to the physical bottom so the maneuver cannot reappear
    // beneath the lane strip. Even one lane erases the FULL source width;
    // only the glyphs are clipped/anchored to the current visible area.
    cr_rect_t mask_clip={0,0,CR_DEFAULT_WIDTH,CR_DEFAULT_HEIGHT};
    render_begin_overlay(mask_clip);
    cr_rect_t cutout={0,top,CR_DEFAULT_WIDTH,CR_DEFAULT_HEIGHT-top};
    render_overlay_cutout(cutout,CR_LANE_PANEL_FADE_X,CR_LANE_PANEL_FADE_Y,opacity);
    render_begin_overlay(visible);
    for(int layer=0;layer<2;++layer) {
        float a=layer?t:1-t;
        if(a<=0 || (!layer && !p->previous.count))continue;
        const Geometry &g=layer?p->mesh:p->old_mesh;
        render_overlay_mesh(g.gray.xy,g.gray.count,cx,top,.60f,.60f,.60f,a);
        render_overlay_mesh(g.blue.xy,g.blue.count,cx,top,0,.75f,.95f,a);
        // Dividers at 50% read as faint dots in the cluster tube (car photo 2026-09-19).
        render_overlay_mesh(g.dividers.xy,g.dividers.count,cx,top,.95f,.95f,.95f,.90f*a);
    }
    render_end_overlay();
}

void cr_lane_panel_framing(cr_lane_panel_t *p,const cr_scene_t *current,
                           const cr_scene_t *next,float out[3]) {
    out[0]=out[1]=out[2]=0;
    if(!p || !p->current.count)return;
    const cr_scene_t *scenes[]={current,next};unsigned builds[2]={0,0};
    bool same=p->frame_cached;
    for(int i=0;i<2;++i) {
        const cr_scene_info_t *info=cr_scene_info(scenes[i]);
        if(info)builds[i]=info->builds;
        if(scenes[i]!=p->frame_scenes[i] || builds[i]!=p->frame_builds[i])same=false;
    }
    if(!same) {
        const float safe[]={CR_POPUP_X+4.f,CR_POPUP_Y+4.f,CR_POPUP_X+CR_POPUP_W-4.f,
                            CR_POPUP_Y+CR_POPUP_H-Height-CR_LANE_PANEL_FADE_Y-3.f};
        float best=1e9f;
        for(int step=0;step<4;++step) {
            float dolly=.12f+step*.02f,m[16],b[]={1e6f,1e6f,-1e6f,-1e6f};bool valid=false;
            render_build_framing_matrix(m,328.f/181,dolly);
            for(int i=0;i<2;++i) {
                float box[4],head[4];
                if(!cr_scene_project_bounds(scenes[i],m,box,head))continue;
                valid=true;
                for(int j=0;j<2;++j) {b[j]=::fminf(b[j],box[j]);b[j+2]=::fmaxf(b[j+2],box[j+2]);}
            }
            float x=0,y=0,overflow=0;
            if(valid) {
                float *offsets[]={&x,&y};
                for(int axis=0;axis<2;++axis) {
                    float lo=safe[axis]-b[axis],hi=safe[axis+2]-b[axis+2];
                    *offsets[axis]=lo<=hi?::fmaxf(lo,::fminf(0.f,hi)):(lo+hi)*.5f;
                    *offsets[axis]=::fmaxf(-36.f,::fminf(36.f,*offsets[axis]));
                    overflow+=::fmaxf(0.f,lo-*offsets[axis])+::fmaxf(0.f,*offsets[axis]-hi);
                }
            }
            float cost=overflow*1000+step*10+std::fabs(x)+std::fabs(y)*.2f;
            if(cost<best) {best=cost;p->framing[0]=x;p->framing[1]=y;p->framing[2]=dolly;}
            if(overflow<.01f)break;
        }
        for(int i=0;i<2;++i) {p->frame_scenes[i]=scenes[i];p->frame_builds[i]=builds[i];}
        p->frame_cached=1;
    }
    std::memcpy(out,p->framing,sizeof(p->framing));
}
