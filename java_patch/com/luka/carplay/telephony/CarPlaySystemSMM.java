/*
 * CarPlaySystemSMM - the stock system state machine, minus the CarPlay popup on TEL.
 *
 * The TEL key enters the phone app through system transition 833 (state 484), whose
 * branch 0 (SHOW_CAR_PLAY_DISCLAIMER_CHOICE 301049 == 1) targets the phone's
 * telCarPlayDisclaimer_compound directly, without passing any PhoneSMM guard.  When it
 * is taken we press "Switch to Apple CarPlay" for the user (CarPlayDisclaimerSkip).
 * Same transition and guard on MU1316 US, MU1329 EU and MU1003 CN; this only subclasses,
 * so each firmware keeps its own SystemSMM.
 *
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */
package com.luka.carplay.telephony;

import de.audi.atip.base.IFrameworkAccess;
import de.audi.tghu.system.sm.SystemSMM;

public class CarPlaySystemSMM extends SystemSMM {
    private static final int TEL_ENTRY_TRANSITION = 833;

    public CarPlaySystemSMM(IFrameworkAccess framework, int n, String name) {
        super(framework, n, name);
    }

    public boolean checkGuard(int transition, int branch) {
        boolean taken = super.checkGuard(transition, branch);
        if (taken && branch == 0 && transition == TEL_ENTRY_TRANSITION)
            CarPlayDisclaimerSkip.press(getFramework(), "system transition " + transition);
        return taken;
    }
}
