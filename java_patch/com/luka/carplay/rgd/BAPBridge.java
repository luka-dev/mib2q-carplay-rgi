/*
 * CarPlay Route Guidance - BAP Bridge
 *
 * Translates route guidance state to BAP protocol calls via AppConnectorNavi,
 * and drives maneuver_render via TCP for LVDS video rendering.
 *
 * BAP path: ManeuverDescriptor, distance, street, lane guidance, ETA -> VC/HUD navigation data.
 * maneuver_render path: CMD_MANEUVER over TCP -> maneuver_render EGL/GLES2 -> video encoder -> MOST -> VC LVDS.
 *
 *
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */
package com.luka.carplay.rgd;

import com.luka.carplay.core.CarPlayApp;
import com.luka.carplay.framework.Log;
import de.audi.atip.base.IFrameworkAccess;
import de.audi.atip.interapp.combi.bap.navi.CombiBAPServiceNavi;
import de.audi.atip.interapp.combi.bap.navi.data.CombiBAPDestinationInfo;
import de.audi.atip.interapp.combi.bap.navi.data.CombiBAPNaviDestination;
import de.audi.atip.interapp.combi.bap.navi.data.CombiBAPNaviLaneGuidanceData;
import de.audi.atip.interapp.combi.bap.navi.data.CombiBAPNaviManeuverDescriptor;
import de.audi.atip.log.LogChannel;
import de.audi.atip.metrics.DateMetric;
import de.audi.atip.metrics.Distance;
import de.audi.tghu.navi.app.Navigation;
import de.audi.tghu.navi.app.cluster.BAPDistanceFormatter;
import de.audi.tghu.navi.app.cluster.ClusterService;
import de.audi.tghu.navi.app.cluster.ClusterViewMode;
import de.audi.tghu.navi.app.cluster.KOMOService;
import de.audi.tghu.navi.app.command.DSIResponseContainer;

public class BAPBridge {

    private static final String TAG = "BAPBridge";
    /* RGType sent to cluster: 0=RGI (BAP ManeuverDescriptor icons for HUD).
     * FPK has rgType=4 hardcoded in CombiBAPListener -- our BAP rgType=0 is for the
     * AppConnectorNavi FSG sync flow, not for view mode selection. */
    private static final int ACTIVE_RGTYPE = 0;  /* RGI -- BAP icons. maneuver_render handles LVDS video. */

    /* ExitView variants (BAP spec FctID 49).  EU/NAR are used by
     * sendExitView() which toggles between them to defeat AppConnectorNavi's
     * sendStatusIfChanged dedup; exitViewNum=0 makes the variant cosmetic.
     * ROW/ASIA kept for protocol parity even though they're not currently
     * selected -- if regional variant logic returns, the codes are here. */
    private static final int EXITVIEW_EU = 0;
    private static final int EXITVIEW_NAR = 1;

    private static final int ROUTE_STATE_REROUTING = 5;
    private static final int CITY_DISPLAY_DISTANCE_M = 457;       // ~1500 ft (city/suburban approach; ~30 s at 30-35 mph)
    private static final int HIGHWAY_DISPLAY_DISTANCE_M = 1600;   // ~1.0 mile (highway/freeway approach)
    private static final int HYSTERESIS_BUFFER_M = 50;            // ~160 ft buffer to prevent boundary flapping
    private static final int CITY_PREPARE_THRESHOLD_M = 1500;
    private static final int HIGHWAY_PREPARE_THRESHOLD_M = 3000;
    private static final int BARGRAPH_ACTION_PERCENT_OF_PREPARE = 15;
    private static final int ACTION_BLINK_INTERVAL_MS = 600;
    /* Arrow fill threshold, exact in centimeters.  The bargraph/level is REMAINING distance
     * (FctID 18 stock semantics: 100 = far, 0 = at the maneuver; the renderer draws
     * fill = 1 - level/16): 100 % remaining at the approach-open denominator (arrow empty),
     * linear down to 0 % remaining at 250 ft (76.2 m, arrow completely full), which is also
     * where blinking starts. */
    private static final int BLINK_BELOW_CM = 7620;

    /* Reroute hold: a reroute pulse (route_state 5) inside the approach zone keeps the presentation
     * and the last arrow up for at most this long instead of closing it (closing then re-opening
     * within a few hundred ms left the Virtual Cockpit KDK pill on screen while we were back in
     * stock ctx 74, i.e. a black pill). */
    private static final int REROUTE_HOLD_MS = 2500;
    /* After a close that should hide the pill, re-check Fct44 visibility and re-close. */
    private static final int KDK_VERIFY_INTERVAL_MS = 1500;
    private static final int KDK_VERIFY_MAX_RETRIES = 3;

    /* BAP distance units defined by Audi's BAPDistanceFormatter. */
    private static final int BAP_DIST_UNIT_METERS = 0;
    private static final int BAP_DIST_UNIT_KILOMETERS = 1;
    private static final int BAP_DIST_UNIT_YARDS = 2;
    private static final int BAP_DIST_UNIT_FEET = 3;
    private static final int BAP_DIST_UNIT_MILES = 4;
    private static final int BAP_DIST_UNIT_QUARTER_MILES = 5;
    /* Audi BAP distance values are scaled by 10 (e.g. 1000 ft is 10000). */
    private static final int BAP_DISTANCE_SCALE_FACTOR = 10;

    /* CarPlay dist_maneuver_units values (hook/routeguidance/rgd_tlv.h DIST_UNIT_*):
     * 0=KM, 1=MILES, 2=M, 3=YARDS, 4=FT. */
    private static final int CARPLAY_DIST_UNIT_KM     = 0;
    private static final int CARPLAY_DIST_UNIT_MILES  = 1;
    private static final int CARPLAY_DIST_UNIT_METERS = 2;
    private static final int CARPLAY_DIST_UNIT_YARDS  = 3;
    private static final int CARPLAY_DIST_UNIT_FEET   = 4;

    private static final double METERS_TO_FEET  = 3.280839895;
    private static final double METERS_PER_MILE = 1609.344;

    /* BAP ManeuverState (FctID 55) values. */
    private static final int BAP_MANEUVER_STATE_INACTIVE = 0;
    private static final int BAP_MANEUVER_STATE_APPROACH = 1;
    private static final int BAP_MANEUVER_STATE_PREPARE = 2;
    private static final int BAP_MANEUVER_STATE_ACTION = 4;

    private CombiBAPServiceNavi appConnectorNavi;
    private final BAPDistanceFormatter distanceFormatter =
        new BAPDistanceFormatter(new SilentLogChannel());

    private boolean initialized = false;
    /* Set only after the complete synchronous BAP start sequence has returned successfully.
     * RouteGuidance combines this with renderer FRAME_READY before exposing context 80. */
    private volatile boolean bapSessionStarted = false;
    private static final String ROUTE_TEXT_PENDING = "\u2026";
    /* Directional turn arrow prefixes for Virtual Cockpit route text */
    public static final String ARROW_STRAIGHT     = "\u2191 "; // ↑
    public static final String ARROW_LEFT         = "\u2190 "; // ←
    public static final String ARROW_RIGHT        = "\u2192 "; // →
    public static final String ARROW_SLIGHT_LEFT  = "\u2196 "; // ↖
    public static final String ARROW_SLIGHT_RIGHT = "\u2197 "; // ↗
    public static final String ROUTE_TURN_PREFIX  = ARROW_STRAIGHT;
    /* U+25CC DOTTED CIRCLE, not a combining mark or an emoji sequence.
     * Present in VC's supplementary fonts; on-unit fallback still needs testing. */
    private static final String ROUTE_TIME_PREFIX = "\u25CC ";

    /* True whenever Java believes Audi BAP RG / KDK presentation is asserted. */
    private volatile boolean bapPresentationActive = false;
    /* Cache for formatted distance in FctID 19 to avoid continuous scroll resets. */
    private String lastFormattedTurnDistance = "";
    /* Latest CarPlay-supplied distance-to-maneuver display value (dist_maneuver_str /
     * dist_maneuver_units), refreshed from RouteGuidance.State whenever DIRTY_DIST_MAN
     * fires. formatDistanceToTurn() is the single resolver that prefers this verbatim
     * value (when parseable and unit-system-compatible) over formatting from meters. */
    private String carPlayDistManStr = null;
    private int carPlayDistManUnits = -1;

    /* CarPlay owns FctID 19/20/21/22/46 for the whole active RGI interval. */
    private String latchedPositionText = "";
    private String positionPrefix = "", positionSuffix = "";
    private final CurrentPositionScroll positionScroll = new CurrentPositionScroll();
    private String lastPositionSent;
    private boolean positionRestart = true;
    private boolean positionSendFailed;
    private boolean positionScrollChanged;
    private int positionManeuverIndex = -1, positionManeuverVersion = -1;
    private long positionRouteGeneration = -1L;
    private boolean routeTextPublished = false;
    /* User-selectable route-info representation. View changes always reset it
     * to 0; only the serialized RouteGuidance presentation worker mutates it. */
    private int infoPhase = 0;
    /* Explicit output caches allow View/OK to repaint immediately without
     * waiting for another iOS RGI delta. */
    private long lastEtaSeconds = -1L;
    private long lastTimeRemainingSeconds = -1L;
    private long lastTimeRemainingSampleSeconds = -1L;
    private int lastDistanceToDestinationM = -1;

    /* Approach mode controls only bargraph/blink timing. The real next-maneuver
     * descriptor remains visible at every distance. */
    private volatile boolean inApproachZone = false;
    /* Track the primary maneuver's slot identity so we know when iOS
     * actually swapped the head of the list vs. just reordered/extended it.
     * mVer changes when the C hook reassigns a slot to a new iAP2 index. */
    private int lastFirstManeuverIdx = -1;
    private int lastFirstManeuverVer = -1;
    private long lastFirstRouteGeneration = -1L;
    private long lastProcessedRouteGeneration = -1L;
    private boolean routeRebindPending = false;
    /* Call-for-action blink phase: true=100%, false=0% */
    private boolean actionBlinkFull = true;
    private final Object distanceToManeuverLock = new Object();
    private boolean hasLastDistM = false;
    private int lastDistM = 0;
    private boolean lastBarOn = false;
    private int lastBar = 0;
    private int lastProgressState = RendererServer.PROGRESS_OFF;
    private Thread actionBlinkThread;
    private boolean actionBlinkThreadRunning = false;
    /* Monotonically increases on every start/stop.  Each spawned blink
     * thread captures the value at start time; on every iteration it
     * re-checks against the current counter and exits if a newer
     * generation has been allocated.  This guarantees a stale thread
     * (e.g., one we couldn't join in time) cannot survive into a new
     * approach-zone cycle and double up the bargraph blink. */
    private int actionBlinkGeneration = 0;
    private int blinkDistM = -1;
    private int blinkBargraphDenominatorM = -1;
    private boolean blinkArmed = false;
    private int exitViewNum = 0;
    /*
     * FSG sync(1) fix: AppConnectorNavi uses sendStatusIfChanged internally.
     * If exitView variant+num is unchanged, FctID 49 is not sent, sync(1) for
     * {23,18,49} never closes, and ManeuverDescriptor updates are silently
     * dropped.  Toggling the variant forces a "change" on every descriptor send.
     * Since exitViewNum=0 (no exit view), the variant is cosmetically irrelevant.
     */
    private int exitViewSendCount = 0;
    /* REPLACE: true from CarPlay connect (engageTakeover) to disconnect (disengageTakeover);
     * keeps onShutdown from reopening the RG gate when CarPlay nav merely ends mid-session. */
    private volatile boolean takeoverEngaged = false;
    private boolean nativeStopAttempted = false;
    /* Our rgActive forge is an overlay on shared HMI state, not our own flag: the whole
     * navigation app reads DSIResponseContainer.isRgActive() (StartRouteGuidanceSequences,
     * RgStopGuidanceGeneralCommand, RGStartGuidanceCalculatedRoute, ClusterViewMode ...).
     * Remember what the DSI last reported so releasing the overlay restores that value
     * instead of guessing -- a stale forged "true" sends the stock "start route guidance"
     * sequence down the add-stopover/replace-destination-on-active-route branch of a route
     * the nav core no longer guides, and it aborts on every attempt until the next reboot. */
    private boolean rgActiveForced = false;
    private boolean rgActiveSaved = false;
    /* A route-absent result is definite for connect-time takeover, but a stock route may
     * still be started later in the same CarPlay session.  Re-check that result once when
     * CarPlay RGI itself activates; a successfully issued stop remains session-final. */
    private boolean nativeStopWasRouteAbsent = false;
    private boolean nativeAbortIssued = false;
    private int nativeStopLogState = -1;

    private ClusterService csRef;
    private KOMOService komoService;
    private RendererServer rendererClient;
    private volatile PresentationListener presentationListener;

    /** Non-blocking edge used by RouteGuidance's presentation worker. */
    public interface PresentationListener {
        void onPresentationStateChanged(String reason);
    }

    public void setPresentationListener(PresentationListener listener) {
        presentationListener = listener;
    }

    private void notifyPresentationStateChanged(String reason) {
        PresentationListener l = presentationListener;
        if (l == null) return;
        try { l.onPresentationStateChanged(reason); }
        catch (Throwable t) { Log.w(TAG, "presentation listener failed: " + t); }
    }

    private static final class SilentLogChannel extends LogChannel {
        public void log(int level, String pattern,
                        Object a, Object b, Object c, Object d,
                        long l1, long l2, long l3, int flags, Throwable t) {
            /* no-op */
        }
        public void log(int level, int messageId,
                        Object a, Object b, Object c, Object d,
                        long l1, long l2, long l3, int flags, Throwable t) {
            /* no-op */
        }
    }

    private static final class FormattedDistance {
        final int value;
        final int unit;

        FormattedDistance(int value, int unit) {
            this.value = value;
            this.unit = unit;
        }
    }

    private synchronized void resetActionBlinkState() {
        actionBlinkFull = true;
        blinkDistM = -1;
        blinkBargraphDenominatorM = -1;
        blinkArmed = false;
        /* RC#1: a reroute or explicit-clear update sets rendererManeuverPending=true but
         * then skips updateRendererIfChanged (which is the only place that clears it),
         * because nowApproach=false/explicitClear suppresses the approach paint path.
         * Resetting here (called from onStart + stopActionBlinkThread) ensures the flag
         * is clean for every new approach cycle, so updateRendererProgress is never
         * permanently blocked by a stale latch from a prior reroute. */
        rendererManeuverPending = false;
    }

    private synchronized void updateActionBlinkContext(boolean armed, int distM, int bargraphDenominatorM) {
        blinkArmed = armed;
        blinkDistM = distM;
        blinkBargraphDenominatorM = bargraphDenominatorM;
        if (!armed || distM <= 0 || bargraphDenominatorM <= 0 || distM > bargraphDenominatorM) {
            actionBlinkFull = true;
        }
    }


    /* ---- Reroute hold + KDK-close verification (one small bounded daemon timer) ----
     * All state below is guarded by timerLock, which is never held while calling anything else
     * (no nesting with `this`, distanceToManeuverLock or ScreenModule).  The single "BAPTimer"
     * thread sleeps in timerLock.wait() until the earliest deadline and exits when idle. */
    private final Object timerLock = new Object();
    /* Serializes update() with the BAPTimer bodies (hold expiry, KDK re-close).  Lock order:
     * updateLock -> timerLock / this; timerLock is never held while taking updateLock. */
    private final Object updateLock = new Object();
    private int rerouteHoldMs = REROUTE_HOLD_MS;
    private int kdkVerifyMs = KDK_VERIFY_INTERVAL_MS;
    private boolean rerouteHoldActive = false;
    private long rerouteHoldDeadline = 0L;
    private int rerouteHoldGen = 0;
    private boolean kdkVerifyPending = false;
    private long kdkVerifyDue = 0L;
    private int kdkVerifyRetries = 0;
    private int kdkVerifyGen = 0;
    private boolean timerRunning = false;
    private int timerThreadGen = 0;

    /** Fct44 KDK visibility as acknowledged by the Virtual Cockpit (overridable for tests). */
    protected boolean isKdkVisibleNow() {
        return com.luka.carplay.cluster.ClusterLayerController.isKdkVisible();
    }

    private void ensureTimerThread() {
        synchronized (timerLock) {
            if (timerRunning) {
                timerLock.notifyAll();
                return;
            }
            timerRunning = true;
            final int myGen = ++timerThreadGen;
            Thread t = new Thread(new Runnable() {
                public void run() {
                    timerLoop(myGen);
                }
            }, "BAPTimer");
            t.setDaemon(true);
            t.start();
        }
    }

    private void timerLoop(int myGen) {
        while (true) {
            int action = 0;      /* 1 = reroute-hold expiry, 2 = KDK verify */
            int actionGen = 0;
            synchronized (timerLock) {
                if (myGen != timerThreadGen) return;
                long now = System.currentTimeMillis();
                long next = -1L;
                if (rerouteHoldActive) {
                    if (now >= rerouteHoldDeadline) {
                        action = 1;
                        actionGen = rerouteHoldGen;
                    } else {
                        next = rerouteHoldDeadline;
                    }
                }
                if (action == 0 && kdkVerifyPending) {
                    if (now >= kdkVerifyDue) {
                        action = 2;
                        actionGen = kdkVerifyGen;
                    } else if (next < 0L || kdkVerifyDue < next) {
                        next = kdkVerifyDue;
                    }
                }
                if (action == 0) {
                    if (next < 0L) {
                        timerRunning = false;   /* idle: nothing pending, thread ends */
                        return;
                    }
                    try {
                        timerLock.wait(next - now);
                    } catch (InterruptedException e) {
                        /* re-evaluate deadlines */
                    }
                    continue;
                }
            }
            try {
                if (action == 1) expireRerouteHold(actionGen);
                else verifyKdkClosed(actionGen);
            } catch (Throwable t) {
                Log.w(TAG, "BAPTimer action failed: " + t);
            }
        }
    }

    private void cancelRerouteHold() {
        synchronized (timerLock) {
            if (rerouteHoldActive) {
                rerouteHoldActive = false;
                ++rerouteHoldGen;
                timerLock.notifyAll();   /* let an idle timer thread exit promptly */
            }
        }
    }

    private void cancelKdkVerify() {
        synchronized (timerLock) {
            if (kdkVerifyPending) {
                kdkVerifyPending = false;
                ++kdkVerifyGen;
                timerLock.notifyAll();
            }
        }
    }

