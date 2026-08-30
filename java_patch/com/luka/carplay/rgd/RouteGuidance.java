/*
 * CarPlay Route Guidance Module
 *
 * Subscribes to EVT_RGD_UPDATE on CarplayBus, parses route state,
 * sends to BAP cluster.
 *
 * Payload is text in key:type:value format carried over into the bus
 * for minimal consumer churn:
 *   @routeguidance
 *   route_state:n:<0-6>
 *   dist_maneuver_m:n:<meters>
 *   m0_type:n:<maneuver type>
 *   ...
 */
package com.luka.carplay.rgd;

import com.luka.carplay.bus.CarplayBus;
import com.luka.carplay.framework.Log;

public class RouteGuidance implements CarplayBus.Listener {

    private static final String TAG = "RouteGuidance";
    private static final int MAX_MANEUVERS = 32;
    /*
     * RouteGuidanceState values verified from MHI3 dio_manager:
     * 0=NO_ROUTE_SET, 1=ROUTE_SET, 2=ARRIVED, 3=LOADING, 4=LOCATING, 5=REROUTING, 6=PROCEED_TO_ROUTE
     */
    private static final int ROUTE_STATE_NO_ROUTE_SET = 0;
    private static final int ROUTE_STATE_ROUTE_SET = 1;
    private static final int ROUTE_STATE_REROUTING = 5;
    /*
     * MHI3 dio_manager behaviour (CRouteGuidanceUpdateProcessorImpl::isRouteGuidanceManeuverIndexGreater):
     * if routeGuidanceState == 5 then maneuver updates are always accepted.
     *
     * Note: naming in accNav docs varies (Locating/Rerouting). We match the observed check (==5).
     */

    /* State */
    private BAPBridge bap;
    private volatile boolean running;
    private boolean rgActive = false;
    /* Separate from rgActive: route intent/BAP ownership may already be active while ctx 74 is
     * still held.  It flips only after BAP update + maneuver renderer FRAME_READY. */
    private boolean presentationConfirmed = false;
    private boolean hasRouteUpdate = false;
    private boolean routeWantsActive = false;

    /* Renderer readiness is independent of iOS RGI deltas. A tiny dedicated worker advances
     * the cached presentation state on READY/FRAME_READY/death and on a bounded retry tick. */
    private final Object presentationLock = new Object();
    private Thread presentationThread;
    private boolean presentationWake;
    private boolean presentationDrivePending;
    private boolean infoPresentationRefreshPending;
    /* Guarded by presentationLock. View always writes phase 0; the steering-
     * wheel encoder press toggles it. The worker is the sole BAP publisher. */
    private int desiredInfoPhase;
    private int presentationGeneration;
    private static final long PRESENTATION_RETRY_MS = 500L;

    private final com.luka.carplay.core.ScreenModule.ViewAreaModeListener viewAreaModeListener =
        new com.luka.carplay.core.ScreenModule.ViewAreaModeListener() {
            public void onViewAreaModeChanged(int mode) {
                requestViewAreaRefresh(mode);
            }
        };

    private final com.luka.carplay.core.ScreenModule.InfoModeListener infoModeListener =
        new com.luka.carplay.core.ScreenModule.InfoModeListener() {
            public void onInfoModeToggle() {
                requestInfoModeToggle();
            }
        };

    private final BAPBridge.PresentationListener bapPresentationListener =
        new BAPBridge.PresentationListener() {
            public void onPresentationStateChanged(String reason) {
                requestPresentationCheck(reason);
            }
        };

    /* Parsed state (reused to avoid allocations) */
    private State state = new State();

    /* ============================================================
     * State Container
     * ============================================================ */

    public static class State {
        /* Dirty flags (top-level) */
        public static final int DIRTY_ROUTE_STATE      = 1 << 0;
        public static final int DIRTY_MANEUVER_STATE   = 1 << 1;
        public static final int DIRTY_MANEUVER_COUNT   = 1 << 2;
        public static final int DIRTY_MANEUVER_LIST    = 1 << 3;
        public static final int DIRTY_DIST_DEST        = 1 << 4;
        public static final int DIRTY_DIST_MAN         = 1 << 7;
        public static final int DIRTY_ETA              = 1 << 10;
        public static final int DIRTY_TIME_REMAINING   = 1 << 11;
        public static final int DIRTY_CURRENT_ROAD     = 1 << 12;
        public static final int DIRTY_DESTINATION      = 1 << 13;
        public static final int DIRTY_DISCONNECT       = 1 << 14;
        public static final int DIRTY_MANEUVER_ICON    = 1 << 15;
        public static final int DIRTY_MANEUVER_TEXT    = 1 << 16;
        public static final int DIRTY_VISIBLE_IN_APP   = 1 << 17;
        public static final int DIRTY_SOURCE_SUPPORTS_RG = 1 << 20;
        public static final int DIRTY_LANE_GUIDANCE    = 1 << 21;

        /* Per-maneuver dirty */
        public static final int MAN_DIR_ICON = 1;
        public static final int MAN_DIR_TEXT = 2;

        public int dirtyMask = 0;
        public int[] mDirty = new int[MAX_MANEUVERS];

        /* Route */
        public int routeState = -1;
        public int maneuverState = -1;
        public int maneuverCount = 0;
        public int[] maneuverOrder = null;
        public int visibleInApp = -1;
        public int sourceSupportsRg = -1;
        public int laneGuidanceShowing = -1;
        public int laneGuidanceTotal = -1;
        public int laneGuidanceIndex = -1;
        public int laneGuidanceSlot = -1;

        /* Distance to destination */
        public int distDestM = -1;

        /* Distance to maneuver */
        public int distManeuverM = -1;

        /* Time */
        public int etaSeconds = -1;
        public long timeRemainingSeconds = -1;

        /* Roads */
        public String currentRoad = null;
        public String destination = null;

        /* Disconnect reason */
        public String disconnectReason = null;

        /* Maneuvers */
        public int[] mType = new int[MAX_MANEUVERS];
        public int[] mTurnAngle = new int[MAX_MANEUVERS];
        /* Z-LevelGuidance: 0=none, 1=up, 2=down, -1=unknown */
        public int[] mZLevel = new int[MAX_MANEUVERS];
        public int[] mJunctionType = new int[MAX_MANEUVERS];
        public int[] mDrivingSide = new int[MAX_MANEUVERS];
        public int[] mDistance = new int[MAX_MANEUVERS];
        public String[] mName = new String[MAX_MANEUVERS];
        public String[] mAfterRoad = new String[MAX_MANEUVERS];
        public String[] mExitInfo = new String[MAX_MANEUVERS];
        public int[][] mJunctionAngles = new int[MAX_MANEUVERS][];
        /*
         * Junction element exit angle in degrees.
         * MHI3 dio_manager uses sentinel 1000 when the field is missing.
         */
        public int[] mExitAngle = new int[MAX_MANEUVERS];
        public int[] mLinkedLaneGuidanceIndex = new int[MAX_MANEUVERS];
        public int[] mLinkedLaneGuidanceSlot = new int[MAX_MANEUVERS];
        /* Slot version: bumps in C hook when a different iOS maneuver is assigned to a slot.
         * Detects maneuver transitions even when type/angles are identical. */
        public int[] mVer = new int[MAX_MANEUVERS];

