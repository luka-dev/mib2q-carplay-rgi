import com.luka.carplay.cluster.ClusterLayoutConfig;
import com.luka.carplay.core.AltScreenModule;
import com.luka.carplay.framework.Log;
import java.io.File;
import java.io.FileOutputStream;
import java.lang.reflect.Field;
import java.lang.reflect.Method;

/** /mnt/app/carplay_layout.cfg on the Java side: the parser, the View-state source policy
 *  (NAV_VIEW_SIZE_CHOICE vs cockpit FctID 54) and the compact-placement override, against the
 *  real AltScreenModule and ClusterLayoutConfig from the shipping JAR. */
public final class ClusterLayoutOptionsTest {
    static int checks;
    static void check(boolean b, String message) {
        checks++;
        if (!b) throw new AssertionError(message);
    }
    static Field f(Class c, String name) throws Exception {
        Field x = c.getDeclaredField(name); x.setAccessible(true); return x;
    }
    static void set(String name, int v) throws Exception {
        f(AltScreenModule.class, name).setInt(null, v);
    }
    static int get(String name) throws Exception { return f(AltScreenModule.class, name).getInt(null); }
    static Object call(Class c, String name, Class[] types, Object[] args) throws Exception {
        Method m = c.getDeclaredMethod(name, types); m.setAccessible(true); return m.invoke(null, args);
    }
    static int effective() throws Exception {
        return ((Integer) call(AltScreenModule.class, "effectiveViewAreaModeLocked", new Class[0], new Object[0])).intValue();
    }
    static void useFile(File cfg, String text) throws Exception {
        if (text == null) { cfg.delete(); }
        else { FileOutputStream o = new FileOutputStream(cfg); o.write(text.getBytes("UTF-8")); o.close(); }
        call(ClusterLayoutConfig.class, "resetForTest", new Class[] { String.class }, new Object[] { cfg.getPath() });
        call(AltScreenModule.class, "refreshLayoutConfig", new Class[] { Boolean.TYPE, String.class },
            new Object[] { Boolean.TRUE, "test" });
    }
    static void fresh() throws Exception {
        set("choiceViewMode", -1); set("vcViewMode", -1); set("lastViewInput", 0);
        set("desiredViewAreaMode", 0);
    }
    static String readAll(File f) throws Exception {
        java.io.FileInputStream in = new java.io.FileInputStream(f);
        byte[] b = new byte[(int) f.length()];
        int n = 0;
        while (n < b.length) { int r = in.read(b, n, b.length - n); if (r <= 0) break; n += r; }
        in.close();
        return new String(b, 0, n, "US-ASCII");
    }
    static int[] parse(String text) throws Exception {
        return (int[]) call(ClusterLayoutConfig.class, "parse", new Class[] { String.class }, new Object[] { text });
    }

