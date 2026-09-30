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
 * VISIBILITY test for the field bug "cluster maneuver pill stays open (ctx 80) but is
 * completely BLACK after CarPlay reroutes on a very short turn".
 *
 * Renderer visibility oracle (verified against maneuver_render/render.c:370-381):
 * the route arrow tint is mix(quiet, ice, filled*path_weight + glow) with quiet = warm white,
 * so a drawn maneuver is VISIBLE in EVERY progress state (OFF, FILL, BLINK_LOW, BLINK_HIGH).
 * OFF/BLINK_LOW merely draw the arrow warm white instead of blue. The renderer is BLANK only
 * when no MANEUVER has been received since the last CLEAR (or since start), because CLEAR
 * forces global alpha 0 and a FOLLOW_STREET stub (main.c clear_maneuver) until the next MANEUVER.
 *
 * This test:
 *   - models that in the mock renderer, decoding the same fields the real RendererServer puts
 *     on the wire (MAN_FLAG_PROGRESS / PROGRESS_FLAG, state byte, mode byte, and main.c's
 *     cr_progress_decode rules) for the failure history only;
 *   - NEVER ticks the blink worker itself: the REAL BAPActionBlink thread (600 ms) runs in
 *     real time; the test only reads its fields for the failure history;
 *   - INVARIANT after every settled step: route active, not rerouting, valid maneuver and
 *     presentation active  =>  the last renderer paint command is a MANEUVER (not a CLEAR, not
 *     nothing) carrying the right icon/exit angle.
 *
 * Scenarios emphasize the sticky-blink mechanism: identical distM repeated in the blink
 * zone (stopped car), reroutes (route_state 5 then 1) at 5..29 m returning identical or new
 * maneuvers, distance omitted/zeroed during or right after a reroute (transientNoDistance ->
 * replayDistanceToManeuver), leaving and re-entering the blink zone with the same distM, and a
 * seeded random sweep (0..40 m) with the real worker running.
 *
 * Exit 1 with "REPRODUCED ..." on violation. Optional arg 0: number of random sequences
 * (default 40). Real time: about 3-5 minutes.
 */
public final class ShortTurnBlinkVisibilityTest {

    private static final int STEP_M = 120;
    private static final int[] TYPES = {1, 2};
    private static final int[] ANGLES = {-90, 90};
    private static final int[] ZONE_DIST = {5, 12, 20, 29};
    private static final long SEED = 20260928L;
    private static final int HISTORY_DUMP = 40;

    private static final int S_OFF = 0, S_FILL = 1, S_LOW = 2, S_HIGH = 3;
    private static final String[] S_NAME = {"OFF", "FILL", "BLINK_LOW", "BLINK_HIGH"};

    private static final long START = System.currentTimeMillis();

    private static long now() { return System.currentTimeMillis() - START; }

    /* ------------------------------------------------------------------ */
    /* Mock renderer with native-visibility model                          */
    /* ------------------------------------------------------------------ */

    private static final class MockRenderer extends RendererServer {
        String lastPaint = null;       // "CLEAR" | "MANEUVER" | null
        int lastIcon = -1;
        int lastExitAngle = 0;
        int state = -1;                // decoded native state; -1 = nothing/cleared
        final List trace = new ArrayList();

        public boolean connect() { return true; }
        public boolean isReady() { return true; }
        public boolean isFrameReady() { return true; }

        /** Same rules as cr_progress_decode in maneuver_render/arrow_progress.h. */
        static int decodeFlagged(int st, int mode) {
            if (st > S_HIGH || st < 0) return S_OFF;
            if ((st == S_OFF && mode != 0) || (st != S_OFF && mode != 1)) return S_OFF;
            return st;
        }

        static int decodeLegacy(int mode) { return mode == 1 ? S_FILL : S_OFF; }

