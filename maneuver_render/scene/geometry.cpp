#include "scene_internal.hpp"

namespace navigation {

void Builder::compile_native(const maneuver_state_t *m) {
    maneuver_build_route(m, &path);
    rpath_densify(&path);
    framing_ok = 1;
    if (m->icon != ICON_NONE) {
        float e = maneuver_builtin_elevation(m);
        rpath_set_elevation(e, e);
        rpath_set_ramp_restart(-1);
        rpath_extrude(&path, &mesh, style::ArrowWidth, style::RouteBase, style::RouteTop, 0, 1);
        rpath_set_elevation(0, 0);
        for (int i = 0; i < mesh.vert_count; ++i) {
            const float *v = mesh.verts + i * 6;
            if (v[2] >= -.38f)
                key_point(v[0], v[1], v[2]);
        }
        projected_bounds(projection, 0, 0, frame_bounds);
        /* Built-in scenes retain their original pose and route. Their
         * route must fit the stock crop, without the retained road layout's inset. */
        const Frame &frame = style::SmallFrame;
        framing_ok = !failed && frame_bounds[0] >= frame.left && frame_bounds[1] >= frame.top &&
                     frame_bounds[2] <= frame.right && frame_bounds[3] <= frame.bottom;
        update_mesh_hash();
    }
}

void Builder::build_maneuver_path(route_path_t *p, const maneuver_state_t *m, float x, float scale, float y) {
    int i;
    maneuver_build_route(m, p);
    float entry_x = p->seg_count ? p->segs[0].x0 : 0;
    x -= entry_x * scale;
    for (i = 0; i < p->seg_count; ++i) {
        route_seg_t *s = &p->segs[i];
        s->x0 = s->x0 * scale + x;
        s->x1 = s->x1 * scale + x;
        s->y0 = s->y0 * scale + y;
        s->y1 = s->y1 * scale + y;
        s->cx = s->cx * scale + x;
        s->cy = s->cy * scale + y;
        s->radius *= scale;
    }
    p->arrow_x = p->arrow_x * scale + x;
    p->arrow_y = p->arrow_y * scale + y;
    rpath_densify(p);
}

static bool collapsed_corner(const route_seg_t &s, float offset, float *x, float *y) {
    float sign = s.end_rad >= s.start_rad ? 1.f : -1.f;
    if (s.type != RSEG_ARC || s.radius + sign * offset > 0) return false;
    float tx0 = -sinf(s.start_rad) * sign, ty0 = cosf(s.start_rad) * sign;
    float tx1 = -sinf(s.end_rad) * sign, ty1 = cosf(s.end_rad) * sign;
    float ex = s.cx + s.radius * cosf(s.end_rad), ey = s.cy + s.radius * sinf(s.end_rad);
    float a = s.cx + s.radius * cosf(s.start_rad) + ty0 * offset;
    float b = s.cy + s.radius * sinf(s.start_rad) - tx0 * offset;
    float c = ex + ty1 * offset, d = ey - tx1 * offset;
    float det = tx0 * ty1 - ty0 * tx1;
    *x = s.cx; *y = s.cy;
    if (fabsf(det) > 1e-5f) {
        float t = ((c - a) * ty1 - (d - b) * tx1) / det;
        *x = a + t * tx0;
        *y = b + t * ty0;
    }
    return true;
}

void Builder::exit_point(int i, float offset, float *x, float *y) {
    int n = path.pt_count;
    float dx, dy, len;
    if (i >= n) {
        dx = cosf(path.arrow_angle);
        dy = sinf(path.arrow_angle);
        float reach = EXIT_SOLID_REACH + (i - n) * EXIT_FADE_LENGTH;
        *x = path.arrow_x + dx * reach;
        *y = path.arrow_y + dy * reach;
    } else {
        int before = i ? i - 1 : 0, after = i + 1 < n ? i + 1 : n - 1;
        dx = path.px[after] - path.px[before];
        dy = path.py[after] - path.py[before];
        len = sqrtf(dx * dx + dy * dy);
        if (len < 1e-6f) {
            dx = 0;
            dy = 1;
        } else {
            dx /= len;
            dy /= len;
        }
        *x = path.px[i];
        *y = path.py[i];
    }
    float center_x = *x, center_y = *y;
    *x += dy * offset;
    *y -= dx * offset;
    /* A wide road's inner offset can exceed the arrow fillet radius. Collapse
     * that arc to the offset tangents' intersection. Also trim the outgoing
     * line against this corner: a short exit can end before the corner, and
     * connecting it backwards paints a white spur across the approach. */
    for (int k = 0; k < path.seg_count; ++k) {
        const route_seg_t *s = &path.segs[k];
        float sign = s->end_rad >= s->start_rad ? 1.f : -1.f;
        float mx, my;
        if (collapsed_corner(*s, offset, &mx, &my)) {
            float tx1 = -sinf(s->end_rad) * sign, ty1 = cosf(s->end_rad) * sign;
            float ex = s->cx + s->radius * cosf(s->end_rad);
            float ey = s->cy + s->radius * sinf(s->end_rad);
            int on_arc = fabsf(hypotf(center_x - s->cx, center_y - s->cy) - s->radius) < 1e-5f;
            int on_exit = fabsf((center_x - ex) * ty1 - (center_y - ey) * tx1) < 1e-5f &&
                          (center_x - ex) * tx1 + (center_y - ey) * ty1 >= -1e-5f;
            if (on_arc || (on_exit && (*x - mx) * tx1 + (*y - my) * ty1 < 0)) {
                *x = mx;
                *y = my;
                return;
            }
        }
    }
}

void Builder::exit_fade_begin(void) {
    float dx = cosf(path.arrow_angle), dy = sinf(path.arrow_angle);
    direction_fade(path.arrow_x + dx * EXIT_ROAD_REACH, path.arrow_y + dy * EXIT_ROAD_REACH, -dx, -dy,
                   exit_fade_length);
}

void Builder::exit_fade_end(void) {
    entry_fade(style::EntryEnd, style::EntryFade);
}

void Builder::exit_strip(float lo, float hi, Color color) {
    for (int i = 1; i <= path.pt_count + 1; ++i) {
        float ax, ay, bx, by, cx, cy, dx, dy;
        exit_point(i - 1, lo, &ax, &ay);
        exit_point(i - 1, hi, &bx, &by);
        exit_point(i, hi, &cx, &cy);
        exit_point(i, lo, &dx, &dy);
        if (i == path.pt_count + 1) exit_fade_begin();
        triangle(ax, ay, bx, by, cx, cy, color, 1);
        triangle(ax, ay, cx, cy, dx, dy, color, 1);
    }
    exit_fade_end();
}

void Builder::compile_road(const maneuver_state_t *maneuver) {
    /* Retain the accepted ordinary road shape, framing and soft end fade. */
    {
        build_maneuver_path(&path, maneuver, 0, style::RouteScale, style::RouteOffsetY);
        path.segs[0].y0 = style::EntryEnd;
        rpath_densify(&path);
        rpath_set_elevation(0, 0);
        rpath_set_ramp_restart(-1);
        rpath_extrude(&path, &mesh, style::ArrowWidth, style::RouteBase, style::RouteTop, 0, 1);
        point_count = 0;
        for (int i = 0; i < mesh.vert_count; ++i) {
            const float *v = mesh.verts + i * 6;
            if (v[2] >= -.38f)
                key_point(v[0], v[1], v[2]);
        }
        frame_scene(0);
        update_mesh_hash();

        begin_outline();
        /* Thin curb strips share the pavement's directional end fade. */
        const float half = style::RoadWidth * .5f;
        exit_strip(-half - style::ShoulderWidth, -half, style::Shoulder);
        exit_strip(half, half + style::ShoulderWidth, style::Shoulder);
        exit_strip(-half, half, style::Asphalt);
        end_mask();
        begin_route();
        end_mask();
    }
}

} // namespace navigation
