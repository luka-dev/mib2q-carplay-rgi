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
import java.util.ArrayList;
import java.util.List;

/**
 * Regression guard for the reported "black cluster pill after CarPlay reroute"
 * defect (route_state -> REROUTING(5) -> back to active(2) while the maneuver
 * stays inside the approach zone).
 *
 * The suspected root cause (from static code reading, since disproven -- see
 * below) was: BAPBridge.update() clears the renderer (sendClear()) on the
 * REROUTING edge, and when the reroute resolves back to byte-identical slot
 * data (same routeGeneration, same m0_ver), RouteGuidance's field-diff dirty
 * tracking (RouteGuidance.State parsing of m0_ver/m0_type/m0_turn_angle: bits
 * are only marked when a value actually CHANGES) leaves only DIRTY_ROUTE_STATE
 * set. That alone does not satisfy updateRendererIfChanged()'s gate
 * (dirty & (DIRTY_MANEUVER_ICON|DIRTY_MANEUVER_LIST|DIRTY_MANEUVER_COUNT)),
 * so the theory was the renderer would never resend and stay on the cleared
 * (black) surface until the approach zone was fully exited and re-entered.
 *
 * THIS IS INCORRECT for the plain single/rapid-reroute case, and this test
 * proves it: BAPBridge.update() computes `showManeuver = isValidType(type0)
 * && !isRerouting` (~line 1318), so `nowApproach` is unconditionally forced
 * false for every tick where isRerouting is true, regardless of distance or
 * unchanged data. That flips BAPBridge's own `inApproachZone` bookkeeping to
 * false during the REROUTING pulse, so the very next tick where the route is
 * active again necessarily reports `approachChanged` = true, which
 * unconditionally forces `dirty |= DIRTY_MANEUVER_ICON | ...` (~line
 * 1381-1385) BEFORE the renderer section is ever reached -- independent of
 * whatever RouteGuidance's own field-diff dirty mask says. Since the CLEAR
 * also resets lastCrIcon/lastCrIdx to -1, updateRendererIfChanged()'s
 * same-geometry dedup cannot suppress the resend either. This was confirmed
 * both by tracing the exact boolean formulas from BAPBridge.java by hand and
 * by mechanically re-running them in an isolated harness (see the defect
 * report for the trace output) for 1 reroute and for 3 consecutive reroutes;
 * every return-to-active tick forces the redraw.
 *
 * Follow-up investigation of the field bug (pill open but BLACK after several
 * quick reroutes; only a full viewport close/reopen recovers). Hypotheses
 * checked by static trace, no reproduction found in BAPBridge logic:
 *  - H1 (BAP close fails, presentation retained, renderer already cleared):
 *    scenario (e). Disproven: the assignment inApproachZone = nowApproach
 *    (BAPBridge.update, approachChanged block) runs BEFORE closeBapPresentation(),
 *    so a failed close does NOT keep inApproachZone true; the return tick still
 *    sees approachChanged and forces the icon-dirty bits.
 *  - H2 (route_generation bump makes isRerouting false): scenario (f). The bump
 *    empties the slots, so nowApproach is false (EXIT) and the next populated
 *    tick re-enters and redraws.
 * Invariant found: every renderer CLEAR happens on a tick with nowApproach=false,
 * and any later nowApproach=true tick is an approachChanged edge. The remaining
 * suspects are therefore outside this logic (physical ctx 72->80 rebind churn
 * from ScreenModule / renderer-side surface not re-swapped). Production code adds
 * only "RGI-DIAG" logging to confirm on the next drive.
 *
 * This test keeps the scenarios from that investigation as a regression
 * guard: if a future change to the approach-zone/dirty-forcing logic in
 * BAPBridge.update() ever breaks this self-healing path, this test will
 * catch it (a real black-screen defect, just not the one originally
 * suspected here).
 *
 * Reuses the createBridge/MockRenderer/createState fixture pattern from
 * RouteContextRendererLifecycleTest.java (not modified here).
 */
public final class RerouteRendererRecoveryTest {

    private static int checks = 0;

    private static void check(boolean ok, String label) {
        checks++;
        if (!ok) {
            throw new AssertionError("CHECK FAILED: " + label);
        }
    }

