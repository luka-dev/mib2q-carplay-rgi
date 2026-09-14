#ifndef HU_CURSOR_GL_H
#define HU_CURSOR_GL_H
#include "cursor_plane.h"
#include "../common/prism_gl.h"
typedef struct {
    prism_gl_t prism;
    unsigned program;
    int position, uv, alpha, motion, light_phase;
} cursor_gl_t;
int cursor_gl_init(cursor_gl_t *scene);
void cursor_gl_destroy(cursor_gl_t *scene);
/* Full transparent clear each frame: no dependency on preserved EGL back buffers. */
int cursor_gl_draw(cursor_gl_t *scene,const cursor_frame *frame);
#endif
