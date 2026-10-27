/*
 * Patched de.audi.tghu.phone.sm.Activator: identical to stock except that it builds
 * CarPlayPhoneSMM, which skips the HU "Your iPhone is in Apple CarPlay mode" popup.
 *
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */
package de.audi.tghu.phone.sm;

import com.luka.carplay.telephony.CarPlayPhoneSMM;
import de.audi.atip.activator.AbstractSMMActivator;

public class Activator extends AbstractSMMActivator {
    public void init() {
        this.smmList = new PhoneSMM[8];
        if (this.framework.isFrontMU()) {
            this.smmList[0] = new CarPlayPhoneSMM(this.framework, 0, "main");
            if (this.framework.isDualView()) {
                this.smmList[2] = new CarPlayPhoneSMM(this.framework, 2, "frontPassenger");
            }
        }
        this.reqActionProxies = PhoneSMMActions.REQUIRED_ACTION_PROXY_MODULE_IDS;
    }
}
