/*
 * GatedCombiService - Wrapper around CombiBAPServiceNavi that gates
 * native route-guidance updates during CarPlay route guidance.
 *
 * When blockRouteGuidance=true, native route-guidance related calls from
 * CombiBAPListener are silently dropped so they don't overwrite BAPBridge.
 * This keeps BAPBridge as the single source for FctIDs 17/18/19/23/24/39/49/55.
 *
 * Non-route-guidance methods always delegate.
 *
 *
 */
package com.luka.carplay.rgd;

import de.audi.atip.interapp.combi.bap.navi.CombiBAPServiceNavi;
import de.audi.atip.interapp.combi.bap.audio.data.CombiBAPTMCInfoMessage;
import de.audi.atip.interapp.combi.bap.navi.data.CombiBAPDestinationInfo;
import de.audi.atip.interapp.combi.bap.navi.data.CombiBAPDestinationListEntry;
import de.audi.atip.interapp.combi.bap.navi.data.CombiBAPNaviDestination;
import de.audi.atip.interapp.combi.bap.navi.data.CombiBAPNaviLaneGuidanceData;
import de.audi.atip.interapp.combi.bap.navi.data.CombiBAPNaviManeuverDescriptor;
import de.audi.atip.interapp.combi.bap.navi.data.CombiBAPSemiDynamicRouteInfo;
import de.audi.atip.interapp.combi.bap.navi.data.EtcStatus;

public class GatedCombiService implements CombiBAPServiceNavi {
    final CombiBAPServiceNavi real;
    volatile boolean blockRouteGuidance;
    private volatile boolean blockNavStatus;
    private boolean deferredInitializing;
    private boolean haveDeferredInitializing;
    private boolean haveCompass, haveTurnTo, haveDistanceDestination, haveTimeDestination;
    private boolean haveMapScale, haveAltitude, haveCurrentPosition;
    private int compassA, compassB;
    private String turnToA, turnToB, currentPosition;
    private int distDestA, distDestB; private boolean distDestC;
    private int timeDestA, timeDestB; private long timeDestC;
    private int mapScaleA, mapScaleC, mapScaleD; private boolean mapScaleB, mapScaleE;
    private int altitudeA, altitudeB;

    public GatedCombiService(CombiBAPServiceNavi r) { this.real = r; }

    /**
     * Enable/disable CarPlay's ownership of the cluster map.
     *
     * The virtual cockpit draws its own NavStatus bar over the LVDS/KDK map
     * hole.  Merely suppressing route-guidance functions does not remove it:
     * altitude (FctID 47), scale (45), compass (16), current-position (19),
     * and turn-to/signpost (20) are independent BAP properties.  On the
     * transition into takeover send each property's protocol-defined invalid
     * value once, then keep the stock producer from painting it back.
     *
     * Do not clear MapVisibility/FctID 44 here.  The VC uses its LVDS-visible
     * bit to open the KDK video hole, so clearing it would hide altScreen too.
     */
    /** Route-data ownership.  BAPBridge drives this independently of the
     * altScreen presentation gate below. */
    public void setRouteGuidanceBlocked(boolean active) {
        blockRouteGuidance = active;
    }

    /** Cluster-presentation ownership.  AltScreenModule drives this for the
     * complete interval in which video/context 99 owns the cluster. */
    public synchronized void setAltScreenTakeover(boolean active) {
        if (blockNavStatus == active) return;

        if (active) {
            /* The stock map may continue toggling its own map-ready state while
             * CarPlay owns the cluster.  AppConnectorNavi translates that into
             * NAVSD app-ready=false (the VC "INITIALIZING" source).  CarPlay's
             * altScreen does not depend on the stock map being ready, so keep
             * NAVSD operational for the takeover and remember the last native
             * request for replay when CarPlay releases it. */
            haveDeferredInitializing = false;
            deferredInitializing = false;
            blockNavStatus = true;
            try { real.hideInitializingScreen(); } catch (Throwable t) { }
            clearNativeMapStatusBar();
            return;
        }

        blockNavStatus = false;
        if (haveDeferredInitializing) {
            try {
                if (deferredInitializing) real.showInitializingScreen();
                else real.hideInitializingScreen();
            } catch (Throwable t) { }
        }
        haveDeferredInitializing = false;
        replayNativeMapStatus();
    }

