package de.audi.app.combi.bap.app.navi;

import de.audi.app.bap.IPowerState;
import de.audi.app.bap.dsi.bap.IDSIBAPController;
import de.audi.app.bap.fw.AbstractBAPModuleInitializationManagerFSG;
import de.audi.app.bap.fw.functiontypes.BAPFunctionPropertyFSG;
import de.audi.app.combi.bap.fw.AbstractCombiModule;
import de.audi.atip.base.AppNameConst;
import de.esolutions.fw.util.commons.Buffer;
import de.vw.mib.bap.generated.navsd.serializer.FSG_OperationState_Status;

/** Stock NAVSD manager with a presentation-only INITIALIZING override.
 * The real app/init/MOST states remain stock. Only an applied CarPlay context
 * with a posted renderer surface may clear the VC's global waiting overlay. */
public class InitializationManagerNavi extends AbstractBAPModuleInitializationManagerFSG {
    private static volatile InitializationManagerNavi instance;
    private volatile boolean carPlayPresentationOverride;

    /** Call only after releasing the AltScreen context/state locks. Re-read the
     * current presentation rather than applying a possibly stale boolean edge. */
    public static void refreshCarPlayPresentation() {
        InitializationManagerNavi manager = instance;
        if (manager != null) manager.updateOperationState();
    }

    public InitializationManagerNavi(
        AbstractCombiModule abstractcombimodule,
        BAPFunctionPropertyFSG bapfunctionpropertyfsg,
        IDSIBAPController idsibapcontroller,
        IPowerState ipowerstate
    ) {
        super(abstractcombimodule, bapfunctionpropertyfsg, idsibapcontroller, ipowerstate);
        instance = this;
    }

    public void appStateChanged(String s, int i) {
        this.logChannel
            .log(100000000, "[CombiModuleNavi.InitializationManagerNavi#appStateChanged] appName=%1, value=%2", s, i);
        if ("Navi".equals(s) || AppNameConst.APP_NAME_NAV_ASIA.equals(s)) {
            this.processAppStateChanged(i);
        }
    }

    public String appStatesToString() {
        Buffer buffer = new Buffer();
        buffer.append("appStateNavi = ");
        buffer.append(this.getAppState());
        buffer.append('\n');
        return buffer.toString();
    }

    public boolean setFSGOperationStateValue(int i) {
        boolean override = i == 2 && this.getBapStackState() != 0
            && this.powerState.isPowerOn() && !this.isSwdlRunning()
            && com.luka.carplay.core.AltScreenModule.isClusterPresented();
        byte b0;
        switch (i) {
            case 0:
                b0 = 0;
                break;
            case 1:
                b0 = 1;
                break;
            case 2:
                b0 = override ? (byte)0 : (byte)3;
                break;
            case 3:
                b0 = 15;
                break;
            default:
                this.logChannel
                    .log(
                        10000,
                        "[CombiModuleNavi.InitializationManagerNavi#setFSGOperationStateValue] invalid opState: %1",
                        i
                    );
                return false;
        }

        FSG_OperationState_Status fsg_operationstate_status = (FSG_OperationState_Status)this.fsgOperationStateFunction
            .getLastStatus();
        carPlayPresentationOverride = override;
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
        /* The BAP init engine must not mistake the presentation override for
         * native Navigation being initialized. Preserve its original gate. */
        return !carPlayPresentationOverride && fsg_operationstate_status.op_State == 0;
    }
}
