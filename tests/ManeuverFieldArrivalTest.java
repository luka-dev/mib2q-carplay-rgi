package com.luka.carplay.rgd;

import com.luka.carplay.bus.CarplayBus;
import com.luka.carplay.core.AltScreenModule;
import com.luka.carplay.framework.Log;
import de.audi.atip.interapp.combi.bap.navi.CombiBAPServiceNavi;
import de.audi.atip.interapp.combi.bap.navi.data.CombiBAPNaviManeuverDescriptor;
import java.lang.reflect.Field;
import java.lang.reflect.InvocationHandler;
import java.lang.reflect.Method;
import java.lang.reflect.Modifier;
import java.lang.reflect.Proxy;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/** PR #32's six-field permutation concept, through the current listener and real bridge. */
public final class ManeuverFieldArrivalTest {
    private static final String[] DELTAS = {
        "maneuver_count:n:1\n", "maneuver_list:s:0\n", "m0_type:n:2\n",
        "m0_turn_angle:n:67\n", "m0_junction_type:n:0\n", "dist_maneuver_m:n:1000\n"
    };
    private static long checks;
    private static int cases;
    private static void check(boolean ok, String why) {
        checks++; if (!ok) throw new AssertionError(why);
    }
    private static Field field(Class<?> owner, String name) throws Exception {
        Field f = owner.getDeclaredField(name); f.setAccessible(true); return f;
    }
    private static void set(Object target, Class<?> owner, String name, Object value) throws Exception {
        field(owner, name).set(target, value);
    }
    private static final class HUD implements InvocationHandler {
        int direction, descriptors;
        public Object invoke(Object proxy, Method method, Object[] args) {
            if (method.getName().equals("updateManeuverDescriptor")) {
                CombiBAPNaviManeuverDescriptor[] data = (CombiBAPNaviManeuverDescriptor[]) args[0];
                direction = data[0].direction; descriptors++;
            }
            return null;
        }
    }
    private static final class Renderer extends RendererServer {
        int icon = -1, direction, angle, writes, failures;
        public boolean isReady() { return true; }
        public boolean isFrameReady() { return true; }
        public boolean isConnected() { return true; }
        public boolean everConnected() { return true; }
        public boolean sendVisibleArea(int x, int y, int w, int h) { return true; }
        boolean sendLaneGuidance(LaneGuidanceSnapshot lanes) { return true; }
        public boolean sendProgress(int level, int mode, int state) { return true; }
        public boolean sendBapProgressManeuver(int nextIcon, int nextDirection, int nextAngle,
                int side, int[] roads, int level, int mode, int perspective,
                boolean refresh, boolean snap, int progress) {
            if (failures > 0) { failures--; return false; }
            icon = nextIcon; direction = nextDirection; angle = nextAngle; writes++; return true;
        }
    }
    private static final class Bridge extends BAPBridge {
        public boolean preparePresentation() { return true; }
        public boolean isPresentationReady() { return true; }
    }
    private static final class Fixture {
        final RouteGuidance rg = new RouteGuidance();
        final Bridge bridge = new Bridge();
        final HUD hud = new HUD();
        final Renderer renderer = new Renderer();
        final RouteGuidance.State state;
        Fixture() throws Exception {
            set(bridge, BAPBridge.class, "initialized", Boolean.TRUE);
            set(bridge, BAPBridge.class, "bapSessionStarted", Boolean.TRUE);
            set(bridge, BAPBridge.class, "customRendererStarted", Boolean.TRUE);
            set(bridge, BAPBridge.class, "rendererClient", renderer);
            set(bridge, BAPBridge.class, "appConnectorNavi", Proxy.newProxyInstance(
                ManeuverFieldArrivalTest.class.getClassLoader(), new Class[]{CombiBAPServiceNavi.class}, hud));
            set(rg, RouteGuidance.class, "bap", bridge);
            set(rg, RouteGuidance.class, "running", Boolean.TRUE);
            set(rg, RouteGuidance.class, "rgActive", Boolean.TRUE);
            set(rg, RouteGuidance.class, "routeWantsActive", Boolean.TRUE);
            set(rg, RouteGuidance.class, "presentationConfirmed", Boolean.TRUE);
            state = (RouteGuidance.State) field(RouteGuidance.class, "state").get(rg);
            set(null, AltScreenModule.class, "connected", Boolean.TRUE);
            set(null, AltScreenModule.class, "videoAvailable", Boolean.FALSE);
            AltScreenModule.setNavActive(true);
        }
        void feed(String text) throws Exception {
            byte[] bytes = text.getBytes("UTF-8");
            rg.onFrame(CarplayBus.EVT_RGD_UPDATE, 0, bytes, bytes.length);
        }
        void replay() throws Exception {
            Method m = RouteGuidance.class.getDeclaredMethod("driveCachedPresentation");
            m.setAccessible(true); m.invoke(rg);
        }
        void close() throws Exception {
            set(rg, RouteGuidance.class, "running", Boolean.FALSE);
            Thread blink = (Thread) field(BAPBridge.class, "actionBlinkThread").get(bridge);
            bridge.onStop();
            check(blink == null || !blink.isAlive(), "permutation leaked an action-blink worker");
            AltScreenModule.setNavActive(false);
        }
    }
    private static String snapshot(RouteGuidance.State state) throws Exception {
        StringBuilder out = new StringBuilder();
        for (Field f : RouteGuidance.State.class.getFields())
            if (!Modifier.isStatic(f.getModifiers()))
                out.append(f.getName()).append(Arrays.deepToString(new Object[]{f.get(state)}));
        return out.toString();
    }
    private static void verifyPrefix(Fixture f, boolean[] seen, int mode, String label) throws Exception {
        check(f.state.routeGeneration == 101 && f.state.routeState == 1, label + ": route authority");
        check(f.state.maneuverCount == (seen[0] ? 1 : 0), label + ": count");
        check(seen[1] ? Arrays.equals(f.state.maneuverOrder, new int[]{0}) : f.state.maneuverOrder == null, label + ": list");
        check(f.state.mType[0] == (seen[2] ? 2 : -1), label + ": type");
        check(f.state.mTurnAnglePresent[0] == seen[3]
            && f.state.mTurnAngle[0] == (seen[3] ? 67 : -1)
            && f.state.mExitAngle[0] == (seen[3] ? 67 : 1000), label + ": separate angle delta");
        check(f.state.mJunctionType[0] == (seen[4] ? 0 : -1), label + ": separate junction delta");
        check(f.state.distManeuverM == (seen[5] ? (mode == 2 ? 5000 : 1000) : -1), label + ": distance");
        if (f.hud.descriptors > 0)
            check(f.hud.direction != ManeuverMapper.DIR_LEFT, label + ": HUD substituted the prefetched secondary turn");
        if (f.renderer.writes > 0)
            check(f.renderer.icon != 2 || f.renderer.angle >= 0, label + ": renderer substituted the prefetched secondary turn");
        if (mode != 2 && seen[0] && seen[1] && seen[2] && seen[4]) {
            check(f.renderer.writes > 0 && f.renderer.icon == 2 && f.renderer.direction == 0
                && f.renderer.angle == (seen[3] ? 134 : 180), label + ": renderer did not apply latest geometry "
                    + "seen=" + Arrays.toString(seen) + " writes=" + f.renderer.writes + " icon=" + f.renderer.icon
                    + " direction=" + f.renderer.direction + " angle=" + f.renderer.angle);
        }
        check(AltScreenModule.isNavActive()
            && field(AltScreenModule.class, "desiredCtx").getInt(null) == 80, label + ": partial/distant delta withdrew route context");
    }
    private static void run(int[] order, int mode) throws Exception {
        Fixture f = new Fixture();
        String label = "mode=" + mode + " order=" + Arrays.toString(order);
        try {
            if (mode == 1) {
                f.feed("route_generation:n:100\nsource_supports_rg:n:1\nroute_state:n:1\nmaneuver_count:n:1\n"
                    + "maneuver_list:s:0\nm0_ver:n:7\nm0_type:n:1\nm0_turn_angle:n:-90\nm0_junction_type:n:0\n"
                    + "m0_name:s:Old route\ndist_maneuver_m:n:5000\n");
                check(f.renderer.angle == -180, label + ": old-route fixture");
            }
            f.feed("route_generation:n:101\nsource_supports_rg:n:1\nroute_state:n:1\nm0_ver:n:7\n");
            check(f.state.mName[0] == null, label + ": reused slot version inherited old text");
            // Look-ahead is cached, but must never replace the incomplete primary slot.
            f.feed("m1_type:n:1\nm1_turn_angle:n:-90\nm1_name:s:Look-ahead\n");
            f.hud.descriptors = 0; f.renderer.writes = 0;
            f.renderer.failures = mode == 2 ? 1 : 0;
            boolean[] seen = new boolean[DELTAS.length];
            for (int index : order) {
                f.feed(mode == 2 && index == 5 ? "dist_maneuver_m:n:5000\n" : DELTAS[index]); seen[index] = true;
                verifyPrefix(f, seen, mode, label);
                if (mode != 2) {
                    String before = snapshot(f.state); int writes = f.renderer.writes, descriptors = f.hud.descriptors;
                    f.feed("route_generation:n:100\nroute_state:n:0\nsource_supports_rg:n:0\ndisconnect_reason:s:old peer\n"
                        + "m0_type:n:1\nm0_turn_angle:n:-90\ndist_maneuver_m:n:1\n");
                    check(before.equals(snapshot(f.state)) && writes == f.renderer.writes && descriptors == f.hud.descriptors,
                        label + ": stale frame changed state or emitted output");
                }
            }
            f.replay();
            check(f.hud.direction == ManeuverMapper.DIR_RIGHT, label + ": HUD final direction");
            check(f.renderer.writes > 0 && f.renderer.icon == 2 && f.renderer.direction == 0 && f.renderer.angle == 134,
                label + ": final renderer geometry");
            check(f.state.dirtyMask == 0 && !field(BAPBridge.class, "rendererManeuverPending").getBoolean(f.bridge),
                label + ": final delta/retry remained pending");
            int writes = f.renderer.writes;
            f.replay();
            check(f.renderer.writes == writes, label + ": clean replay duplicated maneuver transition");
            cases++;
        } catch (Throwable e) {
            System.err.println("ManeuverFieldArrivalTest FAIL " + label); throw e;
        } finally { f.close(); }
    }
    private static void permute(int[] order, int index, List<int[]> permutations) {
        if (index == order.length) { permutations.add(order.clone()); return; }
        for (int i = index; i < order.length; i++) {
            int value = order[index]; order[index] = order[i]; order[i] = value;
            permute(order, index + 1, permutations);
            value = order[index]; order[index] = order[i]; order[i] = value;
        }
    }
    public static void main(String[] args) throws Exception {
        Log.setLevel(-1);
        List<int[]> permutations = new ArrayList<int[]>();
        permute(new int[]{0, 1, 2, 3, 4, 5}, 0, permutations);
        check(permutations.size() == 720, "six distinct fields must have 720 arrival orders");
        for (int mode = 0; mode < 3; mode++) for (int[] order : permutations) run(order, mode);
        System.out.println("ManeuverFieldArrivalTest: PASS (720 orders x 3 modes = " + cases + " cases, " + checks
            + " checks; initial route, reused-slot reroute, failed enqueue/replay, stale deltas and look-ahead)");
    }
}
