package de.audi.app.combi.bap.app.phone2;

import de.audi.app.bap.IPowerState;
import de.audi.app.bap.dsi.bap.IDSIBAPController;
import de.audi.app.bap.fw.AbstractBAPModuleInitializationManagerFSG;
import de.audi.app.bap.fw.functiontypes.BAPFunctionPropertyFSG;
import de.audi.app.combi.bap.fw.AbstractCombiModule;
import de.esolutions.fw.util.commons.Buffer;
import de.vw.mib.bap.generated.telephone2.serializer.FSG_OperationState_Status;

public class InitializationManagerPhone2 extends AbstractBAPModuleInitializationManagerFSG {
    public InitializationManagerPhone2(
        AbstractCombiModule abstractcombimodule,
        BAPFunctionPropertyFSG bapfunctionpropertyfsg,
        IDSIBAPController idsibapcontroller,
        IPowerState ipowerstate
    ) {
        super(abstractcombimodule, bapfunctionpropertyfsg, idsibapcontroller, ipowerstate);
        /* EXPERIMENTAL phone-tab gate: re-push this PHONE2 FSG HMIState whenever the tab-visible
         * condition flips (BT connect/disconnect, CarPlay on/off).  The override below then forces
         * HMIState=NOT_READY(0) while hidden, which de-publishes the PHONE2 tel domain on the cluster
         * (App::setPhoneAvailable(0) -> tab avail=0 -> icon index 0 = nothing drawn).
         *
         * NB: op_State was the wrong lever — the cluster maps op_State into a separate OpState field
         * that only changes the ICON (index 2/5 "no service"), never `avail`; the tab slot stays.
         * `avail` is driven solely by the module being available as a service, i.e. HMIState. */
        com.luka.carplay.phone.PhoneTabGate.registerResend(new Runnable() {
            public void run() { InitializationManagerPhone2.this.updateHMIState(); }
        });
    }

    /* Force PHONE2 NOT_READY while the tab must be hidden; else stock HMIState (RUNNING when up).
     * dsiController + module are protected in the base, so we push the state the same way stock does. */
    protected synchronized void updateHMIState(boolean flag) {
        if (!com.luka.carplay.phone.PhoneTabGate.isTabVisible()) {
            if (this.getHMIState() != 0 || flag) {
                this.setHMIState(0);   /* NOT_READY */
                this.dsiController.setHMIState(this.module.getLSGID(), 0);
            }
            return;
        }
        super.updateHMIState(flag);
    }

    public void appStateChanged(String s, int i) {
        this.logChannel
            .log(
                100000000, "[CombiModulePhone2.InitializationManagerPhone2#appStateChanged] appName=%1, value=%2", s, i
            );
        if ("Phone".equals(s)) {
            this.processAppStateChanged(i);
        }
    }

    public String appStatesToString() {
        Buffer buffer = new Buffer();
        buffer.append("appStatePhone = ");
        buffer.append(this.getAppState());
        buffer.append('\n');
        return buffer.toString();
    }

    public boolean setFSGOperationStateValue(int i) {
        byte b0;
        switch (i) {
            case 0:
                b0 = 0;
                break;
            case 1:
                b0 = 1;
                break;
            case 2:
                b0 = 3;
                break;
            case 3:
                b0 = 15;
                break;
            default:
                this.logChannel
                    .log(
                        10000,
                        "[CombiModulePhone2.InitializationManagerPhone2#setFSGOperationStateValue] invalid opState: %1",
                        i
                    );
                return false;
        }

        FSG_OperationState_Status fsg_operationstate_status = (FSG_OperationState_Status)this.fsgOperationStateFunction
            .getLastStatus();
        if (fsg_operationstate_status.op_State != b0) {
            fsg_operationstate_status.op_State = b0;
            return true;
        } else {
            return false;
        }
    }

    public boolean isOpStateNormalOperation() {
        FSG_OperationState_Status fsg_operationstate_status = (FSG_OperationState_Status)this.fsgOperationStateFunction
            .getLastStatus();
        return fsg_operationstate_status.op_State == 0;
    }
}
