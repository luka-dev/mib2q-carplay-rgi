/* Full Java 1.4 replacement of MU1316 TMDeviceControl.
 * Stock state hierarchy, transition messages and timeouts are retained.
 * Only updateMainWizardModel delegates HMI identity to HuMenuIdentity.
 * States use explicit enclosing-instance constructors (one parent argument),
 * as required by InnerClassInflator; do not add an implicit second parent.
 */
package de.audi.app.terminalmode.device;

import de.audi.app.terminalmode.ConvenientCommandList;
import de.audi.app.terminalmode.IContext;
import de.audi.app.terminalmode.ITMDeviceSubsystem;
import de.audi.app.terminalmode.SmartphoneManager;
import de.audi.app.terminalmode.audio.IAudioManager;
import de.audi.app.terminalmode.audio.TMAudioConnection;
import de.audi.app.terminalmode.dsi.AbstractDispatcherRunnable;
import de.audi.app.terminalmode.dsi.smartphoneintegration.ISmartphoneIntegrationDSIController;
import de.audi.app.terminalmode.dsi.smartphoneintegration.SmartphoneIntegrationDSILifecycleController;
import de.audi.app.terminalmode.events.IEventBus;
import de.audi.app.terminalmode.events.TrackDataChangedEvent;
import de.audi.app.terminalmode.events.TrackPlayPositionEvent;
import de.audi.app.terminalmode.smartphone.IDSISmartphoneManager;
import de.audi.app.terminalmode.smartphone.SPIServiceState;
import de.audi.app.terminalmode.statemachine.Application;
import de.audi.app.terminalmode.statemachine.ApplicationOwner;
import de.audi.app.terminalmode.statemachine.IStateHandler;
import de.audi.app.terminalmode.statemachine.commands.AbstractCommand;
import de.audi.app.terminalmode.statemachine.commands.StopService;
import de.audi.atip.log.LogChannel;
import de.audi.atip.model.ICoreSystemModelBank;
import de.audi.atip.utils.dispatching.IDispatcher;
import de.audi.atip.utils.generics.Consumer;
import de.audi.atip.utils.reactive.observables.Subscription;
import de.audi.atip.utils.statemachine.Handler;
import de.audi.atip.utils.statemachine.Message;
import de.audi.atip.utils.statemachine.State;
import de.audi.atip.utils.statemachine.StateMachine;
import de.audi.atip.utils.statemachine.StateMachineInflator;
import de.audi.atip.utils.statemachine.SyncPoint;
import de.esolutions.fw.util.commons.timeout.ITimeSource;
import org.dsi.ifc.smartphoneintegration.Device;

public class TMDeviceControl {
    public static final int ACTIVE_DEVICE_NONE = 0;
    public static final int ACTIVE_DEVICE_CARPLAY = 1;
    public static final int ACTIVE_DEVICE_ANDRIODAUTO = 2;
    public static final int ACTIVE_DEVICE_CARLIFE = 3;
    private static final int ACTIVE_DEVICE_MAINWIZARD_NONE = 18;
    private static final int ACTIVE_DEVICE_MAINWIZARD_CARPLAY = 16;
    private static final int ACTIVE_DEVICE_MAINWIZARD_ANDROIDAUTO = 17;
    private static final int ACTIVE_DEVICE_MAINWIZARD_CARLIFE = 19;
    public static final long DETACHING_TIMEOUT = 5000L;
    public static final long DETACHING_TIMEOUT_OLD_CONNECTOR = 10000L;
    public static final long ACTIVATING_SUBSYSTEM_TIMEOUT = 10000L;
    public static final long CONNECTING_TIMEOUT = 30000L;
    public static final int SUPRESS_AUTOMATIC_ACTIVATIONS_TIMOUT = 10000;
    public static final int MAXIMAL_RETRIES_AFTER_CONNECTIONSTATE_FAILED_DEVICE_LOCKED = 2;
    public static final int DEVICE_DISCOVERED_DEBOUNE_TIME_FOR_OLD_MEDIA_CONNETOR = 2000;
    private final TMDevice tmDevice;
    private final TMDeviceControl.IDeviceControlToDeviceManager deviceControlToDeviceManager;
    private final IContext context;
    private final LogChannel lc;
    private String logTag;
    private final StateMachine sm;
    private final IDispatcher dispatcher;
    private final IStateHandler stateHandler;
    private final IEventBus eventBus;
    private final IDSISmartphoneManager dsiManager;
    private final ITimeSource timeSource;
    private final IDeviceVariantsHandling deviceVariantsHandling;
    private final IDSISmartphoneManager.ISmartphoneProperties smartphoneProperties;
    private volatile PhysicalDevice lastDevice;
    public TMDeviceControl(
        IContext icontext,
        TMDevice tmdevice,
        TMDeviceControl.IDeviceControlToDeviceManager tmdevicecontrol$idevicecontroltodevicemanager,
        IStateHandler istatehandler,
        ITimeSource itimesource,
        IDeviceVariantsHandling idevicevariantshandling,
        IDSISmartphoneManager.ISmartphoneProperties idsismartphonemanager$ismartphoneproperties
    ) {
        this.tmDevice = tmdevice;
        this.deviceControlToDeviceManager = tmdevicecontrol$idevicecontroltodevicemanager;
        this.smartphoneProperties = idsismartphonemanager$ismartphoneproperties;
        this.context = icontext;
        this.lc = icontext.getLogger().main();
        this.logTag = "TMDeviceControl(" + tmdevice.getDevicename() + ")";
        this.stateHandler = istatehandler;
        this.dispatcher = (IDispatcher)icontext.get(
            de.audi.atip.utils.dispatching.IDispatcher.class
        );
        this.sm = new TMDeviceControl.SM(this);
        this.eventBus = icontext.getEventBus();
        this.dsiManager = icontext.getSmartphoneDSIManager();
        this.deviceVariantsHandling = idevicevariantshandling;
        this.lastDevice = new PhysicalDevice(new Device(), icontext.getSMIDSIController());
        this.sm.start();
        this.timeSource = itimesource;
    }

    public void activateDevice() {
        this.dispatcher.execute(new Runnable() {
            private final TMDeviceControl this$0;

            {
                this.this$0 = TMDeviceControl.this;
            }

            public void run() {
                this.this$0.deviceControlToDeviceManager.requestActivation(this.this$0);
            }
        });
    }

    public void activateOtherModeforDevice() {
        this.dispatcher.execute(new Runnable() {
            private final TMDeviceControl this$0;

            {
                this.this$0 = TMDeviceControl.this;
            }

            public void run() {
                this.this$0.deviceControlToDeviceManager.requestOtherModeActivation(this.this$0);
            }
        });
    }

    public void deactivateDevice() {
        this.dispatcher.execute(new AbstractDispatcherRunnable(this.logTag) {
            private final TMDeviceControl this$0;

            {
                this.this$0 = TMDeviceControl.this;
            }

            public void run() {
                this.this$0.deviceControlToDeviceManager.requestDeactivation(this.this$0);
            }
        });
    }

    public TMDeviceControl setUserAcceptState(TMDevice.UserAcceptState tmdevice$useracceptstate) {
        return this.setUserAcceptState(tmdevice$useracceptstate, true);
    }

    public TMDeviceControl setUserAcceptState(final TMDevice.UserAcceptState tmdevice$useracceptstate, boolean flag) {
        this.dispatcher
            .execute(
                new AbstractDispatcherRunnable(this.logTag) {
                    private final TMDevice.UserAcceptState val$userAcceptState;
                    private final TMDeviceControl this$0;

                    {
                        this.this$0 = TMDeviceControl.this;
                        this.val$userAcceptState = tmdevice$useracceptstate;
                    }

                    public void run() {
                        TMDevice.UserAcceptState tmdevice$useracceptstate1 = this.this$0.tmDevice.userAcceptState();
                        this.this$0
                            .lc
                            .log(
                                10000000,
                                "[%1.setUserAcceptState] %2 -> %3",
                                this.this$0.logTag,
                                tmdevice$useracceptstate1,
                                this.val$userAcceptState
                            );
                        this.this$0.tmDevice.setUserAcceptState(this.val$userAcceptState);
                        if (this.val$userAcceptState.is(TMDevice.UserAcceptState.DISCLAIMER_ACCEPTED)) {
                            this.this$0.tmDevice.setWasDisclaimerPreviouslyAccepted(true);
                        }

                        this.this$0
                            .deviceControlToDeviceManager
                            .notifyAboutChange(this.this$0.tmDevice, "setTMActivated");
                    }
                }
            );
        return this;
    }

