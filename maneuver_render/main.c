/*
 * CarPlay Cluster Renderer -- main entry point.
 *
 * Command-driven rendering engine with TCP server.
 * Receives maneuver commands, handles all animation/transitions internally.
 *
 * macOS: GLFW window for development.
 * QNX: own managed displayable 98 in Java-selected cluster contexts.
 * See protocol.h for DisplayManager routing and ownership.
 *
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */

#include "log_stamp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <sched.h>
#include <signal.h>
#include <pthread.h>
#include <unistd.h>
#ifdef PLATFORM_QNX
#include <sys/neutrino.h>     /* setprio() */
#endif

#include "platform.h"

#define TARGET_FPS     30
#define FRAME_TIME_NS  (1000000000L / TARGET_FPS)
#include "gl_compat.h"
#include "render.h"
#include "arrow_progress.h"
#include "protocol.h"
#include "maneuver_command.h"
#include "maneuver.h"
#include "server.h"
#include "scene/scene.h"
#include "lane_guidance.h"
#include "lane_panel.h"

#define WINDOW_W CR_DEFAULT_WIDTH
#define WINDOW_H CR_DEFAULT_HEIGHT
#define FLAG_ATLAS_LOCAL_PATH      "./flag_atlas.rgba"
#define FLAG_ATLAS_PRODUCTION_PATH "/mnt/app/root/hooks/flag_atlas.rgba"

static int timespec_elapsed_at_least(const struct timespec *now,
                                     const struct timespec *last,
                                     long seconds,
                                     long nanoseconds) {
    long ds;
    long dns;

    if (last->tv_sec == 0) return 1;

    ds = (long)(now->tv_sec - last->tv_sec);
    if (ds > seconds) return 1;
    if (ds < seconds) return 0;

    dns = now->tv_nsec - last->tv_nsec;
    return dns >= nanoseconds;
}

/* ================================================================
 * Engine state machine
 *
 * IDLE:       showing current maneuver, slide=1.0
 * PUSHING:    slide 1->2, current pushes out + next slides in
 * SLIDING_IN: commit next->current, slide 0->1
 * ================================================================ */

typedef enum {
    ENGINE_IDLE,
    ENGINE_PUSHING,
    ENGINE_SLIDING_IN
} engine_phase_t;

typedef struct {
    maneuver_state_t current;
    maneuver_state_t next;
    maneuver_state_t pending;
    cr_scene_t *current_scene, *next_scene;
    int              has_current;   /* invariant: 1 after clear_maneuver() initializes FOLLOW_STREET */
    int              has_next;
    int              has_pending;   /* queued during PUSHING */
    engine_phase_t   phase;
    int              dirty;
} cr_engine_t;

static cr_engine_t g_engine;
// Independent panel input. Never passed into route geometry or animation.
static cr_lane_guidance_t g_lane_guidance;
static cr_lane_decoder_t g_lane_decoder;
static cr_lane_panel_t *g_lane_panel;

static cr_scene_t *engine_scene(const maneuver_state_t *m) {
    if(m==&g_engine.current)return g_engine.current_scene;
    if(m==&g_engine.next)return g_engine.next_scene;
    return NULL;
}
static int scene_handles(void *ctx,const maneuver_state_t *m) {
    cr_scene_t *s=engine_scene(m);(void)ctx;
    return s && cr_scene_route(s) && !cr_scene_is_native(s);
}
static void scene_route(void *ctx,const maneuver_state_t *m,route_path_t *out) {
    const route_path_t *p=cr_scene_route(engine_scene(m));(void)ctx;
    if(p)*out=*p;else memset(out,0,sizeof(*out));
}
static float scene_elevation(void *ctx,const maneuver_state_t *m) {
    const cr_scene_info_t *info=cr_scene_info(engine_scene(m));(void)ctx;
    return info?info->route_elevation:0;
}
static void scene_paint(void *ctx,const maneuver_state_t *m,float x,float y,float c,float s) {
    (void)ctx;cr_scene_paint(engine_scene(m),x,y,c,s);
}
static void prepare_engine_scene(cr_scene_t *scene,const maneuver_state_t *m) {
    if(!scene)return;
    cr_scene_input_t in;cr_scene_view_t view;
    memset(&in,0,sizeof(in));in.maneuver=*m;
    /* The canonical small composition paints the entire source in both stages. */
    view.compact=1;render_get_layout_matrix(view.projection);
    unsigned before=cr_scene_info(scene)->builds;
    if(!cr_scene_prepare(scene,&in,&view)) {
        fprintf(stderr,"scene: invalid input; ordinary maneuver fallback\n");return;
    }
    const cr_scene_info_t *info=cr_scene_info(scene);
    if(info->builds!=before)
        fprintf(stderr,"scene: kind=%d fallback=%d commands=%u\n",
            info->kind,info->fallback,info->command_count);
}
static void prepare_engine_scenes(void) {
    prepare_engine_scene(g_engine.current_scene,&g_engine.current);
    if(g_engine.has_next)prepare_engine_scene(g_engine.next_scene,&g_engine.next);
}

/* eglSwapBuffers on the old Adreno/WFD stack has no timeout. A wedged process
 * still has a live PID, so the external supervisor cannot distinguish it from
 * a healthy always-on renderer. This watchdog never calls EGL or touches the
 * render state; it only observes loop progress and exits this isolated process
 * before a stuck graphics client can linger indefinitely. */
/* A breadcrumb is only read on timeout.  No per-frame logging or graphics
 * calls from the watchdog: a blocked EGL/Screen client may hold libc locks. */
