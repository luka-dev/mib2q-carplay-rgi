package com.sq5.aa.input;

import com.sq5.aa.luka.AaLog;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.RandomAccessFile;

/**
 * Cockpit options menu, milestone 1 (owner 2026-09-29): our own panel drawn by the cluster player over the
 * Android Auto cockpit map, operated from the steering wheel.
 *
 *   hold the wheel button key 36 (DSIKeyPanel board 4, state 2/3 = held) -> open / close
 *     (a short press still opens Audi's own side menu, untouched)
 *   roller turn (BAP MapScale, ScreenCombiBAPListener.setMapScale) -> move the selection, or change the
 *     value of the item being edited; consumed while open (no zoom, nothing to the centre screen)
 *   roller press (board 4 key 40) -> select: edit a value / toggle / Close; the collapsed DDS_SELECT the
 *     keyboard stack posts to Android Auto for that press is consumed (AndroidAuto2KeyEventsController)
 *   15 s without input, or the cockpit leaving the cluster map -> closes
 *
 * Items: Map up/down (live picture offset in the player), Arrow box in map view (TurnCard), Close.
 * State for the player: /tmp/sq5_cluster_menu, a fixed 512-byte text record written in place (shmem /tmp:
 * no rename, no truncate) with a leading and trailing sequence number so a torn read is ignored.
 * Settings persist in /mnt/persist/var/app/sq5_cluster/menu.properties (the car, not the SD card).
 * SD flag sq5_menu_off disables the menu (inputs behave as before).
 */
public final class AaClusterMenu {
    private static final String REC = "/tmp/sq5_cluster_menu";
    private static final String DIR = "/mnt/persist/var/app/sq5_cluster";
    private static final String SETTINGS = DIR + "/menu.properties";
    private static final String[] OFF = { "/fs/sda0/sq5_menu_off", "/net/mmx/fs/sda0/sq5_menu_off" };
    private static final int KBD_MFW = 4, KBD_MFW_3GP = 8;
    private static final int KEY_ROLLER = 40, KEY_ARROW_A = 36, KEY_ARROW_B = 37, KEY_MENU_B = 41;
    /* Run 84/85: the owner's left/right arrow buttons are keys 99/100; holding them reports state 3 (long),
     * then 4/5 while still held, 0 on release. Keys 36/37 report a long press as state 2. */
    private static final int KEY_ARROW_L = 99, KEY_ARROW_R = 100, ST_LONG_ARROW = 3;
    private static final int ST_RELEASED = 0, ST_PRESSED = 1, ST_LONG = 2;
    private static final long IDLE_CLOSE_MS = 15000L, SELECT_WINDOW_MS = 700L;
    static final int ITEM_UP = 0, ITEM_ARROW = 1, ITEM_SIZE = 2, ITEM_RES = 3, ITEM_CLOSE = 4, ITEMS = 5;   /* run 162: Map theme removed (Light crashed Android Auto) */
    /* Owner 2026-09-29: density and resolution from the menu (the phone reads both when it connects, so they
     * apply from the next connection). size 1..3 = Small/Medium/Large (the hook maps it to a density per resolution);
     * res 3 = 1080p, 2 = 720p. Read by the hook (src/uiconfig.c live_dpi / live_res) from the settings file. */
    /* Owner 2026-09-30: three sizes instead of density numbers; the hook maps them per resolution
     * (1080p 1:1: 95 / 110 / 125, 720p: 105 / 120 / 140). */
    /* run 149 (owner photo: wants the bigger, zoomed-in Waze look) -> two larger steps */
    static final String[] SIZE_NAMES = { "", "Small", "Medium", "Large", "X-Large", "XX-Large" };
    static final int UP_MIN = -60, UP_MAX = 200, UP_STEP = 10;