    /** Called after a close that is meant to hide the KDK pill. */
    private void scheduleKdkVerify() {
        synchronized (timerLock) {
            kdkVerifyPending = true;
            kdkVerifyRetries = 0;
            kdkVerifyDue = System.currentTimeMillis() + kdkVerifyMs;
            ++kdkVerifyGen;
        }
        ensureTimerThread();
    }

    /** Timer thread: the hold ran out with no RGI update to notice it. */
    private void expireRerouteHold(int gen) {
        synchronized (updateLock) {
            synchronized (timerLock) {
                if (!rerouteHoldActive || gen != rerouteHoldGen) return;
                rerouteHoldActive = false;
                ++rerouteHoldGen;
            }
            if (!bapPresentationActive && !com.luka.carplay.core.ScreenModule.isPresentationActive()) return;
            Log.i(TAG, "Reroute hold expired after " + rerouteHoldMs + " ms: closing BAP presentation");
            inApproachZone = false;
            stopActionBlinkThread();
            closeApproachPresentation("reroute-hold-expired", -1L, true, true, true);
        }
    }

    /** Timer thread: was the pill really hidden?  Re-issue the (idempotent) close if not. */
    private void verifyKdkClosed(int gen) {
        synchronized (updateLock) {
            boolean holdOn;
            synchronized (timerLock) {
                if (!kdkVerifyPending || gen != kdkVerifyGen) return;
                holdOn = rerouteHoldActive;
            }
            boolean wantClosed = !bapPresentationActive
                && !com.luka.carplay.core.ScreenModule.isPresentationActive() && !holdOn;
            if (!wantClosed || !isKdkVisibleNow()) {
                synchronized (timerLock) {
                    if (gen == kdkVerifyGen) kdkVerifyPending = false;
                }
                return;
            }
            int retry;
            synchronized (timerLock) {
                retry = ++kdkVerifyRetries;
                if (retry > KDK_VERIFY_MAX_RETRIES) {
                    kdkVerifyPending = false;
                } else {
                    kdkVerifyDue = System.currentTimeMillis() + kdkVerifyMs;
                }
            }
            if (retry > KDK_VERIFY_MAX_RETRIES) {
                Log.w(TAG, "KDK still visible after " + KDK_VERIFY_MAX_RETRIES
                    + " close retries; giving up");
                return;
            }
            Log.i(TAG, "KDK still visible after close: retry " + retry);
            closeBapPresentation();
        }
    }

    /** Approach-zone EXIT: withdraw the BAP presentation, hand the cluster back to stock (ctx 74)
     * and blank the renderer. Shared by update() and the reroute-hold expiry. */
    private void closeApproachPresentation(String site, long routeGen, boolean rerouting,
                                           boolean explicit, boolean shouldClear) {
        boolean closed = closeBapPresentation();
        if (closed) {
            com.luka.carplay.core.ScreenModule.setPresentationActive(false);
            if (rendererClient != null) {
                rendererClient.sendClear();
                lastCrIdx = -1;
                lastCrIcon = -1;
                diagClearedAwaitingManeuver = true;
                Log.i(TAG, "RGI-DIAG renderer CLEAR site=" + site + " gen=" + routeGen
                    + " rerouting=" + rerouting + " explicit=" + explicit
                    + " shouldClear=" + shouldClear);
            }
            synchronized (this) { rendererManeuverPending = false; }
            scheduleKdkVerify();
        } else {
            Log.w(TAG, "BAP close incomplete; retaining presentation state for retry");
        }
    }

    /** Force the next update to resend the maneuver: same invalidation as the renderer re-prime. */
    private synchronized void invalidateRendererDedup() {
        lastCrIcon = -1;
        lastCrDirection = -99;
        lastCrExitAngle = -9999;
        lastCrDrivingSide = -1;
        lastCrVer = -1;
        lastCrIdx = -1;
        lastCrRouteGeneration = -1L;
        lastCrJunctionAngles = null;
        lastCrSnapToRoad = false;
        rendererManeuverPending = true;
    }

    private void startActionBlinkThread() {
        final int myGen;
        synchronized (this) {
            if (actionBlinkThreadRunning) return;
            actionBlinkThreadRunning = true;
            myGen = ++actionBlinkGeneration;
            actionBlinkThread = new Thread(new Runnable() {
                public void run() {
                    actionBlinkLoop(myGen);
                }
            }, "BAPActionBlink");
            actionBlinkThread.setDaemon(true);
            actionBlinkThread.start();
        }
    }

    private void stopActionBlinkThread() {
        Thread t;
        synchronized (this) {
            if (!actionBlinkThreadRunning) return;
            actionBlinkThreadRunning = false;
            /* Bump generation immediately so any wakeup of the old thread
             * (even after this method returns without successful join)
             * sees a stale generation and exits cleanly. */
            ++actionBlinkGeneration;
            t = actionBlinkThread;
            actionBlinkThread = null;
            resetActionBlinkState();
        }
        if (t != null) {
            t.interrupt();
            try { t.join(500); } catch (InterruptedException e) { /* ignore */ }
            if (t.isAlive()) {
                /* Thread didn't honor interrupt within 500 ms.  Generation
                 * bump above guarantees it can't actually mutate state
                 * after waking up, so this is a soft warning, not a leak
                 * of behavior. */
                Log.w(TAG, "Blink thread still alive after join(500); orphaned by generation bump");
            }
        }
    }

    private void actionBlinkLoop(int myGen) {
        while (true) {
            try {
                Thread.sleep(ACTION_BLINK_INTERVAL_MS);
            } catch (InterruptedException e) {
                /* loop continues; stop is signaled inside sendActionBlinkTick
                 * via generation check */
            }
            /* Authoritative generation + state check happens atomically
             * inside sendActionBlinkTick(myGen) under synchronized(this).
             * No outer unsynchronized check — reading non-volatile fields
             * here could see stale values and prematurely kill a live
             * thread.  When stop is requested, sendActionBlinkTick will
             * observe the new generation and return; we still return
             * here because we want to exit the loop too. */
            if (!sendActionBlinkTick(myGen)) {
                return;
            }
        }
    }

    /**
     * Emit one blink tick for the calling thread's generation.  Returns
     * true if the loop should continue, false if this thread is now an
     * orphan (generation bumped) and should exit.
     *
     * Generation check + state read + send are all inside one
     * synchronized(this) block, mutually exclusive with start/stop and
     * resetActionBlinkState().  No way for an orphan thread to send a
     * tick using a fresh generation's state.
     */
    /** Single source of truth for "should the bargraph be flashing right now": inside the
     * denominator and closer than 250 ft (76.2 m). */
    private static boolean isInBlinkZone(int distM, int denomM) {
        return distM > 0 && denomM > 0 && distM <= denomM && distM * 100 < BLINK_BELOW_CM;
    }

    /** Remaining-distance percent (0..100) shared by the cluster bar (FctID 18) and the renderer
     * level: 100 at the denominator (approach-open distance; clamped when opened slightly beyond
     * it by hysteresis), 0 at <= BLINK_BELOW_CM (76.2 m: arrow completely full), linear between. */
    static int bargraphPercent(int distM, int denomM) {
        int denomCm = denomM * 100, distCm = distM * 100;
        if (denomCm <= BLINK_BELOW_CM) return distCm <= BLINK_BELOW_CM ? 0 : 100;
        int pct = ((distCm - BLINK_BELOW_CM) * 100) / (denomCm - BLINK_BELOW_CM);
        if (pct < 0) pct = 0;
        if (pct > 100) pct = 100;
        return pct;
    }

    private boolean sendActionBlinkTick(int myGen) {
        synchronized (this) {
            if (myGen != actionBlinkGeneration) return false;
            if (!blinkArmed || !isInBlinkZone(blinkDistM, blinkBargraphDenominatorM)) {
                actionBlinkFull = true;
                return true;
            }
            int bargraph = actionBlinkFull ? 100 : 0;
            actionBlinkFull = !actionBlinkFull;

            try {
                sendDistanceToManeuverRaw(blinkDistM, true, bargraph,
                    bargraph==100 ? RendererServer.PROGRESS_BLINK_HIGH : RendererServer.PROGRESS_BLINK_LOW);
            } catch (Exception e) {
                Log.e(TAG, "Action blink tick failed", e);
            }
        }
        return true;
    }

    /**
     * Send distance to maneuver through AppConnectorNavi using native formatter rules.
     */
    private void sendDistanceToManeuverRaw(int meters, boolean bargraphOn, int bargraph) throws Exception {
        sendDistanceToManeuverRaw(meters,bargraphOn,bargraph,
            bargraphOn ? RendererServer.PROGRESS_FILL : RendererServer.PROGRESS_OFF);
    }

    /* Serialize snapshot + BAP emission + VC enqueue with the action-blink worker.
     * Lock order stays this -> distanceToManeuverLock -> renderer queue. */
    private synchronized void sendDistanceToManeuverRaw(int meters, boolean bargraphOn, int bargraph,
                                           int progressState) throws Exception {
        if(meters<=0 || !bargraphOn) progressState=RendererServer.PROGRESS_OFF;
        synchronized (distanceToManeuverLock) {
            if (meters > 0) {
                hasLastDistM = true;
                lastDistM = meters;
                lastBarOn = bargraphOn;
                lastBar = bargraph;
                lastProgressState = progressState;
            } else {
                hasLastDistM = false;
                lastDistM = 0;
                lastBarOn = false;
                lastBar = 0;
            }
        }

        if (meters <= 0) {
            bargraphOn = false;
            bargraph = 0;
        }

        FormattedDistance fd = formatDistanceToTurn(meters);
        appConnectorNavi.updateDistanceToNextManeuver(fd.value, fd.unit, bargraphOn, bargraph);
        /* BAP confirmed emitted → push same state to renderer so HUD bar
         * flips at the same moment as VC.  Derive renderer level/mode from
         * the BAP parameters directly — no shared mutable, no race. */
        if (rendererClient != null && customRendererStarted && !rendererManeuverPending) {
            try {
                int crLevel = bargraphOn ? (bargraph * 16) / 100 : 0;
                if (crLevel > 16) crLevel = 16;
                int crMode = bargraphOn ? 1 : 0;
                noteRendererSendResult(rendererClient.sendProgress(crLevel, crMode, progressState));
            } catch (Throwable t) {
                noteRendererSendResult(false);
                /* BAP already sent; renderer will resync on next tick */
            }
        }
    }

    /** Replay the same HUD phase atomically; a blink tick cannot overtake the cache read. */
    private synchronized void replayDistanceToManeuver() throws Exception {
        boolean haveCached;
        int cachedDistM;
        boolean cachedBarOn;
        int cachedBar;
        int cachedProgress;
        synchronized (distanceToManeuverLock) {
            haveCached = hasLastDistM;
            cachedDistM = lastDistM;
            cachedBarOn = lastBarOn;
            cachedBar = lastBar;
            cachedProgress = lastProgressState;
        }
        if (haveCached) {
            sendDistanceToManeuverRaw(cachedDistM, cachedBarOn, cachedBar, cachedProgress);
        } else {
            sendDistanceToManeuverRaw(0, false, 0);
        }
    }

    private void sendDistanceToDestinationRaw(int meters, boolean isStopOver) {
        FormattedDistance fd = formatDistanceToDestination(meters);
        appConnectorNavi.updateDistanceToDestination(fd.value, fd.unit, isStopOver);
    }

    /**
     * Single resolver for distance-to-maneuver, used for BOTH the FctID 18 native
     * cluster distance (via sendDistanceToManeuverRaw) AND the VC status text (via
     * formatTurnDistanceForText). Prefers CarPlay's own already-localized display
     * value (dist_maneuver_str/dist_maneuver_units) verbatim when it is present,
     * parseable, and in the same unit system as the car; otherwise falls back to
     * formatting from meters in the car's system with CarPlay-like rounding.
     */
    private FormattedDistance formatDistanceToTurn(int meters) {
        if (meters <= 0) return new FormattedDistance(-1, 0);
        try {
            boolean metric = isMetricDistanceUnits();
            FormattedDistance fromCarPlay =
                resolveCarPlayDistance(carPlayDistManStr, carPlayDistManUnits, metric);
            if (fromCarPlay != null) return fromCarPlay;
            return formatDistanceFallback(meters, metric);
        } catch (Throwable t) {
            Log.w(TAG, "formatDistanceToTurn failed, using invalid distance: " + t.getMessage());
            return new FormattedDistance(-1, 0);
        }
    }

    /**
     * Verbatim path: use CarPlay's own number+unit when present, parseable, and
     * in the same unit system (metric/imperial) as the car. Returns null when
     * any of those conditions fail, so the caller can fall back to formatting
     * from meters.
     */
    private static FormattedDistance resolveCarPlayDistance(String carPlayStr, int carPlayUnits,
                                                              boolean carMetric) {
        if (carPlayStr == null || carPlayStr.length() == 0) return null;
        boolean carPlayMetric;
        int bapUnit;
        switch (carPlayUnits) {
            case CARPLAY_DIST_UNIT_KM:     carPlayMetric = true;  bapUnit = BAP_DIST_UNIT_KILOMETERS; break;
            case CARPLAY_DIST_UNIT_METERS: carPlayMetric = true;  bapUnit = BAP_DIST_UNIT_METERS; break;
            case CARPLAY_DIST_UNIT_MILES:  carPlayMetric = false; bapUnit = BAP_DIST_UNIT_MILES; break;
            case CARPLAY_DIST_UNIT_YARDS:  carPlayMetric = false; bapUnit = BAP_DIST_UNIT_YARDS; break;
            case CARPLAY_DIST_UNIT_FEET:   carPlayMetric = false; bapUnit = BAP_DIST_UNIT_FEET; break;
            default: return null; /* unknown/absent unit -> can't verify system match */
        }
        if (carPlayMetric != carMetric) return null; /* different unit system than the car -> fall back */
        double number = parseLeadingNumber(carPlayStr);
        if (Double.isNaN(number) || number < 0) return null;
        int value = (int) Math.round(number * (double) BAP_DISTANCE_SCALE_FACTOR);
        if (value <= 0) return null;
        return new FormattedDistance(value, bapUnit);
    }

    /** True for a character CarPlay might use as a thousands-grouping separator:
     * ASCII comma, or the narrow-no-break/no-break/thin space variants some
     * locales use in place of a comma. */
    private static boolean isThousandsGroupingSeparator(char c) {
        return c == ',' || c == ' ' || c == ' ' || c == ' ';
    }

    /** Parses the leading decimal number out of a CarPlay display string (e.g.
     * "0.3", "3", "1.2 mi", "1,200" all yield their numeric prefix). NaN if none
     * found. A thousands-grouping separator (see isThousandsGroupingSeparator)
     * is consumed ONLY when followed by exactly 3 digits before the next
     * non-digit/'.' (so "1,200" -> 1200, "12,500" -> 12500); any other use of
     * one of those characters -- most importantly a decimal comma such as
     * "0,3" or "1,2" -- is a hard parse failure (NaN) rather than a silent
     * truncation, so the caller falls back to formatting from meters. */
    private static double parseLeadingNumber(String s) {
        int i = 0, n = s.length();
        while (i < n && Character.isWhitespace(s.charAt(i))) i++;
        boolean neg = false;
        if (i < n && (s.charAt(i) == '-' || s.charAt(i) == '+')) {
            neg = s.charAt(i) == '-';
            i++;
        }
        StringBuffer digits = new StringBuffer();
        boolean sawDigit = false;
        while (true) {
            int j = i;
            while (j < n && Character.isDigit(s.charAt(j))) j++;
            int groupLen = j - i;
            if (groupLen == 0) break;
            digits.append(s.substring(i, j));
            sawDigit = true;
            i = j;
            if (i < n && isThousandsGroupingSeparator(s.charAt(i))) {
                int k = i + 1;
                while (k < n && Character.isDigit(s.charAt(k))) k++;
                if (k - (i + 1) == 3) {
                    /* Valid grouping separator: consume it, loop appends the next group. */
                    i = i + 1;
                    continue;
                }
                /* A grouping-separator character that is NOT followed by exactly 3 digits --
                 * most commonly a decimal comma ("0,3") -- must not be silently truncated
                 * into the wrong number. Reject the whole string. */
                return Double.NaN;
            }
            break;
        }
        if (!sawDigit) return Double.NaN;
        if (i < n && s.charAt(i) == '.') {
            int j = i + 1;
            while (j < n && Character.isDigit(s.charAt(j))) j++;
            /* Only consume the '.' when at least one fractional digit follows;
             * otherwise leave i where it was so a bare "3." still parses as "3". */
            if (j > i + 1) {
                digits.append(s.substring(i, j)); // includes the '.' itself
                i = j;
            }
        }
        String numStr = digits.toString();
        if (numStr.length() == 0 || ".".equals(numStr)) return Double.NaN;
        try {
            double v = Double.parseDouble(numStr);
            return neg ? -v : v;
        } catch (NumberFormatException e) {
            return Double.NaN;
        }
    }

    /**
     * Fallback path: format from meters in the car's unit system with
     * CarPlay-like rounding, never producing the Audi quarter-mile format.
     * Imperial: feet below 0.1 mi (528 ft) -- nearest 10 ft under 100 ft, nearest
     * 50 ft from 100 ft up; miles at 0.1 mi steps below 10 mi; whole miles at/above.
     * Metric: meters under 1 km -- nearest 10 m under 100 m, nearest 50 m from
     * 100 m up; kilometers at 0.1 km steps below 10 km; whole km at/above.
     */
    private static FormattedDistance formatDistanceFallback(int meters, boolean metric) {
        if (meters <= 0) return new FormattedDistance(-1, 0);
        if (metric) {
            if (meters < 1000) {
                int step = (meters < 100) ? 10 : 50;
                int rounded = roundToStep(meters, step);
                if (rounded <= 0) rounded = step;
                return new FormattedDistance(rounded * BAP_DISTANCE_SCALE_FACTOR, BAP_DIST_UNIT_METERS);
            }
            double km = meters / 1000.0;
            if (km < 10.0) {
                int tenths = (int) Math.round(km * 10.0);
                if (tenths <= 0) tenths = 1;
                return new FormattedDistance(tenths, BAP_DIST_UNIT_KILOMETERS);
            }
            int wholeKm = (int) Math.round(km);
            if (wholeKm <= 0) wholeKm = 1;
            return new FormattedDistance(wholeKm * BAP_DISTANCE_SCALE_FACTOR, BAP_DIST_UNIT_KILOMETERS);
        } else {
            double feet = meters * METERS_TO_FEET;
            if (feet < 528.0) {
                int step = (feet < 100.0) ? 10 : 50;
                int rounded = roundToStep((int) Math.round(feet), step);
                if (rounded <= 0) rounded = step;
                return new FormattedDistance(rounded * BAP_DISTANCE_SCALE_FACTOR, BAP_DIST_UNIT_FEET);
            }
            double miles = meters / METERS_PER_MILE;
            if (miles < 10.0) {
                int tenths = (int) Math.round(miles * 10.0);
                if (tenths <= 0) tenths = 1;
                return new FormattedDistance(tenths, BAP_DIST_UNIT_MILES);
            }
            int wholeMiles = (int) Math.round(miles);
            if (wholeMiles <= 0) wholeMiles = 1;
            return new FormattedDistance(wholeMiles * BAP_DISTANCE_SCALE_FACTOR, BAP_DIST_UNIT_MILES);
        }
    }

