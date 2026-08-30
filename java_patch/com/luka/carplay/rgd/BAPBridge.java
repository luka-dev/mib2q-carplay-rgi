/*
 * CarPlay Route Guidance - BAP Bridge
 *
 * Translates route guidance state to BAP protocol calls via AppConnectorNavi,
 * and drives maneuver_render via TCP for LVDS video rendering.
 *
 * BAP path: ManeuverDescriptor, distance, street, lane guidance, ETA -> VC/HUD text overlays.
 * maneuver_render path: CMD_MANEUVER over TCP -> maneuver_render EGL/GLES2 -> video encoder -> MOST -> VC LVDS.
 *
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
import de.audi.tghu.navi.app.cluster.KOMOService;
import de.audi.tghu.navi.app.command.DSIResponseContainer;
import java.lang.reflect.Field;
import java.lang.reflect.Method;

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

    /*
     * BAP supports three maneuver slots, but publish only the current one.
     * Supplying look-ahead maneuvers makes the HUD combine Maneuver_1 and
     * Maneuver_2 into instructions such as "left, then left again".  The full
     * iOS maneuverOrder remains cached for renderer/lane-guidance consumers;
     * this limit applies only to FctID 0x17 ManeuverDescriptor output.
     */
    private static final int MAX_BAP_MANEUVERS = 1;

    /*
     * Fixed maneuver thresholds (meters).
     * These are intentionally static (no speed/time conversion at runtime).
     */
    private static final int CITY_PREPARE_THRESHOLD_M = 1500;
    private static final int HIGHWAY_PREPARE_THRESHOLD_M = 3000;
    private static final int HIGHWAY_STEP_THRESHOLD_M = 2000;
    private static final int BARGRAPH_ACTION_PERCENT_OF_PREPARE = 15;
    private static final int BARGRAPH_BLINK_PERCENT = 20;
    private static final int ACTION_BLINK_INTERVAL_MS = 600;

    private CombiBAPServiceNavi appConnectorNavi;
    private final BAPDistanceFormatter distanceFormatter =
        new BAPDistanceFormatter(new SilentLogChannel());

    private boolean initialized = false;
    /* Set only after the complete synchronous BAP start sequence has returned successfully.
     * RouteGuidance combines this with renderer FRAME_READY before exposing context 80. */
    private volatile boolean bapSessionStarted = false;
    /* CarPlay owns FctID 19/21/22/46 for the whole active RGI interval. */
    private String latchedTurnToText = "";
    private boolean viewAreaLowerBarPublished = false;
    /* User-selectable route-info representation. View changes always reset it
     * to 0; only the serialized RouteGuidance presentation worker mutates it. */
    private int infoPhase = 0;
    /* Explicit output caches allow View/OK to repaint immediately without
     * waiting for another iOS RGI delta. */
    private long lastEtaSeconds = -1L;
    private long lastTimeRemainingSeconds = -1L;
    private long lastTimeRemainingSampleSeconds = -1L;
    private int lastDistanceToDestinationM = -1;

    /*
     * Approach mode: true when distM <= prepareThreshold (showing real maneuver icon),
     * false when further away (showing FOLLOW_STREET).
     */
    private boolean inApproachZone = false;
    /* Track the primary maneuver's slot identity so we know when iOS
     * actually swapped the head of the list vs. just reordered/extended it.
     * mVer changes when the C hook reassigns a slot to a new iAP2 index. */
    private int lastFirstManeuverIdx = -1;
    private int lastFirstManeuverVer = -1;
    /* Call-for-action blink phase: true=100%, false=0% */
    private boolean actionBlinkFull = true;
    private final Object distanceToManeuverLock = new Object();
    private boolean hasLastDistM = false;
    private int lastDistM = 0;
    private boolean lastBarOn = false;
    private int lastBar = 0;
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
    }

    private synchronized void updateActionBlinkContext(boolean armed, int distM, int bargraphDenominatorM) {
        blinkArmed = armed;
        blinkDistM = distM;
        blinkBargraphDenominatorM = bargraphDenominatorM;
        if (!armed || distM <= 0 || bargraphDenominatorM <= 0 || distM > bargraphDenominatorM) {
            actionBlinkFull = true;
        }
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
    private boolean sendActionBlinkTick(int myGen) {
        synchronized (this) {
            if (myGen != actionBlinkGeneration) return false;
            if (!blinkArmed || blinkDistM <= 0 || blinkBargraphDenominatorM <= 0
                    || blinkDistM > blinkBargraphDenominatorM) return true;
            int linBargraph = (blinkDistM * 100) / blinkBargraphDenominatorM;
            if (linBargraph < 0) linBargraph = 0;
            if (linBargraph > 100) linBargraph = 100;
            if (linBargraph >= BARGRAPH_BLINK_PERCENT) {
                actionBlinkFull = true;
                return true;
            }
            int bargraph = actionBlinkFull ? 100 : 0;
            actionBlinkFull = !actionBlinkFull;

            try {
                sendDistanceToManeuverRaw(blinkDistM, true, bargraph);
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
        synchronized (distanceToManeuverLock) {
            if (meters > 0) {
                hasLastDistM = true;
                lastDistM = meters;
                lastBarOn = bargraphOn;
                lastBar = bargraph;
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
        if (rendererClient != null && customRendererStarted) {
            try {
                int crLevel = bargraphOn ? (bargraph * 16) / 100 : 0;
                if (crLevel > 16) crLevel = 16;
                int crMode = bargraphOn ? 1 : 0;
                noteRendererSendResult(rendererClient.sendBargraph(crLevel, crMode));
            } catch (Throwable t) {
                noteRendererSendResult(false);
                /* BAP already sent; renderer will resync on next tick */
            }
        }
    }

    private void sendDistanceToDestinationRaw(int meters, boolean isStopOver) {
        FormattedDistance fd = formatDistanceToDestination(meters);
        appConnectorNavi.updateDistanceToDestination(fd.value, fd.unit, isStopOver);
    }

    private FormattedDistance formatDistanceToTurn(int meters) {
        if (meters <= 0) return new FormattedDistance(-1, 0);
        try {
            boolean metric = isMetricDistanceUnits();
            /* BAPDistanceFormatter$BAPDistance is public, but the outer class .class file
             * lacks the InnerClasses attribute (decompiler artifact), so javac can't
             * resolve BAPDistanceFormatter.BAPDistance as a type.  Use Object + getValue/getUnit. */
            Object d = distanceFormatter.formatDistanceToTurn(meters, metric);
            int v = ((Integer) d.getClass().getMethod("getValue", new Class[0]).invoke(d, new Object[0])).intValue();
            int u = ((Integer) d.getClass().getMethod("getUnit", new Class[0]).invoke(d, new Object[0])).intValue();
            return new FormattedDistance(v, u);
        } catch (Throwable t) {
            Log.w(TAG, "formatDistanceToTurn failed, using invalid distance: " + t.getMessage());
            return new FormattedDistance(-1, 0);
        }
    }

    private FormattedDistance formatDistanceToDestination(int meters) {
        if (meters <= 0) return new FormattedDistance(-1, 0);
        try {
            boolean metric = isMetricDistanceUnits();
            Object d = distanceFormatter.formatDistanceToDestination(meters, metric);
            int v = ((Integer) d.getClass().getMethod("getValue", new Class[0]).invoke(d, new Object[0])).intValue();
            int u = ((Integer) d.getClass().getMethod("getUnit", new Class[0]).invoke(d, new Object[0])).intValue();
            return new FormattedDistance(v, u);
        } catch (Throwable t) {
            Log.w(TAG, "formatDistanceToDestination failed, using invalid distance: " + t.getMessage());
            return new FormattedDistance(-1, 0);
        }
    }

    private static boolean isMetricDistanceUnits() {
        try {
            int unit = Distance.getSystemUnit();
            return unit == Distance.KM || unit == Distance.METERS || unit == Distance.NONE;
        } catch (Throwable t) {
            return true;
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
            if (container != null) container.setRgActive(active);
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
            inApproachZone = false;
            latchedTurnToText = "";
            viewAreaLowerBarPublished = false;
            infoPhase = 0;
            lastEtaSeconds = -1L;
            lastTimeRemainingSeconds = -1L;
            lastTimeRemainingSampleSeconds = -1L;
            lastDistanceToDestinationM = -1;
            lastFirstManeuverIdx = -1;
            lastFirstManeuverVer = -1;
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
                    return false;
                }
            }

            /* FctID 19 is shared with stock outside RGI. Claim it only for the
             * interval in which this bridge publishes CarPlay lower-bar text. */
            com.luka.carplay.core.ScreenNavStatusGate.setCurrentPositionInfoBlocked(true);
            /* Block native route-guidance BAP stream during CarPlay RG. */
            com.luka.carplay.core.ScreenNavStatusGate.setRouteGuidanceBlocked(true);

            /* Start renderer FIRST — takes over native displayable 20
             * (DISPLAYABLE_MAP_ROUTE_GUIDANCE, the KOMO RG widget slot) by
             * registering our screen window with ID="20" in displaymanager's
             * m_surfaceSources.  Doing this before we set rgActive/rgiValid
             * avoids a one-frame KDK flicker. */
            startCustomRenderer();                 /* non-blocking; readiness edges drive completion */

            /* Now safe to set cluster state flags — our window owns
             * displayable 20, encoder reads it via setActiveDisplayable(4,20). */
            forceClusterRouteInfoState(true);

            /*
             * Guidance start -- BAP text overlays for HUD + VC text.
             *
             * 1. RGStatus(1) - FctID 17 -> triggers startSync(0) for {17,39,23,18,49}
             * 2. Complete sync(0) window: rgType(39), descriptor(23), distance(18), exitView(49)
             */
            appConnectorNavi.updateRGStatus(1);                                      /* FctID 17 -> sync(0) */
            appConnectorNavi.updateActiveRGType(ACTIVE_RGTYPE);                      /* FctID 39 */

            /* Sync(0) FctIDs: descriptor, distance, exitView */
            sendFollowStreet();                                                      /* FctID 23 */
            sendDistanceToManeuverRaw(0, false, 0);                                  /* FctID 18 */
            sendExitView();                                                          /* FctID 49 */

            Log.i(TAG, "Started (rgType=" + ACTIVE_RGTYPE
                + ", cr=" + customRendererStarted + ")");
            bapSessionStarted = true;
            /* Never expose the VC's empty "---" shell while the first iOS
             * maneuver snapshot is still arriving. */
            try {
                appConnectorNavi.updateCurrentPositionInfo("\u2191");
            } catch (Throwable t) {
                Log.w(TAG, "BAP FctID 19 startup fallback failed: " + t);
            }
            return true;

        } catch (Throwable e) {
            bapSessionStarted = false;
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
        try { appConnectorNavi.updateRGStatus(0); } catch (Throwable t) { }
        try { appConnectorNavi.updateActiveRGType(0); } catch (Throwable t) { }
        try { sendNoSymbol(); } catch (Throwable t) { }
        try { sendDistanceToManeuverRaw(0, false, 0); } catch (Throwable t) { }
        try { sendExitView(); } catch (Throwable t) { }
        try { appConnectorNavi.updateManeuverState(0); } catch (Throwable t) { }
        try { stopCustomRenderer(true); } catch (Throwable t) { }
        forceGfxAvailable(false);
        bapSessionStarted = false;
        rendererPrimed = false;
        crConsecutiveSendFailures = 0;
        /* Release FctID 19 only after our cleanup transaction has completed,
         * so stock cannot overwrite the lower bar in the middle of rollback. */
        com.luka.carplay.core.ScreenNavStatusGate.setCurrentPositionInfoBlocked(false);
        Log.w(TAG, "onStart rollback complete");
    }

    public void onStop() {
        if (!initialized) return;

        try {
            bapSessionStarted = false;
            /* Lightweight stop — reset internal state only.
             * No BAP teardown, no renderer kill. iOS sends transient route_state=0
             * during maneuver transitions; full teardown causes HUD flicker + renderer
             * black screen. BAP teardown happens in onShutdown() on real disconnect. */
            stopActionBlinkThread();
            inApproachZone = false;
            latchedTurnToText = "";
            viewAreaLowerBarPublished = false;
            infoPhase = 0;
            lastFirstManeuverIdx = -1;
            lastFirstManeuverVer = -1;
        } catch (Exception e) {
            Log.e(TAG, "onStop error", e);
        }
    }

    /**
     * Full shutdown — BAP teardown + renderer socket teardown.
     * Called on actual CarPlay disconnect or stop().
     */
    public void onShutdown() {
        if (!initialized) return;

        try {
            bapSessionStarted = false;
            /* Defensive: stop action blink (it's also stopped on approach
             * zone exit, but onShutdown can be called from non-approach
             * states too — e.g., disconnect mid-route). */
            stopActionBlinkThread();
            latchedTurnToText = "";
            viewAreaLowerBarPublished = false;
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
                appConnectorNavi.updateRGStatus(0);
                appConnectorNavi.updateActiveRGType(0);
                sendNoSymbol();
                sendDistanceToManeuverRaw(0, false, 0);
                sendExitView();
                appConnectorNavi.updateManeuverState(0);
                appConnectorNavi.updateCurrentPositionInfo("");
                sendDistanceToDestinationRaw(0, false);
                appConnectorNavi.updateTimeToDestination(0, 0, -1);
                appConnectorNavi.updateLaneGuidance(false, new CombiBAPNaviLaneGuidanceData[0]);
            } catch (Throwable t) {
                Log.w(TAG, "onShutdown: BAP teardown threw (continuing to cleanup): " + t);
            }
            /* CarPlay RGI no longer owns FctID 19. Keep the rest of native RG
             * gated until phone disconnect, but let stock update the lower bar. */
            com.luka.carplay.core.ScreenNavStatusGate.setCurrentPositionInfoBlocked(false);

            /* Decide before stopping the custom renderer: stop used to clear rgActive
             * unconditionally, making the nativeHasRoute=true branch below ineffective. */
            boolean nativeHasRoute = nativeNavigationHasRoute();
            stopCustomRenderer(!nativeHasRoute);
            /* CarPlay session ending — release the renderer listen socket
             * (port :19800).  stopCustomRenderer keeps it bound for fast
             * route restarts within a session; full session shutdown
             * actually closes it. */
            if (rendererClient != null) {
                rendererClient.setStateListener(null);
                rendererClient.dispose();
                rendererClient = null;
            }
            rendererPrimed = false;
            /* REPLACE: keep the native RG gate SHUT for the whole connected session.
             * onShutdown fires when CarPlay navigation ENDS (not on disconnect), so without
             * this guard the gate would reopen mid-session and stock nav RG could reappear on
             * the cluster.  disengageTakeover() (on real disconnect) is the only reopen. */
            if (!takeoverEngaged)
                com.luka.carplay.core.ScreenNavStatusGate.setRouteGuidanceBlocked(false);

            /*
             * Tear down our cluster RG-state override CONDITIONALLY.
             *
             * forceClusterRouteInfoState(false) sets
             * dsiResponseContainer.rgActive=false.  Stock cluster firmware's
             * RgStopGuidanceGeneralCommand.execute() short-circuits with
             * commandFinished() when rgActive==false AND
             * rgRouteCalculationState==0 — which means the user's
             * "Cancel Map Guidance" button on the MMI map silently does
             * nothing if we leave rgActive=false here while native nav
             * still has an active internal route.
             *
             * Decision: only clear our override if native nav has NO route.
             * If native nav has a route (persisted through our session),
             * leave rgActive=true so its own Cancel command can run.  Native
             * nav drives rgActive itself — it'll go back to false naturally
             * when the user cancels or arrives.
             */
            if (nativeHasRoute) {
                Log.i(TAG, "Shutdown: native nav has active route — leaving rgActive=true so its Cancel still works");
            }
            forceGfxAvailable(false);

            Log.i(TAG, "Shutdown (full teardown)");
        } catch (Exception e) {
            Log.e(TAG, "onShutdown error", e);
        }
    }

    private boolean nativeNavigationHasRoute() {
        try {
            de.audi.tghu.navi.app.Navigation navi =
                de.audi.tghu.navi.app.Navigation.getInstance();
            if (navi == null) return false;
            de.audi.tghu.navi.app.routeguidance.IRouteManager rm = navi.getRouteManager();
            return rm != null && rm.getRoute() != null;
        } catch (Throwable t) {
            return false;
        }
    }

    /* ============================================================
     * Update
     * ============================================================ */

    public boolean update(RouteGuidance.State s) {
        if (!initialized || s == null) return false;

        int dirty = s.dirtyMask;
        cacheTravelInfo(s, dirty);
        if (dirty == 0) return false;

        try {
            /*
             * Explicit clear: count dropped to 0.  But only treat it as a real clear
             * if the route itself is inactive (routeState < 1).
             */
            boolean explicitClear = ((dirty & RouteGuidance.State.DIRTY_MANEUVER_COUNT) != 0)
                && (s.maneuverCount == 0)
                && (s.routeState <= 0);
            int distM = s.distManeuverM;
            int[] idxs = getManeuverIndexList(s);
            boolean hasManeuverList = (idxs != null && idxs.length > 0);
            boolean hasAnyManeuver = (s.maneuverCount > 0);
            boolean shouldClearManeuver = (s.maneuverCount == 0) && (s.routeState <= 0);

            int firstIdx = (idxs != null && idxs.length > 0) ? idxs[0] : -1;
            int type0 = (firstIdx >= 0 && s.mType != null && firstIdx < s.mType.length) ? s.mType[firstIdx] : -1;
            boolean showManeuver = ManeuverMapper.isValidType(type0);

            /* Highway-aware prepare thresholds (meters).
             *
             * iOS sends `step.distance` (= s.mDistance[idx]) — the length of
             * the route segment between the previous maneuver and this one.
             * Long step (>2 km) = highway/limited-access road; short step
             * = city.  This catches `MT_KEEP_RIGHT` / `MT_LEFT_TURN` on
             * highways which the type-based check would miss.
             * Fallback: when step length unknown (route setup), fall back
             * to the maneuver-type heuristic. */
            int rawStepM = (firstIdx >= 0 && s.mDistance != null
                    && firstIdx < s.mDistance.length) ? s.mDistance[firstIdx] : -1;
            boolean isHighway;
            if (rawStepM > 0) {
                isHighway = rawStepM > HIGHWAY_STEP_THRESHOLD_M;
            } else {
                isHighway = (type0 >= 0) && ManeuverMapper.isHighwayManeuver(type0);
            }
            int prepareThreshold  = isHighway ? HIGHWAY_PREPARE_THRESHOLD_M : CITY_PREPARE_THRESHOLD_M;
            int bargraphDenominatorM = getBargraphDenominatorM(s, firstIdx, prepareThreshold);
            updateActionBlinkContext(
                hasManeuverList && showManeuver && (bargraphDenominatorM > 0) && !shouldClearManeuver && !explicitClear,
                distM, bargraphDenominatorM);

            /*
             * Approach zone detection.
             *
             * Reset cached state only when the PRIMARY maneuver actually
             * changes — not on every list update.  iOS sends DIRTY_MANEUVER_LIST
             * for additions/reorders too; resetting in those cases caused a
             * one-frame flicker to FOLLOW_STREET when distM was transiently
             * unknown (slot reassign in the same delta).
             *
             * "Primary changed" = different slot index OR same slot but new
             * mVer (LRU reassigned the slot to a different iAP2 index). */
            int currentFirstVer = (firstIdx >= 0 && s.mVer != null
                    && firstIdx < s.mVer.length) ? s.mVer[firstIdx] : -1;
            boolean primaryChanged = (firstIdx != lastFirstManeuverIdx)
                || (currentFirstVer != lastFirstManeuverVer);
            if (primaryChanged) {
                inApproachZone = false;
                lastFirstManeuverIdx = firstIdx;
                lastFirstManeuverVer = currentFirstVer;
            }
            boolean hasUsableDistance = (distM > 0);
            /* ARRIVED maneuvers must always show real descriptor, not FOLLOW_STREET.
             * distM==0 + new list resets inApproachZone → nowApproach would be false
             * → sendFollowStreet() instead of the destination icon. Treat arrival
             * types as always in approach zone (MHI3 always sends real descriptor). */
            boolean isArrival = (type0 == ManeuverMapper.MT_ARRIVE_END_OF_NAVIGATION
                || type0 == ManeuverMapper.MT_ARRIVE_AT_DESTINATION
                || type0 == ManeuverMapper.MT_ARRIVE_END_OF_DIRECTIONS
                || type0 == ManeuverMapper.MT_ARRIVE_DESTINATION_LEFT
                || type0 == ManeuverMapper.MT_ARRIVE_DESTINATION_RIGHT);
            boolean nowApproach = isArrival
                || (hasUsableDistance ? (distM <= prepareThreshold) : inApproachZone);
            boolean approachChanged = hasUsableDistance
                && (nowApproach != inApproachZone)
                && showManeuver && hasManeuverList;
            if (approachChanged) {
                dirty |= RouteGuidance.State.DIRTY_DIST_MAN
                       | RouteGuidance.State.DIRTY_LANE_GUIDANCE
                       | RouteGuidance.State.DIRTY_MANEUVER_TEXT
                       | RouteGuidance.State.DIRTY_MANEUVER_ICON;  /* BAP needs icon refresh for approach/follow */
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

            /*
             * 1. Maneuver icons (FctID 23)
             */
            boolean descriptorSent = false;
            if ((dirty & (RouteGuidance.State.DIRTY_MANEUVER_ICON |
                          RouteGuidance.State.DIRTY_MANEUVER_LIST |
                          RouteGuidance.State.DIRTY_MANEUVER_COUNT)) != 0) {
                if (explicitClear) {
                    sendNoSymbol();
                    descriptorSent = true;
                } else if (hasManeuverList) {
                    if (showManeuver) {
                        if (nowApproach) {
                            sendManeuvers(s);
                        } else {
                            sendFollowStreet();
                        }
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
                    boolean haveCached;
                    int cachedDistM;
                    boolean cachedBarOn;
                    int cachedBar;
                    synchronized (distanceToManeuverLock) {
                        haveCached = hasLastDistM;
                        cachedDistM = lastDistM;
                        cachedBarOn = lastBarOn;
                        cachedBar = lastBar;
                    }
                    if (haveCached) {
                        sendDistanceToManeuverRaw(cachedDistM, cachedBarOn, cachedBar);
                    } else {
                        sendDistanceToManeuverRaw(0, false, 0);
                    }
                } else if (distM <= 0 || shouldClearManeuver) {
                    resetActionBlinkState();
                    sendDistanceToManeuverRaw(0, false, 0);
                } else {
                    boolean inAction = (bargraphDenominatorM > 0) && (distM <= bargraphDenominatorM);
                    boolean bargraphOn = false;
                    int bargraph = 0;
                    if (inAction) {
                        bargraphOn = true;
                        bargraph = (distM * 100) / bargraphDenominatorM;
                        if (bargraph < 0) bargraph = 0;
                        if (bargraph > 100) bargraph = 100;
                        if (bargraph < BARGRAPH_BLINK_PERCENT) {
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

            /* 4. Persistent CarPlay lower bar (FctID 19).
             * Phase 0 is always the cached real arrow/road text. Phase 1 replaces it with
             * the trip summary: the full one in fullscreen, a short "ETA 4 min" in
             * smallscreen, where the row is narrow. */
            int lowerBarDirty = RouteGuidance.State.DIRTY_CURRENT_ROAD
                | RouteGuidance.State.DIRTY_MANEUVER_TEXT
                | RouteGuidance.State.DIRTY_MANEUVER_LIST
                | RouteGuidance.State.DIRTY_MANEUVER_ICON
                | RouteGuidance.State.DIRTY_MANEUVER_COUNT;
            if (infoPhase != 0) {
                lowerBarDirty |= RouteGuidance.State.DIRTY_DIST_DEST
                    | RouteGuidance.State.DIRTY_TIME_REMAINING
                    | RouteGuidance.State.DIRTY_ETA;
            }
            if (!viewAreaLowerBarPublished || (dirty & lowerBarDirty) != 0) {
                updateLatchedPositionText(s);
                publishCurrentPositionForMode();
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
                    appConnectorNavi.updateManeuverState(0);
                } else if (shouldClearManeuver || (hasManeuverList && showManeuver)) {
                    int bapState;
                    if (showManeuver) {
                        if (!nowApproach) {
                            bapState = 1;   /* Follow */
                        } else if (hasUsableDistance && bargraphDenominatorM > 0 && distM <= bargraphDenominatorM) {
                            bapState = 4;   /* Action */
                        } else {
                            bapState = 2;   /* Prepare */
                        }
                    } else {
                        bapState = 0;
                    }
                    appConnectorNavi.updateManeuverState(bapState);
                } else if (hasManeuverList && hasAnyManeuver) {
                    appConnectorNavi.updateManeuverState(1);
                } else if (hasAnyManeuver && !hasManeuverList) {
                }
            }

            /*
             * 6. Lane guidance (FctID 24)
             */
            int laneRecomputeMask = RouteGuidance.State.DIRTY_LANE_GUIDANCE
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
                boolean wantLaneGuidance = !explicitClear
                    && !shouldClearManeuver
                    && s.laneGuidanceShowing == 1;
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

            /* 8. Time to destination (FctID 22). It always remains the normal
             * absolute ETA; the user-selectable compact summary is fullscreen
             * FctID 19 only. */
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

            /* 10. maneuver_render: CMD_MANEUVER only when icon actually changes,
             * CMD_BARGRAPH for distance updates, CMD_PERSPECTIVE for approach zone */
            /* Non-blocking state advance.  READY/FRAME_READY also wakes RouteGuidance when no
             * further iOS RGI delta arrives (the cold-boot case). */
            if (!customRendererStarted && csRef != null) startCustomRenderer();

            if (rendererClient != null && customRendererStarted) {
                /* Link-loss → noteRendererSendResult drops the view; the retry above reconnects. */

                /* Approach zone enter/exit -> bargraph + icon mode change */
                if (approachChanged) {
                    if (nowApproach) {
                        updateRendererBargraph(s, bargraphDenominatorM);
                    } else {
                        noteRendererSendResult(rendererClient.sendBargraph(0, 0));
                        /* Exiting approach zone: show ICON_APPROACH (follow street)
                         * to match BAP's sendFollowStreet(). */
                        sendRendererFollowStreet();
                    }
                }
                /* Check if rendered maneuver actually changed */
                boolean iconChanged = false;
                int crIconMask = RouteGuidance.State.DIRTY_MANEUVER_ICON
                    | RouteGuidance.State.DIRTY_MANEUVER_LIST
                    | RouteGuidance.State.DIRTY_MANEUVER_COUNT;
                if ((dirty & crIconMask) != 0) {
                    if (nowApproach) {
                        iconChanged = updateRendererIfChanged(s, bargraphDenominatorM);
                    } else if (showManeuver && hasManeuverList && !explicitClear && !shouldClearManeuver) {
                        /* Outside approach zone: show follow street, mirroring BAP path */
                        sendRendererFollowStreet();
                        iconChanged = true;
                    }
                }
                /* Distance-only → CMD_BARGRAPH (no push), only in approach zone */
                if (!iconChanged && !approachChanged && inApproachZone
                        && (dirty & RouteGuidance.State.DIRTY_DIST_MAN) != 0) {
                    updateRendererBargraph(s, bargraphDenominatorM);
                }
            }

            return true;

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
    public void refreshInfoPresentation(RouteGuidance.State s, int phase) {
        infoPhase = (phase == 0) ? 0 : 1;
        if (!initialized || !bapSessionStarted || s == null) return;
        try {
            cacheTravelInfo(s, s.dirtyMask);
            updateLatchedPositionText(s);
            publishCurrentPositionForMode();
            publishTimeToDestinationForMode();
            Log.i(TAG, "route-info phase=" + infoPhase + " applied view="
                + (com.luka.carplay.core.ScreenModule.isSmallScreenViewArea() ? "smallscreen" : "fullscreen"));
        } catch (Throwable t) {
            Log.w(TAG, "route-info refresh failed: " + t);
        }
    }

    /** Refresh only the real FctID 19 position cache; never store the phase-1 summary here. */
    private void updateLatchedPositionText(RouteGuidance.State s) {
        int idx = getFirstManeuverIndex(s);
        String candidate = "";
        if (idx >= 0) {
            if (s.mExitInfo != null && idx < s.mExitInfo.length
                    && s.mExitInfo[idx] != null && s.mExitInfo[idx].length() > 0) {
                candidate = keepLastColonPart(s.mExitInfo[idx]);
            } else if (s.mAfterRoad != null && idx < s.mAfterRoad.length
                    && s.mAfterRoad[idx] != null && s.mAfterRoad[idx].length() > 0) {
                candidate = keepLastColonPart(s.mAfterRoad[idx]);
            } else if (s.mName != null && idx < s.mName.length
                    && s.mName[idx] != null && s.mName[idx].length() > 0) {
                candidate = s.mName[idx];
            }
        }
        if (candidate.length() == 0 && s.currentRoad != null) candidate = s.currentRoad;

        String next = directionArrow(s, idx);
        if (candidate.length() > 0) next += " " + candidate;
        next = limitUtf8(next, 96);
        if (next.length() > 0) latchedTurnToText = next;
        if (latchedTurnToText.length() == 0) latchedTurnToText = "\u2191";
    }

    private void publishCurrentPositionForMode() {
        boolean smallScreen = com.luka.carplay.core.ScreenModule.isSmallScreenViewArea();
        String text = infoPhase == 0 ? latchedTurnToText
            : (smallScreen ? buildShortSummary() : buildTripSummary());
        if (text == null || text.length() == 0) text = latchedTurnToText;
        if (text == null || text.length() == 0) text = "\u2191";
        text = limitUtf8(text, 96);
        appConnectorNavi.updateCurrentPositionInfo(text);
        viewAreaLowerBarPublished = true;
    }

    private void cacheTravelInfo(RouteGuidance.State s, int dirty) {
        if (s == null) return;
        if ((dirty & RouteGuidance.State.DIRTY_ETA) != 0) {
            lastEtaSeconds = s.etaSeconds;
        }
        if ((dirty & RouteGuidance.State.DIRTY_TIME_REMAINING) != 0) {
            lastTimeRemainingSeconds = s.timeRemainingSeconds;
            lastTimeRemainingSampleSeconds = getUtcMillis() / 1000L;
        }
        if ((dirty & RouteGuidance.State.DIRTY_DIST_DEST) != 0) {
            lastDistanceToDestinationM = s.distDestM;
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
         * docs/reference/CLUSTER_KDK_GEOMETRY.md. */
        long timeVal = currentArrivalSeconds();
        /* JVM default TZ is UTC on MHI2. AppConnectorNavi converts a type-1
         * epoch with GregorianCalendar, so shift it to HU local time first. */
        if (timeVal >= 0L) {
            timeVal = convertUtcToLocalMs(timeVal * 1000L) / 1000L;
        }
        appConnectorNavi.updateTimeToDestination(1, timeFormat, timeVal);
    }

    private String buildTripSummary() {
        StringBuffer out = new StringBuffer();
        appendSummaryPart(out, formatArrivalForText(currentArrivalSeconds()));
        appendSummaryPart(out, formatRemainingForText(currentRemainingSeconds()));
        return out.toString();
    }

    /** Smallscreen phase 1: one short field for a narrow row, e.g. "ETA 4 min".
     *  Prefers the remaining duration; falls back to the arrival clock, which
     *  formatArrivalForText already prefixes with "ETA". */
    private String buildShortSummary() {
        String remaining = formatRemainingForText(currentRemainingSeconds());
        if (remaining.length() > 0) return "ETA " + remaining;
        return formatArrivalForText(currentArrivalSeconds());
    }

    private static void appendSummaryPart(StringBuffer out, String value) {
        if (value == null || value.length() == 0) return;
        if (out.length() > 0) out.append(" | ");
        out.append(value);
    }

    private static String formatArrivalForText(long utcSeconds) {
        if (utcSeconds < 0L) return "";
        long localMs = convertUtcToLocalMs(utcSeconds * 1000L);
        java.util.GregorianCalendar cal = new java.util.GregorianCalendar();
        cal.setTimeInMillis(localMs);
        int hour = cal.get(java.util.Calendar.HOUR_OF_DAY);
        int minute = cal.get(java.util.Calendar.MINUTE);
        StringBuffer out = new StringBuffer("ETA ");
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

    private static String formatRemainingForText(long seconds) {
        if (seconds < 0L) return "";
        long minutes = (seconds + 59L) / 60L;
        return String.valueOf(minutes) + " min";
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
     * Send only the current (first valid) maneuver from iOS maneuverOrder.
     * AppConnectorNavi resets absent Maneuver_2/Maneuver_3 slots to NO_SYMBOL.
     */
    private void sendManeuvers(RouteGuidance.State s) throws Exception {
        int[] idxs = getManeuverIndexList(s);
        if (idxs == null || idxs.length == 0) {
            if (s.maneuverCount == 0) {
                sendNoSymbol();
            } else {
            }
            return;
        }

        /* Count valid maneuvers */
        int validCount = 0;
        int maxIdx = (s.mType != null) ? s.mType.length : 0;
        for (int i = 0; i < idxs.length && validCount < MAX_BAP_MANEUVERS; i++) {
            int idx = idxs[i];
            if (idx < 0 || idx >= maxIdx) continue;
            if (ManeuverMapper.isValidType(s.mType[idx])) validCount++;
            else break;
        }

        if (validCount == 0) {
            if (s.maneuverCount == 0) {
                sendNoSymbol();
            } else {
            }
            return;
        }

        /* Build a one-entry CombiBAPNaviManeuverDescriptor array. START_ROUTE
         * already maps to FOLLOW_STREET, so no skip is needed; keeping only
         * pos=0 ensures BAP Maneuver_1 stays aligned with DistanceToNextManeuver. */
        CombiBAPNaviManeuverDescriptor[] arr = new CombiBAPNaviManeuverDescriptor[validCount];
        int out = 0;
        for (int i = 0; i < idxs.length && out < validCount; i++) {
            int idx = idxs[i];
            if (idx < 0 || idx >= maxIdx) continue;
            if (!ManeuverMapper.isValidType(s.mType[idx])) break;
            int[] mapped = ManeuverMapper.map(
                s.mType[idx],
                s.mTurnAngle[idx],
                s.mJunctionType[idx],
                s.mDrivingSide[idx]
            );
            int zLevel = (s.mZLevel != null && idx < s.mZLevel.length) ? s.mZLevel[idx] : 0;
            byte[] sideStreets;
            if (mapped[0] == ManeuverMapper.NO_INFO || mapped[0] == ManeuverMapper.NO_SYMBOL) {
                sideStreets = new byte[0];
            } else {
                sideStreets = SideStreets.calcSideStreetsBytes(
                    s.mType[idx],
                    s.mJunctionType[idx],
                    s.mDrivingSide[idx],
                    s.mJunctionAngles[idx],
                    s.mExitAngle[idx]
                );
            }
            arr[out++] = createDescriptor(mapped[0], mapped[1], zLevel, sideStreets);
        }

        appConnectorNavi.updateManeuverDescriptor(arr);
    }

    /* ============================================================
     * Lane Guidance (FctID 24)
     * ============================================================ */

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
        if (s.lgLaneDirections == null || slot >= s.lgLaneDirections.length) return false;
        if (s.lgLaneDirections[slot] == null) return false;
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
        if (count <= 0) count = s.mLaneDirections[slot].length;
        return count > 0;
    }

    private static int laneCacheCountForSlot(RouteGuidance.State s, int slot) {
        if (slot < 0) return 0;
        int count = 0;
        if (s.lgLaneCount != null && slot < s.lgLaneCount.length) {
            count = s.lgLaneCount[slot];
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
        int max = (s.mLaneCount != null) ? s.mLaneCount.length : 0;
        if (activeIdx < max && hasMCacheLaneForSlot(s, activeIdx)
            && !hasLaneCacheForSlot(s, activeIdx)) {
            return activeIdx;
        }
        if (primary >= 0 && hasMCacheLaneForSlot(s, primary)
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
        if (lanes == null || lanes.length == 0) return false;
        int sel = (laneIdx >= 0 && laneIdx < lanes.length) ? laneIdx : 0;
        int[] angles = lanes[sel];
        return (angles != null && angles.length > 0);
    }

    private static boolean shouldEmitLane(RouteGuidance.State s, int manIdx, int laneIdx, short laneDir) {
        int[] dirs = laneDirectionsFor(s, manIdx);
        if (dirs == null || laneIdx < 0 || laneIdx >= dirs.length) return true;
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
        if (raw == 1000) return 0xFF;

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
        if (laneAngles != null && laneAngles.length > 0) {
            int sel = (laneIdx >= 0 && laneIdx < laneAngles.length) ? laneIdx : 0;
            int[] angles = laneAngles[sel];
            if (angles != null && angles.length > 0) {
                return (short)(mapRawLaneValueToDirectionCode(angles[0]) & 0xFF);
            }
        }
        return (short) 0xFF;
    }

    private static short mapLaneDirection(RouteGuidance.State s, int manIdx, int laneIdx) {
        int[] dirs = laneDirectionsFor(s, manIdx);
        if (dirs == null || laneIdx < 0 || laneIdx >= dirs.length) return (short)0xFF;
        int raw = dirs[laneIdx];

        if (raw == 1000) {
            return mapLaneDirectionFromSentinel(s, manIdx, laneIdx);
        }

        return (short)(mapRawLaneValueToDirectionCode(raw) & 0xFF);
    }

    private static byte[] mapLaneSideStreets(RouteGuidance.State s, int manIdx, int laneIdx, short laneDirection) {
        if (manIdx < 0) return new byte[0];
        int[][] lanes = laneAnglesFor(s, manIdx);
        if (lanes == null || lanes.length == 0) return new byte[0];
        int sel = (laneIdx >= 0 && laneIdx < lanes.length) ? laneIdx : 0;
        int[] angles = lanes[sel];
        if (angles == null || angles.length == 0) return new byte[0];

        int start = 0;
        int[] dirs = laneDirectionsFor(s, manIdx);
        if (dirs != null && laneIdx >= 0 && laneIdx < dirs.length && dirs[laneIdx] == 1000) {
            start = 1;
        }
        if (start >= angles.length) return new byte[0];

        int primaryDir = laneDirection & 0xFF;
        int[] codes = new int[angles.length - start];
        int n = 0;
        for (int i = start; i < angles.length; i++) {
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
     * CMD_MANEUVER packets. See docs/DEPLOYMENT.md.
     * ============================================================== */

    private volatile boolean customRendererStarted = false;
    private boolean rendererPrimed = false;

    /* Last maneuver state sent to renderer — only send CMD_MANEUVER when these change.
     * lastCrVer tracks the slot version so a new maneuver with identical type/angle
     * still triggers a push animation (e.g., consecutive left turns). */
    private int lastCrIcon = -1;
    private int lastCrDirection = -99;
    private int lastCrExitAngle = -9999;
    private int lastCrDrivingSide = -1;
    private int lastCrVer = -1;

    /* Link recovery: count consecutive renderer-side TCP send failures.  When the link to the
     * always-on renderer drops (it was re-exec'd by the framework, or lsd's link reset), drop
     * our view + re-activate on the next update.  We do NOT kill or spawn the process. */
    private int crConsecutiveSendFailures = 0;
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
                forceGfxAvailable(false);
                customRendererStarted = false;
                rendererPrimed = false;
                notifyPresentationStateChanged("renderer-presentation-lost");
            }
            if (customRendererStarted) return true;
            if (!rendererClient.isReady()) return false;

            /* Paint a deterministic frame, but do not expose displayable 98 until the peer
             * acknowledges eglSwapBuffers with FRAME_READY. */
            if (!rendererPrimed) {
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

            /* Readiness is already decided above: the renderer is connected, initialized and
             * has swapped a frame (isFrameReady()).  The ClusterService "pipeline" call that
             * used to sit here returned a constant string and could never report a failure, so
             * the branch that read it was unreachable. */

            /* BAP route info state for HUD icons.  onStart() also forces this,
             * but renderer respawn can happen mid-route without onStart(). */
            forceClusterRouteInfoState(true);

            /* Set gfxAvailable so VC enters MAP mode for LVDS video.
             * Must be after our window owns displayable 20 (so the encoder
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
    private void noteRendererSendResult(boolean ok) {
        if (ok) {
            crConsecutiveSendFailures = 0;
            return;
        }
        crConsecutiveSendFailures++;
        if (!customRendererStarted) return;
        /* Ignore failures before the first successful connect (sock=null naturally fails). */
        if (rendererClient == null || !rendererClient.everConnected()) return;
        if (crConsecutiveSendFailures < CR_SEND_FAIL_THRESHOLD) return;

        Log.w(TAG, "CR: renderer link lost (" + CR_SEND_FAIL_THRESHOLD
                + " send fails) — dropping view, will reconnect on next update");
        try { if (rendererClient != null) rendererClient.disconnectClient(); } catch (Throwable t) { /* ignore */ }
        customRendererStarted = false;
        rendererPrimed = false;
        crConsecutiveSendFailures = 0;
        notifyPresentationStateChanged("renderer-send-failures");
    }

    private synchronized void stopCustomRenderer(boolean clearRouteInfo) {
        try {
            /* Blank the popup (CMD_CLEAR) so no stale maneuver frame lingers, then deactivate the
             * cluster context + stop feeding.  Do NOT kill the renderer (always-on framework
             * service) and keep the server socket + link up for the next route in this session. */
            if (rendererClient != null) rendererClient.sendClear();
            forceGfxAvailable(false);
            if (clearRouteInfo) forceClusterRouteInfoState(false);
            customRendererStarted = false;
            rendererPrimed = false;
            Log.i(TAG, "CR: stopped (renderer blanked; stays up; server socket persists)");
        } catch (Throwable t) {
            Log.w(TAG, "CR stop failed: " + t.getClass().getName() + ": " + t.getMessage());
        }
    }

    /**
     * Push maneuver to maneuver_render ONLY if the rendered icon actually changed.
     * Returns true if CMD_MANEUVER was sent, false if suppressed (same icon).
     */
    private boolean updateRendererIfChanged(RouteGuidance.State s, int bargraphDenominatorM) {
        if (rendererClient == null || !customRendererStarted) return false;

        try {
            int[] idxs = getManeuverIndexList(s);
            if (idxs == null || idxs.length == 0 || s.maneuverCount == 0) return false;

            int maxIdx = (s.mType != null) ? s.mType.length : 0;
            /* Always show the first valid maneuver in the list (pos=0) — matches HUD.
             * HUD shows whatever BAP descriptor pos=0 is: FOLLOW_STREET when far,
             * the actual turn when approach zone updates the list. */
            int firstIdx = -1;
            for (int i = 0; i < idxs.length; i++) {
                int idx = idxs[i];
                if (idx >= 0 && idx < maxIdx && ManeuverMapper.isValidType(s.mType[idx])) {
                    firstIdx = idx;
                    break;
                }
            }
            if (firstIdx < 0) return false;

            int mt = s.mType[firstIdx];
            int icon = RendererMapper.mapIcon(mt);
            int direction = RendererMapper.mapDirection(mt, s.mTurnAngle[firstIdx], s.mDrivingSide[firstIdx]);
            int exitAngle = RendererMapper.mapExitAngle(mt, s.mTurnAngle[firstIdx]);
            int drivingSide = s.mDrivingSide[firstIdx];

            /* Skip if icon hasn't actually changed.
             * Slot version (mVer) detects maneuver transitions even when
             * type/angle are identical (e.g., consecutive left turns).
             * Exception: ARRIVED — iOS re-sends while parking; ignore version
             * to avoid re-triggering the animation. */
            int ver = s.mVer[firstIdx];
            if (icon == RendererMapper.ICON_ARRIVED) {
                if (icon == lastCrIcon && direction == lastCrDirection
                        && exitAngle == lastCrExitAngle && drivingSide == lastCrDrivingSide) {
                    return false;
                }
            } else if (icon == lastCrIcon && direction == lastCrDirection
                    && exitAngle == lastCrExitAngle && drivingSide == lastCrDrivingSide
                    && ver == lastCrVer) {
                return false;
            }
            lastCrIcon = icon;
            lastCrDirection = direction;
            lastCrExitAngle = exitAngle;
            lastCrDrivingSide = drivingSide;
            lastCrVer = ver;

            int[] junctionAngles = (s.mJunctionAngles != null && firstIdx < s.mJunctionAngles.length)
                ? s.mJunctionAngles[firstIdx] : null;

            /* Bargraph: 0-100 percentage -> 0-16 level.
             * In blink zone, send mode=1 with current level — blink thread
             * will override with full/empty toggles synced to BAP emit. */
            int bargraphLevel = 0;
            int bargraphMode = 0;
            int distM = s.distManeuverM;
            if (bargraphDenominatorM > 0 && distM > 0 && distM <= bargraphDenominatorM) {
                int pct = (distM * 100) / bargraphDenominatorM;
                if (pct < 0) pct = 0;
                if (pct > 100) pct = 100;
                bargraphLevel = (pct * 16) / 100;
                bargraphMode = 1;
            }

            int perspective = 1;  /* always 3D — 2D/3D switch disabled for now */

            boolean ok = rendererClient.sendManeuver(icon, direction, exitAngle,
                drivingSide, junctionAngles, bargraphLevel, bargraphMode,
                perspective);
            noteRendererSendResult(ok);
            return ok;
        } catch (Throwable e) {
            Log.w(TAG, "CR update failed: " + e.getClass().getName() + ": " + e.getMessage());
            noteRendererSendResult(false);
            return false;
        }
    }

    /**
     * Send ICON_APPROACH to maneuver_render — mirrors BAP sendFollowStreet().
     * Shown when maneuver exists but car is outside approach zone.
     */
    private void sendRendererFollowStreet() {
        if (rendererClient == null || !customRendererStarted) return;
        int icon = RendererMapper.ICON_APPROACH;
        if (icon == lastCrIcon) return;  /* already showing follow street */
        lastCrIcon = icon;
        lastCrDirection = 0;
        lastCrExitAngle = 0;
        lastCrDrivingSide = 0;
        lastCrVer = -1;
        try {
            boolean ok = rendererClient.sendManeuver(icon, 0, 0, 0, null, 0, 0, 1);
            noteRendererSendResult(ok);
        } catch (Throwable t) {
            Log.w(TAG, "CR follow street failed: " + t.getMessage());
            noteRendererSendResult(false);
        }
    }

    /**
     * Send standalone CMD_BARGRAPH to maneuver_render on distance-only updates.
     * No push transition — just updates the bargraph level/mode in place.
     */
    private void updateRendererBargraph(RouteGuidance.State s, int bargraphDenominatorM) {
        if (rendererClient == null || !customRendererStarted) return;

        int bargraphLevel = 0;
        int bargraphMode = 0;
        int distM = s.distManeuverM;
        if (bargraphDenominatorM > 0 && distM > 0 && distM <= bargraphDenominatorM) {
            int pct = (distM * 100) / bargraphDenominatorM;
            if (pct < 0) pct = 0;
            if (pct > 100) pct = 100;
            bargraphLevel = (pct * 16) / 100;
            if (pct < BARGRAPH_BLINK_PERCENT) {
                /* Blink zone: blink thread controls renderer via sendActionBlinkTick().
                 * Don't send mode=2 here — it would start maneuver_render's independent
                 * blink timer which drifts from BAP. Just skip; blink thread syncs both. */
                return;
            }
            bargraphMode = 1;
        }
        noteRendererSendResult(rendererClient.sendBargraph(bargraphLevel, bargraphMode));
    }

    /**
     * Force gfxAvailable on ClusterViewMode.
     * DSIKOMOGfxStreamSink has no native provider on MU1316 (gated by
     * Util.isClusterMapMOST() which requires SysConst 541==1; FPK cars
     * have it ==2), so the callback chain
     *   videoencoderservice -> DSI -> KOMOService.updateGfxState -> ClusterViewMode
     * never fires. We simulate it directly.
     *
     * Strategy 1: komoService.updateGfxState(1, 1) -- mimics DSI notification
     * Strategy 2: ClusterViewMode.setGFXAvailable(true) -- direct method call
     * Strategy 3: ClusterViewMode.gfxAvailable field reflection -- last resort
     *
     * KOMOService.dataRate is the rate that updateGfxState() passes to
     * setKOMODataRate(); without a DSI provider firing updateDataRate(),
     * dataRate is left at 0 -- so updateGfxState(1,1) would synthesise
     * setKOMODataRate(0) and never raise the MOST pacing hint.  We pre-set
     * dataRate=2 (full framerate) via reflection, then also explicitly
     * call csRef.setKOMODataRate(2) as belt-and-suspenders in case the
     * field write or vtable lookup fails.
     */
    private void forceGfxAvailable(boolean available) {
        int gfxVal = available ? 1 : 0;
        int desiredRate = available ? 2 : 0;

        /* Pre-step: write KOMOService.dataRate so updateGfxState picks it up. */
        if (komoService != null) {
            try {
                Field fRate = null;
                Class c = komoService.getClass();
                while (c != null && fRate == null) {
                    try { fRate = c.getDeclaredField("dataRate"); }
                    catch (NoSuchFieldException nsf) { c = c.getSuperclass(); }
                }
                if (fRate != null) {
                    fRate.setAccessible(true);
                    fRate.setInt(komoService, desiredRate);
                    Log.i(TAG, "KOMO: dataRate=" + desiredRate + " set via reflection");
                }
            } catch (Throwable t) {
                Log.w(TAG, "KOMO: dataRate set failed: " + t.getMessage());
            }
        }

        /* Strategy 1: updateGfxState on KOMOService */
        try {
            if (komoService != null) {
                Method m = komoService.getClass().getMethod(
                    "updateGfxState", new Class[]{int.class, int.class});
                m.invoke(komoService, new Object[]{new Integer(gfxVal), new Integer(1)});
                Log.i(TAG, "KOMO: gfxAvailable=" + available + " via updateGfxState");
            }
        } catch (Throwable t) {
            Log.w(TAG, "KOMO: updateGfxState failed: " + t.getMessage());
        }

        /* Belt-and-suspenders: csRef.setKOMODataRate(2|0) hits the FPK-patched
         * path on ClusterService directly even if Strategy 1 didn't take. */
        if (csRef != null) {
            try {
                Method m = csRef.getClass().getMethod(
                    "setKOMODataRate", new Class[]{int.class});
                m.invoke(csRef, new Object[]{new Integer(desiredRate)});
                Log.i(TAG, "KOMO: setKOMODataRate(" + desiredRate + ") explicit");
            } catch (Throwable t) {
                Log.w(TAG, "KOMO: setKOMODataRate(" + desiredRate + ") failed: " + t.getMessage());
            }
        }

        /* Strategy 2+3: direct ClusterViewMode access as backup */
        try {
            if (csRef != null) {
                /* Get ClusterViewMode from ClusterService */
                Object cvm = null;
                try {
                    Field fCvm = csRef.getClass().getDeclaredField("clusterViewMode");
                    fCvm.setAccessible(true);
                    cvm = fCvm.get(csRef);
                } catch (Throwable t) {
                    /* Try superclass if field is inherited */
                    Class sup = csRef.getClass().getSuperclass();
                    while (sup != null && cvm == null) {
                        try {
                            Field fCvm = sup.getDeclaredField("clusterViewMode");
                            fCvm.setAccessible(true);
                            cvm = fCvm.get(csRef);
                        } catch (NoSuchFieldException nsf) {
                            sup = sup.getSuperclass();
                        }
                    }
                }

                if (cvm != null) {
                    /* Strategy 2: setGFXAvailable method */
                    try {
                        Method setGfx = cvm.getClass().getMethod(
                            "setGFXAvailable", new Class[]{boolean.class});
                        setGfx.invoke(cvm, new Object[]{available ? Boolean.TRUE : Boolean.FALSE});
                        Log.i(TAG, "KOMO: gfxAvailable=" + available + " via setGFXAvailable");
                    } catch (Throwable t) {
                        /* Strategy 3: direct field */
                        try {
                            Field fGfx = cvm.getClass().getDeclaredField("gfxAvailable");
                            fGfx.setAccessible(true);
                            fGfx.setBoolean(cvm, available);
                            Log.i(TAG, "KOMO: gfxAvailable=" + available + " via field reflection");
                        } catch (Throwable t2) {
                            Log.w(TAG, "KOMO: gfxAvailable field failed: " + t2.getMessage());
                        }
                    }
                } else {
                    Log.w(TAG, "KOMO: ClusterViewMode not found on ClusterService");
                }
            }
        } catch (Throwable t) {
            Log.w(TAG, "KOMO: gfxAvailable backup failed: " + t.getMessage());
        }
    }

    /* ============================================================
     * Utilities
     * ============================================================ */

    private static long getUtcMillis() {
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

    private static int getHuNavigationTimeFormat() {
        try {
            int v = DateMetric.timeFormat;
            int bap = (v == 11) ? 1 : 0;
            return bap;
        } catch (Throwable t) {
        }
        return 0;
    }

    /**
     * Returns local timezone offset in seconds for a given UTC epoch.
     *
     * JVM default TZ on MHI2 is UTC, so TimeZone.getDefault() is useless.
     * Instead: get HU raw offset (no DST) via fw, find a matching Java TZ
     * with DST support, and use its getOffset() for DST-aware result.
     * Fallback: HU raw offset (correct except during DST transitions).
     */
    /**
     * Convert UTC epoch millis to local epoch millis using HU's DST-aware offset.
     * Uses IFrameworkAccess.convertUTCTimeToLocalTime() which internally adds
     * utcOffsetMilliseconds (timezone + DST from UTCOffset DSI callback).
     * Always correct regardless of region or DST status.
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
            return s.substring(0, lo);
        } catch (Exception e) {
            if (s.length() <= maxBytes) return s;
            return s.substring(0, maxBytes);
        }
    }

    /** Pick a compact Unicode arrow matching the current BAP maneuver direction. */
    private static String directionArrow(RouteGuidance.State s, int idx) {
        if (idx < 0 || s.mType == null || idx >= s.mType.length) return "\u2191";
        int[] mapped = ManeuverMapper.map(
            s.mType[idx], s.mTurnAngle[idx], s.mJunctionType[idx], s.mDrivingSide[idx]);
        int dir = mapped[1];
        if (dir == ManeuverMapper.DIR_LEFT)         return "\u2190";
        if (dir == ManeuverMapper.DIR_SLIGHT_LEFT)  return "\u2196";
        if (dir == ManeuverMapper.DIR_SHARP_LEFT)   return "\u2199";
        if (dir == ManeuverMapper.DIR_RIGHT)        return "\u2192";
        if (dir == ManeuverMapper.DIR_SLIGHT_RIGHT) return "\u2197";
        if (dir == ManeuverMapper.DIR_SHARP_RIGHT)  return "\u2198";
        if (dir == ManeuverMapper.DIR_UTURN)        return "\u21B6";
        return "\u2191";
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
        if (manIdx < 0 || s == null || s.mDistance == null || manIdx >= s.mDistance.length) {
            return -1;
        }
        int denominator = s.mDistance[manIdx];
        if (denominator <= 0) return -1;
        int policyCap = (prepareThresholdM * BARGRAPH_ACTION_PERCENT_OF_PREPARE) / 100;
        if (policyCap <= 0) return -1;
        if (denominator > policyCap) {
            return policyCap;
        }
        return denominator;
    }
}