    public TMDeviceControl setStoreUserAcceptState(final boolean flag) {
        this.dispatcher
            .execute(
                new AbstractDispatcherRunnable(this.logTag) {
                    private final boolean val$storeAcceptSelection;
                    private final TMDeviceControl this$0;

                    {
                        this.this$0 = TMDeviceControl.this;
                        this.val$storeAcceptSelection = flag;
                    }

                    public void run() {
                        this.this$0
                            .lc
                            .log(
                                1000000,
                                "[%1.setStoreUserAcceptState]  store=%2",
                                this.this$0.logTag,
                                String.valueOf(this.val$storeAcceptSelection)
                            );
                        this.this$0.tmDevice.setStoreUserAcceptState(this.val$storeAcceptSelection);
                    }
                }
            );
        return this;
    }

    public void deleteFromPersistence() {
        this.dispatcher.execute(new AbstractDispatcherRunnable(this.logTag) {
            private final TMDeviceControl this$0;

            {
                this.this$0 = TMDeviceControl.this;
            }

            public void run() {
                this.this$0.sm.sendMessage(14);
            }
        });
    }

    public TMDevice getTmDevice() {
        return this.tmDevice;
    }

    void deviceDiscovered(PhysicalDevice physicaldevice) {
        this.sm.sendMessage(1, physicaldevice);
    }

    void deviceIsNotDiscovered() {
        this.sm.sendMessage(4);
    }

    void setConnectionStateByDSIState(int i) {
        switch (i) {
            case 0:
                this.sm.sendMessage(8);
                break;
            case 1:
                this.sm.sendMessage(5);
                break;
            case 2:
                this.sm.sendMessage(6);
                break;
            case 3:
                this.sm.sendMessage(9);
                break;
            case 4:
            case 5:
            case 6:
            case 7:
            case 8:
            case 9:
            default:
                this.sm.sendMessage(7, i);
        }
    }

    void connectDeviceFailed(
        SmartphoneIntegrationDSILifecycleController.ErrorCode smartphoneintegrationdsilifecyclecontroller$errorcode
    ) {
        this.sm.sendMessage(13, smartphoneintegrationdsilifecyclecontroller$errorcode);
    }

    void activationConfirmed() {
        this.sm.sendMessage(2);
    }

    void deactivationConfirmed() {
        this.sm.sendMessage(3);
    }

    private void changeConnectionState(TMDevice.ConnectionState tmdevice$connectionstate, String s) {
        boolean flag = false;
        synchronized (this.tmDevice) {
            if (this.tmDevice.connectionState().isNot(tmdevice$connectionstate)) {
                this.lc
                    .log(
                        10000000,
                        "[%1.changeConnectionState] %2 -> %3, because %4",
                        this.logTag,
                        this.tmDevice.connectionState(),
                        tmdevice$connectionstate,
                        s
                    );
                this.tmDevice.setConnectionState(tmdevice$connectionstate);
                flag = true;
            }
        }

        if (flag) {
            this.deviceControlToDeviceManager.notifyAboutChange(this.tmDevice, s);
        }
    }

    void deviceDiscoveredPhoneCallActive(PhysicalDevice physicaldevice) {
        this.sm.sendMessage(17, physicaldevice);
    }

    void phoneCallEnded() {
        this.sm.sendMessage(18);
    }

    public String toString() {
        return this.logTag;
    }

    public interface IDeviceControlToDeviceManager {
        void notifyAboutChange(TMDevice tmdevice, String s);

        void requestActivation(TMDeviceControl tmdevicecontrol);

        void requestOtherModeActivation(TMDeviceControl tmdevicecontrol);

        void deleteMe(TMDeviceControl tmdevicecontrol);

        void requestDeactivation(TMDeviceControl tmdevicecontrol);
    }

    private static class SM extends StateMachine {
        private final ISmartphoneIntegrationDSIController dsiController;
        private final TMDeviceControl this$0;

        protected SM(TMDeviceControl tmdevicecontrol) {
            super(
                tmdevicecontrol.logTag,
                tmdevicecontrol.lc,
                (IDispatcher)tmdevicecontrol.context
                    .get(
                        de.audi.atip.utils.dispatching.IDispatcher.class
                    ),
                TMDeviceControl.TMDeviceControlState.getMessageCodeConverter()
            );
            this.this$0 = tmdevicecontrol;
            this.dsiController = tmdevicecontrol.context.getSMIDSIController();

            try {
                new StateMachineInflator(this).inflate();
            } catch (Exception exception) {
                exception.printStackTrace();
            }

            this.setInitialState(
                this.getStateForClass(
                    de.audi.app.terminalmode.device.TMDeviceControl.SM.BaseState.NotDiscoveredState.class
                )
            );
        }

        public static class BaseState extends TMDeviceControl.TMDeviceControlState {
            private static final String APPLE_INC = "Apple Inc.-iPhone";
            private final TMDeviceControl.SM this$1;

            public BaseState(TMDeviceControl.SM tmdevicecontrol$sm) {
                this.this$1 = tmdevicecontrol$sm;
            }

            protected void connectionStateStopped() {
            }

            protected void connectionStateDisconnected() {
            }

            private void updateDevice(PhysicalDevice physicaldevice) {
                if (!this.this$1.this$0.lastDevice.equals(physicaldevice)) {
                    this.this$1.this$0.tmDevice.setId(physicaldevice.getDevice().deviceID);
                    if (APPLE_INC.equals(physicaldevice.getDevice().getDeviceName())
                        && this.this$1.this$0.tmDevice.getDevicename().length() > 0) {
                        this.this$1
                            .this$0
                            .lc
                            .log(
                                10000000,
                                "[%1.deviceDiscovered] Device changed: %2 -> %3, retaining old name %4",
                                this.this$1.this$0.logTag,
                                this.this$1.this$0.lastDevice,
                                physicaldevice,
                                this.this$1.this$0.tmDevice.getDevicename()
                            );
                    } else {
                        this.this$1
                            .this$0
                            .lc
                            .log(
                                10000000,
                                "[%1.deviceDiscovered] Device changed: %2 -> %3",
                                this.this$1.this$0.logTag,
                                this.this$1.this$0.lastDevice,
                                physicaldevice
                            );
                        this.this$1.this$0.tmDevice.setName(physicaldevice.getDevice().getDeviceName());
                    }

                    this.this$1.this$0.logTag = "TMDeviceControl(" + this.this$1.this$0.tmDevice.getDevicename() + ")";
                    this.this$1.this$0.lastDevice = physicaldevice;
                }
            }

            public static class DeviceLocked extends TMDeviceControl.TMDeviceControlState {
                private final TMDeviceControl.SM.BaseState this$2;

                public DeviceLocked(TMDeviceControl.SM.BaseState tmdevicecontrol$sm$basestate) {
                    this.this$2 = tmdevicecontrol$sm$basestate;
                }

                public void enter(Message message) {
                    this.this$2
                        .this$1
                        .this$0
                        .lc
                        .log(
                            100000,
                            "[%1.DeviceLocked.enter] %2",
                            this.this$2.this$1.this$0.logTag,
                            this.this$2.this$1.this$0.tmDevice
                        );
                    this.this$2.this$1.this$0.tmDevice.setConnectionState(TMDevice.ConnectionState.NOT_ATTACHED);
                    this.this$2
                        .this$1
                        .this$0
                        .deviceControlToDeviceManager
                        .notifyAboutChange(this.this$2.this$1.this$0.tmDevice, "DeviceLocked");
                }

                protected void activate() {
                    this.enter(this.this$2.this$1.getCurrentMessage());
                }
            }

            public static class DiscoveredPhoneCallActiveState extends TMDeviceControl.TMDeviceControlState {
                private Subscription subscribe;
                private final TMDeviceControl.SM.BaseState this$2;