    private static int roundToStep(int value, int step) {
        if (step <= 0) return value;
        return ((value + step / 2) / step) * step;
    }

    private String formatTurnDistanceForText(int meters) {
        if (meters <= 0) return "";
        FormattedDistance fd = formatDistanceToTurn(meters);
        if (fd == null || fd.value <= 0) return "";
        return formatBapDistanceString(fd.value, fd.unit);
    }

    private static String formatBapDistanceString(int value, int unit) {
        if (value <= 0) return "";
        switch (unit) {
            case BAP_DIST_UNIT_METERS:
                return (value / 10) + " m";
            case BAP_DIST_UNIT_KILOMETERS: {
                int whole = value / 10;
                int frac = value % 10;
                return (frac > 0) ? (whole + "." + frac + " km") : (whole + " km");
            }
            case BAP_DIST_UNIT_FEET:
                return (value / 10) + " ft";
            case BAP_DIST_UNIT_YARDS:
                return (value / 10) + " yd";
            case BAP_DIST_UNIT_MILES: {
                int whole = value / 10;
                int frac = value % 10;
                return (frac > 0) ? (whole + "." + frac + " mi") : (whole + " mi");
            }
            case BAP_DIST_UNIT_QUARTER_MILES: {
                /* Defensive: our own resolver never emits this unit, but if a
                 * caller (e.g. Audi's BAPDistanceFormatter, used elsewhere for
                 * distance-to-destination) ever routes a quarter-mile value
                 * through here, never surface "1/4 mi"-style fraction text --
                 * convert to the same 0.1-mi decimal-mile format as everywhere
                 * else. value is quarter-miles*10. */
                int quarterMiles = value / 10;
                int milesTenths = (int) Math.round(quarterMiles * 2.5);
                int whole = milesTenths / 10;
                int frac = milesTenths % 10;
                return (frac > 0) ? (whole + "." + frac + " mi") : (whole + " mi");
            }
            default:
                return (value / 10) + " m";
        }
    }

    private FormattedDistance formatDistanceToDestination(int meters) {
        if (meters <= 0) return new FormattedDistance(-1, 0);
        try {
            boolean metric = isMetricDistanceUnits();
            BAPDistanceFormatter.BAPDistance d = distanceFormatter.formatDistanceToDestination(meters, metric);
            return new FormattedDistance(d.getValue(), d.getUnit());
        } catch (Throwable t) {
            Log.w(TAG, "formatDistanceToDestination failed, using invalid distance: " + t.getMessage());
            return new FormattedDistance(-1, 0);
        }
    }

    private static boolean isMetricDistanceUnits() {
        try {
            boolean isMetric;
            int unit = Distance.getSystemUnit();
            if (unit == Distance.KM || unit == Distance.METERS){
                isMetric = true;
            } else {
                isMetric = false;
            }
            return isMetric;
        } catch (Throwable t) {
            /* Fallback is imperial only for the handful of countries that actually
             * use it (US, GB, and the other two commonly cited imperial-distance
             * countries, LR/MM); everywhere else defaults to metric. */
            try {
                String c = java.util.Locale.getDefault().getCountry();
                if ("US".equalsIgnoreCase(c) || "GB".equalsIgnoreCase(c)
                        || "LR".equalsIgnoreCase(c) || "MM".equalsIgnoreCase(c)) {
                    return false;
                }
                return true;
            } catch (Throwable t2) {
                return true;
            }
        }
    }



    /* ============================================================
     * Initialization
     * ============================================================ */

    public boolean init(Object naviService) {
        if (initialized) return true;

        try {
            if (!(naviService instanceof CombiBAPServiceNavi)) {
                String cls = (naviService != null) ? naviService.getClass().getName() : "null";
                Log.e(TAG, "Init failed: service is not CombiBAPServiceNavi (" + cls + ")");
                return false;
            }
            this.appConnectorNavi = (CombiBAPServiceNavi) naviService;

            initialized = true;
            Log.i(TAG, "Initialized successfully (AppConnectorNavi only): " + naviService.getClass().getName());
            return true;

        } catch (Exception e) {
            Log.e(TAG, "Init failed", e);
            return false;
        }
    }

    /**
     * Reach into the HMI Navigation singleton and command native nav
     * (running in AppStartATF) to cancel its current route guidance.
     * Necessary before we set blockRouteGuidance=true: once the gate is
     * closed, native nav's own cancel BAP messages are also dropped, so
     * the user can't dismiss it from the cluster.
     *
     * Path verified against decompiled MU1316 lsd.jar:
     *   Navigation.getInstance().getRouteManager() -> IRouteManager
     *   IRouteManager extends IStartGuidanceManager -> stopRouteGuidance()
     *   RouteManager.stopRouteGuidance() builds and executes the
     *   "stop route guidance" CommandList that walks every component down
     *   (BAP RG state off, route data cleared, KOMO disabled, etc).
     */
    private void logNativeStopEdge(int state, String message, boolean warning) {
        if (nativeStopLogState == state) return;
        nativeStopLogState = state;
        if (warning) Log.w(TAG, message);
        else Log.i(TAG, message);
    }

    /** @return true only after RouteManager supplied a definite result.  A null singleton,
     *  null RouteManager or transient exception is PENDING and must be retried by startRetry. */
    private boolean tryStopNativeNavigation() {
        /* Always issue the cluster-side abort signal first — at minimum it
         * resets cluster UI state even if RouteManager is unavailable. */
        if (!nativeAbortIssued && csRef != null) {
            try {
                csRef.setRouteGuidanceAborted();
                nativeAbortIssued = true;
                Log.i(TAG, "Pre-gate: csRef.setRouteGuidanceAborted() ok");
            } catch (Throwable t) {
                Log.w(TAG, "Pre-gate: setRouteGuidanceAborted: " + t.getMessage());
            }
        }

        try {
            de.audi.tghu.navi.app.Navigation navi =
                de.audi.tghu.navi.app.Navigation.getInstance();
            if (navi == null) {
                nativeStopWasRouteAbsent = false;
                logNativeStopEdge(1,
                    "Pre-gate: PENDING — Navigation.getInstance() is null", true);
                return false;
            }
            de.audi.tghu.navi.app.routeguidance.IRouteManager rm = navi.getRouteManager();
            if (rm == null) {
                nativeStopWasRouteAbsent = false;
                logNativeStopEdge(2,
                    "Pre-gate: PENDING — RouteManager is null", true);
                return false;
            }
            /* Skip work if no active route — stopRouteGuidance() is a heavy
             * CommandList walk; harmless on idle but spammy in the log. */
            org.dsi.ifc.navigation.Route route = rm.getRoute();
            if (route == null) {
                nativeStopWasRouteAbsent = true;
                logNativeStopEdge(3,
                    "Pre-gate: DONE — native route already absent", false);
                return true;
            }
            rm.stopRouteGuidance();
            nativeStopWasRouteAbsent = false;
            logNativeStopEdge(4,
                "Pre-gate: DONE — RouteManager.stopRouteGuidance() issued", false);
            return true;
        } catch (Throwable t) {
            nativeStopWasRouteAbsent = false;
            logNativeStopEdge(5, "Pre-gate: PENDING — stopRouteGuidance failed: "
                    + t.getClass().getName() + ": " + t.getMessage(), true);
            return false;
        }
    }

    /**
     * Get ClusterService via Navigation singleton and install native BAP gate.
     * Non-fatal - if this fails, native RG stream won't be blocked.
     */
    private void initClusterAccess() {
        try {
            Navigation navi = Navigation.getInstance();
            if (navi == null) {
                Log.w(TAG, "ClusterAccess: Navigation.getInstance() returned null");
                return;
            }
            ClusterService cs = navi.getClusterService();
            if (cs == null) {
                Log.w(TAG, "ClusterAccess: ClusterService is null");
                return;
            }
            this.csRef = cs;
            this.komoService = cs.getKomoService();
            if (this.komoService != null) {
                Log.i(TAG, "KOMO service acquired");
            } else {
                Log.w(TAG, "KOMO service is null (non-fatal, LVDS video won't work)");
            }

            Log.i(TAG, "ClusterAccess init OK");
        } catch (Exception e) {
            Log.w(TAG, "ClusterAccess setup failed (non-fatal): " + e.getMessage());
        }
    }

    /**
     * Force cluster acceptance flags so VC accepts RGI BAP messages.
     */
    private void forceClusterRouteInfoState(boolean active) {
        if (csRef == null) return;

        try {
            DSIResponseContainer container = csRef.getDSIResponseContainer();
            if (container != null) {
                if (active) {
                    if (!rgActiveForced) {
                        rgActiveSaved = container.isRgActive();
                        rgActiveForced = true;
                    }
                    container.setRgActive(true);
                } else if (rgActiveForced) {
                    rgActiveForced = false;
                    container.setRgActive(rgActiveSaved);
                    Log.i(TAG, "rgActive overlay released (restored " + rgActiveSaved + ")");
                }
            }
        } catch (Exception e) {
        }

        if (active) {
            try { csRef.updateRGIString(new short[]{1}); }
            catch (Exception e) { Log.w(TAG, "force rgiValid=true failed: " + e.getMessage()); }
        } else {
            try { csRef.updateRGIString(null); }
            catch (Exception e) { Log.w(TAG, "force rgiValid=false failed: " + e.getMessage()); }
        }

        try { csRef.triggerRefreshRGIValid(); }
        catch (Exception e) { Log.w(TAG, "refreshRGIValid failed: " + e.getMessage()); }
    }

    public boolean isBapPresentationActive() {
        return bapPresentationActive;
    }

    /**
     * Asserts the OEM Audi BAP route-guidance presentation and KDK pill on the Virtual Cockpit.
     * Completes the full FSG sync(0) registration window: {17, 39, 23, 18, 49}.
     * On any failure during opening, executes a compensating BAP rollback so the cluster
     * is never left in a partial RGStatus=1 or rgiValid=true state.
     */
    private boolean openBapPresentation() {
        if (bapPresentationActive) return true;

        boolean success = false;
        try {
            forceClusterRouteInfoState(true);

            appConnectorNavi.updateRGStatus(1);
            appConnectorNavi.updateActiveRGType(ACTIVE_RGTYPE);

            // Required to complete the FSG sync(0) registration window:
            sendFollowStreet();
            sendDistanceToManeuverRaw(0, false, 0);
            sendExitView();

            success = true;
            bapPresentationActive = true;
            cancelKdkVerify();
            Log.i(TAG, "openBapPresentation succeeded (rgType=" + ACTIVE_RGTYPE + ")");
            return true;
        } catch (Throwable t) {
            Log.w(TAG, "openBapPresentation failed; executing compensating rollback: " + t);

            try { appConnectorNavi.updateRGStatus(0); } catch (Throwable ignored) {}
            try { appConnectorNavi.updateActiveRGType(0); } catch (Throwable ignored) {}
            try { sendNoSymbol(); } catch (Throwable ignored) {}
            try { sendDistanceToManeuverRaw(0, false, 0); } catch (Throwable ignored) {}
            try { sendExitView(); } catch (Throwable ignored) {}
            try { appConnectorNavi.updateManeuverState(BAP_MANEUVER_STATE_INACTIVE); } catch (Throwable ignored) {}
            try { forceClusterRouteInfoState(false); } catch (Throwable ignored) {}

            bapPresentationActive = false;
            return false;
        } finally {
            if (!success) {
                bapPresentationActive = false;
            }
        }
    }

    /**
     * Unconditional and individually best-effort withdrawal of the OEM BAP presentation.
     * Restores rgiValid=false and RGStatus(0) to close the speedometer KDK pill.
     * Returns true ONLY if every step succeeded without throwing.
     */
    private boolean closeBapPresentation() {
        boolean allOk = true;

        try { appConnectorNavi.updateRGStatus(0); }
        catch (Throwable t) { allOk = false; Log.w(TAG, "updateRGStatus(0) failed: " + t); }

        try { appConnectorNavi.updateActiveRGType(0); }
        catch (Throwable t) { allOk = false; Log.w(TAG, "updateActiveRGType(0) failed: " + t); }

        try { sendNoSymbol(); }
        catch (Throwable t) { allOk = false; }

        try { sendDistanceToManeuverRaw(0, false, 0); }
        catch (Throwable t) { allOk = false; }

        try { sendExitView(); }
        catch (Throwable t) { allOk = false; }

        try { appConnectorNavi.updateManeuverState(BAP_MANEUVER_STATE_INACTIVE); }
        catch (Throwable t) { allOk = false; }

        try { forceClusterRouteInfoState(false); }
        catch (Throwable t) { allOk = false; Log.w(TAG, "forceClusterRouteInfoState(false) failed: " + t); }

        bapPresentationActive = !allOk;
        if (allOk) {
            Log.i(TAG, "closeBapPresentation completed successfully");
        } else {
            Log.w(TAG, "closeBapPresentation incomplete; retaining bapPresentationActive=true");
        }
        return allOk;
    }


    /* ============================================================
     * REPLACE-mode takeover (connect-time, session-long)
     * ============================================================ */

    /** REPLACE: on CarPlay CONNECT (not just when CarPlay navigates), cancel any in-flight
     *  stock route guidance and shut the native RG BAP gate for the whole session, so the
     *  cluster never shows the stock navigator's RG.  Our own maneuvers ride appConnectorNavi
     *  (raw OSGi, never gated), so they are unaffected.  Idempotent.
     *  If ClusterService isn't up yet the gate install no-ops here; onStart() re-tries
     *  the lazy init when CarPlay nav begins — the connect-time CANCEL still fires via the
     *  Navigation singleton regardless. */
    /** @return true once the native RG gate is actually installed+shut; false if ClusterService
     *  isn't up yet so the caller (RgdModule) keeps retrying — otherwise a connected session with
     *  no CarPlay navigation would run with stock RG never blocked. */
    public boolean engageTakeover() {
        if (!initialized) return false;
        try {
            takeoverEngaged = true;
            if (csRef == null) initClusterAccess();
            if (!nativeStopAttempted) {
                nativeStopAttempted = tryStopNativeNavigation();
            }
            if (!nativeStopAttempted) return false;
            if (com.luka.carplay.core.ScreenNavStatusGate.setRouteGuidanceBlocked(true)) {
                Log.i(TAG, "REPLACE: takeover engaged (stock nav cancelled, RG gate shut)");
                return true;
            }
            return false;
        } catch (Throwable t) {
            Log.w(TAG, "engageTakeover failed: " + t);
            return false;
        }
    }

    /** REPLACE: on disconnect, reopen the native RG gate so stock nav works normally again. */
    public void disengageTakeover() {
        try {
            takeoverEngaged = false;
            /* Last chance to hand the container back its DSI value: onShutdown may never have
             * run (CarPlay yanked without a route ever starting), and a leaked forge outlives
             * the phone -- native route guidance then aborts until the head unit reboots. */
            forceClusterRouteInfoState(false);
            nativeStopAttempted = false;
            nativeStopWasRouteAbsent = false;
            nativeAbortIssued = false;
            nativeStopLogState = -1;
            com.luka.carplay.core.ScreenNavStatusGate.setRouteGuidanceBlocked(false);
            Log.i(TAG, "REPLACE: takeover disengaged (stock RG gate reopened)");
        } catch (Throwable t) { /* ignore */ }
    }

    /* ============================================================
     * Lifecycle
     * ============================================================ */

    public boolean onStart() {
        if (!initialized) return false;

        try {
            bapSessionStarted = false;
            clearPositionScroll();
            inApproachZone = false;
            latchedPositionText = "";
            routeTextPublished = false;
            infoPhase = 0;
            lastEtaSeconds = -1L;
            lastTimeRemainingSeconds = -1L;
            lastTimeRemainingSampleSeconds = -1L;
            lastDistanceToDestinationM = -1;
            lastFirstManeuverIdx = -1;
            lastFirstManeuverVer = -1;
            lastFirstRouteGeneration = -1L;
            lastProcessedRouteGeneration = -1L;
            routeRebindPending = false;
            resetActionBlinkState();
            synchronized (distanceToManeuverLock) {
                hasLastDistM = false;
                lastDistM = 0;
                lastBarOn = false;
                lastBar = 0;
            }
            /* Action blink thread starts/stops with approach zone enter/exit
             * (see update() near approachChanged) — not on session start.
             * Outside approach zone the thread does nothing useful, and its
             * 600 ms wakeups otherwise add scheduler pressure for the entire
             * session even when the bargraph isn't pulsing. */

            /*
             * Lazy-init cluster hooks (native stream gate).
             * Navigation singleton may not be available at init() time.
             */
            if (csRef == null) {
                initClusterAccess();
            }

            /*
             * Politely abort any in-flight native route-guidance session
             * BEFORE we slam the BAP gate shut.  Without this, native nav
             * stays stuck on its old route, iOS Maps detects external nav
             * still busy on the cluster, and we get the rapid STOP_LOCATION
             * cycle that makes both navs unusable.
             *
             * setRouteGuidanceAborted() alone only updates cluster UI state
             * — it does NOT command native nav (in AppStartATF) to abort.
             * For that we need to reach into Navigation singleton and call
             * a real cancelRoute / stopGuidance method, but the exact name
             * differs between firmware variants and we don't have a header.
             * Probe several canonical names via reflection — first one that
             * exists wins.
             */
            if (!nativeStopAttempted || nativeStopWasRouteAbsent) {
                nativeStopAttempted = tryStopNativeNavigation();
                if (!nativeStopAttempted) {
                    rollbackFailedStart();
                    return false;
                }
            }

            /* FctIDs 19/20 are shared with stock outside RGI. Claim them only for the
             * interval in which this bridge publishes CarPlay route text. */
            com.luka.carplay.core.ScreenNavStatusGate.setCurrentPositionInfoBlocked(true);
            /* Block native route-guidance BAP stream during CarPlay RG. */
            com.luka.carplay.core.ScreenNavStatusGate.setRouteGuidanceBlocked(true);

            /* Start renderer FIRST — maneuver_render owns its own managed
             * displayable 98 (dc[80]={98,...}); bringing it up before we set
             * rgActive/rgiValid avoids a one-frame KDK flicker. */
            startCustomRenderer();                 /* non-blocking; readiness edges drive completion */

            /* Open Audi BAP presentation and satisfy the initial FSG sync(0) registration window:
             * {17, 39, 23, 18, 49}. If BAP opening fails, rollback cleanly and return false. */
            if (!openBapPresentation()) {
                rollbackFailedStart();
                return false;
            }

            Log.i(TAG, "Started (rgType=" + ACTIVE_RGTYPE
                + ", cr=" + customRendererStarted + ")");
            bapSessionStarted = true;
            com.luka.carplay.core.ScreenModule.setRouteActive(true);
            com.luka.carplay.core.ScreenModule.setPresentationActive(false);
            com.luka.carplay.core.ScreenModule.setRgdActive(true);
            /* Keep the VC's empty "---" shell out while route text is pending.
             * Clear the separate FctID 20 layer; never synthesize a text arrow. */
            try {
                appConnectorNavi.updateTurnToInfo("", "");
                appConnectorNavi.updateCurrentPositionInfo(ROUTE_TEXT_PENDING);
            } catch (Throwable t) {
                Log.w(TAG, "BAP FctID 19/20 startup fallback failed: " + t);
            }
            return true;

        } catch (Throwable e) {
            bapSessionStarted = false;
            com.luka.carplay.core.ScreenModule.setRgdActive(false);
            clearPositionScroll();
            rollbackFailedStart();
            Log.e(TAG, "onStart error: " + e.getClass().getName() + ": " + e.getMessage());
            return false;
        }
    }

