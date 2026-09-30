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
import java.util.Random;

/**
 * Reproduction attempt for the field bug: "rerouting on very SHORT turns leaves the
 * maneuver pill (ctx 80, maneuver_render) open and completely black until a full
 * viewport close/open". Distances under ~91 m (300 ft), especially in the BLINK
 * zone (BAPBridge.isInBlinkZone: 1..76 m, i.e. below 250 ft).
 *
 * The earlier RerouteRendererRecoveryTest only exercised the NON-blink path.
 * This test drives the real BAPBridge with the same fixtures (fake renderer, BAP
 * proxy) across the whole short-distance range, with reroute pulses (route_state
 * 5 then 2), identical or NEW returning data, the blink worker's ticks
 * (sendActionBlinkTick, driven deterministically by reflection with the current
 * generation, plus one scenario with the real 600 ms worker), and a seeded
 * random sweep.
 *
 * INVARIANT (checked after every step and again after blink ticks): if the route
 * is active, not rerouting, has a valid maneuver, and ScreenModule presentation
 * is active, the renderer's last non-progress command MUST be a MANEUVER that
 * matches the current maneuver's icon/exit angle -- never a CLEAR and never
 * "nothing since the clear".
 *
 * Expected result today: the test is meant to FAIL (exit 1, "REPRODUCED ...")
 * if the bug is real, printing the last ~20 events of the failing sequence.
 *
 * Run only this test after building (see scripts/test_route_info.sh) with the
 * same TEST_DIR/CLASSPATH.
 */
public final class ShortTurnRerouteBlinkTest {

    private static final int STEP_M = 120;              // short city step (rawStepM)
    private static final int[] DIST_SET = {250, 150, 90, 60, 40, 25, 15, 8};
    private static final int[] TYPES = {1, 2};          // left / right turn
    private static final int[] ANGLES = {-90, 90};
    private static final int HISTORY_DUMP = 20;
    private static final long SEED = 20260928L;
    private static final int RANDOM_SEQUENCES = 500;

    /* ------------------------------------------------------------------ */
    /* Fixtures (same style as RerouteRendererRecoveryTest)                */
    /* ------------------------------------------------------------------ */

    private static final class MockRenderer extends RendererServer {
        int clears = 0;
        int maneuvers = 0;
        int lastIcon = -1;
        int lastExitAngle = 0;
        /** "CLEAR", "MANEUVER" or null (nothing yet). PROGRESS never changes it. */
        String lastPaint = null;
        boolean mockFrameReady = true;
        boolean mockReady = true;
        final List trace = new ArrayList();

        public boolean connect() { return true; }
        public boolean isReady() { return mockReady; }
        public boolean isFrameReady() { return mockFrameReady; }

        public boolean sendClear() {
            clears++;
            lastPaint = "CLEAR";
            trace.add("CLEAR");
            return true;
        }

        public boolean sendBapProgressManeuver(int icon, int direction, int exitAngle,
                int drivingSide, int[] junctionAngles, int level, int mode, int perspective,
                boolean refresh, boolean snapToRoad, int progressState) {
            maneuvers++;
            lastIcon = icon;
            lastExitAngle = exitAngle;
            lastPaint = "MANEUVER";
            trace.add("MANEUVER(icon=" + icon + ",exit=" + exitAngle + ",lvl=" + level
                + ",ps=" + progressState + ")");
            return true;
        }

        public boolean sendProgress(int level, int mode, int progressState) {
            trace.add("PROGRESS(lvl=" + level + ",ps=" + progressState + ")");
            return true;
        }

        public boolean sendVisibleArea(int x, int y, int w, int h) { return true; }

        public boolean sendManeuver(int icon, int direction, int exitAngle,
                int drivingSide, int[] junctionAngles, int level, int mode, int perspective) {
            maneuvers++;
            lastIcon = icon;
            lastExitAngle = exitAngle;
            lastPaint = "MANEUVER";
            trace.add("MANEUVER(prime,icon=" + icon + ",exit=" + exitAngle + ")");
            return true;
        }
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

