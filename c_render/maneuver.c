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
#include <string.h>
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

/* Arrived */
#define ARRIVE_ROAD_TOP   0.15f       /* arrived road/disc end Y */
#define ARRIVE_SEG        24          /* circle/disc segment count for arrived */
#define SNAP_SENTINEL     999.0f      /* large value for angle snap comparison */

/* Destination flag sprite */
#define ARRIVE_FLAG_Y     ARRIVE_ROAD_TOP  /* flag at road endpoint (arrival dot) */
#define ARRIVE_FLAG_SZ    0.18f       /* half-extent of flag sprite */
#define FLAG_ANIM_SPEED   0.35f       /* frames per render frame */

/* Route extrusion heights (must match render.c) */
#define ROUTE_BASE_Y  0.03f   /* same as RAISE_BASE in render.c */
#define ROUTE_TOP_Y   0.06f   /* RAISE_BASE + EXTRUDE_H */

/* Cached route mesh — rebuilt only on maneuver state change */
static route_path_t g_route_path;
static route_mesh_t g_route_mesh;

/* Route animation state — sliding window */
static float g_route_slide = 1.0f;     /* slide parameter 0..2 (0=hidden, 1=in position, 2=pushed out) */
static int   g_route_animating = 0;    /* 1 while any slide animation running */
static float g_anim_start = 0.0f;     /* slide value at animation start */
static float g_anim_target = 1.0f;    /* slide target (1.0 = arrive, 2.0 = push out) */
static float g_route_pre_frac  = 0.0f; /* fraction of extended path before original start */
static float g_route_end_frac  = 1.0f; /* fraction of extended path at original end */
static float g_t_tail = 0.0f;          /* computed tail fraction for extrusion */
static float g_t_head = 1.0f;          /* computed head fraction for extrusion */
static int   g_route_debug = 0;        /* debug overlay toggle */
static float g_slug_override = -1.0f; /* >0: speed normalization length for combined path */
static float g_flag_frame = 0.0f;     /* destination flag animation frame */
static int   g_flag_active = 0;      /* 1 when showing arrived icon (flag animating) */
static int   g_masks_only_mode = 0;   /* append masks without composite/route draw */
static int   g_combined_window_active = 0;
static float g_combined_start_tail_dist = 0.0f;
static float g_combined_start_head_dist = 0.0f;
static float g_combined_end_tail_dist = 0.0f;
static float g_combined_end_head_dist = 0.0f;
static float g_next_cam_pan_x = 0.0f;
static float g_next_cam_pan_y = 0.0f;
static float g_next_cam_rot = 0.0f;
static float g_next_cam_intro_end_dist = 0.0f;
static float g_next_cam_follow_dist = 0.0f;
static float g_next_cam_release_end_dist = 0.0f;
static float g_cam_pan_x = 0.0f;
static float g_cam_pan_y = 0.0f;
static float g_cam_rot = 0.0f;
static float g_cam_settle_start_x = 0.0f;
static float g_cam_settle_start_y = 0.0f;
static float g_cam_settle_start_rot = 0.0f;
static float g_cam_settle_t = 0.0f;
static int   g_cam_settle_active = 0;
static float g_light_settle_start_rot = 0.0f;
static float g_light_settle_t = 0.0f;
static int   g_light_settle_active = 0;
static float g_last_combined_rot = 0.0f;
static int   g_camera_prepared_this_frame = 0;
#define ROUTE_SPEED_PEAK 0.045f         /* peak animation speed (NDC/frame on straight) */
#define ROUTE_SPEED_MIN  0.010f         /* minimum speed on sharpest turns */
#define ROUTE_EXTEND     1.2f           /* extension length beyond viewport */
#define CURV_WINDOW      3              /* points each side for curvature sampling */
#define CURV_SLOWDOWN    8.0f           /* curvature sensitivity (higher = more slowdown) */
#define CAMERA_SETTLE_SPEED 0.12f
#define CAMERA_SETTLE_MAX_ROT 0.45f
#define LIGHT_SETTLE_SPEED (1.0f / 30.0f)

static void clear_combined_transition(void) {
    g_slug_override = -1.0f;
    g_combined_window_active = 0;
    g_combined_start_tail_dist = 0.0f;
    g_combined_start_head_dist = 0.0f;
    g_combined_end_tail_dist = 0.0f;
    g_combined_end_head_dist = 0.0f;
    g_next_cam_pan_x = 0.0f;
    g_next_cam_pan_y = 0.0f;
    g_next_cam_rot = 0.0f;
    g_next_cam_intro_end_dist = 0.0f;
    g_next_cam_follow_dist = 0.0f;
    g_next_cam_release_end_dist = 0.0f;
    g_last_combined_rot = 0.0f;
}

static void clear_camera_settle(void) {
    g_cam_settle_start_x = 0.0f;
    g_cam_settle_start_y = 0.0f;
    g_cam_settle_start_rot = 0.0f;
    g_cam_settle_t = 0.0f;
    g_cam_settle_active = 0;
}

static void clear_light_settle(void) {
    g_light_settle_start_rot = 0.0f;
    g_light_settle_t = 0.0f;
    g_light_settle_active = 0;
    render_set_light_rotation(0.0f);
}

static void rpath_sample(const route_path_t *p, float t, float *out_x, float *out_y);

void maneuver_start_anim(void) {
    g_route_slide = 0.0f;
    g_anim_start = 0.0f;
    g_anim_target = 1.0f;
    g_route_animating = 1;
    clear_combined_transition();
    clear_camera_settle();
    clear_light_settle();
    g_camera_prepared_this_frame = 0;
}

int maneuver_is_animating(void) {
    return g_route_animating || g_flag_active || g_cam_settle_active || g_light_settle_active;
}

void maneuver_set_slide(float t) {
    if (t < 0.0f) t = 0.0f;
    if (t > 2.0f) t = 2.0f;
    g_route_slide = t;
    g_route_animating = 0;
    clear_combined_transition();
    clear_camera_settle();
    clear_light_settle();
    g_camera_prepared_this_frame = 0;
}

float maneuver_get_slide(void) {
    return g_route_slide;
}

void maneuver_start_push(void) {
    /* Seamless: keep current slide, just retarget to 2.0 */
    g_anim_start = g_route_slide;
    g_anim_target = 2.0f;
    g_route_animating = 1;
    clear_combined_transition();
    clear_camera_settle();
    clear_light_settle();
    g_camera_prepared_this_frame = 0;
}

int maneuver_is_pushing(void) {
    return g_route_animating && g_anim_target > 1.5f;
}

void maneuver_toggle_debug(void) {
    g_route_debug = !g_route_debug;
}

int maneuver_is_debug(void) {
    return g_route_debug;
}

/* Get start point and direction of a path segment */
static void seg_start_dir(const route_seg_t *s,
                           float *x, float *y, float *dx, float *dy) {
    if (s->type == RSEG_LINE) {
        *x = s->x0; *y = s->y0;
        float ddx = s->x1 - s->x0, ddy = s->y1 - s->y0;
        float len = sqrtf(ddx * ddx + ddy * ddy);
        if (len < 1e-6f) { *dx = 0; *dy = 1; }
        else { *dx = ddx / len; *dy = ddy / len; }
    } else {
        *x = s->cx + s->radius * cosf(s->start_rad);
        *y = s->cy + s->radius * sinf(s->start_rad);
        float sign = (s->end_rad - s->start_rad >= 0) ? 1.0f : -1.0f;
        *dx = -sign * sinf(s->start_rad);
        *dy =  sign * cosf(s->start_rad);
    }
}

/* Get end point and direction of a path segment */
static void seg_end_dir(const route_seg_t *s,
                         float *x, float *y, float *dx, float *dy) {
    if (s->type == RSEG_LINE) {
        *x = s->x1; *y = s->y1;
        float ddx = s->x1 - s->x0, ddy = s->y1 - s->y0;
        float len = sqrtf(ddx * ddx + ddy * ddy);
        if (len < 1e-6f) { *dx = 0; *dy = 1; }
        else { *dx = ddx / len; *dy = ddy / len; }
    } else {
        *x = s->cx + s->radius * cosf(s->end_rad);
        *y = s->cy + s->radius * sinf(s->end_rad);
        float sign = (s->end_rad - s->start_rad >= 0) ? 1.0f : -1.0f;
        *dx = -sign * sinf(s->end_rad);
        *dy =  sign * cosf(s->end_rad);
    }
}


