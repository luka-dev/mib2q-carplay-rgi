/* Scene API invariants; real geometry, no window or GL context. */
#include "../../maneuver_render/scene/scene_internal.hpp"
#include "../../maneuver_render/render.h"
#include "fixtures.h"
#include <cassert>
#include <limits>
#include <ctime>
extern "C" float g_3d_offset_adjust;

static cr_scene_input_t input_for(int c) {
    cr_scene_input_t in = {};
    in.maneuver = sample_maneuver(c);
    sample(&in.lanes, c);
    in.generation = static_cast<uint64_t>(c) + 1;
    in.previous_lane = -1;
    return in;
}
static void same_route(const route_path_t *a, const route_path_t *b) {
    assert(a && b && a->seg_count == b->seg_count && a->pt_count == b->pt_count);
    assert(std::memcmp(a, b, sizeof(*a)) == 0);
}
int main() {
    g_3d_offset_adjust = -.16f;
    cr_scene_view_t small = {}, wide = {};
    small.compact = 1;
    render_build_layout_matrix(small.projection, 328.f / 181);
    wide = small;
    wide.compact = 0;
    cr_scene_t *a = cr_scene_create(), *b = cr_scene_create();
    assert(a && b);
    unsigned max_commands = 0;
    clock_t start = clock();
    for (int c = 0; c < CASES; ++c) {
        cr_scene_input_t in = input_for(c), original = in;
        assert(cr_scene_prepare(a, &in, &small));
        cr_scene_info_t info = *cr_scene_info(a);
        route_path_t owned = *cr_scene_route(a);
        assert(info.framed && !info.fallback);
        if (info.command_count > max_commands)
            max_commands = info.command_count;
        for (int repeat = 0; repeat < 1000; ++repeat)
            assert(cr_scene_prepare(a, &in, &small));
        assert(cr_scene_info(a)->builds == info.builds);
        assert(std::memcmp(&in, &original, sizeof(in)) == 0);
        /* Caller storage is not retained. Current and next never alias. */
        std::memset(&in, 0x55, sizeof(in));
        same_route(&owned, cr_scene_route(a));
        assert(cr_scene_prepare(b, &original, &wide));
        const cr_scene_info_t *other = cr_scene_info(b);
        assert(info.camera_x == other->camera_x && info.camera_y == other->camera_y);
        assert(info.secondary_width == other->secondary_width && info.mesh_hash == other->mesh_hash);
        same_route(cr_scene_route(a), cr_scene_route(b));
        if (info.kind == CR_SCENE_NATIVE) {
            route_path_t native = {};
            maneuver_build_route(&original.maneuver, &native);
            rpath_densify(&native);
            same_route(&native, cr_scene_route(a));
            assert(!info.command_count);
        }
    }
    double elapsed = static_cast<double>(clock() - start) / CLOCKS_PER_SEC;
    cr_scene_input_t in = input_for(0);
    assert(cr_scene_prepare(a, &in, &small));
    unsigned builds = cr_scene_info(a)->builds;
    /* Unused protocol array tails are not geometry, padding is not a key. */
    in.maneuver.junction_angles[MAX_JUNCTION_ANGLES - 1] = 90;
    in.lanes.lane[0].angles[LR_MAX_ANGLES - 1] = 90;
    in.lanes.lane[LR_MAX_LANES - 1].status = 2;
    assert(cr_scene_prepare(a, &in, &small) && cr_scene_info(a)->builds == builds);
    ++in.generation;
    assert(cr_scene_prepare(a, &in, &small));
    assert(cr_scene_info(a)->builds == ++builds);
    in.previous_lane = 2;
    assert(cr_scene_prepare(a, &in, &small));
    assert(cr_scene_info(a)->builds == ++builds);
    /* Corrupt counts/status cannot turn a truncated snapshot into a road. */
    const int counts[] = {-1, 9, 2147483647};
    for (unsigned i = 0; i < sizeof(counts) / sizeof(*counts); ++i) {
        in = input_for(0);
        in.lanes.count = counts[i];
        assert(cr_scene_prepare(a, &in, &small));
        assert(cr_scene_info(a)->kind != CR_SCENE_LANES);
        in = input_for(0);
        in.lanes.lane[0].count = counts[i] + (i == 2 ? 0 : 16);
        assert(cr_scene_prepare(a, &in, &small));
        if (in.lanes.lane[0].count > 16)
            assert(cr_scene_info(a)->kind != CR_SCENE_LANES);
    }
    in = input_for(0);
    in.lanes.lane[0].status = 99;
    assert(cr_scene_prepare(a, &in, &small) && cr_scene_info(a)->kind != CR_SCENE_LANES);
    /* Unknown directions remain unknown, especially in the exit corridor. */
    for (int sentinel = -1000; sentinel <= 1000; sentinel += 2000) {
        in = input_for(5);
        in.lanes.lane[4].primary = sentinel;
        in.lanes.lane[4].count = 0;
        assert(cr_scene_prepare(a, &in, &small) && cr_scene_info(a)->kind == CR_SCENE_UNMARKED);
        assert(cr_scene_info(a)->safety == CR_LANE_ROAD_UNCONFIRMED_EXIT);
    }
    /* Invalid calls invalidate the old route; consumers cannot paint stale data. */
    in = input_for(0);
    in.maneuver.exit_angle = std::numeric_limits<float>::quiet_NaN();
    assert(!cr_scene_prepare(a, &in, &small) && !cr_scene_route(a));
    in = input_for(0);
    in.maneuver.junction_angle_count = MAX_JUNCTION_ANGLES + 1;
    assert(!cr_scene_prepare(a, &in, &small) && !cr_scene_route(a));
    in = input_for(0);
    cr_scene_view_t invalid = {};
    assert(!cr_scene_prepare(a, &in, &invalid) && !cr_scene_route(a));
    assert(cr_scene_prepare(a, &in, &small));
    assert(!cr_scene_prepare(a, nullptr, &small) && !cr_scene_route(a));
    /* A valid, unusably tight projection falls back to the independent family. */
    cr_scene_view_t tight = small;
    tight.projection[0] *= 5;
    assert(cr_scene_prepare(a, &in, &tight));
    assert(cr_scene_is_native(a) && cr_scene_info(a)->fallback != CR_SCENE_FALLBACK_NONE);
    route_path_t native = {};
    maneuver_build_route(&in.maneuver, &native);
    rpath_densify(&native);
    same_route(&native, cr_scene_route(a));
    /* The retained command buffer cannot overrun its fixed allocation. */
    navigation::DisplayList *list = new navigation::DisplayList();
    for (unsigned i = 0; i < navigation::MaxCommands + 100; ++i)
        list->push(navigation::Op::EndMask, nullptr, 0);
    assert(list->overflow && list->count == navigation::MaxCommands);
    /* Neither viewport may cut the retained road. Both stages must emit the
     * same complete paint list, including the third turning lane and its curb,
     * so camera travel cannot expose a baked-in rectangular corner. */
    navigation::DisplayList *wide_list = new navigation::DisplayList();
    navigation::Builder *wide_builder = new navigation::Builder();
    navigation::Builder *builder = new navigation::Builder();
    for (int side = -1; side <= 1; side += 2) {
        in = input_for(9);
        if (side < 0) {
            lr_state mirrored = in.lanes;
            for (int i = 0; i < in.lanes.count; ++i) {
                in.lanes.lane[i] = mirrored.lane[in.lanes.count - 1 - i];
                in.lanes.lane[i].position = i;
                in.lanes.lane[i].primary *= -1;
                for (int j = 0; j < in.lanes.lane[i].count; ++j)
                    in.lanes.lane[i].angles[j] *= -1;
            }
        }
        std::memset(list, 0, sizeof(*list));
        std::memset(wide_list, 0, sizeof(*wide_list));
        builder->reset(list, small.projection);
        builder->compile_road(&in.lanes, lr_choose(&in.lanes, 3, -1), 1, navigation::style::LaneWidth);
        wide_builder->reset(wide_list, wide.projection);
        wide_builder->compile_road(&in.lanes, lr_choose(&in.lanes, 3, -1), 0, navigation::style::LaneWidth);
        assert(!builder->failed && !list->overflow && builder->framing_ok);
        assert(builder->hidden_side == side);
        assert(!wide_builder->failed && !wide_list->overflow && wide_builder->framing_ok);
        assert(list->count == wide_list->count);
        assert(std::memcmp(list->commands, wide_list->commands,
                           list->count * sizeof(navigation::Command)) == 0);
        int painted_bend = 0;
        for (unsigned i = 0; i < list->count; ++i) {
            const navigation::Command &cmd = list->commands[i];
            const float *v = cmd.v;
            if (cmd.op == navigation::Op::Triangle && v[9] == 1 &&
                v[6] == navigation::style::RecommendedPavement.r &&
                v[7] == navigation::style::RecommendedPavement.g &&
                v[8] == navigation::style::RecommendedPavement.b &&
                v[1] > builder->path.segs[0].y1 + .02f)
                ++painted_bend;
        }
        assert(painted_bend > 0);
        list->count = 0;
        list->overflow = false;
        builder->exit_strip(side * .65f, side * .65f + .022f, navigation::style::Shoulder, 0);
        assert(list->count > 0); // Full curb retained beyond the framing window.
    }
    delete builder;
    delete wide_builder;
    delete wide_list;
    delete list;
    cr_scene_destroy(a);
    cr_scene_destroy(b);
    cr_scene_destroy(nullptr);
    std::printf("Scene compiler: %d fixtures, 33000 cache hits, immutable current/next, C ABI, native "
                "parity, invalid input/fallback PASS\n",
                CASES);
    std::printf("CPU fixture sweep %.3fs; peak commands %u/%u; compiler scratch %zu B, display list %zu B "
                "(per scene)\n",
                elapsed, max_commands, navigation::MaxCommands, sizeof(navigation::Builder),
                sizeof(navigation::DisplayList));
}