        /** Drawn maneuver == visible in every progress state (warm white when OFF/BLINK_LOW).
         *  Blank only when no MANEUVER arrived since the last CLEAR (or since start). */
        synchronized boolean visibleNow() { return "MANEUVER".equals(lastPaint); }
        synchronized int stateNow() { return state; }
        synchronized String paintNow() { return lastPaint; }
        synchronized int iconNow() { return lastIcon; }
        synchronized int exitNow() { return lastExitAngle; }
        synchronized int traceSize() { return trace.size(); }
        synchronized String traceFrom(int from) {
            StringBuffer b = new StringBuffer();
            for (int i = from; i < trace.size(); i++) {
                if (b.length() > 0) b.append(' ');
                b.append(trace.get(i));
            }
            return b.toString();
        }

        private void add(String s) { trace.add(s + "@" + now()); }

        public synchronized boolean sendClear() {
            lastPaint = "CLEAR";
            state = -1;
            add("CLEAR");
            return true;
        }

        public synchronized boolean sendBapProgressManeuver(int icon, int direction, int exitAngle,
                int drivingSide, int[] junctionAngles, int level, int mode, int perspective,
                boolean refresh, boolean snapToRoad, int progressState) {
            lastIcon = icon;
            lastExitAngle = exitAngle;
            lastPaint = "MANEUVER";
            int dec;
            if (progressState >= 0) dec = decodeFlagged(progressState, mode);
            else if (mode > 0) dec = decodeLegacy(mode);
            else dec = S_OFF;
            state = dec;
            add("MANEUVER(icon=" + icon + ",ps=" + progressState + ",mode=" + mode
                + ",lvl=" + level + " -> " + S_NAME[dec] + ")");
            return true;
        }

        public synchronized boolean sendProgress(int level, int mode, int progressState) {
            int dec = progressState >= 0 ? decodeFlagged(progressState, mode) : decodeLegacy(mode);
            state = dec;
            add("PROGRESS(lvl=" + level + ",ps=" + progressState + ",mode=" + mode
                + " -> " + S_NAME[dec] + ")");
            return true;
        }

        public boolean sendVisibleArea(int x, int y, int w, int h) { return true; }