        public int[][] mLanePositions = new int[MAX_MANEUVERS][];
        public int[] mLaneCount = new int[MAX_MANEUVERS];
        public int[][] mLaneDirections = new int[MAX_MANEUVERS][];
        public int[][] mLaneStatus = new int[MAX_MANEUVERS][];
        /*
         * Per-lane angle vectors from 0x5204 lane informations.
         * Format on the bus: lane0 comma-list, lanes separated by '|'
         * Example: "1000|40,20|"
         */
        public int[][][] mLaneAngles = new int[MAX_MANEUVERS][][];

        /*
         * Lane-guidance cache keyed by iOS composedGuidanceEventIndex.
         * These lg* arrays are separate from maneuver slots: CarPlay can send
         * lane event 0 while maneuver slot 0 currently belongs to iOS
         * maneuver 2 after reconnect/reroute.
         */
        public int[] lgIndex = new int[MAX_MANEUVERS];
        public int[][] lgLanePositions = new int[MAX_MANEUVERS][];
        public int[] lgLaneCount = new int[MAX_MANEUVERS];
        public int[][] lgLaneDirections = new int[MAX_MANEUVERS][];
        public int[][] lgLaneStatus = new int[MAX_MANEUVERS][];
        public int[][][] lgLaneAngles = new int[MAX_MANEUVERS][][];

        public State() {
            reset();
        }

        public void reset() {
            routeState = -1;
            maneuverState = -1;
            maneuverCount = 0;
            maneuverOrder = null;
            distDestM = -1;
            distManeuverM = -1;
            etaSeconds = -1;
            timeRemainingSeconds = -1;
            currentRoad = null;
            destination = null;
            disconnectReason = null;
            visibleInApp = -1;
            sourceSupportsRg = -1;
            laneGuidanceShowing = -1;
            laneGuidanceTotal = -1;
            laneGuidanceIndex = -1;
            laneGuidanceSlot = -1;
            clearAllManeuverSlots();
            clearDirty();
        }

        /* Reset per-maneuver and per-lane-cache arrays to their initial
         * empty values without touching dirty flags or top-level fields.
         * Shared between full reset() and the source_supports_rg=0
         * hard-clear path in parse(). */
        public void clearAllManeuverSlots() {
            for (int i = 0; i < MAX_MANEUVERS; i++) {
                mType[i] = -1;
                mTurnAngle[i] = -1;
                mZLevel[i] = -1;
                mJunctionType[i] = -1;
                mDrivingSide[i] = -1;
                mDistance[i] = -1;
                mName[i] = null;
                mAfterRoad[i] = null;
                mExitInfo[i] = null;
                mJunctionAngles[i] = null;
                mExitAngle[i] = 1000;
                mVer[i] = -1;
                mLinkedLaneGuidanceIndex[i] = -1;
                mLinkedLaneGuidanceSlot[i] = -1;
                mLanePositions[i] = null;
                mLaneCount[i] = -1;
                mLaneDirections[i] = null;
                mLaneStatus[i] = null;
                mLaneAngles[i] = null;
                lgIndex[i] = -1;
                lgLanePositions[i] = null;
                lgLaneCount[i] = -1;
                lgLaneDirections[i] = null;
                lgLaneStatus[i] = null;
                lgLaneAngles[i] = null;
            }
        }

        public void clearDirty() {
            dirtyMask = 0;
            for (int i = 0; i < MAX_MANEUVERS; i++) {
                mDirty[i] = 0;
            }
        }

        public void markDirty(int flag) {
            dirtyMask |= flag;
        }

        public void markManeuverDirty(int idx, int flag) {
            if (idx < 0 || idx >= MAX_MANEUVERS) return;
            mDirty[idx] |= flag;
            if ((flag & MAN_DIR_ICON) != 0) dirtyMask |= DIRTY_MANEUVER_ICON;
            if ((flag & MAN_DIR_TEXT) != 0) dirtyMask |= DIRTY_MANEUVER_TEXT;
        }

        /** Re-publish the cached full snapshot after a late renderer connection/restart. */
        public void markAllDirtyForReplay() {
            dirtyMask |= DIRTY_ROUTE_STATE | DIRTY_MANEUVER_STATE | DIRTY_MANEUVER_COUNT
                | DIRTY_MANEUVER_LIST | DIRTY_DIST_DEST | DIRTY_DIST_MAN | DIRTY_ETA
                | DIRTY_TIME_REMAINING | DIRTY_CURRENT_ROAD | DIRTY_DESTINATION
                | DIRTY_MANEUVER_ICON | DIRTY_MANEUVER_TEXT | DIRTY_VISIBLE_IN_APP
                | DIRTY_SOURCE_SUPPORTS_RG | DIRTY_LANE_GUIDANCE;
            for (int i = 0; i < MAX_MANEUVERS; i++) {
                mDirty[i] |= MAN_DIR_ICON | MAN_DIR_TEXT;
            }
        }

    }

    /* ============================================================
     * Lifecycle
     * ============================================================ */

    /**
     * Initialize with BAP service.
     */
    public boolean init(Object naviService) {
        bap = new BAPBridge();
        if (!bap.init(naviService)) {
            Log.e(TAG, "BAPBridge init failed");
            bap = null;
            return false;
        }
        bap.setPresentationListener(bapPresentationListener);
        com.luka.carplay.core.ScreenModule.setViewAreaModeListener(viewAreaModeListener);
        com.luka.carplay.core.ScreenModule.setInfoModeListener(infoModeListener);

        Log.i(TAG, "Initialized");
        return true;
    }

