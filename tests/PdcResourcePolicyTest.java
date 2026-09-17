import com.luka.carplay.core.CarPlayApp;
import com.luka.carplay.pdc.PdcSmallStageGuard;
import de.audi.app.earlyfunc.core.parking.IParkingSystem;
import de.audi.app.earlyfunc.core.parking.ParkingPopupIdentifier;
import de.audi.app.earlyfunc.core.parking.AbstractParkingSystemControllerComponent;
import de.audi.app.earlyfunc.core.parking.ParkingPartialPopupHandler;
import de.audi.app.earlyfunc.core.parking.IParkingSystemController;
import de.audi.app.earlyfunc.core.parking.ops.OPSViewModeHandler;
import de.audi.app.earlyfunc.core.parking.pla.PLAPopinHandler;
import de.audi.app.earlyfunc.core.parking.pla.IPLAInterappServiceHandler;
import de.audi.app.car.common.app.ICarApplication;
import de.audi.atip.msg.MsgDistributor;
import de.audi.app.earlyfunc.core.parking.pla.IPLAPopinHandler;
import de.audi.app.earlyfunc.evo.parking.ParkingSystemControllerComponentEvo;
import de.audi.app.earlyfunc.evo.parking.ParkingSystemPLAComponentEvo;
import de.audi.app.earlyfunc.evo.parking.ParkingSystemVPSComponentEvo;
import de.audi.app.earlyfunc.evo.parking.ops.ParkingSystemOPSComponentEvo;
import de.audi.app.terminalmode.*;
import de.audi.app.terminalmode.device.*;
import de.audi.app.terminalmode.diagnosis.*;
import de.audi.app.terminalmode.interapp.HighPriorityResourceTracker;
import de.audi.app.terminalmode.osgi.IServiceManager;
import de.audi.app.terminalmode.smartphone.IDSISmartphoneManager;
import de.audi.app.terminalmode.statemachine.*;
import de.audi.atip.base.IFrameworkAccess;
import de.audi.atip.hmi.*;
import de.audi.atip.hmi.modelaccess.ChoiceModelApp;
import de.audi.atip.hmi.view.Screen;
import de.audi.atip.hmi.view.IScreenManager;
import de.audi.atip.interapp.phone.IEcallState;
import de.audi.atip.log.LogChannel;
import de.audi.atip.model.ICoreTerminalModeModelBank;
import de.audi.atip.utils.reactive.properties.*;
import de.audi.tghu.command.*;
import de.esolutions.hmi.widgets.audi.base.AbstractScreenWidget;
import org.dsi.ifc.carparkingsystem.DisplayContent;
import de.esolutions.fw.util.commons.SimpleIntObjectMap;
import java.lang.reflect.*;
import java.util.*;
import sun.misc.Unsafe;
import org.objectweb.asm.ClassReader;
import org.objectweb.asm.tree.*;

/** Runs the shipping tracker with stock MU1316 reactive properties, TMState
 * and commands. Only HMI widgets, device/service edges and job scheduling are
 * substituted. Also runs against the unpatched stock tracker to prove failure. */
