/*
 * ScreenModule — instrument-cluster (LVDS2 / terminal 1) CONTEXT MANAGER.
 *
 * Owns the CarPlay cluster context and selects between exactly two contexts:
 *
 *   dc[80] = {98 maneuver, 101/102 KDK backing, 33 stock native map}   — nav active
 *   dc[74] = stock cluster                                             — otherwise
 *
 * The maneuver overlay (displayable 98, maneuver_render, transparent when idle)
 * composites over the head unit's OWN native map (displayable 33); there is no
 * CarPlay video plane on the cluster.  Every new CarPlay session leaves the cluster
 * on stock (74); we switch to ctx 80 once RouteGuidance has started the RGI
 * presentation through BAP (setNavActive(true)), and drop back to 74 once VC withdraws KDK visibility (Fct44)
 * after guidance ends, and on disconnect.
 *
 * The context tables (dc[80]) are declared to the native compositor at init in
 * DisplayManagerMIB2High; getMappedInternalContext is identity on MIB2High, so
 * switchContext(80) lands on exactly that declared context.
 *
 * There is exactly ONE persistent worker for the module lifetime and it is the SOLE
 * caller of DisplayManager.switchContext/setUpdateRate.  Single writer => two
 * switches can never race the bounce+settle, and no stale per-session worker can
 * exist.  start()/stop() only publish the desired context; the worker converges the
 * cluster to it.
 */
package com.luka.carplay.core;

import com.luka.carplay.framework.Log;

import de.audi.atip.hmi.view.IDisplayManager;
import de.audi.tghu.fwhmi.IDisplayManagerKombiControl;

public final class ScreenModule implements Module {

    private static final String TAG = "Screen";

    public static final int TERMINAL_CLUSTER  = 1;    /* LVDS2 */
    public static final int CTX_CLUSTER       = 80;   /* nav active: {98 maneuver, 101/102 backing, 33 stock map} */
    public static final int CTX_STOCK_CLUSTER = 74;
    private static final int CTX_BOUNCE       = 72;   /* kombi map — never ours; forces a real ctx change */
    private static final int BOUNCE_SLEEP_MS  = 180;  /* preContextSwitchHook settle (proven driver) */
    private static final long CONTEXT_RECONCILE_MS = 250L;
    private static final int CLUSTER_FPS      = 30;   /* cluster encoder rate; MOST/encoder may cap below this */
    private static final int KOMBI_TYPE_G24   = 4;

    private static final Object LOCK = new Object();
    private IDisplayManager dm;                  /* current DisplayManager (guarded by LOCK); stable across sessions */
    private Thread worker;                        /* the ONE persistent switch worker (guarded by LOCK) */
    private static volatile Thread contextWriterThread;

    /* true while the cluster is on OUR context (set AFTER the physical switch completes). */
    private static volatile boolean clusterActive = false;
    private static volatile boolean platformSupported = true;
    private boolean enabled;

    /**
     * True from start() (BEFORE any switch) until stop() — i.e. this returns our *intent* to own the
     * cluster for the whole session, not the applied state.  CombiMapController's View-pin reads THIS:
     * pinning on the applied state leaves a ~180ms window during the first switch where a View press
     * could steal terminal 1 to the stock map (worker then thinks it still owns ctx → stuck).
     * Intent-based pin closes that window from t0. */
    public static boolean isConnected() { return platformSupported && connected; }

    /** DisplayManagerMIB2High uses this to distinguish our serialized 72/80/74
     * writes from stock screen-controller requests while CarPlay owns terminal 1. */
    public static boolean isClusterContextWriterThread() {
        return Thread.currentThread() == contextWriterThread;
    }

    static boolean isPlatformSupported(FrameworkRef fw) {
        try { return fw != null && fw.framework() != null
            && fw.framework().getKombiType() != KOMBI_TYPE_G24; }
        catch (Throwable t) { return true; }  /* only an explicit G24 value disables the feature */
    }

