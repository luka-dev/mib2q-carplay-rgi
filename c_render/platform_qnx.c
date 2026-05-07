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
#include <errno.h>

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
static EGLConfig  g_egl_config  = 0;          /* saved for window recreate */
static int g_display_routed = 0;
static int g_width = 0, g_height = 0;
static volatile int g_should_close = 0;

/* Saved at platform_init for reuse by platform_check_and_recover_window. */
static void *g_displib = NULL;
static display_create_window_fn g_dcw = NULL;

/*
 * Native screen_window_t from libdisplayinit's display_create_window.
 *
 * NOTE on QNX Screen cross-process semantics (verified by RE of libscreen.so
 * and libdm_modMain.so for MU1316):
 *   - Each process holds a process-local malloc()'d struct (with a magic
 *     header) representing its handle to the window.
 *   - screen_destroy_window() called in displaymanager's context only frees
 *     ITS local struct — it does NOT cascade-destroy ours, but for "remote"
 *     windows it does send an RPC to the screen server (opcode 10) which
 *     may eventually invalidate our struct.
 *   - displaymanager's evtNewWindow handler, on collision (another window
 *     registered with same ID="20"), calls screen_destroy_window on its
 *     handle to our window AND removes us from m_surfaceSources[20].  Our
 *     g_native_window struct may stay valid in our process even though the
 *     encoder no longer reads from us — this is the "phantom" failure mode.
 *
 * Recovery strategy (platform_check_and_recover_window):
 *   - Probe the window via screen_get_window_property_iv/cv every ~5 s.
 *   - Detect destroyed (errno=ENOENT/EBADF/EINVAL) OR detached
 *     (MANAGER_STRING no longer matches displaymanager's group).
 *   - On loss: tear down EGL surface, call display_create_window again
 *     to register a fresh ID="20" window with displaymanager, recreate
 *     EGL surface.  Inflight fps backoff (100 ms) prevents flapping.
 */
static EGLNativeWindowType g_native_window = 0;
typedef int (*screen_destroy_window_fn)(void *);
typedef int (*screen_get_property_iv_fn)(void *, int, int *);
typedef int (*screen_get_property_cv_fn)(void *, int, int, char *);
typedef int (*screen_set_property_iv_fn)(void *, int, const int *);
static screen_destroy_window_fn  g_screen_destroy_window      = NULL;
static screen_get_property_iv_fn g_screen_get_window_property_iv = NULL;
static screen_get_property_cv_fn g_screen_get_window_property_cv = NULL;
static screen_set_property_iv_fn g_screen_set_window_property_iv = NULL;

/* QNX Screen property IDs we use (verified by RE — these are stable
 * across QNX 6.5 SDP releases). */
#define SCR_PROP_VISIBLE         51
#define SCR_PROP_TRANSPARENCY    17
#define SCR_PROP_MANAGER_STRING 152
#define SCR_TRANSP_SOURCE_OVER    2

/* The exact MANAGER_STRING that libdm_modMain.so assigns to windows it
 * has taken into m_surfaceSources (verified by RE — see
 * CScreenHandler::handleScreenEvent case 2 SCREEN_PROPERTY_MANAGER_STRING).
 * If our window's MANAGER_STRING ever differs, we have been disowned. */
static const char DM_MANAGED_GROUP[] = "All your base are belong to us!";

/* Recreate stats / backoff. */
static int g_recreate_count = 0;
static struct timespec g_last_recreate_ts = {0, 0};

/* Set to 1 once platform_init has successfully created the initial window.
 * The health check uses this to distinguish "renderer not yet up, ignore"
 * from "we had a window and lost it, retry recreate even though
 * g_native_window is currently 0".  Without this flag, a single failed
 * recreate would wedge the renderer permanently (g_native_window stays 0
 * → check early-exits → no further attempts). */
static int g_window_expected = 0;

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

/*
 * Recreate just the screen_window + EGL surface (preserving the EGL
 * display, EGL context, and libdisplayinit screen_context).  Called both
 * from platform_init (initial window creation) and from
 * platform_check_and_recover_window (after we lose displaymanager's
 * m_surfaceSources[20] binding).
 */
