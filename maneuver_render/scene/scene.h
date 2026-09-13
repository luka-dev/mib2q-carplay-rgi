#ifndef CR_SCENE_H
#define CR_SCENE_H
#include <stdint.h>
#include "../maneuver.h"
#include "lane_model.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Complete immutable inputs. generation identifies ownership, not geometry;
 * callers establish lane-event completeness before setting lanes.complete. */
typedef struct {
    maneuver_state_t maneuver;
    lr_state lanes;
    uint64_t generation;
    int previous_lane;
} cr_scene_input_t;

typedef struct {
    float projection[16]; /* settled, unpanned camera; no GL context required */
    int compact;
} cr_scene_view_t;

typedef enum { CR_SCENE_LANES, CR_SCENE_UNMARKED, CR_SCENE_NATIVE } cr_scene_kind_t;
typedef enum {
    CR_SCENE_FALLBACK_NONE,
    CR_SCENE_FALLBACK_GEOMETRY,
    CR_SCENE_FALLBACK_CAPACITY,
    CR_SCENE_FALLBACK_FRAMING
} cr_scene_fallback_t;

typedef struct {
    cr_scene_kind_t kind;
    float camera_x, camera_y, bounds[4], secondary_width;
    float glyph_clearance, head_min, head_max;
    int framed, visible_count, hidden_side, exit_lanes;
    uint32_t mesh_hash, glyph_hash[LR_MAX_LANES];
    unsigned glyph_seen;
    cr_lane_safety_result_t safety;
    cr_scene_fallback_t fallback;
    unsigned builds, paint_count, command_count;
} cr_scene_info_t;

typedef struct cr_scene cr_scene_t;
/* Allocation happens at scene creation, never in paint. A scene owns its
 * geometry; keep separate current/next instances throughout a transition. */
cr_scene_t *cr_scene_create(void);
void cr_scene_destroy(cr_scene_t *scene);
/* Apply the scene material/road extents; leaves the caller's callbacks intact. */
void cr_scene_configure_provider(maneuver_scene_provider_t *provider);
/* Render-thread API (route extrusion uses shared scratch). Returns 1 ready;
 * invalid maneuver/projection returns 0 and invalidates the old scene. A valid
 * maneuver whose lane geometry cannot fit uses the built-in family instead.
 * Identical semantic inputs reuse the scene. Returned pointers are borrowed
 * until the next prepare/destroy. Native framing covers the main route only. */
int cr_scene_prepare(cr_scene_t *scene, const cr_scene_input_t *input, const cr_scene_view_t *view);
const route_path_t *cr_scene_route(const cr_scene_t *scene);
const cr_scene_info_t *cr_scene_info(const cr_scene_t *scene);
int cr_scene_is_native(const cr_scene_t *scene);
/* Replay prepared pavement only; the native maneuver engine owns route/camera. */
void cr_scene_paint(cr_scene_t *scene, float tx, float ty, float cos_r, float sin_r);

#ifdef __cplusplus
}
#endif
#endif