/* Insert fillet arcs at segment junctions where tangent directions don't match.
 * Trims line segments linearly, arc segments by angular offset (stays on curve).
 * Must be called before rpath_extend/rpath_densify. */
static void rpath_fillet_junctions(route_path_t *p, float fillet_r) {
    if (p->seg_count < 2) return;

    route_seg_t new_segs[RPATH_MAX_SEGS];
    int new_count = 0;
    new_segs[new_count++] = p->segs[0];

    int i;
    for (i = 1; i < p->seg_count; i++) {
        if (new_count >= RPATH_MAX_SEGS - 2) {
            new_segs[new_count++] = p->segs[i];
            continue;
        }

        /* Junction: exit of prev, entry of next */
        float ex, ey, edx, edy;
        seg_end_dir(&new_segs[new_count - 1], &ex, &ey, &edx, &edy);
        float sx, sy, sdx, sdy;
        seg_start_dir(&p->segs[i], &sx, &sy, &sdx, &sdy);

        float dot = edx * sdx + edy * sdy;
        float alpha = acosf(dot < -1.0f ? -1.0f : (dot > 1.0f ? 1.0f : dot));

        if (alpha < 0.09f || alpha > (float)M_PI * 0.97f) {
            new_segs[new_count++] = p->segs[i];
            continue;
        }

        float vertex_half = ((float)M_PI - alpha) * 0.5f;
        float tan_vh = tanf(vertex_half);
        if (tan_vh < 1e-6f) tan_vh = 1e-6f;
        float t_dist = fillet_r / tan_vh;

        /* Segment arc-lengths for clamping */
        route_seg_t *prev = &new_segs[new_count - 1];
        route_seg_t *cur = &p->segs[i];
        float prev_len = (prev->type == RSEG_LINE)
            ? sqrtf((prev->x1-prev->x0)*(prev->x1-prev->x0)+(prev->y1-prev->y0)*(prev->y1-prev->y0))
            : fabsf(prev->end_rad - prev->start_rad) * prev->radius;
        float cur_len = (cur->type == RSEG_LINE)
            ? sqrtf((cur->x1-cur->x0)*(cur->x1-cur->x0)+(cur->y1-cur->y0)*(cur->y1-cur->y0))
            : fabsf(cur->end_rad - cur->start_rad) * cur->radius;
        float max_t = 0.45f * (prev_len < cur_len ? prev_len : cur_len);
        if (t_dist > max_t) t_dist = max_t;
        float actual_r = t_dist * tan_vh;

        /* --- Trim prev segment, get T1 and tangent at T1 --- */
        float t1x, t1y, t1dx, t1dy;
        if (prev->type == RSEG_LINE) {
            t1x = prev->x1 - edx * t_dist;
            t1y = prev->y1 - edy * t_dist;
            t1dx = edx; t1dy = edy;
            prev->x1 = t1x; prev->y1 = t1y;
        } else {
            float sweep = prev->end_rad - prev->start_rad;
            float da = t_dist / prev->radius;
            /* Trim end: move end_rad back towards start */
            float new_end = (sweep > 0) ? prev->end_rad - da : prev->end_rad + da;
            prev->end_rad = new_end;
            t1x = prev->cx + prev->radius * cosf(new_end);
            t1y = prev->cy + prev->radius * sinf(new_end);
            float s = (sweep > 0) ? 1.0f : -1.0f;
            t1dx = -s * sinf(new_end);
            t1dy =  s * cosf(new_end);
        }

        /* --- Trim next segment, get T2 and tangent at T2 --- */
        float t2x, t2y, t2dx, t2dy;
        route_seg_t trimmed = *cur;
        if (trimmed.type == RSEG_LINE) {
            t2x = trimmed.x0 + sdx * t_dist;
            t2y = trimmed.y0 + sdy * t_dist;
            t2dx = sdx; t2dy = sdy;
            trimmed.x0 = t2x; trimmed.y0 = t2y;
        } else {
            float sweep = trimmed.end_rad - trimmed.start_rad;
            float da = t_dist / trimmed.radius;
            /* Trim start: move start_rad forward towards end */
            float new_start = (sweep > 0) ? trimmed.start_rad + da : trimmed.start_rad - da;
            trimmed.start_rad = new_start;
            t2x = trimmed.cx + trimmed.radius * cosf(new_start);
            t2y = trimmed.cy + trimmed.radius * sinf(new_start);
            float s = (sweep > 0) ? 1.0f : -1.0f;
            t2dx = -s * sinf(new_start);
            t2dy =  s * cosf(new_start);
        }

        /* --- Compute fillet arc from actual on-curve T1/T2 and tangents --- */
        float cross2 = t1dx * t2dy - t1dy * t2dx;

        /* Perpendicular to T1 tangent, towards inside of turn */
        float perp_x, perp_y;
        if (cross2 > 0) {
            perp_x = -t1dy; perp_y = t1dx;
        } else {
            perp_x = t1dy; perp_y = -t1dx;
        }

        /* Solve for radius so circle through T1 also passes through T2.
         * center = T1 + perp * r.  Need |center - T2| = r.
         * => r = -(dx²+dy²) / (2*(dx*perp_x + dy*perp_y))
         * where dx = t1x-t2x, dy = t1y-t2y */
        {
            float dx = t1x - t2x, dy = t1y - t2y;
            float d_perp = dx * perp_x + dy * perp_y;
            if (fabsf(d_perp) > 1e-6f) {
                actual_r = -(dx * dx + dy * dy) / (2.0f * d_perp);
                if (actual_r < 0.0f) { perp_x = -perp_x; perp_y = -perp_y; actual_r = -actual_r; }
            }
        }
        float fcx = t1x + perp_x * actual_r;
        float fcy = t1y + perp_y * actual_r;

        float sa = atan2f(t1y - fcy, t1x - fcx);
        float ea = atan2f(t2y - fcy, t2x - fcx);

        if (cross2 > 0) {
            while (ea < sa) ea += 2.0f * (float)M_PI;
        } else {
            while (ea > sa) ea -= 2.0f * (float)M_PI;
        }

        /* Insert fillet arc segment */
        route_seg_t *f = &new_segs[new_count++];
        f->type = RSEG_ARC;
        f->cx = fcx; f->cy = fcy;
        f->radius = actual_r;
        f->start_rad = sa;
        f->end_rad = ea;
        f->x0 = f->x1 = 0; f->y0 = f->y1 = 0;

        new_segs[new_count++] = trimmed;
    }

    memcpy(p->segs, new_segs, new_count * sizeof(route_seg_t));
    p->seg_count = new_count;
}

/* Extend path with pre/post segments for sliding animation */
static void rpath_extend(route_path_t *p) {
    if (p->seg_count < 1 || p->seg_count + 2 > RPATH_MAX_SEGS) return;

    float sx, sy, sdx, sdy;
    seg_start_dir(&p->segs[0], &sx, &sy, &sdx, &sdy);

    float ex, ey, edx, edy;
    seg_end_dir(&p->segs[p->seg_count - 1], &ex, &ey, &edx, &edy);

    /* Shift segments up by 1 to make room for pre-segment */
    int i;
    for (i = p->seg_count - 1; i >= 0; i--)
        p->segs[i + 1] = p->segs[i];

    /* Pre-segment: extend backwards from first point */
    p->segs[0].type = RSEG_LINE;
    p->segs[0].x0 = sx - sdx * ROUTE_EXTEND;
    p->segs[0].y0 = sy - sdy * ROUTE_EXTEND;
    p->segs[0].x1 = sx;
    p->segs[0].y1 = sy;
    p->seg_count++;

    /* Post-segment: extend forward from last point */
    p->segs[p->seg_count].type = RSEG_LINE;
    p->segs[p->seg_count].x0 = ex;
    p->segs[p->seg_count].y0 = ey;
    p->segs[p->seg_count].x1 = ex + edx * ROUTE_EXTEND;
    p->segs[p->seg_count].y1 = ey + edy * ROUTE_EXTEND;
    p->seg_count++;
}

/* Derivative of smoothstep — gives speed multiplier at position t.
 * Peak (1.5) at t=0.5, zero at t=0 and t=1. */
static float ease_inout_speed(float t) {
    if (t <= 0.0f || t >= 1.0f) return 0.0f;
    return 6.0f * t * (1.0f - t);
}

