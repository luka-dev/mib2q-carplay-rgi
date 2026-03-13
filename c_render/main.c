/*
 * CarPlay Cluster Renderer -- main entry point.
 *
 * Platform-independent main loop.
 * macOS: GLFW window for development/testing.
 *   L/R = cycle type, Up = exit angle, Down = driving side,
 *   P = perspective, S = side streets, Space = screenshot.
 * QNX: displayable 200 for LVDS video output (data from PPS).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "platform.h"

#define TARGET_FPS     30
#define FRAME_TIME_NS  (1000000000L / TARGET_FPS)
#include "gl_compat.h"
#include "render.h"
#include "protocol.h"
#include "maneuver.h"

#define WINDOW_W CR_DEFAULT_WIDTH
#define WINDOW_H CR_DEFAULT_HEIGHT

/* ================================================================
 * Test mode — cycle through icon variants with keyboard
 * ================================================================ */

static maneuver_state_t g_state = {
    .icon               = ICON_NONE,
    .exit_angle         = 0,
    .direction          = 0,
    .driving_side       = 0,
    .junction_angles    = {0},
    .junction_angle_count = 0,
};

/* Test entries: {icon, exit_angle, direction, label, rab_preset} */
typedef struct {
    int icon;
    int exit_angle;
    int direction;
    const char *label;
    int rab_preset;   /* 0=none, 1..5 = roundabout junction presets */
} test_entry_t;

static const test_entry_t g_tests[] = {
    /* Straight */
    { ICON_STRAIGHT,    0,    0, "STRAIGHT",        0 },

    /* Turns — all 6 angles */
    { ICON_TURN,        30,   0, "SLIGHT_RIGHT",    0 },
    { ICON_TURN,        90,   0, "RIGHT",           0 },
    { ICON_TURN,       135,   0, "SHARP_RIGHT",     0 },
    { ICON_TURN,       -30,   0, "SLIGHT_LEFT",     0 },
    { ICON_TURN,       -90,   0, "LEFT",            0 },
    { ICON_TURN,      -135,   0, "SHARP_LEFT",      0 },

    /* U-turn */
    { ICON_UTURN,       0,    0, "UTURN",           0 },

    /* Merge (on-ramp) */
    { ICON_MERGE,       0,    1, "MERGE_R",         0 },

    /* Lane change */
    { ICON_LANE_CHANGE, 0,   -1, "LANE_CHG_L",     0 },
    { ICON_LANE_CHANGE, 0,    1, "LANE_CHG_R",      0 },

    /* Roundabout variants */
    { ICON_ROUNDABOUT,  90,   0, "RAB_ENTER",       1 },
    { ICON_ROUNDABOUT,  90,   0, "RAB_EXIT_1",      2 },
    { ICON_ROUNDABOUT,   0,   0, "RAB_EXIT_3",      3 },
    { ICON_ROUNDABOUT, -90,   0, "RAB_EXIT_6",      4 },
    { ICON_ROUNDABOUT, 180,   0, "RAB_UTURN",       5 },

    /* Arrived */
    { ICON_ARRIVED,     0,    0, "ARRIVED",         0 },
    { ICON_ARRIVED,     0,   -1, "ARRIVED_L",       0 },
    { ICON_ARRIVED,     0,    1, "ARRIVED_R",       0 },
};
#define TEST_COUNT (int)(sizeof(g_tests) / sizeof(g_tests[0]))

static int g_test_idx = 0;

/* ---- Side street presets (applied to TURN and STRAIGHT) ---- */

typedef struct {
    const char *name;
    int angles[6];
    int count;
} sides_preset_t;

static const sides_preset_t g_sides_presets[] = {
    { "none",       {0},                    0 },
    { "fwd",        {0},                    1 },  /* forward only (fork) */
    { "cross",      {90, -90},              2 },  /* cross streets */
    { "T-right",    {90},                   1 },
    { "T-left",     {-90},                  1 },
    { "full",       {0, 90, -90},           3 },  /* full intersection */
    { "5-way",      {0, 60, -60, 120},      4 },
};
#define SIDES_COUNT (int)(sizeof(g_sides_presets) / sizeof(g_sides_presets[0]))

static int g_sides_idx = 0;

/* ---- Roundabout junction angle presets ---- */

