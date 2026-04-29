/*
 * CarPlay Cluster Renderer -- main entry point.
 *
 * Command-driven rendering engine with TCP server.
 * Receives maneuver commands, handles all animation/transitions internally.
 *
 * macOS: GLFW window for development.
 * QNX:   displayable 200 for LVDS video output.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <libgen.h>

#include "platform.h"

#define TARGET_FPS     30
#define FRAME_TIME_NS  (1000000000L / TARGET_FPS)
#include "gl_compat.h"
#include "render.h"
#include "protocol.h"
#include "maneuver.h"
#include "server.h"

#define WINDOW_W CR_DEFAULT_WIDTH
#define WINDOW_H CR_DEFAULT_HEIGHT

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
    int              has_current;   /* 0 on cold start */
    int              has_next;
    int              has_pending;   /* queued during PUSHING */
    engine_phase_t   phase;
    int              dirty;
} cr_engine_t;

static cr_engine_t g_engine;

/* Fade-in on cold start */
static float g_fade_alpha = 1.0f;
static int   g_fade_active = 0;
#define FADE_SPEED 0.04f  /* per-frame step (~0.8s at 30fps) */

/* Bargraph overlay */
static int   g_bargraph_on = 0;     /* 0=off, 1=on, 2=blink */
static int   g_bargraph_level = 0;  /* 0..16 */
static int   g_persp_deferred = 0;       /* pending perspective change after push */
static int   g_persp_deferred_value = 1; /* 0=2D, 1=3D */
static float g_bargraph_alpha = 0.0f;  /* fade in/out */
#define BARGRAPH_FADE_SPEED 0.06f      /* per-frame (~0.55s at 30fps) */
static int   g_bargraph_blink_vis = 1;  /* blink phase: 1=show, 0=hide */
static int   g_bargraph_blink_timer = 0;
#define BARGRAPH_BLINK_FRAMES (int)(0.6f * TARGET_FPS)  /* 600ms */
/* Deferred bargraph from maneuver payload -- applied when transition settles */
static int   g_bargraph_deferred = 0;
static int   g_bargraph_deferred_level = 0;
static int   g_bargraph_deferred_mode = 0;

/* Decode CMD_MANEUVER payload into maneuver_state_t */
static void decode_maneuver(const cr_cmd_t *cmd, maneuver_state_t *out) {
    const uint8_t *p = cmd->payload;
    int i, count;

    memset(out, 0, sizeof(*out));
    out->icon         = CR_MAN_ICON(p);
    out->direction    = CR_MAN_DIRECTION(p);
    out->exit_angle   = CR_MAN_EXIT_ANGLE(p);
    out->driving_side = CR_MAN_DRIVING_SIDE(p);

    count = CR_MAN_JUNC_COUNT(p);
    if (count > MAX_JUNCTION_ANGLES) count = MAX_JUNCTION_ANGLES;
    out->junction_angle_count = count;
    for (i = 0; i < count; i++)
        out->junction_angles[i] = CR_MAN_JUNC_ANGLE(p, i);
}

/* Apply a new maneuver to the engine */
static void engine_apply_maneuver(const maneuver_state_t *state) {
    if (!g_engine.has_current) {
        /* Cold start: set as current directly, trigger slide-in */
        g_engine.current = *state;
        g_engine.has_current = 1;
        g_engine.has_next = 0;
        g_engine.phase = ENGINE_SLIDING_IN;
        maneuver_set_slide(0.0f);
        maneuver_start_anim();
        render_invalidate_masks();
        g_engine.dirty = 1;
        g_fade_alpha = 0.0f;
        g_fade_active = 1;
        render_set_global_alpha(0.0f);
        fprintf(stderr, "engine: cold start icon=%d\n", state->icon);
        return;
    }

    if (g_engine.phase == ENGINE_PUSHING || g_engine.phase == ENGINE_SLIDING_IN) {
        /* Mid-transition: queue into pending slot, don't disturb current animation */
        g_engine.pending = *state;
        g_engine.has_pending = 1;
        fprintf(stderr, "engine: queued icon=%d (phase=%d)\n",
                state->icon, g_engine.phase);
        return;
    }

    /* IDLE: store as next, start push */
    g_engine.next = *state;
    g_engine.has_next = 1;
    g_bargraph_on = 0;
    maneuver_start_push();
    g_engine.phase = ENGINE_PUSHING;
    render_invalidate_masks();
    g_engine.dirty = 1;
    fprintf(stderr, "engine: new maneuver icon=%d\n", state->icon);
}