/* Compute curvature factor at fraction t along the densified path.
 * Returns 0..1: 1.0 = straight, 0.0 = sharpest turn.
 * Measures direction change over a small window of points. */
static float path_curvature_factor(const route_path_t *p, float t) {
    if (p->pt_count < 3 || p->total_length < 1e-6f) return 1.0f;

    /* Find the point index at fraction t */
    float target_dist = t * p->total_length;
    int idx = 1;
    while (idx < p->pt_count - 1 && p->dist[idx] < target_dist) idx++;

    /* Sample direction change over a window around idx */
    int lo = idx - CURV_WINDOW;
    int hi = idx + CURV_WINDOW;
    if (lo < 0) lo = 0;
    if (hi >= p->pt_count) hi = p->pt_count - 1;
    if (hi - lo < 2) return 1.0f;

    /* Direction at lo and hi */
    float d0x = p->px[lo + 1] - p->px[lo], d0y = p->py[lo + 1] - p->py[lo];
    float d1x = p->px[hi] - p->px[hi - 1], d1y = p->py[hi] - p->py[hi - 1];
    float len0 = sqrtf(d0x * d0x + d0y * d0y);
    float len1 = sqrtf(d1x * d1x + d1y * d1y);
    if (len0 < 1e-6f || len1 < 1e-6f) return 1.0f;

    float dot = (d0x * d1x + d0y * d1y) / (len0 * len1);
    if (dot > 1.0f) dot = 1.0f;
    if (dot < -1.0f) dot = -1.0f;

    /* angle = 0 for straight, PI for reversal */
    float angle = acosf(dot);
    /* Speed factor: 1/(1 + CURV_SLOWDOWN * curvature) */
    float factor = 1.0f / (1.0f + CURV_SLOWDOWN * angle);
    return factor;
}

/* Compute slide window fractions and t_tail/t_head from g_route_slide.
 * slide 0..1 = slide-in (normal), slide 1..2 = push-out through exit. */
/* Sample position on densified path at fraction t (0..1). */
static void rpath_sample(const route_path_t *p, float t, float *out_x, float *out_y) {
    if (p->pt_count < 2) { *out_x = 0; *out_y = 0; return; }
    float target = t * p->total_length;
    int i;
    for (i = 1; i < p->pt_count; i++) {
        if (p->dist[i] >= target) {
            float seg_len = p->dist[i] - p->dist[i-1];
            float frac = (seg_len > 1e-6f) ? (target - p->dist[i-1]) / seg_len : 0.0f;
            *out_x = p->px[i-1] + frac * (p->px[i] - p->px[i-1]);
            *out_y = p->py[i-1] + frac * (p->py[i] - p->py[i-1]);
            return;
        }
    }
    *out_x = p->px[p->pt_count - 1];
    *out_y = p->py[p->pt_count - 1];
}

static float rpath_sample_heading(const route_path_t *p, float t) {
    float target;
    int i;

    if (p->pt_count < 2)
        return (float)(M_PI * 0.5);

    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    target = t * p->total_length;

    for (i = 1; i < p->pt_count; i++) {
        if (p->dist[i] >= target) {
            float dx = p->px[i] - p->px[i - 1];
            float dy = p->py[i] - p->py[i - 1];
            if (dx * dx + dy * dy > 1e-8f)
                return atan2f(dy, dx);
        }
    }

    {
        float dx = p->px[p->pt_count - 1] - p->px[p->pt_count - 2];
        float dy = p->py[p->pt_count - 1] - p->py[p->pt_count - 2];
        if (dx * dx + dy * dy > 1e-8f)
            return atan2f(dy, dx);
    }

    return (float)(M_PI * 0.5);
}

static float wrap_angle(float a) {
    while (a > (float)M_PI) a -= 2.0f * (float)M_PI;
    while (a < -(float)M_PI) a += 2.0f * (float)M_PI;
    return a;
}

static float lerp_angle(float a, float b, float t) {
    return a + wrap_angle(b - a) * t;
}

static float smoothstep01(float t) {
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    return t * t * (3.0f - 2.0f * t);
}

static float release_blend01(float t) {
    float u;

    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    u = 1.0f - t;
    return 1.0f - u * u * u;
}

static float light_settle_curve(float t) {
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;

    if (t < 0.62f) {
        float u = t / 0.62f;
        return 0.80f * u * u * u;
    }

    if (t < 0.84f) {
        float u = (t - 0.62f) / 0.22f;
        float e = 1.0f - powf(1.0f - u, 3.0f);
        return 0.80f + (1.25f - 0.80f) * e;
    }

    {
        float u = (t - 0.84f) / 0.16f;
        float e = u * u * (3.0f - 2.0f * u);
        return 1.25f + (1.0f - 1.25f) * e;
    }
}

static void apply_camera_pose(float pan_x, float pan_y, float rot) {
    g_cam_pan_x = pan_x;
    g_cam_pan_y = pan_y;
    g_cam_rot = rot;
    render_set_camera_pan(pan_x, pan_y);
    render_set_camera_rotation(rot);
    render_sync_camera();
}

static void set_default_camera(void) {
    apply_camera_pose(0.0f, 0.0f, 0.0f);
}

static void update_light_settle(void) {
    float blend;

    if (!g_light_settle_active) {
        render_set_light_rotation(0.0f);
        return;
    }

    blend = light_settle_curve(g_light_settle_t);
    render_set_light_rotation(lerp_angle(g_light_settle_start_rot, 0.0f, blend));

    g_light_settle_t += LIGHT_SETTLE_SPEED;
    if (g_light_settle_t >= 1.0f)
        clear_light_settle();
}

static void update_camera_settle(void) {
    float blend;

    if (!g_cam_settle_active) {
        set_default_camera();
        return;
    }

    blend = smoothstep01(g_cam_settle_t);
    apply_camera_pose(g_cam_settle_start_x * (1.0f - blend),
                      g_cam_settle_start_y * (1.0f - blend),
                      lerp_angle(g_cam_settle_start_rot, 0.0f, blend));

    g_cam_settle_t += CAMERA_SETTLE_SPEED;
    if (g_cam_settle_t >= 1.0f) {
        clear_camera_settle();
        set_default_camera();
    }
}

static void update_combined_camera(void) {
    float total = g_route_path.total_length;
    float head_dist;
    float intro_dist;
    float follow_t;
    float follow_x, follow_y;
    float follow_rot;

    if (total < 1e-6f) {
        set_default_camera();
        return;
    }

    head_dist = g_t_head * total;
    intro_dist = g_next_cam_intro_end_dist;
    follow_t = (g_next_cam_follow_dist > 0.0f) ? (g_next_cam_follow_dist / total) : 0.0f;
    if (follow_t < 0.0f) follow_t = 0.0f;
    if (follow_t > 1.0f) follow_t = 1.0f;

    if (head_dist <= intro_dist) {
        float denom = intro_dist - g_combined_start_head_dist;
        float blend = (denom > 1e-4f) ? (head_dist - g_combined_start_head_dist) / denom : 1.0f;

        rpath_sample(&g_route_path, g_t_head, &follow_x, &follow_y);
        follow_rot = rpath_sample_heading(&g_route_path, g_t_head) - (float)(M_PI * 0.5);
        blend = smoothstep01(blend);
        apply_camera_pose(follow_x * blend, follow_y * blend,
                          lerp_angle(0.0f, follow_rot, blend));
        return;
    }

    if (head_dist <= g_next_cam_follow_dist) {
        rpath_sample(&g_route_path, g_t_head, &follow_x, &follow_y);
        follow_rot = rpath_sample_heading(&g_route_path, g_t_head) - (float)(M_PI * 0.5);
        apply_camera_pose(follow_x, follow_y, follow_rot);
        return;
    }

    rpath_sample(&g_route_path, follow_t, &follow_x, &follow_y);
    follow_rot = rpath_sample_heading(&g_route_path, follow_t) - (float)(M_PI * 0.5);

    {
        float denom = g_next_cam_release_end_dist - g_next_cam_follow_dist;
        float blend = (denom > 1e-4f) ? (head_dist - g_next_cam_follow_dist) / denom : 1.0f;
        float cam_x, cam_y, cam_rot;

        blend = release_blend01(blend);
        cam_x = follow_x + (g_next_cam_pan_x - follow_x) * blend;
        cam_y = follow_y + (g_next_cam_pan_y - follow_y) * blend;
        cam_rot = lerp_angle(follow_rot, g_next_cam_rot, blend);

        apply_camera_pose(cam_x, cam_y, cam_rot);
    }
}

