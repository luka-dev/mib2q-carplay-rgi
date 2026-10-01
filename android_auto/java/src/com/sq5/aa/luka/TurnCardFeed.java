/*
 * Java -> sq5_mirror turn-card feed: /tmp/sq5_turncard, one fixed-length record rewritten IN PLACE
 * (offset 0, one write of RECORD_LEN bytes, never truncated or renamed: /tmp on the unit is /dev/shmem,
 * which has no rename - same contract as /tmp/sq5_mirror_ready).
 *
 * Record (UTF-8 text, '\n'-separated, space-padded to RECORD_LEN, last byte '\n'):
 *   SQ5TC1 <seq>
 *   route=<0|1> card=<0|1> x=<bx> y=<by> w=<w> h=<h> bar=<0..16> pm=<0..3>
 *   dist=<formatted distance, e.g. "0.3 mi", "400 ft", "150 m", "1.2 km", "1/2 mi" (U+00BD)>
 *   street=<next road / signpost, the text luka publishes on FctID 19 without its decorations>
 *   end=<seq>
 * route = Android Auto route guidance shown with the mirror (masks for Google's ETA card / speed box);
 * card  = ClusterLayerController placed 98 as the turn card; x/y/w/h = the card rect in the MIRROR
 *         frame (terminal rect minus the mirror plane's position), so sq5_mirror draws its panel and
 *         text strip exactly behind/under plane 98.
 * The reader accepts a record only when both seq values match (a read racing the write is dropped).
 * seq also advances every HEARTBEAT_MS while the feed is enabled, so the mirror can tell a live HMI
 * from a stale file (it treats a record unchanged for > 6 s as "no route, no card").
 *
 * bar/pm (run 106, owner: the cockpit box shows no distance bars, the HUD does): the approach bargraph luka
 * sends the renderer - level 0..16 and progress mode 0 off, 1 fill, 2 blink low, 3 blink high. Appended to
 * the route line so older readers (sscanf of the six fields) ignore it. The cluster player draws the bars.
 *
 * Distance text: the same BAP value/unit pair luka sends on FctID 18 (MU0918 BAPDistanceFormatter:
 * value = tenths; unit 0 m, 1 km, 2 mi, 3 ft, 4 yd, 5 quarter miles with value = 10 * quarters).
 *
 * All I/O on one daemon thread (latest wins); callers never block and never see an exception.
 */
package com.sq5.aa.luka;

import java.io.RandomAccessFile;

public final class TurnCardFeed {
    public static final String PATH = "/tmp/sq5_turncard";
    public static final int RECORD_LEN = 512;
    static final int STREET_MAX_BYTES = 240;
    static final long HEARTBEAT_MS = 2000L;

    private static final Object LOCK = new Object();
    private static boolean enabled;             /* guarded by LOCK */
    private static String path = PATH;
    private static boolean route, card;
    private static int bx, by, bw, bh;
    private static String dist = "", street = "";
    private static int barLevel, barMode;
    private static long seq;
    private static boolean dirty;
    private static Thread writer;
    private static String lastLogged;
    private static int writes, writeFailures;

    private TurnCardFeed() {
    }

    /** AaLukaBridge.attach; host tests pass their own path. */
    public static void configure(boolean on, String p) {
        synchronized (LOCK) {
            enabled = on;
            path = p != null ? p : PATH;
            dirty = true;
            if (on && writer == null) {
                Thread t = new Thread("sq5-turncard") {
                    public void run() { writeLoop(); }
                };
                t.setDaemon(true);
                t.start();
                writer = t;
            }
            LOCK.notifyAll();
        }
    }

    /** Layer state from ClusterLayerController (card rect already in mirror-frame coordinates). */
    public static void setLayer(boolean routeActive, boolean cardShown, int x, int y, int w, int h) {
        synchronized (LOCK) {
            if (!cardShown) { x = y = w = h = 0; }
            if (route == routeActive && card == cardShown && bx == x && by == y && bw == w && bh == h) return;
            route = routeActive;
            card = cardShown;
            bx = x; by = y; bw = w; bh = h;
            markDirtyLocked();
        }
    }