static int create_window_and_egl_surface(void) {
    if (!g_dcw || g_egl_display == EGL_NO_DISPLAY || g_egl_config == 0) {
        fprintf(stderr, "platform_qnx: create_window: missing prerequisites\n");
        return -1;
    }

    EGLNativeWindowType native_window = 0;
    int kd_window = 0;
    int ret = g_dcw(g_egl_display, g_egl_config,
                    g_width, g_height, g_displayable_id,
                    &native_window, &kd_window);
    if (!native_window) {
        fprintf(stderr, "platform_qnx: display_create_window FAILED ret=%d\n", ret);
        return -1;
    }
    g_native_window = native_window;
    fprintf(stderr, "platform_qnx: window created native=%p kd=%d\n",
            (void *)(uintptr_t)native_window, kd_window);

    /* Transparency: clear color (0,0,0,0) → transparent so the cluster
     * compositor can blend us on top of native map (displayable 33). */
    if (g_screen_set_window_property_iv) {
        int val = SCR_TRANSP_SOURCE_OVER;
        if (g_screen_set_window_property_iv((void *)(uintptr_t)native_window,
                                            SCR_PROP_TRANSPARENCY, &val) != 0) {
            fprintf(stderr, "platform_qnx: WARN: set transparency failed errno=%d\n",
                    errno);
        }
    }

    g_egl_surface = eglCreateWindowSurface(g_egl_display, g_egl_config,
                                            native_window, NULL);
    if (g_egl_surface == EGL_NO_SURFACE) {
        fprintf(stderr, "platform_qnx: eglCreateWindowSurface FAILED err=0x%x\n",
                eglGetError());
        return -1;
    }

    if (g_egl_context != EGL_NO_CONTEXT) {
        if (!eglMakeCurrent(g_egl_display, g_egl_surface, g_egl_surface,
                            g_egl_context)) {
            fprintf(stderr, "platform_qnx: eglMakeCurrent FAILED err=0x%x\n",
                    eglGetError());
            return -1;
        }
    }

    /* Re-declare context 74 with the full stock composition.
     * display_create_window(displayable_id=20) registers our window and as
     * a side effect strips other displayables from context 74 — so we put
     * them back: 20 (now bound to our screen window) + 102 + 101 + 33
     * (KOMBI_MAP_VIEW = native map background).  Both initial init and
     * the post-collision recreate path benefit: without this on recreate,
     * context 74 would lose 102/101/33 and the cluster compositor would
     * stop blending overlay icons / native map underneath us.
     *
     * setActiveDisplayable(4, 20) (called by stock cluster firmware in
     * preContextSwitchHook) makes the MOST encoder read displayable 20 —
     * which is now (again) our window. */
    {
        char cmd[128];
        snprintf(cmd, sizeof(cmd), "/eso/bin/apps/dmdt dc %d %d 102 101 33",
                 g_context_id, g_displayable_id);
        fprintf(stderr, "platform_qnx: %s\n", cmd);
        system(cmd);
        g_display_routed = 1;
    }

    return 0;
}

/*
 * Tear down current screen_window + EGL surface and rebuild from scratch.
 * Used when we detect we have lost the displaymanager m_surfaceSources[20]
 * binding (either window destroyed, or detached from displaymanager group).
 *
 * Backoff: max one recreate per 100 ms — prevents tight flapping in case
 * native nav also re-creates aggressively (the loser of the collision
 * forces displaymanager to call screen_destroy_window on the previous
 * winner, so a tit-for-tat could otherwise burn CPU).
 */