                public DiscoveredPhoneCallActiveState(TMDeviceControl.SM.BaseState tmdevicecontrol$sm$basestate) {
                    this.this$2 = tmdevicecontrol$sm$basestate;
                }

                public void enter(Message message) {
                    this.this$2.this$1.this$0.lastDevice.disconnectDevice(this.this$2.this$1.this$0);
                    this.subscribe = this.this$2
                        .this$1
                        .this$0
                        .stateHandler
                        .getStateApplicationProperty(Application.PHONE)
                        .subscribe(new Consumer() {
                            private final TMDeviceControl.SM.BaseState.DiscoveredPhoneCallActiveState this$3;

                            {
                                this.this$3 = DiscoveredPhoneCallActiveState.this;
                            }

                            public void accept(Object value) {
                                accept((ApplicationOwner)value);
                            }

                            public void accept(ApplicationOwner applicationowner) {
                                if (applicationowner.is(ApplicationOwner.NOBODY)) {
                                    this.this$3.this$2.this$1.sendMessage(18);
                                }
                            }
                        });
                }

                public void exit(Message message) {
                    this.subscribe.cancel();
                }

                public void phoneCallEnded() {
                    this.this$2
                        .this$1
                        .transitionTo(
                            de.audi.app.terminalmode.device.TMDeviceControl.SM.BaseState.DiscoveredState.ReadyForConnect.class
                        );
                }

                protected void deviceNotDiscovered() {
                    this.this$2
                        .this$1
                        .transitionTo(
                            de.audi.app.terminalmode.device.TMDeviceControl.SM.BaseState.NotDiscoveredState.class
                        );
                }

                protected void deviceDiscoveredPhoneCallActive(PhysicalDevice physicaldevice) {
                    this.this$2
                        .this$1
                        .this$0
                        .lc
                        .log(10000000, "[TMDeviceControl.deviceDiscoveredPhoneCallActive] device already on hold");
                }
            }

            public static class DiscoveredState extends TMDeviceControl.TMDeviceControlState {
                private int deviceLockedRetriesCounter;
                private final TMDeviceControl.SM.BaseState this$2;

                public DiscoveredState(TMDeviceControl.SM.BaseState tmdevicecontrol$sm$basestate) {
                    this.this$2 = tmdevicecontrol$sm$basestate;
                    this.deviceLockedRetriesCounter = 0;
                }

                public void enter(Message message) {
                    if (this.this$2
                            .this$1
                            .this$0
                            .tmDevice
                            .userAcceptState()
                            .isOneOf(TMDevice.UserAcceptState.TM_SELECTED, TMDevice.UserAcceptState.NATIVE)
                        || !this.this$2.this$1.this$0.tmDevice.isStoreUserAcceptState()) {
                        this.this$2
                            .this$1
                            .this$0
                            .lc
                            .log(
                                10000000,
                                "[TMDeviceControl.deviceDiscovered] user accept state change: %1 -> %2",
                                this.this$2.this$1.this$0.tmDevice.userAcceptState(),
                                TMDevice.UserAcceptState.INITIAL
                            );
                        this.this$2.this$1.this$0.tmDevice.setUserAcceptState(TMDevice.UserAcceptState.INITIAL);
                    }

                    if (this.this$2
                        .this$1
                        .this$0
                        .tmDevice
                        .userAcceptState()
                        .is(TMDevice.UserAcceptState.NATIVE_SELECTED)) {
                        this.this$2.this$1.this$0.lastDevice.disconnectDevice(this.this$2.this$1.this$0);
                    }

                    if (this.deviceLockedRetriesCounter > 2) {
                        this.this$2
                            .this$1
                            .transitionTo(
                                de.audi.app.terminalmode.device.TMDeviceControl.SM.BaseState.DeviceLocked.class
                            );
                    }

                    this.this$2
                        .this$1
                        .this$0
                        .tmDevice
                        .setAttachTime(this.this$2.this$1.this$0.timeSource.getCurrentTime());
                }

                protected void deviceDiscovered(PhysicalDevice physicaldevice) {
                    this.this$2.updateDevice(physicaldevice);
                    if (this.this$2
                        .this$1
                        .this$0
                        .tmDevice
                        .smartphoneType()
                        .is(SmartphoneManager.SmartphoneType.UNKNOWN)) {
                        this.this$2
                            .this$1
                            .transitionTo(
                                de.audi.app.terminalmode.device.TMDeviceControl.SM.BaseState.NotSupportedState.class
                            );
                    }
                }

                public static class Busy extends TMDeviceControl.TMDeviceControlState {
                    private final TMDeviceControl.SM.BaseState.DiscoveredState this$3;

                    public Busy(
                        TMDeviceControl.SM.BaseState.DiscoveredState tmdevicecontrol$sm$basestate$discoveredstate
                    ) {
                        this.this$3 = tmdevicecontrol$sm$basestate$discoveredstate;
                    }

                    protected void deviceNotDiscovered() {
                        this.this$3
                            .this$2
                            .this$1
                            .transitionTo(
                                de.audi.app.terminalmode.device.TMDeviceControl.SM.BaseState.DiscoveredState.Busy.TearingDown.CleaningUpAndDetaching.class
                            );
                    }

                    protected void deactivate() {
                        this.this$3.this$2.this$1.deferMessage(this.this$3.this$2.this$1.getCurrentMessage());
                    }

                    protected void handleDelete() {
                        if (this.this$3
                            .this$2
                            .this$1
                            .this$0
                            .context
                            .getConfiguration()
                            .supportsDeletionOfConnectedDevices()) {
                            this.this$3
                                .this$2
                                .this$1
                                .this$0
                                .lc
                                .log(
                                    10000000,
                                    "[%1.handleDelete] resetting the device",
                                    this.this$3.this$2.this$1.this$0.logTag
                                );
                            this.this$3
                                .this$2
                                .this$1
                                .this$0
                                .deviceControlToDeviceManager
                                .requestDeactivation(this.this$3.this$2.this$1.this$0);
                            this.this$3.this$2.this$1.deferMessage(this.this$3.this$2.this$1.getCurrentMessage());
                        } else {
                            this.this$3
                                .this$2
                                .this$1
                                .this$0
                                .lc
                                .log(
                                    100000,
                                    "[TMDeviceControl.handleDelete] cannot delete an attached device if not explicitely configured!"
                                );
                        }
                    }

                    private void cleanupAndNotify() {
                        ConvenientCommandList convenientcommandlist = this.this$3
                            .this$2
                            .this$1
                            .this$0
                            .context
                            .getCommandListHelper()
                            .create();
                        convenientcommandlist.add(
                            this.this$3
                                .this$2
                                .this$1
                                .this$0
                                .deviceVariantsHandling
                                .cleanupAfterDeviceDisconnected(
                                    this.this$3.this$2.this$1.this$0.context,
                                    this.this$3.this$2.this$1.this$0.stateHandler,
                                    this.this$3.this$2.this$1.this$0.lc,
                                    this.this$3.this$2.this$1.this$0.logTag
                                )
                        );
                        convenientcommandlist.add(
                            new AbstractCommand(
                                this.this$3.this$2.this$1.this$0.lc,
                                "NotifyCleanupDone(" + this.this$3.this$2.this$1.this$0.logTag + ")",
                                this.this$3.this$2.this$1.this$0.context
                            ) {
                                private final TMDeviceControl.SM.BaseState.DiscoveredState.Busy this$4;

                                {
                                    this.this$4 = Busy.this;
                                }

                                public void execute() {
                                    this.this$4.this$3.this$2.this$1.this$0.sm.sendMessage(10);
                                    this.getCommandList().commandFinished();
                                }
                            }
                        );
                        convenientcommandlist.execute("TMDeviceControl.Busy.cleanupAndNotify()");
                    }

