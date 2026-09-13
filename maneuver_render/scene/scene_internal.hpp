#ifndef CR_SCENE_INTERNAL_HPP
#define CR_SCENE_INTERNAL_HPP
#include "scene.h"
#include "style.hpp"
#include <cmath>
#include <cstring>
#include <cstdio>
#include <cstdlib>

namespace navigation {
constexpr float PI = 3.14159265358979323846f;
constexpr float EXIT_SOLID_REACH = style::ExitSolid;
constexpr float EXIT_FADE_LENGTH = style::ExitFade;
constexpr float EXIT_ROAD_REACH = EXIT_SOLID_REACH + EXIT_FADE_LENGTH;
enum class Op {
    Rect,
    Triangle,
    Line,
    Arrowhead,
    Disc,
    DirectionFade,
    BeginOutline,
    BeginFill,
    BeginRoute,
    EndMask
};
struct Command {
    Op op;
    float v[12];
};
constexpr unsigned MaxCommands = 8192;
struct DisplayList {
    Command commands[MaxCommands];
    unsigned count;
    bool overflow;
    void push(Op op, const float *v, unsigned n) {
        if (count == MaxCommands) {
            overflow = true;
            return;
        }
        Command &c = commands[count++];
        c.op = op;
        if (n)
            std::memcpy(c.v, v, n * sizeof(float));
    }
};

/* A compiler emits a retained list of geometry operations. There is no GL,
 * live event, fixture number or animation clock in this layer. */
struct Builder {
    route_path_t path, glyph;
    route_mesh_t mesh;
    float camera_x, camera_y;
    int framing_ok;
    float frame_bounds[4];
    static constexpr int MaxKeys = RMESH_MAX_VERTS + 2048;
    float points[MaxKeys][3];
    int point_count;
    int visible_count;
    int hidden_side;
    float scene_secondary_width;
    uint32_t scene_mesh_hash;
    int scene_exit_lanes;
    float scene_glyph_clearance;
    float scene_head_min, scene_head_max;
    uint32_t scene_glyph_hash[LR_MAX_LANES];
    unsigned scene_glyph_seen;
    float forward_fade_start;
    DisplayList *list;
    float projection[16], matrix[16], pan_x, pan_y;
    bool failed;
    void camera_pan(float x, float y) {
        pan_x = x;
        pan_y = y;
    }
    void camera_sync() {
        std::memcpy(matrix, projection, sizeof(matrix));
        for (int i = 0; i < 4; ++i)
            matrix[12 + i] -= projection[i] * pan_x + projection[8 + i] * pan_y;
    }
    void camera_matrix(float out[16]) {
        std::memcpy(out, matrix, sizeof(matrix));
    }
    void emit(Op op, const float *v, unsigned n) {
        list->push(op, v, n);
    }
    void rect(float x, float y, float w, float h, Color color, float a) {
        const float v[] = {x, y, w, h, color.r, color.g, color.b, a};
        emit(Op::Rect, v, 8);
    }
    void triangle(float x, float y, float x1, float y1, float x2, float y2, Color color, float a) {
        const float v[] = {x, y, x1, y1, x2, y2, color.r, color.g, color.b, a};
        emit(Op::Triangle, v, 10);
    }
    void line(float x, float y, float x1, float y1, float w, Color color, float a) {
        const float v[] = {x, y, x1, y1, w, color.r, color.g, color.b, a};
        emit(Op::Line, v, 9);
    }
    void arrowhead(float x, float y, float angle, float size, Color color, float a) {
        const float v[] = {x, y, angle, size, color.r, color.g, color.b, a};
        emit(Op::Arrowhead, v, 8);
    }
    void disc(float x, float y, float radius, int segments, Color color, float a) {
        const float v[] = {x, y, radius, (float)segments, color.r, color.g, color.b, a};
        emit(Op::Disc, v, 8);
    }
    void direction_fade(float x, float y, float dx, float dy, float span) {
        const float v[] = {x, y, dx, dy, span};
        emit(Op::DirectionFade, v, 5);
    }
    void entry_fade(float start, float span) {
        direction_fade(0, start, 0, 1, span);
    }
    void begin_outline() {
        emit(Op::BeginOutline, nullptr, 0);
    }
    void begin_fill() {
        emit(Op::BeginFill, nullptr, 0);
    }
    void begin_route() {
        emit(Op::BeginRoute, nullptr, 0);
    }
    void end_mask() {
        emit(Op::EndMask, nullptr, 0);
    }
    void reset(DisplayList *output, const float camera[16]) {
        std::memset(this, 0, sizeof(*this));
        list = output;
        output->count = 0;
        output->overflow = false;
        std::memcpy(projection, camera, sizeof(projection));
        camera_sync();
    }
    void update_mesh_hash() {
        const unsigned char *bytes = reinterpret_cast<const unsigned char *>(mesh.verts);
        scene_mesh_hash = 2166136261u;
        for (size_t i = 0; i < static_cast<size_t>(mesh.vert_count) * 6 * sizeof(float); ++i)
            scene_mesh_hash = (scene_mesh_hash ^ bytes[i]) * 16777619u;
    }
    void compile_native(const maneuver_state_t *maneuver);
    void key_point(float x, float h, float y);
    void projected_bounds(const float m[16], float px, float py, float b[4]);