    private static BAPBridge createBridge(MockRenderer renderer) throws Exception {
        BAPBridge bridge = new BAPBridge();
        CombiBAPServiceNavi service = (CombiBAPServiceNavi) Proxy.newProxyInstance(
            ShortTurnRerouteBlinkTest.class.getClassLoader(),
            new Class[]{CombiBAPServiceNavi.class},
            new BapProxyHandler());
        setField(BAPBridge.class, bridge, "appConnectorNavi", service);
        setField(BAPBridge.class, bridge, "initialized", Boolean.TRUE);
        setField(BAPBridge.class, bridge, "bapSessionStarted", Boolean.TRUE);
        setField(BAPBridge.class, bridge, "nativeStopAttempted", Boolean.TRUE);
        setField(BAPBridge.class, bridge, "csRef", createClusterService());
        setField(BAPBridge.class, bridge, "rendererClient", renderer);
        setField(BAPBridge.class, bridge, "customRendererStarted", Boolean.TRUE);
        ScreenModule.setRouteActive(true);
        ScreenModule.setPresentationActive(false);
        return bridge;
    }

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
        s.mJunctionType[0] = 0;
        s.mDistance[0] = STEP_M;        // short step (city), distinct from distManeuverM
        s.mVer[0] = ver;
        s.distManeuverM = distM;
        return s;
    }

    /* ------------------------------------------------------------------ */
    /* Model of what iOS/RouteGuidance sends                               */
    /* ------------------------------------------------------------------ */

    private static final class Model {
        long gen = 1000L;
        int routeState = 1;      // 1 = active (test fixtures use 1 as "active")
        int dist = 100;
        int ver = 1;
        int variant = 0;
        // last snapshot delivered, for parser-style field diffing
        int prevRs = -1, prevDist = -1, prevVer = -1, prevVariant = -1;
        boolean first = true;
        int dirtyExtra = 0;
        boolean populated = true;
    }

    private static boolean rerouting(Model m) {
        return m.routeState == 5;
    }

    private static RouteGuidance.State build(Model m) {
        RouteGuidance.State s;
        if (!m.populated) {
            s = new RouteGuidance.State();
            s.routeGeneration = m.gen;
            s.routeState = m.routeState;
            s.routeStateGeneration = -1L;
            s.maneuverCount = 0;
            s.maneuverOrder = new int[0];
            s.distManeuverM = -1;
            s.dirtyMask = RouteGuidance.State.DIRTY_MANEUVER_COUNT | RouteGuidance.State.DIRTY_MANEUVER_LIST
                | RouteGuidance.State.DIRTY_MANEUVER_ICON | RouteGuidance.State.DIRTY_ROUTE_STATE;
            return s;
        }
        s = maneuverState(m.gen, m.routeState, m.gen, TYPES[m.variant], ANGLES[m.variant], m.dist, m.ver);
        if (m.first) {
            s.markAllDirtyForReplay();
        } else {
            int d = m.dirtyExtra;
            if (m.routeState != m.prevRs) d |= RouteGuidance.State.DIRTY_ROUTE_STATE;
            if (m.dist != m.prevDist) d |= RouteGuidance.State.DIRTY_DIST_MAN;
            if (m.ver != m.prevVer || m.variant != m.prevVariant) {
                d |= RouteGuidance.State.DIRTY_MANEUVER_ICON | RouteGuidance.State.DIRTY_MANEUVER_LIST
                    | RouteGuidance.State.DIRTY_DIST_MAN;
            }
            s.dirtyMask = d;
        }
        m.first = false;
        m.dirtyExtra = 0;
        m.prevRs = m.routeState;
        m.prevDist = m.dist;
        m.prevVer = m.ver;
        m.prevVariant = m.variant;
        return s;
    }

    /* ------------------------------------------------------------------ */
    /* Scenario harness                                                     */
    /* ------------------------------------------------------------------ */

    private static final class Violation extends RuntimeException {
        Violation(String msg) { super(msg); }
    }

    private static final int[] EXPECT_ICON = new int[TYPES.length];
    private static final int[] EXPECT_EXIT = new int[TYPES.length];

    private static final class Run {
        final String name;
        final MockRenderer renderer = new MockRenderer();
        final BAPBridge bridge;
        final Model m = new Model();
        final List history = new ArrayList();
        int traceMark = 0;
        int step = 0;

        Run(String name) throws Exception {
            this.name = name;
            this.bridge = createBridge(renderer);
        }