    /** Undo every externally visible stage of a partially completed onStart().  CarPlayApp and
     * RouteGuidance are allowed to retry, so leaving RGStatus=1, gfxAvailable, or a half-owned
     * renderer here would make the retry non-idempotent.  The session-long native-RG gate remains
     * shut; engageTakeover/disengageTakeover own that independently. */
    private void rollbackFailedStart() {
        cancelRerouteHold();
        clearPositionScroll();
        com.luka.carplay.core.ScreenModule.rollbackRouteLifecycle();
        com.luka.carplay.core.ScreenModule.setRgdActive(false);
        closeBapPresentation();
        try { appConnectorNavi.updateTurnToInfo("", ""); } catch (Throwable t) { }
        try { appConnectorNavi.updateCurrentPositionInfo(""); } catch (Throwable t) { }
        try { stopCustomRenderer(false); } catch (Throwable t) { }
        /* Release the rgActive overlay here, not inside stopCustomRenderer: a throw in the
         * renderer teardown must not leave shared HMI state forged. */
        forceClusterRouteInfoState(false);
        forceGfxAvailable(false);
        bapSessionStarted = false;
        rendererPrimed = false;
        crConsecutiveSendFailures = 0;
        lastFormattedTurnDistance = "";
        carPlayDistManStr = null;
        carPlayDistManUnits = -1;
        /* Release FctIDs 19/20 only after our cleanup transaction has completed,
         * so stock cannot overwrite route text in the middle of rollback. */
        com.luka.carplay.core.ScreenNavStatusGate.setCurrentPositionInfoBlocked(false);
        Log.w(TAG, "onStart rollback complete");
    }

    public void onStop() {
        if (!initialized) return;

        try {
            bapSessionStarted = false;
            clearPositionScroll();
            /* Lightweight stop — reset internal state only.
             * No BAP teardown, no renderer kill. iOS sends transient route_state=0
             * during maneuver transitions; full teardown causes HUD flicker + renderer
             * black screen. BAP teardown happens in onShutdown() on real disconnect. */
            cancelRerouteHold();
            cancelKdkVerify();
            stopActionBlinkThread();
            inApproachZone = false;
            closeBapPresentation();
            com.luka.carplay.core.ScreenModule.setPresentationActive(false);
            com.luka.carplay.core.ScreenModule.setRgdActive(false);
            latchedPositionText = "";
            lastFormattedTurnDistance = "";
            carPlayDistManStr = null;
            carPlayDistManUnits = -1;
            routeTextPublished = false;
            infoPhase = 0;
            lastFirstManeuverIdx = -1;
            lastFirstManeuverVer = -1;
            lastFirstRouteGeneration = -1L;
            lastProcessedRouteGeneration = -1L;
            routeRebindPending = false;
        } catch (Exception e) {
            Log.e(TAG, "onStop error", e);
        }
    }

    /**
     * Full shutdown — BAP teardown + renderer socket teardown.
     * Called on actual CarPlay disconnect or stop().
     */
    public void onShutdown() { shutdown(false); }

    /** Route end preserves the last surface through VC hide and reuses its connection.
     * Session shutdown always releases them, independently of module stop ordering. */
    public void onRouteEnd() { shutdown(true); }

    private void shutdown(boolean preserveSurface) {
        if (!initialized) return;

        try {
            bapSessionStarted = false;
            clearPositionScroll();
            /* Defensive: stop action blink (it's also stopped on approach
             * zone exit, but onShutdown can be called from non-approach
             * states too — e.g., disconnect mid-route). */
            cancelRerouteHold();
            cancelKdkVerify();
            stopActionBlinkThread();
            inApproachZone = false;
            if (preserveSurface) {
                com.luka.carplay.core.ScreenModule.setRouteActive(false);
            } else {
                com.luka.carplay.core.ScreenModule.rollbackRouteLifecycle();
            }
            com.luka.carplay.core.ScreenModule.setRgdActive(false);
            latchedPositionText = "";
            lastFormattedTurnDistance = "";
            carPlayDistManStr = null;
            carPlayDistManUnits = -1;
            routeTextPublished = false;
            infoPhase = 0;
            lastEtaSeconds = -1L;
            lastTimeRemainingSeconds = -1L;
            lastTimeRemainingSampleSeconds = -1L;
            lastDistanceToDestinationM = -1;

            /*
             * Guidance stop — full BAP teardown:
             * 1. RGStatus(0) - triggers sync(0) for {17,39,23,18,49}
             * 2. Complete sync(0) window: descriptor(23), distance(18), exitView(49)
             * 3. Non-sync FctIDs last
             * Best-effort: a BAP send throwing here must NOT skip the renderer/gate/context
             * cleanup below (else a mid-teardown exception leaves an orphan renderer or, in TAB,
             * a permanently shut RG gate). */
            try {
                closeBapPresentation();
                appConnectorNavi.updateTurnToInfo("", "");
                appConnectorNavi.updateCurrentPositionInfo("");
                sendDistanceToDestinationRaw(0, false);
                appConnectorNavi.updateTimeToDestination(0, 0, -1);
                appConnectorNavi.updateLaneGuidance(false, new CombiBAPNaviLaneGuidanceData[0]);
            } catch (Throwable t) {
                Log.w(TAG, "onShutdown: BAP teardown threw (continuing to cleanup): " + t);
            }
            /* CarPlay RGI no longer owns FctIDs 19/20. Keep the rest of native RG
             * gated until phone disconnect, but let stock update route text. */
            com.luka.carplay.core.ScreenNavStatusGate.setCurrentPositionInfoBlocked(false);

            stopCustomRenderer(preserveSurface);
            forceClusterRouteInfoState(false);
            /* CarPlay session ending — release the renderer listen socket
             * (port :19800).  stopCustomRenderer keeps it bound for fast
             * route restarts within a session; full session shutdown
             * actually closes it. */
            if (rendererClient != null && !preserveSurface) {
                rendererClient.setStateListener(null);
                rendererClient.dispose();
                rendererClient = null;
            }
            rendererPrimed = false;
            /* REPLACE: keep the native RG gate SHUT for the whole connected session.
             * This path also runs when navigation ends within a session, so without
             * this guard the gate would reopen mid-session and stock nav RG could reappear on
             * the cluster.  disengageTakeover() (on real disconnect) is the only reopen. */
            if (!takeoverEngaged)
                com.luka.carplay.core.ScreenNavStatusGate.setRouteGuidanceBlocked(false);

            /* The cluster RG-state override is always dropped above: forceClusterRouteInfoState
             * restores the value the DSI last reported, so a native route that really is
             * guiding keeps rgActive=true (its "Cancel Map Guidance" still reaches
             * RgStopGuidanceGeneralCommand) while an idle nav core gets its honest false back. */
            forceGfxAvailable(false);

            Log.i(TAG, "Shutdown (full teardown)");
        } catch (Exception e) {
            Log.e(TAG, "onShutdown error", e);
        }
    }

    /* ============================================================
     * Update
     * ============================================================ */

    public boolean update(RouteGuidance.State s) {
        synchronized (updateLock) {
            return updateLocked(s);
        }
    }