    /* desiredCtx = target published by start()/stop()/setNavActive(); currentCtx = what the worker last
     * applied.  Both guarded by LOCK; the single worker switches whenever they differ.
     * desiredCtx is a pure function of these two (guarded by LOCK):
     *   !connected         -> 74 (stock)
     *   connected, no nav  -> 74 (stock native map, no maneuver overlay)
     *   connected, nav     -> 80 (stock native map + backing + maneuver) */
    private static int desiredCtx = CTX_STOCK_CLUSTER;
    private static int currentCtx = -1;
    private static volatile boolean connected = false;
    private static volatile boolean navActive = false;
    private static boolean navHidePending;

    /* SQ5 AA port: Android Auto cockpit mirror (displayable 99, native sq5_mirror).
     * Contexts declared at boot in DisplayManagerMIB2High: 81 = {98,101,102,99} (arrow + backing
     * over the mirror), 82 = {99} (mirror alone, the shape of stock 72 {33}). */
    public static final int CTX_MIRROR_NAV  = 81;
    public static final int CTX_MIRROR_ONLY = 82;
    private static boolean mirrorReady;       /* guarded by LOCK; written by the worker only */

    /**
     * SQ5 AA port: the one context rule (pure, host-tested).
     *   no phone session                 -> 74 stock
     *   mirror ready, route guidance     -> 81 {98 arrow, 101/102 backing, 99 mirror}
     *   mirror ready, no guidance        -> 82 {99}
     *   mirror not ready, guidance       -> 80 luka {98,101,102,33}
     *   mirror not ready, no guidance    -> 74 stock
     * Not gated on the VC's lvdsMapVisible (map view): mirror_context_report 3.4 - in box view the
     * box just shows a slice of the mirror (81: covered by arrow+backing), and there is no car
     * evidence yet that the VC reports map view while Android Auto is active; it is logged instead.
     */
    public static int contextFor(boolean isConnected, boolean isNavActive, boolean isMirrorReady) {
        return contextFor(isConnected, isNavActive, isMirrorReady, false);
    }

    /**
     * SQ5 AA port, turn card (com.sq5.aa.luka.TurnCard): with holdNavContext the mirror stays on 81
     * whether or not a route is active.  81 without a route composes exactly like 82 (98/101/102 are
     * at opacity 0 while !navActive), but route start/stop then no longer switch context: no 72
     * bounce (180 ms of the Audi map 33 in the cockpit) and no window in which the VC opens its KDK
     * tile while ctx 82 = {99} is still selected and shows the Google Maps mirror in the tile (owner
     * drive 2026-09-28).  Route start/stop become plane opacity changes in ClusterLayerController.
     * SD:/sq5_turncard_off -> holdNavContext false -> the legacy 81/82 split above.
     */
    public static int contextFor(boolean isConnected, boolean isNavActive, boolean isMirrorReady,
                                 boolean holdNavContext) {
        if (!isConnected) return CTX_STOCK_CLUSTER;
        if (isMirrorReady) return isNavActive || holdNavContext ? CTX_MIRROR_NAV : CTX_MIRROR_ONLY;
        return isNavActive ? CTX_CLUSTER : CTX_STOCK_CLUSTER;
    }

    /** Caller holds LOCK. */
    private static void recomputeLocked(String why) {
        int next = contextFor(connected, navActive, mirrorReady, com.sq5.aa.luka.TurnCard.isEnabled());
        if (next != desiredCtx) {
            com.sq5.aa.luka.AaLog.log("screen: desired ctx " + desiredCtx + " -> " + next + " (" + why
                + ": session=" + connected + " nav=" + navActive + " mirror=" + mirrorReady + ")");
        }
        desiredCtx = next;
        LOCK.notifyAll();
    }

