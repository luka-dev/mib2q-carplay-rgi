/*
 * CarPlayPhoneSMM - the stock HU phone state machine, minus the CarPlay popup.
 *
 * Inside the phone app, five entry choices pick the "Your iPhone is in Apple CarPlay
 * mode" popup first: transitions 300002 / 301320 / 301327 / 301389 / 301447, branch 0,
 * each guarded by SHOW_CAR_PLAY_DISCLAIMER_CHOICE (301049) == 1.  When one is taken we
 * press "Switch to Apple CarPlay" for the user (CarPlayDisclaimerSkip).  The TEL key
 * reaches the popup through the system SM instead (CarPlaySystemSMM).  checkGuard is the
 * hook: the engine runs execTransitionAction only for transitions with a stock action,
 * and these have none.
 *
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */
package com.luka.carplay.telephony;

import de.audi.atip.base.IFrameworkAccess;
import de.audi.tghu.phone.sm.PhoneSMM;

public class CarPlayPhoneSMM extends PhoneSMM {
    public CarPlayPhoneSMM(IFrameworkAccess framework, int n, String name) {
        super(framework, n, name);
    }

    public boolean checkGuard(int transition, int branch) {
        boolean taken = super.checkGuard(transition, branch);
        if (taken && branch == 0 && isDisclaimerEntry(transition))
            CarPlayDisclaimerSkip.press(getFramework(), "phone transition " + transition);
        return taken;
    }

    private static boolean isDisclaimerEntry(int t) {
        return t == 300002 || t == 301320 || t == 301327 || t == 301389 || t == 301447;
    }
}
