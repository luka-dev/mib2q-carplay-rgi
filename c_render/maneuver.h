/*
 * Maneuver icon rendering for CarPlay cluster widget.
 *
 * Collapsed icon types — one per unique visual icon.
 * iOS ManeuverType → ICON_* mapping done externally.
 *
 * junction_angles[] serves double duty:
 *   ICON_TURN / ICON_STRAIGHT: side street angles at the junction
 *   ICON_ROUNDABOUT: all road angles around the ring
 */

#ifndef CR_MANEUVER_H
#define CR_MANEUVER_H

/* Renderer icon types */
#define ICON_NONE        0   /* no icon / not set */
#define ICON_STRAIGHT    1   /* straight ahead; junction_angles = side streets */
#define ICON_TURN        2   /* exit_angle = turn degrees; junction_angles = side streets */
#define ICON_UTURN       3   /* direction from driving_side */
#define ICON_MERGE       4   /* on-ramp; direction: 1=right, -1=left */
#define ICON_LANE_CHANGE 5   /* highway lane change; direction: 1=right, -1=left */
#define ICON_ROUNDABOUT  6   /* exit_angle + junction_angles */
#define ICON_ARRIVED     7   /* direction: -1=left, 0=center, 1=right */
#define ICON_COUNT       8

#define MAX_JUNCTION_ANGLES 20

/* Maneuver state (from PPS / UDP) */
typedef struct {
    int icon;            /* ICON_* constant */
    int exit_angle;      /* turn angle (ICON_TURN) or roundabout exit (ICON_ROUNDABOUT), signed degrees */
    int direction;       /* -1=left, 0=center, 1=right (ICON_MERGE/LANE_CHANGE/ARRIVED) */
    int driving_side;    /* 0=RHT, 1=LHT */
    int junction_angles[MAX_JUNCTION_ANGLES]; /* side streets (TURN/STRAIGHT) or ring roads (ROUNDABOUT) */
    int junction_angle_count;                 /* number of valid entries */
} maneuver_state_t;

/* Draw the maneuver icon for the given state. */
void maneuver_draw(const maneuver_state_t *state);

/* Route path animation control. */
void maneuver_start_anim(void);       /* reset slide=0, start auto-animation */
int  maneuver_is_animating(void);     /* 1 while auto-animation running */
void maneuver_set_slide(float t);     /* set slide manually (stops auto-anim) */
float maneuver_get_slide(void);       /* get current slide value */

/* Push-out transition: slide the blue path forward through the exit.
 * On completion, maneuver_is_pushing() returns 0 — caller should then
 * switch to new maneuver state and call maneuver_start_anim(). */
void maneuver_start_push(void);       /* begin push-out (slide 1→2) */
int  maneuver_is_pushing(void);       /* 1 while push-out running */

/* Debug overlay toggle. */
void maneuver_toggle_debug(void);     /* toggle path debug overlay */
int  maneuver_is_debug(void);         /* 1 if debug overlay active */

/* Get human-readable name for an icon type (for debug overlay). */
const char *maneuver_icon_name(int icon);

#endif /* CR_MANEUVER_H */