    private boolean updateLocked(RouteGuidance.State s) {
        if (!initialized || s == null) return false;

        int failureSerial = rendererSendFailureSerial;
        int dirty = s.dirtyMask;
        cacheTravelInfo(s, dirty);
        if (dirty == 0) return false;
        int crIconMask = RouteGuidance.State.DIRTY_MANEUVER_ICON
            | RouteGuidance.State.DIRTY_MANEUVER_LIST
            | RouteGuidance.State.DIRTY_MANEUVER_COUNT;
        if ((dirty & crIconMask) != 0) {
            synchronized (this) {
                // Neither this update nor the blink worker may paint the old
                // arrow with the next maneuver's progress before it is queued.
                rendererManeuverPending = true;
            }
        }

        try {
            /*
             * Explicit clear: count dropped to 0, or route is inactive/rerouting.
             */
            boolean isRerouting = (s.routeState == ROUTE_STATE_REROUTING)
                && (s.routeStateGeneration == s.routeGeneration);
            boolean explicitClear = (((dirty & RouteGuidance.State.DIRTY_MANEUVER_COUNT) != 0)
                && (s.maneuverCount == 0)
                && (s.routeState <= 0)) || isRerouting;
            int distM = s.distManeuverM;
            int[] idxs = getManeuverIndexList(s);
            boolean hasManeuverList = (idxs != null && idxs.length > 0);
            boolean hasAnyManeuver = (s.maneuverCount > 0);
            boolean shouldClearManeuver = ((s.maneuverCount == 0) && (s.routeState <= 0)) || isRerouting;

            int firstIdx = primaryManeuverIndex(s);
            int type0 = (firstIdx >= 0 && s.mType != null && firstIdx < s.mType.length) ? s.mType[firstIdx] : -1;
            boolean showManeuver = ManeuverMapper.isValidType(type0) && !isRerouting;

            if (isRerouting != diagRerouting) {
                diagRerouting = isRerouting;
                Log.i(TAG, "RGI-DIAG reroute " + (isRerouting ? "ENTER" : "EXIT")
                    + " gen=" + s.routeGeneration + " routeState=" + s.routeState
                    + " stateGen=" + s.routeStateGeneration
                    + " inZone=" + inApproachZone
                    + " presentation=" + com.luka.carplay.core.ScreenModule.isPresentationActive());
            }
            if (isRerouting) {
                lastFirstManeuverIdx = -1;
                lastFirstManeuverVer = -1;
                lastFirstRouteGeneration = -1;
            }

            /* Highway-aware prepare thresholds (meters).
             *
             * iOS sends `step.distance` (= s.mDistance[idx]) — the length of
             * the route segment between the previous maneuver and this one.
             * Long step (>2 km) = highway/limited-access road; short step
             * = city.  This catches `MT_KEEP_RIGHT` / `MT_LEFT_TURN` on
             * highways which the type-based check would miss.
             * Fallback: when step length unknown (route setup), fall back
             * to the maneuver-type heuristic. */
            boolean isHighway = (type0 >= 0) && ManeuverMapper.isHighwayManeuver(type0);
            int prepareThreshold  = isHighway ? HIGHWAY_PREPARE_THRESHOLD_M : CITY_PREPARE_THRESHOLD_M;
            int bargraphDenominatorM = getBargraphDenominatorM(s, firstIdx, prepareThreshold);
            updateActionBlinkContext(
                hasManeuverList && showManeuver && (bargraphDenominatorM > 0) && !shouldClearManeuver && !explicitClear,
                distM, bargraphDenominatorM);

            /*
             * Approach zone detection.
             *
             * Evaluates whether the vehicle is within the maneuver display threshold:
             * CITY_DISPLAY_DISTANCE_M (~1500 ft) for street turns,
             * HIGHWAY_DISPLAY_DISTANCE_M (~1.0 mi) for highway ramps/interchanges,
             * with HYSTERESIS_BUFFER_M (~160 ft) to prevent boundary flapping.
             */
            int currentFirstVer = (firstIdx >= 0 && s.mVer != null
                    && firstIdx < s.mVer.length) ? s.mVer[firstIdx] : -1;
            boolean routeGenerationChanged = (s.routeGeneration >= 0L)
                && (s.routeGeneration != lastProcessedRouteGeneration);
            if (routeGenerationChanged) {
                lastProcessedRouteGeneration = s.routeGeneration;
                routeRebindPending = routeRebindPending || com.luka.carplay.core.ScreenModule.isPresentationActive();
                if (!routeRebindPending) inApproachZone = false;
            }
            boolean primaryChanged = (firstIdx != lastFirstManeuverIdx)
                || (currentFirstVer != lastFirstManeuverVer)
                || (s.routeGeneration != lastFirstRouteGeneration);
            if (primaryChanged) {
                lastFirstManeuverIdx = firstIdx;
                lastFirstManeuverVer = currentFirstVer;
                lastFirstRouteGeneration = s.routeGeneration;
            }
            boolean hasUsableDistance = (distM >= 0);
            /* Treat arrival types as approach state even with distM==0 so their
             * maneuver-state transition remains Prepare rather than Follow. */
            boolean isArrival = (type0 == ManeuverMapper.MT_ARRIVE_END_OF_NAVIGATION
                || type0 == ManeuverMapper.MT_ARRIVE_AT_DESTINATION
                || type0 == ManeuverMapper.MT_ARRIVE_END_OF_DIRECTIONS
                || type0 == ManeuverMapper.MT_ARRIVE_DESTINATION_LEFT
                || type0 == ManeuverMapper.MT_ARRIVE_DESTINATION_RIGHT);
            int baseThresholdM = isHighway ? HIGHWAY_DISPLAY_DISTANCE_M : CITY_DISPLAY_DISTANCE_M;
            int effectiveThresholdM = inApproachZone ? (baseThresholdM + HYSTERESIS_BUFFER_M) : baseThresholdM;
            boolean inDisplayDistance = (!hasUsableDistance) || isArrival || (distM <= effectiveThresholdM);
            boolean nowApproach = hasManeuverList && showManeuver && !shouldClearManeuver && !explicitClear
                && (isArrival || (hasUsableDistance ? inDisplayDistance : inApproachZone));

            /* Reroute hold.  A reroute pulse inside the approach zone must not close the
             * presentation: closing and re-opening ~350 ms later left the VC KDK pill up while
             * we were back in stock ctx 74 (black pill).  Hold the presentation and the last
             * arrow for at most rerouteHoldMs; the BAPTimer thread expires it even if no further
             * RGI update arrives.  A genuine route end is never rerouting, so it closes at once. */
            boolean holding = false;
            boolean holdBegan = false;
            boolean holdWasActive;
            boolean presentationOpen = isRerouting && rerouteHoldMs > 0 && inApproachZone
                && (bapPresentationActive || com.luka.carplay.core.ScreenModule.isPresentationActive());
            synchronized (timerLock) {
                holdWasActive = rerouteHoldActive;
                long holdNow = System.currentTimeMillis();
                if (isRerouting) {
                    if (rerouteHoldActive) {
                        if (holdNow < rerouteHoldDeadline) {
                            holding = true;
                        } else {
                            /* expired: fall through to the normal approach-EXIT close */
                            rerouteHoldActive = false;
                            ++rerouteHoldGen;
                        }
                    } else if (presentationOpen) {
                        rerouteHoldActive = true;
                        rerouteHoldDeadline = holdNow + rerouteHoldMs;
                        ++rerouteHoldGen;
                        holding = true;
                        holdBegan = true;
                    }
                } else if (rerouteHoldActive) {
                    rerouteHoldActive = false;   /* route returned (or ended): hold is over */
                    ++rerouteHoldGen;
                }
            }
            if (holdBegan) {
                Log.i(TAG, "Reroute hold: keeping BAP presentation open for up to "
                    + rerouteHoldMs + " ms");
                ensureTimerThread();
            }
            if (holding) {
                /* Freeze the maneuver presentation: no clear, no close, no descriptor/distance/
                 * state writes.  The return tick force-refreshes everything. */
                nowApproach = inApproachZone;
                explicitClear = false;
                shouldClearManeuver = false;
                dirty &= ~(RouteGuidance.State.DIRTY_MANEUVER_ICON
                    | RouteGuidance.State.DIRTY_MANEUVER_LIST
                    | RouteGuidance.State.DIRTY_MANEUVER_COUNT
                    | RouteGuidance.State.DIRTY_MANEUVER_STATE
                    | RouteGuidance.State.DIRTY_DIST_MAN);
            }
            boolean approachChanged = (nowApproach != inApproachZone);
            if (approachChanged) {
                dirty |= RouteGuidance.State.DIRTY_DIST_MAN
                       | RouteGuidance.State.DIRTY_LANE_GUIDANCE
                       | RouteGuidance.State.DIRTY_MANEUVER_TEXT
                       | RouteGuidance.State.DIRTY_MANEUVER_ICON
                       | RouteGuidance.State.DIRTY_MANEUVER_STATE;
                inApproachZone = nowApproach;
                Log.i(TAG, "Approach zone " + (nowApproach ? "ENTER" : "EXIT")
                    + " (dist=" + distM + "m, highway=" + isHighway
                    + ", prepThr=" + prepareThreshold + ", barDen=" + bargraphDenominatorM + ")");

                /* Lazy-start action blink: only spin the 600 ms blink loop
                 * while we're actually in the approach zone, where it drives
                 * the BAP+renderer bargraph pulse in lockstep.  Outside the
                 * zone the loop does nothing useful but its wakeups still
                 * contend with the cluster compositor and TCP traffic. */
                if (nowApproach) {
                    startActionBlinkThread();
                } else {
                    stopActionBlinkThread();
                }
            }

            if (holdWasActive && !isRerouting && nowApproach) {
                /* Route came back inside the zone before the hold expired: presentation stayed
                 * open (no close/open happened), so refresh to the NEW maneuver explicitly. */
                dirty |= RouteGuidance.State.DIRTY_MANEUVER_ICON
                       | RouteGuidance.State.DIRTY_MANEUVER_LIST
                       | RouteGuidance.State.DIRTY_DIST_MAN
                       | RouteGuidance.State.DIRTY_MANEUVER_STATE
                       | RouteGuidance.State.DIRTY_LANE_GUIDANCE;
                invalidateRendererDedup();
                Log.i(TAG, "Reroute hold: route returned inside the zone; presentation kept open");
            }

            /*
             * A route-generation change only needs a forced physical rebind when
             * the NEW route is actually inside the display approach window and
             * ctx 80 is already logically active.
             *
             * If navActive is false, the normal setNavActive(true) path below
             * performs the 74 -> 80 acquisition.
             */
            if (routeRebindPending && !holding) {
                if (hasManeuverList && hasUsableDistance) {
                    if (nowApproach) {
                        com.luka.carplay.core.ScreenModule.requestClusterContextRebind(
                            "route-generation=" + s.routeGeneration + ", approach=true");
                    }
                    routeRebindPending = false;
                } else if (shouldClearManeuver || explicitClear) {
                    routeRebindPending = false;
                }
            }

            /* Suspend maneuver presentation when cruising (> 1500 ft) to close the KDK cutout
             * in the speedometer dial, while keeping the status bar route text and overall BAP route
             * active. Full BAP/RouteInfo teardown is reserved for onRouteEnd / onShutdown.
             * Dynamically reactivate when entering the approach zone (<= 1500 ft). */
            if (!nowApproach) {
                if (bapPresentationActive || com.luka.carplay.core.ScreenModule.isPresentationActive()) {
                    Log.i(TAG, "Approach zone EXIT: closing BAP presentation");
                    closeApproachPresentation("exit-close-ok", s.routeGeneration,
                        isRerouting, explicitClear, shouldClearManeuver);
                }
            } else {
                if (!com.luka.carplay.core.ScreenModule.isPresentationActive()) {
                    if (csRef != null) startCustomRenderer();
                    boolean rendererOk = (csRef == null)
                        || (customRendererStarted && rendererClient != null && rendererClient.isFrameReady());
                    if (rendererOk) {
                        Log.i(TAG, "Approach zone ENTER: opening BAP presentation");
                        if (openBapPresentation()) {
                            com.luka.carplay.core.ScreenModule.setPresentationActive(true);
                            dirty |= RouteGuidance.State.DIRTY_MANEUVER_ICON
                                   | RouteGuidance.State.DIRTY_DIST_MAN
                                   | RouteGuidance.State.DIRTY_MANEUVER_STATE
                                   | RouteGuidance.State.DIRTY_LANE_GUIDANCE;
                        } else {
                            Log.w(TAG, "Approach zone ENTER: BAP open failed; holding context switch");
                        }
                    } else {
                        Log.w(TAG, "Approach zone ENTER: renderer not frame-ready, holding context switch");
                    }
                } else if (csRef != null && (rendererClient == null || !rendererClient.isFrameReady())) {
                    Log.w(TAG, "Approach zone: renderer lost frame readiness; closing presentation");
                    if (closeBapPresentation()) {
                        com.luka.carplay.core.ScreenModule.setPresentationActive(false);
                        scheduleKdkVerify();
                    }
                }
            }

            /*
             * 1. Maneuver icons (FctID 23)
             */
            boolean descriptorSent = false;
            if ((dirty & (RouteGuidance.State.DIRTY_MANEUVER_ICON |
                          RouteGuidance.State.DIRTY_MANEUVER_LIST |
                          RouteGuidance.State.DIRTY_MANEUVER_COUNT)) != 0) {
                if (explicitClear || !nowApproach) {
                    sendNoSymbol();
                    descriptorSent = true;
                } else if (hasManeuverList) {
                    if (showManeuver) {
                        sendManeuvers(s);
                        descriptorSent = true;
                    } else if (shouldClearManeuver) {
                        sendNoSymbol();
                        descriptorSent = true;
                    } else if (hasAnyManeuver) {
                    }
                } else if (shouldClearManeuver) {
                    sendNoSymbol();
                    descriptorSent = true;
                } else if (hasAnyManeuver) {
                }
            }

            /*
             * 2. ExitView (FctID 49) - must change on every descriptor send so
             *    AppConnectorNavi's sendStatusIfChanged actually sends it,
             *    completing the FSG sync(1) window for {23,18,49}.
             */
            if (descriptorSent) {
                sendExitView();
            }

            /*
             * 3. Distance to maneuver (FctID 18)
             *
             * Distance is converted with native BAPDistanceFormatter rules.
             */
            if ((dirty & (RouteGuidance.State.DIRTY_DIST_MAN |
                          RouteGuidance.State.DIRTY_MANEUVER_ICON |
                          RouteGuidance.State.DIRTY_MANEUVER_LIST |
                          RouteGuidance.State.DIRTY_MANEUVER_COUNT)) != 0) {
                boolean transientNoDistance = (distM <= 0)
                    && !shouldClearManeuver
                    && !explicitClear
                    && hasManeuverList
                    && hasAnyManeuver;
                if (transientNoDistance) {
                    replayDistanceToManeuver();
                } else if (distM <= 0 || shouldClearManeuver) {
                    resetActionBlinkState();
                    sendDistanceToManeuverRaw(0, false, 0);
                } else {
                    boolean inAction = (bargraphDenominatorM > 0) && (distM <= bargraphDenominatorM);
                    boolean bargraphOn = false;
                    int bargraph = 0;
                    if (inAction) {
                        bargraphOn = true;
                        bargraph = bargraphPercent(distM, bargraphDenominatorM);
                        if (isInBlinkZone(distM, bargraphDenominatorM)) {
                            /* Blink zone: the 600 ms worker sends FctID 18 and
                             * maneuver_render together from one state snapshot. */
                        } else {
                            sendDistanceToManeuverRaw(distM, bargraphOn, bargraph);
                        }
                    } else {
                        /* Reset blink phase under `this` — the BAPActionBlink thread reads and
                         * toggles actionBlinkFull inside sendActionBlinkTick's synchronized(this).
                         * A bare write here (bus thread) races that toggle (lost update / stale
                         * read).  Keep the send OUTSIDE the lock to preserve this<distanceToManeuverLock. */
                        synchronized (this) {
                            actionBlinkFull = true;
                        }
                        sendDistanceToManeuverRaw(distM, bargraphOn, bargraph);
                    }
                }
            }

            /* 4. All route text lives in CurrentPositionInfo (FctID 19).
             * Street / signpost text only (FctID 18 handles maneuver distance).
             * Phase 1: compact ETA duration in smallscreen, arrival + duration
             * in fullscreen. FctID 20 stays empty in both phases.
             * Distance ticks MUST NOT dirty route text or restart scroll. */
            int routeTextDirty = RouteGuidance.State.DIRTY_CURRENT_ROAD
                | RouteGuidance.State.DIRTY_MANEUVER_TEXT
                | RouteGuidance.State.DIRTY_MANEUVER_LIST
                | RouteGuidance.State.DIRTY_MANEUVER_ICON
                | RouteGuidance.State.DIRTY_MANEUVER_COUNT;
            if (infoPhase != 0) {
                routeTextDirty |= RouteGuidance.State.DIRTY_DIST_DEST
                    | RouteGuidance.State.DIRTY_TIME_REMAINING
                    | RouteGuidance.State.DIRTY_ETA;
            } else if ((dirty & RouteGuidance.State.DIRTY_DIST_MAN) != 0) {
                String distStr = (firstIdx >= 0 && distM > 0) ? formatTurnDistanceForText(distM) : "";
                if (!distStr.equals(lastFormattedTurnDistance)) {
                    lastFormattedTurnDistance = distStr;
                    routeTextDirty |= RouteGuidance.State.DIRTY_DIST_MAN;
                }
            }
            if (!routeTextPublished || (dirty & routeTextDirty) != 0) {
                updateLatchedRouteText(s);
                publishRouteTextForMode();
            }

            /*
             * 5. Maneuver state (FctID 55) - for HUD
             */
            if ((dirty & (RouteGuidance.State.DIRTY_MANEUVER_STATE |
                          RouteGuidance.State.DIRTY_MANEUVER_ICON |
                          RouteGuidance.State.DIRTY_MANEUVER_LIST |
                          RouteGuidance.State.DIRTY_MANEUVER_COUNT |
                          RouteGuidance.State.DIRTY_DIST_MAN)) != 0) {
                if (explicitClear) {
                    appConnectorNavi.updateManeuverState(BAP_MANEUVER_STATE_INACTIVE);
                } else if (shouldClearManeuver || (hasManeuverList && showManeuver)) {
                    int bapState;
                    if (showManeuver && nowApproach) {
                        if (hasUsableDistance && bargraphDenominatorM > 0 && distM <= bargraphDenominatorM) {
                            bapState = BAP_MANEUVER_STATE_ACTION;
                        } else {
                            bapState = BAP_MANEUVER_STATE_PREPARE;
                        }
                    } else {
                        bapState = BAP_MANEUVER_STATE_INACTIVE;
                    }
                    appConnectorNavi.updateManeuverState(bapState);
                } else if (hasManeuverList && hasAnyManeuver) {
                    appConnectorNavi.updateManeuverState(BAP_MANEUVER_STATE_APPROACH);
                } else if (hasAnyManeuver && !hasManeuverList) {
                }
            }

            /*
              * 6. Lane guidance (FctID 24)
              */
            int laneRecomputeMask = RouteGuidance.State.DIRTY_LANE_GUIDANCE
                | RouteGuidance.State.DIRTY_ROUTE_STATE
                | RouteGuidance.State.DIRTY_MANEUVER_LIST
                | RouteGuidance.State.DIRTY_MANEUVER_COUNT;
            if ((dirty & laneRecomputeMask) != 0) {
                /* Trust iOS's display intent — `laneGuidanceShowing` is the
                 * authoritative signal.  iOS sets it to 1 only when its own
                 * navigation manager (MNGuidanceManager._considerLaneGuidance)
                 * has decided to show lanes for the user, based on the lane
                 * event's startValidRouteCoordinate / endValidRouteCoordinate
                 * range.  This naturally covers:
                 *   - highway pre-positioning (shows 1-2 km early)
                 *   - active lane choice (right at the maneuver)
                 *   - hide on exit (showing→0 once past endValidRouteCoordinate)
                 * No `nowApproach` gate — that would override iOS's range. */
                boolean wantLaneGuidance = laneGuidanceVisible(s);
                if (wantLaneGuidance) {
                    sendLaneGuidance(s);
                } else {
                    appConnectorNavi.updateLaneGuidance(false, new CombiBAPNaviLaneGuidanceData[0]);
                }
            }

            /* 7. Distance to destination (FctID 21) */
            if ((dirty & RouteGuidance.State.DIRTY_DIST_DEST) != 0) {
                sendDistanceToDestinationRaw(s.distDestM, false);
            }

            /* 8. Time to destination (FctID 22) always remains absolute ETA.
             * The click presentation is separate text in FctID 19 only. */
            if ((dirty & (RouteGuidance.State.DIRTY_TIME_REMAINING |
                          RouteGuidance.State.DIRTY_ETA)) != 0) {
                publishTimeToDestinationForMode();
            }

            /* 9. Destination info (FctID 46) - via AppConnectorNavi */
            if ((dirty & RouteGuidance.State.DIRTY_DESTINATION) != 0) {
                String dest = (s.destination != null) ? s.destination : "";
                dest = limitUtf8(dest, 50);
                CombiBAPNaviDestination naviDest =
                    new CombiBAPNaviDestination("", "", dest, "", "", "", "");
                CombiBAPDestinationInfo destInfo = new CombiBAPDestinationInfo(naviDest);
                appConnectorNavi.updateDestinationInfo(destInfo);
            }

            /* 10. maneuver_render: 3D maneuver graphic active during approach;
             * clear when cruising outside the approach threshold. */
            if (!customRendererStarted && csRef != null) startCustomRenderer();

            if (rendererClient != null && customRendererStarted) {
                if (!refreshRendererViewport()) noteRendererSendResult(false);
                /* Link-loss → noteRendererSendResult drops the view; the retry above reconnects. */

                // Lanes have their own active event and visibility, just as
                // HUD FctID 24. They never refresh or transition the maneuver.
                if (lastCrLaneGuidance == null || (dirty & laneRecomputeMask) != 0) {
                    LaneGuidanceSnapshot lanes = rendererLaneGuidance(s, laneGuidanceVisible(s));
                    if (!lanes.same(lastCrLaneGuidance)) {
                        boolean ok = rendererClient.sendLaneGuidance(lanes);
                        lastCrLaneGuidance = ok ? lanes : null;
                        noteRendererSendResult(ok);
                    }
                }
                /* Check if rendered maneuver actually changed */
                boolean iconChanged = false;
                if (nowApproach && showManeuver && hasManeuverList && !explicitClear && !shouldClearManeuver) {
                    if ((dirty & crIconMask) != 0) {
                        iconChanged = updateRendererIfChanged(s, bargraphDenominatorM);
                    }
                    if (!iconChanged && (approachChanged || (dirty &
                            (crIconMask | RouteGuidance.State.DIRTY_DIST_MAN)) != 0)) {
                        updateRendererProgress(s, bargraphDenominatorM);
                    }
                    /* Diagnostic: presentation open + valid maneuver but the renderer is still
                     * blank after a CLEAR. Logged once per clear (black-pill signature). */
                    if (diagClearedAwaitingManeuver && !diagBlankWarned
                            && com.luka.carplay.core.ScreenModule.isPresentationActive()) {
                        diagBlankWarned = true;
                        Log.w(TAG, "RGI-DIAG pill open but renderer BLANK after CLEAR: gen=" + s.routeGeneration
                            + " dirty=0x" + Integer.toHexString(dirty) + " approachChanged=" + approachChanged
                            + " inZone=" + inApproachZone + " lastCrIdx=" + lastCrIdx);
                    }
                } else if (!holding && (!nowApproach || explicitClear || shouldClearManeuver)) {
                    if (lastCrIdx != -1 || lastCrIcon != -1) {
                        rendererClient.sendClear();
                        lastCrIdx = -1;
                        lastCrIcon = -1;
                        diagClearedAwaitingManeuver = true;
                        Log.i(TAG, "RGI-DIAG renderer CLEAR site=tail gen=" + s.routeGeneration
                            + " approach=" + nowApproach + " rerouting=" + isRerouting
                            + " explicit=" + explicitClear + " shouldClear=" + shouldClearManeuver
                            + " presentation=" + com.luka.carplay.core.ScreenModule.isPresentationActive()
                            + " bapPresentation=" + bapPresentationActive);
                    }
                    /* RC#1: clear the pending flag so the next approach-ENTER cycle is
                     * not permanently blocked on a flag that was set during this reroute/clear. */
                    synchronized (this) { rendererManeuverPending = false; }
                }
            }

            // Enqueue failure is not a successful publication, even when a
            // later progress/lane command succeeded. Replay one shared snapshot.
            return rendererSendFailureSerial == failureSerial;

        } catch (Exception e) {
            Log.e(TAG, "update error", e);
            return false;
        }
    }

    /**
     * Authoritative permission for RouteGuidance to expose context 80:
     *  - the BAP startup transaction completed without throwing;
     *  - maneuver_render is the active custom renderer pipeline;
     *  - the current TCP peer acknowledged at least one eglSwapBuffers frame.
     */
    public boolean isPresentationReady() {
        RendererServer r = rendererClient;
        return bapSessionStarted && customRendererStarted && r != null && r.isFrameReady();
    }

    /**
     * Immediate View/OK refresh, executed by RouteGuidance's serialized worker.
     * State and the explicit output caches survive ordinary delta clearing, so
     * this never waits for the next iOS route-guidance packet.
     */
    public boolean refreshInfoPresentation(RouteGuidance.State s, int phase) {
        infoPhase = (phase == 0) ? 0 : 1;
        positionRestart = true;
        if (!initialized || !bapSessionStarted || s == null) return false;
        try {
            cacheTravelInfo(s, s.dirtyMask);
            updateLatchedRouteText(s);
            publishRouteTextForMode();
            publishTimeToDestinationForMode();
            Log.i(TAG, "route-info phase=" + infoPhase + " applied view="
                + (com.luka.carplay.core.ScreenModule.isSmallScreenViewArea() ? "smallscreen" : "fullscreen"));
            return true;
        } catch (Throwable t) {
            Log.w(TAG, "route-info refresh failed: " + t);
            return false;
        }
    }

    /** Refresh only real route-text caches; never store the phase-1 summary here. */
    private void updateLatchedRouteText(RouteGuidance.State s) {
        int idx = getFirstManeuverIndex(s);
        int version = idx >= 0 && s.mVer != null && idx < s.mVer.length ? s.mVer[idx] : -1;
        if (positionManeuverIndex != idx || positionManeuverVersion != version
                || positionRouteGeneration != s.routeGeneration) positionRestart = true;
        positionManeuverIndex = idx;
        positionManeuverVersion = version;
        positionRouteGeneration = s.routeGeneration;
        String turnTo = "";
        String signPost = "";
        if (idx >= 0) {
            if (s.mAfterRoad != null && idx < s.mAfterRoad.length) {
                turnTo = normalizeRouteText(keepLastColonPart(s.mAfterRoad[idx]));
            }
            if (turnTo.length() == 0 && s.mName != null && idx < s.mName.length) {
                turnTo = normalizeRouteText(s.mName[idx]);
            }
            if (s.mExitInfo != null && idx < s.mExitInfo.length) {
                signPost = normalizeRouteText(s.mExitInfo[idx]);
            }
        }

        if (positionRestart) {
            lastFormattedTurnDistance = "";
        }
        String distStr = (idx >= 0 && s.distManeuverM > 0) ? formatTurnDistanceForText(s.distManeuverM) : "";
        lastFormattedTurnDistance = distStr;

        /* Preserve the full payload. Street / signpost name + distance; distance is from shared formatter.
         * When a distance is present, the " | <dist>" suffix (and any arrow prefix) are pinned and the
         * name is shortened at a grapheme boundary with an ellipsis so the whole line fits in one page
         * by CurrentPositionScroll's own width/byte measure -- it must never trigger multi-page scroll. */
        positionPrefix = positionSuffix = "";
        if (signPost.length() > 0) {
            if (distStr.length() > 0) {
                /* Same turn-arrow prefix the turnTo branch has always used, applied here
                 * too (previously this branch set no prefix at all). Gated on distStr so
                 * a signpost with no distance to show keeps its historical plain-text form. */
                positionPrefix = getTurnArrowPrefix(s, idx);
                String suffix = " | " + distStr;
                String name = CurrentPositionScroll.fitWithEllipsis(signPost, positionPrefix, suffix);
                latchedPositionText = name + suffix;
            } else {
                latchedPositionText = signPost;
            }
        } else if (turnTo.length() > 0) {
            positionPrefix = getTurnArrowPrefix(s, idx);
            if (distStr.length() > 0) {
                String suffix = " | " + distStr;
                String name = CurrentPositionScroll.fitWithEllipsis(turnTo, positionPrefix, suffix);
                latchedPositionText = name + suffix;
            } else {
                latchedPositionText = turnTo;
            }
        } else {
            String road = normalizeRouteText(s.currentRoad);
            if (distStr.length() > 0 && road.length() > 0) {
                String suffix = " | " + distStr;
                String name = CurrentPositionScroll.fitWithEllipsis(road, "", suffix);
                latchedPositionText = name + suffix;
            } else {
                latchedPositionText = road;
            }
        }
    }

