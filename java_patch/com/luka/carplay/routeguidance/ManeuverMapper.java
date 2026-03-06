/*
 * ManeuverMapper - Maps CarPlay (iAP2) ManeuverType to BAP mainElement + direction
 *
 * Based on TBT_MAPPING_PLAN.md and mib2-android-auto-vc patterns.
 * Uses correct MU1316 LSD BAP constants.
 *
 */
package com.luka.carplay.routeguidance;

public class ManeuverMapper {

    /* BAP MainElement constants (from CombiBAPConstantsNavi) */
    public static final int NO_SYMBOL = 0;
    public static final int NO_INFO = 1;
    public static final int DIRECTION_TO_DESTINATION = 2;
    public static final int ARRIVED = 3;
    public static final int NEAR_DESTINATION = 4;
    public static final int ARRIVED_DESTINATION_OFFMAP = 5;
    public static final int OFF_ROAD = 6;                  /* Non-sync */
    public static final int OFF_MAP = 7;
    public static final int NO_ROUTE = 8;
    public static final int CALC_ROUTE = 9;                /* Non-sync */
    public static final int RECALC_ROUTE = 10;             /* Non-sync */
    public static final int FOLLOW_STREET = 11;
    public static final int CHANGE_LANE = 12;
    public static final int TURN = 13;
    public static final int TURN_ON_MAINROAD = 14;
    public static final int EXIT_RIGHT = 15;
    public static final int EXIT_LEFT = 16;
    public static final int SERVICE_ROAD_RIGHT = 17;
    public static final int SERVICE_ROAD_LEFT = 18;
    public static final int FORK_2 = 19;
    public static final int FORK_3 = 20;
    public static final int ROUNDABOUT_TRS_RIGHT = 21;
    public static final int ROUNDABOUT_TRS_LEFT = 22;
    public static final int SQUARE_TRS_RIGHT = 23;
    public static final int SQUARE_TRS_LEFT = 24;
    public static final int UTURN = 25;
    public static final int EXIT_ROUNDABOUT_TRS_RIGHT = 26;
    public static final int EXIT_ROUNDABOUT_TRS_LEFT = 27;
    public static final int PREPARE_TURN = 28;
    public static final int PREPARE_ROUNDABOUT = 29;
    public static final int PREPARE_SQUARE = 30;
    public static final int PREPARE_U_TURN = 31;
    public static final int MICHIGAN_TURN = 34;
    public static final int DOUBLE_TURN = 35;
    public static final int DIRECTION_TO_WAYPOINT = 38;
    /*
     * MHI3 tbt_renderer uses mainElement=44 for ferry, but MU1316 BAP spec
     * (CombiBAPConstantsNavi) only defines up to 38 and IconUtil has no case 44.
     * Map ferry to FOLLOW_STREET instead - matches EXIT_FERRY behavior.
     */

    /* BAP Direction constants */
    public static final int DIR_STRAIGHT = 0;
    public static final int DIR_SLIGHT_LEFT = 32;
    public static final int DIR_LEFT = 64;
    public static final int DIR_SHARP_LEFT = 96;
    public static final int DIR_UTURN = 128;
    public static final int DIR_SHARP_RIGHT = 160;
    public static final int DIR_RIGHT = 192;
    public static final int DIR_SLIGHT_RIGHT = 224;

    /*
     * MHI3 (tbt_renderer) values (as logged by sub_2B5B30):
     * - junctionType: 0=single intersection, 1=roundabout
     * - drivingSide:  0=right-hand traffic, 1=left-hand traffic
     *
     * Our C hook passes the raw iAP2 values through, and captured packets match these values.
     */
    public static final int JUNCTION_SINGLE_INTERSECTION = 0;
    public static final int JUNCTION_ROUNDABOUT = 1;
    public static final int DRIVING_SIDE_RIGHT = 0;
    public static final int DRIVING_SIDE_LEFT = 1;

