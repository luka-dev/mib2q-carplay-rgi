/*
 * Maneuver icon rendering — procedural GL icons for all CarPlay maneuver types.
 *
 * Multi-layer mask architecture:
 *   OUTLINE mask — white road borders + outline fades
 *   FILL mask    — grey road fill + grey fades
 *   ROUTE — real 3D extruded mesh (path-based, no mask)
 *
 * Outline and fill masks are rendered flat (no lighting, no perspective).
 * render_composite() applies subtraction, materials, and perspective.
 * Route is rendered as direct 3D geometry after composite.
 */

#include <stdio.h>
#include <math.h>
#include "render.h"
#include "maneuver.h"
#include "route_path.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ================================================================
 * Colors (used as flat mask colors — lighting applied in composite)
 * ================================================================ */

#define AC_R 0.353f   /* #5AAAE6 */
#define AC_G 0.667f
#define AC_B 0.902f
#define AC_A 1.0f

#define SD_R 0.392f   /* #646464 */
#define SD_G 0.392f
#define SD_B 0.392f
#define SD_A 1.0f

/* shorthand for passing color args */
#define ACTIVE  AC_R, AC_G, AC_B, AC_A
#define SIDE    SD_R, SD_G, SD_B, SD_A

/* ================================================================
 * Arrow dimensions (NDC)
 * ================================================================ */

/* Road thickness layers */
#define SHAFT_T    0.14f    /* active blue thickness */
#define OL_W       0.025f   /* border width per side */
#define SIDE_T     (SHAFT_T + OL_W * 2)  /* grey = active + border */
#define OL_T       (SIDE_T  + OL_W * 2)  /* outline = grey + border */
#define HEAD_SZ    (SHAFT_T * 1.3f)      /* arrowhead height */

/* Universal road lengths from junction center (y=0) */
#define ROAD_LEN   0.55f    /* solid road length from junction (all directions) */
#define BLUE_LEN   0.50f    /* blue active road length from junction */
#define FADE_LEN   0.30f    /* fade distance beyond solid road */

/* Derived positions */
#define SHAFT_BOT  (-ROAD_LEN)           /* entry solid bottom */
#define SHAFT_TOP  (BLUE_LEN)            /* straight arrow tip */
#define SIDE_TOP   (ROAD_LEN)            /* forward road solid end */

/* Joint helpers */
#define JOINT_R    (SHAFT_T * 0.5f)
#define JOINT_SEG  32
#define WHITE  1.0f, 1.0f, 1.0f, 1.0f

/* Roundabout */
#define RAB_RING_R   0.28f   /* roundabout ring radius */
#define RAB_RING_SEG 64      /* ring/arc segments */

/* U-turn */
#define UTURN_GAP    0.18f   /* half-distance between entry/exit shafts */
#define UTURN_TOP    0.30f   /* top of U arc */
#define UTURN_ARROW  (-0.10f) /* arrowhead Y on exit side */
#define UTURN_SEG    16      /* arc segments */

/* Angle thresholds */
#define ANGLE_DEDUP     5.0f    /* degrees: angles closer than this are "same direction" */
#define ANGLE_DEDUP_RAD 0.09f   /* radians: same threshold (~5°) for roundabout internals */
#define ANGLE_SELF      1.0f    /* degrees: skip self when comparing angle lists */

/* Highway exit branch angle (degrees) */
#define EXIT_ANGLE_DEG  30.0f

/* Lane change / merge lateral offsets */
#define LANE_SHIFT   0.35f   /* lane change lateral offset */
#define MERGE_OFFSET 0.38f   /* merge ramp lateral offset */
#define BEND_LO     (-0.15f) /* lower S-bend point */
#define BEND_HI      0.15f   /* upper S-bend point */
#define MERGE_BEND_LO (-0.10f) /* merge lower S-bend point */

/* Flag / arrived */
#define FLAG_POLE_T  OL_W             /* flag pole thickness */
#define FLAG_W       0.22f            /* flag width */
#define FLAG_H       0.16f            /* flag height */
#define FLAG_BASE_R  0.05f            /* flag base disc radius */
#define DOME_R       0.12f            /* arrived dome cap radius */
#define ARRIVE_RING_R 0.11f           /* arrived center ring radius */
#define ARRIVE_RING_T 0.04f           /* arrived ring thickness */
#define ARRIVE_ROAD_TOP   0.10f       /* arrived road end Y */
#define ARRIVE_RING_GAP   0.12f       /* gap between arrowhead and ring center */
#define ARRIVE_FLAG_H_CTR 0.42f       /* flag pole height — center arrived */
#define ARRIVE_FLAG_H_SIDE 0.50f      /* flag pole height — side arrived */
#define ARRIVE_FLAG_X     0.30f       /* flag X offset from road — side arrived */
#define ARRIVE_FLAG_Y_OFF 0.06f       /* flag Y offset below road_top — side arrived */
#define ARRIVE_SEG        24          /* circle/disc segment count for arrived */
#define SNAP_SENTINEL     999.0f      /* large value for angle snap comparison */

/* Route extrusion heights (must match render.c) */
#define ROUTE_BASE_Y  0.03f   /* same as RAISE_BASE in render.c */
#define ROUTE_TOP_Y   0.06f   /* RAISE_BASE + EXTRUDE_H */

/* Cached route mesh — rebuilt only on maneuver state change */
static route_path_t g_route_path;
static route_mesh_t g_route_mesh;

/* Flag checkerboard dark square color (~#1E2430) */
#define CHECK_R 0.12f
#define CHECK_G 0.14f
#define CHECK_B 0.18f

/* ================================================================
 * Fading road helper
 *
 * Draws a fading extension from (x0,y0)→(x1,y1) in N segments with
 * alpha going from a_start→0 via smoothstep.
 *
 * mode: FADE_OUTLINE = white border strips only
 *       FADE_GREY    = grey center fill only
 *       FADE_BOTH    = outline then grey
 * ================================================================ */

#define FADE_SEGS    64
#define FADE_OUTLINE 0
#define FADE_GREY    1
#define FADE_BOTH    2

