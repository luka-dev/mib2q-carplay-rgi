/*
 * CarPlay Route Guidance - BAP Bridge
 *
 * Translates route guidance state to BAP protocol calls via AppConnectorNavi,
 * and pushes KOMO data to PresentationController for LVDS video rendering.
 *
 * BAP path: ManeuverDescriptor, distance, street, lane guidance, ETA → VC/HUD text overlays.
 * KOMO path: RouteInfoElement → KOMOService → PresentationController → video encoder → MOST → VC LVDS map.
 *
 */
package com.luka.carplay.routeguidance;

import com.luka.carplay.CarPlayHook;
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
import org.dsi.ifc.komoview.ManeuverElement;
import org.dsi.ifc.komoview.RouteInfoElement;

public class BAPBridge {

    private static final String TAG = "BAPBridge";
    /* RGType sent to cluster: 0=RGI (BAP ManeuverDescriptor icons for HUD).
     * FPK has rgType=4 hardcoded in CombiBAPListener — our BAP rgType=0 is for the
     * AppConnectorNavi FSG sync flow, not for view mode selection. */
    private static final int ACTIVE_RGTYPE = 4;  /* MAP mode for FPK — VC shows LVDS video area */
    /* KOMO data rate: 2=full bandwidth (hint 2 on ChoiceModel(1,168)) */
    private static final int KOMO_DATA_RATE_FULL = 2;
    private static final boolean BAP_TRACE_ENABLED = true;

    /* ExitView variants (BAP spec FctID 49) */
    private static final int EXITVIEW_EU = 0;
    private static final int EXITVIEW_NAR = 1;
    private static final int EXITVIEW_ROW = 2;
    private static final int EXITVIEW_ASIA = 3;

    /* BAP supports 3 maneuver slots */
    private static final int MAX_BAP_MANEUVERS = 3;

    /*
     * Fixed maneuver thresholds (meters).
     * These are intentionally static (no speed/time conversion at runtime).
     */
    private static final int CITY_PREPARE_THRESHOLD_M = 1500;
    private static final int HIGHWAY_PREPARE_THRESHOLD_M = 3000;
    private static final int BARGRAPH_ACTION_PERCENT_OF_PREPARE = 15;
    private static final int BARGRAPH_BLINK_PERCENT = 20;
    private static final int ACTION_BLINK_INTERVAL_MS = 600;

    private CombiBAPServiceNavi appConnectorNavi;
    private final BAPDistanceFormatter distanceFormatter =
        new BAPDistanceFormatter(new SilentLogChannel());

    private boolean initialized = false;

    /*
     * Approach mode: true when distM <= prepareThreshold (showing real maneuver icon),
     * false when further away (showing FOLLOW_STREET).
     */
    private boolean inApproachZone = false;
    private String latchedTurnToText = "";
    /* Call-for-action blink phase: true=100%, false=0% */
    private boolean actionBlinkFull = true;
    private final Object distanceToManeuverLock = new Object();
    private boolean hasLastDistM = false;
    private int lastDistM = 0;
    private boolean lastBarOn = false;
    private int lastBar = 0;
    private Thread actionBlinkThread;
    private boolean actionBlinkThreadRunning = false;
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
    private GatedCombiService gatedService;

    private ClusterService csRef;
    private KOMOService komoService;
    private boolean komoStarted = false;

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

    private synchronized boolean isActionBlinkThreadRunning() {
        return actionBlinkThreadRunning;
    }

    private void startActionBlinkThread() {
        synchronized (this) {
            if (actionBlinkThreadRunning) return;
            actionBlinkThreadRunning = true;
            actionBlinkThread = new Thread(new Runnable() {
                public void run() {
                    actionBlinkLoop();
                }
            }, "BAPActionBlink");
            actionBlinkThread.setDaemon(true);
            actionBlinkThread.start();
        }
        Log.d(TAG, "Action blink timer started (" + ACTION_BLINK_INTERVAL_MS + "ms)");
    }

    private void stopActionBlinkThread() {
        Thread t;
        synchronized (this) {
            actionBlinkThreadRunning = false;
            t = actionBlinkThread;
            actionBlinkThread = null;
            resetActionBlinkState();
        }
        if (t != null) {
            t.interrupt();
            try { t.join(500); } catch (InterruptedException e) { /* ignore */ }
            if (t.isAlive()) Log.w(TAG, "Blink thread still alive after join(500)");
        }
        Log.d(TAG, "Action blink timer stopped");
    }

    private void actionBlinkLoop() {
        while (true) {
            try {
                Thread.sleep(ACTION_BLINK_INTERVAL_MS);
            } catch (InterruptedException e) {
                /* stop check below */
            }
            if (!isActionBlinkThreadRunning()) break;
            sendActionBlinkTick();
        }
    }

    private void sendActionBlinkTick() {
        int distM;
        int bargraphDenominatorM;
        int bargraph;
        boolean armed;
        synchronized (this) {
            armed = blinkArmed;
            distM = blinkDistM;
            bargraphDenominatorM = blinkBargraphDenominatorM;
            if (!armed || distM <= 0 || bargraphDenominatorM <= 0 || distM > bargraphDenominatorM) return;
            int linBargraph = (distM * 100) / bargraphDenominatorM;
            if (linBargraph < 0) linBargraph = 0;
            if (linBargraph > 100) linBargraph = 100;
            if (linBargraph >= BARGRAPH_BLINK_PERCENT) {
                actionBlinkFull = true;
                return;
            }
            bargraph = actionBlinkFull ? 100 : 0;
            actionBlinkFull = !actionBlinkFull;
        }

        try {
            sendDistanceToManeuverRaw(distM, true, bargraph);
        } catch (Exception e) {
            Log.e(TAG, "Action blink tick failed", e);
        }
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
        traceBap("updateDistanceToNextManeuver",
            fd.value + "," + fd.unit + "," + bargraphOn + "," + bargraph);
        appConnectorNavi.updateDistanceToNextManeuver(fd.value, fd.unit, bargraphOn, bargraph);
    }

