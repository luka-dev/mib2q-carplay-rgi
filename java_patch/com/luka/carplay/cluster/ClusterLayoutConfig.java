/*
 * ClusterLayoutConfig - the Java half of /mnt/app/carplay_layout.cfg.
 *
 * The same small text file is read by the altScreen hook (hook/altscreen/cluster_layout.h:
 * safe areas and the phone's card options, applied at the next phone connection) and here.
 * Java owns two keys:
 *
 *   view_source auto | choice | vc
 *       Where the CarPlay fullscreen/compact state comes from.
 *       choice: the HU's NAV_VIEW_SIZE_CHOICE model only (luka's original behaviour).
 *       vc:     the cockpit's own stage report, NavSD FctID 54 Map_Presentation.largeMapView,
 *               with the choice model only until the cockpit has reported once.
 *       auto:   both; whichever reported last wins (default).  On a Classic B9 (US P5087)
 *               and on a Q7 4M the View button often raises no NAV_VIEW_SIZE_CHOICE while
 *               CarPlay owns the cluster, but the cockpit still sends FctID 54.
 *   compact auto | sport | classic
 *       Which compact safe area the phone gets: auto follows the stock VC skin
 *       (LastmodeStorage.getSkin(): 1 = Sport, 0 = Classic); sport/classic force one.
 *
 * Lines for the hook's keys (full, sport, classic, cards, eta, speed_limit, car_profile) are
 * skipped here;
 * any other line is counted as rejected and logged once per file change.  Missing file =
 * auto/auto.  The file is checked at most every 2 s, by stat only unless it changed.
 *
 * Java 1.4 source level (the head unit's J9): no generics, no autoboxing.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
package com.luka.carplay.cluster;

import com.luka.carplay.framework.Log;

import java.io.File;
import java.io.FileInputStream;
import java.io.InputStream;
import java.util.StringTokenizer;

public final class ClusterLayoutConfig {
    private static final String TAG = "ClusterLayout";
    public static final String PATH = "/mnt/app/carplay_layout.cfg";

    public static final int VIEW_SOURCE_AUTO = 0;
    public static final int VIEW_SOURCE_CHOICE = 1;
    public static final int VIEW_SOURCE_VC = 2;
    public static final int COMPACT_AUTO = 0;
    public static final int COMPACT_SPORT = 1;
    public static final int COMPACT_CLASSIC = 2;

    private static final long CHECK_INTERVAL_MS = 2000L;
    private static final int MAX_FILE = 4096;

    private static String path = PATH;
    private static boolean checked;
    private static long lastCheckMs;
    private static long lastModified = -1L;
    private static long lastLength = -1L;
    private static int viewSource = VIEW_SOURCE_AUTO;
    private static int compact = COMPACT_AUTO;

    private ClusterLayoutConfig() {}

    /** Parsed Java keys of one file: {viewSource, compact, rejectedLines, firstBadLine}. */
    static int[] parse(String text) {
        int[] r = new int[] { VIEW_SOURCE_AUTO, COMPACT_AUTO, 0, 0 };
        int line = 0;
        int start = 0;
        int n = text.length();
        while (start <= n) {
            int eol = text.indexOf('\n', start);
            if (eol < 0) eol = n;
            line++;
            String l = text.substring(start, eol);
            start = eol + 1;
            int hash = l.indexOf('#');
            if (hash >= 0) l = l.substring(0, hash);
            String[] tok = tokens(l);
            if (tok.length == 0) continue;
            String key = tok[0].toLowerCase();
            String val = tok.length == 2 ? tok[1].toLowerCase() : null;
            int v = -1;
            if (key.equals("view_source")) {
                if ("auto".equals(val)) v = VIEW_SOURCE_AUTO;
                else if ("choice".equals(val)) v = VIEW_SOURCE_CHOICE;
                else if ("vc".equals(val)) v = VIEW_SOURCE_VC;
                if (v >= 0) r[0] = v;
            } else if (key.equals("compact")) {
                if ("auto".equals(val)) v = COMPACT_AUTO;
                else if ("sport".equals(val)) v = COMPACT_SPORT;
                else if ("classic".equals(val)) v = COMPACT_CLASSIC;
                if (v >= 0) r[1] = v;
            } else if (key.equals("full") || key.equals("sport") || key.equals("classic")
                    || key.equals("cards") || key.equals("eta") || key.equals("speed_limit")
                    || key.equals("car_profile")) {
                continue;   /* the hook's keys, validated there */
            }
            if (v < 0) {
                r[2]++;
                if (r[3] == 0) r[3] = line;
            }
        }
        return r;
    }

    private static String[] tokens(String s) {
        StringTokenizer st = new StringTokenizer(s, " \t\r\f");
        String[] r = new String[st.countTokens()];
        for (int i = 0; i < r.length; i++) r[i] = st.nextToken();
        return r;
    }

    /** Re-read the file if it changed.  Returns true when a Java value changed. */
    public static synchronized boolean refresh(boolean force) {
        long now = System.currentTimeMillis();
        if (!force && checked && now - lastCheckMs >= 0 && now - lastCheckMs < CHECK_INTERVAL_MS) {
            return false;
        }
        checked = true;
        lastCheckMs = now;
        File f = new File(path);
        long mod = -1L, len = -1L;
        try {
            if (f.isFile()) { mod = f.lastModified(); len = f.length(); }
        } catch (Throwable t) {
            mod = -1L; len = -1L;
        }
        if (!force && mod == lastModified && len == lastLength) return false;
        lastModified = mod;
        lastLength = len;
        int[] r;
        if (mod < 0) {
            r = new int[] { VIEW_SOURCE_AUTO, COMPACT_AUTO, 0, 0 };
        } else {
            String text = read(f);
            r = text == null ? new int[] { VIEW_SOURCE_AUTO, COMPACT_AUTO, 1, 0 } : parse(text);
        }
        boolean changed = r[0] != viewSource || r[1] != compact;
        viewSource = r[0];
        compact = r[1];
        Log.i(TAG, "layout cfg " + (mod < 0 ? "absent" : path)
            + " view_source=" + viewSourceName(viewSource)
            + " compact=" + compactName(compact)
            + " rejected=" + r[2] + (r[3] > 0 ? " first_bad_line=" + r[3] : "")
            + (changed ? " (changed)" : ""));
        return changed;
    }

    private static String read(File f) {
        InputStream in = null;
        try {
            in = new FileInputStream(f);
            byte[] buf = new byte[MAX_FILE];
            int used = 0;
            while (used < buf.length) {
                int r = in.read(buf, used, buf.length - used);
                if (r <= 0) break;
                used += r;
            }
            return new String(buf, 0, used, "UTF-8");
        } catch (Throwable t) {
            Log.w(TAG, "layout cfg unreadable: " + t);
            return null;
        } finally {
            if (in != null) {
                try { in.close(); } catch (Throwable t) { /* nothing to do */ }
            }
        }
    }

    public static synchronized int viewSource() { return viewSource; }
    public static synchronized int compact() { return compact; }

    public static String viewSourceName(int v) {
        return v == VIEW_SOURCE_CHOICE ? "choice" : (v == VIEW_SOURCE_VC ? "vc" : "auto");
    }

    public static String compactName(int v) {
        return v == COMPACT_SPORT ? "sport" : (v == COMPACT_CLASSIC ? "classic" : "auto");
    }

    /** Host tests only: point at another file and forget the stat cache (values are kept, so
     *  the next refresh reports a change exactly as a new file on the car would). */
    static synchronized void resetForTest(String p) {
        path = p == null ? PATH : p;
        checked = false;
        lastModified = -1L;
        lastLength = -1L;
    }
}