typedef enum {
    WATCH_STARTUP, WATCH_POLL, WATCH_COMMANDS, WATCH_ENGINE, WATCH_LANE_UPDATE,
    WATCH_LANE_FRAMING, WATCH_PREPARE, WATCH_BEGIN_FRAME,
    WATCH_SCENE_DRAW, WATCH_LANE_DRAW, WATCH_END_FRAME,
    WATCH_SCREENSHOT, WATCH_SWAP, WATCH_WINDOW_PROBE,
    WATCH_HEARTBEAT, WATCH_SLEEP, WATCH_IDLE
} renderer_watch_stage_t;
static unsigned long g_loop_progress;
static int g_watch_stage = WATCH_STARTUP;
static volatile int g_renderer_running = 1;
static volatile int g_render_loop_started;

static void watch_stage(renderer_watch_stage_t stage) {
    __sync_lock_test_and_set(&g_watch_stage, (int)stage);
}

static const char *watch_stage_name(int stage) {
    switch (stage) {
    case WATCH_STARTUP: return "startup";
    case WATCH_POLL: return "poll";
    case WATCH_COMMANDS: return "commands";
    case WATCH_ENGINE: return "engine";
    case WATCH_LANE_UPDATE: return "lane-update";
    case WATCH_LANE_FRAMING: return "lane-framing";
    case WATCH_PREPARE: return "prepare";
    case WATCH_BEGIN_FRAME: return "begin-frame";
    case WATCH_SCENE_DRAW: return "scene-draw";
    case WATCH_LANE_DRAW: return "lane-draw";
    case WATCH_END_FRAME: return "end-frame";
    case WATCH_SCREENSHOT: return "screenshot";
    case WATCH_SWAP: return "egl-swap";
    case WATCH_WINDOW_PROBE: return "window-probe";
    case WATCH_HEARTBEAT: return "heartbeat";
    case WATCH_SLEEP: return "sleep";
    case WATCH_IDLE: return "idle";
    default: return "unknown";
    }
}

static void *renderer_watchdog_main(void *unused) {
    unsigned long last = __sync_fetch_and_add(&g_loop_progress, 0);
    int stalledSeconds = 0;
    (void)unused;
    while (g_renderer_running) {
        struct timespec pause = { 1, 0 };
        while (nanosleep(&pause, &pause) != 0 && errno == EINTR) { }
        if (!g_renderer_running) break;
        unsigned long progress = __sync_fetch_and_add(&g_loop_progress, 0);
        if (progress != last) {
            last = progress;
            stalledSeconds = 0;
        } else if (++stalledSeconds >= (g_render_loop_started ? 5 : 15)) {
            static const char msg[] = "maneuver_render: progress watchdog TIMEOUT phase=";
            static const char newline[] = "\n";
            const char *phase = watch_stage_name(__sync_fetch_and_add(&g_watch_stage, 0));
            (void)write(STDERR_FILENO, msg, sizeof(msg) - 1);
            (void)write(STDERR_FILENO, phase, strlen(phase));
            (void)write(STDERR_FILENO, newline, sizeof(newline) - 1);
            _exit(89);
        }
    }
    return NULL;
}

/* Presentation alpha.  CLEAR keeps a deterministic FOLLOW_STREET frame prepared
 * underneath the compositor at alpha 0; the next real presentation fades it in. */
static float g_fade_alpha = 1.0f;
static int   g_fade_active = 0;
static int g_anim_prev_rendered;
#define FADE_SPEED 0.125f  /* per-frame step (~0.27s / 8 frames at 30fps) */
static int   g_cleared = 1;

/* Progress is rendered directly on the route arrow. */
static cr_arrow_progress_t g_arrow;
static int g_pending_progress_level=16,g_pending_progress_state=CR_PROGRESS_OFF;
static int   g_persp_deferred = 0;       /* pending perspective change after push */
static int   g_persp_deferred_value = 1; /* 0=2D, 1=3D */
/* Apply a new maneuver to the engine */
static void engine_apply_maneuver(const maneuver_state_t *state,double now) {
    if (state->icon == ICON_NONE || g_engine.current.icon == ICON_NONE) {
        g_engine.current = *state;

        g_engine.has_current = 1;
        g_engine.has_next = g_engine.has_pending = 0;
        g_engine.phase = ENGINE_IDLE;
        cr_progress_reset(&g_arrow);
        maneuver_set_slide(1.0f);
        render_invalidate_masks();
        g_engine.dirty = 1;
        return;
    }
    if (g_engine.phase == ENGINE_PUSHING || g_engine.phase == ENGINE_SLIDING_IN) {
        /* Mid-transition: queue into pending slot, don't disturb current animation */
        g_engine.pending = *state;

        g_engine.has_pending = 1;
        g_pending_progress_level=16;g_pending_progress_state=CR_PROGRESS_OFF;
        fprintf(stderr, "engine: queued icon=%d (phase=%d)\n",
                state->icon, g_engine.phase);
        return;
    }

    /* IDLE: store as next, start push */
    g_engine.next = *state;

    g_engine.has_next = 1;
    cr_progress_begin_handoff(&g_arrow,now);
    maneuver_start_push();
    g_engine.phase = ENGINE_PUSHING;
    render_invalidate_masks();
    g_engine.dirty = 1;
    fprintf(stderr, "engine: new maneuver icon=%d\n", state->icon);
}

/* A roads-only update belongs to the latest accepted command. During a push
 * that is next (or pending), never the outgoing current maneuver. */
static void engine_refresh_maneuver(const maneuver_state_t *state,double now) {
    if (state->icon == ICON_NONE || g_engine.current.icon == ICON_NONE) {
        engine_apply_maneuver(state,now);
        return;
    }
    if (g_engine.has_pending) {g_engine.pending = *state;}
    else {
        if (g_engine.has_next) {g_engine.next = *state;}
        else {g_engine.current = *state;}
    }
    render_invalidate_masks();
    g_engine.dirty = 1;
}

