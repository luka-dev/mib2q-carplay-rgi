/*
 * AltScreenNavStatusGate - installs the single wrapper that prevents stock
 * route-guidance BAP writes from racing CarPlay RGI.  Its independent session
 * gate masks only NAVSD Fct54 so the native navigation side menus disappear;
 * initializing state, compass and every other availability bit remain stock.
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
    private static boolean desiredRouteBlocked;
    private static boolean appliedRouteBlocked;
    private static boolean desiredSideMenusHidden;
    private static boolean appliedSideMenusHidden;
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
                if (current instanceof GatedCombiService) {
                    gate = (GatedCombiService) current;
                } else {
                    gate = new GatedCombiService(current);
                    cs.setCombiBAPListenerCombiService(gate);
                }
                clusterService = cs;
                appliedSideMenusHidden = false;
                Log.i(TAG, "native route-guidance gate installed/reused");
            }

            if (gate.setSideMenusHidden(desiredSideMenusHidden)) {
                appliedSideMenusHidden = desiredSideMenusHidden;
            }
            gate.setRouteGuidanceBlocked(desiredRouteBlocked);
            appliedRouteBlocked = desiredRouteBlocked;
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
                try { gate.setRouteGuidanceBlocked(false); } catch (Throwable t) { }
                try { gate.setSideMenusHidden(false); } catch (Throwable t) { }
                clusterService = null;
                gate = null;
                appliedRouteBlocked = false;
                appliedSideMenusHidden = false;
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
            try { gate.setRouteGuidanceBlocked(false); } catch (Throwable t) { }
            try { gate.setSideMenusHidden(false); } catch (Throwable t) { }
        }
        clusterService = cs;
        gate = next;
        appliedSideMenusHidden = false;
        if (gate.setSideMenusHidden(desiredSideMenusHidden)) {
            appliedSideMenusHidden = desiredSideMenusHidden;
        }
        gate.setRouteGuidanceBlocked(desiredRouteBlocked);
        appliedRouteBlocked = desiredRouteBlocked;
        installScheduled = false;
        Log.i(TAG, "native route-guidance gate installed on Navigation service edge");
        return gate;
    }

    /** Single owner of the native CombiBAP wrapper for both altScreen and RGI. */
    public static synchronized boolean setRouteGuidanceBlocked(boolean blocked) {
        boolean changed = desiredRouteBlocked != blocked;
        desiredRouteBlocked = blocked;
        if (gate == null || changed || appliedRouteBlocked != blocked) scheduleInstallLocked();
        return gate != null && appliedRouteBlocked == blocked && !installScheduled;
    }

    /** Session-level gate for the two native navigation side menus. */
    public static synchronized boolean setSideMenusHidden(boolean hidden) {
        boolean changed = desiredSideMenusHidden != hidden;
        desiredSideMenusHidden = hidden;
        if (gate == null || changed || appliedSideMenusHidden != hidden) scheduleInstallLocked();
        return gate != null && appliedSideMenusHidden == hidden && !installScheduled;
    }

    /** Exact stock-service availability edge, after prepareCombiBAPService + stock updateAll.
     * Also wakes CarPlayApp so RgdModule releases/reacquires its paired OSGi reference when the
     * Navigation service is replaced without a phone reconnect. */
    public static synchronized void onCombiBAPServiceChanged(ClusterService cs) {
        CombiBAPServiceNavi current = null;
        try { if (cs != null) current = cs.getCombiBAPListenerCombiService(); }
        catch (Throwable t) { }

        if (cs != clusterService || current != gate) {
            appliedRouteBlocked = false;
            appliedSideMenusHidden = false;
        }
        if (current == null) {
            if (cs == clusterService) {
                clusterService = null;
                gate = null;
            }
            CarPlayApp.onNavigationServiceChanged();
            return;
        }
        if (desiredRouteBlocked || desiredSideMenusHidden) scheduleInstallLocked();
        CarPlayApp.onNavigationServiceChanged();
    }
}
