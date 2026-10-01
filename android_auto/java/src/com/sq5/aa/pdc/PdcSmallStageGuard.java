/*
 * PdcSmallStageGuard - Android Auto (MU0918) port of luka-dev's
 * com.luka.carplay.pdc.PdcSmallStageGuard ("Audi front PDC no longer hides CarPlay").
 *
 * Identifies the right-hand pure OPS partial popup 2100008.  While it is shown beside a
 * foreground Android Auto projection, the TerminalMode screen 3200000 uses
 * SMALL_STAGE_NO_CHANGE (6) instead of its stock SMALL_STAGE_OMISSION (1) and TerminalMode keeps
 * Resource.SCREEN (ExternalEventsListener asks shouldKeepProjectionScreen()).  The stock screen
 * policy is restored as soon as the popup goes away, the projection ends, another parking system
 * (camera/VPS, ARA, active PLA) takes over, or an unconfirmed show request expires (3 s).
 *
 * Differences from luka's CarPlay guard:
 *   - "CarPlayApp.isActive()" is replaced by isProjectionActive(): the TerminalMode active device is
 *     an Android Auto device in ACTIVATING/ACTIVE (same test as AaLukaBridge).  CarPlay stays stock:
 *     on MU0918 message 108 still drives the CarPlay Main-Wizard takeover in the unpatched
 *     HighPriorityResourceTracker, so a CarPlay exception here would fight it.
 *   - Disconnect cleanup comes from our own IActiveDeviceStateListener (attachTerminalMode), not
 *     from CarPlayApp's lifecycle worker.
 *   - SD kill switch: sq5_pdc_off on the SD root (/fs/sda0 or /net/mmx/fs/sda0) -> never armed,
 *     every patched class takes its stock path.  Checked on every parking content change.
 *   - Logging: com.sq5.aa.luka.AaLog (/tmp/sq5_aa.log), prefix "PDC ".
 *   - MU0918 constants are literals (verified from lsd.jxe ROM field values, see build.sh).
 *
 * Safety: this never suppresses the parking popup, 108, the camera or any parking component; it
 * only decides whether TerminalMode keeps the screen beside the right-hand OPS popup.  Any
 * exception or unknown state answers "stock".
 *
 * Java 1.4 / Foundation 1.1.  Based on code Copyright (c) 2026 LuKa (@LuKa_dev).
 */
package com.sq5.aa.pdc;

import com.sq5.aa.luka.AaLog;
import de.audi.app.earlyfunc.core.parking.IParkingSystem;
import de.audi.app.earlyfunc.core.parking.ParkingPopupIdentifier;
import de.audi.app.earlyfunc.core.parking.pla.AbstractParkingSystemPLAComponent;
import de.audi.app.terminalmode.IContext;
import de.audi.app.terminalmode.device.IActiveDeviceStateListener;
import de.audi.app.terminalmode.device.IDeviceManager;
import de.audi.app.terminalmode.device.TMDevice;
import de.audi.atip.hmi.HMIService;
import de.audi.atip.hmi.view.IScreenManager;
import de.audi.atip.hmi.view.Screen;
import de.esolutions.hmi.widgets.audi.base.AbstractScreenWidget;
import java.io.File;
import java.util.List;
import org.dsi.ifc.carparkingsystem.DisplayContent;

public final class PdcSmallStageGuard {
    public static final int PURE_OPS_POPUP_ID = 2100008;      /* ParkingSystemOPSComponentEvo partial */
    public static final int MAIN_TERMINAL = 0;
    public static final int TERMINAL_MODE_SCREEN_ID = 3200000; /* TerminalModeHMIApplication screen */

    /* MU0918 values (lsd.jxe): AbstractScreenWidget, DSICarParkingSystem, IParkingSystem,
     * ParkingPopupIdentifier.  The converted lsd.jar lost these ConstantValue attributes. */
    static final int SMALL_STAGE_NO_CHANGE = 6;
    static final int POPUP_NONE = 0;
    static final int POPUP_OPS = 1;
    static final int POPUP_OPSAUTOACTIVATION = 4;
    static final int POPUP_OPSFLANKGUARD = 7;
    static final int POPUP_OPSOFFROAD = 17;
    static final int VPSMODE_TRAILERASSIST_ARA = 12;
    static final int PARKING_SYSTEM_OPS = 2;
    static final int PARKING_SYSTEM_PLA = 16;
    static final int POPUP_TYPE_PARTIAL = 1;