    public static void main(String[] args) throws Exception {
        Log.setLevel(-1);
        File dir = File.createTempFile("layout", "");
        dir.delete(); dir.mkdir();
        File cfg = new File(dir, "carplay_layout.cfg");

        /* Parser: Java keys, the hook's keys skipped, junk counted. */
        int[] r = parse("");
        check(r[0] == 0 && r[1] == 0 && r[2] == 0, "empty file = auto/auto");
        r = parse("# Q7\r\nfull 360 87 720 297\r\nclassic 520 87 400 297\ncards left\neta no\n"
            + "speed_limit yes\nVIEW_SOURCE Vc # cockpit\ncompact Classic\n");
        check(r[0] == ClusterLayoutConfig.VIEW_SOURCE_VC && r[1] == ClusterLayoutConfig.COMPACT_CLASSIC
            && r[2] == 0, "Java keys, case and comments; hook keys skipped");
        r = parse("view_source sometimes\ncompact\nzoom 3\nview_source choice\ncompact sport extra\n");
        check(r[0] == ClusterLayoutConfig.VIEW_SOURCE_CHOICE && r[1] == 0 && r[2] == 4 && r[3] == 1,
            "bad lines rejected one by one, first bad line reported");

        /* No file: auto.  The View button alone works as before. */
        useFile(cfg, null);
        check(ClusterLayoutConfig.viewSource() == ClusterLayoutConfig.VIEW_SOURCE_AUTO, "no file = auto");
        fresh();
        AltScreenModule.setViewAreaModeFromChoice(1, false);
        check(get("desiredViewAreaMode") == 1, "choice -> compact");
        AltScreenModule.setViewAreaModeFromChoice(0, false);
        check(get("desiredViewAreaMode") == 0, "choice -> full");

        /* auto: the choice model freezes at compact, the cockpit keeps reporting. */
        AltScreenModule.setViewAreaModeFromChoice(1, false);
        AltScreenModule.onVcMapPresentation(true);
        check(get("desiredViewAreaMode") == 0, "auto: cockpit large overrides a frozen choice");
        AltScreenModule.onVcMapPresentation(false);
        check(get("desiredViewAreaMode") == 1, "auto: cockpit compact");
        AltScreenModule.onVcMapPresentation(true);
        AltScreenModule.setViewAreaModeFromChoice(1, false);
        check(get("desiredViewAreaMode") == 1, "auto: a later real choice event wins again");

        /* auto, connect-time seed: a stale choice does not undo a reported cockpit stage. */
        fresh();
        AltScreenModule.onVcMapPresentation(false);
        AltScreenModule.setViewAreaModeFromChoice(0, true);
        check(get("desiredViewAreaMode") == 1, "initial choice keeps the known cockpit stage");
        fresh();
        AltScreenModule.setViewAreaModeFromChoice(1, true);
        check(get("desiredViewAreaMode") == 1, "initial choice applies when the cockpit is silent");

        /* view_source choice: luka's original behaviour, FctID 54 ignored for the view area. */
        useFile(cfg, "view_source choice\n");
        fresh();
        AltScreenModule.setViewAreaModeFromChoice(0, false);
        AltScreenModule.onVcMapPresentation(false);
        check(get("desiredViewAreaMode") == 0, "choice: cockpit stage ignored");

        /* view_source vc: choice only until the cockpit has spoken. */
        useFile(cfg, "view_source vc\n");
        fresh();
        AltScreenModule.setViewAreaModeFromChoice(1, false);
        check(get("desiredViewAreaMode") == 1, "vc: choice used before the first FctID 54");
        AltScreenModule.onVcMapPresentation(true);
        AltScreenModule.setViewAreaModeFromChoice(1, false);
        check(get("desiredViewAreaMode") == 0, "vc: choice ignored once the cockpit reported");

        /* Switching the source in the file re-resolves at once. */
        fresh();
        AltScreenModule.setViewAreaModeFromChoice(0, false);
        AltScreenModule.onVcMapPresentation(false);
        check(get("desiredViewAreaMode") == 1, "vc: cockpit compact");
        useFile(cfg, "view_source choice\n");
        check(get("desiredViewAreaMode") == 0, "file change to choice re-applies the choice value");

        /* compact override: stock skin Classic, forced Sport and back. */
        useFile(cfg, null);
        set("stockStyle", 1); set("stockSkin", 0); set("clusterStyle", 255);   /* STYLE_CLASSIC=1 */
        call(AltScreenModule.class, "applyClusterStyle", new Class[] { String.class }, new Object[] { "test" });
        set("desiredViewAreaMode", 1);
        check(effective() == 2, "Classic skin -> classicscreen (index 2)");
        useFile(cfg, "compact sport\n");
        check(effective() == 1, "compact sport forces sportscreen (index 1)");
        useFile(cfg, "compact auto\n");
        check(effective() == 2, "compact auto follows the skin again");
        useFile(cfg, "compact classic\n");
        set("stockStyle", 0); set("stockSkin", 1);   /* stock switches to Sport */
        call(AltScreenModule.class, "applyClusterStyle", new Class[] { String.class }, new Object[] { "test" });
        check(effective() == 2, "compact classic holds against a Sport skin");
        set("desiredViewAreaMode", 0);
        check(effective() == 0, "fullscreen is never affected by compact");

        /* Rate limit: an unchanged file is not re-read; a changed one is picked up. */
        check(!ClusterLayoutConfig.refresh(false), "within 2 s: no re-read");
        check(!ClusterLayoutConfig.refresh(true), "same file: no value change");

        /* Car profile: the Q7 Layout classes write the Q7 areas for the hook, every other car
         * (B9 = the A5-measured built-in) removes the file. */
        Class vp = Class.forName("com.luka.carplay.cluster.VehicleLayoutProfile");
        File prof = new File(dir, "carplay_layout.vehicle");
        call(vp, "resetForTest", new Class[] { String.class }, new Object[] { prof.getPath() });
        Method pf = vp.getDeclaredMethod("profileFor", new Class[] { String.class }); pf.setAccessible(true);
        check("q7".equals(pf.invoke(null, new Object[] { "de.esolutions.hmi.widgets.audi.evo.high.LayoutMIB2HighQ7" })), "Q7 classic -> q7");
        check("q7".equals(pf.invoke(null, new Object[] { "de.esolutions.hmi.widgets.audi.evo.high.LayoutMIB2HighQ7Sport" })), "Q7 sport -> q7");
        check("".equals(pf.invoke(null, new Object[] { "de.esolutions.hmi.widgets.audi.evo.high.LayoutMIB2HighB9" })), "B9 -> none");
        check("".equals(pf.invoke(null, new Object[] { "de.esolutions.hmi.widgets.audi.evo.high.LayoutMIB2HighB9Sport" })), "B9 sport -> none");
        check("".equals(pf.invoke(null, new Object[] { null })), "unknown -> none");
        Method ap = vp.getDeclaredMethod("apply", new Class[] { String.class });
        ap.invoke(null, new Object[] { "de.esolutions.hmi.widgets.audi.evo.high.LayoutMIB2HighQ7" });
        check(prof.isFile(), "Q7 writes the profile");
        String text = readAll(prof);
        check(text.indexOf("full    360 87 720 297\n") >= 0 && text.indexOf("sport   114 88 350 296\n") >= 0
            && text.indexOf("classic 520 87 400 297\n") >= 0, "Q7 profile content: " + text);
        check(parse(text)[2] == 0, "profile lines are the hook's keys only");
        prof.delete();
        ap.invoke(null, new Object[] { "de.esolutions.hmi.widgets.audi.evo.high.LayoutMIB2HighQ7Sport" });
        check(!prof.exists(), "same profile (skin switch on a Q7): no rewrite");
        ap.invoke(null, new Object[] { "de.esolutions.hmi.widgets.audi.evo.high.LayoutMIB2HighB9" });
        ap.invoke(null, new Object[] { "de.esolutions.hmi.widgets.audi.evo.high.LayoutMIB2HighQ7" });
        check(prof.isFile(), "profile rewritten after a change");
        ap.invoke(null, new Object[] { "de.esolutions.hmi.widgets.audi.evo.high.LayoutMIB2HighB9" });
        check(!prof.exists(), "B9 removes the profile");

        cfg.delete(); dir.delete();
        System.out.println("ClusterLayoutOptionsTest: parser, auto/choice/vc view source, connect seed, "
            + "compact override, car profile (" + checks + " checks) PASS");
    }
}