void maneuver_prepare_frame(const maneuver_state_t *s, const maneuver_state_t *next_state) {
    int combined = (next_state != NULL && maneuver_is_pushing());

    g_camera_prepared_this_frame = 0;

    if (s == NULL)
        return;

    if (!combined) {
        if (g_cam_settle_active)
            update_camera_settle();
        else
            set_default_camera();
        if (g_light_settle_active)
            update_light_settle();
        else
            render_set_light_rotation(0.0f);
        g_camera_prepared_this_frame = 1;
    }
}

static void compute_slide_params(void) {
    if (g_route_path.total_length < 1e-6f) {
        g_route_pre_frac = 0.0f;
        g_route_end_frac = 1.0f;
        g_t_tail = 0.0f;
        g_t_head = (g_route_slide < 1.0f) ? g_route_slide : 1.0f;
        return;
    }
    g_route_pre_frac = ROUTE_EXTEND / g_route_path.total_length;
    g_route_end_frac = (g_route_path.total_length - ROUTE_EXTEND) / g_route_path.total_length;

    if (g_combined_window_active) {
        float range = g_anim_target - g_anim_start;
        float progress = (range > 0.01f) ? (g_route_slide - g_anim_start) / range : 1.0f;
        if (progress < 0.0f) progress = 0.0f;
        if (progress > 1.0f) progress = 1.0f;

        float tail_dist = g_combined_start_tail_dist +
                          (g_combined_end_tail_dist - g_combined_start_tail_dist) * progress;
        float head_dist = g_combined_start_head_dist +
                          (g_combined_end_head_dist - g_combined_start_head_dist) * progress;

        if (tail_dist < 0.0f) tail_dist = 0.0f;
        if (head_dist < 0.0f) head_dist = 0.0f;
        if (tail_dist > g_route_path.total_length) tail_dist = g_route_path.total_length;
        if (head_dist > g_route_path.total_length) head_dist = g_route_path.total_length;

        g_t_tail = tail_dist / g_route_path.total_length;
        g_t_head = head_dist / g_route_path.total_length;
        return;
    }

    /* slug_frac = fraction of path that the arrow covers.
     * For single maneuver: entire road = end_frac - pre_frac.
     * For combined path: one maneuver's worth (stored in g_slug_override). */
    float slug_frac;
    if (g_slug_override > 0.0f)
        slug_frac = g_slug_override / g_route_path.total_length;
    else
        slug_frac = g_route_end_frac - g_route_pre_frac;

    g_t_head = g_route_pre_frac + g_route_slide * slug_frac;
    g_t_tail = g_t_head - slug_frac;
    if (g_t_tail < 0.0f) g_t_tail = 0.0f;
    /* Clamp to valid range — push-out moves both head and tail forward */
    if (g_t_head > 1.0f) g_t_head = 1.0f;
    if (g_t_tail > 1.0f) g_t_tail = 1.0f;
}

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

/* Build a route path with arc fillets at interior vertices.
 * At each interior point where two segments meet, replace the sharp corner
 * with a circular arc tangent to both adjacent segments.
 * fillet_r = desired fillet radius (clamped to half min adjacent segment). */
static void build_filleted_path(route_path_t *p,
                                 const float *xs, const float *ys, int n,
                                 float fillet_r) {
    int i;
    rpath_clear(p);
    if (n < 2) return;
    if (n == 2) { rpath_add_line(p, xs[0], ys[0], xs[1], ys[1]); return; }

    /* For each interior vertex i (1..n-2), compute fillet arcs. */
    /* We emit: line(prev_end → T1), arc(T1 → T2), then next segment starts at T2. */

    /* Track where the current segment starts (after previous fillet trimmed it) */
    float cur_sx = xs[0], cur_sy = ys[0];

    for (i = 1; i < n - 1; i++) {
        /* Incoming direction: from xs[i-1],ys[i-1] → xs[i],ys[i] */
        float in_dx = xs[i] - xs[i-1], in_dy = ys[i] - ys[i-1];
        float in_len = sqrtf(in_dx * in_dx + in_dy * in_dy);
        /* Outgoing direction: from xs[i],ys[i] → xs[i+1],ys[i+1] */
        float out_dx = xs[i+1] - xs[i], out_dy = ys[i+1] - ys[i];
        float out_len = sqrtf(out_dx * out_dx + out_dy * out_dy);

        if (in_len < 1e-6f || out_len < 1e-6f) {
            /* Degenerate segment — just emit line to vertex */
            rpath_add_line(p, cur_sx, cur_sy, xs[i], ys[i]);
            cur_sx = xs[i]; cur_sy = ys[i];
            continue;
        }

        /* Unit vectors */
        float in_ux = in_dx / in_len, in_uy = in_dy / in_len;
        float out_ux = out_dx / out_len, out_uy = out_dy / out_len;

        /* Cross product (determines turn direction) and dot product */
        float cross = in_ux * out_uy - in_uy * out_ux;
        float dot = in_ux * out_ux + in_uy * out_uy;

        /* Deflection angle (0 = straight, π = U-turn) */
        float alpha = acosf(dot < -1.0f ? -1.0f : (dot > 1.0f ? 1.0f : dot));

        /* Skip fillet for near-straight (<5°) or near-reversal (>175°) */
        if (alpha < 0.09f || alpha > (float)M_PI * 0.97f) {
            rpath_add_line(p, cur_sx, cur_sy, xs[i], ys[i]);
            cur_sx = xs[i]; cur_sy = ys[i];
            continue;
        }

        /* Vertex half-angle = (π - alpha)/2.
         * Tangent distance from vertex to tangent point = R / tan(vertex_half). */
        float vertex_half = ((float)M_PI - alpha) * 0.5f;
        float tan_vh = tanf(vertex_half);
        if (tan_vh < 1e-6f) { tan_vh = 1e-6f; }
        float t_dist = fillet_r / tan_vh;

        /* Clamp to half of each adjacent segment's available length */
        float max_in = in_len * 0.45f;
        float max_out = out_len * 0.45f;
        float max_t = max_in < max_out ? max_in : max_out;
        if (t_dist > max_t) {
            t_dist = max_t;
        }
        float actual_r = t_dist * tan_vh;

        /* Tangent points */
        float t1x = xs[i] - in_ux * t_dist, t1y = ys[i] - in_uy * t_dist;
        float t2x = xs[i] + out_ux * t_dist, t2y = ys[i] + out_uy * t_dist;

        /* Arc center: perpendicular to incoming at T1, towards the turn */
        /* cross > 0 → left turn, cross < 0 → right turn */
        float perp_x, perp_y;
        if (cross > 0) {
            perp_x = -in_uy; perp_y = in_ux;   /* left perpendicular */
        } else {
            perp_x = in_uy; perp_y = -in_ux;    /* right perpendicular */
        }
        float cx = t1x + perp_x * actual_r;
        float cy = t1y + perp_y * actual_r;

        /* Arc angles (math convention: atan2) */
        float start_ang = atan2f(t1y - cy, t1x - cx);
        float end_ang = atan2f(t2y - cy, t2x - cx);

        /* Ensure correct winding: right turn = CW (decreasing angle),
         * left turn = CCW (increasing angle) */
        if (cross > 0) {
            /* Left turn → CCW → end > start */
            while (end_ang < start_ang) end_ang += 2.0f * (float)M_PI;
        } else {
            /* Right turn → CW → end < start */
            while (end_ang > start_ang) end_ang -= 2.0f * (float)M_PI;
        }

        /* Emit: line to T1, arc T1→T2 */
        rpath_add_line(p, cur_sx, cur_sy, t1x, t1y);
        rpath_add_arc(p, cx, cy, actual_r, start_ang, end_ang);

        /* Next segment starts at T2 */
        cur_sx = t2x; cur_sy = t2y;
    }

    /* Final segment: cur → last point */
    rpath_add_line(p, cur_sx, cur_sy, xs[n-1], ys[n-1]);
}

/* ================================================================
 * Route path building (standalone, no mask rendering)
 * ================================================================ */

/* Build route path for a single maneuver — raw segments only.
 * No rpath_extend, no rpath_densify, no rpath_extrude.
 * Sets arrow position. Used by draw_* functions and for path chaining. */
