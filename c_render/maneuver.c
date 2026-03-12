/*
 * Maneuver icon rendering — procedural GL icons for all CarPlay maneuver types.
 *
 * Active color: #5AAAE6  (arrow you follow)
 * Side color:   #646464  (roads you don't take)
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

#define SHAFT_T    0.14f    /* active blue thickness */
#define OL_W       0.025f   /* border width per side */
#define SIDE_T     (SHAFT_T + OL_W * 2)  /* grey = active + border */
#define OL_T       (SIDE_T  + OL_W * 2)  /* outline = grey + border */
#define HEAD_SZ    (SHAFT_T * 1.3f)   /* arrowhead height = 1.3× shaft */
#define SHAFT_BOT -0.55f    /* shaft start y */
#define SHAFT_TOP  0.35f    /* straight arrow shaft end y */
#define SIDE_TOP   0.55f    /* side road end y */
#define TURN_LEN   0.48f    /* turn shaft length */
#define JOINT_R    (SHAFT_T * 0.5f)   /* joint disc radius */
#define JOINT_SEG  12       /* joint disc segments */
#define WHITE  1.0f, 1.0f, 1.0f, 1.0f

/* ================================================================
 * Icon drawing functions
 * ================================================================ */

static void draw_straight(void) {
    /* L1: white outline (flat) */
    render_set_raised(0);
    render_thick_line(0, SHAFT_BOT, 0, SHAFT_TOP, OL_T, WHITE);
    /* L2: grey under active (flat) */
    render_thick_line(0, SHAFT_BOT, 0, SHAFT_TOP, SIDE_T, SIDE);
    /* L3: blue road + arrowhead (raised) */
    render_set_raised(1);
    render_thick_line(0, SHAFT_BOT, 0, SHAFT_TOP, SHAFT_T, ACTIVE);
    render_arrowhead(0, SHAFT_TOP, (float)(M_PI * 0.5), HEAD_SZ, ACTIVE);
}

/*
 * Generic turn icon.  angle_deg measured from straight-ahead (up):
 *   +30 = slight right, +90 = right, +135 = sharp right
 *   -30 = slight left,  -90 = left,  -135 = sharp left
 */
static void draw_turn(float angle_deg) {
    float angle_rad = angle_deg * (float)M_PI / 180.0f;
    float abs_a = fabsf(angle_deg);

    /* turn point height: higher for gentle turns */
    float turn_y;
    if (abs_a <= 45.0f)       turn_y = -0.05f;
    else if (abs_a <= 100.0f) turn_y =  0.05f;
    else                      turn_y =  0.15f;

    /* end of turn shaft */
    float end_x = TURN_LEN * sinf(angle_rad);
    float end_y = turn_y + TURN_LEN * cosf(angle_rad);

    /* L1: white outline (flat) */
    render_set_raised(0);
    render_thick_line(0, turn_y, 0, SIDE_TOP, OL_T, WHITE);
    render_thick_line(0, SHAFT_BOT, 0, turn_y, OL_T, WHITE);
    render_thick_line(0, turn_y, end_x, end_y, OL_T, WHITE);
    render_disc(0, turn_y, OL_T * 0.5f, JOINT_SEG, WHITE);

    /* L2: grey on all roads (flat) */
    render_thick_line(0, turn_y, 0, SIDE_TOP, SIDE_T, SIDE);
    render_thick_line(0, SHAFT_BOT, 0, turn_y, SIDE_T, SIDE);
    render_thick_line(0, turn_y, end_x, end_y, SIDE_T, SIDE);
    render_disc(0, turn_y, SIDE_T * 0.5f, JOINT_SEG, SIDE);

    /* L3: blue entry + turn (raised) */
    render_set_raised(1);
    render_thick_line(0, SHAFT_BOT, 0, turn_y, SHAFT_T, ACTIVE);
    render_thick_line(0, turn_y, end_x, end_y, SHAFT_T, ACTIVE);
    render_disc(0, turn_y, JOINT_R, JOINT_SEG, ACTIVE);
    float head_angle = (float)(M_PI * 0.5) - angle_rad;
    render_arrowhead(end_x, end_y, head_angle, HEAD_SZ, ACTIVE);
}

