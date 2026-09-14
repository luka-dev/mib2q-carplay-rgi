#ifndef CARPLAY_PRISM_GL_H
#define CARPLAY_PRISM_GL_H
#include "prism.h"

/* Context-local scene resources. No window, EGL lifecycle, video import or thread. */
typedef struct {
    unsigned program, material, signature, empty;
    int position, resolution, time, content, view, sig_rect, mix, panel, overlay, pixel_mm;
    int signature_width, signature_height;
} prism_gl_t;

/* A current GLES2 (HU) or OpenGL 2.1 (host preview) context is required. */
int prism_gl_init(prism_gl_t *scene);
void prism_gl_destroy(prism_gl_t *scene);
void prism_gl_draw(prism_gl_t *scene, const prism_layout_t *layout,
                   int width, int height, float elapsed, float reveal,
                   unsigned stream_texture, int transparent_reveal);
#endif
