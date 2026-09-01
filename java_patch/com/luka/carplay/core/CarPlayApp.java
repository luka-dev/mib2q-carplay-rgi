/*
 * CarPlayApp — entry point + module lifecycle.
 *
 * Called from the stock hook in TerminalModeBapCombi$ActiveDeviceStateListener:
 *   CarPlayApp.onActivate(ctx)   // CarPlay device connected  (ctx = IContext)
 *   CarPlayApp.onDeactivate()    // disconnected
 *
 * TerminalMode callbacks only publish the desired active/context generation and
 * return.  A persistent daemon worker serializes every Module start/stop away
 * from the stock HMI lifecycle thread.  A module may return false from start()
 * if a service it needs is not up yet; a separate daemon retry keeps trying
 * until everything is up or the CarPlay generation ends.  Navigation can appear
 * much later than TerminalMode on a cold boot, so this must not have a fixed timeout.
 *
 * Java 1.2 (no generics/autoboxing).
 *
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */
package com.luka.carplay.core;

import com.luka.carplay.bus.CarplayBus;
import com.luka.carplay.framework.Log;

import de.audi.app.terminalmode.IContext;
import de.audi.atip.base.IFrameworkAccess;

public final class CarPlayApp {
    private static final String TAG = "App";
    public static final String BUILD_ID = "@BUILD_ID@";

    /* Modules, in start order.  Populated as each module lands (rgd, coverart,
     * input, screen). */
    private static final Module[] MODULES = new Module[] {
        new ScreenModule(), new RgdModule(), new SteeringWheelInputModule()
    };

    private static final Object lock = new Object();
    private static final Object lifecycleLock = new Object();
    private static final boolean[] started = new boolean[MODULES.length];
    private static volatile boolean active = false;
    private static volatile FrameworkRef fwRef;
    private static Thread retryThread;
    private static int retryGeneration;
    private static int serviceChangeGeneration;
    private static Thread lifecycleThread;
    private static int lifecycleGeneration;
    private static int lifecycleAppliedGeneration;
    private static IContext desiredContext;
    private static final int RGD_MODULE_INDEX = 1;

    private CarPlayApp() {}

    /** Current HMI framework access, or null before activate / after deactivate.
     *  Replaces the old CarPlayHook.getFrameworkAccess() static seam. */
    public static IFrameworkAccess framework() {
        FrameworkRef r = fwRef;
        return r != null ? r.framework() : null;
    }

    /** True while a CarPlay device is connected (onActivate..onDeactivate).
     *  REPLACE mode gates roller/key capture on this (whole-session takeover),
     *  not on cluster-tab focus. */
    public static boolean isActive() { return active; }

    /** Bring the transport up ALWAYS-ON, independent of any CarPlay session.
     *  Called from the patched TerminalModeBapCombi.init() (component start / boot),
     *  so the C hook can connect once and stay connected — onActivate/onDeactivate
     *  then only gate the CarPlay modules, not the socket.  Idempotent.  Also seeds
     *  fwRef so framework() is available before the first activate. */
    public static void startTransport(Object context) {
        CarplayBus.getInstance().start();            /* idempotent */
        if (context instanceof IContext) {
            synchronized (lock) {
                if (fwRef == null) fwRef = new FrameworkRef((IContext) context);
                ensureLifecycleWorkerLocked();
            }
        }
        Log.i(TAG, "transport up (bus always-on) build=" + BUILD_ID);
    }

    public static void onActivate(Object context) {
        if (!(context instanceof IContext)) {
            Log.e(TAG, "context is not IContext: " + context);
            return;
        }
        synchronized (lock) {
            ensureLifecycleWorkerLocked();
            /* ACTIVATING may be repeated for the same TMDevice.  Publishing the
             * same desired edge twice must not restart modules that are already
             * converging on the worker/retry path. */
            if (active && desiredContext == context) return;
            active = true;                           /* visible to stock HMI immediately */
            desiredContext = (IContext) context;
            serviceChangeGeneration++;
            lifecycleGeneration++;
            lock.notifyAll();
        }
    }

