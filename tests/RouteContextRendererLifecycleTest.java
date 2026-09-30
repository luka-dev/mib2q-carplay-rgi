import com.luka.carplay.core.ScreenModule;
import com.luka.carplay.framework.Log;
import com.luka.carplay.rgd.BAPBridge;
import com.luka.carplay.rgd.RendererMapper;
import com.luka.carplay.rgd.RendererServer;
import com.luka.carplay.rgd.RouteGuidance;
import de.audi.atip.hmi.view.IDisplayManager;
import de.audi.atip.interapp.combi.bap.navi.CombiBAPServiceNavi;
import de.audi.tghu.fwhmi.IDisplayManagerKombiControl;

import java.lang.reflect.Field;
import java.lang.reflect.InvocationHandler;
import java.lang.reflect.Method;
import java.lang.reflect.Proxy;
import java.util.ArrayList;
import java.util.List;

/**
 * Layer 5: Visual Renderer, Physical Context, and Route Lifecycle Integration Tests.
 *
 * Tests the interaction between BAPBridge, RendererServer, ScreenModule, and RouteGuidance:
 * - Renderer clear and re-render during reroutes
 * - Stale FRAME_READY rejection across reroutes
 * - Renderer failure and self-healing recovery
 * - Highway <-> City threshold transitions without generation changes
 * - Same-index version updates inside approach zone
 * - 100 consecutive rapid route sessions
 * - Disconnect / reconnect state isolation with identical generation IDs
 */
public final class RouteContextRendererLifecycleTest {

    private static int checks = 0;

    private static void check(boolean ok, String label) {
        checks++;
        if (!ok) {
            throw new AssertionError("CHECK FAILED: " + label);
        }
    }

    private static final class TestRebindCollector implements ScreenModule.RebindListener {
        final List reasons = new ArrayList();

        public void onClusterContextRebindRequested(String reason) {
            reasons.add(reason != null ? reason : "");
        }

        int count() { return reasons.size(); }
        void clear() { reasons.clear(); }
    }

    private static final class MockRenderer extends RendererServer {
        int clears = 0;
        int maneuvers = 0;
        int progressCalls = 0;
        int lastIcon = -1;
        int lastExitAngle = 0;
        boolean failNextSend = false;
        boolean mockFrameReady = true;
        boolean mockReady = true;
        StateListener stateListener = null;
        final List trace = new ArrayList();

        public void setStateListener(StateListener l) {
            this.stateListener = l;
            super.setStateListener(l);
        }

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

        public void notifyFrameCleared() {
            mockFrameReady = false;
            trace.add("FRAME_CLEARED");
            if (stateListener != null) {
                stateListener.onRendererStateChanged("frame-cleared");
            }
        }

        public void notifyFrameReady() {
            mockFrameReady = true;
            trace.add("FRAME_READY");
            if (stateListener != null) {
                stateListener.onRendererStateChanged("frame-ready");
            }
        }

        public boolean sendBapProgressManeuver(int icon, int direction, int exitAngle,
                int drivingSide, int[] junctionAngles, int level, int mode, int perspective,
                boolean refresh, boolean snapToRoad, int progressState) {
            if (failNextSend) {
                return false;
            }
            maneuvers++;
            lastIcon = icon;
            lastExitAngle = exitAngle;
            if (!trace.contains("MANEUVER")) {
                trace.add("MANEUVER");
            }
            return true;
        }

        public boolean sendProgress(int level, int mode, int progressState) {
            if (failNextSend) {
                return false;
            }
            progressCalls++;
            return true;
        }

        public boolean sendVisibleArea(int x, int y, int w, int h) {
            return true;
        }

        public boolean sendManeuver(int icon, int direction, int exitAngle,
                int drivingSide, int[] junctionAngles, int level, int mode, int perspective) {
            maneuvers++;
            if (!trace.contains("MANEUVER")) {
                trace.add("MANEUVER");
            }
            return true;
        }
    }

    private static void setSimulatedPresentationActive(boolean active) {
        ScreenModule.setPresentationActive(active);
    }

