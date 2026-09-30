package com.luka.carplay.rgd;

import com.luka.carplay.core.AltScreenNavStatusGate;
import com.luka.carplay.framework.Log;
import de.audi.atip.interapp.combi.bap.navi.CombiBAPServiceNavi;
import java.lang.reflect.Field;
import java.lang.reflect.InvocationHandler;
import java.lang.reflect.Method;
import java.lang.reflect.Proxy;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/** Real bridge shutdown; a failed BAP write must not suppress any other cleanup. */
public final class BapShutdownFailureTest {
    private static final String[] CALLS = {
        "updateRGStatus", "updateActiveRGType", "updateManeuverDescriptor",
        "updateDistanceToNextManeuver", "updateExitView", "updateManeuverState",
        "updateTurnToInfo", "updateCurrentPositionInfo", "updateDistanceToDestination",
        "updateTimeToDestination", "updateLaneGuidance"
    };
    private static int checks, cases;
    private static void check(boolean ok, String why) {
        checks++; if (!ok) throw new AssertionError(why);
    }
    private static Field field(Class<?> owner, String name) throws Exception {
        Field f = owner.getDeclaredField(name); f.setAccessible(true); return f;
    }
    private static void set(BAPBridge bridge, String name, Object value) throws Exception {
        field(BAPBridge.class, name).set(bridge, value);
    }
    private static final class Output implements InvocationHandler {
        final List<String> calls = new ArrayList<String>();
        int failure;
        boolean linkageError;
        AssertionError validationFailure;
        public Object invoke(Object proxy, Method method, Object[] args) {
            String name = method.getName();
            int index = Arrays.asList(CALLS).indexOf(name);
            try {
                check(index >= 0, "unexpected shutdown call " + name);
                calls.add(name);
                if (index == 0 || index == 1 || index == 5)
                    check(((Integer) args[0]).intValue() == 0, name + " did not withdraw state");
                if (index == 3)
                    check(((Integer) args[0]).intValue() == -1 && !((Boolean) args[2]).booleanValue()
                        && ((Integer) args[3]).intValue() == 0, "maneuver distance/bargraph not cleared");
                if (index == 6) check("".equals(args[0]) && "".equals(args[1]), "turn text not cleared");
                if (index == 7) check("".equals(args[0]), "position text not cleared");
                if (index == 8) check(((Integer) args[0]).intValue() == -1, "destination distance not cleared");
                if (index == 9) check(((Number) args[2]).longValue() == -1L, "destination time not cleared");
                if (index == 10) check(!((Boolean) args[0]).booleanValue()
                    && java.lang.reflect.Array.getLength(args[1]) == 0, "lanes not withdrawn");
            } catch (AssertionError e) { validationFailure = e; throw e; }
            if (failure == index || failure == CALLS.length) {
                if (linkageError) throw new NoSuchMethodError("injected " + name);
                throw new IllegalStateException("injected " + name);
            }
            return null;
        }
    }
    private static final class Renderer extends RendererServer {
        int clears, disposals;
        public boolean sendProgress(int level, int mode, int state) { return true; }
        public boolean sendClear() { clears++; return true; }
        public void dispose() { disposals++; }
    }
    private static void run(int failure, boolean linkageError, boolean preserve, boolean takeover) throws Exception {
        BAPBridge bridge = new BAPBridge(); Renderer renderer = new Renderer(); Output output = new Output();
        output.failure = failure; output.linkageError = linkageError;
        set(bridge, "initialized", Boolean.TRUE);
        set(bridge, "bapSessionStarted", Boolean.TRUE);
        set(bridge, "customRendererStarted", Boolean.TRUE);
        set(bridge, "rendererPrimed", Boolean.TRUE);
        set(bridge, "routeTextPublished", Boolean.TRUE);
        set(bridge, "takeoverEngaged", Boolean.valueOf(takeover));
        set(bridge, "rendererClient", renderer);
        set(bridge, "appConnectorNavi", Proxy.newProxyInstance(BapShutdownFailureTest.class.getClassLoader(),
            new Class[]{CombiBAPServiceNavi.class}, output));
        field(AltScreenNavStatusGate.class, "desiredCurrentPositionBlocked").setBoolean(null, true);
        field(AltScreenNavStatusGate.class, "desiredRouteBlocked").setBoolean(null, true);
        String label = "failure=" + failure + " linkage=" + linkageError + " preserve=" + preserve + " takeover=" + takeover;
        if (preserve) bridge.onRouteEnd(); else bridge.onShutdown();
        check(output.validationFailure == null, label + ": " + output.validationFailure);
        check(output.calls.equals(Arrays.asList(CALLS)), label + ": skipped/reordered BAP cleanup " + output.calls);
        check(!field(BAPBridge.class, "bapSessionStarted").getBoolean(bridge), label + ": session retained");
        check(!field(BAPBridge.class, "customRendererStarted").getBoolean(bridge), label + ": renderer still feeding");
        check(!field(BAPBridge.class, "rendererPrimed").getBoolean(bridge), label + ": stale renderer replay");
        check(!field(BAPBridge.class, "routeTextPublished").getBoolean(bridge), label + ": route text retained");
        check(!field(AltScreenNavStatusGate.class, "desiredCurrentPositionBlocked").getBoolean(null), label + ": text gate retained");
        check(field(AltScreenNavStatusGate.class, "desiredRouteBlocked").getBoolean(null) == takeover, label + ": session gate changed");
        check(renderer.clears == (preserve ? 0 : 1) && renderer.disposals == (preserve ? 0 : 1), label + ": surface/dispose policy");
        check(field(BAPBridge.class, "rendererClient").get(bridge) == (preserve ? renderer : null), label + ": renderer ownership");
        output.calls.clear();
        bridge.onShutdown();
        check(output.validationFailure == null, label + ": repeated " + output.validationFailure);
        check(output.calls.equals(Arrays.asList(CALLS)), label + ": repeated shutdown skipped cleanup");
        check(renderer.disposals == 1, label + ": repeated shutdown leaked or double-disposed renderer");
        cases++;
    }
    public static void main(String[] args) throws Exception {
        Log.setLevel(-1);
        for (int failure = -1; failure <= CALLS.length; failure++)
            for (int error = 0; error < 2; error++) for (int preserve = 0; preserve < 2; preserve++)
                for (int takeover = 0; takeover < 2; takeover++)
                    run(failure, error != 0, preserve != 0, takeover != 0);
        System.out.println("BapShutdownFailureTest: PASS (" + cases + " cases, " + checks
            + " checks; every BAP failure, route/session shutdown, gates and repeated disposal)");
    }
}