    private static int publishDeactivate() {
        synchronized (lock) {
            ensureLifecycleWorkerLocked();
            if (!active && desiredContext == null) return lifecycleGeneration;
            active = false;                          /* release stock HMI immediately */
            desiredContext = null;
            serviceChangeGeneration++;
            lifecycleGeneration++;
            lock.notifyAll();
            return lifecycleGeneration;
        }
    }

    public static void onDeactivate() { publishDeactivate(); }

    /** Component teardown is not the hot TMDevice callback.  Let it wait for
     * the already-published async cleanup before TerminalMode closes its OSGi
     * trackers; the bound prevents a broken module from hanging HMI shutdown. */
    public static void onDeactivateAndWait() {
        int generation = publishDeactivate();
        long deadline = System.currentTimeMillis() + 3000L;
        synchronized (lock) {
            while (lifecycleAppliedGeneration != generation
                    && lifecycleGeneration == generation) {
                long remaining = deadline - System.currentTimeMillis();
                if (remaining <= 0L) {
                    Log.w(TAG, "deactivate cleanup timed out generation=" + generation);
                    break;
                }
                try { lock.wait(remaining); }
                catch (InterruptedException e) {
                    Thread.currentThread().interrupt();
                    break;
                }
            }
        }
    }


    /** Caller holds lock.  Started at TerminalMode component init, so the hot
     * ACTIVATING callback normally performs only field stores + notifyAll(). */
    private static void ensureLifecycleWorkerLocked() {
        if (lifecycleThread != null && lifecycleThread.isAlive()) return;
        Thread t = new Thread(new Runnable() {
            public void run() { lifecycleLoop(); }
        }, "carplay-lifecycle");
        t.setDaemon(true);
        lifecycleThread = t;
        try {
            t.start();
        } catch (Throwable x) {
            lifecycleThread = null;
            Log.e(TAG, "lifecycle worker start failed: " + x);
        }
    }

    private static boolean lifecycleCurrent(int generation, boolean wantActive,
                                            IContext context) {
        synchronized (lock) {
            return generation == lifecycleGeneration && active == wantActive
                && (!wantActive || desiredContext == context);
        }
    }

    private static void clearStarted() {
        synchronized (lock) {
            for (int i = 0; i < MODULES.length; i++) started[i] = false;
        }
    }

    private static void stopModules() {
        /* A false-returning start may still have acquired partial resources, so
         * every new lifecycle generation stops all modules in reverse order. */
        for (int i = MODULES.length - 1; i >= 0; i--) {
            try { MODULES[i].stop(); }
            catch (Throwable t) { Log.w(TAG, MODULES[i].name() + " stop: " + t); }
        }
        clearStarted();
    }

    private static void applyLifecycle(int generation, boolean wantActive,
                                       IContext context) {
        synchronized (lifecycleLock) {
            if (!lifecycleCurrent(generation, wantActive, context)) return;
            stopRetry();

            if (!wantActive) {
                Log.i(TAG, "onDeactivate async apply generation=" + generation);
                stopModules();
                return;
            }

            /* FrameworkRef construction calls into stock IContext and therefore
             * also belongs here, not on ActiveDeviceStateListener. */
            FrameworkRef next = new FrameworkRef(context);
            if (!lifecycleCurrent(generation, true, context)) return;
            synchronized (lock) { fwRef = next; }

            Log.i(TAG, "onActivate async apply generation=" + generation
                + " build=" + BUILD_ID);
            CarplayBus.getInstance().start();        /* idempotent; off stock lifecycle thread */
            stopModules();
            if (!lifecycleCurrent(generation, true, context)) return;
            if (!startPass(generation)
                    && lifecycleCurrent(generation, true, context)) {
                startRetry(generation);
            }
        }
    }

