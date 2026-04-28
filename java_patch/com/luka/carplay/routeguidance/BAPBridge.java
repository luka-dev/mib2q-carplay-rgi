/*
 * CarPlay Route Guidance - BAP Bridge
 *
 * Translates route guidance state to BAP protocol calls via AppConnectorNavi,
 * and drives c_render via TCP for LVDS video rendering.
 *
 * BAP path: ManeuverDescriptor, distance, street, lane guidance, ETA -> VC/HUD text overlays.
 * c_render path: CMD_MANEUVER over TCP -> c_render EGL/GLES2 -> video encoder -> MOST -> VC LVDS.
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
import java.lang.reflect.Field;
import java.lang.reflect.Method;

public class BAPBridge {

    private static final String TAG = "BAPBridge";
    /* RGType sent to cluster: 0=RGI (BAP ManeuverDescriptor icons for HUD).
     * FPK has rgType=4 hardcoded in CombiBAPListener -- our BAP rgType=0 is for the
     * AppConnectorNavi FSG sync flow, not for view mode selection. */
    private static final int ACTIVE_RGTYPE = 0;  /* RGI -- BAP icons. c_render handles LVDS video. */
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
    private RendererClient rendererClient;

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
        /* Read blink state and send BAP+renderer inside the same sync block.
         * resetActionBlinkState() is also synchronized(this), so the clear
         * and the blink tick are mutually exclusive — no stale send after clear. */
        synchronized (this) {
            if (!blinkArmed || blinkDistM <= 0 || blinkBargraphDenominatorM <= 0
                    || blinkDistM > blinkBargraphDenominatorM) return;
            int linBargraph = (blinkDistM * 100) / blinkBargraphDenominatorM;
            if (linBargraph < 0) linBargraph = 0;
            if (linBargraph > 100) linBargraph = 100;
            if (linBargraph >= BARGRAPH_BLINK_PERCENT) {
                actionBlinkFull = true;
                return;
            }
            int bargraph = actionBlinkFull ? 100 : 0;
            actionBlinkFull = !actionBlinkFull;

            try {
                sendDistanceToManeuverRaw(blinkDistM, true, bargraph);
            } catch (Exception e) {
                Log.e(TAG, "Action blink tick failed", e);
            }
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

            /* Start renderer FIRST — takes over displayable 20 before we
             * set rgActive/rgiValid, which would trigger native KDK rendering
             * to the same displayable (race → native icon overwrites our arrow). */
            startCustomRenderer();

            /* Now safe to set cluster state flags — our renderer owns displayable 20 */
            forceClusterRouteInfoState(true);

            /*
             * Guidance start -- BAP text overlays for HUD + VC text.
             *
             * 1. RGStatus(1) - FctID 17 -> triggers startSync(0) for {17,39,23,18,49}
             * 2. Complete sync(0) window: rgType(39), descriptor(23), distance(18), exitView(49)
             */
            traceBap("updateRGStatus", "1");
            appConnectorNavi.updateRGStatus(1);                                      /* FctID 17 -> sync(0) */
            traceBap("updateActiveRGType", String.valueOf(ACTIVE_RGTYPE));
            appConnectorNavi.updateActiveRGType(ACTIVE_RGTYPE);                      /* FctID 39 */

            /* Sync(0) FctIDs: descriptor, distance, exitView */
            sendFollowStreet();                                                      /* FctID 23 */
            sendDistanceToManeuverRaw(0, false, 0);                                  /* FctID 18 */
            sendExitView();                                                          /* FctID 49 */

            /* Non-sync FctIDs */
            traceBap("updateCurrentPositionInfo", "\"\"");
            try {
                appConnectorNavi.updateCurrentPositionInfo("");                       /* FctID 19 */
            } catch (Throwable t) {
                Log.w(TAG, "BAP FctID 19 failed during start: "
                    + t.getClass().getName() + ": " + t.getMessage());
            }

            Log.i(TAG, "Started (rgType=" + ACTIVE_RGTYPE
                + ", cr=" + customRendererStarted + ")");

        } catch (Throwable e) {
            Log.e(TAG, "onStart error: " + e.getClass().getName() + ": " + e.getMessage());
        }
    }

    public void onStop() {
        if (!initialized) return;

        try {
            /* Lightweight stop — reset internal state only.
             * No BAP teardown, no renderer kill. iOS sends transient route_state=0
             * during maneuver transitions; full teardown causes HUD flicker + renderer
             * black screen. BAP teardown happens in onShutdown() on real disconnect. */
            stopActionBlinkThread();
            inApproachZone = false;
            latchedTurnToText = "";
            Log.d(TAG, "Stopped (lightweight, renderer + BAP kept alive)");
        } catch (Exception e) {
            Log.e(TAG, "onStop error", e);
        }
    }

    /**
     * Full shutdown — BAP teardown + renderer kill.
     * Called on actual CarPlay disconnect or stop().
     */
    public void onShutdown() {
        if (!initialized) return;

        try {
            /*
             * Guidance stop — full BAP teardown:
             * 1. RGStatus(0) - triggers sync(0) for {17,39,23,18,49}
             * 2. Complete sync(0) window: descriptor(23), distance(18), exitView(49)
             * 3. Non-sync FctIDs last
             */
            traceBap("updateRGStatus", "0");
            appConnectorNavi.updateRGStatus(0);
            traceBap("updateActiveRGType", "0");
            appConnectorNavi.updateActiveRGType(0);
            sendNoSymbol();
            sendDistanceToManeuverRaw(0, false, 0);
            sendExitView();
            traceBap("updateManeuverState", "0");
            appConnectorNavi.updateManeuverState(0);
            traceBap("updateCurrentPositionInfo", "\"\"");
            try { appConnectorNavi.updateCurrentPositionInfo(""); } catch (Throwable t) {}
            sendDistanceToDestinationRaw(0, false);
            traceBap("updateTimeToDestination", "0,0,-1");
            appConnectorNavi.updateTimeToDestination(0, 0, -1);
            traceBap("updateLaneGuidance", "[],false");
            appConnectorNavi.updateLaneGuidance(false, new CombiBAPNaviLaneGuidanceData[0]);

            stopCustomRenderer();
            if (gatedService != null) gatedService.blockRouteGuidance = false;
            forceClusterRouteInfoState(false);
            forceGfxAvailable(false);

            Log.i(TAG, "Shutdown (full teardown)");
        } catch (Exception e) {
            Log.e(TAG, "onShutdown error", e);
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
                latchedTurnToText = "";
                Log.i(TAG, "Approach zone " + (nowApproach ? "ENTER" : "EXIT")
                    + " (dist=" + distM + "m, highway=" + isHighway
                    + ", prepThr=" + prepareThreshold + ", barDen=" + bargraphDenominatorM + ")");

                /* 2D/3D perspective switch disabled — always 3D for now */
                /* if (rendererClient != null && customRendererStarted) {
                    rendererClient.sendPerspective(nowApproach ? 0 : 1);
                } */
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
                             * Blink zone: blink thread sends FctID 18 + renderer.
                             * Don't send from here to avoid jitter with periodic blink sends.
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
                        String arrow = directionArrow(s, idx);
                        latchedTurnToText = arrow + " " + candidate;
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
                | RouteGuidance.State.DIRTY_MANEUVER_COUNT;
            if ((dirty & laneRecomputeMask) != 0) {
                /* Both conditions required:
                 * - nowApproach: don't show lanes when far away (iOS sends
                 *   lane data even 1800m out with laneGuidanceShowing=1)
                 * - laneGuidanceShowing==1: iOS populates lane data on slots
                 *   before it wants them displayed (e.g. START_ROUTE has lane
                 *   data but showing=0). Respect iOS's display intent. */
                boolean wantLaneGuidance = !explicitClear
                    && !shouldClearManeuver
                    && nowApproach
                    && s.laneGuidanceShowing == 1;
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

                /* JVM default TZ is UTC on MHI2. AppConnectorNavi uses
                 * GregorianCalendar(Date(epoch*1000)) which would show UTC.
                 * Convert UTC epoch to local epoch so calendar shows local time.
                 * Uses fw.convertUTCTimeToLocalTime() -- DST-aware, always correct. */
                if (timeVal >= 0) {
                    long utcMs = timeVal * 1000L;
                    long localMs = convertUtcToLocalMs(utcMs);
                    timeVal = localMs / 1000L;
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

            /* 10. c_render: CMD_MANEUVER only when icon actually changes,
             * CMD_BARGRAPH for distance updates, CMD_PERSPECTIVE for approach zone */
            if (rendererClient != null && customRendererStarted) {
                /* Watchdog: if renderer TCP connection dropped, re-launch.
                 * Native navi boot can take over displayable 20, killing our renderer
                 * or orphaning its window. Re-launching reclaims it. */
                if (!rendererClient.isConnected()) {
                    Log.w(TAG, "CR: watchdog — TCP lost, re-launching renderer");
                    stopCustomRenderer();
                    startCustomRenderer();
                }

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
            byte gi = mapGuidanceInfo(s, idx, i);
            Log.d(TAG, "  lane[" + i + "] pos=" + lanePos + " dir=0x"
                + Integer.toHexString(laneDir & 0xFF) + " gi=" + gi
                + " sideStreets=" + hexBytes(laneSideStreets));
            tmp[out++] = new CombiBAPNaviLaneGuidanceData(
                lanePos, laneDir, laneSideStreets, (short) 0,
                (byte) 0, (byte) 0, (byte) 0, gi);
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
        /* Only resolve lane data for the CURRENT maneuver (primary) or its
         * directly linked lane slot.  MHI3 does a direct map lookup by
         * maneuver index — never scans other maneuvers.  Scanning caused
         * stale lane data from a previous maneuver to be picked up. */
        int primary = getFirstManeuverIndex(s);
        if (hasLaneGuidanceForManeuver(s, primary)) {
            return primary;
        }

        int linked = linkedLaneSlotForManeuver(s, primary);
        if (hasLaneGuidanceForManeuver(s, linked)) {
            return linked;
        }

        return primary;
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

    /* ==============================================================
     * Custom Renderer Pipeline (c_render)
     *
     * Bypasses PresentationController entirely. The c_hook spawns
     * c_render which creates its own displayable (199) and renders
     * maneuver graphics via EGL/GLES2. Java manages the video encoder
     * pipeline (context switch, update rate), VC state (gfxAvailable),
     * and sends CMD_MANEUVER packets over TCP to control what's drawn.
     * ============================================================== */

    private boolean customRendererStarted = false;

    /* Last maneuver state sent to renderer — only send CMD_MANEUVER when these change.
     * lastCrVer tracks the slot version so a new maneuver with identical type/angle
     * still triggers a push animation (e.g., consecutive left turns). */
    private int lastCrIcon = -1;
    private int lastCrDirection = -99;
    private int lastCrExitAngle = -9999;
    private int lastCrDrivingSide = -1;
    private int lastCrVer = -1;

    /* Crash recovery: count consecutive renderer-side TCP send failures.
     * If we hit the threshold while customRendererStarted=true, the renderer
     * process is presumed crashed — kill any orphan, relaunch, reset our
     * dedup state so the next iAP2 update from iOS triggers a fresh send. */
    private int crConsecutiveSendFailures = 0;
    private long crLastRespawnMs = 0;
    private static final int CR_RESPAWN_FAIL_THRESHOLD = 3;
    private static final long CR_RESPAWN_COOLDOWN_MS = 5000;

    private static final String CR_LAUNCH_CMD =
        "/mnt/app/root/hooks/maneuver_render >/tmp/maneuver_render.log 2>&1 &";
    private static final String CR_KILL_CMD =
        "slay -f -Q maneuver_render >/dev/null 2>&1";

    private void startCustomRenderer() {
        if (customRendererStarted) {
            Log.d(TAG, "CR: already started, skipping");
            return;
        }
        if (csRef == null) {
            Log.w(TAG, "CR: cannot start (csRef null)");
            return;
        }
        try {
            /* Kill any previous instance (orphan from crash/reload) */
            try {
                de.audi.atip.util.CommandLineExecuter.executeCommand(
                    "/bin/sh", new String[] { "-c", CR_KILL_CMD });
            } catch (Throwable t) { /* ignore */ }

            /* Launch renderer from Java (not C hook) — Java's process tree
             * has clean EGL access. Same approach as gpSP. */
            Log.i(TAG, "CR: launching maneuver_render from Java");
            de.audi.atip.util.CommandLineExecuter.executeCommand(
                "/bin/sh", new String[] { "-c", CR_LAUNCH_CMD });

            /* Wait for renderer to init EGL + TCP server */
            try { Thread.sleep(2000); } catch (InterruptedException ie) { /* ignore */ }

            /* Switch cluster display to renderer's context via DisplayManager */
            String result = csRef.activateCustomRendererPipeline();
            Log.i(TAG, "CR: pipeline " + result);

            /* BAP route info state for HUD icons */
            forceClusterRouteInfoState(true);

            /* TCP connection to c_render */
            rendererClient = new RendererClient();
            if (!rendererClient.connect()) {
                Log.w(TAG, "CR: TCP connect failed (will retry on send)");
            }

            /* Set gfxAvailable so VC enters MAP mode for LVDS video.
             * Must be after renderer owns displayable 20 (avoids native KDK race). */
            forceGfxAvailable(true);

            customRendererStarted = true;
            lastCrIcon = -1;
            lastCrDirection = -99;
            lastCrExitAngle = -9999;
            lastCrDrivingSide = -1;
            lastCrVer = -1;

            Log.i(TAG, "CR: started");
        } catch (Throwable t) {
            Log.w(TAG, "CR start failed: " + t.getClass().getName() + ": " + t.getMessage());
        }
    }

    /**
     * Track success/failure of a renderer send.  After
     * CR_RESPAWN_FAIL_THRESHOLD consecutive failures while we believe
     * the renderer is up, assume it crashed and respawn it.
     */
    private void noteRendererSendResult(boolean ok) {
        if (ok) {
            crConsecutiveSendFailures = 0;
            return;
        }
        crConsecutiveSendFailures++;
        if (!customRendererStarted) return;
        if (crConsecutiveSendFailures < CR_RESPAWN_FAIL_THRESHOLD) return;

        long now = System.currentTimeMillis();
        if (now - crLastRespawnMs < CR_RESPAWN_COOLDOWN_MS) {
            /* Cooldown — don't spam respawns if launch keeps failing. */
            return;
        }
        crLastRespawnMs = now;
        crConsecutiveSendFailures = 0;

        Log.w(TAG, "CR: " + CR_RESPAWN_FAIL_THRESHOLD
                + " consecutive send failures, respawning renderer");

        /* Tear down our view of it, then re-run the start sequence
         * (which kills any orphan + relaunches).  Reset dedup state so
         * the very next iAP2 update will cause a fresh CMD_MANEUVER
         * (no "same as last" suppression). */
        try {
            if (rendererClient != null) {
                rendererClient.disconnect();
                rendererClient = null;
            }
            customRendererStarted = false;

            startCustomRenderer();

            lastCrIcon = -1;
            lastCrDirection = -99;
            lastCrExitAngle = -9999;
            lastCrDrivingSide = -1;
            lastCrVer = -1;

            Log.i(TAG, "CR: respawn complete (state will resync on next iAP2 update)");
        } catch (Throwable t) {
            Log.e(TAG, "CR respawn failed", t);
        }
    }

    private void stopCustomRenderer() {
        if (!customRendererStarted) return;
        try {
            if (rendererClient != null) {
                rendererClient.disconnect();
                rendererClient = null;
            }
            /* slay as backstop — ensures no orphan even if TCP shutdown missed */
            try {
                de.audi.atip.util.CommandLineExecuter.executeCommand(
                    "/bin/sh", new String[] { "-c", CR_KILL_CMD });
            } catch (Throwable t2) { /* ignore */ }
            if (csRef != null) {
                csRef.deactivateCustomRendererPipeline();
            }
            forceGfxAvailable(false);
            forceClusterRouteInfoState(false);
            customRendererStarted = false;
            Log.i(TAG, "CR: stopped");
        } catch (Throwable t) {
            Log.w(TAG, "CR stop failed: " + t.getClass().getName() + ": " + t.getMessage());
        }
    }

    /**
     * Push maneuver to c_render ONLY if the rendered icon actually changed.
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
     * Send ICON_APPROACH to c_render — mirrors BAP sendFollowStreet().
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
     * Send standalone CMD_BARGRAPH to c_render on distance-only updates.
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
                 * Don't send mode=2 here — it would start c_render's independent
                 * blink timer which drifts from BAP. Just skip; blink thread syncs both. */
                return;
            }
            bargraphMode = 1;
        }
        noteRendererSendResult(rendererClient.sendBargraph(bargraphLevel, bargraphMode));
    }

    /**
     * Force gfxAvailable on ClusterViewMode.
     * DSIKOMOGfxStreamSink has no native provider on MU1316 so the callback
     * chain (videoencoderservice -> DSI -> KOMOService.updateGfxState -> ClusterViewMode)
     * never fires. We simulate it directly.
     *
     * Strategy 1: komoService.updateGfxState(1, 1) -- mimics DSI notification
     * Strategy 2: ClusterViewMode.setGFXAvailable(true) -- direct method call
     * Strategy 3: ClusterViewMode.gfxAvailable field reflection -- last resort
     */
    private void forceGfxAvailable(boolean available) {
        int gfxVal = available ? 1 : 0;

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
            IFrameworkAccess fw = CarPlayHook.getFrameworkAccess();
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

    /**
     * Pick a directional Unicode arrow matching the maneuver's BAP direction.
     * Uses the same ManeuverMapper output that drives FctID 23 icons.
     */
    private static String directionArrow(RouteGuidance.State s, int idx) {
        if (idx < 0 || s.mType == null || idx >= s.mType.length) {
            return "\u2191"; /* up arrow fallback */
        }
        int[] mapped = ManeuverMapper.map(
            s.mType[idx], s.mTurnAngle[idx],
            s.mJunctionType[idx], s.mDrivingSide[idx]);
        int dir = mapped[1];
        if (dir == ManeuverMapper.DIR_LEFT)         return "\u2190"; /* left */
        if (dir == ManeuverMapper.DIR_SLIGHT_LEFT)  return "\u2196"; /* upper-left */
        if (dir == ManeuverMapper.DIR_SHARP_LEFT)   return "\u2199"; /* lower-left */
        if (dir == ManeuverMapper.DIR_RIGHT)        return "\u2192"; /* right */
        if (dir == ManeuverMapper.DIR_SLIGHT_RIGHT) return "\u2197"; /* upper-right */
        if (dir == ManeuverMapper.DIR_SHARP_RIGHT)  return "\u2198"; /* lower-right */
        if (dir == ManeuverMapper.DIR_UTURN)        return "\u21B6"; /* uturn */
        return "\u2191"; /* straight / up */
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
