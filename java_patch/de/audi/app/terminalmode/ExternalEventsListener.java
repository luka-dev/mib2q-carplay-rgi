package de.audi.app.terminalmode;

import com.luka.carplay.core.CarPlayApp;
import com.luka.carplay.pdc.PdcSmallStageGuard;
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
import de.audi.app.terminalmode.smartphone.IDSISmartphoneManager;
import de.audi.app.terminalmode.statemachine.IStateHandler;
import de.audi.app.terminalmode.statemachine.Resource;
import de.audi.app.terminalmode.statemachine.commands.AbstractCommand;
import de.audi.atip.hmi.HMIService;
import de.audi.atip.hmi.event.DrawerEvent;
import de.audi.atip.interapp.phone.ITelState;
import de.audi.atip.interapp.phone.ITelStateListener;
import de.audi.atip.log.LogChannel;
import de.audi.atip.model.ICoreSystemModelBank;
import de.audi.atip.model.ICoreTerminalModeModelBank;
import de.audi.atip.utils.dispatching.IDispatcher;
import de.audi.atip.utils.generics.Consumer;
import de.audi.atip.utils.reactive.properties.LoggingPropertyFactory;
import de.audi.atip.utils.reactive.properties.Property;
import de.audi.atip.utils.reactive.observables.Subscription;
import de.audi.tghu.command.CommandList;
import java.util.Hashtable;
import java.util.Map;
import org.osgi.framework.ServiceRegistration;

