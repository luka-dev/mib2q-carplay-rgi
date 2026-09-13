#include "scene_internal.hpp"

namespace navigation {

void Builder::key_point(float x, float h, float y) {
    if (point_count >= MaxKeys) {
        failed = true;
        return;
    }
    points[point_count][0] = x;
    points[point_count][1] = h;
    points[point_count++][2] = y;
}

void Builder::projected_bounds(const float m[16], float px, float py, float b[4]) {
    int i;
    b[0] = b[1] = 1e6f;
    b[2] = b[3] = -1e6f;
    for (i = 0; i < point_count; ++i) {
        float x = points[i][0] - px, h = points[i][1], y = points[i][2] - py;
        float w = m[3] * x + m[7] * h + m[11] * y + m[15];
        if (!std::isfinite(w) || w <= 1e-6f) {
            failed = true;
            return;
        }
        float sx = (1 + (m[0] * x + m[4] * h + m[8] * y + m[12]) / w) * (style::SourceWidth * .5f);
        float sy = (1 - (m[1] * x + m[5] * h + m[9] * y + m[13]) / w) * (style::SourceHeight * .5f);
        if (!std::isfinite(sx) || !std::isfinite(sy)) {
            failed = true;
            return;
        }
        if (sx < b[0])
            b[0] = sx;
        if (sy < b[1])
            b[1] = sy;
        if (sx > b[2])
            b[2] = sx;
        if (sy > b[3])
            b[3] = sy;
    }
}

void Builder::frame_scene(float arrow_x) {
    const Frame &frame = style::SmallContent;
    float m[16], best = 1e9f;
    int ix, iy;
    camera_pan(0, 0);
    camera_sync();
    camera_matrix(m);
    for (iy = -4; iy <= 4; ++iy)
        for (ix = -50; ix <= 50; ++ix) {
            float px = ix * .01f, py = iy * .02f, b[4], cost, overflow = 0;
            projected_bounds(m, px, py, b);
            if (b[0] < frame.left)
                overflow += frame.left - b[0];
            if (b[1] < frame.top)
                overflow += frame.top - b[1];
            if (b[2] > frame.right)
                overflow += b[2] - frame.right;
            if (b[3] > frame.bottom)
                overflow += b[3] - frame.bottom;
            /* At most a small (~6px) compromise on primary-lane centering. */
            if (fabsf(px - arrow_x) > .04f)
                overflow += (fabsf(px - arrow_x) - .04f) * 100;
            cost = overflow * 10000 + fabsf(px - arrow_x) * 100 + fabsf(py) * 12;
            if (cost < best) {
                best = cost;
                camera_x = px;
                camera_y = py;
                framing_ok = overflow < .01f;
                memcpy(frame_bounds, b, sizeof(frame_bounds));
            }
        }
    camera_pan(camera_x, camera_y);
    camera_sync();
}

void Builder::prepare_road(const lr_state *s, lr_window window, int compact, float secondary_width,
                           RoadLayout &plan) {
    int i;
    plan = {};
    plan.w = window;
    plan.secondary_width = secondary_width;
    plan.exit_lanes = lr_exit_group(s, plan.w.anchor, &plan.exit_first, &plan.exit_last);
    plan.exit_lo = -(style::LaneWidth * .5f) + (plan.exit_first - plan.w.anchor) * secondary_width;
    plan.exit_hi = (style::LaneWidth * .5f) + (plan.exit_last - plan.w.anchor) * secondary_width;
    for (int i = 0; i < plan.w.count; ++i) {
        plan.widths[i] = plan.w.first + i == plan.w.anchor ? style::LaneWidth : secondary_width;
        plan.total += plan.widths[i];
    }
    plan.left = -plan.total * .5f;
    plan.edges[0] = plan.left;
    for (int i = 0; i < plan.w.count; ++i) {
        plan.centers[i] = plan.edges[i] + plan.widths[i] * .5f;
        plan.edges[i + 1] = plan.edges[i] + plan.widths[i];
        if (plan.w.first + i == plan.w.anchor)
            plan.ax = plan.centers[i];
    }
    plan.angle = plan.w.anchor < 0 ? 0 : (float)s->lane[plan.w.anchor].primary;
    build_lane_path((int)plan.angle, plan.ax, plan.exit_lanes, plan.exit_lo, plan.exit_hi);
    /* Extend the entry beyond the visible lower edge for even progress. */
    path.segs[0].y0 = style::EntryEnd;
    rpath_densify(&path);
    /* Lane overlay, not the old floating standalone maneuver. Its extra
     * .035 lift + .060 top projected the head into the adjacent exit lane.
     * Keep a shallow 3D prism, with unchanged lateral width and path. */
    rpath_set_elevation(0, 0);
    rpath_set_ramp_restart(-1);
    rpath_extrude(&path, &mesh, style::ArrowWidth, style::RouteBase, style::RouteTop, 0, 1);
    point_count = 0;
    scene_glyph_clearance = 1;
    if (plan.w.anchor >= 0)
        for (i = 0; i < mesh.vert_count; ++i) {
            const float *v = mesh.verts + i * 6;
            if (v[2] >= -.38f)
                key_point(v[0], v[1], v[2]);
        }
    for (i = 0; i < plan.w.count; ++i) {
        float x = plan.centers[i];
        key_point(x - .02f, 0, -.18f);
        key_point(x + .02f, 0, -.18f);
        lane_glyphs(&s->lane[plan.w.first + i], plan.w.first + i == plan.w.anchor, x, plan.widths[i], 1);
    }
    /* The real edge must remain visible next to the countable lane glyphs. */
    if (!plan.w.hidden_left) {
        key_point(plan.left - style::ShoulderWidth, 0, -.32f);
        key_point(plan.left - style::ShoulderWidth, 0, .10f);
    }
    if (!plan.w.hidden_right) {
        key_point(plan.edges[plan.w.count] + style::ShoulderWidth, 0, -.32f);
        key_point(plan.edges[plan.w.count] + style::ShoulderWidth, 0, .10f);
    }
    /* The inner part of the continuation stays readable; only its outer
     * transparent tail is secondary context allowed beyond the crop. */
    if (plan.w.hidden_left)
        key_point(plan.left - .045f, 0, -.18f);
    if (plan.w.hidden_right)
        key_point(plan.edges[plan.w.count] + .045f, 0, -.18f);
    /* The primary composition ALWAYS targets the small red crop. Stage
     * size never selects another camera, route scale or primary lane. */
    frame_scene(plan.ax);
    /* Keep readable arrow/glyph sizes. Sacrifice secondary context before
     * considering a wider camera, which this preview never does. */
    if (!framing_ok && plan.w.count > 1) {
        lr_window narrower =
            lr_choose(s, plan.w.count - 1, plan.w.anchor < 0 ? -1 : s->lane[plan.w.anchor].position);
        if (narrower.count < plan.w.count) {
            prepare_road(s, narrower, compact, plan.secondary_width, plan);
            return;
        }
        /* Never remove a lane between primary and the real edge. */
        if (plan.secondary_width > style::MinSecondaryWidth + .001f) {
            prepare_road(s, plan.w, compact, plan.secondary_width - style::WidthStep, plan);
            return;
        }
    }
    if (framing_ok) {
        /* Use remaining room in EITHER viewport before fading a neighbor.
         * The small-frame fit above remains the canonical composition;
         * append/prepend context without moving or resizing any lane. */
        lr_window canonical = plan.w, best = plan.w;
        float canonical_left = plan.left, m[16], best_left = plan.left, best_bounds[4];
        int base_keys = point_count, requested;
        memcpy(best_bounds, frame_bounds, sizeof(best_bounds));
        camera_matrix(m);
        for (requested = plan.w.count + 1; requested <= s->count; ++requested) {
            lr_window candidate =
                lr_choose(s, requested, plan.w.anchor < 0 ? -1 : s->lane[plan.w.anchor].position);
            float candidate_left =
                canonical_left - (canonical.first - candidate.first) * plan.secondary_width;
            float edge = candidate_left, b[4];
            point_count = base_keys;
            for (i = 0; i < candidate.count; ++i) {
                int index = candidate.first + i;
                float width = index == candidate.anchor ? style::LaneWidth : plan.secondary_width;
                if (index < canonical.first || index >= canonical.first + canonical.count) {
                    key_point(edge + width * .5f, 0, -.18f);
                    lane_glyphs(&s->lane[index], 0, edge + width * .5f, width, 1);
                }
                edge += width;
            }
            if (!candidate.hidden_left)
                key_point(candidate_left - style::ShoulderWidth, 0, -.32f);
            if (!candidate.hidden_right)
                key_point(edge + style::ShoulderWidth, 0, -.32f);
            projected_bounds(m, 0, 0, b);
            const Frame &content = compact ? style::SmallContent : style::WideContent;
            if (b[0] < content.left || b[1] < content.top || b[2] > content.right || b[3] > content.bottom)
                break;
            best = candidate;
            best_left = candidate_left;
            memcpy(best_bounds, b, sizeof(b));
        }
        plan.w = best;
        plan.left = best_left;
        plan.total = 0;
        plan.edges[0] = plan.left;
        for (i = 0; i < plan.w.count; ++i) {
            plan.widths[i] = plan.w.first + i == plan.w.anchor ? style::LaneWidth : plan.secondary_width;
            plan.centers[i] = plan.edges[i] + plan.widths[i] * .5f;
            plan.edges[i + 1] = plan.edges[i] + plan.widths[i];
            plan.total += plan.widths[i];
        }
        memcpy(frame_bounds, best_bounds, sizeof(frame_bounds));
    }
    visible_count = plan.w.count;
    scene_secondary_width = plan.secondary_width;
    scene_exit_lanes = plan.exit_lanes;
    scene_head_min = scene_head_max = 0;
    if (plan.exit_lanes) {
        float m[16];
        camera_matrix(m);
        for (i = 0; i < mesh.vert_count; ++i)
            if (mesh.path_dist[i] >= path.total_length - 1e-5f) {
                const float *v = mesh.verts + i * 6;
                float gx, gz, offset;
                if (!lr_project_ground(m, v[0], v[1], v[2], &gx, &gz)) {
                    failed = true;
                    return;
                }
                offset = (gx - path.arrow_x) * sinf(path.arrow_angle) -
                         (gz - path.arrow_y) * cosf(path.arrow_angle);
                scene_head_min = fminf(scene_head_min, offset);
                scene_head_max = fmaxf(scene_head_max, offset);
            }
        if (scene_head_min < -.1301f || scene_head_max > .1301f) {
            failed = true;
            return;
        }
    }
    update_mesh_hash();
    hidden_side = plan.w.hidden_left ? -1 : plan.w.hidden_right ? 1 : 0;
    plan.junction = plan.exit_lanes ? path.segs[0].y1 : .25f;
    plan.join = {};
    forward_fade_start = .25f;
    if (plan.exit_lanes) {
        int side = plan.angle > 0 ? 1 : -1, through = -1;
        for (i = 0; i < s->count; ++i)
            if (continues_forward(s, i, plan.exit_first, plan.exit_last, plan.exit_lanes) &&
                (through < 0 || side > 0))
                through = i;
        if (through >= 0) {
            float edge = plan.lane_left(through + (side > 0 ? 1 : 0));
            plan.join = road_join(edge, side > 0 ? plan.exit_lo : plan.exit_hi, side);
            if (plan.join.valid)
                forward_fade_start = fmaxf(.25f, plan.join.cy + .040f);
        }
    }
}

void Builder::compile_road(const lr_state *s, lr_window window, int compact, float secondary_width) {
    RoadLayout plan = {};
    prepare_road(s, window, compact, secondary_width, plan);
    if (!failed && framing_ok)
        emit_road(s, plan);
}

} // namespace navigation
