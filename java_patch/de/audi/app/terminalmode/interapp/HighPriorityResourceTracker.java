package de.audi.app.terminalmode.interapp;

import com.luka.carplay.pdc.PdcSmallStageGuard;
import com.luka.carplay.framework.Log;
import de.audi.app.terminalmode.CommandListHelper;
import de.audi.app.terminalmode.IContext;
import de.audi.app.terminalmode.ITerminalModeComponent;
import de.audi.app.terminalmode.device.TMDevice;
import de.audi.app.terminalmode.diagnosis.IDiagnosisCommandProvider;
import de.audi.app.terminalmode.diagnosis.IDiagnosisManager;
import de.audi.app.terminalmode.osgi.IServiceManager;
import de.audi.app.terminalmode.smartphone.IDSISmartphoneManager;
import de.audi.app.terminalmode.statemachine.IRequestor;
import de.audi.app.terminalmode.statemachine.IStateHandler;
import de.audi.app.terminalmode.statemachine.Resource;
import de.audi.app.terminalmode.statemachine.TMState;
import de.audi.app.terminalmode.statemachine.commands.AbstractCommand;
import de.audi.atip.hmi.modelaccess.ChoiceModelApp;
import de.audi.atip.interapp.bap.ecall.data.EmergencyNumber;
import de.audi.atip.interapp.bap.ecall.data.PhoneCall;
import de.audi.atip.interapp.phone.IEcallState;
import de.audi.atip.interapp.phone.ITelEcallStateListener;
import de.audi.atip.log.LogChannel;
import de.audi.atip.model.ICoreTerminalModeModelBank;
import de.audi.atip.msg.MsgListener;
import de.audi.atip.power.DefaultPowerEventListener;
import de.audi.atip.utils.generics.Consumer;
import de.audi.atip.utils.reactive.observables.Observables;
import de.audi.atip.utils.reactive.properties.Property;
import de.audi.atip.utils.reactive.properties.PropertyFactory;
import de.audi.tghu.command.CommandList;
import de.esolutions.fw.util.commons.Buffer;

