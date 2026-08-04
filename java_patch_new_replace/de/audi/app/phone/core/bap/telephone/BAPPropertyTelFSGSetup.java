package de.audi.app.phone.core.bap.telephone;

import de.audi.app.phone.core.ITelApplication;
import de.audi.app.phone.core.PhoneServiceProvider;
import de.audi.app.phone.core.bap.AbstractTel1EnqueuedBAPPropertyHandler;
import de.audi.app.phone.core.dsi.ITelDSIMobileEquipmentDeviceState;
import de.audi.app.phone.core.state.IGlobalTelephoneStateStruct;
import de.audi.atip.base.IFrameworkAccess;
import de.audi.atip.interapp.combi.bap.phone.CombiBAPServicePhone;
import de.audi.atip.interapp.combi.bap.phone.data.FsgSetup;
import de.audi.atip.interapp.phone.IEcallState;
import de.audi.atip.interapp.terminalmode.DefaultTerminalModeUpdateListener;
import de.audi.atip.interapp.terminalmode.TerminalModeDevice;
import de.esolutions.fw.util.commons.job.DispatcherBase;

class BAPPropertyTelFSGSetup extends AbstractTel1EnqueuedBAPPropertyHandler {
    private static final int E_SIM_CODED_NEVER = 1;
    private static final int E_SIM_CODED_ALWAYS = 2;
    private static final int E_SIM_CODED_SERVICE_DEPENDENT = 0;
    private final boolean cableConnectionToMobilePossible = false;
    protected volatile FsgSetup fsgSetup;
    private final boolean nadCoded;
    private final boolean phoneModulOperationModeWithVoice;
    private final boolean simCardModeSwitch;
    private final boolean bluetoothCoded;
    private final boolean btTelephoneSupportCoded;
    private final IFrameworkAccess frameworkAccess;
    private final PhoneServiceProvider terminalModeUpdateListnerService;
    private final DefaultTerminalModeUpdateListener tmUpdateListener;
    private final boolean isCarplayPossible;
    private volatile boolean isAndroidAutoPossible;
    private final int eSIMCoding;
    private volatile boolean isCarplayActive = false;
    private volatile boolean isAndroidAutoActive = false;
    private static int getClusterMobileConnectionType(int i) {
        switch (i) {
            case 0:
                return 1;
            case 1:
            case 2:
                return 4;
            case 3:
                return 3;
            default:
                return 0;
        }
    }

    BAPPropertyTelFSGSetup(ITelApplication itelapplication, DispatcherBase dispatcherbase) {
        super(itelapplication, dispatcherbase);
        this.frameworkAccess = itelapplication.getFrameworkAccess();
        this.nadCoded = this.frameworkAccess.getSysConstManager().getSysConst(463) == 1;
        this.phoneModulOperationModeWithVoice = this.frameworkAccess
            .getSysApp()
            .getAdaptationANP()
            .isPhoneModuleOperationModeWithVoice();
        this.simCardModeSwitch = this.frameworkAccess.getSysApp().getAdaptationANP().isSimCardModeSwitch();
        this.bluetoothCoded = this.frameworkAccess.getSysApp().getDiagnosisCOD().isBluetoothAvailable();
        this.btTelephoneSupportCoded = this.frameworkAccess.getSysApp().getDiagnosisCOD().isBluetoothPhoneAvailable();
        this.isCarplayPossible = this.frameworkAccess.getSysApp().getAdaptationANP().isAppleDIO();
        this.isAndroidAutoPossible = this.frameworkAccess.getSysApp().getAdaptationANP().isGoogleGAL();
        this.tmUpdateListener = new BAPPropertyTelFSGSetup.BAPTerminalModeUpdateListener(this);
        this.terminalModeUpdateListnerService = new PhoneServiceProvider(
            (de.audi.atip.interapp.terminalmode.ITerminalModeUpdateListener.class)
                .getName(),
            this.tmUpdateListener,
            null,
            itelapplication.getBundleContext(),
            this.log
        );
        this.eSIMCoding = this.frameworkAccess.getSysConstManager().getAdaptationANP().getESIMUUsage();
    }

