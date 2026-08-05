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

    /** All reads/writes of CombiBAPListener.combiservice happen on TerminalMode's dispatcher. */
    private static void scheduleInstallLocked() {
        if (installScheduled) return;
        FrameworkRef fw = CarPlayApp.fw();
        if (fw == null || fw.context() == null || fw.context().getDispatcher() == null) return;
        installScheduled = true;
        try {
            fw.context().getDispatcher().execute(new Runnable() {
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

            gate.setAltScreenTakeover(desiredActive);
            gate.setRouteGuidanceBlocked(desiredRouteBlocked);
            appliedActive = desiredActive;
            appliedRouteBlocked = desiredRouteBlocked;
        } catch (Throwable t) {
            Log.d(TAG, "gate not ready: " + t);
        }
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
}
