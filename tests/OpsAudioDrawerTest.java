import com.luka.carplay.core.CarPlayApp;
import com.luka.carplay.pdc.PdcSmallStageGuard;
import de.audi.atip.base.IFrameworkAccess;
import de.audi.atip.hmi.IHMIServiceApp;
import de.audi.atip.hmi.model.ListCell;
import de.audi.atip.hmi.model.ListModel;
import de.audi.atip.hmi.model.AbstractModelBank;
import de.audi.atip.log.LogChannelFactory;
import de.audi.atip.hmi.modelaccess.ListModelApp;
import de.audi.atip.hmi.modelaccess.ListModelGUI;
import de.audi.atip.interapp.audio.drawer.AudioDrawerContext.SourceAudioState;
import static de.audi.atip.interapp.audio.drawer.AudioDrawerContext.*;
import de.audi.audio.AudioEnv;
import de.audi.audio.context.AudioDrawerContextImpl;
import de.esolutions.hmi.widgets.audi.base.IDrawerManager;
import de.esolutions.hmi.widgets.audi.evo.widgets.entdrawer.EntertainmentDrawerContentManager;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicReference;

/** Real drawer service and stock content selector; only the model transport
 * and framework edges are substituted. Stock-first execution must select the
 * actual APS warning (6001) over protected CarPlay and fail the same assertion. */