void maneuver_build_route(const maneuver_state_t *state, route_path_t *path) {
    switch (state->icon) {
    case ICON_STRAIGHT: {
        float straight_end = ARRIVE_ROAD_TOP - HEAD_SZ;
        rpath_clear(path);
        rpath_add_line(path, 0, SHAFT_BOT, 0, straight_end);
        rpath_set_arrow(path, 0, straight_end, (float)(M_PI * 0.5));
        break;
    }
    case ICON_TURN: {
        float angle_rad = (float)state->exit_angle * (float)M_PI / 180.0f;
        float end_x = BLUE_LEN * sinf(angle_rad);
        float end_y = BLUE_LEN * cosf(angle_rad);
        float pts_x[] = {0, 0, end_x};
        float pts_y[] = {SHAFT_BOT, 0, end_y};
        build_filleted_path(path, pts_x, pts_y, 3, JOINT_R);
        float head_angle = (float)(M_PI * 0.5) - angle_rad;
        rpath_set_arrow(path, end_x, end_y, head_angle);
        break;
    }
    case ICON_UTURN: {
        int go_left = (state->driving_side == 0) ? 1 : 0;
        if (state->direction != 0) go_left = (state->direction < 0) ? 1 : 0;
        float enter_x = go_left ?  UTURN_GAP : -UTURN_GAP;
        float exit_x  = go_left ? -UTURN_GAP :  UTURN_GAP;
        float top_y   = UTURN_TOP;
        rpath_clear(path);
        rpath_add_line(path, enter_x, SHAFT_BOT, enter_x, top_y);
        if (go_left) {
            rpath_add_arc(path, 0, top_y, UTURN_GAP, 0, (float)M_PI);
        } else {
            rpath_add_arc(path, 0, top_y, UTURN_GAP, (float)M_PI, 2.0f * (float)M_PI);
        }
        rpath_add_line(path, exit_x, top_y, exit_x, UTURN_ARROW);
        rpath_fillet_junctions(path, JOINT_R);
        rpath_set_arrow(path, exit_x, UTURN_ARROW, (float)(-M_PI * 0.5));
        break;
    }
    case ICON_ROUNDABOUT: {
        float cx = 0.0f, cy = 0.0f;
        float ring_r = RAB_RING_R;
        float entry_rad = (float)(-M_PI * 0.5);
        /* Snap exit angle */
        float snapped_exit_deg = (float)state->exit_angle;
        if (state->junction_angle_count > 0) {
            int best = 0; float best_diff = SNAP_SENTINEL; int j;
            for (j = 0; j < state->junction_angle_count && j < MAX_JUNCTION_ANGLES; j++) {
                float diff = fabsf((float)state->junction_angles[j] - (float)state->exit_angle);
                if (diff > 180.0f) diff = 360.0f - diff;
                if (diff < best_diff) { best_diff = diff; best = j; }
            }
            float entry_diff = fabsf(-180.0f - (float)state->exit_angle);
            if (entry_diff > 180.0f) entry_diff = 360.0f - entry_diff;
            if (entry_diff < best_diff)
                snapped_exit_deg = -180.0f;
            else
                snapped_exit_deg = (float)state->junction_angles[best];
        }
        float exit_rad = (90.0f - snapped_exit_deg) * (float)M_PI / 180.0f;
        float ext = BLUE_LEN - ring_r;
        float entry_pt_x = cx + ring_r * cosf(entry_rad);
        float entry_pt_y = cy + ring_r * sinf(entry_rad);
        float ex0_x = cx + ring_r * cosf(exit_rad);
        float ex0_y = cy + ring_r * sinf(exit_rad);
        float ex_tip_x = cx + (ring_r + ext) * cosf(exit_rad);
        float ex_tip_y = cy + (ring_r + ext) * sinf(exit_rad);
        float arc_s = entry_rad, arc_e = exit_rad;
        if (state->driving_side == 0) {
            while (arc_e <= arc_s) arc_e += 2.0f * (float)M_PI;
        } else {
            while (arc_e >= arc_s) arc_e -= 2.0f * (float)M_PI;
        }
        rpath_clear(path);
        rpath_add_line(path, 0, SHAFT_BOT, entry_pt_x, entry_pt_y);
        rpath_add_arc(path, cx, cy, ring_r, arc_s, arc_e);
        rpath_add_line(path, ex0_x, ex0_y, ex_tip_x, ex_tip_y);
        rpath_fillet_junctions(path, JOINT_R);
        rpath_set_arrow(path, ex_tip_x, ex_tip_y, exit_rad);
        break;
    }
    case ICON_MERGE: {
        int go_right = (state->direction > 0) ? 1 : 0;
        float sign = go_right ? 1.0f : -1.0f;
        float start_x = sign * -MERGE_OFFSET;
        float pts_x[] = {start_x, start_x, 0, 0};
        float pts_y[] = {SHAFT_BOT, MERGE_BEND_LO, BEND_HI, BLUE_LEN};
        build_filleted_path(path, pts_x, pts_y, 4, JOINT_R);
        rpath_set_arrow(path, 0, BLUE_LEN, (float)(M_PI * 0.5));
        break;
    }
    case ICON_LANE_CHANGE: {
        int go_left = (state->direction < 0) ? 1 : 0;
        float sign = go_left ? -1.0f : 1.0f;
        float shift = sign * LANE_SHIFT;
        float pts_x[] = {0, 0, shift, shift};
        float pts_y[] = {SHAFT_BOT, BEND_LO, BEND_HI, BLUE_LEN};
        build_filleted_path(path, pts_x, pts_y, 4, JOINT_R);
        rpath_set_arrow(path, shift, BLUE_LEN, (float)(M_PI * 0.5));
        break;
    }
    case ICON_ARRIVED:
    default: {
        float route_end = ARRIVE_ROAD_TOP - HEAD_SZ;
        rpath_clear(path);
        rpath_add_line(path, 0, SHAFT_BOT, 0, route_end);
        rpath_set_arrow(path, 0, route_end, (float)(M_PI * 0.5));
        break;
    }
    }
}

/* Get exit point and heading for a maneuver (for chaining). */
maneuver_exit_t maneuver_get_exit(const maneuver_state_t *state) {
    maneuver_exit_t ex = {0, 0, (float)(M_PI * 0.5)};
    switch (state->icon) {
    case ICON_STRAIGHT: {
        ex.x = 0; ex.y = ARRIVE_ROAD_TOP - HEAD_SZ;
        ex.heading = (float)(M_PI * 0.5);
        break;
    }
    case ICON_TURN: {
        float a = (float)state->exit_angle * (float)M_PI / 180.0f;
        ex.x = BLUE_LEN * sinf(a);
        ex.y = BLUE_LEN * cosf(a);
        ex.heading = (float)(M_PI * 0.5) - a;
        break;
    }
    case ICON_UTURN: {
        int go_left = (state->driving_side == 0) ? 1 : 0;
        if (state->direction != 0) go_left = (state->direction < 0) ? 1 : 0;
        ex.x = go_left ? -UTURN_GAP : UTURN_GAP;
        ex.y = UTURN_ARROW;
        ex.heading = (float)(-M_PI * 0.5);
        break;
    }
    case ICON_ROUNDABOUT: {
        float ring_r = RAB_RING_R;
        float snapped_exit_deg = (float)state->exit_angle;
        if (state->junction_angle_count > 0) {
            int best = 0; float best_diff = SNAP_SENTINEL; int j;
            for (j = 0; j < state->junction_angle_count && j < MAX_JUNCTION_ANGLES; j++) {
                float diff = fabsf((float)state->junction_angles[j] - (float)state->exit_angle);
                if (diff > 180.0f) diff = 360.0f - diff;
                if (diff < best_diff) { best_diff = diff; best = j; }
            }
            float entry_diff = fabsf(-180.0f - (float)state->exit_angle);
            if (entry_diff > 180.0f) entry_diff = 360.0f - entry_diff;
            if (entry_diff < best_diff) snapped_exit_deg = -180.0f;
            else snapped_exit_deg = (float)state->junction_angles[best];
        }
        float exit_rad = (90.0f - snapped_exit_deg) * (float)M_PI / 180.0f;
        float ext = BLUE_LEN - ring_r;
        ex.x = (ring_r + ext) * cosf(exit_rad);
        ex.y = (ring_r + ext) * sinf(exit_rad);
        ex.heading = exit_rad;
        break;
    }
    case ICON_MERGE:
        ex.x = 0; ex.y = BLUE_LEN;
        ex.heading = (float)(M_PI * 0.5);
        break;
    case ICON_LANE_CHANGE: {
        int go_left = (state->direction < 0) ? 1 : 0;
        float sign = go_left ? -1.0f : 1.0f;
        ex.x = sign * LANE_SHIFT;
        ex.y = BLUE_LEN;
        ex.heading = (float)(M_PI * 0.5);
        break;
    }
    default: /* ARRIVED — terminal, no meaningful exit */
        ex.x = 0; ex.y = ARRIVE_ROAD_TOP - HEAD_SZ;
        ex.heading = (float)(M_PI * 0.5);
        break;
    }
    return ex;
}