                    private void deactivateSubsystem() {
                        ITMDeviceSubsystem itmdevicesubsystem = (ITMDeviceSubsystem)this.this$3
                            .this$2
                            .this$1
                            .this$0
                            .context
                            .get(
                                this.this$3.this$2.this$1.this$0.tmDevice.smartphoneType(),
                                de.audi.app.terminalmode.ITMDeviceSubsystem.class
                            );
                        this.this$3
                            .this$2
                            .this$1
                            .this$0
                            .lc
                            .log(
                                1000000,
                                "[%1.deactivateSubsystem] deactivating %2",
                                this.this$3.this$2.this$1.this$0.logTag,
                                itmdevicesubsystem
                            );
                        this.this$3
                            .this$2
                            .this$1
                            .this$0
                            .context
                            .getCommandListHelper()
                            .create()
                            .addSingle(
                                new StopService(
                                    this.this$3.this$2.this$1.this$0.dsiManager,
                                    this.this$3.this$2.this$1.this$0.context,
                                    this.this$3.this$2.this$1.this$0.lc,
                                    this.this$3.this$2.this$1.this$0.stateHandler,
                                    this.this$3.this$2.this$1.this$0.smartphoneProperties
                                )
                            )
                            .execute("TMDeviceControl.Busy.deactivateSubsystem()");
                        itmdevicesubsystem.deactivate();
                    }

                    public static class Connecting extends TMDeviceControl.TMDeviceControlState {
                        private final TMDeviceControl.SM.BaseState.DiscoveredState.Busy this$4;

                        public Connecting(
                            TMDeviceControl.SM.BaseState.DiscoveredState.Busy tmdevicecontrol$sm$basestate$discoveredstate$busy
                        ) {
                            this.this$4 = tmdevicecontrol$sm$basestate$discoveredstate$busy;
                        }

                        public void enter(Message message) {
                            this.this$4
                                .this$3
                                .this$2
                                .this$1
                                .this$0
                                .changeConnectionState(TMDevice.ConnectionState.ACTIVATING, "Connecting");
                            this.this$4
                                .this$3
                                .this$2
                                .this$1
                                .this$0
                                .lastDevice
                                .connectDevice(this.this$4.this$3.this$2.this$1.this$0);
                            this.this$4
                                .this$3
                                .this$2
                                .this$1
                                .dsiController
                                .connectDevice(
                                    this.this$4.this$3.this$2.this$1.this$0.tmDevice.getDeviceId(),
                                    this.this$4.this$3.this$2.this$1.this$0.tmDevice.smartphoneType()
                                );
                            this.this$4.this$3.this$2.this$1.sendMessageDelayed(12, "CONNECTING_TIMEOUT", 30000L);
                        }

                        protected void connectionStateConnected() {
                            this.this$4
                                .this$3
                                .this$2
                                .this$1
                                .transitionTo(
                                    de.audi.app.terminalmode.device.TMDeviceControl.SM.BaseState.DiscoveredState.Busy.SubsystemActive.ActivatingSubsystem.class
                                );
                        }

                        protected void connectDeviceFailed(
                            SmartphoneIntegrationDSILifecycleController.ErrorCode errorcode
                        ) {
                            this.handleError();
                        }

                        protected void connectionStateFailed(int i) {
                            if (i == 8) {
                                this.this$4.this$3.this$2.this$1.sendMessageDelayed(15, 10000L);
                            }

                            if (i == 4) {
                                this.this$4.this$3.deviceLockedRetriesCounter++;
                            }

                            if (i == 6) {
                                this.this$4
                                    .this$3
                                    .this$2
                                    .this$1
                                    .this$0
                                    .tmDevice
                                    .setUserAcceptState(TMDevice.UserAcceptState.INITIAL)
                                    .setWasDisclaimerPreviouslyAccepted(false)
                                    .setStoreUserAcceptState(false)
                                    .setConnectionState(TMDevice.ConnectionState.ATTACHED);
                                this.this$4
                                    .this$3
                                    .this$2
                                    .this$1
                                    .this$0
                                    .deviceControlToDeviceManager
                                    .notifyAboutChange(
                                        this.this$4.this$3.this$2.this$1.this$0.tmDevice, "handleNoCarplayDeviceError"
                                    );
                            }

                            this.handleError();
                        }

                        protected void connectionStateStopped() {
                            this.handleError();
                        }

                        protected void connectionStateDisconnected() {
                            this.handleError();
                        }

                        protected void timeout(String s) {
                            this.handleError();
                        }

                        private void handleError() {
                            this.this$4
                                .this$3
                                .this$2
                                .this$1
                                .transitionTo(
                                    de.audi.app.terminalmode.device.TMDeviceControl.SM.BaseState.DiscoveredState.ReadyForConnect.class
                                );
                        }

                        public void exit(Message message) {
                            this.this$4.this$3.this$2.this$1.removeMessages(12);
                        }

                        protected void deviceNotDiscovered() {
                            this.this$4
                                .this$3
                                .this$2
                                .this$1
                                .deferMessage(this.this$4.this$3.this$2.this$1.getCurrentMessage());
                        }
                    }

                    public static class SubsystemActive extends TMDeviceControl.TMDeviceControlState {
                        private final TMDeviceControl.SM.BaseState.DiscoveredState.Busy this$4;

                        public SubsystemActive(
                            TMDeviceControl.SM.BaseState.DiscoveredState.Busy tmdevicecontrol$sm$basestate$discoveredstate$busy
                        ) {
                            this.this$4 = tmdevicecontrol$sm$basestate$discoveredstate$busy;
                        }

                        protected void connectionStateFailed(int i) {
                            if (i == 4) {
                                this.this$4.this$3.deviceLockedRetriesCounter++;
                            }

                            this.handleError();
                        }

                        protected void connectionStateStopped() {
                            this.handleError();
                        }

                        protected void connectionStateDisconnected() {
                            this.handleError();
                        }

                        private void handleError() {
                            this.this$4
                                .this$3
                                .this$2
                                .this$1
                                .transitionTo(
                                    de.audi.app.terminalmode.device.TMDeviceControl.SM.BaseState.DiscoveredState.Busy.TearingDown.CleaningUp.class
                                );
                        }

                        public static class ActivatingSubsystem extends TMDeviceControl.TMDeviceControlState {
                            private final TMDeviceControl.SM.BaseState.DiscoveredState.Busy.SubsystemActive this$5;

                            public ActivatingSubsystem(
                                TMDeviceControl.SM.BaseState.DiscoveredState.Busy.SubsystemActive tmdevicecontrol$sm$basestate$discoveredstate$busy$subsystemactive
                            ) {
                                this.this$5 = tmdevicecontrol$sm$basestate$discoveredstate$busy$subsystemactive;
                            }

                            public void enter(Message message) {
                                ITMDeviceSubsystem itmdevicesubsystem = (ITMDeviceSubsystem)this.this$5
                                    .this$4
                                    .this$3
                                    .this$2
                                    .this$1
                                    .this$0
                                    .context
                                    .get(
                                        this.this$5.this$4.this$3.this$2.this$1.this$0.tmDevice.smartphoneType(),
                                        de.audi.app.terminalmode.ITMDeviceSubsystem.class
                                    );
                                this.this$5
                                    .this$4
                                    .this$3
                                    .this$2
                                    .this$1
                                    .this$0
                                    .lc
                                    .log(10000000, "[%1.enter] activating %2", this.getName(), itmdevicesubsystem);
                                this.this$5
                                    .this$4
                                    .this$3
                                    .this$2
                                    .this$1
                                    .this$0
                                    .changeConnectionState(
                                        TMDevice.ConnectionState.ACTIVATING, "ActivateDeviceSubsystem"
                                    );
                                itmdevicesubsystem.activate();
                                this.this$5
                                    .this$4
                                    .this$3
                                    .this$2
                                    .this$1
                                    .sendMessageDelayed(12, "ACTIVATING_SUBSYSTEM_TIMEOUT", 10000L);
                            }

                            protected void connectionStateStarted() {
                                this.this$5
                                    .this$4
                                    .this$3
                                    .this$2
                                    .this$1
                                    .transitionTo(
                                        de.audi.app.terminalmode.device.TMDeviceControl.SM.BaseState.DiscoveredState.Busy.SubsystemActive.Active.class
                                    );
                            }

