package com.luka.carplay.rgd;

/** AMap 17.03.0 getManeuverType: icon 25 -> CP28 (left), 26 -> CP30
 * (right), 12/18 -> CP29 (generic departure). Its KVC writer always sets
 * junctionType=0. These are not Baidu directions or Yandex exit ordinals.
 * Only these proven encodings may correct an explicitly supplied junction 0.
 * CP3 is also a catch-all in the app: its lost circle information is unrecoverable. */
final class AmapRoundaboutCompensation implements ManeuverCompensation {
    public void apply(ManeuverSemantics value) {
        if (value.type != ManeuverMapper.MT_ROUNDABOUT_EXIT_1
                && value.type != ManeuverMapper.MT_ROUNDABOUT_EXIT_2
                && value.type != ManeuverMapper.MT_ROUNDABOUT_EXIT_3) return;
        if (value.junctionType == ManeuverMapper.JUNCTION_NOT_SET
                || value.junctionType == ManeuverMapper.JUNCTION_SINGLE_INTERSECTION) {
            value.junctionType = ManeuverMapper.JUNCTION_ROUNDABOUT;
            value.junctionInferred = true;
        }
        if (value.junctionType != ManeuverMapper.JUNCTION_ROUNDABOUT) return;
        if (value.type == ManeuverMapper.MT_ROUNDABOUT_EXIT_2) {
            value.roundaboutDeparture = true;
        } else if (!value.hasAngle()) {
            value.angle = value.type == ManeuverMapper.MT_ROUNDABOUT_EXIT_1 ? -90 : 90;
            value.anglePresent = true;
            value.angleSource = ManeuverSemantics.ANGLE_DIRECTION_HINT;
        }
    }
}