    /* A rejected show request must not leave the projection pinned forever. */
    static final long SHOW_REQUEST_TIMEOUT_MS = 3000L;

    private static final String[] SD_ROOTS = {"/fs/sda0/", "/net/mmx/fs/sda0/"};
    static final String KILL_FLAG = "sq5_pdc_off";

    private static final int INACTIVE = 0;
    private static final int SHOW_REQUESTED = 1;
    private static final int VISIBLE = 2;

    private static volatile int state;
    private static volatile long showRequestedAt;
    private static boolean pureOpsContent;
    /* Popup visibility is independent of projection focus. HOME can release our screen override
     * while the same OPS remains open, with no new DSI edge. */
    private static boolean opsPopupVisible;
    private static AbstractParkingSystemPLAComponent passivePla;
    private static HMIService protectedHmi;
    private static int showGeneration;
    private static int takeoverGeneration;
    private static AbstractScreenWidget protectedScreen;
    private static int protectedScreenOriginalType = -1;
    private static boolean killLogged;

    /* TerminalMode side, registered by ExternalEventsListener.init(). */
    private static volatile IDeviceManager deviceManager;
    private static IActiveDeviceStateListener deviceListener;
    /* Test hook: overrides the SD kill-switch probe when non-null. */
    private static volatile Boolean killOverride;

    private PdcSmallStageGuard() {}

    /** Evidence attached to one queued AP 1002, not a persistent exception.  Normal OPS close
     * restores the widget immediately but the AP's 70 ms debounce may finish afterwards.  A
     * takeover revokes it; consecutive pure OPS show/hide cycles on the same screen do not. */
    public static final class ParkingTransition {
        private final int generation;
        private final HMIService hmi;
        private final AbstractScreenWidget screen;
        private final AbstractParkingSystemPLAComponent pla;

        private ParkingTransition() {
            generation = takeoverGeneration;
            hmi = protectedHmi;
            screen = protectedScreen;
            pla = passivePla;
        }
    }

    /* ------------------------------------------------------------------ Android Auto state */

    /** ExternalEventsListener.init(): remember the device manager and restore the stock screen
     * policy on projection end without waiting for another HMI event.  Never throws. */
    public static void attachTerminalMode(IContext context) {
        try {
            IDeviceManager dm = context == null ? null : context.getDeviceManager();
            if (dm == null) return;
            IActiveDeviceStateListener listener = new IActiveDeviceStateListener() {
                public void updateActiveDeviceState(TMDevice device) {
                    if (!isAndroidAutoSession(device)) projectionMayHaveEnded();
                }
            };
            synchronized (PdcSmallStageGuard.class) {
                detachLocked();
                deviceManager = dm;
                deviceListener = listener;
            }
            dm.addActiveDeviceListener(listener);
        } catch (Throwable t) {
            AaLog.log("PDC attach failed: " + t);
        }
    }

    /** ExternalEventsListener.deinit(). */
    public static void detachTerminalMode() {
        try {
            synchronized (PdcSmallStageGuard.class) {
                detachLocked();
                reset();
            }
        } catch (Throwable t) {
            AaLog.log("PDC detach failed: " + t);
        }
    }

    private static void detachLocked() {
        IDeviceManager dm = deviceManager;
        IActiveDeviceStateListener l = deviceListener;
        deviceManager = null;
        deviceListener = null;
        if (dm != null && l != null) {
            try { dm.removeActiveDeviceListener(l); } catch (Throwable t) { /* stock teardown */ }
        }
    }

    static boolean isAndroidAutoSession(TMDevice d) {
        if (d == null || !d.isAndroidAutoDevice()) return false;
        TMDevice.ConnectionState s = d.connectionState();
        return s != null && (s.is(TMDevice.ConnectionState.ACTIVATING) || s.is(TMDevice.ConnectionState.ACTIVE));
    }

    /** True while an Android Auto projection session is up (replaces CarPlayApp.isActive()). */
    public static boolean isProjectionActive() {
        IDeviceManager dm = deviceManager;
        if (dm == null) return false;
        try {
            return isAndroidAutoSession(dm.getActiveDevice());
        } catch (Throwable t) {
            return false;
        }
    }

    /* Device callbacks run on the TerminalMode thread; the restore touches the HMI screen widget
     * and the audio drawer model, so it runs on its own short-lived worker (luka used the
     * CarPlayApp lifecycle worker). */
    private static void projectionMayHaveEnded() {
        if (state == INACTIVE && protectedScreen == null) return;
        Thread t = new Thread(new Runnable() {
            public void run() { projectionEnded(); }
        }, "sq5-pdc-release");
        t.setDaemon(true);
        t.start();
    }

