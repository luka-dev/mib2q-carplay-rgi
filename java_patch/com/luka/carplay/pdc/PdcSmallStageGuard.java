/*
 * PdcSmallStageGuard — identifies the right-hand pure OPS popup.
 *
 * ParkingSystemControllerComponentEvo publishes the incoming component list
 * before stock parking activation can send message 108. Only standalone OPS
 * may preserve CarPlay. ParkingPartialPopupHandler confirms the popup request.
 * This is deliberately earlier than partialPopupVisible(): the MMI
 * compositor may publish SMALL_STAGE or AP_METHOD_HMI_DEACTIVATED while the
 * popup animation is still starting.  While guarded, the current CarPlay
 * screen uses SMALL_STAGE_NO_CHANGE and TerminalMode keeps Resource.SCREEN.
 * The stock screen policy is restored as soon as the popup goes away.
 *
 * Java 1.2 compatible.
 *
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */
package com.luka.carplay.pdc;

import com.luka.carplay.core.CarPlayApp;
import com.luka.carplay.framework.Log;
import de.audi.atip.hmi.HMIService;
import de.audi.atip.hmi.view.Screen;
import de.esolutions.hmi.widgets.audi.base.AbstractScreenWidget;
import de.audi.app.earlyfunc.core.parking.IParkingSystem;
import de.audi.app.earlyfunc.core.parking.ParkingPopupIdentifier;
import org.dsi.ifc.carparkingsystem.DisplayContent;
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
    private static boolean suppressNextHmiDeactivation;
    private static boolean pureOpsContent;
    private static HMIService protectedHmi;
    private static int showGeneration;
    private static AbstractScreenWidget protectedScreen;
    private static int protectedScreenOriginalType = -1;

    private PdcSmallStageGuard() {}

    public static synchronized void parkingContentChanging(
        DisplayContent content, List systems, HMIService hmiService
    ) {
        pureOpsContent = isPureOpsContent(content, systems);
        if (!pureOpsContent) {
            reset();
            return;
        }
        /* Do not re-arm the one-shot AP exception on repeated content updates. */
        if (!shouldKeepCarPlayScreen()) showRequested(PURE_OPS_POPUP_ID, hmiService);
    }

    private static boolean isPureOpsContent(DisplayContent content, List systems) {
        try {
            if (content == null || content.getPopup() == 0 || systems == null || systems.size() != 1)
                return false;
            Object candidate = systems.get(0);
            if (!(candidate instanceof IParkingSystem)) return false;
            IParkingSystem ops = (IParkingSystem)candidate;
            if (ops.getParkingSystemID() != IParkingSystem.PARKING_SYSTEM_OPS) return false;
            ParkingPopupIdentifier popup = ops.getHMIPopupID(content.getPopup());
            return popup != null && popup.getPopupType() == ParkingPopupIdentifier.POPUP_TYPE_PARTIAL
                && popup.getHmiPopupID() == PURE_OPS_POPUP_ID;
        } catch (Throwable t) {
            Log.w(TAG, "OPS guard: cannot classify parking content: " + t);
            return false;
        }
    }

    public static synchronized void parkingStopped() {
        pureOpsContent = false;
        reset();
    }

    public static synchronized void showRequested(int popupId, HMIService hmiService) {
        if (popupId != PURE_OPS_POPUP_ID || !pureOpsContent) return;
        if (shouldKeepCarPlayScreen()) return;
        protectCurrentCarPlayScreen(hmiService);
        if (protectedScreen == null) return;
        showRequestedAt = System.currentTimeMillis();
        state = SHOW_REQUESTED;
        suppressNextHmiDeactivation = true;
        startShowRequestTimeout(++showGeneration);
    }

    public static synchronized void popupVisible(int popupId, int terminalId) {
        if (popupId != PURE_OPS_POPUP_ID || terminalId != MAIN_TERMINAL) return;
        /* A late callback from a previous OPS must not undo a camera takeover. */
        if (shouldKeepCarPlayScreen()) state = VISIBLE;
    }

    public static synchronized void popupHidden(int popupId, int terminalId) {
        if (popupId != PURE_OPS_POPUP_ID || terminalId != MAIN_TERMINAL) return;
        reset();
    }

    public static synchronized void popupRegisteredHidden(int popupId, int terminalId) {
        /* Registration is posted to HMI asynchronously. Its initial "hidden"
         * snapshot can arrive between the early parking intent and showPopup;
         * it is not cancellation of that in-flight request. */
        if (state == VISIBLE) popupHidden(popupId, terminalId);
    }

    public static synchronized void popupUnregistered(int popupId) {
        if (popupId == PURE_OPS_POPUP_ID) reset();
    }

    public static synchronized boolean shouldKeepCarPlayScreen() {
        if (!pureOpsContent || !isCurrentCarPlayScreen()) {
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

    private static boolean isCurrentCarPlayScreen() {
        if (!CarPlayApp.isActive() || protectedScreen == null || protectedHmi == null) return false;
        try {
            return protectedHmi.getRootWindow(MAIN_TERMINAL).getCurrentScreen() == protectedScreen;
        } catch (Throwable t) {
            return false;
        }
    }

    /** AP 1002 has no source parameter.  Consume only the first one belonging
     * to the opening parking transition; later HOME/RVC takeovers must retain
     * their stock meaning even while the right-hand popup is still visible.
     * Also require the original screen to still be current: if parking emitted
     * no AP 1002, the first later HOME must NOT consume the parking exception. */
    public static synchronized boolean consumeParkingHmiDeactivation() {
        if (!suppressNextHmiDeactivation || !shouldKeepCarPlayScreen()) return false;
        suppressNextHmiDeactivation = false;
        return true;
    }

    public static synchronized void reset() {
        state = INACTIVE;
        showRequestedAt = 0L;
        suppressNextHmiDeactivation = false;
        restoreProtectedScreen();
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
                || screen.getID() != TERMINAL_MODE_SCREEN_ID) {
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