static void draw_fading_road(float x0, float y0, float x1, float y1,
                              float a_start, int mode) {
    if (mode == FADE_BOTH) {
        draw_fading_road(x0, y0, x1, y1, a_start, FADE_OUTLINE);
        draw_fading_road(x0, y0, x1, y1, a_start, FADE_GREY);
        return;
    }

    float dx = x1 - x0, dy = y1 - y0;
    float len = sqrtf(dx * dx + dy * dy);
    if (len < 1e-6f) return;
    float px = -dy / len, py = dx / len;
    float ol_off = (SIDE_T + OL_W) * 0.5f;

    int i;
    for (i = 0; i < FADE_SEGS; i++) {
        float t0 = (float)i / FADE_SEGS;
        float t1 = (float)(i + 1) / FADE_SEGS;
        float sx = x0 + dx * t0, sy = y0 + dy * t0;
        float ex = x0 + dx * t1, ey = y0 + dy * t1;
        float u = 1.0f - t0;
        float alpha = a_start * u * u * (3.0f - 2.0f * u);
        if (alpha < 0.01f) break;

        if (mode == FADE_OUTLINE) {
            /* White outline strips — both sides */
            render_thick_line(sx + px * ol_off, sy + py * ol_off,
                              ex + px * ol_off, ey + py * ol_off,
                              OL_W, 1.0f, 1.0f, 1.0f, alpha);
            render_thick_line(sx - px * ol_off, sy - py * ol_off,
                              ex - px * ol_off, ey - py * ol_off,
                              OL_W, 1.0f, 1.0f, 1.0f, alpha);
        } else {
            /* Grey center fill — full width */
            render_thick_line(sx, sy, ex, ey,
                              SIDE_T, SD_R, SD_G, SD_B, alpha);
        }
    }
}

/* ================================================================
 * Icon drawing functions — 3 mask passes each
 * ================================================================ */

/* Angular distance between two angles in degrees (0..180). */
static float angle_dist(float a, float b) {
    float d = fabsf(a - b);
    if (d > 180.0f) d = 360.0f - d;
    return d;
}

/* Helper: check if angle (degrees) overlaps forward (0°) or entry (180°). */
static int angle_near_fixed(float deg, float eps) {
    float ad = fabsf(deg);
    return (ad < eps || fabsf(ad - 180.0f) < eps);
}

/* Helper: check if angle overlaps forward, entry, or any angle in a list. */
static int angle_near_any(float deg, const int *angles, int count, float eps) {
    if (angle_near_fixed(deg, eps)) return 1;
    int i;
    for (i = 0; i < count; i++) {
        if (angle_dist(deg, (float)angles[i]) < eps) return 1;
    }
    return 0;
}

/* ----------------------------------------------------------------
 * draw_straight — outline/fill/route passes
 * ---------------------------------------------------------------- */

static void draw_straight_outline(const int *side_angles, int side_count) {
    int i;
    draw_fading_road(0, SHAFT_BOT, 0, SHAFT_BOT - FADE_LEN, 1.0f, FADE_OUTLINE);
    draw_fading_road(0, SIDE_TOP, 0, SIDE_TOP + FADE_LEN, 1.0f, FADE_OUTLINE);
    for (i = 0; i < side_count && i < MAX_JUNCTION_ANGLES; i++) {
        if (angle_near_fixed((float)side_angles[i], ANGLE_DEDUP)) continue;
        float rad = (float)side_angles[i] * (float)M_PI / 180.0f;
        float sx = ROAD_LEN * sinf(rad), sy = ROAD_LEN * cosf(rad);
        float fx = (ROAD_LEN + FADE_LEN) * sinf(rad);
        float fy = (ROAD_LEN + FADE_LEN) * cosf(rad);
        draw_fading_road(sx, sy, fx, fy, 1.0f, FADE_OUTLINE);
    }
    render_thick_line(0, SHAFT_BOT, 0, SIDE_TOP, OL_T, WHITE);
    for (i = 0; i < side_count && i < MAX_JUNCTION_ANGLES; i++) {
        if (angle_near_fixed((float)side_angles[i], ANGLE_DEDUP)) continue;
        float rad = (float)side_angles[i] * (float)M_PI / 180.0f;
        float sx = ROAD_LEN * sinf(rad), sy = ROAD_LEN * cosf(rad);
        render_thick_line(0, 0, sx, sy, OL_T, WHITE);
    }
    if (side_count > 0)
        render_disc(0, 0, OL_T * 0.5f, JOINT_SEG, WHITE);
}

static void draw_straight_fill(const int *side_angles, int side_count) {
    int i;
    draw_fading_road(0, SHAFT_BOT, 0, SHAFT_BOT - FADE_LEN, 1.0f, FADE_GREY);
    draw_fading_road(0, SIDE_TOP, 0, SIDE_TOP + FADE_LEN, 1.0f, FADE_GREY);
    for (i = 0; i < side_count && i < MAX_JUNCTION_ANGLES; i++) {
        if (angle_near_fixed((float)side_angles[i], ANGLE_DEDUP)) continue;
        float rad = (float)side_angles[i] * (float)M_PI / 180.0f;
        float sx = ROAD_LEN * sinf(rad), sy = ROAD_LEN * cosf(rad);
        float fx = (ROAD_LEN + FADE_LEN) * sinf(rad);
        float fy = (ROAD_LEN + FADE_LEN) * cosf(rad);
        draw_fading_road(sx, sy, fx, fy, 1.0f, FADE_GREY);
    }
    render_thick_line(0, SHAFT_BOT, 0, SIDE_TOP, SIDE_T, SIDE);
    for (i = 0; i < side_count && i < MAX_JUNCTION_ANGLES; i++) {
        if (angle_near_fixed((float)side_angles[i], ANGLE_DEDUP)) continue;
        float rad = (float)side_angles[i] * (float)M_PI / 180.0f;
        float sx = ROAD_LEN * sinf(rad), sy = ROAD_LEN * cosf(rad);
        render_thick_line(0, 0, sx, sy, SIDE_T, SIDE);
    }
    if (side_count > 0)
        render_disc(0, 0, SIDE_T * 0.5f, JOINT_SEG, SIDE);
}