/*
 * U-turn icon.
 * go_left=1: shaft on right, curves left, arrow points down on left (RHT default).
 * go_left=0: mirror.
 */
static void draw_uturn(int go_left) {
    float sign = go_left ? -1.0f : 1.0f;
    float gap = 0.18f;
    float enter_x =  sign * gap;  /* negate: enter on opposite side */
    float exit_x  = -sign * gap;
    float top_y   =  0.30f;
    float arrow_y = -0.10f;

    enter_x = -enter_x; /* enter is on the right for go_left */
    exit_x  = -exit_x;
    /* Correction: for go_left (RHT), enter on right (+gap), exit on left (-gap) */
    enter_x = go_left ?  gap : -gap;
    exit_x  = go_left ? -gap :  gap;

    /* L1: white outline (flat) */
    render_set_raised(0);
    render_thick_line(enter_x, SHAFT_BOT, enter_x, top_y, OL_T, WHITE);
    render_arc(0, top_y, gap, OL_T, 0, (float)M_PI, 16, WHITE);
    render_thick_line(exit_x, top_y, exit_x, arrow_y, OL_T, WHITE);

    /* L2: grey under active (flat) */
    render_thick_line(enter_x, SHAFT_BOT, enter_x, top_y, SIDE_T, SIDE);
    render_arc(0, top_y, gap, SIDE_T, 0, (float)M_PI, 16, SIDE);
    render_thick_line(exit_x, top_y, exit_x, arrow_y, SIDE_T, SIDE);
    render_disc(exit_x, top_y, SIDE_T * 0.5f, JOINT_SEG, SIDE);
    render_disc(enter_x, top_y, SIDE_T * 0.5f, JOINT_SEG, SIDE);

    /* L3: blue fill (raised) */
    render_set_raised(1);
    render_thick_line(enter_x, SHAFT_BOT, enter_x, top_y, SHAFT_T, ACTIVE);
    render_arc(0, top_y, gap, SHAFT_T, 0, (float)M_PI, 16, ACTIVE);
    render_thick_line(exit_x, top_y, exit_x, arrow_y, SHAFT_T, ACTIVE);
    render_disc(exit_x, top_y, JOINT_R, JOINT_SEG, ACTIVE);
    render_disc(enter_x, top_y, JOINT_R, JOINT_SEG, ACTIVE);

    /* L4: arrowhead (raised) */
    render_arrowhead(exit_x, arrow_y, (float)(-M_PI * 0.5), HEAD_SZ, ACTIVE);
}

/*
 * Highway exit icon.
 * go_right=1: exit branches to the right, main road (gray) continues straight.
 */
static void draw_exit(int go_right) {
    float sign = go_right ? 1.0f : -1.0f;
    float fork_y = 0.0f;
    float branch_angle = sign * 30.0f * (float)M_PI / 180.0f;
    float branch_len = 0.48f;
    float end_x = branch_len * sinf(branch_angle);
    float end_y = fork_y + branch_len * cosf(branch_angle);

    /* L1: white outline (flat) */
    render_set_raised(0);
    render_thick_line(0, SHAFT_BOT, 0, SIDE_TOP, OL_T, WHITE);
    render_thick_line(0, fork_y, end_x, end_y, OL_T, WHITE);
    render_disc(0, fork_y, OL_T * 0.5f, JOINT_SEG, WHITE);

    /* L2: grey on all roads (flat) */
    render_thick_line(0, SHAFT_BOT, 0, SIDE_TOP, SIDE_T, SIDE);
    render_thick_line(0, fork_y, end_x, end_y, SIDE_T, SIDE);
    render_disc(0, fork_y, SIDE_T * 0.5f, JOINT_SEG, SIDE);

    /* L3: blue entry + exit branch (raised) */
    render_set_raised(1);
    render_thick_line(0, SHAFT_BOT, 0, fork_y, SHAFT_T, ACTIVE);
    render_thick_line(0, fork_y, end_x, end_y, SHAFT_T, ACTIVE);
    render_disc(0, fork_y, JOINT_R, JOINT_SEG, ACTIVE);
    float head_angle = (float)(M_PI * 0.5) - branch_angle;
    render_arrowhead(end_x, end_y, head_angle, HEAD_SZ, ACTIVE);
}