    private static boolean open, edit;
    private static int sel, seq;
    private static int up;                 /* player picture shift up, full view, cockpit px */
    /* Owner 2026-09-29: margins are shared by both views, so tuning the full view broke Sport -> Map up/down
     * edits the view the cockpit is in; full and Sport values are saved separately. */
    private static int upSmall;            /* Sport (small) view */
    static Boolean testSmallView;
    private static boolean arrow;          /* arrow box in the large map view */
    private static int size = 3;           /* run 154 (owner): Large (125 dpi = the VC panel's physical 125 PPI) is the default */
    /* 2026-09-30: Roller zoom Digital (player enlarges the video) or Map (the hook resizes Google's layout so
     * Google redraws closer, sharp; card pinned by the player). Read by the hook as zoomMode=0/1. */
    /* Run 137 (owner): Digital zoom removed - it enlarged the video (blurry) and dropped the cockpit to ~7 fps.
     * The roller always zooms the map (hook resizes Google's layout); kept as a constant for the record. */
    private static final int zoomMode = 1;
    private static int res = 3;            /* 3 = 1080p (owner 2026-09-30: target 1080p), 2 = 720p */
    private static long lastInputMs;
    private static long selectUntilMs;     /* consume the collapsed DDS_SELECT of a menu roller press */
    /* Run 137 (owner): the menu opens / closes only on a 2 s hold of the right arrow (100) or the button left of
     * the roller (41 in runs 90/91; owner thinks 39, never seen in a log - accepted too). 36 and 99 no longer
     * open it. The hold is timed here from the first event of the press; it fires once per hold, while still
     * held (repeat states 3/4/5) or at release if no repeat arrived after 2 s. */
    private static final int KEY_LEFT_OF_ROLLER = 39;
    private static int heldKey;            /* menu key being held, 0 = none */
    private static long pressedRMs;        /* its press time */
    private static boolean firedR;         /* this hold already toggled the menu */
    static final long HOLD_MS = 2000L;
    private static boolean loaded;
    private static Thread watcher;
    static String testRecPath;             /* host tests */
    static Boolean testClusterShown;

    private AaClusterMenu() { }

    static boolean off() {
        for (int i = 0; i < OFF.length; i++) {
            try { if (new File(OFF[i]).exists()) return true; } catch (Throwable t) { /* ignore */ }
        }
        return false;
    }

    /** Raw steering-wheel key (AaWheelProbe listener). Returns true when the menu used it. */
    public static synchronized boolean onKey(int board, int code, int state, long now) {
        if (board != KBD_MFW && board != KBD_MFW_3GP) return false;
        load();
        boolean longPress = false;
        if (code == KEY_ARROW_R || code == KEY_MENU_B || code == KEY_LEFT_OF_ROLLER) {
            if (state == ST_RELEASED) {
                boolean late = heldKey == code && !firedR && now - pressedRMs >= HOLD_MS;
                if (heldKey == code) { heldKey = 0; firedR = false; }
                if (!late) return false;
                longPress = true;
            } else {
                if (heldKey != code) { heldKey = code; pressedRMs = now; firedR = false; }
                if (firedR || now - pressedRMs < HOLD_MS) return false;
                firedR = true; longPress = true;
            }
        }
        if (longPress) {
            if (open) { close("long press", now); return true; }
            if (off() || !clusterShown(now)) {
                AaLog.log("menu: key " + code + " held, not opened (" + (off() ? "sq5_menu_off" : "cockpit map not shown") + ")");
                return false;
            }
            open = true; edit = false; sel = ITEM_UP; lastInputMs = now;
            AaLog.log("menu: open (key " + code + " long press)");
            publish();
            startWatcher();
            return true;
        }
        if (code == KEY_ROLLER && open) {
            selectUntilMs = now + SELECT_WINDOW_MS;
            if (state == ST_PRESSED) select(now);
            return true;
        }
        return false;
    }