static void draw_straight(const int *side_angles, int side_count) {
    render_set_raised(0);

    render_begin_outline_mask();
    draw_straight_outline(side_angles, side_count);
    render_end_outline_mask();

    render_begin_fill_mask();
    draw_straight_fill(side_angles, side_count);
    render_end_fill_mask();

    /* Build route path */
    rpath_clear(&g_route_path);
    rpath_add_line(&g_route_path, 0, SHAFT_BOT, 0, SHAFT_TOP);
    rpath_densify(&g_route_path);
    rpath_set_arrow(&g_route_path, 0, SHAFT_TOP, (float)(M_PI * 0.5));
    rpath_extrude(&g_route_path, &g_route_mesh, SHAFT_T, ROUTE_BASE_Y, ROUTE_TOP_Y, 1.0f);

    render_composite();
    rpath_draw(&g_route_mesh, AC_R, AC_G, AC_B, AC_A);
}

/* ----------------------------------------------------------------
 * draw_turn — outline/fill/route passes
 * ---------------------------------------------------------------- */

static void draw_turn_outline(float angle_deg, float angle_rad,
                               const int *side_angles, int side_count,
                               int active_has_own_stub,
                               float stub_x, float stub_y,
                               float fade_x, float fade_y) {
    int i;
    draw_fading_road(0, SIDE_TOP, 0, SIDE_TOP + FADE_LEN, 1.0f, FADE_OUTLINE);
    draw_fading_road(0, SHAFT_BOT, 0, SHAFT_BOT - FADE_LEN, 1.0f, FADE_OUTLINE);
    if (active_has_own_stub)
        draw_fading_road(stub_x, stub_y, fade_x, fade_y, 1.0f, FADE_OUTLINE);
    for (i = 0; i < side_count && i < MAX_JUNCTION_ANGLES; i++) {
        if (angle_near_fixed((float)side_angles[i], ANGLE_DEDUP)) continue;
        float rad = (float)side_angles[i] * (float)M_PI / 180.0f;
        float sx = ROAD_LEN * sinf(rad), sy = ROAD_LEN * cosf(rad);
        float fx = (ROAD_LEN + FADE_LEN) * sinf(rad);
        float fy = (ROAD_LEN + FADE_LEN) * cosf(rad);
        draw_fading_road(sx, sy, fx, fy, 1.0f, FADE_OUTLINE);
    }
    render_thick_line(0, 0, 0, SIDE_TOP, OL_T, WHITE);
    render_thick_line(0, SHAFT_BOT, 0, 0, OL_T, WHITE);
    if (active_has_own_stub)
        render_thick_line(0, 0, stub_x, stub_y, OL_T, WHITE);
    for (i = 0; i < side_count && i < MAX_JUNCTION_ANGLES; i++) {
        if (angle_near_fixed((float)side_angles[i], ANGLE_DEDUP)) continue;
        float rad = (float)side_angles[i] * (float)M_PI / 180.0f;
        float sx = ROAD_LEN * sinf(rad), sy = ROAD_LEN * cosf(rad);
        render_thick_line(0, 0, sx, sy, OL_T, WHITE);
    }
    render_disc(0, 0, OL_T * 0.5f, JOINT_SEG, WHITE);
    (void)angle_deg; (void)angle_rad;
}

static void draw_turn_fill(float angle_deg, float angle_rad,
                             const int *side_angles, int side_count,
                             int active_has_own_stub,
                             float stub_x, float stub_y,
                             float fade_x, float fade_y) {
    int i;
    draw_fading_road(0, SIDE_TOP, 0, SIDE_TOP + FADE_LEN, 1.0f, FADE_GREY);
    draw_fading_road(0, SHAFT_BOT, 0, SHAFT_BOT - FADE_LEN, 1.0f, FADE_GREY);
    if (active_has_own_stub)
        draw_fading_road(stub_x, stub_y, fade_x, fade_y, 1.0f, FADE_GREY);
    for (i = 0; i < side_count && i < MAX_JUNCTION_ANGLES; i++) {
        if (angle_near_fixed((float)side_angles[i], ANGLE_DEDUP)) continue;
        float rad = (float)side_angles[i] * (float)M_PI / 180.0f;
        float sx = ROAD_LEN * sinf(rad), sy = ROAD_LEN * cosf(rad);
        float fx = (ROAD_LEN + FADE_LEN) * sinf(rad);
        float fy = (ROAD_LEN + FADE_LEN) * cosf(rad);
        draw_fading_road(sx, sy, fx, fy, 1.0f, FADE_GREY);
    }
    render_thick_line(0, 0, 0, SIDE_TOP, SIDE_T, SIDE);
    render_thick_line(0, SHAFT_BOT, 0, 0, SIDE_T, SIDE);
    if (active_has_own_stub)
        render_thick_line(0, 0, stub_x, stub_y, SIDE_T, SIDE);
    for (i = 0; i < side_count && i < MAX_JUNCTION_ANGLES; i++) {
        if (angle_near_fixed((float)side_angles[i], ANGLE_DEDUP)) continue;
        float rad = (float)side_angles[i] * (float)M_PI / 180.0f;
        float sx = ROAD_LEN * sinf(rad), sy = ROAD_LEN * cosf(rad);
        render_thick_line(0, 0, sx, sy, SIDE_T, SIDE);
    }
    render_disc(0, 0, SIDE_T * 0.5f, JOINT_SEG, SIDE);
    (void)angle_deg; (void)angle_rad;
}

