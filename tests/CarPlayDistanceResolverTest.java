import com.luka.carplay.rgd.BAPBridge;
import java.lang.reflect.Field;
import java.lang.reflect.Method;

/**
 * Exercises the shipping distance-to-maneuver resolver (BAPBridge.resolveCarPlayDistance /
 * formatDistanceFallback / formatBapDistanceString) directly via reflection. These are the
 * single-resolver helpers that back both the FctID 18 native cluster distance and the VC
 * status text; see BAPBridge.formatDistanceToTurn().
 */
public final class CarPlayDistanceResolverTest {
    private static int checks;
    private static void check(boolean ok, String label) {
        checks++;
        if (!ok) throw new AssertionError(label);
    }
    private static void equal(String label, Object expected, Object actual) {
        checks++;
        if (!expected.equals(actual)) throw new AssertionError(label + ": expected <" + expected + "> got <" + actual + ">");
    }

    private static int constant(String name) throws Exception {
        Field f = BAPBridge.class.getDeclaredField(name);
        f.setAccessible(true);
        return f.getInt(null);
    }

    private static int[] valueUnit(Object formattedDistance) throws Exception {
        Field value = formattedDistance.getClass().getDeclaredField("value");
        Field unit = formattedDistance.getClass().getDeclaredField("unit");
        value.setAccessible(true);
        unit.setAccessible(true);
        return new int[] { value.getInt(formattedDistance), unit.getInt(formattedDistance) };
    }

