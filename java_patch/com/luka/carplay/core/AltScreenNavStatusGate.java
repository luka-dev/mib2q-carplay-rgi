/*
 * AltScreenNavStatusGate - owns the stock NavStatus suppression while the
 * CarPlay cluster plane owns terminal 1.
 *
 * Installation is deliberately here, not in RouteGuidance: altScreen exists
 * for the whole projection session, including map-without-maneuver and the
 * short TMDev lifecycle bounces covered by videoAvailable.
 */
package com.luka.carplay.core;

import com.luka.carplay.framework.Log;
import com.luka.carplay.rgd.GatedCombiService;

import de.audi.atip.interapp.combi.bap.navi.CombiBAPServiceNavi;
import de.audi.tghu.navi.app.Navigation;
import de.audi.tghu.navi.app.cluster.ClusterService;

public final class AltScreenNavStatusGate {
    private static final String TAG = "AltStatusGate";

    private static ClusterService clusterService;
    private static GatedCombiService gate;
    private static boolean desiredActive;
    private static boolean desiredRouteBlocked;
    private static boolean appliedActive;
    private static boolean appliedRouteBlocked;
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
                Log.i(TAG, "native NavStatus gate installed/reused");
            }

            boolean releasingTakeover = appliedActive && !desiredActive;
            gate.setAltScreenTakeover(desiredActive);
            gate.setRouteGuidanceBlocked(desiredRouteBlocked);
            appliedActive = desiredActive;
            appliedRouteBlocked = desiredRouteBlocked;
            if (releasingTakeover) {
                /* Deferred callbacks normally restore the last native request.  Recompute it as
                 * well: if stock Navigation made no show/hide call during CarPlay, replay alone
                 * would leave the app-ready=true override behind after disconnect. */
                try { cs.refreshInitializingScreenAfterCarPlay(); }
                catch (Throwable t) { Log.d(TAG, "native init-screen refresh failed: " + t); }
            }
        } catch (Throwable t) {
            Log.d(TAG, "gate not ready: " + t);
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
                clusterService = null;
                gate = null;
                appliedActive = false;
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
        gate.setAltScreenTakeover(desiredActive);
        gate.setRouteGuidanceBlocked(desiredRouteBlocked);
        appliedActive = desiredActive;
        appliedRouteBlocked = desiredRouteBlocked;
        installScheduled = false;
        Log.i(TAG, "native NavStatus gate installed on Navigation service edge");
        return gate;
    }

    /** Called only when connected||videoAvailable changes. */
    public static synchronized boolean setActive(boolean active) {
        boolean changed = desiredActive != active;
        desiredActive = active;
        /* Re-run even with an existing gate: this both applies desiredActive on the stock thread
         * and detects a ClusterService replacement after component restart. */
        if (gate == null || changed || appliedActive != active) scheduleInstallLocked();
        if (gate == null && active) Log.d(TAG, "takeover pending - ClusterService not ready");
        return gate != null && appliedActive == active && !installScheduled;
    }

    /** Single owner of the native CombiBAP wrapper for both altScreen and RGI. */
    public static synchronized boolean setRouteGuidanceBlocked(boolean blocked) {
        boolean changed = desiredRouteBlocked != blocked;
        desiredRouteBlocked = blocked;
        if (gate == null || changed || appliedRouteBlocked != blocked) scheduleInstallLocked();
        return gate != null && appliedRouteBlocked == blocked && !installScheduled;
    }

    /** Exact stock-service availability edge, after prepareCombiBAPService + stock updateAll.
     * Also wakes CarPlayApp so RgdModule releases/reacquires its paired OSGi reference when the
     * Navigation service is replaced without a phone reconnect. */
    public static synchronized void onCombiBAPServiceChanged(ClusterService cs) {
        CombiBAPServiceNavi current = null;
        try { if (cs != null) current = cs.getCombiBAPListenerCombiService(); }
        catch (Throwable t) { }

        if (cs != clusterService || current != gate) {
            appliedActive = false;
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
        if (desiredActive || desiredRouteBlocked) scheduleInstallLocked();
        CarPlayApp.onNavigationServiceChanged();
    }
}
