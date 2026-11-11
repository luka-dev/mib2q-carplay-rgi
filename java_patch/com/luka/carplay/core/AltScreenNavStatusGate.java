/*
 * Owns the native BAP wrapper and restores released fields on NavigationJobs.
 * Copyright (c) 2026 LuKa (@LuKa_dev)
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
    private static boolean desiredCurrentPositionBlocked;
    private static boolean installScheduled;
    private static boolean seedPending;
    private static int policyRevision, appliedRevision, installTicket, queuedWaits;
    private static Thread retryThread;
    private static boolean failureLogged;

    /* A narrow dispatcher boundary also permits deterministic lifecycle tests. */
    interface NavigationAccess {
        ClusterService cluster();
        boolean execute(Runnable task);
    }
    private static NavigationAccess navigationAccess = new NavigationAccess() {
        public ClusterService cluster() {
            Navigation n = Navigation.getInstance();
            return n == null ? null : n.getClusterService();
        }
        public boolean execute(Runnable task) {
            Navigation n = Navigation.getInstance();
            if (n == null || n.getDispatcher() == null) return false;
            n.getDispatcher().execute(task);
            return true;
        }
    };

    private AltScreenNavStatusGate() { }

    private static boolean needsWork() {
        if (gate == null) return desiredRouteBlocked || desiredCurrentPositionBlocked;
        return seedPending || appliedRevision != policyRevision || gate.hasPendingReplay();
    }

    /** One lazy worker, only while policy or a restoration remains pending. It
     * survives phone disconnect, but performs no BAP/service I/O itself. */
    private static void ensureRetryLocked() {
        if (retryThread != null || !needsWork()) return;
        retryThread = new Thread(new Runnable() {
            public void run() {
                try {
                    while (true) {
                        try { Thread.sleep(1000L); }
                        catch (InterruptedException e) { return; }
                        synchronized (AltScreenNavStatusGate.class) {
                            if (!needsWork()) return;
                            // A lost dispatcher task must not hold recovery forever. A late
                            // task's ticket cannot clear or apply a newer submission.
                            if (installScheduled && ++queuedWaits >= 5) invalidateTaskLocked();
                            scheduleInstallLocked();
                        }
                    }
                } finally {
                    synchronized (AltScreenNavStatusGate.class) {
                        retryThread = null;
                        if (needsWork()) ensureRetryLocked();
                    }
                }
            }
        }, "carplay-bap-restore");
        retryThread.setDaemon(true);
        retryThread.start();
    }

    private static void invalidateTaskLocked() {
        installScheduled = false;
        queuedWaits = 0;
        ++installTicket;
    }

    private static void scheduleInstallLocked() {
        ensureRetryLocked();
        if (installScheduled || !needsWork()) return;
        installScheduled = true;
        queuedWaits = 0;
        final int ticket = ++installTicket;
        try {
            if (!navigationAccess.execute(new Runnable() {
                public void run() { installNow(ticket); }
            })) installScheduled = false;
        } catch (Throwable t) {
            installScheduled = false;
            warn(t);
        }
    }

    private static void warn(Throwable t) {
        if (failureLogged) return;
        failureLogged = true;
        Log.w(TAG, "native BAP restoration pending: " + t);
    }

    private static synchronized void installNow(int ticket) {
        if (!installScheduled || ticket != installTicket) return;
        installScheduled = false;
        try {
            ClusterService cs = navigationAccess.cluster();
            if (cs == null) return;
            CombiBAPServiceNavi current = cs.getCombiBAPListenerCombiService();
            if (current == null) return;
            if (cs != clusterService || gate == null || current != gate) {
                GatedCombiService next = current instanceof GatedCombiService
                    ? (GatedCombiService) current : new GatedCombiService(current);
                if (gate != null && gate != next) gate.retire();
                gate = next;
                clusterService = cs;
                seedPending = current != gate;
            }
            applyPolicy();
            // Bootstrap the latest native values, including fields whose stock
            // setters deduplicate identical input. Retry an interrupted bootstrap;
            // no updateAll during ordinary ownership handoff.
            if (seedPending) {
                cs.setCombiBAPListenerCombiService(gate);
                seedPending = false;
            }
            gate.replayPending();
            appliedRouteBlocked = desiredRouteBlocked;
            appliedRevision = policyRevision;
            failureLogged = false;
        } catch (Throwable t) { warn(t); }
    }

    private static void applyPolicy() {
        gate.setRouteGuidanceBlocked(desiredRouteBlocked);
        gate.setCurrentPositionInfoBlocked(desiredCurrentPositionBlocked);
    }

    /** NavigationJobs, before the stock setter's updateAll. A replaced service
     * is retired without publishing its snapshot to an obsolete endpoint. */
    public static synchronized CombiBAPServiceNavi prepareCombiBAPService(
            ClusterService cs, CombiBAPServiceNavi incoming) {
        if (incoming == null) {
            if (cs == clusterService) {
                if (gate != null) gate.retire();
                gate = null;
                clusterService = null;
                seedPending = false;
                appliedRouteBlocked = false;
                invalidateTaskLocked();
            }
            return null;
        }
        GatedCombiService next = incoming instanceof GatedCombiService
            ? (GatedCombiService) incoming : new GatedCombiService(incoming);
        if (gate != null && gate != next) gate.retire();
        gate = next;
        clusterService = cs;
        seedPending = false; // The caller immediately runs stock setCombiService/updateAll.
        applyPolicy();
        appliedRouteBlocked = desiredRouteBlocked;
        appliedRevision = policyRevision;
        invalidateTaskLocked();
        return gate;
    }

    public static synchronized boolean setRouteGuidanceBlocked(boolean blocked) {
        if (desiredRouteBlocked != blocked) ++policyRevision;
        desiredRouteBlocked = blocked;
        if (blocked) {
            // Close synchronously, before CarPlay can publish. No endpoint I/O.
            if (gate != null) gate.setRouteGuidanceBlocked(true);
        } else {
            desiredCurrentPositionBlocked = false;
            ++policyRevision;
            if (gate != null) { gate.requestReplay(true); gate.requestReplay(false); }
        }
        scheduleInstallLocked();
        return gate != null && appliedRouteBlocked == blocked && !installScheduled;
    }

    /** Acquire synchronously; release/replay only after the caller has finished
     * CarPlay cleanup. A newer acquisition supersedes a queued release. */
    public static synchronized void setCurrentPositionInfoBlocked(boolean blocked) {
        if (desiredCurrentPositionBlocked != blocked || !blocked) ++policyRevision;
        desiredCurrentPositionBlocked = blocked;
        if (gate != null) {
            if (blocked) gate.setCurrentPositionInfoBlocked(true);
            else gate.requestReplay(false);
        }
        scheduleInstallLocked();
    }

    public static synchronized void onCombiBAPServiceChanged(ClusterService cs) {
        CombiBAPServiceNavi current = null;
        try { if (cs != null) current = cs.getCombiBAPListenerCombiService(); }
        catch (Throwable t) { warn(t); }
        if (cs != clusterService || current != gate) {
            appliedRouteBlocked = false;
            appliedRevision = policyRevision - 1;
        }
        if (current == null && cs == clusterService) {
            if (gate != null) gate.retire();
            gate = null;
            clusterService = null;
            seedPending = false;
            invalidateTaskLocked();
        }
        scheduleInstallLocked();
        CarPlayApp.onNavigationServiceChanged();
    }
}
