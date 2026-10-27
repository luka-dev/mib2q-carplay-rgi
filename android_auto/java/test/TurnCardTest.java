import com.luka.carplay.cluster.ClusterLayerController;
import com.luka.carplay.core.ScreenModule;
import com.luka.carplay.framework.Log;
import com.sq5.aa.luka.AaLog;
import com.sq5.aa.luka.MirrorGate;
import com.sq5.aa.luka.TurnCard;
import com.sq5.aa.luka.TurnCardFeed;
import com.sq5.aa.luka.VcMapViewGate;
import de.audi.tghu.fwhmi.IDisplayManagerKombiControl;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.lang.reflect.Field;
import java.lang.reflect.InvocationHandler;
import java.lang.reflect.Method;
import java.lang.reflect.Proxy;
import java.util.ArrayList;
import java.util.List;

/**
 * Host test of the cockpit turn card (TurnCard + ClusterLayerController + ScreenModule + TurnCardFeed):
 *  1. the pure layer rule (off / stock / card / prearm) and the held mirror context (81 w/o route);
 *  2. distance text, feed record format, marker capability token;
 *  3. the real single cluster-switch worker + layer controller against a recording DisplayManager:
 *     route start/stop never switch context once 81 is held, every VC view change lands on the
 *     documented plane geometry/opacity, the renderer viewport follows, and the feed file carries the
 *     card rect in mirror-frame coordinates.
 */
public final class TurnCardTest {
    static int failures, checks;
    static void check(boolean ok, String what) { checks++; if (!ok) { failures++; System.out.println("FAIL " + what); } }

    static final class FakeDm implements InvocationHandler {
        final List log = new ArrayList();
        int ctx = 74;
        public synchronized Object invoke(Object p, Method m, Object[] a) {
            String n = m.getName();
            int id = a != null && a.length > 0 && a[0] instanceof Integer ? ((Integer) a[0]).intValue() : -1;
            boolean ours = id == 98 || id == 99 || id == 101 || id == 102;
            if (n.equals("switchContext")) { ctx = id; log.add("sc" + ctx); }
            else if (n.equals("getCurrentContextID")) return new Integer(ctx);
            else if (n.equals("setOpacity") && ours) log.add("op" + id + "=" + a[2]);
            else if (n.equals("setPosition") && ours) log.add("pos" + id + "=" + a[2] + "," + a[3]);
            else if (n.equals("setCropping") && ours)
                log.add("crop" + id + "=" + a[2] + "," + a[3] + "," + a[4] + "," + a[5] + ">" + a[6] + "," + a[7]);
            else if (n.equals("getExtends")) return new int[]{1440, 455};
            if (m.getReturnType() == Boolean.TYPE) return Boolean.FALSE;
            if (m.getReturnType() == Integer.TYPE) return new Integer(0);
            return null;
        }
        synchronized String take() { String s = log.toString(); log.clear(); return s; }
        synchronized int current() { return ctx; }
    }

    static final class GalSink implements VcMapViewGate.Sink {
        public void forwardLater() { VcMapViewGate.takeEffective(); }
    }

    static void setStatic(Class c, String n, Object v) throws Exception {
        Field f = c.getDeclaredField(n); f.setAccessible(true); f.set(null, v);
    }

    /* mirror heartbeat (real marker path, as MirrorGate polls it) */
    static final File marker = new File(MirrorGate.MARKER);
    static volatile String markerTail = " tc1";
    static volatile boolean beating = true;
    static int beat;
    static synchronized void heartbeat() throws Exception {
        marker.getParentFile().mkdirs();
        /* like sq5_mirror: a fixed 40-byte line rewritten in place (no truncate, so a racing reader
         * never sees the empty file MirrorGate treats as "mirror gone") */
        StringBuffer line = new StringBuffer((++beat) + " 4242 1440x455" + markerTail);
        while (line.length() < 39) line.append(' ');
        line.append('\n');
        java.io.RandomAccessFile o = new java.io.RandomAccessFile(marker, "rw");
        o.seek(0);
        o.write(line.toString().getBytes());
        o.close();
    }

