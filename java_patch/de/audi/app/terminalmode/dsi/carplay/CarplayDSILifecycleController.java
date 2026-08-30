package de.audi.app.terminalmode.dsi.carplay;

import de.audi.app.terminalmode.IContext;
import de.audi.app.terminalmode.ITerminalModeConfiguration;
import de.audi.app.terminalmode.SmartphoneManager;
import de.audi.app.terminalmode.TerminalModeUtils;
import de.audi.app.terminalmode.diagnosis.IDiagnosisCommandProvider;
import de.audi.app.terminalmode.dsi.AbstractDSIController;
import de.audi.app.terminalmode.dsi.AbstractDispatcherRunnable;
import de.audi.app.terminalmode.dsi.IAppState;
import de.audi.app.terminalmode.dsi.IDSIAppState;
import de.audi.app.terminalmode.dsi.IDSIResource;
import de.audi.app.terminalmode.dsi.IResource;
import de.audi.app.terminalmode.events.CallStateChangedEvent;
import de.audi.app.terminalmode.events.DefaultEventListener;
import de.audi.app.terminalmode.events.IEventBus;
import de.audi.app.terminalmode.events.PhoneStateChangedEvent;
import de.audi.app.terminalmode.events.PlayPositionEvent;
import de.audi.app.terminalmode.events.PlayPositionLogEvent;
import de.audi.app.terminalmode.events.PlayPositionLogger;
import de.audi.app.terminalmode.events.PlayPositionLoggerConfiguration;
import de.audi.app.terminalmode.events.PlaybackInfoChangedEvent;
import de.audi.app.terminalmode.events.TrackDataChangedEvent;
import de.audi.app.terminalmode.events.TrackPlayPositionEvent;
import de.audi.app.terminalmode.keyevents.ITerminalModeDSIKeyEventsController;
import de.audi.app.terminalmode.keyevents.Key;
import de.audi.app.terminalmode.keyevents.KeyState;
import de.audi.app.terminalmode.logging.Arrays2;
import de.audi.atip.base.IFrameworkAccess;
import de.audi.atip.log.LogChannel;
import de.audi.atip.utils.generics.GList;
import de.audi.atip.utils.generics.Generics;
import de.esolutions.fw.util.commons.Buffer;
import de.esolutions.fw.util.commons.job.DispatcherBase;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import org.dsi.ifc.base.DSIBase;
import org.dsi.ifc.base.DSIListener;
import org.dsi.ifc.carplay.AppState;
import org.dsi.ifc.carplay.AppStateRequest;
import org.dsi.ifc.carplay.CallState;
import org.dsi.ifc.carplay.DSICarplay;
import org.dsi.ifc.carplay.DSICarplayListener;
import org.dsi.ifc.carplay.DeviceInfo;
import org.dsi.ifc.carplay.PlaybackInfo;
import org.dsi.ifc.carplay.Resource;
import org.dsi.ifc.carplay.ResourceRequest;
import org.dsi.ifc.carplay.ServiceConfiguration;
import org.dsi.ifc.carplay.TelephonyState;
import org.dsi.ifc.carplay.TouchEvent;
import com.luka.carplay.cursor.CursorController;
import com.luka.carplay.core.SteeringWheelInputModule;
import org.dsi.ifc.carplay.TrackData;
import org.dsi.ifc.global.ResourceLocator;

public class CarplayDSILifecycleController extends AbstractDSIController implements IDiagnosisCommandProvider {
    private final DSICarplaySafeProxy dsiCarplayProxy;
    private final DSICarplaySafe dsiCarplaySafe;
    private final ITerminalModeConfiguration configuration;
    private final CarplayDSILifecycleController.DSICarplayListenerImpl dsiCarPlayListener;
    private volatile Key lastJoystickkey;
    private final CarplayDSILifecycleController.CarplayDSIController carPlayDsiController;
    private final ITerminalModeDSIKeyEventsController keyEventController;
    private final IFrameworkAccess fw;
    private final IContext tmManager;
    private final IDSICarplayTransferObjectFactory transferObjectFactory;
    private volatile TouchEvent[] lastFingers;
    private static final String postButton = "postButton(CarplayId)";
    private static final String requestUI = "requestUI";

    public CarplayDSILifecycleController(
        IContext icontext,
        IDSICarplayTransferObjectFactory idsicarplaytransferobjectfactory,
        DSICarplaySafe dsicarplaysafe,
        CarplayDSILifecycleController.DSICarplayListenerImpl carplaydsilifecyclecontroller$dsicarplaylistenerimpl,
        DSICarplaySafeProxy dsicarplaysafeproxy
    ) {
        super(
            0,
            icontext.getServiceManager(),
            icontext.getFramework(),
            icontext.getDispatcher(),
            true,
            icontext.getLogger().dsi()
        );
        this.dsiCarplaySafe = dsicarplaysafe;
        this.dsiCarplayProxy = dsicarplaysafeproxy;
        this.dsiCarPlayListener = carplaydsilifecyclecontroller$dsicarplaylistenerimpl;
        this.carPlayDsiController = new CarplayDSILifecycleController.CarplayDSIController(this);
        this.configuration = icontext.getConfiguration();
        this.keyEventController = new CarplayDSILifecycleController.TerminalModeDSIKeyEventsController(this);
        this.fw = icontext.getFramework();
        this.tmManager = icontext;
        icontext.getDiagnosisManager().addCommandProvider(0, this);
        this.transferObjectFactory = idsicarplaytransferobjectfactory;
    }

