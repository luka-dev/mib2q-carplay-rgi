/*
 * QNX platform implementation -- cluster_surface (raw screen) + EGL + GLES2
 *
 * Creates a managed displayable (id 98) via the shared cluster_surface primitive
 * (raw screen_create_window + screen_manage_window, no libdisplayinit) and sets up
 * EGL/GLES2 for rendering. Context routing is Java-driven (no dmdt).
 */

#ifdef PLATFORM_QNX

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>

#include <EGL/egl.h>
#include <GLES2/gl2.h>

#include "platform.h"
#include "protocol.h"
#include "cluster_surface.h"

/* EGL state */
static EGLDisplay g_egl_display = EGL_NO_DISPLAY;
static EGLSurface g_egl_surface = EGL_NO_SURFACE;
static EGLContext g_egl_context = EGL_NO_CONTEXT;
static EGLConfig  g_egl_config  = 0;          /* saved for window recreate */
static int g_width = 0, g_height = 0;
static volatile int g_should_close = 0;

/* The managed cluster window (id 98) — created / probed / recreated by the
 * shared cluster_surface primitive (screen_manage_window + MANAGER_STRING
 * watchdog live there).  We bind the EGL surface to its screen window. */
static cluster_surface_t *g_cs = NULL;

/* Set to 1 once platform_init has successfully created the initial window, so
 * the health check knows to keep recovering (vs "renderer not yet up, ignore"). */
static int g_window_expected = 0;

/* Overridable IDs (env: CR_DISPLAYABLE_ID, CR_CONTEXT_ID, CR_DISPLAY_ID) */
static int g_displayable_id;
static int g_context_id;
static int g_display_id;

static int read_env_int(const char *name, int def) {
    const char *v = getenv(name);
    return (v && v[0]) ? atoi(v) : def;
}

/*
 * Create the managed cluster window (id 98) via the shared cluster_surface primitive.
 * cluster_surface_create() internally does screen_create_context() — which is exactly
 * why this MUST run BEFORE eglGetDisplay: the Qualcomm (Adreno/GSL) libEGL requires a
 * screen context to already exist in the process at eglGetDisplay time.  When
 * maneuver_render ran as a JVM child it inherited the HMI's screen connection so this
 * was masked; standalone / as a framework service there is none → eglGetDisplay SIGSEGVs.
 * So we create the cluster_surface (screen context) first, before any EGL call.
 * Idempotent (g_cs guard) so create_window_and_egl_surface() can call it too.
 * FORMAT=RGBA8888 (8) matches our EGL config; USAGE=OPENGL_ES2 (0x20) lets the GPU render
 * into it; transparent=1 → the cluster compositor blends us over the KDK bg / stock map.
 */
static int ensure_cluster_window(void) {
    if (g_cs) return 0;
    cluster_surface_cfg cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.id          = g_displayable_id;   /* 98 */
    cfg.width       = g_width;
    cfg.height      = g_height;
    cfg.format      = 8;                  /* SCREEN_FORMAT_RGBA8888 */
    cfg.usage       = 0x20;               /* SCREEN_USAGE_OPENGL_ES2 */
    cfg.nbuffers    = 2;                  /* double-buffered (swap interval 2) */
    cfg.transparent = 1;
    g_cs = cluster_surface_create(&cfg);
    if (!g_cs) {
        fprintf(stderr, "platform_qnx: cluster_surface_create failed\n");
        return -1;
    }
    return 0;
}

/*
 * Create the managed cluster window (once, via cluster_surface) + an EGL surface
 * bound to it.  Called from platform_init and (to rebind EGL) from the recreate
 * path after we lose displaymanager's m_surfaceSources[98] binding.
 */
