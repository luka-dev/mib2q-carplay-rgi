/*
 * CarPlayDisclaimerSkip - presses the HU's "Switch to Apple CarPlay" for the user.
 *
 * With CarPlay active the HU puts up "Your iPhone is in Apple CarPlay mode" (phone state
 * telCarPlayDisclaimer_compound) whenever the phone app is entered.  Its button is
 * SWITCH_TO_CARPLAY_BUTTON, which stock CoMaCarplayHandler turns into a flip of
 * SWITCH_TO_TERMINAL_MODE_CHOICE (4095); system mediator 76 maps that change to event
 * 2000 SWITCH_TO_TERMINAL_MODE, which the system root state takes to CarPlay (state 253).
 * The popup is chosen in two places, both hooked: the system SM (TEL key, transition 833)
 * and the phone SM (five in-app entries).  A second press within a second is dropped, so
 * one TEL press that passes both never flips the toggle twice.
 *
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */
package com.luka.carplay.telephony;

import com.luka.carplay.framework.Log;
import de.audi.atip.base.IFrameworkAccess;
import de.audi.atip.hmi.modelaccess.ChoiceModelApp;

public final class CarPlayDisclaimerSkip {
    private static final String TAG = "Telephony";
    private static final int SWITCH_TO_TERMINAL_MODE_CHOICE = 4095;
    private static final long REPRESS_GUARD_MS = 1000;
    private static long lastPress;

    private CarPlayDisclaimerSkip() {}

    /** The CarPlay popup is about to be entered by {@code source}: go to CarPlay instead. */
    public static void press(IFrameworkAccess framework, String source) {
        long now = System.currentTimeMillis();
        synchronized (CarPlayDisclaimerSkip.class) {
            if (now >= lastPress && now - lastPress < REPRESS_GUARD_MS) return;
            lastPress = now;
        }
        try {
            ChoiceModelApp m = framework.getHmiServiceApp().getChoiceModel(SWITCH_TO_TERMINAL_MODE_CHOICE);
            m.setValue(m.getValue() == 0 ? 1 : 0);
            Log.i(TAG, "CarPlay popup skipped (" + source + ") -> Apple CarPlay");
        } catch (Throwable t) {
            Log.w(TAG, "CarPlay popup skip failed: " + t);   /* stock popup stays */
        }
    }
}
