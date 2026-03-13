/*
 * Route path — segment-based path + 3D mesh extrusion.
 *
 * Builds a polyline from LINE/ARC segments, then extrudes it as a
 * single continuous stroked mesh with a top face, side walls, and
 * an arrowhead prism.
 *
 * Interior arc samples resolve to shared left/right offset points so the
 * top surface stays watertight across dense curves. Hard corners are
 * finished with rounded sectors on both sides of the joint, so turns and
 * roundabout entry/exit corners stay consistently rounded.
 *
 * 2D (x, y) → 3D (x, height, z=y).
 */

#include <math.h>
#include <string.h>
#include "route_path.h"
#include "render.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define ARC_STEP_DEG 3.0f   /* densify arcs at ~3° per point */
#define JOIN_MIN_ANGLE 0.03f /* ~1.7° — skip join for tiny direction changes */
#define MITER_LIMIT 3.0f    /* max miter length / half-width ratio */

/* ================================================================
 * Path building
 * ================================================================ */

void rpath_clear(route_path_t *p) {
    p->seg_count = 0;
    p->pt_count = 0;
    p->total_length = 0.0f;
    p->arrow_x = 0.0f;
    p->arrow_y = 0.0f;
    p->arrow_angle = 0.0f;
}

void rpath_add_line(route_path_t *p, float x0, float y0, float x1, float y1) {
    if (p->seg_count >= RPATH_MAX_SEGS) return;
    route_seg_t *s = &p->segs[p->seg_count++];
    s->type = RSEG_LINE;
    s->x0 = x0; s->y0 = y0;
    s->x1 = x1; s->y1 = y1;
}

void rpath_add_arc(route_path_t *p, float cx, float cy, float radius,
                   float start_rad, float end_rad) {
    if (p->seg_count >= RPATH_MAX_SEGS) return;
    route_seg_t *s = &p->segs[p->seg_count++];
    s->type = RSEG_ARC;
    s->cx = cx; s->cy = cy;
    s->radius = radius;
    s->start_rad = start_rad;
    s->end_rad = end_rad;
}

void rpath_set_arrow(route_path_t *p, float x, float y, float angle_rad) {
    p->arrow_x = x;
    p->arrow_y = y;
    p->arrow_angle = angle_rad;
}

static void add_pt(route_path_t *p, float x, float y, int smooth) {
    if (p->pt_count >= RPATH_MAX_PTS) return;
    /* Skip duplicate points */
    if (p->pt_count > 0) {
        float dx = x - p->px[p->pt_count - 1];
        float dy = y - p->py[p->pt_count - 1];
        if (dx * dx + dy * dy < 1e-8f) {
            if (smooth) p->pt_smooth[p->pt_count - 1] = 1;
            return;
        }
    }
    p->px[p->pt_count] = x;
    p->py[p->pt_count] = y;
    p->pt_smooth[p->pt_count] = (unsigned char)(smooth ? 1 : 0);
    p->pt_count++;
}

void rpath_densify(route_path_t *p) {
    p->pt_count = 0;
    p->total_length = 0.0f;
    int i;

    for (i = 0; i < p->seg_count; i++) {
        route_seg_t *s = &p->segs[i];
        if (s->type == RSEG_LINE) {
            add_pt(p, s->x0, s->y0, 0);
            add_pt(p, s->x1, s->y1, 0);
        } else {
            /* Arc — emit points at ARC_STEP_DEG intervals */
            float sweep = s->end_rad - s->start_rad;
            float abs_sweep = fabsf(sweep);
            float step_rad = ARC_STEP_DEG * (float)M_PI / 180.0f;
            int steps = (int)(abs_sweep / step_rad) + 1;
            if (steps < 2) steps = 2;
            int j;
            for (j = 0; j <= steps; j++) {
                float t = (float)j / (float)steps;
                float angle = s->start_rad + sweep * t;
                float x = s->cx + s->radius * cosf(angle);
                float y = s->cy + s->radius * sinf(angle);
                add_pt(p, x, y, (j > 0 && j < steps));
            }
        }
    }

    /* Compute cumulative distance */
    p->dist[0] = 0.0f;
    for (i = 1; i < p->pt_count; i++) {
        float dx = p->px[i] - p->px[i - 1];
        float dy = p->py[i] - p->py[i - 1];
        p->dist[i] = p->dist[i - 1] + sqrtf(dx * dx + dy * dy);
    }
    if (p->pt_count > 0)
        p->total_length = p->dist[p->pt_count - 1];
}