public final class OpsAudioDrawerTest {
    static void check(boolean ok, String message) { PdcResourcePolicyTest.check(ok, message); }
    static <T> T edge(Class<T> type, PdcResourcePolicyTest.Call call) {
        return PdcResourcePolicyTest.proxy(type, call);
    }
    static final class Drawer {
        ListCell[] cells;
        int apsWrites;
        Runnable afterWrite;
        final ListModelApp app = edge(ListModelApp.class, (name,args) -> {
            if (name.equals("setMaxRows") || name.equals("setMaxColumns")) return null;
            if (name.equals("addRow")) { cells = ((ListCell[])args[0]).clone(); return null; }
            if (name.equals("getCell")) return cells[(Integer)args[1]];
            if (name.equals("setCell")) {
                int column = (Integer)args[1]; cells[column] = (ListCell)args[2];
                if (column == PRIO_IDX_APS) apsWrites++;
                Runnable callback = afterWrite;
                if (callback != null) callback.run();
                return null;
            }
            throw new AssertionError("unexpected model write " + name);
        });
        final ListModelGUI gui = edge(ListModelGUI.class, (name,args) -> {
            if (name.equals("getLength")) return 1;
            if (name.equals("getMaxColumns")) return cells.length;
            if (name.equals("getCell")) return cells[(Integer)args[1]];
            throw new AssertionError("unexpected model read " + name);
        });
        final IDrawerManager manager = edge(IDrawerManager.class, (name,args) -> {
            throw new AssertionError("unexpected drawer manager " + name);
        });
        final AudioDrawerContextImpl service;
        final ListModelGUI selectedModel;
        Drawer() { this(true); }
        Drawer(final boolean front) { this(front, null); }
        Drawer(final boolean front, final ListModel actualModel) {
            selectedModel = actualModel == null ? gui : actualModel;
            IHMIServiceApp hmi = edge(IHMIServiceApp.class, (name,args) -> {
                check(name.equals("getListModel") && (Integer)args[0] == 538, "wrong drawer model");
                return actualModel == null ? app : actualModel;
            });
            AudioEnv env = new AudioEnv(edge(IFrameworkAccess.class, (name,args) -> {
                if (name.equals("getLogChannel")) return PdcResourcePolicyTest.LOG;
                if (name.equals("getHmiServiceApp")) return hmi;
                if (name.equals("isFrontMU")) return front;
                throw new AssertionError("unexpected audio framework " + name);
            }));
            service = new AudioDrawerContextImpl(env);
        }
        void aps(SourceAudioState state) { service.setContext(SOURCE_APS, state); }
        int selected() { return EntertainmentDrawerContentManager.getContentIDForAppReq(selectedModel, manager); }
    }
    static void stockModel() throws Exception {
        // SystemModelBank allocates an ordinary ListModel for ID 538. Execute
        // that exact implementation as well as the controllable race fixture.
        AbstractModelBank.initLogging(edge(LogChannelFactory.class, (n,a) -> PdcResourcePolicyTest.LOG));
        PdcResourcePolicyTest.Fixture f = new PdcResourcePolicyTest.Fixture();
        ListModel model = new ListModel(538);
        Drawer d = new Drawer(true, model);
        for (de.audi.atip.interapp.audio.drawer.AudioDrawerContext.Source source :
                EntertainmentDrawerContentManager.AUDIO_CONTENT_TO_SOURCES_LUT) {
            if (source.getColumn() != PRIO_IDX_APS) d.service.setContext(source, SOURCE_AUDIO_STATE_ACTIVE);
        }
        d.aps(SOURCE_AUDIO_STATE_APS_ACTIVE_HIGH_PRIO);
        check(d.selected() == 6002, "stock ListModel did not select APS");
        f.pure(); f.visible();
        check(d.selected() == 22001, "stock ListModel did not expose next drawer request");
        PdcSmallStageGuard.parkingStopped();
        check(d.selected() == 6002, "stock ListModel lost APS restoration");
        for (int column = 1; column < 23; column++)
            check(model.getCell(0, column) == LIST_CELL_ACTIVE, "policy changed non-APS column " + column);
    }
    static void selectionAndRestoration() throws Exception {
        PdcResourcePolicyTest.Fixture f = new PdcResourcePolicyTest.Fixture();
        Drawer d = new Drawer();
        d.service.setContext(SOURCE_TERMINAL_MODE, SOURCE_AUDIO_STATE_ACTIVE);
        d.aps(SOURCE_AUDIO_STATE_ACTIVE);
        check(d.selected() == 6001, "stock APS content was not selected before guard");
        f.pure(); f.visible();
        check(d.selected() == 21001, "APS drawer still selected over CarPlay + side OPS");
        d.aps(SOURCE_AUDIO_STATE_ACTIVE);
        d.aps(SOURCE_AUDIO_STATE_APS_ACTIVE_HIGH_PRIO);
        check(d.selected() == 21001, "repeated/high-priority APS escaped the mask");
        d.service.setContext(SOURCE_PHONE, SOURCE_PHONE_STATE_INCOMING_CALL);
        check(d.selected() == 5003, "APS masking changed phone drawer priority");
        d.service.setContext(SOURCE_PHONE, SOURCE_PHONE_STATE_IDLE);
        f.target(4, 2);
        check(d.selected() == 6002, "camera did not restore latest reduced APS content");
        f.pure(); f.visible(); d.aps(SOURCE_AUDIO_STATE_INACTIVE);
        PdcSmallStageGuard.popupHidden(PdcResourcePolicyTest.OPS, 0);
        check(d.selected() == 21001, "closing OPS resurrected an inactive request");
        d.service.setContext(SOURCE_TERMINAL_MODE, SOURCE_AUDIO_STATE_INACTIVE);
        check(d.selected() == -1, "empty drawer no longer follows stock selection");
        d.service.setContext(null, SOURCE_AUDIO_STATE_ACTIVE);
        d.service.setContext(SOURCE_APS, null);
        check(d.selected() == -1, "invalid arguments changed the model");
    }
    static void serviceLifecycle() throws Exception {
        PdcResourcePolicyTest.Fixture f = new PdcResourcePolicyTest.Fixture(); f.pure(); f.visible();
        Drawer front = new Drawer(); front.aps(SOURCE_AUDIO_STATE_ACTIVE);
        check(front.selected() == -1, "late service initialization lost the mask");
        Drawer rear = new Drawer(false); rear.aps(SOURCE_AUDIO_STATE_ACTIVE);
        check(rear.selected() == 6001, "front OPS policy affected rear MU");
        PdcSmallStageGuard.parkingStopped();
        check(front.selected() == 6001, "rear service replaced front restoration target");
        f.pure(); f.visible();
        Drawer replacement = new Drawer(); replacement.aps(SOURCE_AUDIO_STATE_APS_ACTIVE_HIGH_PRIO);
        check(replacement.selected() == -1, "replacement service lost current mask");
        PdcSmallStageGuard.parkingStopped();
        check(replacement.selected() == 6002, "replacement service did not restore latest APS");
    }
    static void externalEvents() throws Exception {
        for (String event : new String[]{"home", "pending-home", "camera", "pla", "disconnect", "deinit", "close"}) {
            PdcExternalEventsTest.Events f = new PdcExternalEventsTest.Events();
            Drawer d = new Drawer(); d.aps(SOURCE_AUDIO_STATE_ACTIVE);
            f.ops(); f.ap(1002); f.drain();
            check(d.selected() == -1 && f.owned(), "OPS AP failed to retain projection without APS banner");
            d.aps(SOURCE_AUDIO_STATE_APS_ACTIVE_HIGH_PRIO);
            if (event.equals("home")) f.home();
            if (event.equals("pending-home")) { f.pendingScreenId = 1000000; f.ap(1002); }
            if (event.equals("camera")) f.target(4, 2);
            if (event.equals("pla")) f.target(2, 16);
            if (event.equals("disconnect")) { CarPlayApp.active = false; f.ap(1002); }
            if (event.equals("deinit")) f.listener.deinit();
            if (event.equals("close")) f.closeOps();
            f.drain();
            check(d.selected() == 6002, event + " did not restore latest stock APS request");
            f.listener.deinit();
        }
        PdcExternalEventsTest.Events returning = new PdcExternalEventsTest.Events();
        Drawer returnDrawer = new Drawer(); returnDrawer.aps(SOURCE_AUDIO_STATE_ACTIVE);
        returning.ops();
        returning.home(); returning.drain();
        check(returnDrawer.selected() == 6001, "APS was hidden outside CarPlay");
        // The parking display did not change while HOME was shown: no new
        // DSI content notification or partialPopupVisible callback on return.
        returning.current = returning.carplay; returning.ap(1001); returning.drain();
        check(returning.owned() && returnDrawer.selected() == -1,
            "return without new DSI notification lost CarPlay/APS suppression");
        Thread.sleep(3200);
        check(returnDrawer.selected() == -1, "existing visible OPS was treated as an unconfirmed show");
        returning.listener.deinit();
        check(returnDrawer.selected() == 6001, "returning session teardown lost APS restoration");

        PdcResourcePolicyTest.Fixture f = new PdcResourcePolicyTest.Fixture();
        Drawer d = new Drawer(); d.aps(SOURCE_AUDIO_STATE_ACTIVE); f.pure();
        Thread.sleep(3200);
        check(d.selected() == 6001, "rejected popup timeout left APS hidden");
    }
    static void returnEligibility() throws Exception {
        for (String change : new String[]{"visible-outside", "hidden", "unregistered", "registered-hidden",
                "camera", "ara", "active-pla", "restricted", "pending-home"}) {
            PdcExternalEventsTest.Events f = new PdcExternalEventsTest.Events();
            Drawer d = new Drawer(); d.aps(SOURCE_AUDIO_STATE_ACTIVE);
            PdcResourcePolicyTest.StockParking stock = new PdcResourcePolicyTest.StockParking();
            if (!change.equals("visible-outside")) { stock.target(f, 1, 0); f.visible(); }
            f.home(); f.drain();
            if (change.equals("visible-outside")) { stock.target(f, 1, 0); f.visible(); }
            f.listener.updateKombiStage(false);
            if (change.equals("hidden")) PdcSmallStageGuard.popupHidden(2100008, 0);
            if (change.equals("unregistered")) PdcSmallStageGuard.popupUnregistered(2100008);
            if (change.equals("registered-hidden")) PdcSmallStageGuard.popupRegisteredHidden(2100008, 0);
            if (change.equals("camera")) { f.target(4, 2); f.visible(); }
            if (change.equals("ara")) { stock.target(f, 1, 12); f.visible(); }
            if (change.equals("active-pla")) PdcResourcePolicyTest.put(stock.pla, "systemActive", true);
            if (change.equals("restricted")) f.state.restrictAccessTo(de.audi.app.terminalmode.statemachine.Resource.SCREEN);
            if (change.equals("pending-home")) f.pendingScreenId = 1000000;
            f.current = f.carplay; f.ap(1001); f.drain();
            if (change.equals("visible-outside")) {
                check(f.owned() && d.selected() == -1, "OPS opened outside CarPlay could not be resumed");
            } else {
                check(d.selected() == 6001 && f.carplay.getSmallStageType() == 1,
                    "ineligible return rearmed OPS: " + change);
            }
            f.listener.deinit();
        }
    }
    static void crossingModelUpdates() throws Exception {
        final PdcResourcePolicyTest.Fixture f = new PdcResourcePolicyTest.Fixture();
        final Drawer d = new Drawer();
        d.afterWrite = () -> {
            d.afterWrite = null;
            d.aps(SOURCE_AUDIO_STATE_APS_ACTIVE_HIGH_PRIO);
            f.pure(); f.visible();
        };
        d.aps(SOURCE_AUDIO_STATE_ACTIVE);
        check(d.selected() == -1, "reentrant model callback overwrote the OPS mask");
        PdcSmallStageGuard.parkingStopped();
        check(d.selected() == 6002, "reentrant request was not preserved");

        d.aps(SOURCE_AUDIO_STATE_INACTIVE);
        CountDownLatch entered = new CountDownLatch(1), finish = new CountDownLatch(1);
        AtomicReference<Throwable> failure = new AtomicReference<>();
        d.afterWrite = () -> {
            d.afterWrite = null; entered.countDown();
            try { check(finish.await(3, TimeUnit.SECONDS), "model callback blocked"); }
            catch (InterruptedException e) { throw new AssertionError(e); }
        };
        Thread writer = new Thread(() -> {
            try { d.aps(SOURCE_AUDIO_STATE_ACTIVE); } catch (Throwable e) { failure.set(e); }
        });
        writer.setDaemon(true); writer.start();
        try {
            check(entered.await(3, TimeUnit.SECONDS), "model writer did not enter callback");
            f.pure(); f.visible(); d.aps(SOURCE_AUDIO_STATE_APS_ACTIVE_HIGH_PRIO);
        } finally { finish.countDown(); }
        writer.join(3000);
        check(!writer.isAlive() && failure.get() == null, "crossing model updates failed: " + failure.get());
        check(d.selected() == -1, "in-flight old write escaped OPS mask");
        PdcSmallStageGuard.parkingStopped();
        check(d.selected() == 6002, "crossing updates lost latest stock request");
    }
    public static void main(String[] args) throws Exception {
        try {
            selectionAndRestoration();
            stockModel();
            serviceLifecycle();
            externalEvents();
            returnEligibility();
            crossingModelUpdates();
            System.out.println("OpsAudioDrawerTest: PASS (model 538 -> stock content 6001/6002, restoration and crossing updates)");
        } finally { PdcSmallStageGuard.parkingStopped(); }
    }
}