    public void init() {
        super.init();
        this.startDSI();
        this.dsiCarPlayListener.init();
        this.carPlayDsiController.init();
        /* The same component object can be deinit/init'd; deinit clears the singleton sink. */
        ((CarplayDSILifecycleController.TerminalModeDSIKeyEventsController)this.keyEventController)
            .installCursorTouchSink();
    }

    public void deinit() {
        super.deinit();
        /* Remove the touch sink installed at CarPlay start — the CursorController is a
         * long-lived singleton, so leaving it set would let post-session touchpad gestures
         * inject into the torn-down DSI proxy (stale-DSI injection) and accumulate across
         * connect cycles. */
        CursorController.getInstance().setTouchSink(null);
        this.carPlayDsiController.deinit();
    }

    public ICarplayDSIController getDioDsiController() {
        return this.carPlayDsiController;
    }

    public ITerminalModeDSIKeyEventsController getKeyEventController() {
        return this.keyEventController;
    }

    protected Class getDSIServiceClass() {
        return org.dsi.ifc.carplay.DSICarplay.class;
    }

    protected DSIListener getDSIListener() {
        return this.dsiCarPlayListener;
    }

    protected Class getDSIListenerClass() {
        return org.dsi.ifc.carplay.DSICarplayListener.class;
    }

    protected void addDSIService(DSIBase dsibase) {
        this.dsiCarplayProxy.addDSIService((DSICarplay)dsibase);
    }

    protected void removeDSIService() {
        this.dsiCarplayProxy.removeDSIService();
    }

    public String[] getDiagKeys() {
        return new String[]{postButton, requestUI};
    }

    public void executeDiagCommand(String s, String[] astring) {
        if (postButton.equals(s) && astring.length > 0) {
            int i = Integer.parseInt(astring[0]);
            this.dsiCarplaySafe.postButtonEvent(i, 0);
            this.dsiCarplaySafe.postButtonEvent(i, 1);
        } else if (requestUI.equals(s)) {
            this.dsiCarplaySafe.requestUI(0);
        }
    }

    private class CarplayDSIController extends DefaultEventListener implements ICarplayDSIController {
        private static final String DSI_TEL_IDENTIFIER = "tel:";
        private static final String LOGCLASS = "CarplayDSIController";
        private final CarplayDSILifecycleController this$0;

        private CarplayDSIController(CarplayDSILifecycleController carplaydsilifecyclecontroller) {
            this.this$0 = carplaydsilifecyclecontroller;
        }

        public void responseUpdateMainAudioType(int i) {
            this.this$0.logger.log(1000000, "[%1.responseUpdateMainAudioType]", LOGCLASS);
            this.this$0.dsiCarplaySafe.responseUpdateMainAudioType(i);
        }

        public void deinit() {
            this.this$0.tmManager.getEventBus().unregisterListener(this);
        }

        public void init() {
            this.this$0.tmManager.getEventBus().registerListener(this);
        }

        public void startService(IDSIAppState[] aidsiappstate, IDSIResource[] aidsiresource) {
            this.this$0
                .logger
                .log(
                    1000000,
                    "[%1.startService] %2 %3",
                    LOGCLASS,
                    TerminalModeUtils.logAppStates(aidsiappstate),
                    TerminalModeUtils.logResources(aidsiresource)
                );
            ResourceRequest[] aresourcerequest = this.convertResource2DSIResourceRequest(aidsiresource, true);
            AppStateRequest[] aappstaterequest = this.convertAppState2DSIAppStateRequest(aidsiappstate);
            byte b0 = 0;
            if (this.this$0.configuration.hasKnob()) {
                b0 |= 1;
            }

            if (this.this$0.configuration.hasTouchpad()) {
                b0 |= 8;
            }

            if (this.this$0.configuration.hasTouchscreenHigh()) {
                b0 |= 4;
            }

            if (this.this$0.configuration.hasTouchscreenLow()) {
                b0 |= 2;
            }

            ServiceConfiguration serviceconfiguration = new ServiceConfiguration();
            serviceconfiguration.initialAppState = aappstaterequest;
            serviceconfiguration.initialResources = aresourcerequest;
            serviceconfiguration.screenResolution = this.this$0.configuration.getDSICarPlayScreenResolution();
            serviceconfiguration.xOffset = this.this$0.configuration.getScreenOffsetX();
            serviceconfiguration.yOffset = this.this$0.configuration.getScreenOffsetY();
            serviceconfiguration.displayName = this.this$0.configuration.getScreenName();
            serviceconfiguration.useRightHandDrive = this.this$0.configuration.isRightHandDrive();
            serviceconfiguration.touchpadXResolution = this.this$0.configuration.getTouchPadResolutionX();
            serviceconfiguration.touchpadYResolution = this.this$0.configuration.getTouchPadResolutionY();
            serviceconfiguration.bluetoothIdentities = new String[0];
            serviceconfiguration.startInNightMode = this.this$0
                .tmManager
                .getNightDayModeHandler()
                .getRequestedNightMode();
            serviceconfiguration.physicalDisplayHeight = this.this$0.configuration.getCarPlayPhysicalDisplayHeight();
            serviceconfiguration.physicalDisplayWidth = this.this$0.configuration.getCarPlayPhysicalDisplayWidth();
            serviceconfiguration.inputFeatures = b0;
            if (3 == this.this$0.configuration.getDSICarPlayScreenResolution()) {
                serviceconfiguration.xResolution = this.this$0.configuration.getWindowResolutionX();
                serviceconfiguration.yResolution = this.this$0.configuration.getWindowResolutionY();
            }

            this.this$0.dsiCarplaySafe.startService(serviceconfiguration);
            if (this.this$0.logger.isDebug2()) {
                this.this$0
                    .logger
                    .log(
                        100000000,
                        "[%1.startService] %3 -- %2",
                        LOGCLASS,
                        CarplayUtils.logModeRequest(aresourcerequest, aappstaterequest),
                        serviceconfiguration
                    );
            }
        }

