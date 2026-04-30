/*
 * QNX platform implementation -- libdisplayinit.so + EGL + GLES2
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

/* Display library function signatures (libdisplayinit.so) */
typedef void (*display_init_fn)(int, int);
typedef int  (*display_create_window_fn)(EGLDisplay, EGLConfig, int, int, int,
                                         EGLNativeWindowType *, int *);

/* EGL state */
static EGLDisplay g_egl_display = EGL_NO_DISPLAY;
static EGLSurface g_egl_surface = EGL_NO_SURFACE;
static EGLContext g_egl_context = EGL_NO_CONTEXT;
static int g_display_routed = 0;
static int g_width = 0, g_height = 0;
static volatile int g_should_close = 0;

/* Overridable IDs (env: CR_DISPLAYABLE_ID, CR_CONTEXT_ID, CR_DISPLAY_ID) */
static int g_displayable_id;
static int g_context_id;
static int g_display_id;

static int read_env_int(const char *name, int def) {
    const char *v = getenv(name);
    return (v && v[0]) ? atoi(v) : def;
}

/* Display restore — switch cluster back to native context 74
 * (MAP_ROUTE_GUIDANCE + KOMBI_MAP_VIEW). */
static void restore_display(void) {
    if (g_display_routed) {
        char cmd[128];
        snprintf(cmd, sizeof(cmd), "/eso/bin/apps/dmdt sc %d 74", g_display_id);
        system(cmd);
        g_display_routed = 0;
    }
}

static void signal_handler(int sig) {
    /* Only use async-signal-safe functions (write, signal, raise).
     * restore_display() deferred to atexit handler for clean exits;
     * for crashes (SIGSEGV/SIGABRT), system() inside the handler was UB anyway. */
    static const char msg_segv[] = "platform_qnx: caught SIGSEGV\n";
    static const char msg_abrt[] = "platform_qnx: caught SIGABRT\n";
    static const char msg_term[] = "platform_qnx: caught SIGTERM\n";
    static const char msg_unk[]  = "platform_qnx: caught signal\n";

    if (sig == SIGSEGV)      write(STDERR_FILENO, msg_segv, sizeof(msg_segv) - 1);
    else if (sig == SIGABRT) write(STDERR_FILENO, msg_abrt, sizeof(msg_abrt) - 1);
    else if (sig == SIGTERM) write(STDERR_FILENO, msg_term, sizeof(msg_term) - 1);
    else                     write(STDERR_FILENO, msg_unk,  sizeof(msg_unk)  - 1);

    g_should_close = 1;
    signal(sig, SIG_DFL);
    raise(sig);
}

static const char *g_displayinit_paths[] = {
    "/mnt/app/eso/lib/libdisplayinit.so",   /* gpSP uses this exact path */
    "/eso/lib/libdisplayinit.so",
    "/mnt/app/armle/lib/libdisplayinit.so",
    "/lib/libdisplayinit.so",
    "/usr/lib/libdisplayinit.so",
    "libdisplayinit.so",
    NULL
};

static void *open_displayinit(void) {
    void *lib = NULL;
    for (int i = 0; g_displayinit_paths[i]; i++) {
        lib = dlopen(g_displayinit_paths[i], RTLD_LAZY);
        if (lib) {
            fprintf(stderr, "platform_qnx: loaded %s\n", g_displayinit_paths[i]);
            break;
        }
    }
    if (!lib)
        fprintf(stderr, "platform_qnx: failed to load libdisplayinit.so\n");
    return lib;
}

