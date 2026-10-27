package com.luka.carplay.core;

import com.luka.carplay.cluster.ClusterLayerController;
import com.luka.carplay.framework.Log;
import de.audi.atip.hmi.view.IDisplayManager;
import java.lang.reflect.Field;
import java.lang.reflect.InvocationHandler;
import java.lang.reflect.Method;
import java.lang.reflect.Proxy;
import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.List;
import java.util.Random;

/** Seeded fault injection against shipping code, plus an independent KDK lifecycle model. */
public final class AltScreenContextFuzzTest {
    private static final Field LOCK = field("LOCK"), CURRENT = field("currentCtx"), ACTIVE = field("clusterActive"),
        DESIRED = field("desiredCtx"), READY = field("rendererReady"), DM = field("dm"),
        CONNECTED = field("connected"), VIDEO = field("videoAvailable"), NAV = field("navActive"),
        HOLD = field("navHidePending");
    private static final Method APPLY;
    private static long checks;
    private static final ArrayDeque<String> history = new ArrayDeque<String>();
    static {
        try {
            APPLY = AltScreenModule.class.getDeclaredMethod("applySwitch", Integer.TYPE, IDisplayManager.class);
            APPLY.setAccessible(true);
        } catch (Exception e) { throw new ExceptionInInitializerError(e); }
    }
    private static Field field(String name) {
        try {
            Field f = AltScreenModule.class.getDeclaredField(name); f.setAccessible(true); return f;
        } catch (Exception e) { throw new ExceptionInInitializerError(e); }
    }
    private static void check(boolean value, String why) {
        checks++;
        if (!value) throw new AssertionError(why);
    }
    private static void remember(String event) {
        if (history.size() == 32) history.removeFirst();
        history.addLast(event);
    }
    private static final class Display implements InvocationHandler {
        final AltScreenModule module;
        final Object lock;
        final List<Integer> writes = new ArrayList<Integer>();
        int target, eventStage, event, fault, current, rate = 30;
        boolean afterWrite, eventFired, failed, acknowledged;
        IDisplayManager manager;

