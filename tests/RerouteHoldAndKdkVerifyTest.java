import com.luka.carplay.core.ScreenModule;
import com.luka.carplay.framework.Log;
import com.luka.carplay.rgd.BAPBridge;
import com.luka.carplay.rgd.RendererServer;
import com.luka.carplay.rgd.RouteGuidance;
import de.audi.atip.interapp.combi.bap.navi.CombiBAPServiceNavi;

import java.lang.reflect.Field;
import java.lang.reflect.InvocationHandler;
import java.lang.reflect.Method;
import java.lang.reflect.Proxy;

/**
 * Field bug: black KDK pill after rapid reroutes.  BAPBridge opened the presentation and ~350 ms
 * later, on the reroute pulse, ran the approach-EXIT close; the Virtual Cockpit never got its
 * Fct44 hide and kept the pill up while we were back in stock ctx 74.
 *
 * Covers:
 *  1. rapid reroutes inside the hold: no close, no open, no renderer CLEAR, the NEW arrow is sent;
 *  2. hold expiry with NO further RGI update: the BAPTimer thread closes;
 *  3. genuine route end (count 0, route_state <= 0) during a hold closes immediately;
 *  4. KDK still visible after a close: up to 3 re-closes, none once it turns invisible, none
 *     after a re-open;
 *  5. arrow fill: 0 % FILLED when the pill opens (city and long-step city turn), 100 % FILLED at
 *     the 76.2 m blink start, linear in between.  Renderer fill = 1 - level/16.
 */
public final class RerouteHoldAndKdkVerifyTest {

    private static int checks = 0;

    private static void check(boolean ok, String label) {
        checks++;
        if (!ok) throw new AssertionError("CHECK FAILED: " + label);
    }

    private static final class MockRenderer extends RendererServer {
        volatile int clears = 0;
        volatile int maneuvers = 0;
        volatile int lastIcon = -1;
        volatile int lastExitAngle = 0;
        volatile int maneuverLevel = -1;   /* level carried by the last CMD_MANEUVER */
        volatile int progressLevel = -1;   /* level carried by the last CMD_PROGRESS */

        public boolean connect() { return true; }
        public boolean isReady() { return true; }
        public boolean isFrameReady() { return true; }
        public boolean sendClear() { clears++; return true; }
        public boolean sendVisibleArea(int x, int y, int w, int h) { return true; }
        public boolean sendProgress(int level, int mode, int progressState) {
            progressLevel = level;
            return true;
        }
        public boolean sendBapProgressManeuver(int icon, int direction, int exitAngle,
                int drivingSide, int[] junctionAngles, int level, int mode, int perspective,
                boolean refresh, boolean snapToRoad, int progressState) {
            maneuvers++;
            lastIcon = icon;
            lastExitAngle = exitAngle;
            maneuverLevel = level;
            return true;
        }
        public boolean sendManeuver(int icon, int direction, int exitAngle,
                int drivingSide, int[] junctionAngles, int level, int mode, int perspective) {
            maneuvers++;
            lastIcon = icon;
            lastExitAngle = exitAngle;
            return true;
        }
    }

    /** Counts RGStatus(1) opens and RGStatus(0) closes reaching the BAP service. */
    private static final class BapProxyHandler implements InvocationHandler {
        volatile int opens = 0;
        volatile int closes = 0;
        public Object invoke(Object proxy, Method method, Object[] args) {
            if ("updateRGStatus".equals(method.getName()) && args != null && args.length == 1) {
                if (((Integer) args[0]).intValue() == 1) opens++;
                else closes++;
            }
            Class type = method.getReturnType();
            if (type == Boolean.TYPE) return Boolean.FALSE;
            if (type == Integer.TYPE) return new Integer(0);
            return null;
        }
    }

    /** BAPBridge whose Fct44 KDK visibility is scripted. */
    private static final class KdkBridge extends BAPBridge {
        volatile boolean kdkVisible = true;
        protected boolean isKdkVisibleNow() { return kdkVisible; }
    }