    /*
     * iOS accNav ManeuverType values.
     *
     * Verified against MHI3 dio_manager type-info strings
     * (CDIONavigationMetadataTypeInfo::toString(EManeuverType)):
     *   0=NO_TURN, 1=LEFT_TURN, 2=RIGHT_TURN, ... , 53=CHANGE_HIGHWAY_RIGHT
     *
     * RouteGuidance.State initializes "missing" types to -1, so we keep MT_NOT_SET outside
     * the valid range to be explicit.
     */
    public static final int MT_NOT_SET                    = -1;
    public static final int MT_NO_TURN                    = 0;
    public static final int MT_LEFT_TURN                  = 1;
    public static final int MT_RIGHT_TURN                 = 2;
    public static final int MT_STRAIGHT_AHEAD             = 3;
    public static final int MT_U_TURN                     = 4;
    public static final int MT_FOLLOW_ROAD                = 5;   /* "CONTINUE" in MHI3 */
    public static final int MT_ENTER_ROUNDABOUT           = 6;
    public static final int MT_EXIT_ROUNDABOUT            = 7;
    public static final int MT_OFF_RAMP                   = 8;
    public static final int MT_ON_RAMP                    = 9;
    public static final int MT_ARRIVE_END_OF_NAVIGATION   = 10;
    public static final int MT_START_ROUTE                = 11;  /* "PROCEED_TO_BEGINNING_OF_ROUTE" in MHI3 */
    public static final int MT_ARRIVE_AT_DESTINATION      = 12;  /* "ARRIVE" in MHI3 */
    public static final int MT_KEEP_LEFT                  = 13;
    public static final int MT_KEEP_RIGHT                 = 14;
    public static final int MT_ENTER_FERRY                = 15;
    public static final int MT_EXIT_FERRY                 = 16;
    public static final int MT_CHANGE_FERRY               = 17;
    public static final int MT_START_ROUTE_WITH_U_TURN    = 18;  /* "UTURN_PROCEED_TO_ROUTE" in MHI3 */
    public static final int MT_U_TURN_AT_ROUNDABOUT       = 19;  /* "ROUNDABOUT_UTURN" in MHI3 */
    public static final int MT_LEFT_TURN_AT_END           = 20;  /* "TURN_LEFT_END_OF_ROAD" in MHI3 */
    public static final int MT_RIGHT_TURN_AT_END          = 21;  /* "TURN_RIGHT_END_OF_ROAD" in MHI3 */
    public static final int MT_HIGHWAY_OFF_RAMP_LEFT      = 22;
    public static final int MT_HIGHWAY_OFF_RAMP_RIGHT     = 23;
    public static final int MT_ARRIVE_DESTINATION_LEFT    = 24;  /* "ARRIVE_LEFT" in MHI3 */
    public static final int MT_ARRIVE_DESTINATION_RIGHT   = 25;  /* "ARRIVE_RIGHT" in MHI3 */
    public static final int MT_U_TURN_WHEN_POSSIBLE       = 26;
    public static final int MT_ARRIVE_END_OF_DIRECTIONS   = 27;
    public static final int MT_ROUNDABOUT_EXIT_1          = 28;
    public static final int MT_ROUNDABOUT_EXIT_2          = 29;
    public static final int MT_ROUNDABOUT_EXIT_3          = 30;
    public static final int MT_ROUNDABOUT_EXIT_4          = 31;
    public static final int MT_ROUNDABOUT_EXIT_5          = 32;
    public static final int MT_ROUNDABOUT_EXIT_6          = 33;
    public static final int MT_ROUNDABOUT_EXIT_7          = 34;
    public static final int MT_ROUNDABOUT_EXIT_8          = 35;
    public static final int MT_ROUNDABOUT_EXIT_9          = 36;
    public static final int MT_ROUNDABOUT_EXIT_10         = 37;
    public static final int MT_ROUNDABOUT_EXIT_11         = 38;
    public static final int MT_ROUNDABOUT_EXIT_12         = 39;
    public static final int MT_ROUNDABOUT_EXIT_13         = 40;
    public static final int MT_ROUNDABOUT_EXIT_14         = 41;
    public static final int MT_ROUNDABOUT_EXIT_15         = 42;
    public static final int MT_ROUNDABOUT_EXIT_16         = 43;
    public static final int MT_ROUNDABOUT_EXIT_17         = 44;
    public static final int MT_ROUNDABOUT_EXIT_18         = 45;
    public static final int MT_ROUNDABOUT_EXIT_19         = 46;
    public static final int MT_SHARP_LEFT_TURN            = 47;
    public static final int MT_SHARP_RIGHT_TURN           = 48;
    public static final int MT_SLIGHT_LEFT_TURN           = 49;
    public static final int MT_SLIGHT_RIGHT_TURN          = 50;
    public static final int MT_CHANGE_HIGHWAY             = 51;
    public static final int MT_CHANGE_HIGHWAY_LEFT        = 52;
    public static final int MT_CHANGE_HIGHWAY_RIGHT       = 53;