    /** True while terminal 1 physically carries a mirror context (81/82): plane 99 must be opaque. */
    public static boolean isMirrorActive() {
        synchronized (LOCK) {
            return connected && (currentCtx == CTX_MIRROR_NAV || currentCtx == CTX_MIRROR_ONLY);
        }
    }

    /** Recompute desiredCtx from connected/navActive/mirror and wake the worker. Caller must NOT hold LOCK. */
    private static void republish() {
        synchronized (LOCK) {
            recomputeLocked("state");
        }
        /* SQ5 AA port (turn card): with ctx 81 held across route edges no context switch follows a
         * navActive change, so the planes must be re-evaluated here (a switch still reapplies too). */
        com.luka.carplay.cluster.ClusterLayerController.reapply();
    }

    /** Presentation latch, not merely route intent.  RouteGuidance may set true only after the
     *  BAP presentation has started (bap.onStart()); the renderer's FRAME_READY is not waited for.
     *  Navigation owns the context; VC alone controls KDK opacity.  On route end, retain the
     *  composition until VC withdraws visibility (Fct44), without a guessed timer. */
    public static void setNavActive(boolean active) {
        synchronized (LOCK) {
            navHidePending = !active && navActive
                && com.luka.carplay.cluster.ClusterLayerController.isKdkVisible();
            navActive = active || navHidePending;
        }
        republish();
    }

    /** Called after the layer controller has applied the received Fct44 visibility.
     *  A View fade-out must not release context while the route remains active. */
    public static void onVcKdkVisibility(boolean visible) {
        boolean release = false;
        synchronized (LOCK) {
            if (!visible && navHidePending) {
                navHidePending = false;
                navActive = false;
                release = true;
            }
        }
        if (release) republish();
    }

    /** The cluster-layer visibility gate read by CombiMapController.  It follows the confirmed BAP
     *  presentation and, after route end, VC's own KDK withdrawal; never a stray stock KDK bit. */
    public static boolean isNavActive() { return navActive; }

    /* ------------------------------------------------------------
     * Cluster map view size (Audi View button / NAV_VIEW_SIZE_CHOICE).
     * There is no CarPlay video to resize on this branch; the flag drives only LOCAL geometry —
     * which KDK stage (popup/in-tube) and which native map plane (33/58) the maneuver overlay must
     * follow.  No hook command is sent.
     * ------------------------------------------------------------ */
    public static final int VIEWAREA_FULLSCREEN  = 0;
    public static final int VIEWAREA_SMALLSCREEN = 1;
    private static volatile boolean smallScreenViewArea = false;
    private static volatile ViewAreaModeListener viewAreaModeListener;

    /** Lightweight notification for consumers whose cluster presentation differs by view area. */
    public interface ViewAreaModeListener {
        void onViewAreaModeChanged(int mode);
    }

    public static boolean isSmallScreenViewArea() { return smallScreenViewArea; }

    public static void setViewAreaModeListener(ViewAreaModeListener listener) {
        viewAreaModeListener = listener;
    }

    public static void clearViewAreaModeListener(ViewAreaModeListener listener) {
        if (viewAreaModeListener == listener) viewAreaModeListener = null;
    }

    /** Called by the stock NAV_VIEW_SIZE_CHOICE model: value 0=fullscreen, value 1=smallscreen. */
    public static void setViewAreaMode(int mode) {
        boolean small = (mode == VIEWAREA_SMALLSCREEN);
        if (smallScreenViewArea == small) return;
        smallScreenViewArea = small;
        ViewAreaModeListener listener = viewAreaModeListener;
        if (listener != null) {
            try { listener.onViewAreaModeChanged(small ? VIEWAREA_SMALLSCREEN : VIEWAREA_FULLSCREEN); }
            catch (Throwable t) { Log.w(TAG, "viewArea listener failed: " + t); }
        }
    }

    /* ------------------------------------------------------------
     * Route-info toggle (steering-wheel OK press).
     * Flips the cluster route-info text line between the next turn-to street and the
     * trip summary (ETA / arrival clock + remaining).  Driven by SteeringWheelInputModule.
     * ------------------------------------------------------------ */

