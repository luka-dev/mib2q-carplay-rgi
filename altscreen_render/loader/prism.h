#ifndef CARPLAY_PRISM_H
#define CARPLAY_PRISM_H
#include <stdlib.h>
#include <stddef.h>
#include "loader_layout.h"
#include <math.h>

#define PRISM_WIDTH 1440
#define PRISM_HEIGHT 542

typedef struct { float content[4], view[4], signature[4]; } prism_layout_t;
static inline prism_layout_t prism_layout(int view,int style,float sig_w,float sig_h) {
    const loader_layout_t *l=carplay_loader_layout(view,style);
    float x=fmaxf(l->view_x,l->view_x+l->pad_left);
    float y=fmaxf(l->view_y,l->view_y+l->pad_top);
    float right=fminf(l->view_x+l->view_w,l->view_x+l->view_w-l->pad_right);
    float bottom=fminf(l->view_y+l->view_h,l->view_y+l->view_h-l->pad_bottom);
    float w=right-x,h=bottom-y,unit=fminf(w*0.78f,(h-30.0f)*1.02f);
    prism_layout_t result={{x,y,w,h},{l->view_x,l->view_y,l->view_w,l->view_h},
        {x+w*0.5f-sig_w*0.5f,y+h*0.45f+unit*0.36f-sig_h*0.5f,sig_w,sig_h}};
    return result;
}

/* The decoder is PNG-only, memory-only, compiled into the executable. */
unsigned char *prism_decode_png(const unsigned char *bytes,size_t size,int *w,int *h);
void prism_free_image(unsigned char *image);
#endif
