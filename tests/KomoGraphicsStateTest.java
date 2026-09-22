import com.luka.carplay.framework.Log;
import com.luka.carplay.core.CarPlayApp;
import com.luka.carplay.core.FrameworkRef;
import com.luka.carplay.rgd.BAPBridge;
import de.audi.atip.base.IFrameworkAccess;
import de.audi.tghu.navi.app.NavigationEnv;
import de.audi.tghu.navi.app.cluster.ClusterService;
import de.audi.tghu.navi.app.cluster.ClusterViewMode;
import de.audi.tghu.navi.app.cluster.KOMOService;
import java.lang.reflect.*;
import java.util.*;
import sun.misc.Unsafe;

/** FPK must bypass the MOST setters: they hide the stock map after disconnect.
 * Use real MU1316 service objects and install the framework used by BAPBridge.
 */
public final class KomoGraphicsStateTest {
    public static final class Service extends ClusterService {
        List<Integer> rates;
        Service() { super(null, null, null, null, null, null); } // Unsafe bypasses HU startup
        public void setKOMODataRate(int rate) { rates.add(rate); }
    }
    private static Field field(Object object, String name) throws Exception {
        for (Class<?> c = object.getClass(); c != null; c = c.getSuperclass()) {
            try { Field f = c.getDeclaredField(name); f.setAccessible(true); return f; }
            catch (NoSuchFieldException e) { }
        }
        throw new NoSuchFieldException(name);
    }
    private static void set(Object object, String name, Object value) throws Exception {
        field(object, name).set(object, value);
    }
    private static void check(boolean value, String why) {
        if (!value) throw new AssertionError(why);
    }
    public static void main(String[] args) throws Exception {
        Log.setLevel(-1);
        Field uf = Unsafe.class.getDeclaredField("theUnsafe"); uf.setAccessible(true);
        Unsafe unsafe = (Unsafe)uf.get(null);
        IFrameworkAccess fw = (IFrameworkAccess)Proxy.newProxyInstance(
            IFrameworkAccess.class.getClassLoader(), new Class<?>[]{IFrameworkAccess.class},
            (proxy, method, values) -> {
                // FPK, no actual map surface on this host.
                if (method.getName().equals("getSysConst")) return ((Integer)values[0]) == 541 ? 2 : 0;
                if (method.getReturnType() == Integer.TYPE) return 0;
                if (method.getReturnType() == Boolean.TYPE) return false;
                throw new AssertionError("Unexpected framework dependency: " + method);
            });
        FrameworkRef ref = new FrameworkRef(null);
        set(ref, "fw", fw);
        Field appRef = CarPlayApp.class.getDeclaredField("fwRef");
        appRef.setAccessible(true); appRef.set(null, ref);
        NavigationEnv env = (NavigationEnv)unsafe.allocateInstance(NavigationEnv.class);
        set(env, "framework", fw);
        ClusterViewMode view = (ClusterViewMode)unsafe.allocateInstance(ClusterViewMode.class);
        Service service = (Service)unsafe.allocateInstance(Service.class);
        service.rates = new ArrayList<>();
        set(service, "clusterViewMode", view);
        set(view, "env", env); set(view, "clusterService", service);
        KOMOService komo = (KOMOService)unsafe.allocateInstance(KOMOService.class);
        set(komo, "service", service);
        Constructor<?> ctor = Class.forName("com.luka.carplay.rgd.BAPBridge$SilentLogChannel").getDeclaredConstructor();
        ctor.setAccessible(true); set(komo, "logChannel", ctor.newInstance());
        BAPBridge bridge = new BAPBridge(); set(bridge, "csRef", service);
        Method force = BAPBridge.class.getDeclaredMethod("forceGfxAvailable", Boolean.TYPE);
        force.setAccessible(true);
        for (boolean fallback : new boolean[]{false, true}) {
            set(bridge, "komoService", fallback ? null : komo);
            for (boolean active : new boolean[]{true, true, false, false, true, false}) {
                service.rates.clear(); force.invoke(bridge, active);
                check(field(view, "dataRate").getInt(view) == 0, "FPK dataRate mutated");
                check(!field(view, "gfxAvailable").getBoolean(view), "FPK gfxAvailable mutated");
                check(service.rates.isEmpty(), "FPK received MOST pacing updates");
            }
        }
        System.out.println("KomoGraphicsStateTest: FPK leaves stock graphics state untouched, repeat/fallback PASS");
    }
}
