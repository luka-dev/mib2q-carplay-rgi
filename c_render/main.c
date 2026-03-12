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
 * Test mode — cycle through maneuver types with keyboard
 * ================================================================ */

static maneuver_state_t g_state = {
    .maneuver_type      = MT_NOT_SET,
    .exit_angle         = 0,
    .junction_type      = 0,
    .driving_side       = 0,
    .junction_angles    = {0},
    .junction_angle_count = 0,
};

/* Test order: meaningful subset showing all icon variants */
static const int g_test_types[] = {
    MT_STRAIGHT_AHEAD,
    MT_FOLLOW_ROAD,
    MT_SLIGHT_RIGHT_TURN,
    MT_RIGHT_TURN,
    MT_SHARP_RIGHT_TURN,
    MT_SLIGHT_LEFT_TURN,
    MT_LEFT_TURN,
    MT_SHARP_LEFT_TURN,
    MT_KEEP_RIGHT,
    MT_KEEP_LEFT,
    MT_U_TURN,
    MT_START_ROUTE_WITH_U_TURN,
    MT_OFF_RAMP,
    MT_HIGHWAY_OFF_RAMP_RIGHT,
    MT_HIGHWAY_OFF_RAMP_LEFT,
    MT_ON_RAMP,
    MT_ENTER_ROUNDABOUT,
    MT_EXIT_ROUNDABOUT,
    MT_ROUNDABOUT_EXIT_1,    /* will use exit_angle */
    MT_ROUNDABOUT_EXIT_3,
    MT_ROUNDABOUT_EXIT_6,
    MT_U_TURN_AT_ROUNDABOUT,
    MT_CHANGE_HIGHWAY_LEFT,
    MT_CHANGE_HIGHWAY_RIGHT,
    MT_ARRIVE_AT_DESTINATION,
    MT_ARRIVE_DESTINATION_LEFT,
    MT_ARRIVE_DESTINATION_RIGHT,
    MT_LEFT_TURN_AT_END,
    MT_RIGHT_TURN_AT_END,
};
#define TEST_COUNT (int)(sizeof(g_test_types) / sizeof(g_test_types[0]))

static int g_test_idx = 0;

/* Default exit angles for roundabout test types */
static int exit_angle_for_test(int type) {
    switch (type) {
        case MT_ROUNDABOUT_EXIT_1:  return 90;    /* first exit right */
        case MT_ROUNDABOUT_EXIT_3:  return 0;     /* straight through */
        case MT_ROUNDABOUT_EXIT_6:  return -90;   /* left exit */
        case MT_EXIT_ROUNDABOUT:    return 90;
        default: return 0;
    }
}

/* Set junction_type based on maneuver type */
static int junction_for_test(int type) {
    if (type >= MT_ROUNDABOUT_EXIT_1 && type <= MT_ROUNDABOUT_EXIT_19)
        return 1;
    if (type == MT_EXIT_ROUNDABOUT || type == MT_ENTER_ROUNDABOUT ||
        type == MT_U_TURN_AT_ROUNDABOUT)
        return 1;
    return 0;
}

/* Set realistic junction angles for roundabout test types.
 * junction_angles = all roads at junction (iOS degrees), including exit road. */
static void junction_angles_for_test(int type, maneuver_state_t *s) {
    s->junction_angle_count = 0;
    switch (type) {
        case MT_ENTER_ROUNDABOUT:
            /* 4-exit roundabout */
            s->junction_angles[0] = 90;   /* right */
            s->junction_angles[1] = 0;    /* straight */
            s->junction_angles[2] = -90;  /* left */
            s->junction_angle_count = 3;
            break;
        case MT_EXIT_ROUNDABOUT:
            /* 4-exit roundabout, taking right exit */
            s->junction_angles[0] = 90;
            s->junction_angles[1] = 0;
            s->junction_angles[2] = -90;
            s->junction_angles[3] = -150;
            s->junction_angle_count = 4;
            break;
        case MT_ROUNDABOUT_EXIT_1:
            /* 3-exit roundabout, take 1st exit (right) */
            s->junction_angles[0] = 90;
            s->junction_angles[1] = 0;
            s->junction_angles[2] = -90;
            s->junction_angle_count = 3;
            break;
        case MT_ROUNDABOUT_EXIT_3:
            /* 5-exit roundabout, take 3rd exit (straight) */
            s->junction_angles[0] = 90;
            s->junction_angles[1] = 45;
            s->junction_angles[2] = 0;
            s->junction_angles[3] = -45;
            s->junction_angles[4] = -90;
            s->junction_angle_count = 5;
            break;
        case MT_ROUNDABOUT_EXIT_6:
            /* 7-exit roundabout, take 6th exit (left) */
            s->junction_angles[0] = 120;
            s->junction_angles[1] = 80;
            s->junction_angles[2] = 40;
            s->junction_angles[3] = 0;
            s->junction_angles[4] = -40;
            s->junction_angles[5] = -90;
            s->junction_angles[6] = -140;
            s->junction_angle_count = 7;
            break;
        case MT_U_TURN_AT_ROUNDABOUT:
            /* 4-exit roundabout, u-turn */
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
    int t = g_test_types[g_test_idx];
    g_state.maneuver_type = t;
    g_state.exit_angle = exit_angle_for_test(t);
    g_state.junction_type = junction_for_test(t);
    junction_angles_for_test(t, &g_state);
}

static void handle_test_keys(void) {
    if (platform_key_tap(CR_KEY_RIGHT)) {
        g_test_idx = (g_test_idx + 1) % TEST_COUNT;
        update_test_state();
        fprintf(stderr, "c_render: [%d/%d] %s (exit_angle=%d jt=%d ds=%d junc=%d)\n",
                g_test_idx + 1, TEST_COUNT,
                maneuver_type_name(g_state.maneuver_type),
                g_state.exit_angle, g_state.junction_type, g_state.driving_side,
                g_state.junction_angle_count);
    }
    if (platform_key_tap(CR_KEY_LEFT)) {
        g_test_idx = (g_test_idx - 1 + TEST_COUNT) % TEST_COUNT;
        update_test_state();
        fprintf(stderr, "c_render: [%d/%d] %s (exit_angle=%d jt=%d ds=%d junc=%d)\n",
                g_test_idx + 1, TEST_COUNT,
                maneuver_type_name(g_state.maneuver_type),
                g_state.exit_angle, g_state.junction_type, g_state.driving_side,
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
 * Screenshot — save framebuffer as PNG via stb or PPM fallback
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
            TEST_COUNT, maneuver_type_name(g_state.maneuver_type));

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
            save_screenshot(fb_w, fb_h, maneuver_type_name(g_state.maneuver_type));

        platform_swap();
    }

    render_shutdown();
    platform_shutdown();

    fprintf(stderr, "c_render: exit\n");
    return 0;
}