/* ================================================================
 * Mesh extrusion — stroked strip with rounded hard corners
 *
 * For each polyline segment: top face quad + 2 side wall quads.
 * Smooth arc samples resolve to shared offsets so dense curves stay
 * watertight. Hard corners switch to segment-local offsets and rounded
 * sector fills on both sides of the joint.
 * Then arrowhead prism at the end.
 * ================================================================ */

static void mesh_v(route_mesh_t *m, float x, float y, float z,
                   float nx, float ny, float nz) {
    if (m->vert_count >= RMESH_MAX_VERTS) return;
    int idx = m->vert_count * 6;
    m->verts[idx]   = x;  m->verts[idx+1] = y;  m->verts[idx+2] = z;
    m->verts[idx+3] = nx; m->verts[idx+4] = ny; m->verts[idx+5] = nz;
    m->vert_count++;
}

/* Push a quad (2 triangles) with flat normal */
static void mesh_quad(route_mesh_t *m,
                      float x0, float y0, float z0,
                      float x1, float y1, float z1,
                      float x2, float y2, float z2,
                      float x3, float y3, float z3,
                      float nx, float ny, float nz) {
    mesh_v(m, x0,y0,z0, nx,ny,nz);
    mesh_v(m, x1,y1,z1, nx,ny,nz);
    mesh_v(m, x2,y2,z2, nx,ny,nz);
    mesh_v(m, x0,y0,z0, nx,ny,nz);
    mesh_v(m, x2,y2,z2, nx,ny,nz);
    mesh_v(m, x3,y3,z3, nx,ny,nz);
}

static void emit_round_join(route_mesh_t *m,
                            float anchor_x, float anchor_z,
                            float corner_x, float corner_z,
                            float base_y, float top_y,
                            float pa_x, float pa_z,
                            float pb_x, float pb_z,
                            float hw) {
    float ang_a = atan2f(pa_z, pa_x);
    float ang_b = atan2f(pb_z, pb_x);
    float diff = ang_b - ang_a;

    if (diff > (float)M_PI)  diff -= 2.0f * (float)M_PI;
    if (diff < -(float)M_PI) diff += 2.0f * (float)M_PI;

    int fan_segs = 8;
    float abs_diff = fabsf(diff);
    if (abs_diff < 0.3f) fan_segs = 3;
    else if (abs_diff < 1.0f) fan_segs = 5;

    float first_ox = corner_x + hw * cosf(ang_a);
    float first_oz = corner_z + hw * sinf(ang_a);
    float last_ox  = corner_x + hw * cosf(ang_a + diff);
    float last_oz  = corner_z + hw * sinf(ang_a + diff);

    int j;
    for (j = 0; j < fan_segs; j++) {
        float t0 = (float)j / (float)fan_segs;
        float t1 = (float)(j + 1) / (float)fan_segs;
        float a0 = ang_a + diff * t0;
        float a1 = ang_a + diff * t1;
        float ox0 = corner_x + hw * cosf(a0), oz0 = corner_z + hw * sinf(a0);
        float ox1 = corner_x + hw * cosf(a1), oz1 = corner_z + hw * sinf(a1);

        /* Top fill for the rounded join wedge. */
        mesh_v(m, anchor_x, top_y, anchor_z, 0, 1, 0);
        mesh_v(m, ox0,     top_y, oz0,      0, 1, 0);
        mesh_v(m, ox1,     top_y, oz1,      0, 1, 0);

        /* Outer side wall following the rounded edge. */
        {
            float wn_x = cosf((a0 + a1) * 0.5f);
            float wn_z = sinf((a0 + a1) * 0.5f);
            mesh_quad(m,
                      ox0, base_y, oz0,
                      ox1, base_y, oz1,
                      ox1, top_y,  oz1,
                      ox0, top_y,  oz0,
                      wn_x, 0, wn_z);
        }
    }

    /* Spoke walls — close the straight edges of the fan (anchor → first/last arc point).
     * Without these, sharp corners (>90°) show a visible gap in the side wall. */
    {
        float dx, dz, len, wn_x, wn_z;

        /* First spoke: anchor → first outer point (faces toward prev segment) */
        dx = first_ox - anchor_x; dz = first_oz - anchor_z;
        len = sqrtf(dx * dx + dz * dz);
        if (len > 1e-6f) {
            /* Normal perpendicular to spoke, pointing away from fan interior.
             * Fan interior is on the side of positive diff rotation, so the
             * outward normal is on the opposite side. */
            wn_x = -dz / len; wn_z = dx / len;
            if (diff < 0.0f) { wn_x = -wn_x; wn_z = -wn_z; }
            mesh_quad(m,
                      anchor_x,  base_y, anchor_z,
                      first_ox,  base_y, first_oz,
                      first_ox,  top_y,  first_oz,
                      anchor_x,  top_y,  anchor_z,
                      wn_x, 0, wn_z);
        }

        /* Last spoke: last outer point → anchor (faces toward next segment) */
        dx = anchor_x - last_ox; dz = anchor_z - last_oz;
        len = sqrtf(dx * dx + dz * dz);
        if (len > 1e-6f) {
            wn_x = -dz / len; wn_z = dx / len;
            if (diff < 0.0f) { wn_x = -wn_x; wn_z = -wn_z; }
            mesh_quad(m,
                      last_ox,   base_y, last_oz,
                      anchor_x,  base_y, anchor_z,
                      anchor_x,  top_y,  anchor_z,
                      last_ox,   top_y,  last_oz,
                      wn_x, 0, wn_z);
        }
    }
}