    public static synchronized void projectionEnded() {
        if (!isProjectionActive()) {
            if (state != INACTIVE || protectedScreen != null) AaLog.log("PDC projection ended: restore stock screen policy");
            reset();
        }
    }

    static boolean killSwitch() {
        Boolean o = killOverride;
        if (o != null) return o.booleanValue();
        for (int i = 0; i < SD_ROOTS.length; i++) {
            try {
                if (new File(SD_ROOTS[i] + KILL_FLAG).exists()) return true;
            } catch (Throwable t) { /* ignore */ }
        }
        return false;
    }

    /** Host tests only. */
    public static void setKillSwitchForTest(Boolean value) { killOverride = value; }

    /** Host tests only: attach without an IContext. */
    public static synchronized void setDeviceManagerForTest(IDeviceManager dm) { deviceManager = dm; }

    /* ------------------------------------------------------------------ parking side */

    /** ParkingSystemControllerComponentEvo.deactivateCurrentlyVisibleParkingSystems: the target
     * content and its component list, before any component is (de)activated. */
    public static synchronized void parkingContentChanging(DisplayContent content, List systems, HMIService hmiService) {
        boolean killed = killSwitch();
        pureOpsContent = !killed && isPureOpsContent(content, systems);
        if (killed) {
            if (!killLogged) AaLog.log("PDC kill switch " + KILL_FLAG + " present - stock parking presentation");
            killLogged = true;
        } else {
            killLogged = false;
        }
        AaLog.log("PDC parking intent popup=" + (content == null ? -1 : content.getPopup())
            + " mode=" + (content == null ? -1 : content.getMode())
            + " systems=" + systemIDs(systems) + " standaloneOPS=" + pureOpsContent
            + " aa=" + isProjectionActive());
        if (!pureOpsContent) {
            if (content != null && content.getPopup() == POPUP_NONE
                && content.getMode() != VPSMODE_TRAILERASSIST_ARA
                && systems != null && systems.isEmpty()) releaseScreen();
            else reset();
            return;
        }
        /* Repeated content updates belong to the same parking interval. */
        if (!shouldKeepProjectionScreen()) showRequested(PURE_OPS_POPUP_ID, hmiService);
    }

    private static boolean isPureOpsContent(DisplayContent content, List systems) {
        passivePla = null;
        try {
            if (content == null || systems == null || systems.isEmpty()
                || content.getMode() == VPSMODE_TRAILERASSIST_ARA)
                return false;
            /* DSI intent as well as registered components: missing camera or ARA components must
             * not turn a combined request into pure OPS. */
            switch (content.getPopup()) {
                case POPUP_OPS:
                case POPUP_OPSAUTOACTIVATION:
                case POPUP_OPSFLANKGUARD:
                case POPUP_OPSOFFROAD:
                    break;
                default:
                    return false;
            }
            IParkingSystem ops = null;
            AbstractParkingSystemPLAComponent pla = null;
            for (int i = 0; i < systems.size(); i++) {
                Object candidate = systems.get(i);
                if (!(candidate instanceof IParkingSystem)) return false;
                IParkingSystem system = (IParkingSystem) candidate;
                if (system.getParkingSystemID() == PARKING_SYSTEM_OPS) {
                    if (ops != null) return false;
                    ops = system;
                } else if (system.getParkingSystemID() == PARKING_SYSTEM_PLA
                           && system instanceof AbstractParkingSystemPLAComponent) {
                    if (pla != null) return false;
                    pla = (AbstractParkingSystemPLAComponent) system;
                    /* Stock registers PLA for ordinary OPS too; isSystemActive() tracks the
                     * actual PDCPLAStatus (MU0918 AbstractParkingSystemPLAComponent:173). */
                    if (pla.isSystemActive()) return false;
                } else {
                    return false;
                }
            }
            if (ops == null) return false;
            ParkingPopupIdentifier popup = ops.getHMIPopupID(content.getPopup());
            if (popup == null || popup.getPopupType() != POPUP_TYPE_PARTIAL
                || popup.getHmiPopupID() != PURE_OPS_POPUP_ID) return false;
            passivePla = pla;
            return true;
        } catch (Throwable t) {
            AaLog.log("PDC cannot classify parking content: " + t);
            return false;
        }
    }