    /* Pan only: production FOV/eye/tilt and route thickness stay unchanged.
     * Fit importance bounds in the existing source-pixel rectangle, with 8px inset.
     * Camera depth translation is capped at 0.08 world units (~6% of eye distance). */
    void frame_scene(float arrow_x);

    /* Scale and translate existing TURN geometry, preserving its filleted shape. */
    void build_maneuver_path(route_path_t *p, const maneuver_state_t *m, float x, float scale, float y);
    void build_path(route_path_t *p, int angle, float x, float scale, float y);
    /* A lane bundle needs a common circular bend whose INNER curb still has a
     * positive radius. Offsetting the tiny standalone-arrow fillet made the
     * innermost lanes fold backwards or collapse into square corners. Keep the
     * approach tangent fixed; wider outer lanes finish their turn farther ahead. */
    void build_lane_path(int angle, float x, int lanes, float lo, float hi);
    int continues_forward(const lr_state *s, int index, int first, int last, int lanes);
    /* A departing turn-only neighbor exposes a new physical side of the through
     * branch. A shared straight+turn lane does not: its junction stays open. */
    int through_branch_edge(const lr_state *s, int index, int side, int first, int last, int lanes);
    int recommended_exit(const lr_state *s, int index, int first, int last, int lanes, int angle);
    void paint_path(const route_path_t *p, float width, Color color, float a, int head);
    uint32_t glyph_hash_value(uint32_t hash, float value);
    void lane_glyphs(const lr_lane *l, int anchor, float x, float lane_width, int collect);
    void road_rect_from(float left, float width, float start, Color color);
    void road_rect(float left, float width, Color color);
    void exit_point(int i, float offset, float *x, float *y);
    void exit_fade_begin(void);
    void exit_fade_end(void);
    typedef struct {
        int valid, side;
        float x, y, cx, cy, radius, sweep;
    } road_join_t;
    /* Round only a real intersection of the through edge with the departing
     * outer edge. Tangential departures already join smoothly. No new lane or
     * barrier is introduced: the tiny patch lies in the outside corner. */
    road_join_t road_join(float edge, float exit_offset, int side);
    void paint_road_join(const road_join_t *join, int outline);
    void exit_strip(float lo, float hi, Color color, int dashed);
    void lane_tint(float x, float width, float end);
    void lane_divider(float x, float end);
    struct RoadLayout {
        lr_window w;
        float widths[LR_MAX_LANES], centers[LR_MAX_LANES], edges[LR_MAX_LANES + 1];
        float total, left, ax, angle, exit_lo, exit_hi, junction, secondary_width;
        int exit_lanes, exit_first, exit_last;
        road_join_t join;
        // All physical lanes share this geometry, including context outside the crop.
        float lane_width(int index) const {
            return index == w.anchor ? style::LaneWidth : secondary_width;
        }
        float lane_left(int index) const {
            return index <= w.anchor ? ax - style::LaneWidth * .5f + (index - w.anchor) * secondary_width
                                     : ax + style::LaneWidth * .5f + (index - w.anchor - 1) * secondary_width;
        }
        float lane_center(int index) const { return lane_left(index) + lane_width(index) * .5f; }
    };
    void prepare_road(const lr_state *s, lr_window window, int compact, float secondary_width,
                      RoadLayout &plan);
    void emit_road(const lr_state *s, const RoadLayout &plan);
    void compile_road(const lr_state *s, lr_window window, int compact, float secondary_width);
    void compile_unmarked(const maneuver_state_t *maneuver);
};
} // namespace navigation
#endif
