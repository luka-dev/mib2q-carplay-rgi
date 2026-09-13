#include "scene_internal.hpp"
#include "../render.h"
#include <new>

struct cr_scene {
    navigation::DisplayList display;
    navigation::Builder *builder;
    cr_scene_input_t input;
    cr_scene_view_t view;
    cr_scene_info_t info;
    route_path_t route;
    bool ready;
};

namespace {
bool same_maneuver(const maneuver_state_t &a, const maneuver_state_t &b) {
    return a.icon == b.icon && a.exit_angle == b.exit_angle && a.direction == b.direction &&
           a.driving_side == b.driving_side && a.bap_geometry == b.bap_geometry &&
           a.junction_angle_count == b.junction_angle_count &&
           !std::memcmp(a.junction_angles, b.junction_angles, a.junction_angle_count * sizeof(float));
}
bool same_input(const cr_scene_input_t &a, const cr_scene_input_t &b) {
    if (a.generation != b.generation || a.previous_lane != b.previous_lane ||
        !same_maneuver(a.maneuver, b.maneuver) || a.lanes.count != b.lanes.count ||
        a.lanes.complete != b.lanes.complete)
        return false;
    for (int i = 0; i < a.lanes.count; ++i) {
        const lr_lane &x = a.lanes.lane[i], &y = b.lanes.lane[i];
        if (x.position != y.position || x.status != y.status || x.primary != y.primary ||
            x.count != y.count || std::memcmp(x.angles, y.angles, x.count * sizeof(int)))
            return false;
    }
    return true;
}
} // namespace

extern "C" cr_scene_t *cr_scene_create(void) {
    void *storage = std::calloc(1, sizeof(cr_scene));
    if (!storage)
        return nullptr;
    cr_scene *s = new (storage) cr_scene();
    void *scratch = std::calloc(1, sizeof(navigation::Builder));
    if (!scratch) {
        s->~cr_scene();
        std::free(storage);
        return nullptr;
    }
    s->builder = new (scratch) navigation::Builder();
    return s;
}
extern "C" void cr_scene_destroy(cr_scene_t *s) {
    if (!s)
        return;
    s->builder->~Builder();
    std::free(s->builder);
    s->~cr_scene();
    std::free(s);
}

extern "C" void cr_scene_configure_provider(maneuver_scene_provider_t *provider) {
    if (!provider)
        return;
    provider->route_base_y = navigation::style::RouteBase;
    provider->route_top_y = navigation::style::RouteTop;
    provider->entry_road_reach = navigation::style::EntryRoadReach;
    provider->exit_road_reach = navigation::EXIT_ROAD_REACH;
}