    /**
     * Start route guidance.
     */
    public void start() {
        if (running) return;
        synchronized (this) {
            running = true;
            rgActive = false;
            presentationConfirmed = false;
            hasRouteUpdate = false;
            routeWantsActive = false;
            state.reset();
        }
        /* stop() clears static listeners so a restarted module never retains a
         * stale RouteGuidance instance. Re-register both session inputs here. */
        com.luka.carplay.core.ScreenModule.setViewAreaModeListener(viewAreaModeListener);
        com.luka.carplay.core.ScreenModule.setInfoModeListener(infoModeListener);
        if (bap != null) bap.setPresentationListener(bapPresentationListener);
        final int generation;
        synchronized (presentationLock) {
            generation = ++presentationGeneration;
            presentationWake = false;
            presentationDrivePending = false;
            infoPresentationRefreshPending = false;
            desiredInfoPhase = 0;
            presentationThread = new Thread(new Runnable() {
                public void run() { presentationLoop(generation); }
            }, "carplay-rgi-presentation");
            presentationThread.setDaemon(true);
            presentationThread.start();
        }
        com.luka.carplay.core.ScreenModule.setNavActive(false);

        CarplayBus bus = CarplayBus.getInstance();
        bus.on(CarplayBus.EVT_RGD_UPDATE, this);
        bus.start();   /* idempotent */

        /* Cold-boot catch-up: when the phone is already connected, the hook
         * may have sent its automatic sticky snapshot before this listener
         * existed and CarplayBus correctly dropped it.  Request the same
         * snapshot again now that EVT_RGD_UPDATE has a consumer.  If the hook
         * is not connected yet this send simply fails; its normal connect-time
         * replay will arrive later, after the listener is already installed. */
        boolean syncRequested = bus.send(CarplayBus.CMD_SYNC_REQ, 0, null, 0);

        Log.i(TAG, "Started (subscribed to EVT_RGD_UPDATE, sync=" + syncRequested + ")");
    }

    /**
     * Stop route guidance.
     */
    public void stop() {
        if (!running) return;
        CarplayBus.getInstance().off(CarplayBus.EVT_RGD_UPDATE);
        Thread worker;
        synchronized (presentationLock) {
            running = false;
            presentationGeneration++;
            presentationWake = true;
            presentationDrivePending = false;
            infoPresentationRefreshPending = false;
            desiredInfoPhase = 0;
            presentationLock.notifyAll();
            worker = presentationThread;
            presentationThread = null;
        }
        synchronized (this) {
            rgActive = false;
            presentationConfirmed = false;
            hasRouteUpdate = false;
            routeWantsActive = false;
            com.luka.carplay.core.ScreenModule.setNavActive(false);
            if (bap != null) {
                bap.onStop();
                bap.onShutdown();
                bap.setPresentationListener(null);
            }
        }
        com.luka.carplay.core.ScreenModule.clearViewAreaModeListener(viewAreaModeListener);
        com.luka.carplay.core.ScreenModule.clearInfoModeListener(infoModeListener);
        if (worker != null && worker != Thread.currentThread()) {
            try { worker.join(1000L); } catch (InterruptedException e) { Thread.currentThread().interrupt(); }
        }

        Log.i(TAG, "Stopped");
    }

    public boolean isRunning() {
        return running;
    }

    /** REPLACE mode: cancel stock nav + shut RG gate for the whole CarPlay session
     *  (called on connect, not on CarPlay-nav start).  No-op if not initialized. */
    /** @return true once the RG gate is actually shut (else caller retries). */
    public boolean engageTakeover()    { return bap != null && bap.engageTakeover(); }
    public void    disengageTakeover() { if (bap != null) bap.disengageTakeover(); }

    /* ============================================================
     * CarplayBus Listener
     * ============================================================ */

