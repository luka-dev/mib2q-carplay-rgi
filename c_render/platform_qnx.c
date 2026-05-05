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
#include <ctype.h>
#include <signal.h>
#include <dlfcn.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>

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

/* Native window handle from libdisplayinit's display_create_window — kept
 * for periodic re-claim of displayable 20 binding (see platform_reclaim_displayable).
 * Native nav (libRenderSystem in nav app process) can re-register its own
 * screen window with ID="20" at any time, which would steal our binding in
 * displaymanager's m_surfaceSources[20].  Periodic screen_manage_window on
 * our handle wins back the binding within a few seconds. */
static EGLNativeWindowType g_native_window = 0;
typedef int (*screen_manage_window_fn)(void *, const char *);
typedef int (*screen_destroy_window_fn)(void *);
static screen_manage_window_fn g_screen_manage_window = NULL;
static screen_destroy_window_fn g_screen_destroy_window = NULL;

/* Canonical "managed window group" name used by libdisplayinit.so on this
 * platform (verified by RE of screen_manage_window call in
 * display_create_window_nbuffers).  All windows created via libdisplayinit
 * belong to this group — so to re-claim our binding via screen_manage_window
 * we MUST pass exactly this string.  Any other value would either detach
 * the window from libdisplayinit's group or fail outright. */
static const char LIBDISPLAYINIT_WINDOW_GROUP[] = "How are you gentlemen?";

/* Overridable IDs (env: CR_DISPLAYABLE_ID, CR_CONTEXT_ID, CR_DISPLAY_ID) */
static int g_displayable_id;
static int g_context_id;
static int g_display_id;
static uint64_t g_last_focus_force_ms = 0;

#define DMDT_FOCUS_FORCE_BACKOFF_MS 30000ULL

static uint64_t monotonic_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)(ts.tv_nsec / 1000000ULL);
}

static int read_env_int(const char *name, int def) {
    const char *v = getenv(name);
    return (v && v[0]) ? atoi(v) : def;
}

static int ascii_contains_ci(const char *haystack, const char *needle) {
    size_t nlen;
    if (!haystack || !needle) return 0;
    nlen = strlen(needle);
    if (nlen == 0) return 1;
    for (; *haystack; haystack++) {
        size_t i;
        for (i = 0; i < nlen; i++) {
            unsigned char a = (unsigned char)haystack[i];
            unsigned char b = (unsigned char)needle[i];
            if (!a) return 0;
            if (tolower(a) != tolower(b)) break;
        }
        if (i == nlen) return 1;
    }
    return 0;
}

static int parse_first_int(const char *s, int *out) {
    int sign = 1;
    int value = 0;
    int seen = 0;
    if (!s || !out) return 0;
    while (*s && !isdigit((unsigned char)*s) && *s != '-') s++;
    if (*s == '-') {
        sign = -1;
        s++;
    }
    while (*s && isdigit((unsigned char)*s)) {
        value = value * 10 + (*s - '0');
        seen = 1;
        s++;
    }
    if (!seen) return 0;
    *out = value * sign;
    return 1;
}

static int parse_context_line(const char *line, int *ctx) {
    const char *p;
    if (!ascii_contains_ci(line, "context")) return 0;

    p = strchr(line, ':');
    if (p && parse_first_int(p + 1, ctx)) return 1;

    p = strchr(line, '=');
    if (p && parse_first_int(p + 1, ctx)) return 1;

    return parse_first_int(line, ctx);
}

static int line_value_contains_ci(const char *line, const char *key, const char *needle) {
    const char *p;
    if (!ascii_contains_ci(line, key)) return 0;
    p = strchr(line, ':');
    return ascii_contains_ci(p ? p + 1 : line, needle);
}

