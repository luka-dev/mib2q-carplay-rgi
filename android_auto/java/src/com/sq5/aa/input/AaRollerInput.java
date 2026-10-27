/*
 * Steering-wheel roller -> Android Auto rotary steps while the cockpit shows the AA mirror
 * (SQ5 MHI2Q MU0918).
 *
 * Event path (stock MU0918, re/src): the left MFW roller's ROTATION reaches the head unit from the
 * virtual cockpit as Navigation-BAP MapScale steps -> CombiBAPListener.setMapScale(int steps)
 * (re/src/de/audi/tghu/navi/app/cluster/CombiBAPListener.java l.1664-1675), which zooms the native
 * Audi kombi map (MapContext.increment(zoom property, steps)) and acknowledges with updateMapScale().
 * luka never intercepts rotation (docs/input/steering-wheel.md: only the roller PRESS, DSI key 40).
 * Our ScreenCombiBAPListener (the stock factory ClusterService.initBAPListener already returns it)
 * overrides setMapScale and asks onRoller() first:
 *   consumed  (cockpit shows the mirror ctx 81/82, AA session active, DSI known, no sq5_roller_off):
 *             steps -> DSIAndroidAuto2.postRotaryEvent(delta), the same call the stock
 *             AndroidAuto2KeyEventsController.updateRotary() makes for the MMI knob; the listener
 *             then acknowledges the BAP request with the unchanged scale (updateMapScale) instead of
 *             zooming the hidden Audi map;
 *   otherwise stock: super.setMapScale(steps) zooms the Audi map as before.
 *
 * Sign rule: one MapScale step = one knob increment, posted exactly like the MMI knob
 * (TMVirtualButtonListener / AaTouchpadInput): increment -> -1, +1 when the terminal-mode
 * configuration says isKnobDirectionInverted(). The BAP direction of "roller up" was not observed
 * on the car yet: SD flag sq5_roller_invert flips it without a rebuild. |delta| is capped at
 * MAX_STEP per BAP request.
 *
 * Android Auto only ZOOMS with rotary input while the map itself has focus (Maps' pan/zoom mode,
 * entered by pressing the controller on the map: zoom slider + "Press <enter> to return to map
 * view"). Otherwise a rotary step moves the focus highlight between on-screen controls. The stock
 * DSIAndroidAuto2 has no call that puts the focus on the map (only postButtonEvent / postTouchEvent
 * / postRotaryEvent / focus NOTIFICATIONS), and synthesising DPAD_CENTER or a touch on the map is
 * NOT safe (it would activate whatever is focused - e.g. Maps' "end navigation" X - or tap a route
 * bubble). So only rotary steps are ever posted: they never activate anything.
 *
 * SD-card switches (root of SD1, re-read every 5 s): sq5_roller_off (stock zoom only),
 * sq5_roller_invert (flip direction). Every entry point catches Throwable.
 */
package com.sq5.aa.input;

import com.sq5.aa.luka.AaLog;

import java.io.File;
import org.dsi.ifc.androidauto2.DSIAndroidAuto2;

public final class AaRollerInput {
    public static final String VERSION = "aa-roller 1.0";
    public static final int MAX_STEP = 5;

    private static final String[] SD_ROOTS = {"/fs/sda0/", "/net/mmx/fs/sda0/"};
    private static final long FLAG_CHECK_MS = 5000L;

    private static final Object LOCK = new Object();
    private static DSIAndroidAuto2 dsi;                /* guarded by LOCK */
    private static boolean session;                     /* guarded by LOCK */
    private static boolean knobInverted;                /* guarded by LOCK */
    private static boolean off, invert;                 /* guarded by LOCK */
    private static long flagsReadMs = -FLAG_CHECK_MS - 1;
    private static String lastReason = "";              /* last pass-through reason logged */
    private static int nPosted;
    private static String[] testFlagRoots;              /* host tests */

    private AaRollerInput() {
    }

    /** From the rebuilt AndroidAuto2KeyEventsController constructor (the stock DSI proxy). */
    public static void setDsi(DSIAndroidAuto2 d) {
        try {
            synchronized (LOCK) {
                dsi = d;
            }
            AaLog.log("roller: DSI " + (d == null ? "cleared" : "bound") + " (" + VERSION + ")");
        } catch (Throwable t) {
            /* ignore */
        }
    }

    /** From AaLukaBridge (terminal-mode configuration: MMI knob direction). */
    public static void configure(boolean inverted) {
        try {
            synchronized (LOCK) {
                knobInverted = inverted;
            }
            AaLog.log("roller: configured knobInverted=" + inverted);
        } catch (Throwable t) {
            /* ignore */
        }
    }

    /** From AaLukaBridge on Android Auto session start/end. */
    public static void setSessionActive(boolean active) {
        try {
            synchronized (LOCK) {
                session = active;
                lastReason = "";
            }
            AaLog.log("roller: session " + (active ? "active" : "inactive"));
        } catch (Throwable t) {
            /* ignore */
        }
    }

    /** Rotary delta for `steps` BAP MapScale steps (pure; host-tested). 0 = nothing to post. */
    public static int rotaryDelta(int steps, boolean knobInv, boolean rollerInv) {
        if (steps > MAX_STEP) steps = MAX_STEP;
        if (steps < -MAX_STEP) steps = -MAX_STEP;
        int d = knobInv ? steps : -steps;
        return rollerInv ? -d : d;
    }

