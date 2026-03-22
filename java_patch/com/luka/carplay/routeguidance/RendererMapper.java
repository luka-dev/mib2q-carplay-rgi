/*
 * RendererMapper - Maps iAP2 ManeuverType to c_render ICON_* + direction/exitAngle.
 *
 * ICON constants match c_render/maneuver.h exactly.
 * Direction: -1=left, 0=center, +1=right (matches maneuver_state_t.direction).
 * Exit angle: signed degrees, positive=right, negative=left.
 *
 * Java 1.2 compatible (no generics, no autoboxing, no enhanced for).
 */
package com.luka.carplay.routeguidance;

public class RendererMapper {

    /* ICON_* constants -- must match c_render/maneuver.h */
    public static final int ICON_NONE        = 0;
    public static final int ICON_APPROACH    = 1;
    public static final int ICON_TURN        = 2;
    public static final int ICON_UTURN       = 3;
    public static final int ICON_MERGE       = 4;
    public static final int ICON_LANE_CHANGE = 5;
    public static final int ICON_ROUNDABOUT  = 6;
    public static final int ICON_ARRIVED     = 7;

    /**
     * Map iAP2 ManeuverType to c_render ICON_* constant.
     */
    public static int mapIcon(int mt) {
        switch (mt) {
            /* Approach / follow road */
            case ManeuverMapper.MT_NO_TURN:
            case ManeuverMapper.MT_FOLLOW_ROAD:
            case ManeuverMapper.MT_STRAIGHT_AHEAD:
            case ManeuverMapper.MT_START_ROUTE:
            case ManeuverMapper.MT_ENTER_FERRY:
            case ManeuverMapper.MT_EXIT_FERRY:
            case ManeuverMapper.MT_CHANGE_FERRY:
            case ManeuverMapper.MT_CHANGE_HIGHWAY:
                return ICON_APPROACH;

            /* Turn (exit angle from mTurnAngle) */
            case ManeuverMapper.MT_LEFT_TURN:
            case ManeuverMapper.MT_RIGHT_TURN:
            case ManeuverMapper.MT_SLIGHT_LEFT_TURN:
            case ManeuverMapper.MT_SLIGHT_RIGHT_TURN:
            case ManeuverMapper.MT_SHARP_LEFT_TURN:
            case ManeuverMapper.MT_SHARP_RIGHT_TURN:
            case ManeuverMapper.MT_KEEP_LEFT:
            case ManeuverMapper.MT_KEEP_RIGHT:
            case ManeuverMapper.MT_LEFT_TURN_AT_END:
            case ManeuverMapper.MT_RIGHT_TURN_AT_END:
            case ManeuverMapper.MT_OFF_RAMP:
            case ManeuverMapper.MT_HIGHWAY_OFF_RAMP_LEFT:
            case ManeuverMapper.MT_HIGHWAY_OFF_RAMP_RIGHT:
                return ICON_TURN;

            /* U-turn */
            case ManeuverMapper.MT_U_TURN:
            case ManeuverMapper.MT_START_ROUTE_WITH_U_TURN:
            case ManeuverMapper.MT_U_TURN_WHEN_POSSIBLE:
                return ICON_UTURN;

            /* On-ramp / merge */
            case ManeuverMapper.MT_ON_RAMP:
                return ICON_MERGE;

            /* Highway lane change */
            case ManeuverMapper.MT_CHANGE_HIGHWAY_LEFT:
            case ManeuverMapper.MT_CHANGE_HIGHWAY_RIGHT:
                return ICON_LANE_CHANGE;

            /* Roundabout */
            case ManeuverMapper.MT_ENTER_ROUNDABOUT:
            case ManeuverMapper.MT_EXIT_ROUNDABOUT:
            case ManeuverMapper.MT_U_TURN_AT_ROUNDABOUT:
            case ManeuverMapper.MT_ROUNDABOUT_EXIT_1:
            case ManeuverMapper.MT_ROUNDABOUT_EXIT_2:
            case ManeuverMapper.MT_ROUNDABOUT_EXIT_3:
            case ManeuverMapper.MT_ROUNDABOUT_EXIT_4:
            case ManeuverMapper.MT_ROUNDABOUT_EXIT_5:
            case ManeuverMapper.MT_ROUNDABOUT_EXIT_6:
            case ManeuverMapper.MT_ROUNDABOUT_EXIT_7:
            case ManeuverMapper.MT_ROUNDABOUT_EXIT_8:
            case ManeuverMapper.MT_ROUNDABOUT_EXIT_9:
            case ManeuverMapper.MT_ROUNDABOUT_EXIT_10:
            case ManeuverMapper.MT_ROUNDABOUT_EXIT_11:
            case ManeuverMapper.MT_ROUNDABOUT_EXIT_12:
            case ManeuverMapper.MT_ROUNDABOUT_EXIT_13:
            case ManeuverMapper.MT_ROUNDABOUT_EXIT_14:
            case ManeuverMapper.MT_ROUNDABOUT_EXIT_15:
            case ManeuverMapper.MT_ROUNDABOUT_EXIT_16:
            case ManeuverMapper.MT_ROUNDABOUT_EXIT_17:
            case ManeuverMapper.MT_ROUNDABOUT_EXIT_18:
            case ManeuverMapper.MT_ROUNDABOUT_EXIT_19:
                return ICON_ROUNDABOUT;

            /* Arrived */
            case ManeuverMapper.MT_ARRIVE_END_OF_NAVIGATION:
            case ManeuverMapper.MT_ARRIVE_AT_DESTINATION:
            case ManeuverMapper.MT_ARRIVE_END_OF_DIRECTIONS:
            case ManeuverMapper.MT_ARRIVE_DESTINATION_LEFT:
            case ManeuverMapper.MT_ARRIVE_DESTINATION_RIGHT:
                return ICON_ARRIVED;

            default:
                return ICON_NONE;
        }
    }