static void force_display_context(const char *reason, int observed_ctx) {
    char cmd[128];
    uint64_t now = monotonic_ms();
    if (g_last_focus_force_ms != 0 &&
        now - g_last_focus_force_ms < DMDT_FOCUS_FORCE_BACKOFF_MS) {
        if (observed_ctx >= 0) {
            fprintf(stderr, "platform_qnx: dmdt focus ctx=%d, suppressing force ctx=%d (%s)\n",
                    observed_ctx, g_context_id, reason);
        } else {
            fprintf(stderr, "platform_qnx: dmdt focus unknown, suppressing force ctx=%d (%s)\n",
                    g_context_id, reason);
        }
        return;
    }
    g_last_focus_force_ms = now;

    if (observed_ctx >= 0) {
        fprintf(stderr, "platform_qnx: dmdt focus ctx=%d, forcing ctx=%d (%s)\n",
                observed_ctx, g_context_id, reason);
    } else {
        fprintf(stderr, "platform_qnx: dmdt focus unknown, forcing ctx=%d (%s)\n",
                g_context_id, reason);
    }
    snprintf(cmd, sizeof(cmd), "/eso/bin/apps/dmdt sc %d %d", g_display_id, g_context_id);
    system(cmd);
    g_display_routed = 1;
}

/* Re-claim displayable 20 binding by calling screen_manage_window on our
 * window handle.  Defends against native nav (libRenderSystem in nav app
 * process) re-registering its own screen window with ID="20" mid-session
 * and stealing our binding in displaymanager's m_surfaceSources[20].
 *
 * Cost: one screen API call (~ms).  Idempotent — re-managing an already
 * managed window is a no-op on the screen subsystem side, but updates
 * displaymanager's last-writer pointer back to us. */
void platform_reclaim_displayable(void) {
    if (!g_native_window || !g_screen_manage_window) return;
    int rc = g_screen_manage_window((void *)(uintptr_t)g_native_window,
                                    LIBDISPLAYINIT_WINDOW_GROUP);
    if (rc != 0) {
        fprintf(stderr, "platform_qnx: screen_manage_window reclaim rc=%d\n", rc);
    }
}

/* Explicitly destroy our screen window so displaymanager's
 * m_surfaceSources[20] clears immediately — instead of waiting for kernel
 * cleanup at process exit.
 *
 * After this call the slot is *empty*.  It does NOT auto-fall back to a
 * native widget window: with stock libPresentationController, the native
 * KOMO RG widget only calls display_create_window(20) when its
 * GuidanceView state machine enters StartDrawing — which requires an
 * active native route.  In idle (no native route, normal post-CarPlay
 * state) AppStartATF holds no window for displayable 20, so the slot
 * just stays empty.  This matches the cluster baseline state before
 * we ever started: blank widget layer until a native route activates.
 *
 * Counterpart to platform_reclaim_displayable.  Caller (renderer atexit /
 * platform_shutdown) invokes this so the slot vacates promptly. */
void platform_release_displayable(void) {
    if (!g_native_window || !g_screen_destroy_window) return;
    int rc = g_screen_destroy_window((void *)(uintptr_t)g_native_window);
    if (rc != 0) {
        fprintf(stderr, "platform_qnx: screen_destroy_window release rc=%d\n", rc);
    } else {
        fprintf(stderr, "platform_qnx: window destroyed — displayable 20 released\n");
    }
    g_native_window = 0;
}

/* Display restore — re-declare context 74 with its original native
 * composition (MAP_ROUTE_GUIDANCE 20 + images 102/101 + KOMBI_MAP_VIEW 33)
 * and switch cluster back to it.
 *
 * After our screen window is destroyed, m_surfaceSources[20] is empty.
 * The slot stays empty until native nav next enters StartDrawing (when a
 * native route activates), at which point libPresentationController
 * creates its own ID="20" window via display_create_window.  The dmdt
 * commands here just keep the context layout consistent and force the
 * cluster compositor to pick the original definition immediately —
 * ready for a future native render to populate.
 *
 * Idempotent and signal-safe via system() — atexit hook calls this on
 * graceful exit.  Java also re-declares this native layout as a backstop
 * before force-killing the renderer. */
static void restore_display(void) {
    if (g_display_routed) {
        char cmd[128];
        snprintf(cmd, sizeof(cmd), "/eso/bin/apps/dmdt dc 74 20 102 101 33");
        system(cmd);
        snprintf(cmd, sizeof(cmd), "/eso/bin/apps/dmdt sc %d 74", g_display_id);
        system(cmd);
        g_display_routed = 0;
    }
}

