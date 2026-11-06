package com.luka.carplay.rgd;

/** One resolved instruction shared by HUD and renderer. Raw route data stays
 * untouched; RouteGuidance/BAP session ownership decides whether guidance is active. */
final class ManeuverPresentation {
    static final ManeuverPresentation NONE = new ManeuverPresentation(-1,
        ManeuverMapper.NO_SYMBOL, ManeuverMapper.DIR_STRAIGHT);
    static final ManeuverPresentation FOLLOW = new ManeuverPresentation(-1,
        ManeuverMapper.FOLLOW_STREET, ManeuverMapper.DIR_STRAIGHT);

    final int slot;
    final int main;
    final int direction;

    private ManeuverPresentation(int slot, int main, int direction) {
        this.slot = slot;
        this.main = main;
        this.direction = direction;
    }

    boolean hasManeuver() { return slot >= 0; }

    static int currentSlot(RouteGuidance.State s) {
        if (s.maneuverOrder == null || s.maneuverOrder.length == 0) return -1;
        int slot = s.maneuverOrder[0];
        return slot >= 0 && slot < s.mType.length ? slot : -1;
    }

    static ManeuverPresentation resolve(RouteGuidance.State s, boolean active) {
        if (!active) return NONE;
        // Only the current list entry is authoritative. Count is a total, not
        // availability; a missing/unknown current entry must never borrow look-ahead.
        int slot = currentSlot(s);
        if (slot < 0 || !ManeuverMapper.isValidType(s.mType[slot]))
            return FOLLOW;
        int[] mapped = ManeuverMapper.map(s.mType[slot], s.mTurnAngle[slot],
            s.mJunctionType[slot], s.mDrivingSide[slot], anglePresent(s, slot));
        if (mapped[0] == ManeuverMapper.NO_INFO || mapped[0] == ManeuverMapper.NO_SYMBOL)
            return FOLLOW;
        return new ManeuverPresentation(slot, mapped[0], mapped[1]);
    }

    static boolean anglePresent(RouteGuidance.State s, int slot) {
        return (s.mTurnAnglePresent != null && slot < s.mTurnAnglePresent.length
                && s.mTurnAnglePresent[slot])
            || s.mTurnAngle[slot] != -1;
    }
}
