/*
 * AltScreenModule — cluster (LVDS2 / terminal 1) CONTEXT MANAGER.
 *
 * ONE custom context: dc[80] = {98,102,101,99} — maneuver popup(98) over the CarPlay cluster
 * video(99), both over the KDK bg (101/102).  On CarPlay connect (start) the cluster switches to
 * it; on disconnect (stop) it restores the stock cluster (74).  That is the whole policy — the
 * context is binary (connected → ours, else stock).
 *
 * All sub-state lives in the renderers, NOT here: both are always-on framework services
 * (servicemgr external), so windows 98/99 always exist.  maneuver_render(98) draws transparent
 * when not navigating (and is blanked via CMD_CLEAR on route/CarPlay stop); altscreen_render(99)
 * streams black when no 111 video.  So a single held context carries navigating / not-navigating /
 * video / no-video without any context churn — nothing here reacts to RGI or video presence.
 *
 * The context table itself (dc[80]) is declared to the native compositor at init via
 * DisplayManager.declareContexts(dc); getMappedInternalContext is identity on MIB2High, so
 * switchContext(80) lands on exactly that declared context.
 *
 * There is exactly ONE persistent worker for the module lifetime and it is the SOLE caller of
 * DisplayManager.switchContext/setUpdateRate.  Single writer => two switches can never race the
 * bounce+settle, and no stale per-session worker can exist.  start()/stop() only publish the
 * desired context; the worker converges the cluster to it.
 */
package com.luka.carplay.core;

import com.luka.carplay.framework.Log;

import de.audi.atip.hmi.view.IDisplayManager;

public final class AltScreenModule implements Module {

    private static final String TAG = "AltScreen";

    public static final int TERMINAL_CLUSTER  = 1;    /* LVDS2 */
    /* The single CarPlay cluster context: dc[80] = {98 popup, 102/101 KDK bg, 99 CarPlay video}.
     * If on-unit displayable 99 fails to composite, flip this to a video-less sibling as a
     * calibration knob (e.g. a dc[] slot that carries {98,102,101,33} over the stock map). */
    public static final int CTX_CLUSTER       = 80;   /* nav active: {98 maneuver, 103 backing, 99 map} */
    public static final int CTX_CLUSTER_MAP   = 81;   /* no nav: {99 map} only (no backing box, no maneuver) */
    public static final int CTX_STOCK_CLUSTER = 74;
    private static final int CTX_BOUNCE       = 72;   /* kombi map — never ours; forces a real ctx change */
    private static final int BOUNCE_SLEEP_MS  = 180;  /* preContextSwitchHook settle (proven driver) */
    private static final int CLUSTER_FPS      = 30;   /* cluster encoder rate; MOST/encoder may cap below this */

    private static final Object LOCK = new Object();
    private IDisplayManager dm;                  /* current DisplayManager (guarded by LOCK); stable across sessions */
    private Thread worker;                        /* the ONE persistent switch worker (guarded by LOCK) */

    /* true while the cluster is on OUR context (set AFTER the physical switch completes) — the roller
     * injection reads this, because HID must wait until our content is actually shown. */
    private static volatile boolean clusterActive = false;
    public static boolean isClusterActive() { return clusterActive; }

    /* true from start() (BEFORE any switch) until stop() — i.e. we intend to own the cluster this whole
     * session.  The switchToTargetContext View-pin reads THIS, not clusterActive: pinning on the
     * applied state leaves a ~180ms window during the first switch where a View press could steal
     * terminal 1 to the stock map (worker then thinks it still owns ctx → stuck).  Intent-based pin
     * closes that window from t0. */
    public static boolean isConnected() { return connected; }

    /* desiredCtx = target published by start()/stop(); currentCtx = what the worker last applied.
     * Both guarded by LOCK; the single worker switches whenever they differ, converging to desiredCtx. */
    private static int desiredCtx = CTX_STOCK_CLUSTER;
    private static int currentCtx = -1;
    /* desiredCtx is a pure function of these two (guarded by LOCK):
     *   !connected           -> 74 (stock)
     *   connected, no nav    -> 81 (map only)
     *   connected, nav       -> 80 (map + backing + maneuver) */
    private static boolean connected = false;
    private static boolean navActive = false;

    /** Recompute desiredCtx from connected/navActive and wake the worker. Caller must NOT hold LOCK. */
    private static void republish() {
        synchronized (LOCK) {
            desiredCtx = !connected ? CTX_STOCK_CLUSTER : (navActive ? CTX_CLUSTER : CTX_CLUSTER_MAP);
            LOCK.notifyAll();
        }
    }

    /** Authoritative nav-active state: driven by RouteGuidance.rgActive (the moment we tell VC via
     *  BAP that navigation is active), NOT by the stock KDK-visible hint.  a maneuver present ->
     *  ctx 80, gone -> ctx 81 (while connected). */
    public static void setNavActive(boolean active) {
        synchronized (LOCK) {
            if (navActive == active) return;
            navActive = active;
        }
        republish();
    }