void rpath_extrude(const route_path_t *p, route_mesh_t *m,
                   float width, float base_y, float top_y,
                   float t0, float t1) {
    m->vert_count = 0;
    m->valid = 0;
    if (p->pt_count < 2) return;
    if (t0 < 0.0f) t0 = 0.0f;
    if (t1 > 1.0f) t1 = 1.0f;
    if (t0 >= t1) return;
    if (t1 <= 0.0f) return;

    float hw = width * 0.5f;

    /* Determine effective start point from t0 parameter */
    float start_dist = t0 * p->total_length;
    int s_idx = 0;  /* first original point index to include after interpolated start */
    float start_x = p->px[0], start_y = p->py[0];
    if (t0 > 0.0f) {
        int i;
        for (i = 1; i < p->pt_count; i++) {
            if (p->dist[i] >= start_dist) {
                float seg_len = p->dist[i] - p->dist[i - 1];
                float frac = (seg_len > 1e-6f) ?
                    (start_dist - p->dist[i - 1]) / seg_len : 0.0f;
                start_x = p->px[i - 1] + frac * (p->px[i] - p->px[i - 1]);
                start_y = p->py[i - 1] + frac * (p->py[i] - p->py[i - 1]);
                s_idx = i;
                break;
            }
        }
    }

    /* Determine effective endpoint from t1 parameter */
    float target_dist = t1 * p->total_length;
    int e_idx = p->pt_count - 1;
    float end_x = p->px[e_idx], end_y = p->py[e_idx];
    if (t1 < 1.0f) {
        int i;
        for (i = 1; i < p->pt_count; i++) {
            if (p->dist[i] >= target_dist) {
                float seg_len = p->dist[i] - p->dist[i - 1];
                float frac = (seg_len > 1e-6f) ?
                    (target_dist - p->dist[i - 1]) / seg_len : 0.0f;
                end_x = p->px[i - 1] + frac * (p->px[i] - p->px[i - 1]);
                end_y = p->py[i - 1] + frac * (p->py[i] - p->py[i - 1]);
                e_idx = i;
                break;
            }
        }
    }

    /* Build local point array from [t0..t1] window */
    int n_pts = 0;
    float epx[RPATH_MAX_PTS], epy[RPATH_MAX_PTS];
    unsigned char esmooth[RPATH_MAX_PTS];

    /* Interpolated start */
    epx[0] = start_x; epy[0] = start_y; esmooth[0] = 0;
    n_pts = 1;

    /* Original interior points between s_idx and the end boundary */
    {
        int last_orig = (t1 >= 1.0f) ? (p->pt_count - 1) : (e_idx - 1);
        int i;
        for (i = s_idx; i <= last_orig; i++) {
            float dx = p->px[i] - epx[n_pts - 1];
            float dy = p->py[i] - epy[n_pts - 1];
            if (dx * dx + dy * dy < 1e-8f) continue;
            if (n_pts >= RPATH_MAX_PTS) break;
            epx[n_pts] = p->px[i];
            epy[n_pts] = p->py[i];
            esmooth[n_pts] = p->pt_smooth[i];
            n_pts++;
        }
    }

    /* Interpolated end (only when t1 < 1.0) */
    if (t1 < 1.0f) {
        float dx = end_x - epx[n_pts - 1];
        float dy = end_y - epy[n_pts - 1];
        if (dx * dx + dy * dy >= 1e-8f && n_pts < RPATH_MAX_PTS) {
            epx[n_pts] = end_x;
            epy[n_pts] = end_y;
            esmooth[n_pts] = 0;
            n_pts++;
        }
    }

    if (n_pts < 2) return;

    /* Pre-compute per-segment direction and perpendicular */
    int i;
    float dir_x[RPATH_MAX_PTS], dir_y[RPATH_MAX_PTS];   /* unit direction */
    float perp_x[RPATH_MAX_PTS], perp_y[RPATH_MAX_PTS]; /* left perpendicular * hw */
    float corner_cross[RPATH_MAX_PTS];
    unsigned char hard_corner[RPATH_MAX_PTS];

    for (i = 0; i < n_pts - 1; i++) {
        float dx = epx[i+1] - epx[i], dy = epy[i+1] - epy[i];
        float len = sqrtf(dx*dx + dy*dy);
        if (len < 1e-6f) { dir_x[i] = 1; dir_y[i] = 0; }
        else { dir_x[i] = dx/len; dir_y[i] = dy/len; }
        /* Left perpendicular (rotate direction 90° CCW) */
        perp_x[i] = -dir_y[i] * hw;
        perp_y[i] =  dir_x[i] * hw;
    }
    for (i = 0; i < n_pts; i++) {
        corner_cross[i] = 0.0f;
        hard_corner[i] = 0;
    }

    /* Pre-compute offset points at each vertex: left (lx,ly) and right (rx,ry).
     * First/last use the adjacent segment's perpendicular.
     * Smooth interior vertices use a shared miter point on both sides. Hard
     * corners still compute these, but the segment quads switch to segment-local
     * offsets and the rounded sectors below fill the joint. */
    float lx[RPATH_MAX_PTS], ly[RPATH_MAX_PTS];
    float rx[RPATH_MAX_PTS], ry[RPATH_MAX_PTS];

    /* First vertex — use segment 0's perpendicular */
    lx[0] = epx[0] + perp_x[0]; ly[0] = epy[0] + perp_y[0];
    rx[0] = epx[0] - perp_x[0]; ry[0] = epy[0] - perp_y[0];

    /* Last vertex — use last segment's perpendicular */
    {
        int last_seg = n_pts - 2;
        lx[n_pts-1] = epx[n_pts-1] + perp_x[last_seg];
        ly[n_pts-1] = epy[n_pts-1] + perp_y[last_seg];
        rx[n_pts-1] = epx[n_pts-1] - perp_x[last_seg];
        ry[n_pts-1] = epy[n_pts-1] - perp_y[last_seg];
    }

    /* Interior vertices: compute shared left/right stroke offsets. */
    for (i = 1; i < n_pts - 1; i++) {
        int seg_prev = i - 1, seg_next = i;
        float cross = dir_x[seg_prev] * dir_y[seg_next] - dir_y[seg_prev] * dir_x[seg_next];
        float mx = perp_x[seg_prev] + perp_x[seg_next];
        float my = perp_y[seg_prev] + perp_y[seg_next];
        float ml = sqrtf(mx*mx + my*my);

        corner_cross[i] = cross;
        hard_corner[i] = (unsigned char)((!esmooth[i] && fabsf(cross) >= JOIN_MIN_ANGLE) ? 1 : 0);

        if (ml < 1e-6f || fabsf(cross) < JOIN_MIN_ANGLE) {
            /* Nearly straight — use the previous segment's perpendicular. */
            lx[i] = epx[i] + perp_x[seg_prev]; ly[i] = epy[i] + perp_y[seg_prev];
            rx[i] = epx[i] - perp_x[seg_prev]; ry[i] = epy[i] - perp_y[seg_prev];
        } else {
            mx /= ml; my /= ml;
            float pu_x = perp_x[seg_prev] / hw, pu_y = perp_y[seg_prev] / hw;
            float dot = mx * pu_x + my * pu_y;
            if (dot < 0.01f) dot = 0.01f;  /* avoid division by near-zero */
            float miter_len = hw / dot;
            if (miter_len > hw * MITER_LIMIT) miter_len = hw * MITER_LIMIT;
            if (hard_corner[i]) {
                if (cross > 0.0f) {
                    /* Left turn: left side is inner, right side stays segment-local. */
                    lx[i] = epx[i] + mx * miter_len; ly[i] = epy[i] + my * miter_len;
                    rx[i] = epx[i] - perp_x[seg_prev]; ry[i] = epy[i] - perp_y[seg_prev];
                } else {
                    /* Right turn: right side is inner, left side stays segment-local. */
                    lx[i] = epx[i] + perp_x[seg_prev]; ly[i] = epy[i] + perp_y[seg_prev];
                    rx[i] = epx[i] - mx * miter_len; ry[i] = epy[i] - my * miter_len;
                }
            } else {
                lx[i] = epx[i] + mx * miter_len; ly[i] = epy[i] + my * miter_len;
                rx[i] = epx[i] - mx * miter_len; ry[i] = epy[i] - my * miter_len;
            }
        }
    }

    /* ---- Generate mesh ---- */

    /* Segment quads (each segment uses shared offsets at both endpoints).
     * Hard corners: inner side uses miter, outer side uses this segment's perp.
     * The round join fan fills the outer gap between adjacent segments. */
    for (i = 0; i < n_pts - 1; i++) {
        float l0x, l0y, r0x, r0y;
        float l1x, l1y, r1x, r1y;

        if (i == 0 || !hard_corner[i]) {
            l0x = lx[i]; l0y = ly[i];
            r0x = rx[i]; r0y = ry[i];
        } else if (corner_cross[i] > 0.0f) {
            /* Left turn at i: left=inner(miter), right=outer(this seg perp) */
            l0x = lx[i]; l0y = ly[i];
            r0x = epx[i] - perp_x[i]; r0y = epy[i] - perp_y[i];
        } else {
            /* Right turn at i: right=inner(miter), left=outer(this seg perp) */
            l0x = epx[i] + perp_x[i]; l0y = epy[i] + perp_y[i];
            r0x = rx[i]; r0y = ry[i];
        }

        if (i == n_pts - 2 || !hard_corner[i + 1]) {
            l1x = lx[i + 1]; l1y = ly[i + 1];
            r1x = rx[i + 1]; r1y = ry[i + 1];
        } else if (corner_cross[i + 1] > 0.0f) {
            l1x = lx[i + 1]; l1y = ly[i + 1];
            r1x = epx[i + 1] - perp_x[i]; r1y = epy[i + 1] - perp_y[i];
        } else {
            l1x = epx[i + 1] + perp_x[i]; l1y = epy[i + 1] + perp_y[i];
            r1x = rx[i + 1]; r1y = ry[i + 1];
        }

        /* Map 2D y → 3D z */
        float z_l0 = l0y, z_l1 = l1y, z_r0 = r0y, z_r1 = r1y;

        /* Side normal from this segment's perpendicular */
        float snx = perp_x[i] / hw, snz = perp_y[i] / hw;

        /* Top face */
        mesh_quad(m,
                  l0x, top_y, z_l0,
                  r0x, top_y, z_r0,
                  r1x, top_y, z_r1,
                  l1x, top_y, z_l1,
                  0, 1, 0);

        /* Left wall */
        mesh_quad(m,
                  l0x, base_y, z_l0,
                  l1x, base_y, z_l1,
                  l1x, top_y,  z_l1,
                  l0x, top_y,  z_l0,
                  snx, 0, snz);

        /* Right wall */
        mesh_quad(m,
                  r0x, top_y,  z_r0,
                  r1x, top_y,  z_r1,
                  r1x, base_y, z_r1,
                  r0x, base_y, z_r0,
                  -snx, 0, -snz);
    }

    /* Rounded outer join for hard corners only.
     * The inner side uses a miter point; the outer side gets a fan. */
    for (i = 1; i < n_pts - 1; i++) {
        int seg_prev = i - 1, seg_next = i;
        if (!hard_corner[i]) continue;

        if (corner_cross[i] > 0.0f) {
            /* Left turn: outer = right side. Fan from prev right perp to next right perp.
             * Anchor = inner miter point (left). */
            emit_round_join(m,
                            lx[i], ly[i],
                            epx[i], epy[i],
                            base_y, top_y,
                            -perp_x[seg_prev], -perp_y[seg_prev],
                            -perp_x[seg_next], -perp_y[seg_next],
                            hw);
        } else {
            /* Right turn: outer = left side. Fan from prev left perp to next left perp.
             * Anchor = inner miter point (right). */
            emit_round_join(m,
                            rx[i], ry[i],
                            epx[i], epy[i],
                            base_y, top_y,
                            perp_x[seg_prev], perp_y[seg_prev],
                            perp_x[seg_next], perp_y[seg_next],
                            hw);
        }
    }

    /* Front cap (at path start) */
    {
        float fnx = -dir_x[0], fnz = -dir_y[0];
        mesh_quad(m,
                  lx[0], base_y, ly[0],
                  rx[0], base_y, ry[0],
                  rx[0], top_y,  ry[0],
                  lx[0], top_y,  ly[0],
                  fnx, 0, fnz);
    }

    /* Back cap (at path end) */
    {
        int li = n_pts - 1;
        float fnx = dir_x[n_pts - 2], fnz = dir_y[n_pts - 2];
        mesh_quad(m,
                  rx[li], base_y, ry[li],
                  lx[li], base_y, ly[li],
                  lx[li], top_y,  ly[li],
                  rx[li], top_y,  ry[li],
                  fnx, 0, fnz);
    }

    /* Arrowhead prism */
    {
        float ax, az, a_dir;
        if (t1 >= 1.0f) {
            /* Arrow at the actual end of extruded body (tracks post-extension
             * during push-out instead of staying at original arrow position) */
            ax = epx[n_pts - 1];
            az = epy[n_pts - 1];
            a_dir = atan2f(dir_y[n_pts - 2], dir_x[n_pts - 2]);
        } else {
            /* Place arrow at truncated endpoint, direction from last segment */
            ax = end_x;
            az = end_y;
            a_dir = atan2f(dir_y[n_pts - 2], dir_x[n_pts - 2]);
        }

        float arrow_size = width * 1.3f;

        /* Arrow tip = forward from arrow base */
        float tip_x = ax + arrow_size * cosf(a_dir);
        float tip_z = az + arrow_size * sinf(a_dir);

        /* Arrow base corners = perpendicular to direction */
        float perp_ax = -sinf(a_dir) * hw * 1.5f;
        float perp_az =  cosf(a_dir) * hw * 1.5f;

        float bl_x = ax + perp_ax, bl_z = az + perp_az;
        float br_x = ax - perp_ax, br_z = az - perp_az;

        /* Top triangle */
        mesh_v(m, bl_x,  top_y, bl_z,  0, 1, 0);
        mesh_v(m, br_x,  top_y, br_z,  0, 1, 0);
        mesh_v(m, tip_x, top_y, tip_z, 0, 1, 0);

        /* Left side wall */
        float ln_x = tip_z - bl_z, ln_z = -(tip_x - bl_x);
        float ln_len = sqrtf(ln_x * ln_x + ln_z * ln_z);
        if (ln_len > 1e-6f) { ln_x /= ln_len; ln_z /= ln_len; }
        mesh_v(m, bl_x,  base_y, bl_z,  ln_x, 0, ln_z);
        mesh_v(m, tip_x, base_y, tip_z, ln_x, 0, ln_z);
        mesh_v(m, tip_x, top_y,  tip_z, ln_x, 0, ln_z);
        mesh_v(m, bl_x,  base_y, bl_z,  ln_x, 0, ln_z);
        mesh_v(m, tip_x, top_y,  tip_z, ln_x, 0, ln_z);
        mesh_v(m, bl_x,  top_y,  bl_z,  ln_x, 0, ln_z);

        /* Right side wall */
        float rn_x = -(tip_z - br_z), rn_z = (tip_x - br_x);
        float rn_len = sqrtf(rn_x * rn_x + rn_z * rn_z);
        if (rn_len > 1e-6f) { rn_x /= rn_len; rn_z /= rn_len; }
        mesh_v(m, br_x,  base_y, br_z,  rn_x, 0, rn_z);
        mesh_v(m, br_x,  top_y,  br_z,  rn_x, 0, rn_z);
        mesh_v(m, tip_x, top_y,  tip_z, rn_x, 0, rn_z);
        mesh_v(m, br_x,  base_y, br_z,  rn_x, 0, rn_z);
        mesh_v(m, tip_x, top_y,  tip_z, rn_x, 0, rn_z);
        mesh_v(m, tip_x, base_y, tip_z, rn_x, 0, rn_z);

        /* Back face of arrowhead */
        float bn_x = -cosf(a_dir), bn_z = -sinf(a_dir);
        mesh_quad(m,
                  bl_x, base_y, bl_z,
                  bl_x, top_y,  bl_z,
                  br_x, top_y,  br_z,
                  br_x, base_y, br_z,
                  bn_x, 0, bn_z);

        /* Bottom triangle (underside of arrowhead) */
        mesh_v(m, bl_x,  base_y, bl_z,  0, -1, 0);
        mesh_v(m, tip_x, base_y, tip_z, 0, -1, 0);
        mesh_v(m, br_x,  base_y, br_z,  0, -1, 0);
    }

    m->valid = 1;
}