    /**
     * Maps iAP2 ManeuverType to BAP mainElement and direction.
     *
     * @param maneuverType iOS accNav ManeuverType (0-53)
     * @param turnAngle    JunctionElementExitAngle (signed). In MHI3 this is used for end-of-road / ramps.
     *                    For roundabout exits 1..19 it is quantized to nearest 22.5deg and mapped to a 0..240 code.
     * @param junctionType Junction type (0=intersection, 1=roundabout)
     * @param drivingSide  Driving side (0=RHT, 1=LHT)
     * @return int[2] = { mainElement, direction }
     */
    public static int[] map(int maneuverType, int turnAngle, int junctionType, int drivingSide) {
        int mainElement;
        int direction;

        /*
         * IMPORTANT (MHI3 parity):
         * tbt_renderer sub_2B9C90 applies a strong junctionType gate.
         *
         * - junctionType==0: normal intersection mapping (turns/ramps/etc).
         * - junctionType==1: only roundabout exit types (7, 28..46) are mapped here; most other
         *   maneuver types become NO_INFO.
         * - other junctionType values: NO_INFO.
         *
         * Some maneuver types are treated as "global" and mapped before that gate
         * (follow-road, arrived, ferry, etc). We replicate the same ordering below.
         */

        /* Explicit "not set" sentinel from our State init */
        if (maneuverType == MT_NOT_SET) {
            mainElement = NO_SYMBOL;
            direction = DIR_STRAIGHT;
            return new int[] { mainElement, direction };
        }

        /* Global mappings (apply regardless of junctionType) */
        if (maneuverType == MT_NO_TURN || maneuverType == MT_FOLLOW_ROAD) {
            mainElement = FOLLOW_STREET;
            direction = DIR_STRAIGHT;
            return new int[] { mainElement, direction };
        }
        if (maneuverType == MT_ARRIVE_END_OF_NAVIGATION
            || maneuverType == MT_ARRIVE_AT_DESTINATION
            || maneuverType == MT_ARRIVE_END_OF_DIRECTIONS) {
            mainElement = ARRIVED;
            direction = DIR_STRAIGHT;
            return new int[] { mainElement, direction };
        }
        if (maneuverType == MT_CHANGE_HIGHWAY_LEFT) {
            mainElement = CHANGE_LANE;
            direction = DIR_LEFT;
            return new int[] { mainElement, direction };
        }
        if (maneuverType == MT_CHANGE_HIGHWAY_RIGHT) {
            mainElement = CHANGE_LANE;
            direction = DIR_RIGHT;
            return new int[] { mainElement, direction };
        }
        if (maneuverType == MT_ENTER_FERRY || maneuverType == MT_CHANGE_FERRY) {
            mainElement = FOLLOW_STREET;
            direction = DIR_STRAIGHT;
            return new int[] { mainElement, direction };
        }
        if (maneuverType == MT_START_ROUTE) {
            mainElement = FOLLOW_STREET;
            direction = DIR_STRAIGHT;
            return new int[] { mainElement, direction };
        }
        if (maneuverType == MT_EXIT_FERRY || maneuverType == MT_CHANGE_HIGHWAY) {
            mainElement = FOLLOW_STREET;
            direction = DIR_STRAIGHT;
            return new int[] { mainElement, direction };
        }
        if (maneuverType == MT_U_TURN_AT_ROUNDABOUT) {
            mainElement = (drivingSide == DRIVING_SIDE_LEFT) ? ROUNDABOUT_TRS_LEFT : ROUNDABOUT_TRS_RIGHT;
            direction = DIR_UTURN;
            return new int[] { mainElement, direction };
        }
        if (maneuverType == MT_ENTER_ROUNDABOUT) {
            /* MHI3 maps ENTER_ROUNDABOUT to TURN left/right depending on driving side. */
            mainElement = TURN;
            direction = (drivingSide == DRIVING_SIDE_LEFT) ? DIR_LEFT : DIR_RIGHT;
            direction = applyDsiNavBapDirectionOverride(maneuverType, direction);
            return new int[] { mainElement, direction };
        }

        /*
         * JunctionType gate (see comment above):
         * - For junctionType!=0, only allow the "roundabout exit" family for junctionType==1.
         */
        if (junctionType != JUNCTION_SINGLE_INTERSECTION) {
            if (junctionType != JUNCTION_ROUNDABOUT) {
                mainElement = NO_INFO;
                direction = DIR_STRAIGHT;
                return new int[] { mainElement, direction };
            }

            if (maneuverType == MT_EXIT_ROUNDABOUT) {
                mainElement = (drivingSide == DRIVING_SIDE_LEFT) ? EXIT_ROUNDABOUT_TRS_LEFT : EXIT_ROUNDABOUT_TRS_RIGHT;
                direction = (drivingSide == DRIVING_SIDE_LEFT) ? DIR_LEFT : DIR_RIGHT;
                /* DSINavBap override is skipped for this type */
                return new int[] { mainElement, direction };
            }

            if (maneuverType >= MT_ROUNDABOUT_EXIT_1 && maneuverType <= MT_ROUNDABOUT_EXIT_19) {
                mainElement = (drivingSide == DRIVING_SIDE_LEFT) ? ROUNDABOUT_TRS_LEFT : ROUNDABOUT_TRS_RIGHT;
                direction = directionFromAngle16(turnAngle);
                /* DSINavBap override is skipped for this type */
                return new int[] { mainElement, direction };
            }

            /* For any other maneuverType while junctionType==1, MHI3 returns NO_INFO. */
            mainElement = NO_INFO;
            direction = DIR_STRAIGHT;
            return new int[] { mainElement, direction };
        }

        switch (maneuverType) {
            case MT_STRAIGHT_AHEAD:
                mainElement = FOLLOW_STREET;
                direction = DIR_STRAIGHT;
                break;

            case MT_LEFT_TURN:
                mainElement = TURN;
                direction = DIR_LEFT;
                break;

            case MT_RIGHT_TURN:
                mainElement = TURN;
                direction = DIR_RIGHT;
                break;

            case MT_SLIGHT_LEFT_TURN:
            case MT_KEEP_LEFT:
                /* MHI3 maps KEEP_LEFT and SLIGHT_LEFT to TURN+SLIGHT_LEFT */
                mainElement = TURN;
                direction = DIR_SLIGHT_LEFT;
                break;

            case MT_SLIGHT_RIGHT_TURN:
            case MT_KEEP_RIGHT:
                /* MHI3 maps KEEP_RIGHT and SLIGHT_RIGHT to TURN+SLIGHT_RIGHT */
                mainElement = TURN;
                direction = DIR_SLIGHT_RIGHT;
                break;

            case MT_SHARP_LEFT_TURN:
                mainElement = TURN;
                direction = DIR_SHARP_LEFT;
                break;

            case MT_SHARP_RIGHT_TURN:
                mainElement = TURN;
                direction = DIR_SHARP_RIGHT;
                break;

            case MT_U_TURN:
            case MT_START_ROUTE_WITH_U_TURN:
            case MT_U_TURN_WHEN_POSSIBLE:
                /* MHI3 uses a left/right variant depending on driving side. */
                mainElement = UTURN;
                if (drivingSide == DRIVING_SIDE_LEFT) direction = DIR_RIGHT;
                else direction = DIR_LEFT;
                break;

            case MT_OFF_RAMP:
                /*
                 * BAP EXIT_RIGHT/EXIT_LEFT renders a highway off-ramp icon.
                 * MHI3 used TURN+slight (coarsened to 90-degree), but on MHI2
                 * VC the EXIT icon is much clearer for ramp maneuvers.
                 */
                if (drivingSide == DRIVING_SIDE_LEFT) {
                    mainElement = EXIT_LEFT;
                    direction = DIR_SLIGHT_LEFT;
                } else {
                    mainElement = EXIT_RIGHT;
                    direction = DIR_SLIGHT_RIGHT;
                }
                break;

            case MT_ON_RAMP:
                /* On-ramp: slight merge direction, not coarsened to 90-degree. */
                mainElement = TURN;
                if (drivingSide == DRIVING_SIDE_LEFT && ((turnAngle & 0xffffffffL) > 0xB3L)) {
                    direction = DIR_SLIGHT_LEFT;
                } else {
                    direction = DIR_SLIGHT_RIGHT;
                }
                break;

            case MT_HIGHWAY_OFF_RAMP_LEFT:
                mainElement = EXIT_LEFT;
                direction = DIR_SLIGHT_LEFT;
                break;

            case MT_HIGHWAY_OFF_RAMP_RIGHT:
                mainElement = EXIT_RIGHT;
                direction = DIR_SLIGHT_RIGHT;
                break;

            case MT_ARRIVE_DESTINATION_LEFT:
            case MT_ARRIVE_DESTINATION_RIGHT:
                /* MHI3 uses ARRIVED + left/right direction */
                mainElement = ARRIVED;
                direction = (maneuverType == MT_ARRIVE_DESTINATION_LEFT) ? DIR_LEFT : DIR_RIGHT;
                break;

            case MT_LEFT_TURN_AT_END:
                mainElement = TURN;
                direction = dirFromEndOfRoadAngleLeft(turnAngle);
                break;

            case MT_RIGHT_TURN_AT_END:
                mainElement = TURN;
                direction = dirFromEndOfRoadAngleRight(turnAngle);
                break;

            default:
                /* MHI3 sub_2B9C90: unknown junctionType gate → NO_INFO(1) */
                mainElement = NO_INFO;
                direction = DIR_STRAIGHT;
                break;
        }

        /*
         * MHI3 DSINavBap applies an additional direction override for most actions
         * (everything except roundabout-related and uturn actions). It coarsens
         * slight/sharp into plain left/right (and other non-left/right into straight).
         *
         * We don't have "Action"/"Turn" enums in CarPlay iAP2, so we apply the
         * same practical effect by gating on maneuverType categories.
         *
         * TODO to test without it
         */
        direction = applyDsiNavBapDirectionOverride(maneuverType, direction);

        return new int[] { mainElement, direction };
    }