static maneuver_exit_t route_path_get_start_pose(const route_path_t *path) {
    maneuver_exit_t st = {0, SHAFT_BOT, (float)(M_PI * 0.5)};
    if (path->seg_count > 0) {
        float sx, sy, sdx, sdy;
        seg_start_dir(&path->segs[0], &sx, &sy, &sdx, &sdy);
        st.x = sx;
        st.y = sy;
        st.heading = atan2f(sdy, sdx);
    }
    return st;
}

static maneuver_exit_t route_path_get_end_pose(const route_path_t *path) {
    maneuver_exit_t ex = {0, ARRIVE_ROAD_TOP - HEAD_SZ, (float)(M_PI * 0.5)};
    if (path->seg_count > 0) {
        float exx, exy, edx, edy;
        seg_end_dir(&path->segs[path->seg_count - 1], &exx, &exy, &edx, &edy);
        ex.x = exx;
        ex.y = exy;
        ex.heading = atan2f(edy, edx);
    }
    return ex;
}

static void compute_combined_transform(const maneuver_state_t *cur_state,
                                       const maneuver_state_t *next_state,
                                       float *out_tx, float *out_ty,
                                       float *out_cos_r, float *out_sin_r,
                                       float *out_rot);

static void bounds_include_pt(float x, float y, float *max_abs_x, float *max_abs_y) {
    float ax = fabsf(x);
    float ay = fabsf(y);

    if (ax > *max_abs_x) *max_abs_x = ax;
    if (ay > *max_abs_y) *max_abs_y = ay;
}

static void bounds_include_box(float tx, float ty, float cos_r, float sin_r,
                               float half_w, float half_h,
                               float *max_abs_x, float *max_abs_y) {
    static const float k_corners[4][2] = {
        { -1.0f, -1.0f }, { 1.0f, -1.0f }, { 1.0f, 1.0f }, { -1.0f, 1.0f }
    };
    int i;

    for (i = 0; i < 4; i++) {
        float lx = k_corners[i][0] * half_w;
        float ly = k_corners[i][1] * half_h;
        float x = cos_r * lx - sin_r * ly + tx;
        float y = sin_r * lx + cos_r * ly + ty;
        bounds_include_pt(x, y, max_abs_x, max_abs_y);
    }
}

void maneuver_get_transition_mask_bounds(float *out_abs_x, float *out_abs_y) {
    const float local_half_w = ROAD_LEN + FADE_LEN + OL_T * 0.5f;
    const float local_half_h = ROAD_LEN + FADE_LEN + OL_T * 0.5f;
    const float safety = 1.05f;
    const maneuver_state_t samples[] = {
        { .icon = ICON_STRAIGHT },
        { .icon = ICON_TURN, .exit_angle = 30 },
        { .icon = ICON_TURN, .exit_angle = 90 },
        { .icon = ICON_TURN, .exit_angle = 135 },
        { .icon = ICON_TURN, .exit_angle = -30 },
        { .icon = ICON_TURN, .exit_angle = -90 },
        { .icon = ICON_TURN, .exit_angle = -135 },
        { .icon = ICON_UTURN, .driving_side = 0 },
        { .icon = ICON_UTURN, .driving_side = 1 },
        { .icon = ICON_MERGE, .direction = -1 },
        { .icon = ICON_MERGE, .direction = 1 },
        { .icon = ICON_LANE_CHANGE, .direction = -1 },
        { .icon = ICON_LANE_CHANGE, .direction = 1 },
        { .icon = ICON_ROUNDABOUT, .exit_angle = 90, .driving_side = 0 },
        { .icon = ICON_ROUNDABOUT, .exit_angle = 0, .driving_side = 0 },
        { .icon = ICON_ROUNDABOUT, .exit_angle = -90, .driving_side = 0 },
        { .icon = ICON_ROUNDABOUT, .exit_angle = 180, .driving_side = 0 },
        { .icon = ICON_ROUNDABOUT, .exit_angle = 90, .driving_side = 1 },
        { .icon = ICON_ROUNDABOUT, .exit_angle = 0, .driving_side = 1 },
        { .icon = ICON_ROUNDABOUT, .exit_angle = -90, .driving_side = 1 },
        { .icon = ICON_ROUNDABOUT, .exit_angle = 180, .driving_side = 1 },
        { .icon = ICON_ARRIVED },
    };
    float max_abs_x = local_half_w;
    float max_abs_y = local_half_h;
    int i, j;

    for (i = 0; i < (int)(sizeof(samples) / sizeof(samples[0])); i++) {
        for (j = 0; j < (int)(sizeof(samples) / sizeof(samples[0])); j++) {
            float tx, ty, cos_r, sin_r, rot;

            compute_combined_transform(&samples[i], &samples[j], &tx, &ty, &cos_r, &sin_r, &rot);
            bounds_include_box(0.0f, 0.0f, 1.0f, 0.0f,
                               local_half_w, local_half_h,
                               &max_abs_x, &max_abs_y);
            bounds_include_box(tx, ty, cos_r, sin_r,
                               local_half_w, local_half_h,
                               &max_abs_x, &max_abs_y);
        }
    }

    if (out_abs_x != NULL) *out_abs_x = max_abs_x * safety;
    if (out_abs_y != NULL) *out_abs_y = max_abs_y * safety;
}

static void compute_combined_transform(const maneuver_state_t *cur_state,
                                       const maneuver_state_t *next_state,
                                       float *out_tx, float *out_ty,
                                       float *out_cos_r, float *out_sin_r,
                                       float *out_rot) {
    route_path_t cur_path, next_path;
    maneuver_exit_t cur_end, next_start;
    float rot, cos_r, sin_r;
    float ns_rx, ns_ry;

    maneuver_build_route(cur_state, &cur_path);
    rpath_extend(&cur_path);
    maneuver_build_route(next_state, &next_path);
    rpath_extend(&next_path);

    cur_end = route_path_get_end_pose(&cur_path);
    next_start = route_path_get_start_pose(&next_path);
    rot = cur_end.heading - next_start.heading;
    cos_r = cosf(rot);
    sin_r = sinf(rot);
    ns_rx = cos_r * next_start.x - sin_r * next_start.y;
    ns_ry = sin_r * next_start.x + cos_r * next_start.y;

    *out_tx = cur_end.x - ns_rx;
    *out_ty = cur_end.y - ns_ry;
    *out_cos_r = cos_r;
    *out_sin_r = sin_r;
    *out_rot = rot;
}

