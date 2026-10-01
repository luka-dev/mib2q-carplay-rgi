/*
 * MU0918 stock ExternalEventsListener, rebuilt from the lsd.jar decompile (constants restored from
 * lsd.jxe), plus luka-dev's partial-OPS exception, ported to Android Auto for the SQ5:
 *
 *   Stock: parking popup 2100008 -> native view size SMALL -> MMICombiStageListener ->
 *   updateKombiStage(false) -> HMIDeactivated gives Resource.SCREEN to MAINUNIT (the projection
 *   disappears); the parking SMALL_STAGE can also publish AP 1002 (AP_METHOD_HMI_DEACTIVATED).
 *
 *   Here: only while PdcSmallStageGuard reports the pure right-hand OPS popup beside a foreground
 *   Android Auto screen (and SCREEN is not access-restricted by eCall/clamp S), SMALL_STAGE and the
 *   matching AP 1002 keep SCREEN with the device; input is deactivated while the parking popup is
 *   up and restored at BIG_STAGE (stock hmiActivated).  HOME, fullscreen camera/VPS, ARA, active
 *   PLA, CarPlay and every unguarded case take the unchanged MU0918 path.
 *
 * The debounced value is an ActionProxyCall (id + guard evidence captured at AP time) instead of
 * an Integer; equals/hashCode follow the id so the property behaves exactly like the stock Integer.
 */
package de.audi.app.terminalmode;

import com.sq5.aa.luka.AaLog;
import com.sq5.aa.pdc.PdcSmallStageGuard;
import de.audi.app.terminalmode.actionproxy.IActionProxyListener;
import de.audi.app.terminalmode.audio.AudioConnectionState;
import de.audi.app.terminalmode.audio.IAudioStateListener;
import de.audi.app.terminalmode.commands.AudioFocusChanged;
import de.audi.app.terminalmode.commands.HMIActivated;
import de.audi.app.terminalmode.commands.HMIDeactivated;
import de.audi.app.terminalmode.commands.NavigationStateChanged;
import de.audi.app.terminalmode.commands.PhoneStateUpdate;
import de.audi.app.terminalmode.events.DefaultEventListener;
import de.audi.app.terminalmode.events.IEventBus;
import de.audi.app.terminalmode.statemachine.IStateHandler;
import de.audi.app.terminalmode.statemachine.Resource;
import de.audi.app.terminalmode.statemachine.commands.AbstractCommand;
import de.audi.atip.hmi.HMIService;
import de.audi.atip.interapp.phone.ITelState;
import de.audi.atip.interapp.phone.ITelStateListener;
import de.audi.atip.log.LogChannel;
import de.audi.atip.utils.dispatching.IDispatcher;
import de.audi.atip.utils.generics.Consumer;
import de.audi.atip.utils.reactive.properties.LoggingPropertyFactory;
import de.audi.atip.utils.reactive.properties.Property;
import de.audi.tghu.command.CommandList;
import java.util.Hashtable;
import java.util.Map;
import org.osgi.framework.ServiceRegistration;

