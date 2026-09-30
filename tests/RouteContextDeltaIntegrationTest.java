import com.luka.carplay.bus.CarplayBus;
import com.luka.carplay.cluster.ClusterLayerController;
import com.luka.carplay.core.ScreenModule;
import com.luka.carplay.framework.Log;
import com.luka.carplay.rgd.BAPBridge;
import com.luka.carplay.rgd.RendererServer;
import com.luka.carplay.rgd.RouteGuidance;
import de.audi.atip.interapp.combi.bap.navi.CombiBAPServiceNavi;
import de.audi.atip.hmi.view.IDisplayManager;
import de.audi.tghu.fwhmi.IDisplayManagerKombiControl;

import java.lang.reflect.Field;
import java.lang.reflect.InvocationHandler;
import java.lang.reflect.Method;
import java.lang.reflect.Proxy;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * End-to-end integration tests connecting raw hook/iOS string deltas through:
 *   CarplayBus.parseText()
 *     -> RouteGuidance.parse()
 *     -> RouteGuidance.State dirty bits
 *     -> BAPBridge.update()
 *     -> ScreenModule (74 vs 80 context + rebinds)
 *     -> ClusterLayerController (opacity)
 *     -> RendererServer (maneuvers and clears)
 */
public final class RouteContextDeltaIntegrationTest {

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

        int count() {
            return reasons.size();
        }

        void clear() {
            reasons.clear();
        }

