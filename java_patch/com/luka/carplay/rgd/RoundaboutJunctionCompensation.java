package com.luka.carplay.rgd;

/** A roundabout exit type supplies the missing junction kind, never overrides
 * an explicitly supplied intersection or an unknown junction value. */
final class RoundaboutJunctionCompensation implements ManeuverCompensation {
    public void apply(ManeuverSemantics value) {
        if (value.junctionType == ManeuverMapper.JUNCTION_NOT_SET
                && (value.type == ManeuverMapper.MT_EXIT_ROUNDABOUT
                    || value.isNumberedRoundabout())) {
            value.junctionType = ManeuverMapper.JUNCTION_ROUNDABOUT;
            value.junctionInferred = true;
        }
    }
}