    private void clearNativeMapStatusBar() {
        /* VC invalid gates, verified in hmi_app.elf:
         *   Compass: wire angle 0xffff + symbolic 0xff (not supported).
         *   Altitude: altitude 0x7fff or unit > 1 -> empty/not available.
         *   MapScale: scale 0xffff or unit 0xff -> scale visible=0.
         * AppConnectorNavi treats Java int 255 specially and derives a valid
         * heading from angle.  Passing -1/-1 avoids that branch; CompassInfo's
         * byte/short serializer emits exactly 0xff/0xffff on the wire. */
        try { real.updateCompassInfo(-1, -1); } catch (Throwable t) { }
        try { real.updateAltitude(0x7FFF, 2); } catch (Throwable t) { }
        try { real.updateMapScale(15, false, 0xFFFF, 0xFF, false); } catch (Throwable t) { }
        try { real.updateCurrentPositionInfo(""); } catch (Throwable t) { }
        try { real.updateTurnToInfo("", ""); } catch (Throwable t) { }
    }

    /** Restore the exact last values suppressed during takeover, instead of waiting for a
     * producer to happen to change each property again after CarPlay disconnects. */
    private void replayNativeMapStatus() {
        try { if (haveCompass) real.updateCompassInfo(compassA, compassB); } catch (Throwable t) { }
        try { if (haveAltitude) real.updateAltitude(altitudeA, altitudeB); } catch (Throwable t) { }
        try {
            if (haveMapScale) real.updateMapScale(mapScaleA, mapScaleB, mapScaleC, mapScaleD, mapScaleE);
        } catch (Throwable t) { }
        try { if (haveCurrentPosition) real.updateCurrentPositionInfo(currentPosition); } catch (Throwable t) { }
        try { if (haveTurnTo) real.updateTurnToInfo(turnToA, turnToB); } catch (Throwable t) { }
        try {
            if (haveDistanceDestination) real.updateDistanceToDestination(distDestA, distDestB, distDestC);
        } catch (Throwable t) { }
        try {
            if (haveTimeDestination) real.updateTimeToDestination(timeDestA, timeDestB, timeDestC);
        } catch (Throwable t) { }
        haveCompass = haveAltitude = haveMapScale = haveCurrentPosition = false;
        haveTurnTo = haveDistanceDestination = haveTimeDestination = false;
    }

    /* GATED ROUTE-GUIDANCE METHODS */
    public void updateRGStatus(int a) {
        if (!blockRouteGuidance) real.updateRGStatus(a);
    }

    public void updateActiveRGType(int a) {
        if (!blockRouteGuidance) real.updateActiveRGType(a);
    }

    public void updateDistanceToNextManeuver(int a, int b, boolean c, int d) {
        if (!blockRouteGuidance) real.updateDistanceToNextManeuver(a, b, c, d);
    }

    public synchronized void updateCurrentPositionInfo(String s) {
        currentPosition = s; haveCurrentPosition = true;
        if (!blockRouteGuidance && !blockNavStatus) real.updateCurrentPositionInfo(s);
    }

    public void updateManeuverDescriptor(CombiBAPNaviManeuverDescriptor[] a) {
        if (!blockRouteGuidance) real.updateManeuverDescriptor(a);
    }

    public void updateLaneGuidance(boolean a, CombiBAPNaviLaneGuidanceData[] b) {
        if (!blockRouteGuidance) real.updateLaneGuidance(a, b);
    }

    public void updateExitView(int a, int b) {
        if (!blockRouteGuidance) real.updateExitView(a, b);
    }

    public void updateManeuverState(int a) {
        if (!blockRouteGuidance) real.updateManeuverState(a);
    }

