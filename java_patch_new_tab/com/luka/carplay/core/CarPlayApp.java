/*
 * CarPlayApp — entry point + module lifecycle.
 *
 * Called from the stock hook in TerminalModeBapCombi$ActiveDeviceStateListener:
 *   CarPlayApp.onActivate(ctx)   // CarPlay device connected  (ctx = IContext)
 *   CarPlayApp.onDeactivate()    // disconnected
 *
 * Starts the bus, then starts each registered Module.  A module may return false
 * from start() if a service it needs is not up yet; we retry the whole start pass
 * on a short-lived daemon thread until everything is up or we time out (the proven
 * old-hook pattern — CombiBAPServiceNavi etc. appear a beat after activate).
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
    private static final boolean[] started = new boolean[MODULES.length];
    private static volatile boolean active = false;
    private static volatile FrameworkRef fwRef;
    private static Thread retryThread;

    private CarPlayApp() {}

    /** Current HMI framework access, or null before activate / after deactivate.
     *  Replaces the old CarPlayHook.getFrameworkAccess() static seam. */
    public static IFrameworkAccess framework() {
        FrameworkRef r = fwRef;
        return r != null ? r.framework() : null;
    }

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
        synchronized (lock) {
            /* Re-activate safety: stop any modules still running from a prior activate
             * before re-init, so a second ACTIVATING doesn't orphan the live BAP bridge. */
            for (int i = 0; i < MODULES.length; i++) {
                if (started[i]) {
                    try { MODULES[i].stop(); } catch (Throwable t) { Log.w(TAG, MODULES[i].name() + " stop: " + t); }
                }
                started[i] = false;
            }
            active = true;
            fwRef = new FrameworkRef((IContext) context);
        }
        if (!startPass()) startRetry();
    }

    public static void onDeactivate() {
        Log.i(TAG, "onDeactivate");
        synchronized (lock) { active = false; }
        stopRetry();
        synchronized (lock) {
            for (int i = 0; i < MODULES.length; i++) {
                try { MODULES[i].stop(); } catch (Throwable t) { Log.w(TAG, MODULES[i].name() + " stop: " + t); }
                started[i] = false;
            }
        }
    }

    public static FrameworkRef fw() { synchronized (lock) { return fwRef; } }

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
                if (ok) Log.i(TAG, "started " + MODULES[i].name());
                else    allUp = false;
            }
            if (allUp) Log.i(TAG, "all modules started (" + MODULES.length + ")");
            return allUp;
        }
    }

    private static void startRetry() {
        stopRetry();
        Thread t = new Thread(new Runnable() {
            public void run() {
                for (int n = 0; active && n < 50; n++) {       /* ~10 s @ 200 ms */
                    sleep(200);
                    if (startPass()) return;
                }
                if (active) Log.w(TAG, "some modules never came up (services missing)");
            }
        }, "carplay-retry");
        t.setDaemon(true);
        synchronized (lock) { retryThread = t; }
        t.start();
    }

    private static void stopRetry() {
        Thread t;
        synchronized (lock) { t = retryThread; retryThread = null; }
        if (t != null) t.interrupt();
    }

    private static void sleep(long ms) { try { Thread.sleep(ms); } catch (InterruptedException e) {} }
}