                            protected void timeout(String s) {
                                ITMDeviceSubsystem itmdevicesubsystem = (ITMDeviceSubsystem)this.this$5
                                    .this$4
                                    .this$3
                                    .this$2
                                    .this$1
                                    .this$0
                                    .context
                                    .get(
                                        this.this$5.this$4.this$3.this$2.this$1.this$0.tmDevice.smartphoneType(),
                                        de.audi.app.terminalmode.ITMDeviceSubsystem.class
                                    );
                                this.this$5
                                    .this$4
                                    .this$3
                                    .this$2
                                    .this$1
                                    .this$0
                                    .lc
                                    .log(
                                        1000000,
                                        "[%1.ActivatingSubsystem.timeout] deactivating %2",
                                        this.getName(),
                                        itmdevicesubsystem
                                    );
                                this.this$5
                                    .this$4
                                    .this$3
                                    .this$2
                                    .this$1
                                    .this$0
                                    .context
                                    .getCommandListHelper()
                                    .create()
                                    .addSingle(
                                        new StopService(
                                            this.this$5.this$4.this$3.this$2.this$1.this$0.dsiManager,
                                            this.this$5.this$4.this$3.this$2.this$1.this$0.context,
                                            this.this$5.this$4.this$3.this$2.this$1.this$0.lc,
                                            this.this$5.this$4.this$3.this$2.this$1.this$0.stateHandler,
                                            this.this$5.this$4.this$3.this$2.this$1.this$0.smartphoneProperties
                                        )
                                    )
                                    .execute("TMDeviceControl.ActivatingSubsystem.timeout(String)");
                                itmdevicesubsystem.deactivate();
                                this.this$5
                                    .this$4
                                    .this$3
                                    .this$2
                                    .this$1
                                    .transitionTo(
                                        de.audi.app.terminalmode.device.TMDeviceControl.SM.BaseState.DiscoveredState.ReadyForConnect.class
                                    );
                            }

                            public void exit(Message message) {
                                this.this$5.this$4.this$3.this$2.this$1.removeMessages(12);
                            }
                        }

                        public static class Active extends TMDeviceControl.TMDeviceControlState {
                            private final TMDeviceControl.SM.BaseState.DiscoveredState.Busy.SubsystemActive this$5;

                            public Active(
                                TMDeviceControl.SM.BaseState.DiscoveredState.Busy.SubsystemActive tmdevicecontrol$sm$basestate$discoveredstate$busy$subsystemactive
                            ) {
                                this.this$5 = tmdevicecontrol$sm$basestate$discoveredstate$busy$subsystemactive;
                            }

                            public void enter(Message message) {
                                this.this$5.this$4.this$3.this$2.this$1.this$0.stateHandler.setServiceStatus(true);
                                this.this$5
                                    .this$4
                                    .this$3
                                    .this$2
                                    .this$1
                                    .this$0
                                    .changeConnectionState(TMDevice.ConnectionState.ACTIVE, "ActivateDeviceSubsystem");
                                this.this$5
                                    .this$4
                                    .this$3
                                    .this$2
                                    .this$1
                                    .this$0
                                    .smartphoneProperties
                                    .getPropertySPIServiceState()
                                    .accept(SPIServiceState.STARTED);
                                this.setHMIDeviceState(true);
                                IAudioManager iaudiomanager = this.this$5
                                    .this$4
                                    .this$3
                                    .this$2
                                    .this$1
                                    .this$0
                                    .context
                                    .getAudioManager();
                                this.this$5
                                    .this$4
                                    .this$3
                                    .this$2
                                    .this$1
                                    .this$0
                                    .context
                                    .getCommandListHelper()
                                    .create()
                                    .addSingle(
                                        iaudiomanager.createAudioConnectionRequest(
                                            TMAudioConnection.ANNOUNCEMENT, false
                                        )
                                    )
                                    .execute(this.this$5.this$4.this$3.this$2.this$1.this$0.logTag + ".Active.enter()");
                                this.this$5
                                    .this$4
                                    .this$3
                                    .this$2
                                    .this$1
                                    .this$0
                                    .eventBus
                                    .deviceActivated(this.this$5.this$4.this$3.this$2.this$1.this$0.tmDevice);
                            }

                            protected void deactivate() {
                                this.this$5
                                    .this$4
                                    .this$3
                                    .this$2
                                    .this$1
                                    .transitionTo(
                                        de.audi.app.terminalmode.device.TMDeviceControl.SM.BaseState.DiscoveredState.Busy.TearingDown.CleaningUpAndDisconnecting.class
                                    );
                            }

                            public void exit(Message message) {
                                this.setHMIDeviceState(false);
                                this.resetModels();
                            }

                            private void setHMIDeviceState(boolean flag) {
                                int i = this.this$5.this$4.this$3.this$2.this$1.this$0.tmDevice.isCarplayDevice()
                                    ? 1
                                    : (
                                        this.this$5.this$4.this$3.this$2.this$1.this$0.tmDevice.isAndroidAutoDevice()
                                            ? 2
                                            : 3
                                    );
                                this.updateMainWizardModel(flag, i);
                                this.updateTabbarModel(flag, i);
                            }

                            private void updateMainWizardModel(boolean flag, int type) {
                                de.audi.atip.hmi.IHMIServiceApp hmi = this.this$5.this$4.this$3.this$2.this$1.this$0
                                    .context.getFramework().getHmiServiceApp();
                                com.luka.carplay.startup.HuMenuIdentity.stockIdentity(
                                    hmi.getChoiceModel(ICoreSystemModelBank.ACTIVE_SMARTPHONE_DEVICE_CHOICE),
                                    flag ? type : 0, type);
                                int selected = flag ? (type == 1 ? 16 : type == 2 ? 17 : 19) : 18;
                                com.luka.carplay.startup.HuMenuIdentity.stockSelection(hmi.getChoiceModel(138), selected);
                            }

                            private void updateTabbarModel(boolean flag, int i) {
                                if (flag) {
                                    if (1 == i) {
                                        this.this$5
                                            .this$4
                                            .this$3
                                            .this$2
                                            .this$1
                                            .this$0
                                            .context
                                            .getChoiceModel(ICoreSystemModelBank.SMARTPHONE_TABBAR_DEVICE_TYPE_CHOICE)
                                            .setValue(1);
                                    } else {
                                        this.this$5
                                            .this$4
                                            .this$3
                                            .this$2
                                            .this$1
                                            .this$0
                                            .context
                                            .getChoiceModel(ICoreSystemModelBank.SMARTPHONE_TABBAR_DEVICE_TYPE_CHOICE)
                                            .setValue(2);
                                    }
                                } else {
                                    this.this$5
                                        .this$4
                                        .this$3
                                        .this$2
                                        .this$1
                                        .this$0
                                        .context
                                        .getChoiceModel(ICoreSystemModelBank.SMARTPHONE_TABBAR_DEVICE_TYPE_CHOICE)
                                        .setValue(0);
                                }
                            }

                            private void resetModels() {
                                this.this$5
                                    .this$4
                                    .this$3
                                    .this$2
                                    .this$1
                                    .this$0
                                    .eventBus
                                    .updateNowPlayingData(new TrackDataChangedEvent.Builder().build());
                                this.this$5
                                    .this$4
                                    .this$3
                                    .this$2
                                    .this$1
                                    .this$0
                                    .eventBus
                                    .updatePlayPosition(new TrackPlayPositionEvent(0, 0));
                            }
                        }
                    }

                    public static class TearingDown extends TMDeviceControl.TMDeviceControlState {
                        private final TMDeviceControl.SM.BaseState.DiscoveredState.Busy this$4;

                        public TearingDown(
                            TMDeviceControl.SM.BaseState.DiscoveredState.Busy tmdevicecontrol$sm$basestate$discoveredstate$busy
                        ) {
                            this.this$4 = tmdevicecontrol$sm$basestate$discoveredstate$busy;
                        }

                        protected void handleDelete() {
                            this.this$4
                                .this$3
                                .this$2
                                .this$1
                                .deferMessage(this.this$4.this$3.this$2.this$1.getCurrentMessage());
                        }

                        private long getTimeoutConnectorDependent() {
                            return this.this$4
                                    .this$3
                                    .this$2
                                    .this$1
                                    .this$0
                                    .context
                                    .getConfiguration()
                                    .usesOldMediaConnector()
                                ? 10000L
                                : 5000L;
                        }