/* TCP progress belongs to the latest accepted maneuver. A third queued
 * maneuver must not overwrite the target of the route currently entering. */
static void engine_set_progress(int level,int state,double now) {
    if(g_engine.has_pending) {
        g_pending_progress_level=level;g_pending_progress_state=state;
    } else cr_progress_set(&g_arrow,level,state,now);
    g_engine.dirty=1;
}

static void engine_promote_pending(double now) {
    g_engine.next=g_engine.pending;
    g_engine.has_next=1;g_engine.has_pending=0;
    cr_progress_begin_handoff(&g_arrow,now);
    cr_progress_set(&g_arrow,g_pending_progress_level,g_pending_progress_state,now);
    maneuver_start_push();g_engine.phase=ENGINE_PUSHING;
    render_invalidate_masks();g_engine.dirty=1;
    fprintf(stderr,"engine: promoting queued icon=%d\n",g_engine.next.icon);
}

static void engine_apply_presentation(void) {
    if (g_persp_deferred) {
        render_set_perspective(g_persp_deferred_value);
        g_persp_deferred = 0;
        g_engine.dirty = 1;
    }
}

/* Advance engine state machine */
/* Blank the popup without leaving the renderer in an undefined/no-current state.
 * FOLLOW_STREET is always prepared underneath at slide=1, but alpha=0 and the
 * cleared latch prevent it from earning FRAME_READY.  The next CMD_MANEUVER
 * fades the deterministic scene back in. */
static void clear_maneuver(void) {
    cr_lane_clear(&g_lane_decoder, &g_lane_guidance);
    cr_lane_panel_clear(g_lane_panel);
    render_reset_content_offset();
    memset(&g_engine.current, 0, sizeof(g_engine.current));
    g_engine.current.icon = ICON_APPROACH;
    g_engine.has_current = 1;
    g_engine.has_next    = 0;
    g_engine.has_pending = 0;
    g_engine.phase       = ENGINE_IDLE;
    maneuver_set_slide(1.0f);
    render_invalidate_masks();
    cr_progress_reset(&g_arrow);
    g_persp_deferred = 0;
    g_fade_active    = 0;
    g_fade_alpha     = 0.0f;
    render_set_global_alpha(0.0f);
    g_cleared        = 1;
    g_engine.dirty   = 1;
}

static void engine_tick(double now) {
    switch (g_engine.phase) {
    case ENGINE_PUSHING:
        if (!maneuver_is_pushing()) {
            /* Push complete -- commit next as current */
            if (g_engine.has_next) {
                g_engine.current = g_engine.next;
                cr_scene_t *old=g_engine.current_scene;
                g_engine.current_scene=g_engine.next_scene;g_engine.next_scene=old;
                g_engine.has_next = 0;
            }
            maneuver_commit_pushed_state(&g_engine.current);
            cr_progress_finish_handoff(&g_arrow,now);
            render_invalidate_masks();
            g_engine.dirty = 1;

            if (g_engine.has_pending) {
                /* Promote pending -> next, immediately start new push */
                engine_promote_pending(now);
            } else {
                g_engine.phase = ENGINE_SLIDING_IN;
                /* Crossfade during push already brought next roads to full alpha,
                 * no post-push fade needed. */
            }
        }
        break;
    case ENGINE_SLIDING_IN:
        if (!maneuver_is_animating()) {
            if (g_engine.has_pending) {
                /* Promote pending -> next, start push */
                engine_promote_pending(now);
            } else {
                g_engine.phase = ENGINE_IDLE;
            }
        }
        break;
    case ENGINE_IDLE:
        break;
    }
    if (g_engine.phase == ENGINE_IDLE) engine_apply_presentation();
}

/* ================================================================
 * Screenshot -- save framebuffer as PPM
 * ================================================================ */

static int g_snap_counter = 0;