/*
 * Universal roundabout icon.
 * exit_angle_deg: iOS convention (0=straight, +90=right, -90=left, ±180=u-turn).
 * driving_side: 0=RHT (CCW travel), 1=LHT (CW travel).
 * junction_angles: array of ALL junction element angles (iOS degrees), may include exit.
 * junction_angle_count: number of entries in junction_angles.
 *
 * When junction_angles is provided, grey stubs are drawn at real exit positions.
 * When empty, only entry + active exit are shown (no other stubs).
 */
static void draw_roundabout(float exit_angle_deg, int driving_side,
                            const int *junction_angles, int junction_angle_count) {
    float cx = 0.0f, cy = 0.08f;
    float ring_r = 0.28f;
    float ring_ol = OL_T;    /* ring outline thickness */
    float ring_sd = SIDE_T;  /* ring grey thickness */
    float ring_ac = SHAFT_T; /* ring active thickness */

    /* iOS degrees → math radians: math_rad = (90 - ios_deg) * π/180 */
    float entry_rad = (float)(-M_PI * 0.5);   /* entry always at bottom */
    float exit_rad  = (90.0f - exit_angle_deg) * (float)M_PI / 180.0f;

    float ext = 0.28f;    /* active exit stub length beyond ring */
    float stub = 0.20f;   /* side stub length beyond ring */

    float entry_top = cy - ring_r;
    float ex0_x = cx + ring_r * cosf(exit_rad);
    float ex0_y = cy + ring_r * sinf(exit_rad);
    float ex_tip_x = cx + (ring_r + ext) * cosf(exit_rad);
    float ex_tip_y = cy + (ring_r + ext) * sinf(exit_rad);

    /* Convert junction angles to math radians.
     * No filtering — grey stubs at same angle as active exit or entry
     * are hidden under the blue (L3) and entry road respectively. */
    float side_rads[MAX_JUNCTION_ANGLES];
    int n_sides = 0;
    int i;
    for (i = 0; i < junction_angle_count && i < MAX_JUNCTION_ANGLES; i++) {
        side_rads[n_sides++] = (90.0f - (float)junction_angles[i]) * (float)M_PI / 180.0f;
    }

    /* L1: white outlines — entry + exit + side stubs + ring (flat) */
    render_set_raised(0);
    render_rect(-ring_ol * 0.5f, SHAFT_BOT,
                ring_ol, entry_top - SHAFT_BOT, WHITE);
    render_thick_line(ex0_x, ex0_y, ex_tip_x, ex_tip_y, ring_ol, WHITE);
    for (i = 0; i < n_sides; i++) {
        float sr = side_rads[i];
        float sx0 = cx + ring_r * cosf(sr);
        float sy0 = cy + ring_r * sinf(sr);
        float sx1 = cx + (ring_r + stub) * cosf(sr);
        float sy1 = cy + (ring_r + stub) * sinf(sr);
        render_thick_line(sx0, sy0, sx1, sy1, ring_ol, WHITE);
    }
    render_circle(cx, cy, ring_r, ring_ol, 48, WHITE);

    /* L2: grey fill — entry + exit + side stubs + ring (flat) */
    render_rect(-ring_sd * 0.5f, SHAFT_BOT, ring_sd,
                entry_top - SHAFT_BOT, SIDE);
    render_thick_line(ex0_x, ex0_y, ex_tip_x, ex_tip_y, ring_sd, SIDE);
    for (i = 0; i < n_sides; i++) {
        float sr = side_rads[i];
        float sx0 = cx + ring_r * cosf(sr);
        float sy0 = cy + ring_r * sinf(sr);
        float sx1 = cx + (ring_r + stub) * cosf(sr);
        float sy1 = cy + (ring_r + stub) * sinf(sr);
        render_thick_line(sx0, sy0, sx1, sy1, ring_sd, SIDE);
    }
    render_circle(cx, cy, ring_r, ring_sd, 48, SIDE);

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
    render_arc(cx, cy, ring_r, ring_ac, arc_s, arc_e, 48, ACTIVE);

    /* caps at arc start/end */
    {
        float ri = ring_r - ring_ac * 0.5f;
        float ro = ring_r + ring_ac * 0.5f;
        render_thick_line(cx + ri * cosf(arc_s), cy + ri * sinf(arc_s),
                          cx + ro * cosf(arc_s), cy + ro * sinf(arc_s),
                          0.10f, ACTIVE);
        render_thick_line(cx + ri * cosf(arc_e), cy + ri * sinf(arc_e),
                          cx + ro * cosf(arc_e), cy + ro * sinf(arc_e),
                          0.10f, ACTIVE);
    }

    render_arrowhead(ex_tip_x, ex_tip_y, exit_rad, HEAD_SZ, ACTIVE);
}