static void compute_combined_mask_transform(const maneuver_state_t *cur_state,
                                            const maneuver_state_t *next_state,
                                            float *out_tx, float *out_ty,
                                            float *out_cos_r, float *out_sin_r,
                                            float *out_rot) {
    compute_combined_transform(cur_state, next_state,
                               out_tx, out_ty, out_cos_r, out_sin_r, out_rot);
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

    /* Build route path — blue line same length as arrived */
    float straight_end = ARRIVE_ROAD_TOP - HEAD_SZ;
    rpath_clear(&g_route_path);
    rpath_add_line(&g_route_path, 0, SHAFT_BOT, 0, straight_end);
    rpath_extend(&g_route_path);
    rpath_densify(&g_route_path);
    rpath_set_arrow(&g_route_path, 0, straight_end, (float)(M_PI * 0.5));
    compute_slide_params();
    rpath_extrude(&g_route_path, &g_route_mesh, SHAFT_T, ROUTE_BASE_Y, ROUTE_TOP_Y, g_t_tail, g_t_head);

    if (!g_masks_only_mode) {
        render_composite();
        rpath_draw(&g_route_mesh, AC_R, AC_G, AC_B, AC_A);
    }
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

    /* Build route path with fillet at corner */
    {
        float pts_x[] = {0, 0, end_x};
        float pts_y[] = {SHAFT_BOT, 0, end_y};
        build_filleted_path(&g_route_path, pts_x, pts_y, 3, JOINT_R);
        rpath_extend(&g_route_path);
        rpath_densify(&g_route_path);
        float head_angle = (float)(M_PI * 0.5) - angle_rad;
        rpath_set_arrow(&g_route_path, end_x, end_y, head_angle);
        compute_slide_params();
        rpath_extrude(&g_route_path, &g_route_mesh, SHAFT_T, ROUTE_BASE_Y, ROUTE_TOP_Y, g_t_tail, g_t_head);
    }

    if (!g_masks_only_mode) {
        render_composite();
        rpath_draw(&g_route_mesh, AC_R, AC_G, AC_B, AC_A);
    }
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
    rpath_fillet_junctions(&g_route_path, JOINT_R);
    rpath_extend(&g_route_path);
    rpath_densify(&g_route_path);
    rpath_set_arrow(&g_route_path, exit_x, arrow_y, (float)(-M_PI * 0.5));
    compute_slide_params();
    rpath_extrude(&g_route_path, &g_route_mesh, SHAFT_T, ROUTE_BASE_Y, ROUTE_TOP_Y, g_t_tail, g_t_head);

    if (!g_masks_only_mode) {
        render_composite();
        rpath_draw(&g_route_mesh, AC_R, AC_G, AC_B, AC_A);
    }
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

        float entry_pt_x = cx + ring_r * cosf(entry_rad);
        float entry_pt_y = cy + ring_r * sinf(entry_rad);

        rpath_clear(&g_route_path);
        /* Entry shaft → ring entry point */
        rpath_add_line(&g_route_path, 0, SHAFT_BOT, entry_pt_x, entry_pt_y);
        /* Ring arc */
        rpath_add_arc(&g_route_path, cx, cy, ring_r, arc_s, arc_e);
        /* Exit stub */
        rpath_add_line(&g_route_path, ex0_x, ex0_y, ex_tip_x, ex_tip_y);
        rpath_fillet_junctions(&g_route_path, JOINT_R);
        rpath_extend(&g_route_path);
        rpath_densify(&g_route_path);
        rpath_set_arrow(&g_route_path, ex_tip_x, ex_tip_y, exit_rad);
        compute_slide_params();
        rpath_extrude(&g_route_path, &g_route_mesh, SHAFT_T, ROUTE_BASE_Y, ROUTE_TOP_Y, g_t_tail, g_t_head);
    }

    if (!g_masks_only_mode) {
        render_composite();
        rpath_draw(&g_route_mesh, AC_R, AC_G, AC_B, AC_A);
    }
}

/* ----------------------------------------------------------------
 * draw_arrived — outline/fill/route passes
 * ---------------------------------------------------------------- */