    /** The cluster-layer visibility gate read by CombiMapController — same truth as the BAP nav-active
     *  message, so the CarPlay layers (98/101/102) show ONLY while our RGI is active, never on a stray
     *  stock KDK-visible bit. */
    public static boolean isNavActive() { return navActive; }

    public String name() { return "altscreen"; }

    public boolean start(FrameworkRef fw) {
        if (fw == null || !fw.isReady() || fw.framework() == null) return false;

        IDisplayManager d = null;
        try {
            if (fw.framework().getHMIService() != null) {
                d = fw.framework().getHMIService().getDisplayManager();
            }
        } catch (Throwable t) {
            Log.d(TAG, "getDisplayManager not ready: " + t);
        }
        if (d == null) return false;                 /* DM not up yet → retry */

        synchronized (LOCK) {
            /* A replaced DisplayManager invalidates the cached currentCtx — reset so the worker
             * re-applies the desired ctx to the new one.  (In practice the same object each session.) */
            if (dm != d) { dm = d; currentCtx = -1; }
            /* Create the single persistent worker once; recreate only if it never started or died.
             * Assign the field ONLY after start() succeeds so a throw leaves worker==null for retry. */
            if (worker == null || !worker.isAlive()) {
                Thread w = new Thread(new Runnable() { public void run() { switchLoop(); } }, "carplay-cluster-switch");
                w.setDaemon(true);
                w.start();
                worker = w;
            }
        }
        Log.i(TAG, "ready (DisplayManager acquired)");
        synchronized (LOCK) { connected = true; }    /* CarPlay connected → take the cluster */
        republish();                                 /* -> ctx 81 (map) or 80 (map+maneuver) per navActive */
        return true;
    }

    public void stop() {
        /* Disconnect: publish stock (74); the single persistent worker restores it.  We deliberately
         * do NOT kill the worker or null dm — the worker being the sole always-live DM writer is what
         * makes stale-worker races impossible (no per-session worker to outlive its session). */
        synchronized (LOCK) { connected = false; }
        republish();
    }

    /** Publish the desired context and wake the worker (the single serialized DM writer converges to it). */
    private static void setDesired(int ctx) {
        synchronized (LOCK) { desiredCtx = ctx; LOCK.notifyAll(); }
    }

    /* ============================================================
     * Switch worker — the single serialized DM writer.
     * ============================================================ */

    private void switchLoop() {
        while (true) {
            int target; IDisplayManager d;
            synchronized (LOCK) {
                while (dm == null || desiredCtx == currentCtx) {
                    try { LOCK.wait(); } catch (InterruptedException e) { /* re-check predicate */ }
                }
                target = desiredCtx; d = dm;
            }
            applySwitch(target, d);
        }
    }

    /** Perform ONE context switch (bounce + settle + select).  Only this thread ever writes the DM,
     *  so there is no cross-worker race; the loop re-runs if desiredCtx changed during the settle.
     *  ponytail: a stop()/start() landing in the tiny window between the post-sleep recheck and the
     *  switchContext write can still cause ONE transient physical write before the next loop restores
     *  the newly-desired ctx — it is self-healing.  Closing it fully needs LOCK held across a DSI IPC
     *  call → deadlock risk, not worth it for a cosmetic transient on connect/disconnect. */
    private void applySwitch(int ctx, IDisplayManager d) {
        try {
            if (ctx != CTX_STOCK_CLUSTER) {
                /* BOUNCE: switchContext short-circuits (no preContextSwitchHook, no encoder re-point)
                 * when the target is already active.  Force a real change via a different ctx first. */
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
                        return;
                    }
                }
                d.switchContext(ctx, TERMINAL_CLUSTER, null);
                d.setUpdateRate(TERMINAL_CLUSTER, CLUSTER_FPS);   /* start MOST encoder @10fps */
                clusterActive = true;                             /* roller → CMD_ALT_HID */
            } else {
                d.setUpdateRate(TERMINAL_CLUSTER, 0);             /* stop encoder before leaving */
                d.switchContext(CTX_STOCK_CLUSTER, TERMINAL_CLUSTER, null);
                clusterActive = false;
            }
            synchronized (LOCK) { if (dm == d) currentCtx = ctx; }
            Log.i(TAG, "cluster -> ctx " + ctx + " (active=" + clusterActive + ")");
        } catch (Throwable t) {
            Log.w(TAG, "switch(" + ctx + ") failed: " + t);
            synchronized (LOCK) { currentCtx = -1; }
            /* Throttle the retry: the bounce write and the stock path have no settle sleep, so a
             * persistently-throwing switchContext would otherwise hot-spin (busy loop + log flood). */
            try { Thread.sleep(BOUNCE_SLEEP_MS); } catch (InterruptedException ie) { Thread.currentThread().interrupt(); }
        }
    }
}