/*
 * Checkered flag on a pole.
 * bx,by = base of pole.  pole_h = pole height.  flag goes to the right.
 */
static void draw_flag(float bx, float by, float pole_h) {
    render_set_raised(0);
    float pole_t = 0.025f;
    float flag_w = 0.22f;
    float flag_h = 0.16f;
    float fx = bx;
    float fy = by + pole_h;          /* flag top-left */
    float cw = flag_w / 3.0f;
    float ch = flag_h / 2.0f;

    /* pole (gray) */
    render_thick_line(bx, by, bx, fy, pole_t, SIDE);

    /* base disc */
    render_disc(bx, by, 0.05f, 12, ACTIVE);

    /* 3×2 checkerboard */
    int row, col;
    for (row = 0; row < 2; row++) {
        for (col = 0; col < 3; col++) {
            float rx = fx + col * cw;
            float ry = fy - row * ch;   /* draw downward from top */
            if ((row + col) % 2 == 0)
                render_rect(rx, ry - ch, cw, ch, ACTIVE);
            else
                render_rect(rx, ry - ch, cw, ch, 0.12f, 0.14f, 0.18f, 1.0f);
        }
    }
}

/*
 * Destination arrived icon.
 * dir: 0=center (offroad), -1=left, 1=right.
 *
 * Left/Right: blue road + dome cap + checkered flag on pole offset to side.
 * Center:     blue arrow + arrowhead + hollow ring + checkered flag on pole.
 */