    private static int applyDsiNavBapDirectionOverride(int maneuverType, int dir) {
        /* Skip override for roundabout + uturn actions (DSINavBap action in {5..8}). */
        if (maneuverType == MT_ENTER_ROUNDABOUT
            || maneuverType == MT_EXIT_ROUNDABOUT
            || maneuverType == MT_U_TURN_AT_ROUNDABOUT
            || maneuverType == MT_U_TURN
            || maneuverType == MT_START_ROUTE_WITH_U_TURN
            || maneuverType == MT_U_TURN_WHEN_POSSIBLE
            || (maneuverType >= MT_ROUNDABOUT_EXIT_1 && maneuverType <= MT_ROUNDABOUT_EXIT_19)) {
            return dir;
        }
        /* Skip override for ramp/keep/slight - these need fine direction on MHI2 VC.
         * EXIT_RIGHT/EXIT_LEFT mainElement already encodes the ramp semantics;
         * coarsening SLIGHT→full 90-degree defeats the point of the distinct icon. */
        if (maneuverType == MT_OFF_RAMP
            || maneuverType == MT_ON_RAMP
            || maneuverType == MT_HIGHWAY_OFF_RAMP_LEFT
            || maneuverType == MT_HIGHWAY_OFF_RAMP_RIGHT
            || maneuverType == MT_KEEP_LEFT
            || maneuverType == MT_KEEP_RIGHT
            || maneuverType == MT_SLIGHT_LEFT_TURN
            || maneuverType == MT_SLIGHT_RIGHT_TURN) {
            return dir;
        }

        /* Coarsen to straight/left/right/uturn only. */
        if (dir == DIR_SLIGHT_LEFT || dir == DIR_LEFT || dir == DIR_SHARP_LEFT) return DIR_LEFT;
        if (dir == DIR_SLIGHT_RIGHT || dir == DIR_RIGHT || dir == DIR_SHARP_RIGHT) return DIR_RIGHT;
        if (dir == DIR_UTURN) return DIR_UTURN;
        return DIR_STRAIGHT;
    }

