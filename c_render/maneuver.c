/*
 * Maneuver icon rendering — procedural GL icons for all CarPlay maneuver types.
 *
 * Active color: #5AAAE6  (arrow you follow)
 * Side color:   #646464  (roads you don't take)
 *
 * Flat stubs (outlines, grey, fades) render to offscreen FBO with blending
 * disabled — overlapping grey overwrites instead of accumulating alpha.
 * FBO is then blitted to screen with alpha blending.
 * Blue active arm draws directly to screen on top.
 */

#include <stdio.h>
#include <math.h>
#include "render.h"
#include "maneuver.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ================================================================
 * Colors
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

/* Fade-to-solid overlap (eliminates 1px seam at boundary) */
#define FADE_OVERLAP 0.01f

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

/* Flag / arrived */
#define FLAG_POLE_T  OL_W             /* flag pole thickness */
#define FLAG_W       0.22f            /* flag width */
#define FLAG_H       0.16f            /* flag height */
#define FLAG_BASE_R  0.05f            /* flag base disc radius */
#define DOME_R       0.12f            /* arrived dome cap radius */
#define ARRIVE_RING_R 0.11f           /* arrived center ring radius */
#define ARRIVE_RING_T 0.04f           /* arrived ring thickness */

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
 * Split into outline-only and grey-only passes so all outlines
 * are drawn before any grey — prevents a second fade's outline
 * from showing through the first fade's grey in overlap zones.
 * ================================================================ */

#define FADE_SEGS    64
#define FADE_OUTLINE 0
#define FADE_GREY    1
#define FADE_BOTH    2  /* outline then grey — for non-FBO cases with no overlap */

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
 * Icon drawing functions
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

/*
 * Straight road with optional side streets from junction_angles.
 */
static void draw_straight(const int *side_angles, int side_count) {
    int i;

    /* Flat pass to FBO (no blend — overlapping grey overwrites) */
    render_begin_stubs();
    render_set_raised(0);

    /* === All outlines first (fade + solid) === */
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

    /* === All grey second (fade + solid) — overwrites outline interiors === */
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

    render_end_stubs();

    /* Blue active (direct to screen, raised) */
    render_set_raised(1);
    render_thick_line(0, SHAFT_BOT, 0, SHAFT_TOP, SHAFT_T, ACTIVE);
    render_arrowhead(0, SHAFT_TOP, (float)(M_PI * 0.5), HEAD_SZ, ACTIVE);
}

/*
 * Generic turn/fork icon.
 * angle_deg: active exit direction from straight-ahead (up):
 *   +30 = slight right, +90 = right, +135 = sharp right
 *   -30 = slight left,  -90 = left,  -135 = sharp left
 * side_angles: array of side street angles (same convention), may be empty.
 * side_count: number of side streets.
 */
static void draw_turn(float angle_deg, const int *side_angles, int side_count) {
    float angle_rad = angle_deg * (float)M_PI / 180.0f;
    int i;

    float end_x = BLUE_LEN * sinf(angle_rad);
    float end_y = BLUE_LEN * cosf(angle_rad);
    float stub_x = ROAD_LEN * sinf(angle_rad);
    float stub_y = ROAD_LEN * cosf(angle_rad);
    float fade_x = (ROAD_LEN + FADE_LEN) * sinf(angle_rad);
    float fade_y = (ROAD_LEN + FADE_LEN) * cosf(angle_rad);

    int active_has_own_stub = !angle_near_any(angle_deg, side_angles, side_count, ANGLE_DEDUP);

    /* Flat pass to FBO (no blend) */
    render_begin_stubs();
    render_set_raised(0);

    /* === All outlines first (fade + solid) === */
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

    /* === All grey second (fade + solid) — overwrites outline interiors === */
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

    render_end_stubs();

    /* Blue entry + turn (raised, direct to screen) */
    render_set_raised(1);
    render_thick_line(0, SHAFT_BOT, 0, 0, SHAFT_T, ACTIVE);
    render_thick_line(0, 0, end_x, end_y, SHAFT_T, ACTIVE);
    render_disc(0, 0, JOINT_R, JOINT_SEG, ACTIVE);
    float head_angle = (float)(M_PI * 0.5) - angle_rad;
    render_arrowhead(end_x, end_y, head_angle, HEAD_SZ, ACTIVE);
}

