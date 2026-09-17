import com.luka.carplay.core.CarPlayApp;
import com.luka.carplay.pdc.PdcSmallStageGuard;
import de.audi.app.terminalmode.*;
import de.audi.app.terminalmode.audio.IAudioManager;
import de.audi.app.terminalmode.device.IDeviceManager;
import de.audi.app.terminalmode.events.IEventBus;
import de.audi.app.terminalmode.keyevents.TMKeyEventsHandler;
import de.audi.app.terminalmode.osgi.IServiceManager;
import de.audi.app.terminalmode.smartphone.IDSISmartphoneManager;
import de.audi.app.terminalmode.statemachine.*;
import de.audi.atip.base.IFrameworkAccess;
import de.audi.atip.hmi.*;
import de.audi.atip.hmi.event.EventDispatcher;
import de.audi.atip.utils.dispatching.IDispatcher;
import de.audi.tghu.command.*;
import de.esolutions.hmi.widgets.audi.base.AbstractScreenWidget;
import java.util.*;

/** Real ExternalEventsListener, stock debounce/property chain and SCREEN
 * commands. Only clock/job dispatch, key hardware and physical HMI are fake. */
public final class PdcExternalEventsTest {
    static void check(boolean value, String message) { PdcResourcePolicyTest.check(value, message); }
    static <T> T edge(Class<T> type, PdcResourcePolicyTest.Call call) {
        return PdcResourcePolicyTest.proxy(type, call);
    }
    static final class Queue implements IDispatcher {
        long now;
        final List<Task> tasks = new ArrayList<>();
        final class Task implements ICancelable {
            final Runnable runnable; final long due; boolean canceled;
            Task(Runnable runnable, long delay) { this.runnable = runnable; due = now + delay; }
            public void cancel() { canceled = true; }
        }
        public void execute(Runnable r) { execute(r, 0); }
        public ICancelable execute(Runnable r, long delay) {
            Task t = new Task(r, delay); tasks.add(t); return t;
        }
        public boolean isDispatchThread() { return true; }
        void advance(long elapsed) {
            long until = now + elapsed;
            while (true) {
                Task next = null;
                for (Task t : tasks) if (t.due <= until && (next == null || t.due < next.due)) next = t;
                if (next == null) break;
                tasks.remove(next); now = next.due;
                if (!next.canceled) next.runnable.run();
            }
            now = until;
        }
    }
    static final class Keys extends TMKeyEventsHandler {
        boolean active;
        Keys() { super(edge(ITerminalLogger.class, (n,a) -> PdcResourcePolicyTest.LOG), null); }
        public void activate() { active = true; }
        public void deactivate() { active = false; }
    }
    public static final class Navigation extends NaviAppHandler {
        public Navigation() { super(null); }
        public void setNavigationStateListener(INavigationStateListener listener) { }
    }
    static final class Events extends PdcResourcePolicyTest.Fixture {
        final Queue queue = new Queue();
        final Keys keys = new Keys();
        int screenChanges;
        final ExternalEventsListener listener;
        Events() throws Exception {
            final Navigation navigation = (Navigation)PdcResourcePolicyTest.UNSAFE.allocateInstance(Navigation.class);
            final HMIService eventHmi = edge(HMIService.class, (n,a) -> {
                if (n.equals("getRootWindow")) return edge(IRootWindow.class, (nn,aa) -> current);
                if (n.equals("getScreenManager")) return screenManager;
                if (n.equals("getEventDispatcher")) return edge(EventDispatcher.class, (nn,aa) -> null);
                throw new AssertionError("unexpected HMI " + n);
            });
            final IDeviceManager devices = edge(IDeviceManager.class, (n,a) -> {
                if (n.equals("getActiveDevice")) return device.get();
                throw new AssertionError("unexpected devices " + n);
            });
            final CommandListManager manager = PdcResourcePolicyTest.manager();
            final CommandListHelper commands = new CommandListHelper(null) {
                public ConvenientCommandList create() {
                    return new ConvenientCommandList(manager) {
                        public void execute(String source) {
                            for (Object c : getCommands()) ((Command)c).execute();
                        }
                        public void commandFinished() { }
                        public void commandFinishedWithPostSequence(CommandList next) {
                            check(next == null, "unexpected post sequence");
                        }
                    };
                }
            };
            IContext context = edge(IContext.class, (n,a) -> {
                if (n.equals("getLogger")) return edge(ITerminalLogger.class, (nn,aa) -> PdcResourcePolicyTest.LOG);
                if (n.equals("getFramework")) return edge(IFrameworkAccess.class, (nn,aa) -> eventHmi);
                if (n.equals("getEventBus")) return edge(IEventBus.class, (nn,aa) -> Boolean.TRUE);
                if (n.equals("getServiceManager")) return edge(IServiceManager.class, (nn,aa) -> null);
                if (n.equals("getAudioManager")) return edge(IAudioManager.class, (nn,aa) -> null);
                if (n.equals("getNaviAppHandler")) return navigation;
                if (n.equals("getKeyEventsHandler")) return keys;
                if (n.equals("getCommandListHelper")) return commands;
                if (n.equals("getDeviceManager")) return devices;
                if (n.equals("getChoiceModel")) return new PdcResourcePolicyTest.Choice().model;
                if (n.equals("addActionProxyListener") || n.equals("removeActionProxyListener")) return null;
                throw new AssertionError("unexpected context " + n);
            });
            IStateHandler stateHandler = edge(IStateHandler.class, (n,a) -> {
                if (n.equals("getCurrentState")) return state;
                if (n.equals("changeState")) { screenChanges++; return null; }
                throw new AssertionError("unexpected state " + n);
            });
            listener = new ExternalEventsListener(context, stateHandler, queue,
                edge(IDSISmartphoneManager.ISmartphoneProperties.class, (n,a) -> muRvc));
            listener.init();
            ap(1001); drain();
            check(owned() && keys.active, "initial AP activation failed");
        }
        void ap(int id) { listener.actionProxyCallPerformed(id, Collections.emptyMap()); }
        void drain() { queue.advance(70); }
        boolean owned() { return state.getOwnerForResource(Resource.SCREEN) == ResourceOwner.DEVICE; }
        void ops() { pure(); visible(); tracker.processMsg(108); listener.updateKombiStage(false); }
        void home() { current = new AbstractScreenWidget(1000000); ap(1002); }
        void closeOps() { PdcSmallStageGuard.popupHidden(PdcResourcePolicyTest.OPS, 0); listener.updateKombiStage(true); }
    }
    interface Scenario { void run() throws Exception; }
    static int failures;
    static void scenario(String name, Scenario test) throws Exception {
        try { test.run(); System.out.println("PASS " + name); }
        catch (AssertionError error) { failures++; System.out.println("FAIL " + name + ": " + error.getMessage()); }
        finally { PdcSmallStageGuard.parkingStopped(); }
    }
    public static void main(String[] args) throws Exception {
        scenario("OPS small stage and first AP", () -> {
            Events f = new Events(); f.ops(); f.ap(1002); f.drain();
            check(f.owned() && !f.keys.active, "opening OPS lost SCREEN/input policy");
            f.closeOps(); check(f.owned() && f.keys.active, "OPS close did not restore input");
        });
        scenario("repeated AP while OPS still owns focus", () -> {
            Events f = new Events(); f.ops(); f.ap(1002); f.drain(); f.ap(1002); f.drain();
            check(f.owned(), "second OPS deactivation handed SCREEN to MAINUNIT");
        });
        scenario("OPS focus leaves and returns while small", () -> {
            Events f = new Events(); f.ops(); f.ap(1002); f.drain(); f.ap(1001); f.drain(); f.ap(1002); f.drain();
            check(f.owned() && !f.keys.active, "repeat OPS focus cycle lost projection");
        });
        scenario("return to CarPlay while OPS already small", () -> {
            Events f = new Events(); f.home(); f.drain(); f.current = f.carplay;
            f.ops(); f.ap(1001); f.drain();
            check(f.owned() && !f.keys.active, "AP activation in small OPS did not acquire SCREEN");
        });
        scenario("small stage without OPS retains stock policy", () -> {
            Events f = new Events(); f.listener.updateKombiStage(false); f.ap(1001); f.drain();
            check(!f.owned() && !f.keys.active, "non-parking small stage acquired SCREEN");
        });
        scenario("popup closes while deactivation is debouncing", () -> {
            Events f = new Events(); f.ops(); f.ap(1002); f.queue.advance(30); f.closeOps(); f.drain();
            check(f.owned() && f.keys.active, "late parking AP hid CarPlay after close");
        });
        scenario("DSI popup NONE before delayed deactivation", () -> {
            Events f = new Events(); f.ops(); f.ap(1002); f.target(); f.closeOps(); f.drain();
            check(f.owned() && f.keys.active, "DSI close invalidated queued parking AP");
        });
        scenario("rapid OPS close reopen close during debounce", () -> {
            Events f = new Events(); f.ops(); f.ap(1002); f.target(); f.closeOps();
            f.ops(); f.target(); f.closeOps(); f.drain();
            check(f.owned() && f.keys.active, "rapid OPS updates converted old AP into takeover");
        });
        scenario("HOME during debounce", () -> {
            Events f = new Events(); f.ops(); f.ap(1002); f.queue.advance(30); f.home(); f.drain();
            check(!f.owned() && !f.keys.active, "HOME was suppressed");
        });
        scenario("HOME after suppressed parking AP", () -> {
            Events f = new Events(); f.ops(); f.ap(1002); f.drain(); f.home(); f.drain();
            check(!f.owned(), "HOME after parking was suppressed");
        });
        scenario("HOME pending while current screen is still CarPlay", () -> {
            Events f = new Events(); f.ops(); f.pendingScreenId = 1000000; f.ap(1002); f.drain();
            check(!f.owned(), "pending HOME consumed unused parking exception");
            check(f.carplay.getSmallStageType() == 1, "pending HOME retained screen override");
        });
        scenario("full popup over current CarPlay", () -> {
            Events f = new Events(); f.ops(); f.connected = new AbstractScreenWidget(2100000); f.ap(1002); f.drain();
            check(!f.owned(), "full popup takeover was suppressed");
        });
        scenario("pending HOME after OPS closed", () -> {
            Events f = new Events(); f.ops(); f.ap(1002); f.closeOps(); f.pendingScreenId = 1000000; f.drain();
            check(!f.owned() && !f.keys.active, "captured OPS AP overruled pending HOME");
        });
        scenario("disconnected HMI screen", () -> {
            Events f = new Events(); f.ops(); f.ap(1002); f.connected = null; f.drain();
            check(!f.owned(), "missing connected screen retained projection");
        });
        scenario("camera during debounce", () -> {
            Events f = new Events(); f.ops(); f.ap(1002); f.queue.advance(30); f.target(4,2); f.tracker.processMsg(108); f.drain();
            check(f.restricted() && f.wizard.writes == 1, "camera takeover was suppressed");
        });
        scenario("camera takeover then close during debounce", () -> {
            Events f = new Events(); f.ops(); f.ap(1002); f.closeOps();
            f.target(4,2); f.tracker.processMsg(108); f.target(); f.tracker.processMsg(107); f.drain();
            check(!f.owned(), "camera takeover failed to revoke old parking AP");
        });
        scenario("disconnect during debounce", () -> {
            Events f = new Events(); f.ops(); f.ap(1002); CarPlayApp.active = false; f.drain();
            check(!f.owned(), "disconnect retained projection");
        });
        scenario("deinit with AP job pending", () -> {
            Events f = new Events(); f.ap(1002); f.listener.deinit(); int before = f.screenChanges; f.drain();
            check(f.screenChanges == before, "disposed listener still changed SCREEN");
        });
        scenario("deinit reinit and already dispatched old AP", () -> {
            Events f = new Events(); f.ap(1002);
            Runnable stale = f.queue.tasks.get(f.queue.tasks.size() - 1).runnable;
            f.listener.deinit(); f.listener.init();
            int before = f.screenChanges; stale.run(); f.drain();
            check(f.screenChanges == before, "old lifecycle AP executed after reinit");
            f.ap(1001); f.drain();
            check(f.screenChanges == before + 1 && f.keys.active, "new lifecycle did not receive AP exactly once");
        });
        scenario("already dispatched activation superseded by HOME", () -> {
            Events f = new Events(); f.ap(1001);
            Runnable stale = f.queue.tasks.get(f.queue.tasks.size() - 1).runnable;
            f.home(); f.drain(); int before = f.screenChanges; stale.run();
            check(!f.owned() && f.screenChanges == before, "superseded AP reactivated CarPlay over HOME");
        });
        scenario("callbacks after deinit", () -> {
            Events f = new Events(); f.listener.deinit(); int before = f.screenChanges;
            f.ap(1001); f.listener.updateKombiStage(true); f.drain();
            check(f.screenChanges == before, "disposed listener accepted fresh callbacks");
        });
        scenario("duplicate init and deinit", () -> {
            Events f = new Events(); int before = f.screenChanges;
            f.listener.init(); f.ap(1001); f.drain();
            check(f.screenChanges == before + 1, "duplicate init registered another AP consumer");
            f.listener.deinit(); f.listener.deinit();
        });
        scenario("eCall during OPS and delayed AP", () -> {
            Events f = new Events(); f.ops(); f.ap(1002);
            f.tracker.updateEcallState(0, edge(de.audi.atip.interapp.phone.IEcallState.class, (n,a) -> Boolean.TRUE));
            f.drain(); check(f.restricted() && !f.keys.active, "parking AP interfered with eCall");
        });
        scenario("clamp-S during OPS and delayed AP", () -> {
            Events f = new Events(); f.ops(); f.ap(1002); f.tracker.notifyPowerListenerOnExitState(0, 0);
            f.drain(); check(f.restricted() && !f.keys.active, "parking AP interfered with clamp-S");
        });
        scenario("active PLA after OPS close", () -> {
            Events f = new Events();
            PdcResourcePolicyTest.StockParking stock = new PdcResourcePolicyTest.StockParking();
            stock.target(f, 1, 0); f.visible(); f.ap(1002); f.closeOps();
            PdcResourcePolicyTest.put(stock.pla, "systemActive", Boolean.TRUE);
            f.drain(); check(!f.owned(), "queued OPS AP ignored independently activated PLA");
        });
        if (failures != 0) throw new AssertionError(failures + " external event scenarios failed");
    }
}