    /**
     * MHI3 roundabout exit direction encoding uses 16-step values (0..240 step 16),
     * derived by quantizing the exit angle to the nearest multiple of 22.5 degrees.
     *
     * Table extracted from MHI3 tbt_renderer init (unk_530F68 + unk_531010).
     */
    public static int directionFromAngle16(int angle) {
        /*
         * MHI3 tbt_renderer quantizes by picking the closest entry from the fixed
         * exit-angle bins [-180..180 step 22.5] (no wrap-around). Clamp here so
         * out-of-range inputs behave like native "closest bin" selection.
         */
        int a = angle;
        if (a > 180) a = 180;
        if (a < -180) a = -180;

        /* Nearest index in the 17-element list: -180..180 step 22.5 */
        int idx = (int) Math.floor(((a + 180) / 22.5) + 0.5);
        if (idx < 0) idx = 0;
        if (idx > 16) idx = 16;

        /* Direction codes extracted from unk_531010 (ordered by angle ascending). */
        final int[] DIR16 = {
            128, 112, 96, 80, 64, 48, 32, 16, 0, 240, 224, 208, 192, 176, 160, 144, 128
        };
        return DIR16[idx];
    }

    private static int dirFromEndOfRoadAngleLeft(int exitAngle) {
        /*
         * MHI3 logic (sub_2B9C90, MANEUVER_TYPE_TURN_LEFT_END_OF_ROAD):
         * - default: LEFT
         * - if angle is negative and within [-180..-1]:
         *   - [-67..-1]   => SLIGHT_LEFT
         *   - [-112..-68] => LEFT (default)
         *   - < -112      => SHARP_LEFT
         */
        if (exitAngle != 0 && exitAngle < 0 && exitAngle >= -180) {
            if (exitAngle >= -67) return DIR_SLIGHT_LEFT;
            if (exitAngle < -112) return DIR_SHARP_LEFT;
        }
        return DIR_LEFT;
    }