    /** Renderer mock that records EVERY call (not deduped) so we can inspect
     * ordering across a clear + re-send, unlike a "seen once" trace. */
    private static final class MockRenderer extends RendererServer {
        int clears = 0;
        int maneuvers = 0;
        int progressCalls = 0;
        int lastIcon = -1;
        int lastExitAngle = 0;
        boolean mockFrameReady = true;
        boolean mockReady = true;
        final List trace = new ArrayList();

        public boolean connect() {
            return true;
        }

        public boolean isReady() {
            return mockReady;
        }

        public boolean isFrameReady() {
            return mockFrameReady;
        }

        public boolean sendClear() {
            clears++;
            trace.add("CLEAR");
            return true;
        }

        public boolean sendBapProgressManeuver(int icon, int direction, int exitAngle,
                int drivingSide, int[] junctionAngles, int level, int mode, int perspective,
                boolean refresh, boolean snapToRoad, int progressState) {
            maneuvers++;
            lastIcon = icon;
            lastExitAngle = exitAngle;
            trace.add("MANEUVER");
            return true;
        }

        public boolean sendProgress(int level, int mode, int progressState) {
            progressCalls++;
            trace.add("PROGRESS");
            return true;
        }

        public boolean sendVisibleArea(int x, int y, int w, int h) {
            return true;
        }

        public boolean sendManeuver(int icon, int direction, int exitAngle,
                int drivingSide, int[] junctionAngles, int level, int mode, int perspective) {
            maneuvers++;
            lastIcon = icon;
            lastExitAngle = exitAngle;
            trace.add("MANEUVER");
            return true;
        }

        /** Last non-progress command recorded, or null. */
        String lastPaintCommand() {
            for (int i = trace.size() - 1; i >= 0; i--) {
                String e = (String) trace.get(i);
                if (!"PROGRESS".equals(e)) return e;
            }
            return null;
        }
    }

    private static final class BapProxyHandler implements InvocationHandler {
        /** When true, updateRGStatus(0) throws, so closeBapPresentation() reports failure
         * (models a BAP transaction that is still busy under rapid reroutes). */
        static volatile boolean failClose = false;

        public Object invoke(Object proxy, Method method, Object[] args) {
            if (failClose && "updateRGStatus".equals(method.getName())
                    && args != null && args.length == 1
                    && ((Integer) args[0]).intValue() == 0) {
                throw new IllegalStateException("injected BAP close failure");
            }
            Class type = method.getReturnType();
            if (type == Boolean.TYPE) return Boolean.FALSE;
            if (type == Integer.TYPE) return new Integer(0);
            return null;
        }
    }

    private static void setField(Class clazz, Object target, String name, Object val) throws Exception {
        Field f = clazz.getDeclaredField(name);
        f.setAccessible(true);
        f.set(target, val);
    }

    private static de.audi.tghu.navi.app.cluster.ClusterService createClusterService() throws Exception {
        Field uf = sun.misc.Unsafe.class.getDeclaredField("theUnsafe");
        uf.setAccessible(true);
        sun.misc.Unsafe unsafe = (sun.misc.Unsafe) uf.get(null);
        return (de.audi.tghu.navi.app.cluster.ClusterService)
            unsafe.allocateInstance(de.audi.tghu.navi.app.cluster.ClusterService.class);
    }

    private static BAPBridge createBridge(MockRenderer renderer) throws Exception {
        return createBridge(renderer, -1);
    }

    /** holdMs < 0 keeps the shipping REROUTE_HOLD_MS; 0 disables the reroute hold. */
    private static BAPBridge createBridge(MockRenderer renderer, int holdMs) throws Exception {
        BAPBridge bridge = new BAPBridge();
        if (holdMs >= 0) setField(BAPBridge.class, bridge, "rerouteHoldMs", Integer.valueOf(holdMs));
        CombiBAPServiceNavi service = (CombiBAPServiceNavi) Proxy.newProxyInstance(
            RerouteRendererRecoveryTest.class.getClassLoader(),
            new Class[]{CombiBAPServiceNavi.class},
            new BapProxyHandler());
        setField(BAPBridge.class, bridge, "appConnectorNavi", service);
        setField(BAPBridge.class, bridge, "initialized", Boolean.TRUE);
        setField(BAPBridge.class, bridge, "bapSessionStarted", Boolean.TRUE);
        setField(BAPBridge.class, bridge, "nativeStopAttempted", Boolean.TRUE);
        setField(BAPBridge.class, bridge, "csRef", createClusterService());
        if (renderer != null) {
            setField(BAPBridge.class, bridge, "rendererClient", renderer);
            setField(BAPBridge.class, bridge, "customRendererStarted", Boolean.TRUE);
        }
        ScreenModule.setRouteActive(true);
        ScreenModule.setPresentationActive(false);
        return bridge;
    }