/**
 * Stock MU1316 listener with a parking-specific partial-OPS exception.
 *
 * Only CarPlay + pure OPS popup 2100008 are exempted on SMALL_STAGE/AP 1002.
 * A queued parking AP is revalidated after debounce, including normal close.
 * HOME, another HMI component, fullscreen RVC/VPS and disconnect still follow
 * the original stock path when that narrow popup guard is not active.
 */
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
    private Subscription actionProxySubscription;
    private boolean initialized;
    private int lifecycleGeneration;
    private HMIService hmiService;
    private IDSISmartphoneManager.ISmartphoneProperties smartphoneProperties;

    private static final class ActionProxyCall {
        final int id;
        final PdcSmallStageGuard.ParkingTransition parking;

        ActionProxyCall(int id) {
            this.id = id;
            this.parking = id == 1002 ? PdcSmallStageGuard.captureHmiDeactivation() : null;
        }

        public String toString() { return String.valueOf(id); }
    }

    public ExternalEventsListener(
        IContext context,
        IStateHandler stateHandler,
        IDispatcher dispatcher,
        IDSISmartphoneManager.ISmartphoneProperties smartphoneProperties
    ) {
        this.logger = context.getLogger().main();
        this.context = context;
        this.hmiService = context.getFramework().getHMIService();
        this.stateHandler = stateHandler;
        this.smartphoneProperties = smartphoneProperties;
        this.eventBus = context.getEventBus();
        this.dispatcher = dispatcher;
        this.lastActionProxyCall = LoggingPropertyFactory.create().createProperty("lastActionProxyCall");
    }

    public synchronized void init() {
        if (this.initialized) return;
        this.initialized = true;
        final int generation = ++this.lifecycleGeneration;
        this.logger.log(100000000, "[%1.init]", LOGCLASS);
        this.bigStage = true;
        this.screenActive = false;
        this.actionProxySubscription = this.lastActionProxyCall
            .observeNonSticky()
            .log(this.logger, "beforeDebounce")
            .debounce(ACTION_PROXY_DEBOUNCE_TIME, this.dispatcher)
            .log(this.logger, "afterDebounce")
            .redirectTo(
                new Consumer() {
                    public void accept(Object value) {
                        synchronized (ExternalEventsListener.this) {
                            if (!ExternalEventsListener.this.initialized
                                || generation != ExternalEventsListener.this.lifecycleGeneration
                                || value != ExternalEventsListener.this.lastActionProxyCall.get()) return;
                            ActionProxyCall actionProxy = (ActionProxyCall)value;
                            switch (actionProxy.id) {
                                case 1001:
                                    ExternalEventsListener.this.logger.log(
                                        1000000,
                                        "<<- [%1.actionProxyCallPerformed] AP_METHOD_HMI_ACTIVATED",
                                        LOGCLASS
                                    );
                                    if (CarPlayApp.isActive()
                                        && !ExternalEventsListener.this.stateHandler.getCurrentState()
                                            .isAccessRestricted(Resource.SCREEN)) {
                                        PdcSmallStageGuard.carPlayScreenActivated(
                                            ExternalEventsListener.this.hmiService
                                        );
                                    }
                                    if (ExternalEventsListener.this.bigStage) {
                                        ExternalEventsListener.this.hmiActivated();
                                    } else if (CarPlayApp.isActive()
                                        && !ExternalEventsListener.this.stateHandler.getCurrentState()
                                            .isAccessRestricted(Resource.SCREEN)
                                        && PdcSmallStageGuard.shouldKeepCarPlayScreen()) {
                                        /* Entering CarPlay while OPS is already small still
                                         * needs SCREEN. Keep input with the parking popup. */
                                        ExternalEventsListener.this.context.getKeyEventsHandler().deactivate();
                                        ExternalEventsListener.this.activateScreen();
                                    }
                                    ExternalEventsListener.this.screenActive = true;
                                    break;

                                case 1002:
                                    ExternalEventsListener.this.logger.log(
                                        1000000,
                                        "<<- [%1.actionProxyCallPerformed] AP_METHOD_HMI_DEACTIVATED",
                                        LOGCLASS
                                    );
                                    if (ExternalEventsListener.this.screenActive
                                        && CarPlayApp.isActive()
                                        && !ExternalEventsListener.this.stateHandler.getCurrentState()
                                            .isAccessRestricted(Resource.SCREEN)
                                        && PdcSmallStageGuard.isParkingHmiDeactivation(actionProxy.parking)) {
                                        /* On the real unit the parking SMALL_STAGE can also publish
                                         * AP 1002.  Treat it as part of the same partial-OPS
                                         * transition; a genuine HOME/fullscreen takeover still takes
                                         * the stock branch when popup 2100008 is not guarded. */
                                        ExternalEventsListener.this.logger.log(
                                            1000000,
                                            "[%1.actionProxyCallPerformed] partial OPS: suppress HMI_DEACTIVATED",
                                            LOGCLASS
                                        );
                                        /* If OPS has already closed while this AP was
                                         * debouncing, BIG_STAGE restored input. Do not
                                         * turn it off again for that obsolete AP. */
                                        if (!ExternalEventsListener.this.bigStage
                                            || PdcSmallStageGuard.shouldKeepCarPlayScreen()) {
                                            ExternalEventsListener.this.context.getKeyEventsHandler().deactivate();
                                        }
                                    } else {
                                        ExternalEventsListener.this.hmiDeactivated();
                                        ExternalEventsListener.this.screenActive = false;
                                    }
                                    break;

                                default:
                                    break;
                            }
                        }
                    }
                }
            );
        this.context.addActionProxyListener(1001, this);
        this.context.addActionProxyListener(1002, this);
        this.eventBus.registerListener(this);
        this.context
            .getChoiceModel(ICoreTerminalModeModelBank.TERMINAL_MODE_DISPLAY_CONTEXT_CHOICE)
            .setValue(78);

        this.phoneServiceRegistration = this.context.getServiceManager().registerService(
            ITelStateListener.class,
            new ITelStateListener() {
                public void updateTelState(int terminalId, ITelState telState) {
                    ExternalEventsListener.this.logger.log(
                        1000000,
                        "<- [%1.updateTelState]",
                        LOGCLASS
                    );
                    ExternalEventsListener.this.context
                        .getCommandListHelper()
                        .create()
                        .addSingle(
                            new PhoneStateUpdate(
                                ExternalEventsListener.this.context,
                                telState.getCallActive(),
                                ExternalEventsListener.this.stateHandler
                            )
                        )
                        .execute("ExternalEventsListener.updateTelState");
                    ExternalEventsListener.this.smartphoneProperties
                        .getPropertyMUHFPPhonecallActive()
                        .accept(new Boolean(telState.getCallActive()));
                }
            },
            new Hashtable(0)
        );

        this.context.getNaviAppHandler().setNavigationStateListener(this);
        this.context.getAudioManager().addAudioContextListener(this);
    }

    public synchronized void deinit() {
        if (!this.initialized) return;
        this.initialized = false;
        this.lifecycleGeneration++;
        this.screenActive = false;
        if (this.actionProxySubscription != null) {
            this.actionProxySubscription.cancel();
            this.actionProxySubscription = null;
        }
        this.lastActionProxyCall.accept(null);
        this.logger.log(1000000, "[%1.deinit]", LOGCLASS);
        PdcSmallStageGuard.reset();
        this.context.removeActionProxyListener(this);
        this.eventBus.unregisterListener(this);
        if (this.phoneServiceRegistration != null) {
            this.context.getServiceManager().unregisterService(this.phoneServiceRegistration);
        }
        this.context.getNaviAppHandler().setNavigationStateListener(null);
        this.context.getAudioManager().removeAudioContextListener(this);
    }

    public synchronized void actionProxyCallPerformed(int actionProxyId, Map parameters) {
        if (!this.initialized || (actionProxyId != 1001 && actionProxyId != 1002)) return;
        this.lastActionProxyCall.accept(new ActionProxyCall(actionProxyId));
    }

    private void hmiDeactivated() {
        // SMALL_STAGE can precede AP 1001 when returning to CarPlay with OPS
        // already open. Keep that valid guard; revoke it for a real takeover
        // even when an inactive app/restricted SCREEN short-circuited callers.
        if (!CarPlayApp.isActive()
            || this.stateHandler.getCurrentState().isAccessRestricted(Resource.SCREEN)
            || !PdcSmallStageGuard.shouldKeepCarPlayScreen()) {
            PdcSmallStageGuard.reset();
        }
        this.context.getKeyEventsHandler().deactivate();
        this.context
            .getCommandListHelper()
            .create()
            .addSingle(new HMIDeactivated(this.context, this.stateHandler))
            .execute("ExternalEventsListener.AP_METHOD_HMI_DEACTIVATED");
    }

    private void closeAllDrawer() {
        this.logger.log(10000000, "[%1.hmiActivated] - closeSelectionDrawer()", LOGCLASS);
        DrawerEvent drawerEvent = new DrawerEvent(this.hmiService.getRootWindow(0), 7);
        this.hmiService.getEventDispatcher().postEvent(drawerEvent);
    }

    private void hmiActivated() {
        this.context.getKeyEventsHandler().activate();
        this.closeAllDrawer();
        this.activateScreen();
    }

    private void activateScreen() {
        this.context
            .getCommandListHelper()
            .create()
            .addSingle(new HMIActivated(this.context, this.stateHandler))
            .execute("ExternalEventsListener.AP_METHOD_HMI_ACTIVATED");
    }

    public void audioStateChanged(AudioConnectionState state) {
        this.logger.log(100000000, "[%1.audioStateChanged] '%2'", LOGCLASS, state);
    }

    public void audioFocusChanged(boolean hasFocus) {
        SmartphoneManager.SmartphoneType smartphoneType = this.context
            .getSmartphoneDSIManager()
            .getSmartphoneType();
        this.logger.log(
            1000000,
            "<- [%1.audioFocusChanged] %2 %3",
            LOGCLASS,
            String.valueOf(hasFocus),
            smartphoneType
        );
        this.context
            .getChoiceModel(ICoreSystemModelBank.ACTIVE_SMARTPHONE_ENTERTAINMENT_CHOICE)
            .setValue(
                hasFocus && smartphoneType.isNot(SmartphoneManager.SmartphoneType.UNKNOWN)
                    ? TerminalModeUtils.mapActiveSmartphoneTypeToEntertainmentAudioModelType(smartphoneType)
                    : 0
            );

        AbstractCommand currentCommand = this.getCurrentCommand();
        if (currentCommand == null || !currentCommand.updateAudioFocus(hasFocus)) {
            this.context
                .getCommandListHelper()
                .create()
                .addSingle(new AudioFocusChanged(this.context, hasFocus, this.stateHandler))
                .execute("ExternalEventsListener.audioFocusChanged]");
        }
    }

    public void stateChanged(boolean active) {
        this.logger.log(1000000, "<- [%1.stateChanged]", LOGCLASS);
        this.context
            .getCommandListHelper()
            .create()
            .addSingle(new NavigationStateChanged(this.context, active, this.stateHandler))
            .execute("ExternalEventsListener.stateChanged]");
    }

    private AbstractCommand getCurrentCommand() {
        AbstractCommand command = null;
        CommandList commandList = this.context.getCommandListManager().getActiveCommandList();
        if (commandList != null) {
            command = (AbstractCommand)commandList.getActiveCommand();
        }
        return command;
    }

    public synchronized void updateKombiStage(boolean isBigStage) {
        if (!this.initialized) return;
        if (isBigStage && this.screenActive) {
            this.hmiActivated();
        } else if (!isBigStage
                   && this.screenActive
                   && CarPlayApp.isActive()
                   && !this.stateHandler.getCurrentState().isAccessRestricted(Resource.SCREEN)
                   && PdcSmallStageGuard.shouldKeepCarPlayScreen()) {
            /* Keep SCREEN owned by TerminalMode so the right OPS panel overlays
             * CarPlay instead of minimizing it.  Input remains disabled while
             * parking owns the partial popup and is restored on BIG_STAGE. */
            this.logger.log(
                1000000,
                "[%1.updateKombiStage] SMALL_STAGE pure OPS: keep CarPlay SCREEN owner",
                LOGCLASS
            );
            this.context.getKeyEventsHandler().deactivate();
        } else {
            this.hmiDeactivated();
        }

        this.bigStage = isBigStage;
    }
}