        String blinkPhase() {
            try {
                return "blinkArmed=" + getField(BAPBridge.class, bridge, "blinkArmed")
                    + ",full=" + getField(BAPBridge.class, bridge, "actionBlinkFull")
                    + ",gen=" + getField(BAPBridge.class, bridge, "actionBlinkGeneration")
                    + ",pending=" + getField(BAPBridge.class, bridge, "rendererManeuverPending");
            } catch (Throwable t) {
                return "blink?" + t;
            }
        }

        void log(String label) {
            StringBuffer cmds = new StringBuffer();
            for (int i = traceMark; i < renderer.trace.size(); i++) {
                if (cmds.length() > 0) cmds.append(' ');
                cmds.append(renderer.trace.get(i));
            }
            traceMark = renderer.trace.size();
            history.add("#" + (step++) + " " + label + " distM=" + m.dist + " routeState=" + m.routeState
                + " ver=" + m.ver + " variant=" + m.variant + " presentation="
                + ScreenModule.isPresentationActive() + " lastPaint=" + renderer.lastPaint
                + " " + blinkPhase() + " cmds=[" + cmds + "]");
        }

        void send(String label) throws Exception {
            RouteGuidance.State s = build(m);
            if (s.dirtyMask == 0) {
                log(label + " (no dirty bits; bridge not called)");
                return;
            }
            bridge.update(s);
            log(label + " dirty=0x" + Integer.toHexString(s.dirtyMask));
        }

        void tick() throws Exception {
            int gen = ((Integer) getField(BAPBridge.class, bridge, "actionBlinkGeneration")).intValue();
            Method t = BAPBridge.class.getDeclaredMethod("sendActionBlinkTick", new Class[]{Integer.TYPE});
            t.setAccessible(true);
            t.invoke(bridge, new Object[]{new Integer(gen)});
            log("blink tick");
        }

        void stopWorker() {
            try {
                Method st = BAPBridge.class.getDeclaredMethod("stopActionBlinkThread", new Class[0]);
                st.setAccessible(true);
                st.invoke(bridge, new Object[0]);
            } catch (Throwable ignored) { }
        }

        /** The invariant. Throws Violation carrying the failing step's context. */
        void check(String where) {
            if (!m.populated || rerouting(m)) return;
            if (!ScreenModule.isPresentationActive()) return;
            String why = null;
            if (renderer.lastPaint == null) {
                why = "no renderer paint command at all";
            } else if (!"MANEUVER".equals(renderer.lastPaint)) {
                why = "last non-progress renderer command is " + renderer.lastPaint + " (pill open but BLACK)";
            } else if (renderer.lastIcon != EXPECT_ICON[m.variant]
                    || renderer.lastExitAngle != EXPECT_EXIT[m.variant]) {
                why = "renderer shows icon=" + renderer.lastIcon + "/exit=" + renderer.lastExitAngle
                    + " but current maneuver is icon=" + EXPECT_ICON[m.variant]
                    + "/exit=" + EXPECT_EXIT[m.variant];
            }
            if (why != null) {
                log("VIOLATION at " + where);
                throw new Violation(why);
            }
        }

        void checkSettled(String label, int ticks) throws Exception {
            check(label);
            for (int i = 0; i < ticks; i++) {
                tick();
                check(label + " +tick" + (i + 1));
            }
        }

        void dump() {
            System.out.println("---- scenario: " + name + " : last " + HISTORY_DUMP + " events ----");
            int from = Math.max(0, history.size() - HISTORY_DUMP);
            for (int i = from; i < history.size(); i++) System.out.println("  " + history.get(i));
        }

        void finish() {
            stopWorker();
        }
    }

    /** Precompute the expected icon/exit for each maneuver variant using a scratch bridge. */
    private static void precomputeExpectations() throws Exception {
        for (int v = 0; v < TYPES.length; v++) {
            Run r = new Run("expect-" + v);
            r.m.variant = v;
            r.m.dist = 200;
            RouteGuidance.State s = build(r.m);
            r.bridge.update(s);
            if (r.renderer.lastPaint == null || !"MANEUVER".equals(r.renderer.lastPaint)) {
                throw new IllegalStateException("fixture: could not derive expected icon for variant " + v);
            }
            EXPECT_ICON[v] = r.renderer.lastIcon;
            EXPECT_EXIT[v] = r.renderer.lastExitAngle;
            r.finish();
        }
        if (EXPECT_EXIT[0] >= 0 || EXPECT_EXIT[1] <= 0) {
            throw new IllegalStateException("fixture: left/right variants must differ in exit angle sign");
        }
    }

