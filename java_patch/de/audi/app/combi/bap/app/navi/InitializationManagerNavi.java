package de.audi.app.combi.bap.app.navi;

import de.audi.app.bap.IPowerState;
import de.audi.app.bap.dsi.bap.IDSIBAPController;
import de.audi.app.bap.fw.AbstractBAPModuleInitializationManagerFSG;
import de.audi.app.bap.fw.functiontypes.BAPFunctionPropertyFSG;
import de.audi.app.combi.bap.fw.AbstractCombiModule;
import de.audi.atip.base.AppNameConst;
import de.esolutions.fw.util.commons.Buffer;
import de.vw.mib.bap.generated.navsd.serializer.FSG_OperationState_Status;

/* Patched stock class (MU1316).  While the CarPlay cluster map owns the Virtual
 * Cockpit, NavSD FSG_OperationState INITIALIZING is sent as NORMAL.
 *
 * The VC turns OP_State 3 into its Nav NA screen ("Initialising navigation
 * system. Please wait..."), which replaces the whole map view, CarPlay plane
 * included.  Stock sends INITIALIZING not only through show/hideInitializingScreen
 * (appIsReady) but also while the "Navi" app has not started, during the BAP init
 * sequence, a software download or a BAP stack drop
 * (AbstractBAPModuleInitializationManagerFSG.getGeneralOperationState), so the
 * clamp sits here, where every input converges.  No cache is needed: on release
 * updateOperationState() recomputes the real value and, because it differs, resends
 * it.  See docs/cluster-and-rgi/NAV_INITIALIZING_SCREEN.md. */
public class InitializationManagerNavi extends AbstractBAPModuleInitializationManagerFSG {
    /* Internal operation states (getGeneralOperationState). */
    private static final int OP_INTERNAL_NORMAL = 0;
    private static final int OP_INTERNAL_INITIALIZING = 2;

    private static volatile InitializationManagerNavi instance;
    private static volatile boolean carPlayClusterOwner;

    public InitializationManagerNavi(
        AbstractCombiModule abstractcombimodule,
        BAPFunctionPropertyFSG bapfunctionpropertyfsg,
        IDSIBAPController idsibapcontroller,
        IPowerState ipowerstate
    ) {
        super(abstractcombimodule, bapfunctionpropertyfsg, idsibapcontroller, ipowerstate);
        instance = this;
    }

    /** CarPlay cluster ownership edge (AltScreenNavStatusGate.setAltScreenActive). */
    public static void setCarPlayClusterOwner(boolean owner) {
        if (carPlayClusterOwner == owner) return;
        carPlayClusterOwner = owner;
        InitializationManagerNavi m = instance;
        if (m != null) m.updateOperationState();
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
        /* With the BAP stack down nothing reaches the VC anyway; keep stock there. */
        if (carPlayClusterOwner && i == OP_INTERNAL_INITIALIZING && this.getBapStackState() != 0) {
            i = OP_INTERNAL_NORMAL;
        }
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
                        "[CombiModuleNavi.InitializationManagerNavi#setFSGOperationStateValue] invalid opState: %1",
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