                        public static class CleaningUp extends TMDeviceControl.TMDeviceControlState {
                            private final TMDeviceControl.SM.BaseState.DiscoveredState.Busy.TearingDown this$5;

                            public CleaningUp(
                                TMDeviceControl.SM.BaseState.DiscoveredState.Busy.TearingDown tmdevicecontrol$sm$basestate$discoveredstate$busy$tearingdown
                            ) {
                                this.this$5 = tmdevicecontrol$sm$basestate$discoveredstate$busy$tearingdown;
                            }

                            public void enter(Message message) {
                                this.this$5.this$4.cleanupAndNotify();
                            }

                            protected void cleanupDone() {
                                this.this$5.this$4.deactivateSubsystem();
                                this.this$5
                                    .this$4
                                    .this$3
                                    .this$2
                                    .this$1
                                    .transitionTo(
                                        de.audi.app.terminalmode.device.TMDeviceControl.SM.BaseState.DiscoveredState.ReadyForConnect.class
                                    );
                            }
                        }

                        public static class CleaningUpAndDetaching extends TMDeviceControl.TMDeviceControlState {
                            private static final String CLEANUP_DONE = "CLEANUP_DONE";
                            private static final String DISCONNECTED = "DISCONNECTED";
                            private final SyncPoint cleanedUpAndDetached;
                            private final TMDeviceControl.SM.BaseState.DiscoveredState.Busy.TearingDown this$5;

                            public CleaningUpAndDetaching(
                                TMDeviceControl.SM.BaseState.DiscoveredState.Busy.TearingDown tmdevicecontrol$sm$basestate$discoveredstate$busy$tearingdown
                            ) {
                                this.this$5 = tmdevicecontrol$sm$basestate$discoveredstate$busy$tearingdown;
                                this.cleanedUpAndDetached = new SyncPoint.Builder()
                                    .addRequirement(CLEANUP_DONE)
                                    .addRequirement(DISCONNECTED)
                                    .setCallback(
                                        new Runnable() {
                                            private final TMDeviceControl.SM.BaseState.DiscoveredState.Busy.TearingDown.CleaningUpAndDetaching this$6;

                                            {
                                                this.this$6 = CleaningUpAndDetaching.this;
                                            }

                                            public void run() {
                                                this.this$6
                                                    .this$5
                                                    .this$4
                                                    .this$3
                                                    .this$2
                                                    .this$1
                                                    .transitionTo(
                                                        de.audi.app.terminalmode.device.TMDeviceControl.SM.BaseState.NotDiscoveredState.class
                                                    );
                                            }
                                        }
                                    )
                                    .setLogChannel(this.this$5.this$4.this$3.this$2.this$1.this$0.lc)
                                    .setName(this.this$5.this$4.this$3.this$2.this$1.this$0.logTag)
                                    .build();
                            }

                            public void enter(Message message) {
                                this.cleanedUpAndDetached.reset();
                                this.this$5
                                    .this$4
                                    .this$3
                                    .this$2
                                    .this$1
                                    .this$0
                                    .changeConnectionState(
                                        TMDevice.ConnectionState.NOT_ATTACHED, "deviceIsNotDiscovered"
                                    );
                                this.this$5.this$4.cleanupAndNotify();
                                this.this$5
                                    .this$4
                                    .this$3
                                    .this$2
                                    .this$1
                                    .sendMessageDelayed(
                                        12, "DETACHING_TIMEOUT", this.this$5.getTimeoutConnectorDependent()
                                    );
                            }

                            protected void cleanupDone() {
                                this.this$5.this$4.deactivateSubsystem();
                                this.cleanedUpAndDetached.completeRequirement(CLEANUP_DONE);
                            }

                            protected void connectionStateStopped() {
                                this.this$5
                                    .this$4
                                    .this$3
                                    .this$2
                                    .this$1
                                    .this$0
                                    .lc
                                    .log(
                                        1000000,
                                        "[%1.CleaningUpAndDetaching.connectionStateStopped] waiting for disconnected",
                                        this.this$5.this$4.this$3.this$2.this$1.this$0.logTag
                                    );
                            }

                            protected void connectionStateDisconnected() {
                                this.cleanedUpAndDetached.completeRequirement(DISCONNECTED);
                            }

                            protected void deviceDiscovered(PhysicalDevice physicaldevice) {
                                this.this$5
                                    .this$4
                                    .this$3
                                    .this$2
                                    .this$1
                                    .deferMessage(this.this$5.this$4.this$3.this$2.this$1.getCurrentMessage());
                            }

                            public void exit(Message message) {
                                this.this$5.this$4.this$3.this$2.this$1.removeMessages(12);
                            }

                            protected void timeout(String s) {
                                this.this$5
                                    .this$4
                                    .this$3
                                    .this$2
                                    .this$1
                                    .this$0
                                    .lc
                                    .log(100000, "[%1.DetachingState.timeout] did not receive disconnected in time!");
                                this.cleanedUpAndDetached.completeRequirement(DISCONNECTED);
                            }
                        }

                        public static class CleaningUpAndDisconnecting extends TMDeviceControl.TMDeviceControlState {
                            private final TMDeviceControl.SM.BaseState.DiscoveredState.Busy.TearingDown this$5;

                            public CleaningUpAndDisconnecting(
                                TMDeviceControl.SM.BaseState.DiscoveredState.Busy.TearingDown tmdevicecontrol$sm$basestate$discoveredstate$busy$tearingdown
                            ) {
                                this.this$5 = tmdevicecontrol$sm$basestate$discoveredstate$busy$tearingdown;
                            }

                            public void enter(Message message) {
                                this.this$5.this$4.cleanupAndNotify();
                            }

                            protected void cleanupDone() {
                                this.this$5.this$4.deactivateSubsystem();
                                this.this$5
                                    .this$4
                                    .this$3
                                    .this$2
                                    .this$1
                                    .dsiController
                                    .disconnectDevice(
                                        this.this$5.this$4.this$3.this$2.this$1.this$0.tmDevice.getDeviceId()
                                    );
                                this.this$5
                                    .this$4
                                    .this$3
                                    .this$2
                                    .this$1
                                    .sendMessageDelayed(
                                        12, "DETACHING_TIMEOUT", this.this$5.getTimeoutConnectorDependent()
                                    );
                            }

                            protected void connectionStateStopped() {
                                this.this$5
                                    .this$4
                                    .this$3
                                    .this$2
                                    .this$1
                                    .this$0
                                    .lc
                                    .log(
                                        1000000,
                                        "[%1.Disconnecting.connectionStateStopped] waiting for Disconnected",
                                        this.getName()
                                    );
                            }

                            protected void connectionStateDisconnected() {
                                this.this$5
                                    .this$4
                                    .this$3
                                    .this$2
                                    .this$1
                                    .transitionTo(
                                        de.audi.app.terminalmode.device.TMDeviceControl.SM.BaseState.DiscoveredState.ReadyForConnect.class
                                    );
                            }

                            public void exit(Message message) {
                                this.this$5.this$4.this$3.this$2.this$1.removeMessages(12);
                            }

                            protected void timeout(String s) {
                                this.this$5
                                    .this$4
                                    .this$3
                                    .this$2
                                    .this$1
                                    .this$0
                                    .lc
                                    .log(
                                        100000,
                                        "[%1.CleaningUpAndDisconnecting.timeout] Waited for disconnected but never got it!"
                                    );
                                this.this$5
                                    .this$4
                                    .this$3
                                    .this$2
                                    .this$1
                                    .transitionTo(
                                        de.audi.app.terminalmode.device.TMDeviceControl.SM.BaseState.DiscoveredState.ReadyForConnect.class
                                    );
                            }
                        }
                    }
                }

                public static class ReadyForConnect extends TMDeviceControl.TMDeviceControlState {
                    private boolean isDisconnectedInSameState;
                    private final TMDeviceControl.SM.BaseState.DiscoveredState this$3;