    /** Value of the LAST entry starting with prefix in a List.toString() "[a, b, c]" (entries hold no ", "). */
    static String last(String log, String prefix) {
        String body = log.length() >= 2 ? log.substring(1, log.length() - 1) : "";
        String hit = null;
        int i = 0;
        while (i <= body.length()) {
            int j = body.indexOf(", ", i);
            String e = body.substring(i, j < 0 ? body.length() : j);
            if (e.startsWith(prefix)) hit = e.substring(prefix.length());
            if (j < 0) break;
            i = j + 2;
        }
        return hit;
    }

    static String readFeed(File f) throws Exception {
        if (!f.exists()) return "";
        FileInputStream in = new FileInputStream(f);
        byte[] b = new byte[TurnCardFeed.RECORD_LEN];
        int n = in.read(b);
        in.close();
        return n > 0 ? new String(b, 0, n, "UTF-8") : "";
    }

    static boolean waitFeed(File f, String needle, long ms) throws Exception {
        long end = System.currentTimeMillis() + ms;
        while (System.currentTimeMillis() < end) {
            if (readFeed(f).indexOf(needle) >= 0) return true;
            Thread.sleep(50);
        }
        return false;
    }

    static boolean waitCtx(FakeDm dm, int ctx, long ms) throws Exception {
        long end = System.currentTimeMillis() + ms;
        while (System.currentTimeMillis() < end) {
            if (dm.current() == ctx && ScreenModule.isMirrorActive() == (ctx == 81 || ctx == 82)) return true;
            Thread.sleep(100);
        }
        return false;
    }

    static int viewportEvents;

