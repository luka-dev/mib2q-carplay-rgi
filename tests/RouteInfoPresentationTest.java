import com.luka.carplay.core.ScreenModule;
import com.luka.carplay.rgd.BAPBridge;
import com.luka.carplay.rgd.RouteGuidance;
import de.audi.atip.interapp.combi.bap.navi.CombiBAPServiceNavi;
import java.lang.reflect.Field;
import java.lang.reflect.InvocationHandler;
import java.lang.reflect.Method;
import java.lang.reflect.Proxy;
import java.util.TimeZone;

/** Exercises the shipping BAPBridge, not a copy of the presentation algorithm. */
public final class RouteInfoPresentationTest {
    private static int checks;

    private static final class Output implements InvocationHandler {
        String position, turnTo, signPost;
        int positionCalls, turnCalls, timeCalls, timeType;
        public Object invoke(Object proxy, Method method, Object[] args) {
            String name = method.getName();
            if (name.equals("updateCurrentPositionInfo")) {
                position = (String) args[0];
                positionCalls++;
            } else if (name.equals("updateTurnToInfo")) {
                turnTo = (String) args[0];
                signPost = (String) args[1];
                turnCalls++;
            } else if (name.equals("updateTimeToDestination")) {
                timeType = ((Integer) args[0]).intValue();
                timeCalls++;
            } else {
                Class type = method.getReturnType();
                if (type == Boolean.TYPE) return Boolean.FALSE;
                if (type == Integer.TYPE) return Integer.valueOf(0);
                return null;
            }
            return null;
        }
    }

    private static void set(Class owner, Object target, String name, Object value)
            throws Exception {
        Field field = owner.getDeclaredField(name);
        field.setAccessible(true);
        field.set(target, value);
    }

    private static void equal(String label, Object expected, Object actual) {
        checks++;
        if (!expected.equals(actual))
            throw new AssertionError(label + ": expected <" + expected + ">, got <" + actual + ">");
    }

    private static void check(String label, boolean ok) {
        checks++;
        if (!ok) throw new AssertionError(label);
    }

    private static String repeat(String value, int count) {
        StringBuilder result = new StringBuilder();
        for (int i = 0; i < count; i++) result.append(value);
        return result.toString();
    }

    private static RouteGuidance.State route() {
        RouteGuidance.State state = new RouteGuidance.State();
        state.maneuverOrder = new int[] {0};
        state.mType[0] = 1;
        state.mAfterRoad[0] = "Road: Main Street";
        state.mName[0] = "Maneuver name";
        state.mExitInfo[0] = "Exit 12: Main Street";
        state.currentRoad = "Current Road";
        return state;
    }

    private static Output render(BAPBridge bridge, Output output,
            RouteGuidance.State state, boolean small, int phase) throws Exception {
        set(ScreenModule.class, null, "smallScreenViewArea", Boolean.valueOf(small)); // target: boolean flag
        output.position = output.turnTo = output.signPost = "STALE";
        output.positionCalls = output.turnCalls = output.timeCalls = 0;
        bridge.refreshInfoPresentation(state, phase);
        equal("one position publish", Integer.valueOf(1), Integer.valueOf(output.positionCalls));
        equal("clear separate text layer", Integer.valueOf(1), Integer.valueOf(output.turnCalls));
        equal("TurnTo stays empty", "", output.turnTo);
        equal("SignPost stays empty", "", output.signPost);
        equal("native ETA is republished", Integer.valueOf(1), Integer.valueOf(output.timeCalls));
        equal("native ETA never switches to duration", Integer.valueOf(1), Integer.valueOf(output.timeType));
        check("position never empty during RGI", output.position.length() > 0);
        check("UTF-8 wire budget", output.position.getBytes("UTF-8").length <= 96);
        for (int i = 0; i < output.position.length(); i++) {
            char c = output.position.charAt(i);
            if (Character.isHighSurrogate(c)) {
                check("complete supplementary character", i + 1 < output.position.length()
                    && Character.isLowSurrogate(output.position.charAt(++i)));
            } else check("no orphan low surrogate", !Character.isLowSurrogate(c));
        }
        return output;
    }

