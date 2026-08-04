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

    GatedCombiService(CombiBAPServiceNavi r) { this.real = r; }

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
    public void setTakeover(boolean active) {
        boolean changed = blockRouteGuidance != active;
        blockRouteGuidance = active;
        if (active && changed) clearNativeMapStatusBar();
    }

    private void clearNativeMapStatusBar() {
        /* VC invalid gates, verified in hmi_app.elf:
         *   Compass: angle > 360 and symbolic > 15 -> visible=0.
         *   Altitude: altitude 0x7fff or unit > 1 -> empty/not available.
         *   MapScale: scale 0xffff or unit 0xff -> scale visible=0.
         * AppConnectorNavi normalises compass symbolic=0xff back to a valid
         * direction, hence the deliberate out-of-range pair 361/16. */
        try { real.updateCompassInfo(361, 16); } catch (Throwable t) { }
        try { real.updateAltitude(0x7FFF, 2); } catch (Throwable t) { }
        try { real.updateMapScale(15, false, 0xFFFF, 0xFF, false); } catch (Throwable t) { }
        try { real.updateCurrentPositionInfo(""); } catch (Throwable t) { }
        try { real.updateTurnToInfo("", ""); } catch (Throwable t) { }
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

    public void updateCurrentPositionInfo(String s) {
        if (!blockRouteGuidance) real.updateCurrentPositionInfo(s);
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
    public void showInitializingScreen() { real.showInitializingScreen(); }
    public void hideInitializingScreen() { real.hideInitializingScreen(); }
    public void updateCompassInfo(int a, int b) {
        if (!blockRouteGuidance) real.updateCompassInfo(a, b); }
    public void updateTurnToInfo(String a, String b) {
        if (!blockRouteGuidance) real.updateTurnToInfo(a, b); }
    public void updateDistanceToDestination(int a, int b, boolean c) {
        real.updateDistanceToDestination(a, b, c); }
    public void updateTimeToDestination(int a, int b, long c) {
        real.updateTimeToDestination(a, b, c); }
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
    public void updateMapScale(int a, boolean b, int c, int d, boolean e) {
        if (!blockRouteGuidance) real.updateMapScale(a, b, c, d, e); }
    public void updateDestinationInfo(CombiBAPDestinationInfo a) {
        real.updateDestinationInfo(a); }
    public void updateAltitude(int a, int b) {
        if (!blockRouteGuidance) real.updateAltitude(a, b); }
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
