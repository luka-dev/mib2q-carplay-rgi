/*
 * STUB <screen/screen.h> — host syntax-check ONLY (make hostcheck).
 * Declares just the QNX Screen surface used by sink_omx_qnx.c so a host compiler
 * can type-check the port offline.  The REAL build (unit/VM) uses the BSP header;
 * this dir is never on the QNX include path.
 */
#ifndef MANEUVER_HOSTCHECK_SCREEN_H
#define MANEUVER_HOSTCHECK_SCREEN_H

#include <stddef.h>

typedef void *screen_context_t;
typedef void *screen_window_t;
typedef void *screen_buffer_t;
typedef void *screen_display_t;
typedef void *screen_pixmap_t;
#define SCREEN_FORMAT_RGBA8888 8
#define SCREEN_USAGE_READ      (1 << 1)
#define SCREEN_USAGE_WRITE     (1 << 2)
#define SCREEN_USAGE_NATIVE    (1 << 3)
#define SCREEN_USAGE_OPENGL_ES2 (1 << 5)
#define SCREEN_USAGE_VIDEO     (1 << 7)
#define SCREEN_WAIT_IDLE       (1 << 0)

enum {
    SCREEN_APPLICATION_CONTEXT,
    SCREEN_PROPERTY_DISPLAY_COUNT,
    SCREEN_PROPERTY_DISPLAYS,
    SCREEN_PROPERTY_DISPLAY,
    SCREEN_PROPERTY_ID_STRING,
    SCREEN_PROPERTY_VISIBLE,
    SCREEN_PROPERTY_POSITION,
    SCREEN_PROPERTY_GLOBAL_ALPHA,
    SCREEN_PROPERTY_FORMAT,
    SCREEN_PROPERTY_USAGE,
    SCREEN_PROPERTY_BUFFER_SIZE,
    SCREEN_PROPERTY_SIZE,
    SCREEN_PROPERTY_RENDER_BUFFERS,
    SCREEN_PROPERTY_POINTER,
    SCREEN_PROPERTY_STRIDE,
    SCREEN_PROPERTY_PLANAR_OFFSETS
};

int screen_create_context(screen_context_t *ctx, int type);
int screen_destroy_context(screen_context_t ctx);
int screen_create_window(screen_window_t *win, screen_context_t ctx);
int screen_destroy_window(screen_window_t win);
int screen_create_window_group(screen_window_t win, const char *name);
int screen_manage_window(screen_window_t win, const char *id);
int screen_create_window_buffers(screen_window_t win, int count);
int screen_create_pixmap(screen_pixmap_t *pix, screen_context_t ctx);
int screen_destroy_pixmap(screen_pixmap_t pix);
int screen_attach_pixmap_buffer(screen_pixmap_t pix, screen_buffer_t buf);
int screen_post_window(screen_window_t win, screen_buffer_t buf, int count, const int *dirty, int flags);
int screen_flush_context(screen_context_t ctx, int flags);

int screen_set_window_property_iv(screen_window_t win, int prop, const int *v);
int screen_set_window_property_cv(screen_window_t win, int prop, int len, const char *v);
int screen_set_window_property_pv(screen_window_t win, int prop, void **v);
int screen_set_pixmap_property_iv(screen_pixmap_t pix, int prop, const int *v);
int screen_get_window_property_pv(screen_window_t win, int prop, void **v);
int screen_get_window_property_iv(screen_window_t win, int prop, int *v);
int screen_get_window_property_cv(screen_window_t win, int prop, int len, char *v);
int screen_get_buffer_property_pv(screen_buffer_t buf, int prop, void **v);
int screen_get_buffer_property_iv(screen_buffer_t buf, int prop, int *v);
int screen_get_context_property_iv(screen_context_t ctx, int prop, int *v);
int screen_get_context_property_pv(screen_context_t ctx, int prop, void **v);

#endif