    /* All other methods: pure delegation */
    public synchronized void showInitializingScreen() {
        if (blockNavStatus) {
            deferredInitializing = true;
            haveDeferredInitializing = true;
            return;
        }
        real.showInitializingScreen();
    }
    public synchronized void hideInitializingScreen() {
        if (blockNavStatus) {
            deferredInitializing = false;
            haveDeferredInitializing = true;
            /* Reassert ready in case a show call crossed the takeover edge. */
            real.hideInitializingScreen();
            return;
        }
        real.hideInitializingScreen();
    }
    public synchronized void updateCompassInfo(int a, int b) {
        compassA = a; compassB = b; haveCompass = true;
        if (!blockNavStatus) real.updateCompassInfo(a, b); }
    public synchronized void updateTurnToInfo(String a, String b) {
        turnToA = a; turnToB = b; haveTurnTo = true;
        if (!blockNavStatus) real.updateTurnToInfo(a, b); }
    public synchronized void updateDistanceToDestination(int a, int b, boolean c) {
        distDestA = a; distDestB = b; distDestC = c; haveDistanceDestination = true;
        if (!blockNavStatus) real.updateDistanceToDestination(a, b, c); }
    public synchronized void updateTimeToDestination(int a, int b, long c) {
        timeDestA = a; timeDestB = b; timeDestC = c; haveTimeDestination = true;
        if (!blockNavStatus) real.updateTimeToDestination(a, b, c); }
    public void updateTMCInfoMessages(CombiBAPTMCInfoMessage[] a) {
        real.updateTMCInfoMessages(a); }
    public void updateLastDestinationsList(CombiBAPDestinationListEntry[] a) {
        real.updateLastDestinationsList(a); }
    public void updateFavoriteDestinationsList(CombiBAPDestinationListEntry[] a) {
        real.updateFavoriteDestinationsList(a); }
    public void updateHomeAddress(CombiBAPNaviDestination a) { real.updateHomeAddress(a); }
    public void routeGuidanceActDeactResult(int a) { real.routeGuidanceActDeactResult(a); }
    public void repeatLastNavAnnouncementResult(int a) {
        real.repeatLastNavAnnouncementResult(a); }
    public void updateVoiceGuidanceState(int a) { real.updateVoiceGuidanceState(a); }
    public void updateInfoStates(int a) { real.updateInfoStates(a); }
    public void updateTrafficBlockIndication(int a) { real.updateTrafficBlockIndication(a); }
    public void updateMapColor(int a) { real.updateMapColor(a); }
    public void updateMapType(int a, int b) { real.updateMapType(a, b); }
    public void updateSupportedMapTypes(boolean a, int b) { real.updateSupportedMapTypes(a, b); }
    public void updateMapView(int a, int b) { real.updateMapView(a, b); }
    public void updateSupportedMapViews(int a, int b) { real.updateSupportedMapViews(a, b); }
    public void updateMapVisibility(boolean a, boolean b) { real.updateMapVisibility(a, b); }
    public void updateMapOrientation(int a) { real.updateMapOrientation(a); }
    public synchronized void updateMapScale(int a, boolean b, int c, int d, boolean e) {
        mapScaleA = a; mapScaleB = b; mapScaleC = c; mapScaleD = d; mapScaleE = e;
        haveMapScale = true;
        if (blockNavStatus) {
            /* MapScale is a BAP SetGet property.  A steering-wheel request sets
             * BAPFunctionPropertyFSG.setGetPending before CombiBAPListener calls
             * us; dropping this callback leaves the ASG request unanswered and
             * the cluster can fall back to NAVSD INITIALIZING.  Re-submit the
             * hidden status instead.  sendStatusIfChanged deliberately sends an
             * unchanged value while setGetPending is true, so this is both the
             * SetGet acknowledgement and a continued visibility=0 assertion. */
            real.updateMapScale(15, false, 0xFFFF, 0xFF, false);
        } else {
            real.updateMapScale(a, b, c, d, e);
        }
    }
    public void updateDestinationInfo(CombiBAPDestinationInfo a) {
        real.updateDestinationInfo(a); }
    public synchronized void updateAltitude(int a, int b) {
        altitudeA = a; altitudeB = b; haveAltitude = true;
        if (!blockNavStatus) real.updateAltitude(a, b); }
    public void updateOnlineNavigationState(int a, int b, int c) {
        real.updateOnlineNavigationState(a, b, c); }
    public void updateSemidynamicRouteGuidance(CombiBAPSemiDynamicRouteInfo a) {
        real.updateSemidynamicRouteGuidance(a); }
    public void poiSearchResult(int a, int b) { real.poiSearchResult(a, b); }
    public void updatePOIListSize(int a) { real.updatePOIListSize(a); }
    public void updateFSGSetup(int a, boolean b) { real.updateFSGSetup(a, b); }
    public void updateMapPresentation(boolean a, boolean b, boolean c) {
        real.updateMapPresentation(a, b, c); }
    public void updateEtcStatus(EtcStatus a) { real.updateEtcStatus(a); }
}
