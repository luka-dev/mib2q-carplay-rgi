/*
 * Maneuver icon rendering for CarPlay cluster widget.
 *
 * Maps iOS maneuver types to procedural GL icons.
 * Colors: #5AAAE6 (active arrow), #646464 (side roads).
 */

#ifndef CR_MANEUVER_H
#define CR_MANEUVER_H

/* iOS ManeuverType values (from iAP2 RouteGuidanceManeuverUpdate) */
#define MT_NOT_SET                     255
#define MT_NO_TURN                     0
#define MT_LEFT_TURN                   1
#define MT_RIGHT_TURN                  2
#define MT_STRAIGHT_AHEAD              3
#define MT_U_TURN                      4
#define MT_FOLLOW_ROAD                 5
#define MT_ENTER_ROUNDABOUT            6
#define MT_EXIT_ROUNDABOUT             7
#define MT_OFF_RAMP                    8
#define MT_ON_RAMP                     9
#define MT_ARRIVE_END_OF_NAVIGATION    10
#define MT_START_ROUTE                 11
#define MT_ARRIVE_AT_DESTINATION       12
#define MT_KEEP_LEFT                   13
#define MT_KEEP_RIGHT                  14
#define MT_ENTER_FERRY                 15
#define MT_EXIT_FERRY                  16
#define MT_CHANGE_FERRY                17
#define MT_START_ROUTE_WITH_U_TURN     18
#define MT_U_TURN_AT_ROUNDABOUT        19
#define MT_LEFT_TURN_AT_END            20
#define MT_RIGHT_TURN_AT_END           21
#define MT_HIGHWAY_OFF_RAMP_LEFT       22
#define MT_HIGHWAY_OFF_RAMP_RIGHT      23
#define MT_ARRIVE_DESTINATION_LEFT     24
#define MT_ARRIVE_DESTINATION_RIGHT    25
#define MT_U_TURN_WHEN_POSSIBLE        26
#define MT_ARRIVE_END_OF_DIRECTIONS    27
#define MT_ROUNDABOUT_EXIT_1           28
#define MT_ROUNDABOUT_EXIT_2           29
#define MT_ROUNDABOUT_EXIT_3           30
#define MT_ROUNDABOUT_EXIT_4           31
#define MT_ROUNDABOUT_EXIT_5           32
#define MT_ROUNDABOUT_EXIT_6           33
#define MT_ROUNDABOUT_EXIT_7           34
#define MT_ROUNDABOUT_EXIT_8           35
#define MT_ROUNDABOUT_EXIT_9           36
#define MT_ROUNDABOUT_EXIT_10          37
#define MT_ROUNDABOUT_EXIT_11          38
#define MT_ROUNDABOUT_EXIT_12          39
#define MT_ROUNDABOUT_EXIT_13          40
#define MT_ROUNDABOUT_EXIT_14          41
#define MT_ROUNDABOUT_EXIT_15          42
#define MT_ROUNDABOUT_EXIT_16          43
#define MT_ROUNDABOUT_EXIT_17          44
#define MT_ROUNDABOUT_EXIT_18          45
#define MT_ROUNDABOUT_EXIT_19          46
#define MT_SHARP_LEFT_TURN             47
#define MT_SHARP_RIGHT_TURN            48
#define MT_SLIGHT_LEFT_TURN            49
#define MT_SLIGHT_RIGHT_TURN           50
#define MT_CHANGE_HIGHWAY              51
#define MT_CHANGE_HIGHWAY_LEFT         52
#define MT_CHANGE_HIGHWAY_RIGHT        53

#define MT_COUNT                        54

/* Maneuver state (from PPS / UDP) */
typedef struct {
    int maneuver_type;   /* MT_* constant (0-53, 255=not set) */
    int exit_angle;      /* signed degrees for roundabout exits */
    int junction_type;   /* 0=intersection, 1=roundabout */
    int driving_side;    /* 0=RHT, 1=LHT */
} maneuver_state_t;

/* Draw the maneuver icon for the given state. */
void maneuver_draw(const maneuver_state_t *state);

/* Get human-readable name for a maneuver type (for debug overlay). */
const char *maneuver_type_name(int type);

#endif /* CR_MANEUVER_H */
