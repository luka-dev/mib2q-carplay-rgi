/*
 * CarPlayPhoneSMM - the stock HU phone state machine, minus the CarPlay popup.
 *
 * With CarPlay active, stock TelTerminalModeHandler sets SHOW_CAR_PLAY_DISCLAIMER_CHOICE
 * (301049) = 1, and every way into the phone app (TEL key, menu, ...) first picks the
 * "Hinweis: Your iPhone is in Apple CarPlay mode" popup (state telCarPlayDisclaimer,
 * screen TELINITPHONECARPLAY).  Those are the first branch of five entry choices,
 * transitions 300002 / 301320 / 301327 / 301389 / 301447, each guarded by that model
 * == 1.  When one is taken we press "Switch to Apple CarPlay" for the user, exactly as
 * stock CoMaCarplayHandler does: flip SWITCH_TO_TERMINAL_MODE_CHOICE (4095).
 *
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */
package com.luka.carplay.telephony;

import com.luka.carplay.framework.Log;
import de.audi.atip.base.IFrameworkAccess;
import de.audi.atip.hmi.modelaccess.ChoiceModelApp;
import de.audi.tghu.phone.sm.PhoneSMM;

public class CarPlayPhoneSMM extends PhoneSMM {
    private static final String TAG = "Telephony";
    private static final int SWITCH_TO_TERMINAL_MODE_CHOICE = 4095;

    public CarPlayPhoneSMM(IFrameworkAccess framework, int n, String name) {
        super(framework, n, name);
    }

    public boolean checkGuard(int transition, int branch) {
        boolean taken = super.checkGuard(transition, branch);
        if (taken && branch == 0 && isDisclaimerEntry(transition)) {
            try {
                ChoiceModelApp m = getFramework().getHmiServiceApp().getChoiceModel(SWITCH_TO_TERMINAL_MODE_CHOICE);
                m.setValue(m.getValue() == 0 ? 1 : 0);
                Log.i(TAG, "CarPlay popup skipped (transition " + transition + ") -> Apple CarPlay");
            } catch (Throwable t) {
                Log.w(TAG, "CarPlay popup skip failed: " + t);   /* stock popup stays */
            }
        }
        return taken;
    }

    private static boolean isDisclaimerEntry(int t) {
        return t == 300002 || t == 301320 || t == 301327 || t == 301389 || t == 301447;
    }
}
