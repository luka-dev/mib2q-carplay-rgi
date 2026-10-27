import com.luka.carplay.cluster.ClusterLayerController;
import com.luka.carplay.core.ScreenModule;
import com.luka.carplay.framework.Log;
import com.sq5.aa.luka.AaLog;
import com.sq5.aa.luka.MirrorGate;
import com.sq5.aa.luka.VcMapViewGate;
import de.audi.tghu.fwhmi.IDisplayManagerKombiControl;

import java.io.File;
import java.io.FileOutputStream;
import java.lang.reflect.Field;
import java.lang.reflect.InvocationHandler;
import java.lang.reflect.Method;
import java.lang.reflect.Proxy;
import java.util.ArrayList;
import java.util.List;

/**
 * Host test of luka's single cluster-switch worker with the SQ5 mirror rule: a recording
 * IDisplayManagerKombiControl, the real ScreenModule worker + ClusterLayerController, and the real
 * marker file MirrorGate polls (/tmp/sq5_mirror_ready as the JVM resolves it on this host).
 */
public final class ScreenModuleMirrorTest {
    static int failures, checks;
    static void check(boolean ok, String what) { checks++; if (!ok) { failures++; System.out.println("FAIL " + what); } }

    static final class FakeDm implements InvocationHandler {
        final List log = new ArrayList();
        int ctx = 74;
        public synchronized Object invoke(Object p, Method m, Object[] a) {
            String n = m.getName();
            if (n.equals("switchContext")) { ctx = ((Integer) a[0]).intValue(); log.add("sc" + ctx); }
            else if (n.equals("getCurrentContextID")) return new Integer(ctx);
            else if (n.equals("setOpacity") && ((Integer) a[0]).intValue() == 99) log.add("op99=" + a[2]);
            else if (n.equals("setPosition") && ((Integer) a[0]).intValue() == 99) log.add("pos99=" + a[2] + "," + a[3]);
            else if (n.equals("setUpdateRate")) log.add("rate" + a[1]);
            else if (n.equals("getExtends")) return new int[]{0, 26, 1440, 455};
            if (m.getReturnType() == Boolean.TYPE) return Boolean.FALSE;
            if (m.getReturnType() == Integer.TYPE) return new Integer(0);
            return null;
        }
        synchronized String take() { String s = log.toString(); log.clear(); return s; }
        synchronized int current() { return ctx; }
    }

    /** VC map-view gate sink: records what the worker's mirror edges re-forward (setGALState). */
    static final class GalSink implements VcMapViewGate.Sink {
        final List applied = new ArrayList();
        public synchronized void forwardLater() { applied.add(Boolean.valueOf(VcMapViewGate.takeEffective())); }
        synchronized String take() { String s = applied.toString(); applied.clear(); return s; }
        synchronized boolean peekEndsWithTrue() {
            return !applied.isEmpty() && Boolean.TRUE.equals(applied.get(applied.size() - 1));
        }
    }

    static void setStatic(Class c, String n, Object v) throws Exception {
        Field f = c.getDeclaredField(n); f.setAccessible(true); f.set(null, v);
    }

    static File marker = new File(MirrorGate.MARKER);
    static int beat;
    static void heartbeat() throws Exception {
        marker.getParentFile().mkdirs();
        /* like sq5_mirror: a fixed 40-byte line rewritten in place (no truncate, so a racing reader
         * never sees the empty file MirrorGate treats as "mirror gone") */
        StringBuffer line = new StringBuffer((++beat) + " 4242 1440x455");
        while (line.length() < 39) line.append(' ');
        line.append('\n');
        java.io.RandomAccessFile o = new java.io.RandomAccessFile(marker, "rw");
        o.seek(0);
        o.write(line.toString().getBytes());
        o.close();
    }

    static boolean waitCtx(FakeDm dm, int ctx, long ms, boolean beating) throws Exception {
        long end = System.currentTimeMillis() + ms;
        while (System.currentTimeMillis() < end) {
            if (dm.current() == ctx && ScreenModule.isMirrorActive() == (ctx == 81 || ctx == 82)) {
                /* the worker finishes the switch (GAL re-forward, plane reapply) right after the
                 * switchContext observed here: let it settle before the caller inspects the sinks */
                Thread.sleep(150);
                return true;
            }
            if (beating) heartbeat();
            Thread.sleep(200);
        }
        return false;
    }

