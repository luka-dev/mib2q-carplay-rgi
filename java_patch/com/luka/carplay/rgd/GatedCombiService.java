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

import com.luka.carplay.framework.Log;

import de.audi.app.bap.fw.AbstractBAPModuleFSG;
import de.audi.app.bap.fw.functiontypes.BAPFunctionPropertyFSG;
import de.audi.atip.interapp.combi.bap.navi.CombiBAPServiceNavi;
import de.audi.atip.interapp.combi.bap.audio.data.CombiBAPTMCInfoMessage;
import de.audi.atip.interapp.combi.bap.navi.data.CombiBAPDestinationInfo;
import de.audi.atip.interapp.combi.bap.navi.data.CombiBAPDestinationListEntry;
import de.audi.atip.interapp.combi.bap.navi.data.CombiBAPNaviDestination;
import de.audi.atip.interapp.combi.bap.navi.data.CombiBAPNaviLaneGuidanceData;
import de.audi.atip.interapp.combi.bap.navi.data.CombiBAPNaviManeuverDescriptor;
import de.audi.atip.interapp.combi.bap.navi.data.CombiBAPSemiDynamicRouteInfo;
import de.audi.atip.interapp.combi.bap.navi.data.EtcStatus;
import de.vw.mib.bap.generated.navsd.serializer.FunctionList_Status;
import de.vw.mib.bap.generated.navsd.serializer.Map_Presentation_Status;
import de.vw.mib.bap.requests.StatusProperty;
import java.lang.reflect.Field;

public class GatedCombiService implements CombiBAPServiceNavi {
    private static final String TAG = "GatedCombi";
    private static final int FCT_FUNCTION_LIST = 3;
    private static final int FCT_MAP_PRESENTATION = 54;

    final CombiBAPServiceNavi real;
    volatile boolean blockRouteGuidance;
    private boolean sideMenusHidden;
    private boolean haveMapPresentation;
    private boolean largeMapView;
    private BAPFunctionPropertyFSG functionListProperty;
    private BAPFunctionPropertyFSG mapPresentationProperty;
    private FunctionList_Status stockFunctionList;
    private boolean haveMapScale;
    private int mapScaleAutoZoom, mapScaleValue, mapScaleUnit;
    private boolean mapScaleAutoZoomActive, mapScaleIntersectionSupported;
    private boolean haveAltitude;
    private int altitudeValue, altitudeUnit;

    public GatedCombiService(CombiBAPServiceNavi r) { this.real = r; }

    /**
     * Hide only the native navigation side-menu capability while altScreen owns
     * the cluster.  The stock FunctionList snapshot is cloned bit-for-bit and
     * only FctID 54 (Map_Presentation) is cleared.  All destination, map-option,
     * RGI and HUD functions therefore keep their stock availability.
     *
     * Fail open if the concrete AppConnector/module or its initialized stock
     * FunctionList cannot be recovered: emitting a default all-false list would
     * remove the whole NAVSD domain from the cluster.
     */
    public synchronized boolean setSideMenusHidden(boolean hidden) {
        if (sideMenusHidden == hidden) return true;

        if (hidden) {
            if (!resolveBapProperties()) return false;

            StatusProperty last = functionListProperty.getLastStatus();
            if (!(last instanceof FunctionList_Status)) {
                Log.w(TAG, "side-menu gate unavailable: Fct3 status has unexpected type");
                return false;
            }

            FunctionList_Status stock = copyFunctionList((FunctionList_Status)last);
            if (stock == null || !isInitializedFunctionList(stock)) {
                Log.w(TAG, "side-menu gate deferred: stock FunctionList is not initialized");
                return false;
            }

            stockFunctionList = stock;
            captureMapPresentation();
            closeSideMenusIfKnown();

            FunctionList_Status masked = copyFunctionList(stock);
            if (masked == null) return false;
            masked.fctMap_PresentationAvailable = false;
            functionListProperty.sendStatus(masked);
            sideMenusHidden = true;
            Log.i(TAG, "native side menus hidden: FunctionList Fct54=false");
            return true;
        }

        if (stockFunctionList != null && functionListProperty != null) {
            FunctionList_Status restored = copyFunctionList(stockFunctionList);
            if (restored == null) return false;
            functionListProperty.sendStatus(restored);
        }
        closeSideMenusIfKnown();
        sideMenusHidden = false;
        stockFunctionList = null;
        Log.i(TAG, "native side menus released: stock FunctionList restored");
        return true;
    }

    private boolean resolveBapProperties() {
        if (functionListProperty != null && mapPresentationProperty != null) return true;
        try {
            Class type = real.getClass();
            Field moduleField = null;
            while (type != null && moduleField == null) {
                try { moduleField = type.getDeclaredField("moduleFsg"); }
                catch (NoSuchFieldException e) { type = type.getSuperclass(); }
            }
            if (moduleField == null) throw new NoSuchFieldException("moduleFsg");
            moduleField.setAccessible(true);
            Object value = moduleField.get(real);
            if (!(value instanceof AbstractBAPModuleFSG)) {
                throw new IllegalStateException("moduleFsg type=" + value);
            }
            AbstractBAPModuleFSG module = (AbstractBAPModuleFSG)value;
            if (!module.getFunctionList().isFunctionSupported(FCT_FUNCTION_LIST)) {
                throw new IllegalStateException("NAVSD FunctionList unsupported");
            }
            functionListProperty = module.getBAPFunctionPropertyFSG(FCT_FUNCTION_LIST);
            mapPresentationProperty = module.getBAPFunctionPropertyFSG(FCT_MAP_PRESENTATION);
            return functionListProperty != null && mapPresentationProperty != null;
        } catch (Throwable t) {
            Log.w(TAG, "side-menu gate fail-open: " + t);
            functionListProperty = null;
            mapPresentationProperty = null;
            return false;
        }
    }