        public void stopService() {
            this.this$0.logger.log(1000000, "[%1.stopService]", LOGCLASS);
        }

        public void postButtonEvent(int i, int j) {
            this.this$0.logger.log(1000000, "[%1.requestButtonEvent] %2 %3", LOGCLASS, i, j);
            this.this$0.dsiCarplaySafe.postButtonEvent(this.mapButtonId(i), this.mapButtonState(j));
        }

        public void postDSICarplayButtonEvent(int i, int j) {
            this.this$0.dsiCarplaySafe.postButtonEvent(i, j);
        }

        public void responseBTDeactivation() {
            this.this$0.logger.log(1000000, "[%1.responseBTDeactivation]", LOGCLASS);
            this.this$0.dsiCarplaySafe.responseBTDeactivation();
        }

        public void requestUI(int i) {
            this.this$0.logger.log(1000000, "[%1.requestUI]", LOGCLASS);
            this.this$0.dsiCarplaySafe.requestUI(this.mapUIId(i));
        }

        public void requestModeChange(IDSIAppState[] aidsiappstate, IDSIResource[] aidsiresource, String s) {
            this.this$0
                .logger
                .log(
                    1000000,
                    "[%1.requestModeChange] %2 %3",
                    LOGCLASS,
                    TerminalModeUtils.logAppStates(aidsiappstate),
                    TerminalModeUtils.logResources(aidsiresource)
                );
            ResourceRequest[] aresourcerequest = this.convertResource2DSIResourceRequest(aidsiresource, false);
            AppStateRequest[] aappstaterequest = this.convertAppState2DSIAppStateRequest(aidsiappstate);
            this.this$0.dsiCarplaySafe.requestModeChange(aresourcerequest, aappstaterequest, s);
            if (this.this$0.logger.isDebug2()) {
                this.this$0
                    .logger
                    .log(
                        100000000,
                        "[%1.requestModeChange] %2",
                        LOGCLASS,
                        CarplayUtils.logModeRequest(aresourcerequest, aappstaterequest)
                    );
            }
        }

        public void responseUpdateMode(IDSIResource[] aidsiresource, IDSIAppState[] aidsiappstate) {
            this.this$0
                .logger
                .log(
                    1000000,
                    "[%1.responseModeChange] %2 %3",
                    LOGCLASS,
                    TerminalModeUtils.logAppStates(aidsiappstate),
                    TerminalModeUtils.logResources(aidsiresource)
                );
            this.this$0
                .dsiCarplaySafe
                .responseUpdateMode(this.convertResource2DSI(aidsiresource), this.convertAppState2DSI(aidsiappstate));
        }

        public void requestSIRIAction(SiriAction siriaction) {
            this.this$0.logger.log(1000000, "[%1.requestSIRIAction] %2", LOGCLASS, siriaction);
            this.this$0.dsiCarplaySafe.requestSIRIAction(siriaction);
        }

        public void requestNightMode(boolean flag) {
            this.this$0.logger.log(1000000, "[%1.requestNightMode] %2", LOGCLASS, Boolean.toString(flag));
            this.this$0.dsiCarplaySafe.requestNightMode(flag);
        }

        private AppState[] convertAppState2DSI(IDSIAppState[] aidsiappstate) {
            this.this$0.logger.log(10000000, "[%1.convertAppState2DSI]", LOGCLASS);
            AppState[] aappstate = new AppState[aidsiappstate.length];

            for (int i = 0; i < aidsiappstate.length; i++) {
                aappstate[i] = this.this$0
                    .transferObjectFactory
                    .createAppState(
                        aidsiappstate[i].getDSIAppStateId(),
                        aidsiappstate[i].getDSIOwner(),
                        aidsiappstate[i].getDSISpeechMode()
                    );
            }

            return aappstate;
        }

        private AppStateRequest[] convertAppState2DSIAppStateRequest(IDSIAppState[] aidsiappstate) {
            this.this$0.logger.log(10000000, "[%1.convertAppState2DSI]", LOGCLASS);
            AppStateRequest[] aappstaterequest = new AppStateRequest[aidsiappstate.length];

            for (int i = 0; i < aidsiappstate.length; i++) {
                aappstaterequest[i] = this.this$0
                    .transferObjectFactory
                    .createAppStateRequest(
                        aidsiappstate[i].getDSIAppStateId(),
                        aidsiappstate[i].getOwner() == 1,
                        aidsiappstate[i].getDSISpeechMode()
                    );
            }

            return aappstaterequest;
        }

        private Resource[] convertResource2DSI(IDSIResource[] aidsiresource) {
            this.this$0.logger.log(10000000, "[%1.convertResource2DSI]", LOGCLASS);
            Resource[] aresource = new Resource[aidsiresource.length];

            for (int i = 0; i < aidsiresource.length; i++) {
                aresource[i] = this.this$0
                    .transferObjectFactory
                    .createResource(aidsiresource[i].getDSIResourceId(), aidsiresource[i].getDSIResourceOwner());
            }

            return aresource;
        }

