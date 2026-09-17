import com.luka.carplay.core.CarPlayApp;
import com.luka.carplay.pdc.PdcSmallStageGuard;
import de.audi.atip.base.IFrameworkAccess;
import de.audi.atip.hmi.HMIService;
import de.audi.atip.hmi.view.IScreenManager;
import de.audi.tghu.hmi.evo.IPartialPopupControllerEvo;
import de.esolutions.hmi.widgets.audi.base.AbstractScreenWidget;
import de.esolutions.hmi.widgets.audi.base.HMITerminalImpl;
import de.esolutions.hmi.widgets.audi.evo.high.HMITerminalImplMIB2High;
import de.esolutions.hmi.widgets.audi.evo.high.PartialPopupManagerEvoHigh;
import java.util.*;

/** Execute the shipping manager AND stock queue/animation dispatch. Only the
 * physical popup show/hide and HMI/service boundaries are simulated. */
public final class OpsStatusLineTest {
    static void check(boolean value, String message) {
        PdcResourcePolicyTest.check(value, message);
    }
    static void put(Object target, String name, Object value) throws Exception {
        PdcResourcePolicyTest.put(target, name, value);
    }
    static final class Popup {
        final int id, slot;
        final IPartialPopupControllerEvo controller;
        boolean visible;
        int shows, hides;
        Popup(int id, int slot) {
            this.id = id; this.slot = slot;
            controller = PdcResourcePolicyTest.proxy(IPartialPopupControllerEvo.class, (name,args) -> {
                if (name.equals("getID")) return id;
                if (name.equals("getSlot")) return slot;
                if (name.equals("getPriority")) return 250;
                if (name.equals("getStyle")) return 1;
                if (name.equals("isPopupActive")) return true;
                if (name.equals("isVisible")) return visible;
                if (name.equals("show")) { visible = true; shows++; return 1; }
                if (name.equals("hide")) { visible = false; hides++; return 2; }
                if (name.equals("activateBackgroundGrayOut") || name.equals("deactivateBackgroundGrayOut")
                    || name.equals("restartAutoHideTimer")) return null;
                throw new AssertionError("unexpected popup call " + name);
            });
        }
    }
    static final class Manager {
        final PartialPopupManagerEvoHigh manager;
        final Popup footer = new Popup(62, 3), ops = new Popup(2100008, 4), volume = new Popup(52, 5);
        final List[] queues = new List[14];
        final IPartialPopupControllerEvo[] animating = new IPartialPopupControllerEvo[14];
        final IPartialPopupControllerEvo[] visible = new IPartialPopupControllerEvo[14];
        Manager(PdcResourcePolicyTest.Fixture f, int terminalId) throws Exception {
            manager = (PartialPopupManagerEvoHigh)PdcResourcePolicyTest.UNSAFE.allocateInstance(PartialPopupManagerEvoHigh.class);
            HMITerminalImpl terminal = (HMITerminalImpl)PdcResourcePolicyTest.UNSAFE.allocateInstance(HMITerminalImplMIB2High.class);
            put(terminal, "terminalID", terminalId);
            put(manager, "terminal", terminal);
            put(manager, "currentConnectedScreen", f.connected);
            put(manager, "registeredAtDrawerFocusManager", true);
            put(manager, "partialPopupsGloballyEnabled", true);
            Map popups = new HashMap();
            for (Popup p : new Popup[]{footer, ops, volume}) popups.put(p.id, p.controller);
            put(manager, "popups", popups);
            put(manager, "blockedPopups", new HashSet());
            put(manager, "partialPopupListenerList", new HashMap());
            for (int i = 0; i < queues.length; i++) queues[i] = new ArrayList();
            put(manager, "waitingPopupsQueues", queues);
            put(manager, "currentVisiblePopups", visible);
            put(manager, "currentAnimatingPopups", animating);
            IScreenManager sm = PdcResourcePolicyTest.proxy(IScreenManager.class, (n,a) -> {
                if (n.equals("removePartialPopupFromScreenData")) return false;
                throw new AssertionError("unexpected manager call " + n);
            });
            put(manager, "screenManager", sm);
            HMIService hmi = PdcResourcePolicyTest.proxy(HMIService.class, (n,a) -> {
                if (n.equals("getCurrentPopupPriority")) return 0;
                throw new AssertionError("unexpected HMI call " + n);
            });
            put(manager, "framework", PdcResourcePolicyTest.proxy(IFrameworkAccess.class, (n,a) -> {
                if (n.equals("getHMIService")) return hmi;
                throw new AssertionError("unexpected framework call " + n);
            }));
        }
        void finishHide(Popup p) {
            check(manager.popupHidden(p.id) == 2, "stock hidden callback rejected");
            check(animating[p.slot] == null && visible[p.slot] == null, "hide left stale popup ownership");
        }
    }
    static void guarded() throws Exception {
        PdcResourcePolicyTest.Fixture f = new PdcResourcePolicyTest.Fixture();
        Manager m = new Manager(f, 0);
        check(m.manager.showPopup(62) == 1 && m.footer.visible, "stock footer could not be shown");
        check(m.manager.popupVisible(62) == 1, "stock visible callback rejected");
        f.pure();
        check(m.manager.showPopup(2100008) == 1 && m.ops.visible, "OPS was suppressed");
        check(!m.footer.visible && m.queues[3].isEmpty(), "MMI status line 62 remained over CarPlay");
        m.finishHide(m.footer);
        check(m.manager.showPopup(62) == 2 && m.footer.shows == 1, "repeated footer request escaped guard");
        check(m.queues[3].isEmpty(), "blocked footer request leaked into stock queue");
        check(m.manager.showPopup(52) == 1 && m.volume.visible, "unrelated popup suppressed");

        // Queue replay can bypass showPopup: exercise the inherited stock method
        // which dispatches doShowPopup virtually, using an already queued footer.
        m.queues[3].add(m.footer.controller);
        m.manager.checkPriosAgainstFullScreenPopup();
        check(!m.footer.visible && m.footer.shows == 1, "stock queue replay bypassed footer guard");
        m.queues[3].clear();

        PdcSmallStageGuard.reset();
        check(m.manager.showPopup(62) == 1 && m.footer.visible, "released guard blocked next stock show");
        check(m.manager.hidePopup(62) == 2 && !m.footer.visible, "stock hide changed after release");
    }
    static void showingFooterInterrupted() throws Exception {
        PdcResourcePolicyTest.Fixture f = new PdcResourcePolicyTest.Fixture();
        Manager m = new Manager(f, 0);
        m.manager.showPopup(62); // animation has not delivered popupVisible yet
        f.pure();
        m.manager.showPopup(2100008);
        check(!m.footer.visible && m.queues[3].isEmpty(), "animating footer escaped hide");
        m.finishHide(m.footer);
        PdcSmallStageGuard.reset();
        check(m.manager.showPopup(62) == 1, "interrupted footer could not be shown later");
    }
    static void scope() throws Exception {
        for (String edge : new String[]{"home", "camera", "disconnect", "rear", "unrelated", "stale", "timeout"}) {
            PdcResourcePolicyTest.Fixture f = new PdcResourcePolicyTest.Fixture();
            f.pure();
            Manager m = new Manager(f, edge.equals("rear") ? 1 : 0);
            if (edge.equals("home")) f.pendingScreenId = 100000;
            if (edge.equals("camera")) f.connected = new AbstractScreenWidget(2100000);
            if (edge.equals("disconnect")) CarPlayApp.active = false;
            if (edge.equals("unrelated")) put(m.manager, "currentConnectedScreen", new AbstractScreenWidget(100000));
            if (edge.equals("stale")) put(m.manager, "currentConnectedScreen", new AbstractScreenWidget(3200000));
            if (edge.equals("timeout")) {
                java.lang.reflect.Field at = PdcSmallStageGuard.class.getDeclaredField("showRequestedAt");
                at.setAccessible(true); at.setLong(null, System.currentTimeMillis() - 5000);
            }
            check(m.manager.showPopup(62) == 1 && m.footer.visible, edge + " retained footer suppression");
        }
    }
    public static void main(String[] args) throws Exception {
        java.lang.reflect.Field fw = de.esolutions.hmi.widgets.audi.base.AbstractWidget.class.getDeclaredField("framework");
        fw.setAccessible(true);
        fw.set(null, PdcResourcePolicyTest.proxy(IFrameworkAccess.class, (n,a) -> {
            if (n.equals("getLogChannel")) return PdcResourcePolicyTest.LOG;
            throw new AssertionError("unexpected logging framework call " + n);
        }));
        try {
            guarded(); showingFooterInterrupted(); scope();
            System.out.println("OpsStatusLineTest: PASS (stock popup 62 show/hide/queue, OPS, HOME/camera/disconnect/rear/timeout)");
        } finally { PdcSmallStageGuard.parkingStopped(); CarPlayApp.active = false; }
    }
}