    public synchronized void onFrame(int type, int flags, byte[] payload, int len) {
        if (!running) return;
        if (type != CarplayBus.EVT_RGD_UPDATE) return;
        CarplayBus.Data d = CarplayBus.parseText(payload, len);
        if (d == null) return;

        /* Parse into state (delta) */
        parse(d);
        if (state.dirtyMask == 0) {
            if (rgActive && !presentationConfirmed) requestPresentationCheck("empty-rgi-delta");
            return;
        }

        /* Check for disconnect */
        if (state.disconnectReason != null) {
            if (bap != null) { bap.onStop(); bap.onShutdown(); }
            rgActive = false;
            presentationConfirmed = false;
            routeWantsActive = false;
            com.luka.carplay.core.ScreenModule.setNavActive(false);  /* hide maneuver overlay (ctx 74) */
            hasRouteUpdate = false;
            state.reset();
            return;
        }

        /*
         * Start/stop gating:
         * - Prefer explicit active authority from visible_in_app (native-like TBT_Active semantics).
         * - Fall back to route/maneuver heuristics only when visible_in_app is unknown.
         * - iOS can emit metadata-only bus updates (component_ids/current_road/etc) even when not navigating.
         *
         * Only change RG active state when we received an activation-relevant delta.
         */
        int actMask = State.DIRTY_ROUTE_STATE
            | State.DIRTY_MANEUVER_STATE
            | State.DIRTY_MANEUVER_COUNT
            | State.DIRTY_MANEUVER_LIST
            | State.DIRTY_MANEUVER_ICON
            | State.DIRTY_VISIBLE_IN_APP
            | State.DIRTY_SOURCE_SUPPORTS_RG;
        boolean hasActivationDelta = (state.dirtyMask & actMask) != 0;

        if (hasActivationDelta) {
            boolean explicitClear = ((state.dirtyMask & State.DIRTY_MANEUVER_COUNT) != 0)
                && (state.maneuverCount == 0);

            /*
             * visible_in_app is iAP2 RouteGuidanceUpdate TLV 0x0F.  Verified against Apple's own
             * source (accessoryd 23G71, +[ACCNavigationRouteGuidanceUpdateInfo keyForType:] case 0xF):
             * it is ACCNav_RGUpdate_RouteGuidanceBeingShownInApp — whether the nav app's guidance UI
             * is currently ON SCREEN, NOT whether a route is active.  iOS sends it 0 for third-party
             * maps and whenever the nav app isn't the foreground CarPlay app, even mid-route, so it
             * must NOT override an otherwise-active route (issue #8: 3rd-party nav on iOS 26+ showed
             * cached info then vanished within a second).  Route activity comes from the separate
             * fields RouteGuidanceState / ManeuverCount / maneuver list.
             */
            boolean routeLooksActive = (state.routeState >= ROUTE_STATE_ROUTE_SET)
                || (state.maneuverCount > 0)
                || (state.maneuverOrder != null && state.maneuverOrder.length > 0);
            boolean hasActiveAuthority = (state.visibleInApp >= 0);
            boolean wantActive;
            if (hasActiveAuthority) {
                /* being-shown-in-app=1 activates; being-shown-in-app=0 only deactivates when the
                 * route itself no longer looks active (real end-of-route is still caught by the
                 * routeState==NO_ROUTE_SET hard override below). */
                wantActive = (state.visibleInApp != 0) || routeLooksActive;
            } else {
                wantActive = routeLooksActive;
                if (explicitClear && state.routeState < ROUTE_STATE_ROUTE_SET) {
                    wantActive = false;
                }
            }
            if (state.sourceSupportsRg == 0) wantActive = false;

            /* route_state=0 (NO_ROUTE_SET) is authoritative -- no route means
             * nothing to show, regardless of stale visible_in_app from C hook. */
            if (state.routeState == ROUTE_STATE_NO_ROUTE_SET) wantActive = false;
            routeWantsActive = wantActive;

            /* Re-check running: a frame captured by the bus dispatcher just before stop()
             * must not resurrect the bridge (bap.onStart re-shuts the gate + relaunches the
             * renderer) after disconnect already tore it down.  running is volatile and stop()
             * clears it before bap teardown. */
            if (!running) { state.clearDirty(); return; }

            if (wantActive && !rgActive) {
                Log.i(TAG, "RG activate: route_state=" + state.routeState
                    + " maneuver_count=" + state.maneuverCount
                    + " visible_in_app=" + state.visibleInApp
                    + " source_supports_rg=" + state.sourceSupportsRg);
                /* Stage 1: own BAP and publish the BAP start sync, but deliberately keep ctx 74.
                 * Renderer readiness remains non-blocking; the current update or presentation
                 * worker may expose 80 only after the current peer reports FRAME_READY. */
                presentationConfirmed = false;
                com.luka.carplay.core.ScreenModule.setNavActive(false);
                rgActive = bap != null && bap.onStart();
                if (!rgActive) {
                    Log.w(TAG, "RG activation pending: BAP start did not complete; keeping ctx 74");
                    requestPresentationCheck("bap-start-pending");
                }
            } else if (!wantActive && rgActive) {
                Log.i(TAG, "RG deactivate: route_state=" + state.routeState
                    + " maneuver_count=" + state.maneuverCount
                    + " visible_in_app=" + state.visibleInApp
                    + " source_supports_rg=" + state.sourceSupportsRg);
                /* With C hook debounce, transient route_state=0 never reaches
                 * Java — any deactivation here is genuine (source_supports_rg=0,
                 * visibleInApp=0, or real route end).  Full shutdown. */
                if (bap != null) { bap.onStop(); bap.onShutdown(); }
                rgActive = false;
                presentationConfirmed = false;
                /* RGI off -> hide the maneuver overlay (ctx 74 = stock native map, maneuver hidden). */
                com.luka.carplay.core.ScreenModule.setNavActive(false);
            }
        }

        if (!rgActive) {
            state.clearDirty();
            return;
        }

        /* Send only while RG is active to avoid fighting native BAP when inactive */
        if (bap != null) {
            boolean updatePublished = bap.update(state);
            boolean rendererAndBapReady = bap.isPresentationReady();
            /* First entry to ctx 80 requires THIS cached/current RGI update to have completed.
             * After that, an unrelated recoverable BAP delta error must not flicker the context;
             * renderer link loss still drops readiness immediately on the next update. */
            boolean confirmed = rendererAndBapReady && (presentationConfirmed || updatePublished);
            if (confirmed != presentationConfirmed) {
                presentationConfirmed = confirmed;
                if (confirmed) {
                    Log.i(TAG, "RG presentation CONFIRMED: BAP snapshot published + popup frame ready");
                } else {
                    Log.w(TAG, "RG presentation lost: keeping route state, falling back to ctx 74");
                }
                com.luka.carplay.core.ScreenModule.setNavActive(confirmed);
            }
            state.clearDirty();
            if (!confirmed) requestPresentationCheck("presentation-not-ready");
        }
    }

    private void requestPresentationCheck(String reason) {
        synchronized (presentationLock) {
            if (!running) return;
            presentationDrivePending = true;
            presentationWake = true;
            presentationLock.notifyAll();
        }
    }

    /**
     * View is a hard presentation boundary: reset to phase 0 and immediately
     * replay the cached real position/ETA on the serialized worker.
     */
    private void requestViewAreaRefresh(int mode) {
        synchronized (presentationLock) {
            if (!running) return;
            desiredInfoPhase = 0;
            infoPresentationRefreshPending = true;
            presentationWake = true;
            presentationLock.notifyAll();
        }
        Log.i(TAG, "viewArea info phase reset=0 queued: "
            + (mode == com.luka.carplay.core.ScreenModule.VIEWAREA_SMALLSCREEN ? "smallscreen" : "fullscreen"));
    }

    /** Steering-wheel roller press: toggle the cluster route-info line between the next turn-to
     *  street (phase 0) and the trip summary — ETA/arrival clock + remaining (phase 1).  Runs off
     *  the HMI/BAP caller thread via the presentation worker. */
    private void requestInfoModeToggle() {
        synchronized (this) {
            if (!running || !rgActive) {
                Log.i(TAG, "route-info toggle ignored without active RGI");
                return;
            }
        }
        int phase;
        synchronized (presentationLock) {
            if (!running) return;
            desiredInfoPhase ^= 1;
            phase = desiredInfoPhase;
            infoPresentationRefreshPending = true;
            presentationWake = true;
            presentationLock.notifyAll();
        }
        Log.i(TAG, "route-info phase=" + phase + " queued");
    }

    private void presentationLoop(int generation) {
        while (true) {
            boolean drivePresentation;
            int infoPhase = -1;
            synchronized (presentationLock) {
                while (running && generation == presentationGeneration && !presentationWake) {
                    try { presentationLock.wait(); }
                    catch (InterruptedException e) { Thread.currentThread().interrupt(); return; }
                }
                if (!running || generation != presentationGeneration) return;
                presentationWake = false;
                drivePresentation = presentationDrivePending;
                presentationDrivePending = false;
                if (infoPresentationRefreshPending) {
                    infoPhase = desiredInfoPhase;
                    infoPresentationRefreshPending = false;
                }
            }

            if (infoPhase >= 0) driveInfoPresentation(infoPhase);
            boolean retry = drivePresentation && driveCachedPresentation();
            if (retry) {
                synchronized (presentationLock) {
                    if (!running || generation != presentationGeneration) return;
                    if (!presentationWake) {
                        try { presentationLock.wait(PRESENTATION_RETRY_MS); }
                        catch (InterruptedException e) { Thread.currentThread().interrupt(); return; }
                    }
                    if (running && generation == presentationGeneration) {
                        presentationDrivePending = true;
                        presentationWake = true;
                    }
                }
            }
        }
    }

