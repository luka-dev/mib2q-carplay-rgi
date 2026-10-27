/*
 * Android Auto next-turn event -> iAP2/Apple EManeuverType + junction geometry, i.e. the
 * per-maneuver fields luka's RouteGuidance/ManeuverMapper consume (m<i>_type, m<i>_turn_angle,
 * m<i>_junction_type, m<i>_driving_side).  Pure Java 1.4, no Audi classes (host-testable).
 *
 * Android Auto values (org.dsi.ifc.androidauto2.Constants, DSI 2_61):
 *   turnSide 1=left 2=right 3=unspecified (0 is also seen for "depart")
 *   event 1 DEPART, 2 NAME_CHANGE, 3 SLIGHT_TURN, 4 TURN, 5 SHARP_TURN, 6 U_TURN, 7 ON_RAMP,
 *         8 OFF_RAMP, 9 FORK, 10 MERGE, 11 ROUNDABOUT_ENTER, 12 ROUNDABOUT_EXIT,
 *         13 ROUNDABOUT_ENTER_AND_EXIT, 14 STRAIGHT, 16/17 FERRY boat/train, 19 DESTINATION
 *
 * Angle convention on the luka side: signed degrees, negative = left, positive = right,
 * 0 = straight, +/-180 = U-turn (Apple JunctionElementExitAngle, see ManeuverMapper).
 * Plain turns carry a representative angle for the renderer's geometry (same values as
 * luka's tests/ManeuverChainAudit.exampleAngle); the BAP icon comes from the type.
 */
package com.sq5.aa.luka;

public final class AaManeuverMap {
    /* Android Auto turn side */
    public static final int SIDE_LEFT = 1;
    public static final int SIDE_RIGHT = 2;
    public static final int SIDE_UNSPECIFIED = 3;

    /* Android Auto turn events */
    public static final int EV_UNKNOWN = 0;
    public static final int EV_DEPART = 1;
    public static final int EV_NAME_CHANGE = 2;
    public static final int EV_SLIGHT_TURN = 3;
    public static final int EV_TURN = 4;
    public static final int EV_SHARP_TURN = 5;
    public static final int EV_U_TURN = 6;
    public static final int EV_ON_RAMP = 7;
    public static final int EV_OFF_RAMP = 8;
    public static final int EV_FORK = 9;
    public static final int EV_MERGE = 10;
    public static final int EV_ROUNDABOUT_ENTER = 11;
    public static final int EV_ROUNDABOUT_EXIT = 12;
    public static final int EV_ROUNDABOUT_ENTER_AND_EXIT = 13;
    public static final int EV_STRAIGHT = 14;
    public static final int EV_FERRY_BOAT = 16;
    public static final int EV_FERRY_TRAIN = 17;
    public static final int EV_DESTINATION = 19;

    /* Apple accNav EManeuverType (identical to com.luka.carplay.rgd.ManeuverMapper.MT_*) */
    public static final int MT_LEFT_TURN = 1;
    public static final int MT_RIGHT_TURN = 2;
    public static final int MT_STRAIGHT_AHEAD = 3;
    public static final int MT_U_TURN = 4;
    public static final int MT_FOLLOW_ROAD = 5;
    public static final int MT_ENTER_ROUNDABOUT = 6;
    public static final int MT_EXIT_ROUNDABOUT = 7;
    public static final int MT_OFF_RAMP = 8;
    public static final int MT_ON_RAMP = 9;
    public static final int MT_START_ROUTE = 11;
    public static final int MT_ARRIVE_AT_DESTINATION = 12;
    public static final int MT_KEEP_LEFT = 13;
    public static final int MT_KEEP_RIGHT = 14;
    public static final int MT_ENTER_FERRY = 15;
    public static final int MT_HIGHWAY_OFF_RAMP_LEFT = 22;
    public static final int MT_HIGHWAY_OFF_RAMP_RIGHT = 23;
    public static final int MT_ARRIVE_DESTINATION_LEFT = 24;
    public static final int MT_ARRIVE_DESTINATION_RIGHT = 25;
    public static final int MT_ROUNDABOUT_EXIT_1 = 28;
    public static final int MT_ROUNDABOUT_EXIT_19 = 46;
    public static final int MT_SHARP_LEFT_TURN = 47;
    public static final int MT_SHARP_RIGHT_TURN = 48;
    public static final int MT_SLIGHT_LEFT_TURN = 49;
    public static final int MT_SLIGHT_RIGHT_TURN = 50;