static void draw_turn(float angle_deg, const int *side_angles, int side_count) {
    float angle_rad = angle_deg * (float)M_PI / 180.0f;
    float end_x = BLUE_LEN * sinf(angle_rad);
    float end_y = BLUE_LEN * cosf(angle_rad);
    float stub_x = ROAD_LEN * sinf(angle_rad);
    float stub_y = ROAD_LEN * cosf(angle_rad);
    float fade_x = (ROAD_LEN + FADE_LEN) * sinf(angle_rad);
    float fade_y = (ROAD_LEN + FADE_LEN) * cosf(angle_rad);
    int active_has_own_stub = !angle_near_any(angle_deg, side_angles, side_count, ANGLE_DEDUP);

    render_set_raised(0);

    render_begin_outline_mask();
    draw_turn_outline(angle_deg, angle_rad, side_angles, side_count,
                      active_has_own_stub, stub_x, stub_y, fade_x, fade_y);
    render_end_outline_mask();

    render_begin_fill_mask();
    draw_turn_fill(angle_deg, angle_rad, side_angles, side_count,
                   active_has_own_stub, stub_x, stub_y, fade_x, fade_y);
    render_end_fill_mask();

    /* Build route path */
    rpath_clear(&g_route_path);
    rpath_add_line(&g_route_path, 0, SHAFT_BOT, 0, 0);
    rpath_add_line(&g_route_path, 0, 0, end_x, end_y);
    rpath_densify(&g_route_path);
    float head_angle = (float)(M_PI * 0.5) - angle_rad;
    rpath_set_arrow(&g_route_path, end_x, end_y, head_angle);
    rpath_extrude(&g_route_path, &g_route_mesh, SHAFT_T, ROUTE_BASE_Y, ROUTE_TOP_Y, 1.0f);

    render_composite();
    rpath_draw(&g_route_mesh, AC_R, AC_G, AC_B, AC_A);
}

/* ----------------------------------------------------------------
 * draw_uturn — outline/fill/route passes
 * ---------------------------------------------------------------- */

static void draw_uturn_outline(float enter_x, float exit_x, float top_y) {
    draw_fading_road(enter_x, SHAFT_BOT, enter_x, SHAFT_BOT - FADE_LEN, 1.0f, FADE_OUTLINE);
    draw_fading_road(exit_x, SHAFT_BOT, exit_x, SHAFT_BOT - FADE_LEN, 1.0f, FADE_OUTLINE);
    render_thick_line(enter_x, SHAFT_BOT, enter_x, top_y, OL_T, WHITE);
    render_arc(0, top_y, UTURN_GAP, OL_T, 0, (float)M_PI, UTURN_SEG, WHITE);
    render_thick_line(exit_x, top_y, exit_x, SHAFT_BOT, OL_T, WHITE);
}

static void draw_uturn_fill(float enter_x, float exit_x, float top_y) {
    draw_fading_road(enter_x, SHAFT_BOT, enter_x, SHAFT_BOT - FADE_LEN, 1.0f, FADE_GREY);
    draw_fading_road(exit_x, SHAFT_BOT, exit_x, SHAFT_BOT - FADE_LEN, 1.0f, FADE_GREY);
    render_thick_line(enter_x, SHAFT_BOT, enter_x, top_y, SIDE_T, SIDE);
    render_arc(0, top_y, UTURN_GAP, SIDE_T, 0, (float)M_PI, UTURN_SEG, SIDE);
    render_thick_line(exit_x, top_y, exit_x, SHAFT_BOT, SIDE_T, SIDE);
    render_disc(exit_x, top_y, SIDE_T * 0.5f, JOINT_SEG, SIDE);
    render_disc(enter_x, top_y, SIDE_T * 0.5f, JOINT_SEG, SIDE);
}

static void draw_uturn(int go_left) {
    float enter_x = go_left ?  UTURN_GAP : -UTURN_GAP;
    float exit_x  = go_left ? -UTURN_GAP :  UTURN_GAP;
    float top_y   = UTURN_TOP;
    float arrow_y = UTURN_ARROW;

    render_set_raised(0);

    render_begin_outline_mask();
    draw_uturn_outline(enter_x, exit_x, top_y);
    render_end_outline_mask();

    render_begin_fill_mask();
    draw_uturn_fill(enter_x, exit_x, top_y);
    render_end_fill_mask();

    /* Build route path: entry shaft → U-arc → exit shaft */
    rpath_clear(&g_route_path);
    rpath_add_line(&g_route_path, enter_x, SHAFT_BOT, enter_x, top_y);
    /* Arc from entry side to exit side over the top.
     * Arc center at (0, top_y), radius = UTURN_GAP.
     * go_left: enter is right (+x), exit is left (-x) → arc from 0 to PI
     * go_right: enter is left (-x), exit is right (+x) → arc from PI to 0 (= PI to 2PI mapped) */
    if (go_left) {
        rpath_add_arc(&g_route_path, 0, top_y, UTURN_GAP, 0, (float)M_PI);
    } else {
        rpath_add_arc(&g_route_path, 0, top_y, UTURN_GAP, (float)M_PI, 2.0f * (float)M_PI);
    }
    rpath_add_line(&g_route_path, exit_x, top_y, exit_x, arrow_y);
    rpath_densify(&g_route_path);
    rpath_set_arrow(&g_route_path, exit_x, arrow_y, (float)(-M_PI * 0.5));
    rpath_extrude(&g_route_path, &g_route_mesh, SHAFT_T, ROUTE_BASE_Y, ROUTE_TOP_Y, 1.0f);

    render_composite();
    rpath_draw(&g_route_mesh, AC_R, AC_G, AC_B, AC_A);
}

/* ----------------------------------------------------------------
 * draw_roundabout — outline/fill/route passes
 * ---------------------------------------------------------------- */

