/*
 * Maneuver icon rendering -- procedural GL icons for all CarPlay maneuver types.
 *
 * Single-FBO painter's algorithm:
 *   ROAD mask -- white outline drawn first, grey fill on top (same FBO)
 *   ROUTE -- real 3D extruded mesh (path-based, no mask)
 *
 * Outline and fill are rendered flat into the same FBO (no blending).
 * render_composite() applies materials and perspective (no subtraction).
 * Route is rendered as direct 3D geometry after composite.
 *
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */

#include <stdio.h>
#include <math.h>
#include <string.h>
#include "render.h"
#include "maneuver.h"
#include "route_path.h"
#include "visual_style.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ================================================================
 * Colors (used as flat mask colors -- lighting applied in composite)
 * ================================================================ */

#define AC_R 0.353f   /* #5AAAE6 */
#define AC_G 0.667f
#define AC_B 0.902f
#define AC_A 1.0f

#define SD_R CR_STYLE_ASPHALT_R
#define SD_G CR_STYLE_ASPHALT_G
#define SD_B CR_STYLE_ASPHALT_B
#define SD_A 1.0f

/* shorthand for passing color args */
#define ACTIVE  AC_R, AC_G, AC_B, AC_A
#define SIDE    SD_R, SD_G, SD_B, SD_A

/* ================================================================
 * Arrow dimensions (NDC)
 * ================================================================ */

/* Road thickness layers */
#define SHAFT_T    CR_STYLE_ARROW_WIDTH
#define OL_W       CR_STYLE_SHOULDER_WIDTH
#define SIDE_T     CR_STYLE_ROAD_WIDTH
#define OL_T       (SIDE_T + OL_W * 2)
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
#define WHITE  CR_STYLE_SHOULDER_R, CR_STYLE_SHOULDER_G, CR_STYLE_SHOULDER_B, 1.0f

/* Roundabout */
#define RAB_RING_R         0.29f   /* roundabout ring radius */
#define RAB_BLUE_LEN       0.46f   /* roundabout active exit tip radius (fits SmallFrame across all 360 deg) */
#define RAB_ROAD_LEN       0.55f   /* roundabout solid stub outer radius (full standard road length) */
#define RAB_FADE_LEN       0.28f   /* roundabout radial stub fade length */
#define RAB_RING_SEG       64      /* ring/arc segments */

/* U-turn */
#define UTURN_GAP    0.18f   /* half-distance between entry/exit shafts */
#define UTURN_TOP    0.30f   /* top of U arc */
#define UTURN_ARROW  (-0.10f) /* arrowhead Y on exit side */
#define UTURN_SEG    16      /* arc segments */

/* Angle thresholds */
#define ANGLE_DEDUP     5.0f    /* degrees: angles closer than this are "same direction" */
#define ANGLE_DEDUP_RAD 0.09f   /* radians: same threshold (~5deg) for roundabout internals */
#define ANGLE_SELF      1.0f    /* degrees: skip self when comparing angle lists */

/* Highway exit branch angle (degrees) */
#define EXIT_ANGLE_DEG  30.0f

/* Lane change / merge lateral offsets */
#define LANE_SHIFT   0.35f   /* lane change lateral offset */
#define MERGE_OFFSET 0.38f   /* merge ramp lateral offset */
#define BEND_LO     (-0.15f) /* lower S-bend point */
#define BEND_HI      0.15f   /* upper S-bend point */
#define MERGE_BEND_LO (-0.10f) /* merge lower S-bend point */

/* Arrived — concentric circle destination marker (matches stock HUD icon)
 *    ___
 *  (( . ))   outer = outline only (transparent inside)
 *    |X|     inner = filled (road end cap)
 *    |X|     road shaft
 */
#define ARRIVE_CENTER_Y   0.12f       /* shared center of both circles */
#define ARRIVE_INNER_R    0.18f       /* destination geometry independent of road stroke */
#define ARRIVE_OUTER_R    0.28f       /* outer circle: outline ring only */
#define ARRIVE_ROAD_TOP   ARRIVE_CENTER_Y  /* road extends into inner circle center */
#define ARRIVE_SEG        24          /* circle segment count */
#define SNAP_SENTINEL     999.0f      /* large value for angle snap comparison */

/* Destination flag sprite */
#define ARRIVE_FLAG_SZ    0.18f       /* half-extent of flag sprite */
#define FLAG_ANIM_SPEED   0.35f       /* frames per render frame */

/* Route extrusion heights */
#define ROUTE_BASE_Y  CR_STYLE_ROUTE_BASE_Y
#define ROUTE_TOP_Y   CR_STYLE_ROUTE_TOP_Y

/* Cached route mesh -- rebuilt only on maneuver state change */
static route_path_t g_route_path;
static route_mesh_t g_route_mesh;
static route_mesh_t g_route_tail_mesh;
static maneuver_scene_provider_t g_scene_provider;
static int g_cached_state_valid = 0;
static int g_cached_combined = 0;
static maneuver_state_t g_cached_s;
static maneuver_state_t g_cached_next;

void maneuver_set_scene_provider(const maneuver_scene_provider_t *provider) {
    if(provider && provider->build_route && provider->paint_masks)g_scene_provider=*provider;
    else memset(&g_scene_provider,0,sizeof(g_scene_provider));
    g_cached_state_valid = 0;
    render_invalidate_masks();
}
static int scene_supplied(const maneuver_state_t *state) {
    return g_scene_provider.build_route && (!g_scene_provider.handles ||
        g_scene_provider.handles(g_scene_provider.context,state));
}
static void build_scene_route(const maneuver_state_t *state,route_path_t *path) {
    if(scene_supplied(state))g_scene_provider.build_route(g_scene_provider.context,state,path);
    else maneuver_build_route(state,path);
}
static float route_base_y(void) { return g_scene_provider.build_route?g_scene_provider.route_base_y:ROUTE_BASE_Y; }
static float route_top_y(void) { return g_scene_provider.build_route?g_scene_provider.route_top_y:ROUTE_TOP_Y; }

/* Route animation state -- sliding window */
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
static float g_tip_morph_t = 1.0f;   /* 0→1 independent timer for arrow→bulb morph */
static int   g_tip_morph_active = 0; /* 1 when morph animation is running */
#define TIP_MORPH_SPEED  0.07f       /* per frame (~0.5s at 30fps) */
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
static float g_xfade_mid_dist = 0.0f;  /* midpoint between maneuver zones on combined path */
static float g_man_cam_rot = 0.0f;
static float g_man_cam_x = 0.0f,g_man_cam_y = 0.0f;
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
#define ROUTE_EXTEND     RPATH_ANIMATION_EXTENSION           /* extension length beyond viewport */
#define ROUTE_TAIL_FADE_DIST 0.35f      /* only the pre-extension section that is normally off-screen */
#define ROUTE_TAIL_FADE_SLICES 24
#define TRANSITION_PAD   0.15f          /* minimum clear road between maneuver road zones */
#define XFADE_HALF       0.20f          /* spatial crossfade half-width in path distance */
#define CURV_WINDOW      3              /* points each side for curvature sampling */
#define CURV_SLOWDOWN    8.0f           /* curvature sensitivity (higher = more slowdown) */
#define CAMERA_SETTLE_SPEED 0.12f
#define CAMERA_SETTLE_MAX_ROT 0.45f
#define LIGHT_SETTLE_SPEED (1.0f / 50.0f)