static int create_window_and_egl_surface(void) {
    if (g_egl_display == EGL_NO_DISPLAY || g_egl_config == 0) {
        fprintf(stderr, "platform_qnx: create_window: missing EGL prerequisites\n");
        return -1;
    }

    if (ensure_cluster_window() != 0) return -1;

    EGLNativeWindowType native_window = (EGLNativeWindowType)cluster_surface_window(g_cs);
    if (!native_window) {
        fprintf(stderr, "platform_qnx: cluster_surface has no window\n");
        return -1;
    }

    g_egl_surface = eglCreateWindowSurface(g_egl_display, g_egl_config, native_window, NULL);
    if (g_egl_surface == EGL_NO_SURFACE) {
        fprintf(stderr, "platform_qnx: eglCreateWindowSurface FAILED err=0x%x\n", eglGetError());
        return -1;
    }

    if (g_egl_context != EGL_NO_CONTEXT) {
        if (!eglMakeCurrent(g_egl_display, g_egl_surface, g_egl_surface, g_egl_context)) {
            fprintf(stderr, "platform_qnx: eglMakeCurrent FAILED err=0x%x\n", eglGetError());
            return -1;
        }
    }

    /* Context routing is Java's job: DisplayManagerMIB2High.defineContexts() declares
     * dc[80]={98,102,101} and DisplayManager.switchContext() points the cluster at it
     * → setActiveDisplayable(4, 98) → MOST encoder reads our window.  NO dmdt. */
    return 0;
}

/*
 * Rebuild the EGL surface on a fresh managed window after loss/disown.
 * cluster_surface owns the window teardown + recreate (100 ms backoff inside);
 * we only re-bind EGL.  With our own id (98) there is no stock collision, so
 * this now fires rarely — only on a genuine DM disown (context switched away).
 */