    /** Grace window to absorb a transient TMDevice deactivate bounce (Mode A) before
     * committing a full teardown; a re-activation within it drops the stale edge. */
    private static final long DEACTIVATE_DEBOUNCE_MS = 400L;

    private static void lifecycleLoop() {
        while (true) {
            int generation;
            boolean wantActive;
            IContext context;
            synchronized (lock) {
                while (lifecycleAppliedGeneration == lifecycleGeneration) {
                    try { lock.wait(); }
                    catch (InterruptedException e) { /* persistent worker */ }
                }
                generation = lifecycleGeneration;
                wantActive = active;
                context = desiredContext;

                /* Debounce a deactivate edge (Mode A): a rough reconnect/replug bounces the
                 * stock TMDevice ACTIVATING<->INVALID<->ACTIVE, publishing a transient inactive
                 * edge superseded by an active one ~ms later.  Applying it at once does a full
                 * BAPBridge takeover teardown that then has to re-init — and if a dio reconnect
                 * lands mid-teardown the session collapses.  Wait briefly; if a newer edge
                 * arrives (the re-activation), drop this stale inactive edge and coalesce to it. */
                if (!wantActive) {
                    long deadline = System.currentTimeMillis() + DEACTIVATE_DEBOUNCE_MS;
                    while (lifecycleGeneration == generation) {
                        long remaining = deadline - System.currentTimeMillis();
                        if (remaining <= 0L) break;
                        try { lock.wait(remaining); }
                        catch (InterruptedException e) { Thread.currentThread().interrupt(); break; }
                    }
                    if (lifecycleGeneration != generation) {
                        lifecycleAppliedGeneration = generation;   /* stale inactive edge — drop it */
                        lock.notifyAll();
                        continue;                                  /* re-snapshot the newer edge */
                    }
                }
            }
            try {
                applyLifecycle(generation, wantActive, context);
            } catch (Throwable t) {
                Log.e(TAG, "lifecycle apply failed generation=" + generation + ": " + t);
            } finally {
                synchronized (lock) {
                    /* If a newer request arrived while external code ran, mark
                     * only this snapshot applied; the loop immediately observes
                     * the newer generation and coalesces to its final state. */
                    lifecycleAppliedGeneration = generation;
                    lock.notifyAll();
                }
            }
        }
    }

    /** Navigation's OSGi service can disappear/reappear without a CarPlay reconnect.  The RGD
     * module owns a paired ServiceReference, so rebuild only that module on the exact service
     * edge instead of continuing to call a stale AppConnectorNavi instance.  ClusterService
     * invokes this from NavigationJobs; the small worker keeps teardown/start outside that
     * dispatcher and coalesces rapid remove+add pairs. */
    public static void onNavigationServiceChanged() {
        final int generation;
        final int activeLifecycleGeneration;
        final IContext activeContext;
        synchronized (lock) {
            if (!active) return;
            generation = ++serviceChangeGeneration;
            activeLifecycleGeneration = lifecycleGeneration;
            activeContext = desiredContext;
        }
        Thread t = new Thread(new Runnable() {
            public void run() {
                if (!sleepInterruptibly(50)) return;
                synchronized (lifecycleLock) {
                    synchronized (lock) {
                        if (!active || generation != serviceChangeGeneration
                                || activeLifecycleGeneration != lifecycleGeneration
                                || lifecycleAppliedGeneration
                                    != activeLifecycleGeneration) return;
                    }
                    stopRetry();
                    try { MODULES[RGD_MODULE_INDEX].stop(); }
                    catch (Throwable x) { Log.w(TAG, "rgd rebind stop: " + x); }
                    synchronized (lock) { started[RGD_MODULE_INDEX] = false; }
                    Log.i(TAG, "Navigation service changed - rebinding RGD");
                    if (!startPass(activeLifecycleGeneration)
                            && lifecycleCurrent(activeLifecycleGeneration, true,
                                                activeContext)) {
                        startRetry(activeLifecycleGeneration);
                    }
                }
            }
        }, "carplay-rgd-rebind");
        t.setDaemon(true);
        t.start();
    }

