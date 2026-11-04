#ifndef CARPLAY_PRISM_TRANSITION_H
#define CARPLAY_PRISM_TRANSITION_H
#include "prism.h"

#define PRISM_TRANSITION_MS 700ULL
#define PRISM_CACHE_WIDTH 256
#define PRISM_CACHE_HEIGHT 256

/* Only the Apple and signature, at one cache texel per panel pixel. Integer
 * origin preserves sample centres at takeover. All four layouts fit 256². */
static inline void prism_cache_rect(const prism_layout_t *l,float rect[4]) {
    float unit=fminf(l->content[2]*0.78f,(l->content[3]-30.0f)*1.02f);
    float cx=l->content[0]+l->content[2]*0.5f;
    float cy=l->content[1]+l->content[3]*0.45f;
    float left=fminf(cx-unit*0.23f,l->signature[0]);
    float right=fmaxf(cx+unit*0.23f,l->signature[0]+l->signature[2]);
    float top=fminf(cy-unit*0.23f,l->signature[1]);
    float bottom=fmaxf(cy+unit*0.23f,l->signature[1]+l->signature[3]);
    rect[0]=floorf((left+right-PRISM_CACHE_WIDTH)*0.5f);
    rect[1]=floorf((top+bottom-PRISM_CACHE_HEIGHT)*0.5f);
    rect[2]=PRISM_CACHE_WIDTH;rect[3]=PRISM_CACHE_HEIGHT;
}

/* All camera/easing work is per frame on the CPU, never per fragment. The
 * centre of the Apple body (below the leaf/notch) becomes the camera target. */
static inline void prism_transition_uniforms(const prism_layout_t *l, float progress,
                                             float camera[4], float mix[4]) {
    float t=fmaxf(0.0f,fminf(1.0f,progress));
    float e=t*t*t*(t*(t*6.0f-15.0f)+10.0f);
    float unit=fminf(l->content[2]*0.78f,(l->content[3]-30.0f)*1.02f);
    float cx=l->content[0]+l->content[2]*0.5f;
    float cy=l->content[1]+l->content[3]*0.45f;
    float reach=fmaxf(fmaxf(cx,PRISM_WIDTH-cx)/(unit*0.08f),
                     fmaxf(cy,PRISM_HEIGHT-cy)/(unit*0.075f));
    float zoom=powf(fmaxf(1.0f,reach*1.1f),e);
    camera[0]=cx; camera[1]=cy; camera[2]=1.0f/zoom; camera[3]=unit;
    mix[0]=e; mix[1]=0.025f*(1.0f-camera[2]);
    t=fmaxf(0.0f,fminf(1.0f,(e-0.05f)/0.65f));
    mix[2]=t*t*(3.0f-2.0f*t);
    t=fmaxf(0.0f,fminf(1.0f,(e-0.86f)/0.14f));
    mix[3]=t*t*(3.0f-2.0f*t);
}
#endif