    /** Serialized with onFrame/stop; publishes cached FctID 19/22 immediately. */
    private synchronized void driveInfoPresentation(int phase) {
        if (!running || bap == null || !rgActive) return;
        bap.refreshInfoPresentation(state, phase);
    }

    /** Serialized with onFrame/stop so cached State and BAP are never mutated concurrently. */
    private synchronized boolean driveCachedPresentation() {
        if (!running || bap == null || !routeWantsActive) return false;

        if (!rgActive) {
            rgActive = bap.onStart();
            if (!rgActive) return true;
        }

        if (!bap.preparePresentation()) {
            if (presentationConfirmed) {
                presentationConfirmed = false;
                com.luka.carplay.core.ScreenModule.setNavActive(false);
                Log.w(TAG, "RG presentation lost: renderer not ready");
            }
            return true;
        }

        /* The primer proved the current renderer can swap. Now replay the cached RGI snapshot so
         * displayable 98 contains the real current maneuver before context 80 is exposed. */
        state.markAllDirtyForReplay();
        boolean published = bap.update(state);
        state.clearDirty();
        boolean confirmed = published && bap.isPresentationReady();
        if (confirmed != presentationConfirmed) {
            presentationConfirmed = confirmed;
            com.luka.carplay.core.ScreenModule.setNavActive(confirmed);
            if (confirmed) Log.i(TAG, "RG presentation CONFIRMED from cached snapshot");
        }
        return !confirmed;
    }

    /* ============================================================
     * Parsing
     * ============================================================ */

