/*
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */

import com.luka.carplay.rgd.ManeuverMapper;
import com.luka.carplay.rgd.RendererMapper;

public final class RendererMapperDirectionTest {
    private static RendererMapper.Mapping mapping(int type, int angle, int side) {
        int junction = type == ManeuverMapper.MT_EXIT_ROUNDABOUT
            || type == ManeuverMapper.MT_U_TURN_AT_ROUNDABOUT
            || (type >= ManeuverMapper.MT_ROUNDABOUT_EXIT_1 && type <= ManeuverMapper.MT_ROUNDABOUT_EXIT_19) ? 1 : 0;
        int[] bap = ManeuverMapper.map(type, angle, junction, side);
        return RendererMapper.map(bap[0], bap[1], side, type, angle, angle != -1, null);
    }

    private static void expect(String name, int expected, int actual) {
        if (expected != actual) {
            throw new AssertionError(name + ": expected " + expected + ", got " + actual);
        }
    }

    private static void testType(int maneuverType) {
        expect("renderer signed left", -1,
            mapping(maneuverType, -180,
                ManeuverMapper.DRIVING_SIDE_LEFT).direction);
        expect("renderer signed right", 1,
            mapping(maneuverType, 180,
                ManeuverMapper.DRIVING_SIDE_RIGHT).direction);
        expect("renderer RHT fallback", -1,
            mapping(maneuverType, 1000,
                ManeuverMapper.DRIVING_SIDE_RIGHT).direction);
        expect("renderer LHT fallback", 1,
            mapping(maneuverType, 1000,
                ManeuverMapper.DRIVING_SIDE_LEFT).direction);
        expect("renderer negative sentinel fallback", -1,
            mapping(maneuverType, -1000,
                ManeuverMapper.DRIVING_SIDE_RIGHT).direction);
        expect("renderer zero fallback", 1,
            mapping(maneuverType, 0,
                ManeuverMapper.DRIVING_SIDE_LEFT).direction);

        expect("BAP signed left", ManeuverMapper.DIR_LEFT,
            ManeuverMapper.map(maneuverType, -180,
                ManeuverMapper.JUNCTION_SINGLE_INTERSECTION,
                ManeuverMapper.DRIVING_SIDE_LEFT)[1]);
        expect("BAP signed right", ManeuverMapper.DIR_RIGHT,
            ManeuverMapper.map(maneuverType, 180,
                ManeuverMapper.JUNCTION_SINGLE_INTERSECTION,
                ManeuverMapper.DRIVING_SIDE_RIGHT)[1]);
        expect("BAP RHT fallback", ManeuverMapper.DIR_LEFT,
            ManeuverMapper.map(maneuverType, 1000,
                ManeuverMapper.JUNCTION_SINGLE_INTERSECTION,
                ManeuverMapper.DRIVING_SIDE_RIGHT)[1]);
        expect("BAP LHT fallback", ManeuverMapper.DIR_RIGHT,
            ManeuverMapper.map(maneuverType, 1000,
                ManeuverMapper.JUNCTION_SINGLE_INTERSECTION,
                ManeuverMapper.DRIVING_SIDE_LEFT)[1]);
        expect("BAP negative sentinel fallback", ManeuverMapper.DIR_LEFT,
            ManeuverMapper.map(maneuverType, -1000,
                ManeuverMapper.JUNCTION_SINGLE_INTERSECTION,
                ManeuverMapper.DRIVING_SIDE_RIGHT)[1]);
        expect("BAP zero fallback", ManeuverMapper.DIR_RIGHT,
            ManeuverMapper.map(maneuverType, 0,
                ManeuverMapper.JUNCTION_SINGLE_INTERSECTION,
                ManeuverMapper.DRIVING_SIDE_LEFT)[1]);
    }