static int read_dmdt_cluster_context(void) {
    FILE *fp = popen("/eso/bin/apps/dmdt gs 2>&1", "r");
    if (!fp) return -1;

    char line[256];
    char sample[512];
    int display_index = -1;
    int in_target_display = 0;
    int last_context = -1;
    int context_count = 0;
    int line_count = 0;
    int ctx = -1;
    int status;
    size_t sample_len = 0;

    sample[0] = '\0';
    while (fgets(line, sizeof(line), fp)) {
        size_t line_len;
        line_count++;
        line_len = strlen(line);
        if (sample_len + line_len + 1 < sizeof(sample)) {
            memcpy(sample + sample_len, line, line_len);
            sample_len += line_len;
            sample[sample_len] = '\0';
        }
        if (ascii_contains_ci(line, "display:")) {
            int display_id = -1;
            display_index++;
            parse_first_int(line, &display_id);
            in_target_display = ascii_contains_ci(line, "Cluster")
                             || display_index == g_display_id
                             || display_id == g_display_id;
            continue;
        }
        if (line_value_contains_ci(line, "terminal", "LVDS2")) {
            in_target_display = in_target_display || g_display_id == 1;
            continue;
        }
        if (line_value_contains_ci(line, "terminal", "LVDS1")) {
            in_target_display = in_target_display || g_display_id == 0;
            continue;
        }
        if (parse_context_line(line, &last_context)) {
            context_count++;
            if (in_target_display) {
                ctx = last_context;
                break;
            }
        }
    }
    status = pclose(fp);
    if (ctx < 0 && context_count == 1) ctx = last_context;
    if (ctx < 0) {
        fprintf(stderr,
                "platform_qnx: dmdt gs parse failed status=%d lines=%d displays=%d contexts=%d sample=%s\n",
                status, line_count, display_index + 1, context_count,
                sample[0] ? sample : "(empty)");
    }
    return ctx;
}

/* Sync version — runs `dmdt gs` (popen, ~100-200 ms on QNX) and reroutes
 * if context drifted.  Called only from the watchdog thread; main render
 * loop must NEVER call this directly because the popen fork+exec causes
 * a visible micro-freeze (~150 ms) at each 5 s tick. */
static void check_and_restore_focus(void) {
    int ctx = read_dmdt_cluster_context();
    if (ctx == g_context_id) return;

    if (ctx < 0) {
        force_display_context("parse-failed", -1);
        return;
    }

    force_display_context("mismatch", ctx);
}

/* One-shot focus check thread — spawned by main loop on demand.
 * Detached, auto-exits when check_and_restore_focus() returns.
 * Avoids both:
 *  - Blocking the render loop with popen() (~150 ms freeze).
 *  - Keeping a persistent watchdog thread alive between checks.
 *
 * `g_focus_check_inflight` prevents overlapping spawns if the previous
 * check hasn't finished by the time main triggers another. */
static volatile int g_focus_check_inflight = 0;

static void *focus_check_once(void *arg) {
    (void)arg;
    check_and_restore_focus();
    g_focus_check_inflight = 0;
    return NULL;
}