    public static void main(String[] args) throws Exception {
        AaLog.setEnabled(false);
        Log.setLevel(-1);
        marker.delete();
        FakeDm fake = new FakeDm();
        IDisplayManagerKombiControl dm = (IDisplayManagerKombiControl) Proxy.newProxyInstance(
            ScreenModuleMirrorTest.class.getClassLoader(), new Class[]{IDisplayManagerKombiControl.class}, fake);
        ClusterLayerController.bind(dm, 1);
        /* Android Auto already holds navigation focus (stock GALHandler -> updateGALState(true)) */
        VcMapViewGate.resetForTest();
        GalSink gal = new GalSink();
        check(VcMapViewGate.onStockGalState(gal, true), "no mirror yet: stock setGALState(true) passes");
        final ScreenModule sm = new ScreenModule();
        /* what start() does once the HMI DisplayManager is available */
        setStatic(ScreenModule.class, "connected", Boolean.TRUE);
        Field dmf = ScreenModule.class.getDeclaredField("dm"); dmf.setAccessible(true); dmf.set(sm, dm);
        Field en = ScreenModule.class.getDeclaredField("enabled"); en.setAccessible(true); en.set(sm, Boolean.TRUE);
        final Method loop = ScreenModule.class.getDeclaredMethod("switchLoop"); loop.setAccessible(true);
        Thread w = new Thread(new Runnable() { public void run() { try { loop.invoke(sm); } catch (Throwable t) { } } });
        w.setDaemon(true);
        w.start();

        check(waitCtx(fake, 74, 3000, false), "session, no mirror, no guidance -> 74");
        fake.take();
        heartbeat();
        check(waitCtx(fake, 82, 3000, true), "mirror marker fresh -> 82 {99}");
        String s = fake.take();
        check(s.indexOf("sc72, sc82, rate30") >= 0, "bounce 72 -> 82 -> rate 30: " + s);
        check(s.indexOf("pos99=0,26") >= 0 && s.indexOf("op99=0, op99=100") >= 0,
            "plane 99 at the map anchor, opacity written 0 then 100: " + s);
        String g = gal.take();
        check(g.equals("[false]"), "ctx 82 shown -> InfoStates 6 suppressed (setGALState(false)): " + g);
        check(!VcMapViewGate.onStockGalState(gal, true), "while 82: a new stock setGALState(true) -> false");

        ScreenModule.setNavActive(true);
        check(waitCtx(fake, 81, 3000, true), "guidance while mirror ready -> 81 {98,101,102,99}");
        s = fake.take();
        check(s.indexOf("sc72, sc81, rate30") >= 0, "bounce 72 -> 81: " + s);
        g = gal.take();
        check(g.equals("[]"), "82 -> 81 (route) keeps the suppression, no re-forward: " + g);

        long t0 = System.currentTimeMillis();
        check(waitCtx(fake, 80, 6000, false), "heartbeat stops -> fallback to luka 80");
        long fallbackMs = System.currentTimeMillis() - t0;
        check(fallbackMs >= 2500 && fallbackMs <= 4500, "fallback after ~3 s: " + fallbackMs + " ms");
        s = fake.take();
        check(s.indexOf("sc80") >= 0 && s.indexOf("op99=0") >= 0, "80 selected, mirror plane hidden: " + s);
        g = gal.take();
        check(g.equals("[true]"), "mirror lost (80) -> stock setGALState(true) restored: " + g);

        heartbeat();
        check(waitCtx(fake, 81, 3000, true), "heartbeat back -> 81 again");
        g = gal.take();
        check(g.equals("[false]"), "81 again -> suppressed again: " + g);
        marker.delete();
        t0 = System.currentTimeMillis();
        check(waitCtx(fake, 80, 2000, false), "marker deleted -> fallback at once");
        check(System.currentTimeMillis() - t0 < 1000, "deleted marker falls back within one poll");

        ScreenModule.setNavActive(false);
        check(waitCtx(fake, 74, 2000, false), "guidance ends, no mirror -> 74");
        heartbeat();
        check(waitCtx(fake, 82, 3000, true), "mirror again -> 82");
        sm.stop();
        check(waitCtx(fake, 74, 3000, true), "session end -> 74 even with a fresh marker");
        /* the worker re-forwards the GAL state just AFTER the switchContext the wait above observed */
        for (int i = 0; i < 20 && !gal.peekEndsWithTrue(); i++) Thread.sleep(50);
        g = gal.take();
        check(g.endsWith("[true]") || g.endsWith("true]"), "session end -> stock GAL state restored last: " + g);
        s = fake.take();
        check(s.indexOf("rate0, sc74, rate30") >= 0 && s.indexOf("op99=0") >= 0, "stock restore recipe, 99 hidden: " + s);
        marker.delete();
        System.out.println((failures == 0 ? "PASS" : "FAILED") + ": ScreenModuleMirrorTest " + checks + " checks, " + failures + " failures");
        System.exit(failures == 0 ? 0 : 1);
    }
}