static void draw_arrived(int dir) {
    float road_top = ARRIVE_ROAD_TOP;
    (void)dir;

    render_set_raised(0);

    /* === OUTLINE MASK === */
    render_begin_outline_mask();
    draw_fading_road(0, SHAFT_BOT, 0, SHAFT_BOT - FADE_LEN, 1.0f, FADE_OUTLINE);
    render_thick_line(0, SHAFT_BOT, 0, road_top, OL_T, WHITE);
    render_disc(0, road_top, OL_T * 0.5f, ARRIVE_SEG, WHITE);
    render_end_outline_mask();

    /* === FILL MASK === */
    render_begin_fill_mask();
    draw_fading_road(0, SHAFT_BOT, 0, SHAFT_BOT - FADE_LEN, 1.0f, FADE_GREY);
    render_thick_line(0, SHAFT_BOT, 0, road_top, SIDE_T, SIDE);
    render_disc(0, road_top, SIDE_T * 0.5f, ARRIVE_SEG, SIDE);
    render_end_fill_mask();

    /* === ROUTE PATH === */
    /* Pull route back so arrow TIP lands at flag base (arrow extends HEAD_SZ beyond endpoint) */
    float route_end = road_top - HEAD_SZ;
    rpath_clear(&g_route_path);
    rpath_add_line(&g_route_path, 0, SHAFT_BOT, 0, route_end);
    rpath_extend(&g_route_path);
    rpath_densify(&g_route_path);
    rpath_set_arrow(&g_route_path, 0, route_end, (float)(M_PI * 0.5));
    compute_slide_params();
    rpath_extrude(&g_route_path, &g_route_mesh, SHAFT_T, ROUTE_BASE_Y, ROUTE_TOP_Y, g_t_tail, g_t_head);

    if (!g_masks_only_mode) {
        render_composite();

        /* Animated destination flag — drawn before route so blue line overlays it */
        render_sprite_flag(0, ARRIVE_FLAG_Y, ARRIVE_FLAG_SZ, (int)g_flag_frame);

        rpath_draw(&g_route_mesh, AC_R, AC_G, AC_B, AC_A);
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

    /* Build route path with fillets at S-bend corners */
    {
        float pts_x[] = {0, 0, shift, shift};
        float pts_y[] = {SHAFT_BOT, bend_lo, bend_hi, BLUE_LEN};
        build_filleted_path(&g_route_path, pts_x, pts_y, 4, JOINT_R);
        rpath_extend(&g_route_path);
        rpath_densify(&g_route_path);
        rpath_set_arrow(&g_route_path, shift, BLUE_LEN, (float)(M_PI * 0.5));
        compute_slide_params();
        rpath_extrude(&g_route_path, &g_route_mesh, SHAFT_T, ROUTE_BASE_Y, ROUTE_TOP_Y, g_t_tail, g_t_head);
    }

    if (!g_masks_only_mode) {
        render_composite();
        rpath_draw(&g_route_mesh, AC_R, AC_G, AC_B, AC_A);
    }
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

    /* Build route path with fillets at S-bend corners */
    {
        float pts_x[] = {start_x, start_x, 0, 0};
        float pts_y[] = {SHAFT_BOT, bend_lo, bend_hi, BLUE_LEN};
        build_filleted_path(&g_route_path, pts_x, pts_y, 4, JOINT_R);
        rpath_extend(&g_route_path);
        rpath_densify(&g_route_path);
        rpath_set_arrow(&g_route_path, 0, BLUE_LEN, (float)(M_PI * 0.5));
        compute_slide_params();
        rpath_extrude(&g_route_path, &g_route_mesh, SHAFT_T, ROUTE_BASE_Y, ROUTE_TOP_Y, g_t_tail, g_t_head);
    }

    if (!g_masks_only_mode) {
        render_composite();
        rpath_draw(&g_route_mesh, AC_R, AC_G, AC_B, AC_A);
    }
}

/* ================================================================
 * Main dispatch
 * ================================================================ */

void maneuver_draw(const maneuver_state_t *s, const maneuver_state_t *next_state) {
    /* Track whether flag animation is active */
    g_flag_active = (s->icon == ICON_ARRIVED);

    /* Advance flag animation (always running for arrived) */
    if (g_flag_active) {
        g_flag_frame += FLAG_ANIM_SPEED;
        if (g_flag_frame >= 14.0f)
            g_flag_frame = fmodf(g_flag_frame, 14.0f);
    }

    /* Combined path: when pushing with a known next maneuver, build joined route
     * after the draw_* pass so arrow slides continuously between maneuvers. */
    int combined = (next_state != NULL && maneuver_is_pushing());
    if (!combined)
        g_combined_window_active = 0;

    if (!combined) {
        if (!g_camera_prepared_this_frame) {
            if (g_cam_settle_active)
                update_camera_settle();
            else
                set_default_camera();
        }
    }
    g_camera_prepared_this_frame = 0;

    /* Compute length-normalized slide delta.
     * Speed uses the slug length (one maneuver's road) for consistent animation pace,
     * even when the path is a combined two-maneuver path. */
    float orig_len = (g_slug_override > 0.0f) ? g_slug_override
                   : (g_route_path.total_length - 2.0f * ROUTE_EXTEND);
    if (orig_len < 0.1f) orig_len = 0.1f;

    /* Unified animation: slide from g_anim_start → g_anim_target.
     * Progress normalized to [0,1] within that range.
     * Ease in/out + curvature slowdown on turns. */
    if (g_route_animating) {
        float range = g_anim_target - g_anim_start;
        if (range < 0.01f) range = 0.01f;
        float progress = (g_route_slide - g_anim_start) / range;
        float e_speed = ease_inout_speed(progress);
        if (e_speed < 0.15f) e_speed = 0.15f;
        float curv = path_curvature_factor(&g_route_path, g_t_head);
        float speed = ROUTE_SPEED_MIN + (ROUTE_SPEED_PEAK - ROUTE_SPEED_MIN) * e_speed * curv;
        g_route_slide += speed / orig_len;
        if (g_route_slide >= g_anim_target) {
            g_route_slide = g_anim_target;
            g_route_animating = 0;
        }
    }

    /* If masks are cached and still valid, just re-composite + rebuild mesh at current slide
     * (handles perspective animation and route animation without re-rendering masks) */
    if (!render_masks_dirty()) {
        compute_slide_params();
        if (combined)
            update_combined_camera();
        if (g_route_animating || g_route_slide != 1.0f) {
            /* Rebuild mesh at current slide (path segments still cached in g_route_path) */
            rpath_extrude(&g_route_path, &g_route_mesh, SHAFT_T, ROUTE_BASE_Y, ROUTE_TOP_Y, g_t_tail, g_t_head);
        }
        render_composite();
        if (s->icon == ICON_ARRIVED)
            render_sprite_flag(0, ARRIVE_FLAG_Y, ARRIVE_FLAG_SZ, (int)g_flag_frame);
        rpath_draw(&g_route_mesh, AC_R, AC_G, AC_B, AC_A);
        if (g_route_debug)
            rpath_draw_debug(&g_route_path, g_t_tail, g_t_head);
        return;
    }

    /* Dispatch mask + route rendering for a state */
    #define DISPATCH_DRAW(st) do { \
        switch ((st)->icon) { \
            case ICON_STRAIGHT: draw_straight((st)->junction_angles, (st)->junction_angle_count); break; \
            case ICON_TURN: draw_turn((float)(st)->exit_angle, (st)->junction_angles, (st)->junction_angle_count); break; \
            case ICON_UTURN: draw_uturn((st)->driving_side != 1); break; \
            case ICON_MERGE: draw_merge((st)->direction > 0); break; \
            case ICON_LANE_CHANGE: draw_lane_change((st)->direction < 0); break; \
            case ICON_ROUNDABOUT: draw_roundabout((float)(st)->exit_angle, (st)->driving_side, (st)->junction_angles, (st)->junction_angle_count); break; \
            case ICON_ARRIVED: draw_arrived((st)->direction); break; \
            default: break; \
        } \
    } while(0)

    /* Render current maneuver masks + composite */
    DISPATCH_DRAW(s);

    /* For combined transition: also render next maneuver's masks (appended),
     * then re-composite with both, and draw combined route. */
    if (combined) {
        float rot, cos_r, sin_r;
        float tx, ty;

        compute_combined_mask_transform(s, next_state, &tx, &ty, &cos_r, &sin_r, &rot);

        render_set_mask_append(1);
        g_masks_only_mode = 1;
        render_push_mask_transform(tx, ty, cos_r, sin_r);
        DISPATCH_DRAW(next_state);
        render_pop_mask_transform();
        render_set_mask_append(0);
        g_masks_only_mode = 0;
    }

    #undef DISPATCH_DRAW

    /* When combined, rebuild joined route path and draw it */
    if (combined) {
        /* Rebuild combined path (draw_* overwrote g_route_path).
         * Each maneuver keeps its own pre/post extension so the handoff has
         * spacing, and the FBO uses the same transform. */
        route_path_t next_path;
        float rot, cos_r, sin_r;
        float tx, ty;
        maneuver_build_route(s, &g_route_path);

        /* Measure first maneuver's road length BEFORE extending —
         * use it for speed normalization during the combined handoff. */
        rpath_densify(&g_route_path);
        g_slug_override = g_route_path.total_length;
        rpath_extend(&g_route_path);

        maneuver_build_route(next_state, &next_path);
        rpath_densify(&next_path);
        float next_road_len = next_path.total_length;
        rpath_extend(&next_path);
        compute_combined_transform(s, next_state, &tx, &ty, &cos_r, &sin_r, &rot);
        g_last_combined_rot = rot;

        rpath_xform_append(&g_route_path, &next_path, tx, ty, cos_r, sin_r, rot);
        float ax = cos_r * next_path.arrow_x - sin_r * next_path.arrow_y + tx;
        float ay = sin_r * next_path.arrow_x + cos_r * next_path.arrow_y + ty;
        rpath_set_arrow(&g_route_path, ax, ay, next_path.arrow_angle + rot);
        rpath_densify(&g_route_path);

        /* Morph the visible blue body from the current maneuver window to the
         * committed next-maneuver window so the handoff lands on the exact
         * standalone next-state pose. */
        {
            float current_total = g_slug_override + 2.0f * ROUTE_EXTEND;
            float start_head = ROUTE_EXTEND + g_anim_start * g_slug_override;
            float start_tail = start_head - g_slug_override;
            float end_head = g_route_path.total_length - ROUTE_EXTEND;
            float end_tail = end_head - next_road_len;
            float start_mid, end_mid, transition_len;

            if (start_head < 0.0f) start_head = 0.0f;
            if (start_head > current_total) start_head = current_total;
            if (start_tail < 0.0f) start_tail = 0.0f;
            if (start_tail > current_total) start_tail = current_total;
            if (end_tail < 0.0f) end_tail = 0.0f;

            g_combined_window_active = 1;
            g_combined_start_tail_dist = start_tail;
            g_combined_start_head_dist = start_head;
            g_combined_end_tail_dist = end_tail;
            g_combined_end_head_dist = end_head;
            g_next_cam_pan_x = tx;
            g_next_cam_pan_y = ty;
            g_next_cam_rot = rot;
            g_next_cam_follow_dist = current_total + ROUTE_EXTEND;
            g_next_cam_intro_end_dist = start_head + 0.30f * g_slug_override;
            if (g_next_cam_intro_end_dist < start_head)
                g_next_cam_intro_end_dist = start_head;
            if (g_next_cam_intro_end_dist > g_next_cam_follow_dist)
                g_next_cam_intro_end_dist = g_next_cam_follow_dist;
            g_next_cam_release_end_dist = end_head;

            start_mid = 0.5f * (start_tail + start_head);
            end_mid = 0.5f * (end_tail + end_head);
            transition_len = end_mid - start_mid;
            if (transition_len < 0.01f) transition_len = 0.01f;

            if (g_slug_override > 0.01f)
                g_anim_target = g_anim_start + transition_len / g_slug_override;
        }

        compute_slide_params();
        update_combined_camera();
        render_composite();
        rpath_extrude(&g_route_path, &g_route_mesh, SHAFT_T, ROUTE_BASE_Y, ROUTE_TOP_Y, g_t_tail, g_t_head);
        rpath_draw(&g_route_mesh, AC_R, AC_G, AC_B, AC_A);
    }

    if (g_route_debug)
        rpath_draw_debug(&g_route_path, g_t_tail, g_t_head);
}

void maneuver_commit_pushed_state(const maneuver_state_t *state) {
    float settle_rot;
    float light_rot;

    (void)state;
    g_route_slide = 1.0f;
    g_anim_start = 1.0f;
    g_anim_target = 1.0f;
    g_route_animating = 0;
    g_cam_settle_start_x = 0.0f;
    g_cam_settle_start_y = 0.0f;
    settle_rot = wrap_angle(g_cam_rot - g_next_cam_rot);
    if (settle_rot > CAMERA_SETTLE_MAX_ROT) settle_rot = CAMERA_SETTLE_MAX_ROT;
    if (settle_rot < -CAMERA_SETTLE_MAX_ROT) settle_rot = -CAMERA_SETTLE_MAX_ROT;
    g_cam_settle_start_rot = settle_rot;
    g_cam_settle_t = 0.0f;
    g_cam_settle_active = 1;
    light_rot = wrap_angle(g_last_combined_rot);
    g_light_settle_start_rot = light_rot;
    g_light_settle_t = 0.0f;
    g_light_settle_active = (fabsf(light_rot) > 1e-4f);
    clear_combined_transition();
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
