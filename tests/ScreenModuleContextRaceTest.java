import com.luka.carplay.cluster.ClusterLayerController;
import com.luka.carplay.core.ScreenModule;
import com.luka.carplay.framework.Log;
import de.audi.atip.hmi.view.IDisplayManager;
import de.audi.tghu.fwhmi.IDisplayManagerKombiControl;

import java.lang.reflect.Field;
import java.lang.reflect.InvocationHandler;
import java.lang.reflect.Method;
import java.lang.reflect.Proxy;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/**
 * Layer 4: Adversarial Concurrency & Physical Context Worker Race Tests.
 *
 * Exercises the multi-threaded ScreenModule switchLoop() worker, DisplayManager IPC,
 * bounce interleavings, disconnects during sleep, physical drift reconciliation,
 * and KDK opacity/fadeout transitions.
 */
public final class ScreenModuleContextRaceTest {

    private static int checks = 0;

    private static void check(boolean ok, String label) {
        checks++;
        if (!ok) {
            throw new AssertionError("CHECK FAILED: " + label);
        }
    }

    private static Object getField(Class clazz, Object target, String name) throws Exception {
        Field f = clazz.getDeclaredField(name);
        f.setAccessible(true);
        return f.get(target);
    }

    private static void setField(Class clazz, Object target, String name, Object val) throws Exception {
        Field f = clazz.getDeclaredField(name);
        f.setAccessible(true);
        f.set(target, val);
    }

    private static boolean getRebindPending() throws Exception {
        return ((Boolean) getField(ScreenModule.class, null, "rebindPending")).booleanValue();
    }

    private static String getRebindReason() throws Exception {
        return (String) getField(ScreenModule.class, null, "rebindReason");
    }

    private static final class TestDisplayManagerHandler implements InvocationHandler {
        final List switchCalls = Collections.synchronizedList(new ArrayList());
        final List rateCalls = Collections.synchronizedList(new ArrayList());
        volatile int currentContextId = 74;
        volatile boolean throwOnBounce = false;
        volatile boolean throwOnTarget = false;
        volatile boolean throwOnRate = false;
        volatile Runnable onBounceAction = null;
        volatile Runnable onTargetAction = null;
        volatile Runnable onRateAction = null;

        public Object invoke(Object proxy, Method method, Object[] args) throws Throwable {
            String name = method.getName();
            if ("switchContext".equals(name)) {
                int ctx = ((Integer) args[0]).intValue();
                switchCalls.add(new Integer(ctx));
                if (ctx == 72) {
                    if (onBounceAction != null) {
                        Runnable action = onBounceAction;
                        onBounceAction = null;
                        try { action.run(); } catch (Throwable ignored) {}
                    }
                    if (throwOnBounce) {
                        throw new RuntimeException("Simulated hardware error on bounce ctx 72");
                    }
                } else {
                    if (onTargetAction != null) {
                        Runnable action = onTargetAction;
                        onTargetAction = null;
                        try { action.run(); } catch (Throwable ignored) {}
                    }
                    if (throwOnTarget) {
                        throw new RuntimeException("Simulated hardware error on target ctx " + ctx);
                    }
                    currentContextId = ctx;
                }
                return null;
            } else if ("setUpdateRate".equals(name)) {
                rateCalls.add(args[1]);
                if (onRateAction != null) {
                    Runnable action = onRateAction;
                    onRateAction = null;
                    try { action.run(); } catch (Throwable ignored) {}
                }
                if (throwOnRate) {
                    throw new RuntimeException("Simulated hardware error on setUpdateRate");
                }
                return null;
            } else if ("getCurrentContextID".equals(name)) {
                return new Integer(currentContextId);
            }
            Class type = method.getReturnType();
            if (type == Boolean.TYPE) return Boolean.FALSE;
            if (type == Integer.TYPE) return new Integer(0);
            return null;
        }
    }

    private static IDisplayManager createMockDm(TestDisplayManagerHandler handler) {
        return (IDisplayManager) Proxy.newProxyInstance(
            ScreenModuleContextRaceTest.class.getClassLoader(),
            new Class[]{IDisplayManager.class, IDisplayManagerKombiControl.class},
            handler);
    }

