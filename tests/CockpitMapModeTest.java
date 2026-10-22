import com.luka.carplay.core.AltScreenModule;
import com.luka.carplay.core.CockpitMapSetting;
import com.luka.carplay.framework.Log;
import de.audi.atip.storage.IStorageAccess;
import java.lang.reflect.*;
import java.util.HashMap;
import java.util.Map;

/** Cockpit map modes (CockpitMapSetting) against the real AltScreenModule: stored value, the
 *  shortcut cycle, the cluster context per mode, the pin and the ownership predicates that the
 *  stock seams (CombiMapController, DisplayManager, roller zoom, KDK planes) read. */
public final class CockpitMapModeTest {
    static int checks;
    static void check(boolean b, String m) { checks++; if (!b) throw new AssertionError(m); }
    static Field f(Class c, String n) throws Exception { Field x = c.getDeclaredField(n); x.setAccessible(true); return x; }
    static void setA(String n, Object v) throws Exception { f(AltScreenModule.class, n).set(null, v); }
    static Object call(Class c, String n, Class[] t, Object[] a) throws Exception {
        Method m = c.getDeclaredMethod(n, t); m.setAccessible(true); return m.invoke(null, a);
    }
    static int desired() throws Exception {
        return ((Integer) call(AltScreenModule.class, "computeDesiredCtxLocked", new Class[0], new Object[0])).intValue();
    }