static void draw_roundabout(float exit_angle_deg, int driving_side,
                            const int *junction_angles, int junction_angle_count) {
    float cx = 0.0f, cy = 0.0f;
    float ring_r = RAB_RING_R;
    float ring_ol = OL_T;
    float ring_sd = SIDE_T;

    float entry_rad = (float)(-M_PI * 0.5);

    /* Snap exit_angle to nearest junction angle OR entry */
    float entry_deg = -180.0f;
    float snapped_exit_deg = exit_angle_deg;
    int snapped_idx = -1;
    if (junction_angle_count > 0) {
        int best = 0;
        float best_diff = SNAP_SENTINEL;
        int i_s;
        for (i_s = 0; i_s < junction_angle_count && i_s < MAX_JUNCTION_ANGLES; i_s++) {
            float diff = fabsf((float)junction_angles[i_s] - exit_angle_deg);
            if (diff > 180.0f) diff = 360.0f - diff;
            if (diff < best_diff) { best_diff = diff; best = i_s; }
        }
        float entry_diff = fabsf(entry_deg - exit_angle_deg);
        if (entry_diff > 180.0f) entry_diff = 360.0f - entry_diff;
        if (entry_diff < best_diff) {
            snapped_exit_deg = entry_deg;
            snapped_idx = -2;
        } else {
            snapped_exit_deg = (float)junction_angles[best];
            snapped_idx = best;
        }
    }
    float exit_rad = (90.0f - snapped_exit_deg) * (float)M_PI / 180.0f;

    float ext = BLUE_LEN - ring_r;
    float stub = ROAD_LEN - ring_r;
    float stub_fade = FADE_LEN;

    float entry_top = cy - ring_r;
    float ex0_x = cx + ring_r * cosf(exit_rad);
    float ex0_y = cy + ring_r * sinf(exit_rad);
    float ex_tip_x = cx + (ring_r + ext) * cosf(exit_rad);
    float ex_tip_y = cy + (ring_r + ext) * sinf(exit_rad);
    float ex_ol_x = cx + (ring_r + stub) * cosf(exit_rad);
    float ex_ol_y = cy + (ring_r + stub) * sinf(exit_rad);

    /* Convert junction angles to math radians, skip active exit */
    float side_rads[MAX_JUNCTION_ANGLES];
    int n_sides = 0;
    int i;
    for (i = 0; i < junction_angle_count && i < MAX_JUNCTION_ANGLES; i++) {
        if (i == snapped_idx) continue;
        side_rads[n_sides++] = (90.0f - (float)junction_angles[i]) * (float)M_PI / 180.0f;
    }

    /* Track drawn stub angles to avoid double-drawing overlaps */
    float drawn_rads[MAX_JUNCTION_ANGLES + 2];
    int n_drawn = 0;
    drawn_rads[n_drawn++] = entry_rad;

    int exit_dup = 0;
    {
        float ed = fabsf(exit_rad - entry_rad);
        if (ed > (float)M_PI) ed = 2.0f * (float)M_PI - ed;
        if (ed < ANGLE_DEDUP_RAD) exit_dup = 1;
    }
    if (!exit_dup) drawn_rads[n_drawn++] = exit_rad;

    int side_skip[MAX_JUNCTION_ANGLES];
    for (i = 0; i < n_sides; i++) {
        float sr = side_rads[i];
        int dup = 0, j;
        for (j = 0; j < n_drawn; j++) {
            float d = fabsf(sr - drawn_rads[j]);
            if (d > (float)M_PI) d = 2.0f * (float)M_PI - d;
            if (d < ANGLE_DEDUP_RAD) { dup = 1; break; }
        }
        side_skip[i] = dup;
        if (!dup) drawn_rads[n_drawn++] = sr;
    }

    render_set_raised(0);

    /* === OUTLINE MASK === */
    render_begin_outline_mask();
    draw_fading_road(0, SHAFT_BOT, 0, SHAFT_BOT - FADE_LEN, 1.0f, FADE_OUTLINE);
    if (!exit_dup) {
        float ef_x0 = cx + (ring_r + stub) * cosf(exit_rad);
        float ef_y0 = cy + (ring_r + stub) * sinf(exit_rad);
        float ef_x1 = cx + (ring_r + stub + stub_fade) * cosf(exit_rad);
        float ef_y1 = cy + (ring_r + stub + stub_fade) * sinf(exit_rad);
        draw_fading_road(ef_x0, ef_y0, ef_x1, ef_y1, 1.0f, FADE_OUTLINE);
    }
    for (i = 0; i < n_sides; i++) {
        if (side_skip[i]) continue;
        float sr = side_rads[i];
        float sx1 = cx + (ring_r + stub) * cosf(sr);
        float sy1 = cy + (ring_r + stub) * sinf(sr);
        float sx2 = cx + (ring_r + stub + stub_fade) * cosf(sr);
        float sy2 = cy + (ring_r + stub + stub_fade) * sinf(sr);
        draw_fading_road(sx1, sy1, sx2, sy2, 1.0f, FADE_OUTLINE);
    }
    render_rect(-ring_ol * 0.5f, SHAFT_BOT, ring_ol, entry_top - SHAFT_BOT, WHITE);
    if (!exit_dup)
        render_thick_line(ex0_x, ex0_y, ex_ol_x, ex_ol_y, ring_ol, WHITE);
    for (i = 0; i < n_sides; i++) {
        if (side_skip[i]) continue;
        float sr = side_rads[i];
        float sx0 = cx + ring_r * cosf(sr);
        float sy0 = cy + ring_r * sinf(sr);
        float sx1 = cx + (ring_r + stub) * cosf(sr);
        float sy1 = cy + (ring_r + stub) * sinf(sr);
        render_thick_line(sx0, sy0, sx1, sy1, ring_ol, WHITE);
    }
    render_circle(cx, cy, ring_r, ring_ol, RAB_RING_SEG, WHITE);
    render_end_outline_mask();

    /* === FILL MASK === */
    render_begin_fill_mask();
    draw_fading_road(0, SHAFT_BOT, 0, SHAFT_BOT - FADE_LEN, 1.0f, FADE_GREY);
    if (!exit_dup) {
        float ef_x0 = cx + (ring_r + stub) * cosf(exit_rad);
        float ef_y0 = cy + (ring_r + stub) * sinf(exit_rad);
        float ef_x1 = cx + (ring_r + stub + stub_fade) * cosf(exit_rad);
        float ef_y1 = cy + (ring_r + stub + stub_fade) * sinf(exit_rad);
        draw_fading_road(ef_x0, ef_y0, ef_x1, ef_y1, 1.0f, FADE_GREY);
    }
    for (i = 0; i < n_sides; i++) {
        if (side_skip[i]) continue;
        float sr = side_rads[i];
        float sx1 = cx + (ring_r + stub) * cosf(sr);
        float sy1 = cy + (ring_r + stub) * sinf(sr);
        float sx2 = cx + (ring_r + stub + stub_fade) * cosf(sr);
        float sy2 = cy + (ring_r + stub + stub_fade) * sinf(sr);
        draw_fading_road(sx1, sy1, sx2, sy2, 1.0f, FADE_GREY);
    }
    render_rect(-ring_sd * 0.5f, SHAFT_BOT, ring_sd, entry_top - SHAFT_BOT, SIDE);
    if (!exit_dup)
        render_thick_line(ex0_x, ex0_y, ex_ol_x, ex_ol_y, ring_sd, SIDE);
    for (i = 0; i < n_sides; i++) {
        if (side_skip[i]) continue;
        float sr = side_rads[i];
        float sx0 = cx + ring_r * cosf(sr);
        float sy0 = cy + ring_r * sinf(sr);
        float sx1 = cx + (ring_r + stub) * cosf(sr);
        float sy1 = cy + (ring_r + stub) * sinf(sr);
        render_thick_line(sx0, sy0, sx1, sy1, ring_sd, SIDE);
    }
    render_circle(cx, cy, ring_r, ring_sd, RAB_RING_SEG, SIDE);
    render_end_fill_mask();

    /* === ROUTE PATH (3D mesh) === */
    {
        float arc_s, arc_e;
        if (driving_side == 0) {
            /* RHT: CCW from entry to exit */
            arc_s = entry_rad;
            arc_e = exit_rad;
            while (arc_e <= arc_s) arc_e += 2.0f * (float)M_PI;
        } else {
            /* LHT: CW from entry to exit */
            arc_s = entry_rad;
            arc_e = exit_rad;
            while (arc_e >= arc_s) arc_e -= 2.0f * (float)M_PI;
        }

        rpath_clear(&g_route_path);
        /* Entry shaft: bottom to ring entry point */
        rpath_add_line(&g_route_path, 0, SHAFT_BOT, 0, entry_top);
        /* Ring arc */
        rpath_add_arc(&g_route_path, cx, cy, ring_r, arc_s, arc_e);
        /* Exit stub: ring exit point to arrow tip */
        rpath_add_line(&g_route_path, ex0_x, ex0_y, ex_tip_x, ex_tip_y);
        rpath_densify(&g_route_path);
        rpath_set_arrow(&g_route_path, ex_tip_x, ex_tip_y, exit_rad);
        rpath_extrude(&g_route_path, &g_route_mesh, SHAFT_T, ROUTE_BASE_Y, ROUTE_TOP_Y, 1.0f);
    }

    render_composite();
    rpath_draw(&g_route_mesh, AC_R, AC_G, AC_B, AC_A);
}