    private static boolean isInitializedFunctionList(FunctionList_Status status) {
        return status.fctGetAllAvailable
            && status.fctFunctionListAvailable
            && status.fctFsg_OperationStateAvailable;
    }

    private static FunctionList_Status copyFunctionList(FunctionList_Status source) {
        if (source == null) return null;
        try {
            FunctionList_Status copy = new FunctionList_Status();
            Field[] fields = FunctionList_Status.class.getFields();
            for (int i = 0; i < fields.length; i++) {
                if (fields[i].getType() == Boolean.TYPE) {
                    fields[i].setBoolean(copy, fields[i].getBoolean(source));
                }
            }
            return copy;
        } catch (Throwable t) {
            Log.w(TAG, "FunctionList clone failed: " + t);
            return null;
        }
    }

    private void captureMapPresentation() {
        if (mapPresentationProperty == null || !mapPresentationProperty.isDataValid()) return;
        StatusProperty last = mapPresentationProperty.getLastStatus();
        if (last instanceof Map_Presentation_Status) {
            largeMapView = ((Map_Presentation_Status)last).asg_Hmi_State.largeMapView;
            haveMapPresentation = true;
        }
    }

    /** Preserve large/small map mode; only force the two open bits low. */
    private void closeSideMenusIfKnown() {
        if (!haveMapPresentation) return;
        real.updateMapPresentation(largeMapView, false, false);
    }

    /** Route-data ownership. BAPBridge is the only writer while CarPlay RGI is active.
     * Keep stock compass and NAVSD state untouched; hide only scale and altitude. */
    public synchronized void setRouteGuidanceBlocked(boolean active) {
        if (blockRouteGuidance == active) return;
        blockRouteGuidance = active;
        if (active) {
            try { sendHiddenMapScale(); } catch (Throwable t) { }
            try { real.updateAltitude(0x7FFF, 2); } catch (Throwable t) { }
        } else {
            try {
                if (haveMapScale) real.updateMapScale(
                    mapScaleAutoZoom, mapScaleAutoZoomActive, mapScaleValue,
                    mapScaleUnit, mapScaleIntersectionSupported);
            } catch (Throwable t) { }
            try {
                if (haveAltitude) real.updateAltitude(altitudeValue, altitudeUnit);
            } catch (Throwable t) { }
        }
    }

    private void sendHiddenMapScale() {
        real.updateMapScale(15, false, 0xFFFF, 0xFF, false);
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
    public void updateCompassInfo(int a, int b) { real.updateCompassInfo(a, b); }
    public void updateTurnToInfo(String a, String b) { real.updateTurnToInfo(a, b); }
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
    public synchronized void updateMapScale(int a, boolean b, int c, int d, boolean e) {
        mapScaleAutoZoom = a;
        mapScaleAutoZoomActive = b;
        mapScaleValue = c;
        mapScaleUnit = d;
        mapScaleIntersectionSupported = e;
        haveMapScale = true;
        /* A steering-wheel MapScale SetGet must always receive a Status.  The
         * BAP framework bypasses value deduplication while setGetPending=true,
         * so resending the hidden value is both the ACK and the visibility rule. */
        if (blockRouteGuidance) sendHiddenMapScale();
        else real.updateMapScale(a, b, c, d, e);
    }
    public void updateDestinationInfo(CombiBAPDestinationInfo a) {
        real.updateDestinationInfo(a); }
    public synchronized void updateAltitude(int a, int b) {
        altitudeValue = a;
        altitudeUnit = b;
        haveAltitude = true;
        if (!blockRouteGuidance) real.updateAltitude(a, b);
    }
    public void updateOnlineNavigationState(int a, int b, int c) {
        real.updateOnlineNavigationState(a, b, c); }
    public void updateSemidynamicRouteGuidance(CombiBAPSemiDynamicRouteInfo a) {
        real.updateSemidynamicRouteGuidance(a); }
    public void poiSearchResult(int a, int b) { real.poiSearchResult(a, b); }
    public void updatePOIListSize(int a) { real.updatePOIListSize(a); }
    public void updateFSGSetup(int a, boolean b) { real.updateFSGSetup(a, b); }
    public synchronized void updateMapPresentation(boolean a, boolean b, boolean c) {
        largeMapView = a;
        haveMapPresentation = true;
        if (sideMenusHidden) real.updateMapPresentation(a, false, false);
        else real.updateMapPresentation(a, b, c);
    }
    public void updateEtcStatus(EtcStatus a) { real.updateEtcStatus(a); }
}
