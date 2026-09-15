#define PLATFORM_QNX 1
#include <assert.h>
#include <stdint.h>
#include "../maneuver_render/platform_qnx.c"
static unsigned creates, destroys, intervals, recreates;
static int fail_interval, fail_bind, native_interval;
static EGLSurface bound;
cluster_surface_t *cluster_surface_create(const cluster_surface_cfg *cfg)
{ (void)cfg; return (cluster_surface_t *)1; }
screen_window_t cluster_surface_window(cluster_surface_t *s)
{ assert(s == (cluster_surface_t *)1); return (screen_window_t)2; }
int cluster_surface_recreate(cluster_surface_t *s)
{ assert(s == (cluster_surface_t *)1); ++recreates; native_interval = 1; return 0; }
int screen_set_window_property_iv(screen_window_t w, int property, const int *v)
{ (void)w; assert(property == SCREEN_PROPERTY_SWAP_INTERVAL && *v == 2); native_interval = *v; return 0; }
int screen_get_window_property_iv(screen_window_t w, int property, int *v)
{ (void)w; assert(property == SCREEN_PROPERTY_SWAP_INTERVAL); *v = native_interval; return 0; }
EGLSurface eglCreateWindowSurface(EGLDisplay d, EGLConfig c, EGLNativeWindowType w, const EGLint *a)
{ (void)d;(void)c;(void)w;(void)a; assert(native_interval == 2); ++creates; return (EGLSurface)(uintptr_t)(100 + creates); }
EGLBoolean eglMakeCurrent(EGLDisplay d, EGLSurface draw, EGLSurface read, EGLContext c)
{ (void)d; (void)read; (void)c; if (draw && fail_bind) return 0; bound = draw; return 1; }
EGLBoolean eglSwapInterval(EGLDisplay d, EGLint interval)
{ (void)d; assert(bound && interval == 2); ++intervals; return !fail_interval; }
EGLBoolean eglDestroySurface(EGLDisplay d, EGLSurface s)
{ (void)d; assert(s); ++destroys; return 1; }
EGLint eglGetError(void) { return 0x300d; }
int main(void) {
    g_egl_display = (EGLDisplay)1; g_egl_config = (EGLConfig)2; g_egl_context = (EGLContext)3;
    assert(create_window_and_egl_surface() == 0 && intervals == 1);
    platform_recreate_window("test-loss");
    assert(recreates == 1 && creates == 2 && destroys == 1 && intervals == 2);
    fail_interval = 1; platform_recreate_window("test-interval-rejected");
    assert(g_egl_surface == EGL_NO_SURFACE && intervals == 3 && destroys == 3);
    fail_interval = 0; fail_bind = 1; platform_recreate_window("test-bind-failed");
    assert(g_egl_surface == EGL_NO_SURFACE && intervals == 3 && destroys == 4);
    fail_bind = 0; platform_recreate_window("test-retry");
    assert(g_egl_surface != EGL_NO_SURFACE && intervals == 4);
    puts("maneuver_surface_test: interval reapplied on recreate, failed bind/interval cleanup and retry PASS");
}
