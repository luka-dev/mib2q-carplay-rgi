/*
 * SQ5 (MHI2Q MU0918) Android Auto -> luka-dev mib2q-carplay-rgi cluster stack.
 *
 * Replaces aa-cluster's AaClusterBridge (which wrote BAP itself).  This class writes NO BAP:
 * it only
 *   1. drives luka's lifecycle: Android Auto device ACTIVATING/ACTIVE -> CarPlayApp.onActivate
 *      (ScreenModule + RgdModule: RouteGuidance, BAPBridge, RendererServer :19800, cluster
 *      context worker 74<->80); device gone -> CarPlayApp.onDeactivate;
 *   2. turns Android Auto's next-turn/distance DSI events into the same EVT_RGD_UPDATE text frame
 *      luka's C hook produces for an iPhone (AaRgState) and hands it to RouteGuidance in-process
 *      through CarplayBus.injectLocal (sticky, so a RouteGuidance that starts later gets the
 *      latest frame).  luka's BAPBridge is the single BAP writer.
 *
 * Hook points (same as aa-cluster v2.5): the rebuilt AndroidAuto2ListenerDistributor constructor
 * calls attach(); the rebuilt AndroidAuto2NavHandler constructor calls subscribeNavigation().
 * The lifecycle signal is the terminal-mode IDeviceManager active-device callback (the same
 * IActiveDeviceStateListener luka hooks inside TerminalModeBapCombi for CarPlay), registered
 * from attach() with the distributor's IContext, so no further stock class is replaced.
 *
 * SD-card switches (root of SD1, read at attach):
 *   sq5_aa_off      adapter disabled completely (no subscription, no luka activation)
 *   sq5_aa_noctx80  luka ScreenModule disabled: stock cluster contexts, BAP/HUD guidance only
 *                   (use when maneuver_render is not deployed)
 *   sq5_turncard_off  no cockpit turn card in the VC large map view (TurnCard): legacy 81/82 context
 *                   split, luka's KDK-only layer rule, sq5_mirror gets "no route, no card"
 *   sq5_turncard_pos  "x,y" terminal position of the turn card (default 1064,40)
 *   sq5_lanes_off   no Android Auto lane guidance (the /tmp/sq5_aa_lanes poller is not started, so
 *                   no FctID 24 / renderer lanes from Android Auto)
 *
 * Safety: every entry point catches Throwable; BAP/cluster work runs on luka's threads or on
 * this adapter's own worker, never on the Android Auto DSI dispatcher.
 */
package com.sq5.aa.luka;

import com.luka.carplay.bus.CarplayBus;
import com.luka.carplay.core.CarPlayApp;
import com.luka.carplay.core.RgdModule;
import com.luka.carplay.core.ScreenModule;

import de.audi.app.terminalmode.IContext;
import de.audi.app.terminalmode.device.IActiveDeviceStateListener;
import de.audi.app.terminalmode.device.IDeviceManager;
import de.audi.app.terminalmode.device.TMDevice;
import de.audi.app.terminalmode.dsi.androidauto2.DSIAndroidAuto2DefaultListener;
import de.audi.app.terminalmode.smartphone.androidauto2.AndroidAuto2ListenerDistributor;
import java.io.File;
import org.dsi.ifc.androidauto2.DSIAndroidAuto2;

public final class AaLukaBridge extends DSIAndroidAuto2DefaultListener implements IActiveDeviceStateListener {
    public static final String VERSION = "aa-luka 1.0";
    private static final String[] SD_ROOTS = {"/fs/sda0/", "/net/mmx/fs/sda0/"};
    private static final long WATCHDOG_MS = 15L * 60L * 1000L;
    private static final long LANE_POLL_MS = 500L;

    private static AaLukaBridge instance;

    private final IContext context;
    private final AaRgState rg = new AaRgState();
    private final Object queueLock = new Object();
    private byte[] queued;
    private boolean sessionActive;          /* guarded by this */
    private long lastNavUpdateMs;
    private int nTurn, nDist, nFrames, nDelivered;

    private AaLukaBridge(IContext context) {
        this.context = context;
    }