/* Advance engine state machine */
static void engine_tick(void) {
    switch (g_engine.phase) {
    case ENGINE_PUSHING:
        if (!maneuver_is_pushing()) {
            /* Push complete -- commit next as current */
            if (g_engine.has_next) {
                g_engine.current = g_engine.next;
                g_engine.has_next = 0;
            }
            maneuver_commit_pushed_state(&g_engine.current);
            render_invalidate_masks();
            g_engine.dirty = 1;

            if (g_engine.has_pending) {
                /* Promote pending -> next, immediately start new push */
                g_engine.next = g_engine.pending;
                g_engine.has_next = 1;
                g_engine.has_pending = 0;
                maneuver_start_push();
                g_engine.phase = ENGINE_PUSHING;
                fprintf(stderr, "engine: promoting queued icon=%d\n",
                        g_engine.next.icon);
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
                g_engine.next = g_engine.pending;
                g_engine.has_next = 1;
                g_engine.has_pending = 0;
                maneuver_start_push();
                g_engine.phase = ENGINE_PUSHING;
                render_invalidate_masks();
                g_engine.dirty = 1;
                fprintf(stderr, "engine: promoting queued icon=%d\n",
                        g_engine.next.icon);
            } else {
                g_engine.phase = ENGINE_IDLE;
                /* Apply deferred perspective when settled */
                if (g_persp_deferred) {
                    render_set_perspective(g_persp_deferred_value);
                    fprintf(stderr, "engine: applied deferred perspective=%s\n",
                            g_persp_deferred_value ? "3D" : "2D");
                    g_persp_deferred = 0;
                    g_engine.dirty = 1;
                }
                /* Apply deferred bargraph when settled */
                if (g_bargraph_deferred) {
                    g_bargraph_level = g_bargraph_deferred_level;
                    g_bargraph_on = g_bargraph_deferred_mode;
                    if (g_bargraph_on == 2) {
                        g_bargraph_blink_vis = 1;
                        g_bargraph_blink_timer = 0;
                    }
                    g_bargraph_deferred = 0;
                    g_engine.dirty = 1;
                }
            }
        }
        break;
    case ENGINE_IDLE:
        break;
    }
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
    fprintf(stderr, "c_render: starting %dx%d\n", WINDOW_W, WINDOW_H);

    /* Initialize TCP server FIRST so Java can connect while display inits.
     * If server fails, nothing works — exit immediately. */
    if (cr_server_init(CR_TCP_PORT) < 0) {
        fprintf(stderr, "c_render: server init failed\n");
        return 1;
    }

    if (platform_init(WINDOW_W, WINDOW_H) < 0) {
        fprintf(stderr, "c_render: platform init failed\n");
        cr_server_shutdown();
        return 1;
    }

    int fb_w, fb_h;
    platform_get_framebuffer_size(&fb_w, &fb_h);
    fprintf(stderr, "c_render: framebuffer %dx%d\n", fb_w, fb_h);

    if (render_init(fb_w, fb_h) < 0) {
        fprintf(stderr, "c_render: render init failed\n");
        platform_shutdown();
        cr_server_shutdown();
        return 1;
    }

    /* Try multiple paths for flag atlas */
    if (render_load_flag_atlas("resources/flag_atlas.rgba", 128, 128, 14) != 0) {
        if (render_load_flag_atlas("flag_atlas.rgba", 128, 128, 14) != 0) {
            if (argc > 0 && argv[0] != NULL) {
                char tmp[512];
                strncpy(tmp, argv[0], sizeof(tmp) - 1);
                tmp[sizeof(tmp) - 1] = '\0';
                char *dir = dirname(tmp);
                char atlas_path[512];
                snprintf(atlas_path, sizeof(atlas_path), "%s/resources/flag_atlas.rgba", dir);
                if (render_load_flag_atlas(atlas_path, 128, 128, 14) != 0) {
                    snprintf(atlas_path, sizeof(atlas_path), "%s/flag_atlas.rgba", dir);
                    render_load_flag_atlas(atlas_path, 128, 128, 14);
                }
            }
        }
    }

#ifdef CR_DEBUG_GRID
    render_set_debug_grid(1);
#endif

    /* Engine starts with no current maneuver */
    memset(&g_engine, 0, sizeof(g_engine));
    g_engine.phase = ENGINE_IDLE;

    fprintf(stderr, "c_render: ready, waiting for commands on :%d\n", CR_TCP_PORT);

    int dirty = 1;
    int running = 1;

    while (running && !platform_should_close()) {
        struct timespec t_start;
        clock_gettime(CLOCK_MONOTONIC, &t_start);

        platform_poll();
        cr_server_poll();

        /* Drain all pending commands, keep only the last CMD_MANEUVER */
        cr_cmd_t cmd;
        int got_maneuver = 0;
        maneuver_state_t pending_maneuver;
        uint8_t pending_flags = 0;
        uint8_t pending_perspective = 1;
        uint8_t pending_bargraph_level = 0;
        uint8_t pending_bargraph_mode = 0;
        int got_screenshot = 0;
        char screenshot_label[17];

        while (cr_server_read_cmd(&cmd)) {
            switch (cmd.cmd) {
            case CMD_MANEUVER: {
                decode_maneuver(&cmd, &pending_maneuver);
                pending_flags = cmd.flags;
                pending_perspective = cmd.payload[43];
                pending_bargraph_level = cmd.payload[44];
                pending_bargraph_mode = cmd.payload[45];
                got_maneuver = 1;
                break;
            }
            case CMD_SCREENSHOT: {
                got_screenshot = 1;
                memcpy(screenshot_label, cmd.payload, 16);
                screenshot_label[16] = '\0';
                break;
            }
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
            case CMD_BARGRAPH:
                g_bargraph_level = cmd.payload[0];
                if (g_bargraph_level > 16) g_bargraph_level = 16;
                g_bargraph_on = cmd.payload[1];  /* 0=off, 1=on, 2=blink */
                if (g_bargraph_on > 2) g_bargraph_on = 1;
                if (g_bargraph_on == 2) {
                    g_bargraph_blink_vis = 1;
                    g_bargraph_blink_timer = 0;
                }
                g_engine.dirty = 1;
                fprintf(stderr, "engine: bargraph level=%d on=%d\n",
                        g_bargraph_level, g_bargraph_on);
                break;
            case CMD_SHUTDOWN:
                fprintf(stderr, "engine: shutdown command received\n");
                running = 0;
                break;
            }
        }

        /* Apply the latest maneuver (draining to latest) */
        if (got_maneuver) {
            if (pending_flags & MAN_FLAG_SET_PERSP) {
                g_persp_deferred = 1;
                g_persp_deferred_value = pending_perspective ? 1 : 0;
                fprintf(stderr, "engine: deferred perspective=%s\n",
                        g_persp_deferred_value ? "3D" : "2D");
            }
            if (pending_flags & MAN_FLAG_BARGRAPH) {
                g_bargraph_deferred = 1;
                g_bargraph_deferred_level = pending_bargraph_level;
                if (g_bargraph_deferred_level > 16) g_bargraph_deferred_level = 16;
                g_bargraph_deferred_mode = pending_bargraph_mode;
                if (g_bargraph_deferred_mode > 2) g_bargraph_deferred_mode = 1;
            }
            engine_apply_maneuver(&pending_maneuver);
        }

        /* Tick engine state machine */
        engine_tick();

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
            g_fade_alpha += FADE_SPEED;
            if (g_fade_alpha >= 1.0f) {
                g_fade_alpha = 1.0f;
                g_fade_active = 0;
            }
            render_set_global_alpha(g_fade_alpha);
            g_engine.dirty = 1;
        }

        /* Render if needed */
        if (g_engine.dirty || render_is_animating() || maneuver_needs_redraw() || got_screenshot
#ifdef CR_DEBUG_GRID
            || 1  /* always render when grid is compiled in */
#endif
           )
            dirty = 1;
        g_engine.dirty = 0;

        /* Bargraph alpha fade */
        {
            float target = (g_bargraph_on > 0) ? 1.0f : 0.0f;
            if (g_bargraph_alpha < target) {
                g_bargraph_alpha += BARGRAPH_FADE_SPEED;
                if (g_bargraph_alpha > target) g_bargraph_alpha = target;
                dirty = 1;
            } else if (g_bargraph_alpha > target) {
                g_bargraph_alpha -= BARGRAPH_FADE_SPEED;
                if (g_bargraph_alpha < target) g_bargraph_alpha = target;
                dirty = 1;
            }
        }

        /* Bargraph blink tick */
        if (g_bargraph_on == 2) {
            g_bargraph_blink_timer++;
            if (g_bargraph_blink_timer >= BARGRAPH_BLINK_FRAMES) {
                g_bargraph_blink_timer = 0;
                g_bargraph_blink_vis = !g_bargraph_blink_vis;
                dirty = 1;
            }
        }

        if (dirty) {
            maneuver_state_t *next_ptr = g_engine.has_next ? &g_engine.next : NULL;
            maneuver_prepare_frame(&g_engine.current, next_ptr);
            render_begin_frame();
            maneuver_draw(&g_engine.current, next_ptr);
            if (g_bargraph_alpha > 0.0f) {
                float ba = g_bargraph_alpha * g_fade_alpha;
                int bl = g_bargraph_level;
                if (g_bargraph_on == 2)
                    bl = g_bargraph_blink_vis ? 16 : 0;  /* blink: full ↔ empty */
                render_bargraph(bl, ba);
            }
            render_debug_grid();
            render_end_frame();

            if (got_screenshot)
                save_screenshot(fb_w, fb_h, screenshot_label);

            platform_swap();

            dirty = render_is_animating() || maneuver_needs_redraw() || g_fade_active
                 || g_bargraph_on == 2
                 || (g_bargraph_alpha > 0.0f && g_bargraph_alpha < 1.0f);
        }

        /* Watchdog: periodically re-declare context 74 to reclaim displayable 20
         * if native navi boot took it over.  Time-based (not frame-based) so
         * it still fires at 5s intervals when adaptive idle sleep slows the
         * main loop below 30 FPS. */
#ifdef PLATFORM_QNX
        {
            static struct timespec wd_last = {0, 0};
            const long WD_INTERVAL_NS = 5L * 1000000000L;  /* 5 s */
            struct timespec now;
            clock_gettime(CLOCK_MONOTONIC, &now);
            long since_ns = (now.tv_sec - wd_last.tv_sec) * 1000000000L
                          + (now.tv_nsec - wd_last.tv_nsec);
            if (wd_last.tv_sec == 0 || since_ns >= WD_INTERVAL_NS) {
                int display_id = CR_DISPLAY_ID;
                int context_id = CR_CONTEXT_ID;
                int displayable_id = CR_DISPLAYABLE_ID;
                char cmd[128];

                wd_last = now;
                /* Re-declare context composition (idempotent if already correct) */
                platform_get_routing_ids(&display_id, &context_id, &displayable_id);
                snprintf(cmd, sizeof(cmd),
                         "/eso/bin/apps/dmdt dc %d %d 102 101 33 >/dev/null 2>&1",
                         context_id, displayable_id);
                system(cmd);
            }
        }
#endif

        /* Heartbeat is dispatched on a dedicated pthread inside server.c —
         * the render loop must never touch send() syscalls.  Even a
         * non-blocking loopback send can take a few ms on QNX 6.5, which
         * shows up as 1 Hz frame jitter when done in the main loop. */

        /* Adaptive idle sleep — render-on-demand for the main loop.
         *
         * The render block above already only draws when `dirty` is set,
         * so we don't waste GPU on unchanged frames.  But the loop itself
         * still spins at 30 Hz polling TCP + platform events, which burns
         * CPU when nothing is happening (CarPlay idle, no route).
         *
         * Strategy: count consecutive idle frames; once we've been idle
         * for a second, drop the poll rate from 30 Hz -> 10 Hz; after 5 s
         * idle, drop to 3 Hz.  Any activity (incoming commands, render
         * still animating, dirty surface) resets the counter back to 0
         * and we go back to full 30 Hz immediately.
         *
         * Watchdog still fires on its own 5 s clock above, independent
         * of poll rate. */
        static int idle_frames = 0;
        int active = dirty || got_maneuver || got_screenshot
                   || render_is_animating() || maneuver_needs_redraw()
                   || g_fade_active || (g_bargraph_on == 2);
        if (active) {
            idle_frames = 0;
        } else if (idle_frames < 10000) {
            idle_frames++;
        }

        long target_frame_ns;
        if (idle_frames < TARGET_FPS) {           /* < 1 s idle: 30 Hz */
            target_frame_ns = FRAME_TIME_NS;
        } else if (idle_frames < TARGET_FPS * 5) { /* 1-5 s idle: 10 Hz */
            target_frame_ns = 100L * 1000000L;
        } else {                                   /* > 5 s idle: 3 Hz */
            target_frame_ns = 333L * 1000000L;
        }

        /* Frame pacing */
        struct timespec t_end;
        clock_gettime(CLOCK_MONOTONIC, &t_end);
        long elapsed_ns = (t_end.tv_sec - t_start.tv_sec) * 1000000000L
                        + (t_end.tv_nsec - t_start.tv_nsec);
        long sleep_ns = target_frame_ns - elapsed_ns;
        if (sleep_ns > 0) {
            struct timespec ts = { sleep_ns / 1000000000L,
                                   sleep_ns % 1000000000L };
            nanosleep(&ts, NULL);
        }
    }

    cr_server_shutdown();
    render_shutdown();
    platform_shutdown();

    fprintf(stderr, "c_render: exit\n");
    return 0;
}