    /** Builds a maneuver-bearing state the way createState() does in
     * RouteContextRendererLifecycleTest, but WITHOUT markAllDirtyForReplay() --
     * the caller sets dirtyMask explicitly so the test can reproduce exactly
     * what RouteGuidance's real field-diff parser would mark dirty. */
    private static RouteGuidance.State maneuverState(long gen, int routeState, long stateGen,
                                                       int mType, int angle, int distM, int ver) {
        RouteGuidance.State s = new RouteGuidance.State();
        s.routeGeneration = gen;
        s.routeState = routeState;
        s.routeStateGeneration = stateGen;
        s.maneuverCount = 1;
        s.maneuverOrder = new int[]{0};
        s.mType[0] = mType;
        s.mTurnAngle[0] = angle;
        s.mTurnAnglePresent[0] = (angle != -1);
        s.mJunctionType[0] = 0; // intersection
        s.mDistance[0] = distM;
        s.mVer[0] = ver;
        s.distManeuverM = distM;
        return s;
    }

    /** route_state:n:<v> alone -- exactly what RouteGuidance's parser marks
     * dirty when only the route_state key changes value (see State parsing
     * around DIRTY_ROUTE_STATE, ~line 995): DIRTY_ROUTE_STATE only. */
    private static final int DIRTY_ROUTE_STATE_ONLY = RouteGuidance.State.DIRTY_ROUTE_STATE;

    private static RouteGuidance.State createEmptyState(long gen, int routeState, long stateGen) {
        RouteGuidance.State s = new RouteGuidance.State();
        s.routeGeneration = gen;
        s.routeState = routeState;
        s.routeStateGeneration = stateGen;
        s.maneuverCount = 0;
        s.maneuverOrder = new int[0];
        s.distManeuverM = -1;
        return s;
    }