    private static boolean flag(String name) {
        for (int i = 0; i < SD_ROOTS.length; i++) {
            try {
                if (new File(SD_ROOTS[i] + name).exists()) return true;
            } catch (Throwable t) {
                /* ignore */
            }
        }
        return false;
    }

    /** Called from the AndroidAuto2ListenerDistributor constructor. Never throws. */
    public static synchronized void attach(AndroidAuto2ListenerDistributor distributor, IContext context) {
        try {
            if (flag("sq5_aa_off")) {
                AaLog.log("kill switch sq5_aa_off present - adapter disabled");
                return;
            }
            try {
                com.sq5.aa.coverart.AaCoverArt.attach(distributor, context);
            } catch (Throwable t) {
                AaLog.log("coverart attach failed: " + t);
            }
            if (instance != null) {
                AaLog.log("attach: already attached, adding existing adapter to new distributor");
                distributor.addSingleListener(instance);
                return;
            }
            /* Android Auto connecting must not cancel a native route or gate native RG for the
             * whole session: luka's BAPBridge takes over per phone route instead. */
            RgdModule.setSessionTakeover(false);
            boolean ctx80 = !flag("sq5_aa_noctx80");
            ScreenModule.setClusterContextEnabled(ctx80);
            /* VC map view while the mirror is shown: suppress stock InfoStates=6 (sq5_vcmap_nogate = stock);
             * sq5_rgtype4 = opt-in stock FPK ActiveRGType 4 for Android Auto routes. */
            VcMapViewGate.configure(!flag("sq5_vcmap_nogate"), flag("sq5_rgtype4"));
            /* cockpit turn card in the VC large map view (TurnCard) + its data feed to sq5_mirror */
            boolean turnCard = !flag("sq5_turncard_off");
            TurnCard.configure(turnCard, TurnCard.parsePos(TurnCard.readSmallFile(new String[]{
                SD_ROOTS[0] + "sq5_turncard_pos", SD_ROOTS[1] + "sq5_turncard_pos"})));
            /* 2026-09-29 owner: the Google cluster card shows the same turn -> no arrow box over the full map
             * view by default (frees the top-right for the Google card); sq5_turncard_map_on restores it. */
            com.sq5.aa.input.AaClusterMenu.load();   /* saved menu settings (arrow box, map offset) */
            /* Run 102 (owner): hiding only makes the box's contents transparent (the cockpit keeps drawing the
             * empty frame) -> the arrow box is shown by default and the Google card sits left of it. */
            TurnCard.setMapCard(flag("sq5_turncard_map_on") || com.sq5.aa.input.AaClusterMenu.arrowOn());
            TurnCardFeed.configure(turnCard, null);
            AaLukaBridge b = new AaLukaBridge(context);
            instance = b;
            distributor.addSingleListener(b);
            b.startWorker();
            b.startWatchdog();
            boolean lanes = !flag("sq5_lanes_off");
            if (lanes) b.startLanePoller();
            com.sq5.aa.input.AaTouchpadInput.configure(context);
            try {
                de.audi.app.terminalmode.ITerminalModeConfiguration tc = context.getConfiguration();
                com.sq5.aa.input.AaRollerInput.configure(tc != null && tc.isKnobDirectionInverted());
            } catch (Throwable t) {
                AaLog.log("roller: configure failed: " + t);
            }
            IDeviceManager dm = context.getDeviceManager();
            dm.addActiveDeviceListener(b);
            AaLog.log("attached (" + VERSION + ", luka " + CarPlayApp.BUILD_ID + ", ctx80=" + ctx80 + ", lanes=" + lanes + ")");
            TMDevice active = dm.getActiveDevice();
            if (active != null) b.updateActiveDeviceState(active);
        } catch (Throwable t) {
            AaLog.log("attach failed: " + t);
        }
    }