    private static void testGenericRamp(int maneuverType) {
        // Both ramp types use one slight bend. Direction is carried by angle.
        expect("renderer ramp family", RendererMapper.ICON_TURN, mapping(maneuverType, 1000, 0).icon);
        expect("renderer ramp left angle", -45, (mapping(maneuverType, -35, 0).exitAngle / 2));
        expect("renderer ramp right angle", 45, (mapping(maneuverType, 35, 0).exitAngle / 2));
        expect("renderer ramp RHT missing", 45, (mapping(maneuverType, 1000, 0).exitAngle / 2));

        int[] left = ManeuverMapper.map(maneuverType, -35,
            ManeuverMapper.JUNCTION_SINGLE_INTERSECTION,
            ManeuverMapper.DRIVING_SIDE_RIGHT);
        int[] right = ManeuverMapper.map(maneuverType, 35,
            ManeuverMapper.JUNCTION_SINGLE_INTERSECTION,
            ManeuverMapper.DRIVING_SIDE_LEFT);
        int[] fallbackRight = ManeuverMapper.map(maneuverType, 1000,
            ManeuverMapper.JUNCTION_SINGLE_INTERSECTION,
            ManeuverMapper.DRIVING_SIDE_RIGHT);
        int[] fallbackLeft = ManeuverMapper.map(maneuverType, 0,
            ManeuverMapper.JUNCTION_SINGLE_INTERSECTION,
            ManeuverMapper.DRIVING_SIDE_LEFT);
        int[] fallbackNegative = ManeuverMapper.map(maneuverType, -1000,
            ManeuverMapper.JUNCTION_SINGLE_INTERSECTION,
            ManeuverMapper.DRIVING_SIDE_RIGHT);

        int leftDir = ManeuverMapper.DIR_SLIGHT_LEFT;
        int rightDir = ManeuverMapper.DIR_SLIGHT_RIGHT;
        expect("BAP ramp signed left direction", leftDir, left[1]);
        expect("BAP ramp signed right direction", rightDir, right[1]);
        expect("BAP ramp RHT missing direction", rightDir,
            fallbackRight[1]);
        expect("BAP ramp LHT zero direction", leftDir,
            fallbackLeft[1]);
        expect("BAP ramp negative sentinel direction", rightDir,
            fallbackNegative[1]);

        for (int[] result : new int[][]{left, right, fallbackRight, fallbackLeft, fallbackNegative})
            expect("BAP ramp uses single TURN", ManeuverMapper.TURN, result[0]);
    }

    private static void testHudIconSelection() {
        int[] angles = {-1000, -180, -135, -90, -45, -1, 0, 45, 90, 135, 180, 1000};
        int[] ramps = {ManeuverMapper.MT_OFF_RAMP, ManeuverMapper.MT_HIGHWAY_OFF_RAMP_LEFT,
            ManeuverMapper.MT_HIGHWAY_OFF_RAMP_RIGHT};
        for (int mt : ramps) for (int side = 0; side < 2; side++) for (int angle : angles) {
            int[] result = ManeuverMapper.map(mt, angle, 0, side);
            expect("single bend ramp family", ManeuverMapper.TURN, result[0]);
            if (mt == ManeuverMapper.MT_HIGHWAY_OFF_RAMP_LEFT)
                expect("explicit left ramp", ManeuverMapper.DIR_SLIGHT_LEFT, result[1]);
            if (mt == ManeuverMapper.MT_HIGHWAY_OFF_RAMP_RIGHT)
                expect("explicit right ramp", ManeuverMapper.DIR_SLIGHT_RIGHT, result[1]);
            if (result[1] != 32 && result[1] != 224) throw new AssertionError("ramp must be slight");
        }
        int[] generic = {ManeuverMapper.MT_START_ROUTE, ManeuverMapper.MT_EXIT_FERRY,
            ManeuverMapper.MT_CHANGE_HIGHWAY};
        int[] heading = {-181, -1000, -135, -113, -112, -68, -67, -23, -22, -1,
            0, 22, 23, 67, 68, 112, 113, 135, 181, 1000};
        // Protocol expectations: single TURN uses 7 valid direction values;
        // Sentinels must not fabricate a turn; other out-of-range values retain
        // the previous signed left/right fallback.
        int[] expected = {64, 0, 96, 96, 64, 64, 32, 32, 0, 0,
            0, 0, 224, 224, 192, 192, 160, 160, 192, 0};
        for (int mt : generic) for (int i = 0; i < heading.length; i++) {
            int[] result = ManeuverMapper.map(mt, heading[i], 0, 0);
            expect("generic TURN family", ManeuverMapper.TURN, result[0]);
            expect("generic quantized heading " + heading[i], expected[i], result[1]);
        }
        expect("left end-of-road slight", 32, ManeuverMapper.map(20, -45, 0, 0)[1]);
        expect("left end-of-road sharp", 96, ManeuverMapper.map(20, -135, 0, 1)[1]);
        expect("left end-of-road missing", 64, ManeuverMapper.map(20, -1, 0, 0)[1]);
        expect("left type wins conflicting angle", 64, ManeuverMapper.map(20, 45, 0, 0)[1]);
        expect("right end-of-road slight", 224, ManeuverMapper.map(21, 45, 0, 1)[1]);
        expect("right end-of-road sharp", 160, ManeuverMapper.map(21, 135, 0, 0)[1]);
        expect("right end-of-road missing", 192, ManeuverMapper.map(21, 1000, 0, 0)[1]);
        expect("right type wins conflicting angle", 192, ManeuverMapper.map(21, -45, 0, 0)[1]);
    }