    /** Route-info toggle seam driven by the raw MFW left-roller press listener. */
    public interface InfoModeListener {
        void onInfoModeToggle();
    }
    private static volatile InfoModeListener infoModeListener;

    public static void setInfoModeListener(InfoModeListener listener) {
        infoModeListener = listener;
    }

    public static void clearInfoModeListener(InfoModeListener listener) {
        if (infoModeListener == listener) infoModeListener = null;
    }

    /** Raw DSI key 40 (left steering-wheel roller press).  SteeringWheelInputModule already gates
     *  this callback to the confirmed VC map tab; the toggle is meaningful only with active RGI. */
    public static void onSteeringWheelOkPressed() {
        if (!isConnected() || !isNavActive()) return;
        InfoModeListener listener = infoModeListener;
        if (listener == null) return;
        listener.onInfoModeToggle();
    }

    public String name() { return "screen"; }

    /* SQ5 AA port: config switch for "BAP/HUD only" operation (no maneuver_render deployed).
     * false = never take terminal 1: the module reports started but stays disabled, so
     * isConnected() is false and every stock cluster context path runs unchanged. */
    private static volatile boolean clusterContextEnabled = true;
    public static void setClusterContextEnabled(boolean enabled) { clusterContextEnabled = enabled; }

    public boolean start(FrameworkRef fw) {
        if (fw == null || !fw.isReady() || fw.framework() == null) return false;
        if (!clusterContextEnabled) {
            enabled = false;
            Log.w(TAG, "disabled by configuration: stock cluster contexts, BAP/HUD guidance only");
            return true;
        }
        if (!isPlatformSupported(fw)) {
            platformSupported = false;
            enabled = false;
            Log.w(TAG, "disabled: G24 cluster has no ctx 80 maneuver composition");
            return true;
        }
        platformSupported = true;
        enabled = true;

        IDisplayManager d = null;
        try {
            if (fw.framework().getHMIService() != null) {
                d = fw.framework().getHMIService().getDisplayManager();
            }
        } catch (Throwable t) {
        }
        if (d == null) return false;                 /* DM not up yet → retry */
        if (d instanceof IDisplayManagerKombiControl) {
            com.luka.carplay.cluster.ClusterLayerController.bind(
                (IDisplayManagerKombiControl)d, TERMINAL_CLUSTER);
        }

        /* Publish the new session target before a new worker can observe currentCtx=-1. */
        synchronized (LOCK) {
            /* A replaced DisplayManager invalidates the cached currentCtx — reset so the worker
             * re-applies the desired ctx to the new one.  (In practice the same object each session.) */
            if (dm != d) { dm = d; currentCtx = -1; }
            connected = true;
            navActive = false;
            navHidePending = false;
            mirrorReady = false;
            desiredCtx = CTX_STOCK_CLUSTER;
        }
        com.sq5.aa.luka.MirrorGate.reset();
        synchronized (LOCK) {
            /* Create the single persistent worker once; recreate only if it never started or died.
             * Assign the field ONLY after start() succeeds so a throw leaves worker==null for retry. */
            if (worker == null || !worker.isAlive()) {
                Thread w = new Thread(new Runnable() { public void run() { switchLoop(); } }, "carplay-cluster-switch");
                w.setDaemon(true);
                w.start();
                worker = w;
            }
            LOCK.notifyAll();
        }
        Log.i(TAG, "ready (DisplayManager acquired, session reset -> ctx 74)");
        com.sq5.aa.luka.AaLog.log("screen: module started, owns terminal " + TERMINAL_CLUSTER
            + " (stock cluster switches blocked from now on)");
        return true;
    }

