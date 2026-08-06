#ifndef ALTSCREEN_SCREEN_EGL_PRESENT_H
#define ALTSCREEN_SCREEN_EGL_PRESENT_H

typedef struct screen_egl_present screen_egl_present_t;

/*
 * Allocate the decoder-native Qualcomm 32m Screen WINDOW buffers and a managed
 * RGBA8888 cluster EGL window.  OMX writes to the returned source pointers;
 * EGL imports non-owning pixmap aliases of those Screen buffers as external
 * images (the original window allocations remain owned by this presenter).
 */
screen_egl_present_t *screen_egl_present_create(int width, int height,
                                                 unsigned int source_count);
void *screen_egl_present_source_pointer(screen_egl_present_t *p,
                                        unsigned int index);
unsigned int screen_egl_present_source_capacity(screen_egl_present_t *p,
                                                 unsigned int index);

/* Draw one decoded 32m buffer through the Adreno external-image sampler into
 * the RGBA cluster window.  The call completes GPU reads before returning, so
 * the caller may safely recycle that exact OMX output buffer. */
int screen_egl_present_frame(screen_egl_present_t *p,
                             unsigned int source_index,
                             unsigned int filled_len);

/* Recreate only the managed RGBA8888 plane-99 window and its EGLSurface.
 * Decoder-native WINDOW buffers, pixmap aliases, EGLImages, textures and OMX
 * pointers remain intact.  Must be called by the server thread, never from an
 * OMX callback. */
int screen_egl_present_recover_output(screen_egl_present_t *p);

void screen_egl_present_destroy(screen_egl_present_t *p);

#endif /* ALTSCREEN_SCREEN_EGL_PRESENT_H */