    private void parse(CarplayBus.Data d) {
        state.clearDirty();

        boolean isRouteUpdateDelta =
            d.has("route_state") ||
            d.has("maneuver_state") ||
            d.has("maneuver_count") ||
            d.has("maneuver_list") ||
            d.has("dist_dest_m") ||
            d.has("dist_maneuver_m") ||
            d.has("eta_seconds") ||
            d.has("time_remaining_seconds") ||
            d.has("current_road") ||
            d.has("destination") ||
            d.has("visible_in_app") ||
            d.has("component_ids") ||
            d.has("component_count") ||
            d.has("source_supports_rg");
        if (isRouteUpdateDelta) {
            hasRouteUpdate = true;
        }

        if (d.has("disconnect_reason")) {
            String v = d.str("disconnect_reason");
            if (!strEq(state.disconnectReason, v)) {
                state.disconnectReason = v;
                state.markDirty(State.DIRTY_DISCONNECT);
            }
        } else if (state.disconnectReason != null && d.size() > 0) {
            /* One-shot field: clear on any subsequent non-empty update */
            state.disconnectReason = null;
            state.markDirty(State.DIRTY_DISCONNECT);
        }
        if (d.has("source_supports_rg")) {
            int v = d.num("source_supports_rg", -1);
            if (v != state.sourceSupportsRg) {
                state.sourceSupportsRg = v;
                state.markDirty(State.DIRTY_SOURCE_SUPPORTS_RG);
                /*
                 * MHI3 (dio_manager) uses the "source supports route guidance" signal to suppress
                 * route guidance updates. For our VC/HUD bridge, treat support==0 as a hard clear
                 * to prevent stale icons/distances from lingering.
                 */
                if (v == 0) {
                    /* support=0 is the authoritative hard-clear edge even if an earlier transient
                     * route_state=0 already changed routeState.  Never let that transient suppress
                     * clearing cached slots/lane data. */
                    if (state.routeState != ROUTE_STATE_NO_ROUTE_SET) {
                        state.routeState = ROUTE_STATE_NO_ROUTE_SET;
                        state.markDirty(State.DIRTY_ROUTE_STATE);
                    }
                    if (state.maneuverCount != 0) {
                        state.maneuverCount = 0;
                        state.markDirty(State.DIRTY_MANEUVER_COUNT);
                    }
                    state.maneuverOrder = null;
                    state.markDirty(State.DIRTY_MANEUVER_LIST);
                    state.clearAllManeuverSlots();
                    state.laneGuidanceShowing = -1;
                    state.laneGuidanceTotal = -1;
                    state.laneGuidanceIndex = -1;
                    state.laneGuidanceSlot = -1;
                    state.markDirty(State.DIRTY_MANEUVER_ICON | State.DIRTY_MANEUVER_TEXT | State.DIRTY_LANE_GUIDANCE);
                }
            }
        }
        if (d.has("route_state")) {
            int v = d.num("route_state", -1);
            /*
             * route_state=0 handling.
             *
             * iOS sends transient route_state=0 during route setup and periodically
             * during active navigation (~every 30s).  A full per-slot clear here
             * causes permanent data loss because iOS doesn't resend 0x5202 for
             * already-sent maneuver indices, and it causes visible flicker on the
             * cluster (NO_SYMBOL + distance=-1 for ~1s each cycle).
             *
             * Only do a full per-slot clear when we have positive evidence of a
             * genuine route end: sourceSupportsRg=0 or disconnect_reason set.
             * Transient route_state=0 only resets count and order so BAPBridge
             * can suppress maneuver output without losing cached data.
             */
            if (v == 0) {
                if (state.routeState != 0) {
                    state.routeState = 0;
                    state.markDirty(State.DIRTY_ROUTE_STATE);
                }
                if (state.maneuverCount != 0) {
                    state.maneuverCount = 0;
                    state.markDirty(State.DIRTY_MANEUVER_COUNT);
                }
                state.maneuverOrder = null;
                state.markDirty(State.DIRTY_MANEUVER_LIST);
                state.laneGuidanceShowing = -1;
                state.laneGuidanceTotal = -1;
                state.laneGuidanceIndex = -1;
                state.laneGuidanceSlot = -1;
                state.markDirty(State.DIRTY_MANEUVER_ICON | State.DIRTY_MANEUVER_TEXT | State.DIRTY_LANE_GUIDANCE);
                /* Per-slot data (mType, mTurnAngle, etc.) is NOT cleared here.
                 * It is preserved so that when count>0 returns, BAPBridge can
                 * immediately display the correct maneuver icons.
                 * Full per-slot clear only happens on sourceSupportsRg=0 or
                 * disconnect_reason (handled elsewhere). */
            } else if (v > 0) {
                if (v != state.routeState) {
                    state.routeState = v;
                    state.markDirty(State.DIRTY_ROUTE_STATE);
                }
            }
        }
        if (d.has("maneuver_state")) {
            int v = d.num("maneuver_state", -1);
            /* MHI3 EManeuverState: 0..3 are valid (CONTINUE/INITIAL/PREPARE/EXECUTE). */
            if (v >= 0 && v <= 3 && v != state.maneuverState) {
                state.maneuverState = v;
                state.markDirty(State.DIRTY_MANEUVER_STATE);
            }
        }
        if (d.has("maneuver_count")) {
            int v = d.num("maneuver_count", 0);
            if (v != state.maneuverCount) {
                state.maneuverCount = v;
                state.markDirty(State.DIRTY_MANEUVER_COUNT);
                /*
                 * Explicit clear: iOS (and injected test frames) can send maneuver_count=0 without
                 * sending maneuver_list. Treat that as a real clear and drop cached order,
                 * otherwise BAPBridge may keep re-sending stale maneuver icons.
                 */
                if (v == 0) {
                    state.maneuverOrder = null;
                    state.markDirty(State.DIRTY_MANEUVER_LIST);
                    /*
                     * Don't clear per-slot data (mType, mTurnAngle, etc.) here.
                     *
                     * The slot data was written by the C hook from real 0x5202 ManeuverUpdate
                     * messages and corresponds to valid maneuver info.  Bus deltas only report
                     * changes, so if we clear Java state, the bus won't re-send unchanged values,
                     * and we lose the data permanently until a new 0x5202 arrives.
                     *
                     * maneuver_count=0 can be transient - iOS briefly clears the count before
                     * re-populating (observed at route start).  By preserving slot data we avoid
                     * the race where maneuver_list advances to slots whose data was cleared.
                     *
                     * Full per-slot clear still happens on:
                     *   - route_state=0  (genuine route end / reset)
                     *   - source_supports_rg=0  (hard reset)
                     */
                }
            }
        }
        if (d.has("maneuver_list")) {
            /*
             * MHI3 behaviour:
             * - maneuver_list missing -> keep previous (null or last value)
             * - maneuver_list present but empty -> treat as an explicit empty list
             */
            String raw = d.str("maneuver_list", null);
            int[] v;
            if (raw == null) {
                v = null;
            } else if (raw.length() == 0) {
                v = new int[0];
            } else {
                v = d.intList("maneuver_list");
            }
            if (!intArrayEq(state.maneuverOrder, v)) {
                state.maneuverOrder = v;
                state.markDirty(State.DIRTY_MANEUVER_LIST);
            }
        }
        if (d.has("visible_in_app")) {
            int raw = d.num("visible_in_app", -1);
            int v = (raw == 0 || raw == 1) ? raw : -1;
            if (v != state.visibleInApp) {
                state.visibleInApp = v;
                state.markDirty(State.DIRTY_VISIBLE_IN_APP);
            }
        }
        if (d.has("lane_guidance_showing")) {
            int v = d.num("lane_guidance_showing", -1);
            if (v != state.laneGuidanceShowing) {
                state.laneGuidanceShowing = v;
                state.markDirty(State.DIRTY_LANE_GUIDANCE);
            }
        }
        if (d.has("lane_guidance_total")) {
            int v = d.num("lane_guidance_total", -1);
            if (v != state.laneGuidanceTotal) {
                state.laneGuidanceTotal = v;
                state.markDirty(State.DIRTY_LANE_GUIDANCE);
            }
        }
        if (d.has("lane_guidance_index")) {
            int v = d.num("lane_guidance_index", -1);
            if (v != state.laneGuidanceIndex) {
                state.laneGuidanceIndex = v;
                state.markDirty(State.DIRTY_LANE_GUIDANCE);
            }
        }
        if (d.has("lane_guidance_slot")) {
            int v = d.num("lane_guidance_slot", -1);
            if (v != state.laneGuidanceSlot) {
                state.laneGuidanceSlot = v;
                state.markDirty(State.DIRTY_LANE_GUIDANCE);
            }
        }

        if (d.has("dist_dest_m")) {
            int v = d.num("dist_dest_m", -1);
            if (v != state.distDestM) {
                state.distDestM = v;
                state.markDirty(State.DIRTY_DIST_DEST);
            }
        }
        if (d.has("dist_maneuver_m")) {
            int v = d.num("dist_maneuver_m", -1);
            if (v != state.distManeuverM) {
                state.distManeuverM = v;
                state.markDirty(State.DIRTY_DIST_MAN);
            }
        }
        if (d.has("eta_seconds")) {
            int v = d.num("eta_seconds", -1);
            if (v != state.etaSeconds) {
                state.etaSeconds = v;
                state.markDirty(State.DIRTY_ETA);
            }
        }
        if (d.has("time_remaining_seconds")) {
            long v = d.num64("time_remaining_seconds", -1);
            if (v != state.timeRemainingSeconds) {
                state.timeRemainingSeconds = v;
                state.markDirty(State.DIRTY_TIME_REMAINING);
            }
        }
        if (d.has("current_road")) {
            String v = d.str("current_road");
            if (!strEq(state.currentRoad, v)) {
                state.currentRoad = v;
                state.markDirty(State.DIRTY_CURRENT_ROAD);
            }
        }
        if (d.has("destination")) {
            String v = d.str("destination");
            if (!strEq(state.destination, v)) {
                state.destination = v;
                state.markDirty(State.DIRTY_DESTINATION);
            }
        }

        /* Parse lane-guidance cache lg0_, lg1_, ... */
        for (int i = 0; i < MAX_MANEUVERS; i++) {
            String p = "lg" + i + "_";

            if (d.has(p + "index")) {
                int v = d.num(p + "index", -1);
                if (v != state.lgIndex[i]) {
                    state.lgIndex[i] = v;
                    state.markDirty(State.DIRTY_LANE_GUIDANCE);
                }
            }
            if (d.has(p + "lane_count")) {
                int v = d.num(p + "lane_count", -1);
                if (v != state.lgLaneCount[i]) {
                    state.lgLaneCount[i] = v;
                    state.markDirty(State.DIRTY_LANE_GUIDANCE);
                }
            }
            if (d.has(p + "lane_positions")) {
                int[] v = d.intList(p + "lane_positions");
                if (!intArrayEq(state.lgLanePositions[i], v)) {
                    state.lgLanePositions[i] = v;
                    state.markDirty(State.DIRTY_LANE_GUIDANCE);
                }
            }
            if (d.has(p + "lane_directions")) {
                int[] v = d.intList(p + "lane_directions");
                if (!intArrayEq(state.lgLaneDirections[i], v)) {
                    state.lgLaneDirections[i] = v;
                    state.markDirty(State.DIRTY_LANE_GUIDANCE);
                }
            }
            if (d.has(p + "lane_status")) {
                int[] v = d.intList(p + "lane_status");
                if (!intArrayEq(state.lgLaneStatus[i], v)) {
                    state.lgLaneStatus[i] = v;
                    state.markDirty(State.DIRTY_LANE_GUIDANCE);
                }
            }
            if (d.has(p + "lane_angles")) {
                int[][] v = parseLaneAnglesMatrix(d.str(p + "lane_angles", null));
                if (!intMatrixEq(state.lgLaneAngles[i], v)) {
                    state.lgLaneAngles[i] = v;
                    state.markDirty(State.DIRTY_LANE_GUIDANCE);
                }
            }
        }

        /* Parse maneuvers m0_, m1_, ... */
        for (int i = 0; i < MAX_MANEUVERS; i++) {
            String p = "m" + i + "_";

            /* MHI3 gating: discard ALL per-index maneuver data for this slot when we have no route
             * update yet, or the maneuver list is explicitly empty (canProcessManeuverIndex is
             * index-independent — it's a global "accept maneuver data right now?" check).
             * Gate on the predicate ALONE: the old key-list form omitted z_level/exit_info/
             * exit_angle/lane_count/lane_positions/lane_directions/lane_status/linked_lane_guidance_*,
             * so a delta carrying only those keys bypassed the gate and polluted a rejected slot
             * (audit #7).  Predicate-only is also future-proof — new keys are gated automatically. */
            if (!canProcessManeuverIndex(i)) {
                continue;
            }

            if (d.has(p + "type")) {
                int v = d.num(p + "type", -1);
                if (v != state.mType[i]) {
                    state.mType[i] = v;
                    state.markManeuverDirty(i, State.MAN_DIR_ICON);
                }
            }
            if (d.has(p + "turn_angle")) {
                int v = d.num(p + "turn_angle", -1);
                if (v != state.mTurnAngle[i]) {
                    state.mTurnAngle[i] = v;
                    /*
                     * Our C hook currently publishes the iAP2 "exit angle" field under the
                     * legacy key name "turn_angle". Keep an explicit copy so side-streets
                     * mapping can use MHI3-like naming.
                     */
                    state.mExitAngle[i] = v;
                    state.markManeuverDirty(i, State.MAN_DIR_ICON);
                }
            }
            if (d.has(p + "z_level")) {
                int v = d.num(p + "z_level", -1);
                if (v != state.mZLevel[i]) {
                    state.mZLevel[i] = v;
                    state.markManeuverDirty(i, State.MAN_DIR_ICON);
                }
            }
            if (d.has(p + "zlevel")) {
                int v = d.num(p + "zlevel", -1);
                if (v != state.mZLevel[i]) {
                    state.mZLevel[i] = v;
                    state.markManeuverDirty(i, State.MAN_DIR_ICON);
                }
            }
            if (d.has(p + "junction_type")) {
                int v = d.num(p + "junction_type", -1);
                if (v != state.mJunctionType[i]) {
                    state.mJunctionType[i] = v;
                    state.markManeuverDirty(i, State.MAN_DIR_ICON);
                }
            }
            if (d.has(p + "driving_side")) {
                int v = d.num(p + "driving_side", -1);
                if (v != state.mDrivingSide[i]) {
                    state.mDrivingSide[i] = v;
                    state.markManeuverDirty(i, State.MAN_DIR_ICON);
                }
            }
            if (d.has(p + "distance")) {
                int v = d.num(p + "distance", -1);
                if (v != state.mDistance[i]) {
                    state.mDistance[i] = v;
                    /*
                     * MHI3 computes bargraph in the maneuver-update path (tbt_renderer updateCurrentManeuver)
                     * using distanceBetweenManeuver. Trigger a distance+bargraph refresh when this changes.
                     */
                    state.markDirty(State.DIRTY_DIST_MAN);
                }
            }
            if (d.has(p + "name")) {
                String v = d.str(p + "name");
                if (!strEq(state.mName[i], v)) {
                    state.mName[i] = v;
                    state.markManeuverDirty(i, State.MAN_DIR_TEXT);
                }
            }
            if (d.has(p + "after_road")) {
                String v = d.str(p + "after_road");
                if (!strEq(state.mAfterRoad[i], v)) {
                    state.mAfterRoad[i] = v;
                    state.markManeuverDirty(i, State.MAN_DIR_TEXT);
                }
            }
            if (d.has(p + "exit_info")) {
                String v = d.str(p + "exit_info");
                if (!strEq(state.mExitInfo[i], v)) {
                    state.mExitInfo[i] = v;
                    state.markManeuverDirty(i, State.MAN_DIR_TEXT);
                }
            }
            if (d.has(p + "junction_angles")) {
                int[] newAngles = d.intList(p + "junction_angles");
                if (!intArrayEq(state.mJunctionAngles[i], newAngles)) {
                    state.mJunctionAngles[i] = newAngles;
                    /* Side streets influence the maneuver rendering. */
                    state.markManeuverDirty(i, State.MAN_DIR_ICON);
                }
            }
            if (d.has(p + "exit_angle")) {
                int v = d.num(p + "exit_angle", 1000);
                if (v != state.mExitAngle[i]) {
                    state.mExitAngle[i] = v;
                    state.markManeuverDirty(i, State.MAN_DIR_ICON);
                }
            }
            if (d.has(p + "ver")) {
                int v = d.num(p + "ver", -1);
                if (v != state.mVer[i]) {
                    state.mVer[i] = v;
                    /* Slot was reassigned to a different iOS maneuver.
                     * Force full icon+text refresh even if type/angles are identical. */
                    state.markManeuverDirty(i, State.MAN_DIR_ICON | State.MAN_DIR_TEXT);
                }
            }
            if (d.has(p + "lane_count")) {
                int v = d.num(p + "lane_count", -1);
                if (v != state.mLaneCount[i]) {
                    state.mLaneCount[i] = v;
                    state.markDirty(State.DIRTY_LANE_GUIDANCE);
                }
            }
            if (d.has(p + "linked_lane_guidance_index")) {
                int v = d.num(p + "linked_lane_guidance_index", -1);
                if (v != state.mLinkedLaneGuidanceIndex[i]) {
                    state.mLinkedLaneGuidanceIndex[i] = v;
                    state.markDirty(State.DIRTY_LANE_GUIDANCE);
                }
            }
            if (d.has(p + "linked_lane_guidance_slot")) {
                int v = d.num(p + "linked_lane_guidance_slot", -1);
                if (v != state.mLinkedLaneGuidanceSlot[i]) {
                    state.mLinkedLaneGuidanceSlot[i] = v;
                    state.markDirty(State.DIRTY_LANE_GUIDANCE);
                }
            }
            if (d.has(p + "lane_positions")) {
                int[] v = d.intList(p + "lane_positions");
                if (!intArrayEq(state.mLanePositions[i], v)) {
                    state.mLanePositions[i] = v;
                    state.markDirty(State.DIRTY_LANE_GUIDANCE);
                }
            }
            if (d.has(p + "lane_directions")) {
                int[] v = d.intList(p + "lane_directions");
                if (!intArrayEq(state.mLaneDirections[i], v)) {
                    state.mLaneDirections[i] = v;
                    state.markDirty(State.DIRTY_LANE_GUIDANCE);
                }
            }
            if (d.has(p + "lane_status")) {
                int[] v = d.intList(p + "lane_status");
                if (!intArrayEq(state.mLaneStatus[i], v)) {
                    state.mLaneStatus[i] = v;
                    state.markDirty(State.DIRTY_LANE_GUIDANCE);
                }
            }
            if (d.has(p + "lane_angles")) {
                int[][] v = parseLaneAnglesMatrix(d.str(p + "lane_angles", null));
                if (!intMatrixEq(state.mLaneAngles[i], v)) {
                    state.mLaneAngles[i] = v;
                    state.markDirty(State.DIRTY_LANE_GUIDANCE);
                }
            }
        }

    }