    public void stop() {
        if (!enabled) return;
        enabled = false;
        /* Disconnect: publish stock (74); the single persistent worker restores it.  We deliberately
         * do NOT kill the worker or null dm — the worker being the sole always-live DM writer is what
         * makes stale-worker races impossible (no per-session worker to outlive its session). */
        synchronized (LOCK) {
            connected = false;
            navActive = false;
            navHidePending = false;
            mirrorReady = false;
        }
        com.sq5.aa.luka.AaLog.log("screen: module stopped, terminal " + TERMINAL_CLUSTER + " back to stock");
        republish();
    }

    /* ============================================================
     * Switch worker — the single serialized DM writer.
     * ============================================================ */

    private void switchLoop() {
        contextWriterThread = Thread.currentThread();
        while (true) {
            int target; IDisplayManager d; boolean reconcileOnly = false;
            /* SQ5 AA port: poll the mirror marker (file I/O) outside LOCK, every wake-up / 250 ms
             * while a session owns terminal 1; a stale marker falls back to 80/74 on this pass. */
            boolean session = connected;
            boolean panelBefore = com.sq5.aa.luka.MirrorGate.drawsCardPanel();
            boolean mirror = session && com.sq5.aa.luka.MirrorGate.poll();
            /* turn card: the card-mode backing follows the mirror's "tc1" (panel) capability */
            if (panelBefore != com.sq5.aa.luka.MirrorGate.drawsCardPanel())
                com.luka.carplay.cluster.ClusterLayerController.reapply();
            synchronized (LOCK) {
                if (connected == session && mirror != mirrorReady) {
                    mirrorReady = mirror;
                    recomputeLocked(mirror ? "mirror ready" : "mirror not ready");
                }
                while (dm == null) {
                    try { LOCK.wait(); } catch (InterruptedException e) { /* persistent worker */ }
                }
                if (desiredCtx == currentCtx) {
                    try {
                        if (desiredCtx >= CTX_CLUSTER || connected)
                            LOCK.wait(CONTEXT_RECONCILE_MS);
                        else
                            LOCK.wait();
                    } catch (InterruptedException e) { /* persistent worker */ }
                    if (dm == null || desiredCtx != currentCtx) continue;
                    if (desiredCtx < CTX_CLUSTER) continue;
                    reconcileOnly = true;
                }
                target = desiredCtx; d = dm;
            }
            if (reconcileOnly) {
                int actual;
                try { actual = d.getCurrentContextID(TERMINAL_CLUSTER); }
                catch (Throwable t) {
                    Log.w(TAG, "context reconcile read failed: " + t);
                    continue;
                }
                if (actual != target) {
                    boolean retry = false;
                    synchronized (LOCK) {
                        if (dm == d && desiredCtx == target && currentCtx == target) {
                            currentCtx = -1;
                            retry = true;
                            LOCK.notifyAll();
                        }
                    }
                    if (retry) {
                        Log.w(TAG, "physical context drift actual=" + actual
                            + " desired=" + target + " -> reconcile");
                        com.sq5.aa.luka.AaLog.log("screen: physical context drift actual=" + actual
                            + " desired=" + target + " -> reconcile");
                    }
                }
                continue;
            }
            applySwitch(target, d);
        }
    }

    /** SQ5 AA port: diagnostics only (the Java DM's view of terminal 1). */
    private static int safeCurrentContext(IDisplayManager d) {
        try { return d.getCurrentContextID(TERMINAL_CLUSTER); } catch (Throwable t) { return -2; }
    }

