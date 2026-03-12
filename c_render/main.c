/*
 * CarPlay Cluster Renderer -- main entry point.
 *
 * Platform-independent main loop.
 * macOS: GLFW window for development/testing (left/right = cycle type, up/down = exit angle).
 * QNX:   displayable 200 for LVDS video output (data from PPS).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "platform.h"
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

/* Test entries: {icon, exit_angle, direction, label, junction setup index} */
typedef struct {
    int icon;
    int exit_angle;
    int direction;
    const char *label;
    int rab_preset;   /* 0=none, 1..6 = roundabout junction presets */
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

    /* Exit (off-ramp) */
    { ICON_EXIT,        0,    1, "EXIT_R",          0 },
    { ICON_EXIT,        0,   -1, "EXIT_L",          0 },

    /* Merge (on-ramp) */
    { ICON_MERGE,       0,    1, "MERGE_R",         0 },

    /* Lane change */
    { ICON_LANE_CHANGE, 0,   -1, "LANE_CHG_L",     0 },
    { ICON_LANE_CHANGE, 0,    1, "LANE_CHG_R",      0 },

    /* Roundabout variants */
    { ICON_ROUNDABOUT,  90,   0, "RAB_ENTER",       1 },  /* 4-exit, enter */
    { ICON_ROUNDABOUT,  90,   0, "RAB_EXIT_1",      2 },  /* 4-exit, 1st exit right */
    { ICON_ROUNDABOUT,   0,   0, "RAB_EXIT_3",      3 },  /* 5-exit, straight */
    { ICON_ROUNDABOUT, -90,   0, "RAB_EXIT_6",      4 },  /* 7-exit, left */
    { ICON_ROUNDABOUT, 180,   0, "RAB_UTURN",       5 },  /* 4-exit, u-turn */

    /* Arrived */
    { ICON_ARRIVED,     0,    0, "ARRIVED",         0 },
    { ICON_ARRIVED,     0,   -1, "ARRIVED_L",       0 },
    { ICON_ARRIVED,     0,    1, "ARRIVED_R",       0 },
};
#define TEST_COUNT (int)(sizeof(g_tests) / sizeof(g_tests[0]))

static int g_test_idx = 0;

/* Roundabout junction angle presets */
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

static void update_test_state(void) {
    const test_entry_t *e = &g_tests[g_test_idx];
    g_state.icon = e->icon;
    g_state.exit_angle = e->exit_angle;
    g_state.direction = e->direction;
    set_rab_preset(e->rab_preset, &g_state);
}

static const char *test_label(void) {
    return g_tests[g_test_idx].label;
}

static void handle_test_keys(void) {
    if (platform_key_tap(CR_KEY_RIGHT)) {
        g_test_idx = (g_test_idx + 1) % TEST_COUNT;
        update_test_state();
        fprintf(stderr, "c_render: [%d/%d] %s (angle=%d dir=%d ds=%d junc=%d)\n",
                g_test_idx + 1, TEST_COUNT, test_label(),
                g_state.exit_angle, g_state.direction, g_state.driving_side,
                g_state.junction_angle_count);
    }
    if (platform_key_tap(CR_KEY_LEFT)) {
        g_test_idx = (g_test_idx - 1 + TEST_COUNT) % TEST_COUNT;
        update_test_state();
        fprintf(stderr, "c_render: [%d/%d] %s (angle=%d dir=%d ds=%d junc=%d)\n",
                g_test_idx + 1, TEST_COUNT, test_label(),
                g_state.exit_angle, g_state.direction, g_state.driving_side,
                g_state.junction_angle_count);
    }
    if (platform_key_tap(CR_KEY_UP)) {
        g_state.exit_angle += 30;
        if (g_state.exit_angle > 180) g_state.exit_angle = -150;
        fprintf(stderr, "c_render: exit_angle=%d\n", g_state.exit_angle);
    }
    if (platform_key_tap(CR_KEY_DOWN)) {
        g_state.driving_side = !g_state.driving_side;
        fprintf(stderr, "c_render: driving_side=%d (%s)\n",
                g_state.driving_side, g_state.driving_side ? "LHT" : "RHT");
    }
    if (platform_key_tap(CR_KEY_P)) {
        static int persp = 1;
        persp = !persp;
        render_set_perspective(persp);
        fprintf(stderr, "c_render: perspective=%s\n", persp ? "ON" : "OFF");
    }
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
    fprintf(stderr, "c_render: [1/%d] %s  (L/R=type, Up=angle, Down=side, P=perspective)\n",
            TEST_COUNT, test_label());

    while (!platform_should_close()) {
        platform_poll();
        handle_test_keys();

        /* Update framebuffer size (HiDPI) */
        int new_w, new_h;
        platform_get_framebuffer_size(&new_w, &new_h);
        if (new_w != fb_w || new_h != fb_h) {
            fb_w = new_w;
            fb_h = new_h;
            render_set_viewport(fb_w, fb_h);
        }

        render_begin_frame();
        maneuver_draw(&g_state);
        render_end_frame();

        if (platform_key_tap(CR_KEY_SPACE))
            save_screenshot(fb_w, fb_h, test_label());

        platform_swap();
    }

    render_shutdown();
    platform_shutdown();

    fprintf(stderr, "c_render: exit\n");
    return 0;
}