/* ================================================================
 * Transform + append — for chaining maneuver route paths
 * ================================================================ */

void rpath_xform_append(route_path_t *dst, const route_path_t *src,
                         float tx, float ty, float cos_r, float sin_r,
                         float rot_rad) {
    int i;
    for (i = 0; i < src->seg_count; i++) {
        if (dst->seg_count >= RPATH_MAX_SEGS) break;
        const route_seg_t *s = &src->segs[i];
        route_seg_t *d = &dst->segs[dst->seg_count++];
        d->type = s->type;
        if (s->type == RSEG_LINE) {
            d->x0 = cos_r * s->x0 - sin_r * s->y0 + tx;
            d->y0 = sin_r * s->x0 + cos_r * s->y0 + ty;
            d->x1 = cos_r * s->x1 - sin_r * s->y1 + tx;
            d->y1 = sin_r * s->x1 + cos_r * s->y1 + ty;
        } else {
            d->cx = cos_r * s->cx - sin_r * s->cy + tx;
            d->cy = sin_r * s->cx + cos_r * s->cy + ty;
            d->radius = s->radius;
            d->start_rad = s->start_rad + rot_rad;
            d->end_rad = s->end_rad + rot_rad;
        }
    }
}

/* ================================================================
 * Debug overlay — polyline + active window highlight
 * ================================================================ */