static void set_rab_preset(int preset, maneuver_state_t *s) {
    s->junction_angle_count = 0;
    switch (preset) {
        case 1: /* 4-exit, enter */
            s->junction_angles[0] = 90;
            s->junction_angles[1] = 0;
            s->junction_angles[2] = -90;
            s->junction_angle_count = 3;
            break;
        case 2: /* 4-exit, right exit */
            s->junction_angles[0] = 90;
            s->junction_angles[1] = 0;
            s->junction_angles[2] = -90;
            s->junction_angles[3] = -150;
            s->junction_angle_count = 4;
            break;
        case 3: /* 5-exit, straight */
            s->junction_angles[0] = 90;
            s->junction_angles[1] = 45;
            s->junction_angles[2] = 0;
            s->junction_angles[3] = -45;
            s->junction_angles[4] = -90;
            s->junction_angle_count = 5;
            break;
        case 4: /* 7-exit, left */
            s->junction_angles[0] = 120;
            s->junction_angles[1] = 80;
            s->junction_angles[2] = 40;
            s->junction_angles[3] = 0;
            s->junction_angles[4] = -40;
            s->junction_angles[5] = -90;
            s->junction_angles[6] = -140;
            s->junction_angle_count = 7;
            break;
        case 5: /* 4-exit, u-turn */
            s->junction_angles[0] = 90;
            s->junction_angles[1] = 0;
            s->junction_angles[2] = -90;
            s->junction_angles[3] = 180;
            s->junction_angle_count = 4;
            break;
        default:
            break;
    }
}

static void apply_sides_preset(maneuver_state_t *s) {
    const sides_preset_t *p = &g_sides_presets[g_sides_idx];
    int i;
    s->junction_angle_count = p->count;
    for (i = 0; i < p->count; i++)
        s->junction_angles[i] = p->angles[i];
}

static void update_test_state(void) {
    const test_entry_t *e = &g_tests[g_test_idx];
    g_state.icon = e->icon;
    g_state.exit_angle = e->exit_angle;
    g_state.direction = e->direction;
    if (e->rab_preset > 0) {
        set_rab_preset(e->rab_preset, &g_state);
    } else if (e->icon == ICON_TURN || e->icon == ICON_STRAIGHT) {
        apply_sides_preset(&g_state);
    } else {
        g_state.junction_angle_count = 0;
    }
}

static const char *test_label(void) {
    return g_tests[g_test_idx].label;
}

static void print_state(void) {
    const test_entry_t *e = &g_tests[g_test_idx];
    if (e->icon == ICON_TURN || e->icon == ICON_STRAIGHT) {
        fprintf(stderr, "c_render: [%d/%d] %s (angle=%d sides=%s ds=%d)\n",
                g_test_idx + 1, TEST_COUNT, test_label(),
                g_state.exit_angle, g_sides_presets[g_sides_idx].name,
                g_state.driving_side);
    } else {
        fprintf(stderr, "c_render: [%d/%d] %s (angle=%d dir=%d ds=%d junc=%d)\n",
                g_test_idx + 1, TEST_COUNT, test_label(),
                g_state.exit_angle, g_state.direction, g_state.driving_side,
                g_state.junction_angle_count);
    }
}

static int g_dirty = 1;  /* set when state changes — triggers re-render */

