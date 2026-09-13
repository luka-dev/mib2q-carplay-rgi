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
enum class Op { Triangle, DirectionFade, BeginOutline, BeginRoute, EndMask };
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
    route_path_t path;
    route_mesh_t mesh;
    float camera_x, camera_y, route_elevation;
    int framing_ok;
    float frame_bounds[4];
    static constexpr int MaxKeys = RMESH_MAX_VERTS + 2048;
    float points[MaxKeys][3];
    int point_count;
    uint32_t scene_mesh_hash;
    float exit_fade_length;
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
    void triangle(float x, float y, float x1, float y1, float x2, float y2, Color color, float a) {
        const float v[] = {x, y, x1, y1, x2, y2, color.r, color.g, color.b, a};
        emit(Op::Triangle, v, 10);
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
    void begin_route() {
        emit(Op::BeginRoute, nullptr, 0);
    }
    void end_mask() {
        emit(Op::EndMask, nullptr, 0);
    }
    void reset(DisplayList *output, const float camera[16]) {
        std::memset(this, 0, sizeof(*this));
        list = output;
        exit_fade_length = EXIT_FADE_LENGTH;
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
    void exit_point(int i, float offset, float *x, float *y);
    void exit_fade_begin();
    void exit_fade_end();
    void exit_strip(float lo, float hi, Color color);
    void compile_road(const maneuver_state_t *maneuver);
};
} // namespace navigation
#endif