void rpath_draw_debug(const route_path_t *p, float t0, float t1) {
    if (p->pt_count < 2) return;

    float sd = t0 * p->total_length;
    float ed = t1 * p->total_length;
    float line_w = 0.003f;
    float dot_r = 0.006f;
    int i;

    for (i = 0; i < p->pt_count - 1; i++) {
        float mid_dist = (p->dist[i] + p->dist[i + 1]) * 0.5f;
        if (mid_dist >= sd && mid_dist <= ed) {
            /* Active window: yellow */
            render_thick_line(p->px[i], p->py[i], p->px[i + 1], p->py[i + 1],
                              line_w, 1.0f, 1.0f, 0.0f, 0.8f);
        } else {
            /* Inactive: dark grey */
            render_thick_line(p->px[i], p->py[i], p->px[i + 1], p->py[i + 1],
                              line_w, 0.3f, 0.3f, 0.3f, 0.5f);
        }
    }

    /* Red dots at vertices */
    for (i = 0; i < p->pt_count; i++) {
        render_disc(p->px[i], p->py[i], dot_r, 8, 1.0f, 0.0f, 0.0f, 0.8f);
    }
}

/* ================================================================
 * Drawing — uses render.c vertex buffer + flush
 * ================================================================ */

void rpath_draw(const route_mesh_t *m,
                float r, float g, float b, float a) {
    if (!m->valid || m->vert_count == 0) return;

    /* Draw in batches that fit the render.c vertex buffer (MAX_VERTS=1200) */
    int drawn = 0;
    while (drawn < m->vert_count) {
        vb_reset();
        int batch = m->vert_count - drawn;
        if (batch > 1200) batch = 1200;
        /* Ensure batch is multiple of 3 (triangles) */
        batch = (batch / 3) * 3;
        if (batch == 0) break;

        int i;
        for (i = 0; i < batch; i++) {
            int idx = (drawn + i) * 6;
            vb_v(m->verts[idx], m->verts[idx+1], m->verts[idx+2],
                 m->verts[idx+3], m->verts[idx+4], m->verts[idx+5]);
        }
        vb_flush(r, g, b, a);
        drawn += batch;
    }
}