static void handle_test_keys(void) {
    if (platform_key_tap(CR_KEY_RIGHT)) {
        g_test_idx = (g_test_idx + 1) % TEST_COUNT;
        update_test_state();
        print_state();
        g_dirty = 1;
    }
    if (platform_key_tap(CR_KEY_LEFT)) {
        g_test_idx = (g_test_idx - 1 + TEST_COUNT) % TEST_COUNT;
        update_test_state();
        print_state();
        g_dirty = 1;
    }
    if (platform_key_tap(CR_KEY_UP)) {
        g_state.exit_angle += 30;
        if (g_state.exit_angle > 180) g_state.exit_angle = -150;
        fprintf(stderr, "c_render: exit_angle=%d\n", g_state.exit_angle);
        g_dirty = 1;
    }
    if (platform_key_tap(CR_KEY_DOWN)) {
        g_state.driving_side = !g_state.driving_side;
        fprintf(stderr, "c_render: driving_side=%d (%s)\n",
                g_state.driving_side, g_state.driving_side ? "LHT" : "RHT");
        g_dirty = 1;
    }
    if (platform_key_tap(CR_KEY_P)) {
        static int persp = 1;
        persp = !persp;
        render_set_perspective(persp);
        fprintf(stderr, "c_render: perspective=%s\n", persp ? "ON" : "OFF");
        g_dirty = 1;
    }
    if (platform_key_tap(CR_KEY_S)) {
        g_sides_idx = (g_sides_idx + 1) % SIDES_COUNT;
        update_test_state();
        fprintf(stderr, "c_render: sides=%s (%d streets)\n",
                g_sides_presets[g_sides_idx].name,
                g_sides_presets[g_sides_idx].count);
        g_dirty = 1;
    }
    /* Camera tuning: Z/X=eyeZ, C/V=eyeY, B/N=ctrZ, F/G=fov */
    if (platform_key_tap(CR_KEY_Z)) { render_cam_adjust(0, -0.2f); g_dirty = 1; }
    if (platform_key_tap(CR_KEY_X)) { render_cam_adjust(0,  0.2f); g_dirty = 1; }
    if (platform_key_tap(CR_KEY_C)) { render_cam_adjust(1,  0.1f); g_dirty = 1; }
    if (platform_key_tap(CR_KEY_V)) { render_cam_adjust(1, -0.1f); g_dirty = 1; }
    if (platform_key_tap(CR_KEY_B)) { render_cam_adjust(2, -0.05f); g_dirty = 1; }
    if (platform_key_tap(CR_KEY_N)) { render_cam_adjust(2,  0.05f); g_dirty = 1; }
    if (platform_key_tap(CR_KEY_F)) { render_cam_adjust(3, -1.0f); g_dirty = 1; }
    if (platform_key_tap(CR_KEY_G)) { render_cam_adjust(3,  1.0f); g_dirty = 1; }
    /* H/J = center Y up/down */
    if (platform_key_tap(CR_KEY_H)) { render_cam_adjust(4,  0.02f); g_dirty = 1; }
    if (platform_key_tap(CR_KEY_J)) { render_cam_adjust(4, -0.02f); g_dirty = 1; }
}

/* ================================================================
 * Screenshot — save framebuffer as PPM
 * ================================================================ */

static int g_snap_counter = 0;

static void save_screenshot(int fb_w, int fb_h, const char *name) {
    unsigned char *pixels = (unsigned char *)malloc(fb_w * fb_h * 4);
    if (!pixels) return;
    glReadPixels(0, 0, fb_w, fb_h, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

    char path[256];
    snprintf(path, sizeof(path), "snap_%03d_%s.ppm", g_snap_counter++, name);

    FILE *f = fopen(path, "wb");
    if (!f) { free(pixels); return; }
    fprintf(f, "P6\n%d %d\n255\n", fb_w, fb_h);
    /* PPM is top-to-bottom, GL is bottom-to-top — flip rows */
    int y, x;
    for (y = fb_h - 1; y >= 0; y--) {
        for (x = 0; x < fb_w; x++) {
            unsigned char *p = pixels + (y * fb_w + x) * 4;
            fwrite(p, 1, 3, f);  /* RGB only, skip A */
        }
    }
    fclose(f);
    free(pixels);
    fprintf(stderr, "c_render: saved %s\n", path);
}

/* ================================================================
 * Main
 * ================================================================ */

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

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

    /* Start with first test maneuver */
    update_test_state();
    fprintf(stderr, "c_render: [1/%d] %s  (L/R=type, Up=angle, Down=side, P=persp, S=sides)\n",
            TEST_COUNT, test_label());

    int dirty = 1;  /* first frame always renders */

    while (!platform_should_close()) {
        struct timespec t_start;
        clock_gettime(CLOCK_MONOTONIC, &t_start);

        platform_poll();
        handle_test_keys();

        /* Update framebuffer size (HiDPI) */
        int new_w, new_h;
        platform_get_framebuffer_size(&new_w, &new_h);
        if (new_w != fb_w || new_h != fb_h) {
            fb_w = new_w;
            fb_h = new_h;
            render_set_viewport(fb_w, fb_h);
            dirty = 1;
        }

        /* Check if any key changed state or animation in progress */
        int want_screenshot = platform_key_tap(CR_KEY_SPACE);
        if (g_dirty || render_is_animating() || want_screenshot)
            dirty = 1;
        g_dirty = 0;

        if (dirty) {
            render_begin_frame();
            maneuver_draw(&g_state);
            render_end_frame();

            if (want_screenshot)
                save_screenshot(fb_w, fb_h, test_label());

            platform_swap();

            dirty = render_is_animating();  /* keep rendering while animating */
        }

        /* Frame pacing — sleep remainder of frame budget */
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

    render_shutdown();
    platform_shutdown();

    fprintf(stderr, "c_render: exit\n");
    return 0;
}