/* ----------------------------------------------------------------
 * draw_flag — flag on a pole (used by arrived)
 * ---------------------------------------------------------------- */

static void draw_flag_outline(float bx, float by, float pole_h) {
    float fy = by + pole_h;
    float cw = FLAG_W / 3.0f;
    float ch = FLAG_H / 2.0f;
    /* pole outline */
    render_thick_line(bx, by, bx, fy, FLAG_POLE_T + OL_W * 2, WHITE);
    /* flag outline — a rect around the whole flag */
    render_rect(bx - OL_W, fy - FLAG_H - OL_W, FLAG_W + OL_W * 2, FLAG_H + OL_W * 2, WHITE);
    (void)cw; (void)ch;
}

static void draw_flag_fill(float bx, float by, float pole_h) {
    float fy = by + pole_h;
    float cw = FLAG_W / 3.0f;
    float ch = FLAG_H / 2.0f;
    /* pole */
    render_thick_line(bx, by, bx, fy, FLAG_POLE_T, SIDE);
    /* base disc */
    render_disc(bx, by, FLAG_BASE_R, JOINT_SEG, SIDE);
    /* 3×2 checkerboard */
    int row, col;
    for (row = 0; row < 2; row++) {
        for (col = 0; col < 3; col++) {
            float rx = bx + col * cw;
            float ry = fy - row * ch;
            if ((row + col) % 2 == 0)
                render_rect(rx, ry - ch, cw, ch, SIDE);
            else
                render_rect(rx, ry - ch, cw, ch, CHECK_R, CHECK_G, CHECK_B, 1.0f);
        }
    }
}

static void draw_flag_route(float bx, float by, float pole_h) {
    float fy = by + pole_h;
    float cw = FLAG_W / 3.0f;
    float ch = FLAG_H / 2.0f;
    /* base disc */
    render_disc(bx, by, FLAG_BASE_R, JOINT_SEG, ACTIVE);
    /* 3×2 checkerboard */
    int row, col;
    for (row = 0; row < 2; row++) {
        for (col = 0; col < 3; col++) {
            float rx = bx + col * cw;
            float ry = fy - row * ch;
            if ((row + col) % 2 == 0)
                render_rect(rx, ry - ch, cw, ch, ACTIVE);
            else
                render_rect(rx, ry - ch, cw, ch, CHECK_R, CHECK_G, CHECK_B, 1.0f);
        }
    }
}

/* ----------------------------------------------------------------
 * draw_arrived — outline/fill/route passes
 * ---------------------------------------------------------------- */