        private ResourceRequest[] convertResource2DSIResourceRequest(IDSIResource[] aidsiresource, boolean flag) {
            this.this$0.logger.log(10000000, "[%1.convertResource2DSI]", LOGCLASS);
            ResourceRequest[] aresourcerequest = new ResourceRequest[aidsiresource.length];

            for (int i = 0; i < aidsiresource.length; i++) {
                int j = aidsiresource[i].getDSITakeType(flag);
                this.this$0
                    .logger
                    .log(
                        1000000,
                        "[%1.convertResource2DSIResourceRequest] %2 %3",
                        LOGCLASS,
                        aidsiresource[i].getResourceId(),
                        j
                    );
                aresourcerequest[i] = this.this$0
                    .transferObjectFactory
                    .createResourceRequest(
                        aidsiresource[i].getDSIResourceId(),
                        aidsiresource[i].getOwner() == 1 ? j : CarplayUtils.getInvertTransferType(j),
                        aidsiresource[i].getDSITransferPriority(flag),
                        aidsiresource[i].getDSITakeConstraint(flag),
                        aidsiresource[i].getDSIBorrowConstraint(flag),
                        aidsiresource[i].getDSIUnborrowConstraint(flag)
                    );
            }

            return aresourcerequest;
        }

        private int mapButtonState(int i) {
            switch (i) {
                case 1:
                    return 0;
                case 2:
                    return 1;
                default:
                    this.this$0.logger.log(100000, "[%1.mapButtonState] %2 unknown", LOGCLASS, i);
                    return 1;
            }
        }

        private int mapButtonId(int i) {
            switch (i) {
                case 1:
                    return 4;
                case 2:
                    return 9;
                case 3:
                    return 10;
                case 4:
                    return 11;
                case 5:
                    return 12;
                case 6:
                    return 19;
                case 7:
                    return 18;
                default:
                    this.this$0.logger.log(100000, "[%1.mapButtonId] %2 unknown", LOGCLASS, i);
                    return 0;
            }
        }

        private int mapUIId(int i) {
            if (1 == i) {
                return 1;
            }

            if (0 == i) {
                return 0;
            }

            this.this$0.logger.log(100000, "[%1.mapUIId] %2 unknown", LOGCLASS, i);
            return 0;
        }

        public void callNumber(String s) {
            Buffer buffer = new Buffer();
            buffer.append(DSI_TEL_IDENTIFIER);
            buffer.append(s);
            this.this$0.dsiCarplaySafe.requestUI2(buffer.toString());
        }

        public void endCall() {
            this.this$0.logger.log(1000000, "[%1.endCall] ending carplay call via BUTTON_END_CALL", LOGCLASS);
            this.this$0.dsiCarplaySafe.postButtonEvent(14, 0);
            this.this$0.dsiCarplaySafe.postButtonEvent(14, 1);
        }
    }

    public static class DSICarplayListenerImpl implements DSICarplayListener {
        private static final String LOGCLASS = "DSICarplayListenerImpl";
        private final LogChannel logger;
        private final DispatcherBase dispatcher;
        private final IEventBus eventBus;
        private final IContext context;
        private ICarplayDSIControllerListener dioDSIControllerListener;
        private volatile TrackPlayPositionEvent notifiedPlayPosition;
        private final PlayPositionLogger playPositionLogger;

        private IResource[] convertResource(Resource[] aresource) {
            IResource[] airesource = new IResource[aresource.length];

            for (int i = 0; i < aresource.length; i++) {
                airesource[i] = new CarPlayResource(aresource[i]);
            }

            this.logger.log(10000000, "[%1.convertResource] %2", LOGCLASS, Arrays2.toString(airesource));
            return airesource;
        }

        private IAppState[] convertAppState(AppState[] aappstate) {
            IAppState[] aiappstate = new IAppState[aappstate.length];
            boolean flag = false;

            for (int i = 0; i < aappstate.length; i++) {
                /* Narrow navignore seam: suppress only incoming CarPlay NAVI ownership (ID 2).
                 * Keep AppState itself stock so PHONE/SPEECH and outgoing DSI serialization retain
                 * their original semantics. TMState ignores appStateId 0. */
                if (aappstate[i] != null && aappstate[i].getAppStateID() == 2) {
                    aiappstate[i] = new CarPlayAppState(0, 0, 0);
                    continue;
                }
                if (aappstate[i].getAppStateID() == 1 && aappstate[i].getOwner() == 2) {
                    flag = true;
                }

                if (!flag || aappstate[i].getAppStateID() != 3) {
                    aiappstate[i] = new CarPlayAppState(aappstate[i]);
                }
            }

            this.logger.log(10000000, "[%1.convertAppState] %2", LOGCLASS, Arrays2.toString(aiappstate));
            return aiappstate;
        }

        public DSICarplayListenerImpl(
            LogChannel logchannel, DispatcherBase dispatcherbase, IEventBus ieventbus, IContext icontext
        ) {
            this.logger = logchannel;
            this.dispatcher = dispatcherbase;
            this.eventBus = ieventbus;
            this.context = icontext;
            this.notifiedPlayPosition = new TrackPlayPositionEvent(-1, -1);
            this.playPositionLogger = new PlayPositionLogger(
                new PlayPositionLoggerConfiguration()
                    .setPlayPositionLogEvent(
                        PlayPositionEvent.STARTUP, new PlayPositionLogEvent(logchannel, 1000000, 3)
                    )
                    .setPlayPositionLogEvent(
                        PlayPositionEvent.PLAYBACK_STATE_CHANGED, new PlayPositionLogEvent(logchannel, 1000000, 2)
                    )
                    .setPlayPositionLogEvent(
                        PlayPositionEvent.TRACK_CHANGED, new PlayPositionLogEvent(logchannel, 1000000, 3)
                    )
                    .setPlayPositionLogEvent(
                        PlayPositionEvent.ILLEGAL_PLAYPOSITION, new PlayPositionLogEvent(logchannel, 10000, 3)
                    )
                    .setPlayPositionLogEvent(
                        PlayPositionEvent.TO_LATE_PLAYPOSITION, new PlayPositionLogEvent(logchannel, 10000, 3)
                    )
            );
        }

