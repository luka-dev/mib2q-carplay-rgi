#ifndef CARPLAY_PRISM_WAITING_H
#define CARPLAY_PRISM_WAITING_H
#include "prism.h"

/* Only these two rectangles can contain resting PRISM artwork. Clear the
 * window to black, then draw both in one batch at native resolution. No shader
 * runs over the rest of the panel; no intermediate image or video texture. */
static inline void prism_waiting_vertices(const prism_layout_t *layout,
                                          float vertices[24]) {
    float unit=fminf(layout->content[2]*0.78f,
                     (layout->content[3]-30.0f)*1.02f);
    float center_x=layout->content[0]+layout->content[2]*0.5f;
    float center_y=layout->content[1]+layout->content[3]*0.45f;
    const float rects[2][4]={
        {center_x-unit*0.23f,center_y-unit*0.23f,unit*0.46f,unit*0.46f},
        {layout->signature[0],layout->signature[1],
         layout->signature[2],layout->signature[3]}
    };
    int i;
    for(i=0;i<2;i++) {
        float l=rects[i][0]*2.0f/PRISM_WIDTH-1.0f;
        float r=(rects[i][0]+rects[i][2])*2.0f/PRISM_WIDTH-1.0f;
        float t=1.0f-rects[i][1]*2.0f/PRISM_HEIGHT;
        float b=1.0f-(rects[i][1]+rects[i][3])*2.0f/PRISM_HEIGHT;
        float *v=vertices+i*12;
        v[0]=l;v[1]=b;v[2]=r;v[3]=b;v[4]=l;v[5]=t;
        v[6]=l;v[7]=t;v[8]=r;v[9]=b;v[10]=r;v[11]=t;
    }
}
#endif