public class ExternalEventsListener
extends DefaultEventListener
implements IActionProxyListener,
IAudioStateListener,
INavigationStateListener,
ITerminalModeComponent {
    public static final int ACTION_PROXY_DEBOUNCE_TIME = 70;
    private static final String LOGCLASS = "ExternalEventsListener";
    private final LogChannel logger;
    private final IContext context;
    private volatile ServiceRegistration phoneServiceRegistration;
    private final IStateHandler stateHandler;
    private final IEventBus eventBus;
    private volatile boolean bigStage;
    private volatile boolean screenActive;
    private final IDispatcher dispatcher;
    private final Property lastActionProxyCall;

    /* SQ5 PDC: one action-proxy call plus the guard evidence at the time it arrived. */
    private static final class ActionProxyCall {
        final int id;
        final PdcSmallStageGuard.ParkingTransition parking;

        ActionProxyCall(int id) {
            this.id = id;
            PdcSmallStageGuard.ParkingTransition p = null;
            if (id == 1002) {
                try {
                    p = PdcSmallStageGuard.captureHmiDeactivation();
                } catch (Throwable t) {
                    p = null;
                }
            }
            this.parking = p;
        }

        public boolean equals(Object o) {
            return o instanceof ActionProxyCall && ((ActionProxyCall)o).id == this.id;
        }

        public int hashCode() {
            return this.id;
        }

        public String toString() {
            return String.valueOf(this.id);
        }
    }

    public ExternalEventsListener(IContext iContext, IStateHandler iStateHandler, IDispatcher iDispatcher) {
        this.logger = iContext.getLogger().main();
        this.context = iContext;
        this.stateHandler = iStateHandler;
        this.eventBus = iContext.getEventBus();
        this.dispatcher = iDispatcher;
        this.lastActionProxyCall = LoggingPropertyFactory.create().createProperty("lastActionProxyCall");
    }

    public void init() {
        this.logger.log(100000000, "[%1.init]", LOGCLASS);
        this.bigStage = true;
        this.screenActive = false;
        PdcSmallStageGuard.attachTerminalMode(this.context);
        this.context.addActionProxyListener(1001, this);
        this.context.addActionProxyListener(1002, this);
        this.eventBus.registerListener(this);
        this.context.getChoiceModel(3200035).setValue(78);
        this.phoneServiceRegistration = this.context.getServiceManager().registerService(ITelStateListener.class, new ITelStateListener() {

            public void updateTelState(int n, ITelState iTelState) {
                ExternalEventsListener.this.logger.log(1000000, "<- [%1.updateTelState]", LOGCLASS);
                ExternalEventsListener.this.context.getCommandListHelper().create().addSingle(new PhoneStateUpdate(ExternalEventsListener.this.context, iTelState.getCallActive(), ExternalEventsListener.this.stateHandler)).execute("ExternalEventsListener.updateTelState");
            }
        }, new Hashtable(0));
        this.context.getNaviAppHandler().setNavigationStateListener(this);
        this.context.getAudioManager().addAudioContextListener(this);
        this.lastActionProxyCall.observe().log(this.logger, "beforeDebounce").debounce(70, this.dispatcher).log(this.logger, "afterDebounce").redirectTo(new Consumer() {

            public void accept(Object object) {
                ExternalEventsListener.this.actionProxyDebounced((ActionProxyCall)object);
            }
        });
    }

    private void actionProxyDebounced(ActionProxyCall call) {
        switch (call.id) {
            case 1001: {
                this.logger.log(1000000, "<<- [%1.actionProxyCallPerformed] AP_METHOD_HMI_ACTIVATED", LOGCLASS);
                if (this.isScreenAvailable()) {
                    try {
                        PdcSmallStageGuard.projectionScreenActivated(this.getHMIService());
                    } catch (Throwable t) {
                        /* stock path */
                    }
                }
                if (this.bigStage) {
                    this.hmiActivated();
                } else if (this.keepScreenForParking()) {
                    /* SQ5 PDC: entering Android Auto while the side OPS is already small still
                     * needs SCREEN; input stays with the parking popup until BIG_STAGE. */
                    this.context.getKeyEventsHandler().deactivate();
                    this.activateScreen();
                }
                this.screenActive = true;
                break;
            }
            case 1002: {
                this.logger.log(1000000, "<<- [%1.actionProxyCallPerformed] AP_METHOD_HMI_DEACTIVATED", LOGCLASS);
                if (this.screenActive && this.isParkingHmiDeactivation(call)) {
                    /* SQ5 PDC: the parking SMALL_STAGE also publishes AP 1002; treat it as part of
                     * the same partial-OPS transition.  If OPS already closed while this AP was
                     * debouncing, BIG_STAGE restored input: do not turn it off again. */
                    this.logger.log(1000000, "[%1.actionProxyCallPerformed] partial OPS: suppress HMI_DEACTIVATED", LOGCLASS);
                    AaLog.log("PDC AP 1002 during side OPS: Android Auto keeps SCREEN");
                    if (!this.bigStage || this.keepScreenForParking()) {
                        this.context.getKeyEventsHandler().deactivate();
                    }
                } else {
                    this.hmiDeactivated();
                    this.screenActive = false;
                }
                break;
            }
        }
    }

    public void deinit() {
        this.logger.log(1000000, "[%1.deinit]", LOGCLASS);
        PdcSmallStageGuard.detachTerminalMode();
        this.context.removeActionProxyListener(this);
        this.eventBus.unregisterListener(this);
        if (null != this.phoneServiceRegistration) {
            this.context.getServiceManager().unregisterService(this.phoneServiceRegistration);
        }
        this.context.getNaviAppHandler().setNavigationStateListener(null);
    }

    public void actionProxyCallPerformed(int n, Map map) {
        this.lastActionProxyCall.accept(new ActionProxyCall(n));
    }

    private void hmiDeactivated() {
        /* SQ5 PDC: SMALL_STAGE can precede AP 1001 when returning to Android Auto with OPS already
         * open; keep that valid guard, revoke it for a real takeover. */
        if (!this.keepScreenForParking()) {
            try {
                PdcSmallStageGuard.reset();
            } catch (Throwable t) {
                /* stock path */
            }
        }
        this.context.getKeyEventsHandler().deactivate();
        this.context.getCommandListHelper().create().addSingle(new HMIDeactivated(this.context, this.stateHandler)).execute("ExternalEventsListener.AP_METHOD_HMI_DEACTIVATED");
    }

    private void hmiActivated() {
        this.context.getKeyEventsHandler().activate();
        this.activateScreen();
    }

    private void activateScreen() {
        this.context.getCommandListHelper().create().addSingle(new HMIActivated(this.context, this.stateHandler)).execute("ExternalEventsListener.AP_METHOD_HMI_ACTIVATED");
    }

    /* SQ5 PDC helpers: any failure answers "stock". */
    private boolean isScreenAvailable() {
        try {
            return PdcSmallStageGuard.isProjectionActive()
                && !this.stateHandler.getCurrentState().isAccessRestricted(Resource.SCREEN);
        } catch (Throwable t) {
            return false;
        }
    }

    private boolean keepScreenForParking() {
        try {
            return this.isScreenAvailable() && PdcSmallStageGuard.shouldKeepProjectionScreen();
        } catch (Throwable t) {
            return false;
        }
    }

    private boolean isParkingHmiDeactivation(ActionProxyCall call) {
        try {
            return this.isScreenAvailable() && PdcSmallStageGuard.isParkingHmiDeactivation(call.parking);
        } catch (Throwable t) {
            return false;
        }
    }

    private HMIService getHMIService() {
        return this.context.getFramework().getHMIService();
    }

    public void audioStateChanged(AudioConnectionState audioConnectionState) {
        this.logger.log(100000000, "[%1.audioStateChanged] '%2'", LOGCLASS, audioConnectionState);
    }

    public void audioFocusChanged(boolean bl) {
        SmartphoneManager.SmartphoneType smartphoneType = this.context.getSmartphoneDSIManager().getSmartphoneType();
        this.logger.log(1000000, "<- [%1.audioFocusChanged] %2 %3", LOGCLASS, String.valueOf(bl), smartphoneType);
        this.context.getChoiceModel(4451).setValue(bl && smartphoneType.isNot(SmartphoneManager.SmartphoneType.UNKNOWN) ? TerminalModeUtils.mapActiveSmartphoneTypeToEntertainmentAudioModelType(smartphoneType) : 0);
        AbstractCommand abstractCommand = this.getCurrentCommand();
        if (null == abstractCommand || !abstractCommand.updateAudioFocus(bl)) {
            this.context.getCommandListHelper().create().addSingle(new AudioFocusChanged(this.context, bl, this.stateHandler)).execute("ExternalEventsListener.audioFocusChanged]");
        }
    }

    public void stateChanged(boolean bl) {
        this.logger.log(1000000, "<- [%1.stateChanged]", LOGCLASS);
        this.context.getCommandListHelper().create().addSingle(new NavigationStateChanged(this.context, bl, this.stateHandler)).execute("ExternalEventsListener.stateChanged]");
    }

    private AbstractCommand getCurrentCommand() {
        AbstractCommand abstractCommand = null;
        CommandList commandList = this.context.getCommandListManager().getActiveCommandList();
        if (null != commandList) {
            abstractCommand = (AbstractCommand)commandList.getActiveCommand();
        }
        return abstractCommand;
    }

    public void updateKombiStage(boolean bl) {
        if (bl && this.screenActive) {
            this.hmiActivated();
        } else if (!bl && this.screenActive && this.keepScreenForParking()) {
            /* SQ5 PDC: keep SCREEN owned by the device so the right OPS panel overlays Android
             * Auto instead of minimizing it; input returns at BIG_STAGE (hmiActivated). */
            this.logger.log(1000000, "[%1.updateKombiStage] SMALL_STAGE pure OPS: keep projection SCREEN owner", LOGCLASS);
            AaLog.log("PDC SMALL_STAGE pure OPS: Android Auto keeps SCREEN");
            this.context.getKeyEventsHandler().deactivate();
        } else {
            this.hmiDeactivated();
        }
        this.bigStage = bl;
    }
}
