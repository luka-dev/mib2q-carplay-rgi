/*
 * Maneuver icon rendering for CarPlay cluster widget.
 *
 * Collapsed icon types -- one per unique visual icon.
 * iOS ManeuverType -> ICON_* mapping done externally.
 *
 * junction_angles[] serves double duty:
 *   ICON_TURN / ICON_APPROACH: side street angles at the junction
 *   ICON_ROUNDABOUT: all road angles around the ring
 */

#ifndef CR_MANEUVER_H
#define CR_MANEUVER_H

#include "route_path.h"

/* Renderer icon types */
#define ICON_NONE        0   /* no icon / not set */
#define ICON_APPROACH    1   /* approach junction; junction_angles = side streets */
#define ICON_TURN        2   /* exit_angle = turn degrees; junction_angles = side streets */
#define ICON_UTURN       3   /* direction from driving_side */
#define ICON_MERGE       4   /* on-ramp; direction: 1=right, -1=left */
#define ICON_EXIT        5   /* off-ramp exit; direction: 1=right, -1=left */
#define ICON_ROUNDABOUT  6   /* exit_angle + junction_angles */
#define ICON_ARRIVED     7   /* direction: -1=left, 0=center, 1=right */
#define ICON_COUNT       8

#define MAX_JUNCTION_ANGLES 18  /* max 18: payload[6..41], keeps [42..45] for persp/bargraph */

/* Maneuver state (from PPS / UDP) */
typedef struct {
    int icon;            /* ICON_* constant */
    int exit_angle;      /* turn angle (ICON_TURN) or roundabout exit (ICON_ROUNDABOUT), signed degrees */
    int direction;       /* -1=left, 0=center, 1=right (ICON_MERGE/LANE_CHANGE/ARRIVED) */
    int driving_side;    /* 0=RHT, 1=LHT */
    int junction_angles[MAX_JUNCTION_ANGLES]; /* side streets (TURN/STRAIGHT) or ring roads (ROUNDABOUT) */
    int junction_angle_count;                 /* number of valid entries */
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

#endif /* CR_MANEUVER_H */
