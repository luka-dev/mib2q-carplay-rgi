import com.luka.carplay.rgd.BAPBridge;
import com.luka.carplay.rgd.RendererServer;
import com.luka.carplay.framework.Log;
import de.audi.app.combi.bap.app.navi.AppConnectorNavi;
import de.audi.app.combi.bap.app.navi.CombiModuleNavi;
import de.audi.app.bap.fw.functiontypes.BAPFunctionPropertyFSG;
import de.audi.atip.metrics.Distance;
import de.vw.mib.bap.requests.StatusProperty;
import de.vw.mib.bap.generated.navsd.serializer.DistanceToNextManeuver_Status;
import de.vw.mib.bap.stream.ByteArrayStream;
import java.lang.reflect.Constructor;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import sun.misc.Unsafe;

/** Shipping BAPBridge -> stock AppConnectorNavi -> stock serializer, no HU.
 * Unsafe skips only the framework constructors requiring a running BAP service.
 * The stock sender and serializer methods themselves execute without overrides.
 */
public final class DistanceBargraphChainTest {
    public static final class Capture extends BAPFunctionPropertyFSG {
        StatusProperty last;
        Capture() { super(null, 18); } // never invoked
        public void sendStatusIfChanged(StatusProperty status) { last = status; }
    }
    public static final class Module extends CombiModuleNavi {
        Capture capture;
        Module() { super(null, null); } // never invoked
        public BAPFunctionPropertyFSG getBAPFunctionPropertyFSG(int id) {
            check(id == 18, "unexpected BAP function"); return capture;
        }
    }
    public static final class Renderer extends RendererServer {
        int level, mode, progress;
        public boolean sendProgress(int level, int mode, int state) {
            this.level=level; this.mode=mode; this.progress=state; return true;
        }
    }
    private static void check(boolean ok, String why) { if (!ok) throw new AssertionError(why); }
    private static void set(Object target, String name, Object value) throws Exception {
        for (Class<?> c = target.getClass(); c != null; c = c.getSuperclass()) {
            try { Field f = c.getDeclaredField(name); f.setAccessible(true); f.set(target, value); return; }
            catch (NoSuchFieldException e) { }
        }
        throw new NoSuchFieldException(name);
    }
    public static void main(String[] args) throws Exception {
        Log.setLevel(-1);
        Distance.setSystemUnit(Distance.KM);
        Field uf = Unsafe.class.getDeclaredField("theUnsafe"); uf.setAccessible(true);
        Unsafe unsafe = (Unsafe) uf.get(null);
        Capture capture = (Capture)unsafe.allocateInstance(Capture.class);
        Module module = (Module)unsafe.allocateInstance(Module.class); module.capture = capture;
        AppConnectorNavi sender = (AppConnectorNavi)unsafe.allocateInstance(AppConnectorNavi.class);
        Constructor<?> logCtor = Class.forName("com.luka.carplay.rgd.BAPBridge$SilentLogChannel").getDeclaredConstructor();
        logCtor.setAccessible(true);
        set(sender, "logChannelFrequent", logCtor.newInstance()); set(sender, "moduleFsg", module);
        BAPBridge bridge = new BAPBridge(); Renderer renderer = new Renderer();
        set(bridge, "appConnectorNavi", sender); set(bridge, "rendererClient", renderer);
        set(bridge, "customRendererStarted", Boolean.TRUE);
        Method send = BAPBridge.class.getDeclaredMethod("sendDistanceToManeuverRaw", Integer.TYPE, Boolean.TYPE, Integer.TYPE);
        send.setAccessible(true);
        int[][] cases = {{150,1,75}, {30,1,100}, {30,1,0}, {300,0,0}, {0,1,50}};
        for (int[] c : cases) {
            send.invoke(bridge, Integer.valueOf(c[0]), Boolean.valueOf(c[1] != 0), Integer.valueOf(c[2]));
            DistanceToNextManeuver_Status status = (DistanceToNextManeuver_Status)capture.last;
            ByteArrayStream stream = new ByteArrayStream(); status.serialize(stream);
            byte[] bytes = stream.toByteArray();
            check(bytes.length == 8, "FctID 18 status payload length");
            int distance = (bytes[0]&255) | ((bytes[1]&255)<<8) | ((bytes[2]&255)<<16) | ((bytes[3]&255)<<24);
            int on = c[0] > 0 ? c[1] : 0, bar = c[0] > 0 ? c[2] : 0;
            check((bytes[5]&255) == on && (bytes[6]&255) == bar, "bargraph fields changed");
            check((bytes[7]&1) == (c[0] > 0 ? 1 : 0), "bargraph invalidated numeric distance");
            // FctID 18 transports displayed distance * 10, not raw meters.
            check(distance == c[0] * 10 && (bytes[4]&255) == (c[0] > 0 ? 0 : 255),
                    "distance/unit changed: input="+c[0]+" output="+distance+" unit="+(bytes[4]&255));
            check(renderer.mode == on && renderer.level == (on != 0 ? bar * 16 / 100 : 0), "renderer does not match BAP bargraph");
            check(renderer.progress==(on!=0 ? RendererServer.PROGRESS_FILL : RendererServer.PROGRESS_OFF),
                    "normal empty/full must not infer blink");
            StringBuilder hex = new StringBuilder();
            for (byte b : bytes) { if (hex.length() > 0) hex.append(' '); hex.append(String.format("%02x", b & 255)); }
            System.out.println("distance="+c[0]+" barOn="+on+" bar="+bar+" Fct18="+hex);
        }
        Method tick=BAPBridge.class.getDeclaredMethod("sendActionBlinkTick",Integer.TYPE);
        tick.setAccessible(true);
        set(bridge,"actionBlinkGeneration",Integer.valueOf(7));
        set(bridge,"blinkArmed",Boolean.TRUE); set(bridge,"blinkDistM",Integer.valueOf(10));
        set(bridge,"blinkBargraphDenominatorM",Integer.valueOf(100));
        set(bridge,"actionBlinkFull",Boolean.TRUE);
        for(int phase=0;phase<4;phase++) {
            check(((Boolean)tick.invoke(bridge,Integer.valueOf(7))).booleanValue(),"live blink worker stopped");
            ByteArrayStream stream=new ByteArrayStream(); capture.last.serialize(stream);
            byte[] wire=stream.toByteArray(); boolean high=(phase%2)==0;
            check((wire[6]&255)==(high?100:0) && renderer.level==(high?16:0) && renderer.mode==1,
                "HUD/renderer phase mismatch");
            check(renderer.progress==(high?RendererServer.PROGRESS_BLINK_HIGH:RendererServer.PROGRESS_BLINK_LOW),
                "explicit phase missing");
        }
        check(!((Boolean)tick.invoke(bridge,Integer.valueOf(6))).booleanValue(),"orphan worker emitted");
        check(renderer.progress==RendererServer.PROGRESS_BLINK_LOW,"orphan changed phase");
        Method replay=BAPBridge.class.getDeclaredMethod("replayDistanceToManeuver");replay.setAccessible(true);
        renderer.progress=-1;replay.invoke(bridge);
        check(renderer.progress==RendererServer.PROGRESS_BLINK_LOW && renderer.level==0 && renderer.mode==1,
            "cached replay reinterpreted low blink as complete fill");
        System.out.println("DistanceBargraphChainTest: stock sender/serializer retain distance validity with bargraph and both blink phases PASS");
    }
}