        public void init() {
            this.dioDSIControllerListener = (ICarplayDSIControllerListener)this.context
                .get(
                    SmartphoneManager.SmartphoneType.CARPLAY,
                    ICarplayDSIControllerListener.class
                );
        }

        public void updateMode(Resource[] aresource, AppState[] aappstate, int i) {
            if (!this.isValid(i)) {
                this.logger.log(10000000, "<- [%1.updateMode] Invalid.", LOGCLASS);
            } else {
                final IAppState[] aiappstate = this.convertAppState(aappstate);
                final IResource[] airesource = this.convertResource(aresource);
                this.dispatcher
                    .execute(
                        new AbstractDispatcherRunnable("digitalIpodOutListener.updateMode") {
                            private final IAppState[] val$appStateList;
                            private final IResource[] val$resourceList;
                            private final CarplayDSILifecycleController.DSICarplayListenerImpl this$0;

                            {
                                this.this$0 = DSICarplayListenerImpl.this;
                                this.val$appStateList = aiappstate;
                                this.val$resourceList = airesource;
                            }

                            public void run() {
                                if (this.this$0.logger.isDebug()) {
                                    this.this$0
                                        .logger
                                        .log(
                                            1000000,
                                            "<- [%1.updateMode] %2 %3",
                                            "DSICarplayListenerImpl",
                                            TerminalModeUtils.logAppStates(this.val$appStateList),
                                            TerminalModeUtils.logResources(this.val$resourceList)
                                        );
                                }

                                this.this$0
                                    .dioDSIControllerListener
                                    .updateMode(this.val$appStateList, this.val$resourceList);
                            }
                        }
                    );
            }
        }

        public void responseModeChange(Resource[] aresource, AppState[] aappstate, int i) {
        }

        public void asyncException(int i, String s, int j) {
            this.logger
                .log(
                    10000,
                    "<- [%1.asyncException] errCode='%2' msg='%3' requestType='%4'",
                    LOGCLASS,
                    String.valueOf(i),
                    String.valueOf(s),
                    String.valueOf(j)
                );
        }

        public void requestBTDeactivation(String s, int i) {
        }

        public void updateTextInputState(final int i, int j) {
            if (!this.isValid(j)) {
                this.logger.log(10000000, "<- [%1.updateTextInputState] Invalid.", LOGCLASS);
            } else {
                this.dispatcher
                    .execute(
                        new AbstractDispatcherRunnable("digitalIpodOutListener.updateTextInputState") {
                            private final int val$state;
                            private final CarplayDSILifecycleController.DSICarplayListenerImpl this$0;

                            {
                                this.this$0 = DSICarplayListenerImpl.this;
                                this.val$state = i;
                            }

                            public void run() {
                                this.this$0
                                    .logger
                                    .log(
                                        1000000,
                                        "<- [%1.updateTextInputState] %2",
                                        "DSICarplayListenerImpl",
                                        this.val$state
                                    );
                                this.this$0.dioDSIControllerListener.updateTextInputState(1 == this.val$state);
                            }
                        }
                    );
            }
        }

        public void updateDeviceInfo(DeviceInfo deviceinfo, int i) {
        }

        public void updateTelephonyState(TelephonyState telephonystate, int i) {
            if (this.isValid(i)) {
                PhoneStateChangedEvent phonestatechangedevent = new PhoneStateChangedEvent(
                    telephonystate.getSignalStrength(),
                    telephonystate.getRegistrationStatus(),
                    telephonystate.isAirplaneMode(),
                    telephonystate.getMobileOperator()
                );
                this.logger.log(1000000, "<- [%1.updateTelephonyState] %2", LOGCLASS, phonestatechangedevent);
                this.eventBus.updatePhoneState(phonestatechangedevent);
            }
        }

        public void updateNowPlayingData(TrackData trackdata, int i) {
            if (this.isValid(i)) {
                TrackDataChangedEvent trackdatachangedevent = new TrackDataChangedEvent.Builder()
                    .setAlbum(trackdata.getAlbum())
                    .setArtist(trackdata.getArtist())
                    .setComposer(trackdata.getComposer())
                    .setDuration(trackdata.getDuration())
                    .setGenre(trackdata.getGenre())
                    .setTitle(trackdata.getTitle())
                    .build();
                this.logger.log(1000000, "<- [%1.updateNowPlayingData] %2", LOGCLASS, trackdatachangedevent);
                this.playPositionLogger.trackChanged();
                this.eventBus.updateNowPlayingData(trackdatachangedevent);
                this.notifyPlayPosition(new TrackPlayPositionEvent(0, trackdata.getDuration() / 1000));
            }
        }

        private void notifyPlayPosition(TrackPlayPositionEvent trackplaypositionevent) {
            if (!trackplaypositionevent.equals(this.notifiedPlayPosition)) {
                this.notifiedPlayPosition = trackplaypositionevent;
                this.eventBus.updatePlayPosition(trackplaypositionevent);
            }
        }

