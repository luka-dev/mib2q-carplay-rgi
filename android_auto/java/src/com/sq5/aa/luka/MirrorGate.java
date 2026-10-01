/*
 * Readiness of the Android Auto cockpit mirror (native sq5_mirror, displayable 99).
 *
 * sq5_mirror rewrites /tmp/sq5_mirror_ready about once a second ("<counter> <pid> <w>x<h>"), but
 * only after the DM adopted its window (MANAGER_STRING seen) AND it posted a real Android Auto
 * frame; it deletes the file when it stops.  "Fresh" = the content changed within STALE_MS of
 * Java time and within STALE_POLLS polls, so neither a wall-clock jump (GPS time sync) nor a
 * burst of worker wake-ups can fake or break freshness on its own.
 *
 * SD flag sq5_mirror_off (either SD path) forces "not ready".  Polled by ScreenModule's single
 * cluster-switch worker every 250 ms while a phone session owns terminal 1.
 * Pure Java 1.4; the file access is isolated so the rule is host-testable (see observe()).
 */
package com.sq5.aa.luka;

import java.io.File;
import java.io.FileInputStream;

public final class MirrorGate {
    public static final String MARKER = "/tmp/sq5_mirror_ready";
    private static final String[] OFF_FLAGS = {"/fs/sda0/sq5_mirror_off", "/net/mmx/fs/sda0/sq5_mirror_off"};
    public static final long STALE_MS = 3000L;
    public static final int STALE_POLLS = 8;
    private static final long FLAG_CHECK_MS = 2000L;

    private String lastContent;
    private long lastChangeMs;
    private int pollsSinceChange;
    private boolean ready;
    private boolean off;
    private long lastFlagCheckMs = Long.MIN_VALUE;

    private static final MirrorGate INSTANCE = new MirrorGate();
    /* capability tokens after "<counter> <pid> <w>x<h>" (sq5_mirror 2026-09-28+):
     *   tc1 = the mirror draws the turn-card panel + text strip behind plane 98 (TurnCardFeed card=1),
     *         so ClusterLayerController leaves the 987 backing off in card mode. */
    private static volatile boolean drawsCardPanel;

    /** Host tests drive observe() on their own instance. */
    public MirrorGate() {
    }

    /** Poll the real marker + SD flags (cluster-switch worker only). */
    public static boolean poll() {
        publishView();
        return INSTANCE.pollReal(System.currentTimeMillis());
    }

    private static int lastView = -1;

    /** /tmp/sq5_cluster_view = "small" / "full" (cluster player geometry; written only on change). */
    private static void publishView() {
        int v = com.luka.carplay.core.ScreenModule.isSmallScreenViewArea() ? 1 : 0;
        if (v == lastView) return;
        java.io.FileOutputStream o = null;
        try {
            o = new java.io.FileOutputStream("/tmp/sq5_cluster_view");
            o.write((v == 1 ? "small\n" : "full\n").getBytes());
            lastView = v;
            AaLog.log("cluster view " + (v == 1 ? "small" : "full") + " -> /tmp/sq5_cluster_view");
        } catch (Throwable t) {
            /* retried on the next poll */
        } finally {
            if (o != null) try { o.close(); } catch (Throwable t) { /* ignore */ }
        }
    }

    /** Forget the previous session's marker state (ScreenModule start/stop). */
    public static void reset() {
        synchronized (INSTANCE) {
            INSTANCE.lastContent = null;
            INSTANCE.pollsSinceChange = 0;
            INSTANCE.ready = false;
        }
        drawsCardPanel = false;
    }

    private synchronized boolean pollReal(long now) {
        if (lastFlagCheckMs == Long.MIN_VALUE || now - lastFlagCheckMs >= FLAG_CHECK_MS || now < lastFlagCheckMs) {
            lastFlagCheckMs = now;
            boolean o = false;
            for (int i = 0; i < OFF_FLAGS.length; i++) {
                try { if (new File(OFF_FLAGS[i]).exists()) o = true; } catch (Throwable t) { /* ignore */ }
            }
            if (o != off) AaLog.log("mirror: sq5_mirror_off " + (o ? "present - mirror contexts disabled" : "removed"));
            off = o;
        }
        // A dedicated cluster feed can be ready while main-screen mirroring is off.
        String cluster = readClusterMarker();
        return observe(cluster != null ? cluster : (off ? null : readMarker()), now);
    }