    public static void main(String[] args) throws Exception {
        Log.setLevel(-1);
        final Map store = new HashMap();
        IStorageAccess s = (IStorageAccess) Proxy.newProxyInstance(CockpitMapModeTest.class.getClassLoader(),
            new Class[] { IStorageAccess.class }, new InvocationHandler() {
                public Object invoke(Object p, Method m, Object[] a) {
                    if (m.getName().equals("setInt")) { store.put(a[0] + "/" + a[1], a[2]); return null; }
                    if (m.getName().equals("getInt")) {
                        Object v = store.get(a[0] + "/" + a[1]);
                        return v != null ? v : a[2];
                    }
                    Class t = m.getReturnType();
                    if (t == Boolean.TYPE) return Boolean.FALSE;
                    if (t == Integer.TYPE) return Integer.valueOf(0);
                    if (t == Long.TYPE) return Long.valueOf(0);
                    return null;
                }
            });
        call(CockpitMapSetting.class, "resetForTest", new Class[] { IStorageAccess.class }, new Object[] { s });

        /* Defaults and the shortcut cycle. */
        check(CockpitMapSetting.mode() == CockpitMapSetting.PHONE, "default = phone map");
        check(CockpitMapSetting.next(0) == 1 && CockpitMapSetting.next(1) == 2 && CockpitMapSetting.next(2) == 0,
            "cycle phone -> audi+arrows -> audi -> phone");

        /* No session: always stock, nothing pinned or owned. */
        setA("platformSupported", Boolean.TRUE);
        setA("connected", Boolean.FALSE); setA("videoAvailable", Boolean.FALSE); setA("navActive", Boolean.FALSE);
        check(desired() == 74, "disconnected -> 74");
        check(!AltScreenModule.isClusterPinned() && !AltScreenModule.ownsRouteGuidance()
            && !AltScreenModule.isPhoneMapShown(), "disconnected owns nothing");

        /* PHONE: luka's behaviour, unchanged. */
        setA("connected", Boolean.TRUE);
        check(desired() == 81, "phone, no route -> 81");
        setA("navActive", Boolean.TRUE);
        check(desired() == 80, "phone, route -> 80");
        check(AltScreenModule.isClusterPinned() && AltScreenModule.ownsRouteGuidance()
            && AltScreenModule.isPhoneMapShown(), "phone owns everything");
        check(AltScreenModule.consumeSteeringWheelScale(0), "phone: roller zoom goes to the phone");

        /* AUDI_ARROWS: stock map, our maneuver box only while CarPlay guides. */
        CockpitMapSetting.select(CockpitMapSetting.AUDI_ARROWS, "test");
        check(((Integer) store.get("1009/1001")).intValue() == 1, "stored in 1009/1001");
        check(desired() == 82, "arrows, route, stock map 33 -> 82");
        AltScreenModule.onStockMapTarget(77);
        check(desired() == 83, "arrows, route, stock alternative map 58 -> 83");
        AltScreenModule.onStockMapTarget(74);
        check(AltScreenModule.isClusterPinned(), "arrows pins while CarPlay guides");
        check(AltScreenModule.ownsRouteGuidance() && !AltScreenModule.isPhoneMapShown(),
            "arrows: CarPlay keeps guidance and KDK, the phone map is not shown");
        check(!AltScreenModule.consumeSteeringWheelScale(0), "arrows: roller zoom stays with the Audi map");
        setA("navActive", Boolean.FALSE);
        check(desired() == 74 && !AltScreenModule.isClusterPinned(), "arrows, no route -> stock, unpinned");

        /* AUDI: the stock navigation keeps the whole cluster. */
        CockpitMapSetting.select(CockpitMapSetting.AUDI, "test");
        setA("navActive", Boolean.TRUE);
        check(desired() == 74, "audi -> 74 even with a route");
        check(!AltScreenModule.isClusterPinned() && !AltScreenModule.ownsRouteGuidance()
            && !AltScreenModule.isPhoneMapShown(), "audi owns nothing on the cluster");
        check(AltScreenModule.isConnected(), "audi: the CarPlay session itself stays up");

        /* Boot reads the stored mode back; garbage falls back to the phone map. */
        call(CockpitMapSetting.class, "resetForTest", new Class[] { IStorageAccess.class }, new Object[] { s });
        CockpitMapSetting.boot(null);
        check(CockpitMapSetting.mode() == CockpitMapSetting.AUDI, "boot restores audi");
        store.put("1009/1001", Integer.valueOf(7));
        call(CockpitMapSetting.class, "resetForTest", new Class[] { IStorageAccess.class }, new Object[] { s });
        CockpitMapSetting.boot(null);
        check(CockpitMapSetting.mode() == CockpitMapSetting.PHONE, "invalid stored value -> phone");
        CockpitMapSetting.select(5, "test");
        check(CockpitMapSetting.mode() == CockpitMapSetting.PHONE, "out-of-range select ignored");

        /* Map-tab state for the shortcut gate. */
        check(AltScreenModule.vcMapTabState() == -1, "map tab unknown until the cockpit reports");
        AltScreenModule.onVcMapTabVisible(true);
        check(AltScreenModule.vcMapTabState() == 1, "map tab visible");
        AltScreenModule.onVcMapTabVisible(false);
        check(AltScreenModule.vcMapTabState() == 0, "other tab");

        /* Steering wheel: short press = OK (route info) on release, long press = next mode,
         * only on the cockpit's map tab, never both for one press. */
        CockpitMapSetting.select(CockpitMapSetting.PHONE, "test");
        setA("connected", Boolean.TRUE); setA("navActive", Boolean.TRUE);
        final int[] ok = new int[1];
        AltScreenModule.setInfoModeListener(new AltScreenModule.InfoModeListener() {
            public void onInfoModeToggle() { ok[0]++; }
        });
        Class swc = Class.forName("com.luka.carplay.core.SteeringWheelInputModule");
        Constructor ctor = swc.getDeclaredConstructor(new Class[0]); ctor.setAccessible(true);
        Object sw = ctor.newInstance(new Object[0]);
        f(swc, "running").setBoolean(sw, true);
        Object listener = f(swc, "listener").get(sw);
        Method key = listener.getClass().getMethod("updateKey2",
            new Class[] { Integer.TYPE, Integer.TYPE, Integer.TYPE, Integer.TYPE, Integer.TYPE });
        key.setAccessible(true);
        Object[] press = { Integer.valueOf(4), Integer.valueOf(40), Integer.valueOf(1), Integer.valueOf(0), Integer.valueOf(1) };
        Object[] release = { Integer.valueOf(4), Integer.valueOf(40), Integer.valueOf(0), Integer.valueOf(0), Integer.valueOf(1) };
        Object[] longp = { Integer.valueOf(4), Integer.valueOf(40), Integer.valueOf(3), Integer.valueOf(0), Integer.valueOf(1) };
        Object[] longs = { Integer.valueOf(4), Integer.valueOf(40), Integer.valueOf(4), Integer.valueOf(0), Integer.valueOf(1) };
        Object[] longr = { Integer.valueOf(4), Integer.valueOf(40), Integer.valueOf(5), Integer.valueOf(0), Integer.valueOf(1) };
        key.invoke(listener, press);
        check(ok[0] == 0, "short press waits for the release");
        key.invoke(listener, release);
        check(ok[0] == 1 && CockpitMapSetting.mode() == CockpitMapSetting.PHONE, "short press = OK only");

        AltScreenModule.onVcMapTabVisible(true);
        key.invoke(listener, press); key.invoke(listener, longs); key.invoke(listener, longp);
        key.invoke(listener, longp); key.invoke(listener, longr); key.invoke(listener, release);
        check(CockpitMapSetting.mode() == CockpitMapSetting.AUDI_ARROWS, "one long press = one step: " + CockpitMapSetting.mode());
        check(ok[0] == 1, "a long press never also toggles OK");
        key.invoke(listener, press); key.invoke(listener, longp); key.invoke(listener, release);
        check(CockpitMapSetting.mode() == CockpitMapSetting.AUDI, "second long press -> audi");
        key.invoke(listener, press); key.invoke(listener, longp); key.invoke(listener, release);
        check(CockpitMapSetting.mode() == CockpitMapSetting.PHONE, "third long press -> phone");

        AltScreenModule.onVcMapTabVisible(false);
        key.invoke(listener, press); key.invoke(listener, longp); key.invoke(listener, release);
        check(CockpitMapSetting.mode() == CockpitMapSetting.PHONE, "long press on another cockpit tab is ignored");
        AltScreenModule.onVcMapTabVisible(true);
        setA("connected", Boolean.FALSE);
        key.invoke(listener, press); key.invoke(listener, longp); key.invoke(listener, release);
        check(CockpitMapSetting.mode() == CockpitMapSetting.PHONE, "no CarPlay session: long press ignored");
        Object[] other = { Integer.valueOf(4), Integer.valueOf(41), Integer.valueOf(3), Integer.valueOf(0), Integer.valueOf(1) };
        setA("connected", Boolean.TRUE);
        key.invoke(listener, other);
        check(CockpitMapSetting.mode() == CockpitMapSetting.PHONE, "other keys are not the shortcut");

        setA("connected", Boolean.FALSE); setA("navActive", Boolean.FALSE);
        System.out.println("CockpitMapModeTest: stored mode, shortcut cycle, contexts 80/81/82/83/74, pin and ownership per mode, steering-wheel short/long press ("
            + checks + " checks) PASS");
    }
}