static void platform_recreate_window(const char *reason) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    int64_t delta_ms = (int64_t)(now.tv_sec - g_last_recreate_ts.tv_sec) * 1000
                     + (int64_t)(now.tv_nsec - g_last_recreate_ts.tv_nsec) / 1000000;
    if (g_last_recreate_ts.tv_sec != 0 && delta_ms < 100) {
        fprintf(stderr,
                "platform_qnx: window recreate suppressed (last=%lld ms ago, reason=%s)\n",
                (long long)delta_ms, reason ? reason : "?");
        return;
    }
    g_last_recreate_ts = now;
    g_recreate_count++;

    fprintf(stderr, "platform_qnx: recreating window (reason=%s, count=%d)\n",
            reason ? reason : "?", g_recreate_count);

    /* 1. Detach EGL from current surface — must precede eglDestroySurface. */
    if (g_egl_display != EGL_NO_DISPLAY) {
        eglMakeCurrent(g_egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE,
                       EGL_NO_CONTEXT);
        if (g_egl_surface != EGL_NO_SURFACE) {
            eglDestroySurface(g_egl_display, g_egl_surface);
            g_egl_surface = EGL_NO_SURFACE;
        }
    }

    /* 2. Best-effort destroy our local screen_window struct.  May fail if
     * it was already invalidated by the screen server (errno=ENOENT). */
    if (g_native_window && g_screen_destroy_window) {
        int rc = g_screen_destroy_window((void *)(uintptr_t)g_native_window);
        if (rc != 0) {
            fprintf(stderr,
                    "platform_qnx: recreate: screen_destroy_window rc=%d errno=%d (%s)\n",
                    rc, errno, strerror(errno));
        }
    }
    g_native_window = 0;

    /* 3. Re-register with displaymanager: new screen_window with ID="20",
     * new EGL surface bound to it. */
    if (create_window_and_egl_surface() != 0) {
        fprintf(stderr, "platform_qnx: recreate FAILED — renderer will continue "
                        "with no surface; main loop will keep trying\n");
        return;
    }

    fprintf(stderr, "platform_qnx: window recreated OK\n");
}

/*
 * Hybrid health check (200 µs of work — cheap to call every 5 s):
 *   1. screen_get_window_property_iv(SCR_PROP_VISIBLE) — if it errors
 *      with ENOENT/EBADF/EINVAL, our local struct was invalidated.
 *      (Less common in QNX 6.5 because screen_destroy_window in another
 *      process only frees that process's local handle, but we cover it
 *      anyway in case the screen-server RPC opcode 10 invalidates ours.)
 *   2. screen_get_window_property_cv(SCR_PROP_MANAGER_STRING) — if our
 *      window is no longer in displaymanager's "All your base ..." group,
 *      we have been disowned (this is the COMMON failure mode: native nav
 *      created its own ID="20" window, displaymanager swapped
 *      m_surfaceSources[20] over to native).
 *
 * Probe 2 is gated on a "first sighting" handshake so we never act on a
 * driver/firmware that doesn't populate MANAGER_STRING at all (would
 * otherwise look like a permanent disown to us and cause infinite
 * recreate loops).  We only treat manager_string mismatches as authoritative
 * after observing the expected value at least once.
 *
 * On either signal: trigger platform_recreate_window().
 */
static int g_manager_string_seen = 0;  /* observed expected value at least once */

