/*
 * PdcSmallStageGuard — identifies the right-hand pure OPS popup.
 *
 * ParkingSystemControllerComponentEvo publishes the incoming component list
 * before stock parking activation can send message 108. Standalone OPS can
 * include a registered but idle PLA observer; active PLA/camera/ARA still take
 * over. ParkingPartialPopupHandler confirms the popup request.
 * This is deliberately earlier than partialPopupVisible(): the MMI
 * compositor may publish SMALL_STAGE or AP_METHOD_HMI_DEACTIVATED while the
 * popup animation is still starting.  While guarded, the current CarPlay
 * screen uses SMALL_STAGE_NO_CHANGE and TerminalMode keeps Resource.SCREEN.
 * The stock screen policy is restored as soon as the popup goes away.
 *
 * Java 1.4 / Foundation 1.1.
 *
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */
package com.luka.carplay.pdc;

import com.luka.carplay.core.CarPlayApp;
import com.luka.carplay.framework.Log;
import de.audi.atip.hmi.HMIService;
import de.audi.atip.hmi.view.Screen;
import de.audi.atip.hmi.view.IScreenManager;
import de.esolutions.hmi.widgets.audi.base.AbstractScreenWidget;
import de.audi.app.earlyfunc.core.parking.IParkingSystem;
import de.audi.app.earlyfunc.core.parking.ParkingPopupIdentifier;
import de.audi.app.earlyfunc.core.parking.pla.AbstractParkingSystemPLAComponent;
import org.dsi.ifc.carparkingsystem.DisplayContent;
import org.dsi.ifc.carparkingsystem.DSICarParkingSystem;
import java.util.List;

public final class PdcSmallStageGuard {
    public static final int PURE_OPS_POPUP_ID = 2100008;
    public static final int MAIN_TERMINAL = 0;
    public static final int TERMINAL_MODE_SCREEN_ID = 3200000;

    private static final String TAG = "PDC";

    /* Keep the live projection screen in the small stage instead of applying
     * its stock SMALL_STAGE_OMISSION policy.  This is changed only for the
     * lifetime of CarPlay + popup 2100008 and restored afterwards. */
    private static final int SMALL_STAGE_NO_CHANGE = 6;

    /* A rejected show request must not leave the projection pinned forever. */
    private static final long SHOW_REQUEST_TIMEOUT_MS = 3000L;

    private static final int INACTIVE = 0;
    private static final int SHOW_REQUESTED = 1;
    private static final int VISIBLE = 2;

    /* One volatile state prevents a show/hide reader from observing a mixed
     * pair of booleans on the MMI and TerminalMode dispatcher threads. */
    private static volatile int state;
    private static volatile long showRequestedAt;
    private static boolean pureOpsContent;
    /* Popup visibility is independent of CarPlay focus. HOME can release our
     * screen override while the same OPS remains open, with no new DSI edge. */
    private static boolean opsPopupVisible;
    private static AbstractParkingSystemPLAComponent passivePla;
    private static HMIService protectedHmi;
    private static int showGeneration;
    private static int takeoverGeneration;
    private static AbstractScreenWidget protectedScreen;
    private static int protectedScreenOriginalType = -1;

    private PdcSmallStageGuard() {}

    /** Evidence attached to one queued AP 1002, not a persistent exception.
     * Normal OPS close restores the widget immediately but the AP's 70 ms
     * debounce may finish afterwards. A takeover revokes it; consecutive pure
     * OPS show/hide cycles on the same CarPlay screen do not. */
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

    public static synchronized void parkingContentChanging(
        DisplayContent content, List systems, HMIService hmiService
    ) {
        pureOpsContent = isPureOpsContent(content, systems);
        Log.i(TAG, "parking intent popup=" + (content == null ? -1 : content.getPopup())
            + " mode=" + (content == null ? -1 : content.getMode())
            + " systems=" + systemIDs(systems) + " standaloneOPS=" + pureOpsContent);
        if (!pureOpsContent) {
            if (content != null && content.getPopup() == DSICarParkingSystem.POPUP_NONE
                && content.getMode() != DSICarParkingSystem.VPSMODE_TRAILERASSIST_ARA
                && systems != null && systems.isEmpty()) releaseScreen();
            else reset();
            return;
        }
        /* Repeated content updates belong to the same parking interval. */
        if (!shouldKeepCarPlayScreen()) showRequested(PURE_OPS_POPUP_ID, hmiService);
    }