    private static void setField(Class clazz, Object target, String name, Object val) throws Exception {
        Field f = clazz.getDeclaredField(name);
        f.setAccessible(true);
        f.set(target, val);
    }

    private static Object getField(Class clazz, Object target, String name) throws Exception {
        Field f = clazz.getDeclaredField(name);
        f.setAccessible(true);
        return f.get(target);
    }

    private static de.audi.tghu.navi.app.cluster.ClusterService createClusterService() throws Exception {
        Field uf = sun.misc.Unsafe.class.getDeclaredField("theUnsafe");
        uf.setAccessible(true);
        sun.misc.Unsafe unsafe = (sun.misc.Unsafe) uf.get(null);
        return (de.audi.tghu.navi.app.cluster.ClusterService)
            unsafe.allocateInstance(de.audi.tghu.navi.app.cluster.ClusterService.class);
    }

    private static BAPBridge prepare(BAPBridge bridge, MockRenderer renderer, BapProxyHandler bap,
                                     int holdMs, int kdkMs) throws Exception {
        CombiBAPServiceNavi service = (CombiBAPServiceNavi) Proxy.newProxyInstance(
            RerouteHoldAndKdkVerifyTest.class.getClassLoader(),
            new Class[]{CombiBAPServiceNavi.class}, bap);
        setField(BAPBridge.class, bridge, "appConnectorNavi", service);
        setField(BAPBridge.class, bridge, "initialized", Boolean.TRUE);
        setField(BAPBridge.class, bridge, "bapSessionStarted", Boolean.TRUE);
        setField(BAPBridge.class, bridge, "nativeStopAttempted", Boolean.TRUE);
        setField(BAPBridge.class, bridge, "csRef", createClusterService());
        setField(BAPBridge.class, bridge, "rendererClient", renderer);
        setField(BAPBridge.class, bridge, "customRendererStarted", Boolean.TRUE);
        if (holdMs >= 0) setField(BAPBridge.class, bridge, "rerouteHoldMs", Integer.valueOf(holdMs));
        if (kdkMs >= 0) setField(BAPBridge.class, bridge, "kdkVerifyMs", Integer.valueOf(kdkMs));
        ScreenModule.setRouteActive(true);
        ScreenModule.setPresentationActive(false);
        return bridge;
    }

    private static RouteGuidance.State maneuverState(long gen, int routeState, long stateGen,
                                                     int mType, int angle, int distM, int ver,
                                                     int stepM) {
        RouteGuidance.State s = new RouteGuidance.State();
        s.routeGeneration = gen;
        s.routeState = routeState;
        s.routeStateGeneration = stateGen;
        s.maneuverCount = 1;
        s.maneuverOrder = new int[]{0};
        s.mType[0] = mType;
        s.mTurnAngle[0] = angle;
        s.mTurnAnglePresent[0] = (angle != -1);
        s.mJunctionType[0] = 0;
        s.mDistance[0] = stepM;
        s.mVer[0] = ver;
        s.distManeuverM = distM;
        return s;
    }

    private static RouteGuidance.State maneuverState(long gen, int routeState, long stateGen,
                                                     int mType, int angle, int distM, int ver) {
        return maneuverState(gen, routeState, stateGen, mType, angle, distM, ver, distM);
    }

    private static RouteGuidance.State endState(long gen) {
        RouteGuidance.State s = new RouteGuidance.State();
        s.routeGeneration = gen;
        s.routeState = 0;
        s.routeStateGeneration = -1L;
        s.maneuverCount = 0;
        s.maneuverOrder = new int[0];
        s.distManeuverM = -1;
        s.dirtyMask = RouteGuidance.State.DIRTY_ROUTE_STATE | RouteGuidance.State.DIRTY_MANEUVER_COUNT;
        return s;
    }

    private static final int ROUTE_STATE_ONLY = RouteGuidance.State.DIRTY_ROUTE_STATE;

    private interface Condition { boolean ok(); }