    private String getTurnArrowPrefix(RouteGuidance.State s, int idx) {
        if (s == null || idx < 0) return ARROW_STRAIGHT;
        try {
            int[] mapped = mapManeuver(s, idx);
            int dir = mapped[1];
            switch (dir) {
                case ManeuverMapper.DIR_SLIGHT_LEFT:
                    return ARROW_SLIGHT_LEFT;
                case ManeuverMapper.DIR_LEFT:
                case ManeuverMapper.DIR_SHARP_LEFT:
                case ManeuverMapper.DIR_UTURN:
                    return ARROW_LEFT;
                case ManeuverMapper.DIR_SLIGHT_RIGHT:
                    return ARROW_SLIGHT_RIGHT;
                case ManeuverMapper.DIR_RIGHT:
                case ManeuverMapper.DIR_SHARP_RIGHT:
                    return ARROW_RIGHT;
                case ManeuverMapper.DIR_STRAIGHT:
                default:
                    return ARROW_STRAIGHT;
            }
        } catch (Throwable t) {
            return ARROW_STRAIGHT;
        }
    }


    private void publishRouteTextForMode() {
        boolean smallScreen = com.luka.carplay.core.ScreenModule.isSmallScreenViewArea();
        String text = infoPhase == 0 ? "" : (smallScreen ? buildShortSummary() : buildTripSummary());
        String before = "", after = "";
        if (text.length() == 0) {
            text = latchedPositionText;
            before = positionPrefix;
            after = positionSuffix;
        } else if (text.startsWith(ROUTE_TIME_PREFIX)) {
            before = ROUTE_TIME_PREFIX;
            text = text.substring(before.length());
        }
        if (text.length() == 0) text = ROUTE_TEXT_PENDING;
        boolean changed = positionScroll.configure(text, before, after, positionRestart);
        positionScrollChanged |= changed;
        positionRestart = false;
        if (changed && (positionScroll.missingGlyphs || positionScroll.isFallback())) {
            Log.w(TAG, "CurrentPosition font coverage=" + !positionScroll.missingGlyphs
                + " oversized-cluster=" + positionScroll.isFallback());
        }
        /* Keep the stock text gate for FctID 20. Timer ticks only write FctID 19. */
        String frame = positionScroll.current();
        boolean delivered = false;
        try {
            appConnectorNavi.updateTurnToInfo("", "");
            appConnectorNavi.updateCurrentPositionInfo(frame);
            delivered = true;
        } finally {
            if (!delivered) positionScroll.failed(System.currentTimeMillis());
        }
        lastPositionSent = frame;
        positionScroll.sent(System.currentTimeMillis());
        positionSendFailed = false;
        routeTextPublished = true;
    }

    private void clearPositionScroll() {
        positionScroll.clear();
        lastPositionSent = null;
        positionRestart = true;
        positionSendFailed = false;
        positionScrollChanged = true;
        positionManeuverIndex = positionManeuverVersion = -1;
        positionRouteGeneration = -1L;
        positionPrefix = positionSuffix = "";
    }

    /** Called under RouteGuidance's monitor, outside presentationLock. */
    void suspendPositionScroll() {
        positionRestart = true;
        routeTextPublished = false;
        lastPositionSent = null;
    }

    long positionScrollWait(long now) {
        return positionRestart || !bapSessionStarted || !routeTextPublished ? -1L : positionScroll.waitMillis(now);
    }

    boolean takePositionScrollChange() {
        boolean changed = positionScrollChanged;
        positionScrollChanged = false;
        return changed;
    }

    /** No FctID 20/22, renderer frame, icon, or BAP sync transaction here. */
    void tickPositionScroll(long now) {
        if (positionRestart || !bapSessionStarted || !routeTextPublished) return;
        String frame = positionScroll.next(now);
        if (frame == null) return;
        try {
            if (!frame.equals(lastPositionSent)) appConnectorNavi.updateCurrentPositionInfo(frame);
            lastPositionSent = frame;
            positionScroll.sent(now);
            positionSendFailed = false;
        } catch (Throwable t) {
            positionScroll.failed(now);
            if (!positionSendFailed) Log.w(TAG, "CurrentPosition scroll retry: " + t);
            positionSendFailed = true;
        }
    }

    private String buildTripSummary() {
        String arrival = formatArrivalForText(currentArrivalSeconds());
        String remaining = formatRemainingForText(currentRemainingSeconds());
        if (arrival.length() == 0) return remaining;
        return remaining.length() == 0 ? arrival : arrival + " | " + remaining;
    }

    private String buildShortSummary() {
        String remaining = formatRemainingForText(currentRemainingSeconds());
        if (remaining.length() > 0) return ROUTE_TIME_PREFIX + remaining;
        return formatArrivalForText(currentArrivalSeconds());
    }

    private void cacheTravelInfo(RouteGuidance.State s, int dirty) {
        if (s == null) return;
        if ((dirty & RouteGuidance.State.DIRTY_ETA) != 0) {
            lastEtaSeconds = s.etaSeconds;
        }
        if ((dirty & RouteGuidance.State.DIRTY_TIME_REMAINING) != 0) {
            // Preserve source-sample age across retries, reconnect and info-mode
            // refresh. Directly constructed/legacy states acquire a timestamp once.
            if (s.timeRemainingSeconds >= 0L && s.timeRemainingSampleSeconds < 0L)
                s.timeRemainingSampleSeconds = getUtcMillis() / 1000L;
            lastTimeRemainingSeconds = s.timeRemainingSeconds;
            lastTimeRemainingSampleSeconds = s.timeRemainingSampleSeconds;
        }
        if ((dirty & RouteGuidance.State.DIRTY_DIST_DEST) != 0) {
            lastDistanceToDestinationM = s.distDestM;
        }
        if ((dirty & RouteGuidance.State.DIRTY_DIST_MAN) != 0) {
            carPlayDistManStr = s.distManeuverCarPlayStr;
            carPlayDistManUnits = s.distManeuverCarPlayUnits;
        }
    }

    private long currentRemainingSeconds() {
        long now = getUtcMillis() / 1000L;
        if (lastEtaSeconds >= 0L) {
            long remaining = lastEtaSeconds - now;
            return remaining > 0L ? remaining : 0L;
        }
        if (lastTimeRemainingSeconds < 0L) return -1L;
        long elapsed = lastTimeRemainingSampleSeconds >= 0L
            ? now - lastTimeRemainingSampleSeconds : 0L;
        if (elapsed < 0L) elapsed = 0L;
        long remaining = lastTimeRemainingSeconds - elapsed;
        return remaining > 0L ? remaining : 0L;
    }

    private long currentArrivalSeconds() {
        if (lastEtaSeconds >= 0L) return lastEtaSeconds;
        long remaining = currentRemainingSeconds();
        return remaining >= 0L ? (getUtcMillis() / 1000L) + remaining : -1L;
    }

    private void publishTimeToDestinationForMode() {
        int timeFormat = getHuNavigationTimeFormat();
        /* ALWAYS type 1, the absolute arrival clock.  The cluster has no widget for
         * timeInfoType 0: its whole distance-to-destination block in the VC is
         * NavFPK_DTD_{ArrivalTime,ArrivalTime_visible,Distance,Distance_Unit,Distance_visible}
         * - there is no remaining/duration field anywhere in gtf2.  Sending type 0 therefore
         * does not switch the display, it blanks it (ArrivalTime_visible goes false), which is
         * exactly what the smallscreen OK toggle used to do.  See
         * docs/cluster/kdk-geometry.md. */
        long timeVal = currentArrivalSeconds();
        /* JVM default TZ is UTC on MHI2Q. AppConnectorNavi converts a type-1
         * epoch with GregorianCalendar, so shift it to HU local time first. */
        if (timeVal >= 0L) {
            timeVal = convertUtcToLocalMs(timeVal * 1000L) / 1000L;
        }
        appConnectorNavi.updateTimeToDestination(1, timeFormat, timeVal);
    }

    private static String formatArrivalForText(long utcSeconds) {
        if (utcSeconds < 0L) return "";
        long localMs = convertUtcToLocalMs(utcSeconds * 1000L);
        java.util.GregorianCalendar cal = new java.util.GregorianCalendar();
        cal.setTimeInMillis(localMs);
        int hour = cal.get(java.util.Calendar.HOUR_OF_DAY);
        int minute = cal.get(java.util.Calendar.MINUTE);
        StringBuffer out = new StringBuffer(ROUTE_TIME_PREFIX);
        if (getHuNavigationTimeFormat() == 1) {
            boolean pm = hour >= 12;
            int hour12 = hour % 12;
            if (hour12 == 0) hour12 = 12;
            out.append(hour12).append(':');
            if (minute < 10) out.append('0');
            out.append(minute).append(pm ? " PM" : " AM");
        } else {
            if (hour < 10) out.append('0');
            out.append(hour).append(':');
            if (minute < 10) out.append('0');
            out.append(minute);
        }
        return out.toString();
    }

    /** Past an hour, plain minutes stop being readable at a glance -- a long trip showed
     *  "905 min". Split into hours and minutes, keeping the bare "45 min" form below the hour
     *  so short trips read exactly as before. StringBuffer, not String.format: this runs on
     *  Foundation Profile 1.1. */
    private static String formatRemainingForText(long seconds) {
        if (seconds < 0L) return "";
        long minutes = (seconds + 59L) / 60L;
        if (minutes < 60L) return String.valueOf(minutes) + " min";
        long rest = minutes % 60L;
        StringBuffer out = new StringBuffer();
        out.append(minutes / 60L).append(" h ");
        if (rest < 10L) out.append('0');
        out.append(rest).append(" min");
        return out.toString();
    }

    /** Advance the renderer handshake without waiting. Called by the presentation worker. */
    public boolean preparePresentation() {
        startCustomRenderer();
        return isPresentationReady();
    }

    /* ============================================================
     * Maneuver Sending (current maneuver only for VC/HUD)
     * ============================================================ */

    /**
     * Send FOLLOW_STREET descriptor.
     */
    private void sendFollowStreet() throws Exception {
        CombiBAPNaviManeuverDescriptor[] arr = new CombiBAPNaviManeuverDescriptor[1];
        arr[0] = createDescriptor(ManeuverMapper.FOLLOW_STREET, ManeuverMapper.DIR_STRAIGHT, 0, new byte[0]);
        appConnectorNavi.updateManeuverDescriptor(arr);
    }

    /**
     * Send NO_SYMBOL descriptor.
     */
    private void sendNoSymbol() throws Exception {
        CombiBAPNaviManeuverDescriptor[] arr = new CombiBAPNaviManeuverDescriptor[1];
        arr[0] = createDescriptor(0, 0, 0, new byte[0]);
        appConnectorNavi.updateManeuverDescriptor(arr);
    }

    /**
     * Send only maneuverOrder[0]. Wait if that slot is not yet valid; never skip ahead.
     * BAP supports three slots, but look-ahead would combine two instructions.
     * AppConnectorNavi resets absent Maneuver_2/Maneuver_3 slots to NO_SYMBOL.
     */
    private void sendManeuvers(RouteGuidance.State s) throws Exception {
        int[] idxs = getManeuverIndexList(s);
        if (idxs == null || idxs.length == 0) {
            if (s.maneuverCount == 0) sendNoSymbol();
            return;
        }

        int idx = primaryManeuverIndex(s);
        if (idx < 0) return; // wait for the current slot; never borrow the next turn's distance
        int[] mapped = mapManeuver(s, idx);
        int z = s.mZLevel != null && idx < s.mZLevel.length ? s.mZLevel[idx] : 0;
        CombiBAPNaviManeuverDescriptor[] arr = new CombiBAPNaviManeuverDescriptor[] {
            createDescriptor(mapped[0], mapped[1], z, maneuverSideStreets(s, idx, mapped[0]))
        };

        appConnectorNavi.updateManeuverDescriptor(arr);
    }

    /* ============================================================
     * Lane Guidance (FctID 24)
     * ============================================================ */

    private static boolean laneGuidanceVisible(RouteGuidance.State s) {
        return s.laneGuidanceShowing == 1 && !(s.maneuverCount == 0 && s.routeState <= 0);
    }

    private static LaneGuidanceSnapshot rendererLaneGuidance(RouteGuidance.State s, boolean showing) {
        if (!showing) return LaneGuidanceSnapshot.HIDDEN;
        int slot = resolveLaneGuidanceManeuverIndex(s); // Same selection as HUD.
        boolean complete = slot >= 0 && slot < s.lgLaneComplete.length
            && lgSlotMatches(s, slot, s.laneGuidanceIndex) && s.lgLaneComplete[slot] == 1;
        return LaneGuidanceSnapshot.copy(s.laneGuidanceIndex, true, complete,
            laneCountForManeuver(s, slot), lanePositionsFor(s, slot), laneDirectionsFor(s, slot),
            laneStatusFor(s, slot), laneAnglesFor(s, slot));
    }

    private void sendLaneGuidance(RouteGuidance.State s) throws Exception {
        int idx = resolveLaneGuidanceManeuverIndex(s);
        int count = laneCountForManeuver(s, idx);
        boolean hasRealData = hasLaneGuidanceForManeuver(s, idx);

        if (hasRealData) {
            sendRealLaneGuidance(s, idx, count);
        } else {
            appConnectorNavi.updateLaneGuidance(false, new CombiBAPNaviLaneGuidanceData[0]);
        }
    }

    private void sendRealLaneGuidance(RouteGuidance.State s, int idx, int count) throws Exception {
        int n = Math.min(count, 8);
        CombiBAPNaviLaneGuidanceData[] tmp = new CombiBAPNaviLaneGuidanceData[n];
        int out = 0;

        for (int i = 0; i < n; i++) {
            short lanePos = mapLanePosition(s, idx, i);
            short laneDir = mapLaneDirection(s, idx, i);
            if (!shouldEmitLane(s, idx, i, laneDir)) {
                continue;
            }
            byte[] laneSideStreets = mapLaneSideStreets(s, idx, i, laneDir);
            byte gi = mapGuidanceInfo(s, idx, i);
            tmp[out++] = new CombiBAPNaviLaneGuidanceData(
                lanePos, laneDir, laneSideStreets, (short) 0,
                (byte) 0, (byte) 0, (byte) 0, gi);
        }

        if (out <= 0) {
            appConnectorNavi.updateLaneGuidance(false, new CombiBAPNaviLaneGuidanceData[0]);
            return;
        }

        CombiBAPNaviLaneGuidanceData[] arr = tmp;
        if (out != n) {
            arr = new CombiBAPNaviLaneGuidanceData[out];
            for (int i = 0; i < out; i++) arr[i] = tmp[i];
        }

        appConnectorNavi.updateLaneGuidance(true, arr);
    }

    private static boolean hasLaneGuidanceForManeuver(RouteGuidance.State s, int manIdx) {
        if (manIdx < 0) return false;
        if (hasLaneCacheForSlot(s, manIdx)) return true;
        if (s.mLaneDirections == null || manIdx >= s.mLaneDirections.length) return false;
        if (s.mLaneDirections[manIdx] == null) return false;
        return laneCountForManeuver(s, manIdx) > 0;
    }

    private static boolean hasLaneCacheForSlot(RouteGuidance.State s, int slot) {
        if (slot < 0) return false;
        return laneCacheCountForSlot(s, slot) > 0;
    }

    /* Verify lg-cache slot's stored event id still matches the active idx.
     * Needed because lg cache slots are LRU-allocated independently of
     * maneuver slot numbers, so a slot can be remapped to a different
     * event after eviction. */
    private static boolean lgSlotMatches(RouteGuidance.State s, int slot, int idx) {
        if (slot < 0 || s.lgIndex == null || slot >= s.lgIndex.length) return false;
        return s.lgIndex[slot] == idx;
    }

    /* m-cache-only check (skips lg-cache).  Used in legacy fallback paths
     * where we need m-cache data without aliasing into lg-cache that
     * happens to occupy the same numeric slot. */
    private static boolean hasMCacheLaneForSlot(RouteGuidance.State s, int slot) {
        if (slot < 0) return false;
        if (s.mLaneDirections == null || slot >= s.mLaneDirections.length) return false;
        if (s.mLaneDirections[slot] == null) return false;
        int count = -1;
        if (s.mLaneCount != null && slot < s.mLaneCount.length) count = s.mLaneCount[slot];
        if (count < 0) count = s.mLaneDirections[slot].length;
        return count > 0;
    }

    private static int laneCacheCountForSlot(RouteGuidance.State s, int slot) {
        if (slot < 0) return 0;
        int count = 0;
        if (s.lgLaneCount != null && slot < s.lgLaneCount.length) {
            count = s.lgLaneCount[slot];
            if (count >= 0) return count; // Explicit zero clears; old arrays are not a fallback.
        }
        if (count <= 0 && s.lgLaneDirections != null && slot < s.lgLaneDirections.length
            && s.lgLaneDirections[slot] != null) {
            count = s.lgLaneDirections[slot].length;
        }
        if (count <= 0 && s.lgLaneStatus != null && slot < s.lgLaneStatus.length
            && s.lgLaneStatus[slot] != null) {
            count = s.lgLaneStatus[slot].length;
        }
        return Math.max(count, 0);
    }

    private static int laneSlotForGuidanceIndex(RouteGuidance.State s, int guidanceIndex) {
        if (guidanceIndex < 0 || s.lgIndex == null) return -1;
        for (int i = 0; i < s.lgIndex.length; i++) {
            if (s.lgIndex[i] == guidanceIndex && hasLaneCacheForSlot(s, i)) {
                return i;
            }
        }
        return -1;
    }

