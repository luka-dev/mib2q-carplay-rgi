/*
 * Patched de.audi.tghu.system.sm.Activator: identical to stock except that the front
 * system SMs are CarPlaySystemSMM, which skips the HU "Your iPhone is in Apple CarPlay
 * mode" popup on the TEL key.  Byte-identical stock on MU1316, MU1329 and MU1003.
 *
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */
package de.audi.tghu.system.sm;

import com.luka.carplay.telephony.CarPlaySystemSMM;
import de.audi.atip.activator.AbstractSMMActivator;
import de.audi.atip.statemachine.JointUseDummySMM;

public class Activator extends AbstractSMMActivator {
    public void init() {
        this.smmList = new SystemSMM[9];
        if (this.framework.isFrontMU()) {
            this.smmList[0] = new CarPlaySystemSMM(this.framework, 0, "main");
            if (this.framework.isDualView()) {
                this.smmList[2] = new CarPlaySystemSMM(this.framework, 2, "frontPassenger");
            }
        } else {
            this.smmList[3] = new SystemSMM(this.framework, 3, "rearSeatLeft");
            this.smmList[4] = new SystemSMM(this.framework, 4, "rearSeatRight");
            if (!Boolean.getBoolean("disableJointMode")) {
                this.smmList[5] = new JointUseSystemSMM(this.framework);
                this.smmList[8] = new JointUseDummySMM(this.framework, 5, "rearSeatJoint", 23, "NotNaviSMM", "DummyTLS");
            }
        }

        this.reqActionProxies = SystemSMMActions.REQUIRED_ACTION_PROXY_MODULE_IDS;
    }
}
