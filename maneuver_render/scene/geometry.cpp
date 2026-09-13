#include "scene_internal.hpp"

namespace navigation {

void Builder::compile_native(const maneuver_state_t *m) {
    maneuver_build_route(m, &path);
    rpath_densify(&path);
    framing_ok = 1;
    scene_glyph_clearance = 1;
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
         * route must fit the stock crop, without the lane layout's inset. */
        const Frame &frame = style::SmallFrame;
        framing_ok = !failed && frame_bounds[0] >= frame.left && frame_bounds[1] >= frame.top &&
                     frame_bounds[2] <= frame.right && frame_bounds[3] <= frame.bottom;
        update_mesh_hash();
    }
}

void Builder::build_maneuver_path(route_path_t *p, const maneuver_state_t *m, float x, float scale, float y) {
    int i;
    maneuver_build_route(m, p);
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

void Builder::build_path(route_path_t *p, int angle, float x, float scale, float y) {
    maneuver_state_t m;
    memset(&m, 0, sizeof(m));
    m.icon = ICON_TURN;
    m.exit_angle = angle;
    build_maneuver_path(p, &m, x, scale, y);
}

void Builder::build_lane_path(int angle, float x, int lanes, float lo, float hi) {
    if (lanes <= 1 || abs(angle) > 100) {
        build_path(&path, angle, x, style::RouteScale, style::RouteOffsetY);
        return;
    }
    float side = angle > 0 ? 1.f : -1.f, theta = abs(angle) * PI / 180.f;
    float radius = .055f + (angle > 0 ? hi : -lo), start_y = -.04f;
    float start = angle > 0 ? PI : 0, end = start - angle * PI / 180.f;
    float cx = x + side * radius, ex = cx + radius * cosf(end), ey = start_y + radius * sinf(end);
    float leg = fmaxf(.04f, .45f - radius * tanf(theta * .5f));
    float heading = PI * .5f - angle * PI / 180.f;
    rpath_clear(&path);
    rpath_add_line(&path, x, style::EntryEnd, x, start_y);
    rpath_add_arc(&path, cx, start_y, radius, start, end);
    rpath_add_line(&path, ex, ey, ex + cosf(heading) * leg, ey + sinf(heading) * leg);
    rpath_set_arrow(&path, ex + cosf(heading) * leg, ey + sinf(heading) * leg, heading);
    rpath_densify(&path);
}

int Builder::continues_forward(const lr_state *s, int index, int first, int last, int lanes) {
    return !lanes || index < first || index > last || lr_has_direction(&s->lane[index], 0);
}

int Builder::through_branch_edge(const lr_state *s, int index, int side, int first, int last, int lanes) {
    int neighbor = index + side;
    return lanes && neighbor >= 0 && neighbor < s->count && continues_forward(s, index, first, last, lanes) &&
           !continues_forward(s, neighbor, first, last, lanes);
}

int Builder::recommended_exit(const lr_state *s, int index, int first, int last, int lanes, int angle) {
    const lr_lane *l = &s->lane[index];
    return lanes && index >= first && index <= last && l->status > 0 && lr_valid_angle(l->primary) &&
           lr_difference(l->primary, angle) < 5;
}

void Builder::paint_path(const route_path_t *p, float width, Color color, float a, int head) {
    int i;
    for (i = 1; i < p->pt_count; ++i)
        line(p->px[i - 1], p->py[i - 1], p->px[i], p->py[i], width, color, a);
    if (head)
        arrowhead(p->arrow_x, p->arrow_y, p->arrow_angle, width * 1.9f, color, a);
}

uint32_t Builder::glyph_hash_value(uint32_t hash, float value) {
    /* Ignore subpixel floating-point summation noise when adding context. */
    uint32_t v = (uint32_t)(int)roundf(value * 10000.f);
    for (int i = 0; i < 4; ++i) {
        hash = (hash ^ (v & 255)) * 16777619u;
        v >>= 8;
    }
    return hash;
}

void Builder::lane_glyphs(const lr_lane *l, int anchor, float x, float lane_width, int collect) {
    int j, k, iteration;
    float scale = .24f * fminf(1, lane_width / style::LaneWidth), shift = 0, lo = 0, hi = 0;
    /* The large arrow covers its approach lane. Put an alternative straight
     * cue beyond the fork, on the actual through continuation. */
    float glyph_y = anchor && lr_has_direction(l, 0) && lr_difference(l->primary, 0) >= 10 ? .18f : -.18f;
    /* Fit the whole multi-direction glyph together, preserving its common
     * stem. Include actual arrowhead corners, not just the centerline. */
    for (iteration = 0; iteration < 40; ++iteration) {
        lo = 1e6f;
        hi = -1e6f;
        for (j = 0; j < l->count + 1; ++j) {
            int a = j < l->count ? l->angles[j] : l->primary, dup = 0;
            if (!lr_valid_angle(a))
                continue;
            for (k = 0; k < j && k < l->count; ++k)
                if (lr_difference(a, l->angles[k]) < 5)
                    dup = 1;
            if (dup || (anchor && lr_difference(a, l->primary) < 5))
                continue;
            int active = l->status > 0 && lr_difference(a, l->primary) < 5;
            float width = fminf(active ? .040f : .034f, (lane_width - 2 * style::GlyphInset) / 3.8f);
            float size = width * 1.9f, dx, dy;
            build_path(&glyph, a, x, scale, glyph_y);
            dx = cosf(glyph.arrow_angle);
            dy = sinf(glyph.arrow_angle);
            for (k = 0; k < glyph.pt_count; ++k) {
                lo = fminf(lo, glyph.px[k] - width * .5f);
                hi = fmaxf(hi, glyph.px[k] + width * .5f);
            }
            lo = fminf(lo, fminf(glyph.arrow_x + dx * size, glyph.arrow_x - fabsf(dy) * size * .885f));
            hi = fmaxf(hi, fmaxf(glyph.arrow_x + dx * size, glyph.arrow_x + fabsf(dy) * size * .885f));
        }
        if (hi < lo)
            return;
        if (hi - lo <= lane_width - 2 * style::GlyphInset)
            break;
        scale *= .9f;
    }
    shift = x - (lo + hi) * .5f;
    scene_glyph_clearance = fminf(scene_glyph_clearance, (lane_width - (hi - lo)) * .5f);
    if (hi - lo > lane_width - 2 * style::GlyphInset + 1e-5f) {
        failed = true;
        return;
    }
    for (j = 0; j < l->count + 1; ++j) {
        int a = j < l->count ? l->angles[j] : l->primary, dup = 0;
        if (!lr_valid_angle(a))
            continue;
        for (k = 0; k < j && k < l->count; ++k)
            if (lr_difference(a, l->angles[k]) < 5)
                dup = 1;
        if (dup || (anchor && lr_difference(a, l->primary) < 5))
            continue;
        int active = l->status > 0 && lr_difference(a, l->primary) < 5;
        float width = fminf(active ? .040f : .034f, (lane_width - 2 * style::GlyphInset) / 3.8f);
        build_path(&glyph, a, x + shift, scale, glyph_y);
        if (collect) {
            float dx = cosf(glyph.arrow_angle), dy = sinf(glyph.arrow_angle), size = width * 1.9f;
            for (k = 0; k < glyph.pt_count; ++k) {
                key_point(glyph.px[k] - width * .5f, 0, glyph.py[k] - width * .5f);
                key_point(glyph.px[k] + width * .5f, 0, glyph.py[k] + width * .5f);
            }
            key_point(glyph.arrow_x + dx * size, 0, glyph.arrow_y + dy * size);
            key_point(glyph.arrow_x - dy * size * .885f, 0, glyph.arrow_y + dx * size * .885f);
            key_point(glyph.arrow_x + dy * size * .885f, 0, glyph.arrow_y - dx * size * .885f);
        } else {
            int id = l->position;
            if (id >= 0 && id < LR_MAX_LANES) {
                uint32_t h = (scene_glyph_seen & (1u << id)) ? scene_glyph_hash[id] : 2166136261u;
                h = glyph_hash_value(h, width);
                h = glyph_hash_value(h, glyph.arrow_angle);
                for (k = 0; k < glyph.pt_count; ++k) {
                    h = glyph_hash_value(h, glyph.px[k]);
                    h = glyph_hash_value(h, glyph.py[k]);
                }
                h = glyph_hash_value(h, glyph.arrow_x);
                h = glyph_hash_value(h, glyph.arrow_y);
                scene_glyph_hash[id] = h;
                scene_glyph_seen |= 1u << id;
            }
            paint_path(&glyph, width, active ? style::RecommendedGlyph : style::MutedGlyph, 1, 1);
        }
    }
}

void Builder::road_rect_from(float left, float width, float start, Color color) {
    int j;
    rect(left, start, width, forward_fade_start - start, color, 1);
    for (j = 0; j < 18; ++j) {
        float t = (j + .5f) / 18, alpha = 1 - t * t * (3 - 2 * t);
        rect(left, forward_fade_start + j * .024f, width, .0241f, color, alpha);
    }
}

void Builder::road_rect(float left, float width, Color color) {
    road_rect_from(left, width, style::EntryEnd, color);
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
        if (s->type == RSEG_ARC && s->radius + sign * offset <= 0) {
            float tx0 = -sinf(s->start_rad) * sign, ty0 = cosf(s->start_rad) * sign;
            float tx1 = -sinf(s->end_rad) * sign, ty1 = cosf(s->end_rad) * sign;
            float ex = s->cx + s->radius * cosf(s->end_rad);
            float ey = s->cy + s->radius * sinf(s->end_rad);
            float a = s->cx + s->radius * cosf(s->start_rad) + ty0 * offset;
            float b = s->cy + s->radius * sinf(s->start_rad) - tx0 * offset;
            float c = ex + ty1 * offset, d = ey - tx1 * offset;
            float det = tx0 * ty1 - ty0 * tx1, mx = s->cx, my = s->cy;
            if (fabsf(det) > 1e-5f) {
                float t = ((c - a) * ty1 - (d - b) * tx1) / det;
                mx = a + t * tx0;
                my = b + t * ty0;
            }
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
                   EXIT_FADE_LENGTH);
}

void Builder::exit_fade_end(void) {
    entry_fade(style::EntryEnd, style::EntryFade);
}

Builder::road_join_t Builder::road_join(float edge, float exit_offset, int side) {
    road_join_t join = {};
    float ax, ay;
    exit_point(0, exit_offset, &ax, &ay);
    for (int i = 1; i <= path.pt_count; ++i) {
        float bx, by;
        exit_point(i, exit_offset, &bx, &by);
        float dx = bx - ax, dy = by - ay;
        if (side * dx > 1e-6f && side * (edge - ax) >= -1e-6f && side * (bx - edge) >= -1e-6f) {
            float length = hypotf(dx, dy), theta = atan2f(side * dx, dy);
            if (theta < .55f || theta > 1.8f)
                return join;
            float t = fminf(1, fmaxf(0, (edge - ax) / dx));
            float radius = fminf(.040f, .10f * tanf(theta * .5f));
            if (radius < .030f)
                return join;
            float tangent = radius / tanf(theta * .5f), ex, ey;
            float y = ay + t * dy;
            exit_point(path.pt_count, exit_offset, &ex, &ey);
            if (((ex - edge) * dx + (ey - y) * dy) / length < tangent + style::ShoulderWidth)
                return join;
            join.valid = 1;
            join.side = side;
            join.x = edge;
            join.y = y;
            join.cx = edge + side * radius;
            join.cy = y + tangent;
            join.radius = radius;
            join.sweep = PI - theta;
            return join;
        }
        ax = bx;
        ay = by;
    }
    return join;
}

void Builder::paint_road_join(const road_join_t *join, int outline) {
    if (!join->valid)
        return;
    float start = join->side > 0 ? PI : 0;
    for (int i = 0; i < 20; ++i) {
        float a = start + join->side * join->sweep * i / 20.f;
        float b = start + join->side * join->sweep * (i + 1) / 20.f;
        float ax = join->cx + join->radius * cosf(a), ay = join->cy + join->radius * sinf(a);
        float bx = join->cx + join->radius * cosf(b), by = join->cy + join->radius * sinf(b);
        if (outline) {
            float r = join->radius - style::ShoulderWidth;
            float cx = join->cx + r * cosf(a), cy = join->cy + r * sinf(a);
            float dx = join->cx + r * cosf(b), dy = join->cy + r * sinf(b);
            triangle(ax, ay, cx, cy, dx, dy, style::Shoulder, 1);
            triangle(ax, ay, dx, dy, bx, by, style::Shoulder, 1);
        } else
            triangle(join->x, join->y, ax, ay, bx, by, style::Asphalt, 1);
    }
}

void Builder::exit_strip(float lo, float hi, Color color, int dashed) {
    int i;
    for (i = 1; i <= path.pt_count + 1; ++i) {
        float ax, ay, bx, by, cx, cy, dx, dy;
        exit_point(i - 1, lo, &ax, &ay);
        exit_point(i - 1, hi, &bx, &by);
        exit_point(i, hi, &cx, &cy);
        exit_point(i, lo, &dx, &dy);
        /* Curbs belong to the departing curve/leg, never a solid barrier
         * down an internal approach lane before the junction. */
        if (dashed == 2 && i < path.pt_count && fabsf(path.px[i] - path.px[i - 1]) < 1e-6f)
            continue;
        /* Keep pavement under the arrow head solid, then fade the complete
         * cross-section along its exit heading. Curbs, tint and dividers use
         * exactly the same plane; the route shader is unaffected. */
        if (i == path.pt_count + 1)
            exit_fade_begin();
        if (dashed != 1) {
            triangle(ax, ay, bx, by, cx, cy, color, 1);
            triangle(ax, ay, cx, cy, dx, dy, color, 1);
        } else {
            /* Dashes are distances, not a sample-index switch: long straight
             * exit segments must not turn into a single solid separator. */
            float start = i > path.pt_count ? path.total_length + EXIT_SOLID_REACH : path.dist[i - 1];
            float end = i >= path.pt_count
                            ? path.total_length + EXIT_SOLID_REACH + (i - path.pt_count) * EXIT_FADE_LENGTH
                            : path.dist[i];
            float pos = start, length = end - start;
            while (length > 1e-6f && pos < end - 1e-6f) {
                int cell = (int)floorf((pos + 1e-6f) / .105f);
                float next = fminf(end, (cell + 1) * .105f), t0 = (pos - start) / length,
                      t1 = (next - start) / length;
                if (!(cell % 2)) {
                    float x0 = ax + (dx - ax) * t0, y0 = ay + (dy - ay) * t0;
                    float x1 = bx + (cx - bx) * t0, y1 = by + (cy - by) * t0;
                    float x2 = bx + (cx - bx) * t1, y2 = by + (cy - by) * t1;
                    float x3 = ax + (dx - ax) * t1, y3 = ay + (dy - ay) * t1;
                    triangle(x0, y0, x1, y1, x2, y2, color, 1);
                    triangle(x0, y0, x2, y2, x3, y3, color, 1);
                }
                pos = next;
            }
        }
    }
    exit_fade_end();
}

void Builder::lane_tint(float x, float width, float end) {
    int j;
    rect(x - width * .45f, style::EntryEnd, width * .90f, fminf(.05f, end) + 1.4f, style::RecommendedPavement,
         1);
    for (j = 0; j < 14; ++j) {
        float t = (j + .5f) / 14, a = 1 - t * t * (3 - 2 * t);
        if (.05f + j * .014f >= end)
            break;
        Color tint = {style::Asphalt.r + style::RecommendationGain.r * a,
                      style::Asphalt.g + style::RecommendationGain.g * a,
                      style::Asphalt.b + style::RecommendationGain.b * a};
        rect(x - width * .45f, .05f + j * .014f, width * .90f, .0141f, tint, 1);
    }
}

void Builder::lane_divider(float x, float end) {
    int j;
    for (j = 0; j < 7; ++j) {
        float y = -1.15f + j * .21f;
        if (y >= end)
            break;
        line(x, y, x, fminf(y + .095f, end), .010f, style::Divider, 1);
    }
}

void Builder::emit_road(const lr_state *s, const RoadLayout &plan) {
    int i;
    scene_glyph_seen = 0;
    memset(scene_glyph_hash, 0, sizeof(scene_glyph_hash));

    begin_outline();
    /* Outline the union, not the individual overlapping roads: thin
     * contours go FIRST, then every asphalt surface covers interior plan.edges.
     * Never paint a whole white road-shaped backing surface. */
    if (plan.exit_lanes) {
        exit_strip(plan.exit_lo - style::ShoulderWidth, plan.exit_lo, style::Shoulder, 2);
        exit_strip(plan.exit_hi, plan.exit_hi + style::ShoulderWidth, style::Shoulder, 2);
    }
    for (int side = -1; side <= 1; side += 2) {
        int index = side < 0 ? 0 : s->count - 1;
        float edge = side < 0 ? plan.lane_left(0) - style::ShoulderWidth : plan.lane_left(s->count);
        if (continues_forward(s, index, plan.exit_first, plan.exit_last, plan.exit_lanes))
            road_rect(edge, style::ShoulderWidth, style::Shoulder);
        else
            rect(edge, style::EntryEnd, style::ShoulderWidth, plan.junction - style::EntryEnd,
                 style::Shoulder, 1);
    }
    for (i = 0; i < s->count; ++i) {
        if (through_branch_edge(s, i, -1, plan.exit_first, plan.exit_last, plan.exit_lanes))
            road_rect_from(plan.lane_left(i) - style::ShoulderWidth, style::ShoulderWidth, plan.junction,
                           style::Shoulder);
        if (through_branch_edge(s, i, 1, plan.exit_first, plan.exit_last, plan.exit_lanes))
            road_rect_from(plan.lane_left(i + 1), style::ShoulderWidth, plan.junction, style::Shoulder);
    }
    paint_road_join(&plan.join, 1);
    end_mask();
    begin_fill();
    if (plan.exit_lanes)
        exit_strip(plan.exit_lo, plan.exit_hi, style::Asphalt, 0);
    /* Turning separators also sit underneath any through-road pavement. */
    if (plan.exit_lanes > 1)
        for (i = plan.exit_first + 1; i <= plan.exit_last; ++i) {
            float offset = i <= plan.w.anchor
                               ? -(style::LaneWidth * .5f) + (i - plan.w.anchor) * plan.secondary_width
                               : (style::LaneWidth * .5f) + (i - plan.w.anchor - 1) * plan.secondary_width;
            exit_strip(offset - .005f, offset + .005f, style::Divider, 1);
        }
    /* Turn-only lanes end their straight approach at the common tangent.
     * A straight+turn lane legitimately contributes BOTH road surfaces. */
    rect(plan.lane_left(0), style::EntryEnd, plan.lane_left(s->count) - plan.lane_left(0),
         plan.junction - style::EntryEnd, style::Asphalt, 1);
    for (i = 0; i < s->count; ++i) {
        if (continues_forward(s, i, plan.exit_first, plan.exit_last, plan.exit_lanes))
            road_rect(plan.lane_left(i), plan.lane_width(i), style::Asphalt);
    }
    paint_road_join(&plan.join, 0);
    for (i = 0; i < s->count; ++i) {
        int forward = continues_forward(s, i, plan.exit_first, plan.exit_last, plan.exit_lanes);
        int selected_turn = recommended_exit(s, i, plan.exit_first, plan.exit_last, plan.exit_lanes,
                                             (int)plan.angle);
        if (s->lane[i].status > 0)
            lane_tint(plan.lane_center(i), plan.lane_width(i), forward && !selected_turn ? 1.f : plan.junction);
    }
    /* Paint the complete road independently of the viewport. A cached crop
     * would rotate with the road during travel and slice off visible corners.
     * Only entry/exit distance fades belong in the retained road geometry. */
    for (i = 0; i < s->count; ++i) {
        if (recommended_exit(s, i, plan.exit_first, plan.exit_last, plan.exit_lanes, (int)plan.angle)) {
            float offset = plan.lane_center(i) - plan.ax, half = plan.lane_width(i) * .45f;
            exit_strip(offset - half, offset + half, style::RecommendedPavement, 0);
        }
    }
    for (i = 0; i < s->count; ++i)
        lane_glyphs(&s->lane[i], i == plan.w.anchor, plan.lane_center(i), plan.lane_width(i), 0);
    for (i = 1; i < s->count; ++i) {
        int both = continues_forward(s, i - 1, plan.exit_first, plan.exit_last, plan.exit_lanes) &&
                   continues_forward(s, i, plan.exit_first, plan.exit_last, plan.exit_lanes);
        lane_divider(plan.lane_left(i), both ? 1.f : plan.junction);
    }
    end_mask();
    begin_route();
    /* Empty route mask: use the same mesh/contact shading as the production
     * maneuver path. Its legacy mask shadow is not enabled there either. */
    end_mask();
}

void Builder::compile_unmarked(const maneuver_state_t *maneuver) {
    /* Use only the independent maneuver. No lane widths/counts/positions,
     * lane glyphs or inferred branch connectivity survive into this view. */
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
        visible_count = hidden_side = scene_exit_lanes = 0;
        scene_secondary_width = 0;
        scene_glyph_seen = 0;
        memset(scene_glyph_hash, 0, sizeof(scene_glyph_hash));
        scene_glyph_clearance = 1;
        scene_head_min = scene_head_max = 0;
        update_mesh_hash();

        begin_outline();
        /* Same cross-section and edge geometry as one lane of a lane road.
         * Paint only real curb strips, with the same directional end fade. */
        const float half = style::LaneWidth * .5f;
        exit_strip(-half - style::ShoulderWidth, -half, style::Shoulder, 0);
        exit_strip(half, half + style::ShoulderWidth, style::Shoulder, 0);
        exit_strip(-half, half, style::Asphalt, 0);
        end_mask();
        begin_route();
        end_mask();
    }
}

} // namespace navigation