    /* ------------------------------------------------------------------ */
    /* Deterministic scenarios                                             */
    /* ------------------------------------------------------------------ */

    private static int detCount = 0;
    private static final List failures = new ArrayList();
    private static Run firstFailure = null;
    private static String firstFailureReason = null;

    private static void noteFailure(Run r, String label, Violation v) {
        failures.add(label + " -> " + v.getMessage());
        if (firstFailure == null) {
            firstFailure = r;
            firstFailureReason = label + " -> " + v.getMessage();
        }
    }

    /**
     * @param retNew   false: reroute returns identical data; true: NEW maneuver (ver+1, other turn)
     * @param retDist  distance on return (for identical data: same as start)
     * @param shift    true: the distance already moved to retDist while rerouting
     * @param ticks    blink ticks run after every step
     */
    private static void runDeterministic(int startDist, int reroutes, boolean retNew, int retDist,
                                         boolean shift, int ticks) throws Exception {
        String label = "start=" + startDist + "m reroutes=" + reroutes
            + " return=" + (retNew ? "NEW@" + retDist + "m" : "identical")
            + (shift ? " distShiftDuringReroute" : "") + " ticks=" + ticks
            + (isBlink(retNew ? retDist : startDist) ? " [return in blink zone]" : "");
        Run r = new Run(label);
        detCount++;
        try {
            r.m.dist = startDist;
            r.send("establish");
            r.checkSettled("establish", ticks);
            for (int i = 0; i < reroutes; i++) {
                r.m.routeState = 5;
                if (shift) r.m.dist = retNew ? retDist : startDist;
                r.send("reroute-begin#" + (i + 1));
                r.checkSettled("reroute-begin#" + (i + 1), ticks);
                r.m.routeState = 1;
                if (retNew) {
                    r.m.ver++;
                    r.m.variant = 1 - r.m.variant;
                    r.m.dist = retDist;
                }
                r.send("reroute-return#" + (i + 1));
                r.checkSettled("reroute-return#" + (i + 1), ticks);
            }
            // a couple of follow-up distance ticks and blink ticks must not heal-or-break
            r.m.dist = Math.max(1, r.m.dist - 3);
            r.send("dist-tick");
            r.checkSettled("dist-tick", 2);
        } catch (Violation v) {
            noteFailure(r, label, v);
        } finally {
            r.finish();
        }
    }

    private static boolean isBlink(int distM) {
        return distM > 0 && distM <= 76;  // 0 < distM*100 < 7620 (250 ft)
    }

    /** Real 600 ms BAPActionBlink worker runs between reroute steps (bounded waits). */
    private static void runRealWorkerScenario() throws Exception {
        String label = "real BAPActionBlink worker, 25m identical reroute x2";
        Run r = new Run(label);
        detCount++;
        try {
            r.m.dist = 25;
            r.send("establish");
            r.check("establish");
            Thread.sleep(700);
            r.log("after real worker wait");
            r.check("real wait #1");
            for (int i = 0; i < 2; i++) {
                r.m.routeState = 5;
                r.send("reroute-begin#" + (i + 1));
                r.m.routeState = 1;
                r.send("reroute-return#" + (i + 1));
                r.check("return#" + (i + 1));
                Thread.sleep(700);
                r.log("after real worker wait");
                r.check("real wait after return#" + (i + 1));
            }
        } catch (Violation v) {
            noteFailure(r, label, v);
        } finally {
            r.finish();
        }
    }

    /* ------------------------------------------------------------------ */
    /* Randomized sweep                                                     */
    /* ------------------------------------------------------------------ */