/*
 * U-turn icon.
 * go_left=1: shaft on right, curves left, arrow points down on left (RHT default).
 * go_left=0: mirror.
 */
static void draw_uturn(int go_left) {
    float enter_x = go_left ?  UTURN_GAP : -UTURN_GAP;
    float exit_x  = go_left ? -UTURN_GAP :  UTURN_GAP;
    float top_y   = UTURN_TOP;
    float arrow_y = UTURN_ARROW;

    /* Fades (flat) */
    render_set_raised(0);
    draw_fading_road(enter_x, SHAFT_BOT, enter_x, SHAFT_BOT - FADE_LEN, 1.0f, FADE_BOTH);
    draw_fading_road(exit_x, SHAFT_BOT, exit_x, SHAFT_BOT - FADE_LEN, 1.0f, FADE_BOTH);

    /* L1: white outline (flat) */
    render_thick_line(enter_x, SHAFT_BOT, enter_x, top_y, OL_T, WHITE);
    render_arc(0, top_y, UTURN_GAP, OL_T, 0, (float)M_PI, UTURN_SEG, WHITE);
    render_thick_line(exit_x, top_y, exit_x, SHAFT_BOT, OL_T, WHITE);

    /* L2: grey under active (flat) */
    render_thick_line(enter_x, SHAFT_BOT, enter_x, top_y, SIDE_T, SIDE);
    render_arc(0, top_y, UTURN_GAP, SIDE_T, 0, (float)M_PI, UTURN_SEG, SIDE);
    render_thick_line(exit_x, top_y, exit_x, SHAFT_BOT, SIDE_T, SIDE);
    render_disc(exit_x, top_y, SIDE_T * 0.5f, JOINT_SEG, SIDE);
    render_disc(enter_x, top_y, SIDE_T * 0.5f, JOINT_SEG, SIDE);

    /* L3: blue fill (raised) */
    render_set_raised(1);
    render_thick_line(enter_x, SHAFT_BOT, enter_x, top_y, SHAFT_T, ACTIVE);
    render_arc(0, top_y, UTURN_GAP, SHAFT_T, 0, (float)M_PI, UTURN_SEG, ACTIVE);
    render_thick_line(exit_x, top_y, exit_x, arrow_y, SHAFT_T, ACTIVE);
    render_disc(exit_x, top_y, JOINT_R, JOINT_SEG, ACTIVE);
    render_disc(enter_x, top_y, JOINT_R, JOINT_SEG, ACTIVE);

    /* L4: arrowhead (raised) */
    render_arrowhead(exit_x, arrow_y, (float)(-M_PI * 0.5), HEAD_SZ, ACTIVE);
}

/*
 * Universal roundabout icon.
 * exit_angle_deg: iOS convention (0=straight, +90=right, -90=left, ±180=u-turn).
 * driving_side: 0=RHT (CCW travel), 1=LHT (CW travel).
 * junction_angles: array of ALL junction element angles (iOS degrees), may include exit.
 * junction_angle_count: number of entries in junction_angles.
 */
