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
        int main, direction, descriptors, distance, bar, distanceFailures, maneuverStateFailures;
        boolean barOn;
        public Object invoke(Object proxy, Method method, Object[] args) {
            if (method.getName().equals("updateManeuverState") && maneuverStateFailures > 0) {
                maneuverStateFailures--;
                throw new IllegalStateException("injected maneuver-state delivery failure");
            }
            if (method.getName().equals("updateManeuverDescriptor")) {
                CombiBAPNaviManeuverDescriptor[] data = (CombiBAPNaviManeuverDescriptor[]) args[0];
                main = data[0].mainElement; direction = data[0].direction; descriptors++;
            }
            if (method.getName().equals("updateDistanceToNextManeuver")) {
                if (distanceFailures > 0) {
                    distanceFailures--;
                    throw new IllegalStateException("injected distance delivery failure");
                }
                distance = ((Integer) args[0]).intValue();
                barOn = ((Boolean) args[2]).booleanValue();
                bar = ((Integer) args[3]).intValue();
            }
            return null;
        }
    }
    private static final class Renderer extends RendererServer {
        int icon = -1, direction, angle, writes, failures, progressLevel, progressMode, progressState;
        int progressOnFollow;
        public boolean isReady() { return true; }
        public boolean isFrameReady() { return true; }
        public boolean isConnected() { return true; }
        public boolean everConnected() { return true; }
        public boolean sendVisibleArea(int x, int y, int w, int h) { return true; }
        boolean sendLaneGuidance(LaneGuidanceSnapshot lanes) { return true; }
        public boolean sendProgress(int level, int mode, int state) {
            if (icon == RendererMapper.ICON_APPROACH && mode != 0) progressOnFollow++;
            progressLevel = level; progressMode = mode; progressState = state; return true;
        }
        public boolean sendBapProgressManeuver(int nextIcon, int nextDirection, int nextAngle,
                int side, int[] roads, int level, int mode, int perspective,
                boolean refresh, boolean snap, int progress) {
            if (failures > 0) { failures--; return false; }
            progressLevel = level; progressMode = mode; progressState = progress;
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
        if (mode != 2 && seen[0] && seen[1] && seen[2]) {
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
    private static void neutral(Fixture f, String why) {
        check(f.hud.main == ManeuverMapper.FOLLOW_STREET
            && f.hud.direction == ManeuverMapper.DIR_STRAIGHT
            && f.renderer.icon == RendererMapper.ICON_APPROACH && f.renderer.angle == 0, why);
    }
    private static void distanceOwnership() throws Exception {
        String[] replacements = {
            "maneuver_list:s:1\nm1_ver:n:2\n", // current payload has not arrived
            "m0_type:n:54\n", // same slot becomes unsupported
            "maneuver_list:s:1\nm1_ver:n:2\nm1_type:n:1\nm1_junction_type:n:0\n",
            "m0_ver:n:2\nm0_type:n:1\nm0_junction_type:n:0\n",
            "route_generation:n:302\nroute_state:n:1\nmaneuver_count:n:2\n"
                + "maneuver_list:s:0\nm0_ver:n:1\nm0_type:n:1\nm0_junction_type:n:0\n"
        };
        for (int i = 0; i < replacements.length; i++) for (int failure = 0; failure < 3; failure++) {
            Fixture f = new Fixture();
            String label = "distance ownership case=" + i + " failure=" + failure;
            try {
                f.feed("route_generation:n:301\nroute_state:n:1\nmaneuver_count:n:2\n"
                    + "maneuver_list:s:0\nm0_ver:n:1\nm0_type:n:2\nm0_junction_type:n:0\n"
                    + "dist_maneuver_m:n:100\n");
                check(f.hud.distance > 0 && f.hud.barOn && f.hud.bar > 0
                    && f.renderer.progressMode == 1, label + ": initial progress missing");
                int distance = f.hud.distance, bar = f.hud.bar;
                f.feed("dist_maneuver_m:n:0\n");
                check(f.hud.distance == distance && f.hud.barOn && f.hud.bar == bar,
                    label + ": transient distance gap within the same maneuver lost HUD cache");
                check(f.renderer.progressMode == 1 && f.renderer.progressLevel == bar * 16 / 100
                    && f.renderer.progressState == RendererServer.PROGRESS_FILL,
                    label + ": transient distance gap lost shared renderer progress");
                // Geometry refresh and cached reconnect replay must use that same HUD phase.
                f.feed("m0_junction_angles:s:-90,0,90\n");
                check(f.renderer.progressMode == 1 && f.renderer.progressLevel == bar * 16 / 100,
                    label + ": geometry packet replaced cached progress with OFF");
                f.replay();
                check(f.renderer.progressMode == 1 && f.renderer.progressLevel == bar * 16 / 100,
                    label + ": cached replay replaced HUD progress");
                f.hud.distanceFailures = failure == 1 ? 1 : 0;
                f.renderer.failures = failure == 2 ? 1 : 0;
                f.feed(replacements[i] + "dist_maneuver_m:n:0\n");
                if (failure != 0) {
                    check(f.state.dirtyMask != 0, label + ": failed delivery lost retry");
                    f.replay();
                }
                if (i < 2) neutral(f, label + ": unavailable maneuver must be neutral");
                else check(f.hud.main == ManeuverMapper.TURN && f.renderer.icon == RendererMapper.ICON_TURN,
                    label + ": known replacement lost its turn");
                check(f.hud.distance <= 0 && !f.hud.barOn && f.hud.bar == 0,
                    label + ": HUD inherited previous maneuver distance/progress");
                check(f.renderer.progressMode == 0 && f.renderer.progressLevel == 0
                    && f.renderer.progressState == RendererServer.PROGRESS_OFF,
                    label + ": renderer retained previous maneuver progress");
                check(f.state.dirtyMask == 0, label + ": retry did not converge");
                if (i == 0) {
                    f.feed("dist_maneuver_m:n:100\n");
                    check(f.hud.distance == distance && !f.hud.barOn && f.renderer.progressMode == 0,
                        label + ": neutral guidance must retain a fresh distance without progress");
                    f.feed("m1_type:n:1\nm1_junction_type:n:0\n");
                    check(f.hud.barOn && f.renderer.progressMode == 1
                        && f.renderer.progressLevel == f.hud.bar * 16 / 100,
                        label + ": late maneuver data did not restore shared progress");
                }
            } finally { f.close(); }
        }
    }
    private static void presentationLifecycle() throws Exception {
        Fixture f = new Fixture();
        try {
            f.feed("route_generation:n:201\nroute_state:n:1\n");
            neutral(f, "active route without maneuver data must be neutral");
            f.feed("m1_type:n:1\nm1_turn_angle:n:-90\nm1_junction_type:n:0\nmaneuver_list:s:0,1\n");
            neutral(f, "missing current maneuver must not borrow the prefetched left turn");
            for (int type : new int[]{54,255}) {
                f.feed("m0_type:n:" + type + "\n");
                neutral(f, "unknown received type must remain neutral");
                check(f.state.mType[0] == type, "presentation overwrote raw unknown type");
            }
            f.feed("m0_type:n:2\n");
            check(f.hud.main == ManeuverMapper.TURN && f.hud.direction == ManeuverMapper.DIR_RIGHT
                && f.renderer.icon == RendererMapper.ICON_TURN && f.renderer.angle == 180,
                "known right turn must not wait for junction metadata");
            f.feed("m0_junction_type:n:1\n");
            neutral(f, "incompatible junction must remain neutral");
            f.feed("m0_junction_type:n:0\nm0_turn_angle:n:90\n");
            check(f.state.maneuverCount == 0 && f.hud.main == ManeuverMapper.TURN
                && f.renderer.icon == RendererMapper.ICON_TURN && f.renderer.angle == 180,
                "usable current maneuver must not wait for total count");
            f.renderer.failures = 1;
            f.feed("m0_ver:n:2\nm0_type:n:54\n");
            check(f.state.dirtyMask != 0 && f.renderer.icon == RendererMapper.ICON_TURN,
                "failed neutral send must retain pending publication");
            f.replay();
            neutral(f, "unknown replacement must withdraw old turn");
            check(f.state.dirtyMask == 0, "neutral retry did not converge");
            f.feed("route_generation:n:202\nroute_state:n:5\n");
            neutral(f, "rerouting without new data must not retain old turn");
            f.feed("maneuver_list:s:0\nm0_type:n:28\nm0_junction_type:n:0\n");
            neutral(f, "neutral roundabout mismatch must not regain roundabout geometry");
            f.feed("m0_junction_type:n:1\nm0_turn_angle:n:90\n");
            check(f.renderer.icon == RendererMapper.ICON_ROUNDABOUT,
                "late junction geometry must restore actual roundabout");
            f.feed("route_state:n:0\n");
            check(f.hud.main == ManeuverMapper.NO_SYMBOL
                && !field(BAPBridge.class,"bapSessionStarted").getBoolean(f.bridge),
                "route end must clear guidance instead of emitting neutral arrow");
            int writes = f.renderer.writes;
            f.feed("current_road:s:After route\n");
            check(f.renderer.writes == writes, "inactive metadata resurrected guidance");
        } finally { f.close(); }
    }
    private static void blinkGapPhases() throws Exception {
        Method tick = BAPBridge.class.getDeclaredMethod("sendActionBlinkTick", Integer.TYPE);
        tick.setAccessible(true);
        for (int ticks = 1; ticks <= 2; ticks++) {
            Fixture f = new Fixture();
            try {
                // Hold the same lock as the blink worker so each tested phase
                // is deterministic, without sleeping or racing its 600 ms tick.
                synchronized (f.bridge) {
                    f.feed("route_generation:n:401\nroute_state:n:1\nmaneuver_count:n:1\n"
                        + "maneuver_list:s:0\nm0_type:n:2\nm0_junction_type:n:0\ndist_maneuver_m:n:10\n");
                    int generation = field(BAPBridge.class, "actionBlinkGeneration").getInt(f.bridge);
                    for (int i = 0; i < ticks; i++) tick.invoke(f.bridge, Integer.valueOf(generation));
                    int phase = ticks == 1 ? RendererServer.PROGRESS_BLINK_HIGH : RendererServer.PROGRESS_BLINK_LOW;
                    int bar = f.hud.bar;
                    check(f.hud.barOn && bar == (ticks == 1 ? 100 : 0)
                        && f.renderer.progressState == phase, "blink fixture phase missing");
                    f.feed("dist_maneuver_m:n:0\n");
                    f.renderer.failures = 1;
                    f.feed("m0_junction_angles:s:-90,0,90\n");
                    check(f.state.dirtyMask != 0, "blink geometry enqueue failure lost retry");
                    f.replay();
                    check(f.hud.barOn && f.hud.bar == bar && f.renderer.progressMode == 1
                        && f.renderer.progressLevel == bar * 16 / 100 && f.renderer.progressState == phase,
                        "distance gap/geometry retry changed the HUD blink phase");
                    check(f.state.dirtyMask == 0, "blink geometry retry did not converge");
                }
            } finally { f.close(); }
        }
    }
    private static void missingJunction() throws Exception {
        Fixture f = new Fixture();
        try {
            f.feed("route_generation:n:501\nroute_state:n:1\nmaneuver_count:n:1\nmaneuver_list:s:0\n");
            for (int type : new int[]{1,2}) {
                int direction = type == 1 ? ManeuverMapper.DIR_LEFT : ManeuverMapper.DIR_RIGHT;
                int angle = type == 1 ? -180 : 180;
                f.feed("m0_full:n:1\nm0_ver:n:" + type + "\nm0_type:n:" + type
                    + "\ndist_maneuver_m:n:100\n");
                check(f.state.mJunctionType[0] == -1 && !f.state.mTurnAnglePresent[0],
                    "mapping must preserve absent junction and angle in source state");
                check(f.hud.main == ManeuverMapper.TURN && f.hud.direction == direction
                    && f.renderer.icon == RendererMapper.ICON_TURN && f.renderer.angle == angle,
                    "type-only maneuver must reach both HUD and renderer");
                check(f.hud.barOn && f.hud.bar > 0 && f.renderer.progressMode == 1
                    && f.renderer.progressLevel == f.hud.bar * 16 / 100,
                    "type-only maneuver must retain shared approach progress");
                for (int junction : new int[]{1,2,255}) {
                    f.feed("m0_junction_type:n:" + junction + "\n");
                    neutral(f, "explicit incompatible/unknown junction must not use the default");
                }
                f.feed("m0_junction_type:n:0\n");
                check(f.hud.main == ManeuverMapper.TURN && f.hud.direction == direction
                    && f.renderer.angle == angle, "late ordinary junction must restore the same turn");
            }
            for (int type : new int[]{-1,54,255}) {
                f.feed("m0_full:n:1\nm0_type:n:" + type + "\n");
                neutral(f, "missing junction must not make an unknown maneuver concrete");
            }
            for (int type : new int[]{7,28,46}) {
                check(ManeuverMapper.map(type,90,-1,0,true)[0] == ManeuverMapper.NO_INFO,
                    "ordinary default must not fabricate roundabout metadata");
                check(ManeuverMapper.map(type,90,1,0,true)[0] != ManeuverMapper.NO_INFO,
                    "explicit roundabout exit must remain supported");
            }
        } finally { f.close(); }
    }
    private static void presentationMatrix() {
        RouteGuidance.State s = new RouteGuidance.State();
        s.maneuverOrder = new int[]{0,1}; s.mType[1] = ManeuverMapper.MT_LEFT_TURN;
        for (int type = -1; type <= 255; type++) for (int junction : new int[]{-1,0,1,2,255}) {
            s.mType[0] = type; s.mJunctionType[0] = junction;
            s.mTurnAngle[0] = 90; s.mTurnAnglePresent[0] = true;
            int[] raw = ManeuverMapper.map(type,90,junction,0,true);
            ManeuverPresentation p = ManeuverPresentation.resolve(s,true);
            check(p != ManeuverPresentation.NONE, "active instruction must not be blank");
            check(ManeuverPresentation.resolve(s,false) == ManeuverPresentation.NONE,
                "inactive stale maneuver must not resurrect guidance");
            if (ManeuverMapper.isValidType(type) && raw[0] != ManeuverMapper.NO_INFO
                    && raw[0] != ManeuverMapper.NO_SYMBOL) {
                check(p.slot == 0 && p.main == raw[0] && p.direction == raw[1],
                    "supported mapping changed");
            } else {
                check(p == ManeuverPresentation.FOLLOW, "unavailable instruction must be neutral");
            }
            check(s.mType[0] == type && s.mJunctionType[0] == junction,
                "selection overwrote source metadata");
        }
    }
    private static void farStraightTransition() throws Exception {
        Class<?> settings = com.luka.carplay.connectivity.CarPlaySettingsUi.class;
        Field far = field(settings, "currentRgiFar");
        int saved = far.getInt(null);
        far.setInt(null, com.luka.carplay.connectivity.CarPlaySettingsUi.RGI_FAR_STRAIGHT);
        try {
            for (int failure = 0; failure < 2; failure++) {
                Fixture f = new Fixture();
                try {
                    f.feed("route_generation:n:401\nroute_state:n:1\nmaneuver_count:n:1\n"
                        + "maneuver_list:s:0\nm0_ver:n:1\nm0_type:n:2\nm0_junction_type:n:0\n"
                        + "dist_maneuver_m:n:5000\n");
                    check(f.renderer.icon == RendererMapper.ICON_APPROACH && f.renderer.progressMode == 0,
                        "far-straight fixture must start with an unfilled follow icon");
                    f.hud.maneuverStateFailures = failure;
                    f.feed("dist_maneuver_m:n:100\n");
                    check(f.renderer.progressOnFollow == 0,
                        "approach progress reached the old follow icon before maneuver geometry");
                    if (failure != 0) {
                        check(f.renderer.icon == RendererMapper.ICON_APPROACH && f.renderer.progressMode == 0,
                            "failed HUD update changed the old renderer's progress");
                        check((f.state.dirtyMask & RouteGuidance.State.DIRTY_MANEUVER_ICON) != 0,
                            "synthetic approach icon refresh was lost on failure");
                        f.feed("dist_maneuver_m:n:90\n");
                    }
                    check(f.renderer.icon == 2 && f.renderer.progressMode == 1,
                        "approach update/retry did not converge to the turn with progress");
                    check(f.state.dirtyMask == 0 && !field(BAPBridge.class, "rendererManeuverPending").getBoolean(f.bridge),
                        "successful approach geometry remained pending");
                    int writes = f.renderer.writes;
                    f.replay();
                    check(f.renderer.writes == writes, "accepted approach geometry restarted on replay");
                } finally { f.close(); }
            }
        } finally { far.setInt(null, saved); }
    }
    public static void main(String[] args) throws Exception {
        Log.setLevel(-1);
        missingJunction();
        farStraightTransition();
        distanceOwnership();
        blinkGapPhases();
        presentationMatrix();
        presentationLifecycle();
        List<int[]> permutations = new ArrayList<int[]>();
        permute(new int[]{0, 1, 2, 3, 4, 5}, 0, permutations);
        check(permutations.size() == 720, "six distinct fields must have 720 arrival orders");
        for (int mode = 0; mode < 3; mode++) for (int[] order : permutations) run(order, mode);
        System.out.println("ManeuverFieldArrivalTest: PASS (720 orders x 3 modes = " + cases + " cases, " + checks
            + " checks; initial route, reused-slot reroute, failed enqueue/replay, stale deltas and look-ahead)");
    }
}