extern "C" int cr_scene_prepare(cr_scene_t *s, const cr_scene_input_t *input, const cr_scene_view_t *view) {
    if (!s)
        return 0;
    if (!input || !view) {
        s->ready = false;
        return 0;
    }
    bool was_ready = s->ready;
    s->ready = false;
    if (input->maneuver.icon < ICON_NONE || input->maneuver.icon >= ICON_COUNT ||
        !cr_lane_angle_valid(input->maneuver.exit_angle) || input->maneuver.junction_angle_count < 0 ||
        input->maneuver.junction_angle_count > MAX_JUNCTION_ANGLES)
        return 0;
    for (int i = 0; i < 16; ++i)
        if (!std::isfinite(view->projection[i]))
            return 0;
    for (int i = 0; i < input->maneuver.junction_angle_count; ++i)
        if (!cr_lane_angle_valid(input->maneuver.junction_angles[i]))
            return 0;
    float ground_x, ground_y;
    if (!lr_project_ground(view->projection, 0, 0, 0, &ground_x, &ground_y))
        return 0;
    cr_scene_input_t normalized = *input;
    lr_normalize(&normalized.lanes);
    if (was_ready && same_input(s->input, normalized) && s->view.compact == view->compact &&
        !std::memcmp(s->view.projection, view->projection, sizeof(view->projection))) {
        s->ready = true;
        return 1;
    }
    navigation::Builder &b = *s->builder;
    b.reset(&s->display, view->projection);
    lr_window window = lr_choose(&normalized.lanes, 3, normalized.previous_lane);
    const maneuver_state_t &m = normalized.maneuver;
    cr_lane_safety_result_t safety =
        lr_geometry_check(&normalized.lanes, window.anchor, m.icon == ICON_TURN, m.exit_angle);
    cr_scene_kind_t kind = safety == CR_LANE_ROAD_OK ? CR_SCENE_LANES : CR_SCENE_UNMARKED;
    /* Preserve the complete ordinary family whenever it carries more than
     * the simple unmarked path: no lost junction branches, circle or flag. */
    if (kind != CR_SCENE_LANES &&
        (m.icon != ICON_TURN || m.junction_angle_count || std::fabs(m.exit_angle) > 100))
        kind = CR_SCENE_NATIVE;
    if (kind == CR_SCENE_LANES)
        b.compile_road(&normalized.lanes, window, view->compact, navigation::style::LaneWidth);
    else if (kind == CR_SCENE_UNMARKED)
        b.compile_unmarked(&m);
    cr_scene_fallback_t fallback = CR_SCENE_FALLBACK_NONE;
    if (kind != CR_SCENE_NATIVE && (b.failed || s->display.overflow || !b.framing_ok || !b.mesh.valid)) {
        fallback = s->display.overflow         ? CR_SCENE_FALLBACK_CAPACITY
                   : b.failed || !b.mesh.valid ? CR_SCENE_FALLBACK_GEOMETRY
                                               : CR_SCENE_FALLBACK_FRAMING;
        kind = CR_SCENE_NATIVE;
        b.reset(&s->display, view->projection);
    }
    if (kind == CR_SCENE_NATIVE)
        b.compile_native(&m);
    if (b.failed || (!b.mesh.valid && m.icon != ICON_NONE))
        return 0;
    s->route = b.path;
    /* The native motion engine adds its own animation extensions. Masks keep
     * the longer hidden pavement; the route retains its nominal entry. */
    if (kind != CR_SCENE_NATIVE)
        s->route.segs[0].y0 = navigation::style::NominalEntry;
    for (int i = 0; i < s->route.seg_count; ++i) {
        route_seg_t &seg = s->route.segs[i];
        seg.x0 -= b.camera_x;
        seg.x1 -= b.camera_x;
        seg.cx -= b.camera_x;
        seg.y0 -= b.camera_y;
        seg.y1 -= b.camera_y;
        seg.cy -= b.camera_y;
    }
    s->route.arrow_x -= b.camera_x;
    s->route.arrow_y -= b.camera_y;
    rpath_densify(&s->route);
    cr_scene_info_t &d = s->info;
    d.kind = kind;
    d.camera_x = b.camera_x;
    d.camera_y = b.camera_y;
    std::memcpy(d.bounds, b.frame_bounds, sizeof(d.bounds));
    d.secondary_width = b.scene_secondary_width;
    d.glyph_clearance = b.scene_glyph_clearance;
    d.head_min = b.scene_head_min;
    d.head_max = b.scene_head_max;
    d.framed = b.framing_ok;
    d.visible_count = b.visible_count;
    d.hidden_side = b.hidden_side;
    d.exit_lanes = b.scene_exit_lanes;
    d.mesh_hash = b.scene_mesh_hash;
    d.glyph_seen = b.scene_glyph_seen;
    std::memcpy(d.glyph_hash, b.scene_glyph_hash, sizeof(d.glyph_hash));
    d.safety = safety;
    d.fallback = fallback;
    d.command_count = s->display.count;
    ++d.builds;
    s->input = normalized;
    s->view = *view;
    s->ready = true;
    return 1;
}

extern "C" const route_path_t *cr_scene_route(const cr_scene_t *s) {
    return s && s->ready ? &s->route : nullptr;
}
extern "C" const cr_scene_info_t *cr_scene_info(const cr_scene_t *s) {
    return s ? &s->info : nullptr;
}
extern "C" int cr_scene_is_native(const cr_scene_t *s) {
    return s && s->ready && s->info.kind == CR_SCENE_NATIVE;
}

extern "C" void cr_scene_paint(cr_scene_t *s, float tx, float ty, float c, float sn) {
    if (!s || !s->ready || s->info.kind == CR_SCENE_NATIVE)
        return;
    const cr_scene_info_t &d = s->info;
    render_push_mask_transform(tx - c * d.camera_x + sn * d.camera_y, ty - sn * d.camera_x - c * d.camera_y,
                               c, sn);
    render_set_mask_entry_fade(navigation::style::EntryEnd, navigation::style::EntryFade);
    for (unsigned i = 0; i < s->display.count; ++i) {
        const navigation::Command &cmd = s->display.commands[i];
        const float *v = cmd.v;
        using navigation::Op;
        switch (cmd.op) {
        case Op::Rect:
            render_rect(v[0], v[1], v[2], v[3], v[4], v[5], v[6], v[7]);
            break;
        case Op::Triangle:
            render_triangle(v[0], v[1], v[2], v[3], v[4], v[5], v[6], v[7], v[8], v[9]);
            break;
        case Op::Line:
            render_thick_line(v[0], v[1], v[2], v[3], v[4], v[5], v[6], v[7], v[8]);
            break;
        case Op::Arrowhead:
            render_arrowhead(v[0], v[1], v[2], v[3], v[4], v[5], v[6], v[7]);
            break;
        case Op::Disc:
            render_disc(v[0], v[1], v[2], (int)v[3], v[4], v[5], v[6], v[7]);
            break;
        case Op::DirectionFade:
            render_set_mask_direction_fade(v[0], v[1], v[2], v[3], v[4]);
            break;
        case Op::BeginOutline:
            render_begin_outline_mask();
            break;
        case Op::BeginFill:
            render_begin_fill_mask();
            break;
        case Op::BeginRoute:
            render_begin_route_mask();
            break;
        case Op::EndMask:
            render_end_outline_mask();
            break;
        }
    }
    render_pop_mask_transform();
    render_set_mask_entry_fade(0, 0);
    ++s->info.paint_count;
}
