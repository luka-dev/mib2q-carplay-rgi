/*
 * CarPlayApp — entry point + module lifecycle.
 *
 * Called from the stock hook in TerminalModeBapCombi$ActiveDeviceStateListener:
 *   CarPlayApp.onActivate(ctx)   // CarPlay device connected  (ctx = IContext)
 *   CarPlayApp.onDeactivate()    // disconnected
 *
 * Starts the bus, then starts each registered Module.  A module may return false
 * from start() if a service it needs is not up yet; we retry the whole start pass
 * on a daemon thread until everything is up or the CarPlay generation ends.  Navigation can
 * appear much later than TerminalMode on a cold boot, so this must not have a fixed timeout.
 *
 * Java 1.2 (no generics/autoboxing).
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
     * input, altscreen). */
    private static final Module[] MODULES = new Module[] { new AltScreenModule(), new RgdModule() };

    private static final Object lock = new Object();
    private static final Object lifecycleLock = new Object();
    private static final boolean[] started = new boolean[MODULES.length];
    private static volatile boolean active = false;
    private static volatile FrameworkRef fwRef;
    private static Thread retryThread;
    private static int retryGeneration;
    private static int serviceChangeGeneration;
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
        AltscreenControlServer.start();              /* always-on :19832 — altscreen_render reports video on/off */
        if (context instanceof IContext) {
            synchronized (lock) {
                if (fwRef == null) fwRef = new FrameworkRef((IContext) context);
            }
        }
        Log.i(TAG, "transport up (bus always-on) build=" + BUILD_ID);
    }

    public static void onActivate(Object context) {
        Log.i(TAG, "onActivate build=" + BUILD_ID);
        CarplayBus.getInstance().start();            /* idempotent (already up via init) */

        if (!(context instanceof IContext)) {
            Log.e(TAG, "context is not IContext: " + context);
            return;
        }
        synchronized (lifecycleLock) {
            stopRetry();
            synchronized (lock) { active = false; serviceChangeGeneration++; }
            /* Stop every module, including one whose previous start returned false after partial
             * setup. Never call external module teardown while holding the shared state lock. */
            for (int i = MODULES.length - 1; i >= 0; i--) {
                try { MODULES[i].stop(); } catch (Throwable t) { Log.w(TAG, MODULES[i].name() + " stop: " + t); }
            }
            synchronized (lock) {
                for (int i = 0; i < MODULES.length; i++) started[i] = false;
                active = true;
                fwRef = new FrameworkRef((IContext) context);
            }
            if (!startPass()) startRetry();
        }
    }

    public static void onDeactivate() {
        Log.i(TAG, "onDeactivate");
        synchronized (lifecycleLock) {
            synchronized (lock) { active = false; serviceChangeGeneration++; }
            stopRetry();
            for (int i = MODULES.length - 1; i >= 0; i--) {
                try { MODULES[i].stop(); } catch (Throwable t) { Log.w(TAG, MODULES[i].name() + " stop: " + t); }
            }
            synchronized (lock) {
                for (int i = 0; i < MODULES.length; i++) started[i] = false;
            }
        }
    }

    public static FrameworkRef fw() { synchronized (lock) { return fwRef; } }

    /** Navigation's OSGi service can disappear/reappear without a CarPlay reconnect.  The RGD
     * module owns a paired ServiceReference, so rebuild only that module on the exact service
     * edge instead of continuing to call a stale AppConnectorNavi instance.  ClusterService
     * invokes this from NavigationJobs; the small worker keeps teardown/start outside that
     * dispatcher and coalesces rapid remove+add pairs. */
    public static void onNavigationServiceChanged() {
        final int generation;
        synchronized (lock) {
            if (!active) return;
            generation = ++serviceChangeGeneration;
        }
        Thread t = new Thread(new Runnable() {
            public void run() {
                if (!sleepInterruptibly(50)) return;
                synchronized (lifecycleLock) {
                    synchronized (lock) {
                        if (!active || generation != serviceChangeGeneration) return;
                    }
                    stopRetry();
                    try { MODULES[RGD_MODULE_INDEX].stop(); }
                    catch (Throwable x) { Log.w(TAG, "rgd rebind stop: " + x); }
                    synchronized (lock) { started[RGD_MODULE_INDEX] = false; }
                    Log.i(TAG, "Navigation service changed - rebinding RGD");
                    if (!startPass()) startRetry();
                }
            }
        }, "carplay-rgd-rebind");
        t.setDaemon(true);
        t.start();
    }

    /* start every not-yet-started module; return true when all are started. */
    private static boolean startPass() {
        synchronized (lock) {
            if (!active || fwRef == null || !fwRef.isReady()) {
                Log.d(TAG, "startPass: framework not ready");
                return false;
            }
            boolean allUp = true;
            for (int i = 0; i < MODULES.length; i++) {
                if (started[i]) continue;
                boolean ok;
                try { ok = MODULES[i].start(fwRef); }
                catch (Throwable t) { Log.w(TAG, MODULES[i].name() + " start threw: " + t); ok = false; }
                started[i] = ok;
                if (ok) {
                    Log.i(TAG, "started " + MODULES[i].name());
                } else {
                    /* MODULES is an ordered dependency chain: RGI must not consume/replay a
                     * snapshot before altScreen has reset navActive and physically owns ctx 81. */
                    allUp = false;
                    break;
                }
            }
            if (allUp) Log.i(TAG, "all modules started (" + MODULES.length + ")");
            return allUp;
        }
    }

    private static void startRetry() {
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
                                        || retryThread != Thread.currentThread()) return;
                            }
                            if (startPass()) return;
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
