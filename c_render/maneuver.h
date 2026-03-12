/*
 * Maneuver icon rendering for CarPlay cluster widget.
 *
 * Collapsed icon types — one per unique visual icon.
 * iOS ManeuverType → ICON_* mapping done externally.
 */

#ifndef CR_MANEUVER_H
#define CR_MANEUVER_H

/* Renderer icon types */
#define ICON_NONE        0   /* no icon / not set */
#define ICON_STRAIGHT    1   /* straight ahead, follow road, ferry, etc. */
#define ICON_TURN        2   /* exit_angle: +right, -left (degrees) */
#define ICON_UTURN       3   /* direction from driving_side */
#define ICON_EXIT        4   /* highway off-ramp; direction: 1=right, -1=left */
#define ICON_MERGE       5   /* on-ramp; direction: 1=right, -1=left */
#define ICON_LANE_CHANGE 6   /* highway lane change; direction: 1=right, -1=left */
#define ICON_ROUNDABOUT  7   /* exit_angle + junction_angles */
#define ICON_ARRIVED     8   /* direction: -1=left, 0=center, 1=right */
#define ICON_COUNT       9

#define MAX_JUNCTION_ANGLES 20

/* Maneuver state (from PPS / UDP) */
typedef struct {
    int icon;            /* ICON_* constant */
    int exit_angle;      /* turn angle (ICON_TURN) or roundabout exit (ICON_ROUNDABOUT), signed degrees */
    int direction;       /* -1=left, 0=center, 1=right (ICON_EXIT/MERGE/LANE_CHANGE/ARRIVED) */
    int driving_side;    /* 0=RHT, 1=LHT */
    int junction_angles[MAX_JUNCTION_ANGLES]; /* iOS JunctionElementAngle degrees */
    int junction_angle_count;                 /* number of valid entries */
} maneuver_state_t;

/* Draw the maneuver icon for the given state. */
void maneuver_draw(const maneuver_state_t *state);

/* Get human-readable name for an icon type (for debug overlay). */
const char *maneuver_icon_name(int icon);

#endif /* CR_MANEUVER_H */