    /** FctID 18 value/unit exactly as sent to BAP (value <= 0 or invalid unit: no distance). */
    public static void setDistance(int bapValue, int bapUnit) {
        String d = formatDistance(bapValue, bapUnit);
        synchronized (LOCK) {
            if (d.equals(dist)) return;
            dist = d;
            markDirtyLocked();
        }
    }

    /** Approach bargraph as sent to maneuver_render (level 0..16, RendererServer.PROGRESS_* mode). */
    public static void setBar(int level, int mode) {
        if (level < 0) level = 0;
        if (level > 16) level = 16;
        if (mode < 0 || mode > 3) mode = 0;
        synchronized (LOCK) {
            if (barLevel == level && barMode == mode) return;
            barLevel = level;
            barMode = mode;
            markDirtyLocked();
        }
    }

    public static void setStreet(String s) {
        String t = clean(s);
        synchronized (LOCK) {
            if (t.equals(street)) return;
            street = t;
            markDirtyLocked();
        }
    }

    /** Session end: nothing to show. */
    public static void clear() {
        synchronized (LOCK) {
            route = card = false;
            bx = by = bw = bh = 0;
            dist = street = "";
            barLevel = barMode = 0;
            markDirtyLocked();
        }
    }

    private static void markDirtyLocked() {
        dirty = true;
        LOCK.notifyAll();
    }

    /** BAP distance -> display text.  "" = no distance. */
    public static String formatDistance(int value, int unit) {
        if (value <= 0) return "";
        switch (unit) {
            case 0: return tenths(value) + " m";
            case 1: return tenths(value) + " km";
            case 2: return tenths(value) + " mi";
            case 3: return tenths(value) + " ft";
            case 4: return tenths(value) + " yd";
            case 5: {
                int q = value / 10, whole = q / 4, frac = q % 4;
                /* Latin-1 vulgar fractions U+00BC/BD/BE (in sq5_mirror's font) */
                String f = frac == 0 ? "" : String.valueOf((char) (0xBB + frac));
                if (whole == 0 && f.length() == 0) return "";
                return (whole > 0 ? String.valueOf(whole) : "") + f + " mi";
            }
            default: return "";
        }
    }

    private static String tenths(int v) {
        int i = v / 10, d = v % 10;
        return d == 0 ? String.valueOf(i) : i + "." + d;
    }

    /** One line, no control characters, whitespace collapsed. */
    static String clean(String s) {
        if (s == null) return "";
        StringBuffer b = new StringBuffer(s.length());
        boolean space = false;
        for (int i = 0; i < s.length(); i++) {
            char c = s.charAt(i);
            if (c < 0x20 || c == 0x7f || Character.isWhitespace(c)) {
                space = b.length() > 0;
                continue;
            }
            if (space) { b.append(' '); space = false; }
            b.append(c);
        }
        return b.toString();
    }