    private static int dirFromEndOfRoadAngleRight(int exitAngle) {
        /*
         * MHI3 logic (sub_2B9C90, MANEUVER_TYPE_TURN_RIGHT_END_OF_ROAD):
         * - default: RIGHT
         * - if 1..67  => SLIGHT_RIGHT
         * - if 113..180 => SHARP_RIGHT
         */
        if (exitAngle != 0 && exitAngle > 0 && exitAngle <= 180) {
            if (exitAngle <= 67) return DIR_SLIGHT_RIGHT;
            if (exitAngle > 112) return DIR_SHARP_RIGHT;
        }
        return DIR_RIGHT;
    }

    /**
     * Check if maneuver type is highway-related (ramps, highway changes).
     * Used for distance threshold selection: highway maneuvers need earlier
     * warning because of higher speeds.
     */
    public static boolean isHighwayManeuver(int maneuverType) {
        return maneuverType == MT_OFF_RAMP
            || maneuverType == MT_ON_RAMP
            || maneuverType == MT_HIGHWAY_OFF_RAMP_LEFT
            || maneuverType == MT_HIGHWAY_OFF_RAMP_RIGHT
            || maneuverType == MT_CHANGE_HIGHWAY
            || maneuverType == MT_CHANGE_HIGHWAY_LEFT
            || maneuverType == MT_CHANGE_HIGHWAY_RIGHT;
    }

    public static boolean isValidType(int maneuverType) {
        return maneuverType >= MT_NO_TURN && maneuverType <= MT_CHANGE_HIGHWAY_RIGHT;
    }

}