    BAPPropertyTelFSGSetup(
        ITelApplication itelapplication,
        DispatcherBase dispatcherbase,
        boolean flag,
        boolean flag1,
        boolean flag2,
        boolean flag3,
        boolean flag4,
        boolean flag5,
        boolean flag6,
        int i
    ) {
        super(itelapplication, dispatcherbase);
        this.frameworkAccess = itelapplication.getFrameworkAccess();
        this.nadCoded = flag;
        this.phoneModulOperationModeWithVoice = flag1;
        this.simCardModeSwitch = flag2;
        this.bluetoothCoded = flag3;
        this.btTelephoneSupportCoded = flag4;
        this.tmUpdateListener = new BAPPropertyTelFSGSetup.BAPTerminalModeUpdateListener(this);
        this.terminalModeUpdateListnerService = new PhoneServiceProvider(
            (de.audi.atip.interapp.terminalmode.ITerminalModeUpdateListener.class)
                .getName(),
            this.tmUpdateListener,
            null,
            itelapplication.getBundleContext(),
            this.log
        );
        this.isCarplayPossible = flag5;
        this.isAndroidAutoPossible = flag6;
        this.eSIMCoding = i;
    }

    public void init() {
        this.terminalModeUpdateListnerService.startService();
        super.init();
    }

    public void deinit() {
        this.terminalModeUpdateListnerService.stopService();
        super.deinit();
    }

    boolean isInternalSimCardReaderAvailable(int i, boolean flag) {
        return this.nadCoded ? this.nadCodedCheckESIMConditions(i, flag) : false;
    }

    private boolean nadCodedCheckESIMConditions(int i, boolean flag) {
        switch (this.eSIMCoding) {
            case 0:
                return this.nadCodedESIMServiceDependentCheckESIMActive(i, flag);
            case 1:
                return this.nadCodedESIMNotCodedCheckFullConnectConditions(i);
            case 2:
                return false;
            default:
                this.log
                    .log(
                        10000,
                        "[BAPTerminalModeUpdateListener#isInternalSimCardReaderAvailable] invalid eSIMCoding=%1",
                        this.eSIMCoding
                    );
                return this.nadCodedESIMNotCodedCheckFullConnectConditions(i);
        }
    }

    private boolean nadCodedESIMServiceDependentCheckESIMActive(int i, boolean flag) {
        return flag && this.simCardModeSwitch ? true : this.nadCodedESIMNotCodedCheckFullConnectConditions(i);
    }

    private boolean nadCodedESIMNotCodedCheckFullConnectConditions(int i) {
        return !this.simCardModeSwitch ? this.phoneModulOperationModeWithVoice : i == 1 || i == 3;
    }

    boolean isHfpConnectionAvailable() {
        return this.bluetoothCoded && this.btTelephoneSupportCoded;
    }

    boolean isRsapConnectionToMobilePossible(int i, boolean flag) {
        return this.isInternalSimCardReaderAvailable(i, flag) && this.bluetoothCoded;
    }

    private int getMobileConnectionType() {
        if (this.isCarplayActive) {
            return 5;
        } else if (this.isAndroidAutoActive) {
            return 6;
        } else if (this.isLowPrioritySOSEmergencyCallType()) {
            return 3;
        } else if (this.isConnectionPresent()) {
            IGlobalTelephoneStateStruct iglobaltelephonestatestruct = this.getTelephoneState();
            ITelDSIMobileEquipmentDeviceState iteldsimobileequipmentdevicestate = iglobaltelephonestatestruct != null
                ? iglobaltelephonestatestruct.getCallLeadingDevice()
                : null;
            return getClusterMobileConnectionType(iteldsimobileequipmentdevicestate.getTelMode());
        } else {
            return 0;
        }
    }

    private boolean isConnectionPresent() {
        IGlobalTelephoneStateStruct iglobaltelephonestatestruct = this.getTelephoneState();
        ITelDSIMobileEquipmentDeviceState iteldsimobileequipmentdevicestate = iglobaltelephonestatestruct != null
            ? iglobaltelephonestatestruct.getCallLeadingDevice()
            : null;
        if (iteldsimobileequipmentdevicestate != null && iteldsimobileequipmentdevicestate.getActivationState() != null
            )
         {
            int i = iteldsimobileequipmentdevicestate.getActivationState().getTelActivationState();
            return i == 5 || i == 2;
        } else {
            return false;
        }
    }

    private boolean isLowPrioritySOSEmergencyCallType() {
        IGlobalTelephoneStateStruct iglobaltelephonestatestruct = this.getTelephoneState();
        IEcallState iecallstate = iglobaltelephonestatestruct != null
            ? iglobaltelephonestatestruct.getConnectedGatewayState()
            : null;
        return iecallstate != null && iecallstate.isLowPrioritySOSEmergencyCallType();
    }