    private static boolean waitFor(Condition c, long timeoutMs) throws Exception {
        long end = System.currentTimeMillis() + timeoutMs;
        while (System.currentTimeMillis() < end) {
            if (c.ok()) return true;
            Thread.sleep(10);
        }
        return c.ok();
    }

    private static boolean holdActive(BAPBridge b) throws Exception {
        return ((Boolean) getField(BAPBridge.class, b, "rerouteHoldActive")).booleanValue();
    }

    /** Establish an open presentation inside the city zone (200 m left turn). */
    private static void openInZone(BAPBridge bridge, long gen) {
        RouteGuidance.State s = maneuverState(gen, 1, gen, 1, -90, 200, 1);
        s.markAllDirtyForReplay();
        bridge.update(s);
    }

    public static void main(String[] args) throws Exception {
        Log.setLevel(-1);
        Field cnf = ScreenModule.class.getDeclaredField("connected");
        cnf.setAccessible(true);
        cnf.setBoolean(null, true);

        // the shipping constants
        Field hold = BAPBridge.class.getDeclaredField("REROUTE_HOLD_MS");
        hold.setAccessible(true);
        check(hold.getInt(null) == 2500, "REROUTE_HOLD_MS is 2500");

        // ============================================================
        // 1. Rapid reroutes inside the hold: no close / open / CLEAR; NEW arrow sent.
        // ============================================================
        {
            MockRenderer renderer = new MockRenderer();
            BapProxyHandler bap = new BapProxyHandler();
            BAPBridge bridge = prepare(new BAPBridge(), renderer, bap, -1, -1);

            openInZone(bridge, 900L);
            check(ScreenModule.isPresentationActive(), "1.0: presentation open");
            check(bap.opens == 1 && bap.closes == 0, "1.0: one open, no close (opens=" + bap.opens
                + " closes=" + bap.closes + ")");
            check(renderer.maneuvers == 1 && renderer.lastExitAngle < 0, "1.0: left turn painted");

            for (int i = 0; i < 3; i++) {
                RouteGuidance.State r = maneuverState(900L, 5, 900L, 1, -90, 200, 1);
                r.dirtyMask = ROUTE_STATE_ONLY;
                bridge.update(r);
                check(holdActive(bridge), "1." + i + ": hold active during reroute");
                check(ScreenModule.isPresentationActive(), "1." + i + ": presentation held open");
                check(bap.closes == 0, "1." + i + ": no BAP close during hold");
                check(renderer.clears == 0, "1." + i + ": renderer CLEAR deferred");
                if (i < 2) {
                    RouteGuidance.State a = maneuverState(900L, 1, 900L, 1, -90, 200, 1);
                    a.dirtyMask = ROUTE_STATE_ONLY;
                    int before = renderer.maneuvers;
                    bridge.update(a);
                    check(!holdActive(bridge), "1." + i + ": hold ends when the route returns");
                    check(renderer.maneuvers == before + 1,
                        "1." + i + ": identical returned arrow is still resent");
                }
            }
            // Last reroute returns with a DIFFERENT next maneuver (right turn, new ver).
            RouteGuidance.State a3 = maneuverState(900L, 1, 900L, 2, 90, 180, 2);
            a3.dirtyMask = RouteGuidance.State.DIRTY_ROUTE_STATE
                | RouteGuidance.State.DIRTY_MANEUVER_ICON | RouteGuidance.State.DIRTY_DIST_MAN;
            bridge.update(a3);
            check(renderer.lastExitAngle > 0, "1.3: the NEW (right turn) arrow was sent");
            check(ScreenModule.isPresentationActive(), "1.3: presentation open");
            check(bap.opens == 1 && bap.closes == 0,
                "1.3: never closed or re-opened (opens=" + bap.opens + " closes=" + bap.closes + ")");
            check(renderer.clears == 0, "1.3: renderer never cleared");
            bridge.onStop();
        }

        // ============================================================
        // 2. Hold expiry with no further RGI update: the timer closes.
        // ============================================================
        {
            final MockRenderer renderer = new MockRenderer();
            final BapProxyHandler bap = new BapProxyHandler();
            final BAPBridge bridge = prepare(new BAPBridge(), renderer, bap, 200, 60000);

            openInZone(bridge, 910L);
            RouteGuidance.State r = maneuverState(910L, 5, 910L, 1, -90, 200, 1);
            r.dirtyMask = ROUTE_STATE_ONLY;
            bridge.update(r);
            check(holdActive(bridge) && ScreenModule.isPresentationActive(), "2.1: held open");
            check(bap.closes == 0, "2.1: not closed inside the hold");

            // No further update() call at all.
            boolean closed = waitFor(new Condition() {
                public boolean ok() { return !ScreenModule.isPresentationActive(); }
            }, 5000);
            check(closed, "2.2: hold expiry closed the presentation without any RGI update");
            check(!holdActive(bridge), "2.2: hold cleared");
            check(bap.closes >= 1, "2.2: BAP RGStatus(0) issued");
            check(renderer.clears == 1, "2.2: deferred renderer CLEAR now sent");

            // Route comes back later: normal approach ENTER re-opens.
            RouteGuidance.State back = maneuverState(910L, 1, 910L, 1, -90, 200, 1);
            back.markAllDirtyForReplay();
            bridge.update(back);
            check(ScreenModule.isPresentationActive() && bap.opens == 2, "2.3: re-opened by normal ENTER");
            bridge.onStop();
        }

        // ============================================================
        // 3. Genuine route end during a hold closes immediately (no hold).
        // ============================================================
        {
            MockRenderer renderer = new MockRenderer();
            BapProxyHandler bap = new BapProxyHandler();
            BAPBridge bridge = prepare(new BAPBridge(), renderer, bap, -1, 60000);

            openInZone(bridge, 920L);
            RouteGuidance.State r = maneuverState(920L, 5, 920L, 1, -90, 200, 1);
            r.dirtyMask = ROUTE_STATE_ONLY;
            bridge.update(r);
            check(holdActive(bridge) && bap.closes == 0, "3.1: hold active");

            bridge.update(endState(920L));
            check(!holdActive(bridge), "3.2: hold cancelled by route end");
            check(!ScreenModule.isPresentationActive(), "3.2: presentation closed immediately");
            check(bap.closes >= 1 && renderer.clears >= 1, "3.2: BAP close and renderer CLEAR issued at once");

            // A genuine end with NO preceding reroute never holds either.
            MockRenderer renderer2 = new MockRenderer();
            BapProxyHandler bap2 = new BapProxyHandler();
            BAPBridge bridge2 = prepare(new BAPBridge(), renderer2, bap2, -1, 60000);
            openInZone(bridge2, 921L);
            bridge2.update(endState(921L));
            check(!holdActive(bridge2) && !ScreenModule.isPresentationActive(), "3.3: plain end closes");
            bridge.onStop();
            bridge2.onStop();
        }

        // ============================================================
        // 4. KDK still visible after a close: <= 3 re-closes; none once invisible / re-opened.
        // ============================================================
        {
            // 4a. stays visible forever: exactly 3 retries, then give up.
            MockRenderer renderer = new MockRenderer();
            final BapProxyHandler bap = new BapProxyHandler();
            KdkBridge bridge = (KdkBridge) prepare(new KdkBridge(), renderer, bap, -1, 80);
            openInZone(bridge, 930L);
            RouteGuidance.State far = maneuverState(930L, 1, 930L, 1, -90, 900, 1);  // zone EXIT
            far.dirtyMask = RouteGuidance.State.DIRTY_DIST_MAN;
            bridge.update(far);
            check(!ScreenModule.isPresentationActive(), "4a.0: approach EXIT closed");
            check(bap.closes == 1, "4a.0: one close so far (closes=" + bap.closes + ")");
            boolean three = waitFor(new Condition() {
                public boolean ok() { return bap.closes >= 4; }
            }, 5000);
            check(three, "4a.1: KDK visible -> the close was re-issued (closes=" + bap.closes + ")");
            Thread.sleep(500);
            check(bap.closes == 4, "4a.2: exactly 3 retries then give up (closes=" + bap.closes + ")");
            bridge.onStop();

            // 4b. becomes invisible after the first retry: no more re-closes.
            final BapProxyHandler bap2 = new BapProxyHandler();
            final KdkBridge bridge2 = (KdkBridge) prepare(new KdkBridge(), new MockRenderer(), bap2, -1, 80);
            openInZone(bridge2, 931L);
            RouteGuidance.State far2 = maneuverState(931L, 1, 931L, 1, -90, 900, 1);
            far2.dirtyMask = RouteGuidance.State.DIRTY_DIST_MAN;
            bridge2.update(far2);
            check(waitFor(new Condition() {
                public boolean ok() { return bap2.closes >= 2; }
            }, 5000), "4b.1: first retry issued");
            bridge2.kdkVisible = false;
            Thread.sleep(500);
            check(bap2.closes == 2, "4b.2: no re-close once KDK is hidden (closes=" + bap2.closes + ")");
            bridge2.onStop();

            // 4c. KDK already hidden at the check: never re-closed.
            final BapProxyHandler bap3 = new BapProxyHandler();
            final KdkBridge bridge3 = (KdkBridge) prepare(new KdkBridge(), new MockRenderer(), bap3, -1, 80);
            bridge3.kdkVisible = false;
            openInZone(bridge3, 932L);
            RouteGuidance.State far3 = maneuverState(932L, 1, 932L, 1, -90, 900, 1);
            far3.dirtyMask = RouteGuidance.State.DIRTY_DIST_MAN;
            bridge3.update(far3);
            Thread.sleep(500);
            check(bap3.closes == 1, "4c: hidden KDK is not re-closed (closes=" + bap3.closes + ")");
            bridge3.onStop();

            // 4d. re-opened before the check: pending retries are cancelled.
            final BapProxyHandler bap4 = new BapProxyHandler();
            final KdkBridge bridge4 = (KdkBridge) prepare(new KdkBridge(), new MockRenderer(), bap4, -1, 300);
            openInZone(bridge4, 933L);
            RouteGuidance.State far4 = maneuverState(933L, 1, 933L, 1, -90, 900, 1);
            far4.dirtyMask = RouteGuidance.State.DIRTY_DIST_MAN;
            bridge4.update(far4);
            RouteGuidance.State near4 = maneuverState(933L, 1, 933L, 1, -90, 200, 1);
            near4.markAllDirtyForReplay();
            bridge4.update(near4);
            check(ScreenModule.isPresentationActive(), "4d.0: re-opened");
            Thread.sleep(1000);
            check(bap4.closes == 1, "4d: retries cancelled by the re-open (closes=" + bap4.closes + ")");
            bridge4.onStop();
        }

        // ============================================================
        // 5. Arrow fill: 0 % filled at open, 100 % filled at 76.2 m, linear.
        //    Renderer fill = 1 - level/16 (level = remaining, 16 = far).
        // ============================================================
        {
            // City turn whose STEP is long (3000 m): used to pick the 1600 m highway denominator
            // while the pill opened at 457 m -> arrow ~80 % filled the moment it appeared.
            MockRenderer renderer = new MockRenderer();
            BapProxyHandler bap = new BapProxyHandler();
            BAPBridge bridge = prepare(new BAPBridge(), renderer, bap, -1, 60000);

            RouteGuidance.State open = maneuverState(940L, 1, 940L, 1, -90, 457, 1, 3000);
            open.markAllDirtyForReplay();
            bridge.update(open);
            check(ScreenModule.isPresentationActive(), "5.1: pill opens at 457 m");
            check(renderer.maneuverLevel == 16,
                "5.1: level at open is 16 -> 0% FILLED (level=" + renderer.maneuverLevel + ")");

            RouteGuidance.State mid = maneuverState(940L, 1, 940L, 1, -90, 267, 1, 3000);
            mid.dirtyMask = RouteGuidance.State.DIRTY_DIST_MAN;
            bridge.update(mid);
            check(renderer.progressLevel == 8,
                "5.2: halfway (267 m) is 50% FILLED, level 8 (level=" + renderer.progressLevel + ")");

            RouteGuidance.State full = maneuverState(940L, 1, 940L, 1, -90, 77, 1, 3000);
            full.dirtyMask = RouteGuidance.State.DIRTY_DIST_MAN;
            bridge.update(full);
            check(renderer.progressLevel == 0,
                "5.3: at the blink start the arrow is 100% FILLED, level 0 (level="
                + renderer.progressLevel + ")");
            bridge.onStop();

            // Highway ramp: same shape against the 1600 m denominator.
            Method pct = BAPBridge.class.getDeclaredMethod("bargraphPercent", Integer.TYPE, Integer.TYPE);
            pct.setAccessible(true);
            check(((Integer) pct.invoke(null, new Integer(1600), new Integer(1600))).intValue() == 100,
                "5.4: 1600 m highway opens at 100% remaining (0% filled)");
            check(((Integer) pct.invoke(null, new Integer(1650), new Integer(1600))).intValue() == 100,
                "5.4: hysteresis-open beyond the denominator clamps to 0% filled");
            check(((Integer) pct.invoke(null, new Integer(76), new Integer(1600))).intValue() == 0,
                "5.4: 76 m (blink zone) is 100% filled");
            check(((Integer) pct.invoke(null, new Integer(91), new Integer(457))).intValue() > 0,
                "5.4: 91.44 m is no longer the full point");

            // Opened slightly beyond the denominator through hysteresis is still 0 % filled.
            MockRenderer renderer2 = new MockRenderer();
            BAPBridge bridge2 = prepare(new BAPBridge(), renderer2, new BapProxyHandler(), -1, 60000);
            RouteGuidance.State in = maneuverState(941L, 1, 941L, 1, -90, 400, 1, 3000);
            in.markAllDirtyForReplay();
            bridge2.update(in);
            RouteGuidance.State beyond = maneuverState(941L, 1, 941L, 1, -90, 500, 1, 3000);
            beyond.dirtyMask = RouteGuidance.State.DIRTY_DIST_MAN;
            bridge2.update(beyond);   // 500 m: inside the 507 m hysteresis window, past the 457 denominator
            check(ScreenModule.isPresentationActive(), "5.5: still open at 500 m (hysteresis)");
            check(renderer2.maneuverLevel != 0, "5.5: arrow not full while beyond the denominator");
            bridge2.onStop();
        }

        // ============================================================
        // 6. Hold expiry racing a re-open: whichever wins, BAP and ScreenModule must agree (open).
        // ============================================================
        for (int i = 0; i < 40; i++) {
            MockRenderer renderer = new MockRenderer();
            BapProxyHandler bap = new BapProxyHandler();
            BAPBridge bridge = prepare(new BAPBridge(), renderer, bap, 100, 60000);
            openInZone(bridge, 960L + i);
            RouteGuidance.State r = maneuverState(960L + i, 5, 960L + i, 1, -90, 200, 1);
            r.dirtyMask = ROUTE_STATE_ONLY;
            bridge.update(r);
            Thread.sleep(80 + (i % 25));   // straddle the 100 ms deadline
            RouteGuidance.State back = maneuverState(960L + i, 1, 960L + i, 1, -90, 200, 1);
            back.markAllDirtyForReplay();
            bridge.update(back);
            Thread.sleep(150);             // let a late timer fire
            boolean bapOpen = ((Boolean) getField(BAPBridge.class, bridge, "bapPresentationActive")).booleanValue();
            check(ScreenModule.isPresentationActive() && bapOpen,
                "6." + i + ": presentation open and BAP open after expiry/re-open race (screen="
                + ScreenModule.isPresentationActive() + " bap=" + bapOpen + ")");
            check(!holdActive(bridge), "6." + i + ": hold cleared");
            bridge.onStop();
        }

        System.out.println("RerouteHoldAndKdkVerifyTest: PASS (" + checks + " checks)");
    }
}
