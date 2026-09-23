/*
 * AltScreenNavStatusGate - installs the single wrapper that prevents stock
 * route-guidance BAP writes from racing CarPlay RGI and keeps the independent
 * altScreen plane visible while stock Navigation is still INITIALIZING.
 *
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */
package com.luka.carplay.core;

import de.audi.app.combi.bap.app.navi.InitializationManagerNavi;

import com.luka.carplay.framework.Log;
import com.luka.carplay.rgd.GatedCombiService;

import de.audi.atip.interapp.combi.bap.navi.CombiBAPServiceNavi;
import de.audi.tghu.navi.app.Navigation;
import de.audi.tghu.navi.app.cluster.ClusterService;

public final class AltScreenNavStatusGate {
    private static final String TAG = "AltStatusGate";

    private static ClusterService clusterService;
    private static GatedCombiService gate;
    private static boolean desiredAltScreenActive;
    private static boolean desiredRouteBlocked;
    private static boolean appliedAltScreenActive;
    private static boolean appliedRouteBlocked;
    private static boolean desiredCurrentPositionBlocked;
    private static boolean installScheduled;

    private AltScreenNavStatusGate() { }

    /** All reads/writes of CombiBAPListener.combiservice must happen on NavigationJobs. */
    private static void scheduleInstallLocked() {
        if (installScheduled) return;
        Navigation navigation = Navigation.getInstance();
        if (navigation == null || navigation.getDispatcher() == null) return;
        installScheduled = true;
        try {
            navigation.getDispatcher().execute(new Runnable() {
                public void run() { installNow(); }
            });
        } catch (Throwable t) {
            installScheduled = false;
        }
    }

    private static synchronized void installNow() {
        installScheduled = false;
        try {
            Navigation navigation = Navigation.getInstance();
            if (navigation == null) return;
            ClusterService cs = navigation.getClusterService();
            if (cs == null) return;

            CombiBAPServiceNavi current = cs.getCombiBAPListenerCombiService();
            if (current == null) return;

            if (cs != clusterService || gate == null || current != gate) {
                if (gate != null && clusterService != cs) {
                    try { gate.setAltScreenTakeover(false); } catch (Throwable t) { }
                }
                if (current instanceof GatedCombiService) {
                    gate = (GatedCombiService) current;
                } else {
                    gate = new GatedCombiService(current);
                    cs.setCombiBAPListenerCombiService(gate);
                }
                clusterService = cs;
                Log.i(TAG, "native route-guidance/init-state gate installed/reused");
            }

            boolean releasingTakeover = appliedAltScreenActive && !desiredAltScreenActive;
            gate.setAltScreenTakeover(desiredAltScreenActive);
            gate.setRouteGuidanceBlocked(desiredRouteBlocked);
            appliedAltScreenActive = desiredAltScreenActive;
            gate.setCurrentPositionInfoBlocked(desiredCurrentPositionBlocked);
            appliedRouteBlocked = desiredRouteBlocked;
            if (releasingTakeover) {
                /* A replay handles a state edge seen during takeover.  Recompute as
                 * well for the no-edge case so stock Navigation receives its exact
                 * current INITIALIZING/NORMAL state after CarPlay disconnects. */
                try { cs.refreshInitializingScreenAfterCarPlay(); }
                catch (Throwable t) { Log.w(TAG, "native init-screen refresh failed: " + t); }
            }
        } catch (Throwable t) {
            Log.w(TAG, "init-state gate install failed: " + t);
        }
    }