    public static final int JUNCTION_INTERSECTION = 0;
    public static final int JUNCTION_ROUNDABOUT = 1;
    public static final int DRIVING_SIDE_RIGHT = 0;   /* right-hand traffic (US) */
    public static final int DRIVING_SIDE_LEFT = 1;

    /** One mapped maneuver. anglePresent=false means "omit m<i>_turn_angle". */
    public static final class Result {
        public int type;
        public boolean anglePresent;
        public int angle;
        public int junctionType;
        public int drivingSide;
        /** DEPART / NAME_CHANGE / UNKNOWN: shown only once a distance > 0 belongs to it. */
        public boolean soft;

        public String toString() {
            return "type=" + type + " angle=" + (anglePresent ? String.valueOf(angle) : "-")
                + " junction=" + junctionType + " side=" + drivingSide + (soft ? " soft" : "");
        }
    }

    private AaManeuverMap() {
    }

    private static Result r(int type, int angle, boolean anglePresent) {
        Result m = new Result();
        m.type = type;
        m.angle = angle;
        m.anglePresent = anglePresent;
        m.junctionType = JUNCTION_INTERSECTION;
        m.drivingSide = DRIVING_SIDE_RIGHT;
        return m;
    }

    private static Result sided(int turnSide, int leftType, int leftAngle, int rightType, int rightAngle,
                                int noneType) {
        if (turnSide == SIDE_LEFT) return r(leftType, leftAngle, true);
        if (turnSide == SIDE_RIGHT) return r(rightType, rightAngle, true);
        return r(noneType, 0, false);
    }

    /**
     * Android Auto roundabout exit angle -> signed exit angle.  AA gives the angle travelled
     * around the roundabout from the entry, 1..360 (Car App Library Maneuver.roundaboutExitAngle
     * convention: 90 = first quarter, 180 = straight across, 360 = back where you came from).
     * Right-hand traffic (counter-clockwise): 90 -> +90 (right), 180 -> 0, 270 -> -90 (left).
     * Left-hand traffic (clockwise): mirrored.  Out of range -> Integer.MIN_VALUE (absent).
     * ASSUMPTION: the drive logs contain no roundabout yet; the raw angle is logged.
     */
    public static int roundaboutExitAngle(int aaAngle, boolean leftHandTraffic) {
        if (aaAngle <= 0 || aaAngle > 360) return Integer.MIN_VALUE;
        return leftHandTraffic ? aaAngle - 180 : 180 - aaAngle;
    }