    /**
     * Map iAP2 ManeuverType to c_render direction (-1=left, 0=center, +1=right).
     * For ICON_TURN the direction is implicit from exit_angle; this returns 0.
     * For ICON_MERGE/LANE_CHANGE/ARRIVED, direction encodes left/right.
     */
    public static int mapDirection(int mt, int turnAngle, int drivingSide) {
        switch (mt) {
            /* Merge: direction from driving side */
            case ManeuverMapper.MT_ON_RAMP:
                return (drivingSide == ManeuverMapper.DRIVING_SIDE_LEFT) ? -1 : 1;

            /* Lane change */
            case ManeuverMapper.MT_CHANGE_HIGHWAY_LEFT:
                return -1;
            case ManeuverMapper.MT_CHANGE_HIGHWAY_RIGHT:
                return 1;

            /* Arrived with side */
            case ManeuverMapper.MT_ARRIVE_DESTINATION_LEFT:
                return -1;
            case ManeuverMapper.MT_ARRIVE_DESTINATION_RIGHT:
                return 1;

            /* Off-ramp: direction hint for rendering */
            case ManeuverMapper.MT_HIGHWAY_OFF_RAMP_LEFT:
                return -1;
            case ManeuverMapper.MT_HIGHWAY_OFF_RAMP_RIGHT:
                return 1;
            case ManeuverMapper.MT_OFF_RAMP:
                return (drivingSide == ManeuverMapper.DRIVING_SIDE_LEFT) ? -1 : 1;

            default:
                return 0;
        }
    }

    /**
     * Get exit angle for c_render. For ICON_TURN this is the raw turnAngle.
     * For off-ramps without a real angle, use a default slight angle.
     * For roundabouts, the turnAngle from iOS is the exit angle.
     */
    /** Sentinel value: iOS sends 1000 when turn angle is unknown. */
    private static final int ANGLE_UNKNOWN = 1000;

    /** Default angles for maneuver types when iOS doesn't provide one. */
    private static int defaultAngle(int mt) {
        switch (mt) {
            case ManeuverMapper.MT_LEFT_TURN:
            case ManeuverMapper.MT_LEFT_TURN_AT_END:
                return -90;
            case ManeuverMapper.MT_RIGHT_TURN:
            case ManeuverMapper.MT_RIGHT_TURN_AT_END:
                return 90;
            case ManeuverMapper.MT_SLIGHT_LEFT_TURN:
            case ManeuverMapper.MT_KEEP_LEFT:
                return -45;
            case ManeuverMapper.MT_SLIGHT_RIGHT_TURN:
            case ManeuverMapper.MT_KEEP_RIGHT:
                return 45;
            case ManeuverMapper.MT_SHARP_LEFT_TURN:
                return -135;
            case ManeuverMapper.MT_SHARP_RIGHT_TURN:
                return 135;
            case ManeuverMapper.MT_U_TURN:
            case ManeuverMapper.MT_START_ROUTE_WITH_U_TURN:
            case ManeuverMapper.MT_U_TURN_WHEN_POSSIBLE:
                return -180;
            default:
                return 0;
        }
    }

    public static int mapExitAngle(int mt, int turnAngle) {
        /* iOS sentinel 1000 = no angle data — use default for the maneuver type */
        if (turnAngle == ANGLE_UNKNOWN || turnAngle == -ANGLE_UNKNOWN) {
            turnAngle = defaultAngle(mt);
        }
        switch (mt) {
            case ManeuverMapper.MT_OFF_RAMP:
                return (turnAngle != 0) ? turnAngle : 30;
            case ManeuverMapper.MT_HIGHWAY_OFF_RAMP_LEFT:
                return (turnAngle != 0) ? turnAngle : -30;
            case ManeuverMapper.MT_HIGHWAY_OFF_RAMP_RIGHT:
                return (turnAngle != 0) ? turnAngle : 30;
            default:
                return turnAngle;
        }
    }
}