public final class PdcResourcePolicyTest {
    static final int OPS = 2100008;
    static final LogChannel LOG = new LogChannel() {
        public void log(int a, String b, Object c, Object d, Object e, Object f,
                        long g, long h, long i, int j, Throwable k) { }
        public void log(int a, int b, Object c, Object d, Object e, Object f,
                        long g, long h, long i, int j, Throwable k) { }
    };
    static final Unsafe UNSAFE;
    static {
        try {
            Field f = Unsafe.class.getDeclaredField("theUnsafe"); f.setAccessible(true);
            UNSAFE = (Unsafe)f.get(null);
        } catch (Exception e) { throw new AssertionError(e); }
    }
    interface Call { Object call(String name, Object[] args) throws Exception; }
    static <T> T proxy(Class<T> type, final Call call) {
        return type.cast(Proxy.newProxyInstance(type.getClassLoader(), new Class[]{type}, (p,m,a) -> {
            if (m.getName().equals("toString")) return type.getSimpleName() + " fixture";
            if (m.getName().equals("hashCode")) return System.identityHashCode(p);
            if (m.getName().equals("equals")) return p == a[0];
            return call.call(m.getName(), a);
        }));
    }
    static void check(boolean ok, String message) { if (!ok) throw new AssertionError(message); }
    static void put(Object target, String name, Object value) throws Exception {
        for (Class<?> type = target.getClass(); type != null; type = type.getSuperclass()) {
            try {
                Field f = type.getDeclaredField(name); f.setAccessible(true); f.set(target, value); return;
            } catch (NoSuchFieldException ignored) { }
        }
        throw new AssertionError("missing field " + name);
    }
    /** Real component popup tables + real stock controller list assembly. Native
     * service startup is skipped, not the classification that failed on HU. */
    static class StockParking {
        final ParkingSystemControllerComponentEvo controller =
            (ParkingSystemControllerComponentEvo)UNSAFE.allocateInstance(ParkingSystemControllerComponentEvo.class);
        final ParkingSystemOPSComponentEvo ops =
            (ParkingSystemOPSComponentEvo)UNSAFE.allocateInstance(ParkingSystemOPSComponentEvo.class);
        final ParkingSystemPLAComponentEvo pla =
            (ParkingSystemPLAComponentEvo)UNSAFE.allocateInstance(ParkingSystemPLAComponentEvo.class);
        final ParkingSystemVPSComponentEvo vps =
            (ParkingSystemVPSComponentEvo)UNSAFE.allocateInstance(ParkingSystemVPSComponentEvo.class);
        StockParking() throws Exception {
            put(ops, "opsType", 2);
            SimpleIntObjectMap available = new SimpleIntObjectMap();
            Map<Integer,List<IParkingSystem>> mappings = new HashMap<>();
            for (IParkingSystem system : new IParkingSystem[]{ops, vps, pla}) {
                available.add(system.getParkingSystemID(), system);
                for (int popup : system.getSupportedDSIPopupIDs()) {
                    if (!mappings.containsKey(popup)) mappings.put(popup, new ArrayList<>());
                    mappings.get(popup).add(system);
                }
            }
            put(controller, "availableParkingSystems", available);
            put(controller, "parkingSystemsForPopup", mappings);
            put(pla, "logChannel", LOG);
            put(pla, "popinHandler", proxy(IPLAPopinHandler.class, (name,args) -> {
                check(name.equals("updateVpsOpsPopup"), "passive PLA attempted " + name); return null;
            }));
        }
        void target(Fixture f, int popup, int mode) {
            DisplayContent content = new DisplayContent(); content.popup = popup; content.mode = mode;
            List systems = controller.getInvolvedParkingSystems(content);
            if (popup == 1 || popup == 4 || popup == 7 || popup == 17) {
                check(systems.size() == 2 && systems.get(0) == ops && systems.get(1) == pla,
                    "stock standalone OPS no longer includes passive PLA");
            }
            PdcSmallStageGuard.parkingContentChanging(content, systems, f.hmi);
        }
    }
    static void stockParkingClassification() throws Exception {
        StockParking stock = new StockParking();
        // Stock PLA.setActive(true) observes OPS content; it does not activate
        // parking assist. Rejecting every registered PLA caused the HU failure.
        stock.pla.setActive(true, new DisplayContent());
        check(!stock.pla.isSystemActive(), "stock passive PLA unexpectedly active");
        for (int popup : new int[]{1, 4, 7, 17}) {
            Fixture f = new Fixture(); stock.target(f, popup, 0); f.tracker.processMsg(108);
            check(f.wizard.writes == 0 && !f.restricted() && f.carplay.getSmallStageType() == 6,
                "stock OPS + passive PLA toggled Main Wizard, popup=" + popup);
            f.visible(); f.tracker.processMsg(108);
            check(f.wizard.writes == 0 && !f.restricted(), "repeated passive PLA/OPS took SCREEN");
        }
        // Popup semantics must also reject camera/ARA/explicit PLA when a
        // component is missing (or its update has not arrived yet).
        for (int popup : new int[]{0, 2, 3, 5, 6, 8, 9, 10, 11, 12, 13, 14, 15, 16, 18, 99}) {
            Fixture f = new Fixture(); stock.target(f, popup, 0); f.tracker.processMsg(108);
            check(f.restricted() && f.wizard.writes == 1, "non-standalone popup bypassed takeover: " + popup);
        }
        Fixture f = new Fixture(); stock.target(f, 1, 12); f.tracker.processMsg(108);
        check(f.restricted(), "ARA mode bypassed takeover without registered ARA");
        put(stock.pla, "systemActive", true);
        f = new Fixture(); stock.target(f, 4, 0); f.tracker.processMsg(108);
        check(f.restricted(), "active PLA classified as passive");
        put(stock.pla, "systemActive", false);
        f = new Fixture(); stock.target(f, 4, 0); f.visible();
        put(stock.pla, "systemActive", true); f.tracker.processMsg(108);
        check(f.restricted() && f.carplay.getSmallStageType() == 1,
            "PLA activated during OPS but retained protection");
    }
    /** Skip only physical PLA message/spot rendering, retaining the real idle
     * popin state machine and its popup-request/suppression operations. */
    public static class PhysicalPla extends ParkingSystemPLAComponentEvo {
        public PhysicalPla() { super(null, null); }
        public void notifyOPSActive(boolean active) { }
        public void notifyPlaOpsStandaloneVisibleState(boolean visible) { }
    }
    static void stockActivationLifecycle() throws Exception {
        final Fixture f = new Fixture();
        final StockParking stock = new StockParking();
        final OpsAudioDrawerTest.Drawer drawer = new OpsAudioDrawerTest.Drawer();
        put(stock.controller, "audioDrawerContextService", drawer.service);
        final Map<Integer,Choice> models = new HashMap<>();
        final List<Integer> messages = new ArrayList<>();
        final ParkingPartialPopupHandler[] partial = new ParkingPartialPopupHandler[1];
        HMIService hmi = proxy(HMIService.class, (name,args) -> {
            if (name.equals("getRootWindow")) return proxy(IRootWindow.class, (n,a) -> f.current);
            if (name.equals("getScreenManager")) return f.screenManager;
            if (name.equals("showPartialPopup")) {
                check((Integer)args[1] == OPS && !f.restricted() && f.wizard.writes == 0,
                    "stock activation hid CarPlay before requesting OPS");
                partial[0].partialPopupVisible((Integer)args[1], (Integer)args[0]);
                return null;
            }
            if (name.equals("removePartialPopup")) {
                partial[0].partialPopupHidden((Integer)args[1], (Integer)args[0]);
                return null;
            }
            if (name.equals("getChoiceModel")) {
                int id = (Integer)args[0];
                if (!models.containsKey(id)) models.put(id, new Choice());
                return models.get(id).model;
            }
            throw new AssertionError("unexpected parking HMI " + name);
        });
        IFrameworkAccess framework = proxy(IFrameworkAccess.class, (name,args) -> {
            if (name.equals("getHMIService") || name.equals("getHmiServiceApp")) return hmi;
            throw new AssertionError("unexpected parking framework " + name);
        });
        ICarApplication app = proxy(ICarApplication.class, (name,args) -> {
            if (name.equals("getFrameworkAccess")) return framework;
            throw new AssertionError("unexpected parking application " + name);
        });
        PhysicalPla pla = (PhysicalPla)UNSAFE.allocateInstance(PhysicalPla.class);
        put(pla, "logChannel", LOG); put(pla, "application", app); put(pla, "controller", stock.controller);
        put(pla, "popinHandler", new PLAPopinHandler(stock.controller, pla, LOG, app,
            proxy(IPLAInterappServiceHandler.class, (n,a) -> null)));
        stock.controller.getAvailableParkingSystems().add(16, pla);
        // Replace only the physical message edge in every real popup mapping.
        Field mappingsField = AbstractParkingSystemControllerComponent.class.getDeclaredField("parkingSystemsForPopup");
        mappingsField.setAccessible(true);
        for (Object value : ((Map)mappingsField.get(stock.controller)).values()) {
            List list = (List)value;
            int index = list.indexOf(stock.pla); if (index >= 0) list.set(index, pla);
        }
        put(stock.controller, "application", app); put(stock.controller, "logChannel", LOG);
        put(stock.controller, "currentDisplayContent", new DisplayContent());
        put(stock.controller, "popupRequests", new Hashtable());
        put(stock.controller, "opsViewModeHandler", new OPSViewModeHandler(app, LOG));
        put(stock.controller, "msgDistributor", proxy(MsgDistributor.class, (name,args) -> {
            if (name.equals("sendMessage")) {
                int message = (Integer)args[0]; messages.add(message); f.tracker.processMsg(message); return null;
            }
            throw new AssertionError("unexpected parking distributor " + name);
        }));
        put(stock.ops, "logChannel", LOG); put(stock.ops, "application", app);
        put(stock.ops, "controller", stock.controller);
        partial[0] = new ParkingPartialPopupHandler(app, stock.controller, LOG);
        put(partial[0], "registeredPopupIDs", new ArrayList<>(Arrays.asList(OPS)));
        put(stock.controller, "partialPopupHandler", partial[0]);
        Method activate = AbstractParkingSystemControllerComponent.class.getDeclaredMethod("activateParkingSystem", DisplayContent.class);
        activate.setAccessible(true);
        for (int popup : new int[]{4, 1, 7, 17, 1}) {
            DisplayContent content = new DisplayContent(); content.popup = popup;
            activate.invoke(stock.controller, content);
            put(stock.controller, "currentDisplayContent", content);
            check(!f.restricted() && f.wizard.writes == 0 && partial[0].isPopupActive()
                && PdcSmallStageGuard.shouldKeepCarPlayScreen(), "stock OPS activation/update lost projection");
            check(models.get(3915).value == 1 && drawer.selected() == -1 && drawer.apsWrites == 0,
                "stock parking producer selected APS warning during protected side OPS");
        }
        check(messages.size() == 5 && messages.get(0) == 108,
            "test bypassed stock parking producer");
        PdcSmallStageGuard.ParkingTransition closing = PdcSmallStageGuard.captureHmiDeactivation();
        activate.invoke(stock.controller, new DisplayContent());
        put(stock.controller, "currentDisplayContent", new DisplayContent());
        check(messages.get(messages.size() - 1) == 107 && !partial[0].isPopupActive()
            && !PdcSmallStageGuard.shouldKeepCarPlayScreen() && f.carplay.getSmallStageType() == 1,
            "stock parking close retained popup/guard");
        check(models.get(3915).value == 0 && drawer.selected() == -1,
            "stock parking close did not clear APS request");
        check(closing != null && PdcSmallStageGuard.isParkingHmiDeactivation(closing),
            "real stock close invalidated its queued parking AP");
        DisplayContent reopen = new DisplayContent(); reopen.popup = 4;
        activate.invoke(stock.controller, reopen);
        check(!f.restricted() && partial[0].isPopupActive() && PdcSmallStageGuard.shouldKeepCarPlayScreen(),
            "stock parking reopen lost CarPlay");
        PdcSmallStageGuard.parkingStopped();
        check(drawer.selected() == 6001, "stock producer request was lost instead of visually masked");
        System.out.println("PDC stock activation: real controller -> APS drawer + OPS/idle PLA -> 108 -> partial popup PASS");
    }