static void platform_recreate_window(const char *reason) {
    fprintf(stderr, "platform_qnx: recreating window (reason=%s)\n", reason ? reason : "?");

    /* Detach + destroy the EGL surface bound to the dying window. */
    if (g_egl_display != EGL_NO_DISPLAY && g_egl_surface != EGL_NO_SURFACE) {
        eglMakeCurrent(g_egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        eglDestroySurface(g_egl_display, g_egl_surface);
        g_egl_surface = EGL_NO_SURFACE;
    }

    /* Recreate the managed window (backoff-throttled inside cluster_surface).  On
     * suppression/failure we leave g_egl_surface == EGL_NO_SURFACE; the next check
     * tick retries via the "egl surface missing" path. */
    if (!g_cs || cluster_surface_recreate(g_cs) != 0) {
        fprintf(stderr, "platform_qnx: window recreate suppressed/failed — will retry\n");
        return;
    }

    /* Re-bind EGL to the new window. */
    {
        EGLNativeWindowType nw = (EGLNativeWindowType)cluster_surface_window(g_cs);
        g_egl_surface = eglCreateWindowSurface(g_egl_display, g_egl_config, nw, NULL);
        if (g_egl_surface == EGL_NO_SURFACE) {
            fprintf(stderr, "platform_qnx: recreate eglCreateWindowSurface FAILED err=0x%x\n",
                    eglGetError());
            return;
        }
        if (g_egl_context != EGL_NO_CONTEXT) {
            eglMakeCurrent(g_egl_display, g_egl_surface, g_egl_surface, g_egl_context);
        }
    }

    fprintf(stderr, "platform_qnx: window recreated OK\n");
}

/*
 * Health check (cheap — call ~every 5 s).  cluster_surface_lost() does the
 * VISIBLE + MANAGER_STRING probes (disown detection with the seen-once handshake).
 * We also recover the case where a prior recreate was backoff-suppressed and left
 * the EGL surface torn down.
 */
void platform_check_and_recover_window(void) {
    if (!g_window_expected || !g_cs) return;

    /* A prior recreate was suppressed → EGL surface still down → retry. */
    if (g_egl_surface == EGL_NO_SURFACE) {
        platform_recreate_window("egl surface missing");
        return;
    }
    if (cluster_surface_lost(g_cs)) {
        platform_recreate_window("window lost/disowned");
    }
}

/* Destroy our managed window so displaymanager's m_surfaceSources[98] clears
 * promptly at shutdown (instead of at process-exit cleanup).  The slot then
 * stays empty; Java switches the cluster back to the stock context.
 * Counterpart to platform_check_and_recover_window (renderer atexit / shutdown). */
void platform_release_displayable(void) {
    if (g_cs) {
        cluster_surface_destroy(g_cs);
        g_cs = NULL;
        fprintf(stderr, "platform_qnx: managed window destroyed — displayable 98 released\n");
    }
}

/* Context focus is Java-driven now (DisplayManager.switchContext); this
 * renderer runs no dmdt.  Kept as a no-op so main.c / the macOS stub still
 * link against the same symbol. */
void platform_ensure_focus(void) {
}

static void signal_handler(int sig) {
    /* Only use async-signal-safe functions (write, signal, raise). */
    static const char msg_segv[] = "platform_qnx: caught SIGSEGV\n";
    static const char msg_abrt[] = "platform_qnx: caught SIGABRT\n";
    static const char msg_term[] = "platform_qnx: caught SIGTERM\n";
    static const char msg_unk[]  = "platform_qnx: caught signal\n";

    if (sig == SIGSEGV)      write(STDERR_FILENO, msg_segv, sizeof(msg_segv) - 1);
    else if (sig == SIGABRT) write(STDERR_FILENO, msg_abrt, sizeof(msg_abrt) - 1);
    else if (sig == SIGTERM) write(STDERR_FILENO, msg_term, sizeof(msg_term) - 1);
    else                     write(STDERR_FILENO, msg_unk,  sizeof(msg_unk)  - 1);

    g_should_close = 1;
    if (sig == SIGTERM) {
        return;
    }
    signal(sig, SIG_DFL);
    raise(sig);
}

int platform_init(int width, int height) {
    g_width = width;
    g_height = height;

    /* Our own displayable id (98), NOT the stock route-guidance slot (20).
     * No env override — pointing the renderer at a different id would silently
     * leave us out of the cluster MOST encoder path. */
    g_displayable_id = CR_DISPLAYABLE_ID;
    g_context_id     = read_env_int("CR_CONTEXT_ID", CR_CONTEXT_ID);
    g_display_id     = read_env_int("CR_DISPLAY_ID", CR_DISPLAY_ID);

    if (!getenv("IPL_CONFIG_DIR"))
        putenv("IPL_CONFIG_DIR=/etc/eso/production");

    signal(SIGTERM, signal_handler);
    signal(SIGSEGV, signal_handler);
    signal(SIGABRT, signal_handler);
    atexit(platform_release_displayable);

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

    /* Create the managed cluster window (→ screen_create_context) BEFORE eglGetDisplay.
     * The Qualcomm Adreno/GSL libEGL derefs a screen context that must already exist in
     * the process; without it eglGetDisplay SIGSEGVs.  This was masked when the HMI JVM
     * spawned us (inherited screen connection) but not standalone / as a framework service. */
    fprintf(stderr, "platform_qnx: pre-EGL cluster window (screen_create_context)...\n");
    if (ensure_cluster_window() != 0) return -1;

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

    EGLint num_configs = 0;
    fprintf(stderr, "platform_qnx: eglChooseConfig...\n");
    if (!eglChooseConfig(g_egl_display, config_attrs, &g_egl_config, 1, &num_configs) ||
        num_configs == 0) {
            fprintf(stderr, "platform_qnx: FAIL eglChooseConfig (err=0x%x)\n", eglGetError());
            return -1;
    }
    fprintf(stderr, "platform_qnx: got %d config(s)\n", num_configs);

    /* screen_* are linked directly (-lscreen) via cluster_surface — no dlsym. */

    /* Create the EGL context up-front (no surface needed for it).  This way
     * create_window_and_egl_surface() can perform eglMakeCurrent itself,
     * and the same helper is reusable from the recreate-on-loss path. */
    EGLint ctx_attrs[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
    eglBindAPI(EGL_OPENGL_ES_API);
    fprintf(stderr, "platform_qnx: eglCreateContext...\n");
    g_egl_context = eglCreateContext(g_egl_display, g_egl_config, EGL_NO_CONTEXT, ctx_attrs);
    if (g_egl_context == EGL_NO_CONTEXT) {
        fprintf(stderr, "platform_qnx: FAIL eglCreateContext (err=0x%x)\n", eglGetError());
        return -1;
    }

    fprintf(stderr, "platform_qnx: display_create_window(%dx%d, disp=%d)...\n",
            width, height, g_displayable_id);
    if (create_window_and_egl_surface() != 0) {
        return -1;
    }
    g_window_expected = 1;

    /* Keep the live BSP capability strings in the renderer log.  They are the
     * authoritative answer for cross-process Screen/EGL buffer import support;
     * SDK headers alone only describe APIs that may not be implemented by this
     * APQ8064 image. */
    fprintf(stderr, "platform_qnx: EGL vendor=%s version=%s extensions=%s\n",
            eglQueryString(g_egl_display, EGL_VENDOR),
            eglQueryString(g_egl_display, EGL_VERSION),
            eglQueryString(g_egl_display, EGL_EXTENSIONS));
    fprintf(stderr, "platform_qnx: GL vendor=%s renderer=%s version=%s extensions=%s\n",
            (const char *)glGetString(GL_VENDOR),
            (const char *)glGetString(GL_RENDERER),
            (const char *)glGetString(GL_VERSION),
            (const char *)glGetString(GL_EXTENSIONS));

    /* Do NOT switch focus here.  Java waits for EVT_FRAME_READY and then
     * forces a real away->74 context transition so DisplayManager's
     * preContextSwitchHook updates the MOST encoder after our first frame
     * is already queued. */

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

int platform_swap(void) {
    static unsigned failure_streak = 0;
    if (eglSwapBuffers(g_egl_display, g_egl_surface) == EGL_TRUE) {
        if (failure_streak > 0)
            fprintf(stderr, "platform_qnx: eglSwapBuffers recovered after %u failures\n",
                    failure_streak);
        failure_streak = 0;
        return 1;
    }
    /* Swap failed.  Common causes when displaymanager yanks our window
     * (native nav collision, KOMO context exit): EGL_BAD_SURFACE,
     * EGL_BAD_NATIVE_WINDOW, EGL_CONTEXT_LOST.  Without an explicit
     * recreate the renderer would spin on a dead surface forever,
     * since swap drives the loop's frame pacing. */
    EGLint err = eglGetError();
    failure_streak++;
    if (failure_streak == 1 || (failure_streak % 30u) == 0)
        fprintf(stderr, "platform_qnx: eglSwapBuffers failed err=0x%x streak=%u → recreate\n",
                err, failure_streak);
    platform_recreate_window("swap-failed");
    return 0;
}

void platform_poll(void) {
    /* QNX: no event loop, just check signal flag */
}

int platform_should_close(void) {
    return g_should_close;
}

void platform_shutdown(void) {
    int hidden = 0;
    struct timespec release_wait;
    /* Clean shutdown sequence:
     *   1. Release GL context and EGL surface while keeping EGLDisplay /
     *      displayinit globals alive (full teardown of shared display
     *      resources can collide with native components).
     *   2. Explicitly destroy our screen_window via screen_destroy_window
     *      so displaymanager's m_surfaceSources[98] vacates promptly.
     * Context restore (switching the cluster back to the stock context) is
     * Java's job now — this renderer runs no dmdt. */
    if (g_egl_display != EGL_NO_DISPLAY) {
        if (g_cs && cluster_surface_window(g_cs))
            screen_set_window_property_iv(cluster_surface_window(g_cs),
                                          SCREEN_PROPERTY_VISIBLE, &hidden);
        if (g_egl_context != EGL_NO_CONTEXT) glFinish();
        release_wait.tv_sec = 0;
        release_wait.tv_nsec = 50L * 1000L * 1000L;
        while (nanosleep(&release_wait, &release_wait) != 0 && errno == EINTR) {}
        eglMakeCurrent(g_egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (g_egl_surface != EGL_NO_SURFACE) {
            eglDestroySurface(g_egl_display, g_egl_surface);
            g_egl_surface = EGL_NO_SURFACE;
        }
        if (g_egl_context != EGL_NO_CONTEXT) {
            /* Release the GL context so the driver frees its internal
             * resources (shader cache, framebuffer attachments, command
             * buffers).  Skip eglTerminate — EGLDisplay is shared with
             * native cluster components and terminating it would tear
             * down their surfaces too. */
            eglDestroyContext(g_egl_display, g_egl_context);
            g_egl_context = EGL_NO_CONTEXT;
        }
    }
    platform_release_displayable();
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