    private static final class BapProxyHandler implements InvocationHandler {
        public Object invoke(Object proxy, Method method, Object[] args) {
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
        BAPBridge bridge = new BAPBridge();
        CombiBAPServiceNavi service = (CombiBAPServiceNavi) Proxy.newProxyInstance(
            RouteContextRendererLifecycleTest.class.getClassLoader(),
            new Class[]{CombiBAPServiceNavi.class},
            new BapProxyHandler());
        setField(BAPBridge.class, bridge, "appConnectorNavi", service);
        setField(BAPBridge.class, bridge, "initialized", Boolean.TRUE);
        setField(BAPBridge.class, bridge, "bapSessionStarted", Boolean.TRUE);
        setField(BAPBridge.class, bridge, "nativeStopAttempted", Boolean.TRUE);
        /* These suites pin the approach-EXIT-on-reroute behaviour itself, so they run with the
         * reroute hold disabled (the hold is covered by RerouteHoldAndKdkVerifyTest). */
        setField(BAPBridge.class, bridge, "rerouteHoldMs", Integer.valueOf(0));
        setField(BAPBridge.class, bridge, "csRef", createClusterService());
        if (renderer != null) {
            setField(BAPBridge.class, bridge, "rendererClient", renderer);
            setField(BAPBridge.class, bridge, "customRendererStarted", Boolean.TRUE);
        }
        ScreenModule.setRouteActive(true);
        ScreenModule.setPresentationActive(false);
        return bridge;
    }

    private static RouteGuidance.State createState(long gen, int routeState, long stateGen,
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
        s.markAllDirtyForReplay();
        return s;
    }

