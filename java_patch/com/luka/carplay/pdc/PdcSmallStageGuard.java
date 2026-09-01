/*
 * PdcSmallStageGuard — identifies the right-hand pure OPS popup.
 *
 * ParkingPartialPopupHandler raises this guard before asking HMI to show popup
 * 2100008.  This is deliberately earlier than partialPopupVisible(): the MMI
 * compositor may publish SMALL_STAGE while the popup animation is still
 * starting.  ExternalEventsListener uses the guard only on the Kombi-stage
 * path; ordinary AP_METHOD_HMI_DEACTIVATED events remain completely stock.
 *
 * Java 1.2 compatible.
 *
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */
package com.luka.carplay.pdc;

public final class PdcSmallStageGuard {
    public static final int PURE_OPS_POPUP_ID = 2100008;
    public static final int MAIN_TERMINAL = 0;

    /* A rejected show request must not leave the projection pinned forever. */
    private static final long SHOW_REQUEST_TIMEOUT_MS = 3000L;

    private static final int INACTIVE = 0;
    private static final int SHOW_REQUESTED = 1;
    private static final int VISIBLE = 2;

    /* One volatile state prevents a show/hide reader from observing a mixed
     * pair of booleans on the MMI and TerminalMode dispatcher threads. */
    private static volatile int state;
    private static volatile long showRequestedAt;

    private PdcSmallStageGuard() {}

    public static void showRequested(int popupId) {
        if (popupId != PURE_OPS_POPUP_ID) return;
        showRequestedAt = System.currentTimeMillis();
        state = SHOW_REQUESTED;
    }

    public static void popupVisible(int popupId, int terminalId) {
        if (popupId != PURE_OPS_POPUP_ID || terminalId != MAIN_TERMINAL) return;
        state = VISIBLE;
    }

    public static void popupHidden(int popupId, int terminalId) {
        if (popupId != PURE_OPS_POPUP_ID || terminalId != MAIN_TERMINAL) return;
        reset();
    }

    public static void popupUnregistered(int popupId) {
        if (popupId == PURE_OPS_POPUP_ID) reset();
    }

    public static boolean shouldKeepCarPlayScreen() {
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

    public static void reset() {
        state = INACTIVE;
        showRequestedAt = 0L;
    }
}