    private static boolean isPureOpsContent(DisplayContent content, List systems) {
        passivePla = null;
        try {
            if (content == null || systems == null || systems.isEmpty()
                || content.getMode() == DSICarParkingSystem.VPSMODE_TRAILERASSIST_ARA)
                return false;
            /* Check DSI intent as well as registered components. Missing camera
             * or ARA components must not turn a combined request into pure OPS. */
            switch (content.getPopup()) {
                case DSICarParkingSystem.POPUP_OPS:
                case DSICarParkingSystem.POPUP_OPSAUTOACTIVATION:
                case DSICarParkingSystem.POPUP_OPSFLANKGUARD:
                case DSICarParkingSystem.POPUP_OPSOFFROAD:
                    break;
                default:
                    return false;
            }
            IParkingSystem ops = null;
            AbstractParkingSystemPLAComponent pla = null;
            for (int i = 0; i < systems.size(); i++) {
                Object candidate = systems.get(i);
                if (!(candidate instanceof IParkingSystem)) return false;
                IParkingSystem system = (IParkingSystem)candidate;
                if (system.getParkingSystemID() == IParkingSystem.PARKING_SYSTEM_OPS) {
                    if (ops != null) return false;
                    ops = system;
                } else if (system.getParkingSystemID() == IParkingSystem.PARKING_SYSTEM_PLA
                           && system instanceof AbstractParkingSystemPLAComponent) {
                    if (pla != null) return false;
                    pla = (AbstractParkingSystemPLAComponent)system;
                    /* Stock registers PLA for ordinary OPS too. setActive(true)
                     * merely forwards DisplayContent to its popin state machine;
                     * isSystemActive() tracks actual PDCPLAStatus instead. */
                    if (pla.isSystemActive()) return false;
                } else {
                    return false;
                }
            }
            if (ops == null) return false;
            ParkingPopupIdentifier popup = ops.getHMIPopupID(content.getPopup());
            if (popup == null || popup.getPopupType() != ParkingPopupIdentifier.POPUP_TYPE_PARTIAL
                || popup.getHmiPopupID() != PURE_OPS_POPUP_ID) return false;
            passivePla = pla;
            return true;
        } catch (Throwable t) {
            Log.w(TAG, "OPS guard: cannot classify parking content: " + t);
            return false;
        }
    }

    public static synchronized void parkingStopped() {
        pureOpsContent = false;
        passivePla = null;
        opsPopupVisible = false;
        reset();
    }

    public static synchronized void carPlayScreenActivated(HMIService hmiService) {
        if (pureOpsContent && opsPopupVisible) {
            showRequested(PURE_OPS_POPUP_ID, hmiService);
        }
    }

    public static synchronized void carPlayDisconnected() {
        // The lifecycle worker may have waited behind a new activation. Check
        // again under the same monitor that protects the new OPS interval.
        if (!CarPlayApp.isActive()) reset();
    }

    public static synchronized void showRequested(int popupId, HMIService hmiService) {
        if (popupId != PURE_OPS_POPUP_ID || !pureOpsContent) return;
        if (shouldKeepCarPlayScreen()) {
            return;
        }
        // The validation above can discover an independently activated PLA.
        if (!pureOpsContent) return;
        protectCurrentCarPlayScreen(hmiService);
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
        if (shouldKeepCarPlayScreen()) {
            state = VISIBLE;
        }
    }

    public static synchronized void popupHidden(int popupId, int terminalId) {
        if (popupId != PURE_OPS_POPUP_ID || terminalId != MAIN_TERMINAL) return;
        opsPopupVisible = false;
        releaseScreen();
    }

    public static synchronized void popupRegisteredHidden(int popupId, int terminalId) {
        /* Registration is posted to HMI asynchronously. Its initial "hidden"
         * snapshot can arrive between the early parking intent and showPopup;
         * it is not cancellation of that in-flight request. */
        if (state != SHOW_REQUESTED) popupHidden(popupId, terminalId);
    }

    public static synchronized void popupUnregistered(int popupId) {
        if (popupId == PURE_OPS_POPUP_ID) {
            opsPopupVisible = false;
            reset();
        }
    }

    /** A popup manager may still reference the previous TerminalMode screen
     * during replacement. Only the exact protected screen owns this override. */
    public static synchronized boolean shouldKeepCarPlayScreen(Screen screen) {
        return screen != null && screen == protectedScreen && shouldKeepCarPlayScreen();
    }