        public synchronized boolean sendManeuver(int icon, int direction, int exitAngle,
                int drivingSide, int[] junctionAngles, int level, int mode, int perspective) {
            lastIcon = icon;
            lastExitAngle = exitAngle;
            lastPaint = "MANEUVER";
            state = mode > 0 ? decodeLegacy(mode) : S_OFF;
            add("MANEUVER(legacy,icon=" + icon + " -> " + S_NAME[state] + ")");
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
            ShortTurnBlinkVisibilityTest.class.getClassLoader(),
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

    /* ------------------------------------------------------------------ */
    /* CarPlay model (same style as ShortTurnRerouteBlinkTest)             */
    /* ------------------------------------------------------------------ */

    private static final class Model {
        long gen = 1000L;
        int routeState = 1;
        int dist = 100;
        int ver = 1;
        int variant = 0;
        int prevRs = -1, prevDist = -1, prevVer = -1, prevVariant = -1;
        boolean first = true;
        int dirtyExtra = 0;
    }

    private static boolean rerouting(Model m) { return m.routeState == 5; }

    private static RouteGuidance.State build(Model m) {
        RouteGuidance.State s = new RouteGuidance.State();
        s.routeGeneration = m.gen;
        s.routeState = m.routeState;
        s.routeStateGeneration = m.gen;
        s.maneuverCount = 1;
        s.maneuverOrder = new int[]{0};
        s.mType[0] = TYPES[m.variant];
        s.mTurnAngle[0] = ANGLES[m.variant];
        s.mTurnAnglePresent[0] = true;
        s.mJunctionType[0] = 0;
        s.mDistance[0] = STEP_M;
        s.mVer[0] = m.ver;
        s.distManeuverM = m.dist;
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
    /* Harness                                                             */
    /* ------------------------------------------------------------------ */

    private static final class Violation extends RuntimeException {
        Violation(String msg) { super(msg); }
    }

    private static final int[] EXPECT_ICON = new int[TYPES.length];
    private static final int[] EXPECT_EXIT = new int[TYPES.length];
    private static final List failures = new ArrayList();
    private static Run firstFailure = null;
    private static String firstFailureReason = null;
    private static int scenarios = 0;
    private static int workerWarnings = 0;

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

        /** Read-only view of the REAL worker: never ticks, never starts, never stops. */
        String worker() {
            try {
                Thread t = (Thread) getField(BAPBridge.class, bridge, "actionBlinkThread");
                return "worker[thread=" + (t == null ? "null" : (t.isAlive() ? "alive" : "DEAD"))
                    + ",running=" + getField(BAPBridge.class, bridge, "actionBlinkThreadRunning")
                    + ",armed=" + getField(BAPBridge.class, bridge, "blinkArmed")
                    + ",gen=" + getField(BAPBridge.class, bridge, "actionBlinkGeneration")
                    + ",blinkDistM=" + getField(BAPBridge.class, bridge, "blinkDistM")
                    + ",cache(has=" + getField(BAPBridge.class, bridge, "hasLastDistM")
                    + ",d=" + getField(BAPBridge.class, bridge, "lastDistM")
                    + ",bar=" + getField(BAPBridge.class, bridge, "lastBarOn")
                    + ",ps=" + getField(BAPBridge.class, bridge, "lastProgressState") + ")"
                    + ",pending=" + getField(BAPBridge.class, bridge, "rendererManeuverPending") + "]";
            } catch (Throwable t) {
                return "worker?" + t;
            }
        }

        boolean workerAliveAndRunning() {
            try {
                Thread t = (Thread) getField(BAPBridge.class, bridge, "actionBlinkThread");
                return t != null && t.isAlive()
                    && ((Boolean) getField(BAPBridge.class, bridge, "actionBlinkThreadRunning")).booleanValue();
            } catch (Throwable t) {
                return false;
            }
        }

        void log(String label) {
            String cmds = renderer.traceFrom(traceMark);
            traceMark = renderer.traceSize();
            history.add("#" + (step++) + " +" + now() + "ms " + label + " distM=" + m.dist
                + " routeState=" + m.routeState + " ver=" + m.ver + " variant=" + m.variant
                + " presentation=" + ScreenModule.isPresentationActive()
                + " nativeState=" + (renderer.stateNow() < 0 ? "none" : S_NAME[renderer.stateNow()])
                + " " + worker() + " cmds=[" + cmds + "]");
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

        /** Resend the identical snapshot with DIST_MAN dirty (a stopped car). */
        void resendSame(String label) throws Exception {
            m.dirtyExtra |= RouteGuidance.State.DIRTY_DIST_MAN;
            send(label);
        }

        void sleep(int ms) throws Exception {
            if (ms > 0) Thread.sleep(ms);
        }

        static boolean inZone(int d) { return d > 0 && d <= 76; }

        /** The invariant: a MANEUVER has been drawn since the last CLEAR, with the right geometry. */
        void settle(String where) throws Exception {
            if (rerouting(m)) return;
            if (!ScreenModule.isPresentationActive()) {
                log(where + " (presentation inactive; not checked)");
                return;
            }
            String why = null;
            String paint = renderer.paintNow();
            if (!renderer.visibleNow()) {
                why = "renderer blank (no MANEUVER since CLEAR)"
                    + (paint == null ? " [nothing sent at all]" : "")
                    + " distM=" + m.dist + " worker " + (workerAliveAndRunning() ? "alive" : "NOT RUNNING");
            } else if (renderer.iconNow() != EXPECT_ICON[m.variant]
                    || renderer.exitNow() != EXPECT_EXIT[m.variant]) {
                why = "renderer shows icon=" + renderer.iconNow() + "/exit=" + renderer.exitNow()
                    + " but current maneuver is icon=" + EXPECT_ICON[m.variant]
                    + "/exit=" + EXPECT_EXIT[m.variant];
            }
            if (why != null) {
                log("VIOLATION at " + where);
                throw new Violation(where + ": " + why);
            }
            if (inZone(m.dist) && !workerAliveAndRunning()) {
                workerWarnings++;
                log(where + " OK-visible but WARNING: in blink zone and worker NOT running");
            } else {
                log(where + " OK-visible");
            }
        }

        void dump() {
            System.out.println("---- scenario: " + name + " : last " + HISTORY_DUMP + " events ----");
            int from = Math.max(0, history.size() - HISTORY_DUMP);
            for (int i = from; i < history.size(); i++) System.out.println("  " + history.get(i));
        }

        void finish() {
            try {
                Method st = BAPBridge.class.getDeclaredMethod("stopActionBlinkThread", new Class[0]);
                st.setAccessible(true);
                st.invoke(bridge, new Object[0]);
            } catch (Throwable ignored) { }
        }
    }

    private static void noteFailure(Run r, Violation v) {
        failures.add(r.name + " -> " + v.getMessage());
        if (firstFailure == null) {
            firstFailure = r;
            firstFailureReason = r.name + " -> " + v.getMessage();
        }
    }

    private static void precomputeExpectations() throws Exception {
        for (int v = 0; v < TYPES.length; v++) {
            Run r = new Run("expect-" + v);
            r.m.variant = v;
            r.m.dist = 200;
            r.bridge.update(build(r.m));
            if (!"MANEUVER".equals(r.renderer.paintNow())) {
                throw new IllegalStateException("fixture: could not derive expected icon for variant " + v);
            }
            EXPECT_ICON[v] = r.renderer.iconNow();
            EXPECT_EXIT[v] = r.renderer.exitNow();
            r.finish();
        }
        if (EXPECT_EXIT[0] >= 0 || EXPECT_EXIT[1] <= 0) {
            throw new IllegalStateException("fixture: left/right variants must differ in exit angle sign");
        }
    }

    /* ------------------------------------------------------------------ */
    /* Deterministic scenarios (all with the real worker running)          */
    /* ------------------------------------------------------------------ */

    /** A. Stopped car: identical distM repeated inside the blink zone at varied worker phases. */
    private static void stoppedCar(int d) throws Exception {
        Run r = new Run("A stopped car in blink zone distM=" + d + " (identical repeats)");
        scenarios++;
        try {
            r.m.dist = d;
            r.send("establish");
            r.settle("establish");
            int[] gaps = {0, 300, 650, 120, 700, 450, 50, 599, 601};
            for (int i = 0; i < gaps.length; i++) {
                r.sleep(gaps[i]);
                r.resendSame("same-dist repeat gap=" + gaps[i]);
                r.settle("repeat#" + i);
            }
        } catch (Violation v) {
            noteFailure(r, v);
        } finally {
            r.finish();
        }
    }

    /** B. Reroutes inside the blink zone, identical or new maneuver, with worker in varied phases. */
    private static void rerouteInZone(int d, boolean retNew, int retDist, int gapBefore, int gapDuring,
                                      int reroutes) throws Exception {
        Run r = new Run("B reroute x" + reroutes + " at " + d + "m return="
            + (retNew ? "NEW@" + retDist + "m" : "identical") + " gapBefore=" + gapBefore
            + " gapDuring=" + gapDuring);
        scenarios++;
        try {
            r.m.dist = d;
            r.send("establish");
            r.settle("establish");
            for (int i = 0; i < reroutes; i++) {
                r.sleep(gapBefore);
                r.m.routeState = 5;
                r.send("reroute-begin#" + (i + 1));
                r.sleep(gapDuring);
                r.m.routeState = 1;
                if (retNew) {
                    r.m.ver++;
                    r.m.variant = 1 - r.m.variant;
                    r.m.dist = retDist;
                }
                r.send("reroute-return#" + (i + 1));
                r.settle("reroute-return#" + (i + 1));
                r.sleep(350);
                r.resendSame("same-dist after return#" + (i + 1));
                r.settle("after-return-repeat#" + (i + 1));
                r.sleep(650);
                r.resendSame("same-dist later#" + (i + 1));
                r.settle("later#" + (i + 1));
            }
        } catch (Violation v) {
            noteFailure(r, v);
        } finally {
            r.finish();
        }
    }

    /**
     * C. CarPlay omits/zeroes the distance (distM <= 0 transient: transientNoDistance ->
     * replayDistanceToManeuver). mode 0: during reroute AND on the return step; mode 1: only on
     * the return step; mode 2: no reroute, just a transient zero; mode 3: zero during reroute,
     * real distance on return.
     */
    private static void omittedDistance(int d, int omit, int mode) throws Exception {
        Run r = new Run("C omitted distance (" + omit + ") mode=" + mode + " at " + d + "m");
        scenarios++;
        try {
            r.m.dist = d;
            r.send("establish");
            r.settle("establish");
            r.sleep(mode == 2 ? 350 : 0);
            if (mode == 2) {
                r.m.dist = omit;
                r.send("transient no-distance");
                r.settle("transient no-distance");
            } else {
                r.m.routeState = 5;
                if (mode == 0 || mode == 3) r.m.dist = omit;
                r.send("reroute-begin");
                r.sleep(200);
                r.m.routeState = 1;
                r.m.dist = (mode == 3) ? d : omit;
                r.send("reroute-return (dist=" + r.m.dist + ")");
                r.settle("reroute-return");
            }
            r.sleep(250);
            r.m.dist = d;
            r.send("real distance back");
            r.settle("real distance back");
            r.sleep(700);
            r.resendSame("same-dist later");
            r.settle("later");
        } catch (Violation v) {
            noteFailure(r, v);
        } finally {
            r.finish();
        }
    }

    /** D. Reroute leaves the blink zone and re-enters with the same distM; also without reroute. */
    private static void zoneExitReenter(int d, boolean withReroute, boolean newOnReturn) throws Exception {
        Run r = new Run("D zone exit/re-enter d=" + d + (withReroute ? " via reroute" : " plain")
            + (newOnReturn ? " NEW-maneuver" : ""));
        scenarios++;
        try {
            r.m.dist = d;
            r.send("establish");
            r.settle("establish");
            r.sleep(400);
            if (withReroute) {
                r.m.routeState = 5;
                r.send("reroute-begin");
                r.sleep(150);
                r.m.routeState = 1;
                r.m.dist = 85;
                if (newOnReturn) {
                    r.m.ver++;
                    r.m.variant = 1 - r.m.variant;
                }
                r.send("reroute-return outside zone @85");
            } else {
                r.m.dist = 85;
                r.send("leave zone @85");
            }
            r.settle("outside zone");
            r.sleep(300);
            r.m.dist = d;
            r.send("re-enter zone, same distM");
            r.settle("re-enter");
            r.sleep(650);
            r.resendSame("re-entered stopped");
            r.settle("re-entered later");
            r.sleep(300);
            r.resendSame("re-entered stopped 2");
            r.settle("re-entered later 2");
        } catch (Violation v) {
            noteFailure(r, v);
        } finally {
            r.finish();
        }
    }

    /* ------------------------------------------------------------------ */
    /* Seeded random sweep, real worker                                    */
    /* ------------------------------------------------------------------ */

    private static int randDist(Random rnd) {
        int k = rnd.nextInt(12);
        if (k == 0) return 0;
        if (k == 1) return -1;
        return rnd.nextInt(41);                    // 0..40 m
    }

    private static void runRandom(int index, long seqSeed) throws Exception {
        Random rnd = new Random(seqSeed);
        Run r = new Run("random#" + index + " (seed=" + SEED + ", seq-seed=" + seqSeed + ")");
        scenarios++;
        try {
            r.m.dist = 1 + rnd.nextInt(40);
            r.m.variant = rnd.nextInt(2);
            r.send("establish");
            r.settle("establish");
            int steps = 8 + rnd.nextInt(8);
            for (int i = 0; i < steps; i++) {
                r.sleep(rnd.nextInt(4) == 0 ? rnd.nextInt(700) : rnd.nextInt(60));
                int op = rnd.nextInt(10);
                String what;
                switch (op) {
                    case 0: case 1: case 2:      // stopped car: identical distM again
                        what = "same-dist repeat";
                        r.m.dirtyExtra |= RouteGuidance.State.DIRTY_DIST_MAN;
                        break;
                    case 3: case 4:              // distance moves
                        r.m.dist = randDist(rnd);
                        what = "dist-change";
                        break;
                    case 5: case 6:              // reroute begins (distance may vanish or stay)
                        r.m.routeState = 5;
                        int k = rnd.nextInt(3);
                        if (k == 0) r.m.dist = rnd.nextInt(2) == 0 ? 0 : -1;
                        else if (k == 1) r.m.dist = randDist(rnd);
                        what = "reroute-begin";
                        break;
                    case 7: case 8:              // reroute returns
                        r.m.routeState = 1;
                        if (rnd.nextBoolean()) {
                            r.m.ver++;
                            r.m.variant = rnd.nextInt(2);
                            r.m.dist = randDist(rnd);
                            what = "return-NEW";
                        } else {
                            if (rnd.nextInt(3) == 0) r.m.dist = randDist(rnd);
                            what = "return-identical";
                        }
                        break;
                    default:                     // new maneuver while active
                        r.m.ver++;
                        r.m.variant = rnd.nextInt(2);
                        r.m.dist = randDist(rnd);
                        what = "new-maneuver";
                        break;
                }
                r.send(what);
                r.settle(what);
            }
            r.m.routeState = 1;
            if (r.m.dist <= 0) r.m.dist = 1 + rnd.nextInt(40);
            r.send("final-active");
            r.settle("final-active");
        } catch (Violation v) {
            noteFailure(r, v);
        } finally {
            r.finish();
        }
    }

    public static void main(String[] args) throws Exception {
        Log.setLevel(-1);
        Field cnf = ScreenModule.class.getDeclaredField("connected");
        cnf.setAccessible(true);
        cnf.setBoolean(null, true);

        int randomCount = 40;
        if (args != null && args.length > 0) randomCount = Integer.parseInt(args[0]);

        precomputeExpectations();

        for (int i = 0; i < ZONE_DIST.length; i++) {
            int d = ZONE_DIST[i];
            stoppedCar(d);
            for (int c = 0; c < 2; c++) {
                boolean retNew = c == 1;
                for (int g = 0; g < 2; g++) {
                    int gapBefore = g == 0 ? 0 : 350;
                    int gapDuring = g == 0 ? 100 : 700;
                    rerouteInZone(d, retNew, d, gapBefore, gapDuring, 2);
                    if (retNew) rerouteInZone(d, true, ZONE_DIST[(i + 1) % ZONE_DIST.length],
                        gapBefore, gapDuring, 1);
                }
            }
            for (int mode = 0; mode < 4; mode++) {
                omittedDistance(d, 0, mode);
                omittedDistance(d, -1, mode);
            }
            zoneExitReenter(d, true, false);
            zoneExitReenter(d, true, true);
            zoneExitReenter(d, false, false);
        }
        int detFailures = failures.size();
        int detCount = scenarios;

        System.out.println("ShortTurnBlinkVisibilityTest: random seed=" + SEED + " sequences=" + randomCount);
        Random master = new Random(SEED);
        for (int i = 0; i < randomCount; i++) runRandom(i, master.nextLong());

        System.out.println("scenarios=" + scenarios + " (deterministic " + detCount + "), "
            + "worker-not-running warnings=" + workerWarnings);
        if (!failures.isEmpty()) {
            firstFailure.dump();
            System.out.println("failing deterministic scenarios: " + detFailures + " of " + detCount
                + "; failing total (incl. random): " + failures.size());
            int show = Math.min(20, failures.size());
            for (int i = 0; i < show; i++) System.out.println("  FAIL: " + failures.get(i));
            System.out.println("ShortTurnBlinkVisibilityTest: REPRODUCED " + firstFailureReason);
            System.exit(1);
        }
        System.out.println("ShortTurnBlinkVisibilityTest: PASS (" + scenarios + " scenarios)");
        System.exit(0);
    }
}