    private static void testMissingRoundaboutAngle() {
        expect("BAP roundabout positive sentinel is generic", ManeuverMapper.DIR_STRAIGHT,
            ManeuverMapper.directionFromAngle16(1000));
        expect("BAP roundabout negative sentinel is generic", ManeuverMapper.DIR_STRAIGHT,
            ManeuverMapper.directionFromAngle16(-1000));
        expect("BAP real roundabout U-turn angle preserved", ManeuverMapper.DIR_UTURN,
            ManeuverMapper.directionFromAngle16(180));

        int[] missingExit = ManeuverMapper.map(ManeuverMapper.MT_ROUNDABOUT_EXIT_5,
            1000, ManeuverMapper.JUNCTION_ROUNDABOUT,
            ManeuverMapper.DRIVING_SIDE_RIGHT);
        expect("BAP roundabout mapping keeps roundabout element",
            ManeuverMapper.ROUNDABOUT_TRS_RIGHT, missingExit[0]);
        expect("BAP roundabout mapping does not invent U-turn", ManeuverMapper.DIR_STRAIGHT,
            missingExit[1]);

        expect("renderer typed roundabout U-turn missing angle", 180,
            (mapping(ManeuverMapper.MT_U_TURN_AT_ROUNDABOUT, 1000, 0).exitAngle / 2));
        expect("renderer generic roundabout missing angle", 0,
            (mapping(ManeuverMapper.MT_ROUNDABOUT_EXIT_5, 1000, 0).exitAngle / 2));
        expect("renderer real roundabout angle preserved", -67,
            (mapping(ManeuverMapper.MT_ROUNDABOUT_EXIT_5, -67, 0).exitAngle / 2));
    }

    private static void testAllManeuverSafetySentinels() {
        // Unknown geometry must use the same categorical/traffic-side fallback
        // as zero; neither sentinel's sign is a direction. Cover every type,
        // both traffic sides and every supported/unknown junction category.
        for (int mt = 0; mt <= 53; mt++) {
            for (int side = 0; side < 2; side++) {
                for (int junction = -1; junction <= 1; junction++) {
                    int[] fallback = ManeuverMapper.map(mt, 0, junction, side);
                    for (int angle : new int[]{-1000, 1000, -1}) {
                        int[] actual = ManeuverMapper.map(mt, angle, junction, side);
                        String label = "sentinel mt=" + mt + " angle=" + angle
                            + " side=" + side + " junction=" + junction;
                        expect(label + " main", fallback[0], actual[0]);
                        expect(label + " direction", fallback[1], actual[1]);
                    }
                }
            }
        }
        for (int mt : new int[]{11, 16, 51}) {
            for (int angle : new int[]{Integer.MIN_VALUE, -32768, -1001, -999, -181,
                    181, 999, 1001, 32767, Integer.MAX_VALUE}) {
                expect("legacy out-of-range signed fallback",
                    angle < 0 ? ManeuverMapper.DIR_LEFT : ManeuverMapper.DIR_RIGHT,
                    ManeuverMapper.map(mt, angle, 0, 0)[1]);
            }
        }
    }

    public static void main(String[] args) {
        testType(ManeuverMapper.MT_U_TURN);
        testType(ManeuverMapper.MT_START_ROUTE_WITH_U_TURN);
        testType(ManeuverMapper.MT_U_TURN_WHEN_POSSIBLE);
        testGenericRamp(ManeuverMapper.MT_OFF_RAMP);
        testGenericRamp(ManeuverMapper.MT_ON_RAMP);
        testMissingRoundaboutAngle();
        testHudIconSelection();
        testAllManeuverSafetySentinels();
        System.out.println("RendererMapperDirectionTest: PASS");
    }
}
