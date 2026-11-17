/*
 * WazeCardManeuver - recognised CarPlay instruction card -> HUD maneuver arrow
 *
 * The Waze path reads the maneuver back from the card iOS draws on the cluster
 * stream (altscreen_render/card_rec.h).  This class turns that card class into
 * the BAP ManeuverDescriptor pair the HUD draws.
 *
 * Why the card class does not go through ManeuverMapper.map(): the HUD shows
 * one pre-made picture chosen only by (mainElement, direction), and Waze must
 * never get a roundabout schematic.  The recogniser's cp_maneuver_type is not
 * the type the app sent, it is a stand-in: every ROUNDABOUT_LEFT / STRAIGHT /
 * RIGHT card carries cp 6 (ENTER_ROUNDABOUT), which ManeuverMapper.map() turns
 * into TURN right (left in left-hand traffic) whatever exit the card shows;
 * cp 19 (U_TURN_AT_ROUNDABOUT) becomes the ring schematic, and cp 28 + n with
 * no exit angle becomes a ring with an invented straight exit.  So the Waze
 * path maps from the card class (and traffic side) here, and never from cp.
 *
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */
package com.luka.carplay.rgd;

public final class WazeCardManeuver {

    /* card_rec_class_t (altscreen_render/card_rec.h): the values MUST equal the
     * C enum.  tests/WazeCardManeuverTest.java parses the header to check it. */
    public static final int CLASS_NONE = 0;
    public static final int CLASS_LEFT_TURN = 1;
    public static final int CLASS_RIGHT_TURN = 2;
    public static final int CLASS_STRAIGHT = 3;
    public static final int CLASS_KEEP_LEFT = 4;
    public static final int CLASS_KEEP_RIGHT = 5;
    public static final int CLASS_ARRIVE = 6;
    public static final int CLASS_UTURN_LEFT = 7;
    public static final int CLASS_UTURN_RIGHT = 8;
    public static final int CLASS_ROUNDABOUT_NUMBERED = 9;
    public static final int CLASS_ROUNDABOUT_LEFT = 10;
    public static final int CLASS_ROUNDABOUT_STRAIGHT = 11;
    public static final int CLASS_ROUNDABOUT_RIGHT = 12;
    public static final int CLASS_ROUNDABOUT_UTURN = 13;
    public static final int CLASS_COUNT = 14;

    /* card_rec_result_t.traffic_side: the side the card art was drawn for. */
    public static final char SIDE_RIGHT = 'R';     /* right-hand traffic */
    public static final char SIDE_LEFT = 'L';      /* left-hand traffic */
    public static final char SIDE_NONE = 0;        /* art without a side */

    private WazeCardManeuver() {
    }

    /**
     * The HUD arrow for one recognised card.
     *
     * Send the pair with EMPTY sideStreets and zLevel 0 (the HUD adds grey
     * stubs for sideStreets and a triangle for zLevel; neither belongs to a
     * card read).  { NO_SYMBOL, 0 } means no correct arrow exists: clear the
     * HUD with the NO_SYMBOL descriptor, never with FOLLOW_STREET (the HUD
     * draws nothing for FOLLOW_STREET/0 and then reads stale payload bytes as
     * a second maneuver).
     *
     * No symbol for: NONE; ROUNDABOUT_NUMBERED (the HUD cannot draw the exit
     * number, any arrow would be a guess); ROUNDABOUT_UTURN without a traffic
     * side; an unknown class; a traffic side other than 'R', 'L' or 0.
     *
     * @param cardClass   card_rec_class_t value (CLASS_*)
     * @param trafficSide card_rec_result_t.traffic_side: SIDE_RIGHT, SIDE_LEFT
     *                    or SIDE_NONE
     * @return a new int[2] = { mainElement, direction }
     */
    public static int[] hudArrow(int cardClass, char trafficSide) {
        if (trafficSide != SIDE_RIGHT && trafficSide != SIDE_LEFT && trafficSide != SIDE_NONE) {
            return pair(ManeuverMapper.NO_SYMBOL, ManeuverMapper.DIR_STRAIGHT);
        }
        switch (cardClass) {
            case CLASS_LEFT_TURN:
            case CLASS_ROUNDABOUT_LEFT:
                return pair(ManeuverMapper.TURN, ManeuverMapper.DIR_LEFT);
            case CLASS_RIGHT_TURN:
            case CLASS_ROUNDABOUT_RIGHT:
                return pair(ManeuverMapper.TURN, ManeuverMapper.DIR_RIGHT);
            case CLASS_STRAIGHT:
            case CLASS_ROUNDABOUT_STRAIGHT:
                return pair(ManeuverMapper.TURN, ManeuverMapper.DIR_STRAIGHT);
            case CLASS_KEEP_LEFT:
                return pair(ManeuverMapper.TURN, ManeuverMapper.DIR_SLIGHT_LEFT);
            case CLASS_KEEP_RIGHT:
                return pair(ManeuverMapper.TURN, ManeuverMapper.DIR_SLIGHT_RIGHT);
            case CLASS_ARRIVE:
                return pair(ManeuverMapper.ARRIVED, ManeuverMapper.DIR_STRAIGHT);
            case CLASS_UTURN_LEFT:
                return pair(ManeuverMapper.UTURN, ManeuverMapper.DIR_LEFT);
            case CLASS_UTURN_RIGHT:
                return pair(ManeuverMapper.UTURN, ManeuverMapper.DIR_RIGHT);
            case CLASS_ROUNDABOUT_UTURN:
                /* Right-hand traffic circles anticlockwise: the U-turn bends left. */
                if (trafficSide == SIDE_RIGHT) {
                    return pair(ManeuverMapper.UTURN, ManeuverMapper.DIR_LEFT);
                }
                if (trafficSide == SIDE_LEFT) {
                    return pair(ManeuverMapper.UTURN, ManeuverMapper.DIR_RIGHT);
                }
                return pair(ManeuverMapper.NO_SYMBOL, ManeuverMapper.DIR_STRAIGHT);
            default:
                /* NONE, ROUNDABOUT_NUMBERED, unknown or out-of-range ids */
                return pair(ManeuverMapper.NO_SYMBOL, ManeuverMapper.DIR_STRAIGHT);
        }
    }

    private static int[] pair(int mainElement, int direction) {
        return new int[] { mainElement, direction };
    }
}