    public static Result map(int event, int turnSide, int turnAngle, int turnNumber) {
        Result m;
        switch (event) {
            case EV_DEPART:
                m = r(MT_START_ROUTE, 0, false);
                m.soft = true;
                return m;
            case EV_NAME_CHANGE:
                m = r(MT_FOLLOW_ROAD, 0, false);
                m.soft = true;
                return m;
            case EV_STRAIGHT:
                return r(MT_STRAIGHT_AHEAD, 0, true);
            case EV_SLIGHT_TURN:
                return sided(turnSide, MT_SLIGHT_LEFT_TURN, -45, MT_SLIGHT_RIGHT_TURN, 45, MT_STRAIGHT_AHEAD);
            case EV_TURN:
                return sided(turnSide, MT_LEFT_TURN, -90, MT_RIGHT_TURN, 90, MT_STRAIGHT_AHEAD);
            case EV_SHARP_TURN:
                return sided(turnSide, MT_SHARP_LEFT_TURN, -135, MT_SHARP_RIGHT_TURN, 135, MT_STRAIGHT_AHEAD);
            case EV_U_TURN:
                /* signed angle picks the side; absent -> ManeuverMapper's traffic-side fallback */
                return sided(turnSide, MT_U_TURN, -180, MT_U_TURN, 180, MT_U_TURN);
            case EV_ON_RAMP:
                /* ramps: only the sign matters for the BAP icon (TURN + slight L/R) */
                return sided(turnSide, MT_ON_RAMP, -30, MT_ON_RAMP, 30, MT_ON_RAMP);
            case EV_OFF_RAMP:
                return sided(turnSide, MT_HIGHWAY_OFF_RAMP_LEFT, -30, MT_HIGHWAY_OFF_RAMP_RIGHT, 30, MT_OFF_RAMP);
            case EV_FORK:
                return sided(turnSide, MT_KEEP_LEFT, -45, MT_KEEP_RIGHT, 45, MT_STRAIGHT_AHEAD);
            case EV_MERGE:
                /* Apple has no merge type; a merge is a single slight bend like a ramp */
                return sided(turnSide, MT_ON_RAMP, -30, MT_ON_RAMP, 30, MT_FOLLOW_ROAD);
            case EV_ROUNDABOUT_ENTER:
            case EV_ROUNDABOUT_EXIT:
            case EV_ROUNDABOUT_ENTER_AND_EXIT:
                return roundabout(event, turnSide, turnAngle, turnNumber);
            case EV_FERRY_BOAT:
            case EV_FERRY_TRAIN:
                return r(MT_ENTER_FERRY, 0, false);
            case EV_DESTINATION:
                if (turnSide == SIDE_LEFT) return r(MT_ARRIVE_DESTINATION_LEFT, 0, false);
                if (turnSide == SIDE_RIGHT) return r(MT_ARRIVE_DESTINATION_RIGHT, 0, false);
                return r(MT_ARRIVE_AT_DESTINATION, 0, false);
            default:
                m = r(MT_FOLLOW_ROAD, 0, false);
                m.soft = true;
                return m;
        }
    }

    private static Result roundabout(int event, int turnSide, int turnAngle, int turnNumber) {
        /* AA turnSide on a roundabout = direction of travel: LEFT = clockwise (left-hand traffic) */
        boolean lht = turnSide == SIDE_LEFT;
        int exit = roundaboutExitAngle(turnAngle, lht);
        boolean anglePresent = exit != Integer.MIN_VALUE;
        Result m;
        if (event == EV_ROUNDABOUT_ENTER) {
            m = r(MT_ENTER_ROUNDABOUT, anglePresent ? exit : 0, anglePresent);
        } else if (event == EV_ROUNDABOUT_EXIT) {
            m = r(MT_EXIT_ROUNDABOUT, anglePresent ? exit : 0, anglePresent);
        } else if (turnNumber >= 1 && turnNumber <= MT_ROUNDABOUT_EXIT_19 - MT_ROUNDABOUT_EXIT_1 + 1) {
            m = r(MT_ROUNDABOUT_EXIT_1 + turnNumber - 1, anglePresent ? exit : 0, anglePresent);
        } else if (anglePresent) {
            /* exit number unknown: the BAP/renderer direction comes from the angle alone */
            m = r(MT_ROUNDABOUT_EXIT_1, exit, true);
        } else {
            m = r(MT_ENTER_ROUNDABOUT, 0, false);
        }
        /* ManeuverMapper only accepts the roundabout-exit family with junctionType 1 */
        m.junctionType = JUNCTION_ROUNDABOUT;
        m.drivingSide = lht ? DRIVING_SIDE_LEFT : DRIVING_SIDE_RIGHT;
        return m;
    }
}