    /* start every not-yet-started module; return true when all are started. */
    private static boolean startPass(int expectedLifecycleGeneration) {
        FrameworkRef fw;
        synchronized (lock) {
            if (!active || expectedLifecycleGeneration != lifecycleGeneration
                    || fwRef == null || !fwRef.isReady()) {
                return false;
            }
            fw = fwRef;
        }

        /* Every caller holds lifecycleLock, which serializes module start/stop and
         * keeps active/fwRef stable for this pass.  Never hold the smaller state
         * lock across Module.start(): ScreenModule/RgdModule enter the static
         * ScreenNavStatusGate monitor, while NavigationJobs enters that monitor
         * before calling onNavigationServiceChanged() (which takes this lock).
         * Holding both in the old order made cold boot an ABBA deadlock. */
        boolean allUp = true;
        for (int i = 0; i < MODULES.length; i++) {
            synchronized (lock) {
                if (!active || expectedLifecycleGeneration != lifecycleGeneration
                        || fwRef != fw) return false;
                if (started[i]) continue;
            }
            boolean ok;
            try { ok = MODULES[i].start(fw); }
            catch (Throwable t) { Log.w(TAG, MODULES[i].name() + " start threw: " + t); ok = false; }
            boolean current;
            synchronized (lock) {
                current = active && expectedLifecycleGeneration == lifecycleGeneration
                    && fwRef == fw;
                started[i] = current && ok;
            }
            if (!current) {
                /* A disconnect/reconnect was published while external start()
                 * ran.  Undo even a false-returning partial start here; the
                 * worker will apply the newer generation next. */
                try { MODULES[i].stop(); }
                catch (Throwable t) { Log.w(TAG, MODULES[i].name() + " stale-start stop: " + t); }
                return false;
            }
            if (ok) {
                Log.i(TAG, "started " + MODULES[i].name());
            } else {
                /* MODULES is an ordered dependency chain: RGI must not consume/replay a
                 * snapshot before the cluster module has reset navActive and taken cluster
                 * ownership (stock ctx 74, ready to switch to ctx 80 on the first maneuver). */
                allUp = false;
                break;
            }
        }
        if (allUp) Log.i(TAG, "all modules started (" + MODULES.length + ")");
        return allUp;
    }

    private static void startRetry(final int expectedLifecycleGeneration) {
        stopRetry();
        final int generation;
        synchronized (lock) { generation = ++retryGeneration; }
        Thread t = new Thread(new Runnable() {
            public void run() {
                int n = 0;
                try {
                    while (true) {
                        /* Fast convergence during normal startup, then a low-rate indefinite wait
                         * for late Navigation/CombiBAP publication. */
                        long delay = n < 50 ? 200L : 2000L;
                        if (!sleepInterruptibly(delay)) return;
                        synchronized (lifecycleLock) {
                            synchronized (lock) {
                                if (!active || generation != retryGeneration
                                        || expectedLifecycleGeneration != lifecycleGeneration
                                        || retryThread != Thread.currentThread()) return;
                            }
                            if (startPass(expectedLifecycleGeneration)) return;
                        }
                        n++;
                        if (n == 50) Log.w(TAG, "modules still pending after 10 s; continuing low-rate retry");
                    }
                } finally {
                    synchronized (lock) {
                        if (retryThread == Thread.currentThread()) retryThread = null;
                    }
                }
            }
        }, "carplay-retry");
        t.setDaemon(true);
        synchronized (lock) { retryThread = t; }
        t.start();
    }

    private static void stopRetry() {
        Thread t;
        synchronized (lock) { retryGeneration++; t = retryThread; retryThread = null; }
        if (t != null) t.interrupt();
    }

    private static boolean sleepInterruptibly(long ms) {
        try { Thread.sleep(ms); return true; }
        catch (InterruptedException e) { Thread.currentThread().interrupt(); return false; }
    }
}
