import com.luka.carplay.rgd.ManeuverMapper;
import com.luka.carplay.rgd.RendererMapper;

public final class RendererMapperDirectionTest {
    private static void expect(String name, int expected, int actual) {
        if (expected != actual) {
            throw new AssertionError(name + ": expected " + expected + ", got " + actual);
        }
    }

    private static void testType(int maneuverType) {
        expect("renderer signed left", -1,
            RendererMapper.mapDirection(maneuverType, -180,
                ManeuverMapper.DRIVING_SIDE_LEFT));
        expect("renderer signed right", 1,
            RendererMapper.mapDirection(maneuverType, 180,
                ManeuverMapper.DRIVING_SIDE_RIGHT));
        expect("renderer RHT fallback", -1,
            RendererMapper.mapDirection(maneuverType, 1000,
                ManeuverMapper.DRIVING_SIDE_RIGHT));
        expect("renderer LHT fallback", 1,
            RendererMapper.mapDirection(maneuverType, 1000,
                ManeuverMapper.DRIVING_SIDE_LEFT));
        expect("renderer negative sentinel fallback", -1,
            RendererMapper.mapDirection(maneuverType, -1000,
                ManeuverMapper.DRIVING_SIDE_RIGHT));
        expect("renderer zero fallback", 1,
            RendererMapper.mapDirection(maneuverType, 0,
                ManeuverMapper.DRIVING_SIDE_LEFT));

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
        expect("renderer ramp signed left overrides RHT", -1,
            RendererMapper.mapDirection(maneuverType, -35,
                ManeuverMapper.DRIVING_SIDE_RIGHT));
        expect("renderer ramp signed right overrides LHT", 1,
            RendererMapper.mapDirection(maneuverType, 35,
                ManeuverMapper.DRIVING_SIDE_LEFT));
        expect("renderer ramp RHT missing fallback", 1,
            RendererMapper.mapDirection(maneuverType, 1000,
                ManeuverMapper.DRIVING_SIDE_RIGHT));
        expect("renderer ramp LHT negative sentinel fallback", -1,
            RendererMapper.mapDirection(maneuverType, -1000,
                ManeuverMapper.DRIVING_SIDE_LEFT));
        expect("renderer ramp zero fallback", -1,
            RendererMapper.mapDirection(maneuverType, 0,
                ManeuverMapper.DRIVING_SIDE_LEFT));

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

        expect("BAP ramp signed left direction", ManeuverMapper.DIR_SLIGHT_LEFT, left[1]);
        expect("BAP ramp signed right direction", ManeuverMapper.DIR_SLIGHT_RIGHT, right[1]);
        expect("BAP ramp RHT missing direction", ManeuverMapper.DIR_SLIGHT_RIGHT,
            fallbackRight[1]);
        expect("BAP ramp LHT zero direction", ManeuverMapper.DIR_SLIGHT_LEFT,
            fallbackLeft[1]);
        expect("BAP ramp negative sentinel direction", ManeuverMapper.DIR_SLIGHT_RIGHT,
            fallbackNegative[1]);

        if (maneuverType == ManeuverMapper.MT_OFF_RAMP) {
            expect("BAP off-ramp signed left element", ManeuverMapper.EXIT_LEFT, left[0]);
            expect("BAP off-ramp signed right element", ManeuverMapper.EXIT_RIGHT, right[0]);
            expect("BAP off-ramp RHT missing element", ManeuverMapper.EXIT_RIGHT,
                fallbackRight[0]);
            expect("BAP off-ramp LHT zero element", ManeuverMapper.EXIT_LEFT,
                fallbackLeft[0]);
        } else {
            expect("BAP on-ramp left element", ManeuverMapper.TURN, left[0]);
            expect("BAP on-ramp right element", ManeuverMapper.TURN, right[0]);
        }
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
            RendererMapper.mapExitAngle(ManeuverMapper.MT_U_TURN_AT_ROUNDABOUT, 1000));
        expect("renderer generic roundabout missing angle", 0,
            RendererMapper.mapExitAngle(ManeuverMapper.MT_ROUNDABOUT_EXIT_5, 1000));
        expect("renderer real roundabout angle preserved", -67,
            RendererMapper.mapExitAngle(ManeuverMapper.MT_ROUNDABOUT_EXIT_5, -67));
    }

    public static void main(String[] args) {
        testType(ManeuverMapper.MT_U_TURN);
        testType(ManeuverMapper.MT_START_ROUTE_WITH_U_TURN);
        testType(ManeuverMapper.MT_U_TURN_WHEN_POSSIBLE);
        testGenericRamp(ManeuverMapper.MT_OFF_RAMP);
        testGenericRamp(ManeuverMapper.MT_ON_RAMP);
        testMissingRoundaboutAngle();
        System.out.println("RendererMapperDirectionTest: PASS");
    }
}