    /* Slot ids are shared by the legacy m-cache fallback and the lg-cache
     * arrays.  Keep both caches in the same bounded numeric slot range. */
    private static int[] lanePositionsFor(RouteGuidance.State s, int slot) {
        if (hasLaneCacheForSlot(s, slot) && s.lgLanePositions != null && slot < s.lgLanePositions.length)
            return s.lgLanePositions[slot];
        if (s.mLanePositions != null && slot >= 0 && slot < s.mLanePositions.length)
            return s.mLanePositions[slot];
        return null;
    }

    private static int[] laneDirectionsFor(RouteGuidance.State s, int slot) {
        if (hasLaneCacheForSlot(s, slot) && s.lgLaneDirections != null && slot < s.lgLaneDirections.length)
            return s.lgLaneDirections[slot];
        if (s.mLaneDirections != null && slot >= 0 && slot < s.mLaneDirections.length)
            return s.mLaneDirections[slot];
        return null;
    }

    private static int[] laneStatusFor(RouteGuidance.State s, int slot) {
        if (hasLaneCacheForSlot(s, slot) && s.lgLaneStatus != null && slot < s.lgLaneStatus.length)
            return s.lgLaneStatus[slot];
        if (s.mLaneStatus != null && slot >= 0 && slot < s.mLaneStatus.length)
            return s.mLaneStatus[slot];
        return null;
    }

    private static int[][] laneAnglesFor(RouteGuidance.State s, int slot) {
        if (hasLaneCacheForSlot(s, slot) && s.lgLaneAngles != null && slot < s.lgLaneAngles.length)
            return s.lgLaneAngles[slot];
        if (s.mLaneAngles != null && slot >= 0 && slot < s.mLaneAngles.length)
            return s.mLaneAngles[slot];
        return null;
    }

    private static int laneCountForManeuver(RouteGuidance.State s, int manIdx) {
        if (manIdx < 0) return 0;
        if (hasLaneCacheForSlot(s, manIdx)) {
            return laneCacheCountForSlot(s, manIdx);
        }
        int count = 0;
        if (s.mLaneCount != null && manIdx < s.mLaneCount.length) {
            count = s.mLaneCount[manIdx];
            if (count >= 0) return count;
        }
        if (count <= 0 && s.mLaneDirections != null && manIdx < s.mLaneDirections.length
            && s.mLaneDirections[manIdx] != null) {
            count = s.mLaneDirections[manIdx].length;
        }
        if (count <= 0 && s.mLaneStatus != null && manIdx < s.mLaneStatus.length
            && s.mLaneStatus[manIdx] != null) {
            count = s.mLaneStatus[manIdx].length;
        }
        if (count < 0) count = 0;
        return count;
    }

    private static int resolveLaneGuidanceManeuverIndex(RouteGuidance.State s) {
        /*
         * iOS exposes the currently displayed lane guidance as a top-level
         * currentLaneGuidanceIndex (0x5201 InfoType 16 — written by Maps.app
         * in setCurrentLaneGuidanceIndex: from CarMetadataNavigationListener
         * each location update).  0x5204's TLV1 is iOS's
         * composedGuidanceEventIndex — an EVENT IDENTIFIER, sent for every
         * cached event including future precache.  So the active is purely
         * 0x5201, and we must look up the lg-cache slot whose stored event
         * id matches the active.
         *
         * lg cache slot indices are NOT the same numeric space as maneuver
         * slot indices: in C, rgd_lane_slot_for_iap_index() maintains an
         * independent LRU.  After eviction, lg slot N may hold any cached
         * event id, not necessarily event N — which means resolving by
         * "primary maneuver slot" via lg-cache silently sends data for
         * the wrong event.  Validate s.lgIndex[slot] == active before
         * trusting any lg-cache slot.
         */

        int activeIdx = s.laneGuidanceIndex;
        if (activeIdx < 0) return -1;

        if (s.laneGuidanceSlot >= 0 && hasLaneCacheForSlot(s, s.laneGuidanceSlot)
            && lgSlotMatches(s, s.laneGuidanceSlot, activeIdx)) {
            return s.laneGuidanceSlot;
        }

        int byIndex = laneSlotForGuidanceIndex(s, activeIdx);
        if (byIndex >= 0) return byIndex;

        /*
         * Linked-lane fallback: C publishes mN_linked_lane_guidance_slot
         * pointing at a real lg-cache slot (not a maneuver slot), so it's
         * safe to consult directly.  Only use it when its stored event id
         * still matches the active (the linked field can outlive a cache
         * remap of its target).
         */
        int primary = getFirstManeuverIndex(s);
        int linked = linkedLaneSlotForManeuver(s, primary);
        if (linked >= 0 && hasLaneCacheForSlot(s, linked)
            && lgSlotMatches(s, linked, activeIdx)) {
            return linked;
        }

        /*
         * Legacy m-cache fallback for compatibility with pre-lg-split
         * hooks (which wrote lane data into mN_lane_*).  Skip if the
         * primary slot has any lg-cache data — that lg entry could be
         * unrelated event data (post-eviction) and m-cache lookup with
         * the same numeric slot would alias into it.
         */
        // Once the independent lg cache is present, a missing/empty active
        // event must stay empty. Never substitute a maneuver's old lane data.
        if (s.laneGuidanceSlot >= 0) return -1;
        if (s.lgIndex != null) for (int i = 0; i < s.lgIndex.length; i++) {
            if (s.lgIndex[i] >= 0) return -1;
        }
        int max = (s.mLaneCount != null) ? s.mLaneCount.length : 0;
        if (activeIdx < max && hasMCacheLaneForSlot(s, activeIdx)
            && !hasLaneCacheForSlot(s, activeIdx)) {
            return activeIdx;
        }
        if (primary >= 0 && s.mLinkedLaneGuidanceIndex != null
            && primary < s.mLinkedLaneGuidanceIndex.length
            && s.mLinkedLaneGuidanceIndex[primary] == activeIdx
            && hasMCacheLaneForSlot(s, primary)
            && !hasLaneCacheForSlot(s, primary)) {
            return primary;
        }

        return -1;
    }



    private static int linkedLaneSlotForManeuver(RouteGuidance.State s, int manIdx) {
        if (manIdx < 0) return -1;
        int max = (s.mLaneCount != null) ? s.mLaneCount.length : 0;

        if (s.mLinkedLaneGuidanceSlot != null && manIdx < s.mLinkedLaneGuidanceSlot.length) {
            int slot = s.mLinkedLaneGuidanceSlot[manIdx];
            if (slot >= 0 && slot < max) return slot;
        }

        if (s.mLinkedLaneGuidanceIndex != null && manIdx < s.mLinkedLaneGuidanceIndex.length) {
            int linkedIdx = s.mLinkedLaneGuidanceIndex[manIdx];
            if (linkedIdx >= 0 && linkedIdx < max) return linkedIdx;
        }
        return -1;
    }

    private static short mapLanePosition(RouteGuidance.State s, int manIdx, int laneIdx) {
        int[] pos = lanePositionsFor(s, manIdx);
        if (pos == null || laneIdx < 0 || laneIdx >= pos.length) {
            return (short) laneIdx;
        }
        int v = pos[laneIdx];
        if (v < 0 || v > 0x7FFF) return (short) laneIdx;
        return (short) v;
    }

    private static boolean hasLaneAnglesForLane(RouteGuidance.State s, int manIdx, int laneIdx) {
        int[][] lanes = laneAnglesFor(s, manIdx);
        if (lanes == null || laneIdx < 0 || laneIdx >= lanes.length) return false;
        int[] angles = lanes[laneIdx];
        return (angles != null && angles.length > 0);
    }

    private static boolean shouldEmitLane(RouteGuidance.State s, int manIdx, int laneIdx, short laneDir) {
        int[] dirs = laneDirectionsFor(s, manIdx);
        if (dirs == null || laneIdx < 0 || laneIdx >= dirs.length) return laneDir != (short)0xFF;
        int raw = dirs[laneIdx];

        if (raw == 1000 && !hasLaneAnglesForLane(s, manIdx, laneIdx)) return false;
        return laneDir != (short)0xFF;
    }

    private static final int[] LANE_DIR_NATIVE_ANGLES = {
        -180, -135, -90, -45, 0, 45, 90, 135, 180
    };
    private static final int[] LANE_DIR_NATIVE_CODES = {
        0x72, 0x60, 0x40, 0x20, 0x00, 0xE0, 0xC0, 0xA0, 0x92
    };

    private static int mapRawLaneValueToDirectionCode(int raw) {
        return mapRawLaneValueToDirectionCode(raw, false, 0);
    }

    private static int mapRawLaneValueToDirectionCode(int raw, boolean excludeOneNativeKey, int keyToExclude) {
        if (raw == 1000 || raw == -1000) return 0xFF; // Safety sentinels, never turn angles.
        if (raw < -180 || raw > 180) return 0xFF;

        int bestCode = 0;
        int bestDiff = 100000;
        for (int i = 0; i < LANE_DIR_NATIVE_ANGLES.length; i++) {
            int key = LANE_DIR_NATIVE_ANGLES[i];
            if (excludeOneNativeKey && key == keyToExclude) continue;
            int d = raw - key;
            if (d < 0) d = -d;
            if (d < bestDiff) {
                bestDiff = d;
                bestCode = LANE_DIR_NATIVE_CODES[i];
            }
        }
        return bestCode;
    }

    private static short mapLaneDirectionFromSentinel(RouteGuidance.State s, int manIdx, int laneIdx) {
        if (manIdx < 0) return (short) 0xFF;

        int[][] laneAngles = laneAnglesFor(s, manIdx);
        if (laneAngles != null && laneIdx >= 0 && laneIdx < laneAngles.length) {
            int[] angles = laneAngles[laneIdx];
            if (angles != null) for (int i = 0; i < angles.length; i++) {
                int direction = mapRawLaneValueToDirectionCode(angles[i]);
                if (direction != 0xFF) return (short)direction;
            }
        }
        return (short) 0xFF;
    }

    private static short mapLaneDirection(RouteGuidance.State s, int manIdx, int laneIdx) {
        int[] dirs = laneDirectionsFor(s, manIdx);
        if (dirs == null || laneIdx < 0 || laneIdx >= dirs.length)
            return mapLaneDirectionFromSentinel(s, manIdx, laneIdx);
        int raw = dirs[laneIdx];

        if (raw < -180 || raw > 180) {
            return mapLaneDirectionFromSentinel(s, manIdx, laneIdx);
        }

        return (short)(mapRawLaneValueToDirectionCode(raw) & 0xFF);
    }

    private static byte[] mapLaneSideStreets(RouteGuidance.State s, int manIdx, int laneIdx, short laneDirection) {
        if (manIdx < 0) return new byte[0];
        int[][] lanes = laneAnglesFor(s, manIdx);
        if (lanes == null || laneIdx < 0 || laneIdx >= lanes.length) return new byte[0];
        int[] angles = lanes[laneIdx];
        if (angles == null || angles.length == 0) return new byte[0];

        int primaryDir = laneDirection & 0xFF;
        int[] codes = new int[angles.length];
        int n = 0;
        for (int i = 0; i < angles.length; i++) {
            int code = mapRawLaneValueToDirectionCode(angles[i]) & 0xFF;
            if (code == 0xFF) continue;
            /* Skip angles that map to the same BAP direction as the primary --
             * they're not additional directions, just the same lane. */
            if (code == primaryDir) continue;

            boolean dup = false;
            for (int j = 0; j < n; j++) {
                if (codes[j] == code) {
                    dup = true;
                    break;
                }
            }
            if (!dup) codes[n++] = code;
        }
        if (n == 0) return new byte[0];

        for (int i = 1; i < n; i++) {
            int key = codes[i];
            int j = i - 1;
            while (j >= 0 && codes[j] > key) {
                codes[j + 1] = codes[j];
                j--;
            }
            codes[j + 1] = key;
        }

        byte[] out = new byte[n];
        for (int i = 0; i < n; i++) out[i] = (byte)(codes[i] & 0xFF);
        return out;
    }

    private static byte mapGuidanceInfo(RouteGuidance.State s, int manIdx, int laneIdx) {
        int[] primary = laneDirectionsFor(s, manIdx);
        // A fallback branch can describe the lane, but cannot become a
        // highlighted recommendation when its authoritative angle is unknown.
        if (primary == null || laneIdx < 0 || laneIdx >= primary.length
                || primary[laneIdx] < -180 || primary[laneIdx] > 180) return 0;
        int[] status = laneStatusFor(s, manIdx);
        if (status == null || laneIdx < 0 || laneIdx >= status.length) return 0;
        int v = status[laneIdx];
        if (v < 0 || v > 2) return 0;
        return (byte)v;
    }

    /**
     * Create a single CombiBAPNaviManeuverDescriptor.
     */
    private CombiBAPNaviManeuverDescriptor createDescriptor(int main, int dir, int zLevel, byte[] sideStreets) {
        int mappedMain = main;
        if (main == ManeuverMapper.PREPARE_TURN &&
            (dir == ManeuverMapper.DIR_STRAIGHT || dir == ManeuverMapper.DIR_LEFT)) {
            mappedMain = ManeuverMapper.CHANGE_LANE;
        }

        /* Direction is used as-is - ManeuverMapper.applyDsiNavBapDirectionOverride()
         * already handles per-type coarsening.  No second coarsening layer needed. */
        int mappedDir = dir;

        if (sideStreets == null) sideStreets = new byte[0];
        int mappedZ = (zLevel == 1 || zLevel == 2) ? zLevel : 0;
        return new CombiBAPNaviManeuverDescriptor(mappedMain, mappedDir, mappedZ, sideStreets);
    }

    /* ==============================================================
     * Custom Renderer Pipeline (maneuver_render)
     *
     * maneuver_render is owned by the CarPlay supervisor; Java does not spawn or kill it.
     * The renderer registers cluster displayable 98 via screen_manage_window and connects back
     * to our TCP server. Java only activates the cluster context + gfxAvailable and sends
     * CMD_MANEUVER packets. See docs/build-and-deploy/DEPLOY_AND_RELEASE.md.
     * ============================================================== */

    private volatile boolean customRendererStarted = false;
    private boolean rendererPrimed = false;

    /* Last maneuver state sent to renderer — only send CMD_MANEUVER when these change.
     * lastCrVer tracks the slot version so a new maneuver with identical type/angle
     * still triggers a push animation (e.g., consecutive left turns). */
    private LaneGuidanceSnapshot lastCrLaneGuidance;
    private int lastCrIcon = -1;
    private int lastCrDirection = -99;
    private int lastCrExitAngle = -9999;
    private int lastCrDrivingSide = -1;
    private int lastCrVer = -1;
    private long lastCrRouteGeneration = -1L;
    private boolean rendererManeuverPending; // guarded by this, also read by blink worker
    private int lastCrIdx = -1;
    /* Diagnostics only (update()/worker thread): transition-edge state for RGI-DIAG logs. */
    private boolean diagRerouting;
    private boolean diagClearedAwaitingManeuver;
    private boolean diagBlankWarned;
    private int[] lastCrJunctionAngles;
    private boolean lastCrSnapToRoad;

    /* Link recovery: count consecutive renderer-side TCP send failures.  When the link to the
     * always-on renderer drops (it was re-exec'd by the framework, or lsd's link reset), drop
     * our view + re-activate on the next update.  We do NOT kill or spawn the process. */
    private int crConsecutiveSendFailures = 0;
    private volatile int rendererSendFailureSerial;
    private static final int CR_SEND_FAIL_THRESHOLD = 3;

    private synchronized boolean startCustomRenderer() {
        if (csRef == null) {
            return false;
        }
        try {
            /* Bind our TCP server (idempotent; listen socket persists across route stop/start
             * within a session).  The ALWAYS-ON maneuver_render service connects to it via its
             * own reconnect loop — we never launch or kill the process. */
            if (rendererClient == null) {
                rendererClient = new RendererServer();
                rendererClient.setStateListener(new RendererServer.StateListener() {
                    public void onRendererStateChanged(String reason) {
                        if ("frame-cleared".equals(reason)
                                || "disconnect-request".equals(reason)
                                || "disposed".equals(reason)
                                || "disconnected".equals(reason)) {
                            if (com.luka.carplay.core.ScreenModule.isPresentationActive()) {
                                com.luka.carplay.core.ScreenModule.setPresentationActive(false);
                            }
                        }
                        notifyPresentationStateChanged("renderer-" + reason);
                    }
                });
            }
            if (!rendererClient.connect()) {
                Log.w(TAG, "CR: bind failed; renderer pipeline disabled");
                return false;
            }

            /* If the current peer vanished/reconnected, withdraw the old presentation before
             * priming the replacement. All calls remain on RouteGuidance's worker/update path. */
            if (customRendererStarted && !rendererClient.isFrameReady()) {
                if (com.luka.carplay.core.ScreenModule.isPresentationActive()) {
                    com.luka.carplay.core.ScreenModule.setPresentationActive(false);
                }
                forceGfxAvailable(false);
                customRendererStarted = false;
                rendererPrimed = false;
                notifyPresentationStateChanged("renderer-presentation-lost");
            }
            if (customRendererStarted) return true;
            if (!rendererClient.isReady()) return false;
            if (!refreshRendererViewport()) return false;

            /* Paint a deterministic frame, but do not expose displayable 98 until the peer
             * acknowledges eglSwapBuffers with FRAME_READY. */
            if (!rendererPrimed) {
                lastCrLaneGuidance = null;
                lastCrIcon = -1;
                lastCrDirection = -99;
                lastCrExitAngle = -9999;
                lastCrDrivingSide = -1;
                lastCrVer = -1;
                boolean sent = rendererClient.sendManeuver(
                    RendererMapper.ICON_APPROACH, 0, 0, 0, null, 0, 0, 1);
                noteRendererSendResult(sent);
                rendererPrimed = sent;
                if (!sent) return false;
            }
            if (!rendererClient.isFrameReady()) return false;

            /* BAP route info state for HUD icons.  onStart() also forces this,
             * but renderer respawn can happen mid-route without onStart(). */
            forceClusterRouteInfoState(true);

            /* Set gfxAvailable so VC enters MAP mode for LVDS video.
             * Must be after our window owns displayable 98 (so the encoder
             * reads our buffer, not whatever native KDK composition lingered
             * on the cluster before). */
            forceGfxAvailable(true);

            customRendererStarted = true;
            Log.i(TAG, "CR: started");
            notifyPresentationStateChanged("renderer-presentation-ready");
            return true;
        } catch (Throwable t) {
            Log.w(TAG, "CR start failed: " + t.getClass().getName() + ": " + t.getMessage());
            return false;
        }
    }

