/* CarPlay renderer: BAP semantic fallback with richer compatible geometry.
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */
package com.luka.carplay.rgd;

public final class RendererMapper {
    private static final int[] NO_ROADS = new int[0];
    private RendererMapper() { }
    public static final int ICON_NONE = 0;
    public static final int ICON_APPROACH = 1;
    public static final int ICON_TURN = 2;
    public static final int ICON_UTURN = 3;
    public static final int ICON_MERGE = 4; // legacy wire icon; no automatic iOS mapping
    public static final int ICON_EXIT = 5;
    public static final int ICON_ROUNDABOUT = 6;
    public static final int ICON_ARRIVED = 7;
    public static final int ICON_LANE_CHANGE = 8;
    public static final int ICON_ROUNDABOUT_EXIT = 9;

    /** Angles are signed half-degrees, preserving every 22.5-degree BAP bin. */
    public static final class Mapping {
        public int icon;
        public int direction;
        public int exitAngle;
        public int drivingSide;
        public int[] junctionAngles;
        public boolean snapToRoad;
    }

    public static int directionAngleHalfDegrees(int direction) {
        int d = direction & 255;
        if (d == 128) return 360;
        return d < 128 ? -(d * 45 / 16) : ((256 - d) * 45 / 16);
    }

    /** Resolve the shared HUD semantics once, adding only raw geometry used by
     * the procedural renderer. No BAP side-street round trip or lane input. */
    public static Mapping map(int main, int direction, int drivingSide, int maneuverType,
                              int angle, boolean anglePresent, int[] roads) {
        return withCarPlayGeometry(fromBap(main, direction, drivingSide),
            maneuverType, angle, anglePresent, roads);
    }

    private static Mapping fromBap(int main, int direction, int drivingSide) {
        Mapping out = new Mapping();
        out.drivingSide = drivingSide == ManeuverMapper.DRIVING_SIDE_LEFT ? 1 : 0;
        out.exitAngle = directionAngleHalfDegrees(direction);
        switch (main) {
            case ManeuverMapper.FOLLOW_STREET: out.icon = ICON_APPROACH; break;
            case ManeuverMapper.TURN: out.icon = ICON_TURN; break;
            case ManeuverMapper.UTURN:
                out.icon = ICON_UTURN;
                out.direction = direction == ManeuverMapper.DIR_LEFT ? -1 : 1;
                out.exitAngle = out.direction * 360;
                break;
            case ManeuverMapper.EXIT_LEFT:
            case ManeuverMapper.EXIT_RIGHT:
                out.icon = ICON_EXIT;
                out.direction = main == ManeuverMapper.EXIT_LEFT ? -1 : 1;
                out.exitAngle = 0;
                break;
            case ManeuverMapper.CHANGE_LANE:
                out.icon = ICON_LANE_CHANGE;
                out.direction = direction == ManeuverMapper.DIR_LEFT ? -1 : 1;
                out.exitAngle = 0;
                break;
            case ManeuverMapper.ROUNDABOUT_TRS_LEFT:
            case ManeuverMapper.ROUNDABOUT_TRS_RIGHT:
                out.icon = ICON_ROUNDABOUT;
                out.drivingSide = main == ManeuverMapper.ROUNDABOUT_TRS_LEFT ? 1 : 0;
                break;
            case ManeuverMapper.EXIT_ROUNDABOUT_TRS_LEFT:
            case ManeuverMapper.EXIT_ROUNDABOUT_TRS_RIGHT:
                out.icon = ICON_ROUNDABOUT_EXIT;
                out.drivingSide = main == ManeuverMapper.EXIT_ROUNDABOUT_TRS_LEFT ? 1 : 0;
                out.direction = out.drivingSide == 1 ? -1 : 1;
                out.exitAngle = out.direction * 90; // fixed diagonal exit, as HUD s112/s113
                break;
            case ManeuverMapper.ARRIVED:
                out.icon = ICON_ARRIVED;
                out.direction = direction == ManeuverMapper.DIR_LEFT ? -1
                    : direction == ManeuverMapper.DIR_RIGHT ? 1 : 0;
                out.exitAngle = 0;
                break;
            default:
                out.icon = ICON_NONE;
                out.exitAngle = 0;
                break;
        }
        out.junctionAngles = NO_ROADS;
        return out;
    }