    /** Called synchronously by ClusterService#setCombiBAPService on NavigationJobs, before the
     * stock CombiBAPListener setter performs updateAll().  Installing here closes the old race in
     * which updateAll painted native INITIALIZING/status values before the asynchronous wrapper
     * appeared, and avoids cross-thread writes to CombiBAPListener.combiservice. */
    public static synchronized CombiBAPServiceNavi prepareCombiBAPService(
            ClusterService cs, CombiBAPServiceNavi incoming) {
        if (incoming == null) {
            if (cs == clusterService && gate != null) {
                try { gate.setAltScreenTakeover(false); } catch (Throwable t) { }
                try { gate.setRouteGuidanceBlocked(false); } catch (Throwable t) { }
                try { gate.setCurrentPositionInfoBlocked(false); } catch (Throwable t) { }
                clusterService = null;
                gate = null;
                appliedAltScreenActive = false;
                appliedRouteBlocked = false;
            }
            return null;
        }

        GatedCombiService next;
        if (incoming instanceof GatedCombiService) {
            next = (GatedCombiService) incoming;
        } else {
            next = new GatedCombiService(incoming);
        }
        if (gate != null && gate != next) {
            try { gate.setAltScreenTakeover(false); } catch (Throwable t) { }
            try { gate.setRouteGuidanceBlocked(false); } catch (Throwable t) { }
        }
        clusterService = cs;
        gate = next;
        gate.setAltScreenTakeover(desiredAltScreenActive);
        gate.setRouteGuidanceBlocked(desiredRouteBlocked);
        gate.setCurrentPositionInfoBlocked(desiredCurrentPositionBlocked);
        appliedAltScreenActive = desiredAltScreenActive;
        appliedRouteBlocked = desiredRouteBlocked;
        installScheduled = false;
        Log.i(TAG, "native route-guidance/init-state gate installed on Navigation service edge");
        return gate;
    }

    /** Presentation ownership follows connected || videoAvailable, independently of
     * CarPlay route guidance.  A cold-boot stock map must not hold plane 99 behind
     * NAVSD INITIALIZING: the stock init manager sends it for several inputs (Navi app
     * not started, BAP init sequence, download, appIsReady), so it is clamped to NORMAL
     * in the patched InitializationManagerNavi.  That call re-enters the stock BAP
     * init manager and runs outside this class's lock. */
    public static boolean setAltScreenActive(boolean active) {
        InitializationManagerNavi.setCarPlayClusterOwner(active);
        return applyAltScreenActive(active);
    }

    private static synchronized boolean applyAltScreenActive(boolean active) {
        boolean changed = desiredAltScreenActive != active;
        desiredAltScreenActive = active;
        if (gate == null || changed || appliedAltScreenActive != active) scheduleInstallLocked();
        return gate != null && appliedAltScreenActive == active && !installScheduled;
    }

    /** Single owner of the native CombiBAP wrapper for both altScreen and RGI. */
    public static synchronized boolean setRouteGuidanceBlocked(boolean blocked) {
        boolean changed = desiredRouteBlocked != blocked;
        desiredRouteBlocked = blocked;
        if (!blocked) {
            desiredCurrentPositionBlocked = false;
            if (gate != null) gate.setCurrentPositionInfoBlocked(false);
        }
        if (gate == null || changed || appliedRouteBlocked != blocked) scheduleInstallLocked();
        return gate != null && appliedRouteBlocked == blocked && !installScheduled;
    }

    /** FctID 19/20/21/22/46 ownership follows actual CarPlay RGI, not the
     * session-long native-RG gate. The wrapper is already installed on
     * NavigationJobs, so changing these volatile policy bits requires no
     * service-field mutation. */
    public static synchronized void setCurrentPositionInfoBlocked(boolean blocked) {
        desiredCurrentPositionBlocked = blocked;
        if (gate != null) gate.setCurrentPositionInfoBlocked(blocked);
    }

    /** Exact stock-service availability edge, after prepareCombiBAPService + stock updateAll.
     * Also wakes CarPlayApp so RgdModule releases/reacquires its paired OSGi reference when the
     * Navigation service is replaced without a phone reconnect. */
    public static synchronized void onCombiBAPServiceChanged(ClusterService cs) {
        CombiBAPServiceNavi current = null;
        try { if (cs != null) current = cs.getCombiBAPListenerCombiService(); }
        catch (Throwable t) { }

        if (cs != clusterService || current != gate) {
            appliedAltScreenActive = false;
            appliedRouteBlocked = false;
        }
        if (current == null) {
            if (cs == clusterService) {
                clusterService = null;
                gate = null;
            }
            CarPlayApp.onNavigationServiceChanged();
            return;
        }
        if (desiredAltScreenActive || desiredRouteBlocked) scheduleInstallLocked();
        CarPlayApp.onNavigationServiceChanged();
    }
}
