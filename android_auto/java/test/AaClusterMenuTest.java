import com.sq5.aa.input.AaClusterMenu;

import java.io.File;
import java.io.FileInputStream;

/** Cockpit options menu state machine (host test, no car). */
public final class AaClusterMenuTest {
    static int checks, failures;
    static void check(boolean ok, String what) { checks++; if (!ok) { failures++; System.out.println("FAIL " + what); } }

    /* right arrow (100): press at t0, a repeat (state 4) at t0 + ms; returns what that repeat returned */
    static boolean holdRight(long t0, long ms) {
        AaClusterMenu.onKey(4, 100, 1, t0);
        boolean r = AaClusterMenu.onKey(4, 100, 4, t0 + ms);
        AaClusterMenu.onKey(4, 100, 0, t0 + ms + 100);
        return r;
    }

    static String rec(File f) throws Exception {
        FileInputStream in = new FileInputStream(f);
        byte[] b = new byte[600];
        int n = in.read(b);
        in.close();
        return new String(b, 0, n);
    }

    public static void main(String[] args) throws Exception {
        File f = new File(System.getProperty("java.io.tmpdir"), "sq5_menu_test_" + System.currentTimeMillis());
        f.deleteOnExit();
        AaClusterMenu.resetForTest();
        AaClusterMenu.setTestHooks(f.getPath(), Boolean.FALSE);
        long t = 1000000L;

        /* run 137: opens only while the cockpit shows the cluster map, only on the right arrow held 2 s */
        check(!holdRight(t - 5000, 2100), "no cluster map -> not opened");
        check(!AaClusterMenu.isOpen(), "still closed");
        AaClusterMenu.setTestHooks(f.getPath(), Boolean.TRUE);
        check(!AaClusterMenu.onKey(4, 100, 1, t - 900) && !AaClusterMenu.onKey(4, 100, 0, t - 700), "short press ignored (Audi keeps it)");
        check(!holdRight(t - 600, 1500) && !AaClusterMenu.isOpen(), "held 1.5 s: not yet");
        check(!AaClusterMenu.onKey(1, 100, 3, t - 400), "centre console board ignored");
        check(!AaClusterMenu.onRoller(-1, t), "roller not consumed while closed");
        check(!AaClusterMenu.consumeSelect(t), "no select consumed while closed");
        AaClusterMenu.onKey(4, 100, 1, t - 2100);
        check(AaClusterMenu.onKey(4, 100, 3, t) && AaClusterMenu.isOpen(), "right arrow held 2.1 s opens");
        check(!AaClusterMenu.onKey(4, 100, 4, t + 1) && AaClusterMenu.isOpen(), "same hold fires once");
        AaClusterMenu.onKey(4, 100, 0, t + 2);
        check(AaClusterMenu.isOpen() && AaClusterMenu.selection() == 0, "open at item 0");
        String r = rec(f);
        check(r.startsWith("SQ5M1 ") && r.indexOf("\nend ") > 0 && r.length() == 512, "record 512 bytes with seq/end: " + r.length());
        check(r.indexOf("Map up/down\t0") > 0 && r.indexOf("Arrow tile\tAudi") > 0 && r.indexOf("Close\t") > 0, "items");

        /* roller moves the selection, wraps */
        check(AaClusterMenu.onRoller(-1, t + 10), "roller consumed while open");
        check(AaClusterMenu.selection() == 1, "roller +1 -> item 1");
        for (int i = 0; i < 4; i++) AaClusterMenu.onRoller(-1, t + 20 + i);
        check(AaClusterMenu.selection() == 0, "wraps to item 0");
        AaClusterMenu.onRoller(1, t + 40);
        check(AaClusterMenu.selection() == 4, "wraps backwards to Close");
        AaClusterMenu.onRoller(-1, t + 50);

        /* press: edit Map up/down, roller changes the value in steps of 10, clamped */
        check(AaClusterMenu.onKey(4, 40, 1, t + 60), "roller press used");
        check(AaClusterMenu.consumeSelect(t + 61), "collapsed DDS_SELECT consumed");
        check(AaClusterMenu.isEdit(), "edit mode");
        AaClusterMenu.onRoller(-1, t + 70); AaClusterMenu.onRoller(-1, t + 80);
        check(AaClusterMenu.up() == 20, "up 20: " + AaClusterMenu.up());
        for (int i = 0; i < 40; i++) AaClusterMenu.onRoller(-1, t + 90 + i);
        check(AaClusterMenu.up() == 200, "clamped at 200");
        for (int i = 0; i < 40; i++) AaClusterMenu.onRoller(1, t + 200 + i);
        check(AaClusterMenu.up() == -60, "clamped at -60");
        for (int i = 0; i < 10; i++) AaClusterMenu.onRoller(-1, t + 300 + i);
        check(AaClusterMenu.up() == 40, "back to 40");
        check(rec(f).indexOf("SQ5M1 ") == 0 && rec(f).indexOf(" 40 0\n") > 0, "record carries up=40 (Sport 0)");
        AaClusterMenu.onKey(4, 40, 1, t + 400);
        check(!AaClusterMenu.isEdit(), "press leaves edit mode");

        /* per view: in Sport the same item edits the Sport value only */
        AaClusterMenu.setTestSmallView(Boolean.TRUE);
        AaClusterMenu.onKey(4, 40, 1, t + 410);
        AaClusterMenu.onRoller(1, t + 420); AaClusterMenu.onRoller(1, t + 430);
        AaClusterMenu.onKey(4, 40, 1, t + 440);
        check(AaClusterMenu.upSmall() == -20 && AaClusterMenu.up() == 40, "Sport edit leaves full value: " + AaClusterMenu.upSmall() + "/" + AaClusterMenu.up());
        check(rec(f).indexOf(" 40 -20\n") > 0 && rec(f).indexOf("Map up/down (Sport)\t-20") > 0, "record carries both, label per view");
        AaClusterMenu.setTestSmallView(Boolean.FALSE);
        /* Size: press cycles Small -> Medium -> Large (Medium default); Resolution: press toggles 1080p/720p */
        AaClusterMenu.onRoller(-1, t + 450); AaClusterMenu.onRoller(-1, t + 451);
        check(AaClusterMenu.selection() == 2, "on Size");
        check(AaClusterMenu.size() == 3 && rec(f).indexOf("Size (on reconnect)\tLarge") > 0, "size Large by default (run 154)");
        AaClusterMenu.onKey(4, 40, 1, t + 455);
        check(AaClusterMenu.size() == 4 && rec(f).indexOf("Size (on reconnect)\tX-Large") > 0, "press -> X-Large");
        AaClusterMenu.onKey(4, 40, 1, t + 457);
        check(AaClusterMenu.size() == 5 && rec(f).indexOf("Size (on reconnect)\tXX-Large") > 0, "press -> XX-Large");
        AaClusterMenu.onKey(4, 40, 1, t + 460);
        check(AaClusterMenu.size() == 1 && rec(f).indexOf("Size (on reconnect)\tSmall") > 0, "press -> Small (wraps)");
        AaClusterMenu.onKey(4, 40, 1, t + 470);
        check(AaClusterMenu.size() == 2 && !AaClusterMenu.isEdit(), "press -> Medium, no edit mode");
        AaClusterMenu.onKey(4, 40, 1, t + 480);
        check(AaClusterMenu.size() == 3, "press -> Large again");
        AaClusterMenu.onRoller(-1, t + 513);
        check(AaClusterMenu.selection() == 3 && AaClusterMenu.res() == 3, "on Resolution, 1080p by default");
        AaClusterMenu.onKey(4, 40, 1, t + 514);
        check(AaClusterMenu.res() == 2 && rec(f).indexOf("Resolution (on reconnect)\t720p") > 0, "toggled to 720p");
        /* Close item closes; its press is still consumed for the centre screen */
        /* run 137: no Roller zoom item any more (Digital zoom removed; the roller always zooms the map) */
        AaClusterMenu.onRoller(-1, t + 517);
        check(AaClusterMenu.selection() == 4 && AaClusterMenu.zoomMode() == 1 && rec(f).indexOf("Roller zoom") < 0 && rec(f).indexOf("Map theme") < 0, "on Close, no Roller zoom / Map theme item");
        AaClusterMenu.onKey(4, 40, 1, t + 520);
        check(rec(f).length() == 512, "record still 512 bytes with 5 items");
        check(!AaClusterMenu.isOpen(), "Close closes");
        check(AaClusterMenu.consumeSelect(t + 600), "select right after closing still consumed");
        check(!AaClusterMenu.consumeSelect(t + 2000), "later centre-knob select not consumed");
        check(!AaClusterMenu.onRoller(-1, t + 2000), "roller back to zoom after closing");
        check(rec(f).indexOf("SQ5M1 ") == 0 && AaClusterMenu.up() == 40, "value kept after closing");

        /* run 137: the other buttons no longer open it */
        check(!AaClusterMenu.onKey(4, 36, 2, t + 2100) && !AaClusterMenu.onKey(4, 99, 3, t + 2200) && !AaClusterMenu.isOpen(), "36 / 99 held: no menu");
        /* the button left of the roller (41; 39 accepted too) held 2 s opens / closes as well */
        AaClusterMenu.onKey(4, 41, 1, t + 2300);
        check(!AaClusterMenu.onKey(4, 41, 0, t + 3300) && !AaClusterMenu.isOpen(), "41 held 1 s: nothing");
        AaClusterMenu.onKey(4, 41, 1, t + 3400);
        check(AaClusterMenu.onKey(4, 41, 0, t + 5500) && AaClusterMenu.isOpen(), "41 held 2.1 s opens");
        AaClusterMenu.onKey(4, 39, 1, t + 5600);
        check(AaClusterMenu.onKey(4, 39, 0, t + 7700) && !AaClusterMenu.isOpen(), "39 held 2.1 s closes");
        /* hold with no repeat events: counted at release */
        AaClusterMenu.onKey(4, 100, 1, t + 7800);
        check(AaClusterMenu.onKey(4, 100, 0, t + 9900) && AaClusterMenu.isOpen(), "right arrow released after 2.1 s opens");
        check(holdRight(t + 10000, 2000) && !AaClusterMenu.isOpen(), "held 2 s again closes");
        System.out.println((failures == 0 ? "PASS" : "FAIL") + ": AaClusterMenuTest " + checks + " checks, " + failures + " failures");
        if (failures != 0) System.exit(1);
    }
}
