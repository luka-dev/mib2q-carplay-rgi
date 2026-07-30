/*
 * AltScreenModule — cluster (LVDS2 / terminal 1) CONTEXT MANAGER.  Arbitrates the single shared
 * cluster context from three independent inputs and drives the HMI DisplayManager:
 *
 *   inputs:  rgiActive (RGD: maneuver popup wanted) · videoAvailable (altScreen 111 video present)
 *            · smartphone-tab focus (KombiSyncController)
 *   context: 74 stock
 *            80 popup(98) over stock map(33) fullscreen        — RGI, not on the tab
 *            82 popup(98) over CarPlay video(99) fullscreen    — on the tab, projecting + navigating
 *            83 CarPlay video(99) only (99 = active plane)     — on the tab, projecting, NOT navigating
 *
 * There is exactly ONE persistent worker thread for the whole module lifetime, and it is the SOLE
 * caller of DisplayManager.switchContext/setUpdateRate.  That is the design: with a single writer,
 * two workers can never race the bounce+180ms settle, and a stale worker from a previous session
 * cannot exist (we never create a per-session worker or tear one down).  start()/stop() only update
 * inputs and post the recomputed desired context; the worker converges the cluster to it.
 * (The name is legacy — the module manages the whole cluster context, not just the altScreen video.)
 */
package com.luka.carplay.core;

import com.luka.carplay.framework.Log;

import de.audi.atip.hmi.view.IDisplayManager;

public final class AltScreenModule implements Module {

    private static final String TAG = "AltScreen";

    public static final int TERMINAL_CLUSTER   = 1;   /* LVDS2 */
    public static final int CTX_MANEUVER       = 80;  /* popup(98) over stock map(33) */
    public static final int CTX_MANEUVER_VIDEO = 82;  /* popup(98) over CarPlay video(99) */
    public static final int CTX_VIDEO          = 83;  /* CarPlay video(99) only, no popup */
    public static final int CTX_STOCK_CLUSTER  = 74;
    private static final int CTX_BOUNCE        = 72;  /* kombi map — never one of ours; forces a ctx change */
    private static final int BOUNCE_SLEEP_MS   = 180; /* preContextSwitchHook settle (old proven driver) */
    private static final int CLUSTER_FPS       = 10;  /* full MOST encoder rate (CombiMapController hint&2) */

    /* Displayable ids owned by our renderers (see protocol.h CR_DISPLAYABLE_ID / altscreen). */
    private static final int DISP_MANEUVER     = 98;  /* maneuver_render — KDK popup graphics */
    private static final int DISP_VIDEO        = 99;  /* altscreen_render — CarPlay cluster video */

    /* Geometry: cluster panel is 1440x542 (LVDS2 visible res).  setPosition sets x,y only — size is
     * the renderer window's own: maneuver_render (98) is 328x181 and ALREADY rendered correctly at
     * ctx 80, altscreen_render (99) is fullscreen.  APPLY_GEOMETRY is OFF by default so we don't
     * disturb the working ctx-80 position; flip on only if the VC lays our ids out differently. */
    private static final boolean APPLY_GEOMETRY = false;
    private static final int POPUP_X = 1040, POPUP_Y = 40;   /* fallback only — tune on-unit if needed */

    private static final Object LOCK = new Object();
    private IDisplayManager dm;                 /* current DisplayManager (guarded by LOCK); stable across sessions */
    private KombiSyncController kombi;
    private Thread worker;                      /* the ONE persistent switch worker (guarded by LOCK) */

    /* true while the cluster is on one of OUR contexts (80/82/83) — the roller injection reads this. */
    private static volatile boolean clusterActive = false;
    public static boolean isClusterActive() { return clusterActive; }

    /* Independent inputs (see class doc). */
    private static volatile boolean rgiActive = false;
    private static volatile boolean videoAvailable = false;
    /* desiredCtx = target computed from the inputs; currentCtx = what the worker last applied.
     * Both guarded by LOCK; the single worker switches whenever they differ, converging to desiredCtx. */
    private static int desiredCtx = CTX_STOCK_CLUSTER;
    private static int currentCtx = -1;

    public String name() { return "altscreen"; }

    public boolean start(FrameworkRef fw) {
        if (fw == null || !fw.isReady() || fw.framework() == null) return false;

        /* Subscribe to DSIKombiSync FIRST, independent of the DisplayManager — tab-availability +
         * focus path.  Must not be gated behind the DM being ready. */
        if (kombi == null) {
            try { kombi = new KombiSyncController(fw.context()); kombi.init(); }
            catch (Throwable t) { Log.w(TAG, "KombiSync subscribe failed: " + t); kombi = null; }
        }

        IDisplayManager d = null;
        try {
            if (fw.framework().getHMIService() != null) {
                d = fw.framework().getHMIService().getDisplayManager();
            }
        } catch (Throwable t) {
            Log.d(TAG, "getDisplayManager not ready: " + t);
        }
        if (d == null) return false;                 /* DM not up yet → retry (kombisync already up) */

        synchronized (LOCK) {
            /* A replaced DisplayManager invalidates the cached currentCtx (it referred to the OLD
             * manager) — reset so the worker re-applies the desired ctx to the new one.  (In practice
             * the manager is the same object every session, but don't rely on it.) */
            if (dm != d) { dm = d; currentCtx = -1; }
            /* Create the single persistent worker once; recreate only if it never started (a throw
             * below left it null) or somehow died.  Assign the field ONLY after start() succeeds, so
             * a setDaemon/start throw leaves worker==null and the next retry recreates it. */
            if (worker == null || !worker.isAlive()) {
                Thread w = new Thread(new Runnable() { public void run() { switchLoop(); } }, "carplay-cluster-switch");
                w.setDaemon(true);
                w.start();
                worker = w;
            }
        }
        Log.i(TAG, "ready (DisplayManager acquired)");
        /* (Re)compute the desired context from current inputs — RGD may have set rgiActive before we
         * were ready; the worker then converges the cluster to it. */
        refreshClusterContext();
        return true;
    }