    private void sendDistanceToDestinationRaw(int meters, boolean isStopOver) {
        FormattedDistance fd = formatDistanceToDestination(meters);
        traceBap("updateDistanceToDestination", fd.value + "," + fd.unit + "," + isStopOver);
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

    private void traceBap(String call, String args) {
        if (!BAP_TRACE_ENABLED) return;
        Log.d(TAG, "[BAP] " + call + "(" + args + ")");
    }

    private void traceDescriptor(int outPos, int idx, int type, int main, int dir, int zLevel, byte[] sideStreets) {
        if (!BAP_TRACE_ENABLED) return;
        StringBuffer sb = new StringBuffer();
        sb.append("pos=").append(outPos)
          .append(" idx=").append(idx)
          .append(" type=").append(type)
          .append(" main=").append(main)
          .append(" dir=").append(dir)
          .append(" z=").append(zLevel)
          .append(" side=[");
        if (sideStreets != null) {
            for (int i = 0; i < sideStreets.length; i++) {
                if (i > 0) sb.append(',');
                sb.append(sideStreets[i] & 0xFF);
            }
        }
        sb.append(']');
        Log.d(TAG, "[BAP] descriptor " + sb.toString());
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

            /* Install native route-guidance gate on CombiBAPListener.combiservice. */
            try {
                CombiBAPServiceNavi realSvc = cs.getCombiBAPListenerCombiService();
                if (realSvc != null) {
                    gatedService = new GatedCombiService(realSvc);
                    cs.setCombiBAPListenerCombiService(gatedService);
                    Log.i(TAG, "Route-guidance gate installed on CombiBAPListener");
                }
            } catch (Exception ex) {
                Log.d(TAG, "Route-guidance gate install failed (non-fatal): " + ex.getMessage());
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
            Log.d(TAG, "force container rgActive failed: " + e.getMessage());
        }

        if (active) {
            try { csRef.updateRGIString(new short[]{1}); }
            catch (Exception e) { Log.d(TAG, "force rgiValid=true failed: " + e.getMessage()); }
        } else {
            try { csRef.updateRGIString(null); }
            catch (Exception e) { Log.d(TAG, "force rgiValid=false failed: " + e.getMessage()); }
        }

        try { csRef.triggerRefreshRGIValid(); }
        catch (Exception e) { Log.d(TAG, "refreshRGIValid failed: " + e.getMessage()); }
    }


    /* ============================================================
     * Lifecycle
     * ============================================================ */

    public void onStart() {
        if (!initialized) return;

        try {
            inApproachZone = false;
            latchedTurnToText = "";
            resetActionBlinkState();
            synchronized (distanceToManeuverLock) {
                hasLastDistM = false;
                lastDistM = 0;
                lastBarOn = false;
                lastBar = 0;
            }
            startActionBlinkThread();

            /*
             * Lazy-init cluster hooks (native stream gate).
             * Navigation singleton may not be available at init() time.
             */
            if (gatedService == null) {
                initClusterAccess();
            }

            /* Block native route-guidance BAP stream during CarPlay RG. */
            if (gatedService != null) gatedService.blockRouteGuidance = true;

            /* Force cluster state flags (rgActive, rgiValid, viewMode) BEFORE
             * sending any BAP data so AppConnectorNavi accepts our sends. */
            forceClusterRouteInfoState(true);

            /*
             * Guidance start — BAP text overlays for HUD + VC text,
             * KOMO pipeline for LVDS video maneuver rendering.
             *
             * 1. RGStatus(1) - FctID 17 → triggers startSync(0) for {17,39,23,18,49}
             * 2. Complete sync(0) window: rgType(39), descriptor(23), distance(18), exitView(49)
             */
            traceBap("updateRGStatus", "1");
            appConnectorNavi.updateRGStatus(1);                                      /* FctID 17 → sync(0) */
            traceBap("updateActiveRGType", String.valueOf(ACTIVE_RGTYPE));
            appConnectorNavi.updateActiveRGType(ACTIVE_RGTYPE);                      /* FctID 39 */

            /* Sync(0) FctIDs: descriptor, distance, exitView */
            sendFollowStreet();                                                      /* FctID 23 */
            sendDistanceToManeuverRaw(0, false, 0);                                  /* FctID 18 */
            sendExitView();                                                          /* FctID 49 */

            /* Non-sync FctIDs */
            traceBap("updateCurrentPositionInfo", "\"\"");
            appConnectorNavi.updateCurrentPositionInfo("");                           /* FctID 19 */

            /* KOMO: start video encoder and LVDS rendering pipeline */
            startKOMO();

            Log.i(TAG, "Started (rgType=" + ACTIVE_RGTYPE + ", komo=" + komoStarted + ")");

        } catch (Exception e) {
            Log.e(TAG, "onStart error", e);
        }
    }

    public void onStop() {
        if (!initialized) return;

        try {
            stopActionBlinkThread();
            inApproachZone = false;
            latchedTurnToText = "";

            /*
             * Guidance stop:
             * 1. RGStatus(0) - triggers sync(0) for {17,39,23,18,49}
             * 2. Complete sync(0) window: descriptor(23), distance(18), exitView(49)
             * 3. Non-sync FctIDs last
             */
            traceBap("updateRGStatus", "0");
            appConnectorNavi.updateRGStatus(0);                                      /* FctID 17 → sync(0) */
            traceBap("updateActiveRGType", "0");
            appConnectorNavi.updateActiveRGType(0);                                  /* FctID 39 */

            /* Sync(0) FctIDs */
            sendNoSymbol();                                                          /* FctID 23 */
            sendDistanceToManeuverRaw(0, false, 0);                                  /* FctID 18 */
            sendExitView();                                                          /* FctID 49 */

            /* Non-sync FctIDs */
            traceBap("updateManeuverState", "0");
            appConnectorNavi.updateManeuverState(0);                                 /* FctID 24 */
            traceBap("updateCurrentPositionInfo", "\"\"");
            appConnectorNavi.updateCurrentPositionInfo("");                           /* FctID 19 */
            sendDistanceToDestinationRaw(0, false);                                  /* FctID 21 */
            traceBap("updateTimeToDestination", "0,0,-1");
            appConnectorNavi.updateTimeToDestination(0, 0, -1);                      /* FctID 22 */
            traceBap("updateLaneGuidance", "[],false");
            appConnectorNavi.updateLaneGuidance(false, new CombiBAPNaviLaneGuidanceData[0]); /* FctID 24 */

            /* KOMO: stop video encoder and clear maneuver data */
            stopKOMO();

            /* Unblock native route-guidance BAP stream. */
            if (gatedService != null) gatedService.blockRouteGuidance = false;

            /* Restore cluster state flags. */
            forceClusterRouteInfoState(false);

            Log.d(TAG, "Stopped");
        } catch (Exception e) {
            Log.e(TAG, "onStop error", e);
        }
    }

    /* ============================================================
     * Update
     * ============================================================ */

    public void update(RouteGuidance.State s) {
        if (!initialized || s == null) return;

        int dirty = s.dirtyMask;
        if (dirty == 0) return;
        Log.d(TAG, "Update delta mask=0x" + Integer.toHexString(dirty));

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

            /* Highway-aware prepare thresholds (meters). */
            boolean isHighway = (type0 >= 0) && ManeuverMapper.isHighwayManeuver(type0);
            int prepareThreshold  = isHighway ? HIGHWAY_PREPARE_THRESHOLD_M : CITY_PREPARE_THRESHOLD_M;
            int bargraphDenominatorM = getBargraphDenominatorM(s, firstIdx, prepareThreshold);
            updateActionBlinkContext(
                hasManeuverList && showManeuver && (bargraphDenominatorM > 0) && !shouldClearManeuver && !explicitClear,
                distM, bargraphDenominatorM);

            /*
             * Approach zone detection.
             */
            if ((dirty & RouteGuidance.State.DIRTY_MANEUVER_LIST) != 0) {
                inApproachZone = false;
            }
            boolean hasUsableDistance = (distM > 0);
            boolean nowApproach = hasUsableDistance ? (distM <= prepareThreshold) : inApproachZone;
            boolean approachChanged = hasUsableDistance
                && (nowApproach != inApproachZone)
                && showManeuver && hasManeuverList;
            if (approachChanged) {
                dirty |= RouteGuidance.State.DIRTY_MANEUVER_ICON
                       | RouteGuidance.State.DIRTY_DIST_MAN
                       | RouteGuidance.State.DIRTY_LANE_GUIDANCE
                       | RouteGuidance.State.DIRTY_MANEUVER_TEXT;
                inApproachZone = nowApproach;
                latchedTurnToText = "";
                Log.i(TAG, "Approach zone " + (nowApproach ? "ENTER" : "EXIT")
                    + " (dist=" + distM + "m, highway=" + isHighway
                    + ", prepThr=" + prepareThreshold + ", barDen=" + bargraphDenominatorM + ")");
            }

            if (explicitClear || shouldClearManeuver) {
                latchedTurnToText = "";
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
                        Log.d(TAG, "Slot data pending for list, keeping last icons (count=" + s.maneuverCount + ")");
                    }
                } else if (shouldClearManeuver) {
                    sendNoSymbol();
                    descriptorSent = true;
                } else if (hasAnyManeuver) {
                    Log.d(TAG, "Maneuver list missing (count=" + s.maneuverCount + "), keep last icons");
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
                            /*
                             * Blink zone: the blink thread is the sole sender of FctID 18.
                             * No forced send here to avoid jitter with periodic blink sends.
                             */
                        } else {
                            sendDistanceToManeuverRaw(distM, bargraphOn, bargraph);
                        }
                    } else {
                        actionBlinkFull = true;
                        sendDistanceToManeuverRaw(distM, bargraphOn, bargraph);
                    }
                }
            }

            /* 4. Street text (FctID 19 - CurrentPositionInfo) */
            if ((dirty & (RouteGuidance.State.DIRTY_CURRENT_ROAD |
                          RouteGuidance.State.DIRTY_MANEUVER_TEXT |
                          RouteGuidance.State.DIRTY_MANEUVER_LIST |
                          RouteGuidance.State.DIRTY_MANEUVER_ICON)) != 0) {
                String road;
                boolean turnTextMode = inApproachZone && !explicitClear && !shouldClearManeuver;
                if (turnTextMode) {
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
                    if (candidate.length() > 0) {
                        latchedTurnToText = "\u2192 " + candidate;
                    }
                    road = latchedTurnToText;
                } else {
                    latchedTurnToText = "";
                    road = (s.currentRoad != null) ? s.currentRoad : "";
                }
                road = limitUtf8(road, 96);
                traceBap("updateCurrentPositionInfo", "\"" + road + "\"");
                appConnectorNavi.updateCurrentPositionInfo(road);
            }

            /*
             * 5. Maneuver state (FctID 24) - for HUD
             */
            if ((dirty & (RouteGuidance.State.DIRTY_MANEUVER_STATE |
                          RouteGuidance.State.DIRTY_MANEUVER_ICON |
                          RouteGuidance.State.DIRTY_MANEUVER_LIST |
                          RouteGuidance.State.DIRTY_MANEUVER_COUNT |
                          RouteGuidance.State.DIRTY_DIST_MAN)) != 0) {
                if (explicitClear) {
                    traceBap("updateManeuverState", "0");
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
                    traceBap("updateManeuverState", String.valueOf(bapState));
                    appConnectorNavi.updateManeuverState(bapState);
                } else if (hasManeuverList && hasAnyManeuver) {
                    traceBap("updateManeuverState", "1");
                    appConnectorNavi.updateManeuverState(1);
                } else if (hasAnyManeuver && !hasManeuverList) {
                    Log.d(TAG, "Maneuver state unchanged: list missing (count=" + s.maneuverCount + ")");
                }
            }

            /*
             * 6. Lane guidance (FctID 24)
             */
            int laneRecomputeMask = RouteGuidance.State.DIRTY_LANE_GUIDANCE
                | RouteGuidance.State.DIRTY_MANEUVER_LIST
                | RouteGuidance.State.DIRTY_MANEUVER_COUNT
                | RouteGuidance.State.DIRTY_MANEUVER_ICON
                | RouteGuidance.State.DIRTY_MANEUVER_STATE
                | RouteGuidance.State.DIRTY_DIST_MAN
                | RouteGuidance.State.DIRTY_ROUTE_STATE;
            if ((dirty & laneRecomputeMask) != 0) {
                boolean wantLaneGuidance = !explicitClear
                    && !shouldClearManeuver
                    && nowApproach
                    && (s.laneGuidanceShowing == 1);
                if (wantLaneGuidance) {
                    sendLaneGuidance(s);
                } else {
                    traceBap("updateLaneGuidance", "[],false");
                    appConnectorNavi.updateLaneGuidance(false, new CombiBAPNaviLaneGuidanceData[0]);
                }
            }

            /* 7. Distance to destination (FctID 21) */
            if ((dirty & RouteGuidance.State.DIRTY_DIST_DEST) != 0) {
                sendDistanceToDestinationRaw(s.distDestM, false);
            }

            /* 8. Time to destination (FctID 22) - via AppConnectorNavi */
            if ((dirty & (RouteGuidance.State.DIRTY_TIME_REMAINING |
                          RouteGuidance.State.DIRTY_ETA)) != 0) {
                long eta = s.etaSeconds;
                int timeInfoType = 1;
                int timeFormat = getHuNavigationTimeFormat();
                long timeVal;

                if (eta >= 0) {
                    timeVal = eta;
                } else if (s.timeRemainingSeconds >= 0) {
                    long nowMs = getUtcMillis();
                    timeVal = (nowMs / 1000L) + s.timeRemainingSeconds;
                } else {
                    timeVal = -1;
                }

                if (timeVal >= 0) {
                    long tzAdjustSec = getTimezoneAdjustSeconds(timeVal);
                    if (tzAdjustSec != 0) {
                        Log.d(TAG, "ETA tz adjust: " + tzAdjustSec + "s (raw=" + timeVal + ")");
                        timeVal = timeVal + tzAdjustSec;
                    }
                }

                traceBap("updateTimeToDestination", timeInfoType + "," + timeFormat + "," + timeVal);
                appConnectorNavi.updateTimeToDestination(timeInfoType, timeFormat, timeVal);
            }

            /* 9. Destination info (FctID 46) - via AppConnectorNavi */
            if ((dirty & RouteGuidance.State.DIRTY_DESTINATION) != 0) {
                String dest = (s.destination != null) ? s.destination : "";
                dest = limitUtf8(dest, 50);
                CombiBAPNaviDestination naviDest =
                    new CombiBAPNaviDestination("", "", dest, "", "", "", "");
                CombiBAPDestinationInfo destInfo = new CombiBAPDestinationInfo(naviDest);
                traceBap("updateDestinationInfo", "\"" + dest + "\"");
                appConnectorNavi.updateDestinationInfo(destInfo);
            }

            /* 10. KOMO: push maneuver data to PresentationController for LVDS video */
            int komoMask = RouteGuidance.State.DIRTY_MANEUVER_ICON
                | RouteGuidance.State.DIRTY_MANEUVER_LIST
                | RouteGuidance.State.DIRTY_MANEUVER_COUNT
                | RouteGuidance.State.DIRTY_MANEUVER_TEXT
                | RouteGuidance.State.DIRTY_DIST_MAN;
            if ((dirty & komoMask) != 0) {
                updateKOMO(s);
            }

            Log.d(TAG, "Update: dist=" + distM + "m maneuvers=" + Math.min(s.maneuverCount, MAX_BAP_MANEUVERS));

        } catch (Exception e) {
            Log.e(TAG, "update error", e);
        }
    }

    /* ============================================================
     * Maneuver Sending (supports up to 3 maneuvers for VC/HUD)
     * ============================================================ */

    /**
     * Send FOLLOW_STREET descriptor.
     */
    private void sendFollowStreet() throws Exception {
        CombiBAPNaviManeuverDescriptor[] arr = new CombiBAPNaviManeuverDescriptor[1];
        arr[0] = createDescriptor(ManeuverMapper.FOLLOW_STREET, ManeuverMapper.DIR_STRAIGHT, 0, new byte[0]);
        traceBap("updateManeuverDescriptor", "count=1 FOLLOW_STREET");
        appConnectorNavi.updateManeuverDescriptor(arr);
    }

    /**
     * Send NO_SYMBOL descriptor.
     */
    private void sendNoSymbol() throws Exception {
        CombiBAPNaviManeuverDescriptor[] arr = new CombiBAPNaviManeuverDescriptor[1];
        arr[0] = createDescriptor(0, 0, 0, new byte[0]);
        traceBap("updateManeuverDescriptor", "count=1 NO_SYMBOL");
        appConnectorNavi.updateManeuverDescriptor(arr);
    }

    /**
     * Send up to 3 maneuvers from state.
     */
    private void sendManeuvers(RouteGuidance.State s) throws Exception {
        int[] idxs = getManeuverIndexList(s);
        if (idxs == null || idxs.length == 0) {
            if (s.maneuverCount == 0) {
                sendNoSymbol();
            } else {
                Log.d(TAG, "Maneuver list missing (count=" + s.maneuverCount + "), keep last icons");
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
                Log.d(TAG, "No valid maneuvers yet (count=" + s.maneuverCount + "), keep last icons");
            }
            return;
        }

        /* Build array of CombiBAPNaviManeuverDescriptor - send all valid
         * maneuvers in order.  START_ROUTE already maps to FOLLOW_STREET,
         * so no skip needed; keeping pos=0 ensures BAP descriptor index
         * stays aligned with distance[0]. */
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
            traceDescriptor(out, idx, s.mType[idx], mapped[0], mapped[1], zLevel, sideStreets);
            arr[out++] = createDescriptor(mapped[0], mapped[1], zLevel, sideStreets);
        }

        traceBap("updateManeuverDescriptor", "count=" + validCount);
        appConnectorNavi.updateManeuverDescriptor(arr);
        Log.d(TAG, "Sent " + validCount + " maneuvers");
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
            traceBap("updateLaneGuidance", "[],false");
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
            tmp[out++] = new CombiBAPNaviLaneGuidanceData(
                lanePos, laneDir, laneSideStreets, (short) 0,
                (byte) 0, (byte) 0, (byte) 0, mapGuidanceInfo(s, idx, i));
        }

        if (out <= 0) {
            traceBap("updateLaneGuidance", "[],false");
            appConnectorNavi.updateLaneGuidance(false, new CombiBAPNaviLaneGuidanceData[0]);
            return;
        }

        CombiBAPNaviLaneGuidanceData[] arr = tmp;
        if (out != n) {
            arr = new CombiBAPNaviLaneGuidanceData[out];
            for (int i = 0; i < out; i++) arr[i] = tmp[i];
        }

        traceBap("updateLaneGuidance", "real,count=" + out + ",slot=" + idx);
        appConnectorNavi.updateLaneGuidance(true, arr);
    }

    private static boolean hasLaneGuidanceForManeuver(RouteGuidance.State s, int manIdx) {
        if (manIdx < 0) return false;
        if (s.mLaneDirections == null || manIdx >= s.mLaneDirections.length) return false;
        if (s.mLaneDirections[manIdx] == null) return false;
        return laneCountForManeuver(s, manIdx) > 0;
    }

    private static int laneCountForManeuver(RouteGuidance.State s, int manIdx) {
        if (manIdx < 0) return 0;
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
        int primary = getFirstManeuverIndex(s);
        if (hasLaneGuidanceForManeuver(s, primary)) {
            return primary;
        }

        int linked = linkedLaneSlotForManeuver(s, primary);
        if (hasLaneGuidanceForManeuver(s, linked)) {
            return linked;
        }

        int[] ordered = getManeuverIndexList(s);
        int indexed = laneGuidanceIndexedSlot(s, ordered);
        if (hasLaneGuidanceForManeuver(s, indexed)) {
            return indexed;
        }

        if (ordered != null) {
            for (int i = 0; i < ordered.length; i++) {
                int idx = ordered[i];
                if (hasLaneGuidanceForManeuver(s, idx)) {
                    return idx;
                }
                int linkedIdx = linkedLaneSlotForManeuver(s, idx);
                if (hasLaneGuidanceForManeuver(s, linkedIdx)) {
                    return linkedIdx;
                }
            }
        }

        return primary;
    }

    private static int laneGuidanceIndexedSlot(RouteGuidance.State s, int[] ordered) {
        int laneIdx = s.laneGuidanceIndex;
        if (laneIdx < 0) return -1;

        int max = (s.mLaneCount != null) ? s.mLaneCount.length : 0;
        if (laneIdx < max && hasLaneGuidanceForManeuver(s, laneIdx)) {
            return laneIdx;
        }

        if (ordered == null || s.mLinkedLaneGuidanceIndex == null) {
            return -1;
        }

        for (int i = 0; i < ordered.length; i++) {
            int idx = ordered[i];
            if (idx < 0 || idx >= s.mLinkedLaneGuidanceIndex.length) continue;
            if (s.mLinkedLaneGuidanceIndex[idx] != laneIdx) continue;

            if (hasLaneGuidanceForManeuver(s, idx)) {
                return idx;
            }

            int linked = linkedLaneSlotForManeuver(s, idx);
            if (hasLaneGuidanceForManeuver(s, linked)) {
                return linked;
            }
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
        if (s.mLanePositions == null || manIdx < 0 || manIdx >= s.mLanePositions.length) {
            return (short) laneIdx;
        }
        int[] pos = s.mLanePositions[manIdx];
        if (pos == null || laneIdx < 0 || laneIdx >= pos.length) {
            return (short) laneIdx;
        }
        int v = pos[laneIdx];
        if (v < 0 || v > 0x7FFF) return (short) laneIdx;
        return (short) v;
    }

    private static boolean hasLaneAnglesForLane(RouteGuidance.State s, int manIdx, int laneIdx) {
        if (s.mLaneAngles == null || manIdx < 0 || manIdx >= s.mLaneAngles.length) return false;
        int[][] lanes = s.mLaneAngles[manIdx];
        if (lanes == null || lanes.length == 0) return false;
        int sel = (laneIdx >= 0 && laneIdx < lanes.length) ? laneIdx : 0;
        int[] angles = lanes[sel];
        return (angles != null && angles.length > 0);
    }

    private static boolean shouldEmitLane(RouteGuidance.State s, int manIdx, int laneIdx, short laneDir) {
        if (s.mLaneDirections == null || manIdx < 0 || manIdx >= s.mLaneDirections.length) return true;
        int[] dirs = s.mLaneDirections[manIdx];
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

        if (s.mLaneAngles != null && manIdx < s.mLaneAngles.length) {
            int[][] laneAngles = s.mLaneAngles[manIdx];
            if (laneAngles != null && laneAngles.length > 0) {
                int sel = (laneIdx >= 0 && laneIdx < laneAngles.length) ? laneIdx : 0;
                int[] angles = laneAngles[sel];
                if (angles != null && angles.length > 0) {
                    return (short)(mapRawLaneValueToDirectionCode(angles[0]) & 0xFF);
                }
            }
        }
        return (short) 0xFF;
    }

    private static short mapLaneDirection(RouteGuidance.State s, int manIdx, int laneIdx) {
        if (s.mLaneDirections == null || manIdx < 0 || manIdx >= s.mLaneDirections.length) return (short)0xFF;
        int[] dirs = s.mLaneDirections[manIdx];
        if (dirs == null || laneIdx < 0 || laneIdx >= dirs.length) return (short)0xFF;
        int raw = dirs[laneIdx];

        if (raw == 1000) {
            return mapLaneDirectionFromSentinel(s, manIdx, laneIdx);
        }

        return (short)(mapRawLaneValueToDirectionCode(raw) & 0xFF);
    }

    private static byte[] mapLaneSideStreets(RouteGuidance.State s, int manIdx, int laneIdx, short laneDirection) {
        if (manIdx < 0) return new byte[0];
        if (s.mLaneAngles == null || manIdx >= s.mLaneAngles.length) return new byte[0];
        int[][] lanes = s.mLaneAngles[manIdx];
        if (lanes == null || lanes.length == 0) return new byte[0];
        int sel = (laneIdx >= 0 && laneIdx < lanes.length) ? laneIdx : 0;
        int[] angles = lanes[sel];
        if (angles == null || angles.length == 0) return new byte[0];

        int start = 0;
        if (s.mLaneDirections != null && manIdx < s.mLaneDirections.length) {
            int[] dirs = s.mLaneDirections[manIdx];
            if (dirs != null && laneIdx >= 0 && laneIdx < dirs.length && dirs[laneIdx] == 1000) {
                start = 1;
            }
        }
        if (start >= angles.length) return new byte[0];

        int primaryDir = laneDirection & 0xFF;
        int[] codes = new int[angles.length - start];
        int n = 0;
        for (int i = start; i < angles.length; i++) {
            int code = mapRawLaneValueToDirectionCode(angles[i]) & 0xFF;
            if (code == 0xFF) continue;
            /* Skip angles that map to the same BAP direction as the primary —
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
        if (s.mLaneStatus == null || manIdx < 0 || manIdx >= s.mLaneStatus.length) return 0;
        int[] status = s.mLaneStatus[manIdx];
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

    /* ============================================================
     * KOMO / LVDS Video Pipeline
     * ============================================================ */

    /**
     * Start KOMO pipeline: enable view, start video encoder, activate RG.
     * This triggers the chain:
     *   setKOMODataRate(2) → ChoiceModel(1,168) hint 2
     *     → CombiMapController.updateFrameRate() → DisplayManager.setUpdateRate()
     *     → videoencoderservice starts capturing
     *     → reports gfxAvailable=true
     *     → ViewModeSM activates MAP view (FPK hardcoded rgType=4)
     *     → BAP rgType=4 → VC sets INTERN_Active_NavFPK_Content=1
     *   komoService.setRouteInfo() → PresentationController renders
     *     → video → MOST → VC detects LVDS_Available=1
     *     → SV_LVDS_NavMap_FPK activates
     */
    private void startKOMO() {
        if (komoService == null || csRef == null) {
            Log.w(TAG, "KOMO: cannot start (komoService=" + (komoService != null) + " csRef=" + (csRef != null) + ")");
            return;
        }
        try {
            /* 1. Enable KOMO view on PresentationController */
            komoService.enableKomoView(true);
            komoService.notifyVisibility(true);

            /* 2. Tell PresentationController to use KDK render mode.
             * For FPK, refreshViewMode() is a NO-OP so refreshRgMode() never
             * fires — setRgSelect() is never called. We must call it explicitly.
             * rgSelect: 0=RGI, 1=KDK, 2=COMPASS, 3=MAP */
            komoService.setRgSelect(1);

            /* 2b. Activate cluster video pipeline directly on DisplayManager.
             * Bypasses CombiMapController (HMI widget that may not be connected
             * during CarPlay). Sets context 72 → KDK visible (displayable 20)
             * → context 151 → native display manager → setActiveDisplayable
             * on video encoder → encoder starts capturing. */
            String pipelineResult = csRef.activateClusterVideoPipeline();
            Log.i(TAG, "KOMO: pipeline " + pipelineResult);

            /* 3. Set KDK visibility + rgActive for hint logic */
            csRef.setKDKVisibility(true);
            csRef.updateKDKRgActive(true);

            /* 5. Start video encoder: both chains needed */
            csRef.setKOMODataRate(KOMO_DATA_RATE_FULL);
            csRef.setClusterUpdateRate(10);

            /* 6. Clear sentinel strings from followInfoRIE */
            csRef.updateFollowInfoData("", "", "");

            komoStarted = true;
            Log.i(TAG, "KOMO: started (hints=0x" + Integer.toHexString(csRef.getKOMOHintsRaw())
                + " " + csRef.getClusterViewModeDiag() + ")");
        } catch (Exception e) {
            Log.w(TAG, "KOMO start failed (non-fatal): " + e.getMessage());
        }
    }

    /**
     * Stop KOMO pipeline: stop video encoder, clear maneuver data, disable view.
     */
    private void stopKOMO() {
        if (!komoStarted) return;
        try {
            if (csRef != null) {
                csRef.deactivateClusterVideoPipeline();
                csRef.setClusterUpdateRate(0);
                csRef.setKOMODataRate(0);
                csRef.setKDKVisibility(false);
                csRef.updateKDKRgActive(false);
                csRef.updateNextManeuver(new RouteInfoElement());
            }
            if (komoService != null) {
                komoService.setRgSelect(2);  /* restore COMPASS render mode */
                komoService.notifyVisibility(false);
                komoService.enableKomoView(false);
            }
            komoStarted = false;
            Log.i(TAG, "KOMO: stopped");
        } catch (Exception e) {
            Log.w(TAG, "KOMO stop failed (non-fatal): " + e.getMessage());
        }
    }

    /**
     * Push current maneuver data to PresentationController via KOMOService.
     * Builds a RouteInfoElement with ManeuverElement from the same mapper
     * values used for BAP (element=mainCategory, direction=bapDirection).
     */
    private void updateKOMO(RouteGuidance.State s) {
        if (!komoStarted || csRef == null) return;

        try {
            int[] idxs = getManeuverIndexList(s);
            if (idxs == null || idxs.length == 0 || s.maneuverCount == 0) {
                csRef.updateNextManeuver(new RouteInfoElement());
                return;
            }

            int maxIdx = (s.mType != null) ? s.mType.length : 0;
            /* Pick the first directional maneuver (skip FOLLOW_STREET).
             * Native FPK sends "nextManeuver" = the upcoming turn, not the
             * current "go straight" instruction.  Fall back to FOLLOW_STREET
             * if it's the only valid maneuver. */
            int firstIdx = -1;
            int followIdx = -1;
            for (int i = 0; i < idxs.length; i++) {
                int idx = idxs[i];
                if (idx >= 0 && idx < maxIdx && ManeuverMapper.isValidType(s.mType[idx])) {
                    int[] probe = ManeuverMapper.map(
                        s.mType[idx], s.mTurnAngle[idx],
                        s.mJunctionType[idx], s.mDrivingSide[idx]);
                    if (probe[0] == ManeuverMapper.FOLLOW_STREET) {
                        if (followIdx < 0) followIdx = idx;
                    } else {
                        firstIdx = idx;
                        break;
                    }
                }
            }
            if (firstIdx < 0) firstIdx = followIdx;

            if (firstIdx < 0) {
                csRef.updateNextManeuver(new RouteInfoElement());
                return;
            }

            int[] mapped = ManeuverMapper.map(
                s.mType[firstIdx],
                s.mTurnAngle[firstIdx],
                s.mJunctionType[firstIdx],
                s.mDrivingSide[firstIdx]
            );

            ManeuverElement me = new ManeuverElement(
                mapped[0],
                (short) mapped[1],
                0
            );

            String turnTo = getKomoTurnToStreet(s, firstIdx);

            RouteInfoElement rie = new RouteInfoElement();
            rie.routeInfoElementType = 3;
            rie.maneuverDescriptor = new ManeuverElement[]{ me };
            rie.turnToStreet = turnTo;
            rie.exitNumber = getKomoExitNumber(s, firstIdx);
            rie.exitIconId = 0;

            /* Update followInfoRIE trip data so PresentationController sees
             * changed data and re-renders.  Without this, the DSI layer skips
             * the send because followInfoRIE never changes. */
            csRef.updateFollowInfoData(formatKomoDistance(s), formatKomoEta(s), turnTo);

            csRef.updateNextManeuver(rie);
            Log.d(TAG, "KOMO: pushed element=" + mapped[0] + " dir=" + mapped[1]
                + " hints=0x" + Integer.toHexString(csRef.getKOMOHintsRaw()));

        } catch (Exception e) {
            Log.w(TAG, "KOMO update failed: " + e.getMessage());
        }
    }

    private static String getKomoTurnToStreet(RouteGuidance.State s, int idx) {
        if (s.mExitInfo != null && idx < s.mExitInfo.length
            && s.mExitInfo[idx] != null && s.mExitInfo[idx].length() > 0) {
            return keepLastColonPart(s.mExitInfo[idx]);
        }
        if (s.mAfterRoad != null && idx < s.mAfterRoad.length
            && s.mAfterRoad[idx] != null && s.mAfterRoad[idx].length() > 0) {
            return keepLastColonPart(s.mAfterRoad[idx]);
        }
        if (s.mName != null && idx < s.mName.length
            && s.mName[idx] != null && s.mName[idx].length() > 0) {
            return s.mName[idx];
        }
        return null;
    }

    private static String getKomoExitNumber(RouteGuidance.State s, int idx) {
        if (s.mExitInfo != null && idx < s.mExitInfo.length
            && s.mExitInfo[idx] != null && s.mExitInfo[idx].length() > 0) {
            return s.mExitInfo[idx];
        }
        return null;
    }

    /** Format distance-to-destination as a string for followInfoRIE. */
    private String formatKomoDistance(RouteGuidance.State s) {
        if (s.distDestM <= 0) return "";
        FormattedDistance fd = formatDistanceToTurn(s.distDestM);
        if (fd.value <= 0) return "";
        String u;
        switch (fd.unit) {
            case 1: u = " m"; break;
            case 2: u = " km"; break;
            case 3: u = " mi"; break;
            case 4: u = " ft"; break;
            default: u = ""; break;
        }
        return fd.value + u;
    }

    /** Format ETA as HH:MM string for followInfoRIE. */
    private String formatKomoEta(RouteGuidance.State s) {
        if (s.etaSeconds <= 0) return "";
        long etaSec = s.etaSeconds;
        long tzAdj = getTimezoneAdjustSeconds(etaSec);
        etaSec += tzAdj;
        int h = (int) ((etaSec / 3600) % 24);
        if (h < 0) h += 24;
        int m = (int) ((etaSec % 3600) / 60);
        if (m < 0) m += 60;
        return (h < 10 ? "0" : "") + h + ":" + (m < 10 ? "0" : "") + m;
    }

    /* ============================================================
     * Utilities
     * ============================================================ */

    private static long getUtcMillis() {
        try {
            IFrameworkAccess fw = CarPlayHook.getFrameworkAccess();
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
            Log.d(TAG, "timeFormat DateMetric.timeFormat=" + v + " bap=" + bap);
            return bap;
        } catch (Throwable t) {
            Log.d(TAG, "timeFormat access failed: " + t.getMessage());
        }
        return 0;
    }

    private static long getTimezoneAdjustSeconds(long etaEpochSec) {
        try {
            IFrameworkAccess fw = CarPlayHook.getFrameworkAccess();
            if (fw == null) return 0;

            long huOffsetMs = fw.getCurrentTimezoneOffsetMilliseconds();
            long javaOffsetMs = java.util.TimeZone.getDefault().getOffset(etaEpochSec * 1000L);

            long deltaMs = huOffsetMs - javaOffsetMs;
            if (deltaMs == 0) return 0;

            return deltaMs / 1000L;
        } catch (Exception e) {
            return 0;
        }
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
        traceBap("updateExitView", variant + "," + exitViewNum);
        appConnectorNavi.updateExitView(variant, exitViewNum);
    }

    private int getExitViewVariant() {
        return defaultExitViewVariant();
    }

    private static int defaultExitViewVariant() {
        String country = null;
        try {
            country = java.util.Locale.getDefault().getCountry();
        } catch (Exception e) {
            country = null;
        }
        if (country == null || country.length() == 0) {
            try {
                country = System.getProperty("user.country");
            } catch (Exception e) {
                country = null;
            }
        }
        if (country != null) {
            String c = country.toUpperCase();
            if ("US".equals(c) || "CA".equals(c) || "MX".equals(c)) return EXITVIEW_NAR;
            if ("JP".equals(c) || "CN".equals(c) || "KR".equals(c) || "TW".equals(c)) return EXITVIEW_ASIA;
        }
        return EXITVIEW_EU;
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
        if (s.maneuverCount > 0) return 0;
        return -1;
    }

    private static int[] getManeuverIndexList(RouteGuidance.State s) {
        if (s.maneuverOrder != null && s.maneuverOrder.length == 0) {
            return null;
        }
        if (s.maneuverOrder != null && s.maneuverOrder.length > 0) {
            return s.maneuverOrder;
        }
        int count = s.maneuverCount;
        int maxIdx = (s.mType != null) ? s.mType.length : 0;
        if (count > maxIdx) count = maxIdx;
        if (count <= 0) return null;
        int[] out = new int[count];
        for (int i = 0; i < count; i++) out[i] = i;
        return out;
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