static void draw_arrived(int dir) {
    float road_top = 0.10f;
    float dome_r = 0.12f;

    if (dir == 0) {
        /* --- Center / offroad --- */
        float ring_y = road_top + HEAD_SZ + 0.12f;
        float ring_r = 0.11f;

        /* L1: white outline (flat) */
        render_set_raised(0);
        render_thick_line(0, SHAFT_BOT, 0, road_top, OL_T, WHITE);
        render_circle(0, ring_y, ring_r, 0.04f + OL_W * 2, 24, WHITE);

        /* L2: grey under active (flat) */
        render_thick_line(0, SHAFT_BOT, 0, road_top, SIDE_T, SIDE);

        /* L3: blue road + arrowhead (raised) */
        render_set_raised(1);
        render_thick_line(0, SHAFT_BOT, 0, road_top, SHAFT_T, ACTIVE);
        render_arrowhead(0, road_top, (float)(M_PI * 0.5), HEAD_SZ, ACTIVE);
        render_circle(0, ring_y, ring_r, 0.04f, 24, ACTIVE);

        /* flag */
        draw_flag(0, ring_y, 0.42f);
    } else {
        /* --- Left / Right: road + dome cap + flag to side --- */
        float sign = (dir < 0) ? -1.0f : 1.0f;
        float flag_x = sign * 0.30f;

        /* L1: white outline (flat) */
        render_set_raised(0);
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
    float shift = sign * 0.22f;

    /* L1: white outline + joint discs (flat) */
    render_set_raised(0);
    render_thick_line(0, SHAFT_BOT, 0, 0.50f, OL_T, WHITE);
    render_thick_line(0, SHAFT_BOT, 0, -0.15f, OL_T, WHITE);
    render_thick_line(0, -0.15f, shift, 0.15f, OL_T, WHITE);
    render_disc(0, -0.15f, OL_T * 0.5f, JOINT_SEG, WHITE);
    render_disc(shift, 0.15f, OL_T * 0.5f, JOINT_SEG, WHITE);
    render_thick_line(shift, 0.15f, shift, 0.35f, OL_T, WHITE);

    /* L2: grey on all roads (flat) */
    render_thick_line(0, SHAFT_BOT, 0, 0.50f, SIDE_T, SIDE);
    render_thick_line(0, SHAFT_BOT, 0, -0.15f, SIDE_T, SIDE);
    render_thick_line(0, -0.15f, shift, 0.15f, SIDE_T, SIDE);
    render_disc(0, -0.15f, SIDE_T * 0.5f, JOINT_SEG, SIDE);
    render_disc(shift, 0.15f, SIDE_T * 0.5f, JOINT_SEG, SIDE);
    render_thick_line(shift, 0.15f, shift, 0.35f, SIDE_T, SIDE);

    /* L3: blue active route (raised) */
    render_set_raised(1);
    render_thick_line(0, SHAFT_BOT, 0, -0.15f, SHAFT_T, ACTIVE);
    render_thick_line(0, -0.15f, shift, 0.15f, SHAFT_T, ACTIVE);
    render_disc(0, -0.15f, JOINT_R, JOINT_SEG, ACTIVE);
    render_disc(shift, 0.15f, JOINT_R, JOINT_SEG, ACTIVE);
    render_thick_line(shift, 0.15f, shift, 0.35f, SHAFT_T, ACTIVE);
    render_arrowhead(shift, 0.35f, (float)(M_PI * 0.5), HEAD_SZ, ACTIVE);
}

/*
 * Merge onto highway (on-ramp).
 * go_right=1: merging from left to right lane.
 */
static void draw_merge(int go_right) {
    float sign = go_right ? 1.0f : -1.0f;
    float start_x = sign * -0.25f;

    /* L1: white side markings + joint discs (flat) */
    render_set_raised(0);
    render_thick_line(0, SHAFT_BOT, 0, SIDE_TOP, OL_T, WHITE);
    render_thick_line(start_x, SHAFT_BOT, start_x, -0.10f, OL_T, WHITE);
    render_thick_line(start_x, -0.10f, 0, 0.15f, OL_T, WHITE);
    render_disc(start_x, -0.10f, OL_T * 0.5f, JOINT_SEG, WHITE);
    render_disc(0, 0.15f, OL_T * 0.5f, JOINT_SEG, WHITE);
    render_thick_line(0, 0.15f, 0, SHAFT_TOP, OL_T, WHITE);

    /* L2: grey on all roads (flat) */
    render_thick_line(0, SHAFT_BOT, 0, SIDE_TOP, SIDE_T, SIDE);
    render_thick_line(start_x, SHAFT_BOT, start_x, -0.10f, SIDE_T, SIDE);
    render_thick_line(start_x, -0.10f, 0, 0.15f, SIDE_T, SIDE);
    render_disc(start_x, -0.10f, SIDE_T * 0.5f, JOINT_SEG, SIDE);
    render_disc(0, 0.15f, SIDE_T * 0.5f, JOINT_SEG, SIDE);
    render_thick_line(0, 0.15f, 0, SHAFT_TOP, SIDE_T, SIDE);

    /* L3: blue active route (raised) */
    render_set_raised(1);
    render_thick_line(start_x, SHAFT_BOT, start_x, -0.10f, SHAFT_T, ACTIVE);
    render_thick_line(start_x, -0.10f, 0, 0.15f, SHAFT_T, ACTIVE);
    render_disc(start_x, -0.10f, JOINT_R, JOINT_SEG, ACTIVE);
    render_disc(0, 0.15f, JOINT_R, JOINT_SEG, ACTIVE);
    render_thick_line(0, 0.15f, 0, SHAFT_TOP, SHAFT_T, ACTIVE);
    render_arrowhead(0, SHAFT_TOP, (float)(M_PI * 0.5), HEAD_SZ, ACTIVE);
}

/* ================================================================
 * Main dispatch — maps iOS ManeuverType to icon
 * ================================================================ */

void maneuver_draw(const maneuver_state_t *s) {
    int t = s->maneuver_type;
    int ds = s->driving_side;

    /* Not set → draw nothing */
    if (t == MT_NOT_SET || t < 0) return;

    /* ---- Global types (apply regardless of junction_type) ---- */

    if (t == MT_NO_TURN || t == MT_FOLLOW_ROAD || t == MT_STRAIGHT_AHEAD ||
        t == MT_START_ROUTE || t == MT_ENTER_FERRY || t == MT_EXIT_FERRY ||
        t == MT_CHANGE_FERRY || t == MT_CHANGE_HIGHWAY) {
        draw_straight();
        return;
    }

    if (t == MT_ARRIVE_END_OF_NAVIGATION || t == MT_ARRIVE_AT_DESTINATION ||
        t == MT_ARRIVE_END_OF_DIRECTIONS) {
        draw_arrived(0);
        return;
    }
    if (t == MT_ARRIVE_DESTINATION_LEFT) { draw_arrived(-1); return; }
    if (t == MT_ARRIVE_DESTINATION_RIGHT) { draw_arrived(1); return; }

    if (t == MT_CHANGE_HIGHWAY_LEFT)  { draw_lane_change(1); return; }
    if (t == MT_CHANGE_HIGHWAY_RIGHT) { draw_lane_change(0); return; }

    if (t == MT_U_TURN_AT_ROUNDABOUT) {
        draw_roundabout(180.0f, ds, s->junction_angles, s->junction_angle_count);
        return;
    }

    if (t == MT_ENTER_ROUNDABOUT) {
        draw_roundabout((float)s->exit_angle, ds,
                        s->junction_angles, s->junction_angle_count);
        return;
    }

    /* ---- Junction type gate (replicates ManeuverMapper logic) ---- */

    if (s->junction_type == 1) {
        /* Roundabout context — all types use universal draw */
        if (t == MT_EXIT_ROUNDABOUT || (t >= MT_ROUNDABOUT_EXIT_1 && t <= MT_ROUNDABOUT_EXIT_19)) {
            draw_roundabout((float)s->exit_angle, ds,
                            s->junction_angles, s->junction_angle_count);
            return;
        }
        /* Other types with junction_type=1 → no icon (same as ManeuverMapper) */
        return;
    }

    if (s->junction_type != 0) {
        /* Unknown junction type → no icon */
        return;
    }

    /* ---- Normal intersection (junction_type=0) ---- */

    switch (t) {
        case MT_LEFT_TURN:
        case MT_LEFT_TURN_AT_END:
            draw_turn(-90.0f);
            break;

        case MT_RIGHT_TURN:
        case MT_RIGHT_TURN_AT_END:
            draw_turn(90.0f);
            break;

        case MT_SLIGHT_LEFT_TURN:
        case MT_KEEP_LEFT:
            draw_turn(-30.0f);
            break;

        case MT_SLIGHT_RIGHT_TURN:
        case MT_KEEP_RIGHT:
            draw_turn(30.0f);
            break;

        case MT_SHARP_LEFT_TURN:
            draw_turn(-135.0f);
            break;

        case MT_SHARP_RIGHT_TURN:
            draw_turn(135.0f);
            break;

        case MT_U_TURN:
        case MT_START_ROUTE_WITH_U_TURN:
        case MT_U_TURN_WHEN_POSSIBLE:
            draw_uturn(ds != 1);  /* RHT → go left, LHT → go right */
            break;

        case MT_OFF_RAMP:
            draw_exit(ds != 1);   /* RHT → exit right, LHT → exit left */
            break;

        case MT_HIGHWAY_OFF_RAMP_RIGHT:
            draw_exit(1);
            break;

        case MT_HIGHWAY_OFF_RAMP_LEFT:
            draw_exit(0);
            break;

        case MT_ON_RAMP:
            draw_merge(ds != 1);
            break;

        /* Roundabout exits without junction_type=1 (fallback) */
        case MT_EXIT_ROUNDABOUT:
            draw_roundabout((float)s->exit_angle, ds,
                            s->junction_angles, s->junction_angle_count);
            break;

        default:
            /* Unknown maneuver → straight arrow as safe fallback */
            draw_straight();
            break;
    }
}

/* ================================================================
 * Debug names
 * ================================================================ */

const char *maneuver_type_name(int type) {
    switch (type) {
        case MT_NOT_SET:                    return "NOT_SET";
        case MT_NO_TURN:                    return "NO_TURN";
        case MT_LEFT_TURN:                  return "LEFT_TURN";
        case MT_RIGHT_TURN:                 return "RIGHT_TURN";
        case MT_STRAIGHT_AHEAD:             return "STRAIGHT";
        case MT_U_TURN:                     return "U_TURN";
        case MT_FOLLOW_ROAD:                return "FOLLOW_ROAD";
        case MT_ENTER_ROUNDABOUT:           return "ENTER_RAB";
        case MT_EXIT_ROUNDABOUT:            return "EXIT_RAB";
        case MT_OFF_RAMP:                   return "OFF_RAMP";
        case MT_ON_RAMP:                    return "ON_RAMP";
        case MT_ARRIVE_END_OF_NAVIGATION:   return "ARRIVE_END";
        case MT_START_ROUTE:                return "START_ROUTE";
        case MT_ARRIVE_AT_DESTINATION:      return "ARRIVED";
        case MT_KEEP_LEFT:                  return "KEEP_LEFT";
        case MT_KEEP_RIGHT:                 return "KEEP_RIGHT";
        case MT_ENTER_FERRY:                return "ENTER_FERRY";
        case MT_EXIT_FERRY:                 return "EXIT_FERRY";
        case MT_CHANGE_FERRY:               return "CHANGE_FERRY";
        case MT_START_ROUTE_WITH_U_TURN:    return "START_UTURN";
        case MT_U_TURN_AT_ROUNDABOUT:       return "UTURN_RAB";
        case MT_LEFT_TURN_AT_END:           return "LEFT_END";
        case MT_RIGHT_TURN_AT_END:          return "RIGHT_END";
        case MT_HIGHWAY_OFF_RAMP_LEFT:      return "HWY_EXIT_L";
        case MT_HIGHWAY_OFF_RAMP_RIGHT:     return "HWY_EXIT_R";
        case MT_ARRIVE_DESTINATION_LEFT:    return "ARRIVE_L";
        case MT_ARRIVE_DESTINATION_RIGHT:   return "ARRIVE_R";
        case MT_U_TURN_WHEN_POSSIBLE:       return "UTURN_POSS";
        case MT_ARRIVE_END_OF_DIRECTIONS:   return "ARRIVE_DIRS";
        case MT_SHARP_LEFT_TURN:            return "SHARP_LEFT";
        case MT_SHARP_RIGHT_TURN:           return "SHARP_RIGHT";
        case MT_SLIGHT_LEFT_TURN:           return "SLIGHT_LEFT";
        case MT_SLIGHT_RIGHT_TURN:          return "SLIGHT_RIGHT";
        case MT_CHANGE_HIGHWAY:             return "CHG_HWY";
        case MT_CHANGE_HIGHWAY_LEFT:        return "CHG_HWY_L";
        case MT_CHANGE_HIGHWAY_RIGHT:       return "CHG_HWY_R";
        default:
            if (type >= MT_ROUNDABOUT_EXIT_1 && type <= MT_ROUNDABOUT_EXIT_19) {
                static char buf[16];
                snprintf(buf, sizeof(buf), "RAB_EXIT_%d", type - MT_ROUNDABOUT_EXIT_1 + 1);
                return buf;
            }
            return "UNKNOWN";
    }
}