    static void partialPopupCallbackIsolation() throws Exception {
        Fixture f = new Fixture(); f.pure();
        final int[] canceled = {0};
        ParkingPartialPopupHandler handler = new ParkingPartialPopupHandler(null,
            proxy(IParkingSystemController.class, (n,a) -> {
                if (n.equals("notifyPartialPopupCanceled")) { canceled[0]++; return null; }
                throw new AssertionError("unexpected callback " + n);
            }), LOG);
        handler.partialPopupVisible(OPS, 0);
        handler.partialPopupListenerRegistered(2100380, 0, false);
        handler.partialPopupHidden(2100380, 0);
        handler.partialPopupHidden(OPS, 1);
        check(canceled[0] == 0 && handler.isPopupActive() && PdcSmallStageGuard.shouldKeepCarPlayScreen(),
            "unrelated popup/terminal hidden callback canceled live OPS");
        handler.partialPopupHidden(OPS, 0);
        check(canceled[0] == 1 && !handler.isPopupActive() && !PdcSmallStageGuard.shouldKeepCarPlayScreen(),
            "matching OPS hidden callback did not clean up");
    }

    static void callsBefore(String type, String method, String early, String late) throws Exception {
        ClassNode node = new ClassNode(); new ClassReader(type).accept(node, 0);
        for (MethodNode m : node.methods) if (m.name.equals(method)) {
            int a = -1, b = -1, pos = 0;
            for (AbstractInsnNode instruction : m.instructions) {
                if (instruction instanceof MethodInsnNode) {
                    String name = ((MethodInsnNode)instruction).name;
                    if (a < 0 && name.equals(early)) a = pos;
                    if (b < 0 && name.equals(late)) b = pos;
                }
                pos++;
            }
            check(a >= 0 && b > a, type + "." + method + " lost early publication before " + late);
            return;
        }
        throw new AssertionError("missing method " + type + "." + method);
    }
    static class Choice {
        int value, writes;
        final ChoiceModelApp model = proxy(ChoiceModelApp.class, (name,args) -> {
            if (name.equals("getID")) return 0;
            if (name.equals("getValue")) return value;
            if (name.equals("setValue")) { value = ((Integer)args[0]); writes++; }
            return null;
        });
    }
    static CommandListManager manager() throws Exception {
        // Avoid starting the native HMI job dispatcher; retain real command lists.
        CommandListManager manager = (CommandListManager)UNSAFE.allocateInstance(CommandListManager.class);
        Field log = CommandListManager.class.getDeclaredField("log"); log.setAccessible(true); log.set(manager, LOG);
        return manager;
    }
    static class ImmediateCommands extends CommandListHelper {
        final CommandListManager manager;
        ImmediateCommands() throws Exception { super(null); manager = manager(); }
        public ConvenientCommandList create() {
            return new ConvenientCommandList(manager) {
                public void execute(String source) {
                    for (Object command : getCommands()) ((Command)command).execute();
                }
                public void commandFinishedWithPostSequence(CommandList next) {
                    check(next == null, "unexpected native post sequence");
                }
            };
        }
    }
    static class Fixture {
        final AbstractScreenWidget carplay = new AbstractScreenWidget(3200000);
        Screen current = carplay;
        Screen connected = carplay;
        int pendingScreenId = -1;
        final IScreenManager screenManager = proxy(IScreenManager.class, (name,args) -> {
            if (name.equals("getCurrentConnectedScreen")) return connected;
            if (name.equals("isScreenChangePending")) return pendingScreenId != -1;
            if (name.equals("getPendingScreenId")) return pendingScreenId;
            throw new AssertionError("unexpected screen manager call " + name);
        });
        final TMState state = new TMState();
        final LoggingPropertyFactory props = LoggingPropertyFactory.create();
        final Property muRvc = props.createProperty("muRvc", Boolean.FALSE);
        final Choice wizard = new Choice(), blocked = new Choice();
        final Property device = props.createProperty("device", new TMDevice(
            new TMDeviceID("fixture", SmartphoneManager.SmartphoneType.CARPLAY)));
        final HMIService hmi = proxy(HMIService.class, (name,args) -> {
            if (name.equals("getRootWindow")) return proxy(IRootWindow.class, (n,a) -> current);
            if (name.equals("getScreenManager")) return screenManager;
            throw new AssertionError("unexpected HMI call " + name);
        });
        final HighPriorityResourceTracker tracker;
        Fixture() throws Exception {
            PdcSmallStageGuard.parkingStopped(); CarPlayApp.active = true;
            IDeviceManager manager = proxy(IDeviceManager.class, (name,args) -> {
                if (name.equals("getActiveDevice")) return device.get();
                if (name.equals("getProperties")) return proxy(IDeviceManager.IDeviceManagerProperties.class,
                    (n,a) -> device);
                return null;
            });
            IContext context = proxy(IContext.class, (name,args) -> {
                if (name.equals("getDeviceManager")) return manager;
                if (name.equals("getChoiceModel")) {
                    int id = (Integer)args[0];
                    if (id == ICoreTerminalModeModelBank.SWITCH_TO_MAIN_WIZARD_CHOICE) return wizard.model;
                    if (id == ICoreTerminalModeModelBank.SMARTPHONE_SCREEN_BLOCKED_CHOICE) return blocked.model;
                }
                throw new AssertionError("unexpected context call " + name);
            });
            IStateHandler handler = proxy(IStateHandler.class, (name,args) -> {
                if (name.equals("getCurrentState")) return state;
                if (name.equals("changeState")) {
                    check(args[0] == state && args[1] == IRequestor.MAINUNIT, "wrong state requester");
                    return null;
                }
                throw new AssertionError("unexpected state call " + name);
            });
            tracker = new HighPriorityResourceTracker(proxy(IServiceManager.class, (n,a) -> null),
                LOG, new ImmediateCommands(), context, proxy(IDiagnosisManager.class, (n,a) -> null),
                handler, props, proxy(IDSISmartphoneManager.ISmartphoneProperties.class, (n,a) -> {
                    if (n.equals("getPropertyMURVCActive")) return muRvc;
                    throw new AssertionError("unexpected smartphone property " + n);
                }));
            tracker.init();
        }
        void target(int... ids) {
            List<IParkingSystem> systems = new ArrayList<>();
            for (final int id : ids) systems.add(proxy(IParkingSystem.class, (n,a) -> {
                if (n.equals("getParkingSystemID")) return id;
                if (n.equals("getHMIPopupID")) return new ParkingPopupIdentifier(1, OPS);
                throw new AssertionError("unexpected parking call " + n);
            }));
            DisplayContent content = new DisplayContent(); content.popup = ids.length == 0 ? 0 : 1;
            PdcSmallStageGuard.parkingContentChanging(content, systems, hmi);
        }
        void pure() { target(2); }
        void visible() { PdcSmallStageGuard.popupVisible(OPS, 0); }
        boolean restricted() { return state.isAccessRestricted(Resource.SCREEN); }
    }
    public static void main(String[] args) throws Exception {
        Fixture f = new Fixture(); f.pure();
        check(f.carplay.getSmallStageType() == 6, "screen not protected before 108");
        f.tracker.processMsg(108); // BEFORE showPopup and visible, exactly as stock OPS emits it
        check(f.wizard.writes == 0, "pure OPS 108 toggled Main Wizard");
        check(!f.restricted() && f.blocked.value == 0 && Boolean.FALSE.equals(f.muRvc.get()),
              "pure OPS 108 took SCREEN");
        // Verify the integration against the actual stock activation producer,
        // not a fixture that assumes our callback happens before message 108.
        callsBefore("de.audi.app.earlyfunc.core.parking.AbstractParkingSystemControllerComponent",
            "activateParkingSystem", "deactivateCurrentlyVisibleParkingSystems", "setActive");
        callsBefore("de.audi.app.earlyfunc.evo.parking.ParkingSystemControllerComponentEvo",
            "deactivateCurrentlyVisibleParkingSystems", "parkingContentChanging", "deactivateCurrentlyVisibleParkingSystems");
        callsBefore("de.audi.app.earlyfunc.evo.parking.ParkingSystemControllerComponentEvo",
            "notifyParkingSystemActive", "parkingStopped", "notifyParkingSystemActive");
        callsBefore("de.audi.app.earlyfunc.core.parking.ops.AbstractParkingSystemOPSComponent",
            "setActive", "notifyParkingSystemActive", "addPopupRequest");
        f.visible(); f.tracker.processMsg(108);
        check(f.wizard.writes == 0 && !f.restricted(), "duplicate 108 took SCREEN");
        check(PdcSmallStageGuard.isParkingHmiDeactivation(null), "opening AP exception absent");
        f.pure();
        check(PdcSmallStageGuard.isParkingHmiDeactivation(null), "repeat parking AP lost protection");

        // New target must win before a camera's first 108, even with OPS still visible.
        f.target(4, 2); f.tracker.processMsg(108);
        check(f.wizard.writes == 1 && f.restricted() && f.blocked.value == 3
            && Boolean.TRUE.equals(f.muRvc.get()), "OPS-to-VPS takeover suppressed");
        check(f.carplay.getSmallStageType() == 1, "camera retained small-stage override");
        f.visible(); PdcSmallStageGuard.showRequested(OPS, f.hmi);
        check(!PdcSmallStageGuard.shouldKeepCarPlayScreen(), "late OPS callback rearmed camera guard");
        f.tracker.processMsg(107);
        check(!f.restricted() && f.blocked.value == 0, "parking end retained SCREEN restriction");

        // Deactivation of old components can emit 107 after new intent is known.
        f = new Fixture(); f.pure(); f.tracker.processMsg(107); f.tracker.processMsg(108);
        check(!f.restricted() && f.wizard.writes == 0, "old 107 erased new OPS intent");
        PdcSmallStageGuard.popupRegisteredHidden(OPS, 0); f.tracker.processMsg(108);
        check(!f.restricted(), "initial registration snapshot canceled pending OPS");
        f.visible(); PdcSmallStageGuard.popupHidden(OPS, 0);
        check(!PdcSmallStageGuard.shouldKeepCarPlayScreen() && f.carplay.getSmallStageType() == 1,
            "hidden popup retained protection");

        f = new Fixture(); f.pure(); f.visible();
        f.current = new AbstractScreenWidget(1000000);
        check(!PdcSmallStageGuard.isParkingHmiDeactivation(null), "HOME consumed unused parking AP");
        check(f.carplay.getSmallStageType() == 1, "HOME retained screen override");
        f = new Fixture(); f.current = new AbstractScreenWidget(1000000); f.pure();
        f.tracker.processMsg(108);
        check(f.restricted(), "OPS outside CarPlay bypassed stock policy");

        for (int[] ids : new int[][]{{4}, {2,16}, {8}, {32}, {99}, {2,99}, {}}) {
            f = new Fixture(); f.pure(); f.visible(); f.target(ids); f.tracker.processMsg(108);
            check(f.restricted(), "non-OPS target bypassed stock policy: " + Arrays.toString(ids));
        }
        f = new Fixture();
        PdcSmallStageGuard.parkingContentChanging(null, null, f.hmi);
        f.tracker.processMsg(108); check(f.restricted(), "unknown content bypassed stock policy");
        f = new Fixture(); f.pure(); f.visible(); CarPlayApp.active = false;
        f.tracker.processMsg(108); check(f.restricted(), "disconnect left parking exception");
        f = new Fixture(); f.pure(); f.visible();
        f.device.accept(new TMDevice(new TMDeviceID("aa", SmartphoneManager.SmartphoneType.ANDROIDAUTO2)));
        f.tracker.processMsg(108);
        check(f.restricted() && f.blocked.value == 4 && f.wizard.writes == 0, "Android Auto policy changed");

        // Emergency-call and clamp-S restrictions still run through stock OR/combine and real commands.
        f = new Fixture(); f.pure(); f.visible();
        f.tracker.updateEcallState(0, proxy(IEcallState.class, (n,a) -> Boolean.TRUE));
        f.tracker.processMsg(108);
        check(f.restricted() && f.state.isAccessRestricted(Resource.AUDIO_MEDIA) && f.blocked.value == 1,
            "pure OPS released emergency-call restriction");
        f = new Fixture(); f.pure(); f.visible(); f.tracker.notifyPowerListenerOnExitState(0, 0);
        f.tracker.processMsg(108); check(f.restricted(), "pure OPS released clamp-S restriction");
        f.tracker.notifyPowerListenerOnEnterState(0, 0); check(!f.restricted(), "clamp-S restore failed");

        f = new Fixture(); f.pure(); f.visible();
        Field diagnostic = HighPriorityResourceTracker.class.getDeclaredField("diagnosisCommandProvider");
        diagnostic.setAccessible(true);
        ((IDiagnosisCommandProvider)diagnostic.get(f.tracker)).executeDiagCommand(
            "HighPriorityResourceTracker enable RVC", new String[0]);
        check(f.restricted(), "explicit camera diagnosis was suppressed");
        PdcSmallStageGuard.showRequested(OPS, f.hmi);
        check(!PdcSmallStageGuard.shouldKeepCarPlayScreen(), "OPS popup rearmed after explicit camera takeover");

        f = new Fixture(); f.pure(); f.visible(); f.target(4); f.tracker.processMsg(108);
        f.pure(); f.tracker.processMsg(108);
        check(!f.restricted() && f.blocked.value == 0 && Boolean.FALSE.equals(f.muRvc.get()),
            "return to eligible OPS retained camera restriction");
        check(f.wizard.writes == 1, "return to OPS toggled wizard again");

        stockParkingClassification();
        stockActivationLifecycle();
        partialPopupCallbackIsolation();

        f = new Fixture(); f.pure();
        PdcSmallStageGuard.ParkingTransition timedOut = PdcSmallStageGuard.captureHmiDeactivation();
        Thread.sleep(3200); // exercise the shipping rejected-show timeout and restoration
        check(!PdcSmallStageGuard.shouldKeepCarPlayScreen() && f.carplay.getSmallStageType() == 1,
            "unconfirmed request pinned screen");
        check(!PdcSmallStageGuard.isParkingHmiDeactivation(timedOut),
            "timed-out show left its queued AP exception valid");
        PdcSmallStageGuard.parkingStopped();
        System.out.println("PdcResourcePolicyTest: PASS (stock properties, commands and SCREEN state)");
    }
}