        String lastReason() {
            return reasons.isEmpty() ? null : (String) reasons.get(reasons.size() - 1);
        }
    }

    private static final class MockRenderer extends RendererServer {
        int clears = 0;
        int maneuvers = 0;
        int lastIcon = -1;
        int lastDirection = -1;
        int lastExitAngle = 0;
        int progressCalls = 0;

        public boolean sendClear() {
            clears++;
            return true;
        }

        public boolean sendBapProgressManeuver(int icon, int direction, int exitAngle,
                int drivingSide, int[] junctionAngles, int level, int mode, int perspective,
                boolean refresh, boolean snapToRoad, int progressState) {
            maneuvers++;
            lastIcon = icon;
            lastDirection = direction;
            lastExitAngle = exitAngle;
            return true;
        }

        public boolean isFrameReady() {
            return true;
        }

        public boolean sendProgress(int level, int mode, int progressState) {
            progressCalls++;
            return true;
        }
    }

    private static final class MockDisplayManagerHandler implements InvocationHandler {
        final Map opacity = new HashMap();

        public Object invoke(Object proxy, Method method, Object[] args) {
            String name = method.getName();
            if (name.equals("setOpacity")) {
                opacity.put(args[0], args[2]);
            }
            Class type = method.getReturnType();
            if (type == Boolean.TYPE) return Boolean.FALSE;
            if (type == Integer.TYPE) return Integer.valueOf(0);
            return null;
        }

        int opacity(int id) {
            Integer val = (Integer) opacity.get(Integer.valueOf(id));
            return val != null ? val.intValue() : 0;
        }
    }

    private static final class BapProxyHandler implements InvocationHandler {
        final List rgStatusHistory = new ArrayList();
        final List activeRgTypeHistory = new ArrayList();
        boolean throwOnRGStatus = false;
        int throwOnRGStatusTarget = -1;
        boolean throwOnActiveRGType = false;
        int throwOnActiveRGTypeTarget = -1;
        boolean throwOnceActiveRGType = false;

        public Object invoke(Object proxy, Method method, Object[] args) throws Throwable {
            String name = method.getName();
            if ("updateRGStatus".equals(name)) {
                if (args != null && args.length > 0) {
                    int val = ((Integer) args[0]).intValue();
                    if (throwOnRGStatus && (throwOnRGStatusTarget == -1 || throwOnRGStatusTarget == val)) {
                        throw new RuntimeException("Injected updateRGStatus failure");
                    }
                    rgStatusHistory.add(args[0]);
                }
            } else if ("updateActiveRGType".equals(name)) {
                if (args != null && args.length > 0) {
                    int val = ((Integer) args[0]).intValue();
                    if (throwOnActiveRGType && (throwOnActiveRGTypeTarget == -1 || throwOnActiveRGTypeTarget == val)) {
                        if (throwOnceActiveRGType) throwOnActiveRGType = false;
                        throw new RuntimeException("Injected updateActiveRGType failure");
                    }
                    activeRgTypeHistory.add(args[0]);
                }
            }
            Class type = method.getReturnType();
            if (type == Boolean.TYPE) return Boolean.FALSE;
            if (type == Integer.TYPE) return Integer.valueOf(0);
            return null;
        }
    }

    public static final class TestClusterService extends de.audi.tghu.navi.app.cluster.ClusterService {
        boolean rgiValid = false;
        final List rgiHistory = new ArrayList();
        de.audi.tghu.navi.app.command.DSIResponseContainer container;

        TestClusterService() {
            super(null, null, null, null, null, null);
        }

        public de.audi.tghu.navi.app.command.DSIResponseContainer getDSIResponseContainer() {
            if (container == null) {
                container = new de.audi.tghu.navi.app.command.DSIResponseContainer();
            }
            return container;
        }

        public void updateRGIString(short[] ashort) {
            rgiValid = (ashort != null && ashort.length > 0);
            if (rgiHistory != null) {
                rgiHistory.add(Boolean.valueOf(rgiValid));
            }
        }

        public void triggerRefreshRGIValid() {
        }
    }

    private static TestClusterService createClusterService() throws Exception {
        Field uf = sun.misc.Unsafe.class.getDeclaredField("theUnsafe");
        uf.setAccessible(true);
        sun.misc.Unsafe unsafe = (sun.misc.Unsafe) uf.get(null);
        TestClusterService cs = (TestClusterService) unsafe.allocateInstance(TestClusterService.class);
        setField(TestClusterService.class, cs, "rgiHistory", new ArrayList());
        return cs;
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

    private static BAPBridge createBridge(MockRenderer renderer) throws Exception {
        return createBridge(renderer, new BapProxyHandler(), null);
    }

    private static BAPBridge createBridge(MockRenderer renderer, BapProxyHandler bapHandler, TestClusterService cs) throws Exception {
        ScreenModule.setNavActive(false);
        BAPBridge bridge = new BAPBridge();
        CombiBAPServiceNavi service = (CombiBAPServiceNavi) Proxy.newProxyInstance(
            RouteContextDeltaIntegrationTest.class.getClassLoader(),
            new Class[]{CombiBAPServiceNavi.class},
            bapHandler != null ? bapHandler : new BapProxyHandler());
        setField(BAPBridge.class, bridge, "appConnectorNavi", service);
        setField(BAPBridge.class, bridge, "initialized", Boolean.TRUE);
        setField(BAPBridge.class, bridge, "bapSessionStarted", Boolean.TRUE);
        setField(BAPBridge.class, bridge, "nativeStopAttempted", Boolean.TRUE);
        /* These suites pin the approach-EXIT-on-reroute behaviour itself, so they run with the
         * reroute hold disabled (the hold is covered by RerouteHoldAndKdkVerifyTest). */
        setField(BAPBridge.class, bridge, "rerouteHoldMs", Integer.valueOf(0));
        if (cs != null) {
            setField(BAPBridge.class, bridge, "csRef", cs);
        }
        if (renderer != null) {
            setField(BAPBridge.class, bridge, "rendererClient", renderer);
            setField(BAPBridge.class, bridge, "customRendererStarted", Boolean.TRUE);
        }
        ScreenModule.setRouteActive(true);
        ScreenModule.setPresentationActive(false);
        return bridge;
    }

    private static void setSimulatedPresentationActive(boolean active) {
        ScreenModule.setPresentationActive(active);
    }

    private static void feed(RouteGuidance rg, Method parseMethod, RouteGuidance.State state,
                             BAPBridge bridge, String text) throws Exception {
        byte[] bytes = text.getBytes("UTF-8");
        CarplayBus.Data d = CarplayBus.parseText(bytes, bytes.length);
        parseMethod.invoke(rg, new Object[]{d});
        boolean published = bridge.update(state);
        if (published) {
            state.clearDirty();
        }
    }

    private static void permute(int[] arr, int k, List result) {
        if (k == arr.length) {
            int[] copy = new int[arr.length];
            System.arraycopy(arr, 0, copy, 0, arr.length);
            result.add(copy);
            return;
        }
        for (int i = k; i < arr.length; i++) {
            int temp = arr[k]; arr[k] = arr[i]; arr[i] = temp;
            permute(arr, k + 1, result);
            temp = arr[k]; arr[k] = arr[i]; arr[i] = temp;
        }
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

        Method parseMethod = RouteGuidance.class.getDeclaredMethod("parse", new Class[]{CarplayBus.Data.class});
        parseMethod.setAccessible(true);

        MockDisplayManagerHandler dmHandler = new MockDisplayManagerHandler();
        IDisplayManagerKombiControl dm = (IDisplayManagerKombiControl) Proxy.newProxyInstance(
            RouteContextDeltaIntegrationTest.class.getClassLoader(),
            new Class[]{IDisplayManagerKombiControl.class},
            dmHandler);
        ClusterLayerController.bind(dm, 1);
        ClusterLayerController.onVcPresentation(true);
        ClusterLayerController.onVcVisibility(false); // No KDK hide delay in standard context tests

        // ============================================================
        // Suite 1: Fragmented Reroute Sequence A (Delayed maneuver delivery, 483 m: just outside the 457 m city zone)
        // ============================================================
        {
            MockRenderer renderer = new MockRenderer();
            BAPBridge bridge = createBridge(renderer);
            RouteGuidance rg = new RouteGuidance();
            RouteGuidance.State state = (RouteGuidance.State) getField(RouteGuidance.class, rg, "state");
            collector.clear();
            setSimulatedPresentationActive(false);

            // Step 1: Initial active route at 200m
            feed(rg, parseMethod, state, bridge,
                "source_supports_rg:n:1\n" +
                "route_generation:n:100\n" +
                "route_state:n:1\n" +
                "maneuver_count:n:1\n" +
                "maneuver_list:s:0\n" +
                "m0_type:n:1\n" +
                "dist_maneuver_m:n:200\n");
            check(ScreenModule.isNavActive(), "S1.1: 200m enters approach (ctx 80)");
            check(collector.count() == 0, "S1.1: initial entry is clean 74->80, no rebind");
            check(renderer.maneuvers >= 1, "S1.1: renderer received maneuver");

            // Step 2: Route state 5 (rerouting begins)
            feed(rg, parseMethod, state, bridge, "route_state:n:5\n");
            check(!ScreenModule.isNavActive(), "S1.2: rerouting suppresses navActive (drops to 74)");
            check(renderer.clears >= 1, "S1.2: rerouting sent clear to renderer");
            check(collector.count() == 0, "S1.2: reroute entry triggers no rebind");

            // Step 3: New generation 101 arrives in isolated delta (no maneuver or distance yet)
            feed(rg, parseMethod, state, bridge, "route_generation:n:101\n");
            check(state.routeGeneration == 101, "S1.3: generation bumped to 101");
            check(state.routeState == 5, "S1.3: numeric routeState retained 5");
            check(state.routeStateGeneration == -1L, "S1.3: routeStateGeneration invalidated to -1");
            check(!ScreenModule.isNavActive(), "S1.3: still 74");
            check(collector.count() == 0, "S1.3: no premature rebind without maneuver");

            // Step 4..6: Fragmented maneuver headers arrive
            feed(rg, parseMethod, state, bridge, "maneuver_count:n:1\n");
            feed(rg, parseMethod, state, bridge, "maneuver_list:s:0\n");
            feed(rg, parseMethod, state, bridge, "m0_type:n:1\n");
            check(collector.count() == 0, "S1.4: fragmented parts trigger no rebind");

            // Step 7: Distance 483m (~0.3 mi, just outside the 457 m city boundary) arrives -> resolved OUTSIDE approach
            feed(rg, parseMethod, state, bridge, "dist_maneuver_m:n:483\n");
            check(!ScreenModule.isNavActive(), "S1.7: 483m stays in ctx 74");
            check(collector.count() == 0, "S1.7: 483m cancels pending rebind permanently, zero rebinds");

            // Step 8: Travel toward turn outside approach: 600m, 500m, 458m
            feed(rg, parseMethod, state, bridge, "dist_maneuver_m:n:600\n");
            check(!ScreenModule.isNavActive() && collector.count() == 0, "S1.8: 600m stays 74, no rebind");
            feed(rg, parseMethod, state, bridge, "dist_maneuver_m:n:500\n");
            check(!ScreenModule.isNavActive() && collector.count() == 0, "S1.8: 500m stays 74, no rebind");
            feed(rg, parseMethod, state, bridge, "dist_maneuver_m:n:458\n");
            check(!ScreenModule.isNavActive() && collector.count() == 0, "S1.8: 458m stays 74, no rebind");

            // Step 9: Reach 457m (city approach threshold)
            feed(rg, parseMethod, state, bridge, "dist_maneuver_m:n:457\n");
            check(ScreenModule.isNavActive(), "S1.9: 457m enters approach (ctx 80)");
            check(collector.count() == 0, "S1.9: clean 74->80 acquisition, ZERO forced rebinds!");
        }

        // ============================================================
        // Suite 2: Fragmented Reroute Sequence B (Inverted field ordering, 200 m approach)
        // ============================================================
        {
            MockRenderer renderer = new MockRenderer();
            BAPBridge bridge = createBridge(renderer);
            RouteGuidance rg = new RouteGuidance();
            RouteGuidance.State state = (RouteGuidance.State) getField(RouteGuidance.class, rg, "state");
            collector.clear();
            ClusterLayerController.onVcVisibility(true);
            setSimulatedPresentationActive(true); // Vehicle currently in ctx 80 with KDK visible

            // Establish baseline generation 101 at 200m
            feed(rg, parseMethod, state, bridge,
                "route_generation:n:101\n" +
                "route_state:n:1\n" +
                "maneuver_count:n:1\n" +
                "maneuver_list:s:0\n" +
                "m0_type:n:1\n" +
                "dist_maneuver_m:n:200\n");
            collector.clear();

            // Reverse order for generation 102:
            // 1. Generation arrives
            feed(rg, parseMethod, state, bridge, "route_generation:n:102\n");
            check(collector.count() == 0, "S2.1: generation alone does not rebind");

            // 2. Maneuver type
            feed(rg, parseMethod, state, bridge, "m0_type:n:1\n");
            check(collector.count() == 0, "S2.2: type alone does not rebind");

            // 3. Maneuver list
            feed(rg, parseMethod, state, bridge, "maneuver_list:s:0\n");
            check(collector.count() == 0, "S2.3: list alone does not rebind");

            // 4. Distance 200m completes primary maneuver criteria
            feed(rg, parseMethod, state, bridge, "dist_maneuver_m:n:200\n");
            check(collector.count() == 1, "S2.4: distance completes approach criteria -> exactly 1 rebind");
            check(collector.lastReason().indexOf("102") >= 0, "S2.4: rebind reason identifies gen 102");

            // 5. Maneuver count arrives -> must not re-trigger rebind
            feed(rg, parseMethod, state, bridge, "maneuver_count:n:1\n");
            check(collector.count() == 1, "S2.5: subsequent count delta does NOT re-trigger rebind");
            ClusterLayerController.onVcVisibility(false);
        }

        // ============================================================
        // Suite 3: 457 m (1500 ft) City Boundary Hysteresis with Unit Equivalence
        // ============================================================
        {
            BAPBridge bridge = createBridge(null);
            RouteGuidance rg = new RouteGuidance();
            RouteGuidance.State state = (RouteGuidance.State) getField(RouteGuidance.class, rg, "state");
            collector.clear();
            setSimulatedPresentationActive(false);

            feed(rg, parseMethod, state, bridge,
                "route_generation:n:110\n" +
                "route_state:n:1\n" +
                "maneuver_count:n:1\n" +
                "maneuver_list:s:0\n" +
                "m0_type:n:1\n" +
                "dist_maneuver_m:n:484\n");
            check(!ScreenModule.isNavActive(), "S3: 484m outside approach");

            feed(rg, parseMethod, state, bridge, "dist_maneuver_m:n:483\n");
            check(!ScreenModule.isNavActive(), "S3: 483m (just outside the 457m boundary) outside approach");

            feed(rg, parseMethod, state, bridge, "dist_maneuver_m:n:482\n");
            check(!ScreenModule.isNavActive(), "S3: 482m outside approach");

            feed(rg, parseMethod, state, bridge, "dist_maneuver_m:n:458\n");
            check(!ScreenModule.isNavActive(), "S3: 458m just outside city threshold");

            feed(rg, parseMethod, state, bridge, "dist_maneuver_m:n:457\n");
            check(ScreenModule.isNavActive(), "S3: 457m (1500 ft) enters city approach");

            // Hysteresis buffer is 50m -> stays active up to 507m
            feed(rg, parseMethod, state, bridge, "dist_maneuver_m:n:507\n");
            check(ScreenModule.isNavActive(), "S3: 507m retained by 50m hysteresis");

            feed(rg, parseMethod, state, bridge, "dist_maneuver_m:n:508\n");
            check(!ScreenModule.isNavActive(), "S3: 508m exits city approach");
        }

        // ============================================================
        // Suite 4: Stale Route-State Across Multiple Generations
        // ============================================================
        {
            BAPBridge bridge = createBridge(null);
            RouteGuidance rg = new RouteGuidance();
            RouteGuidance.State state = (RouteGuidance.State) getField(RouteGuidance.class, rg, "state");
            collector.clear();
            setSimulatedPresentationActive(true);

            // Gen 200: state 5
            feed(rg, parseMethod, state, bridge,
                "route_generation:n:200\nroute_state:n:5\nmaneuver_count:n:1\nmaneuver_list:s:0\nm0_type:n:1\ndist_maneuver_m:n:200\n");
            check(!ScreenModule.isNavActive(), "S4: gen 200 state 5 is rerouting");

            // Gen 201: no route_state, 483m
            feed(rg, parseMethod, state, bridge,
                "route_generation:n:201\nmaneuver_count:n:1\nmaneuver_list:s:0\nm0_type:n:1\ndist_maneuver_m:n:483\n");
            check(state.routeState == 5 && state.routeStateGeneration == -1L, "S4: gen 201 unauthenticated state");
            check(!ScreenModule.isNavActive(), "S4: gen 201 outside approach (74)");

            // Gen 202: no route_state, 200m -> approach + rebind
            feed(rg, parseMethod, state, bridge,
                "route_generation:n:202\nmaneuver_count:n:1\nmaneuver_list:s:0\nm0_type:n:1\ndist_maneuver_m:n:200\n");
            check(state.routeState == 5 && state.routeStateGeneration == -1L, "S4: gen 202 unauthenticated state");
            check(ScreenModule.isNavActive(), "S4: gen 202 inside approach (80)");

            // Gen 203: explicit state 5, 200m -> authoritative rerouting
            feed(rg, parseMethod, state, bridge,
                "route_generation:n:203\nroute_state:n:5\nmaneuver_count:n:1\nmaneuver_list:s:0\nm0_type:n:1\ndist_maneuver_m:n:200\n");
            check(state.routeState == 5 && state.routeStateGeneration == 203L, "S4: gen 203 authenticated state 5");
            check(!ScreenModule.isNavActive(), "S4: gen 203 explicit rerouting suppresses navActive");

            // Gen 204: no route_state, 200m -> unauthenticated state, normal approach
            feed(rg, parseMethod, state, bridge,
                "route_generation:n:204\nmaneuver_count:n:1\nmaneuver_list:s:0\nm0_type:n:1\ndist_maneuver_m:n:200\n");
            check(state.routeState == 5 && state.routeStateGeneration == -1L, "S4: gen 204 unauthenticated state");
            check(ScreenModule.isNavActive(), "S4: gen 204 recovers to active approach");
        }

        // ============================================================
        // Suite 5: Absurd Repeated Event Jitter ("One Rebind Per Generation")
        // ============================================================
        {
            BAPBridge bridge = createBridge(null);
            RouteGuidance rg = new RouteGuidance();
            RouteGuidance.State state = (RouteGuidance.State) getField(RouteGuidance.class, rg, "state");
            collector.clear();
            setSimulatedPresentationActive(true);

            feed(rg, parseMethod, state, bridge,
                "route_generation:n:210\nroute_state:n:1\nmaneuver_count:n:1\nmaneuver_list:s:0\nm0_type:n:1\ndist_maneuver_m:n:200\n");
            collector.clear();

            // Reroute to gen 211 at 200m
            feed(rg, parseMethod, state, bridge,
                "route_generation:n:211\nmaneuver_count:n:1\nmaneuver_list:s:0\nm0_type:n:1\ndist_maneuver_m:n:200\n");
            check(collector.count() == 1, "S5: initial reroute rebind fires");

            // Send 30 rapid distance fluctuations
            int[] jitters = new int[]{199, 200, 198, 250, 201, 180, 220, 150, 457, 456, 200, 190, 185, 180, 175, 170};
            for (int i = 0; i < jitters.length; i++) {
                feed(rg, parseMethod, state, bridge, "dist_maneuver_m:n:" + jitters[i] + "\n");
                check(collector.count() == 1, "S5: jitter distance " + jitters[i] + " must NOT re-arm rebind");
            }

            // New generation 212 at 200m
            feed(rg, parseMethod, state, bridge,
                "route_generation:n:212\nmaneuver_count:n:1\nmaneuver_list:s:0\nm0_type:n:1\ndist_maneuver_m:n:200\n");
            check(collector.count() == 2, "S5: generation bump to 212 triggers second rebind");
        }

        // ============================================================
        // Suite 6: Explicit Pending-Rebind Cancellation Latch
        // ============================================================
        {
            BAPBridge bridge = createBridge(null);
            RouteGuidance rg = new RouteGuidance();
            RouteGuidance.State state = (RouteGuidance.State) getField(RouteGuidance.class, rg, "state");
            collector.clear();
            setSimulatedPresentationActive(true);

            // Establish gen 300 active in 80
            feed(rg, parseMethod, state, bridge,
                "route_generation:n:300\nroute_state:n:1\nmaneuver_count:n:1\nmaneuver_list:s:0\nm0_type:n:1\ndist_maneuver_m:n:200\n");
            collector.clear();

            // Gen 301 arrives without maneuver -> pending rebind latched
            feed(rg, parseMethod, state, bridge, "route_generation:n:301\n");
            check(collector.count() == 0, "S6: gen 301 alone does not rebind yet");

            // Distance 660m arrives -> outside approach -> pending rebind cancelled
            feed(rg, parseMethod, state, bridge,
                "maneuver_count:n:1\nmaneuver_list:s:0\nm0_type:n:1\ndist_maneuver_m:n:660\n");
            check(!ScreenModule.isNavActive(), "S6: drops to 74");
            check(collector.count() == 0, "S6: 660m cancels pending rebind");

            // Travel to 200m -> enters approach normally via setNavActive(true)
            feed(rg, parseMethod, state, bridge, "dist_maneuver_m:n:200\n");
            check(ScreenModule.isNavActive(), "S6: enters approach at 200m");
            check(collector.count() == 0, "S6: stale pending rebind did NOT fire! Count remains 0.");
        }

        // ============================================================
        // Suite 7: Combined Renderer / Context Lifecycle Integration
        // ============================================================
        {
            MockRenderer renderer = new MockRenderer();
            BAPBridge bridge = createBridge(renderer);
            RouteGuidance rg = new RouteGuidance();
            RouteGuidance.State state = (RouteGuidance.State) getField(RouteGuidance.class, rg, "state");
            collector.clear();
            setSimulatedPresentationActive(false);

            // 1. Enter approach at 200m (left turn)
            feed(rg, parseMethod, state, bridge,
                "route_generation:n:400\nroute_state:n:1\nmaneuver_count:n:1\nmaneuver_list:s:0\nm0_type:n:1\nm0_turn_angle:n:-90\nm0_junction_type:n:0\ndist_maneuver_m:n:200\n");
            check(ScreenModule.isNavActive(), "S7.1: approach active");
            check(renderer.maneuvers == 1, "S7.1: renderer received maneuver");
            check(renderer.lastIcon == 2, "S7.1: ICON_TURN");
            check(renderer.lastExitAngle < 0, "S7.1: left turn negative exit angle");

            // 2. Reroute occurs
            int preClears = renderer.clears;
            feed(rg, parseMethod, state, bridge, "route_state:n:5\n");
            check(!ScreenModule.isNavActive(), "S7.2: reroute left approach");
            check(renderer.clears > preClears, "S7.2: reroute invoked sendClear() on renderer");

            // 3. New route outside approach (483m) with right turn
            feed(rg, parseMethod, state, bridge,
                "route_generation:n:401\nmaneuver_count:n:1\nmaneuver_list:s:0\nm0_type:n:2\nm0_turn_angle:n:90\nm0_junction_type:n:0\ndist_maneuver_m:n:483\n");
            check(!ScreenModule.isNavActive(), "S7.3: 483m stays 74");

            // 4. Approach at 200m with new maneuver (right turn)
            feed(rg, parseMethod, state, bridge, "dist_maneuver_m:n:200\n");
            check(ScreenModule.isNavActive(), "S7.4: enters approach");
            check(renderer.maneuvers >= 2, "S7.4: renderer received new maneuver");
            check(renderer.lastIcon == 2, "S7.4: ICON_TURN");
            check(renderer.lastExitAngle > 0, "S7.4: new maneuver right turn positive exit angle (not stale left turn)");
        }

        // ============================================================
        // Suite 8: ScreenModule Concurrency & Inter-Route Races
        // ============================================================
        {
            Field rpf = ScreenModule.class.getDeclaredField("rebindPending");
            rpf.setAccessible(true);
            Field rrf = ScreenModule.class.getDeclaredField("rebindReason");
            rrf.setAccessible(true);

            // Test 8.1: Request Coalescing
            ScreenModule.requestClusterContextRebind("Reason-A");
            check(rpf.getBoolean(null), "S8.1: rebindPending is true after request A");
            check("Reason-A".equals(rrf.get(null)), "S8.1: rebindReason is Reason-A");

            ScreenModule.requestClusterContextRebind("Reason-B");
            check(rpf.getBoolean(null), "S8.1: rebindPending remains true after request B");
            check("Reason-B".equals(rrf.get(null)), "S8.1: rebindReason updated to Reason-B");

            // Test 8.2: Inter-Route Leakage Prevention on Route End
            ScreenModule.setNavActive(false);
            check(!rpf.getBoolean(null), "S8.2: setNavActive(false) cleared rebindPending");
            check("".equals(rrf.get(null)), "S8.2: setNavActive(false) cleared rebindReason");

            // Test 8.3: In-Flight Bounce Race (Concurrent Request Non-Swallowing)
            final List switches = new ArrayList();
            final boolean[] requestedInFlight = new boolean[]{false};
            IDisplayManager mockDm = (IDisplayManager) Proxy.newProxyInstance(
                RouteContextDeltaIntegrationTest.class.getClassLoader(),
                new Class[]{IDisplayManager.class, IDisplayManagerKombiControl.class},
                new InvocationHandler() {
                    public Object invoke(Object proxy, Method method, Object[] args) {
                        if (method.getName().equals("switchContext")) {
                            int ctx = ((Integer) args[0]).intValue();
                            switches.add(new Integer(ctx));
                            if (ctx == 72 && !requestedInFlight[0]) {
                                requestedInFlight[0] = true;
                                ScreenModule.requestClusterContextRebind("Reason-In-Flight-B");
                            }
                        }
                        Class type = method.getReturnType();
                        if (type == Boolean.TYPE) return Boolean.FALSE;
                        if (type == Integer.TYPE) return Integer.valueOf(0);
                        return null;
                    }
                });

            Field dmf = ScreenModule.class.getDeclaredField("dm");
            dmf.setAccessible(true);
            Field desCf = ScreenModule.class.getDeclaredField("desiredCtx");
            desCf.setAccessible(true);
            Field curCf = ScreenModule.class.getDeclaredField("currentCtx");
            curCf.setAccessible(true);
            Field lockF = ScreenModule.class.getDeclaredField("LOCK");
            lockF.setAccessible(true);
            Object lockObj = lockF.get(null);

            // Set up active cluster session with our mockDm
            final ScreenModule sm = new ScreenModule();
            synchronized (lockObj) {
                dmf.set(sm, mockDm);
                desCf.setInt(null, 80);
                curCf.setInt(null, 80);
            }

            // Start worker thread
            Field wf = ScreenModule.class.getDeclaredField("worker");
            wf.setAccessible(true);
            final Method swLoop = ScreenModule.class.getDeclaredMethod("switchLoop", new Class[0]);
            swLoop.setAccessible(true);
            Thread worker = new Thread(new Runnable() {
                public void run() {
                    try { swLoop.invoke(sm, new Object[0]); }
                    catch (Exception ignored) {}
                }
            }, "test-cluster-switch");
            worker.setDaemon(true);
            worker.start();
            wf.set(sm, worker);

            // Issue rebind A
            ScreenModule.requestClusterContextRebind("Reason-A");

            // Wait up to 1000ms for both bounces (A and in-flight B) to execute
            long deadline = System.currentTimeMillis() + 1000;
            while (switches.size() < 4 && System.currentTimeMillis() < deadline) {
                Thread.sleep(20);
            }

            // Verify both bounces occurred: [72, 80, 72, 80]
            check(switches.size() >= 4, "S8.3: both rebinds executed (switches=" + switches + ")");
            check(((Integer) switches.get(0)).intValue() == 72 && ((Integer) switches.get(1)).intValue() == 80,
                "S8.3: first rebind executed 72->80");
            check(((Integer) switches.get(2)).intValue() == 72 && ((Integer) switches.get(3)).intValue() == 80,
                "S8.3: second in-flight rebind was NOT swallowed, executed 72->80");

            // Clean up: reset dm
            synchronized (lockObj) {
                dmf.set(sm, null);
                desCf.setInt(null, 74);
                curCf.setInt(null, 74);
            }
        }

        // ============================================================
        // Suite 9: Stale / Out-of-Order Generation Delivery
        // ============================================================
        {
            BAPBridge bridge = createBridge(null);
            RouteGuidance rg = new RouteGuidance();
            RouteGuidance.State state = (RouteGuidance.State) getField(RouteGuidance.class, rg, "state");
            collector.clear();
            setSimulatedPresentationActive(true);

            // Establish gen 100 at 200m
            feed(rg, parseMethod, state, bridge,
                "route_generation:n:100\nroute_state:n:1\nmaneuver_count:n:1\nmaneuver_list:s:0\nm0_type:n:1\ndist_maneuver_m:n:200\n");
            check(state.routeGeneration == 100, "S9.1: gen 100 established");

            // Bump to gen 101 at 200m
            feed(rg, parseMethod, state, bridge,
                "route_generation:n:101\nroute_state:n:1\nmaneuver_count:n:1\nmaneuver_list:s:0\nm0_type:n:1\ndist_maneuver_m:n:200\n");
            check(state.routeGeneration == 101, "S9.2: gen 101 established");
            collector.clear();

            // Stale old packet with gen 100 arrives!
            feed(rg, parseMethod, state, bridge,
                "route_generation:n:100\nmaneuver_count:n:1\nmaneuver_list:s:0\nm0_type:n:1\ndist_maneuver_m:n:200\n");
            check(state.routeGeneration == 101, "S9.3: out-of-order stale generation 100 must not regress active generation 101");
        }

        // ============================================================
        // Suite 10: Late route_state=5 Arriving After Generation & Maneuver
        // ============================================================
        {
            BAPBridge bridge = createBridge(null);
            RouteGuidance rg = new RouteGuidance();
            RouteGuidance.State state = (RouteGuidance.State) getField(RouteGuidance.class, rg, "state");
            collector.clear();
            setSimulatedPresentationActive(false);

            // Gen 201 arrives with maneuver at 200m, but NO route_state
            feed(rg, parseMethod, state, bridge,
                "route_generation:n:201\nmaneuver_count:n:1\nmaneuver_list:s:0\nm0_type:n:1\ndist_maneuver_m:n:200\n");
            check(ScreenModule.isNavActive(), "S10.1: enters approach at 200m");
            check(state.routeStateGeneration == -1L, "S10.1: routeStateGeneration is unauthenticated");

            // Late route_state=5 arrives for gen 201
            feed(rg, parseMethod, state, bridge, "route_state:n:5\n");
            check(state.routeState == 5, "S10.2: routeState is 5");
            check(state.routeStateGeneration == 201L, "S10.2: routeState authenticated for gen 201");
            check(!ScreenModule.isNavActive(), "S10.2: late route_state 5 legitimately enters rerouting, drops to 74");

            // Recovery: route_state=1 arrives
            feed(rg, parseMethod, state, bridge, "route_state:n:1\n");
            check(state.routeState == 1 && state.routeStateGeneration == 201L, "S10.3: state 1 authenticated");
            check(ScreenModule.isNavActive(), "S10.3: recovered from reroute to active approach");
        }

        // ============================================================
        // Suite 11: Generation 300 -> 301 (Reroute) -> 301 (Recovery) -> 302 (No State)
        // ============================================================
        {
            BAPBridge bridge = createBridge(null);
            RouteGuidance rg = new RouteGuidance();
            RouteGuidance.State state = (RouteGuidance.State) getField(RouteGuidance.class, rg, "state");
            collector.clear();
            setSimulatedPresentationActive(false);

            // Gen 300 state 1
            feed(rg, parseMethod, state, bridge,
                "route_generation:n:300\nroute_state:n:1\nmaneuver_count:n:1\nmaneuver_list:s:0\nm0_type:n:1\ndist_maneuver_m:n:200\n");
            check(ScreenModule.isNavActive(), "S11.1: gen 300 active");

            // Gen 301 explicit state 5
            feed(rg, parseMethod, state, bridge,
                "route_generation:n:301\nroute_state:n:5\nmaneuver_count:n:1\nmaneuver_list:s:0\nm0_type:n:1\ndist_maneuver_m:n:200\n");
            check(!ScreenModule.isNavActive(), "S11.2: gen 301 rerouting active");

            // Gen 301 state 1 recovery
            feed(rg, parseMethod, state, bridge, "route_state:n:1\n");
            check(ScreenModule.isNavActive(), "S11.3: gen 301 recovered to approach");

            // Gen 302 arrives WITHOUT route_state at 200m
            feed(rg, parseMethod, state, bridge,
                "route_generation:n:302\nmaneuver_count:n:1\nmaneuver_list:s:0\nm0_type:n:1\ndist_maneuver_m:n:200\n");
            check(state.routeGeneration == 302L, "S11.4: gen 302 active");
            check(state.routeStateGeneration == -1L, "S11.4: routeStateGeneration reset for gen 302");
            check(ScreenModule.isNavActive(), "S11.4: gen 302 enters approach normally without stale rerouting");
        }

        // ============================================================
        // Suite 12: Incomplete Maneuver Permutations
        // ============================================================
        {
            // Permutation 1: gen -> count -> dist -> type -> list
            BAPBridge bridge = createBridge(null);
            RouteGuidance rg = new RouteGuidance();
            RouteGuidance.State state = (RouteGuidance.State) getField(RouteGuidance.class, rg, "state");
            collector.clear();
            setSimulatedPresentationActive(false);

            feed(rg, parseMethod, state, bridge, "route_generation:n:501\nroute_state:n:1\n");
            check(!ScreenModule.isNavActive(), "S12.1: gen+state alone not approach");
            feed(rg, parseMethod, state, bridge, "maneuver_count:n:1\n");
            check(!ScreenModule.isNavActive(), "S12.1: count alone not approach");
            feed(rg, parseMethod, state, bridge, "dist_maneuver_m:n:200\n");
            check(!ScreenModule.isNavActive(), "S12.1: dist without type/list not approach");
            feed(rg, parseMethod, state, bridge, "m0_type:n:1\n");
            check(!ScreenModule.isNavActive(), "S12.1: type without list not approach");
            feed(rg, parseMethod, state, bridge, "maneuver_list:s:0\n");
            check(ScreenModule.isNavActive(), "S12.1: list completes maneuver -> enters approach");

            // Permutation 2: gen -> list -> dist -> count -> type
            feed(rg, parseMethod, state, bridge, "route_generation:n:502\n");
            check(!ScreenModule.isNavActive(), "S12.2: new gen clears approach");
            feed(rg, parseMethod, state, bridge, "maneuver_list:s:0\n");
            check(!ScreenModule.isNavActive(), "S12.2: list alone not approach");
            feed(rg, parseMethod, state, bridge, "dist_maneuver_m:n:200\n");
            check(!ScreenModule.isNavActive(), "S12.2: dist alone not approach");
            feed(rg, parseMethod, state, bridge, "maneuver_count:n:1\n");
            check(!ScreenModule.isNavActive(), "S12.2: count alone not approach");
            feed(rg, parseMethod, state, bridge, "m0_type:n:1\n");
            check(ScreenModule.isNavActive(), "S12.2: type completes maneuver -> enters approach");
        }

        // ============================================================
        // Suite 13: Invalid Maneuver Types
        // ============================================================
        {
            BAPBridge bridge = createBridge(null);
            RouteGuidance rg = new RouteGuidance();
            RouteGuidance.State state = (RouteGuidance.State) getField(RouteGuidance.class, rg, "state");
            collector.clear();
            setSimulatedPresentationActive(false);

            int[] invalidTypes = new int[]{-1, 54, 100, 255};
            for (int i = 0; i < invalidTypes.length; i++) {
                long gen = 600L + i;
                feed(rg, parseMethod, state, bridge,
                    "route_generation:n:" + gen + "\nroute_state:n:1\nmaneuver_count:n:1\nmaneuver_list:s:0\nm0_type:n:"
                    + invalidTypes[i] + "\ndist_maneuver_m:n:200\n");
                check(!ScreenModule.isNavActive(), "S13: invalid type " + invalidTypes[i] + " must not enter approach");
            }

            // Transition from invalid type 54 to valid type 1 at same generation
            feed(rg, parseMethod, state, bridge, "m0_type:n:1\n");
            check(ScreenModule.isNavActive(), "S13: updating to valid type enters approach");
        }

        // ============================================================
        // Suite 14: All 7 Highway Maneuver Types at Boundaries
        // ============================================================
        {
            int[] highwayTypes = new int[]{8, 9, 22, 23, 51, 52, 53};
            for (int i = 0; i < highwayTypes.length; i++) {
                int ht = highwayTypes[i];
                BAPBridge bridge = createBridge(null);
                RouteGuidance rg = new RouteGuidance();
                RouteGuidance.State state = (RouteGuidance.State) getField(RouteGuidance.class, rg, "state");
                collector.clear();
                setSimulatedPresentationActive(false);
                long gen = 700L + i;

                // 1601m: outside approach
                feed(rg, parseMethod, state, bridge,
                    "route_generation:n:" + gen + "\nroute_state:n:1\nmaneuver_count:n:1\nmaneuver_list:s:0\nm0_type:n:"
                    + ht + "\ndist_maneuver_m:n:1601\n");
                check(!ScreenModule.isNavActive(), "S14 type " + ht + ": 1601m is outside approach");

                // 1600m: enters approach
                feed(rg, parseMethod, state, bridge, "dist_maneuver_m:n:1600\n");
                check(ScreenModule.isNavActive(), "S14 type " + ht + ": 1600m enters approach");

                // 1650m: retained by 50m hysteresis
                feed(rg, parseMethod, state, bridge, "dist_maneuver_m:n:1650\n");
                check(ScreenModule.isNavActive(), "S14 type " + ht + ": 1650m retained by hysteresis");

                // 1651m: exits approach
                feed(rg, parseMethod, state, bridge, "dist_maneuver_m:n:1651\n");
                check(!ScreenModule.isNavActive(), "S14 type " + ht + ": 1651m exits approach");
            }
        }

        // ============================================================
        // Suite 15: Missing route_generation for Entire Route
        // ============================================================
        {
            BAPBridge bridge = createBridge(null);
            RouteGuidance rg = new RouteGuidance();
            RouteGuidance.State state = (RouteGuidance.State) getField(RouteGuidance.class, rg, "state");
            collector.clear();
            setSimulatedPresentationActive(false);

            // Feed route with NO route_generation ever
            feed(rg, parseMethod, state, bridge,
                "source_supports_rg:n:1\nroute_state:n:1\nmaneuver_count:n:1\nmaneuver_list:s:0\nm0_type:n:1\ndist_maneuver_m:n:200\n");
            check(state.routeGeneration == -1L, "S15.1: routeGeneration remains -1");
            check(ScreenModule.isNavActive(), "S15.1: approach opens even without route_generation");

            // Reroute occurs
            feed(rg, parseMethod, state, bridge, "route_state:n:5\n");
            check(!ScreenModule.isNavActive(), "S15.2: reroute recognized even without generation");

            // Recover
            feed(rg, parseMethod, state, bridge, "route_state:n:1\n");
            check(ScreenModule.isNavActive(), "S15.3: recovered to approach");
        }

        // ============================================================
        // Suite 16: Dirty-Mask Failure & Sticky Replay
        // ============================================================
        {
            BAPBridge bridge = createBridge(null);
            RouteGuidance rg = new RouteGuidance();
            RouteGuidance.State state = (RouteGuidance.State) getField(RouteGuidance.class, rg, "state");
            collector.clear();
            setSimulatedPresentationActive(false);

            // Send full delta into parser
            byte[] bytes = ("route_generation:n:900\nroute_state:n:1\nmaneuver_count:n:1\nmaneuver_list:s:0\nm0_type:n:1\ndist_maneuver_m:n:200\n").getBytes("UTF-8");
            parseMethod.invoke(rg, new Object[]{CarplayBus.parseText(bytes, bytes.length)});

            // Simulate BAPBridge.update() failing or not being called (dirty flags NOT cleared)
            int dirtyBefore = state.dirtyMask;
            check(dirtyBefore != 0, "S16: dirty mask populated");

            // Now send a distance-only update
            byte[] dBytes = "dist_maneuver_m:n:190\n".getBytes("UTF-8");
            parseMethod.invoke(rg, new Object[]{CarplayBus.parseText(dBytes, dBytes.length)});

            // Check that previous uncommitted dirty flags were preserved!
            check((state.dirtyMask & RouteGuidance.State.DIRTY_ROUTE_STATE) != 0,
                "S16: uncommitted DIRTY_ROUTE_STATE preserved across distance update");
            check((state.dirtyMask & RouteGuidance.State.DIRTY_MANEUVER_LIST) != 0,
                "S16: uncommitted DIRTY_MANEUVER_LIST preserved across distance update");

            // Now publish succeeds -> dirty flags cleared
            boolean published = bridge.update(state);
            if (published) state.clearDirty();
            check(state.dirtyMask == 0, "S16: dirty mask cleared after successful publish");
            check(ScreenModule.isNavActive(), "S16: approach active");
        }

        // ============================================================
        // Suite 17: Same-generation maneuver replacement with index/list reordering
        // ============================================================
        {
            MockRenderer renderer = new MockRenderer();
            BAPBridge bridge = createBridge(renderer);
            RouteGuidance rg = new RouteGuidance();
            RouteGuidance.State state = (RouteGuidance.State) getField(RouteGuidance.class, rg, "state");
            collector.clear();
            setSimulatedPresentationActive(false);

            feed(rg, parseMethod, state, bridge,
                "source_supports_rg:n:1\n" +
                "route_generation:n:500\n" +
                "route_state:n:1\n" +
                "maneuver_count:n:3\n" +
                "maneuver_list:s:0,1,2\n" +
                "m0_type:n:1\nm0_turn_angle:n:-90\nm0_junction_type:n:0\n" +
                "m1_type:n:2\nm1_turn_angle:n:90\nm1_junction_type:n:0\n" +
                "m2_type:n:3\nm2_junction_type:n:0\n" +
                "dist_maneuver_m:n:200\n");

            check(ScreenModule.isNavActive(), "S17: approach active at 200m");
            check(collector.count() == 0, "S17: initial approach transition does not request rebind");
            check(renderer.maneuvers > 0, "S17: initial maneuver rendered");
            int initialManeuvers = renderer.maneuvers;
            int initialExitAngle = renderer.lastExitAngle;
            check(initialExitAngle < 0, "S17: initial exit angle is negative (left turn)");

            // Reorder list to [1, 2, 0]: Slot 1 (right turn) becomes the new primary!
            feed(rg, parseMethod, state, bridge,
                "maneuver_list:s:1,2,0\n");

            check(ScreenModule.isNavActive(), "S17: approach remains active after reordering");
            check(collector.count() == 0, "S17: zero rebinds on same-generation reorder");
            check(renderer.maneuvers > initialManeuvers, "S17: new primary rendered to renderer");
            check(renderer.lastExitAngle != initialExitAngle, "S17: renderer exit angle updated to slot 1");
            check(renderer.lastExitAngle > 0, "S17: slot 1 exit angle is positive (right turn)");

            // Verify old primary (slot 0) never repaints: send a distance tick
            feed(rg, parseMethod, state, bridge, "dist_maneuver_m:n:180\n");
            check(renderer.lastExitAngle > 0, "S17: primary exit angle remained slot 1 on distance tick");
        }

        // ============================================================
        // Suite 18: Dynamic City <-> Highway Flip Without Generation Change
        // ============================================================
        {
            BAPBridge bridge = createBridge(null);
            RouteGuidance rg = new RouteGuidance();
            RouteGuidance.State state = (RouteGuidance.State) getField(RouteGuidance.class, rg, "state");
            collector.clear();
            setSimulatedPresentationActive(false);

            // Gen 550, city type 1 @ 600m (>457m city threshold) -> NOT in approach
            feed(rg, parseMethod, state, bridge,
                "source_supports_rg:n:1\n" +
                "route_generation:n:550\n" +
                "route_state:n:1\n" +
                "maneuver_count:n:1\n" +
                "maneuver_list:s:0\n" +
                "m0_type:n:1\n" +
                "dist_maneuver_m:n:600\n");
            check(!ScreenModule.isNavActive(), "S18: city type 1 @ 600m is outside approach (threshold 457m)");

            // Dynamic update: type becomes highway (8) at the same 600m distance and same generation
            feed(rg, parseMethod, state, bridge, "m0_type:n:8\n");
            check(ScreenModule.isNavActive(), "S18: highway type 8 @ 600m enters approach (threshold 1609m)");

            // Distance increases to 1700m (>1609m highway threshold) -> exits approach
            feed(rg, parseMethod, state, bridge, "dist_maneuver_m:n:1700\n");
            check(!ScreenModule.isNavActive(), "S18: highway type 8 @ 1700m exits approach");

            // Update type to city (type 1) at 1700m -> remains outside approach
            feed(rg, parseMethod, state, bridge, "m0_type:n:1\n");
            check(!ScreenModule.isNavActive(), "S18: city type 1 @ 1700m remains outside approach");

            // Distance decreases to 250m (<=457m city threshold) -> enters approach
            feed(rg, parseMethod, state, bridge, "dist_maneuver_m:n:250\n");
            check(ScreenModule.isNavActive(), "S18: city type 1 @ 250m enters approach");
        }

        // ============================================================
        // Suite 19: Lane-guidance Stale Data Across Reroute
        // ============================================================
        {
            BAPBridge bridge = createBridge(null);
            RouteGuidance rg = new RouteGuidance();
            RouteGuidance.State state = (RouteGuidance.State) getField(RouteGuidance.class, rg, "state");
            collector.clear();
            setSimulatedPresentationActive(false);

            // Route A (gen 600) with active lane guidance
            feed(rg, parseMethod, state, bridge,
                "source_supports_rg:n:1\n" +
                "route_generation:n:600\n" +
                "route_state:n:1\n" +
                "maneuver_count:n:1\n" +
                "maneuver_list:s:0\n" +
                "m0_type:n:1\n" +
                "dist_maneuver_m:n:200\n" +
                "lane_guidance_showing:n:1\n" +
                "lane_guidance_total:n:3\n" +
                "lane_guidance_index:n:0\n" +
                "lane_guidance_slot:n:0\n");

            check(state.laneGuidanceShowing == 1, "S19: Route A laneGuidanceShowing is 1");
            check(state.laneGuidanceSlot == 0, "S19: Route A laneGuidanceSlot is 0");
            check(state.laneGuidanceTotal == 3, "S19: Route A laneGuidanceTotal is 3");

            // Reroute to Route B (gen 601) with NO lane guidance in packet
            feed(rg, parseMethod, state, bridge,
                "route_generation:n:601\n" +
                "route_state:n:1\n" +
                "maneuver_count:n:1\n" +
                "maneuver_list:s:0\n" +
                "m0_type:n:1\n" +
                "dist_maneuver_m:n:200\n");

            check(state.laneGuidanceShowing == -1, "S19: Route B cleared laneGuidanceShowing");
            check(state.laneGuidanceSlot == -1, "S19: Route B cleared laneGuidanceSlot");
            check(state.laneGuidanceIndex == -1, "S19: Route B cleared laneGuidanceIndex");
            check(state.laneGuidanceTotal == -1, "S19: Route B cleared laneGuidanceTotal");

            // Now feed Route B lane guidance
            feed(rg, parseMethod, state, bridge,
                "lane_guidance_showing:n:1\n" +
                "lane_guidance_total:n:4\n" +
                "lane_guidance_index:n:2\n" +
                "lane_guidance_slot:n:0\n");

            check(state.laneGuidanceShowing == 1, "S19: Route B received new laneGuidanceShowing");
            check(state.laneGuidanceTotal == 4, "S19: Route B received new laneGuidanceTotal");
            check(state.laneGuidanceIndex == 2, "S19: Route B received new laneGuidanceIndex");
        }

        // ============================================================
        // Suite 20: Route Text Stale-state Across Reroute
        // ============================================================
        {
            BAPBridge bridge = createBridge(null);
            RouteGuidance rg = new RouteGuidance();
            RouteGuidance.State state = (RouteGuidance.State) getField(RouteGuidance.class, rg, "state");
            collector.clear();
            setSimulatedPresentationActive(false);

            // Route A (gen 700) with road and destination
            feed(rg, parseMethod, state, bridge,
                "source_supports_rg:n:1\n" +
                "route_generation:n:700\n" +
                "route_state:n:1\n" +
                "maneuver_count:n:1\n" +
                "maneuver_list:s:0\n" +
                "m0_type:n:1\n" +
                "dist_maneuver_m:n:200\n" +
                "current_road:s:Main Street\n" +
                "destination:s:Airport\n");

            check("Main Street".equals(state.currentRoad), "S20: Route A currentRoad is Main Street");
            check("Airport".equals(state.destination), "S20: Route A destination is Airport");

            // Reroute to Route B (gen 701) with NO road or destination keys
            feed(rg, parseMethod, state, bridge,
                "route_generation:n:701\n" +
                "route_state:n:1\n" +
                "maneuver_count:n:1\n" +
                "maneuver_list:s:0\n" +
                "m0_type:n:1\n" +
                "dist_maneuver_m:n:200\n");

            check(state.currentRoad == null, "S20: Route B cleared currentRoad");
            check(state.destination == null, "S20: Route B cleared destination");

            // Now feed Route B road and destination
            feed(rg, parseMethod, state, bridge,
                "current_road:s:Oak Avenue\n" +
                "destination:s:Home\n");

            check("Oak Avenue".equals(state.currentRoad), "S20: Route B received Oak Avenue");
            check("Home".equals(state.destination), "S20: Route B received Home");
        }

        // ============================================================
        // Suite 21: Exhaustive 720-Permutation (6!) Individual-Field Arrival Order
        // ============================================================
        {
            String[] fields = new String[]{
                "maneuver_count:n:1\n",
                "maneuver_list:s:0\n",
                "m0_type:n:1\n",
                "m0_turn_angle:n:90\n",
                "m0_junction_type:n:0\n",
                "dist_maneuver_m:n:200\n"
            };
            List permutations = new ArrayList();
            permute(new int[]{0, 1, 2, 3, 4, 5}, 0, permutations);
            check(permutations.size() == 720, "S21: exactly 720 permutations (6!)");

            for (int i = 0; i < permutations.size(); i++) {
                int[] p = (int[]) permutations.get(i);
                BAPBridge bridge = createBridge(null);
                RouteGuidance rg = new RouteGuidance();
                RouteGuidance.State state = (RouteGuidance.State) getField(RouteGuidance.class, rg, "state");
                collector.clear();
                setSimulatedPresentationActive(false);

                // Pre-established route session
                feed(rg, parseMethod, state, bridge, "source_supports_rg:n:1\nroute_generation:n:750\nroute_state:n:1\n");

                // Feed each maneuver field in this permutation's order
                for (int j = 0; j < p.length; j++) {
                    feed(rg, parseMethod, state, bridge, fields[p[j]]);
                }

                // After all 6 fields have arrived, approach MUST be active
                check(ScreenModule.isNavActive(), "S21 perm " + i + ": approach active after all 6 fields arrive");
                check(collector.count() == 0, "S21 perm " + i + ": 0 rebinds on initial approach entry");
            }
        }

        // ============================================================
        // Suite 22: Malicious Repeated Stale Old-Generation Packets
        // ============================================================
        {
            BAPBridge bridge = createBridge(null);
            RouteGuidance rg = new RouteGuidance();
            RouteGuidance.State state = (RouteGuidance.State) getField(RouteGuidance.class, rg, "state");
            collector.clear();
            setSimulatedPresentationActive(false);

            feed(rg, parseMethod, state, bridge,
                "source_supports_rg:n:1\n" +
                "route_generation:n:800\n" +
                "route_state:n:1\n" +
                "maneuver_count:n:1\n" +
                "maneuver_list:s:0\n" +
                "m0_type:n:1\n" +
                "dist_maneuver_m:n:200\n");

            check(state.routeGeneration == 800L, "S22: routeGeneration is 800");
            check(state.distManeuverM == 200, "S22: distManeuverM is 200");
            check(ScreenModule.isNavActive(), "S22: approach active at 200m");

            long[] staleGens = new long[]{799L, 799L, 750L, 700L, 100L, 50L, 0L};
            for (int i = 0; i < staleGens.length; i++) {
                // Attempt to inject malicious reroute, invalid maneuver type, and far distance from older generations
                feed(rg, parseMethod, state, bridge,
                    "route_generation:n:" + staleGens[i] + "\n" +
                    "route_state:n:5\n" +
                    "maneuver_count:n:0\n" +
                    "m0_type:n:99\n" +
                    "dist_maneuver_m:n:5000\n");

                check(state.routeGeneration == 800L, "S22: stale gen " + staleGens[i] + " did not regress routeGeneration");
                check(state.routeState == 1, "S22: stale gen did not poison routeState");
                check(state.maneuverCount == 1, "S22: stale gen did not overwrite maneuverCount");
                check(state.mType[0] == 1, "S22: stale gen did not poison mType[0]");
                check(state.distManeuverM == 200, "S22: stale gen did not alter distManeuverM");
                check(ScreenModule.isNavActive(), "S22: approach remained active despite stale flood");
            }

            // Valid gen 800 distance update
            feed(rg, parseMethod, state, bridge,
                "route_generation:n:800\n" +
                "dist_maneuver_m:n:180\n");
            check(state.distManeuverM == 180, "S22: valid gen 800 update applied distManeuverM=180");
            check(ScreenModule.isNavActive(), "S22: approach active at 180m");

            // Another stale packet from 799
            feed(rg, parseMethod, state, bridge,
                "route_generation:n:799\n" +
                "dist_maneuver_m:n:3000\n");
            check(state.distManeuverM == 180, "S22: stale packet 799 dropped, distance kept at 180");
        }

        // ============================================================
        // Suite 23: Large Generation Jumps & Arithmetic Overflow Safety
        // ============================================================
        {
            BAPBridge bridge = createBridge(null);
            RouteGuidance rg = new RouteGuidance();
            RouteGuidance.State state = (RouteGuidance.State) getField(RouteGuidance.class, rg, "state");
            collector.clear();
            setSimulatedPresentationActive(false);

            long[] jumpGens = new long[]{
                100L,
                101L,
                1000000L,
                1000001L,
                Long.MAX_VALUE - 1L,
                Long.MAX_VALUE
            };

            for (int i = 0; i < jumpGens.length; i++) {
                feed(rg, parseMethod, state, bridge,
                    "source_supports_rg:n:1\n" +
                    "route_generation:n:" + jumpGens[i] + "\n" +
                    "route_state:n:1\n" +
                    "maneuver_count:n:1\n" +
                    "maneuver_list:s:0\n" +
                    "m0_type:n:1\n" +
                    "dist_maneuver_m:n:200\n");
                check(state.routeGeneration == jumpGens[i],
                    "S23 jump " + i + ": routeGeneration updated to " + jumpGens[i]);
                check(ScreenModule.isNavActive(),
                    "S23 jump " + i + ": approach active");
            }

            // While at Long.MAX_VALUE, feed Long.MAX_VALUE - 1
            feed(rg, parseMethod, state, bridge,
                "route_generation:n:" + (Long.MAX_VALUE - 1L) + "\n" +
                "dist_maneuver_m:n:5000\n");
            check(state.routeGeneration == Long.MAX_VALUE,
                "S23: Long.MAX_VALUE - 1 dropped as stale against Long.MAX_VALUE");
            check(state.distManeuverM == 200,
                "S23: distManeuverM not corrupted by stale Long.MAX_VALUE - 1");
            check(ScreenModule.isNavActive(),
                "S23: approach remains active at Long.MAX_VALUE");
        }

        // ============================================================
        // Suite 24: Multi-Cycle Approach Sequence & DSI/DisplayManager Verification
        // Sequence: START -> 200m -> 660m -> 200m -> 660m -> 200m
        // ============================================================
        {
            BapProxyHandler bapHandler = new BapProxyHandler();
            TestClusterService cs = createClusterService();
            MockRenderer renderer = new MockRenderer();
            BAPBridge bridge = createBridge(renderer, bapHandler, cs);

            // Wire up a test DisplayManager Kombi control to record physical switchContext calls
            final List dmSwitches = new ArrayList();
            IDisplayManagerKombiControl testDm = (IDisplayManagerKombiControl) Proxy.newProxyInstance(
                RouteContextDeltaIntegrationTest.class.getClassLoader(),
                new Class[]{IDisplayManagerKombiControl.class},
                new InvocationHandler() {
                    public Object invoke(Object proxy, Method method, Object[] args) {
                        String name = method.getName();
                        if ("switchContext".equals(name) && args != null && args.length > 0) {
                            dmSwitches.add(args[0]);
                        }
                        Class type = method.getReturnType();
                        if (type == Boolean.TYPE) return Boolean.FALSE;
                        if (type == Integer.TYPE) return Integer.valueOf(0);
                        return null;
                    }
                });

            Field dmf = ScreenModule.class.getDeclaredField("dm");
            dmf.setAccessible(true);
            Field desCf = ScreenModule.class.getDeclaredField("desiredCtx");
            desCf.setAccessible(true);
            Field curCf = ScreenModule.class.getDeclaredField("currentCtx");
            curCf.setAccessible(true);
            Field lockF = ScreenModule.class.getDeclaredField("LOCK");
            lockF.setAccessible(true);
            Object lockObj = lockF.get(null);

            final ScreenModule sm = new ScreenModule();
            synchronized (lockObj) {
                dmf.set(sm, testDm);
                desCf.setInt(null, 74);
                curCf.setInt(null, 74);
            }

            Field wf = ScreenModule.class.getDeclaredField("worker");
            wf.setAccessible(true);
            final Method swLoop = ScreenModule.class.getDeclaredMethod("switchLoop", new Class[0]);
            swLoop.setAccessible(true);
            Thread worker = new Thread(new Runnable() {
                public void run() {
                    try { swLoop.invoke(sm, new Object[0]); }
                    catch (Exception ignored) {}
                }
            }, "suite24-cluster-switch");
            worker.setDaemon(true);
            worker.start();
            wf.set(sm, worker);

            // 1. Initial route START
            boolean started = bridge.onStart();
            check(started, "S24: onStart succeeded");
            check(bridge.isBapPresentationActive(), "S24 START: bapPresentationActive true");
            check(cs.rgiValid, "S24 START: rgiValid true");
            check(ScreenModule.isRouteActive(), "S24 START: routeActive true");
            check(!ScreenModule.isPresentationActive(), "S24 START: presentationActive false");
            check(ScreenModule.getDesiredCtx() == 74, "S24 START: desiredCtx 74");

            RouteGuidance rg = new RouteGuidance();
            RouteGuidance.State state = (RouteGuidance.State) getField(RouteGuidance.class, rg, "state");

            // 2. Approach Entry 1: 200m (<= 457m)
            feed(rg, parseMethod, state, bridge,
                "source_supports_rg:n:1\n" +
                "route_generation:n:900\n" +
                "route_state:n:1\n" +
                "maneuver_count:n:1\n" +
                "maneuver_list:s:0\n" +
                "m0_type:n:1\n" +
                "dist_maneuver_m:n:200\n");
            check(ScreenModule.isRouteActive(), "S24 Cycle1 200m: routeActive true");
            check(ScreenModule.isPresentationActive(), "S24 Cycle1 200m: presentationActive true");
            check(bridge.isBapPresentationActive(), "S24 Cycle1 200m: bapPresentationActive true");
            check(cs.rgiValid, "S24 Cycle1 200m: rgiValid true");
            check(ScreenModule.getDesiredCtx() == 80, "S24 Cycle1 200m: desiredCtx 80");

            long deadline = System.currentTimeMillis() + 1000;
            while (ScreenModule.getCurrentCtx() != 80 && System.currentTimeMillis() < deadline) {
                Thread.sleep(20);
            }
            check(ScreenModule.getCurrentCtx() == 80, "S24 Cycle1 200m: currentCtx reached 80");

            // 3. Approach Exit 1: 660m (> 457m)
            feed(rg, parseMethod, state, bridge, "dist_maneuver_m:n:660\n");
            check(ScreenModule.isRouteActive(), "S24 Cycle1 660m: routeActive STILL true");
            check(!ScreenModule.isPresentationActive(), "S24 Cycle1 660m: presentationActive false");
            check(!bridge.isBapPresentationActive(), "S24 Cycle1 660m: bapPresentationActive false");
            check(!cs.rgiValid, "S24 Cycle1 660m: rgiValid false");
            check(ScreenModule.getDesiredCtx() == 74, "S24 Cycle1 660m: desiredCtx 74");

            deadline = System.currentTimeMillis() + 1000;
            while (ScreenModule.getCurrentCtx() != 74 && System.currentTimeMillis() < deadline) {
                Thread.sleep(20);
            }
            check(ScreenModule.getCurrentCtx() == 74, "S24 Cycle1 660m: currentCtx reached 74");

            // 4. Approach Re-Entry 1: 200m (<= 457m)
            feed(rg, parseMethod, state, bridge, "dist_maneuver_m:n:200\n");
            check(ScreenModule.isRouteActive(), "S24 Cycle2 200m: routeActive true");
            check(ScreenModule.isPresentationActive(), "S24 Cycle2 200m: presentationActive true");
            check(bridge.isBapPresentationActive(), "S24 Cycle2 200m: bapPresentationActive true");
            check(cs.rgiValid, "S24 Cycle2 200m: rgiValid true");
            check(ScreenModule.getDesiredCtx() == 80, "S24 Cycle2 200m: desiredCtx 80");

            deadline = System.currentTimeMillis() + 1000;
            while (ScreenModule.getCurrentCtx() != 80 && System.currentTimeMillis() < deadline) {
                Thread.sleep(20);
            }
            check(ScreenModule.getCurrentCtx() == 80, "S24 Cycle2 200m: currentCtx reached 80");

            // 5. Approach Exit 2: 660m (> 457m)
            feed(rg, parseMethod, state, bridge, "dist_maneuver_m:n:660\n");
            check(ScreenModule.isRouteActive(), "S24 Cycle2 660m: routeActive STILL true");
            check(!ScreenModule.isPresentationActive(), "S24 Cycle2 660m: presentationActive false");
            check(!bridge.isBapPresentationActive(), "S24 Cycle2 660m: bapPresentationActive false");
            check(!cs.rgiValid, "S24 Cycle2 660m: rgiValid false");
            check(ScreenModule.getDesiredCtx() == 74, "S24 Cycle2 660m: desiredCtx 74");

            deadline = System.currentTimeMillis() + 1000;
            while (ScreenModule.getCurrentCtx() != 74 && System.currentTimeMillis() < deadline) {
                Thread.sleep(20);
            }
            check(ScreenModule.getCurrentCtx() == 74, "S24 Cycle2 660m: currentCtx reached 74");

            // 6. Approach Re-Entry 2: 200m (<= 457m)
            feed(rg, parseMethod, state, bridge, "dist_maneuver_m:n:200\n");
            check(ScreenModule.isRouteActive(), "S24 Cycle3 200m: routeActive true");
            check(ScreenModule.isPresentationActive(), "S24 Cycle3 200m: presentationActive true");
            check(bridge.isBapPresentationActive(), "S24 Cycle3 200m: bapPresentationActive true");
            check(cs.rgiValid, "S24 Cycle3 200m: rgiValid true");
            check(ScreenModule.getDesiredCtx() == 80, "S24 Cycle3 200m: desiredCtx 80");

            deadline = System.currentTimeMillis() + 1000;
            while (ScreenModule.getCurrentCtx() != 80 && System.currentTimeMillis() < deadline) {
                Thread.sleep(20);
            }
            check(ScreenModule.getCurrentCtx() == 80, "S24 Cycle3 200m: currentCtx reached 80");

            // Verify explicit histories across START -> 200m -> 660m -> 200m -> 660m -> 200m:
            check(bapHandler.rgStatusHistory.size() == 5, "S24: exactly 5 RGStatus transactions");
            check(((Integer) bapHandler.rgStatusHistory.get(0)).intValue() == 1, "S24 RGStatus[0] == 1");
            check(((Integer) bapHandler.rgStatusHistory.get(1)).intValue() == 0, "S24 RGStatus[1] == 0");
            check(((Integer) bapHandler.rgStatusHistory.get(2)).intValue() == 1, "S24 RGStatus[2] == 1");
            check(((Integer) bapHandler.rgStatusHistory.get(3)).intValue() == 0, "S24 RGStatus[3] == 0");
            check(((Integer) bapHandler.rgStatusHistory.get(4)).intValue() == 1, "S24 RGStatus[4] == 1");

            Field argf = BAPBridge.class.getDeclaredField("ACTIVE_RGTYPE");
            argf.setAccessible(true);
            int activeRgType = argf.getInt(null);

            check(bapHandler.activeRgTypeHistory.size() == 5, "S24: exactly 5 ActiveRGType transactions");
            check(((Integer) bapHandler.activeRgTypeHistory.get(0)).intValue() == activeRgType, "S24 ActiveRGType[0] == activeRgType");
            check(((Integer) bapHandler.activeRgTypeHistory.get(1)).intValue() == 0, "S24 ActiveRGType[1] == 0");
            check(((Integer) bapHandler.activeRgTypeHistory.get(2)).intValue() == activeRgType, "S24 ActiveRGType[2] == activeRgType");
            check(((Integer) bapHandler.activeRgTypeHistory.get(3)).intValue() == 0, "S24 ActiveRGType[3] == 0");
            check(((Integer) bapHandler.activeRgTypeHistory.get(4)).intValue() == activeRgType, "S24 ActiveRGType[4] == activeRgType");

            // Verify physical DisplayManager switches occurred
            check(dmSwitches.contains(Integer.valueOf(80)), "S24: DisplayManager executed switchContext(80)");
            check(dmSwitches.contains(Integer.valueOf(74)), "S24: DisplayManager executed switchContext(74)");

            // Teardown DisplayManager mock & worker
            synchronized (lockObj) {
                dmf.set(sm, null);
                desCf.setInt(null, 74);
                curCf.setInt(null, 74);
            }
        }

        // ============================================================
        // Suite 25: Failed openBapPresentation() Compensating Rollback
        // ============================================================
        {
            BapProxyHandler bapHandler = new BapProxyHandler();
            TestClusterService cs = createClusterService();
            MockRenderer renderer = new MockRenderer();
            BAPBridge bridge = createBridge(renderer, bapHandler, cs);

            // Inject failure when updateActiveRGType is called during open
            bapHandler.throwOnActiveRGType = true;
            bapHandler.throwOnceActiveRGType = true;

            Method openMethod = BAPBridge.class.getDeclaredMethod("openBapPresentation", new Class[0]);
            openMethod.setAccessible(true);

            boolean openResult = ((Boolean) openMethod.invoke(bridge, new Object[0])).booleanValue();
            check(!openResult, "S25: openBapPresentation returns false on partial failure");
            check(!bridge.isBapPresentationActive(), "S25: bapPresentationActive is false after failed open");
            check(!cs.rgiValid, "S25: rgiValid rolled back to false");

            // Compensating rollback must have sent RGStatus(0) and ActiveRGType(0)
            check(bapHandler.rgStatusHistory.size() >= 2, "S25: RGStatus sent 1 then compensating 0");
            int lastRgStatus = ((Integer) bapHandler.rgStatusHistory.get(bapHandler.rgStatusHistory.size() - 1)).intValue();
            check(lastRgStatus == 0, "S25: final RGStatus is 0 (compensating close)");
        }

        // ============================================================
        // Suite 26: Failed closeBapPresentation() Context Retention & Retry
        // ============================================================
        {
            BapProxyHandler bapHandler = new BapProxyHandler();
            TestClusterService cs = createClusterService();
            MockRenderer renderer = new MockRenderer();
            BAPBridge bridge = createBridge(renderer, bapHandler, cs);

            RouteGuidance rg = new RouteGuidance();
            RouteGuidance.State state = (RouteGuidance.State) getField(RouteGuidance.class, rg, "state");

            // Establish approach (200m)
            feed(rg, parseMethod, state, bridge,
                "source_supports_rg:n:1\n" +
                "route_generation:n:950\n" +
                "route_state:n:1\n" +
                "maneuver_count:n:1\n" +
                "maneuver_list:s:0\n" +
                "m0_type:n:1\n" +
                "dist_maneuver_m:n:200\n");
            check(bridge.isBapPresentationActive(), "S26: bridge BAP presentation active");
            check(ScreenModule.isPresentationActive(), "S26: ScreenModule presentation active");
            check(ScreenModule.getDesiredCtx() == 80, "S26: desiredCtx is 80");

            // Inject failure during close on updateRGStatus(0)
            bapHandler.throwOnRGStatus = true;
            bapHandler.throwOnRGStatusTarget = 0;

            // Trigger approach exit (660m)
            feed(rg, parseMethod, state, bridge, "dist_maneuver_m:n:660\n");

            // Close failed, so Java MUST retain presentationActive=true and desiredCtx=80 to prevent split-brain!
            check(bridge.isBapPresentationActive(), "S26 failed close: bapPresentationActive retained true");
            check(ScreenModule.isPresentationActive(), "S26 failed close: presentationActive retained true");
            check(ScreenModule.getDesiredCtx() == 80, "S26 failed close: desiredCtx 80 retained for retry");

            // Remove failure injection and retry close on next update
            bapHandler.throwOnRGStatus = false;
            bapHandler.throwOnRGStatusTarget = -1;

            feed(rg, parseMethod, state, bridge, "dist_maneuver_m:n:660\n");

            // Retry succeeded: presentation closed cleanly, drops to 74
            check(!bridge.isBapPresentationActive(), "S26 retry: bapPresentationActive is false");
            check(!ScreenModule.isPresentationActive(), "S26 retry: presentationActive is false");
            check(ScreenModule.getDesiredCtx() == 74, "S26 retry: desiredCtx dropped to 74");
            check(!cs.rgiValid, "S26 retry: rgiValid is false");
        }

        System.out.println("RouteContextDeltaIntegrationTest: ALL 26 END-TO-END SUITES PASS (" + checks + " checks)");
    }
}