import com.luka.carplay.core.CarPlayApp;
import com.luka.carplay.pdc.PdcSmallStageGuard;
import de.audi.app.earlyfunc.core.parking.IParkingSystem;
import de.audi.app.earlyfunc.core.parking.ParkingPopupIdentifier;
import de.audi.app.terminalmode.IContext;
import de.audi.atip.hmi.HMIService;
import de.audi.atip.hmi.IRootWindow;
import de.audi.atip.hmi.view.IScreenManager;
import de.esolutions.hmi.widgets.audi.base.AbstractScreenWidget;
import java.util.Collections;
import org.dsi.ifc.carparkingsystem.DisplayContent;
import static de.audi.atip.interapp.audio.drawer.AudioDrawerContext.*;

/** Actual CarPlayApp lifecycle worker + actual OPS guard/drawer service.
 * Disconnect intentionally emits no AP 1002, popup hide or parking update. */
public final class CarPlayPdcLifecycleTest {
    static void check(boolean ok, String message) { PdcResourcePolicyTest.check(ok, message); }
    static <T> T edge(Class<T> type, PdcResourcePolicyTest.Call call) {
        return PdcResourcePolicyTest.proxy(type, call);
    }
    static Object field(String name) throws Exception {
        java.lang.reflect.Field field = CarPlayApp.class.getDeclaredField(name);
        field.setAccessible(true);
        return field.get(null);
    }
    static void awaitActivation() throws Exception {
        long deadline = System.currentTimeMillis() + 3000;
        for (;;) {
            synchronized (field("lock")) {
                if (field("lifecycleGeneration").equals(field("lifecycleAppliedGeneration"))) {
                    for (boolean started : (boolean[])field("started"))
                        check(started, "external module fixture failed to start");
                    return;
                }
            }
            check(System.currentTimeMillis() < deadline, "activation worker did not settle");
            Thread.sleep(5);
        }
    }
    static void awaitBlockedCleanup() throws Exception {
        long deadline = System.currentTimeMillis() + 3000;
        for (;;) {
            for (Thread thread : Thread.getAllStackTraces().keySet()) {
                if (thread.getName().equals("carplay-lifecycle") && thread.getState() == Thread.State.BLOCKED) {
                    StackTraceElement[] stack = thread.getStackTrace();
                    if (stack.length > 0 && stack[0].getClassName().equals(PdcSmallStageGuard.class.getName())) return;
                }
            }
            check(System.currentTimeMillis() < deadline, "lifecycle cleanup did not reach OPS guard");
            Thread.sleep(5);
        }
    }
    public static void main(String[] args) throws Exception {
        CarPlayApp.onActivate(edge(IContext.class, (name,a) -> {
            throw new AssertionError("unexpected native lifecycle service " + name);
        }));
        check(CarPlayApp.isActive(), "real lifecycle failed to activate");
        awaitActivation();
        AbstractScreenWidget screen = new AbstractScreenWidget(3200000);
        IScreenManager screens = edge(IScreenManager.class, (name,a) -> {
            if (name.equals("getCurrentConnectedScreen")) return screen;
            if (name.equals("isScreenChangePending")) return false;
            throw new AssertionError("unexpected screen manager " + name);
        });
        HMIService hmi = edge(HMIService.class, (name,a) -> {
            if (name.equals("getRootWindow")) return edge(IRootWindow.class, (n,b) -> screen);
            if (name.equals("getScreenManager")) return screens;
            throw new AssertionError("unexpected HMI " + name);
        });
        IParkingSystem ops = edge(IParkingSystem.class, (name,a) -> {
            if (name.equals("getParkingSystemID")) return 2;
            if (name.equals("getHMIPopupID")) return new ParkingPopupIdentifier(1, 2100008);
            throw new AssertionError("unexpected OPS " + name);
        });
        OpsAudioDrawerTest.Drawer drawer = new OpsAudioDrawerTest.Drawer();
        drawer.aps(SOURCE_AUDIO_STATE_APS_ACTIVE_HIGH_PRIO);
        DisplayContent content = new DisplayContent(); content.popup = 1;
        PdcSmallStageGuard.parkingContentChanging(content, Collections.singletonList(ops), hmi);
        PdcSmallStageGuard.popupVisible(2100008, 0);
        check(drawer.selected() == -1 && screen.getSmallStageType() == 6, "OPS protection not armed");
        synchronized (PdcSmallStageGuard.class) {
            CarPlayApp.onDeactivate();
            awaitBlockedCleanup();
            CarPlayApp.onActivate(edge(IContext.class, (n,a) -> null));
            // A new valid OPS interval wins while the old lifecycle cleanup
            // is already dispatched but still waiting for the guard monitor.
            PdcSmallStageGuard.parkingStopped();
            PdcSmallStageGuard.parkingContentChanging(content, Collections.singletonList(ops), hmi);
            PdcSmallStageGuard.popupVisible(2100008, 0);
        }
        awaitActivation();
        check(drawer.selected() == -1 && screen.getSmallStageType() == 6,
            "stale disconnect cleanup released reconnected CarPlay OPS");
        CarPlayApp.onDeactivateAndWait();
        check(!CarPlayApp.isActive(), "real lifecycle failed to disconnect");
        // Inspect presentation directly: querying the guard here would itself
        // reset it, hiding the missing teardown notification under test.
        check(drawer.selected() == 6002, "disconnect without HMI event left APS drawer suppressed");
        check(screen.getSmallStageType() == 1, "disconnect retained OPS screen override");
        System.out.println("CarPlayPdcLifecycleTest: PASS (real disconnect without extra HMI event)");
    }
}
