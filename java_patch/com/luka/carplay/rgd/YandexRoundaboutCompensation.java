package com.luka.carplay.rgd;

/** Navigator 30.5.0 and Maps 30.0.1/31.1.5 map
 * leaveRoundaboutMetadata.exitNumber 1..19 to CP 28..46.
 * The ordinal carries no angle. CP 7 also represents exits >=20, so its exact
 * number cannot be recovered. */
final class YandexRoundaboutCompensation implements ManeuverCompensation {
    public void apply(ManeuverSemantics value) {
        if (value.junctionType == ManeuverMapper.JUNCTION_ROUNDABOUT
                && value.isNumberedRoundabout())
            value.exitNumber = value.type - ManeuverMapper.MT_ROUNDABOUT_EXIT_1 + 1;
    }
}