    public static synchronized void parkingStopped() {
        pureOpsContent = false;
        passivePla = null;
        opsPopupVisible = false;
        reset();
    }

    /** AP 1001 (HMI activated) while the projection may be entered with OPS already open. */
    public static synchronized void projectionScreenActivated(HMIService hmiService) {
        if (pureOpsContent && opsPopupVisible) {
            showRequested(PURE_OPS_POPUP_ID, hmiService);
        }
    }

    public static synchronized void showRequested(int popupId, HMIService hmiService) {
        if (popupId != PURE_OPS_POPUP_ID || !pureOpsContent) return;
        if (shouldKeepProjectionScreen()) return;
        // The validation above can discover an independently activated PLA.
        if (!pureOpsContent) return;
        protectCurrentProjectionScreen(hmiService);
        if (protectedScreen == null) return;
        showRequestedAt = System.currentTimeMillis();
        state = opsPopupVisible ? VISIBLE : SHOW_REQUESTED;
        OpsAudioDrawerPolicy.setSuppressed(true);
        int generation = ++showGeneration;
        if (state == SHOW_REQUESTED) startShowRequestTimeout(generation);
    }

    public static synchronized void popupVisible(int popupId, int terminalId) {
        if (popupId != PURE_OPS_POPUP_ID || terminalId != MAIN_TERMINAL) return;
        opsPopupVisible = true;
        /* A late callback from a previous OPS must not undo a camera takeover. */
        if (shouldKeepProjectionScreen()) state = VISIBLE;
    }

    public static synchronized void popupHidden(int popupId, int terminalId) {
        if (popupId != PURE_OPS_POPUP_ID || terminalId != MAIN_TERMINAL) return;
        opsPopupVisible = false;
        releaseScreen();
    }

    public static synchronized void popupRegisteredHidden(int popupId, int terminalId) {
        /* Registration is posted to HMI asynchronously. Its initial "hidden" snapshot can arrive
         * between the early parking intent and showPopup; it is not a cancellation. */
        if (state != SHOW_REQUESTED) popupHidden(popupId, terminalId);
    }

    public static synchronized void popupUnregistered(int popupId) {
        if (popupId == PURE_OPS_POPUP_ID) {
            opsPopupVisible = false;
            reset();
        }
    }

    /** A popup manager may still reference the previous TerminalMode screen during replacement.
     * Only the exact protected screen owns this override. */
    public static synchronized boolean shouldKeepProjectionScreen(Screen screen) {
        return screen != null && screen == protectedScreen && shouldKeepProjectionScreen();
    }

    public static synchronized boolean shouldKeepProjectionScreen() {
        /* PLA status can change during an existing OPS popup without a new DisplayContent. */
        if (passivePla != null) {
            boolean plaActive;
            try { plaActive = passivePla.isSystemActive(); } catch (Throwable t) { plaActive = true; }
            if (plaActive) parkingStopped();
        }
        if (!pureOpsContent || !isCurrentProjectionScreen(protectedHmi, protectedScreen)) {
            if (state != INACTIVE || protectedScreen != null) reset();
            return false;
        }
        int snapshot = state;
        if (snapshot == VISIBLE) return true;
        if (snapshot != SHOW_REQUESTED) return false;

        long age = System.currentTimeMillis() - showRequestedAt;
        if (age >= 0L && age <= SHOW_REQUEST_TIMEOUT_MS) return true;

        /* No visible callback arrived: show was rejected or canceled. */
        if (state == SHOW_REQUESTED) reset();
        return false;
    }

    private static boolean isCurrentProjectionScreen(HMIService hmi, Screen screen) {
        if (screen == null || hmi == null || !isProjectionActive()) return false;
        try {
            if (hmi.getRootWindow(MAIN_TERMINAL).getCurrentScreen() != screen) return false;
            IScreenManager manager = hmi.getScreenManager(MAIN_TERMINAL);
            /* currentScreen remains the projection during a HOME fade-out; a full popup can also
             * replace only currentConnectedScreen. Partial OPS changes neither. */
            return manager != null && manager.getCurrentConnectedScreen() == screen
                && (!manager.isScreenChangePending()
                    || manager.getPendingScreenId() == TERMINAL_MODE_SCREEN_ID);
        } catch (Throwable t) {
            return false;
        }
    }