void platform_check_and_recover_window(void) {
    /* Renderer may not have finished init yet — nothing to recover. */
    if (!g_window_expected) return;

    /* Initial-init succeeded then a previous recreate failed (g_native_window
     * cleared but never re-populated).  Retry recreate on this tick — backoff
     * inside platform_recreate_window throttles the retry rate. */
    if (!g_native_window) {
        platform_recreate_window("retry after failed recreate");
        return;
    }

    int needs_recreate = 0;
    const char *reason = NULL;

    /* Probe 1: window struct still valid in our context. */
    if (g_screen_get_window_property_iv) {
        int visible = -1;
        errno = 0;
        int rc = g_screen_get_window_property_iv(
            (void *)(uintptr_t)g_native_window,
            SCR_PROP_VISIBLE, &visible);
        if (rc != 0) {
            int err = errno;
            fprintf(stderr,
                    "platform_qnx: window probe property_iv rc=%d errno=%d (%s)\n",
                    rc, err, strerror(err));
            if (err == ENOENT || err == EBADF || err == EINVAL) {
                needs_recreate = 1;
                reason = "window struct invalidated";
            }
        }
    }

    /* Probe 2: still in displaymanager's group. */
    if (!needs_recreate && g_screen_get_window_property_cv) {
        char manager[64] = {0};
        errno = 0;
        int rc = g_screen_get_window_property_cv(
            (void *)(uintptr_t)g_native_window,
            SCR_PROP_MANAGER_STRING,
            (int)(sizeof(manager) - 1), manager);
        if (rc != 0) {
            int err = errno;
            fprintf(stderr,
                    "platform_qnx: window probe manager_string rc=%d errno=%d (%s)\n",
                    rc, err, strerror(err));
            if (err == ENOENT || err == EBADF || err == EINVAL) {
                needs_recreate = 1;
                reason = "manager_string probe failed";
            }
        } else if (strcmp(manager, DM_MANAGED_GROUP) == 0) {
            /* Expected value — record so future mismatches are authoritative. */
            if (!g_manager_string_seen) {
                fprintf(stderr,
                        "platform_qnx: manager_string handshake OK ('%s')\n",
                        manager);
                g_manager_string_seen = 1;
            }
        } else if (g_manager_string_seen) {
            /* Saw the expected value before, now it has changed → genuine
             * disown.  Treat as authoritative loss signal. */
            fprintf(stderr,
                    "platform_qnx: window manager_string='%s' (expected '%s')\n",
                    manager, DM_MANAGED_GROUP);
            needs_recreate = 1;
            reason = "manager_string changed";
        } else {
            /* Never saw expected value — driver may not populate this
             * property.  Log once so we know to fall back on probe 1
             * exclusively, but do not recreate based on probe 2 alone. */
            static int warned = 0;
            if (!warned) {
                fprintf(stderr,
                        "platform_qnx: WARN: manager_string='%s' from first probe "
                        "(expected '%s') — disabling probe 2 disown detection\n",
                        manager, DM_MANAGED_GROUP);
                warned = 1;
            }
        }
    }

    if (needs_recreate) {
        platform_recreate_window(reason);
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
 * Counterpart to platform_check_and_recover_window.  Caller (renderer
 * atexit / platform_shutdown) invokes this so the slot vacates promptly. */
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
    g_displib = open_displayinit();
    if (!g_displib) return -1;

    display_init_fn di = (display_init_fn)dlsym(g_displib, "display_init");
    if (!di) {
        fprintf(stderr, "platform_qnx: FAIL dlsym display_init\n");
        dlclose(g_displib);
        return -1;
    }
    fprintf(stderr, "platform_qnx: display_init...\n");
    di(0, 0);

    g_dcw = (display_create_window_fn)dlsym(g_displib, "display_create_window");
    if (!g_dcw) {
        fprintf(stderr, "platform_qnx: FAIL dlsym display_create_window\n");
        dlclose(g_displib);
        return -1;
    }

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

    /* Resolve libscreen.so symbols once, keep open: we need them for the
     * health-check / window-recreate path during the session.  Closing
     * libscreen mid-session would invalidate the function pointers. */
    {
        void *libscr = dlopen("libscreen.so.1", RTLD_LAZY);
        if (!libscr) libscr = dlopen("libscreen.so", RTLD_LAZY);
        if (libscr) {
            g_screen_destroy_window =
                (screen_destroy_window_fn)dlsym(libscr, "screen_destroy_window");
            g_screen_get_window_property_iv =
                (screen_get_property_iv_fn)dlsym(libscr, "screen_get_window_property_iv");
            g_screen_get_window_property_cv =
                (screen_get_property_cv_fn)dlsym(libscr, "screen_get_window_property_cv");
            g_screen_set_window_property_iv =
                (screen_set_property_iv_fn)dlsym(libscr, "screen_set_window_property_iv");
            if (!g_screen_destroy_window)
                fprintf(stderr, "platform_qnx: WARN: dlsym(screen_destroy_window) failed\n");
            if (!g_screen_get_window_property_iv)
                fprintf(stderr, "platform_qnx: WARN: dlsym(screen_get_window_property_iv) failed\n");
            if (!g_screen_get_window_property_cv)
                fprintf(stderr, "platform_qnx: WARN: dlsym(screen_get_window_property_cv) failed\n");
            if (!g_screen_set_window_property_iv)
                fprintf(stderr, "platform_qnx: WARN: dlsym(screen_set_window_property_iv) failed\n");
            /* DO NOT dlclose(libscr) — function pointers must stay live */
        } else {
            fprintf(stderr, "platform_qnx: WARN: libscreen.so dlopen failed — health-check disabled\n");
        }
    }

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
