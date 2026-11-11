package com.luka.carplay.rgd;

/** Per-resolution working value. Compensation never edits the received State.
 * An ordinal, a measured angle and a coarse direction are different evidence. */
final class ManeuverSemantics {
    static final int ANGLE_UNKNOWN = 0;
    static final int ANGLE_RECEIVED = 1;
    static final int ANGLE_DIRECTION_HINT = 2;

    final int type; // received enum, retained even when a provider encodes a different action
    final int drivingSide;
    final boolean drivingSideInferred;
    int junctionType;
    boolean junctionInferred;
    int angle;
    boolean anglePresent;
    int angleSource;
    int exitNumber; // 0 = unknown; never derived from a Baidu enum
    boolean roundaboutDeparture; // known departure stage, without an exit ordinal

    ManeuverSemantics(int type, int junction, int side, int fallbackSide, int angle, boolean present) {
        this.type = type;
        this.junctionType = junction;
        if (side == ManeuverMapper.DRIVING_SIDE_NOT_SET) {
            this.drivingSide = fallbackSide == ManeuverMapper.DRIVING_SIDE_LEFT
                ? ManeuverMapper.DRIVING_SIDE_LEFT : ManeuverMapper.DRIVING_SIDE_RIGHT;
            this.drivingSideInferred = true;
        } else {
            this.drivingSide = side;
            this.drivingSideInferred = false;
        }
        this.angle = present ? angle : 1000;
        this.anglePresent = present;
        this.angleSource = present && angle >= -180 && angle <= 180
            ? ANGLE_RECEIVED : ANGLE_UNKNOWN;
    }

    boolean hasAngle() { return angleSource != ANGLE_UNKNOWN; }

    int mappingType() {
        return roundaboutDeparture ? ManeuverMapper.MT_EXIT_ROUNDABOUT : type;
    }

    boolean isNumberedRoundabout() {
        return type >= ManeuverMapper.MT_ROUNDABOUT_EXIT_1
            && type <= ManeuverMapper.MT_ROUNDABOUT_EXIT_19;
    }
}