    /** Roller turn (BAP MapScale steps). True = consumed by the open menu. */
    public static synchronized boolean onRoller(int steps, long now) {
        if (!open || steps == 0) return false;
        lastInputMs = now;
        int d = steps > 0 ? -1 : 1;   /* run 137 (owner): the menu scrolled the wrong way */
        if (edit && sel == ITEM_UP) {
            if (smallView()) upSmall = clamp(upSmall + d * UP_STEP, UP_MIN, UP_MAX);
            else up = clamp(up + d * UP_STEP, UP_MIN, UP_MAX);
            save();
        } else {
            sel = (sel + d + ITEMS) % ITEMS;
        }
        publish();
        return true;
    }

    /** AndroidAuto2KeyEventsController: consume the DDS_SELECT that came from a menu roller press. */
    public static synchronized boolean consumeSelect(long now) {
        return open || now <= selectUntilMs;
    }

    private static void select(long now) {
        lastInputMs = now;
        if (sel == ITEM_UP) {
            edit = !edit;
        } else if (sel == ITEM_SIZE) {
            size = size % 5 + 1;              /* press cycles Small -> Medium -> Large -> X-Large -> XX-Large */
            save();
        } else if (sel == ITEM_RES) {
            res = res == 3 ? 2 : 3;
            save();
        } else if (sel == ITEM_ARROW) {
            arrow = !arrow;
            applyArrow();
            save();
        } else {
            close("Close", now);
            return;
        }
        publish();
    }

    private static void close(String why, long now) {
        open = false; edit = false;
        selectUntilMs = now + SELECT_WINDOW_MS;
        AaLog.log("menu: closed (" + why + ") up=" + up + " upSmall=" + upSmall + " arrow=" + arrow + " size=" + size + " res=" + res);
        publish();
    }