/** Stock MU1316 resource policy, with one standalone-OPS exception for CarPlay. */
public class HighPriorityResourceTracker
    extends DefaultPowerEventListener
    implements ITerminalModeComponent,
    ITelEcallStateListener,
    MsgListener {
    private final Property eCallActive;
    private final Property clampSOff;
    private final Property rvcActive;
    private final IServiceManager serviceManager;
    private final CommandListHelper commandListHelper;
    private final IContext context;
    private final LogChannel lc;
    private final IDiagnosisManager diagnosisManager;
    private final IDiagnosisCommandProvider diagnosisCommandProvider;
    private final IStateHandler stateHandler;
    private final IDSISmartphoneManager.ISmartphoneProperties smartphoneProperties;
    public static final int SCREEN_NOT_BLOCKED = 0;
    public static final int SCREEN_BLOCKED_CARPLAY_BY_ECALL = 1;
    public static final int SCREEN_BLOCKED_ANDROIDAUTO_BY_ECALL = 2;
    public static final int SCREEN_BLOCKED_CARPLAY_BY_RVC = 3;
    public static final int SCREEN_BLOCKED_ANDROIDAUTO_BY_RVC = 4;
    public static final int SCREEN_BLOCKED_CARLIFE_BY_ECALL = 5;
    public static final int SCREEN_BLOCKED_CARLIFE_BY_RVC = 6;

    public HighPriorityResourceTracker(
        IServiceManager iservicemanager,
        LogChannel logchannel,
        CommandListHelper commandlisthelper,
        IContext icontext,
        IDiagnosisManager idiagnosismanager,
        IStateHandler istatehandler,
        PropertyFactory propertyfactory,
        IDSISmartphoneManager.ISmartphoneProperties idsismartphonemanager$ismartphoneproperties
    ) {
        this.serviceManager = iservicemanager;
        this.commandListHelper = commandlisthelper;
        this.lc = logchannel;
        this.context = icontext;
        this.diagnosisManager = idiagnosismanager;
        this.stateHandler = istatehandler;
        this.smartphoneProperties = idsismartphonemanager$ismartphoneproperties;
        this.eCallActive = propertyfactory.createProperty("eCallActive", new Boolean(false));
        this.clampSOff = propertyfactory.createProperty("clampSOff", new Boolean(false));
        this.rvcActive = propertyfactory.createProperty("rvcActive", new Boolean(false));
        this.diagnosisCommandProvider = new IDiagnosisCommandProvider() {
            public String[] getDiagKeys() {
                return new String[]{
                    "HighPriorityResourceTracker enable RVC",
                    "HighPriorityResourceTracker disable RVC",
                    "HighPriorityResourceTracker enable eCall",
                    "HighPriorityResourceTracker disable eCall"
                };
            }

            public void executeDiagCommand(String s, String[] astring) {
                if ("HighPriorityResourceTracker enable RVC".equals(s)) {
                    HighPriorityResourceTracker.this.parkingActivated();
                } else if ("HighPriorityResourceTracker disable RVC".equals(s)) {
                    HighPriorityResourceTracker.this.processMsg(107);
                } else if ("HighPriorityResourceTracker enable eCall".equals(s)) {
                    HighPriorityResourceTracker.this.updateEcallState(0, new DiagECallState(true));
                } else if ("HighPriorityResourceTracker disable eCall".equals(s)) {
                    HighPriorityResourceTracker.this.updateEcallState(0, new DiagECallState(false));
                }
            }
        };
    }

    public void init() {
        this.serviceManager.registerService(ITelEcallStateListener.class, this, IServiceManager.EMPTY_PARAMETERS);
        this.serviceManager.registerService(de.audi.atip.power.PowerEventListener.class, this, IServiceManager.EMPTY_PARAMETERS);
        this.serviceManager.registerService(MsgListener.class, this, IServiceManager.EMPTY_PARAMETERS);
        this.diagnosisManager.addCommandProvider(0, this.diagnosisCommandProvider);
        this.context
            .getDeviceManager()
            .getProperties()
            .activeDevice()
            .subscribe(
                new Consumer() {
                    public void accept(Object value) {
                        TMDevice tmdevice = (TMDevice)value;
                        if (((Boolean)HighPriorityResourceTracker.this.eCallActive.get()).booleanValue()) {
                            HighPriorityResourceTracker.this.lc.log(1000000, "[HighPriorityResourceTracker] ECall Lockscreenchanges");
                            HighPriorityResourceTracker.this
                                .context
                                .getChoiceModel(ICoreTerminalModeModelBank.SMARTPHONE_SCREEN_BLOCKED_CHOICE)
                                .setValue(tmdevice.isCarplayDevice() ? 1 : (tmdevice.isAndroidAutoDevice() ? 2 : 5));
                        } else if (((Boolean)HighPriorityResourceTracker.this.rvcActive.get()).booleanValue()) {
                            HighPriorityResourceTracker.this.lc.log(1000000, "[HighPriorityResourceTracker] RVC Lockscreenchanges");
                            HighPriorityResourceTracker.this
                                .context
                                .getChoiceModel(ICoreTerminalModeModelBank.SMARTPHONE_SCREEN_BLOCKED_CHOICE)
                                .setValue(tmdevice.isCarplayDevice() ? 3 : (tmdevice.isAndroidAutoDevice() ? 4 : 6));
                        }
                    }
                }
            );
        Observables.combineLatest(this.eCallActive, this.clampSOff, this.rvcActive)
            .using(
                new Observables.Combinator3() {
                    public Object combine(Object a, Object b, Object c) {
                        boolean obool = ((Boolean)a).booleanValue();
                        boolean obool1 = ((Boolean)b).booleanValue();
                        boolean obool2 = ((Boolean)c).booleanValue();
                        boolean flag = obool || obool1 || obool2;
                        String s = !flag
                            ? "not blocked"
                            : new Buffer()
                                .append(obool ? "eCallActive" : "")
                                .append(obool1 ? "clampSOff" : "")
                                .append(obool2 ? "rvcActive" : "")
                                .toString();
                        HighPriorityResourceTracker.this.lc.log(1000000, "[HighPriorityResourceTracker.combine] %1 ", s);
                        return new Boolean(flag);
                    }
                }
            )
            .distinctUntilChanged()
            .redirectTo(new Consumer() {
                public void accept(Object value) {
                    boolean obool = ((Boolean)value).booleanValue();
                    if (obool) {
                        HighPriorityResourceTracker.this.acquireHighPriorityResources(Resource.SCREEN);
                    } else {
                        HighPriorityResourceTracker.this.releaseHighPriorityResources(Resource.SCREEN);
                    }
                }
            });
    }

    public void deinit() {
    }

    public void updateEcallState(int i, IEcallState iecallstate) {
        this.eCallActive.accept(new Boolean(iecallstate.hasActiveCall()));
        if (iecallstate.hasActiveCall()) {
            this.acquireHighPriorityResources(Resource.AUDIO_MEDIA);
            this.context
                .getChoiceModel(ICoreTerminalModeModelBank.SMARTPHONE_SCREEN_BLOCKED_CHOICE)
                .setValue(this.context.getDeviceManager().getActiveDevice().isCarplayDevice() ? 1 : 2);
        } else {
            this.releaseHighPriorityResources(Resource.AUDIO_MEDIA);
            this.context.getChoiceModel(ICoreTerminalModeModelBank.SMARTPHONE_SCREEN_BLOCKED_CHOICE).setValue(0);
        }
    }

    public void processMsg(int i) {
        if (i == 108) {
            if (this.context.getDeviceManager().getActiveDevice().isCarplayDevice()
                && PdcSmallStageGuard.shouldKeepCarPlayScreen()) {
                /* 108 means parking active, not necessarily video camera. The
                 * controller has classified the NEW component list already.
                 * Never toggle Main Wizard or acquire SCREEN for standalone OPS. */
                if (((Boolean)this.rvcActive.get()).booleanValue()) {
                    this.rvcActive.accept(Boolean.FALSE);
                    this.smartphoneProperties.getPropertyMURVCActive().accept(Boolean.FALSE);
                    this.context.getChoiceModel(ICoreTerminalModeModelBank.SMARTPHONE_SCREEN_BLOCKED_CHOICE)
                        .setValue(((Boolean)this.eCallActive.get()).booleanValue() ? 1 : 0);
                }
                Log.i("PDC", "OPS 108: keep CarPlay SCREEN; no Main Wizard takeover");
                return;
            }
            Log.i("PDC", "parking 108: stock SCREEN/Main Wizard takeover");
            parkingActivated();
        } else if (i == 107) {
            /* Old components can send 107 while the controller is switching
             * to new pure OPS. Do not erase the already published target. */
            this.rvcActive.accept(new Boolean(false));
            this.context.getChoiceModel(ICoreTerminalModeModelBank.SMARTPHONE_SCREEN_BLOCKED_CHOICE).setValue(0);
            this.smartphoneProperties.getPropertyMURVCActive().accept(new Boolean(false));
        }
    }

    private void parkingActivated() {
        PdcSmallStageGuard.parkingStopped();
        if (this.context.getDeviceManager().getActiveDevice().isCarplayDevice()) {
            ChoiceModelApp choicemodelapp = this.context
                .getChoiceModel(ICoreTerminalModeModelBank.SWITCH_TO_MAIN_WIZARD_CHOICE);
            choicemodelapp.setValue(choicemodelapp.getValue() == 0 ? 1 : 0);
            this.context
                .getChoiceModel(ICoreTerminalModeModelBank.SMARTPHONE_SCREEN_BLOCKED_CHOICE)
                .setValue(this.context.getDeviceManager().getActiveDevice().isCarplayDevice() ? 3 : 4);
            this.rvcActive.accept(new Boolean(true));
        } else {
            this.context
                .getChoiceModel(ICoreTerminalModeModelBank.SMARTPHONE_SCREEN_BLOCKED_CHOICE)
                .setValue(this.context.getDeviceManager().getActiveDevice().isCarplayDevice() ? 3 : 4);
            this.rvcActive.accept(new Boolean(true));
        }

        this.smartphoneProperties.getPropertyMURVCActive().accept(new Boolean(true));
    }

    public void notifyPowerListenerOnEnterState(int i, int j) {
        if (i == 0) {
            this.clampSOff.accept(new Boolean(false));
        }
    }

    public void notifyPowerListenerOnExitState(int i, int j) {
        if (i == 0) {
            this.clampSOff.accept(new Boolean(true));
        }
    }

    private void acquireHighPriorityResources(final Resource resource) {
        this.commandListHelper
            .create()
            .addSingle(
                new AbstractCommand(
                    this.lc, "HighPriorityResourceTracker$Acquire " + resource, this.context
                ) {
                    public void execute() {
                        TMState tmstate = HighPriorityResourceTracker.this.stateHandler.getCurrentState();
                        tmstate.restrictAccessTo(resource);
                        CommandList commandlist = HighPriorityResourceTracker.this
                            .stateHandler
                            .changeState(tmstate, IRequestor.MAINUNIT, -1L);
                        this.getCommandList().commandFinishedWithPostSequence(commandlist);
                    }
                }
            )
            .execute("HighPriorityResourceTracker.acquireHighPriorityResources");
    }

    private void releaseHighPriorityResources(final Resource resource) {
        this.commandListHelper
            .create()
            .addSingle(
                new AbstractCommand(
                    this.lc, "HighPriorityResourceTracker$Release " + resource, this.context
                ) {
                    public void execute() {
                        TMState tmstate = HighPriorityResourceTracker.this.stateHandler.getCurrentState();
                        tmstate.allowAccessTo(resource);
                        CommandList commandlist = HighPriorityResourceTracker.this
                            .stateHandler
                            .changeState(tmstate, IRequestor.MAINUNIT, -1L);
                        this.getCommandList().commandFinishedWithPostSequence(commandlist);
                    }
                }
            )
            .execute("HighPriorityResourceTracker.releaseHiPrioResources");
    }

    private class DiagECallState implements IEcallState {
        private final boolean serviceActive;

        public DiagECallState(boolean flag) {
            this.serviceActive = flag;
        }

        public boolean hasActiveCall() {
            return this.serviceActive;
        }

        public boolean isServiceActive() {
            return this.serviceActive;
        }

        public int getServiceKind() {
            return 0;
        }

        public PhoneCall getCall() {
            return null;
        }

        public boolean isEmergencyCallType() {
            return false;
        }

        public boolean isCustomerCallNotAllowed() {
            return false;
        }

        public boolean isCustomerCallAllowed() {
            return false;
        }

        public boolean isLowPrioritySOSEmergencyCallType() {
            return false;
        }

        public EmergencyNumber[] getAllowedEmergencyNumbers() {
            return new EmergencyNumber[0];
        }

        public String getEmergencyNumberToBeDialed() {
            return null;
        }
    }
}
