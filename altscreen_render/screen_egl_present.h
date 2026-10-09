/*
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */

#ifndef ALTSCREEN_SCREEN_EGL_PRESENT_H
#define ALTSCREEN_SCREEN_EGL_PRESENT_H

typedef struct screen_egl_present screen_egl_present_t;

/* Create the managed RGBA8888 cluster output immediately, before an RTSP/OMX
 * session exists. The presenter owns a local 30-fps PRISM waiting loader;
 * decoder-native Qualcomm 32m sources are attached later. */
screen_egl_present_t *screen_egl_present_create_output(int width, int height,
                                                       volatile int *view_area_mode,
                                                       volatile int *layout_style);

/* Allocate decoder-native Qualcomm 32m Screen WINDOW buffers and import them as
 * external EGLImages without replacing the existing RGBA output window. */
int screen_egl_present_attach_sources(screen_egl_present_t *p,
                                      unsigned int source_count);

/* Loader lifecycle. start is idempotent; stop joins the loader thread and
 * guarantees it no longer draws or posts the output window. */
int  screen_egl_present_start_loader(screen_egl_present_t *p);
void screen_egl_present_stop_loader(screen_egl_present_t *p);
/* During OMX startup/first-IDR wait, reduce only PRISM draw cadence to 15 Hz.
 * Direct decoded-video presentation never uses this budget. */
void screen_egl_present_set_stream_pending(screen_egl_present_t *p, int pending);

void *screen_egl_present_source_pointer(screen_egl_present_t *p,
                                        unsigned int index);
unsigned int screen_egl_present_source_capacity(screen_egl_present_t *p,
                                                 unsigned int index);

/* Stop/join the waiting loader, then draw one decoded 32m buffer directly through
 * the Adreno external-image sampler into
 * the RGBA cluster window.  The call completes GPU reads before returning, so
 * the presentation worker may safely recycle that exact OMX output buffer.
 * Never call from the decoder callback; queue its source index to the worker. */
int screen_egl_present_frame(screen_egl_present_t *p,
                             unsigned int source_index,
                             unsigned int filled_len);
/* Called after the presentation worker has stopped, before the next session. */
void screen_egl_present_reset_timing(screen_egl_present_t *p);

/* Recreate only the managed RGBA8888 plane-99 window and its EGLSurface.
 * Decoder-native WINDOW buffers, pixmap aliases, EGLImages, textures and OMX
 * pointers remain intact.  Must be called by the server thread, never from an
 * OMX callback. */
int screen_egl_present_recover_output(screen_egl_present_t *p);

void screen_egl_present_destroy(screen_egl_present_t *p);

#endif /* ALTSCREEN_SCREEN_EGL_PRESENT_H */