    /** UTF-8 of s cut at a character boundary to at most max bytes (surrogate pairs kept whole). */
    static byte[] utf8Limit(String s, int max) {
        byte[] out = new byte[max];
        int n = 0;
        for (int i = 0; i < s.length(); i++) {
            int cp = s.charAt(i);
            int adv = 1;
            if (cp >= 0xD800 && cp <= 0xDBFF && i + 1 < s.length()) {
                int lo = s.charAt(i + 1);
                if (lo >= 0xDC00 && lo <= 0xDFFF) { cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00); adv = 2; }
            }
            int len = cp < 0x80 ? 1 : cp < 0x800 ? 2 : cp < 0x10000 ? 3 : 4;
            if (n + len > max) break;
            if (len == 1) out[n++] = (byte) cp;
            else if (len == 2) { out[n++] = (byte) (0xC0 | (cp >> 6)); out[n++] = (byte) (0x80 | (cp & 0x3F)); }
            else if (len == 3) {
                out[n++] = (byte) (0xE0 | (cp >> 12)); out[n++] = (byte) (0x80 | ((cp >> 6) & 0x3F));
                out[n++] = (byte) (0x80 | (cp & 0x3F));
            } else {
                out[n++] = (byte) (0xF0 | (cp >> 18)); out[n++] = (byte) (0x80 | ((cp >> 12) & 0x3F));
                out[n++] = (byte) (0x80 | ((cp >> 6) & 0x3F)); out[n++] = (byte) (0x80 | (cp & 0x3F));
            }
            i += adv - 1;
        }
        byte[] r = new byte[n];
        System.arraycopy(out, 0, r, 0, n);
        return r;
    }

    /** The record (pure; host-tested). */
    public static byte[] record(long seq, boolean route, boolean card, int x, int y, int w, int h,
                                String dist, String street) {
        return record(seq, route, card, x, y, w, h, dist, street, 0, 0);
    }

    public static byte[] record(long seq, boolean route, boolean card, int x, int y, int w, int h,
                                String dist, String street, int bar, int pm) {
        byte[] buf = new byte[RECORD_LEN];
        for (int i = 0; i < RECORD_LEN; i++) buf[i] = (byte) ' ';
        String head = "SQ5TC1 " + seq + "\nroute=" + (route ? 1 : 0) + " card=" + (card ? 1 : 0) + " x=" + x + " y=" + y
            + " w=" + w + " h=" + h + " bar=" + bar + " pm=" + pm + "\ndist=";
        int n = put(buf, 0, ascii(head));
        n = put(buf, n, utf8Limit(clean(dist), 32));
        n = put(buf, n, ascii("\nstreet="));
        n = put(buf, n, utf8Limit(clean(street), STREET_MAX_BYTES));
        n = put(buf, n, ascii("\nend=" + seq + "\n"));
        buf[RECORD_LEN - 1] = (byte) '\n';
        return buf;
    }

    private static byte[] ascii(String s) {
        byte[] b = new byte[s.length()];
        for (int i = 0; i < b.length; i++) b[i] = (byte) s.charAt(i);
        return b;
    }

    private static int put(byte[] buf, int at, byte[] src) {
        int n = Math.min(src.length, RECORD_LEN - 1 - at);
        if (n > 0) System.arraycopy(src, 0, buf, at, n);
        return at + Math.max(n, 0);
    }

    private static void writeLoop() {
        long lastWrite = 0L;
        while (true) {
            byte[] rec = null;
            String p = null, summary = null;
            try {
                synchronized (LOCK) {
                    while (true) {
                        long now = System.currentTimeMillis();
                        long due = lastWrite + HEARTBEAT_MS - now;
                        if (dirty || (enabled && (due <= 0 || now < lastWrite))) break;
                        LOCK.wait(enabled ? Math.max(due, 1L) : 0L);
                    }
                    dirty = false;
                    seq++;
                    boolean on = enabled;
                    rec = record(seq, on && route, on && card, bx, by, bw, bh, on ? dist : "", on ? street : "",
                        on ? barLevel : 0, on ? barMode : 0);
                    p = path;
                    summary = "route=" + (on && route) + " card=" + (on && card) + (on && card
                        ? " rect=(" + bx + "," + by + " " + bw + "x" + bh + ")" : "") + " dist='" + (on ? dist : "")
                        + "' street='" + (on ? street : "") + "'";
                    if (summary.equals(lastLogged)) summary = null;
                    else lastLogged = summary;
                }
                lastWrite = System.currentTimeMillis();
                write(p, rec);
                if (summary != null) AaLog.log("turncard: " + summary);
            } catch (Throwable t) {
                /* keep the writer alive */
            }
        }
    }

    private static void write(String p, byte[] rec) {
        RandomAccessFile f = null;
        try {
            f = new RandomAccessFile(p, "rw");
            f.seek(0);
            f.write(rec);
            writes++;
        } catch (Throwable t) {
            if (writeFailures++ == 0) AaLog.log("turncard: cannot write " + p + ": " + t);
        } finally {
            if (f != null) try { f.close(); } catch (Throwable t) { /* ignore */ }
        }
    }

    /** Host tests: records written so far. */
    public static int writesForTest() { return writes; }
}