static void save_screenshot(int fb_w, int fb_h, const char *label) {
    unsigned char *pixels = (unsigned char *)malloc(fb_w * fb_h * 4);
    if (!pixels) return;
    glReadPixels(0, 0, fb_w, fb_h, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

    char path[256];
    snprintf(path, sizeof(path), "snap_%03d_%s.ppm", g_snap_counter++, label);

    FILE *f = fopen(path, "wb");
    if (!f) { free(pixels); return; }
    fprintf(f, "P6\n%d %d\n255\n", fb_w, fb_h);
    int y, x;
    for (y = fb_h - 1; y >= 0; y--) {
        for (x = 0; x < fb_w; x++) {
            unsigned char *p = pixels + (y * fb_w + x) * 4;
            fwrite(p, 1, 3, f);
        }
    }
    fclose(f);
    free(pixels);
    fprintf(stderr, "engine: saved %s\n", path);
}

/* ================================================================
 * Main
 * ================================================================ */

int main(int argc, char **argv) {
    int port=CR_TCP_PORT, arg;
    for(arg=1;arg<argc;arg++) {
        if(!strncmp(argv[arg],"--port=",7)) {
            char *end; long parsed=strtol(argv[arg]+7,&end,10);
            if(end==argv[arg]+7 || *end || parsed<1 || parsed>65535) {
                fprintf(stderr,"Invalid renderer port\n"); return 2;
            }
            port=(int)parsed;
        } else {
            fprintf(stderr,"Usage: %s [--port=1..65535]\n",argv[0]);
            return 2;
        }
    }
#ifdef SIGPIPE
    signal(SIGPIPE, SIG_IGN);
#endif

    fprintf(stderr, "%s maneuver_render: starting %dx%d\n", log_stamp(), WINDOW_W, WINDOW_H);

    /* Cover platform_init as well as runtime/teardown. A process blocked in
     * eglInitialize has a valid PID but no useful output; the supervisor's
     * PID-only maneuver health probe cannot detect it. Cold graphics gets a
     * generous 15 s, then the isolated process exits for a backed-off restart. */
    {
        pthread_t watchdog;
        if (pthread_create(&watchdog, NULL, renderer_watchdog_main, NULL) == 0)
            pthread_detach(watchdog);
        else
            fprintf(stderr, "maneuver_render: progress watchdog create failed\n");
    }

#ifdef PLATFORM_QNX
    /* Raise our scheduling priority to match native HMI graphics workers
     * (typically 13-18 on QNX 6.5).  Default user priority 10 lets busy
     * iAP2 / dio_manager / Java HMI threads preempt us for ~14-16 vsync
     * cycles at a time, which manifests as 245 ms render stalls every
     * ~530 ms.  At priority 15 we sit in the same league as the cluster
     * compositor's own workers and are no longer starved.
     *
     * Failure (EPERM if not root) is non-fatal — we'll still run, just
     * with stalls. */
    if (setprio(0, 15) < 0) {
        fprintf(stderr, "maneuver_render: setprio(15) failed: %s — running at default\n",
                strerror(errno));
    } else {
        fprintf(stderr, "maneuver_render: priority raised to 15\n");
    }
#endif

    /* Initialize TCP server FIRST so Java can connect while display inits.
     * If server fails, nothing works — exit immediately. */
    if (cr_server_init(port) < 0) {
        fprintf(stderr, "maneuver_render: server init failed\n");
        return 1;
    }

    if (platform_init(WINDOW_W, WINDOW_H) < 0) {
        fprintf(stderr, "maneuver_render: platform init failed\n");
        cr_server_shutdown();
        return 1;
    }

    int fb_w, fb_h;
    platform_get_framebuffer_size(&fb_w, &fb_h);
    fprintf(stderr, "maneuver_render: framebuffer %dx%d\n", fb_w, fb_h);

    maneuver_scene_provider_t provider={0};
    provider.handles=scene_handles;provider.build_route=scene_route;
    provider.paint_masks=scene_paint;provider.elevation=scene_elevation;
    cr_scene_configure_provider(&provider);maneuver_set_scene_provider(&provider);
    if (render_init(fb_w, fb_h) < 0) {
        fprintf(stderr, "maneuver_render: render init failed\n");
        platform_shutdown();
        cr_server_shutdown();
        return 1;
    }

    /* Runtime has one colocated asset.  Keep one relative development/deploy
     * lookup and one explicit HU production fallback — no argv/CWD guessing. */
    if (render_load_flag_atlas(FLAG_ATLAS_LOCAL_PATH, 128, 128, 14) != 0)
        render_load_flag_atlas(FLAG_ATLAS_PRODUCTION_PATH, 128, 128, 14);

#ifdef CR_DEBUG_GRID
    render_set_debug_grid(1);
#endif
    /* Engine always has a deterministic hidden FOLLOW_STREET frame. */
    memset(&g_engine, 0, sizeof(g_engine));
    g_engine.current_scene=cr_scene_create();g_engine.next_scene=cr_scene_create();
    g_lane_panel=cr_lane_panel_create();
    if(!g_lane_panel)fprintf(stderr,"lanes: panel allocation failed\n");
    if(!g_engine.current_scene || !g_engine.next_scene) {
        fprintf(stderr,"scene: allocation failed; retaining ordinary renderer\n");
        cr_scene_destroy(g_engine.current_scene);cr_scene_destroy(g_engine.next_scene);
        g_engine.current_scene=g_engine.next_scene=NULL; maneuver_set_scene_provider(NULL);
    }
    { extern float g_3d_offset_adjust;g_3d_offset_adjust=-.16f; }
    clear_maneuver();

    fprintf(stderr, "maneuver_render: ready, waiting for commands on :%d\n", port);
    cr_server_mark_ready();

    int dirty = 1;
    int running = 1;
    int announced_first_frame = 0;
    g_render_loop_started = 1;

#ifdef CR_DIAG_FRAME_LOG
    /* Per-second + per-frame render-loop instrumentation — only compiled
     * in for diagnostic builds (`-DCR_DIAG_FRAME_LOG`).  Production never
     * pays the fprintf cost, which on QNX 6.5 with /tmp on slow flash can
     * add noticeable jitter at 60 lines/sec. */
    static struct timespec stat_last = {0, 0};
    static int stat_frames_rendered = 0;
    static int stat_loop_iters = 0;
    static long stat_render_total_us = 0;
    static long stat_render_max_us = 0;
    static int stat_cmds_received = 0;
#endif

    while (running && !platform_should_close()) {
        struct timespec t_start;
        watch_stage(WATCH_POLL);
        __sync_fetch_and_add(&g_loop_progress, 1);
        clock_gettime(CLOCK_MONOTONIC, &t_start);
#ifdef CR_DIAG_FRAME_LOG
        stat_loop_iters++;
#endif

        platform_poll();
#ifdef CR_DIAG_FRAME_LOG
        struct timespec t_after_pp;
        clock_gettime(CLOCK_MONOTONIC, &t_after_pp);
#endif

        watch_stage(WATCH_POLL);
        cr_server_poll();
#ifdef CR_DIAG_FRAME_LOG
        struct timespec t_after_sp;
        clock_gettime(CLOCK_MONOTONIC, &t_after_sp);
#endif

        /* Java closed the bus socket (route/CarPlay off).  We are an always-on framework
         * service — do NOT exit.  Blank the popup so no stale frame lingers, clear the
         * peer-closed latch, and let cr_server_poll() reconnect when Java is back. */
        if (cr_server_peer_closed()) {
            fprintf(stderr, "engine: peer closed — clearing, staying alive for reconnect\n");
            clear_maneuver();
            announced_first_frame = 0;
            cr_server_clear_frame_ready();
            cr_server_clear_peer_closed();
        }

        /* Drain all pending commands, keep only the last CMD_MANEUVER */
        cr_cmd_t cmd;
        int got_maneuver = 0;
        maneuver_state_t pending_maneuver;
        uint8_t pending_flags = 0;
        uint8_t pending_perspective = 1;
        int got_progress=0, progress_level=0, progress_mode=0;
        int progress_state=CR_PROGRESS_OFF;
        double progress_now=(double)t_start.tv_sec+t_start.tv_nsec*1e-9;
        /* Animations advance by elapsed time, not by frames: at 20 fps a per-frame step
         * made every slide 1.5x slower.  Iterations that render are paced by the swap, so
         * the gap since the previous rendering iteration is the frame time.  After idle
         * the first frame is one step; a stall longer than 4 frames slows, never jumps. */
        {
            static double anim_prev;
            float step = 1.0f;
            if (g_anim_prev_rendered) {
                step = (float)((progress_now - anim_prev) * TARGET_FPS);
                if (step < 0.0f) step = 0.0f;
                if (step > 4.0f) step = 4.0f;
            }
            anim_prev = progress_now;
            render_set_frame_step(step);
        }
        int got_screenshot = 0;
        char screenshot_label[17];

        watch_stage(WATCH_COMMANDS);
        while (cr_server_read_cmd(&cmd)) {
#ifdef CR_DIAG_FRAME_LOG
            stat_cmds_received++;
#endif
            if (cmd.cmd == CMD_LANES_BEGIN || cmd.cmd == CMD_LANES_LANE || cmd.cmd == CMD_LANES_COMMIT) {
                if (cr_lane_receive(&g_lane_decoder, &cmd, &g_lane_guidance)) {
                    g_engine.dirty=1;
                    fprintf(stderr, "lanes: event=%d showing=%d count=%d complete=%d\n",
                        (int)g_lane_guidance.event_index, g_lane_guidance.showing,
                        g_lane_guidance.count, g_lane_guidance.complete);
                }
                continue;
            }
            switch (cmd.cmd) {
            case CMD_MANEUVER: {
                cr_decode_maneuver(&cmd, &pending_maneuver);
                pending_flags = cr_merge_maneuver_flags(got_maneuver, pending_flags, cmd.flags);
                pending_perspective = cmd.payload[43];
                got_progress=1;
                progress_level=(cmd.flags & MAN_FLAG_PROGRESS) ? cmd.payload[44] : 0;
                progress_mode=(cmd.flags & MAN_FLAG_PROGRESS) ? cmd.payload[45] : 0;
                progress_state=cr_progress_decode(cmd.flags,cmd.payload[42],progress_mode);
                got_maneuver = 1;
                break;
            }
            case CMD_SCREENSHOT: {
                got_screenshot = 1;
                memcpy(screenshot_label, cmd.payload, 16);
                screenshot_label[16] = '\0';
                break;
            }
            case CMD_VISIBLE_AREA:
                if (render_set_visible_area((cmd.payload[0] << 8) | cmd.payload[1],
                        (cmd.payload[2] << 8) | cmd.payload[3],
                        (cmd.payload[4] << 8) | cmd.payload[5],
                        (cmd.payload[6] << 8) | cmd.payload[7]))
                    g_engine.dirty = 1;
                break;
            case CMD_PERSPECTIVE: {
                int persp = cmd.payload[0] ? 1 : 0;
                render_set_perspective(persp);
                fprintf(stderr, "engine: perspective=%s\n", persp ? "ON" : "OFF");
                g_engine.dirty = 1;
                break;
            }
            case CMD_DEBUG:
                if (cmd.payload[0] == 2) {
                    static int grid_on = 0;
                    grid_on = !grid_on;
                    render_set_debug_grid(grid_on);
                    fprintf(stderr, "engine: grid=%s\n", grid_on ? "ON" : "OFF");
                } else if (cmd.payload[0] == 3) {
                    /* 3D offset adjust up */
                    extern float g_3d_offset_adjust;
                    g_3d_offset_adjust -= 0.02f;
                    fprintf(stderr, "engine: 3d_offset=%.3f\n", g_3d_offset_adjust);
                } else if (cmd.payload[0] == 4) {
                    /* 3D offset adjust down */
                    extern float g_3d_offset_adjust;
                    g_3d_offset_adjust += 0.02f;
                    fprintf(stderr, "engine: 3d_offset=%.3f\n", g_3d_offset_adjust);
                } else {
                    maneuver_toggle_debug();
                    fprintf(stderr, "engine: debug=%s\n", maneuver_is_debug() ? "ON" : "OFF");
                }
                g_engine.dirty = 1;
                break;
            case CMD_PROGRESS:
                got_progress=1;
                progress_level=cmd.payload[0]; progress_mode=cmd.payload[1];
                progress_state=cr_progress_decode(cmd.flags,cmd.payload[2],progress_mode);
                break;
            case CMD_CLEAR:
                /* CLEAR wins over an earlier MANEUVER drained in this same loop. */
                got_maneuver = 0;
                got_progress=0;
                clear_maneuver();
                announced_first_frame = 0;
                cr_server_clear_frame_ready();
                /* This barrier is sent before any later command in the same TCP
                 * drain can draw and earn a new FRAME_READY.  Java therefore can
                 * reject an old in-flight FRAME_READY without a timing guess. */
                cr_server_mark_frame_cleared();
                fprintf(stderr, "engine: clear (blank)\n");
                break;
            case CMD_SHUTDOWN:
                fprintf(stderr, "engine: shutdown command received\n");
                running = 0;
                break;
            }
        }

        /* Apply the latest maneuver (draining to latest) */
        watch_stage(WATCH_ENGINE);
        if (got_maneuver) {
            int reveal_from_clear = g_cleared;
            if (reveal_from_clear) cr_progress_reset(&g_arrow);
            if (g_cleared) {
                g_cleared = 0;
                g_fade_alpha = 0.0f;
                g_fade_active = 1;
                render_set_global_alpha(0.0f);
            }
            if (pending_flags & MAN_FLAG_SET_PERSP) {
                g_persp_deferred = 1;
                g_persp_deferred_value = pending_perspective ? 1 : 0;
                fprintf(stderr, "engine: deferred perspective=%s\n",
                        g_persp_deferred_value ? "3D" : "2D");
            }
            if (reveal_from_clear) {
                /* The hidden FOLLOW_STREET is only a deterministic backing
                 * state.  Replace it directly: the global alpha is the sole
                 * CLEAR->visible transition, with no duplicate route push. */
                g_engine.current = pending_maneuver;
                g_engine.has_current = 1;
                g_engine.has_next = 0;
                g_engine.has_pending = 0;
                g_engine.phase = ENGINE_IDLE;
                maneuver_set_slide(1.0f);
                render_invalidate_masks();
                g_engine.dirty = 1;
                /* This direct reveal deliberately bypasses ENGINE_SLIDING_IN,
                 * whose settle edge normally consumes the deferred presentation
                 * fields.  Apply them here so the first real frame after CLEAR
                 * carries the requested perspective as well. */
                engine_apply_presentation();
                fprintf(stderr, "engine: reveal from clear icon=%d\n",
                        pending_maneuver.icon);
            } else if (pending_flags & MAN_FLAG_REFRESH) {
                engine_refresh_maneuver(&pending_maneuver,progress_now);
            } else {
                engine_apply_maneuver(&pending_maneuver,progress_now);
            }
        }

        if(got_progress) {
            engine_set_progress(progress_level,progress_state,progress_now);
            /* TCP order wins, including PROGRESS after MANEUVER in one drain.
             * Never let the maneuver's deferred snapshot rewind a newer tick. */
            g_engine.dirty=1;
        }
        if(cr_progress_tick(&g_arrow,progress_now)) g_engine.dirty=1;

        /* Tick engine state machine */
        prepare_engine_scenes();
        engine_tick(progress_now);
        prepare_engine_scenes();

        /* Update framebuffer size (HiDPI) */
        int new_w, new_h;
        platform_get_framebuffer_size(&new_w, &new_h);
        if (new_w != fb_w || new_h != fb_h) {
            fb_w = new_w;
            fb_h = new_h;
            render_set_viewport(fb_w, fb_h);
            dirty = 1;
        }

        /* Fade-in animation */
        if (g_fade_active) {
            g_fade_alpha += FADE_SPEED * render_frame_step();
            if (g_fade_alpha >= 1.0f) {
                g_fade_alpha = 1.0f;
                g_fade_active = 0;
            }
            render_set_global_alpha(g_fade_alpha);
            g_engine.dirty = 1;
        }

        /* Render if needed */
        cr_rect_t panel_target;
        render_get_visible_area(NULL,&panel_target);
        watch_stage(WATCH_LANE_UPDATE);
        if(cr_lane_panel_update(g_lane_panel,&g_lane_guidance,panel_target.w,progress_now))
            g_engine.dirty=1;
        float content_frame[3];
        watch_stage(WATCH_LANE_FRAMING);
        cr_lane_panel_framing(g_lane_panel,g_engine.current_scene,
                             g_engine.has_next?g_engine.next_scene:NULL,content_frame);
        render_set_content_framing(content_frame[0],content_frame[1],content_frame[2]);
        watch_stage(WATCH_IDLE);
        if (g_engine.dirty || render_is_animating() || maneuver_needs_redraw() || got_screenshot
            || cr_lane_panel_animating(g_lane_panel,progress_now)
#ifdef CR_DEBUG_GRID
            || 1  /* always render when grid is compiled in */
#endif
           )
            dirty = 1;
        g_engine.dirty = 0;

        int rendered_this_frame = 0;
        if (dirty) {
            rendered_this_frame = 1;
#ifdef CR_DIAG_FRAME_LOG
            struct timespec rt_start;
            clock_gettime(CLOCK_MONOTONIC, &rt_start);
#endif

            maneuver_state_t *next_ptr = g_engine.has_next ? &g_engine.next : NULL;
            watch_stage(WATCH_PREPARE);
            maneuver_prepare_frame(&g_engine.current, next_ptr);
#ifdef CR_DIAG_FRAME_LOG
            struct timespec t_after_prep;
            clock_gettime(CLOCK_MONOTONIC, &t_after_prep);
#endif

            render_set_route_progress(g_arrow.fill,g_arrow.path_weight,g_arrow.glow);
            watch_stage(WATCH_BEGIN_FRAME);
            render_begin_frame();
            watch_stage(WATCH_SCENE_DRAW);
            maneuver_draw(&g_engine.current, next_ptr);
            cr_rect_t panel_visible;
            render_get_visible_area(&panel_visible,NULL);
            watch_stage(WATCH_LANE_DRAW);
            cr_lane_panel_draw(g_lane_panel,panel_visible,progress_now);
            watch_stage(WATCH_IDLE);
            render_debug_grid();
            watch_stage(WATCH_END_FRAME);
            render_end_frame();
            watch_stage(WATCH_IDLE);
#ifdef CR_DIAG_FRAME_LOG
            struct timespec t_after_draw;
            clock_gettime(CLOCK_MONOTONIC, &t_after_draw);
#endif

            watch_stage(WATCH_SCREENSHOT);
            if (got_screenshot)
                save_screenshot(fb_w, fb_h, screenshot_label);

            watch_stage(WATCH_SWAP);
            int swap_ok = platform_swap();
            watch_stage(WATCH_IDLE);
            if (!swap_ok) {
                /* platform_swap recreated the QNX surface.  Repaint it on the
                 * next iteration even when the engine is otherwise idle.  Also
                 * classify this iteration as idle so the generic pacing below
                 * sleeps; a dead EGL surface does not provide vsync pacing. */
                g_engine.dirty = 1;
                rendered_this_frame = 0;
            }
            /* A transparent post-CLEAR FOLLOW_STREET frame is deliberately not
             * presentation-ready.  Only a later real command may re-arm Java's
             * ctx80 gate. */
            if (swap_ok && !announced_first_frame && g_engine.has_current && !g_cleared) {
                announced_first_frame = 1;
                cr_server_mark_frame_ready();
                fprintf(stderr, "engine: first frame ready\n");
            }

            /* Voluntarily yield to the compositor right after queuing a
             * new buffer.  Without this, the kernel eventually forces us
             * off-CPU when the buffer pool fills, but in long bursts
             * (~14-16 vsync cycles).  A cooperative yield keeps the
             * compositor's worker scheduled in time for the next frame
             * — much smoother on QNX displayable composition. */
            sched_yield();

#ifdef CR_DIAG_FRAME_LOG
            struct timespec rt_end;
            clock_gettime(CLOCK_MONOTONIC, &rt_end);
            long rt_us = (rt_end.tv_sec - rt_start.tv_sec) * 1000000L
                       + (rt_end.tv_nsec - rt_start.tv_nsec) / 1000L;
            long prep_us = (t_after_prep.tv_sec - rt_start.tv_sec) * 1000000L
                         + (t_after_prep.tv_nsec - rt_start.tv_nsec) / 1000L;
            long draw_us = (t_after_draw.tv_sec - t_after_prep.tv_sec) * 1000000L
                         + (t_after_draw.tv_nsec - t_after_prep.tv_nsec) / 1000L;
            long swap_us = (rt_end.tv_sec - t_after_draw.tv_sec) * 1000000L
                         + (rt_end.tv_nsec - t_after_draw.tv_nsec) / 1000L;
            stat_frames_rendered++;
            stat_render_total_us += rt_us;
            if (rt_us > stat_render_max_us) stat_render_max_us = rt_us;

            static struct timespec last_frame_end = {0, 0};
            long delta_us = 0;
            if (last_frame_end.tv_sec > 0) {
                delta_us = (rt_start.tv_sec - last_frame_end.tv_sec) * 1000000L
                         + (rt_start.tv_nsec - last_frame_end.tv_nsec) / 1000L;
            }
            fprintf(stderr,
                    "frame: gap=%ldus render=%ldus prep=%ldus draw=%ldus swap=%ldus icon=%d\n",
                    delta_us, rt_us, prep_us, draw_us, swap_us,
                    g_engine.has_current ? g_engine.current.icon : -1);
            if (delta_us > 100000L) {
                long pp_us = (t_after_pp.tv_sec - last_frame_end.tv_sec) * 1000000L
                           + (t_after_pp.tv_nsec - last_frame_end.tv_nsec) / 1000L;
                long sp_us = (t_after_sp.tv_sec - last_frame_end.tv_sec) * 1000000L
                           + (t_after_sp.tv_nsec - last_frame_end.tv_nsec) / 1000L;
                long top_us = (t_start.tv_sec - last_frame_end.tv_sec) * 1000000L
                            + (t_start.tv_nsec - last_frame_end.tv_nsec) / 1000L;
                fprintf(stderr,
                        "STALL breakdown: post_swap_to_top=%ldus top_to_pp=%ldus pp_to_sp=%ldus sp_to_render=%ldus\n",
                        top_us,
                        pp_us - top_us,
                        sp_us - pp_us,
                        delta_us - sp_us);
            }
            last_frame_end = rt_end;
#endif /* CR_DIAG_FRAME_LOG */

            dirty = render_is_animating() || maneuver_needs_redraw() || g_fade_active
                 || cr_lane_panel_animating(g_lane_panel,progress_now)
                 || g_arrow.active || g_arrow.tint_active;
        }

        /* dmdt focus watchdog: spawn one-shot detached thread to run
         * the popen("dmdt gs") + optional sc on a worker.  The render
         * loop never blocks on the ~150 ms popen cost.
         * Trigger only when idle + not animating to avoid any chance of
         * the detached thread perturbing a frame in flight. */
        {
            static struct timespec focus_last = {0, 0};
            if (g_engine.phase == ENGINE_IDLE
                    && !render_is_animating()
                    && timespec_elapsed_at_least(&t_start, &focus_last, 30, 0)) {
                focus_last = t_start;
                platform_ensure_focus();
            }
        }

        /* Window health-check + auto-recreate every 5 s.
         *
         * Two failure modes covered (see platform_qnx.c::platform_check_and_recover_window
         * for full RE-derived rationale):
         *   1. Our screen_window struct invalidated cross-process — the
         *      probe screen_get_window_property_iv returns ENOENT/EBADF/EINVAL.
         *   2. displaymanager disowned our managed window (m_surfaceSources[98]
         *      no longer holds it, e.g. the context switched away) — our
         *      SCREEN_PROPERTY_MANAGER_STRING no longer matches "All your base
         *      are belong to us!".  With our own id 98 there is no stock
         *      collision, so this is rare.
         *
         * On either signal, cluster_surface recreates the managed window (id 98,
         * 100 ms backoff inside) and platform_recreate_window re-binds EGL. */
        {
            static struct timespec health_last = {0, 0};
            if (timespec_elapsed_at_least(&t_start, &health_last, 5, 0)) {
                health_last = t_start;
                watch_stage(WATCH_WINDOW_PROBE);
                platform_check_and_recover_window();
                watch_stage(WATCH_IDLE);
            }
        }

        /* Heartbeat — single-thread design: send EVT_HEARTBEAT to Java
         * once per second, dispatched right here on the main loop tick.
         * Java's RendererServer SO_TIMEOUT=5s; one beat per second
         * keeps it happy with ample margin.
         *
         * NOTE: an earlier iteration moved this to a dedicated pthread
         * (commit c1aa2fd) on the theory that send() over QNX 6.5
         * loopback could occasionally take a few ms and disturb frame
         * pacing.  Diagnostics on hardware showed the periodic ~245 ms
         * stalls persisted even with the heartbeat thread fully
         * disabled, so the thread wasn't actually buying us anything.
         * Rolling it back restores single-thread renderer.  send() is
         * non-blocking; if the kernel ever returns a multi-ms latency
         * here, the next 1 s tick still has ample margin before Java's
         * 5 s SO_TIMEOUT.  Worst case observed: ~5 ms per send. */
        {
            static struct timespec hb_last = {0, 0};
            if (timespec_elapsed_at_least(&t_start, &hb_last, 1, 0)) {
                hb_last = t_start;
                watch_stage(WATCH_HEARTBEAT);
                cr_server_send_heartbeat();
                watch_stage(WATCH_IDLE);
            }
        }

        /* Adaptive IDLE pacing (proven approach from the c_render history, commit e26cce5).
         * ACTIVE output pacing rides eglSwapBuffers (QNX: eglSwapInterval(2); macOS below) — NEVER
         * nanosleep, because nanosleep on QNX 6.5 rounds up to the kernel timer tick and won't hold
         * a precise 30 Hz.  But when the loop produced NO frame this iteration (idle: no route / no
         * maneuver) there is no swap to pace it, so an always-on renderer would busy-spin 100% CPU.
         * Throttle idle polling: 30 Hz for the first second, then 10 Hz (1–5 s), then 3 Hz (>5 s);
         * any activity snaps back to 30 Hz.  Idle timing needs no precision → the QNX nanosleep
         * granularity is harmless here. */
        static int idle_frames = 0;
        g_anim_prev_rendered = rendered_this_frame;
        if (rendered_this_frame) {
            idle_frames = 0;
        } else {
            if (idle_frames < 10000) idle_frames++;
            long idle_ns = (idle_frames < TARGET_FPS)     ? FRAME_TIME_NS     /* <1 s: 30 Hz */
                         : (idle_frames < TARGET_FPS * 5) ? 100L * 1000000L  /* 1–5 s: 10 Hz */
                         :                                  333L * 1000000L; /* >5 s: 3 Hz  */
            struct timespec ts = { idle_ns / 1000000000L, idle_ns % 1000000000L };
            watch_stage(WATCH_SLEEP);
            nanosleep(&ts, NULL);
            watch_stage(WATCH_IDLE);
        }

#ifndef PLATFORM_QNX
        /* macOS/dev only: GLFW swap interval 0 gives no vsync throttle, so pace ACTIVE frames to
         * 30 FPS here. QNX requests interval 2 on each surface; MOST capture has its own clock. */
        if (rendered_this_frame) {
            struct timespec t_end;
            clock_gettime(CLOCK_MONOTONIC, &t_end);
            long elapsed_ns = (t_end.tv_sec - t_start.tv_sec) * 1000000000L
                            + (t_end.tv_nsec - t_start.tv_nsec);
            long sleep_ns = FRAME_TIME_NS - elapsed_ns;
            if (sleep_ns > 0) {
                struct timespec ts = { sleep_ns / 1000000000L, sleep_ns % 1000000000L };
                watch_stage(WATCH_SLEEP);
                nanosleep(&ts, NULL);
                watch_stage(WATCH_IDLE);
            }
        }
#else
        (void)t_start;
#endif

#ifdef CR_DIAG_FRAME_LOG
        /* 1 Hz render-loop stats.  Steady state should report
         * iters≈30 frames≈30 (ARRIVED flag keeps maneuver_needs_redraw
         * true).  Anything else means the loop is being throttled. */
        {
            struct timespec stat_now;
            clock_gettime(CLOCK_MONOTONIC, &stat_now);
            if (stat_last.tv_sec == 0) stat_last = stat_now;
            long el_ms = (stat_now.tv_sec - stat_last.tv_sec) * 1000L
                       + (stat_now.tv_nsec - stat_last.tv_nsec) / 1000000L;
            if (el_ms >= 1000) {
                long avg_us = stat_frames_rendered > 0
                            ? stat_render_total_us / stat_frames_rendered : 0;
                int icon = g_engine.has_current ? g_engine.current.icon : -1;
                fprintf(stderr,
                        "stats: iters=%d frames=%d cmds=%d render_us=%ld/%ld(avg/max) icon=%d redraw=%d anim=%d\n",
                        stat_loop_iters, stat_frames_rendered, stat_cmds_received,
                        avg_us, stat_render_max_us, icon,
                        maneuver_needs_redraw() ? 1 : 0,
                        render_is_animating() ? 1 : 0);
                stat_loop_iters = 0;
                stat_frames_rendered = 0;
                stat_cmds_received = 0;
                stat_render_total_us = 0;
                stat_render_max_us = 0;
                stat_last = stat_now;
            }
        }
#endif /* CR_DIAG_FRAME_LOG */
        watch_stage(WATCH_IDLE);
    }

    cr_server_shutdown();
    maneuver_set_scene_provider(NULL);
    cr_scene_destroy(g_engine.current_scene);cr_scene_destroy(g_engine.next_scene);
    cr_lane_panel_destroy(g_lane_panel);
    render_shutdown();
    platform_shutdown();
    g_renderer_running = 0;

    fprintf(stderr, "maneuver_render: exit\n");
    return 0;
}