    /**
     * Match dio_manager CRouteGuidanceUpdateProcessorImpl::isRouteGuidanceManeuverIndexGreater.
     */
    private boolean canProcessManeuverIndex(int idx) {
        if (!hasRouteUpdate) {
            return false;
        }
        if (state.maneuverOrder == null) {
            /* Null list -> allow processing. */
            return true;
        }
        if (state.maneuverOrder.length == 0) {
            /* Explicit empty list -> reject. */
            return false;
        }
        /*
         * Our C hook remaps iOS maneuver indexes into fixed slots [0..MAX_MANEUVERS-1] and applies the
         * real MHI3 index gating using the original iOS indexes. At this layer, "idx" is a slot index,
         * so comparing it to min(maneuverOrder) would be meaningless and could drop valid updates.
         */
        return true;
    }

    private static boolean strEq(String a, String b) {
        if (a == null) return b == null;
        return a.equals(b);
    }

    private static boolean intArrayEq(int[] a, int[] b) {
        if (a == b) return true;
        if (a == null || b == null) return false;
        if (a.length != b.length) return false;
        for (int i = 0; i < a.length; i++) {
            if (a[i] != b[i]) return false;
        }
        return true;
    }

    private static boolean intMatrixEq(int[][] a, int[][] b) {
        if (a == b) return true;
        if (a == null || b == null) return false;
        if (a.length != b.length) return false;
        for (int i = 0; i < a.length; i++) {
            if (!intArrayEq(a[i], b[i])) return false;
        }
        return true;
    }