    /** Keep the renderer's full circles and continuous turn geometry. BAP has a
     * finite icon catalogue; that does not limit our procedural path builder.
     * A missing/invalid angle or a side conflicting with the resolved maneuver
     * still uses the BAP/type fallback. Angles here remain in half-degrees. */
    private static Mapping withCarPlayGeometry(Mapping out, int mt, int angle,
                                              boolean present, int[] roads) {
        boolean valid = present && angle >= -180 && angle <= 180;
        boolean ramp = mt == ManeuverMapper.MT_OFF_RAMP || mt == ManeuverMapper.MT_ON_RAMP
            || mt == ManeuverMapper.MT_HIGHWAY_OFF_RAMP_LEFT
            || mt == ManeuverMapper.MT_HIGHWAY_OFF_RAMP_RIGHT;
        if (out.icon != ICON_NONE && (mt == ManeuverMapper.MT_ENTER_ROUNDABOUT
                || mt == ManeuverMapper.MT_EXIT_ROUNDABOUT
                || (mt >= ManeuverMapper.MT_ROUNDABOUT_EXIT_1
                    && mt <= ManeuverMapper.MT_ROUNDABOUT_EXIT_19))) {
            out.icon = ICON_ROUNDABOUT;
            out.direction = 0;
            out.exitAngle = valid ? angle * 2 : 0;
            // Only actual angles may snap. Missing fallback and typed U-turn
            // must not be redirected to a neighbouring exit.
            out.snapToRoad = valid;
        } else if (out.icon == ICON_TURN && !ramp && valid) {
            boolean generic = mt == ManeuverMapper.MT_START_ROUTE
                || mt == ManeuverMapper.MT_EXIT_FERRY || mt == ManeuverMapper.MT_CHANGE_HIGHWAY;
            if (generic || (out.exitAngle < 0 && angle < 0) || (out.exitAngle > 0 && angle > 0))
                out.exitAngle = angle * 2;
        }
        if (out.icon == ICON_APPROACH || out.icon == ICON_TURN || out.icon == ICON_ROUNDABOUT)
            out.junctionAngles = rawRoads(roads);
        // EXIT used to supply the continuing main road through its geometry.
        // A single-bend TURN needs that road explicitly, just as the HUD's
        // fixed OFF_RAMP side street 0. Keep the richer CarPlay roads as well.
        if (out.icon == ICON_TURN && (mt == ManeuverMapper.MT_OFF_RAMP
                || mt == ManeuverMapper.MT_HIGHWAY_OFF_RAMP_LEFT
                || mt == ManeuverMapper.MT_HIGHWAY_OFF_RAMP_RIGHT))
            out.junctionAngles = withForwardRoad(out.junctionAngles);
        return out;
    }

    private static int[] withForwardRoad(int[] roads) {
        for (int i = 0; i < roads.length; i++) if (roads[i] == 0) return roads;
        // Reserve one of the wire's 18 slots for the continuing main road.
        int count = roads.length < 18 ? roads.length : 17;
        int[] out = new int[count + 1];
        System.arraycopy(roads, 0, out, 0, count);
        out[count] = 0;
        return out;
    }

    /** BAP removes the active exit and quantizes/limits side streets. The
     * procedural renderer needs the original roads, including that exit for
     * roundabout snap. Keep valid signed angles, not missing-value sentinels. */
    private static int[] rawRoads(int[] roads) {
        int count = 0;
        if (roads != null) {
            for (int i = 0; i < roads.length && count < 18; i++)
                if (roads[i] >= -180 && roads[i] <= 180) count++;
        }
        int[] out = new int[count];
        int n = 0;
        if (roads != null) {
            for (int i = 0; i < roads.length && n < count; i++)
                if (roads[i] >= -180 && roads[i] <= 180) out[n++] = roads[i] * 2;
        }
        return out;
    }

}
