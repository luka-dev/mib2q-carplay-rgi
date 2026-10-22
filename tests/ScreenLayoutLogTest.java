import de.audi.atip.hmi.model.BaseListRow;
import de.audi.atip.hmi.model.IntegerListCell;
import de.audi.atip.hmi.model.ListCell;
import de.audi.atip.hmi.modelaccess.ListModelApp;
import de.audi.tghu.navi.app.NavigationEnv;
import de.audi.tghu.navi.app.cluster.AltScreenCombiBAPListener;
import com.luka.carplay.framework.Log;
import java.lang.reflect.*;
import sun.misc.Unsafe;

/** The read-only list-176 diagnostics in AltScreenCombiBAPListener against the real stock
 *  model classes: the table text, de-duplication, the stock visible-area formula
 *  (LayoutProviderG22Kombi.getVisibleArea), and that a missing or broken model never throws. */
public final class ScreenLayoutLogTest {
    static int checks;
    static void check(boolean b, String m) { checks++; if (!b) throw new AssertionError(m); }

    /* NavigationEnv whose list model 176 is whatever the test sets. */
    public static final class Env extends NavigationEnv {
        Env() { super(null, null, null); }
        static ListModelApp model;
        public ListModelApp getListModel(int id) { return id == 176 ? model : null; }
    }

    static ListModelApp model(final BaseListRow[] rows) {
        return (ListModelApp) Proxy.newProxyInstance(ScreenLayoutLogTest.class.getClassLoader(),
            new Class[] { ListModelApp.class }, new InvocationHandler() {
                public Object invoke(Object p, Method m, Object[] a) {
                    String n = m.getName();
                    if (n.equals("getLength")) return Integer.valueOf(rows.length);
                    if (n.equals("getMaxColumns")) return Integer.valueOf(28);
                    if (n.equals("getRow") && a.length == 1) return rows[((Integer) a[0]).intValue()];
                    Class t = m.getReturnType();
                    if (t == Boolean.TYPE) return Boolean.FALSE;
                    if (t == Integer.TYPE) return Integer.valueOf(0);
                    return null;
                }
            });
    }

    static BaseListRow row(int[] v) {
        ListCell[] c = new ListCell[v.length];
        for (int i = 0; i < v.length; i++) c[i] = new IntegerListCell(v[i]);
        return new BaseListRow(c);
    }

    public static void main(String[] args) throws Exception {
        Log.setLevel(-1);
        Field uf = Unsafe.class.getDeclaredField("theUnsafe"); uf.setAccessible(true);
        Unsafe u = (Unsafe) uf.get(null);
        AltScreenCombiBAPListener l = (AltScreenCombiBAPListener) u.allocateInstance(AltScreenCombiBAPListener.class);
        Env env = (Env) u.allocateInstance(Env.class);
        Field ef = Class.forName("de.audi.tghu.navi.app.cluster.CombiBAPListener").getDeclaredField("env");
        ef.setAccessible(true);
        ef.set(l, env);
        Method log = AltScreenCombiBAPListener.class.getDeclaredMethod("logScreenLayout", new Class[] { String.class, Integer.TYPE });
        log.setAccessible(true);
        Field last = AltScreenCombiBAPListener.class.getDeclaredField("lastScreenLayout"); last.setAccessible(true);
        Method vis = AltScreenCombiBAPListener.class.getDeclaredMethod("visibleArea", new Class[] { int[].class, Integer.TYPE, Integer.TYPE });
        vis.setAccessible(true);

        /* No model yet: nothing logged, nothing thrown. */
        Env.model = null;
        log.invoke(l, new Object[] { "init", Integer.valueOf(-1) });
        check(last.get(null) == null, "no model, no log");

        /* Row 1 = cockpit: 1440x540, top line 87, info line 156, tubes 360/360 large, 114/976 Kb,
         * map offset 0/0 (an invented example; columns as AbstractLayoutProvider reads them). */
        int[] r1 = new int[28];
        r1[0] = 1440; r1[1] = 540; r1[8] = 156; r1[9] = 87;
        r1[11] = 360; r1[12] = 360; r1[13] = 114; r1[14] = 976; r1[24] = 0; r1[25] = 0;
        Env.model = model(new BaseListRow[] { row(new int[] { 800, 480 }), row(r1) });
        log.invoke(l, new Object[] { "presentation", Integer.valueOf(1) });
        String t = (String) last.get(null);
        check(t != null && t.startsWith("rows=2 maxColumns=28 row0=[800,480] row1=[1440,540,"), "table text: " + t);
        check("(360,87 720x297)".equals(vis.invoke(null, new Object[] { r1, Integer.valueOf(11), Integer.valueOf(12) })),
            "normal tubes -> visible area");
        check("(114,87 350x297)".equals(vis.invoke(null, new Object[] { r1, Integer.valueOf(13), Integer.valueOf(14) })),
            "Kb tubes -> visible area");
        r1[24] = 10; r1[25] = 5;
        check("(350,82 720x297)".equals(vis.invoke(null, new Object[] { r1, Integer.valueOf(11), Integer.valueOf(12) })),
            "map offset shifts the origin only");

        /* Same content again: de-duplicated (the field keeps the same object). */
        r1[24] = 0; r1[25] = 0;
        Env.model = model(new BaseListRow[] { row(new int[] { 800, 480 }), row(r1) });
        log.invoke(l, new Object[] { "visibility", Integer.valueOf(-1) });
        check(t.equals(last.get(null)), "unchanged table not re-logged");

        /* Short row, a null row and a non-integer cell are reported, never thrown. */
        ListCell[] odd = new ListCell[] { new IntegerListCell(5), null };
        Env.model = model(new BaseListRow[] { null, new BaseListRow(odd) });
        log.invoke(l, new Object[] { "presentation", Integer.valueOf(0) });
        check("rows=2 maxColumns=28 row0=null row1=[5,-]".equals(last.get(null)), "odd rows: " + last.get(null));

        System.out.println("ScreenLayoutLogTest: list 176 table, stock visible-area formula, de-dup, odd rows ("
            + checks + " checks) PASS");
    }
}