        public void updatePlaybackState(PlaybackInfo playbackinfo, int i) {
            if (this.isValid(i)) {
                PlaybackInfoChangedEvent playbackinfochangedevent = new PlaybackInfoChangedEvent.Builder()
                    .setPlaybackState(this.convert(playbackinfo.getStatus()))
                    .build();
                this.logger.log(1000000, "<- [%1.updatePlaybackState] %2", LOGCLASS, playbackinfochangedevent);
                this.playPositionLogger.playbackStateChanged();
                this.eventBus.updatePlaybackInfo(playbackinfochangedevent);
            }
        }

        private PlaybackInfoChangedEvent.PlaybackState convert(int i) {
            switch (i) {
                case 0:
                default:
                    return PlaybackInfoChangedEvent.PlaybackState.STOPPED;
                case 1:
                    return PlaybackInfoChangedEvent.PlaybackState.PLAYING;
                case 2:
                    return PlaybackInfoChangedEvent.PlaybackState.PAUSED;
                case 3:
                    return PlaybackInfoChangedEvent.PlaybackState.SEEKBACKWARD;
                case 4:
                    return PlaybackInfoChangedEvent.PlaybackState.SEEKBACKWARD;
            }
        }

        public void updatePlayposition(int i, int j) {
            if (this.isValid(j)) {
                this.playPositionLogger.updatePlayPosition(i / 1000, this.notifiedPlayPosition.getTotalTimeOfTrack());
                this.notifyPlayPosition(
                    new TrackPlayPositionEvent(i / 1000, this.notifiedPlayPosition.getTotalTimeOfTrack())
                );
            }
        }

        public void updateCoverArtUrl(ResourceLocator resourcelocator, int i) {
        }

        public void updateCallState(final CallState[] acallstate, int i) {
            if (!this.isValid(i)) {
                this.logger.log(10000000, "<- [%1.updateCallState] Invalid.", LOGCLASS);
            } else {
                this.dispatcher
                    .execute(
                        new AbstractDispatcherRunnable("digitalIpodOutListener.updateCallState") {
                            private final CallState[] val$callState;
                            private final CarplayDSILifecycleController.DSICarplayListenerImpl this$0;

                            {
                                this.this$0 = DSICarplayListenerImpl.this;
                                this.val$callState = acallstate;
                            }

                            public void run() {
                                GList glist = Generics.newArrayListWithCapacity(this.val$callState.length);

                                for (int j = 0; j < this.val$callState.length; j++) {
                                    glist.add(new CallStateChangedEvent(this.val$callState[j]));
                                }

                                this.this$0.logger.log(1000000, "<- [%1.updateCallState]", "DSICarplayListenerImpl");
                                this.this$0.eventBus.updateCallState(glist);
                            }
                        }
                    );
            }
        }

        public void duckAudio(final int i, final double d0) {
            this.dispatcher.execute(new AbstractDispatcherRunnable("digitalIpodOutListener.duckAudio") {
                private final int val$duration;
                private final double val$volume;
                private final CarplayDSILifecycleController.DSICarplayListenerImpl this$0;

                {
                    this.this$0 = DSICarplayListenerImpl.this;
                    this.val$duration = i;
                    this.val$volume = d0;
                }

                public void run() {
                    this.this$0.logger.log(1000000, "<- [%1.duckAudio]", "DSICarplayListenerImpl");
                    this.this$0.dioDSIControllerListener.duckAudio(this.val$duration, this.val$volume);
                }
            });
        }

        public void unduckAudio(final int i) {
            this.dispatcher
                .execute(
                    new AbstractDispatcherRunnable("digitalIpodOutListener.unduckAudio") {
                        private final int val$duration;
                        private final CarplayDSILifecycleController.DSICarplayListenerImpl this$0;

                        {
                            this.this$0 = DSICarplayListenerImpl.this;
                            this.val$duration = i;
                        }

                        public void run() {
                            this.this$0
                                .logger
                                .log(1000000, "<- [%1.unduckAudio] %2", "DSICarplayListenerImpl", this.val$duration);
                            this.this$0.dioDSIControllerListener.unduckAudio(this.val$duration);
                        }
                    }
                );
        }

        public void oemAppSelected() {
            this.dispatcher.execute(new AbstractDispatcherRunnable("digitalIpodOutListener.oemAppSelected") {
                private final CarplayDSILifecycleController.DSICarplayListenerImpl this$0;

                {
                    this.this$0 = DSICarplayListenerImpl.this;
                }

                public void run() {
                    this.this$0.logger.log(1000000, "<- [%1.oemAppSelected]", "DSICarplayListenerImpl");
                    this.this$0.dioDSIControllerListener.oemAppSelected();
                }
            });
        }

        public void updateMainAudioType(final int i, int j) {
            if (!this.isValid(j)) {
                this.logger.log(10000000, "<- [%1.updateMainAudioType] Invalid.", LOGCLASS);
            } else {
                this.dispatcher
                    .execute(
                        new AbstractDispatcherRunnable("digitalIpodOutListener.updateMainAudioType") {
                            private final int val$audioType;
                            private final CarplayDSILifecycleController.DSICarplayListenerImpl this$0;

                            {
                                this.this$0 = DSICarplayListenerImpl.this;
                                this.val$audioType = i;
                            }

                            public void run() {
                                this.this$0
                                    .logger
                                    .log(
                                        1000000,
                                        "<- [%1.updateMainAudioType] %2",
                                        "DSICarplayListenerImpl",
                                        this.val$audioType
                                    );
                                this.this$0.dioDSIControllerListener.updateMainAudioType(this.val$audioType);
                            }
                        }
                    );
            }
        }

        protected boolean isValid(int i) {
            return 1 == i;
        }
    }

