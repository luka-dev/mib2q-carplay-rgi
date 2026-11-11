/*
 * Native BAP ownership and latest-state restoration. CarPlay writes to the raw
 * endpoint; only the stock listener passes through this wrapper.
 * Copyright (c) 2026 LuKa (@LuKa_dev)
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

import com.luka.carplay.framework.Log;

public class GatedCombiService implements CombiBAPServiceNavi {
    final CombiBAPServiceNavi real;
    volatile boolean blockRouteGuidance;
    volatile boolean blockCurrentPositionInfo;
    private boolean retired;

    /* Index by BAP function ID. Typed closures retain one value per function,
     * never a history of maneuvers. SYNC is the stock RGStatus sync(0) group. */
    private static final long SYNC = bit(17) | bit(39) | bit(23) | bit(18) | bit(49);
    private static final long POSITION = bit(19) | bit(20) | bit(21) | bit(22) | bit(46);
    private static final long ROUTE = SYNC | bit(24) | bit(55) | bit(45) | bit(47);
    private static final int[] ORDER = {17, 39, 23, 18, 49, 24, 55, 19, 20, 21, 22, 46, 45, 47};
    private interface Value { void send(CombiBAPServiceNavi to); }
    private final Value[] latest = new Value[56];
    private long known, pending;
    private boolean hideScalePending, hideAltitudePending;
    private boolean replayFailureLogged;

    public GatedCombiService(CombiBAPServiceNavi r) { this.real = r; }
    private static long bit(int id) { return 1L << id; }
    private long allowed() {
        return (blockRouteGuidance ? 0L : ROUTE) | (blockCurrentPositionInfo ? 0L : POSITION);
    }

    /** Acquiring ownership is synchronous with stock writes, but does no I/O.
     * Releasing and replaying are owned by NavigationJobs in AltScreenNavStatusGate. */
    public synchronized void setRouteGuidanceBlocked(boolean active) {
        if (retired || blockRouteGuidance == active) return;
        blockRouteGuidance = active;
        pending |= known & ROUTE;
        hideScalePending = hideAltitudePending = active;
    }

    public synchronized void setCurrentPositionInfoBlocked(boolean blocked) {
        if (retired || blockCurrentPositionInfo == blocked) return;
        blockCurrentPositionInfo = blocked;
        pending |= known & POSITION;
    }

    /** Also used after repeated CarPlay cleanup while ownership is already released. */
    public synchronized void requestReplay(boolean route) {
        if (!retired) pending |= known & (route ? ROUTE : POSITION);
    }

    public synchronized void retire() {
        retired = true;
        pending = known = 0L;
        hideScalePending = hideAltitudePending = false;
        for (int i = 0; i < latest.length; i++) latest[i] = null;
    }

    public synchronized boolean hasPendingReplay() {
        return !retired && (hideScalePending || hideAltitudePending || (pending & allowed()) != 0L);
    }

    private void publish(int id, Value value) {
        if (retired) return;
        latest[id] = value;
        known |= bit(id);
        pending |= bit(id);
        if ((allowed() & bit(id)) != 0L) {
            // Keep the stock caller's exception handling; a failed handoff remains pending.
            value.send(real);
            pending &= ~bit(id);
        }
    }

    /** NavigationJobs only. A failed sync-group member keeps the entire current
     * group pending: retry RGStatus first, then complete its sync window. */
    public synchronized void replayPending() {
        if (retired) return;
        if (hideScalePending) {
            try { sendHiddenMapScale(); hideScalePending = false; }
            catch (Throwable t) { replayFailure(45, t); }
        }
        if (hideAltitudePending) {
            try { real.updateAltitude(0x7FFF, 2); hideAltitudePending = false; }
            catch (Throwable t) { replayFailure(47, t); }
        }
        long work = pending & allowed();
        long syncWork = (work & SYNC) == 0L ? 0L : known & SYNC;
        work |= syncWork;
        boolean syncFailed = false;
        for (int i = 0; i < ORDER.length; i++) {
            int id = ORDER[i];
            if ((work & bit(id)) == 0L) continue;
            try { latest[id].send(real); pending &= ~bit(id); }
            catch (Throwable t) {
                pending |= bit(id);
                if ((SYNC & bit(id)) != 0L) syncFailed = true;
                replayFailure(id, t);
            }
        }
        if (syncFailed) pending |= syncWork;
        if (!hasPendingReplay()) replayFailureLogged = false;
    }

    private void replayFailure(int id, Throwable t) {
        if (replayFailureLogged) return;
        replayFailureLogged = true;
        Log.w("NativeBAP", "restore pending FctID=" + id + ": " + t);
    }

    private void sendHiddenMapScale() {
        real.updateMapScale(15, false, 0xFFFF, 0xFF, false);
    }

    public synchronized void updateRGStatus(final int status) {
        publish(17, new Value() { public void send(CombiBAPServiceNavi to) {
            to.updateRGStatus(status);
        } });
    }

    public synchronized void updateActiveRGType(final int type) {
        publish(39, new Value() { public void send(CombiBAPServiceNavi to) {
            to.updateActiveRGType(type);
        } });
    }

    public synchronized void updateDistanceToNextManeuver(final int distance, final int unit, final boolean bargraph, final int value) {
        publish(18, new Value() { public void send(CombiBAPServiceNavi to) {
            to.updateDistanceToNextManeuver(distance, unit, bargraph, value);
        } });
    }

    public synchronized void updateCurrentPositionInfo(final String street) {
        publish(19, new Value() { public void send(CombiBAPServiceNavi to) {
            to.updateCurrentPositionInfo(street);
        } });
    }

    public synchronized void updateExitView(final int variant, final int id) {
        publish(49, new Value() { public void send(CombiBAPServiceNavi to) {
            to.updateExitView(variant, id);
        } });
    }

    public synchronized void updateManeuverState(final int state) {
        publish(55, new Value() { public void send(CombiBAPServiceNavi to) {
            to.updateManeuverState(state);
        } });
    }

    public synchronized void updateTurnToInfo(final String turn, final String sign) {
        publish(20, new Value() { public void send(CombiBAPServiceNavi to) {
            to.updateTurnToInfo(turn, sign);
        } });
    }

    public synchronized void updateDistanceToDestination(final int distance, final int unit, final boolean stopover) {
        publish(21, new Value() { public void send(CombiBAPServiceNavi to) {
            to.updateDistanceToDestination(distance, unit, stopover);
        } });
    }

    public synchronized void updateTimeToDestination(final int type, final int unit, final long time) {
        publish(22, new Value() { public void send(CombiBAPServiceNavi to) {
            to.updateTimeToDestination(type, unit, time);
        } });
    }

    public synchronized void updateManeuverDescriptor(CombiBAPNaviManeuverDescriptor[] data) {
        final CombiBAPNaviManeuverDescriptor[] saved = copyManeuvers(data);
        publish(23, new Value() { public void send(CombiBAPServiceNavi to) {
            to.updateManeuverDescriptor(copyManeuvers(saved));
        } });
    }

    public synchronized void updateLaneGuidance(final boolean enabled, CombiBAPNaviLaneGuidanceData[] data) {
        final CombiBAPNaviLaneGuidanceData[] saved = copyLanes(data);
        publish(24, new Value() { public void send(CombiBAPServiceNavi to) {
            to.updateLaneGuidance(enabled, copyLanes(saved));
        } });
    }

    public synchronized void updateDestinationInfo(CombiBAPDestinationInfo data) {
        final CombiBAPDestinationInfo saved = copyDestination(data);
        publish(46, new Value() { public void send(CombiBAPServiceNavi to) {
            to.updateDestinationInfo(copyDestination(saved));
        } });
    }

    public synchronized void updateMapScale(final int autoZoom, final boolean active,
            final int scale, final int unit, final boolean intersection) {
        publish(45, new Value() { public void send(CombiBAPServiceNavi to) {
            to.updateMapScale(autoZoom, active, scale, unit, intersection);
        } });
        if (!retired && blockRouteGuidance) {
            // Every MapScale SetGet needs a Status ACK, even if hidden value is unchanged.
            hideScalePending = true;
            sendHiddenMapScale();
            hideScalePending = false;
        }
    }

    public synchronized void updateAltitude(final int altitude, final int unit) {
        publish(47, new Value() { public void send(CombiBAPServiceNavi to) {
            to.updateAltitude(altitude, unit);
        } });
    }

    private static byte[] copyBytes(byte[] data) { return data == null ? null : (byte[]) data.clone(); }
    private static CombiBAPNaviManeuverDescriptor[] copyManeuvers(CombiBAPNaviManeuverDescriptor[] data) {
        if (data == null) return null;
        CombiBAPNaviManeuverDescriptor[] copy = new CombiBAPNaviManeuverDescriptor[data.length];
        for (int i = 0; i < data.length; i++) {
            CombiBAPNaviManeuverDescriptor d = data[i];
            if (d != null) copy[i] = new CombiBAPNaviManeuverDescriptor(
                d.mainElement, d.direction, d.zLevelGuidance, copyBytes(d.sideStreets));
        }
        return copy;
    }
    private static CombiBAPNaviLaneGuidanceData[] copyLanes(CombiBAPNaviLaneGuidanceData[] data) {
        if (data == null) return null;
        CombiBAPNaviLaneGuidanceData[] copy = new CombiBAPNaviLaneGuidanceData[data.length];
        for (int i = 0; i < data.length; i++) {
            CombiBAPNaviLaneGuidanceData d = data[i];
            if (d != null) copy[i] = new CombiBAPNaviLaneGuidanceData(d.posID, d.laneDirection,
                copyBytes(d.laneSideStreets), d.laneType, d.laneMarkingLeft,
                d.laneMarkingRight, d.laneDescription, d.guidanceInfo);
        }
        return copy;
    }
    private static CombiBAPDestinationInfo copyDestination(CombiBAPDestinationInfo data) {
        if (data == null) return null;
        CombiBAPNaviDestination d = data.getDestination();
        CombiBAPNaviDestination copy = null;
        if (d != null) {
            copy = new CombiBAPNaviDestination(d.getLastName(), d.getFirstName(), d.getStreet(),
                d.getHouseNumber(), d.getCity(), d.getCityPart(), d.getState(), d.getPostalCode(),
                d.getCountry(), d.getLatitude(), d.getLongitude(), d.getPOIType(),
                d.getPOIDescription(), d.getPOICategory(), d.getAddressType());
            copy.setNaviID(d.getNaviID());
            copy.setNaviType(d.getNaviType());
        }
        return new CombiBAPDestinationInfo(data.getNoOfStopovers(), data.getNoOfNextStopover(), copy);
    }

    /* Unowned functions retain stock behavior. */
    public void showInitializingScreen() { real.showInitializingScreen(); }
    public void hideInitializingScreen() { real.hideInitializingScreen(); }
    public void updateCompassInfo(int a, int b) { real.updateCompassInfo(a, b); }
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