int platform_init(int width, int height) {
    g_width = width;
    g_height = height;

    g_displayable_id = read_env_int("CR_DISPLAYABLE_ID", CR_DISPLAYABLE_ID);
    g_context_id     = read_env_int("CR_CONTEXT_ID", CR_CONTEXT_ID);
    g_display_id     = read_env_int("CR_DISPLAY_ID", CR_DISPLAY_ID);

    if (!getenv("IPL_CONFIG_DIR"))
        putenv("IPL_CONFIG_DIR=/etc/eso/production");

    signal(SIGTERM, signal_handler);
    signal(SIGSEGV, signal_handler);
    signal(SIGABRT, signal_handler);
    atexit(restore_display);

    /* Force line-buffered stderr so output isn't lost on crash */
    setvbuf(stderr, NULL, _IOLBF, 0);

    fprintf(stderr, "platform_qnx: displayable=%d context=%d display=%d\n",
            g_displayable_id, g_context_id, g_display_id);
    fprintf(stderr, "platform_qnx: LD_LIBRARY_PATH=%s\n",
            getenv("LD_LIBRARY_PATH") ? getenv("LD_LIBRARY_PATH") : "(unset)");
    fprintf(stderr, "platform_qnx: IPL_CONFIG_DIR=%s\n",
            getenv("IPL_CONFIG_DIR") ? getenv("IPL_CONFIG_DIR") : "(unset)");
    {
        char cwd[256];
        fprintf(stderr, "platform_qnx: pid=%d ppid=%d cwd=%s\n",
                getpid(), getppid(),
                getcwd(cwd, sizeof(cwd)) ? cwd : "(unknown)");
    }

    /* Load libdisplayinit.so once, keep open for both display_init and
     * display_create_window.  display_init creates a screen_context that
     * display_create_window needs — dlclose between them destroys it. */
    void *g_displib = open_displayinit();
    if (!g_displib) return -1;

    display_init_fn di = (display_init_fn)dlsym(g_displib, "display_init");
    if (!di) {
        fprintf(stderr, "platform_qnx: FAIL dlsym display_init\n");
        dlclose(g_displib);
        return -1;
    }
    fprintf(stderr, "platform_qnx: display_init...\n");
    di(0, 0);

    fprintf(stderr, "platform_qnx: eglGetDisplay...\n");
    g_egl_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (g_egl_display == EGL_NO_DISPLAY) {
        fprintf(stderr, "platform_qnx: FAIL eglGetDisplay\n");
        return -1;
    }

    EGLint major, minor;
    if (!eglInitialize(g_egl_display, &major, &minor)) {
        fprintf(stderr, "platform_qnx: FAIL eglInitialize (err=0x%x)\n", eglGetError());
        return -1;
    }
    fprintf(stderr, "platform_qnx: EGL %d.%d\n", major, minor);

    /* No MSAA — FXAA post-process handles edge smoothing instead */
    EGLint config_attrs[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8,
        EGL_NONE
    };

    EGLConfig config;
    EGLint num_configs = 0;
    fprintf(stderr, "platform_qnx: eglChooseConfig...\n");
    if (!eglChooseConfig(g_egl_display, config_attrs, &config, 1, &num_configs) ||
        num_configs == 0) {
            fprintf(stderr, "platform_qnx: FAIL eglChooseConfig (err=0x%x)\n", eglGetError());
            return -1;
    }
    fprintf(stderr, "platform_qnx: got %d config(s)\n", num_configs);

    EGLNativeWindowType native_window = 0;
    int kd_window = 0;
    fprintf(stderr, "platform_qnx: display_create_window(%dx%d, disp=%d)...\n",
            width, height, g_displayable_id);
    {
        display_create_window_fn dcw = (display_create_window_fn)dlsym(g_displib, "display_create_window");
        if (!dcw) {
            fprintf(stderr, "platform_qnx: FAIL dlsym display_create_window\n");
            dlclose(g_displib);
            return -1;
        }
        int ret = dcw(g_egl_display, config,
                       width, height, g_displayable_id,
                       &native_window, &kd_window);
        /* Keep g_displib open — screen_context lives inside it */
        if (!native_window) {
            fprintf(stderr, "platform_qnx: FAIL display_create_window ret=%d win=NULL\n", ret);
            dlclose(g_displib);
            return -1;
        }
        fprintf(stderr, "platform_qnx: window created ret=%d native=%p kd=%d\n",
                ret, (void *)(uintptr_t)native_window, kd_window);

        /* Enable alpha transparency so clear color (0,0,0,0) is transparent
         * and the native map (displayable 33) shows through.
         * QNX Screen constants: SCREEN_PROPERTY_TRANSPARENCY=17, SOURCE_OVER=2 */
        {
            void *libscr = dlopen("libscreen.so.1", RTLD_LAZY);
            if (!libscr) libscr = dlopen("libscreen.so", RTLD_LAZY);
            if (libscr) {
                int (*set_prop_iv)(void*, int, const int*) =
                    (int (*)(void*, int, const int*))dlsym(libscr, "screen_set_window_property_iv");
                if (set_prop_iv) {
                    int val = 2;  /* SCREEN_TRANSPARENCY_SOURCE_OVER */
                    if (set_prop_iv((void*)(uintptr_t)native_window, 17, &val) == 0) {
                        fprintf(stderr, "platform_qnx: transparency=SOURCE_OVER\n");
                    } else {
                        fprintf(stderr, "platform_qnx: WARN: set transparency failed\n");
                    }
                }
                dlclose(libscr);
            }
        }
    }

    /* Re-declare context 74 with the full displayable list.
     * display_create_window re-registers displayable 20 which strips
     * other displayables from context 74.  Restore the original composition:
     * 20 (our renderer) + 102 + 101 + 33 (KOMBI_MAP_VIEW = native map). */
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "/eso/bin/apps/dmdt dc %d %d 102 101 33",
             g_context_id, g_displayable_id);
    fprintf(stderr, "platform_qnx: %s\n", cmd);
    system(cmd);

    if (read_env_int("CR_ROUTE_DISPLAY", 0)) {
        snprintf(cmd, sizeof(cmd), "/eso/bin/apps/dmdt sc %d %d", g_display_id, g_context_id);
        fprintf(stderr, "platform_qnx: %s\n", cmd);
        system(cmd);
        g_display_routed = 1;
    }

    fprintf(stderr, "platform_qnx: eglCreateWindowSurface...\n");
    g_egl_surface = eglCreateWindowSurface(g_egl_display, config,
                                            native_window, NULL);
    if (g_egl_surface == EGL_NO_SURFACE) {
        fprintf(stderr, "platform_qnx: FAIL eglCreateWindowSurface (err=0x%x)\n", eglGetError());
        return -1;
    }

    EGLint ctx_attrs[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
    eglBindAPI(EGL_OPENGL_ES_API);
    fprintf(stderr, "platform_qnx: eglCreateContext...\n");
    g_egl_context = eglCreateContext(g_egl_display, config, EGL_NO_CONTEXT, ctx_attrs);
    if (g_egl_context == EGL_NO_CONTEXT) {
        fprintf(stderr, "platform_qnx: FAIL eglCreateContext (err=0x%x)\n", eglGetError());
        return -1;
    }

    fprintf(stderr, "platform_qnx: eglMakeCurrent...\n");
    if (!eglMakeCurrent(g_egl_display, g_egl_surface, g_egl_surface, g_egl_context)) {
        fprintf(stderr, "platform_qnx: FAIL eglMakeCurrent (err=0x%x)\n", eglGetError());
        return -1;
    }

    /* swap interval = 2 → eglSwapBuffers() blocks until the *second*
     * vsync after submission.  On the cluster's 60 Hz display this gives
     * us a hardware-paced 30 FPS, identical to how the native HMI
     * graphics workers (and CarPlay video pipeline) drive their own
     * displayables.  No software nanosleep needed in the render loop —
     * eglSwapBuffers itself paces.
     *
     * Was 0 (no vsync wait, software pacing via nanosleep).  That gave
     * ~21-25 FPS effective on QNX 6.5 due to the kernel's 10 ms timer
     * tick rounding `nanosleep(31 ms)` up to 40 ms. */
    eglSwapInterval(g_egl_display, 2);

    fprintf(stderr, "platform_qnx: OK %dx%d (swap interval=2 → 30 FPS vsync)\n", width, height);
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
    /* Skip EGL teardown — destroying the surface can kill shared displayables
     * (e.g. displayable 20 = native route guidance widget).
     * In production the C hook sends SIGKILL, so this is only for clean exit.
     * Just release the GL context, don't destroy surface/terminate. */
    if (g_egl_display != EGL_NO_DISPLAY) {
        eglMakeCurrent(g_egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    }
    restore_display();
}

void platform_get_framebuffer_size(int *width, int *height) {
    *width = g_width;
    *height = g_height;
}

void platform_get_routing_ids(int *display_id, int *context_id, int *displayable_id) {
    if (display_id) *display_id = g_display_id;
    if (context_id) *context_id = g_context_id;
    if (displayable_id) *displayable_id = g_displayable_id;
}

int platform_key_tap(int key) {
    (void)key;
    return 0;
}

#endif /* PLATFORM_QNX */