    /** Perform ONE context switch (bounce + settle + select).  Only this thread ever writes the DM,
     *  so there is no cross-worker race; the loop re-runs if desiredCtx changed during the settle.
     *  Note: a stop()/start() landing in the tiny window between the post-sleep recheck and the
     *  switchContext write can still cause ONE transient physical write before the next loop restores
     *  the newly-desired ctx — it is self-healing.  Closing it fully needs LOCK held across a DSI IPC
     *  call → deadlock risk, not worth it for a cosmetic transient on connect/disconnect. */
    private void applySwitch(int ctx, IDisplayManager d) {
        try {
            if (ctx != CTX_STOCK_CLUSTER) {
                /* Coming from stock (74) or an unknown state (-1): the MOST encoder is off, so the grab
                 * of the cluster needs a real context change via a throwaway ctx (72) + settle before
                 * switchContext(80) will re-point the encoder. */
                int bounce = (ctx != CTX_BOUNCE) ? CTX_BOUNCE : CTX_STOCK_CLUSTER;
                d.switchContext(bounce, TERMINAL_CLUSTER, null);
                try { Thread.sleep(BOUNCE_SLEEP_MS); }
                catch (InterruptedException ie) { Thread.currentThread().interrupt(); }
                /* Coalesce: if the desired target or the DM changed during the settle, abandon THIS
                 * switch (cluster is on the bounce ctx) and let the loop apply the latest desired. */
                synchronized (LOCK) {
                    if (dm != d || desiredCtx != ctx) {
                        currentCtx = -1;
                        Log.i(TAG, "switch(" + ctx + ") superseded during bounce → " + desiredCtx);
                        com.sq5.aa.luka.AaLog.log("screen: switch(" + ctx + ") superseded during bounce -> " + desiredCtx);
                        return;
                    }
                }
                d.switchContext(ctx, TERMINAL_CLUSTER, null);
                d.setUpdateRate(TERMINAL_CLUSTER, CLUSTER_FPS);   /* (idempotent when already running) */
                clusterActive = true;
            } else {
                /* Preserve the stop-before-switch ordering, but never leave terminal 1
                 * parked at 0 FPS. On this A5/MHI2Q the stock
                 * KOMBI_KDK_VIA_DISPLAYABLES branch bypasses CombiMapController's
                 * optional 10/1/0 updateFrameRate() path, so 10 is not an authoritative
                 * restore value here. Return the terminal to the same full 30 Hz rate
                 * used by the live cluster encoder; stock may change it later if it has
                 * an applicable producer. */
                d.setUpdateRate(TERMINAL_CLUSTER, 0);
                try {
                    d.switchContext(CTX_STOCK_CLUSTER, TERMINAL_CLUSTER, null);
                } finally {
                    d.setUpdateRate(TERMINAL_CLUSTER, CLUSTER_FPS);
                }
                clusterActive = false;
            }
            synchronized (LOCK) { if (dm == d) currentCtx = ctx; }
            /* SQ5 AA port: the VC map-view gate follows the physically selected context (81/82 = mirror). */
            com.sq5.aa.luka.VcMapViewGate.onMirrorActive(isMirrorActive());
            /* Context composition is now final: replay the last KDK popup geometry so a
             * navActive edge cannot leave planes 98/101/102 at their previous opacity. */
            com.luka.carplay.cluster.ClusterLayerController.reapply();
            Log.i(TAG, "cluster -> ctx " + ctx + " (active=" + clusterActive + ")");
            com.sq5.aa.luka.AaLog.log("screen: cluster -> ctx " + ctx
                + (ctx != CTX_STOCK_CLUSTER ? " (bounce " + CTX_BOUNCE + ", " + BOUNCE_SLEEP_MS + " ms, rate "
                    + CLUSTER_FPS + ")" : " (rate 0 -> switch -> " + CLUSTER_FPS + ")")
                + " physical=" + safeCurrentContext(d));
        } catch (Throwable t) {
            Log.w(TAG, "switch(" + ctx + ") failed: " + t);
            com.sq5.aa.luka.AaLog.log("screen: switch(" + ctx + ") failed: " + t);
            synchronized (LOCK) { currentCtx = -1; }
            /* Throttle the retry: the bounce write and the stock path have no settle sleep, so a
             * persistently-throwing switchContext would otherwise hot-spin (busy loop + log flood). */
            try { Thread.sleep(BOUNCE_SLEEP_MS); } catch (InterruptedException ie) { Thread.currentThread().interrupt(); }
        }
    }
}