    public static void main(String[] args) throws Exception {
        Log.setLevel(-1);

        Field cnf = ScreenModule.class.getDeclaredField("connected");
        cnf.setAccessible(true);
        cnf.setBoolean(null, true);

        // ============================================================
        // (a) Single reroute, byte-identical data on return -> renderer must
        //     resend a MANEUVER, not stay stuck on the CLEAR.
        // ============================================================
        {
            MockRenderer renderer = new MockRenderer();
            BAPBridge bridge = createBridge(renderer);
            ScreenModule.setPresentationActive(false);

            // 1. Establish: active route, left turn (MT type 1) at 200m, inside
            //    city approach zone (< 457m). Full initial snapshot.
            RouteGuidance.State s1 = maneuverState(900L, 1, 900L, 1, -90, 200, 1);
            s1.markAllDirtyForReplay();
            bridge.update(s1);
            check(ScreenModule.isNavActive(), "(a).1: approach entered");
            check(renderer.maneuvers == 1, "(a).1: initial maneuver painted");
            check(renderer.lastExitAngle < 0, "(a).1: left turn geometry");

            // 2. Reroute begins: route_state 2->5, same generation, identical
            //    maneuver data. Only DIRTY_ROUTE_STATE toggles (real parser
            //    never marks m0_ver/m0_type dirty for an unchanged value).
            RouteGuidance.State s2 = maneuverState(900L, 5, 900L, 1, -90, 200, 1);
            s2.dirtyMask = DIRTY_ROUTE_STATE_ONLY;
            bridge.update(s2);
            // Reroute hold: presentation and last arrow stay up; no close, no CLEAR.
            check(renderer.clears == 0, "(a).2: reroute hold defers the renderer CLEAR");
            check(ScreenModule.isPresentationActive(), "(a).2: presentation held open during reroute");
            check("MANEUVER".equals(renderer.lastPaintCommand()), "(a).2: last arrow kept");

            // 3. Reroute resolves: route_state 5->2, SAME generation, SAME
            //    maneuver ver/type/angle/distance -- byte-identical slot data.
            //    Again only DIRTY_ROUTE_STATE toggles.
            RouteGuidance.State s3 = maneuverState(900L, 1, 900L, 1, -90, 200, 1);
            s3.dirtyMask = DIRTY_ROUTE_STATE_ONLY;
            int maneuversBefore = renderer.maneuvers;
            bridge.update(s3);

            // The cluster must not be left on a black/cleared surface. A
            // MANEUVER must have been resent after the CLEAR. This passes on
            // unmodified BAPBridge.java: the approach-zone exit/re-entry that
            // isRerouting forces (showManeuver=false during isRerouting) makes
            // BAPBridge's own approachChanged detection force DIRTY_MANEUVER_ICON
            // back on for this very tick, independent of RouteGuidance's dirty
            // mask -- see the class comment above.
            check(ScreenModule.isPresentationActive(), "(a).3: presentation still open (no close/open)");
            check(renderer.clears == 0, "(a).3: no CLEAR at any point");
            check(renderer.maneuvers == maneuversBefore + 1,
                "(a).3: no MANEUVER resent after reroute returned identical data "
                + "(renderer.maneuvers stayed at " + maneuversBefore + " -- cluster pill would be black)");
            check("MANEUVER".equals(renderer.lastPaintCommand()),
                "(a).3: last renderer paint command after the reroute is not MANEUVER "
                + "(was: " + renderer.lastPaintCommand() + ")");
            check(renderer.lastExitAngle < 0, "(a).3: repainted maneuver has the correct (left turn) geometry");
        }

        // ============================================================
        // (b) 3-5 rapid reroutes (5,2,5,2,5,2), some identical and one with a
        //     changed next maneuver -> renderer ends up showing the CURRENT
        //     (correct) maneuver, not a stale or blank one.
        // ============================================================
        {
            MockRenderer renderer = new MockRenderer();
            BAPBridge bridge = createBridge(renderer);
            ScreenModule.setPresentationActive(false);

            RouteGuidance.State s0 = maneuverState(910L, 1, 910L, 1, -90, 200, 1); // left turn
            s0.markAllDirtyForReplay();
            bridge.update(s0);
            check(renderer.lastExitAngle < 0, "(b).0: baseline left turn");

            // Reroute #1: identical data
            RouteGuidance.State r1 = maneuverState(910L, 5, 910L, 1, -90, 200, 1);
            r1.dirtyMask = DIRTY_ROUTE_STATE_ONLY;
            bridge.update(r1);
            RouteGuidance.State a1 = maneuverState(910L, 1, 910L, 1, -90, 200, 1);
            a1.dirtyMask = DIRTY_ROUTE_STATE_ONLY;
            bridge.update(a1);
            check(renderer.lastExitAngle < 0, "(b).1: still left turn after identical reroute #1");

            // Reroute #2: identical data again
            RouteGuidance.State r2 = maneuverState(910L, 5, 910L, 1, -90, 200, 1);
            r2.dirtyMask = DIRTY_ROUTE_STATE_ONLY;
            bridge.update(r2);
            RouteGuidance.State a2 = maneuverState(910L, 1, 910L, 1, -90, 200, 1);
            a2.dirtyMask = DIRTY_ROUTE_STATE_ONLY;
            bridge.update(a2);
            check(renderer.lastExitAngle < 0, "(b).2: still left turn after identical reroute #2");

            // Reroute #3: this one actually changes the next maneuver (right
            // turn, new version) -- the normal dirty path handles this one
            // (m0_ver changed -> DIRTY_MANEUVER_ICON set for real).
            RouteGuidance.State r3 = maneuverState(910L, 5, 910L, 1, -90, 200, 1);
            r3.dirtyMask = DIRTY_ROUTE_STATE_ONLY;
            bridge.update(r3);
            RouteGuidance.State a3 = maneuverState(910L, 1, 910L, 2, 90, 180, 2); // right turn, new ver
            a3.dirtyMask = RouteGuidance.State.DIRTY_ROUTE_STATE
                | RouteGuidance.State.DIRTY_MANEUVER_ICON
                | RouteGuidance.State.DIRTY_DIST_MAN;
            bridge.update(a3);

            check(renderer.lastExitAngle > 0, "(b).3: final maneuver reflects the CURRENT (right turn) geometry");
            check("MANEUVER".equals(renderer.lastPaintCommand()), "(b).3: last paint command is the current MANEUVER");
        }

        // ============================================================
        // (c) Genuine route end (maneuver_count 0, route_state <= 0) still
        //     clears immediately and must NOT redraw.
        // ============================================================
        {
            MockRenderer renderer = new MockRenderer();
            BAPBridge bridge = createBridge(renderer);
            ScreenModule.setPresentationActive(false);

            RouteGuidance.State s1 = maneuverState(920L, 1, 920L, 1, -90, 200, 1);
            s1.markAllDirtyForReplay();
            bridge.update(s1);
            check(renderer.maneuvers == 1, "(c).1: baseline maneuver painted");

            RouteGuidance.State end = createEmptyState(920L, 0, -1L);
            end.dirtyMask = RouteGuidance.State.DIRTY_ROUTE_STATE | RouteGuidance.State.DIRTY_MANEUVER_COUNT;
            int maneuversBefore = renderer.maneuvers;
            bridge.update(end);
            check(renderer.clears >= 1, "(c).2: genuine route end clears the renderer");
            check(renderer.maneuvers == maneuversBefore, "(c).2: genuine route end must NOT trigger a redraw");

            // A later, otherwise-eligible update (e.g. a distance-only tick)
            // still must not conjure a maneuver back onto a torn-down route.
            RouteGuidance.State tick = createEmptyState(920L, 0, -1L);
            tick.dirtyMask = 0;
            bridge.update(tick);
            check(renderer.maneuvers == maneuversBefore, "(c).3: no phantom redraw after route end");
        }

        // ============================================================
        // (d) Reroute while presentation/approach is NOT active -> must not
        //     force a redraw or open anything.
        // ============================================================
        {
            MockRenderer renderer = new MockRenderer();
            BAPBridge bridge = createBridge(renderer);
            ScreenModule.setPresentationActive(false);

            // Cruising far outside the approach zone (city threshold 457m).
            RouteGuidance.State sFar = maneuverState(930L, 1, 930L, 1, -90, 5000, 1);
            sFar.markAllDirtyForReplay();
            bridge.update(sFar);
            check(!ScreenModule.isNavActive(), "(d).1: cruising outside approach stays inactive");
            check(renderer.maneuvers == 0, "(d).1: no maneuver painted outside approach");
            check(!ScreenModule.isPresentationActive(), "(d).1: presentation not active outside approach");

            // Reroute happens while still far away, identical data both sides.
            RouteGuidance.State rFar = maneuverState(930L, 5, 930L, 1, -90, 5000, 1);
            rFar.dirtyMask = DIRTY_ROUTE_STATE_ONLY;
            bridge.update(rFar);
            RouteGuidance.State aFar = maneuverState(930L, 1, 930L, 1, -90, 5000, 1);
            aFar.dirtyMask = DIRTY_ROUTE_STATE_ONLY;
            bridge.update(aFar);

            check(!ScreenModule.isNavActive(), "(d).2: reroute outside approach does not force approach entry");
            check(!ScreenModule.isPresentationActive(), "(d).2: reroute outside approach does not open presentation");
            check(renderer.maneuvers == 0, "(d).2: reroute outside approach does not force a redraw");
        }

        // ============================================================
        // (e) H1: BAP close FAILS during the reroute ticks (presentation is
        //     retained, renderer already cleared), then the route returns to
        //     active with byte-identical data inside the zone. The renderer
        //     must not stay on the CLEAR while the presentation is open.
        //     Traced to pass: inApproachZone is already false after the
        //     reroute tick (approachChanged assigns it before the close), so
        //     the return tick forces DIRTY_MANEUVER_ICON again.
        // ============================================================
        {
            MockRenderer renderer = new MockRenderer();
            BAPBridge bridge = createBridge(renderer);
            ScreenModule.setPresentationActive(false);

            RouteGuidance.State s1 = maneuverState(940L, 1, 940L, 1, -90, 200, 1);
            s1.markAllDirtyForReplay();
            bridge.update(s1);
            check(ScreenModule.isPresentationActive(), "(e).1: presentation open in zone");
            check(renderer.maneuvers == 1, "(e).1: initial maneuver painted");

            BapProxyHandler.failClose = true;
            try {
                for (int i = 0; i < 2; i++) { // two consecutive failed closes (rapid reroutes)
                    RouteGuidance.State r = maneuverState(940L, 5, 940L, 1, -90, 200, 1);
                    r.dirtyMask = DIRTY_ROUTE_STATE_ONLY;
                    bridge.update(r);
                }
            } finally {
                BapProxyHandler.failClose = false;
            }
            check(ScreenModule.isPresentationActive(), "(e).2: presentation retained");
            check("MANEUVER".equals(renderer.lastPaintCommand()),
                "(e).2: hold keeps the last arrow (no close is even attempted)");

            RouteGuidance.State s3 = maneuverState(940L, 1, 940L, 1, -90, 200, 1);
            s3.dirtyMask = DIRTY_ROUTE_STATE_ONLY;
            bridge.update(s3);
            check(ScreenModule.isPresentationActive(), "(e).3: presentation still open");
            check("MANEUVER".equals(renderer.lastPaintCommand()),
                "(e).3: pill open but renderer left blank after failed-close reroute "
                + "(last paint command: " + renderer.lastPaintCommand() + ")");
        }

        // ============================================================
        // (e2) Same as (e) with the hold DISABLED: the pre-hold close path (renderer
        //      cleared, close fails, presentation retained) must still self-heal.
        // ============================================================
        {
            MockRenderer renderer = new MockRenderer();
            BAPBridge bridge = createBridge(renderer, 0);
            ScreenModule.setPresentationActive(false);

            RouteGuidance.State s1 = maneuverState(941L, 1, 941L, 1, -90, 200, 1);
            s1.markAllDirtyForReplay();
            bridge.update(s1);
            BapProxyHandler.failClose = true;
            try {
                for (int i = 0; i < 2; i++) {
                    RouteGuidance.State r = maneuverState(941L, 5, 941L, 1, -90, 200, 1);
                    r.dirtyMask = DIRTY_ROUTE_STATE_ONLY;
                    bridge.update(r);
                }
            } finally {
                BapProxyHandler.failClose = false;
            }
            check(ScreenModule.isPresentationActive(), "(e2).2: failed close retains presentation");
            check("CLEAR".equals(renderer.lastPaintCommand()), "(e2).2: renderer cleared during reroute");
            RouteGuidance.State s3 = maneuverState(941L, 1, 941L, 1, -90, 200, 1);
            s3.dirtyMask = DIRTY_ROUTE_STATE_ONLY;
            bridge.update(s3);
            check("MANEUVER".equals(renderer.lastPaintCommand()),
                "(e2).3: pill open but renderer left blank after failed-close reroute");
        }

        // ============================================================
        // (f) H2: route_generation bump clears the slots (routeStateGeneration
        //     invalidated, so isRerouting is false while routeState stays 1),
        //     then the new generation's maneuver arrives. Must end on MANEUVER.
        // ============================================================
        {
            MockRenderer renderer = new MockRenderer();
            BAPBridge bridge = createBridge(renderer);
            ScreenModule.setPresentationActive(false);

            RouteGuidance.State s1 = maneuverState(950L, 1, 950L, 1, -90, 200, 1);
            s1.markAllDirtyForReplay();
            bridge.update(s1);
            check(renderer.maneuvers == 1, "(f).1: baseline painted");

            RouteGuidance.State g = createEmptyState(951L, 1, -1L);
            g.dirtyMask = RouteGuidance.State.DIRTY_MANEUVER_COUNT | RouteGuidance.State.DIRTY_MANEUVER_LIST
                | RouteGuidance.State.DIRTY_MANEUVER_ICON | RouteGuidance.State.DIRTY_ROUTE_STATE;
            bridge.update(g);
            RouteGuidance.State n = maneuverState(951L, 1, 951L, 1, -90, 200, 1);
            n.dirtyMask = RouteGuidance.State.DIRTY_MANEUVER_COUNT | RouteGuidance.State.DIRTY_MANEUVER_LIST
                | RouteGuidance.State.DIRTY_MANEUVER_ICON | RouteGuidance.State.DIRTY_DIST_MAN
                | RouteGuidance.State.DIRTY_ROUTE_STATE;
            bridge.update(n);
            check("MANEUVER".equals(renderer.lastPaintCommand()),
                "(f).2: renderer blank after route_generation bump (last: " + renderer.lastPaintCommand() + ")");
        }

        System.out.println("RerouteRendererRecoveryTest: reroute renderer recovery PASS (" + checks + " checks)");
    }
}