static void draw_arrived(int dir) {
    float road_top = ARRIVE_ROAD_TOP;
    float dome_r = DOME_R;

    render_set_raised(0);

    if (dir == 0) {
        /* --- Center / offroad --- */
        float ring_y = road_top + HEAD_SZ + ARRIVE_RING_GAP;
        float ring_r = ARRIVE_RING_R;

        render_begin_outline_mask();
        draw_fading_road(0, SHAFT_BOT, 0, SHAFT_BOT - FADE_LEN, 1.0f, FADE_OUTLINE);
        render_thick_line(0, SHAFT_BOT, 0, road_top, OL_T, WHITE);
        render_circle(0, ring_y, ring_r, ARRIVE_RING_T + OL_W * 2, ARRIVE_SEG, WHITE);
        draw_flag_outline(0, ring_y, ARRIVE_FLAG_H_CTR);
        render_end_outline_mask();

        render_begin_fill_mask();
        draw_fading_road(0, SHAFT_BOT, 0, SHAFT_BOT - FADE_LEN, 1.0f, FADE_GREY);
        render_thick_line(0, SHAFT_BOT, 0, road_top, SIDE_T, SIDE);
        draw_flag_fill(0, ring_y, ARRIVE_FLAG_H_CTR);
        render_end_fill_mask();

        /* Route path */
        rpath_clear(&g_route_path);
        rpath_add_line(&g_route_path, 0, SHAFT_BOT, 0, road_top);
        rpath_densify(&g_route_path);
        rpath_set_arrow(&g_route_path, 0, road_top, (float)(M_PI * 0.5));
        rpath_extrude(&g_route_path, &g_route_mesh, SHAFT_T, ROUTE_BASE_Y, ROUTE_TOP_Y, 1.0f);

        render_composite();
        rpath_draw(&g_route_mesh, AC_R, AC_G, AC_B, AC_A);
        /* Arrived extras: ring + flag drawn as regular 3D primitives */
        render_circle(0, ring_y, ring_r, ARRIVE_RING_T, ARRIVE_SEG, ACTIVE);
        draw_flag_route(0, ring_y, ARRIVE_FLAG_H_CTR);
    } else {
        /* --- Left / Right --- */
        float sign = (dir < 0) ? -1.0f : 1.0f;
        float flag_x = sign * ARRIVE_FLAG_X;

        render_begin_outline_mask();
        draw_fading_road(0, SHAFT_BOT, 0, SHAFT_BOT - FADE_LEN, 1.0f, FADE_OUTLINE);
        render_thick_line(0, SHAFT_BOT, 0, road_top, OL_T, WHITE);
        render_disc(0, road_top, dome_r + OL_W, ARRIVE_SEG, WHITE);
        draw_flag_outline(flag_x, road_top - ARRIVE_FLAG_Y_OFF, ARRIVE_FLAG_H_SIDE);
        render_end_outline_mask();

        render_begin_fill_mask();
        draw_fading_road(0, SHAFT_BOT, 0, SHAFT_BOT - FADE_LEN, 1.0f, FADE_GREY);
        render_thick_line(0, SHAFT_BOT, 0, road_top, SIDE_T, SIDE);
        render_disc(0, road_top, dome_r + OL_W * 0.5f, ARRIVE_SEG, SIDE);
        draw_flag_fill(flag_x, road_top - ARRIVE_FLAG_Y_OFF, ARRIVE_FLAG_H_SIDE);
        render_end_fill_mask();

        /* Route path */
        rpath_clear(&g_route_path);
        rpath_add_line(&g_route_path, 0, SHAFT_BOT, 0, road_top);
        rpath_densify(&g_route_path);
        rpath_set_arrow(&g_route_path, 0, road_top, (float)(M_PI * 0.5));
        rpath_extrude(&g_route_path, &g_route_mesh, SHAFT_T, ROUTE_BASE_Y, ROUTE_TOP_Y, 1.0f);

        render_composite();
        rpath_draw(&g_route_mesh, AC_R, AC_G, AC_B, AC_A);
        /* Arrived extras: dome + flag drawn as regular 3D primitives */
        render_disc(0, road_top, dome_r, ARRIVE_SEG, ACTIVE);
        draw_flag_route(flag_x, road_top - ARRIVE_FLAG_Y_OFF, ARRIVE_FLAG_H_SIDE);
    }
}

/* ----------------------------------------------------------------
 * draw_lane_change — outline/fill/route passes
 * ---------------------------------------------------------------- */

static void draw_lane_change(int go_left) {
    float sign = go_left ? -1.0f : 1.0f;
    float shift = sign * LANE_SHIFT;
    float bend_lo = BEND_LO;
    float bend_hi = BEND_HI;

    render_set_raised(0);

    render_begin_outline_mask();
    draw_fading_road(0, SHAFT_BOT, 0, SHAFT_BOT - FADE_LEN, 1.0f, FADE_OUTLINE);
    draw_fading_road(0, SIDE_TOP, 0, SIDE_TOP + FADE_LEN, 1.0f, FADE_OUTLINE);
    draw_fading_road(shift, SIDE_TOP, shift, SIDE_TOP + FADE_LEN, 1.0f, FADE_OUTLINE);
    render_thick_line(0, SHAFT_BOT, 0, SIDE_TOP, OL_T, WHITE);
    render_thick_line(0, SHAFT_BOT, 0, bend_lo, OL_T, WHITE);
    render_thick_line(0, bend_lo, shift, bend_hi, OL_T, WHITE);
    render_disc(0, bend_lo, OL_T * 0.5f, JOINT_SEG, WHITE);
    render_disc(shift, bend_hi, OL_T * 0.5f, JOINT_SEG, WHITE);
    render_thick_line(shift, bend_hi, shift, SIDE_TOP, OL_T, WHITE);
    render_end_outline_mask();

    render_begin_fill_mask();
    draw_fading_road(0, SHAFT_BOT, 0, SHAFT_BOT - FADE_LEN, 1.0f, FADE_GREY);
    draw_fading_road(0, SIDE_TOP, 0, SIDE_TOP + FADE_LEN, 1.0f, FADE_GREY);
    draw_fading_road(shift, SIDE_TOP, shift, SIDE_TOP + FADE_LEN, 1.0f, FADE_GREY);
    render_thick_line(0, SHAFT_BOT, 0, SIDE_TOP, SIDE_T, SIDE);
    render_thick_line(0, SHAFT_BOT, 0, bend_lo, SIDE_T, SIDE);
    render_thick_line(0, bend_lo, shift, bend_hi, SIDE_T, SIDE);
    render_disc(0, bend_lo, SIDE_T * 0.5f, JOINT_SEG, SIDE);
    render_disc(shift, bend_hi, SIDE_T * 0.5f, JOINT_SEG, SIDE);
    render_thick_line(shift, bend_hi, shift, SIDE_TOP, SIDE_T, SIDE);
    render_end_fill_mask();

    /* Build route path */
    rpath_clear(&g_route_path);
    rpath_add_line(&g_route_path, 0, SHAFT_BOT, 0, bend_lo);
    rpath_add_line(&g_route_path, 0, bend_lo, shift, bend_hi);
    rpath_add_line(&g_route_path, shift, bend_hi, shift, BLUE_LEN);
    rpath_densify(&g_route_path);
    rpath_set_arrow(&g_route_path, shift, BLUE_LEN, (float)(M_PI * 0.5));
    rpath_extrude(&g_route_path, &g_route_mesh, SHAFT_T, ROUTE_BASE_Y, ROUTE_TOP_Y, 1.0f);

    render_composite();
    rpath_draw(&g_route_mesh, AC_R, AC_G, AC_B, AC_A);
}