void platform_ensure_focus(void) {
    if (g_focus_check_inflight) return;
    g_focus_check_inflight = 1;
    pthread_t tid;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    if (pthread_create(&tid, &attr, focus_check_once, NULL) != 0) {
        fprintf(stderr, "platform_qnx: focus check pthread_create failed\n");
        g_focus_check_inflight = 0;
    }
    pthread_attr_destroy(&attr);
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
    if (sig == SIGTERM) {
        return;
    }
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

    /* Displayable ID is hardcoded to 20 (DISPLAYABLE_MAP_ROUTE_GUIDANCE).
     * No env override — accidentally pointing the renderer at a different
     * id would silently leave us out of the cluster MOST encoder path. */
    g_displayable_id = CR_DISPLAYABLE_ID;
    g_context_id     = read_env_int("CR_CONTEXT_ID", CR_CONTEXT_ID);
    g_display_id     = read_env_int("CR_DISPLAY_ID", CR_DISPLAY_ID);

    if (!getenv("IPL_CONFIG_DIR"))
        putenv("IPL_CONFIG_DIR=/etc/eso/production");

    signal(SIGTERM, signal_handler);
    signal(SIGSEGV, signal_handler);
    signal(SIGABRT, signal_handler);
    atexit(restore_display);
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

        /* Persist handle for periodic reclaim of displayable 20 binding. */
        g_native_window = native_window;

        /* Resolve libscreen.so once and KEEP it open: we need
         * screen_manage_window for the periodic reclaim path, and
         * screen_set_window_property_iv for the transparency setup below.
         * Closing libscreen mid-session would lose g_screen_manage_window. */
        {
            void *libscr = dlopen("libscreen.so.1", RTLD_LAZY);
            if (!libscr) libscr = dlopen("libscreen.so", RTLD_LAZY);
            if (libscr) {
                g_screen_manage_window =
                    (screen_manage_window_fn)dlsym(libscr, "screen_manage_window");
                if (!g_screen_manage_window) {
                    fprintf(stderr, "platform_qnx: WARN: dlsym(screen_manage_window) failed — reclaim disabled\n");
                }
                g_screen_destroy_window =
                    (screen_destroy_window_fn)dlsym(libscr, "screen_destroy_window");
                if (!g_screen_destroy_window) {
                    fprintf(stderr, "platform_qnx: WARN: dlsym(screen_destroy_window) failed — release disabled\n");
                }

                int (*set_prop_iv)(void*, int, const int*) =
                    (int (*)(void*, int, const int*))dlsym(libscr, "screen_set_window_property_iv");
                if (set_prop_iv) {
                    /* SCREEN_PROPERTY_TRANSPARENCY=17, SOURCE_OVER=2 — clear color
                     * (0,0,0,0) becomes transparent so native map (displayable 33)
                     * composites underneath when both are in context 74. */
                    int val = 2;
                    if (set_prop_iv((void*)(uintptr_t)native_window, 17, &val) == 0) {
                        fprintf(stderr, "platform_qnx: transparency=SOURCE_OVER\n");
                    } else {
                        fprintf(stderr, "platform_qnx: WARN: set transparency failed\n");
                    }
                }
                /* DO NOT dlclose(libscr) — g_screen_manage_window must stay live */
            } else {
                fprintf(stderr, "platform_qnx: WARN: libscreen.so dlopen failed — reclaim disabled\n");
            }
        }
    }

    /* Re-declare context 74 with the full stock composition.
     * display_create_window(displayable_id=20) registers our window and as
     * a side effect strips other displayables from context 74 — so we put
     * them back: 20 (now bound to our screen window) + 102 + 101 + 33
     * (KOMBI_MAP_VIEW = native map background).
     *
     * setActiveDisplayable(4, 20) (called by stock cluster firmware in
     * preContextSwitchHook) makes the MOST encoder read displayable 20 —
     * which is now our window. */
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "/eso/bin/apps/dmdt dc %d %d 102 101 33",
             g_context_id, g_displayable_id);
    fprintf(stderr, "platform_qnx: %s\n", cmd);
    system(cmd);

    /* Do NOT switch focus here.  Java waits for EVT_FRAME_READY and then
     * forces a real away->74 context transition so DisplayManager's
     * preContextSwitchHook updates the MOST encoder after our first frame
     * is already queued. */
    g_display_routed = 1;

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
    /* Clean shutdown sequence:
     *   1. Release GL context and EGL surface while keeping EGLDisplay /
     *      displayinit globals alive (full teardown of shared display
     *      resources can collide with native components).
     *   2. Explicitly destroy our screen_window via screen_destroy_window
     *      so displaymanager's m_surfaceSources[20] vacates promptly.
     *      With stock libPresentationController, native widget only
     *      creates a window for displayable 20 when its state machine
     *      enters StartDrawing (i.e. an active native route).  In idle
     *      (typical post-CarPlay state) the slot just stays empty —
     *      this matches the cluster baseline before we ever started.
     *   3. Restore context 74's stock composition via dmdt as a backstop.
     *
     * In-flight focus check thread (if any) is detached and self-cleans;
     * no need to wait for it. */
    if (g_egl_display != EGL_NO_DISPLAY) {
        eglMakeCurrent(g_egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (g_egl_surface != EGL_NO_SURFACE) {
            eglDestroySurface(g_egl_display, g_egl_surface);
            g_egl_surface = EGL_NO_SURFACE;
        }
    }
    platform_release_displayable();
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