    private class TerminalModeDSIKeyEventsController implements ITerminalModeDSIKeyEventsController {
        private static final String LOGCLASS = "TerminalModeDSIKeyEventsController";
        private final CarplayDSILifecycleController this$0;

        private TerminalModeDSIKeyEventsController(CarplayDSILifecycleController carplaydsilifecyclecontroller) {
            this.this$0 = carplaydsilifecyclecontroller;
            installCursorTouchSink();   /* CarPlay: touchpad -> DPAD ticks via CursorController */
        }

        /* Wire CursorController's DPAD output back through the stock DSI bridge's
         * postButtonEvent (JS_WEST=5 / JS_EAST=6 / JS_NORTH=7 / JS_SOUTH=8;
         * state 0=press, 1=release) — CarPlay's iOS side moves focus accordingly. */
        private void installCursorTouchSink() {
            final CarplayDSILifecycleController outer = this.this$0;
            CursorController.getInstance().setTouchSink(new CursorController.TouchSink() {
                public void postDpad(int keyCode) {
                    int id;
                    switch (keyCode) {
                        case CursorController.KEY_DPAD_LEFT:  id = 5; break;
                        case CursorController.KEY_DPAD_RIGHT: id = 6; break;
                        case CursorController.KEY_DPAD_UP:    id = 7; break;
                        case CursorController.KEY_DPAD_DOWN:  id = 8; break;
                        default: return;
                    }
                    try {
                        if (outer.dsiCarplaySafe != null) {
                            outer.dsiCarplaySafe.postButtonEvent(id, 0);
                            outer.dsiCarplaySafe.postButtonEvent(id, 1);
                        }
                    } catch (Throwable t) { /* never break the DSI flow */ }
                }
            });
        }

        public void updateKey(Key key, KeyState keystate) {
            /* Raw key 40 (left MFW roller) and raw key 16 (centre DDS) both become
             * DDS_SELECT in the stock keyboard stack.  The raw listener marks only
             * key 40, so consume that copy here before it reaches iOS.  This applies
             * on every VC tab: on MAP the press drives cluster route-info; elsewhere
             * it remains a VC-only key.  The centre-console knob is never marked. */
            if (Key.DDS_SELECT.is(key)
                    && SteeringWheelInputModule.consumeCollapsedSelect(keystate)) {
                this.this$0.logger.log(
                    1000000, "[%1.updateKey] suppress MFW DDS_SELECT for CarPlay", LOGCLASS);
                return;
            }

            /* Central-console knob press (DDS_SELECT = Dreh-Drück-Steller) must SELECT in the
             * CarPlay Main UI — it falls through to postButtonEvent below.  An earlier build
             * redirected DDS_SELECT to a cluster virtual HID and returned, swallowing the
             * select → "knob press does nothing". Cluster map zoom is driven separately by the
             * steering-wheel MapScale callback, never by this central-console select. */
            if (TerminalModeUtils.isJoystickMiddleposition(key)) {
                if (null != this.this$0.lastJoystickkey) {
                    this.this$0
                        .logger
                        .log(
                            1000000,
                            "[%1.updateKey] joystick middle position, release button %2",
                            LOGCLASS,
                            this.this$0.lastJoystickkey
                        );
                    this.this$0.dsiCarplaySafe.postButtonEvent(this.getKeyId(this.this$0.lastJoystickkey), 1);
                    this.this$0.lastJoystickkey = null;
                }
            } else {
                int i = this.getKeyId(key);
                if (0 == i) {
                    this.this$0.logger.log(1000000, "[%1.updateKey] unknown key id", LOGCLASS);
                } else {
                    int j = this.getKeyState(keystate);
                    this.this$0.logger.log(1000000, "[%1.updateKey] %2 %3", LOGCLASS, String.valueOf(i), j);
                    if (TerminalModeUtils.isJoystick(key)) {
                        this.this$0.lastJoystickkey = key;
                    }

                    this.this$0.dsiCarplaySafe.postButtonEvent(i, j);
                }
            }
        }

        public void updateTouchEvent(int i, int j, int l1, int k, int l, int i1, int j1) {
            this.this$0
                .logger
                .log(
                    1000000,
                    "[%1.updateTouchEvent] c=%2 x=%3 y=%4",
                    LOGCLASS,
                    String.valueOf(j),
                    String.valueOf(k),
                    String.valueOf(l)
                );
            TouchEvent[] atouchevent = new TouchEvent[j];
            if (j >= 1) {
                atouchevent[0] = new TouchEvent(k, l);
            }

            if (j >= 2) {
                int[] aint = this.calcSecondCorr(k, l, i1, j1);
                atouchevent[1] = new TouchEvent(aint[0], aint[1]);
            }

            TouchEvent[] atouchevent1;
            if (null != this.this$0.lastFingers && this.this$0.lastFingers.length > atouchevent.length) {
                atouchevent1 = new TouchEvent[this.this$0.lastFingers.length];

                for (int k1 = 0; k1 < this.this$0.lastFingers.length; k1++) {
                    if (k1 < atouchevent.length - 1) {
                        atouchevent1[k1] = atouchevent[k1];
                    } else {
                        atouchevent1[k1] = this.this$0.lastFingers[k1];
                    }
                }
            } else {
                atouchevent1 = atouchevent;
            }

            if (this.this$0.logger.isDebug()) {
                if (j == 1) {
                    this.this$0
                        .logger
                        .log(
                            1000000,
                            "[%1.updateTouchEvent] %2/%3",
                            LOGCLASS,
                            atouchevent1[0].getX(),
                            atouchevent1[0].getY()
                        );
                } else if (j >= 2) {
                    Buffer buffer = new Buffer(50);
                    buffer.append(atouchevent1[0].getX());
                    buffer.append('/');
                    buffer.append(atouchevent1[0].getY());
                    buffer.append(' ');
                    buffer.append(atouchevent1[1].getX());
                    buffer.append('/');
                    buffer.append(atouchevent1[1].getY());
                    this.this$0.logger.log(1000000, "[%1.updateTouchEvent] %2", LOGCLASS, buffer.toString());
                }
            }

            if (0 != j) {
                this.this$0.lastFingers = atouchevent1;
            } else {
                this.this$0.lastFingers = null;
            }

            this.this$0.dsiCarplaySafe.postTouchEvent(this.getDSITouchInputId(i), j, atouchevent1);
        }