    private static int[][] parseLaneAnglesMatrix(String raw) {
        if (raw == null || raw.length() == 0) return null;

        int lanes = 1;
        for (int i = 0; i < raw.length(); i++) {
            if (raw.charAt(i) == '|') lanes++;
        }

        int[][] out = new int[lanes][];
        int start = 0;
        int lane = 0;
        for (int i = 0; i <= raw.length() && lane < lanes; i++) {
            if (i == raw.length() || raw.charAt(i) == '|') {
                out[lane++] = parseCsvIntList(raw.substring(start, i));
                start = i + 1;
            }
        }
        return out;
    }

    private static int[] parseCsvIntList(String s) {
        if (s == null) return null;
        s = s.trim();
        if (s.length() == 0) return new int[0];

        int commas = 0;
        for (int i = 0; i < s.length(); i++) {
            if (s.charAt(i) == ',') commas++;
        }

        int[] out = new int[commas + 1];
        int start = 0;
        int idx = 0;
        for (int i = 0; i <= s.length() && idx < out.length; i++) {
            if (i == s.length() || s.charAt(i) == ',') {
                String part = s.substring(start, i).trim();
                if (part.length() == 0) return null;
                try {
                    out[idx++] = Integer.parseInt(part);
                } catch (Exception e) {
                    return null;
                }
                start = i + 1;
            }
        }
        return out;
    }

    private static String dirtyToString(State s) {
        StringBuffer sb = new StringBuffer();
        appendIf(sb, (s.dirtyMask & State.DIRTY_ROUTE_STATE) != 0, "route_state");
        appendIf(sb, (s.dirtyMask & State.DIRTY_MANEUVER_STATE) != 0, "maneuver_state");
        appendIf(sb, (s.dirtyMask & State.DIRTY_MANEUVER_COUNT) != 0, "maneuver_count");
        appendIf(sb, (s.dirtyMask & State.DIRTY_MANEUVER_LIST) != 0, "maneuver_list");
        appendIf(sb, (s.dirtyMask & State.DIRTY_DIST_DEST) != 0, "dist_dest_m");
        appendIf(sb, (s.dirtyMask & State.DIRTY_DIST_MAN) != 0, "dist_maneuver_m");
        appendIf(sb, (s.dirtyMask & State.DIRTY_ETA) != 0, "eta_seconds");
        appendIf(sb, (s.dirtyMask & State.DIRTY_TIME_REMAINING) != 0, "time_remaining");
        appendIf(sb, (s.dirtyMask & State.DIRTY_CURRENT_ROAD) != 0, "current_road");
        appendIf(sb, (s.dirtyMask & State.DIRTY_DESTINATION) != 0, "destination");
        appendIf(sb, (s.dirtyMask & State.DIRTY_DISCONNECT) != 0, "disconnect_reason");
        appendIf(sb, (s.dirtyMask & State.DIRTY_VISIBLE_IN_APP) != 0, "visible_in_app");
        appendIf(sb, (s.dirtyMask & State.DIRTY_SOURCE_SUPPORTS_RG) != 0, "source_supports_rg");
        appendIf(sb, (s.dirtyMask & State.DIRTY_LANE_GUIDANCE) != 0, "lane_guidance");

        boolean firstMan = true;
        for (int i = 0; i < MAX_MANEUVERS; i++) {
            int md = s.mDirty[i];
            if (md == 0) continue;
            if (firstMan) {
                appendSep(sb);
                sb.append("maneuver=");
                firstMan = false;
            } else {
                sb.append(',');
            }
            sb.append(i);
            sb.append(':');
            if ((md & State.MAN_DIR_ICON) != 0) sb.append('I');
            if ((md & State.MAN_DIR_TEXT) != 0) sb.append('T');
        }

        return sb.toString();
    }

    private static void appendIf(StringBuffer sb, boolean cond, String label) {
        if (!cond) return;
        appendSep(sb);
        sb.append(label);
    }

    private static void appendSep(StringBuffer sb) {
        if (sb.length() > 0) sb.append(',');
    }
}