                    public ReadyForConnect(
                        TMDeviceControl.SM.BaseState.DiscoveredState tmdevicecontrol$sm$basestate$discoveredstate
                    ) {
                        this.this$3 = tmdevicecontrol$sm$basestate$discoveredstate;
                        this.isDisconnectedInSameState = false;
                    }

                    public void enter(Message message) {
                        this.this$3
                            .this$2
                            .this$1
                            .this$0
                            .changeConnectionState(TMDevice.ConnectionState.ATTACHED, message.toString());
                        if ((
                                this.this$3.this$2.this$1.getCurrentMessage().getCode() == 16
                                    || this.this$3.this$2.this$1.getCurrentMessage().getCode() == 18
                            )
                            && !this.this$3.this$2.this$1.hasMessages(15)) {
                            this.this$3
                                .this$2
                                .this$1
                                .this$0
                                .eventBus
                                .readyForActivation(this.this$3.this$2.this$1.this$0.tmDevice);
                        } else {
                            this.this$3
                                .this$2
                                .this$1
                                .this$0
                                .lc
                                .log(
                                    10000000,
                                    "[%1.ReadyForConnect.enter] skipped readyForActivation, reason %2%3",
                                    this.this$3.this$2.this$1.this$0.logTag,
                                    message.tag,
                                    this.this$3.this$2.this$1.hasMessages(15)
                                        ? ", SUPRESS_AUTOMATIC_ACTIVATIONS_TIMER"
                                        : ""
                                );
                        }
                    }

                    protected void activate() {
                        this.this$3
                            .this$2
                            .this$1
                            .transitionTo(
                                de.audi.app.terminalmode.device.TMDeviceControl.SM.BaseState.DiscoveredState.Busy.Connecting.class
                            );
                    }

                    protected void connectionStateFailed(int i) {
                        if (i == 7) {
                            this.this$3
                                .this$2
                                .this$1
                                .this$0
                                .lc
                                .log(
                                    1000000,
                                    "[%1.ReadyForConnect.connectionStateFailed] CONNECTIONSTATE_FAILED_NO_GAL_DEVICE while ReadyForConnect",
                                    this.getName()
                                );
                            this.this$3
                                .this$2
                                .this$1
                                .this$0
                                .tmDevice
                                .setUserAcceptState(TMDevice.UserAcceptState.NATIVE_SELECTED);
                        }
                    }

                    protected void deviceDiscovered(PhysicalDevice physicaldevice) {
                        this.this$3
                            .this$2
                            .this$1
                            .this$0
                            .lc
                            .log(
                                10000000,
                                "[%1.deviceDiscovered] was disconnected in same state: %2",
                                this.this$3.this$2.this$1.this$0.logTag,
                                this.isDisconnectedInSameState ? "true" : "false"
                            );
                        if (this.isDisconnectedInSameState
                            && this.this$3
                                .this$2
                                .this$1
                                .this$0
                                .tmDevice
                                .userAcceptState()
                                .isNot(TMDevice.UserAcceptState.NATIVE_SELECTED)) {
                            this.this$3
                                .this$2
                                .this$1
                                .this$0
                                .lc
                                .log(
                                    10000000,
                                    "[%1.deviceDiscovered] re-activate device",
                                    this.this$3.this$2.this$1.this$0.logTag
                                );
                            this.this$3
                                .this$2
                                .this$1
                                .this$0
                                .eventBus
                                .readyForActivation(this.this$3.this$2.this$1.this$0.tmDevice);
                        }
                    }

                    protected void connectionStateDisconnected() {
                        this.this$3
                            .this$2
                            .this$1
                            .this$0
                            .lc
                            .log(
                                10000000,
                                "[%1.connectionStateDisconnected] set disconnected flag",
                                this.this$3.this$2.this$1.this$0.logTag
                            );
                        this.isDisconnectedInSameState = true;
                    }

                    protected void deviceNotDiscovered() {
                        this.this$3
                            .this$2
                            .this$1
                            .this$0
                            .context
                            .getChoiceModel(ICoreSystemModelBank.SMARTPHONE_TABBAR_DEVICE_TYPE_CHOICE)
                            .setValue(0);
                        this.this$3
                            .this$2
                            .this$1
                            .transitionTo(
                                de.audi.app.terminalmode.device.TMDeviceControl.SM.BaseState.NotDiscoveredState.class
                            );
                    }

                    protected void connectionStateConnected() {
                        this.this$3
                            .this$2
                            .this$1
                            .transitionTo(
                                de.audi.app.terminalmode.device.TMDeviceControl.SM.BaseState.DiscoveredState.Busy.SubsystemActive.ActivatingSubsystem.class
                            );
                    }

                    protected void deactivate() {
                        if (this.this$3.this$2.this$1.this$0.context.getConfiguration().usesOldMediaConnector()) {
                            this.this$3.this$2.this$1.sendMessageDelayed(15, 10000L);
                        }

                        this.this$3.this$2.this$1.this$0.lastDevice.disconnectDevice(this.this$3.this$2.this$1.this$0);
                    }

                    protected void handleDelete() {
                        this.this$3
                            .this$2
                            .this$1
                            .this$0
                            .tmDevice
                            .setUserAcceptState(TMDevice.UserAcceptState.INITIAL)
                            .setWasDisclaimerPreviouslyAccepted(false)
                            .setConnectionState(TMDevice.ConnectionState.ATTACHED);
                        this.this$3
                            .this$2
                            .this$1
                            .this$0
                            .deviceControlToDeviceManager
                            .notifyAboutChange(this.this$3.this$2.this$1.this$0.tmDevice, "handleDelete");
                        this.this$3
                            .this$2
                            .this$1
                            .this$0
                            .eventBus
                            .readyForActivation(this.this$3.this$2.this$1.this$0.tmDevice);
                    }
                }
            }

            public static class NotDiscoveredState extends TMDeviceControl.TMDeviceControlState {
                private final TMDeviceControl.SM.BaseState this$2;

                public NotDiscoveredState(TMDeviceControl.SM.BaseState tmdevicecontrol$sm$basestate) {
                    this.this$2 = tmdevicecontrol$sm$basestate;
                }

                public void enter(Message message) {
                    this.this$2
                        .this$1
                        .this$0
                        .changeConnectionState(TMDevice.ConnectionState.NOT_ATTACHED, "deviceIsNotDiscovered");
                }

                public void exit(Message message) {
                    this.this$2.this$1.removeMessages(16);
                }

                protected void deviceDiscoveredDebounced(PhysicalDevice physicaldevice) {
                    this.this$2.updateDevice(physicaldevice);
                    if (this.this$2
                        .this$1
                        .this$0
                        .tmDevice
                        .smartphoneType()
                        .isNot(SmartphoneManager.SmartphoneType.UNKNOWN)) {
                        this.this$2
                            .this$1
                            .transitionTo(
                                de.audi.app.terminalmode.device.TMDeviceControl.SM.BaseState.DiscoveredState.ReadyForConnect.class
                            );
                    } else {
                        this.this$2
                            .this$1
                            .transitionTo(
                                de.audi.app.terminalmode.device.TMDeviceControl.SM.BaseState.NotSupportedState.class
                            );
                    }
                }

                protected void deviceDiscovered(PhysicalDevice physicaldevice) {
                    int i = this.this$2.this$1.this$0.context.getConfiguration().usesOldMediaConnector()
                            && !this.this$2.this$1.this$0.tmDevice.isStoreUserAcceptState()
                        ? 2000
                        : 0;
                    this.this$2
                        .this$1
                        .this$0
                        .lc
                        .log(1000000, "[%1.deviceDiscovered] %2", this.this$2.this$1.this$0.logTag, i);
                    this.this$2.this$1.sendMessageDelayed(16, physicaldevice, i);
                }

                protected void deviceNotDiscovered() {
                    this.this$2.this$1.removeMessages(16);
                }

                protected void deactivate() {
                    this.this$2.this$1.removeMessages(16);
                }

                protected void handleDelete() {
                    this.this$2.this$1.this$0.deviceControlToDeviceManager.deleteMe(this.this$2.this$1.this$0);
                }

