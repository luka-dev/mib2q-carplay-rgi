package com.luka.carplay.rgd;

import com.luka.carplay.framework.Log;
import de.audi.app.combi.bap.app.navi.AppConnectorNavi;
import de.audi.app.combi.bap.app.navi.CombiModuleNavi;
import de.audi.app.bap.fw.functiontypes.BAPFunctionPropertyFSG;
import de.vw.mib.bap.requests.StatusProperty;
import de.vw.mib.bap.generated.navsd.serializer.CurrentPositionInfo_Status;
import de.vw.mib.bap.generated.navsd.serializer.TurnToInfo_Status;
import de.vw.mib.bap.stream.ByteArrayStream;
import java.lang.reflect.Constructor;
import java.lang.reflect.Field;
import sun.misc.Unsafe;

/** Actual bridge -> MU1316 sender -> UTF-8 wire serializer. Framework only is stubbed. */
public final class CurrentPositionStockChainTest {
    public static final class Capture extends BAPFunctionPropertyFSG {
        CurrentPositionInfo_Status text;
        TurnToInfo_Status turn;
        int textWrites, otherWrites;
        Capture() { super(null, 19); } // framework constructor skipped by Unsafe
        public void sendStatusIfChanged(StatusProperty status) {
            if (status instanceof CurrentPositionInfo_Status) {
                text = (CurrentPositionInfo_Status) status; textWrites++;
            } else {
                if (status instanceof TurnToInfo_Status) turn = (TurnToInfo_Status) status;
                otherWrites++;
            }
        }
    }
    public static final class Module extends CombiModuleNavi {
        Capture capture;
        Module() { super(null, null); }
        public BAPFunctionPropertyFSG getBAPFunctionPropertyFSG(int id) {
            check(id == 19 || id == 20 || id == 22, "unexpected BAP function " + id);
            return capture;
        }
    }
    private static void check(boolean value, String why) { RgiDeliveryRecoveryTest.check(value, why); }
    private static void set(Object obj, String key, Object value) throws Exception { RgiDeliveryRecoveryTest.set(obj, key, value); }
    private static Object get(Object obj, String key) throws Exception { return RgiDeliveryRecoveryTest.get(obj, key); }
    public static void main(String[] args) throws Exception {
        Log.setLevel(-1);
        Field uf = Unsafe.class.getDeclaredField("theUnsafe"); uf.setAccessible(true);
        Unsafe unsafe = (Unsafe) uf.get(null);
        Capture capture = (Capture) unsafe.allocateInstance(Capture.class);
        Module module = (Module) unsafe.allocateInstance(Module.class); module.capture = capture;
        AppConnectorNavi sender = (AppConnectorNavi) unsafe.allocateInstance(AppConnectorNavi.class);
        Constructor log = Class.forName("com.luka.carplay.rgd.BAPBridge$SilentLogChannel").getDeclaredConstructor();
        log.setAccessible(true);
        set(sender, "logChannel", log.newInstance()); set(sender, "logChannelFrequent", log.newInstance());
        set(sender, "moduleFsg", module);
        BAPBridge bridge = new BAPBridge();
        check(bridge.init(sender), "bridge init"); set(bridge, "bapSessionStarted", true);
        GatedCombiService nativeGate = new GatedCombiService(sender);
        nativeGate.setCurrentPositionInfoBlocked(true);
        String[] samples = {
            "North Pennsylvania Avenue via Washington Boulevard and Main Street",
            "Ленинградский проспект — Международное шоссе — аэропорт Шереметьево",
            "北京市朝阳区建国路前往东三环中路国际贸易中心出口",
            "Avenida de la Constitución hacia Plaza de España y estación central",
            "Avenue des Champs-Élysées vers l’aéroport Charles-de-Gaulle",
            "شارع الشيخ زايد نحو المطار الدولي عبر الطريق الرئيسي 123",
            "Exit 123 شارع الشيخ زايد Airport International Terminal 4",
            "i\uFE0F\uFE0F\uFE0Fi\uFE0F\uFE0F\uFE0Fi\uFE0F\uFE0F\uFE0Fi\uFE0F\uFE0F\uFE0Fi\uFE0F\uFE0F\uFE0Fi\uFE0F\uFE0F\uFE0Fi\uFE0F\uFE0F\uFE0Fi\uFE0F\uFE0F\uFE0Fi\uFE0F\uFE0F\uFE0Fi\uFE0F\uFE0F\uFE0F"
        };
        int frames = 0;
        for (int sample = 0; sample < samples.length; sample++) {
            RouteGuidance.State state = new RouteGuidance.State();
            state.maneuverCount = 1; state.maneuverOrder = new int[] {0}; state.mType[0] = 1;
            state.mExitInfo[0] = samples[sample];
            check(bridge.refreshInfoPresentation(state, 0), "initial text transaction");
            ByteArrayStream turn = new ByteArrayStream(); capture.turn.serialize(turn);
            byte[] empty = turn.toByteArray();
            check(empty.length == 2 && empty[0] == 0 && empty[1] == 0, "Fct20 gate not empty");
            CurrentPositionScroll plan = (CurrentPositionScroll) get(bridge, "positionScroll");
            int count = ((int[]) get(plan, "starts")).length;
            int other = capture.otherWrites;
            for (int i = 0; i < count; i++) {
                String expected = plan.current();
                ByteArrayStream wire = new ByteArrayStream(); capture.text.serialize(wire);
                byte[] bytes = wire.toByteArray();
                check(bytes.length <= 97 && (bytes[0] & 255) == bytes.length - 1, "wire byte budget");
                check(expected.equals(new String(bytes, 1, bytes.length - 1, "UTF-8")),
                    "stock sender truncated/changed Unicode: sample=" + sample + " frame=" + i);
                int writes = capture.textWrites;
                nativeGate.updateCurrentPositionInfo("native overwrite");
                check(capture.textWrites == writes, "native text crossed active CarPlay gate");
                bridge.tickPositionScroll((Long) get(plan, "deadline"));
                check(capture.otherWrites == other, "scroll republished non-text stock fields");
                frames++;
            }
        }
        nativeGate.setCurrentPositionInfoBlocked(false);
        int before = capture.textWrites;
        nativeGate.updateCurrentPositionInfo("native restored");
        check(capture.textWrites == before + 1, "native gate did not release");
        System.out.println("CurrentPositionStockChainTest: " + frames
            + " multilingual frames survive stock sender/UTF-8 serializer, empty Fct20 and ownership gate PASS");
    }
}