    public static void main(String[] args) throws Exception {
        int CARPLAY_KM = constant("CARPLAY_DIST_UNIT_KM");
        int CARPLAY_MILES = constant("CARPLAY_DIST_UNIT_MILES");
        int CARPLAY_METERS = constant("CARPLAY_DIST_UNIT_METERS");
        int CARPLAY_FEET = constant("CARPLAY_DIST_UNIT_FEET");
        int BAP_MILES = constant("BAP_DIST_UNIT_MILES");
        int BAP_KM = constant("BAP_DIST_UNIT_KILOMETERS");
        int BAP_METERS = constant("BAP_DIST_UNIT_METERS");
        int BAP_FEET = constant("BAP_DIST_UNIT_FEET");
        int BAP_QUARTER_MILES = constant("BAP_DIST_UNIT_QUARTER_MILES");

        Method resolve = BAPBridge.class.getDeclaredMethod("resolveCarPlayDistance",
            String.class, Integer.TYPE, Boolean.TYPE);
        resolve.setAccessible(true);
        Method fallback = BAPBridge.class.getDeclaredMethod("formatDistanceFallback",
            Integer.TYPE, Boolean.TYPE);
        fallback.setAccessible(true);
        Method bapString = BAPBridge.class.getDeclaredMethod("formatBapDistanceString",
            Integer.TYPE, Integer.TYPE);
        bapString.setAccessible(true);

        // ---- "0.3"/MILES -> "0.3 mi", BAP value 3, used verbatim when systems agree (both imperial) ----
        Object agree = resolve.invoke(null, "0.3", Integer.valueOf(CARPLAY_MILES), Boolean.valueOf(false));
        check(agree != null, "verbatim CarPlay distance resolved when units agree");
        int[] vu = valueUnit(agree);
        equal("verbatim BAP value = number*10", Integer.valueOf(3), Integer.valueOf(vu[0]));
        equal("verbatim BAP unit = miles", Integer.valueOf(BAP_MILES), Integer.valueOf(vu[1]));
        equal("verbatim text", "0.3 mi", bapString.invoke(null, Integer.valueOf(vu[0]), Integer.valueOf(vu[1])));

        // Same idea in metric: "1.2"/KM with a metric car.
        Object agreeKm = resolve.invoke(null, "1.2", Integer.valueOf(CARPLAY_KM), Boolean.valueOf(true));
        check(agreeKm != null, "verbatim CarPlay distance resolved (metric)");
        int[] vuKm = valueUnit(agreeKm);
        equal("verbatim km BAP value", Integer.valueOf(12), Integer.valueOf(vuKm[0]));
        equal("verbatim km BAP unit", Integer.valueOf(BAP_KM), Integer.valueOf(vuKm[1]));

        // ---- Thousands-grouping separators (Gate 2 remediation #1) ----
        // "1,200"/FT -> value 12000 (1200*10), unit feet.
        Object grouped = resolve.invoke(null, "1,200", Integer.valueOf(CARPLAY_FEET), Boolean.valueOf(false));
        check(grouped != null, "\"1,200\" with a valid 3-digit group resolves");
        int[] vuGrouped = valueUnit(grouped);
        equal("\"1,200\" BAP value", Integer.valueOf(12000), Integer.valueOf(vuGrouped[0]));
        equal("\"1,200\" BAP unit", Integer.valueOf(BAP_FEET), Integer.valueOf(vuGrouped[1]));

        // "12,500" -> 12500 as well (multi-group).
        Object grouped2 = resolve.invoke(null, "12,500", Integer.valueOf(CARPLAY_FEET), Boolean.valueOf(false));
        check(grouped2 != null, "\"12,500\" resolves");
        equal("\"12,500\" BAP value", Integer.valueOf(125000), Integer.valueOf(valueUnit(grouped2)[0]));

        // A decimal comma ("0,3") is NOT a thousands group (only 1 digit follows) -> hard failure,
        // resolver returns null so the caller falls back to formatting from meters.
        Object decimalComma = resolve.invoke(null, "0,3", Integer.valueOf(CARPLAY_MILES), Boolean.valueOf(false));
        check(decimalComma == null, "\"0,3\" decimal comma must fail to parse, not truncate to 0");

        Object decimalComma2 = resolve.invoke(null, "1,2", Integer.valueOf(CARPLAY_MILES), Boolean.valueOf(false));
        check(decimalComma2 == null, "\"1,2\" decimal comma must fail to parse, not truncate to 1");

        // NBSP-style thousands grouping ("1 200" using U+00A0) -> 1200.
        Object nbspGrouped = resolve.invoke(null, "1 200", Integer.valueOf(CARPLAY_FEET), Boolean.valueOf(false));
        check(nbspGrouped != null, "NBSP-grouped \"1 200\" resolves");
        equal("NBSP-grouped BAP value", Integer.valueOf(12000), Integer.valueOf(valueUnit(nbspGrouped)[0]));

        // ---- Fallback when CarPlay units disagree with the car's system ----
        Object disagree = resolve.invoke(null, "0.3", Integer.valueOf(CARPLAY_MILES), Boolean.valueOf(true));
        check(disagree == null, "CarPlay imperial vs metric car falls back to null (caller formats from meters)");

        Object disagreeMetric = resolve.invoke(null, "300", Integer.valueOf(CARPLAY_METERS), Boolean.valueOf(false));
        check(disagreeMetric == null, "CarPlay metric vs imperial car falls back to null");

        // ---- Fallback when the string is missing/blank/unparseable ----
        check(resolve.invoke(null, null, Integer.valueOf(CARPLAY_MILES), Boolean.valueOf(false)) == null,
            "null CarPlay string falls back");
        check(resolve.invoke(null, "", Integer.valueOf(CARPLAY_MILES), Boolean.valueOf(false)) == null,
            "empty CarPlay string falls back");
        check(resolve.invoke(null, "mi", Integer.valueOf(CARPLAY_MILES), Boolean.valueOf(false)) == null,
            "unparseable CarPlay string (no leading digits) falls back");

        // formatDistanceFallback must still produce a sane value from meters in that case.
        Object fell = fallback.invoke(null, Integer.valueOf(966), Boolean.valueOf(false)); // ~0.6 mi
        int[] vuFell = valueUnit(fell);
        equal("fallback unit is miles for an imperial car past the feet cutoff",
            Integer.valueOf(BAP_MILES), Integer.valueOf(vuFell[1]));

        // ---- No fraction output for 0.25/0.5/0.75 mi: the removed BAP_DIST_UNIT_QUARTER_MILES
        // ("1/4 mi"/"1/2 mi"/"3/4 mi") text branch must never resurface, even defensively when a
        // caller (Audi's own formatter, still used for distance-to-destination) hands unit=5 back. ----
        String quarter = (String) bapString.invoke(null, Integer.valueOf(10), Integer.valueOf(BAP_QUARTER_MILES)); // 1 quarter mile = 0.25mi
        String half     = (String) bapString.invoke(null, Integer.valueOf(20), Integer.valueOf(BAP_QUARTER_MILES)); // 2 quarter miles = 0.5mi
        String threeQtr = (String) bapString.invoke(null, Integer.valueOf(30), Integer.valueOf(BAP_QUARTER_MILES)); // 3 quarter miles = 0.75mi
        for (String s : new String[] {quarter, half, threeQtr}) {
            check(s.indexOf('/') < 0, "no '/' fraction character in: " + s);
            check(s.indexOf("1/4") < 0 && s.indexOf("1/2") < 0 && s.indexOf("3/4") < 0,
                "no Audi fraction text in: " + s);
            check(s.endsWith(" mi"), "still a decimal-mile string: " + s);
        }

        // Realistic meters equivalents of 0.25/0.5/0.75 mi through the actual fallback formatter
        // (never routes through BAPDistanceFormatter's quarter-mile unit at all).
        int[] quarterMileMeters = { 402, 805, 1207 };
        for (int m : quarterMileMeters) {
            Object fd = fallback.invoke(null, Integer.valueOf(m), Boolean.valueOf(false));
            int[] v = valueUnit(fd);
            check(v[1] != BAP_QUARTER_MILES, "fallback never selects the quarter-mile unit for " + m + " m");
            String text = (String) bapString.invoke(null, Integer.valueOf(v[0]), Integer.valueOf(v[1]));
            check(text.indexOf('/') < 0, "no fraction text for " + m + " m: " + text);
        }

        System.out.println("CarPlayDistanceResolverTest: PASS (" + checks + " checks)");
    }
}