                protected void connectionStateFailed(int i) {
                    if (i == 6) {
                        this.this$2
                            .this$1
                            .this$0
                            .tmDevice
                            .setUserAcceptState(TMDevice.UserAcceptState.INITIAL)
                            .setWasDisclaimerPreviouslyAccepted(false)
                            .setStoreUserAcceptState(false)
                            .setConnectionState(TMDevice.ConnectionState.ATTACHED);
                        this.this$2
                            .this$1
                            .this$0
                            .deviceControlToDeviceManager
                            .notifyAboutChange(this.this$2.this$1.this$0.tmDevice, "handleNoCarplayDeviceError");
                    }
                }

                protected void deviceDiscoveredPhoneCallActive(PhysicalDevice physicaldevice) {
                    this.this$2.updateDevice(physicaldevice);
                    this.this$2
                        .this$1
                        .transitionTo(
                            de.audi.app.terminalmode.device.TMDeviceControl.SM.BaseState.DiscoveredPhoneCallActiveState.class
                        );
                }
            }

            public static class NotSupportedState extends TMDeviceControl.TMDeviceControlState {
                private final TMDeviceControl.SM.BaseState this$2;

                public NotSupportedState(TMDeviceControl.SM.BaseState tmdevicecontrol$sm$basestate) {
                    this.this$2 = tmdevicecontrol$sm$basestate;
                }

                public void enter(Message message) {
                    this.this$2
                        .this$1
                        .this$0
                        .lc
                        .log(
                            100000,
                            "[%1.NotSupportedState.enter] %2",
                            this.this$2.this$1.this$0.logTag,
                            this.this$2.this$1.this$0.tmDevice
                        );
                    this.this$2.this$1.this$0.tmDevice.setConnectionState(TMDevice.ConnectionState.NOT_ATTACHED);
                    this.this$2
                        .this$1
                        .this$0
                        .deviceControlToDeviceManager
                        .notifyAboutChange(this.this$2.this$1.this$0.tmDevice, "NotSupportedState");
                }

                protected void deviceDiscovered(PhysicalDevice physicaldevice) {
                    this.this$2.updateDevice(physicaldevice);
                    if (this.this$2
                        .this$1
                        .this$0
                        .tmDevice
                        .smartphoneType()
                        .isNot(SmartphoneManager.SmartphoneType.UNKNOWN)) {
                        this.this$2
                            .this$1
                            .transitionTo(
                                de.audi.app.terminalmode.device.TMDeviceControl.SM.BaseState.DiscoveredState.ReadyForConnect.class
                            );
                    }
                }

                protected void handleDelete() {
                    this.this$2.this$1.this$0.deviceControlToDeviceManager.deleteMe(this.this$2.this$1.this$0);
                }
            }
        }
    }

    private static class TMDeviceControlState extends State {
        public static final int DEVICE_DISCOVERED = 1;
        public static final int ACTIVATE = 2;
        public static final int DEACTIVATE = 3;
        public static final int DEVICE_NOT_DISCOVERED = 4;
        public static final int CONNECTIONSTATE_CONNECTED = 5;
        public static final int CONNECTIONSTATE_STARTED = 6;
        public static final int CONNECTIONSTATE_FAILED = 7;
        public static final int CONNECTIONSTATE_DISCONNECTED = 8;
        public static final int CONNECTIONSTATE_STOPPED = 9;
        public static final int CLEANUP_DONE = 10;
        public static final int TIMEOUT = 12;
        public static final int CONNECT_DEVICE_FAILED = 13;
        public static final int HANDLE_DELETE = 14;
        public static final int SUPRESS_AUTOMATIC_ACTIVATIONS_TIMER = 15;
        public static final int DEVICE_DISCOVERED_DEBOUNCED = 16;
        public static final int DEVICE_DISCOVERED_PHONE_CALL_ACTIVE = 17;
        public static final int PHONE_CALL_ENDED = 18;
        private boolean handled = false;

        private TMDeviceControlState() {
        }

        public final boolean processMessage(Message message) {
            this.handled = true;
            switch (message.getCode()) {
                case 1:
                    this.deviceDiscovered((PhysicalDevice)message.obj);
                    break;
                case 2:
                    this.activate();
                    break;
                case 3:
                    this.deactivate();
                    break;
                case 4:
                    this.deviceNotDiscovered();
                    break;
                case 5:
                    this.connectionStateConnected();
                    break;
                case 6:
                    this.connectionStateStarted();
                    break;
                case 7:
                    this.connectionStateFailed(message.arg1);
                    break;
                case 8:
                    this.connectionStateDisconnected();
                    break;
                case 9:
                    this.connectionStateStopped();
                    break;
                case 10:
                    this.cleanupDone();
                    break;
                case 11:
                default:
                    this.markNotHandled();
                    break;
                case 12:
                    this.timeout((String)message.obj);
                    break;
                case 13:
                    this.connectDeviceFailed((SmartphoneIntegrationDSILifecycleController.ErrorCode)message.obj);
                    break;
                case 14:
                    this.handleDelete();
                case 15:
                    break;
                case 16:
                    this.deviceDiscoveredDebounced((PhysicalDevice)message.obj);
                    break;
                case 17:
                    this.deviceDiscoveredPhoneCallActive((PhysicalDevice)message.obj);
                    break;
                case 18:
                    this.phoneCallEnded();
            }

            return this.handled;
        }

        protected void deviceDiscovered(PhysicalDevice physicaldevice) {
            this.markNotHandled();
        }

        protected void deviceDiscoveredDebounced(PhysicalDevice physicaldevice) {
            this.markNotHandled();
        }

        protected void deviceNotDiscovered() {
            this.markNotHandled();
        }

        protected void activate() {
            this.markNotHandled();
        }

        protected void deactivate() {
            this.markNotHandled();
        }

        protected void connectionStateConnected() {
            this.markNotHandled();
        }

        protected void connectionStateStopped() {
            this.markNotHandled();
        }

        protected void connectionStateDisconnected() {
            this.markNotHandled();
        }

        protected void connectionStateFailed(int i) {
            this.markNotHandled();
        }

        protected void connectionStateStarted() {
            this.markNotHandled();
        }

        protected void cleanupDone() {
            this.markNotHandled();
        }

        protected void timeout(String s) {
            this.markNotHandled();
        }

        protected void connectDeviceFailed(SmartphoneIntegrationDSILifecycleController.ErrorCode errorcode) {
            this.markNotHandled();
        }

        protected void handleDelete() {
            this.markNotHandled();
        }

        protected void deviceDiscoveredPhoneCallActive(PhysicalDevice physicaldevice) {
            this.markNotHandled();
        }

        protected void phoneCallEnded() {
            this.markNotHandled();
        }

        protected void markNotHandled() {
            this.handled = false;
        }

        static Handler.IMessageCodeToStringConverter getMessageCodeConverter() {
            return new Handler.IMessageCodeToStringConverter() {
                public String messageCodeToString(int i) {
                    switch (i) {
                        case 1:
                            return "DEVICE_DISCOVERED";
                        case 2:
                            return "ACTIVATE";
                        case 3:
                            return "DEACTIVATE";
                        case 4:
                            return "DEVICE_NOT_DISCOVERED";
                        case 5:
                            return "CONNECTIONSTATE_CONNECTED";
                        case 6:
                            return "CONNECTIONSTATE_STARTED";
                        case 7:
                            return "CONNECTIONSTATE_FAILED";
                        case 8:
                            return "CONNECTIONSTATE_DISCONNECTED";
                        case 9:
                            return "CONNECTIONSTATE_STOPPED";
                        case 10:
                            return "CLEANUP_DONE";
                        case 11:
                        default:
                            return Integer.toString(i);
                        case 12:
                            return "TIMEOUT";
                        case 13:
                            return "CONNECT_DEVICE_FAILED";
                        case 14:
                            return "HANDLE_DELETE";
                        case 15:
                            return "SUPRESS_AUTOMATIC_ACTIVATIONS_TIMER";
                        case 16:
                            return "DEVICE_DISCOVERED_DEBOUNCED";
                        case 17:
                            return "DEVICE_DISCOVERED_PHONE_CALL_ACTIVE";
                        case 18:
                            return "PHONE_CALL_ENDED";
                    }
                }
            };
        }
    }
}