        public void updateTouchEvents(de.audi.app.terminalmode.keyevents.TouchEvent[] atouchevent) {
            /* CarPlay input override: real touchscreen events follow the stock conversion/sort
             * path; MMI touchpad single-finger events route through CursorController as DPAD
             * ticks. Partition by each event instead of trusting element 0, so a mixed batch can
             * never inject touchpad coordinates into the DSI touchscreen stream (or vice versa).
             * Active-finger count uses getTouchState()!=1 (1 = RELEASED). */
            CursorController c = CursorController.getInstance();
            if (atouchevent == null || atouchevent.length == 0) { c.onTouchEnd(); return; }

            ArrayList screen = new ArrayList(atouchevent.length);
            int screenActive = 0;
            int padCount = 0, padActive = 0, padFirstActive = -1;
            for (int i = 0; i < atouchevent.length; i++) {
                if (atouchevent[i].isTouchScreen()) {
                    screen.add(new TouchEvent(
                        atouchevent[i].getCurrentX() - this.this$0.configuration.getScreenOffsetX(),
                        atouchevent[i].getCurrentY() - this.this$0.configuration.getScreenOffsetY()));
                    if (atouchevent[i].getTouchState() != 1) screenActive++;
                } else {
                    padCount++;
                    if (atouchevent[i].getTouchState() != 1) {
                        if (padActive == 0) padFirstActive = i;
                        padActive++;
                    }
                }
            }
            if (!screen.isEmpty()) {
                Collections.sort(screen, new TouchXComparator());
                this.this$0.dsiCarplaySafe.postTouchEvent(
                    1, screenActive, (TouchEvent[])screen.toArray(new TouchEvent[screen.size()]));
            }
            if (padCount > 0 && padActive == 1) {
                c.onOneFinger(atouchevent[padFirstActive].getCurrentX(),
                    atouchevent[padFirstActive].getCurrentY());
            } else {
                c.onTouchEnd();
            }
        }

        /** Stock updateTouchEvents orders fingers left-to-right before DSI serialization. */
        private final class TouchXComparator implements Comparator {
            public int compare(Object a, Object b) {
                return ((TouchEvent)a).getX() - ((TouchEvent)b).getX();
            }
        }

        public void updateRotary(int i) {
            this.this$0.logger.log(1000000, "[%1.updateRotary] %2", LOGCLASS, i);
            /* updateRotary = the CENTRAL console MMI knob → must drive the CarPlay Main UI
             * (next/prev). The steering-wheel MapScale BAP callback is a separate seam (it
             * scales the stock native cluster map) and must never use this one. */
            this.this$0.dsiCarplaySafe.postRotaryEvent(i);
        }

        private int getKeyState(KeyState keystate) {
            if (keystate.is(KeyState.PRESSED)) {
                return 0;
            }

            if (keystate.is(KeyState.RELEASED)) {
                return 1;
            }

            this.this$0.logger.log(1000000, "[%1.getKeyState] Unsupported key state %2", LOGCLASS, keystate);
            return -1;
        }

        private int getKeyId(Key key) {
            if (Key.BACK.is(key)) {
                return 3;
            }

            if (Key.DDS_SELECT.is(key)) {
                return 1;
            }

            if (key.isOneOf(Key.JS_NORTH, Key.SOFTKEY_SOUTHWEST)) {
                return 7;
            }

            if (key.isOneOf(Key.JS_EAST, Key.SOFTKEY_NORTHEAST)) {
                return 6;
            }

            if (key.isOneOf(Key.JS_SOUTH, Key.SOFTKEY_SOUTHEAST)) {
                return 8;
            }

            if (key.isOneOf(Key.JS_WEST, Key.SOFTKEY_NORTHWEST)) {
                return 5;
            }

            if (key.is(Key.SOFTKEY_EAST)) {
                return 17;
            }

            if (key.is(Key.SOFTKEY_WEST)) {
                return 16;
            }

            this.this$0.logger.log(1000000, "[%1.getKeyId] Unsupport %2", LOGCLASS, key);
            return 0;
        }

        private int getDSITouchInputId(int i) {
            switch (i) {
                case 0:
                case 1:
                default:
                    return 0;
                case 2:
                    return 1;
            }
        }

        private int[] calcSecondCorr(int i, int j, int j1, int k) {
            if (k > 100) {
                k -= 100;
            }

            int l = (int)(i + Math.acos(k / 100) * k);
            int i1 = (int)(j + Math.asin(k / 100) * k);
            return new int[]{l, i1};
        }

        public void updateCharacterEvent(String[] astring, int[] aint) {
            this.this$0.logger.log(1000000, "[%1.updateCharacterEvent]", LOGCLASS);
            this.this$0.dsiCarplaySafe.postCharacterEvent(astring.length, astring);
        }
    }
}