    /** The freshness rule (host-testable). content == null means "no marker / disabled". */
    public synchronized boolean observe(String content, long now) {
        boolean was = ready;
        if (content == null || content.length() == 0) {
            lastContent = null;
            pollsSinceChange = 0;
            ready = false;
        } else if (!content.equals(lastContent)) {
            lastContent = content;
            lastChangeMs = now;
            pollsSinceChange = 0;
            ready = true;
        } else {
            if (now < lastChangeMs) lastChangeMs = now;          /* clock stepped back */
            pollsSinceChange++;
            if (now - lastChangeMs >= STALE_MS && pollsSinceChange >= STALE_POLLS) ready = false;
        }
        if (was != ready) AaLog.log("mirror: marker " + (ready ? "fresh (" + content + ")" : "stale/absent -> fallback"));
        if (this == INSTANCE) {
            boolean panel = ready && hasToken(lastContent, "tc1");
            if (panel != drawsCardPanel) AaLog.log("mirror: turn-card panel " + (panel ? "drawn by sq5_mirror (tc1)"
                : "not drawn by sq5_mirror -> 987 backing used in card mode"));
            drawsCardPanel = panel;
        }
        return ready;
    }

    /** True while the fresh marker advertises that sq5_mirror draws the turn-card panel. */
    public static boolean drawsCardPanel() { return drawsCardPanel; }

    /** Whitespace-separated token present in the marker line (pure; host-tested). */
    public static boolean hasToken(String content, String token) {
        if (content == null) return false;
        int from = 0;
        while (true) {
            int i = content.indexOf(token, from);
            if (i < 0) return false;
            int e = i + token.length();
            boolean startOk = i == 0 || content.charAt(i - 1) == ' ';
            boolean endOk = e == content.length() || content.charAt(e) == ' ' || content.charAt(e) == (char) 10;
            if (startOk && endOk) return true;
            from = i + 1;
        }
    }

    /* ------------------------------------------------------------------ Maps on screen (touch path) */

    /*
     * Since the maps gate, sq5_mirror writes the marker ONLY while it detects Google Maps on the AA
     * screen and deletes it otherwise, so "marker fresh" == "Maps on screen".  Read by the MMI
     * touchpad bridge (com.sq5.aa.input.AaTouchpadInput, via reflection, once per stroke).
     * Separate state from poll(): the cluster worker's poll counters are not disturbed.
     *   - at most one marker read per MAPS_CACHE_MS; callers in between get the cached answer;
     *   - never waits: a caller that finds another thread mid-refresh gets the cached answer;
     *   - fresh = file mtime within STALE_MS of now (either side: clock steps only fall back to
     *     "not Maps" briefly), OR the content changed since a read no older than STALE_MS;
     *   - absent/empty marker, unreadable file, sq5_mirror_off or sq5_mapgate_off (the mirror then
     *     claims ready on any screen) -> NOT Maps.  When unsure the answer is "not Maps".
     */
    public static final long MAPS_CACHE_MS = 250L;
    private static final String[] MAPGATE_OFF_FLAGS = {"/fs/sda0/sq5_mapgate_off", "/net/mmx/fs/sda0/sq5_mapgate_off"};
    private static final Object MAPS_LOCK = new Object();
    private static volatile boolean mapsCached;
    private static volatile long mapsCheckedMs;
    private static volatile boolean mapsChecked;
    private static boolean mapsRefreshing;                 /* guarded by MAPS_LOCK */
    /* rule state: only touched by the single refreshing thread (or host tests) */
    private static String mapsLastContent;
    private static long mapsLastReadMs;
    private static long mapsLastChangeMs;
    private static boolean mapsChangeKnown;
    private static long mapsFlagsMs;
    private static boolean mapsFlagsRead;
    private static boolean mapsFlagOff;

    /** True only while a fresh marker says Google Maps is on the AA screen. Cheap, never throws. */
    public static boolean isMapsOnScreen() {
        try {
            return mapsOnScreen(System.currentTimeMillis());
        } catch (Throwable t) {
            return false;
        }
    }