    protected boolean doProcessGlobalTelephoneStateUpdate(
        int i, IGlobalTelephoneStateStruct iglobaltelephonestatestruct
    ) {
        return true;
    }

    protected void updateAsync() {
        CombiBAPServicePhone combibapservicephone = this.getCombiService();
        IGlobalTelephoneStateStruct iglobaltelephonestatestruct = this.getTelephoneState();
        int i = this.getMobileConnectionType();
        /* EXPERIMENTAL cluster telephone-tab gate: mobileConnectionType==3 (HANDS_FREE_PROFILE) means a
         * BT/HFP phone is connected+active and NOT CarPlay(5)/AndroidAuto(6).  Publish that as the
         * tab-visible condition; PhoneTabGate drives the PHONE2 FSG op-state accordingly. */
        com.luka.carplay.phone.PhoneTabGate.setTabVisible(i == 3);
        int j = iglobaltelephonestatestruct != null ? iglobaltelephonestatestruct.getNadMode() : 0;
        boolean flag = iglobaltelephonestatestruct != null ? iglobaltelephonestatestruct.isESIMActive() : false;
        boolean flag1 = this.isInternalSimCardReaderAvailable(j, flag);
        boolean flag2 = this.isHfpConnectionAvailable();
        boolean flag3 = this.isRsapConnectionToMobilePossible(j, flag);
        FsgSetup.Builder fsgsetup$builder = FsgSetup.builder();
        fsgsetup$builder.setInternalSimCardReaderPresent(flag1);
        fsgsetup$builder.setCableConnectionPossible(false);
        fsgsetup$builder.setHfpConnectionPossible(flag2);
        fsgsetup$builder.setRsapConnectionPossible(flag3);
        fsgsetup$builder.setAppleLinkConnectionPossible(this.isCarplayPossible);
        fsgsetup$builder.setGoogleLinkConnectionPossible(this.isAndroidAutoPossible);
        fsgsetup$builder.setMobileConnectionType(i);
        FsgSetup fsgsetup = fsgsetup$builder.build();
        if (combibapservicephone != null) {
            if (!fsgsetup.equals(this.fsgSetup)) {
                this.log.log(1000000, "[BAPPropertyTelFSGSetup#update] %1", fsgsetup);
                combibapservicephone.updateFsgSetup(fsgsetup);
                this.fsgSetup = fsgsetup;
            }
        } else {
            this.log.log(100000, "[BAPPropertyTelFSGSetup#update] combi service is null! --> NOP!");
        }
    }

    private class BAPTerminalModeUpdateListener extends DefaultTerminalModeUpdateListener {
        private final BAPPropertyTelFSGSetup this$0;

        private BAPTerminalModeUpdateListener(BAPPropertyTelFSGSetup bappropertytelfsgsetup) {
            this.this$0 = bappropertytelfsgsetup;
        }

        public void updateActiveDeviceState(TerminalModeDevice terminalmodedevice) {
            if (terminalmodedevice == null) {
                this.this$0
                    .log
                    .log(10000, "BAPTerminalModeUpdateListener#updateActiveDeviceState pActiveDevice is null");
            } else {
                boolean flag = this.this$0.isCarplayActive;
                boolean flag1 = this.this$0.isAndroidAutoActive;
                this.this$0.isCarplayActive = terminalmodedevice.isActive()
                    && terminalmodedevice.getConnectionMethod() == 1;
                this.this$0.isAndroidAutoActive = terminalmodedevice.isActive()
                    && terminalmodedevice.getConnectionMethod() == 2;
                if (flag != this.this$0.isCarplayActive) {
                    this.this$0
                        .log
                        .log(
                            10000000,
                            "[BAPTerminalModeUpdateListener#updateActiveDeviceState] changed. sending update to combi. isCarplayActive=%1",
                            this.this$0.isCarplayActive
                        );
                    this.this$0.enqueueUpdate();
                }

                if (flag1 != this.this$0.isAndroidAutoActive) {
                    this.this$0
                        .log
                        .log(
                            10000000,
                            "[BAPTerminalModeUpdateListener#updateActiveDeviceState] changed. sending update to combi. isAndroidAutoActive=%1",
                            this.this$0.isAndroidAutoActive
                        );
                    this.this$0.enqueueUpdate();
                }
            }
        }
    }
}