    private static void startWorker(ScreenModule sm) throws Exception {
        Field wf = ScreenModule.class.getDeclaredField("worker");
        wf.setAccessible(true);
        Thread worker = (Thread) wf.get(sm);
        if (worker == null || !worker.isAlive()) {
            final Method swLoop = ScreenModule.class.getDeclaredMethod("switchLoop", new Class[0]);
            swLoop.setAccessible(true);
            worker = new Thread(new Runnable() {
                public void run() {
                    try { swLoop.invoke(sm, new Object[0]); }
                    catch (Throwable ignored) {}
                }
            }, "test-screen-worker");
            worker.setDaemon(true);
            worker.start();
            wf.set(sm, worker);
        }
    }

    public static void main(String[] args) throws Exception {
        Log.setLevel(-1);

        Field lockField = ScreenModule.class.getDeclaredField("LOCK");
        lockField.setAccessible(true);
        Object lock = lockField.get(null);

        // Bind mock kombi control to ClusterLayerController
        TestDisplayManagerHandler kdkHandler = new TestDisplayManagerHandler();
        IDisplayManagerKombiControl kdkDm = (IDisplayManagerKombiControl) createMockDm(kdkHandler);
        ClusterLayerController.bind(kdkDm, 1);
        ClusterLayerController.onVcPresentation(true);
        ClusterLayerController.onVcVisibility(false);

        // ============================================================
        // Test 1: Rebind Request Coalescing
        // ============================================================
        {
            ScreenModule.requestClusterContextRebind("Reason-A");
            check(getRebindPending(), "T1: request A sets rebindPending");
            check("Reason-A".equals(getRebindReason()), "T1: rebindReason is Reason-A");

            ScreenModule.requestClusterContextRebind("Reason-B");
            check(getRebindPending(), "T1: request B keeps rebindPending");
            check("Reason-B".equals(getRebindReason()), "T1: rebindReason superseded to Reason-B");

            // Clean up
            ScreenModule.setNavActive(false);
            check(!getRebindPending(), "T1: setNavActive(false) cleared rebindPending");
        }

        // ============================================================
        // Test 2: Invalidation on Explicit Route End
        // ============================================================
        {
            ScreenModule.requestClusterContextRebind("Stale-Reroute");
            check(getRebindPending(), "T2: request active");
            ScreenModule.setNavActive(false);
            check(!getRebindPending(), "T2: setNavActive(false) clears rebindPending");
            check("".equals(getRebindReason()), "T2: setNavActive(false) clears rebindReason");
        }

        // ============================================================
        // Test 3: KDK Fade Race & Invalidation on KDK Withdrawal
        // ============================================================
        {
            // Establish active route with KDK visible
            setField(ScreenModule.class, null, "connected", Boolean.TRUE);
            ScreenModule.setNavActive(true);
            ClusterLayerController.onVcVisibility(true);
            check(ScreenModule.isNavActive(), "T3: route active with KDK visible");

            // Route deactivates while KDK is still visible -> navHidePending keeps navActive true
            ScreenModule.setNavActive(false);
            check(ScreenModule.isNavActive(), "T3: navActive remains true during KDK fadeout");
            check(!getRebindPending(), "T3: setNavActive(false) cleared previous rebinds");

            // Stale rebind arrives during fadeout window
            ScreenModule.requestClusterContextRebind("fadeout-race");
            check(getRebindPending(), "T3: rebind pending set during fadeout");

            // Now KDK completes fadeout and withdraws
            ScreenModule.onVcKdkVisibility(false);
            check(!ScreenModule.isNavActive(), "T3: navActive released after KDK withdrawal");
            check(!getRebindPending(), "T3: KDK withdrawal MUST invalidate outstanding rebind");
        }

        // ============================================================
        // Test 4: Disconnect During the 180 ms Bounce Sleep
        // ============================================================
        {
            final ScreenModule sm = new ScreenModule();
            final TestDisplayManagerHandler dmHandler = new TestDisplayManagerHandler();
            IDisplayManager mockDm = createMockDm(dmHandler);

            dmHandler.onBounceAction = new Runnable() {
                public void run() {
                    // Stop ScreenModule while worker is inside applySwitch() bounce sleep
                    sm.stop();
                }
            };

            synchronized (lock) {
                setField(ScreenModule.class, sm, "enabled", Boolean.TRUE);
                setField(ScreenModule.class, sm, "dm", mockDm);
                setField(ScreenModule.class, null, "connected", Boolean.TRUE);
                setField(ScreenModule.class, null, "navActive", Boolean.TRUE);
                setField(ScreenModule.class, null, "desiredCtx", new Integer(80));
                setField(ScreenModule.class, null, "currentCtx", new Integer(80));
            }
            startWorker(sm);

            // Request rebind to trigger bounce
            ScreenModule.requestClusterContextRebind("stop-during-bounce");

            // Wait for worker to handle bounce and observe stop()
            long deadline = System.currentTimeMillis() + 1000;
            while (dmHandler.switchCalls.size() < 2 && System.currentTimeMillis() < deadline) {
                Thread.sleep(20);
            }

            // Expected switches: 72 (bounce started) -> superseded -> 74 (restored stock ctx)
            // It MUST NOT write 80!
            check(dmHandler.switchCalls.contains(new Integer(72)), "T4: bounce 72 was written");
            check(!dmHandler.switchCalls.contains(new Integer(80)), "T4: 80 was NOT written after stop() during bounce");
            check(dmHandler.currentContextId == 74, "T4: final physical context is 74");

            // Reset
            synchronized (lock) { setField(ScreenModule.class, sm, "dm", null); }
        }

        // ============================================================
        // Test 5: Replaced DisplayManager During Bounce Sleep
        // ============================================================
        {
            final ScreenModule sm = new ScreenModule();
            final TestDisplayManagerHandler dmAHandler = new TestDisplayManagerHandler();
            final TestDisplayManagerHandler dmBHandler = new TestDisplayManagerHandler();
            final IDisplayManager dmA = createMockDm(dmAHandler);
            final IDisplayManager dmB = createMockDm(dmBHandler);

            dmAHandler.onBounceAction = new Runnable() {
                public void run() {
                    // Replace DM while worker is sleeping in dmA bounce
                    synchronized (lock) {
                        try {
                            setField(ScreenModule.class, sm, "dm", dmB);
                            setField(ScreenModule.class, null, "currentCtx", new Integer(-1));
                            lock.notifyAll();
                        } catch (Throwable ignored) {}
                    }
                }
            };

            synchronized (lock) {
                setField(ScreenModule.class, sm, "dm", dmA);
                setField(ScreenModule.class, null, "connected", Boolean.TRUE);
                setField(ScreenModule.class, null, "navActive", Boolean.TRUE);
                setField(ScreenModule.class, null, "desiredCtx", new Integer(80));
                setField(ScreenModule.class, null, "currentCtx", new Integer(80));
            }
            startWorker(sm);

            ScreenModule.requestClusterContextRebind("dm-replaced");

            long deadline = System.currentTimeMillis() + 1500;
            while (dmBHandler.switchCalls.size() < 2 && System.currentTimeMillis() < deadline) {
                Thread.sleep(20);
            }

            // dmA got bounce 72, but never got target 80
            check(dmAHandler.switchCalls.contains(new Integer(72)), "T5: dmA received initial bounce 72");
            check(!dmAHandler.switchCalls.contains(new Integer(80)), "T5: dmA switch was superseded, no 80 write");

            // dmB picked up the context switch cleanly
            check(dmBHandler.switchCalls.contains(new Integer(72)), "T5: dmB received bounce 72");
            check(dmBHandler.switchCalls.contains(new Integer(80)), "T5: dmB received target 80");
            check(dmBHandler.currentContextId == 80, "T5: dmB established in ctx 80");

            synchronized (lock) { setField(ScreenModule.class, sm, "dm", null); }
        }

        // ============================================================
        // Test 6: Physical Context Drift Reconciliation
        // ============================================================
        {
            final ScreenModule sm = new ScreenModule();
            final TestDisplayManagerHandler dmHandler = new TestDisplayManagerHandler();
            IDisplayManager mockDm = createMockDm(dmHandler);
            dmHandler.currentContextId = 80;

            synchronized (lock) {
                setField(ScreenModule.class, sm, "dm", mockDm);
                setField(ScreenModule.class, null, "connected", Boolean.TRUE);
                setField(ScreenModule.class, null, "navActive", Boolean.TRUE);
                setField(ScreenModule.class, null, "desiredCtx", new Integer(80));
                setField(ScreenModule.class, null, "currentCtx", new Integer(80));
            }
            startWorker(sm);

            // Simulate external drift: HU unexpectedly kicked cluster to ctx 74
            dmHandler.switchCalls.clear();
            dmHandler.currentContextId = 74;

            // Wait for CONTEXT_RECONCILE_MS (250ms) reconciliation poll
            long deadline = System.currentTimeMillis() + 1200;
            while (dmHandler.switchCalls.size() < 2 && System.currentTimeMillis() < deadline) {
                Thread.sleep(50);
            }

            // Reconcile must detect drift and re-apply 72 -> 80
            check(dmHandler.switchCalls.contains(new Integer(72)), "T6: reconcile detected drift, triggered bounce 72");
            check(dmHandler.switchCalls.contains(new Integer(80)), "T6: reconcile re-acquired ctx 80");
            check(dmHandler.currentContextId == 80, "T6: physical context reconciled to 80");

            synchronized (lock) { setField(ScreenModule.class, sm, "dm", null); }
        }

        // ============================================================
        // Test 7: Overlapping Rebind Requests (A, B, C) During Bounce
        // ============================================================
        {
            final ScreenModule sm = new ScreenModule();
            final TestDisplayManagerHandler dmHandler = new TestDisplayManagerHandler();
            IDisplayManager mockDm = createMockDm(dmHandler);

            dmHandler.onBounceAction = new Runnable() {
                public void run() {
                    // Send rapid overlapping requests during bounce
                    ScreenModule.requestClusterContextRebind("Reason-B");
                    ScreenModule.requestClusterContextRebind("Reason-C");
                }
            };

            synchronized (lock) {
                setField(ScreenModule.class, sm, "dm", mockDm);
                setField(ScreenModule.class, null, "connected", Boolean.TRUE);
                setField(ScreenModule.class, null, "navActive", Boolean.TRUE);
                setField(ScreenModule.class, null, "desiredCtx", new Integer(80));
                setField(ScreenModule.class, null, "currentCtx", new Integer(80));
            }
            startWorker(sm);

            ScreenModule.requestClusterContextRebind("Reason-A");

            long deadline = System.currentTimeMillis() + 1500;
            while (dmHandler.switchCalls.size() < 4 && System.currentTimeMillis() < deadline) {
                Thread.sleep(20);
            }

            // Must execute initial bounce/switch + subsequent coalesced bounce/switch
            check(dmHandler.switchCalls.size() >= 4, "T7: overlapping rebinds executed (calls=" + dmHandler.switchCalls + ")");
            check(!getRebindPending(), "T7: all rebinds fully drained");

            synchronized (lock) { setField(ScreenModule.class, sm, "dm", null); }
        }

        // ============================================================
        // Test 8: Resilient Recovery on Failed switchContext()
        // ============================================================
        {
            final ScreenModule sm = new ScreenModule();
            final TestDisplayManagerHandler dmHandler = new TestDisplayManagerHandler();
            IDisplayManager mockDm = createMockDm(dmHandler);

            // 1. Throw on bounce 72
            dmHandler.throwOnBounce = true;
            synchronized (lock) {
                setField(ScreenModule.class, sm, "dm", mockDm);
                setField(ScreenModule.class, null, "connected", Boolean.TRUE);
                setField(ScreenModule.class, null, "navActive", Boolean.TRUE);
                setField(ScreenModule.class, null, "desiredCtx", new Integer(80));
                setField(ScreenModule.class, null, "currentCtx", new Integer(74));
            }
            startWorker(sm);

            // Let worker attempt switch and fail
            Thread.sleep(150);
            check(dmHandler.switchCalls.contains(new Integer(72)), "T8.1: attempted switch 72");
            check(dmHandler.currentContextId == 74, "T8.1: failure throttled, not marked 80");

            // Stop throwing and verify self-healing recovery
            dmHandler.throwOnBounce = false;
            long deadline = System.currentTimeMillis() + 1000;
            while (dmHandler.currentContextId != 80 && System.currentTimeMillis() < deadline) {
                Thread.sleep(20);
            }
            check(dmHandler.currentContextId == 80, "T8.1: worker recovered and established ctx 80");

            // 2. Throw on target 80
            dmHandler.switchCalls.clear();
            dmHandler.throwOnTarget = true;
            ScreenModule.requestClusterContextRebind("retry-target");
            Thread.sleep(150);
            check(dmHandler.switchCalls.contains(new Integer(72)), "T8.2: bounce 72 succeeded before target error");

            dmHandler.throwOnTarget = false;
            deadline = System.currentTimeMillis() + 1000;
            while (dmHandler.currentContextId != 80 && System.currentTimeMillis() < deadline) {
                Thread.sleep(20);
            }
            check(dmHandler.currentContextId == 80, "T8.2: target recovered to ctx 80");

            synchronized (lock) { setField(ScreenModule.class, sm, "dm", null); }
        }

        // ============================================================
        // Test 9: Start/Stop/Start During the 180 ms Bounce
        // ============================================================
        {
            final ScreenModule smA = new ScreenModule();
            final ScreenModule smB = new ScreenModule();
            final TestDisplayManagerHandler dmAHandler = new TestDisplayManagerHandler();
            dmAHandler.currentContextId = 80;
            final IDisplayManager dmA = createMockDm(dmAHandler);
            final TestDisplayManagerHandler dmBHandler = new TestDisplayManagerHandler();
            dmBHandler.currentContextId = 74;
            final IDisplayManager dmB = createMockDm(dmBHandler);

            dmAHandler.onBounceAction = new Runnable() {
                public void run() {
                    // While worker A is in 180 ms sleep after writing 72:
                    // Stop Session A and Start Session B in stock context 74
                    try {
                        smA.stop();
                        setField(ScreenModule.class, smB, "enabled", Boolean.TRUE);
                        synchronized (lock) {
                            setField(ScreenModule.class, smB, "dm", dmB);
                            setField(ScreenModule.class, null, "desiredCtx", new Integer(74));
                            setField(ScreenModule.class, null, "currentCtx", new Integer(74));
                            setField(ScreenModule.class, null, "navActive", Boolean.FALSE);
                            lock.notifyAll();
                        }
                        startWorker(smB);
                    } catch (Throwable ignored) {}
                }
            };

            setField(ScreenModule.class, smA, "enabled", Boolean.TRUE);
            synchronized (lock) {
                setField(ScreenModule.class, smA, "dm", dmA);
                setField(ScreenModule.class, null, "connected", Boolean.TRUE);
                setField(ScreenModule.class, null, "navActive", Boolean.TRUE);
                setField(ScreenModule.class, null, "desiredCtx", new Integer(80));
                setField(ScreenModule.class, null, "currentCtx", new Integer(80));
            }
            startWorker(smA);

            // Initiate rebind in session A
            ScreenModule.requestClusterContextRebind("rebind-session-A");

            // Wait for bounce sleep to elapse and worker A to exit
            Thread.sleep(300);

            // Assert: old bounce did NOT write 80 into session B
            check(!dmBHandler.switchCalls.contains(new Integer(80)),
                "T9: old bounce 180ms expiry did NOT write ctx 80 into Session B");
            check(dmBHandler.currentContextId == 74,
                "T9: Session B cleanly retained stock ctx 74");
            check(!dmAHandler.switchCalls.contains(new Integer(80)),
                "T9: Session A worker aborted before writing 80 after stop");

            smB.stop();
        }

        // ============================================================
        // Test 10: Pending Rebind Across Connect / Disconnect / Reconnect
        // ============================================================
        {
            ScreenModule sm = new ScreenModule();
            TestDisplayManagerHandler dmHandler = new TestDisplayManagerHandler();
            dmHandler.currentContextId = 80;
            IDisplayManager dm = createMockDm(dmHandler);

            setField(ScreenModule.class, sm, "enabled", Boolean.TRUE);
            synchronized (lock) {
                setField(ScreenModule.class, sm, "dm", dm);
                setField(ScreenModule.class, null, "connected", Boolean.TRUE);
                setField(ScreenModule.class, null, "navActive", Boolean.TRUE);
                setField(ScreenModule.class, null, "desiredCtx", new Integer(80));
                setField(ScreenModule.class, null, "currentCtx", new Integer(80));
            }
            startWorker(sm);

            // Session A: route generation rebind arrives
            synchronized (lock) {
                ScreenModule.requestClusterContextRebind("session-A-leak-check");
                check(((Boolean) getField(ScreenModule.class, null, "rebindPending")).booleanValue(),
                    "T10: rebindPending is true in Session A");
            }

            // Session A disconnects / stops before worker consumes it
            sm.stop();
            check(!((Boolean) getField(ScreenModule.class, null, "rebindPending")).booleanValue(),
                "T10: stop() cleared rebindPending");
            check("".equals(getField(ScreenModule.class, null, "rebindReason")),
                "T10: stop() cleared rebindReason");

            // Session B starts
            ScreenModule smB = new ScreenModule();
            TestDisplayManagerHandler dmBHandler = new TestDisplayManagerHandler();
            dmBHandler.currentContextId = 74;
            IDisplayManager dmB = createMockDm(dmBHandler);
            setField(ScreenModule.class, smB, "enabled", Boolean.TRUE);
            synchronized (lock) {
                setField(ScreenModule.class, smB, "dm", dmB);
                setField(ScreenModule.class, null, "connected", Boolean.TRUE);
                setField(ScreenModule.class, null, "navActive", Boolean.FALSE);
                setField(ScreenModule.class, null, "desiredCtx", new Integer(74));
                setField(ScreenModule.class, null, "currentCtx", new Integer(74));
            }
            startWorker(smB);

            // Let worker B run
            Thread.sleep(100);

            // Assert: Session B is completely clean, no rebind executed from Session A
            check(!((Boolean) getField(ScreenModule.class, null, "rebindPending")).booleanValue(),
                "T10: Session B rebindPending remains false");
            check(dmBHandler.switchCalls.isEmpty(),
                "T10: zero switchContext calls in Session B from leftover Session A rebind");
            check(dmBHandler.currentContextId == 74,
                "T10: Session B currentContextId is 74");

            smB.stop();
        }

        // ============================================================
        // Test 11: Physical Context Drifted Before Route-Generation Rebind
        // ============================================================
        {
            ScreenModule sm = new ScreenModule();
            // Start physical context in 74 (drifted from logical 80)
            TestDisplayManagerHandler dmHandler = new TestDisplayManagerHandler();
            dmHandler.currentContextId = 74;
            IDisplayManager dm = createMockDm(dmHandler);

            setField(ScreenModule.class, sm, "enabled", Boolean.TRUE);
            synchronized (lock) {
                setField(ScreenModule.class, sm, "dm", dm);
                setField(ScreenModule.class, null, "connected", Boolean.TRUE);
                setField(ScreenModule.class, null, "navActive", Boolean.TRUE);
                setField(ScreenModule.class, null, "desiredCtx", new Integer(80));
                setField(ScreenModule.class, null, "currentCtx", new Integer(80));
            }
            startWorker(sm);

            // Request route generation rebind while physical context is drifted
            ScreenModule.requestClusterContextRebind("rebind-on-drifted");

            long deadline = System.currentTimeMillis() + 1000;
            while (dmHandler.currentContextId != 80 && System.currentTimeMillis() < deadline) {
                Thread.sleep(20);
            }

            // Assert: physical context reconciled to 80 with exactly one 72->80 bounce
            check(dmHandler.currentContextId == 80,
                "T11: physical context reconciled to 80");
            check(dmHandler.switchCalls.size() == 2,
                "T11: exactly one recovery cycle (calls=" + dmHandler.switchCalls + ")");
            check(((Integer) dmHandler.switchCalls.get(0)).intValue() == 72,
                "T11: first call was 72");
            check(((Integer) dmHandler.switchCalls.get(1)).intValue() == 80,
                "T11: second call was 80");

            sm.stop();
        }

        // ============================================================
        // Test 12: Disconnect Exactly Between Final Validation and Target IPC
        // ============================================================
        {
            final ScreenModule sm = new ScreenModule();
            final TestDisplayManagerHandler dmHandler = new TestDisplayManagerHandler();
            IDisplayManager mockDm = createMockDm(dmHandler);

            dmHandler.onTargetAction = new Runnable() {
                public void run() {
                    // Route ends or disconnects at the exact instant switchContext(80) executes
                    sm.stop();
                }
            };

            setField(ScreenModule.class, sm, "enabled", Boolean.TRUE);
            synchronized (lock) {
                setField(ScreenModule.class, sm, "dm", mockDm);
                setField(ScreenModule.class, null, "connected", Boolean.TRUE);
                setField(ScreenModule.class, null, "navActive", Boolean.TRUE);
                setField(ScreenModule.class, null, "desiredCtx", new Integer(80));
                setField(ScreenModule.class, null, "currentCtx", new Integer(74));
            }
            startWorker(sm);

            // Wait for worker to complete the switch sequence:
            // 72 (bounce) -> post-sleep check sees 80 -> switchContext(80) executes and fires onTargetAction (sm.stop())
            // -> self-heals by keeping currentCtx=-1 -> worker detects desiredCtx=74 -> switchContext(74)
            long deadline = System.currentTimeMillis() + 1500;
            while (dmHandler.switchCalls.size() < 3 && System.currentTimeMillis() < deadline) {
                Thread.sleep(20);
            }

            // Assert:
            // physical trace = 72, 80, 74
            List expectedCalls = new ArrayList();
            expectedCalls.add(new Integer(72));
            expectedCalls.add(new Integer(80));
            expectedCalls.add(new Integer(74));
            check(dmHandler.switchCalls.equals(expectedCalls),
                "T12: physical trace mismatch! Expected " + expectedCalls + " but got " + dmHandler.switchCalls);
            check(dmHandler.currentContextId == 74, "T12: final physical context is 74");
            check(ScreenModule.getDesiredCtx() == 74, "T12: final desired context is 74");
            check(ScreenModule.getCurrentCtx() == 74, "T12: final currentCtx is 74");
            check(!ScreenModule.isNavActive(), "T12: navActive is false");

            // Reset
            synchronized (lock) { setField(ScreenModule.class, sm, "dm", null); }
        }

        // ============================================================
        // Test 13: Exception in setUpdateRate Clears Both currentCtx and clusterActive
        // ============================================================
        {
            final ScreenModule sm = new ScreenModule();
            final TestDisplayManagerHandler dmHandler = new TestDisplayManagerHandler();
            dmHandler.throwOnRate = true;
            IDisplayManager mockDm = createMockDm(dmHandler);

            setField(ScreenModule.class, sm, "enabled", Boolean.TRUE);
            synchronized (lock) {
                setField(ScreenModule.class, sm, "dm", mockDm);
                setField(ScreenModule.class, null, "connected", Boolean.TRUE);
                setField(ScreenModule.class, null, "navActive", Boolean.TRUE);
                setField(ScreenModule.class, null, "desiredCtx", new Integer(80));
                setField(ScreenModule.class, null, "currentCtx", new Integer(74));
            }
            startWorker(sm);

            // Wait for first switch attempt where setUpdateRate throws:
            // 72 (bounce) -> 80 (switchContext) -> setUpdateRate throws -> outer catch resets currentCtx = -1 AND clusterActive = false
            long deadline = System.currentTimeMillis() + 1500;
            while (dmHandler.rateCalls.size() < 1 && System.currentTimeMillis() < deadline) {
                Thread.sleep(20);
            }

            // Assert: during exception recovery, currentCtx is -1 and clusterActive is explicitly false
            check(ScreenModule.getCurrentCtx() == -1, "T13: currentCtx is -1 during recovery");
            check(!ScreenModule.isClusterActive(), "T13: clusterActive is false during recovery (no stale active state)");

            // Now allow recovery:
            dmHandler.throwOnRate = false;
            deadline = System.currentTimeMillis() + 1500;
            while (ScreenModule.getCurrentCtx() != 80 && System.currentTimeMillis() < deadline) {
                Thread.sleep(20);
            }

            check(ScreenModule.getCurrentCtx() == 80, "T13: currentCtx recovered to 80");
            check(ScreenModule.isClusterActive(), "T13: clusterActive is true after recovery");

            sm.stop();
            // Reset
            synchronized (lock) { setField(ScreenModule.class, sm, "dm", null); }
        }

        System.out.println("ScreenModuleContextRaceTest: ALL 13 CONCURRENCY/RACE SUITES PASS (" + checks + " checks)");
    }
}