    private static boolean mapsOnScreen(long now) {
        if (mapsChecked) {
            long at = mapsCheckedMs;
            if (now >= at && now - at < MAPS_CACHE_MS) return mapsCached;
        }
        synchronized (MAPS_LOCK) {
            if (mapsRefreshing) return mapsCached;
            mapsRefreshing = true;
        }
        boolean m = false;
        try {
            if (!mapsFlagsRead || now - mapsFlagsMs >= FLAG_CHECK_MS || now < mapsFlagsMs) {
                mapsFlagsRead = true;
                mapsFlagsMs = now;
                boolean o = false;
                for (int i = 0; i < OFF_FLAGS.length && !o; i++) {
                    try { if (new File(OFF_FLAGS[i]).exists() || new File(MAPGATE_OFF_FLAGS[i]).exists()) o = true; } catch (Throwable t) { /* ignore */ }
                }
                mapsFlagOff = o;
            }
            if (!mapsFlagOff) {
                long mtime = 0L;
                String c = null;
                try {
                    File f = new File(MARKER);
                    mtime = f.lastModified();
                    c = readMarker();
                } catch (Throwable t) {
                    c = null;
                }
                m = mapsRule(c, mtime, now);
            } else {
                m = mapsRule(null, 0L, now);
            }
        } catch (Throwable t) {
            m = false;
        } finally {
            mapsCached = m;
            mapsCheckedMs = now;
            mapsChecked = true;
            synchronized (MAPS_LOCK) {
                mapsRefreshing = false;
            }
        }
        return m;
    }

    /** The Maps freshness rule. content == null: no marker. mtime <= 0: unknown. */
    private static boolean mapsRule(String content, long mtime, long now) {
        if (content == null || content.length() == 0) {
            mapsLastContent = null;
            mapsChangeKnown = false;
            mapsLastReadMs = now;
            return false;
        }
        if (mapsLastContent != null && !content.equals(mapsLastContent)
                && now >= mapsLastReadMs && now - mapsLastReadMs <= STALE_MS) {
            mapsLastChangeMs = now;                           /* changed within the last STALE_MS */
            mapsChangeKnown = true;
        }
        mapsLastContent = content;
        mapsLastReadMs = now;
        boolean byChange = mapsChangeKnown && now >= mapsLastChangeMs && now - mapsLastChangeMs <= STALE_MS;
        boolean byMtime = mtime > 0L && now - mtime <= STALE_MS && mtime - now <= STALE_MS;
        return byChange || byMtime;
    }

    /** Host tests: drive the Maps rule with synthetic marker content / mtime / clock. */
    public static boolean mapsRuleForTest(String content, long mtime, long now) {
        synchronized (MAPS_LOCK) {
            return mapsRule(content, mtime, now);
        }
    }

    /** Host tests: forget Maps rule and cache state. */
    public static void resetMapsForTest() {
        synchronized (MAPS_LOCK) {
            mapsLastContent = null;
            mapsChangeKnown = false;
            mapsLastReadMs = 0L;
            mapsChecked = false;
            mapsCached = false;
            mapsFlagsRead = false;
        }
    }

    private static String readClusterMarker() {
        String[] roots = {"/fs/sda0/", "/net/mmx/fs/sda0/"};
        for (int i = 0; i < roots.length; i++) {
            if (new File(roots[i] + "mib2q_aa_off").exists()
                    || new File(roots[i] + "sq5_cluster_live_off").exists()
                    || new File(roots[i] + "sq5_hook_off").exists()
                    || new File(roots[i] + "sq5_luka_off").exists()) return null;
        }
        /* Route-only option (owner: no route -> keep the Audi map): SD flag sq5_cluster_route_only. Off by
         * default: the owner also likes Google's overview when there is no route. */
        for (int i = 0; i < roots.length; i++) {
            if (new File(roots[i] + "sq5_cluster_route_only").exists()
                    && !com.luka.carplay.core.ScreenModule.isNavActive()) return null;
        }
        String c = readMarkerFile("/tmp/sq5_cluster_ready");
        return hasToken(c, "cluster1") ? c : null;
    }

    private static String readMarker() { return readMarkerFile(MARKER); }

    private static String readMarkerFile(String path) {
        FileInputStream in = null;
        try {
            File f = new File(path);
            if (!f.exists()) return null;
            in = new FileInputStream(f);
            byte[] b = new byte[96];
            int n = in.read(b);
            if (n <= 0) return null;
            return new String(b, 0, n).trim();
        } catch (Throwable t) {
            return null;
        } finally {
            if (in != null) try { in.close(); } catch (Throwable t) { /* ignore */ }
        }
    }
}