    /**
     * One roller rotation from the VC (BAP MapScale). mirrorShown = ScreenModule.isMirrorActive()
     * (terminal 1 physically carries ctx 81/82 during a phone session). Returns true when the steps
     * went to Android Auto (caller must NOT zoom the Audi map), false = stock behaviour.
     */
    public static boolean onRoller(int steps, boolean mirrorShown) {
        try {
            return handle(steps, mirrorShown, System.currentTimeMillis());
        } catch (Throwable t) {
            try { AaLog.log("roller: handler failed, stock zoom: " + t); } catch (Throwable t2) { /* ignore */ }
            return false;
        }
    }

    private static boolean handle(int steps, boolean mirrorShown, long now) {
        DSIAndroidAuto2 d;
        String reason = null;
        int delta;
        boolean ki, ri;
        synchronized (LOCK) {
            readFlags(now);
            if (off) reason = "sq5_roller_off";
            else if (!session) reason = "no Android Auto session";
            else if (!mirrorShown) reason = "cockpit not on the AA mirror";
            else if (dsi == null) reason = "no DSI";
            else if (steps == 0) reason = "zero steps";
            d = dsi;
            ki = knobInverted;
            ri = invert;
            if (reason != null) {
                if (!reason.equals(lastReason)) {
                    lastReason = reason;
                    AaLog.log("roller: steps=" + steps + " -> stock map zoom (" + reason + ")");
                }
                return false;
            }
            lastReason = "";
            delta = rotaryDelta(steps, ki, ri);
            nPosted++;
        }
        if (clusterMapShown(now) && !flag("sq5_cluster_rotary_off")) {
            if (postClusterRotary(delta)) {
                AaLog.log("roller: steps=" + steps + " -> cockpit map rotary " + delta + " (total " + clusterTotal + ")");
                return true;
            }
        }
        d.postRotaryEvent(delta);
        AaLog.log("roller: steps=" + steps + " -> AA rotary " + delta + " (knobInverted=" + ki
            + " sq5_roller_invert=" + ri + ", #" + nPosted + ")");
        return true;
    }

    /* Owner 2026-09-29: "the scroll has to correspond to the cockpit screen, then it acts as zoom". While the
     * cockpit shows the independent Android Auto cluster map (player marker token cluster1), the steps go to
     * that display's own input channel: a cumulative counter in /tmp/sq5_cluster_rotary (fixed 32-byte
     * record, written in place - /tmp is shmem, no rename), which the hook in gal turns into InputReport
     * rotary events (keycode 65536) on the cluster input channel. SD flag sq5_cluster_rotary_off = old path. */
    private static final String CLUSTER_READY = "/tmp/sq5_cluster_ready";
    private static final String CLUSTER_ROTARY = "/tmp/sq5_cluster_rotary";
    private static int clusterTotal;
    private static long clusterCheckedMs = -1L;
    private static boolean clusterShown;

    static boolean clusterMapShown(long now) {
        if (clusterCheckedMs >= 0 && now - clusterCheckedMs < 500L && now >= clusterCheckedMs) return clusterShown;
        clusterCheckedMs = now;
        java.io.FileInputStream in = null;
        boolean shown = false;
        try {
            in = new java.io.FileInputStream(CLUSTER_READY);
            byte[] b = new byte[128];
            int n = in.read(b);
            shown = n > 0 && new String(b, 0, n).indexOf("cluster1") >= 0;
        } catch (Throwable t) {
            shown = false;
        } finally {
            if (in != null) try { in.close(); } catch (Throwable t) { /* ignore */ }
        }
        clusterShown = shown;
        return shown;
    }

    private static boolean postClusterRotary(int delta) {
        java.io.RandomAccessFile f = null;
        try {
            int total = clusterTotal + delta;
            StringBuffer s = new StringBuffer("SQ5R ").append(total);
            while (s.length() < 31) s.append(' ');
            s.append('\n');
            f = new java.io.RandomAccessFile(CLUSTER_ROTARY, "rw");
            f.seek(0);
            f.write(s.toString().getBytes());
            clusterTotal = total;
            return true;
        } catch (Throwable t) {
            AaLog.log("roller: cockpit rotary write failed, centre screen instead: " + t);
            return false;
        } finally {
            if (f != null) try { f.close(); } catch (Throwable t) { /* ignore */ }
        }
    }

    private static void readFlags(long now) {
        if (now - flagsReadMs < FLAG_CHECK_MS && now >= flagsReadMs) return;
        flagsReadMs = now;
        boolean o = flag("sq5_roller_off");
        boolean i = flag("sq5_roller_invert");
        if (o != off || i != invert) AaLog.log("roller: flags off=" + o + " invert=" + i);
        off = o;
        invert = i;
    }

    private static boolean flag(String name) {
        String[] roots = testFlagRoots != null ? testFlagRoots : SD_ROOTS;
        for (int i = 0; i < roots.length; i++) {
            try {
                if (new File(roots[i] + name).exists()) return true;
            } catch (Throwable t) {
                /* ignore */
            }
        }
        return false;
    }

    /* ------------------------------------------------------------------ host-test support */

    /** Host tests: flag directory (trailing separator) replacing the SD roots; resets all state. */
    public static void resetForTest(String flagRoot) {
        synchronized (LOCK) {
            testFlagRoots = flagRoot == null ? null : new String[]{flagRoot};
            dsi = null;
            session = false;
            knobInverted = false;
            off = false;
            invert = false;
            flagsReadMs = -FLAG_CHECK_MS - 1;
            lastReason = "";
            nPosted = 0;
        }
    }

    /** Host tests: force a flag re-read on the next roller step. */
    public static void rereadFlagsForTest() {
        synchronized (LOCK) {
            flagsReadMs = -FLAG_CHECK_MS - 1;
        }
    }

    public static int postedForTest() {
        synchronized (LOCK) {
            return nPosted;
        }
    }
}