static void draw_roundabout(float exit_angle_deg, int driving_side,
                            const int *junction_angles, int junction_angle_count) {
    float cx = 0.0f, cy = 0.0f;
    float ring_r = RAB_RING_R;
    float ring_ol = OL_T;    /* ring outline thickness */
    float ring_sd = SIDE_T;  /* ring grey thickness */
    float ring_ac = SHAFT_T; /* ring active thickness */

    float entry_rad = (float)(-M_PI * 0.5);   /* entry always at bottom */

    /* Snap exit_angle to nearest junction angle OR entry */
    float entry_deg = -180.0f;
    float snapped_exit_deg = exit_angle_deg;
    int snapped_idx = -1;
    if (junction_angle_count > 0) {
        int best = 0;
        float best_diff = 999.0f;
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

    float ext = BLUE_LEN - ring_r;    /* blue exit stub beyond ring */
    float stub = ROAD_LEN - ring_r;  /* white+grey stub beyond ring */
    float stub_fade = FADE_LEN;      /* fade beyond stub */

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

    /* Flat pass to FBO (no blend) */
    render_begin_stubs();
    render_set_raised(0);

    /* Precompute which fade stubs to draw (dedup once) */
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

    /* === All outlines first (fade + solid) === */
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
    render_rect(-ring_ol * 0.5f, SHAFT_BOT,
                ring_ol, entry_top - SHAFT_BOT, WHITE);
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

    /* === All grey second (fade + solid) — overwrites outline interiors === */
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
    render_rect(-ring_sd * 0.5f, SHAFT_BOT, ring_sd,
                entry_top - SHAFT_BOT, SIDE);
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

    render_end_stubs();

    /* L3: blue — entry + exit + arc + caps + arrowhead (raised) */
    render_set_raised(1);
    render_rect(-ring_ac * 0.5f, SHAFT_BOT, ring_ac,
                entry_top - SHAFT_BOT, ACTIVE);
    render_thick_line(ex0_x, ex0_y, ex_tip_x, ex_tip_y, ring_ac, ACTIVE);

    float arc_s, arc_e;
    if (driving_side == 0) {
        arc_s = entry_rad;
        arc_e = exit_rad;
        while (arc_e <= arc_s) arc_e += 2.0f * (float)M_PI;
    } else {
        arc_s = exit_rad;
        arc_e = entry_rad;
        while (arc_e <= arc_s) arc_e += 2.0f * (float)M_PI;
    }
    render_arc(cx, cy, ring_r, ring_ac, arc_s, arc_e, RAB_RING_SEG, ACTIVE);

    /* Discs at arc/stub junctions to fill gaps */
    render_disc(cx + ring_r * cosf(arc_s), cy + ring_r * sinf(arc_s),
                ring_ac * 0.5f, JOINT_SEG, ACTIVE);
    render_disc(cx + ring_r * cosf(arc_e), cy + ring_r * sinf(arc_e),
                ring_ac * 0.5f, JOINT_SEG, ACTIVE);

    render_arrowhead(ex_tip_x, ex_tip_y, exit_rad, HEAD_SZ, ACTIVE);
}

/*
 * Checkered flag on a pole.
 * bx,by = base of pole.  pole_h = pole height.  flag goes to the right.
 */
static void draw_flag(float bx, float by, float pole_h) {
    render_set_raised(0);
    float fx = bx;
    float fy = by + pole_h;          /* flag top-left */
    float cw = FLAG_W / 3.0f;
    float ch = FLAG_H / 2.0f;

    /* pole (gray) */
    render_thick_line(bx, by, bx, fy, FLAG_POLE_T, SIDE);

    /* base disc */
    render_disc(bx, by, FLAG_BASE_R, JOINT_SEG, ACTIVE);

    /* 3×2 checkerboard */
    int row, col;
    for (row = 0; row < 2; row++) {
        for (col = 0; col < 3; col++) {
            float rx = fx + col * cw;
            float ry = fy - row * ch;   /* draw downward from top */
            if ((row + col) % 2 == 0)
                render_rect(rx, ry - ch, cw, ch, ACTIVE);
            else
                render_rect(rx, ry - ch, cw, ch, CHECK_R, CHECK_G, CHECK_B, 1.0f);
        }
    }
}

/*
 * Destination arrived icon.
 * dir: 0=center (offroad), -1=left, 1=right.
 */
static void draw_arrived(int dir) {
    float road_top = 0.10f;
    float dome_r = DOME_R;

    if (dir == 0) {
        /* --- Center / offroad --- */
        float ring_y = road_top + HEAD_SZ + 0.12f;
        float ring_r = ARRIVE_RING_R;

        /* Entry fade (flat) */
        render_set_raised(0);
        draw_fading_road(0, SHAFT_BOT, 0, SHAFT_BOT - FADE_LEN, 1.0f, FADE_BOTH);

        /* L1: white outline (flat) */
        render_thick_line(0, SHAFT_BOT, 0, road_top, OL_T, WHITE);
        render_circle(0, ring_y, ring_r, ARRIVE_RING_T + OL_W * 2, 24, WHITE);

        /* L2: grey under active (flat) */
        render_thick_line(0, SHAFT_BOT, 0, road_top, SIDE_T, SIDE);

        /* L3: blue road + arrowhead (raised) */
        render_set_raised(1);
        render_thick_line(0, SHAFT_BOT, 0, road_top, SHAFT_T, ACTIVE);
        render_arrowhead(0, road_top, (float)(M_PI * 0.5), HEAD_SZ, ACTIVE);
        render_circle(0, ring_y, ring_r, ARRIVE_RING_T, 24, ACTIVE);

        /* flag */
        draw_flag(0, ring_y, 0.42f);
    } else {
        /* --- Left / Right: road + dome cap + flag to side --- */
        float sign = (dir < 0) ? -1.0f : 1.0f;
        float flag_x = sign * 0.30f;

        /* Entry fade (flat) */
        render_set_raised(0);
        draw_fading_road(0, SHAFT_BOT, 0, SHAFT_BOT - FADE_LEN, 1.0f, FADE_BOTH);

        /* L1: white outline (flat) */
        render_thick_line(0, SHAFT_BOT, 0, road_top, OL_T, WHITE);
        render_disc(0, road_top, dome_r + OL_W, 24, WHITE);

        /* L2: grey under active (flat) */
        render_thick_line(0, SHAFT_BOT, 0, road_top, SIDE_T, SIDE);
        render_disc(0, road_top, dome_r + OL_W * 0.5f, 24, SIDE);

        /* L3: blue road + dome cap (raised) */
        render_set_raised(1);
        render_thick_line(0, SHAFT_BOT, 0, road_top, SHAFT_T, ACTIVE);
        render_disc(0, road_top, dome_r, 24, ACTIVE);

        /* flag */
        draw_flag(flag_x, road_top - 0.06f, 0.50f);
    }
}

/*
 * Lane change icon (S-curve shift).
 * go_left=1: shift to the left lane.
 */
static void draw_lane_change(int go_left) {
    float sign = go_left ? -1.0f : 1.0f;
    float shift = sign * LANE_SHIFT;
    float bend_lo = BEND_LO;
    float bend_hi = BEND_HI;

    /* Fades (flat) */
    render_set_raised(0);
    draw_fading_road(0, SHAFT_BOT, 0, SHAFT_BOT - FADE_LEN, 1.0f, FADE_BOTH);
    draw_fading_road(0, SIDE_TOP, 0, SIDE_TOP + FADE_LEN, 1.0f, FADE_BOTH);
    draw_fading_road(shift, SIDE_TOP, shift, SIDE_TOP + FADE_LEN, 1.0f, FADE_BOTH);

    /* L1: white outline + joint discs (flat) */
    render_thick_line(0, SHAFT_BOT, 0, SIDE_TOP, OL_T, WHITE);
    render_thick_line(0, SHAFT_BOT, 0, bend_lo, OL_T, WHITE);
    render_thick_line(0, bend_lo, shift, bend_hi, OL_T, WHITE);
    render_disc(0, bend_lo, OL_T * 0.5f, JOINT_SEG, WHITE);
    render_disc(shift, bend_hi, OL_T * 0.5f, JOINT_SEG, WHITE);
    render_thick_line(shift, bend_hi, shift, SIDE_TOP, OL_T, WHITE);

    /* L2: grey on all roads (flat) */
    render_thick_line(0, SHAFT_BOT, 0, SIDE_TOP, SIDE_T, SIDE);
    render_thick_line(0, SHAFT_BOT, 0, bend_lo, SIDE_T, SIDE);
    render_thick_line(0, bend_lo, shift, bend_hi, SIDE_T, SIDE);
    render_disc(0, bend_lo, SIDE_T * 0.5f, JOINT_SEG, SIDE);
    render_disc(shift, bend_hi, SIDE_T * 0.5f, JOINT_SEG, SIDE);
    render_thick_line(shift, bend_hi, shift, SIDE_TOP, SIDE_T, SIDE);

    /* L3: blue active route (raised) */
    render_set_raised(1);
    render_thick_line(0, SHAFT_BOT, 0, bend_lo, SHAFT_T, ACTIVE);
    render_thick_line(0, bend_lo, shift, bend_hi, SHAFT_T, ACTIVE);
    render_disc(0, bend_lo, JOINT_R, JOINT_SEG, ACTIVE);
    render_disc(shift, bend_hi, JOINT_R, JOINT_SEG, ACTIVE);
    render_thick_line(shift, bend_hi, shift, BLUE_LEN, SHAFT_T, ACTIVE);
    render_arrowhead(shift, BLUE_LEN, (float)(M_PI * 0.5), HEAD_SZ, ACTIVE);
}

/*
 * Merge onto highway (on-ramp).
 * go_right=1: merging from left to right lane.
 */
static void draw_merge(int go_right) {
    float sign = go_right ? 1.0f : -1.0f;
    float start_x = sign * -MERGE_OFFSET;
    float bend_lo = -0.10f;
    float bend_hi = BEND_HI;

    /* Fades (flat) */
    render_set_raised(0);
    draw_fading_road(0, SHAFT_BOT, 0, SHAFT_BOT - FADE_LEN, 1.0f, FADE_BOTH);
    draw_fading_road(0, SIDE_TOP, 0, SIDE_TOP + FADE_LEN, 1.0f, FADE_BOTH);
    draw_fading_road(start_x, SHAFT_BOT, start_x, SHAFT_BOT - FADE_LEN, 1.0f, FADE_BOTH);

    /* L1: white side markings + joint discs (flat) */
    render_thick_line(0, SHAFT_BOT, 0, SIDE_TOP, OL_T, WHITE);
    render_thick_line(start_x, SHAFT_BOT, start_x, bend_lo, OL_T, WHITE);
    render_thick_line(start_x, bend_lo, 0, bend_hi, OL_T, WHITE);
    render_disc(start_x, bend_lo, OL_T * 0.5f, JOINT_SEG, WHITE);
    render_disc(0, bend_hi, OL_T * 0.5f, JOINT_SEG, WHITE);
    render_thick_line(0, bend_hi, 0, SHAFT_TOP, OL_T, WHITE);

    /* L2: grey on all roads (flat) */
    render_thick_line(0, SHAFT_BOT, 0, SIDE_TOP, SIDE_T, SIDE);
    render_thick_line(start_x, SHAFT_BOT, start_x, bend_lo, SIDE_T, SIDE);
    render_thick_line(start_x, bend_lo, 0, bend_hi, SIDE_T, SIDE);
    render_disc(start_x, bend_lo, SIDE_T * 0.5f, JOINT_SEG, SIDE);
    render_disc(0, bend_hi, SIDE_T * 0.5f, JOINT_SEG, SIDE);
    render_thick_line(0, bend_hi, 0, SHAFT_TOP, SIDE_T, SIDE);

    /* L3: blue active route (raised) */
    render_set_raised(1);
    render_thick_line(start_x, SHAFT_BOT, start_x, bend_lo, SHAFT_T, ACTIVE);
    render_thick_line(start_x, bend_lo, 0, bend_hi, SHAFT_T, ACTIVE);
    render_disc(start_x, bend_lo, JOINT_R, JOINT_SEG, ACTIVE);
    render_disc(0, bend_hi, JOINT_R, JOINT_SEG, ACTIVE);
    render_thick_line(0, bend_hi, 0, BLUE_LEN, SHAFT_T, ACTIVE);
    render_arrowhead(0, BLUE_LEN, (float)(M_PI * 0.5), HEAD_SZ, ACTIVE);
}

/* ================================================================
 * Main dispatch
 * ================================================================ */

void maneuver_draw(const maneuver_state_t *s) {
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