    /**
     * Track renderer-send failures.  When the link to the always-on renderer drops (it was
     * re-exec'd by the framework, or lsd's link reset), drop our view so the update-path retry
     * reconnects + re-activates.  We do NOT kill or respawn the process — the framework owns it.
     */
    private synchronized void noteRendererSendResult(boolean ok) {
        if (ok) {
            crConsecutiveSendFailures = 0;
            return;
        }
        rendererSendFailureSerial++;
        notifyPresentationStateChanged("renderer-update-pending");
        crConsecutiveSendFailures++;
        if (!customRendererStarted) return;
        /* Ignore failures before the first successful connect (sock=null naturally fails). */
        if (rendererClient == null || !rendererClient.everConnected()) return;
        // A live socket with a full queue needs retry, not a disconnect that
        // would throw away accepted maneuvers and flicker an otherwise valid view.
        if (rendererClient.isConnected()) return;
        if (crConsecutiveSendFailures < CR_SEND_FAIL_THRESHOLD) return;

        Log.w(TAG, "CR: renderer link lost (" + CR_SEND_FAIL_THRESHOLD
                + " send fails) — dropping view, will reconnect on next update");
        try { if (rendererClient != null) rendererClient.disconnectClient(); } catch (Throwable t) { /* ignore */ }
        customRendererStarted = false;
        rendererPrimed = false;
        crConsecutiveSendFailures = 0;
        notifyPresentationStateChanged("renderer-send-failures");
    }

    private synchronized void stopCustomRenderer(boolean preserveSurface) {
        try {
            /* Stop feeding, but preserve the final surface while connected: VC controls
             * its disappearance via Fct44. A route end must not clear pixels during fade-out.
             * The always-on renderer/link is reused and re-primed on the next route. */
            lastCrLaneGuidance = null;
            if (rendererClient != null && !preserveSurface) rendererClient.sendClear();
            forceGfxAvailable(false);
            customRendererStarted = false;
            rendererPrimed = false;
            Log.i(TAG, "CR: stopped (preserveSurface=" + preserveSurface + ")");
        } catch (Throwable t) {
            Log.w(TAG, "CR stop failed: " + t.getClass().getName() + ": " + t.getMessage());
        }
    }

    /** Refresh the screen-space overlay without changing the maneuver or blink state. */
    public boolean refreshRendererViewport() {
        if (rendererClient == null || !rendererClient.isReady()) return true;
        int[] area = com.luka.carplay.cluster.ClusterLayerController.maneuverViewport();
        // A cached viewport is not a new successful write. Do not let this
        // frequent no-op reset the maneuver/bargraph transport failure count.
        return rendererClient.sendVisibleArea(area[0], area[1], area[2], area[3]);
    }

    /** Send only changed maneuver geometry; viewport changes use their own command. */
    private synchronized boolean updateRendererIfChanged(RouteGuidance.State s, int bargraphDenominatorM) {
        if (rendererClient == null || !customRendererStarted) return false;

        try {
            int firstIdx = primaryManeuverIndex(s);
            if (firstIdx < 0 || s.maneuverCount == 0) return false;
            int[] bap = mapManeuver(s, firstIdx);
            RendererMapper.Mapping mapped = RendererMapper.map(bap[0], bap[1],
                s.mDrivingSide[firstIdx], s.mType[firstIdx], s.mTurnAngle[firstIdx],
                anglePresent(s, firstIdx), s.mJunctionAngles[firstIdx]);
            int icon = mapped.icon;
            int direction = mapped.direction;
            int exitAngle = mapped.exitAngle;
            int drivingSide = mapped.drivingSide;
            int[] junctionAngles = mapped.junctionAngles;
            int ver = s.mVer[firstIdx];
            boolean sameGeometry = icon == lastCrIcon && direction == lastCrDirection
                && exitAngle == lastCrExitAngle && drivingSide == lastCrDrivingSide
                && mapped.snapToRoad == lastCrSnapToRoad
                && sameIntArray(junctionAngles, lastCrJunctionAngles);
            if (sameGeometry && (icon == RendererMapper.ICON_ARRIVED
                    || (firstIdx == lastCrIdx && ver == lastCrVer
                        && s.routeGeneration == lastCrRouteGeneration))) {
                rendererManeuverPending = false;
                return false;
            }

            /* Remaining distance becomes the arrow's 0..16 progress input.
             * In the blink zone preserve the last phase already emitted to HUD. */
            int remainingLevel = 0;
            int progressMode = 0;
            int distM = s.distManeuverM;
            if (bargraphDenominatorM > 0 && distM > 0 && distM <= bargraphDenominatorM) {
                int pct = bargraphPercent(distM, bargraphDenominatorM);
                remainingLevel = (pct * 16) / 100;
                progressMode = 1;
            }

            int progressState=progressMode>0 ? RendererServer.PROGRESS_FILL : RendererServer.PROGRESS_OFF;
            if(progressMode>0 && isInBlinkZone(distM, bargraphDenominatorM)) {
                synchronized(distanceToManeuverLock) {
                    if(hasLastDistM && lastDistM==distM && lastBarOn
                            && (lastProgressState==RendererServer.PROGRESS_BLINK_LOW
                                || lastProgressState==RendererServer.PROGRESS_BLINK_HIGH)) {
                        progressState=lastProgressState;
                        remainingLevel=(lastBar*16)/100;
                    }
                }
            }

            int perspective = 1;  /* always 3D — 2D/3D switch disabled for now */

            boolean roadsOnly = icon == lastCrIcon && direction == lastCrDirection
                && exitAngle == lastCrExitAngle && drivingSide == lastCrDrivingSide
                && mapped.snapToRoad == lastCrSnapToRoad
                && firstIdx == lastCrIdx && ver == lastCrVer
                && s.routeGeneration == lastCrRouteGeneration;
            boolean ok = rendererClient.sendBapProgressManeuver(icon, direction, exitAngle,
                drivingSide, junctionAngles, remainingLevel, progressMode,
                perspective, roadsOnly, mapped.snapToRoad,
                progressState);
            // Failed enqueue must remain eligible for retry with the same input.
            if (ok) {
                lastCrIcon = icon;
                lastCrDirection = direction;
                lastCrExitAngle = exitAngle;
                lastCrDrivingSide = drivingSide;
                lastCrVer = ver;
                lastCrRouteGeneration = s.routeGeneration;
                lastCrIdx = firstIdx;
                lastCrJunctionAngles = junctionAngles;
                lastCrSnapToRoad = mapped.snapToRoad;
                rendererManeuverPending = false;
                if (diagClearedAwaitingManeuver) {
                    diagClearedAwaitingManeuver = false;
                    diagBlankWarned = false;
                    Log.i(TAG, "RGI-DIAG MANEUVER sent after CLEAR gen=" + s.routeGeneration
                        + " idx=" + firstIdx + " ver=" + ver + " icon=" + icon
                        + " presentation=" + com.luka.carplay.core.ScreenModule.isPresentationActive());
                }
            }
            noteRendererSendResult(ok);
            return ok;
        } catch (Throwable e) {
            Log.w(TAG, "CR update failed: " + e.getClass().getName() + ": " + e.getMessage());
            noteRendererSendResult(false);
            return false;
        }
    }

    /**
     * Update arrow progress on distance-only updates, without a maneuver push.
     */
    private synchronized void updateRendererProgress(RouteGuidance.State s, int bargraphDenominatorM) {
        if (rendererClient == null || !customRendererStarted || rendererManeuverPending) return;

        synchronized (distanceToManeuverLock) {
            if (hasLastDistM && lastDistM == s.distManeuverM) {
                noteRendererSendResult(rendererClient.sendProgress(lastBarOn ? (lastBar * 16) / 100 : 0,
                    lastBarOn ? 1 : 0, lastBarOn ? lastProgressState : RendererServer.PROGRESS_OFF));
                return;
            }
        }

        int remainingLevel = 0;
        int progressMode = 0;
        int distM = s.distManeuverM;
        if (bargraphDenominatorM > 0 && distM > 0 && distM <= bargraphDenominatorM) {
            int pct = bargraphPercent(distM, bargraphDenominatorM);
            remainingLevel = (pct * 16) / 100;
            if (isInBlinkZone(distM, bargraphDenominatorM)) {
                /* sendActionBlinkTick() supplies the same explicit phase to HUD
                 * and renderer. A distance-only update must not overwrite it. */
                return;
            }
            progressMode = 1;
        }
        noteRendererSendResult(rendererClient.sendProgress(remainingLevel, progressMode,
            progressMode>0 ? RendererServer.PROGRESS_FILL : RendererServer.PROGRESS_OFF));
    }

    /**
     * Supply graphics-sink notifications only for MOST cluster maps.
     * MU1316 stores dataRate on ClusterViewMode, not KOMOService. The stock
     * updateDataRate callback updates both that cache and the output rate; it
     * must precede updateGfxState, which replays the cached rate when enabled.
     */
    private void forceGfxAvailable(boolean available) {
        /* KOMO gfxAvailable/dataRate gate the stock view modes only on a MOST cluster map
         * (Util.isClusterMapMOST).  On this FPK cluster (sysConst 541 == 2) the whole chain is a
         * no-op EXCEPT one side effect: ClusterViewMode.setDataRate -> refreshMapVisibility ->
         * showKombiMap(viewMode == 3), and viewMode is pinned at COMPASS on FPK, so every rate
         * change parked the stock kombi map in its hidden context (frozen ~10 fps, roller zoom
         * swallowed) until the VC re-sent MapViewAndOrientation on a tab switch - visible right
         * after a disconnect that followed a route (seen 2026-09-21).  Stock itself never calls
         * refreshMapVisibility on FPK (refreshViewMode returns early). */
        IFrameworkAccess fw = CarPlayApp.framework();
        if (fw == null || !de.audi.tghu.navi.app.util.Util.isClusterMapMOST(fw)) return;
        int desiredRate = available ? 2 : 0;
        try {
            if (komoService != null) {
                komoService.updateDataRate(desiredRate, 1);
                komoService.updateGfxState(available ? 1 : 0, 1);
            } else if (csRef != null) {
                // Preserve recovery when the service reference was not acquired.
                ClusterViewMode viewMode = csRef.getClusterViewMode();
                viewMode.setDataRate(desiredRate);
                viewMode.setGFXAvailable(available);
                csRef.setKOMODataRate(desiredRate);
            } else {
                return;
            }
            Log.i(TAG, "KOMO: gfxAvailable=" + available + " dataRate=" + desiredRate);
        } catch (Throwable t) {
            Log.w(TAG, "KOMO: graphics state update failed: " + t.getMessage());
        }
    }

    /* ============================================================
     * Utilities
     * ============================================================ */

    static long getUtcMillis() {
        try {
            IFrameworkAccess fw = CarPlayApp.framework();
            if (fw != null) {
                return fw.getUTCTime();
            }
        } catch (Exception e) {
            /* ignore */
        }
        return System.currentTimeMillis();
    }

    private static final int TIME_FORMAT_12H = 11;

    private static int getHuNavigationTimeFormat() {
        try {
            int v = DateMetric.timeFormat;
            int bap = (v == TIME_FORMAT_12H) ? 1 : 0;
            return bap;
        } catch (Throwable t) {
        }
        return 0;
    }

    /**
     * Convert UTC epoch millis to HU local epoch millis.  The JVM default TZ on
     * MHI2Q is UTC, so TimeZone.getDefault() cannot be used; instead
     * IFrameworkAccess.convertUTCTimeToLocalTime() adds the HU's
     * utcOffsetMilliseconds (timezone + DST from the UTCOffset DSI callback).
     * Without a framework the input is returned unchanged (UTC).
     */
    private static long convertUtcToLocalMs(long utcMs) {
        try {
            IFrameworkAccess fw = CarPlayApp.framework();
            if (fw != null) {
                return fw.convertUTCTimeToLocalTime(utcMs);
            }
        } catch (Throwable t) {
            /* ignore */
        }
        return utcMs;
    }

    /**
     * Send ExitView with toggling variant to force AppConnectorNavi to always
     * consider it "changed" (sendStatusIfChanged).  Without this toggle,
     * FctID 49 is skipped when variant+num match the previous send, causing
     * FSG sync(1) for {23,18,49} to stall and blocking descriptor delivery.
     */
    private void sendExitView() {
        exitViewNum = 0;
        exitViewSendCount++;
        int variant = (exitViewSendCount % 2 == 0) ? EXITVIEW_EU : EXITVIEW_NAR;
        appConnectorNavi.updateExitView(variant, exitViewNum);
    }

    private static String hexBytes(byte[] b) {
        if (b == null || b.length == 0) return "[]";
        StringBuffer sb = new StringBuffer("[");
        for (int i = 0; i < b.length; i++) {
            if (i > 0) sb.append(",");
            sb.append("0x");
            sb.append(Integer.toHexString(b[i] & 0xFF));
        }
        sb.append("]");
        return sb.toString();
    }

    private static String limitUtf8(String s, int maxBytes) {
        if (s == null) return "";
        if (maxBytes <= 0) return "";
        try {
            byte[] b = s.getBytes("UTF-8");
            if (b.length <= maxBytes) return s;
            int lo = 0;
            int hi = s.length();
            while (lo < hi) {
                int mid = (lo + hi + 1) / 2;
                byte[] bm = s.substring(0, mid).getBytes("UTF-8");
                if (bm.length <= maxBytes) {
                    lo = mid;
                } else {
                    hi = mid - 1;
                }
            }
            /* A substring ending between a surrogate pair encodes a replacement
             * character. Keep the actual Unicode scalar intact at the limit. */
            if (lo > 0 && lo < s.length()
                    && s.charAt(lo - 1) >= '\uD800' && s.charAt(lo - 1) <= '\uDBFF'
                    && s.charAt(lo) >= '\uDC00' && s.charAt(lo) <= '\uDFFF') lo--;
            return s.substring(0, lo);
        } catch (Exception e) {
            if (s.length() <= maxBytes) return s;
            return s.substring(0, maxBytes);
        }
    }

    /** Collapse transport whitespace while preserving the user's Unicode text. */
    private static String normalizeRouteText(String value) {
        if (value == null || value.length() == 0) return "";
        StringBuffer out = new StringBuffer(value.length());
        boolean pendingSpace = false;
        for (int i = 0; i < value.length(); i++) {
            char c = value.charAt(i);
            if (c < 0x20 || Character.isWhitespace(c)) {
                pendingSpace = out.length() > 0;
            } else {
                if (pendingSpace) out.append(' ');
                out.append(c);
                pendingSpace = false;
            }
        }
        return out.toString();
    }

    private static String keepLastColonPart(String v) {
        if (v == null) return "";
        int pos = v.lastIndexOf(':');
        if (pos >= 0 && pos + 1 < v.length()) {
            String tail = v.substring(pos + 1).trim();
            if (tail.length() > 0) return tail;
        }
        return v;
    }

    private static int getFirstManeuverIndex(RouteGuidance.State s) {
        int maxIdx = (s.mType != null) ? s.mType.length : 0;
        if (s.maneuverOrder != null && s.maneuverOrder.length == 0) {
            return -1;
        }
        if (s.maneuverOrder != null && s.maneuverOrder.length > 0) {
            for (int i = 0; i < s.maneuverOrder.length; i++) {
                int idx = s.maneuverOrder[i];
                if (idx >= 0 && idx < maxIdx) return idx;
            }
        }
        return -1;
    }

    private static int primaryManeuverIndex(RouteGuidance.State s) {
        int[] list = getManeuverIndexList(s);
        if (list == null || list.length == 0 || s.mType == null) return -1;
        int idx = list[0];
        return idx >= 0 && idx < s.mType.length && ManeuverMapper.isValidType(s.mType[idx]) ? idx : -1;
    }

    private static boolean anglePresent(RouteGuidance.State s, int idx) {
        return (s.mTurnAnglePresent != null && idx < s.mTurnAnglePresent.length && s.mTurnAnglePresent[idx])
            || s.mTurnAngle[idx] != -1; // compatibility for callers constructing State directly
    }

    private static int[] mapManeuver(RouteGuidance.State s, int idx) {
        return ManeuverMapper.map(s.mType[idx], s.mTurnAngle[idx], s.mJunctionType[idx],
            s.mDrivingSide[idx], anglePresent(s, idx));
    }

    private static byte[] maneuverSideStreets(RouteGuidance.State s, int idx, int main) {
        if (main == ManeuverMapper.NO_INFO || main == ManeuverMapper.NO_SYMBOL) return new byte[0];
        int angle = s.mExitAngle[idx];
        if (angle == -1 && !anglePresent(s, idx)) angle = 1000;
        return SideStreets.calcSideStreetsBytes(s.mType[idx], s.mJunctionType[idx], s.mDrivingSide[idx],
            s.mJunctionAngles[idx], angle);
    }

    private static boolean sameIntArray(int[] a, int[] b) {
        if (a == b) return true;
        if (a == null || b == null || a.length != b.length) return false;
        for (int i = 0; i < a.length; i++) if (a[i] != b[i]) return false;
        return true;
    }

    private static int[] getManeuverIndexList(RouteGuidance.State s) {
        if (s.maneuverOrder != null && s.maneuverOrder.length == 0) {
            return null;
        }
        if (s.maneuverOrder != null && s.maneuverOrder.length > 0) {
            return s.maneuverOrder;
        }
        /*
         * Do not synthesize [0..maneuver_count) when iOS has not published
         * an explicit maneuver_list yet.  During reroute, count often arrives
         * before the new slot payloads; falling back to numeric slot order can
         * briefly expose stale pre-reroute slots to BAP and maneuver_render.
         */
        return null;
    }

    private static int getBargraphDenominatorM(RouteGuidance.State s, int manIdx, int prepareThresholdM) {
        if (manIdx < 0 || s == null) {
            return -1;
        }
        /* MUST classify exactly like the approach-zone threshold in update() (maneuver type only).
         * A step-length term here made a long-step city turn use the 1600 m highway denominator
         * while the pill opened at 457 m, so the arrow was already ~80 % filled when it appeared. */
        boolean isHighway = s.mType != null && manIdx < s.mType.length
            && ManeuverMapper.isHighwayManeuver(s.mType[manIdx]);
        int policyCap = isHighway ? HIGHWAY_DISPLAY_DISTANCE_M : CITY_DISPLAY_DISTANCE_M;
        if (policyCap <= 0) return -1;
        /*
         * The arrow graphic is composed in the approach zone (<= policyCap: ~1500 ft city, ~1.0 mi highway).
         * To ensure the arrow smoothly fills throughout the approach zone where it is visible (and does not
         * stay at 0 fill or immediately jump into flashing because of a short prior step length), the denominator
         * must span the entire approach window.
         */
        return policyCap;
    }
}