/* ----------------------------------------------------------------
 * draw_merge — outline/fill/route passes
 * ---------------------------------------------------------------- */

static void draw_merge(int go_right) {
    float sign = go_right ? 1.0f : -1.0f;
    float start_x = sign * -MERGE_OFFSET;
    float bend_lo = MERGE_BEND_LO;
    float bend_hi = BEND_HI;

    render_set_raised(0);

    render_begin_outline_mask();
    draw_fading_road(0, SHAFT_BOT, 0, SHAFT_BOT - FADE_LEN, 1.0f, FADE_OUTLINE);
    draw_fading_road(0, SIDE_TOP, 0, SIDE_TOP + FADE_LEN, 1.0f, FADE_OUTLINE);
    draw_fading_road(start_x, SHAFT_BOT, start_x, SHAFT_BOT - FADE_LEN, 1.0f, FADE_OUTLINE);
    render_thick_line(0, SHAFT_BOT, 0, SIDE_TOP, OL_T, WHITE);
    render_thick_line(start_x, SHAFT_BOT, start_x, bend_lo, OL_T, WHITE);
    render_thick_line(start_x, bend_lo, 0, bend_hi, OL_T, WHITE);
    render_disc(start_x, bend_lo, OL_T * 0.5f, JOINT_SEG, WHITE);
    render_disc(0, bend_hi, OL_T * 0.5f, JOINT_SEG, WHITE);
    render_thick_line(0, bend_hi, 0, SHAFT_TOP, OL_T, WHITE);
    render_end_outline_mask();

    render_begin_fill_mask();
    draw_fading_road(0, SHAFT_BOT, 0, SHAFT_BOT - FADE_LEN, 1.0f, FADE_GREY);
    draw_fading_road(0, SIDE_TOP, 0, SIDE_TOP + FADE_LEN, 1.0f, FADE_GREY);
    draw_fading_road(start_x, SHAFT_BOT, start_x, SHAFT_BOT - FADE_LEN, 1.0f, FADE_GREY);
    render_thick_line(0, SHAFT_BOT, 0, SIDE_TOP, SIDE_T, SIDE);
    render_thick_line(start_x, SHAFT_BOT, start_x, bend_lo, SIDE_T, SIDE);
    render_thick_line(start_x, bend_lo, 0, bend_hi, SIDE_T, SIDE);
    render_disc(start_x, bend_lo, SIDE_T * 0.5f, JOINT_SEG, SIDE);
    render_disc(0, bend_hi, SIDE_T * 0.5f, JOINT_SEG, SIDE);
    render_thick_line(0, bend_hi, 0, SHAFT_TOP, SIDE_T, SIDE);
    render_end_fill_mask();

    /* Build route path */
    rpath_clear(&g_route_path);
    rpath_add_line(&g_route_path, start_x, SHAFT_BOT, start_x, bend_lo);
    rpath_add_line(&g_route_path, start_x, bend_lo, 0, bend_hi);
    rpath_add_line(&g_route_path, 0, bend_hi, 0, BLUE_LEN);
    rpath_densify(&g_route_path);
    rpath_set_arrow(&g_route_path, 0, BLUE_LEN, (float)(M_PI * 0.5));
    rpath_extrude(&g_route_path, &g_route_mesh, SHAFT_T, ROUTE_BASE_Y, ROUTE_TOP_Y, 1.0f);

    render_composite();
    rpath_draw(&g_route_mesh, AC_R, AC_G, AC_B, AC_A);
}

/* ================================================================
 * Main dispatch
 * ================================================================ */

void maneuver_draw(const maneuver_state_t *s) {
    /* If masks are cached and still valid, just re-composite + redraw cached mesh
     * (handles perspective animation without re-rendering masks) */
    if (!render_masks_dirty()) {
        render_composite();
        rpath_draw(&g_route_mesh, AC_R, AC_G, AC_B, AC_A);
        return;
    }

    switch (s->icon) {
        case ICON_STRAIGHT:
            draw_straight(s->junction_angles, s->junction_angle_count);
            break;
        case ICON_TURN:
            draw_turn((float)s->exit_angle,
                      s->junction_angles, s->junction_angle_count);
            break;
        case ICON_UTURN:
            draw_uturn(s->driving_side != 1);
            break;
        case ICON_MERGE:
            draw_merge(s->direction > 0);
            break;
        case ICON_LANE_CHANGE:
            draw_lane_change(s->direction < 0);
            break;
        case ICON_ROUNDABOUT:
            draw_roundabout((float)s->exit_angle, s->driving_side,
                            s->junction_angles, s->junction_angle_count);
            break;
        case ICON_ARRIVED:
            draw_arrived(s->direction);
            break;
        default:
            break;
    }
    /* Each draw_* calls render_composite() which clears the dirty flag */
}

/* ================================================================
 * Debug names
 * ================================================================ */

const char *maneuver_icon_name(int icon) {
    switch (icon) {
        case ICON_NONE:        return "NONE";
        case ICON_STRAIGHT:    return "STRAIGHT";
        case ICON_TURN:        return "TURN";
        case ICON_UTURN:       return "UTURN";
        case ICON_MERGE:       return "MERGE";
        case ICON_LANE_CHANGE: return "LANE_CHG";
        case ICON_ROUNDABOUT:  return "ROUNDABOUT";
        case ICON_ARRIVED:     return "ARRIVED";
        default:               return "UNKNOWN";
    }
}
