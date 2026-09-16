/*
 * Maneuver icon rendering for CarPlay cluster widget.
 *
 * Collapsed icon types -- one per unique visual icon.
 * BAP descriptor -> ICON_* mapping done in Java; legacy clients remain supported.
 *
 * junction_angles[] serves double duty:
 *   ICON_TURN / ICON_APPROACH: side street angles at the junction
 *   ICON_ROUNDABOUT: side road angles around the ring
 *
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */

#ifndef CR_MANEUVER_H
#define CR_MANEUVER_H

#include "route_path.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Renderer icon types */
#define ICON_NONE        0   /* no icon / not set */
#define ICON_APPROACH    1   /* approach junction; junction_angles = side streets */
#define ICON_TURN        2   /* exit_angle = turn degrees; junction_angles = side streets */
#define ICON_UTURN       3   /* direction from driving_side */
#define ICON_MERGE       4   /* legacy merge; direction: 1=right, -1=left */
#define ICON_EXIT        5   /* off-ramp exit; direction: 1=right, -1=left */
#define ICON_ROUNDABOUT  6   /* exit_angle + junction_angles */
#define ICON_ARRIVED     7   /* direction: -1=left, 0=center, 1=right */
#define ICON_LANE_CHANGE 8   /* offset path, without a highway continuation */
#define ICON_ROUNDABOUT_EXIT 9 /* departure stage: partial ring + exit */
#define ICON_COUNT       10

#define MAX_JUNCTION_ANGLES 18  /* max 18: payload[6..41], keeps [42..45] for progress/perspective */

/* Maneuver state decoded from the TCP command */
typedef struct {
    int icon;            /* ICON_* constant */
    float exit_angle;    /* signed degrees, including exact BAP half-degree bins */
    int direction;       /* -1=left, 0=center, 1=right (ICON_MERGE/LANE_CHANGE/ARRIVED) */
    int driving_side;    /* 0=RHT, 1=LHT */
    float junction_angles[MAX_JUNCTION_ANGLES]; /* signed degrees */
    int junction_angle_count;                 /* number of valid entries */
    int bap_geometry;    /* inhibit snap (BAP roads or missing/typed exit fallback) */
} maneuver_state_t;

/* Exit point of a maneuver path (for chaining). */
typedef struct {
    float x, y;       /* exit point in local maneuver coords */
    float heading;     /* exit heading in radians (math convention) */
} maneuver_exit_t;

/* Get the exit point and heading for a maneuver (pure computation). */
maneuver_exit_t maneuver_get_exit(const maneuver_state_t *state);

/* Build route path segments for a maneuver (no extend/densify/extrude).
 * Used for path chaining during transitions. */
void maneuver_build_route(const maneuver_state_t *state, route_path_t *path);
float maneuver_builtin_elevation(const maneuver_state_t *state);
/* Optional prebuilt scene source, using the SAME route/camera transition engine.
 * Paths and masks must share coordinates with settled camera at (0,0).
 * paint_masks draws only road/route masks, with the supplied rigid transform.
 * Snapshots must remain immutable throughout a push. NULL restores built-ins. */
typedef struct {
    void *context;
    void (*build_route)(void *context,const maneuver_state_t *state,route_path_t *path);
    void (*paint_masks)(void *context,const maneuver_state_t *state,
                        float tx,float ty,float cos_r,float sin_r);
    float route_base_y,route_top_y;
    float entry_road_reach,exit_road_reach;
    /* Optional per-state routing. A declined scene uses the full built-in
     * family, including side roads, roundabouts, arrival flag and elevation. */
    int (*handles)(void *context,const maneuver_state_t *state);
    /* Scene-specific overlap lift, shared by framing and the native mesh engine. */
    float (*elevation)(void *context,const maneuver_state_t *state);
} maneuver_scene_provider_t;
void maneuver_set_scene_provider(const maneuver_scene_provider_t *provider);
/* Conservative world-space bounds needed to render any two-maneuver transition masks. */
void maneuver_get_transition_mask_bounds(float *out_abs_x, float *out_abs_y);

/* Draw the maneuver icon for the given state.
 * next_state: if non-NULL and pushing, builds combined path for seamless transition. */
void maneuver_prepare_frame(const maneuver_state_t *state, const maneuver_state_t *next_state);
void maneuver_draw(const maneuver_state_t *state, const maneuver_state_t *next_state);

/* Route path animation control. */
void maneuver_start_anim(void);       /* reset slide=0, start auto-animation */
int  maneuver_is_animating(void);     /* 1 while transition animation running (for engine state) */
int  maneuver_needs_redraw(void);    /* 1 while any animation needs continuous rendering */
int  maneuver_is_presentationally_settled(void); /* flag may still loop after ARRIVED settles */
void maneuver_set_slide(float t);     /* set slide manually (stops auto-anim) */
float maneuver_get_slide(void);       /* get current slide value */

/* Push-out transition: slide the blue path forward through the exit.
 * On completion, maneuver_is_pushing() returns 0 -- caller should then
 * switch to new maneuver state and call maneuver_start_anim(). */
void maneuver_start_push(void);       /* begin push-out (slide 1->2) */
int  maneuver_is_pushing(void);       /* 1 while push-out running */
void maneuver_commit_pushed_state(const maneuver_state_t *state);

/* Debug overlay toggle. */
void maneuver_toggle_debug(void);     /* toggle path debug overlay */
int  maneuver_is_debug(void);         /* 1 if debug overlay active */

/* Get human-readable name for an icon type (for debug overlay). */
const char *maneuver_icon_name(int icon);

#ifdef __cplusplus
}
#endif

#endif /* CR_MANEUVER_H */