    public static synchronized boolean shouldKeepCarPlayScreen() {
        /* PLA status can change during an existing OPS popup, without a new
         * DisplayContent. Its independent active notification also revokes the
         * guard in the controller before broadcasting 108. */
        if (passivePla != null && passivePla.isSystemActive()) parkingStopped();
        if (!pureOpsContent || !isCurrentCarPlayScreen(protectedHmi, protectedScreen)) {
            if (state != INACTIVE || protectedScreen != null) reset();
            return false;
        }
        int snapshot = state;
        if (snapshot == VISIBLE) return true;
        if (snapshot != SHOW_REQUESTED) return false;

        long age = System.currentTimeMillis() - showRequestedAt;
        if (age >= 0L && age <= SHOW_REQUEST_TIMEOUT_MS) {
            return true;
        }

        /* No visible callback arrived: show was rejected or canceled. */
        if (state == SHOW_REQUESTED) reset();
        return false;
    }

    private static boolean isCurrentCarPlayScreen(HMIService hmi, Screen screen) {
        if (!CarPlayApp.isActive() || screen == null || hmi == null) return false;
        try {
            if (hmi.getRootWindow(MAIN_TERMINAL).getCurrentScreen() != screen) return false;
            IScreenManager manager = hmi.getScreenManager(MAIN_TERMINAL);
            /* currentScreen remains CarPlay during a HOME fade-out; a full
             * popup can also replace only currentConnectedScreen. Partial OPS
             * changes neither. Unknown/disconnected state follows stock. */
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
                if (system instanceof IParkingSystem) ids.append(((IParkingSystem)system).getParkingSystemID());
                else ids.append('?');
                if (system instanceof AbstractParkingSystemPLAComponent)
                    ids.append(((AbstractParkingSystemPLAComponent)system).isSystemActive() ? "(active)" : "(idle)");
            }
        } catch (Throwable ignored) { ids.append('?'); }
        return ids.toString();
    }

    public static synchronized ParkingTransition captureHmiDeactivation() {
        return shouldKeepCarPlayScreen() ? new ParkingTransition() : null;
    }

    public static synchronized boolean isParkingHmiDeactivation(ParkingTransition transition) {
        if (shouldKeepCarPlayScreen()) return true;
        return transition != null && transition.generation == takeoverGeneration
            && (transition.pla == null || !transition.pla.isSystemActive())
            && isCurrentCarPlayScreen(transition.hmi, transition.screen);
    }

    public static synchronized void reset() {
        takeoverGeneration++;
        releaseScreen();
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
                        Log.w(TAG, "OPS guard: show request timed out");
                        reset();
                    }
                }
            }
        }, "pdc-small-stage-timeout");
        timeout.setDaemon(true);
        timeout.start();
    }

    private static void protectCurrentCarPlayScreen(HMIService hmiService) {
        if (!CarPlayApp.isActive() || hmiService == null || protectedScreen != null) return;

        try {
            Screen screen = hmiService.getRootWindow(MAIN_TERMINAL).getCurrentScreen();
            if (!(screen instanceof AbstractScreenWidget)
                || screen.getID() != TERMINAL_MODE_SCREEN_ID
                || !isCurrentCarPlayScreen(hmiService, screen)) {
                Log.w(TAG, "OPS guard: active screen is not TerminalMode 3200000");
                return;
            }

            AbstractScreenWidget widget = (AbstractScreenWidget)screen;
            int originalType = widget.getSmallStageType();
            protectedScreen = widget;
            protectedHmi = hmiService;
            protectedScreenOriginalType = originalType;
            if (originalType != SMALL_STAGE_NO_CHANGE) {
                widget.setSmallStageType(SMALL_STAGE_NO_CHANGE);
            }
            Log.i(TAG, "OPS guard: screen 3200000 smallStageType "
                + originalType + " -> " + SMALL_STAGE_NO_CHANGE);
        } catch (Throwable t) {
            protectedScreen = null;
            protectedHmi = null;
            protectedScreenOriginalType = -1;
            Log.w(TAG, "OPS guard: cannot protect TerminalMode screen: " + t);
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
            if (widget.getSmallStageType() == SMALL_STAGE_NO_CHANGE) {
                widget.setSmallStageType(originalType);
            }
            Log.i(TAG, "OPS guard: restored screen 3200000 smallStageType " + originalType);
        } catch (Throwable t) {
            Log.w(TAG, "OPS guard: cannot restore TerminalMode screen: " + t);
        }
    }
}
