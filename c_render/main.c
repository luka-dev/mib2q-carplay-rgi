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
 * PUSHING:    slide 1→2, current pushes out + next slides in
 * SLIDING_IN: commit next→current, slide 0→1
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
            /* Push complete — commit next as current */
            if (g_engine.has_next) {
                g_engine.current = g_engine.next;
                g_engine.has_next = 0;
            }
            maneuver_commit_pushed_state(&g_engine.current);
            render_invalidate_masks();
            g_engine.dirty = 1;

            if (g_engine.has_pending) {
                /* Promote pending → next, immediately start new push */
                g_engine.next = g_engine.pending;
                g_engine.has_next = 1;
                g_engine.has_pending = 0;
                maneuver_start_push();
                g_engine.phase = ENGINE_PUSHING;
                fprintf(stderr, "engine: promoting queued icon=%d\n",
                        g_engine.next.icon);
            } else {
                g_engine.phase = ENGINE_SLIDING_IN;
            }
        }
        break;
    case ENGINE_SLIDING_IN:
        if (!maneuver_is_animating()) {
            if (g_engine.has_pending) {
                /* Promote pending → next, start push */
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
            }
        }
        break;
    case ENGINE_IDLE:
        break;
    }
}

/* ================================================================
 * Screenshot — save framebuffer as PPM
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
    (void)argc;

    fprintf(stderr, "c_render: starting %dx%d\n", WINDOW_W, WINDOW_H);

    if (platform_init(WINDOW_W, WINDOW_H) < 0) {
        fprintf(stderr, "c_render: platform init failed\n");
        return 1;
    }

    int fb_w, fb_h;
    platform_get_framebuffer_size(&fb_w, &fb_h);
    fprintf(stderr, "c_render: framebuffer %dx%d\n", fb_w, fb_h);

    if (render_init(fb_w, fb_h) < 0) {
        fprintf(stderr, "c_render: render init failed\n");
        platform_shutdown();
        return 1;
    }

    /* Try multiple paths for flag atlas */
    if (render_load_flag_atlas("resources/flag_atlas.rgba", 128, 128, 14) != 0)
    if (render_load_flag_atlas("flag_atlas.rgba", 128, 128, 14) != 0) {
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

    /* Initialize TCP server */
    if (cr_server_init(CR_TCP_PORT) < 0) {
        fprintf(stderr, "c_render: server init failed\n");
        render_shutdown();
        platform_shutdown();
        return 1;
    }

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
        int got_screenshot = 0;
        char screenshot_label[17];

        while (cr_server_read_cmd(&cmd)) {
            switch (cmd.cmd) {
            case CMD_MANEUVER: {
                decode_maneuver(&cmd, &pending_maneuver);
                pending_flags = cmd.flags;
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
                maneuver_toggle_debug();
                fprintf(stderr, "engine: debug=%s\n", maneuver_is_debug() ? "ON" : "OFF");
                g_engine.dirty = 1;
                break;
            case CMD_SHUTDOWN:
                fprintf(stderr, "engine: shutdown command received\n");
                running = 0;
                break;
            }
        }

        /* Apply the latest maneuver (draining to latest) */
        if (got_maneuver) {
            if (pending_flags & MAN_FLAG_RESET_PERSP) {
                render_set_perspective(1);
                g_engine.dirty = 1;
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
        if (g_engine.dirty || render_is_animating() || maneuver_is_animating() || got_screenshot)
            dirty = 1;
        g_engine.dirty = 0;

        if (dirty) {
            maneuver_state_t *next_ptr = g_engine.has_next ? &g_engine.next : NULL;
            maneuver_prepare_frame(&g_engine.current, next_ptr);
            render_begin_frame();
            maneuver_draw(&g_engine.current, next_ptr);
            render_end_frame();

            if (got_screenshot)
                save_screenshot(fb_w, fb_h, screenshot_label);

            platform_swap();

            dirty = render_is_animating() || maneuver_is_animating() || g_fade_active;
        }

        /* Frame pacing */
        struct timespec t_end;
        clock_gettime(CLOCK_MONOTONIC, &t_end);
        long elapsed_ns = (t_end.tv_sec - t_start.tv_sec) * 1000000000L
                        + (t_end.tv_nsec - t_start.tv_nsec);
        long sleep_ns = FRAME_TIME_NS - elapsed_ns;
        if (sleep_ns > 0) {
            struct timespec ts = {0, sleep_ns};
            nanosleep(&ts, NULL);
        }
    }

    cr_server_shutdown();
    render_shutdown();
    platform_shutdown();

    fprintf(stderr, "c_render: exit\n");
    return 0;
}