static void clear_combined_transition(void) {
    if (g_combined_window_active || g_slug_override > 0.0f)
        g_cached_state_valid = 0;
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
    g_xfade_mid_dist = 0.0f;
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

/* Spatial crossfade: 0.0 = fully current maneuver, 1.0 = fully next maneuver.
 * Based on arrow head distance along combined path relative to the midpoint
 * between maneuver road zones. Smoothstepped over ±XFADE_HALF. */
static float spatial_xfade(void) {
    float total, head_dist, rel, pp;
    if (!g_combined_window_active || g_xfade_mid_dist <= 0.0f)
        return 0.0f;
    total = g_route_path.total_length;
    if (total < 1e-6f) return 0.0f;
    head_dist = g_t_head * total;
    rel = (head_dist - g_xfade_mid_dist) / XFADE_HALF;
    pp = (rel + 1.0f) * 0.5f;
    if (pp < 0.0f) pp = 0.0f;
    if (pp > 1.0f) pp = 1.0f;
    pp = pp * pp * (3.0f - 2.0f * pp);   /* smoothstep */
    return pp;
}

void maneuver_start_anim(void) {
    g_route_slide = 0.0f;
    g_anim_start = 0.0f;
    g_anim_target = 1.0f;
    g_route_animating = 1;
    clear_combined_transition();
    clear_camera_settle();
    clear_light_settle();
    g_camera_prepared_this_frame = 0;
    /* Reset tip morph — will be started when ARRIVED icon is detected */
    g_tip_morph_t = 0.0f;
    g_tip_morph_active = 0;
    /* Standalone: no ramp restart */
    rpath_set_ramp_restart(-1.0f);
}

int maneuver_is_animating(void) {
    /* Only route slide + camera settle gate engine transitions.
     * g_flag_active excluded — loops forever (ARRIVED flag animation).
     * g_light_settle_active excluded — purely cosmetic lighting blend
     * (~1.7s), must not delay queued icon promotion. Both still drive
     * continuous rendering via maneuver_needs_redraw(). */
    return g_route_animating || g_cam_settle_active;
}

int maneuver_needs_redraw(void) {
    return g_route_animating || g_flag_active || g_cam_settle_active
        || g_light_settle_active || (g_tip_morph_active && g_tip_morph_t < 1.0f);
}

int maneuver_is_presentationally_settled(void) {
    /* ARRIVED intentionally redraws forever for its flag sprite. That loop
     * cannot gate completion of a route/camera/light/tip transition. */
    return !g_route_animating && !g_cam_settle_active && !g_light_settle_active
        && !(g_tip_morph_active && g_tip_morph_t < 1.0f);
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
        float t_line_trim = t_dist;
        float da_arc_trim = 0.0f;

        /* Exact C1 tangent fillet for radial line meeting circle at 90 deg (roundabout):
         * Keep actual_r > JOINT_R (hw) so the inner stroke offset never pinches to zero. */
        if (prev->type != cur->type && fabsf(dot) < 0.05f) {
            const route_seg_t *orig_arc = (prev->type == RSEG_ARC) ? &p->segs[i - 1] : cur;
            float R = orig_arc->radius;
            float orig_sweep = fabsf(orig_arc->end_rad - orig_arc->start_rad);
            float line_len = (prev->type == RSEG_LINE) ? prev_len : cur_len;
            float da = fillet_r / R;
            if (da > 0.45f * orig_sweep)
                da = 0.45f * orig_sweep;
            float s_da = sinf(da);
            float rf = R * s_da / (1.0f - s_da);
            float tl = (R + rf) * cosf(da) - R;
            if (tl > 0.45f * line_len) {
                tl = 0.45f * line_len;
                rf = (tl * tl + 2.0f * R * tl) / (2.0f * R);
                da = asinf(rf / (R + rf));
            }
            actual_r = rf;
            t_line_trim = tl;
            da_arc_trim = da;
        }

        /* --- Trim prev segment, get T1 and tangent at T1 --- */
        float t1x, t1y, t1dx, t1dy;
        if (prev->type == RSEG_LINE) {
            t1x = prev->x1 - edx * t_line_trim;
            t1y = prev->y1 - edy * t_line_trim;
            t1dx = edx; t1dy = edy;
            prev->x1 = t1x; prev->y1 = t1y;
        } else {
            float sweep = prev->end_rad - prev->start_rad;
            float da = (da_arc_trim > 0.0f) ? da_arc_trim : (t_dist / prev->radius);
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
            t2x = trimmed.x0 + sdx * t_line_trim;
            t2y = trimmed.y0 + sdy * t_line_trim;
            t2dx = sdx; t2dy = sdy;
            trimmed.x0 = t2x; trimmed.y0 = t2y;
        } else {
            float sweep = trimmed.end_rad - trimmed.start_rad;
            float da = (da_arc_trim > 0.0f) ? da_arc_trim : (t_dist / trimmed.radius);
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

/* Derivative of smoothstep -- gives speed multiplier at position t.
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

    float target_dist = t * p->total_length;
    int idx = 1;
    while (idx < p->pt_count - 1 && p->dist[idx] < target_dist) idx++;

    int lo = idx - CURV_WINDOW;
    int hi = idx + CURV_WINDOW;
    if (lo < 0) lo = 0;
    if (hi >= p->pt_count) hi = p->pt_count - 1;
    if (hi - lo < 2) return 1.0f;

    float d0x = p->px[lo + 1] - p->px[lo], d0y = p->py[lo + 1] - p->py[lo];
    float d1x = p->px[hi] - p->px[hi - 1], d1y = p->py[hi] - p->py[hi - 1];
    float len0 = sqrtf(d0x * d0x + d0y * d0y);
    float len1 = sqrtf(d1x * d1x + d1y * d1y);
    if (len0 < 1e-6f || len1 < 1e-6f) return 1.0f;

    float dot = (d0x * d1x + d0y * d1y) / (len0 * len1);
    if (dot > 1.0f) dot = 1.0f;
    if (dot < -1.0f) dot = -1.0f;

    float angle = acosf(dot);
    /* Gentle arcs (roundabout ring) get mild slowdown, sharp junctions get full */
    if (angle < 0.3f) return 0.7f;  /* mild slowdown on gentle arcs */
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



static float release_blend01(float t) {
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    /* Smoothstep: derivative=0 at both ends.
     * Avoids velocity spike at FOLLOW→RELEASE boundary. */
    return t * t * (3.0f - 2.0f * t);
}

static float smooth01(float t) {
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    return t * t * (3.0f - 2.0f * t);
}

static float light_settle_curve(float t) {
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;

    /* Single smooth curve: critically damped spring feel.
     * Starts near zero velocity, gently accelerates, small overshoot, settle. */
    float u = t * t * (3.0f - 2.0f * t);  /* smoothstep base */
    float overshoot = 4.2f * t * t * (1.0f - t) * (1.0f - t);  /* bell at ~0.5 */
    return u + overshoot * 0.18f;
}

static void apply_camera_pose(float pan_x, float pan_y, float rot) {
    g_man_cam_x=pan_x;g_man_cam_y=pan_y;
    g_man_cam_rot = rot;
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

    g_light_settle_t += LIGHT_SETTLE_SPEED * render_frame_step();
    if (g_light_settle_t >= 1.0f)
        clear_light_settle();
}

static void update_camera_settle(void) {
    float blend;

    if (!g_cam_settle_active) {
        set_default_camera();
        return;
    }

    /* Ease-out (quadratic): starts at full velocity, decelerates to stop.
     * Avoids the velocity hiccup when transitioning from linear slide tracking. */
    blend = 1.0f - (1.0f - g_cam_settle_t) * (1.0f - g_cam_settle_t);
    apply_camera_pose(g_cam_settle_start_x * (1.0f - blend),
                      g_cam_settle_start_y * (1.0f - blend),
                      lerp_angle(g_cam_settle_start_rot, 0.0f, blend));

    g_cam_settle_t += CAMERA_SETTLE_SPEED * render_frame_step();
    if (g_cam_settle_t >= 1.0f) {
        clear_camera_settle();
        set_default_camera();
    }
}

static float g_release_cam_x = 0.0f, g_release_cam_y = 0.0f;
static int   g_was_following = 0;

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
        blend = 2.0f * blend - blend * blend;
        apply_camera_pose(follow_x * blend, follow_y * blend,
                          lerp_angle(0.0f, follow_rot, blend));
        return;
    }

    if (head_dist <= g_next_cam_follow_dist) {
        rpath_sample(&g_route_path, g_t_head, &follow_x, &follow_y);
        follow_rot = rpath_sample_heading(&g_route_path, g_t_head) - (float)(M_PI * 0.5);
        g_was_following = 1;
        g_release_cam_x = follow_x;
        g_release_cam_y = follow_y;
        apply_camera_pose(follow_x, follow_y, follow_rot);
        return;
    }

    /* Rotation: fixed anchor at follow_t (no spin on curved paths) */
    follow_rot = rpath_sample_heading(&g_route_path, follow_t) - (float)(M_PI * 0.5);

    /* Position: spring damper from last FOLLOW pos toward target.
     * Inherently velocity-continuous — no blend curve needed. */
    if (g_was_following) {
        /* g_release_cam already holds last FOLLOW position */
        g_was_following = 0;
    }

    {
        float denom = g_next_cam_release_end_dist - g_next_cam_follow_dist;
        float blend = (denom > 1e-4f) ? (head_dist - g_next_cam_follow_dist) / denom : 1.0f;
        float cam_rot;
        float spring = 0.07f;

        if (blend > 1.0f) blend = 1.0f;

        g_release_cam_x += (g_next_cam_pan_x - g_release_cam_x) * spring;
        g_release_cam_y += (g_next_cam_pan_y - g_release_cam_y) * spring;

        cam_rot = lerp_angle(follow_rot, g_next_cam_rot, release_blend01(blend));
        apply_camera_pose(g_release_cam_x, g_release_cam_y, cam_rot);
    }
}

/* Check if a maneuver needs elevation (path overlaps itself).
 * Returns lift in world units (0 = flat). Scaled to route thickness
 * (CR_STYLE_ROUTE_TOP_Y - CR_STYLE_ROUTE_BASE_Y = 0.006) so the upper pass
 * clears the lower pass without detaching from the road or contact shadow.
 * Roundabout: |exit_angle|>150 means exit stub near entry stub.
 * Turn: |exit_angle|>150 means near-U-turn. */
#define ELEV_LIFT 0.010f
float maneuver_builtin_elevation(const maneuver_state_t *s) {
    if (!s) return 0.0f;
    switch (s->icon) {
    case ICON_ROUNDABOUT: {
        float a = s->exit_angle;
        if (a < 0) a = -a;
        if (a > 150) return ELEV_LIFT;
        return 0.0f;
    }
    case ICON_TURN: {
        float a = s->exit_angle;
        if (a < 0) a = -a;
        if (a > 150) return ELEV_LIFT;
        return 0.0f;
    }
    default:
        return 0.0f;
    }
}

static float maneuver_elevation(const maneuver_state_t *s) {
    if (scene_supplied(s))
        return g_scene_provider.elevation ? g_scene_provider.elevation(g_scene_provider.context,s) : 0.0f;
    return maneuver_builtin_elevation(s);
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
        /* Standalone: no ramp restart */
        rpath_set_ramp_restart(-1.0f);
        { float e = maneuver_elevation(s); rpath_set_elevation(e, e); }
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

        (void)tail_dist;
        g_t_tail = 0.0f;
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
    /* Tail stays at the path start; route visibility is driven by head only. */
    g_t_tail = 0.0f;
    /* Clamp to valid range -- push-out moves head forward */
    if (g_t_head > 1.0f) g_t_head = 1.0f;
}

/* Extrude the full route into g_route_mesh. */
static void route_extrude_body(void) {
    rpath_extrude(&g_route_path, &g_route_mesh, SHAFT_T,
                  route_base_y(), route_top_y(), g_t_tail, g_t_head);
}

/* Draw the route mesh. */
static void route_draw_window(float t0, float t1, float alpha,
                              int cap_start, int cap_end, int tip_end) {
    if (alpha <= 0.001f || t1 - t0 <= 1e-5f)
        return;

    rpath_extrude_partial(&g_route_path, &g_route_tail_mesh, SHAFT_T,
                          route_base_y(), route_top_y(), t0, t1,
                          cap_start, cap_end, tip_end);
    rpath_draw(&g_route_tail_mesh, AC_R, AC_G, AC_B, AC_A * alpha);
}

static void route_draw_with_fade(void) {
    if (g_combined_window_active && g_route_path.total_length > 1e-6f) {
        float fade_t = g_t_tail + (ROUTE_TAIL_FADE_DIST / g_route_path.total_length);
        int i;

        if (fade_t > g_t_head)
            fade_t = g_t_head;

        if (fade_t > g_t_tail + 1e-5f) {
            for (i = 0; i < ROUTE_TAIL_FADE_SLICES; i++) {
                float u0 = (float)i / (float)ROUTE_TAIL_FADE_SLICES;
                float u1 = (float)(i + 1) / (float)ROUTE_TAIL_FADE_SLICES;
                float seg_t0 = g_t_tail + (fade_t - g_t_tail) * u0;
                float seg_t1 = g_t_tail + (fade_t - g_t_tail) * u1;
                int final_tail_slice = (i == ROUTE_TAIL_FADE_SLICES - 1 && fade_t >= g_t_head - 1e-5f);

                route_draw_window(seg_t0, seg_t1, smooth01(u1),
                                  (i == 0), final_tail_slice, final_tail_slice);
            }
        }

        if (fade_t < g_t_head - 1e-5f) {
            route_draw_window(fade_t, g_t_head, 1.0f, 0, 1, 1);
        }
        return;
    }

    rpath_draw(&g_route_mesh, AC_R, AC_G, AC_B, AC_A);
}

/* ================================================================
 * Fading road helper
 *
 * Draws fading road extensions from (x0,y0)->(x1,y1) in FADE_SEGS segments
 * with alpha going from a_start->0 via smoothstep.
 * ================================================================ */

#define FADE_SEGS 64

typedef struct {
    float x0, y0, x1, y1;
    float width;
    float a_start;
} fade_road_t;

/* Draw multiple fading road extensions from outer low-alpha slices to inner
 * high-alpha slices across all stubs together. At each alpha slice, curbs are
 * drawn first and asphalt centers second so adjacent stubs never overwrite
 * a neighbor's high-alpha base with a low-alpha tip or erase solid curbs. */
static void draw_fading_roads_batch(const fade_road_t *roads, int count) {
    int i, r;
    if (count <= 0) return;

    for (i = FADE_SEGS - 1; i >= 0; i--) {
        float t0 = (float)i / FADE_SEGS;
        float t1 = (float)(i + 1) / FADE_SEGS;
        float u = 1.0f - t0;
        float fade = u * u * (3.0f - 2.0f * u);

        for (r = 0; r < count; r++) {
            float alpha = roads[r].a_start * fade;
            if (alpha < 0.01f) continue;
            float dx = roads[r].x1 - roads[r].x0;
            float dy = roads[r].y1 - roads[r].y0;
            float len = sqrtf(dx * dx + dy * dy);
            if (len < 1e-6f) continue;
            float sx = roads[r].x0 + dx * t0, sy = roads[r].y0 + dy * t0;
            float ex = roads[r].x0 + dx * t1, ey = roads[r].y0 + dy * t1;
            float nx = -dy / len, ny = dx / len;
            float offset = (roads[r].width + OL_W) * 0.5f;
            for (int side = -1; side <= 1; side += 2) {
                float ox = nx * offset * side, oy = ny * offset * side;
                render_thick_line(sx + ox, sy + oy, ex + ox, ey + oy, OL_W,
                                  CR_STYLE_SHOULDER_R, CR_STYLE_SHOULDER_G, CR_STYLE_SHOULDER_B, alpha);
            }
        }

        for (r = 0; r < count; r++) {
            float alpha = roads[r].a_start * fade;
            if (alpha < 0.01f) continue;
            float dx = roads[r].x1 - roads[r].x0;
            float dy = roads[r].y1 - roads[r].y0;
            float len = sqrtf(dx * dx + dy * dy);
            if (len < 1e-6f) continue;
            float sx = roads[r].x0 + dx * t0, sy = roads[r].y0 + dy * t0;
            float ex = roads[r].x0 + dx * t1, ey = roads[r].y0 + dy * t1;
            render_thick_line(sx, sy, ex, ey,
                              roads[r].width, SD_R, SD_G, SD_B, alpha);
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
    /* We emit: line(prev_end -> T1), arc(T1 -> T2), then next segment starts at T2. */

    /* Track where the current segment starts (after previous fillet trimmed it) */
    float cur_sx = xs[0], cur_sy = ys[0];

    for (i = 1; i < n - 1; i++) {
        /* Incoming direction: from xs[i-1],ys[i-1] -> xs[i],ys[i] */
        float in_dx = xs[i] - xs[i-1], in_dy = ys[i] - ys[i-1];
        float in_len = sqrtf(in_dx * in_dx + in_dy * in_dy);
        /* Outgoing direction: from xs[i],ys[i] -> xs[i+1],ys[i+1] */
        float out_dx = xs[i+1] - xs[i], out_dy = ys[i+1] - ys[i];
        float out_len = sqrtf(out_dx * out_dx + out_dy * out_dy);

        if (in_len < 1e-6f || out_len < 1e-6f) {
            /* Degenerate segment -- just emit line to vertex */
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

        /* Skip fillet for near-straight (<5deg) or near-reversal (>175deg) */
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
        /* cross > 0 -> left turn, cross < 0 -> right turn */
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
            /* Left turn -> CCW -> end > start */
            while (end_ang < start_ang) end_ang += 2.0f * (float)M_PI;
        } else {
            /* Right turn -> CW -> end < start */
            while (end_ang > start_ang) end_ang -= 2.0f * (float)M_PI;
        }

        /* Emit: line to T1, arc T1->T2 */
        rpath_add_line(p, cur_sx, cur_sy, t1x, t1y);
        rpath_add_arc(p, cx, cy, actual_r, start_ang, end_ang);

        /* Next segment starts at T2 */
        cur_sx = t2x; cur_sy = t2y;
    }

    /* Final segment: cur -> last point */
    rpath_add_line(p, cur_sx, cur_sy, xs[n-1], ys[n-1]);
}

/* ================================================================
 * Roundabout shared helpers
 * ================================================================ */

typedef struct {
    float snapped_deg;  /* snapped exit angle in degrees */
    float exit_rad;     /* math radians: (90 - snapped_deg) * PI / 180 */
    int   snapped_idx;  /* index into junction_angles, or -1 if no snap */
} rab_snap_t;

static rab_snap_t snap_roundabout_exit(float exit_angle_deg,
                                       const float *junction_angles,
                                       int junction_angle_count, int bap_geometry) {
    rab_snap_t r;
    r.snapped_deg = exit_angle_deg;
    r.snapped_idx = -1;
    if (!bap_geometry && junction_angle_count > 0) {
        int best = 0;
        float best_diff = SNAP_SENTINEL;
        int i;
        for (i = 0; i < junction_angle_count && i < MAX_JUNCTION_ANGLES; i++) {
            float diff = fabsf((float)junction_angles[i] - exit_angle_deg);
            if (diff > 180.0f) diff = 360.0f - diff;
            if (diff < best_diff) { best_diff = diff; best = i; }
        }
        if (best_diff < 30.0f) {
            r.snapped_deg = (float)junction_angles[best];
            r.snapped_idx = best;
        }
    }
    r.exit_rad = (90.0f - r.snapped_deg) * (float)M_PI / 180.0f;
    return r;
}

/* Compute arc start/end for roundabout traversal.
 * driving_side=0 → RHT (CCW), driving_side=1 → LHT (CW). */
static void roundabout_arc_range(float entry_rad, float exit_rad, int driving_side,
                                 float *arc_s, float *arc_e) {
    *arc_s = entry_rad;
    *arc_e = exit_rad;
    if (driving_side == 0) {
        while (*arc_e <= *arc_s) *arc_e += 2.0f * (float)M_PI;
        if (*arc_e - *arc_s < 0.01f) *arc_e += 2.0f * (float)M_PI;
    } else {
        while (*arc_e >= *arc_s) *arc_e -= 2.0f * (float)M_PI;
        if (*arc_s - *arc_e < 0.01f) *arc_e -= 2.0f * (float)M_PI;
    }
}

/* ================================================================
 * Route path building (standalone, no mask rendering)
 * ================================================================ */

typedef struct {
    float cx, cy, r, start, end, heading, x0, y0, tip_x, tip_y;
} roundabout_departure_t;

static roundabout_departure_t roundabout_departure(int driving_side) {
    roundabout_departure_t d;
    float sign = driving_side == 1 ? -1.0f : 1.0f;
    d.r = RAB_RING_R;
    d.cx = -sign * d.r;
    d.cy = -0.12f;
    d.start = driving_side == 1 ? (float)M_PI : 0.0f;
    d.end = driving_side == 1 ? (float)M_PI * 0.75f : (float)M_PI * 0.25f;
    d.heading = d.end;
    d.x0 = d.cx + d.r * cosf(d.end);
    d.y0 = d.cy + d.r * sinf(d.end);
    d.tip_x = d.x0 + 0.38f * cosf(d.heading);
    d.tip_y = d.y0 + 0.38f * sinf(d.heading);
    return d;
}

static void build_roundabout_departure(int side, route_path_t *path) {
    roundabout_departure_t d = roundabout_departure(side);
    rpath_clear(path);
    rpath_add_line(path, 0, SHAFT_BOT, 0, d.cy);
    rpath_add_arc(path, d.cx, d.cy, d.r, d.start, d.end);
    rpath_add_line(path, d.x0, d.y0, d.tip_x, d.tip_y);
    rpath_fillet_junctions(path, JOINT_R);
    rpath_set_arrow(path, d.tip_x, d.tip_y, d.heading);
}

/* Build route path for a single maneuver -- raw segments only.
 * No rpath_extend, no rpath_densify, no rpath_extrude.
 * Sets arrow position. Used by draw_* functions and for path chaining. */
void maneuver_build_route(const maneuver_state_t *state, route_path_t *path) {
    switch (state->icon) {
    case ICON_NONE:
        rpath_clear(path);
        break;
    case ICON_ROUNDABOUT_EXIT:
        build_roundabout_departure(state->driving_side, path);
        break;
    case ICON_APPROACH: {
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
            rpath_add_arc(path, 0, top_y, UTURN_GAP, (float)M_PI, 0);
        }
        rpath_add_line(path, exit_x, top_y, exit_x, UTURN_ARROW);
        rpath_fillet_junctions(path, JOINT_R);
        rpath_set_arrow(path, exit_x, UTURN_ARROW, (float)(-M_PI * 0.5));
        break;
    }
    case ICON_ROUNDABOUT: {
        float ring_r = RAB_RING_R;
        float entry_rad = (float)(-M_PI * 0.5);
        rab_snap_t snap = snap_roundabout_exit((float)state->exit_angle,
            state->junction_angles, state->junction_angle_count, state->bap_geometry);
        float ext = RAB_BLUE_LEN - ring_r;
        float entry_pt_x = ring_r * cosf(entry_rad);
        float entry_pt_y = ring_r * sinf(entry_rad);
        float ex0_x = ring_r * cosf(snap.exit_rad);
        float ex0_y = ring_r * sinf(snap.exit_rad);
        float ex_tip_x = (ring_r + ext) * cosf(snap.exit_rad);
        float ex_tip_y = (ring_r + ext) * sinf(snap.exit_rad);
        float arc_s, arc_e;
        roundabout_arc_range(entry_rad, snap.exit_rad, state->driving_side, &arc_s, &arc_e);
        rpath_clear(path);
        rpath_add_line(path, 0, SHAFT_BOT, entry_pt_x, entry_pt_y);
        rpath_add_arc(path, 0, 0, ring_r, arc_s, arc_e);
        rpath_add_line(path, ex0_x, ex0_y, ex_tip_x, ex_tip_y);
        rpath_fillet_junctions(path, JOINT_R);
        rpath_set_arrow(path, ex_tip_x, ex_tip_y, snap.exit_rad);
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
    case ICON_LANE_CHANGE:
    case ICON_EXIT: {
        float sign = (state->direction < 0) ? -1.0f : 1.0f;
        float shift = sign * LANE_SHIFT;
        float pts_x[] = {0, 0, shift, shift};
        float pts_y[] = {SHAFT_BOT, BEND_LO, BEND_HI, BLUE_LEN};
        build_filleted_path(path, pts_x, pts_y, 4, JOINT_R);
        rpath_set_arrow(path, shift, BLUE_LEN, (float)(M_PI * 0.5));
        break;
    }
    case ICON_ARRIVED:
    default: {
        rpath_clear(path);
        rpath_add_line(path, 0, SHAFT_BOT, 0, ARRIVE_CENTER_Y);
        rpath_set_arrow(path, 0, ARRIVE_CENTER_Y, (float)(M_PI * 0.5));
        break;
    }
    }
}

/* Get exit point and heading for a maneuver (for chaining). */
maneuver_exit_t maneuver_get_exit(const maneuver_state_t *state) {
    maneuver_exit_t ex = {0, 0, (float)(M_PI * 0.5)};
    switch (state->icon) {
    case ICON_ROUNDABOUT_EXIT: {
        roundabout_departure_t d = roundabout_departure(state->driving_side);
        ex.x = d.tip_x; ex.y = d.tip_y; ex.heading = d.heading;
        break;
    }
    case ICON_APPROACH: {
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
        rab_snap_t snap = snap_roundabout_exit((float)state->exit_angle,
            state->junction_angles, state->junction_angle_count, state->bap_geometry);
        float ext = RAB_BLUE_LEN - ring_r;
        ex.x = (ring_r + ext) * cosf(snap.exit_rad);
        ex.y = (ring_r + ext) * sinf(snap.exit_rad);
        ex.heading = snap.exit_rad;
        break;
    }
    case ICON_MERGE:
        ex.x = 0; ex.y = BLUE_LEN;
        ex.heading = (float)(M_PI * 0.5);
        break;
    case ICON_LANE_CHANGE:
    case ICON_EXIT: {
        float sign = (state->direction < 0) ? -1.0f : 1.0f;
        ex.x = sign * LANE_SHIFT;
        ex.y = BLUE_LEN;
        ex.heading = (float)(M_PI * 0.5);
        break;
    }
    default: /* ARRIVED -- terminal, no meaningful exit */
        ex.x = 0; ex.y = ARRIVE_CENTER_Y;
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

static float maneuver_exit_road_reach(const maneuver_state_t *state);
static float maneuver_entry_road_reach(const maneuver_state_t *state);
static void compute_combined_transform_from_poses(maneuver_exit_t cur_end,
                                                  maneuver_exit_t cur_exit,
                                                  float cur_exit_reach,
                                                  maneuver_exit_t next_start,
                                                  float next_entry_reach,
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
    static int s_bounds_cached = 0;
    static float s_cached_abs_x = 0.0f;
    static float s_cached_abs_y = 0.0f;

    if (s_bounds_cached) {
        if (out_abs_x != NULL) *out_abs_x = s_cached_abs_x;
        if (out_abs_y != NULL) *out_abs_y = s_cached_abs_y;
        return;
    }

    /* This conservative allocation envelope is computed from built-in sample
     * states, not the provider's current/next snapshot identities. A viewport
     * resize must never ask a provider to resolve these synthetic states. */
    maneuver_scene_provider_t saved_provider = g_scene_provider;
    memset(&g_scene_provider, 0, sizeof(g_scene_provider));
    const float local_half_w = ROAD_LEN + FADE_LEN + OL_T * 0.5f;
    const float local_half_h = ROAD_LEN + FADE_LEN + OL_T * 0.5f;
    const float safety = 1.05f;
    static const maneuver_state_t samples[] = {
        { .icon = ICON_APPROACH },
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
        { .icon = ICON_EXIT, .direction = -1 },
        { .icon = ICON_EXIT, .direction = 1 },
        { .icon = ICON_ROUNDABOUT, .exit_angle = 90, .driving_side = 0 },
        { .icon = ICON_ROUNDABOUT, .exit_angle = 0, .driving_side = 0 },
        { .icon = ICON_ROUNDABOUT, .exit_angle = -90, .driving_side = 0 },
        { .icon = ICON_ROUNDABOUT, .exit_angle = 180, .driving_side = 0 },
        { .icon = ICON_ROUNDABOUT, .exit_angle = 90, .driving_side = 1 },
        { .icon = ICON_ROUNDABOUT, .exit_angle = 0, .driving_side = 1 },
        { .icon = ICON_ROUNDABOUT, .exit_angle = -90, .driving_side = 1 },
        { .icon = ICON_ROUNDABOUT, .exit_angle = 180, .driving_side = 1 },
        { .icon = ICON_LANE_CHANGE, .direction = -1 },
        { .icon = ICON_LANE_CHANGE, .direction = 1 },
        { .icon = ICON_ROUNDABOUT_EXIT, .driving_side = 0 },
        { .icon = ICON_ROUNDABOUT_EXIT, .driving_side = 1 },
        { .icon = ICON_ARRIVED },
    };
    enum { SAMPLE_COUNT = (int)(sizeof(samples) / sizeof(samples[0])) };
    maneuver_exit_t start_poses[SAMPLE_COUNT];
    maneuver_exit_t end_poses[SAMPLE_COUNT];
    maneuver_exit_t exit_poses[SAMPLE_COUNT];
    float exit_reaches[SAMPLE_COUNT];
    float entry_reaches[SAMPLE_COUNT];
    float max_abs_x = local_half_w;
    float max_abs_y = local_half_h;
    int i, j;

    /* Precompute each sample's extended route poses once (26 builds instead of 2028). */
    for (i = 0; i < SAMPLE_COUNT; i++) {
        route_path_t path;
        build_scene_route(&samples[i], &path);
        rpath_extend(&path);
        start_poses[i] = route_path_get_start_pose(&path);
        end_poses[i] = route_path_get_end_pose(&path);
        exit_poses[i] = maneuver_get_exit(&samples[i]);
        exit_reaches[i] = maneuver_exit_road_reach(&samples[i]);
        entry_reaches[i] = maneuver_entry_road_reach(&samples[i]);
    }

    bounds_include_box(0.0f, 0.0f, 1.0f, 0.0f,
                       local_half_w, local_half_h,
                       &max_abs_x, &max_abs_y);

    for (i = 0; i < SAMPLE_COUNT; i++) {
        for (j = 0; j < SAMPLE_COUNT; j++) {
            float tx, ty, cos_r, sin_r, rot;

            compute_combined_transform_from_poses(end_poses[i], exit_poses[i], exit_reaches[i],
                                                  start_poses[j], entry_reaches[j],
                                                  &tx, &ty, &cos_r, &sin_r, &rot);
            bounds_include_box(tx, ty, cos_r, sin_r,
                               local_half_w, local_half_h,
                               &max_abs_x, &max_abs_y);
        }
    }

    s_cached_abs_x = max_abs_x * safety;
    s_cached_abs_y = max_abs_y * safety;
    s_bounds_cached = 1;
    if (out_abs_x != NULL) *out_abs_x = s_cached_abs_x;
    if (out_abs_y != NULL) *out_abs_y = s_cached_abs_y;
    g_scene_provider = saved_provider;
}

/* Road reach past exit/before entry along the connecting heading.
 * Exit: how far road mask extends past maneuver_get_exit() point.
 * Entry: how far road mask extends below SHAFT_BOT. */
static float maneuver_exit_road_reach(const maneuver_state_t *state) {
    if(scene_supplied(state))return g_scene_provider.exit_road_reach;
    if (state != NULL && state->icon == ICON_ROUNDABOUT)
        return (RAB_ROAD_LEN - RAB_BLUE_LEN) + RAB_FADE_LEN;
    return (ROAD_LEN - BLUE_LEN) + FADE_LEN;  /* 0.05 + 0.30 = 0.35 */
}

static float maneuver_entry_road_reach(const maneuver_state_t *state) {
    (void)state;
    if(scene_supplied(state))return g_scene_provider.entry_road_reach;
    return FADE_LEN;  /* 0.30 */
}

static void compute_combined_transform_from_poses(maneuver_exit_t cur_end,
                                                  maneuver_exit_t cur_exit,
                                                  float cur_exit_reach,
                                                  maneuver_exit_t next_start,
                                                  float next_entry_reach,
                                                  float *out_tx, float *out_ty,
                                                  float *out_cos_r, float *out_sin_r,
                                                  float *out_rot) {
    float rot = cur_end.heading - next_start.heading;
    float cos_r = cosf(rot);
    float sin_r = sinf(rot);
    float ns_rx = cos_r * next_start.x - sin_r * next_start.y;
    float ns_ry = sin_r * next_start.x + cos_r * next_start.y;
    float tx = cur_end.x - ns_rx;
    float ty = cur_end.y - ns_ry;

    /* Part A: enforce minimum separation between junction centers.
     * Project center-to-center onto exit heading; if too small, push out. */
    {
        float hdx = cosf(cur_exit.heading);
        float hdy = sinf(cur_exit.heading);
        float proj = tx * hdx + ty * hdy;
        float min_sep = cur_exit_reach + next_entry_reach + TRANSITION_PAD;
        if (proj < min_sep) {
            float deficit = min_sep - proj;
            tx += deficit * hdx;
            ty += deficit * hdy;
        }
    }

    *out_tx = tx;
    *out_ty = ty;
    *out_cos_r = cos_r;
    *out_sin_r = sin_r;
    *out_rot = rot;
}


/* ================================================================
 * Icon drawing functions -- 3 mask passes each
 * ================================================================ */

/* Angular distance between two angles in degrees (0..180). */
static float angle_dist(float a, float b) {
    float d = fabsf(a - b);
    if (d > 180.0f) d = 360.0f - d;
    return d;
}

/* Helper: check if angle (degrees) overlaps forward (0deg) or entry (180deg). */
static int angle_near_fixed(float deg, float eps) {
    float ad = fabsf(deg);
    return (ad < eps || fabsf(ad - 180.0f) < eps);
}

/* ----------------------------------------------------------------
 * draw_approach -- approach junction (arrow stops at center)
 * ---------------------------------------------------------------- */

static void draw_approach(const float *side_angles, int side_count) {
    float accepted[MAX_JUNCTION_ANGLES];
    int n_accepted = 0;
    fade_road_t fades[MAX_JUNCTION_ANGLES + 2];
    int n_fades = 0;
    int i, j;

    for (i = 0; i < side_count && i < MAX_JUNCTION_ANGLES; i++) {
        float deg = (float)side_angles[i];
        int dup = angle_near_fixed(deg, ANGLE_DEDUP);
        for (j = 0; !dup && j < n_accepted; j++) {
            if (angle_dist(deg, accepted[j]) < ANGLE_DEDUP)
                dup = 1;
        }
        if (!dup)
            accepted[n_accepted++] = deg;
    }

    fades[n_fades++] = (fade_road_t){0, SHAFT_BOT, 0, SHAFT_BOT - FADE_LEN, SIDE_T, 1.0f};
    fades[n_fades++] = (fade_road_t){0, SIDE_TOP, 0, SIDE_TOP + FADE_LEN, SIDE_T, 1.0f};
    for (i = 0; i < n_accepted; i++) {
        float rad = accepted[i] * (float)M_PI / 180.0f;
        float sx = ROAD_LEN * sinf(rad), sy = ROAD_LEN * cosf(rad);
        float fx = (ROAD_LEN + FADE_LEN) * sinf(rad);
        float fy = (ROAD_LEN + FADE_LEN) * cosf(rad);
        fades[n_fades++] = (fade_road_t){sx, sy, fx, fy, SIDE_T, 1.0f};
    }

    render_set_raised(0);

    render_begin_outline_mask();
    draw_fading_roads_batch(fades, n_fades);
    render_thick_line(0, SHAFT_BOT, 0, SIDE_TOP, OL_T, WHITE);
    for (i = 0; i < n_accepted; i++) {
        float rad = accepted[i] * (float)M_PI / 180.0f;
        float sx = ROAD_LEN * sinf(rad), sy = ROAD_LEN * cosf(rad);
        render_thick_line(0, 0, sx, sy, OL_T, WHITE);
    }
    if (n_accepted > 0)
        render_disc(0, 0, OL_T * 0.5f, JOINT_SEG, WHITE);

    render_thick_line(0, SHAFT_BOT, 0, SIDE_TOP, SIDE_T, SIDE);
    for (i = 0; i < n_accepted; i++) {
        float rad = accepted[i] * (float)M_PI / 180.0f;
        float sx = ROAD_LEN * sinf(rad), sy = ROAD_LEN * cosf(rad);
        render_thick_line(0, 0, sx, sy, SIDE_T, SIDE);
    }
    if (n_accepted > 0)
        render_disc(0, 0, SIDE_T * 0.5f, JOINT_SEG, SIDE);
    render_end_outline_mask();

    if (g_masks_only_mode)
        return;

    /* Build route path -- blue line same length as arrived */
    float straight_end = ARRIVE_ROAD_TOP - HEAD_SZ;
    rpath_clear(&g_route_path);
    rpath_add_line(&g_route_path, 0, SHAFT_BOT, 0, straight_end);
    rpath_extend(&g_route_path);
    rpath_densify(&g_route_path);
    rpath_set_arrow(&g_route_path, 0, straight_end, (float)(M_PI * 0.5));
    compute_slide_params();
    route_extrude_body();

    render_composite();
    route_draw_with_fade();
}

/* Check if junction angles include a forward road (~0° ± 30°) */
static int has_forward_road(float exit_angle, const float *side_angles, int side_count) {
    int i;
    /* The active straight exit is itself a road, even when iOS does not list
     * it among junction branches. Match the active-stub deduplication below. */
    if (fabsf(exit_angle) < ANGLE_DEDUP) return 1;
    for (i = 0; i < side_count; i++) {
        if (side_angles[i] > -30 && side_angles[i] < 30) return 1;
    }
    return 0;
}

/* ----------------------------------------------------------------
 * draw_turn -- outline/fill/route passes
 * ---------------------------------------------------------------- */

static void draw_turn(float angle_deg, const float *side_angles, int side_count) {
    float angle_rad = angle_deg * (float)M_PI / 180.0f;
    float end_x = BLUE_LEN * sinf(angle_rad);
    float end_y = BLUE_LEN * cosf(angle_rad);
    float stub_x = ROAD_LEN * sinf(angle_rad);
    float stub_y = ROAD_LEN * cosf(angle_rad);
    float fade_x = (ROAD_LEN + FADE_LEN) * sinf(angle_rad);
    float fade_y = (ROAD_LEN + FADE_LEN) * cosf(angle_rad);
    int fwd = has_forward_road(angle_deg, side_angles, side_count);
    int active_has_own_stub = !angle_near_fixed(angle_deg, ANGLE_DEDUP);
    float accepted[MAX_JUNCTION_ANGLES];
    int n_accepted = 0;
    fade_road_t fades[MAX_JUNCTION_ANGLES + 3];
    int n_fades = 0;
    int i, j;

    for (i = 0; i < side_count && i < MAX_JUNCTION_ANGLES; i++) {
        float deg = (float)side_angles[i];
        int dup = angle_near_fixed(deg, ANGLE_DEDUP);
        if (active_has_own_stub && angle_dist(deg, angle_deg) < ANGLE_DEDUP)
            dup = 1;
        for (j = 0; !dup && j < n_accepted; j++) {
            if (angle_dist(deg, accepted[j]) < ANGLE_DEDUP)
                dup = 1;
        }
        if (!dup)
            accepted[n_accepted++] = deg;
    }

    if (fwd)
        fades[n_fades++] = (fade_road_t){0, SIDE_TOP, 0, SIDE_TOP + FADE_LEN, SIDE_T, 1.0f};
    fades[n_fades++] = (fade_road_t){0, SHAFT_BOT, 0, SHAFT_BOT - FADE_LEN, SIDE_T, 1.0f};
    if (active_has_own_stub)
        fades[n_fades++] = (fade_road_t){stub_x, stub_y, fade_x, fade_y, SIDE_T, 1.0f};
    for (i = 0; i < n_accepted; i++) {
        float rad = accepted[i] * (float)M_PI / 180.0f;
        float sx = ROAD_LEN * sinf(rad), sy = ROAD_LEN * cosf(rad);
        float fx = (ROAD_LEN + FADE_LEN) * sinf(rad);
        float fy = (ROAD_LEN + FADE_LEN) * cosf(rad);
        fades[n_fades++] = (fade_road_t){sx, sy, fx, fy, SIDE_T, 1.0f};
    }

    render_set_raised(0);

    render_begin_outline_mask();
    draw_fading_roads_batch(fades, n_fades);
    if (fwd)
        render_thick_line(0, 0, 0, SIDE_TOP, OL_T, WHITE);
    render_thick_line(0, SHAFT_BOT, 0, 0, OL_T, WHITE);
    if (active_has_own_stub)
        render_thick_line(0, 0, stub_x, stub_y, OL_T, WHITE);
    for (i = 0; i < n_accepted; i++) {
        float rad = accepted[i] * (float)M_PI / 180.0f;
        float sx = ROAD_LEN * sinf(rad), sy = ROAD_LEN * cosf(rad);
        render_thick_line(0, 0, sx, sy, OL_T, WHITE);
    }
    render_disc(0, 0, OL_T * 0.5f, JOINT_SEG, WHITE);

    if (fwd)
        render_thick_line(0, 0, 0, SIDE_TOP, SIDE_T, SIDE);
    render_thick_line(0, SHAFT_BOT, 0, 0, SIDE_T, SIDE);
    if (active_has_own_stub)
        render_thick_line(0, 0, stub_x, stub_y, SIDE_T, SIDE);
    for (i = 0; i < n_accepted; i++) {
        float rad = accepted[i] * (float)M_PI / 180.0f;
        float sx = ROAD_LEN * sinf(rad), sy = ROAD_LEN * cosf(rad);
        render_thick_line(0, 0, sx, sy, SIDE_T, SIDE);
    }
    render_disc(0, 0, SIDE_T * 0.5f, JOINT_SEG, SIDE);
    render_end_outline_mask();

    if (g_masks_only_mode)
        return;

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
        route_extrude_body();
    }

    render_composite();
    route_draw_with_fade();
}

/* ----------------------------------------------------------------
 * draw_uturn -- outline/fill/route passes
 * ---------------------------------------------------------------- */

static void draw_uturn(int go_left) {
    float enter_x = go_left ?  UTURN_GAP : -UTURN_GAP;
    float exit_x  = go_left ? -UTURN_GAP :  UTURN_GAP;
    float top_y   = UTURN_TOP;
    float arrow_y = UTURN_ARROW;
    fade_road_t fades[2] = {
        {enter_x, SHAFT_BOT, enter_x, SHAFT_BOT - FADE_LEN, SIDE_T, 1.0f},
        {exit_x,  SHAFT_BOT, exit_x,  SHAFT_BOT - FADE_LEN, SIDE_T, 1.0f}
    };

    render_set_raised(0);

    render_begin_outline_mask();
    draw_fading_roads_batch(fades, 2);
    render_thick_line(enter_x, SHAFT_BOT, enter_x, top_y, OL_T, WHITE);
    render_arc(0, top_y, UTURN_GAP, OL_T, 0, (float)M_PI, UTURN_SEG, WHITE);
    render_thick_line(exit_x, top_y, exit_x, SHAFT_BOT, OL_T, WHITE);

    render_thick_line(enter_x, SHAFT_BOT, enter_x, top_y, SIDE_T, SIDE);
    render_arc(0, top_y, UTURN_GAP, SIDE_T, 0, (float)M_PI, UTURN_SEG, SIDE);
    render_thick_line(exit_x, top_y, exit_x, SHAFT_BOT, SIDE_T, SIDE);
    render_disc(exit_x, top_y, SIDE_T * 0.5f, JOINT_SEG, SIDE);
    render_disc(enter_x, top_y, SIDE_T * 0.5f, JOINT_SEG, SIDE);
    render_end_outline_mask();

    if (g_masks_only_mode)
        return;

    /* Build route path: entry shaft -> U-arc -> exit shaft */
    rpath_clear(&g_route_path);
    rpath_add_line(&g_route_path, enter_x, SHAFT_BOT, enter_x, top_y);
    /* Arc from entry side to exit side over the top.
     * Arc center at (0, top_y), radius = UTURN_GAP.
     * go_left: enter is right (+x), exit is left (-x) -> arc 0->PI (CCW over top)
     * go_right: enter is left (-x), exit is right (+x) -> arc PI->0 (CW over top) */
    if (go_left) {
        rpath_add_arc(&g_route_path, 0, top_y, UTURN_GAP, 0, (float)M_PI);
    } else {
        rpath_add_arc(&g_route_path, 0, top_y, UTURN_GAP, (float)M_PI, 0);
    }
    rpath_add_line(&g_route_path, exit_x, top_y, exit_x, arrow_y);
    rpath_fillet_junctions(&g_route_path, JOINT_R);
    rpath_extend(&g_route_path);
    rpath_densify(&g_route_path);
    rpath_set_arrow(&g_route_path, exit_x, arrow_y, (float)(-M_PI * 0.5));
    compute_slide_params();
    route_extrude_body();

    render_composite();
    route_draw_with_fade();
}

/* ----------------------------------------------------------------
 * draw_roundabout -- outline/fill/route passes
 * ---------------------------------------------------------------- */

static void draw_roundabout(float exit_angle_deg, int driving_side,
                            const float *junction_angles, int junction_angle_count, int bap_geometry) {
    float cx = 0.0f, cy = 0.0f;
    float ring_r = RAB_RING_R;
    float ring_ol = OL_T;

    float entry_rad = (float)(-M_PI * 0.5);

    rab_snap_t snap = snap_roundabout_exit(exit_angle_deg, junction_angles, junction_angle_count, bap_geometry);
    float exit_rad = snap.exit_rad;
    int snapped_idx = snap.snapped_idx;

    float ext = RAB_BLUE_LEN - ring_r;
    float stub = RAB_ROAD_LEN - ring_r;
    float stub_fade = RAB_FADE_LEN;

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
    int i, j;
    for (i = 0; i < junction_angle_count && i < MAX_JUNCTION_ANGLES; i++) {
        if (i == snapped_idx) continue;
        side_rads[n_sides++] = (90.0f - (float)junction_angles[i]) * (float)M_PI / 180.0f;
    }

    /* Track drawn stub angles to avoid double-drawing splitter islands or
     * closely-spaced approaches that would fuse on the roundabout ring. */
    float drawn_rads[MAX_JUNCTION_ANGLES + 2];
    int n_drawn = 0;
    drawn_rads[n_drawn++] = entry_rad;

    int exit_dup = 0;
    {
        float ed = fabsf(wrap_angle(exit_rad - entry_rad));
        if (ed < ANGLE_DEDUP_RAD) exit_dup = 1;
    }
    if (!exit_dup) drawn_rads[n_drawn++] = exit_rad;

    int side_skip[MAX_JUNCTION_ANGLES];
    for (i = 0; i < n_sides; i++) {
        float sr = side_rads[i];
        int dup = 0;
        for (j = 0; j < n_drawn; j++) {
            float d = fabsf(wrap_angle(sr - drawn_rads[j]));
            if (d < ANGLE_DEDUP_RAD) { dup = 1; break; }
        }
        side_skip[i] = dup;
        if (!dup) drawn_rads[n_drawn++] = sr;
    }

    /* Keep ALL roundabout exits at full road width (SIDE_T) by relaxing
     * closely-spaced side road angles within each angular sector between the
     * fixed entry (-pi/2) and fixed active exit (exit_rad). Only if a sector
     * is too crowded to fit full-width spokes even after fan-out do we scale
     * width uniformly within that sector. */
    float exit_w = SIDE_T;
    float exit_ol = exit_w + 2.0f * OL_W;
    float side_w[MAX_JUNCTION_ANGLES];
    for (i = 0; i < n_sides; i++)
        side_w[i] = SIDE_T;

    {
        const float min_sep = 0.72f; /* ~41.2 deg: full-width curbs meet cleanly at ring edge */
        float base = entry_rad;      /* -pi/2 */
        float split = exit_dup ? (2.0f * (float)M_PI) : wrap_angle(exit_rad - base);
        if (!exit_dup && split <= 0.0f) split += 2.0f * (float)M_PI;
        float bounds[3] = { 0.0f, split, 2.0f * (float)M_PI };
        int n_sectors = exit_dup ? 1 : 2;
        int sec;

        for (sec = 0; sec < n_sectors; sec++) {
            float a_lo = bounds[sec];
            float a_hi = bounds[sec + 1];
            float span = a_hi - a_lo;
            int idx[MAX_JUNCTION_ANGLES];
            float rel[MAX_JUNCTION_ANGLES];
            int k = 0;

            for (i = 0; i < n_sides; i++) {
                if (side_skip[i]) continue;
                float r = wrap_angle(side_rads[i] - base);
                if (r <= 0.0f) r += 2.0f * (float)M_PI;
                if (r > a_lo && r < a_hi) {
                    idx[k] = i;
                    rel[k] = r;
                    k++;
                }
            }
            if (k <= 0) continue;

            /* Sort side roads in this sector by angle */
            for (i = 0; i < k - 1; i++) {
                for (j = i + 1; j < k; j++) {
                    if (rel[j] < rel[i]) {
                        float tr = rel[i]; rel[i] = rel[j]; rel[j] = tr;
                        int ti = idx[i]; idx[i] = idx[j]; idx[j] = ti;
                    }
                }
            }

            float sep = min_sep;
            if (span / (float)(k + 1) < sep)
                sep = span / (float)(k + 1);

            /* Symmetric relaxation to enforce minimum angular spacing `sep` */
            for (int iter = 0; iter < 32; iter++) {
                for (i = 0; i < k - 1; i++) {
                    float d = rel[i + 1] - rel[i];
                    if (d < sep) {
                        float push = 0.5f * (sep - d);
                        rel[i] -= push;
                        rel[i + 1] += push;
                    }
                }
                for (i = 0; i < k; i++) {
                    float lo_i = a_lo + (float)(i + 1) * sep;
                    float hi_i = a_hi - (float)(k - i) * sep;
                    if (rel[i] < lo_i) rel[i] = lo_i;
                    if (rel[i] > hi_i) rel[i] = hi_i;
                }
            }

            float sec_w = SIDE_T;
            if (sep < min_sep) {
                sec_w = 2.0f * 0.45f * sinf(sep * 0.5f) - 2.0f * OL_W - 0.008f;
                if (sec_w < 0.12f) sec_w = 0.12f;
                if (sec_w > SIDE_T) sec_w = SIDE_T;
            }
            for (i = 0; i < k; i++) {
                side_rads[idx[i]] = wrap_angle(base + rel[i]);
                side_w[idx[i]] = sec_w;
            }
        }
    }

    fade_road_t fades[MAX_JUNCTION_ANGLES + 2];
    int n_fades = 0;
    fades[n_fades++] = (fade_road_t){0, SHAFT_BOT, 0, SHAFT_BOT - FADE_LEN, SIDE_T, 1.0f};
    if (!exit_dup) {
        float ef_x0 = cx + (ring_r + stub) * cosf(exit_rad);
        float ef_y0 = cy + (ring_r + stub) * sinf(exit_rad);
        float ef_x1 = cx + (ring_r + stub + stub_fade) * cosf(exit_rad);
        float ef_y1 = cy + (ring_r + stub + stub_fade) * sinf(exit_rad);
        fades[n_fades++] = (fade_road_t){ef_x0, ef_y0, ef_x1, ef_y1, exit_w, 1.0f};
    }
    for (i = 0; i < n_sides; i++) {
        if (side_skip[i]) continue;
        float sr = side_rads[i];
        float sx1 = cx + (ring_r + stub) * cosf(sr);
        float sy1 = cy + (ring_r + stub) * sinf(sr);
        float sx2 = cx + (ring_r + stub + stub_fade) * cosf(sr);
        float sy2 = cy + (ring_r + stub + stub_fade) * sinf(sr);
        fades[n_fades++] = (fade_road_t){sx1, sy1, sx2, sy2, side_w[i], 1.0f};
    }

    render_set_raised(0);

    /* === ROAD MASK (fades -> solid outlines -> solid fills) === */
    render_begin_outline_mask();
    draw_fading_roads_batch(fades, n_fades);
    render_rect(-ring_ol * 0.5f, SHAFT_BOT, ring_ol, entry_top - SHAFT_BOT, WHITE);
    if (!exit_dup)
        render_thick_line(ex0_x, ex0_y, ex_ol_x, ex_ol_y, exit_ol, WHITE);
    for (i = 0; i < n_sides; i++) {
        if (side_skip[i]) continue;
        float sr = side_rads[i];
        float sx0 = cx + ring_r * cosf(sr);
        float sy0 = cy + ring_r * sinf(sr);
        float sx1 = cx + (ring_r + stub) * cosf(sr);
        float sy1 = cy + (ring_r + stub) * sinf(sr);
        render_thick_line(sx0, sy0, sx1, sy1, side_w[i] + 2.0f * OL_W, WHITE);
    }
    render_circle(cx, cy, ring_r, ring_ol, RAB_RING_SEG, WHITE);

    {
        float side_t = SIDE_T;
        render_rect(-side_t * 0.5f, SHAFT_BOT, side_t, entry_top - SHAFT_BOT, SIDE);
        render_disc(0, entry_top, side_t * 0.5f, JOINT_SEG, SIDE);
        if (!exit_dup) {
            render_thick_line(ex0_x, ex0_y, ex_ol_x, ex_ol_y, exit_w, SIDE);
            render_disc(ex0_x, ex0_y, exit_w * 0.5f, JOINT_SEG, SIDE);
        }
        for (i = 0; i < n_sides; i++) {
            if (side_skip[i]) continue;
            float sr = side_rads[i];
            float sx0 = cx + ring_r * cosf(sr);
            float sy0 = cy + ring_r * sinf(sr);
            float sx1 = cx + (ring_r + stub) * cosf(sr);
            float sy1 = cy + (ring_r + stub) * sinf(sr);
            render_thick_line(sx0, sy0, sx1, sy1, side_w[i], SIDE);
            render_disc(sx0, sy0, side_w[i] * 0.5f, JOINT_SEG, SIDE);
        }
        render_circle(cx, cy, ring_r, side_t, RAB_RING_SEG, SIDE);
    }
    render_end_outline_mask();

    if (g_masks_only_mode)
        return;

    /* === ROUTE PATH (3D mesh) === */
    {
        float arc_s, arc_e;
        roundabout_arc_range(entry_rad, exit_rad, driving_side, &arc_s, &arc_e);

        float entry_pt_x = cx + ring_r * cosf(entry_rad);
        float entry_pt_y = cy + ring_r * sinf(entry_rad);

        rpath_clear(&g_route_path);
        /* Entry shaft -> ring entry point */
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
        route_extrude_body();
    }

    render_composite();
    route_draw_with_fade();
}

/* ----------------------------------------------------------------
 * draw_arrived -- outline/fill/route passes
 * ---------------------------------------------------------------- */

static float g_arrive_flag_dx = 0.0f;  /* cached flag X on outer circle */
static float g_arrive_flag_dy = 0.0f;  /* cached flag Y on outer circle */
static float g_combined_flag_x = 0.0f; /* flag position in combined path space */
static float g_combined_flag_y = 0.0f;

static void draw_arrived(int dir) {
    /* Concentric circle destination marker (matches stock HUD icon):
     * Inner circle = filled road end cap (grey fill, white outline)
     * Outer circle = outline ring only (transparent inside)
     * Flag snaps to outer circle edge: left=9h, center=12h, right=3h */
    if (!g_masks_only_mode) {
        if (dir < 0) {
            g_arrive_flag_dx = -ARRIVE_OUTER_R;
            g_arrive_flag_dy = ARRIVE_CENTER_Y;
        } else if (dir > 0) {
            g_arrive_flag_dx = ARRIVE_OUTER_R;
            g_arrive_flag_dy = ARRIVE_CENTER_Y;
        } else {
            g_arrive_flag_dx = 0.0f;
            g_arrive_flag_dy = ARRIVE_CENTER_Y + ARRIVE_OUTER_R;
        }
    }

    render_set_raised(0);

    render_begin_outline_mask();
    /* Fading shaft extension first so it never overwrites solid curb/fill */
    {
        fade_road_t fade = {0, SHAFT_BOT, 0, SHAFT_BOT - FADE_LEN, SIDE_T, 1.0f};
        draw_fading_roads_batch(&fade, 1);
    }
    /* Outline pass — road shaft + outer ring + inner circle (white disc) */
    render_thick_line(0, SHAFT_BOT, 0, ARRIVE_ROAD_TOP, OL_T, WHITE);
    render_circle(0, ARRIVE_CENTER_Y, ARRIVE_OUTER_R, OL_W, ARRIVE_SEG, WHITE);
    render_disc(0, ARRIVE_CENTER_Y, ARRIVE_INNER_R, ARRIVE_SEG, WHITE);
    /* Fill pass — road grey + inner circle grey (leaves OL_W white border) */
    render_thick_line(0, SHAFT_BOT, 0, ARRIVE_ROAD_TOP, SIDE_T, SIDE);
    render_disc(0, ARRIVE_ROAD_TOP, SIDE_T * 0.5f, ARRIVE_SEG, SIDE);
    render_disc(0, ARRIVE_CENTER_Y, ARRIVE_INNER_R - OL_W, ARRIVE_SEG, SIDE);
    render_end_outline_mask();

    if (g_masks_only_mode)
        return;

    /* === ROUTE PATH — road ends at circle center, no arrowhead === */
    rpath_clear(&g_route_path);
    rpath_add_line(&g_route_path, 0, SHAFT_BOT, 0, ARRIVE_CENTER_Y);
    rpath_extend(&g_route_path);
    rpath_densify(&g_route_path);
    /* Morph arrow→bulb via independent timer (not tied to slide).
     * Started by maneuver_start_anim / maneuver_commit_pushed_state. */
    g_route_path.tip_blend = g_tip_morph_t;
    g_route_path.bulb_radius = ARRIVE_INNER_R - OL_W;
    rpath_set_arrow(&g_route_path, 0, ARRIVE_CENTER_Y, (float)(M_PI * 0.5));
    compute_slide_params();
    route_extrude_body();

    render_composite();
    render_sprite_flag(g_arrive_flag_dx, g_arrive_flag_dy, ARRIVE_FLAG_SZ, (int)g_flag_frame);
    route_draw_with_fade();
}

/* ----------------------------------------------------------------
 * draw_exit -- outline/fill/route passes
 * ---------------------------------------------------------------- */

static void draw_exit(int go_left, int through_road) {
    float sign = go_left ? -1.0f : 1.0f;
    float shift = sign * LANE_SHIFT;
    float bend_lo = BEND_LO;
    float bend_hi = BEND_HI;
    fade_road_t fades[3];
    int n_fades = 0;

    fades[n_fades++] = (fade_road_t){0, SHAFT_BOT, 0, SHAFT_BOT - FADE_LEN, SIDE_T, 1.0f};
    if (through_road)
        fades[n_fades++] = (fade_road_t){0, SIDE_TOP, 0, SIDE_TOP + FADE_LEN, SIDE_T, 1.0f};
    fades[n_fades++] = (fade_road_t){shift, SIDE_TOP, shift, SIDE_TOP + FADE_LEN, SIDE_T, 1.0f};

    render_set_raised(0);

    render_begin_outline_mask();
    draw_fading_roads_batch(fades, n_fades);
    /* Outline -- white, OL_T */
    if (through_road) render_thick_line(0, SHAFT_BOT, 0, SIDE_TOP, OL_T, WHITE);
    render_thick_line(0, SHAFT_BOT, 0, bend_lo, OL_T, WHITE);
    render_thick_line(0, bend_lo, shift, bend_hi, OL_T, WHITE);
    render_disc(0, bend_lo, OL_T * 0.5f, JOINT_SEG, WHITE);
    render_disc(shift, bend_hi, OL_T * 0.5f, JOINT_SEG, WHITE);
    render_thick_line(shift, bend_hi, shift, SIDE_TOP, OL_T, WHITE);
    /* Fill -- grey, SIDE_T (overwrites interior) */
    if (through_road) render_thick_line(0, SHAFT_BOT, 0, SIDE_TOP, SIDE_T, SIDE);
    render_thick_line(0, SHAFT_BOT, 0, bend_lo, SIDE_T, SIDE);
    render_thick_line(0, bend_lo, shift, bend_hi, SIDE_T, SIDE);
    render_disc(0, bend_lo, SIDE_T * 0.5f, JOINT_SEG, SIDE);
    render_disc(shift, bend_hi, SIDE_T * 0.5f, JOINT_SEG, SIDE);
    render_thick_line(shift, bend_hi, shift, SIDE_TOP, SIDE_T, SIDE);
    render_end_outline_mask();

    if (g_masks_only_mode)
        return;

    /* Build route path with fillets at S-bend corners */
    {
        float pts_x[] = {0, 0, shift, shift};
        float pts_y[] = {SHAFT_BOT, bend_lo, bend_hi, BLUE_LEN};
        build_filleted_path(&g_route_path, pts_x, pts_y, 4, JOINT_R);
        rpath_extend(&g_route_path);
        rpath_densify(&g_route_path);
        rpath_set_arrow(&g_route_path, shift, BLUE_LEN, (float)(M_PI * 0.5));
        compute_slide_params();
        route_extrude_body();
    }

    render_composite();
    route_draw_with_fade();
}

/* ----------------------------------------------------------------
 * draw_merge -- outline/fill/route passes
 * ---------------------------------------------------------------- */

static void draw_merge(int go_right) {
    float sign = go_right ? 1.0f : -1.0f;
    float start_x = sign * -MERGE_OFFSET;
    float bend_lo = MERGE_BEND_LO;
    float bend_hi = BEND_HI;
    fade_road_t fades[3] = {
        {0, SHAFT_BOT, 0, SHAFT_BOT - FADE_LEN, SIDE_T, 1.0f},
        {0, SIDE_TOP,  0, SIDE_TOP + FADE_LEN,  SIDE_T, 1.0f},
        {start_x, SHAFT_BOT, start_x, SHAFT_BOT - FADE_LEN, SIDE_T, 1.0f}
    };

    render_set_raised(0);

    render_begin_outline_mask();
    draw_fading_roads_batch(fades, 3);
    /* Outline -- white, OL_T */
    render_thick_line(0, SHAFT_BOT, 0, SIDE_TOP, OL_T, WHITE);
    render_thick_line(start_x, SHAFT_BOT, start_x, bend_lo, OL_T, WHITE);
    render_thick_line(start_x, bend_lo, 0, bend_hi, OL_T, WHITE);
    render_disc(start_x, bend_lo, OL_T * 0.5f, JOINT_SEG, WHITE);
    render_disc(0, bend_hi, OL_T * 0.5f, JOINT_SEG, WHITE);
    render_thick_line(0, bend_hi, 0, SHAFT_TOP, OL_T, WHITE);
    /* Fill -- grey, SIDE_T (overwrites interior) */
    render_thick_line(0, SHAFT_BOT, 0, SIDE_TOP, SIDE_T, SIDE);
    render_thick_line(start_x, SHAFT_BOT, start_x, bend_lo, SIDE_T, SIDE);
    render_thick_line(start_x, bend_lo, 0, bend_hi, SIDE_T, SIDE);
    render_disc(start_x, bend_lo, SIDE_T * 0.5f, JOINT_SEG, SIDE);
    render_disc(0, bend_hi, SIDE_T * 0.5f, JOINT_SEG, SIDE);
    render_thick_line(0, bend_hi, 0, SHAFT_TOP, SIDE_T, SIDE);
    render_end_outline_mask();

    if (g_masks_only_mode)
        return;

    /* Build route path with fillets at S-bend corners */
    {
        float pts_x[] = {start_x, start_x, 0, 0};
        float pts_y[] = {SHAFT_BOT, bend_lo, bend_hi, BLUE_LEN};
        build_filleted_path(&g_route_path, pts_x, pts_y, 4, JOINT_R);
        rpath_extend(&g_route_path);
        rpath_densify(&g_route_path);
        rpath_set_arrow(&g_route_path, 0, BLUE_LEN, (float)(M_PI * 0.5));
        compute_slide_params();
        route_extrude_body();
    }

    render_composite();
    route_draw_with_fade();
}

/* Separate exit-from-roundabout stage: only the local departure arc is shown. */
static void draw_roundabout_departure(int side) {
    roundabout_departure_t d = roundabout_departure(side);
    int pass;
    render_set_raised(0);
    render_begin_outline_mask();
    for (pass = 0; pass < 2; pass++) {
        float width = pass ? SIDE_T : OL_T;
        float color[] = { WHITE };
        if (pass) { float side[] = { SIDE }; memcpy(color, side, sizeof(color)); }
        render_thick_line(0, SHAFT_BOT, 0, d.cy, width, color[0], color[1], color[2], color[3]);
        render_arc(d.cx, d.cy, d.r, width, 0, (float)M_PI, RAB_RING_SEG, color[0], color[1], color[2], color[3]);
        render_thick_line(d.x0, d.y0,
            d.tip_x + 0.05f * cosf(d.heading), d.tip_y + 0.05f * sinf(d.heading), width, color[0], color[1], color[2], color[3]);
    }
    render_end_outline_mask();
    if (g_masks_only_mode)
        return;
    build_roundabout_departure(side, &g_route_path);
    rpath_extend(&g_route_path);
    rpath_densify(&g_route_path);
    compute_slide_params();
    route_extrude_body();
    render_composite();
    route_draw_with_fade();
}

/* ================================================================
 * Main dispatch
 * ================================================================ */

static void draw_supplied_scene(const maneuver_state_t *s,float tx,float ty,float cos_r,float sin_r) {
    /* Providers may use the camera for local layout calculations. Restore the
     * engine's pose before compositing; road masks never steer the handoff. */
    float cam_x=g_man_cam_x,cam_y=g_man_cam_y,cam_rot=g_man_cam_rot;
    g_scene_provider.paint_masks(g_scene_provider.context,s,tx,ty,cos_r,sin_r);
    apply_camera_pose(cam_x,cam_y,cam_rot);
    if(g_masks_only_mode)
        return;
    build_scene_route(s,&g_route_path);
    rpath_extend(&g_route_path);rpath_densify(&g_route_path);
    compute_slide_params();
    route_extrude_body();render_composite();route_draw_with_fade();
}

void maneuver_draw(const maneuver_state_t *s, const maneuver_state_t *next_state) {
    if (s->icon == ICON_NONE && next_state == NULL) {
        g_flag_active = 0;
        g_cached_state_valid = 0;
        render_select_road_fbo(0);
        render_begin_outline_mask();
        render_end_outline_mask();
        render_composite();
        return;
    }
    /* Track whether flag animation is active — also during push TO arrived */
    g_flag_active = (s->icon == ICON_ARRIVED)
                 || (next_state != NULL && next_state->icon == ICON_ARRIVED);
    if (!g_flag_active) {
        g_combined_flag_x = 0.0f;
        g_combined_flag_y = 0.0f;
    }
    if (s->icon == ICON_ARRIVED) {
        if (s->direction < 0) {
            g_arrive_flag_dx = -ARRIVE_OUTER_R;
            g_arrive_flag_dy = ARRIVE_CENTER_Y;
        } else if (s->direction > 0) {
            g_arrive_flag_dx = ARRIVE_OUTER_R;
            g_arrive_flag_dy = ARRIVE_CENTER_Y;
        } else {
            g_arrive_flag_dx = 0.0f;
            g_arrive_flag_dy = ARRIVE_CENTER_Y + ARRIVE_OUTER_R;
        }
    }

    /* Advance flag animation */
    if (g_flag_active) {
        int fc = render_get_flag_frame_count();
        g_flag_frame += FLAG_ANIM_SPEED * render_frame_step();
        if (fc > 0 && g_flag_frame >= (float)fc)
            g_flag_frame = fmodf(g_flag_frame, (float)fc);
    }

    /* Combined path: when pushing with a known next maneuver, build joined route
     * after the draw_* pass so arrow slides continuously between maneuvers. */
    int combined = (next_state != NULL && maneuver_is_pushing());
    if (!combined)
        g_combined_window_active = 0;

    if (!g_cached_state_valid
            || g_cached_combined != combined
            || memcmp(&g_cached_s, s, sizeof(*s)) != 0
            || (combined && memcmp(&g_cached_next, next_state, sizeof(*next_state)) != 0)) {
        render_invalidate_masks();
        g_cached_state_valid = 1;
        g_cached_combined = combined;
        g_cached_s = *s;
        if (combined)
            g_cached_next = *next_state;
    }

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

    /* Unified animation: slide from g_anim_start -> g_anim_target.
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
        g_route_slide += speed * render_frame_step() / orig_len;
        if (g_route_slide >= g_anim_target) {
            g_route_slide = g_anim_target;
            g_route_animating = 0;
        }
    }

    /* Advance tip morph timer (independent of slide).
     * Starts when ARRIVED slide reaches 60%, or immediately after push commit. */
    if (s != NULL && s->icon == ICON_ARRIVED && !g_tip_morph_active
            && (g_route_slide >= 0.5f || g_anim_start >= 1.0f)) {
        g_tip_morph_active = 1;
    }
    if (g_tip_morph_active && g_tip_morph_t < 1.0f) {
        g_tip_morph_t += TIP_MORPH_SPEED * render_frame_step();
        if (g_tip_morph_t > 1.0f) g_tip_morph_t = 1.0f;
    }

    /* If masks are cached and still valid, just re-composite + rebuild mesh at current slide
     * (handles perspective animation and route animation without re-rendering masks) */
    if (!render_masks_dirty()) {
        compute_slide_params();
        if (combined) {
            rpath_set_ramp_restart(g_slug_override + 2.0f * ROUTE_EXTEND);
            rpath_set_elevation(maneuver_elevation(s), maneuver_elevation(next_state));
            update_combined_camera();
        }
        /* Update tip_blend on cached frames:
         * - Pushing FROM ARRIVED: reverse morph bulb→arrow (1→0)
         * - Settled ARRIVED: forward morph via g_tip_morph_t */
        if (combined && s->icon == ICON_ARRIVED) {
            float range = g_anim_target - g_anim_start;
            float progress = (range > 0.01f) ? (g_route_slide - g_anim_start) / range : 1.0f;
            if (progress < 0.0f) progress = 0.0f;
            if (progress > 1.0f) progress = 1.0f;
            /* Morph out in first 40% of push (synced with morph-in timing) */
            float morph_out = (progress < 0.4f) ? (1.0f - progress / 0.4f) : 0.0f;
            g_route_path.tip_blend = morph_out;
            g_route_path.bulb_radius = ARRIVE_INNER_R - OL_W;
        } else if (s->icon == ICON_ARRIVED) {
            g_route_path.tip_blend = g_tip_morph_t;
        }
        if (!g_combined_window_active
                && (g_route_animating || g_route_slide != 1.0f
                    || (g_tip_morph_active && g_tip_morph_t < 1.0f))) {
            /* Rebuild mesh at current slide (path segments still cached in g_route_path) */
            route_extrude_body();
        }
        /* Composite with road fade during push (spatial crossfade using cached FBO 0 and FBO 1) */
        if (combined) {
            float pp = spatial_xfade();
            float base_alpha = render_get_global_alpha();
            render_select_road_fbo(0);
            render_set_global_alpha(base_alpha * (1.0f - pp));
            render_composite();
            render_reset_depth();
            render_select_road_fbo(1);
            render_set_global_alpha(base_alpha * pp);
            render_composite();
            render_select_road_fbo(0);
            render_set_global_alpha(base_alpha);
        } else {
            render_select_road_fbo(0);
            render_composite();
        }
        /* Crossfade flags on cached frames (spatial) */
        if (s->icon == ICON_ARRIVED || (combined && next_state != NULL && next_state->icon == ICON_ARRIVED)) {
            float pp = combined ? spatial_xfade() : 0.0f;
            float ba = render_get_global_alpha();
            if (s->icon == ICON_ARRIVED) {
                render_set_global_alpha(ba * (1.0f - pp));
                render_sprite_flag(g_arrive_flag_dx, g_arrive_flag_dy, ARRIVE_FLAG_SZ, (int)g_flag_frame);
            }
            if (combined && next_state != NULL && next_state->icon == ICON_ARRIVED) {
                render_set_global_alpha(ba * pp);
                render_sprite_flag(g_combined_flag_x, g_combined_flag_y, ARRIVE_FLAG_SZ,
                                   (int)g_flag_frame);
            }
            render_set_global_alpha(ba);
        }
        route_draw_with_fade();
        if (g_route_debug)
            rpath_draw_debug(&g_route_path, g_t_tail, g_t_head);
        return;
    }

    /* Dispatch mask + route rendering for a state */
    #define DISPATCH_DRAW(st) do { \
        if(scene_supplied(st))draw_supplied_scene((st),0,0,1,0); \
        else switch ((st)->icon) { \
            case ICON_APPROACH: draw_approach((st)->junction_angles, (st)->junction_angle_count); break; \
            case ICON_TURN: draw_turn((float)(st)->exit_angle, (st)->junction_angles, (st)->junction_angle_count); break; \
            case ICON_UTURN: { \
                int _go = ((st)->driving_side == 0) ? 1 : 0; \
                if ((st)->direction != 0) _go = ((st)->direction < 0) ? 1 : 0; \
                draw_uturn(_go); \
            } break; \
            case ICON_MERGE: draw_merge((st)->direction > 0); break; \
            case ICON_EXIT: draw_exit((st)->direction < 0, 1); break; \
            case ICON_LANE_CHANGE: draw_exit((st)->direction < 0, 0); break; \
            case ICON_ROUNDABOUT_EXIT: draw_roundabout_departure((st)->driving_side); break; \
            case ICON_ROUNDABOUT: draw_roundabout((float)(st)->exit_angle, (st)->driving_side, (st)->junction_angles, (st)->junction_angle_count, (st)->bap_geometry); break; \
            case ICON_ARRIVED: draw_arrived((st)->direction); break; \
            default: break; \
        } \
    } while(0)

    /* Render current maneuver masks into FBO 0 (+ composite if not combined) */
    render_select_road_fbo(0);
    if (combined) {
        g_masks_only_mode = 1;
        DISPATCH_DRAW(s);
        g_masks_only_mode = 0;
    } else {
        DISPATCH_DRAW(s);
    }

    /* When combined, rebuild joined route path and draw it */
    if (combined) {
        /* Rebuild combined path (draw_* overwrote g_route_path).
         * Each maneuver keeps its own pre/post extension so the handoff has
         * spacing, and the FBO uses the same transform. */
        route_path_t next_path;
        float rot, cos_r, sin_r;
        float tx, ty;
        build_scene_route(s, &g_route_path);

        /* Measure first maneuver's road length BEFORE extending --
         * use it for speed normalization during the combined handoff. */
        rpath_densify(&g_route_path);
        g_slug_override = g_route_path.total_length;
        rpath_extend(&g_route_path);

        build_scene_route(next_state, &next_path);
        rpath_densify(&next_path);
        float next_road_len = next_path.total_length;
        rpath_extend(&next_path);
        {
            maneuver_exit_t cur_end = route_path_get_end_pose(&g_route_path);
            maneuver_exit_t cur_exit = scene_supplied(s) ? cur_end : maneuver_get_exit(s);
            compute_combined_transform_from_poses(cur_end, cur_exit,
                                                  maneuver_exit_road_reach(s),
                                                  route_path_get_start_pose(&next_path),
                                                  maneuver_entry_road_reach(next_state),
                                                  &tx, &ty, &cos_r, &sin_r, &rot);
        }
        g_last_combined_rot = rot;

        /* Pre-compute flag position for ARRIVED in combined space.
         * Full rotation: flag stays anchored to the rotated circle. */
        if (next_state->icon == ICON_ARRIVED) {
            float nfx, nfy;
            if (next_state->direction < 0) {
                nfx = -ARRIVE_OUTER_R;
                nfy = ARRIVE_CENTER_Y;
            } else if (next_state->direction > 0) {
                nfx = ARRIVE_OUTER_R;
                nfy = ARRIVE_CENTER_Y;
            } else {
                nfx = 0.0f;
                nfy = ARRIVE_CENTER_Y + ARRIVE_OUTER_R;
            }
            g_combined_flag_x = cos_r * nfx - sin_r * nfy + tx;
            g_combined_flag_y = sin_r * nfx + cos_r * nfy + ty;
        }

        /* Set ramp restart at boundary where next maneuver's path begins.
         * Current extended path length = slug + 2*ROUTE_EXTEND.
         * Elevation = max of current and next (either may self-overlap). */
        rpath_set_ramp_restart(g_slug_override + 2.0f * ROUTE_EXTEND);
        rpath_set_elevation(maneuver_elevation(s), maneuver_elevation(next_state));
        rpath_xform_append(&g_route_path, &next_path, tx, ty, cos_r, sin_r, rot);
        float ax = cos_r * next_path.arrow_x - sin_r * next_path.arrow_y + tx;
        float ay = sin_r * next_path.arrow_x + cos_r * next_path.arrow_y + ty;
        rpath_set_arrow(&g_route_path, ax, ay, next_path.arrow_angle + rot);
        rpath_densify(&g_route_path);

        /* Part B: spatial crossfade midpoint -- halfway between road zones */
        {
            float cur_zone_end = ROUTE_EXTEND + g_slug_override
                               + maneuver_exit_road_reach(s);
            float next_zone_start = g_route_path.total_length - ROUTE_EXTEND
                                  - next_road_len
                                  - maneuver_entry_road_reach(next_state);
            if (next_zone_start < cur_zone_end)
                next_zone_start = cur_zone_end;
            g_xfade_mid_dist = 0.5f * (cur_zone_end + next_zone_start);
        }

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
            if (end_head < 0.0f) end_head = 0.0f;
            if (end_head > g_route_path.total_length)
                end_head = g_route_path.total_length;

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

        /* Reverse morph bulb→arrow in first 40% of push (synced with morph-in) */
        if (s->icon == ICON_ARRIVED) {
            float range = g_anim_target - g_anim_start;
            float progress = (range > 0.01f) ? (g_route_slide - g_anim_start) / range : 1.0f;
            if (progress < 0.0f) progress = 0.0f;
            if (progress > 1.0f) progress = 1.0f;
            float morph_out = (progress < 0.4f) ? (1.0f - progress / 0.4f) : 0.0f;
            g_route_path.tip_blend = morph_out;
            g_route_path.bulb_radius = ARRIVE_INNER_R - OL_W;
        }

        /* Crossfade: current roads in FBO 0 fade out, next roads in FBO 1 fade in (spatial) */
        {
            float pp = spatial_xfade();
            float base_alpha = render_get_global_alpha();

            /* Pass 1: composite current maneuver roads (in FBO 0) fading out */
            render_select_road_fbo(0);
            render_set_global_alpha(base_alpha * (1.0f - pp));
            render_composite();

            /* Reset depth so second composite doesn't fight first */
            render_reset_depth();

            /* Pass 2: draw next maneuver masks into FBO 1, composite fading in. */
            render_select_road_fbo(1);
            g_masks_only_mode = 1;
            if(scene_supplied(next_state))draw_supplied_scene(next_state,tx,ty,cos_r,sin_r);
            else {
                render_push_mask_transform(tx, ty, cos_r, sin_r);
                DISPATCH_DRAW(next_state);
                render_pop_mask_transform();
            }
            g_masks_only_mode = 0;

            render_set_global_alpha(base_alpha * pp);
            render_composite();

            render_select_road_fbo(0);
            render_set_global_alpha(base_alpha);
        }

        /* Crossfade flags: current fades out, next fades in (spatial) */
        {
            float pp = spatial_xfade();
            float ba = render_get_global_alpha();
            if (s->icon == ICON_ARRIVED) {
                render_set_global_alpha(ba * (1.0f - pp));
                render_sprite_flag(g_arrive_flag_dx, g_arrive_flag_dy, ARRIVE_FLAG_SZ, (int)g_flag_frame);
            }
            if (next_state->icon == ICON_ARRIVED) {
                render_set_global_alpha(ba * pp);
                render_sprite_flag(g_combined_flag_x, g_combined_flag_y, ARRIVE_FLAG_SZ,
                                   (int)g_flag_frame);
            }
            render_set_global_alpha(ba);
        }

        if (!g_combined_window_active)
            route_extrude_body();
        route_draw_with_fade();
    }

    #undef DISPATCH_DRAW

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
    /* Carry spring's remaining offset into settle (transformed to new local space).
     * Without this, short-road maneuvers (ARRIVED) jump at commit. */
    {
        float dx = g_release_cam_x - g_next_cam_pan_x;
        float dy = g_release_cam_y - g_next_cam_pan_y;
        float cr = cosf(-g_next_cam_rot);
        float sr = sinf(-g_next_cam_rot);
        g_cam_settle_start_x = cr * dx - sr * dy;
        g_cam_settle_start_y = sr * dx + cr * dy;
    }
    /* Reset tip morph — ARRIVED will start it on next frame */
    g_tip_morph_t = 0.0f;
    g_tip_morph_active = 0;
    settle_rot = wrap_angle(g_man_cam_rot - g_next_cam_rot);
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
        case ICON_APPROACH:    return "APPROACH";
        case ICON_TURN:        return "TURN";
        case ICON_UTURN:       return "UTURN";
        case ICON_MERGE:       return "MERGE";
        case ICON_EXIT: return "EXIT";
        case ICON_LANE_CHANGE: return "LANE_CHANGE";
        case ICON_ROUNDABOUT_EXIT: return "ROUNDABOUT_EXIT";
        case ICON_ROUNDABOUT:  return "ROUNDABOUT";
        case ICON_ARRIVED:     return "ARRIVED";
        default:               return "UNKNOWN";
    }
}
