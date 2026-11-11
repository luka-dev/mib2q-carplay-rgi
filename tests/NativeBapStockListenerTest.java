import java.lang.reflect.*;
import sun.misc.Unsafe;
import com.luka.carplay.rgd.GatedCombiService;
import de.audi.atip.log.LogChannel;
import de.audi.atip.interapp.combi.bap.navi.CombiBAPServiceNavi;
import de.audi.tghu.navi.app.cluster.CombiBAPListener;
import org.dsi.ifc.navigation.BapTurnToInfo;

/** Component probe: actual firmware listener bytecode and production gate;
 * no Navigation singleton, CAN transport or vehicle UI is simulated. */
public class NativeBapStockListenerTest {
    static class Quiet extends LogChannel {
        public void log(int a,String b,Object c,Object d,Object e,Object f,long g,long h,long i,int j,Throwable k) {}
        public void log(int a,int b,Object c,Object d,Object e,Object f,long g,long h,long i,int j,Throwable k) {}
    }
    static class Sink implements InvocationHandler {
        int roadWrites, turnWrites;
        String road="",turn="";
        public Object invoke(Object p,Method m,Object[] a) {
            if(m.getName().equals("updateCurrentPositionInfo")){road=(String)a[0];roadWrites++;}
            if(m.getName().equals("updateTurnToInfo")){turn=(String)a[0];turnWrites++;}
            return null;
        }
    }
    static void field(Object o,String n,Object v)throws Exception {
        Field f=CombiBAPListener.class.getDeclaredField(n);f.setAccessible(true);f.set(o,v);
    }
    static Object get(Object o,String n)throws Exception {
        Field f=CombiBAPListener.class.getDeclaredField(n);f.setAccessible(true);return f.get(o);
    }
    static void call(Object o,String n,Class[] types,Object[] args)throws Exception {
        Method m=CombiBAPListener.class.getDeclaredMethod(n,types);m.setAccessible(true);m.invoke(o,args);
    }
    static void street(Object o,String s)throws Exception {call(o,"setCurrentStreet",new Class[]{String.class},new Object[]{s});}
    static void turn(Object o,String s)throws Exception {call(o,"setTurnToInfo",new Class[]{BapTurnToInfo[].class},new Object[]{new BapTurnToInfo[]{new BapTurnToInfo(s,"")}});}
    static void check(boolean b,String s){if(!b)throw new AssertionError(s);}
    public static void main(String[] args)throws Exception {
        Field uf=Unsafe.class.getDeclaredField("theUnsafe");uf.setAccessible(true);
        CombiBAPListener listener=(CombiBAPListener)((Unsafe)uf.get(null)).allocateInstance(CombiBAPListener.class);
        Sink sink=new Sink();
        CombiBAPServiceNavi raw=(CombiBAPServiceNavi)Proxy.newProxyInstance(NativeBapStockListenerTest.class.getClassLoader(),new Class[]{CombiBAPServiceNavi.class},sink);
        GatedCombiService gate=new GatedCombiService(raw);
        field(listener,"logChannel",new Quiet());field(listener,"combiservice",gate);
        field(listener,"currentPositionInfo","");field(listener,"turnToInfo","");field(listener,"signPost","");
        street(listener,"native-before");turn(listener,"turn-before");
        gate.setCurrentPositionInfoBlocked(true);
        street(listener,"native-latest");turn(listener,"turn-latest");
        check(sink.road.equals("native-before") && sink.turn.equals("turn-before"),"blocked stock escaped");
        check(get(listener,"currentPositionInfo").equals("native-latest") && get(listener,"turnToInfo").equals("turn-latest"),"stock cache did not advance");
        raw.updateCurrentPositionInfo("CarPlay road");raw.updateTurnToInfo("CarPlay turn","");
        // Exact text clears used by BAPBridge.shutdown, followed by ownership release.
        raw.updateTurnToInfo("","");raw.updateCurrentPositionInfo("");
        gate.setCurrentPositionInfoBlocked(false);
        check(sink.road.equals("") && sink.turn.equals(""),"release must defer endpoint I/O");
        gate.replayPending();
        check(sink.road.equals("native-latest") && sink.turn.equals("turn-latest"),"native state not restored");
        int roadCount=sink.roadWrites,turnCount=sink.turnWrites;
        turn(listener,"turn-latest");
        check(sink.turnWrites==turnCount && sink.turn.equals("turn-latest"),"stock turn dedup behavior changed");
        street(listener,"native-latest");
        check(sink.roadWrites==roadCount+1 && sink.road.equals("native-latest"),"next position callback failed to repair street");
        raw.updateTurnToInfo("", "");
        gate.requestReplay(false); gate.replayPending();
        check(sink.turn.equals("turn-latest"),"repeated cleanup not restored");
        System.out.println("NativeBapStockListenerTest: PASS (actual stock listener, gated updates, native restoration despite dedup, repeated cleanup)");
        System.out.println("Scope: bytecode component test, not a full Navigation/vehicle test.");
    }
}