    private static String systemIDs(List systems) {
        if (systems == null) return "null";
        StringBuffer ids = new StringBuffer();
        try {
            for (int i = 0; i < systems.size(); i++) {
                if (i != 0) ids.append(',');
                Object system = systems.get(i);
                if (system instanceof IParkingSystem) ids.append(((IParkingSystem) system).getParkingSystemID());
                else ids.append('?');
                if (system instanceof AbstractParkingSystemPLAComponent)
                    ids.append(((AbstractParkingSystemPLAComponent) system).isSystemActive() ? "(active)" : "(idle)");
            }
        } catch (Throwable ignored) { ids.append('?'); }
        return ids.toString();
    }

    public static synchronized ParkingTransition captureHmiDeactivation() {
        return shouldKeepProjectionScreen() ? new ParkingTransition() : null;
    }

    public static synchronized boolean isParkingHmiDeactivation(ParkingTransition transition) {
        if (shouldKeepProjectionScreen()) return true;
        boolean plaActive = false;
        try {
            plaActive = transition != null && transition.pla != null && transition.pla.isSystemActive();
        } catch (Throwable t) {
            plaActive = true;
        }
        return transition != null && transition.generation == takeoverGeneration && !plaActive
            && isCurrentProjectionScreen(transition.hmi, transition.screen);
    }

    public static synchronized void reset() {
        takeoverGeneration++;
        releaseScreen();
    }

    /** Host tests only. */
    public static synchronized boolean isArmedForTest() {
        return state != INACTIVE && protectedScreen != null;
    }

    private static void releaseScreen() {
        state = INACTIVE;
        showRequestedAt = 0L;
        showGeneration++;
        restoreProtectedScreen();
        OpsAudioDrawerPolicy.setSuppressed(false);
    }

    private static void startShowRequestTimeout(final int generation) {
        Thread timeout = new Thread(new Runnable() {
            public void run() {
                try {
                    Thread.sleep(SHOW_REQUEST_TIMEOUT_MS);
                } catch (InterruptedException e) {
                    return;
                }
                synchronized (PdcSmallStageGuard.class) {
                    if (generation == showGeneration && state == SHOW_REQUESTED) {
                        AaLog.log("PDC show request timed out");
                        reset();
                    }
                }
            }
        }, "sq5-pdc-timeout");
        timeout.setDaemon(true);
        timeout.start();
    }

    private static void protectCurrentProjectionScreen(HMIService hmiService) {
        if (hmiService == null || protectedScreen != null || !isProjectionActive()) return;
        try {
            Screen screen = hmiService.getRootWindow(MAIN_TERMINAL).getCurrentScreen();
            if (!(screen instanceof AbstractScreenWidget)
                || screen.getID() != TERMINAL_MODE_SCREEN_ID
                || !isCurrentProjectionScreen(hmiService, screen)) {
                AaLog.log("PDC active screen is not TerminalMode " + TERMINAL_MODE_SCREEN_ID
                    + " (" + (screen == null ? "none" : String.valueOf(screen.getID())) + "): stock");
                return;
            }
            AbstractScreenWidget widget = (AbstractScreenWidget) screen;
            int originalType = widget.getSmallStageType();
            protectedScreen = widget;
            protectedHmi = hmiService;
            protectedScreenOriginalType = originalType;
            if (originalType != SMALL_STAGE_NO_CHANGE) widget.setSmallStageType(SMALL_STAGE_NO_CHANGE);
            AaLog.log("PDC screen " + TERMINAL_MODE_SCREEN_ID + " smallStageType " + originalType
                + " -> " + SMALL_STAGE_NO_CHANGE + " (keep Android Auto beside OPS)");
        } catch (Throwable t) {
            protectedScreen = null;
            protectedHmi = null;
            protectedScreenOriginalType = -1;
            AaLog.log("PDC cannot protect TerminalMode screen: " + t);
        }
    }

    private static void restoreProtectedScreen() {
        AbstractScreenWidget widget = protectedScreen;
        int originalType = protectedScreenOriginalType;
        protectedScreen = null;
        protectedHmi = null;
        protectedScreenOriginalType = -1;
        if (widget == null || originalType < 0) return;
        try {
            if (widget.getSmallStageType() == SMALL_STAGE_NO_CHANGE) widget.setSmallStageType(originalType);
            AaLog.log("PDC restored screen " + TERMINAL_MODE_SCREEN_ID + " smallStageType " + originalType);
        } catch (Throwable t) {
            AaLog.log("PDC cannot restore TerminalMode screen: " + t);
        }
    }
}