    private static void runRandom(int index, Random rnd, long seedForSeq) throws Exception {
        String label = "random#" + index + " (seed=" + SEED + ", seq-seed=" + seedForSeq + ")";
        Run r = new Run(label);
        try {
            r.m.dist = rnd.nextInt(121);
            r.m.variant = rnd.nextInt(2);
            r.send("establish");
            r.checkSettled("establish", rnd.nextInt(3));
            int steps = 6 + rnd.nextInt(20);
            for (int i = 0; i < steps; i++) {
                int op = rnd.nextInt(10);
                String what;
                switch (op) {
                    case 0: case 1:   // distance-only tick
                        r.m.dist = rnd.nextInt(121);
                        what = "dist-only";
                        break;
                    case 2: case 3:   // reroute begins (data may move)
                        r.m.routeState = 5;
                        if (rnd.nextBoolean()) r.m.dist = rnd.nextInt(121);
                        what = "reroute-begin";
                        break;
                    case 4: case 5:   // reroute returns, identical or new
                        r.m.routeState = 1;
                        if (rnd.nextBoolean()) {
                            r.m.ver++;
                            r.m.variant = rnd.nextInt(2);
                            r.m.dist = rnd.nextInt(121);
                            what = "return-NEW";
                        } else {
                            if (rnd.nextInt(3) == 0) r.m.dist = rnd.nextInt(121);
                            what = "return-identical";
                        }
                        break;
                    case 6:           // new maneuver while active
                        r.m.ver++;
                        r.m.variant = rnd.nextInt(2);
                        r.m.dist = rnd.nextInt(121);
                        what = "new-maneuver";
                        break;
                    case 7:           // same geometry, new version (consecutive same-direction turn)
                        r.m.ver++;
                        r.m.dist = rnd.nextInt(121);
                        what = "new-ver-same-turn";
                        break;
                    case 8:           // route generation bump: slots empty, then repopulated
                        r.m.gen++;
                        r.m.populated = false;
                        r.send("gen-bump-empty");
                        r.check("gen-bump-empty");
                        r.m.populated = true;
                        r.m.first = false;
                        r.m.prevRs = -1;
                        r.m.dirtyExtra = RouteGuidance.State.DIRTY_MANEUVER_COUNT
                            | RouteGuidance.State.DIRTY_MANEUVER_LIST
                            | RouteGuidance.State.DIRTY_MANEUVER_ICON
                            | RouteGuidance.State.DIRTY_DIST_MAN;
                        r.m.routeState = 1;
                        r.m.ver = 1;
                        r.m.dist = rnd.nextInt(121);
                        what = "gen-bump-repopulate";
                        break;
                    default:          // blink ticks only
                        r.tick();
                        r.check("blink tick only");
                        continue;
                }
                r.send(what);
                r.checkSettled(what, rnd.nextInt(3));
            }
            // finish in a settled active state
            r.m.routeState = 1;
            r.send("final-active");
            r.checkSettled("final-active", 2);
        } catch (Violation v) {
            noteFailure(r, label, v);
        } finally {
            r.finish();
        }
    }

    public static void main(String[] args) throws Exception {
        Log.setLevel(-1);
        Field cnf = ScreenModule.class.getDeclaredField("connected");
        cnf.setAccessible(true);
        cnf.setBoolean(null, true);

        precomputeExpectations();

        // ---- 1. deterministic ----
        for (int a = 0; a < DIST_SET.length; a++) {
            for (int k = 1; k <= 3; k++) {
                for (int ticks = 0; ticks <= 2; ticks += 2) {
                    runDeterministic(DIST_SET[a], k, false, DIST_SET[a], false, ticks);
                    for (int b = 0; b < DIST_SET.length; b++) {
                        runDeterministic(DIST_SET[a], k, true, DIST_SET[b], false, ticks);
                        runDeterministic(DIST_SET[a], k, true, DIST_SET[b], true, ticks);
                    }
                }
            }
        }
        runRealWorkerScenario();
        int detFailures = failures.size();

        // ---- 2. seeded random sweep ----
        System.out.println("ShortTurnRerouteBlinkTest: random seed=" + SEED);
        Random master = new Random(SEED);
        for (int i = 0; i < RANDOM_SEQUENCES; i++) {
            long seqSeed = master.nextLong();
            runRandom(i, new Random(seqSeed), seqSeed);
        }

        if (!failures.isEmpty()) {
            firstFailure.dump();
            System.out.println("failing deterministic scenarios: " + detFailures
                + " of " + detCount + "; failing total (incl. random): " + failures.size());
            int show = Math.min(15, failures.size());
            for (int i = 0; i < show; i++) System.out.println("  FAIL: " + failures.get(i));
            System.out.println("ShortTurnRerouteBlinkTest: REPRODUCED " + firstFailureReason);
            System.exit(1);
        }
        System.out.println("ShortTurnRerouteBlinkTest: PASS (" + detCount + " deterministic, "
            + RANDOM_SEQUENCES + " random)");
        System.exit(0);
    }
}