    public static void main(String[] args) throws Exception {
        AaLog.setEnabled(false);
        Log.setLevel(-1);

        /* ---------------- 1. pure rules ---------------- */
        final int OFF = TurnCard.MODE_OFF, STOCK = TurnCard.MODE_STOCK, CARD = TurnCard.MODE_CARD, PRE = TurnCard.MODE_PREARM;
        /*                      on     owner  nav    mirror kdk    haveMap map   large */
        check(TurnCard.planeMode(true, false, true, true, false, true, true, true) == OFF, "no owner -> off");
        check(TurnCard.planeMode(true, true, false, true, false, true, true, true) == OFF, "no route -> off");
        check(TurnCard.planeMode(false, true, true, true, false, true, true, true) == STOCK, "disabled -> luka stock");
        check(TurnCard.planeMode(true, true, true, false, false, true, true, true) == STOCK, "no mirror ctx (80) -> luka stock");
        check(TurnCard.planeMode(true, true, true, true, true, true, true, true) == STOCK, "KDK tile shown (popup) -> stock");
        check(TurnCard.planeMode(true, true, true, true, true, true, true, false) == STOCK, "KDK tile shown (in-tube) -> stock");
        check(TurnCard.planeMode(true, true, true, true, false, true, true, true) == CARD, "large map view, KDK hidden -> card");
        check(TurnCard.planeMode(true, true, true, true, false, true, true, false) == STOCK, "small map view, KDK hidden -> stock (0)");
        check(TurnCard.planeMode(true, true, true, true, false, true, false, true) == PRE, "VC shows no map -> prearm");
        check(TurnCard.planeMode(true, true, true, true, false, true, false, false) == PRE, "VC shows no map (small stage) -> prearm");
        check(TurnCard.planeMode(true, true, true, true, false, false, false, true) == STOCK, "map view never reported -> stock");

        check(ScreenModule.contextFor(true, false, true) == 82, "legacy: mirror, no route -> 82");
        check(ScreenModule.contextFor(true, false, true, true) == 81, "turn card: mirror, no route -> 81 held");
        check(ScreenModule.contextFor(true, true, true, true) == 81, "turn card: mirror + route -> 81");
        check(ScreenModule.contextFor(true, true, false, true) == 80, "turn card: no mirror + route -> luka 80");
        check(ScreenModule.contextFor(true, false, false, true) == 74, "turn card: no mirror, no route -> 74");
        check(ScreenModule.contextFor(false, true, true, true) == 74, "no session -> 74");

        /* ---------------- 2. text, record, token ---------------- */
        check(TurnCardFeed.formatDistance(4000, 3).equals("400 ft"), "400 ft");
        check(TurnCardFeed.formatDistance(3, 2).equals("0.3 mi"), "0.3 mi");
        check(TurnCardFeed.formatDistance(120, 2).equals("12 mi"), "12 mi");
        check(TurnCardFeed.formatDistance(1500, 0).equals("150 m"), "150 m");
        check(TurnCardFeed.formatDistance(12, 1).equals("1.2 km"), "1.2 km");
        check(TurnCardFeed.formatDistance(3000, 4).equals("300 yd"), "300 yd");
        check(TurnCardFeed.formatDistance(20, 5).equals((char) 0xBD + " mi"), "1/2 mi (quarter-mile unit)");
        check(TurnCardFeed.formatDistance(50, 5).equals("1" + (char) 0xBC + " mi"), "1 1/4 mi");
        check(TurnCardFeed.formatDistance(40, 5).equals("1 mi"), "4 quarters = 1 mi");
        check(TurnCardFeed.formatDistance(-1, 0).equals(""), "invalid -> empty");
        check(TurnCardFeed.formatDistance(100, 255).equals(""), "unknown unit -> empty");

        byte[] rec = TurnCardFeed.record(7, true, true, 1064, 14, 328, 180, "400 ft", "Locust\nSt  é");
        String rs = new String(rec, "UTF-8");
        check(rec.length == TurnCardFeed.RECORD_LEN && rec[rec.length - 1] == '\n', "record is fixed length, ends in newline");
        check(rs.startsWith("SQ5TC1 7\nroute=1 card=1 x=1064 y=14 w=328 h=180 bar=0 pm=0\ndist=400 ft\nstreet=Locust St é\nend=7\n"),
            "record layout: " + rs.substring(0, 90));
        String rb = new String(TurnCardFeed.record(9, true, true, 1064, 14, 328, 180, "150 ft", "Elm", 11, 1), "UTF-8");
        check(rb.startsWith("SQ5TC1 9\nroute=1 card=1 x=1064 y=14 w=328 h=180 bar=11 pm=1\ndist=150 ft\n"), "run 106: bar level + mode on the route line");
        StringBuffer longName = new StringBuffer();
        for (int i = 0; i < 200; i++) longName.append("ß");          /* 2 bytes each */
        String rl = new String(TurnCardFeed.record(8, true, false, 0, 0, 0, 0, "", longName.toString()), "UTF-8");
        int st = rl.indexOf("street=") + 7, en = rl.indexOf("\nend=8\n");
        check(en > st && rl.substring(st, en).getBytes("UTF-8").length <= 240 && rl.substring(st, en).indexOf('�') < 0,
            "long street cut to <= 240 bytes at a character boundary");

        check(MirrorGate.hasToken("12 345 1440x455 tc1", "tc1"), "token tc1 found");
        check(!MirrorGate.hasToken("12 345 1440x455", "tc1"), "no token");
        check(!MirrorGate.hasToken("12 345 1440x455 tc12", "tc1"), "tc12 is not tc1");
        check(!MirrorGate.hasToken("12 345 xtc1", "tc1"), "xtc1 is not tc1");
        check(TurnCard.parsePos("1000, 60")[0] == 1000 && TurnCard.parsePos("1000, 60")[1] == 60, "pos parse");
        check(TurnCard.parsePos("a,b") == null && TurnCard.parsePos("5") == null, "bad pos -> null");
        TurnCard.configure(true, new int[]{1300, 40});
        check(TurnCard.cardX() == TurnCard.DEFAULT_X && TurnCard.cardY() == TurnCard.DEFAULT_Y, "off-screen pos rejected");
        TurnCard.configure(true, null);
        /* 2026-09-29: the rule itself is unchanged; the full-view hiding (mapCard off) is applied by
         * ClusterLayerController with ScreenModule.isSmallScreenViewArea(), so Sport keeps its arrow. */
        check(TurnCard.planeMode(true, true, true, false, true, true, true) == TurnCard.MODE_CARD, "rule: large map view -> card");
        check(TurnCard.planeMode(true, true, true, true, true, true, true) == TurnCard.MODE_STOCK, "rule: KDK tile -> stock");
        TurnCard.setMapCard(true);
        /* the layer-controller scenarios below exercise the card itself */

        /* ---------------- 3. worker + layer controller ---------------- */
        File feed = new File(System.getProperty("java.io.tmpdir"), "sq5_turncard_test_" + System.currentTimeMillis());
        feed.deleteOnExit();
        TurnCardFeed.configure(true, feed.getPath());
        marker.delete();
        FakeDm fake = new FakeDm();
        IDisplayManagerKombiControl dm = (IDisplayManagerKombiControl) Proxy.newProxyInstance(
            TurnCardTest.class.getClassLoader(), new Class[]{IDisplayManagerKombiControl.class}, fake);
        ClusterLayerController.bind(dm, 1);
        ClusterLayerController.setViewportListener(new ClusterLayerController.ViewportListener() {
            public void onManeuverViewportChanged() { viewportEvents++; }
        });
        VcMapViewGate.resetForTest();
        VcMapViewGate.onStockGalState(new GalSink(), true);
        final ScreenModule sm = new ScreenModule();
        setStatic(ScreenModule.class, "connected", Boolean.TRUE);
        Field dmf = ScreenModule.class.getDeclaredField("dm"); dmf.setAccessible(true); dmf.set(sm, dm);
        Field enf = ScreenModule.class.getDeclaredField("enabled"); enf.setAccessible(true); enf.set(sm, Boolean.TRUE);
        final Method loop = ScreenModule.class.getDeclaredMethod("switchLoop"); loop.setAccessible(true);
        Thread w = new Thread(new Runnable() { public void run() { try { loop.invoke(sm); } catch (Throwable t) { } } });
        w.setDaemon(true);
        w.start();
        Thread hb = new Thread(new Runnable() {
            public void run() {
                while (true) {
                    try { if (beating) heartbeat(); Thread.sleep(200); } catch (Throwable t) { /* test thread */ }
                }
            }
        });
        hb.setDaemon(true);
        hb.start();

        /* VC in its large map view, KDK hidden (the owner's drive state) */
        ClusterLayerController.onVcPresentation(true);
        ClusterLayerController.onVcVisibility(false);
        ClusterLayerController.onVcMapVisibility(true);

        check(waitCtx(fake, 81, 4000), "mirror ready, no route -> 81 held (not 82)");
        Thread.sleep(400);
        String s = fake.take();
        check(s.indexOf("sc82") < 0, "never 82 while the turn card is on: " + s);
        check(last(s, "op98=") != null && last(s, "op98=").equals("0"), "no route: 98 at 0: " + s);
        check(waitFeed(feed, "route=0 card=0", 3000), "feed: no route, no card");

        /* route start: planes only, no context switch */
        viewportEvents = 0;
        ScreenModule.setNavActive(true);
        Thread.sleep(600);
        s = fake.take();
        check(s.indexOf("sc") < 0, "route start: no switchContext (81 held): " + s);
        check("59,27,210,153>1091,110".equals(last(s, "crop98=")), "card: full renderer frame at (1036,48): " + s);
        check("100".equals(last(s, "op98=")), "card: 98 opaque: " + s);
        check("0".equals(last(s, "op101=")) && "0".equals(last(s, "op102=")), "card: mirror draws the panel (tc1), backings off: " + s);
        int[] vp = ClusterLayerController.maneuverViewport();
        check(vp[0] == 59 && vp[1] == 27 && vp[2] == 210 && vp[3] == 153, "renderer visible area = card crop");
        check(viewportEvents == 1, "viewport listener fired once on entering card mode: " + viewportEvents);
        check(waitFeed(feed, "route=1 card=1 x=1091 y=84 w=210 h=153", 3000), "feed: card rect in mirror coords (26 px band offset)");
        TurnCardFeed.setDistance(4000, 3);
        TurnCardFeed.setStreet("Locust St");
        check(waitFeed(feed, "dist=400 ft\nstreet=Locust St\n", 3000), "feed: distance + street");

        /* VC pops its KDK tile up (popup stage): luka stock geometry */
        ClusterLayerController.onVcVisibility(true);
        s = fake.take();
        check("59,27,210,153>1091,110".equals(last(s, "crop98=")), "KDK popup: stock crop + anchor: " + s);
        check("100".equals(last(s, "op98=")) && "1091,110".equals(last(s, "pos102=")) && "100".equals(last(s, "op102=")),
            "KDK popup: 98 + backing 102 opaque: " + s);
        vp = ClusterLayerController.maneuverViewport();
        check(vp[0] == 59 && vp[2] == 210, "renderer visible area back to the popup crop");
        check(waitFeed(feed, "route=1 card=0", 3000), "feed: tile shown -> no card panel");

        ClusterLayerController.onVcVisibility(false);
        s = fake.take();
        check("59,27,210,153>1091,110".equals(last(s, "crop98=")) && "100".equals(last(s, "op98=")), "KDK closed -> card again: " + s);

        /* VC switches away from the map (other tab): prearm at the stock anchor */
        ClusterLayerController.onVcMapVisibility(false);
        s = fake.take();
        check("59,27,210,153>1091,110".equals(last(s, "crop98=")) && "100".equals(last(s, "op98="))
            && "100".equals(last(s, "op102=")), "no map: prearmed at the KDK anchor, opaque: " + s);

        /* small map view (in-tube stage), KDK hidden: luka (opacity 0) */
        ClusterLayerController.onVcMapVisibility(true);
        ClusterLayerController.onVcPresentation(false);
        s = fake.take();
        check("0".equals(last(s, "op98=")) && "0,0,328,180>984,139".equals(last(s, "crop98=")),
            "small map view, KDK hidden: stock in-tube geometry at opacity 0: " + s);
        ClusterLayerController.onVcVisibility(true);
        s = fake.take();
        check("100".equals(last(s, "op98=")) && "984,139".equals(last(s, "pos101=")) && "100".equals(last(s, "op101=")),
            "small map view, KDK in-tube shown: luka stock: " + s);
        ClusterLayerController.onVcVisibility(false);

        /* mirror without the panel capability -> 987 backing under the card */
        markerTail = "";
        Thread.sleep(700);
        ClusterLayerController.onVcPresentation(true);
        s = fake.take();
        check(!MirrorGate.drawsCardPanel(), "marker without tc1 -> no mirror panel");
        check("1091,110".equals(last(s, "pos101=")) && "100".equals(last(s, "op101=")) && "100".equals(last(s, "op98=")),
            "card with 987 backing 101 at the card rect: " + s);
        markerTail = " tc1";
        Thread.sleep(700);
        s = fake.take();
        check(MirrorGate.drawsCardPanel() && "0".equals(last(s, "op101=")),
            "tc1 back -> the worker's poll reapplies: backing off again: " + s);

        /* route end (KDK hidden): planes off at once, context held */
        ScreenModule.setNavActive(false);
        Thread.sleep(600);
        s = fake.take();
        check(s.indexOf("sc") < 0, "route end: no switchContext: " + s);
        check("0".equals(last(s, "op98=")) && "0".equals(last(s, "op101=")) && "0".equals(last(s, "op102=")),
            "route end: 98/101/102 at 0: " + s);
        check(waitFeed(feed, "route=0 card=0", 3000), "feed: route end -> no route, no card");

        /* disabled (sq5_turncard_off): legacy 82 without a route */
        TurnCard.configure(false, null);
        ScreenModule.setNavActive(false);
        check(waitCtx(fake, 82, 4000), "turn card off -> legacy 82 without a route");
        ScreenModule.setNavActive(true);
        check(waitCtx(fake, 81, 4000), "turn card off -> legacy 81 with a route");
        Thread.sleep(300);
        s = fake.take();
        check("0".equals(last(s, "op98=")), "turn card off, large map, KDK hidden: luka keeps 98 at 0: " + s);

        beating = false;
        sm.stop();
        waitCtx(fake, 74, 4000);
        marker.delete();
        System.out.println((failures == 0 ? "PASS" : "FAILED") + ": TurnCardTest " + checks + " checks, " + failures + " failures");
        System.exit(failures == 0 ? 0 : 1);
    }
}