    /**
     * Called from the AndroidAuto2NavHandler constructor.  Stock MU0918 only subscribes Android
     * Auto attributes {3, 5}; gal forwards next-turn data only when subscribed.  The constants
     * are read from the unit's own DSIAndroidAuto2 at runtime (getstatic). Never throws.
     */
    public static void subscribeNavigation(DSIAndroidAuto2 dsi) {
        try {
            if (dsi == null) {
                AaLog.log("subscribeNavigation: no DSI");
                return;
            }
            if (flag("sq5_aa_off")) {
                AaLog.log("kill switch present - navigation subscription skipped");
                return;
            }
            int[] attrs = new int[]{
                    DSIAndroidAuto2.ATTR_CALLSTATE,
                    DSIAndroidAuto2.ATTR_TELEPHONYSTATE,
                    DSIAndroidAuto2.ATTR_NOWPLAYINGDATA,
                    DSIAndroidAuto2.ATTR_PLAYBACKSTATE,
                    DSIAndroidAuto2.ATTR_PLAYPOSITION,
                    DSIAndroidAuto2.ATTR_COVERARTURL,
                    DSIAndroidAuto2.ATTR_NAVIGATIONNEXTTURNEVENT,
                    DSIAndroidAuto2.ATTR_NAVIGATIONNEXTTURNDISTANCE};
            dsi.setNotification(attrs, null);
            StringBuffer sb = new StringBuffer();
            for (int i = 0; i < attrs.length; i++) {
                sb.append(i == 0 ? "" : ",").append(attrs[i]);
            }
            AaLog.log("subscribed Android Auto attributes " + sb + " via " + dsi.getClass().getName());
        } catch (Throwable t) {
            AaLog.log("subscribeNavigation failed: " + t);
        }
    }

    /* ------------------------------------------------------------------ session lifecycle */

    private static boolean sessionState(TMDevice d) {
        if (d == null || !d.isAndroidAutoDevice()) return false;
        TMDevice.ConnectionState s = d.connectionState();
        return s != null && (s.is(TMDevice.ConnectionState.ACTIVATING) || s.is(TMDevice.ConnectionState.ACTIVE));
    }

    private static boolean sessionGone(TMDevice d) {
        if (d == null || !d.isAndroidAutoDevice()) return true;
        TMDevice.ConnectionState s = d.connectionState();
        return s == null || s.is(TMDevice.ConnectionState.INVALID) || s.is(TMDevice.ConnectionState.NOT_ATTACHED)
            || s.is(TMDevice.ConnectionState.ATTACHED);
    }

    public void updateActiveDeviceState(TMDevice d) {
        try {
            boolean start = false, stop = false;
            synchronized (this) {
                if (!sessionActive && sessionState(d)) {
                    sessionActive = true;
                    start = true;
                } else if (sessionActive && sessionGone(d)) {
                    sessionActive = false;
                    stop = true;
                }
            }
            AaLog.log("device " + (d == null ? "null" : d.smartphoneType() + " " + d.connectionState())
                + (start ? " -> luka activate" : stop ? " -> luka deactivate" : ""));
            if (start) {
                /* baseline: no route, so a sticky frame from an earlier session is never replayed */
                rg.resetSession();
                publish("session start");
                CarPlayApp.onActivate(context);
                com.sq5.aa.input.AaTouchpadInput.setSessionActive(true);
                com.sq5.aa.input.AaRollerInput.setSessionActive(true);
            } else if (stop) {
                /* clean route end first (RouteGuidance may still be running for ~400 ms) */
                rg.resetSession();
                publish("session end");
                CarPlayApp.onDeactivate();
                TurnCardFeed.clear();
                com.sq5.aa.input.AaTouchpadInput.setSessionActive(false);
                com.sq5.aa.input.AaRollerInput.setSessionActive(false);
            }
        } catch (Throwable t) {
            AaLog.log("device state handler failed: " + t);
        }
    }

    /* ------------------------------------------------------------------ Android Auto events */

    public void updateNavigationNextTurnEvent(String road, int turnSide, int event, int turnAngle,
                                              int turnNumber, int valid) {
        try {
            nTurn++;
            lastNavUpdateMs = System.currentTimeMillis();
            boolean changed = rg.onTurn(road, turnSide, event, turnAngle, turnNumber, valid);
            AaLog.log("turn road='" + road + "' side=" + turnSide + " event=" + event + " angle=" + turnAngle
                + " number=" + turnNumber + " valid=" + valid + " -> " + rg.lastAction());
            if (changed) publish("turn");
        } catch (Throwable t) {
            AaLog.log("turn handler failed: " + t);
        }
    }