    public static void main(String[] args) throws Exception {
        Log.setLevel(-1);

        TestRebindCollector collector = new TestRebindCollector();
        ScreenModule.setRebindListener(collector);

        Field naf = ScreenModule.class.getDeclaredField("navActive");
        naf.setAccessible(true);
        Field cnf = ScreenModule.class.getDeclaredField("connected");
        cnf.setAccessible(true);
        cnf.setBoolean(null, true);

        // ============================================================
        // Test 1: Full Visual & Context Lifecycle
        // ============================================================
        {
            MockRenderer renderer = new MockRenderer();
            BAPBridge bridge = createBridge(renderer);
            collector.clear();
            setSimulatedPresentationActive(false);

            // 1. Cruising outside approach at 500m -> ctx 74, renderer not active
            RouteGuidance.State sFar = createState(100L, 1, 100L, 1, -90, 500, 1);
            bridge.update(sFar);
            check(!ScreenModule.isNavActive(), "T1.1: cruising at 500m outside approach");
            check(renderer.maneuvers == 0, "T1.1: no maneuver sent to renderer outside approach");

            // 2. Approach zone entry at 200m -> ctx 80, renderer receives left turn
            RouteGuidance.State sNear = createState(100L, 1, 100L, 1, -90, 200, 1);
            bridge.update(sNear);
            check(ScreenModule.isNavActive(), "T1.2: enters approach at 200m");
            check(renderer.maneuvers == 1, "T1.2: renderer received maneuver");
            check(renderer.lastExitAngle < 0, "T1.2: left turn exit angle");

            // 3. Reroute occurs -> drops to ctx 74, renderer sent clear
            RouteGuidance.State sReroute = createState(100L, 5, 100L, 1, -90, 200, 1);
            bridge.update(sReroute);
            check(!ScreenModule.isNavActive(), "T1.3: reroute exits approach");
            check(renderer.clears == 1, "T1.3: renderer received sendClear()");

            // 4. New route at 483m (~0.3 mi) -> unauthenticated state, stays 74
            RouteGuidance.State sNewFar = createState(101L, 5, -1L, 2, 90, 483, 1);
            bridge.update(sNewFar);
            check(!ScreenModule.isNavActive(), "T1.4: 483m stays in ctx 74");
            check(renderer.clears == 1, "T1.4: renderer still in clear state");

            // 5. New route enters approach at 200m -> ctx 80, renderer receives right turn
            RouteGuidance.State sNewNear = createState(101L, 1, 101L, 2, 90, 200, 1);
            bridge.update(sNewNear);
            check(ScreenModule.isNavActive(), "T1.5: 200m enters approach");
            check(renderer.maneuvers == 2, "T1.5: renderer received new maneuver");
            check(renderer.lastExitAngle > 0, "T1.5: right turn exit angle painted");
        }

        // ============================================================
        // Test 2: Stale FRAME_READY Rejection Across Reroute
        // ============================================================
        {
            RendererServer realServer = new RendererServer();
            setField(RendererServer.class, realServer, "running", Boolean.TRUE);
            setField(RendererServer.class, realServer, "out", new java.io.ByteArrayOutputStream());

            Method onEvt = RendererServer.class.getDeclaredMethod("handleRendererEvent",
                new Class[]{java.net.Socket.class, Byte.TYPE});
            onEvt.setAccessible(true);
            Field frameReadyF = RendererServer.class.getDeclaredField("frameReady");
            frameReadyF.setAccessible(true);

            // Establish frame ready on active route
            onEvt.invoke(realServer, new Object[]{null, new Byte((byte) 0x82)}); // EVT_FRAME_READY
            check(frameReadyF.getBoolean(realServer), "T2: initial frameReady is true");

            // Reroute triggers sendClear()
            realServer.sendClear();
            check(!frameReadyF.getBoolean(realServer), "T2: sendClear resets frameReady to false");

            // Stale EVT_FRAME_READY arrives from the old frame while clear is pending
            onEvt.invoke(realServer, new Object[]{null, new Byte((byte) 0x82)});
            check(!frameReadyF.getBoolean(realServer), "T2: stale FRAME_READY rejected while clear pending!");

            // EVT_FRAME_CLEARED acknowledges clear
            onEvt.invoke(realServer, new Object[]{null, new Byte((byte) 0x83)}); // EVT_FRAME_CLEARED
            check(!frameReadyF.getBoolean(realServer), "T2: frameCleared complete");

            // Now new route's FRAME_READY arrives and is accepted
            onEvt.invoke(realServer, new Object[]{null, new Byte((byte) 0x82)});
            check(frameReadyF.getBoolean(realServer), "T2: new route FRAME_READY accepted");
        }

        // ============================================================
        // Test 3: Renderer Send Failure and Self-Healing Recovery
        // ============================================================
        {
            MockRenderer renderer = new MockRenderer();
            BAPBridge bridge = createBridge(renderer);
            collector.clear();
            setSimulatedPresentationActive(false);

            // Simulate broken pipe on renderer
            renderer.failNextSend = true;
            RouteGuidance.State sNear = createState(200L, 1, 200L, 1, -90, 200, 1);
            bridge.update(sNear);

            // Renderer send failed
            check(renderer.maneuvers == 0, "T3: send failed as simulated");

            // Link heals, next delta replays
            renderer.failNextSend = false;
            sNear.markAllDirtyForReplay();
            bridge.update(sNear);
            check(renderer.maneuvers == 1, "T3: replayed maneuver delivered to recovered renderer");
            check(renderer.lastExitAngle < 0, "T3: correct maneuver geometry restored");
        }

        // ============================================================
        // Test 4: Primary Maneuver Update Inside Approach (Version Change)
        // ============================================================
        {
            MockRenderer renderer = new MockRenderer();
            BAPBridge bridge = createBridge(renderer);
            collector.clear();
            setSimulatedPresentationActive(true); // Active in 80

            // Initial left turn at 200m (version 1)
            RouteGuidance.State s1 = createState(300L, 1, 300L, 1, -90, 200, 1);
            bridge.update(s1);
            check(renderer.maneuvers == 1, "T4.1: first maneuver rendered");
            check(renderer.lastExitAngle < 0, "T4.1: left turn exit angle");
            check(collector.count() == 1, "T4.1: generation 300 entry executes its one rebind");

            // Consecutive turn at 180m: right turn with version 2 (same generation 300)
            RouteGuidance.State s2 = createState(300L, 1, 300L, 2, 90, 180, 2);
            bridge.update(s2);
            check(renderer.maneuvers == 2, "T4.2: updated maneuver rendered");
            check(renderer.lastExitAngle > 0, "T4.2: right turn exit angle updated");
            check(collector.count() == 1, "T4.2: ZERO additional context rebinds for same-route maneuver update");
        }

        // ============================================================
        // Test 5: Highway <-> City Maneuver Replacement at 1000m
        // ============================================================
        {
            MockRenderer renderer = new MockRenderer();
            BAPBridge bridge = createBridge(renderer);
            collector.clear();
            setSimulatedPresentationActive(false);

            // 1. Highway ramp at 1000m -> within 1600m threshold -> approaches in 80
            RouteGuidance.State sHighway = createState(400L, 1, 400L, 8, 45, 1000, 1); // MT_OFF_RAMP (8)
            bridge.update(sHighway);
            check(ScreenModule.isNavActive(), "T5.1: 1000m highway ramp enters approach");

            // 2. Primary maneuver becomes city turn at 1000m -> outside 457m city threshold -> exits to 74
            RouteGuidance.State sCity = createState(400L, 1, 400L, 1, -90, 1000, 2); // MT_LEFT_TURN (1)
            bridge.update(sCity);
            check(!ScreenModule.isNavActive(), "T5.2: 1000m city turn exits approach (drops to 74)");

            // 3. Cruising reaches 457m -> enters city approach
            RouteGuidance.State sCityNear = createState(400L, 1, 400L, 1, -90, 457, 2);
            bridge.update(sCityNear);
            check(ScreenModule.isNavActive(), "T5.3: 457m city turn re-enters approach");
        }

        // ============================================================
        // Test 6: 100 Consecutive Rapid Route Sessions
        // ============================================================
        {
            MockRenderer renderer = new MockRenderer();
            BAPBridge bridge = createBridge(renderer);

            for (int session = 0; session < 100; session++) {
                collector.clear();
                bridge.onStart();
                setSimulatedPresentationActive(false);
                long gen = 1000L + session;

                // Route start + approach
                RouteGuidance.State sStart = createState(gen, 1, gen, 1, -90, 200, 1);
                bridge.update(sStart);
                check(ScreenModule.isNavActive(), "T6: session " + session + " start enters approach");

                // Reroute
                RouteGuidance.State sReroute = createState(gen, 5, gen, 1, -90, 200, 1);
                bridge.update(sReroute);
                check(!ScreenModule.isNavActive(), "T6: session " + session + " reroute exits approach");

                // Route end
                bridge.onRouteEnd();
                check(!ScreenModule.isNavActive(), "T6: session " + session + " route end inactive");
            }
        }

        // ============================================================
        // Test 7: Disconnect / Reconnect with Identical Generation ID
        // ============================================================
        {
            MockRenderer renderer = new MockRenderer();
            BAPBridge bridge = createBridge(renderer);
            collector.clear();
            setSimulatedPresentationActive(false);

            // Session 1: gen 500 in approach
            RouteGuidance.State s1 = createState(500L, 1, 500L, 1, -90, 200, 1);
            bridge.update(s1);
            check(ScreenModule.isNavActive(), "T7: session 1 enters approach");

            // Disconnect
            bridge.onStop();
            bridge.onShutdown();
            ScreenModule.setNavActive(false);

            // Session 2 connects with identical gen 500 at 200m
            setField(BAPBridge.class, bridge, "rendererClient", renderer);
            setField(BAPBridge.class, bridge, "customRendererStarted", Boolean.TRUE);
            bridge.onStart();
            collector.clear();
            RouteGuidance.State s2 = createState(500L, 1, 500L, 1, -90, 200, 1);
            bridge.update(s2);

            // Must enter approach cleanly without stale state from session 1
            check(ScreenModule.isNavActive(), "T7: session 2 connects cleanly at gen 500");
            check(collector.count() == 0, "T7: clean 74->80 entry, no stale rebind from session 1");
        }

        // ============================================================
        // Test 8: Physical Operation Trace & Settlement Invariant
        // Note: Verifies deterministic state -> applySwitch() -> mocked DM sequence.
        // Worker concurrency, wakeup arbitration, and bounce sleep are tested in ScreenModuleContextRaceTest.
        // ============================================================
        {
            MockRenderer renderer = new MockRenderer();
            BAPBridge bridge = createBridge(renderer);
            setField(BAPBridge.class, bridge, "csRef", createClusterService());
            collector.clear();
            ScreenModule.setRouteActive(true);
            setSimulatedPresentationActive(false);

            // Mock DisplayManager to record actual physical compositor switches
            final List physicalDmSwitches = new ArrayList();
            IDisplayManager mockDm = (IDisplayManager) Proxy.newProxyInstance(
                RouteContextRendererLifecycleTest.class.getClassLoader(),
                new Class[]{IDisplayManager.class, IDisplayManagerKombiControl.class},
                new InvocationHandler() {
                    public Object invoke(Object proxy, Method method, Object[] args) {
                        if ("switchContext".equals(method.getName())) {
                            physicalDmSwitches.add(args[0]);
                        }
                        Class type = method.getReturnType();
                        if (type == Boolean.TYPE) return Boolean.FALSE;
                        if (type == Integer.TYPE) return new Integer(0);
                        return null;
                    }
                });
            Method applySwitchMethod = ScreenModule.class.getDeclaredMethod("applySwitch",
                new Class[]{Integer.TYPE, IDisplayManager.class});
            applySwitchMethod.setAccessible(true);
            ScreenModule sm = new ScreenModule();
            setField(ScreenModule.class, sm, "dm", mockDm);

            // 1. Initial approach entry with frame-ready renderer
            renderer.mockFrameReady = true;
            RouteGuidance.State s1 = createState(2000L, 1, 2000L, 1, -90, 200, 1);
            bridge.update(s1);
            check(ScreenModule.isPresentationActive(), "T8: approach entered");
            check(ScreenModule.getDesiredCtx() == 80, "T8: desiredCtx is 80 in approach");

            renderer.trace.clear();
            final List logicalLifecycleTrace = new ArrayList();

            // 2. Approach exit -> triggers CLEAR, FRAME_CLEARED, DESIRED_CTX_74
            RouteGuidance.State sFar = createState(2000L, 1, 2000L, 1, -90, 600, 1);
            bridge.update(sFar);
            check(renderer.trace.contains("CLEAR"), "T8: CLEAR sent to renderer on approach exit");
            logicalLifecycleTrace.add("CLEAR");

            // Hardware completes frame clearing
            renderer.notifyFrameCleared();
            check(renderer.trace.contains("FRAME_CLEARED"), "T8: FRAME_CLEARED recorded");
            logicalLifecycleTrace.add("FRAME_CLEARED");

            check(!ScreenModule.isPresentationActive(), "T8: presentation deactivated");
            check(ScreenModule.getDesiredCtx() == 74, "T8: desiredCtx returned to 74");
            logicalLifecycleTrace.add("DESIRED_CTX_74");
            // Execute physical switch for desiredCtx=74
            applySwitchMethod.invoke(sm, new Object[]{new Integer(ScreenModule.getDesiredCtx()), mockDm});

            // 3. New maneuver arrives inside approach (200m) while frame is not yet ready
            RouteGuidance.State sNewMan = createState(2000L, 1, 2000L, 2, 45, 200, 2);
            bridge.update(sNewMan);
            check(renderer.trace.contains("MANEUVER"), "T8: MANEUVER sent to renderer");
            logicalLifecycleTrace.add("MANEUVER");

            // Presentation held inactive while renderer is rasterizing (not frame-ready)
            check(!ScreenModule.isPresentationActive(), "T8: presentation held inactive while not frame-ready");
            check(ScreenModule.getDesiredCtx() == 74, "T8: desiredCtx held in 74");

            // 4. Renderer signals swap completion / FRAME_READY
            renderer.notifyFrameReady();
            check(renderer.trace.contains("FRAME_READY"), "T8: FRAME_READY recorded");
            logicalLifecycleTrace.add("FRAME_READY");

            // Next distance tick triggers context transition to 80
            RouteGuidance.State sTick = createState(2000L, 1, 2000L, 2, 45, 195, 2);
            bridge.update(sTick);
            check(ScreenModule.isPresentationActive(), "T8: presentation activated on frame readiness");
            check(ScreenModule.getDesiredCtx() == 80, "T8: desiredCtx switched to 80");
            logicalLifecycleTrace.add("DESIRED_CTX_80");
            // Execute physical switch for desiredCtx=80
            applySwitchMethod.invoke(sm, new Object[]{new Integer(ScreenModule.getDesiredCtx()), mockDm});

            // A. Verify renderer IPC command trace (pure renderer calls)
            List expectedRendererTrace = new ArrayList();
            expectedRendererTrace.add("CLEAR");
            expectedRendererTrace.add("FRAME_CLEARED");
            expectedRendererTrace.add("MANEUVER");
            expectedRendererTrace.add("FRAME_READY");
            check(renderer.trace.equals(expectedRendererTrace),
                "T8: renderer IPC trace mismatch!\nExpected: " + expectedRendererTrace + "\nActual:   " + renderer.trace);

            // B. Verify logical lifecycle event sequence
            List expectedLogicalTrace = new ArrayList();
            expectedLogicalTrace.add("CLEAR");
            expectedLogicalTrace.add("FRAME_CLEARED");
            expectedLogicalTrace.add("DESIRED_CTX_74");
            expectedLogicalTrace.add("MANEUVER");
            expectedLogicalTrace.add("FRAME_READY");
            expectedLogicalTrace.add("DESIRED_CTX_80");
            check(logicalLifecycleTrace.equals(expectedLogicalTrace),
                "T8: logical lifecycle trace mismatch!\nExpected: " + expectedLogicalTrace + "\nActual:   " + logicalLifecycleTrace);

            // C. Verify physical DisplayManager compositor switches (74 -> 72 bounce -> 80)
            List expectedDmSwitches = new ArrayList();
            expectedDmSwitches.add(new Integer(74));
            expectedDmSwitches.add(new Integer(72)); // bounce context
            expectedDmSwitches.add(new Integer(80)); // target cluster context
            check(physicalDmSwitches.equals(expectedDmSwitches),
                "T8: physical DM switch trace mismatch!\nExpected: " + expectedDmSwitches + "\nActual:   " + physicalDmSwitches);

            // ============================================================
            // INVARIANT: Cannot settle in ctx 80 when renderer readiness is withdrawn
            // ============================================================
            // 1. Renderer readiness lost mid-approach
            renderer.notifyFrameCleared();
            bridge.update(createState(2000L, 1, 2000L, 2, 45, 190, 2));
            check(!ScreenModule.isPresentationActive(),
                "T8 INV: presentationActive must drop when frame readiness is lost");
            check(!ScreenModule.isNavActive(),
                "T8 INV: navActive must drop when frame readiness is lost");
            check(ScreenModule.getDesiredCtx() == 74,
                "T8 INV: cannot settle in ctx 80 when renderer is not frame-ready");

            // 2. Subsequent updates while not frame-ready MUST continue to settle in ctx 74
            for (int d = 180; d >= 50; d -= 20) {
                bridge.update(createState(2000L, 1, 2000L, 2, 45, d, 2));
                check(!ScreenModule.isPresentationActive(), "T8 INV: remains inactive at " + d + "m");
                check(ScreenModule.getDesiredCtx() == 74, "T8 INV: desiredCtx remains 74 at " + d + "m");
            }

            // 3. Late/stale presentation call cannot resurrect routeActive or settle in 80
            ScreenModule.setRouteActive(false);
            ScreenModule.setPresentationActive(true);
            check(!ScreenModule.isRouteActive(), "T8 INV: routeActive remains false");
            check(!ScreenModule.isPresentationActive(), "T8 INV: presentationActive remains false");
            check(ScreenModule.getDesiredCtx() == 74, "T8 INV: desiredCtx remains 74");
            setField(ScreenModule.class, sm, "dm", null);
        }

        // ============================================================
        // Test 9: BAPBridge onStart() Failure Rollback & State Isolation
        // ============================================================
        {
            // Case 9A: tryStopNativeNavigation() fails -> onStart() returns false
            // routeActive, presentationActive, and navActive must remain false
            ScreenModule.setRouteActive(false);
            ScreenModule.setPresentationActive(false);

            MockRenderer renderer = new MockRenderer();
            BAPBridge bridge = new BAPBridge();
            CombiBAPServiceNavi service = (CombiBAPServiceNavi) Proxy.newProxyInstance(
                RouteContextRendererLifecycleTest.class.getClassLoader(),
                new Class[]{CombiBAPServiceNavi.class},
                new BapProxyHandler());
            setField(BAPBridge.class, bridge, "appConnectorNavi", service);
            setField(BAPBridge.class, bridge, "initialized", Boolean.TRUE);
            setField(BAPBridge.class, bridge, "rendererClient", renderer);
            setField(BAPBridge.class, bridge, "customRendererStarted", Boolean.TRUE);
            // Notice: nativeStopAttempted = false and csRef = null -> tryStopNativeNavigation returns false
            boolean started = bridge.onStart();
            check(!started, "T9A: onStart must return false when nativeStop fails");
            check(!ScreenModule.isRouteActive(), "T9A: routeActive must remain false after failed onStart");
            check(!ScreenModule.isPresentationActive(), "T9A: presentationActive must remain false after failed onStart");
            check(!ScreenModule.isNavActive(), "T9A: navActive must remain false after failed onStart");
            check(ScreenModule.getDesiredCtx() == 74, "T9A: desiredCtx must remain 74 after failed onStart");

            // Verify that a late/stale presentation callback cannot activate presentation or context 80
            ScreenModule.setPresentationActive(true);
            check(!ScreenModule.isRouteActive(), "T9A: late presentation call cannot resurrect routeActive");
            check(!ScreenModule.isPresentationActive(), "T9A: late presentation call rejected when routeActive is false");
            check(!ScreenModule.isNavActive(), "T9A: navActive remains false");
            check(ScreenModule.getDesiredCtx() == 74, "T9A: desiredCtx remains 74");

            // Case 9B: Exception thrown during onStart() -> caught and rolled back
            final boolean[] rgiStatusReset = new boolean[]{false};
            CombiBAPServiceNavi throwingService = (CombiBAPServiceNavi) Proxy.newProxyInstance(
                RouteContextRendererLifecycleTest.class.getClassLoader(),
                new Class[]{CombiBAPServiceNavi.class},
                new InvocationHandler() {
                    public Object invoke(Object proxy, Method method, Object[] args) throws Throwable {
                        if ("updateRGStatus".equals(method.getName())) {
                            int status = ((Integer) args[0]).intValue();
                            if (status == 1) {
                                throw new RuntimeException("Simulated BAP bus fault");
                            } else if (status == 0) {
                                rgiStatusReset[0] = true;
                            }
                        }
                        Class type = method.getReturnType();
                        if (type == Boolean.TYPE) return Boolean.FALSE;
                        if (type == Integer.TYPE) return new Integer(0);
                        return null;
                    }
                });
            BAPBridge throwingBridge = new BAPBridge();
            setField(BAPBridge.class, throwingBridge, "appConnectorNavi", throwingService);
            setField(BAPBridge.class, throwingBridge, "initialized", Boolean.TRUE);
            setField(BAPBridge.class, throwingBridge, "rendererClient", renderer);
            setField(BAPBridge.class, throwingBridge, "customRendererStarted", Boolean.TRUE);
            setField(BAPBridge.class, throwingBridge, "nativeStopAttempted", Boolean.TRUE); // bypass nativeStop
            setField(BAPBridge.class, throwingBridge, "csRef", createClusterService());

            boolean throwingStarted = throwingBridge.onStart();
            check(!throwingStarted, "T9B: onStart must return false when exception thrown");
            check(rgiStatusReset[0], "T9B: rollbackFailedStart reset RGStatus to 0");
            check(!ScreenModule.isRouteActive(), "T9B: routeActive must be false after rollback");
            check(!ScreenModule.isPresentationActive(), "T9B: presentationActive must be false after rollback");
            check(!ScreenModule.isNavActive(), "T9B: navActive must be false after rollback");
            check(ScreenModule.getDesiredCtx() == 74, "T9B: desiredCtx must be 74 after rollback");

            // Case 9C: KDK is visible prior to and during a failed onStart() attempt
            // rollbackRouteLifecycle() MUST NOT trigger route-end KDK-hold semantics
            com.luka.carplay.cluster.ClusterLayerController.onVcVisibility(true);
            check(com.luka.carplay.cluster.ClusterLayerController.isKdkVisible(), "T9C precondition: KDK is visible");

            ScreenModule.rollbackRouteLifecycle();
            check(!ScreenModule.isRouteActive(), "T9C precondition: routeActive false");
            check(!ScreenModule.isNavHidePending(), "T9C precondition: navHidePending false");

            BAPBridge kdkFailBridge = new BAPBridge();
            setField(BAPBridge.class, kdkFailBridge, "appConnectorNavi", service);
            setField(BAPBridge.class, kdkFailBridge, "initialized", Boolean.TRUE);
            setField(BAPBridge.class, kdkFailBridge, "rendererClient", renderer);
            setField(BAPBridge.class, kdkFailBridge, "customRendererStarted", Boolean.TRUE);
            // nativeStopAttempted = false -> startup fails
            boolean kdkFailStarted = kdkFailBridge.onStart();
            check(!kdkFailStarted, "T9C: onStart returns false");
            check(!ScreenModule.isRouteActive(), "T9C: routeActive must be false");
            check(!ScreenModule.isPresentationActive(), "T9C: presentationActive must be false");
            check(!ScreenModule.isNavHidePending(), "T9C: navHidePending MUST be false despite visible KDK");
            check(!ScreenModule.isNavActive(), "T9C: navActive must be false");
            check(ScreenModule.getDesiredCtx() == 74, "T9C: desiredCtx MUST settle in 74 (no KDK hold)");

            // Cleanup KDK visibility
            com.luka.carplay.cluster.ClusterLayerController.onVcVisibility(false);
        }

        System.out.println("RouteContextRendererLifecycleTest: ALL 9 RENDERER/LIFECYCLE SUITES PASS (" + checks + " checks)");
    }
}
