package com.luka.carplay.rgd;

/** Baidu 22.1.20 maps Ring_* directions to CP types 28..32, NOT exit numbers.
 * Three right-hand directions collapse to type 28: use a coarse right symbol,
 * not a claimed measured angle. Real received geometry always wins. */
final class BaiduRoundaboutCompensation implements ManeuverCompensation {
    public void apply(ManeuverSemantics value) {
        if (value.hasAngle() || value.junctionType != ManeuverMapper.JUNCTION_ROUNDABOUT)
            return;
        switch (value.type) {
            case ManeuverMapper.MT_ROUNDABOUT_EXIT_1: value.angle = 90; break;
            case ManeuverMapper.MT_ROUNDABOUT_EXIT_2: value.angle = 0; break;
            case ManeuverMapper.MT_ROUNDABOUT_EXIT_3: value.angle = -45; break;
            case ManeuverMapper.MT_ROUNDABOUT_EXIT_4: value.angle = -90; break;
            case ManeuverMapper.MT_ROUNDABOUT_EXIT_5: value.angle = -135; break;
            default: return;
        }
        value.angleSource = ManeuverSemantics.ANGLE_DIRECTION_HINT;
        value.anglePresent = true;
    }
}