    static int clamp(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

    /** Current picture shift for the player (also published while the menu is closed). */
    public static synchronized int up() { load(); return up; }
    public static synchronized int upSmall() { load(); return upSmall; }

    static boolean smallView() {
        if (testSmallView != null) return testSmallView.booleanValue();
        try { return com.luka.carplay.core.ScreenModule.isSmallScreenViewArea(); } catch (Throwable t) { return false; }
    }

    private static boolean clusterShown(long now) {
        if (testClusterShown != null) return testClusterShown.booleanValue();
        return AaRollerInput.clusterMapShown(now);
    }

    private static void applyArrow() {
        try {
            com.sq5.aa.luka.TurnCard.setMapCard(arrow);
            com.luka.carplay.cluster.ClusterLayerController.reapply();
        } catch (Throwable t) {
            AaLog.log("menu: arrow box apply failed: " + t);
        }
    }

    /** Settings from the car's persist partition; published once so the player applies the shift at boot. */
    public static synchronized void load() {
        if (loaded) return;
        loaded = true;
        FileInputStream in = null;
        try {
            in = new FileInputStream(SETTINGS);
            byte[] b = new byte[256];
            int n = in.read(b);
            String s = n > 0 ? new String(b, 0, n) : "";
            up = clamp(intValue(s, "upFull=", 0), UP_MIN, UP_MAX);
            upSmall = clamp(intValue(s, "upSport=", 0) /* run 127: old upSmall=200 pushed the Sport card off the top -> new key, Sport starts at 0 */, UP_MIN, UP_MAX);
            arrow = intValue(s, "arrowBox=", 1) != 0;   /* run 102: new key, default on (hiding left an empty transparent box) */
            size = clamp(intValue(s, "size=", 3), 1, 5);   /* 2026-09-30: replaces density= (old values ignored) */
            res = intValue(s, "stream=", 3) == 2 ? 2 : 3;   /* run 108: new keys - the old up=-60 / res= are baked into the defaults */
            AaLog.log("menu: settings up=" + up + " upSmall=" + upSmall + " arrow=" + arrow + " size=" + size + " res=" + res);
        } catch (Throwable t) {
            arrow = true;
            AaLog.log("menu: no saved settings (" + t.getClass().getName() + "), defaults");
        } finally {
            if (in != null) try { in.close(); } catch (Throwable t) { /* ignore */ }
        }
        if (arrow) applyArrow();
        publish();
    }

    static int intValue(String s, String key, int dflt) {
        int i = s.indexOf(key);
        if (i < 0) return dflt;
        int j = i + key.length(), k = j;
        if (k < s.length() && s.charAt(k) == '-') k++;
        while (k < s.length() && Character.isDigit(s.charAt(k))) k++;
        try { return Integer.parseInt(s.substring(j, k)); } catch (Throwable t) { return dflt; }
    }

    private static void save() {
        FileOutputStream o = null;
        try {
            new File(DIR).mkdirs();
            o = new FileOutputStream(SETTINGS);
            o.write(("upFull=" + up + "\nupSport=" + upSmall + "\narrowBox=" + (arrow ? 1 : 0) + "\nrollerZoom=" + zoomMode + "\nsize=" + size
                + "\nstream=" + res + "\n").getBytes());
        } catch (Throwable t) {
            AaLog.log("menu: settings not saved: " + t);
        } finally {
            if (o != null) try { o.close(); } catch (Throwable t) { /* ignore */ }
        }
    }

    /** The record the player draws from. */
    public static synchronized String record() {
        StringBuffer s = new StringBuffer();
        s.append("SQ5M1 ").append(seq).append(' ').append(open ? 1 : 0).append(' ').append(sel).append(' ')
            .append(edit ? 1 : 0).append(' ').append(up).append(' ').append(upSmall).append('\n');
        boolean small = smallView();
        int v = small ? upSmall : up;
        s.append(small ? "Map up/down (Sport)\t" : "Map up/down\t").append(v > 0 ? "+" : "").append(v)
            .append(edit && sel == ITEM_UP ? " <>" : "").append('\n');
        s.append("Arrow tile\t").append(arrow ? "Always" : "Audi").append('\n');   /* Audi = only near turns, like stock */
        s.append("Size (on reconnect)\t").append(SIZE_NAMES[size]).append('\n');
        s.append("Resolution (on reconnect)\t").append(res == 2 ? "720p" : "1080p").append('\n');
        s.append("Close\t\n");
        s.append("end ").append(seq).append('\n');
        while (s.length() < 511) s.append(' ');
        s.append('\n');
        return s.toString();
    }

    private static void publish() {
        seq++;
        RandomAccessFile f = null;
        try {
            f = new RandomAccessFile(testRecPath != null ? testRecPath : REC, "rw");
            f.seek(0);
            f.write(record().getBytes());
        } catch (Throwable t) {
            AaLog.log("menu: publish failed: " + t);
        } finally {
            if (f != null) try { f.close(); } catch (Throwable t) { /* ignore */ }
        }
    }

    /** Idle close and close when the cockpit leaves the cluster map. */
    private static void startWatcher() {
        if (watcher != null) return;
        watcher = new Thread(new Runnable() {
            public void run() {
                while (true) {
                    try { Thread.sleep(1000L); } catch (InterruptedException e) { return; }
                    long now = System.currentTimeMillis();
                    synchronized (AaClusterMenu.class) {
                        if (!open) continue;
                        if (now - lastInputMs > IDLE_CLOSE_MS) close("idle", now);
                        else if (!clusterShown(now)) close("cockpit left the cluster map", now);
                    }
                }
            }
        }, "sq5-cluster-menu");
        watcher.setDaemon(true);
        watcher.start();
    }

    public static synchronized void setTestHooks(String rec, Boolean shown) { testRecPath = rec; testClusterShown = shown; }
    public static synchronized void setTestSmallView(Boolean small) { testSmallView = small; }

    public static synchronized void resetForTest() {
        open = false; edit = false; sel = 0; seq = 0; up = 0; upSmall = 0; arrow = false; size = 3; res = 3; lastInputMs = 0; selectUntilMs = 0;
        loaded = true;
    }

    public static synchronized boolean isOpen() { return open; }
    public static synchronized int selection() { return sel; }
    public static synchronized boolean isEdit() { return edit; }
    public static synchronized boolean arrowOn() { return arrow; }
    public static synchronized int size() { return size; }
    public static synchronized int zoomMode() { return zoomMode; }
    public static synchronized int res() { return res; }
}
