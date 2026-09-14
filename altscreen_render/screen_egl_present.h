/*
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */

#ifndef ALTSCREEN_SCREEN_EGL_PRESENT_H
#define ALTSCREEN_SCREEN_EGL_PRESENT_H

typedef struct screen_egl_present screen_egl_present_t;

/* Create the managed RGBA8888 cluster output immediately, before an RTSP/OMX
 * session exists.  The returned presenter owns a local 30-fps startup loader;
 * decoder-native Qualcomm 32m sources are attached later. */
screen_egl_present_t *screen_egl_present_create_output(int width, int height,
                                                       volatile int *view_area_mode,
                                                       volatile int *layout_style);

/* Allocate decoder-native Qualcomm 32m Screen WINDOW buffers and import them as
 * external EGLImages without replacing the existing RGBA output window. */
int screen_egl_present_attach_sources(screen_egl_present_t *p,
                                      unsigned int source_count);

/* PRISM compositor lifecycle. start launches the worker or requests a smooth
 * return to the waiting Apple using its owned last-frame cache. The worker
 * remains alive during streaming, so transitions finish even with one input
 * frame. stop joins it and guarantees no further GL reads or window posts. */
int  screen_egl_present_start_loader(screen_egl_present_t *p);
void screen_egl_present_stop_loader(screen_egl_present_t *p);

void *screen_egl_present_source_pointer(screen_egl_present_t *p,
                                        unsigned int index);
unsigned int screen_egl_present_source_capacity(screen_egl_present_t *p,
                                                 unsigned int index);

/* Submit one decoded 32m buffer to the compositor's owned RGBA cache. Success
 * means all source GPU reads finished and the corresponding PRISM frame was
 * posted. The caller can recycle that exact OMX buffer after return. A wedged
 * compositor triggers bounded process exit rather than freeing live storage. */
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