    public static void main(String[] args) throws Exception {
        TimeZone.setDefault(TimeZone.getTimeZone("UTC"));
        Output output = new Output();
        BAPBridge bridge = new BAPBridge();
        CombiBAPServiceNavi service = (CombiBAPServiceNavi) Proxy.newProxyInstance(
            RouteInfoPresentationTest.class.getClassLoader(),
            new Class[] {CombiBAPServiceNavi.class}, output);
        set(BAPBridge.class, bridge, "appConnectorNavi", service);
        set(BAPBridge.class, bridge, "initialized", Boolean.TRUE);
        set(BAPBridge.class, bridge, "bapSessionStarted", Boolean.TRUE);

        for (int view = 0; view < 2; view++) {
            boolean small = view == 1;
            RouteGuidance.State state = route();
            equal("signpost priority, no road stripping", "Exit 12: Main Street",
                render(bridge, output, state, small, 0).position);
            equal("unknown ETA retains navigation", "Exit 12: Main Street",
                render(bridge, output, state, small, 1).position);
            state.mExitInfo[0] = " \t\n";
            equal("next road fallback", "\u2191 Main Street", render(bridge, output, state, small, 0).position);
            state.mAfterRoad[0] = repeat("\u0416", 70);
            String wide = render(bridge, output, state, small, 0).position;
            check("wide Cyrillic fits pixels before wire limit", wide.startsWith("\u2191 ")
                && wide.length() > 3 && wide.length() < 20);
            state.mAfterRoad[0] = " \t";
            equal("maneuver name fallback", "\u2191 Maneuver name", render(bridge, output, state, small, 0).position);
            state.mName[0] = null;
            equal("current road fallback", "Current Road", render(bridge, output, state, small, 0).position);
            state.currentRoad = null;
            equal("pending, not empty or fake arrow", "\u2026", render(bridge, output, state, small, 0).position);
            equal("unknown ETA and road", "\u2026", render(bridge, output, state, small, 1).position);
            state = route();
            state.maneuverOrder = new int[0];
            equal("empty list ignores stale maneuver slots", "Current Road",
                render(bridge, output, state, small, 0).position);
            state = route();
            state.mExitInfo[0] = "  Exit\n12\t Main  Street  ";
            equal("transport whitespace", "Exit 12 Main Street", render(bridge, output, state, small, 0).position);
            String[] longSigns = {repeat("x", 120), repeat("\u0416", 70),
                repeat("\u9053", 31), repeat("x", 89) + "\uD83D\uDE97z",
                repeat("x", 86) + "\uD83D\uDE97z"};
            for (int i = 0; i < longSigns.length; i++) {
                state.mExitInfo[0] = longSigns[i];
                String frame = render(bridge, output, state, small, 0).position;
                check("overflow is fitted in pixels, not only bytes", frame.length() < longSigns[i].length());
            }
            state.mExitInfo[0] = "Cafe\u0301";
            equal("composed accent reaches the firmware font", "Caf\u00E9",
                render(bridge, output, state, small, 0).position);
        }

        RouteGuidance.State state = route();
        set(BAPBridge.class, bridge, "lastEtaSeconds", Long.valueOf(System.currentTimeMillis() / 1000L + 2220));
        equal("small dotted-circle time", "\u25CC 37 min", render(bridge, output, state, true, 1).position);
        byte[] wire = output.position.getBytes("UTF-8");
        check("exact U+25CC wire prefix", (wire[0] & 255) == 0xE2
            && (wire[1] & 255) == 0x97 && (wire[2] & 255) == 0x8C && wire[3] == 0x20);
        String full = render(bridge, output, state, false, 1).position;
        check("full marked arrival and duration in one field", full.startsWith("\u25CC ") && full.endsWith(" | 37 min"));
        check("full marker appears only once", full.indexOf('\u25CC', 1) < 0);
        equal("next click restores navigation", "Exit 12: Main Street",
            render(bridge, output, state, false, 0).position);
        set(BAPBridge.class, bridge, "lastEtaSeconds", Long.valueOf(-1));
        set(BAPBridge.class, bridge, "lastTimeRemainingSeconds", Long.valueOf(3900));
        set(BAPBridge.class, bridge, "lastTimeRemainingSampleSeconds", Long.valueOf(System.currentTimeMillis() / 1000L));
        equal("duration-only input and long-trip format", "\u25CC 1 h 05 min",
            render(bridge, output, state, true, 1).position);
        full = render(bridge, output, state, false, 1).position;
        check("duration-only input produces arrival too", full.startsWith("\u25CC ") && full.endsWith(" | 1 h 05 min"));
        set(BAPBridge.class, bridge, "lastEtaSeconds", Long.valueOf(1));
        equal("past ETA clamps at zero", "\u25CC 0 min", render(bridge, output, state, true, 1).position);

        // ============================================================
        // Test context rebind decision on reroute / route_generation change:
        // ============================================================
        Field rpf = ScreenModule.class.getDeclaredField("rebindPending");
        rpf.setAccessible(true);
        ScreenModule.setRouteActive(true);

        // Case 1: Initial baseline route at gen 300, 200m approach -> enter approach zone (ctx 80 active)
        ScreenModule.setPresentationActive(true);
        rpf.setBoolean(null, false);
        RouteGuidance.State baseline = route();
        baseline.routeGeneration = 300L;
        baseline.routeState = 1;
        baseline.routeStateGeneration = 300L;
        baseline.maneuverCount = 1;
        baseline.maneuverOrder = new int[]{0};
        baseline.mType[0] = 1;
        baseline.distManeuverM = 200;
        baseline.markAllDirtyForReplay();
        bridge.update(baseline);
        rpf.setBoolean(null, false); // clear baseline latch

        // Case 2: Reroute occurs -> new generation 301, city turn at 660 m (~0.4 mi > 457 m)
        // With ctx 80 currently active, must NOT trigger context rebind!
        RouteGuidance.State rerouteFar = route();
        rerouteFar.routeGeneration = 301L;
        rerouteFar.routeState = 1;
        rerouteFar.routeStateGeneration = 301L;
        rerouteFar.maneuverCount = 1;
        rerouteFar.maneuverOrder = new int[]{0};
        rerouteFar.mType[0] = 1;
        rerouteFar.distManeuverM = 660;
        rerouteFar.markAllDirtyForReplay();
        boolean res = bridge.update(rerouteFar);
        check("new generation + 660m city turn (>457m) must NOT rebind context 80",
            !rpf.getBoolean(null));
        check("new generation + 660m city turn must exit approach zone (navActive=false)",
            !ScreenModule.isNavActive());

        // Case 3: Reroute occurs -> new generation 302, city turn at 200 m (<= 457 m)
        // With ctx 80 currently active, MUST trigger forced 72->80 context rebind!
        ScreenModule.setPresentationActive(true);
        rpf.setBoolean(null, false);
        RouteGuidance.State rerouteNear = route();
        rerouteNear.routeGeneration = 302L;
        rerouteNear.routeState = 1;
        rerouteNear.routeStateGeneration = 302L;
        rerouteNear.maneuverCount = 1;
        rerouteNear.maneuverOrder = new int[]{0};
        rerouteNear.mType[0] = 1;
        rerouteNear.distManeuverM = 200;
        rerouteNear.markAllDirtyForReplay();
        bridge.update(rerouteNear);
        check("new generation + 200m city turn (<=457m) with active ctx 80 MUST trigger rebind",
            rpf.getBoolean(null));
        Field rrf = ScreenModule.class.getDeclaredField("rebindReason");
        rrf.setAccessible(true);
        String reason = (String) rrf.get(null);
        check("rebind reason records route generation and approach",
            reason != null && reason.indexOf("route-generation=302") >= 0);

        // Case 4: Reroute occurs -> new generation 303, city turn at 200 m, but ctx 74 active (navActive=false)
        // Must NOT rebind (normal 74->80 switch path handles it)
        ScreenModule.setPresentationActive(false);
        rpf.setBoolean(null, false);
        RouteGuidance.State rerouteStock = route();
        rerouteStock.routeGeneration = 303L;
        rerouteStock.routeState = 1;
        rerouteStock.routeStateGeneration = 303L;
        rerouteStock.maneuverCount = 1;
        rerouteStock.maneuverOrder = new int[]{0};
        rerouteStock.mType[0] = 1;
        rerouteStock.distManeuverM = 200;
        rerouteStock.markAllDirtyForReplay();
        bridge.update(rerouteStock);
        check("new generation with inactive ctx 74 does not rebind",
            !rpf.getBoolean(null));

        // Case 5: Intermediate delta without maneuvers while awaiting maneuver for new generation
        com.luka.carplay.cluster.ClusterLayerController.onVcVisibility(true);
        ScreenModule.setPresentationActive(true);
        rpf.setBoolean(null, false);
        RouteGuidance.State genOnly = route();
        genOnly.routeGeneration = 304L;
        genOnly.routeState = 1;
        genOnly.routeStateGeneration = 304L;
        genOnly.maneuverCount = 0;
        genOnly.maneuverOrder = new int[0];
        genOnly.markAllDirtyForReplay();
        bridge.update(genOnly);
        check("intermediate delta without maneuvers does not prematurely rebind",
            !rpf.getBoolean(null));
        check("navActive drops to 74 while awaiting maneuver for new generation", !ScreenModule.isNavActive());

        // Subsequent maneuver arrival for generation 304 at 150m executes the pending rebind
        RouteGuidance.State genWithManeuver = route();
        genWithManeuver.routeGeneration = 304L;
        genWithManeuver.routeState = 1;
        genWithManeuver.routeStateGeneration = 304L;
        genWithManeuver.maneuverCount = 1;
        genWithManeuver.maneuverOrder = new int[]{0};
        genWithManeuver.mType[0] = 1;
        genWithManeuver.distManeuverM = 150;
        genWithManeuver.markAllDirtyForReplay();
        bridge.update(genWithManeuver);
        check("subsequent maneuver arrival for generation 304 executes the pending rebind",
            rpf.getBoolean(null));
        com.luka.carplay.cluster.ClusterLayerController.onVcVisibility(false);

        // Case 6: Genuine route end holds ctx 80 while KDK visible until Fct44 withdrawal
        ScreenModule.setNavActive(true);
        com.luka.carplay.cluster.ClusterLayerController.onVcVisibility(true);
        ScreenModule.setRouteActive(false);
        check("route end latches navHidePending while KDK visible", ScreenModule.isNavActive());
        ScreenModule.onVcKdkVisibility(false);
        check("navActive drops to 74 after KDK withdrawal", !ScreenModule.isNavActive());
        com.luka.carplay.cluster.ClusterLayerController.onVcVisibility(false);

        // Case 7: Distance bucketing in FctID 19 and maneuver-restart cache invalidation
        // Verify absence of wrapper characters (<, >, \u2039, \u203a)
        ScreenModule.setRouteActive(true);
        set(BAPBridge.class, bridge, "infoPhase", Integer.valueOf(0));
        output.positionCalls = 0;
        RouteGuidance.State bucketState = route();
        bucketState.routeGeneration = 400L;
        bucketState.routeState = 1;
        bucketState.routeStateGeneration = 400L;
        bucketState.maneuverCount = 1;
        bucketState.maneuverOrder = new int[]{0};
        bucketState.mType[0] = 1;
        bucketState.mExitInfo[0] = null;
        bucketState.mAfterRoad[0] = "Main St";
        bucketState.mName[0] = null;
        bucketState.distManeuverM = 200;
        bucketState.markAllDirtyForReplay();
        bridge.update(bucketState);
        check("Case 7: initial publish on maneuver feed", output.positionCalls >= 1);
        check("Case 7: no wrapper < in position", output.position.indexOf('<') == -1);
        check("Case 7: no wrapper > in position", output.position.indexOf('>') == -1);
        check("Case 7: no wrapper \u2039 in position", output.position.indexOf('\u2039') == -1);
        check("Case 7: no wrapper \u203a in position", output.position.indexOf('\u203a') == -1);
        check("Case 7: maneuver distance formatted in position", output.position.indexOf("200 m") >= 0);

        int callsBefore = output.positionCalls;
        // Tick with distance staying in same bucket (200m -> 200m, only DIRTY_DIST_MAN)
        bucketState.dirtyMask = RouteGuidance.State.DIRTY_DIST_MAN;
        bucketState.distManeuverM = 200;
        bridge.update(bucketState);
        check("Case 7: distance in same bucket must NOT republish FctID 19", output.positionCalls == callsBefore);

        // Tick with distance crossing bucket threshold (e.g. 150m)
        bucketState.dirtyMask = RouteGuidance.State.DIRTY_DIST_MAN;
        bucketState.distManeuverM = 150;
        bridge.update(bucketState);
        check("Case 7: distance crossing bucket threshold MUST republish FctID 19", output.positionCalls == callsBefore + 1);
        check("Case 7: updated bucket distance in position", output.position.indexOf("150 m") >= 0);
        check("Case 7: no wrapper chars in updated position",
            output.position.indexOf('<') == -1 && output.position.indexOf('>') == -1 &&
            output.position.indexOf('\u2039') == -1 && output.position.indexOf('\u203a') == -1);

        // Maneuver identity change (mVer changes from 0 to 1) with distance still 150m
        callsBefore = output.positionCalls;
        bucketState.dirtyMask = RouteGuidance.State.DIRTY_MANEUVER_LIST | RouteGuidance.State.DIRTY_MANEUVER_TEXT;
        bucketState.mVer[0] = 1;
        bridge.update(bucketState);
        check("Case 7: maneuver restart invalidates cache and republishes FctID 19 even with same distance",
            output.positionCalls == callsBefore + 1);
        check("Case 7: no wrapper chars after maneuver restart",
            output.position.indexOf('<') == -1 && output.position.indexOf('>') == -1 &&
            output.position.indexOf('\u2039') == -1 && output.position.indexOf('\u203a') == -1);

        System.out.println("RouteInfoPresentationTest: PASS (" + checks + " checks)");
    }
}