        Display(AltScreenModule module) throws Exception {
            this.module = module; lock = LOCK.get(null);
            manager = (IDisplayManager) Proxy.newProxyInstance(IDisplayManager.class.getClassLoader(),
                new Class[]{IDisplayManager.class}, this);
        }
        private void event(int stage) throws Exception {
            if (eventFired || eventStage != stage) return;
            eventFired = true;
            synchronized (lock) {
                if (event == 1) { CONNECTED.setBoolean(null, false); DESIRED.setInt(null, 74); }
                else if (event == 2) DESIRED.setInt(null, target == 80 ? 81 : 80);
                else if (event == 3) READY.setBoolean(null, false);
                else if (event == 4) {
                    DM.set(module, new Display(module).manager);
                    CURRENT.setInt(null, -1); ACTIVE.setBoolean(null, false);
                }
            }
        }
        private void fail() {
            failed = true;
            throw new IllegalStateException("injected IPC fault " + fault);
        }
        public Object invoke(Object proxy, Method method, Object[] args) throws Exception {
            String name = method.getName();
            if ("switchContext".equals(name)) {
                int context = ((Integer) args[0]).intValue();
                int stage = context == 72 ? 1 : 2;
                event(stage);
                boolean failing = (fault == 1 && stage == 1) || (fault == 2 && stage == 2);
                if (failing && !afterWrite) fail();
                current = context; writes.add(Integer.valueOf(context));
                if (failing) fail();
                if (context == target) acknowledged = true;
            } else if ("setUpdateRate".equals(name)) {
                int next = ((Integer) args[1]).intValue();
                if ((fault == 3 && next == 30) || (fault == 4 && next == 0)) fail();
                rate = next;
            } else if ("getCurrentContextID".equals(name)) return Integer.valueOf(current);
            return null;
        }
    }
    private static void apply(AltScreenModule module, int target, IDisplayManager dm) throws Exception {
        // Exercise the production interrupted-settle path to keep exhaustive/fuzz runs bounded.
        // AltScreenStartupTest and AltScreenContextRaceTest exercise real waits and concurrent IPC.
        Thread.currentThread().interrupt();
        try { APPLY.invoke(module, Integer.valueOf(target), dm); }
        finally { Thread.interrupted(); }
    }
    private static void switchCase(int initial, int target, int stage, int event, int fault,
                                   boolean afterWrite, boolean staleActive) throws Exception {
        AltScreenModule module = new AltScreenModule();
        Display display = new Display(module);
        display.current = initial;
        display.target = target; display.eventStage = stage; display.event = event;
        display.fault = fault; display.afterWrite = afterWrite;
        synchronized (display.lock) {
            DM.set(module, display.manager); CURRENT.setInt(null, initial);
            ACTIVE.setBoolean(null, staleActive || initial == 80 || initial == 81);
            DESIRED.setInt(null, target); READY.setBoolean(null, true);
            CONNECTED.setBoolean(null, target != 74); VIDEO.setBoolean(null, false);
        }
        remember("switch initial=" + initial + " target=" + target + " stage=" + stage
            + " event=" + event + " fault=" + fault + " afterWrite=" + afterWrite + " staleActive=" + staleActive);
        apply(module, target, display.manager);
        synchronized (display.lock) {
            boolean retired = DM.get(module) != display.manager || DESIRED.getInt(null) != target
                || (target != 74 && !READY.getBoolean(null));
            boolean confirmed = display.acknowledged && !display.failed && !retired;
            check(CURRENT.getInt(null) == (confirmed ? target : -1), "unconfirmed target cached: " + display.writes);
            check(ACTIVE.getBoolean(null) == (confirmed && target != 74), "unconfirmed cluster activity");
            if (target == 74 && fault != 3 && fault != 4)
                check(display.rate == 30, "stock restoration left terminal without its frame rate");
            if ((initial == 80 || initial == 81) && target != 74)
                check(!display.writes.contains(Integer.valueOf(72)), "healthy 80/81 switch bounced through stock");
        }
        // Every fault/supersession must recover with the latest peer/intent and no remaining faults.
        Display recovery = new Display(module);
        int desired = DESIRED.getInt(null);
        recovery.target = desired;
        synchronized (display.lock) {
            DM.set(module, recovery.manager); CURRENT.setInt(null, -1); READY.setBoolean(null, true);
        }
        apply(module, desired, recovery.manager);
        check(CURRENT.getInt(null) == desired && ACTIVE.getBoolean(null) == (desired != 74), "recovery did not converge");
        check(recovery.current == desired && recovery.rate == 30, "recovery failed physical context/FPS");
    }
    private static void lifecycle(Random random, int iterations) throws Exception {
        boolean route = false, hold = false, visible = false;
        Object lock = LOCK.get(null);
        synchronized (lock) {
            CONNECTED.setBoolean(null, true); VIDEO.setBoolean(null, false);
            NAV.setBoolean(null, false); HOLD.setBoolean(null, false);
            DESIRED.setInt(null, 81);
        }
        ClusterLayerController.onVcVisibility(false);
        for (int i = 0; i < iterations; i++) {
            int event = random.nextInt(5);
            remember("KDK step=" + i + " event=" + event + " route=" + route + " hold=" + hold + " visible=" + visible);
            if (event == 0) {
                route = true; hold = false;
                AltScreenModule.setNavActive(true);
            } else if (event == 1) {
                hold = visible && (route || hold); route = false;
                AltScreenModule.setNavActive(false);
            } else if (event == 2 || event == 3) {
                visible = event == 2;
                if (!visible) hold = false;
                ClusterLayerController.onVcVisibility(visible);
            } else ClusterLayerController.onVcPresentation(random.nextBoolean());
            boolean permitted = route || hold;
            check(NAV.getBoolean(null) == permitted, "route/KDK visibility model diverged");
            check(HOLD.getBoolean(null) == hold, "route-end hold model diverged");
            check(DESIRED.getInt(null) == (permitted ? 80 : 81), "KDK event changed the map context incorrectly");
        }
    }
    public static void main(String[] args) throws Exception {
        Log.setLevel(-1);
        long seed = Long.decode(args[0]).longValue();
        int iterations = Integer.parseInt(args[1]);
        Random random = new Random(seed);
        int[] starts = {-1, 74, 80, 81}, targets = {74, 80, 81};
        long cases = 0;
        try {
            // Exhaust all callback-stage/event/fault combinations before randomized cases.
            for (int initial : starts) for (int target : targets)
                for (int stage = 1; stage <= 2; stage++) for (int event = 0; event <= 4; event++)
                    for (int fault = 0; fault <= 4; fault++) for (int after = 0; after < 2; after++) {
                        switchCase(initial, target, stage, event, fault, after == 1, true); cases++;
                    }
            for (int i = 0; i < iterations; i++) {
                switchCase(starts[random.nextInt(starts.length)], targets[random.nextInt(targets.length)],
                    1 + random.nextInt(2), random.nextInt(5), random.nextInt(5), random.nextBoolean(), random.nextBoolean());
                cases++;
            }
            lifecycle(random, iterations);
        } catch (Throwable t) {
            System.err.println("AltScreenContextFuzzTest FAIL seed=" + seed + " completedSwitchCases=" + cases);
            for (String event : history) System.err.println(event);
            throw t;
        }
        System.out.println("AltScreenContextFuzzTest PASS seed=" + seed + " switchCases=" + cases
            + " lifecycleEvents=" + iterations + " checks=" + checks);
    }
}