    public void updateNavigationNextTurnDistance(int distanceMeters, int timeSeconds, int valid) {
        try {
            nDist++;
            lastNavUpdateMs = System.currentTimeMillis();
            boolean changed = rg.onDistance(distanceMeters, timeSeconds, valid);
            AaLog.log("distance m=" + distanceMeters + " s=" + timeSeconds + " valid=" + valid
                + (changed ? "" : " -> " + rg.lastAction()));
            if (changed) publish("distance");
        } catch (Throwable t) {
            AaLog.log("distance handler failed: " + t);
        }
    }

    public void navFocusRequestNotification(int focus, int valid) {
        try {
            boolean changed = rg.onNavFocus(focus, valid);
            AaLog.log("navFocus focus=" + focus + " valid=" + valid + (changed ? " -> " + rg.lastAction() : ""));
            if (changed) publish("navFocus");
        } catch (Throwable t) {
            AaLog.log("navFocus handler failed: " + t);
        }
    }

    /* ------------------------------------------------------------------ frame delivery */

    /** Latest-wins hand-off to the worker; full frames make coalescing lossless. */
    private void publish(String why) {
        synchronized (queueLock) {
            try {
                queued = rg.snapshot().getBytes("UTF-8");
            } catch (Throwable t) {
                queued = rg.snapshot().getBytes();
            }
            nFrames++;
            queueLock.notifyAll();
        }
    }

    private void startWorker() {
        Thread t = new Thread("sq5-aa-luka-rgd") {
            public void run() {
                while (true) {
                    byte[] p;
                    try {
                        synchronized (queueLock) {
                            while (queued == null) queueLock.wait();
                            p = queued;
                            queued = null;
                        }
                        boolean heard = CarplayBus.getInstance().injectLocal(CarplayBus.EVT_RGD_UPDATE,
                            CarplayBus.FLAG_STICKY, p);
                        if (heard) nDelivered++;
                    } catch (Throwable e) {
                        AaLog.log("rgd worker: " + e);
                    }
                }
            }
        };
        t.setDaemon(true);
        t.start();
    }

    /* Lanes of the current Android Auto step (gal hook record, AaLaneFeed), polled while a route is
     * active; a change is published as a full frame like a turn or distance. */
    private void startLanePoller() {
        Thread t = new Thread("sq5-aa-luka-lanes") {
            public void run() {
                while (true) {
                    try {
                        Thread.sleep(LANE_POLL_MS);
                        if (!rg.isRouteActive()) continue;       /* route end already cleared the lanes */
                        if (rg.onLanes(AaLaneFeed.read(AaLaneFeed.PATH))) {
                            AaLog.log("lanes " + (rg.lanes().length() == 0 ? "none" : rg.lanes()) + " event=" + rg.laneEvent());
                            publish("lanes");
                        }
                    } catch (Throwable e) {
                        AaLog.log("lanes: " + e);
                    }
                }
            }
        };
        t.setDaemon(true);
        t.start();
    }

    private void startWatchdog() {
        Thread t = new Thread("sq5-aa-luka-watchdog") {
            private long lastHb;
            public void run() {
                while (true) {
                    try {
                        Thread.sleep(5000L);
                        long now = System.currentTimeMillis();
                        if (rg.isRouteActive() && now - lastHb >= 30000L) {
                            lastHb = now;
                            AaLog.log("hb route gen=" + rg.routeGeneration() + " v=" + rg.maneuverVersion()
                                + " maneuver=" + rg.current() + " m=" + rg.distance() + " turns=" + nTurn
                                + " dists=" + nDist + " frames=" + nFrames + " delivered=" + nDelivered
                                + " luka=" + CarPlayApp.isActive() + " lastUpdateAgoS=" + ((now - lastNavUpdateMs) / 1000));
                        }
                        if (rg.isRouteActive() && now - lastNavUpdateMs > WATCHDOG_MS
                                && rg.endRoute("safety net: no Android Auto navigation update for "
                                    + (WATCHDOG_MS / 60000) + " min")) {
                            AaLog.log(rg.lastAction());
                            publish("watchdog");
                        }
                    } catch (Throwable e) {
                        AaLog.log("watchdog: " + e);
                    }
                }
            }
        };
        t.setDaemon(true);
        t.start();
    }
}