    public void stop() {
        /* Disconnect: drop our inputs so the desired context computes back to stock (74); the single
         * persistent worker restores it.  We deliberately do NOT kill the worker or null dm — the
         * worker being the sole, always-live DM writer is exactly what makes stale-worker races
         * impossible (no per-session worker to outlive its session). */
        rgiActive = false;
        videoAvailable = false;
        refreshClusterContext();                      /* → desiredCtx = 74; worker restores stock */
        if (kombi != null) { try { kombi.deinit(); } catch (Throwable t) { /* ignore */ } kombi = null; }
    }

    /* ============================================================
     * Inputs — set the signal, then recompute the desired context.
     * ============================================================ */

    /** RGD signal: CarPlay route guidance (maneuver popup content) active. */
    public static void setRgiActive(boolean on)      { rgiActive = on;      refreshClusterContext(); }

    /** altScreen signal: the CarPlay cluster video stream (111 -> altscreen_render, id 99) is
     *  producing frames.  Wired by the video pipeline (libairplay); false until that exists, so
     *  ctx 82/83 are never selected (we never switch the cluster to a black video plane). */
    public static void setVideoAvailable(boolean on) { videoAvailable = on; refreshClusterContext(); }

    /** Recompute the desired cluster context from (rgiActive, videoAvailable, tab focus) and post
     *  it to the worker.  Read + compute + publish ATOMICALLY under LOCK — computing outside the
     *  lock would let a slow refresh publish a stale target last.  Popup (RGI) and video (altScreen)
     *  are independent:
     *    focused + video + rgi  -> 82 popup(98) over CarPlay video(99)
     *    focused + video        -> 83 CarPlay video(99) only (99 is the active/encoded plane)
     *    rgi                    -> 80 popup(98) over stock map(33)
     *    else                   -> 74 stock */
    public static void refreshClusterContext() {
        synchronized (LOCK) {
            boolean focused = KombiSyncController.isSmartphoneTabFocused();
            int target;
            if (focused && videoAvailable) {
                target = rgiActive ? CTX_MANEUVER_VIDEO : CTX_VIDEO;   /* 82 : 83 */
            } else if (rgiActive) {
                target = CTX_MANEUVER;                                 /* 80 */
            } else {
                target = CTX_STOCK_CLUSTER;                            /* 74 */
            }
            desiredCtx = target;
            LOCK.notifyAll();
        }
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
     *  ponytail: a stop()/refresh landing in the tiny window between the post-sleep recheck and the
     *  switchContext write can still cause ONE transient physical write (a brief flash of our ctx or
     *  the 72 bounce) before the next loop restores the newly-desired ctx — it is self-healing.
     *  Fully closing it needs the LOCK held across switchContext (a DSI IPC call) → deadlock risk,
     *  not worth it for a cosmetic transient on connect/disconnect. */
    private void applySwitch(int ctx, IDisplayManager d) {
        try {
            if (ctx != CTX_STOCK_CLUSTER) {
                /* BOUNCE: switchContext short-circuits (no preContextSwitchHook, no encoder re-point
                 * onto displayables[0]) when the target is already active.  Force a real change via a
                 * different ctx first.  Plane selection is displayables[0] of the target (dc[83][0]=99). */
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
                applyGeometry(ctx, d);
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

    /** Position our displayables within the active context.  98 = popup rect, 99 = fullscreen bg.
     *  Gated by APPLY_GEOMETRY (off until measured on-unit — 98 already sits correctly at ctx 80). */
    private static void applyGeometry(int ctx, IDisplayManager d) {
        if (!APPLY_GEOMETRY) return;
        try {
            if (ctx == CTX_MANEUVER || ctx == CTX_MANEUVER_VIDEO) {
                d.setPosition(DISP_MANEUVER, TERMINAL_CLUSTER, POPUP_X, POPUP_Y);
            }
            if (ctx == CTX_MANEUVER_VIDEO || ctx == CTX_VIDEO) {
                d.setPosition(DISP_VIDEO, TERMINAL_CLUSTER, 0, 0);
            }
        } catch (Throwable t) { Log.w(TAG, "applyGeometry(" + ctx + ") failed: " + t); }
    }
}
