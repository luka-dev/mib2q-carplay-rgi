import com.luka.carplay.rgd.BAPBridge;
import de.audi.atip.interapp.combi.bap.navi.CombiBAPServiceNavi;
import java.lang.reflect.Field;
import java.lang.reflect.InvocationHandler;
import java.lang.reflect.Method;
import java.lang.reflect.Proxy;

/**
 * Exercises the shipping isInBlinkZone(int,int) consolidation directly, plus the actual
 * sendActionBlinkTick call site, to prove the new BLINK_MAX_DISTANCE_CM (76.2 m / 250 ft)
 * absolute cap now gates blinking in addition to the pre-existing BARGRAPH_BLINK_PERCENT
 * percent-of-denominator rule -- fixing the bug where a long highway denominator (1600 m)
 * let the bargraph flash as far out as ~1050 ft.
 */
public final class BlinkZoneAbsoluteCapTest {
    private static int checks;
    private static void check(boolean ok, String label) {
        checks++;
        if (!ok) throw new AssertionError(label);
    }

    private static final class Output implements InvocationHandler {
        int value = Integer.MIN_VALUE, unit = Integer.MIN_VALUE, bar = -1;
        boolean on;
        int calls;
        public Object invoke(Object proxy, Method method, Object[] args) {
            if (method.getName().equals("updateDistanceToNextManeuver")) {
                value = ((Integer) args[0]).intValue();
                unit = ((Integer) args[1]).intValue();
                on = ((Boolean) args[2]).booleanValue();
                bar = ((Integer) args[3]).intValue();
                calls++;
            }
            Class<?> type = method.getReturnType();
            if (type == Boolean.TYPE) return Boolean.FALSE;
            if (type == Integer.TYPE) return Integer.valueOf(0);
            return null;
        }
    }

    private static void set(Object target, String name, Object value) throws Exception {
        for (Class<?> c = target.getClass(); c != null; c = c.getSuperclass()) {
            try { Field f = c.getDeclaredField(name); f.setAccessible(true); f.set(target, value); return; }
            catch (NoSuchFieldException e) { }
        }
        throw new NoSuchFieldException(name);
    }

    public static void main(String[] args) throws Exception {
        Method isInBlinkZone = BAPBridge.class.getDeclaredMethod("isInBlinkZone", Integer.TYPE, Integer.TYPE);
        isInBlinkZone.setAccessible(true);

        // --- Direct helper checks (the single consolidated source of truth) ---
        // Blink only when 0 < distM and distM*100 < 7620 (76.2 m = 250 ft) and inside the zone.
        // distM is whole meters, so the 76.2 m edge is tested at 76 (blink) and 77 (no blink).
        int[][] cases = {   // dist, denominator, expected blink (1/0)
            {29, 457, 1}, {29, 1600, 1}, {61, 457, 1}, {76, 457, 1}, {76, 1600, 1},   // below 250 ft: blink
            {77, 457, 0}, {77, 1600, 0},                                  // 76.2 m and beyond: solid
            {91, 457, 0}, {92, 457, 0}, {300, 1600, 0},                   // 300 ft zone and far: no blink
            {0, 100, 0}, {10, 0, 0}, {50, 40, 0}                          // invalid / outside zone
        };
        for (int i = 0; i < cases.length; i++) {
            boolean got = ((Boolean) isInBlinkZone.invoke(null, Integer.valueOf(cases[i][0]),
                Integer.valueOf(cases[i][1]))).booleanValue();
            check(got == (cases[i][2] == 1), "isInBlinkZone(" + cases[i][0] + ", " + cases[i][1] + ") expected " + cases[i][2]);
        }

        // Shared bar/level mapping (REMAINING percent, FctID 18 stock semantics; the renderer draws
        // fill = 1 - level/16): 100 % remaining (arrow EMPTY) at the approach-open denominator,
        // linear, 0 % remaining (arrow completely FULL) at 76.2 m where blinking starts.  There is
        // no 91.44 m full point any more.  Beyond the denominator (hysteresis) clamps to 100.
        Method percent = BAPBridge.class.getDeclaredMethod("bargraphPercent", Integer.TYPE, Integer.TYPE);
        percent.setAccessible(true);
        int[][] pcts = { {1, 457, 0}, {76, 457, 0}, {77, 457, 0}, {457, 457, 100}, {507, 457, 100},
                         {1600, 1600, 100}, {1650, 1600, 100}, {0, 457, 0},
                         {267, 457, 50},      // (26700-7620)*100/(45700-7620) = 50
                         {838, 1600, 50} };   // (83800-7620)*100/(160000-7620) = 49.99 -> 49
        for (int i = 0; i < pcts.length; i++) {
            int got = ((Integer) percent.invoke(null, Integer.valueOf(pcts[i][0]), Integer.valueOf(pcts[i][1]))).intValue();
            if (pcts[i][2] == 50) check(got >= 49 && got <= 50, "midpoint percent " + got);
            else check(got == pcts[i][2], "bargraphPercent(" + pcts[i][0] + ", " + pcts[i][1] + ") = " + got);
        }
        // 91 m is no longer "full": it is still well above 0 % remaining.
        check(((Integer) percent.invoke(null, Integer.valueOf(91), Integer.valueOf(457))).intValue() > 0,
            "91 m must not be the full point any more");
        int prev = -1;
        for (int d = 1; d <= 457; d++) {
            int got = ((Integer) percent.invoke(null, Integer.valueOf(d), Integer.valueOf(457))).intValue();
            check(got >= prev && got >= 0 && got <= 100, "percent monotone at " + d + " m");
            prev = got;
        }

        // --- Integration check: the actual sendActionBlinkTick call site respects the cap ---
        Output output = new Output();
        BAPBridge bridge = new BAPBridge();
        CombiBAPServiceNavi service = (CombiBAPServiceNavi) Proxy.newProxyInstance(
            BlinkZoneAbsoluteCapTest.class.getClassLoader(),
            new Class[] {CombiBAPServiceNavi.class}, output);
        set(bridge, "appConnectorNavi", service);
        set(bridge, "initialized", Boolean.TRUE);

        Method tick = BAPBridge.class.getDeclaredMethod("sendActionBlinkTick", Integer.TYPE);
        tick.setAccessible(true);

        // 300 m / 1600 m highway case: must NOT send any FctID 18 blink tick.
        set(bridge, "actionBlinkGeneration", Integer.valueOf(1));
        set(bridge, "blinkArmed", Boolean.TRUE);
        set(bridge, "blinkDistM", Integer.valueOf(300));
        set(bridge, "blinkBargraphDenominatorM", Integer.valueOf(1600));
        set(bridge, "actionBlinkFull", Boolean.TRUE);
        check(((Boolean) tick.invoke(bridge, Integer.valueOf(1))).booleanValue(), "worker stays alive when out of blink zone");
        check(output.calls == 0, "300 m/1600 m tick must not send FctID 18 (out of absolute blink cap)");

        // 29 m case: must send a blink tick.
        set(bridge, "blinkDistM", Integer.valueOf(29));
        set(bridge, "blinkBargraphDenominatorM", Integer.valueOf(1600));
        check(((Boolean) tick.invoke(bridge, Integer.valueOf(1))).booleanValue(), "worker stays alive in blink zone");
        check(output.calls == 1, "29 m/1600 m tick must send FctID 18 (inside blink zone)");

        System.out.println("BlinkZoneAbsoluteCapTest: PASS (" + checks + " checks)");
    }
}
