/*
 * QNX platform implementation — libdisplayinit.so + EGL + GLES2
 *
 * Creates a displayable on the HU display manager and sets up
 * EGL/GLES2 for rendering. Uses dmdt for context routing.
 */

#ifdef PLATFORM_QNX

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <dlfcn.h>
#include <unistd.h>

#include <EGL/egl.h>
#include <GLES2/gl2.h>

#include "platform.h"
#include "protocol.h"

/* Display library functions */
typedef int  (*display_init_fn)(int, int);
typedef int  (*display_create_window_fn)(EGLDisplay, EGLConfig, int, int, int,
                                         void**, void**);

static display_init_fn        fn_display_init = NULL;
static display_create_window_fn fn_display_create_window = NULL;

/* EGL state */
static EGLDisplay g_egl_display = EGL_NO_DISPLAY;
static EGLSurface g_egl_surface = EGL_NO_SURFACE;
static EGLContext g_egl_context = EGL_NO_CONTEXT;
static int g_display_routed = 0;
static int g_width = 0, g_height = 0;
static volatile int g_should_close = 0;

/* Display restore (signal-safe) */
static void restore_display(void) {
    if (g_display_routed) {
        char cmd[128];
        snprintf(cmd, sizeof(cmd), "/eso/bin/apps/dmdt sc %d 0", CR_DISPLAY_ID);
        system(cmd);
        g_display_routed = 0;
    }
}

static void signal_handler(int sig) {
    restore_display();
    g_should_close = 1;
    signal(sig, SIG_DFL);
    raise(sig);
}

static int load_display_lib(void) {
    const char *paths[] = {
        "/eso/lib/libdisplayinit.so",
        "/mnt/app/armle/lib/libdisplayinit.so",
        "/lib/libdisplayinit.so",
        "/usr/lib/libdisplayinit.so",
        "libdisplayinit.so",
        NULL
    };

    void *lib = NULL;
    for (int i = 0; paths[i]; i++) {
        lib = dlopen(paths[i], RTLD_NOW);
        if (lib) break;
    }

    if (!lib) {
        fprintf(stderr, "platform_qnx: failed to load libdisplayinit.so\n");
        return -1;
    }

    fn_display_init = (display_init_fn)dlsym(lib, "display_init");
    fn_display_create_window = (display_create_window_fn)dlsym(lib, "display_create_window");

    if (!fn_display_init || !fn_display_create_window) {
        fprintf(stderr, "platform_qnx: missing display symbols\n");
        return -1;
    }

    return 0;
}

int platform_init(int width, int height) {
    g_width = width;
    g_height = height;

    if (!getenv("IPL_CONFIG_DIR"))
        putenv("IPL_CONFIG_DIR=/etc/eso/production");

    signal(SIGTERM, signal_handler);
    signal(SIGSEGV, signal_handler);
    signal(SIGABRT, signal_handler);
    atexit(restore_display);

    if (load_display_lib() < 0) return -1;

    fn_display_init(0, 0);

    g_egl_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (g_egl_display == EGL_NO_DISPLAY) return -1;

    EGLint major, minor;
    if (!eglInitialize(g_egl_display, &major, &minor)) return -1;

    EGLint config_attrs[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8,
        EGL_SAMPLE_BUFFERS, 1, EGL_SAMPLES, 4,   /* 4x MSAA */
        EGL_NONE
    };

    EGLConfig config;
    EGLint num_configs;
    if (!eglChooseConfig(g_egl_display, config_attrs, &config, 1, &num_configs) ||
        num_configs == 0) return -1;

    void *native_window = NULL, *kd_window = NULL;
    int ret = fn_display_create_window(g_egl_display, config,
                                        width, height, CR_DISPLAYABLE_ID,
                                        &native_window, &kd_window);
    if (ret != 0 || !native_window) return -1;

    /* Route displayable to context */
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "/eso/bin/apps/dmdt dc %d %d", CR_CONTEXT_ID, CR_DISPLAYABLE_ID);
    system(cmd);
    snprintf(cmd, sizeof(cmd), "/eso/bin/apps/dmdt sc %d %d", CR_DISPLAY_ID, CR_CONTEXT_ID);
    system(cmd);
    g_display_routed = 1;

    g_egl_surface = eglCreateWindowSurface(g_egl_display, config,
                                            (EGLNativeWindowType)native_window, NULL);
    if (g_egl_surface == EGL_NO_SURFACE) return -1;

    EGLint ctx_attrs[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
    eglBindAPI(EGL_OPENGL_ES_API);
    g_egl_context = eglCreateContext(g_egl_display, config, EGL_NO_CONTEXT, ctx_attrs);
    if (g_egl_context == EGL_NO_CONTEXT) return -1;

    if (!eglMakeCurrent(g_egl_display, g_egl_surface, g_egl_surface, g_egl_context))
        return -1;

    eglSwapInterval(g_egl_display, 0);

    fprintf(stderr, "platform_qnx: EGL %d.%d, %dx%d\n", major, minor, width, height);
    return 0;
}

void platform_swap(void) {
    eglSwapBuffers(g_egl_display, g_egl_surface);
}

void platform_poll(void) {
    /* QNX: no event loop, just check signal flag */
}

int platform_should_close(void) {
    return g_should_close;
}

void platform_shutdown(void) {
    if (g_egl_display != EGL_NO_DISPLAY) {
        eglMakeCurrent(g_egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (g_egl_context != EGL_NO_CONTEXT) eglDestroyContext(g_egl_display, g_egl_context);
        if (g_egl_surface != EGL_NO_SURFACE) eglDestroySurface(g_egl_display, g_egl_surface);
        eglTerminate(g_egl_display);
    }
    g_egl_display = EGL_NO_DISPLAY;
    g_egl_surface = EGL_NO_SURFACE;
    g_egl_context = EGL_NO_CONTEXT;
    restore_display();
}

void platform_get_framebuffer_size(int *width, int *height) {
    *width = g_width;
    *height = g_height;
}

int platform_key_tap(int key) {
    (void)key;
    return 0;
}

#endif /* PLATFORM_QNX */
