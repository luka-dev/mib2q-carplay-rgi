/*
 * SQ5 AA port: who writes the cluster planes.  DisplayManagerMIB2High reports every DM write that
 * touches the native map planes (33 main, 58 alt/Google Earth) or the Android Auto mirror (99) on
 * terminal 1, plus terminal-1 update-rate writes and every DM context confirmation for the cluster
 * display, with the calling thread and the first non-DisplayManager stack frame.  Only writes that
 * actually reach DSI (the Java write-through cache would drop the rest) are logged, so the log stays
 * quiet in steady state.  Never throws.
 */
package com.sq5.aa.luka;

public final class DmTrace {
    public static final int CLUSTER_TERMINAL = 1;
    public static final int CLUSTER_DM_DISPLAY = 4;   /* DisplayManager.getInternalDisplayID(1) */

    private static final Object LOCK = new Object();
    private static final String[] lastCrop = new String[128];
    private static int lastRate = -1;

    private DmTrace() {
    }

    /** Planes whose writes are traced: 33 native map, 58 alt map, 99 Android Auto mirror. */
    public static boolean watched(int displayable) {
        return displayable == 33 || displayable == 58 || displayable == 99;
    }

    public static void opacity(int displayable, int terminal, int value, int cachedBefore) {
        if (terminal != CLUSTER_TERMINAL || !watched(displayable) || value == cachedBefore) return;
        AaLog.log("dm: setOpacity(" + displayable + ") " + cachedBefore + "->" + value + by());
    }

    public static void position(int displayable, int terminal, int x, int y, int[] before) {
        if (terminal != CLUSTER_TERMINAL || !watched(displayable)) return;
        if (before != null && before.length >= 2 && before[0] == x && before[1] == y) return;
        AaLog.log("dm: setPosition(" + displayable + ") "
            + (before != null && before.length >= 2 ? "(" + before[0] + "," + before[1] + ")" : "?")
            + "->(" + x + "," + y + ")" + by());
    }

    public static void cropping(int displayable, int terminal, int sx, int sy, int sw, int sh,
                                int dx, int dy, int dw, int dh) {
        if (terminal != CLUSTER_TERMINAL || !watched(displayable)) return;
        String sig = "src=(" + sx + "," + sy + " " + sw + "x" + sh + ") dst=(" + dx + "," + dy + " " + dw + "x" + dh + ")";
        synchronized (LOCK) {
            if (sig.equals(lastCrop[displayable])) return;
            lastCrop[displayable] = sig;
        }
        AaLog.log("dm: setCropping(" + displayable + ") " + sig + by());
    }

    /** fadeToOpacity passes the terminal unmapped (DM display number). */
    public static void fade(int displayable, int rawDisplay, int value, int timeMs) {
        if (!watched(displayable)) return;
        AaLog.log("dm: fadeToOpacity(" + displayable + ", display " + rawDisplay + ") -> " + value
            + " in " + timeMs + " ms" + by());
    }

    public static void updateRate(int terminal, int rate) {
        if (terminal != CLUSTER_TERMINAL) return;
        synchronized (LOCK) {
            if (rate == lastRate) return;
            lastRate = rate;
        }
        AaLog.log("dm: setUpdateRate(terminal 1) -> " + rate + by());
    }

    /**
     * DM confirmation for the cluster display.  javaBefore/javaAfter = getCurrentContextID(1) around
     * the stock handler: when the DM reports a context Java does not adopt (a confirmation carrying a
     * foreign session id is dropped by DisplayManager.setActiveContext), ScreenModule's reconcile,
     * which reads the Java view, cannot see the change; this line is the only trace of it.
     */
    public static void confirm(int ctx, int display, int session, int javaBefore, int javaAfter) {
        if (display != CLUSTER_DM_DISPLAY) return;
        String note;
        if (javaAfter != ctx) {
            note = " IGNORED by Java (foreign/stale session): DM shows ctx " + ctx + ", Java keeps " + javaAfter;
        } else if (javaBefore != ctx) {
            note = " adopted";
        } else {
            note = " (unchanged)";
        }
        AaLog.log("dm: DM confirms display 4 ctx=" + ctx + " session=" + session + " java " + javaBefore + "->"
            + javaAfter + note + " [thread '" + threadName() + "']");
    }

    private static String threadName() {
        try {
            return Thread.currentThread().getName();
        } catch (Throwable t) {
            return "?";
        }
    }

    /** " by <caller> [thread '<name>']" - first frame outside the DisplayManager classes. */
    static String by() {
        String caller = "?";
        try {
            StackTraceElement[] st = new Throwable().getStackTrace();
            for (int i = 0; st != null && i < st.length; i++) {
                String c = st[i].getClassName();
                if (c.startsWith("com.sq5.aa.luka.DmTrace") || c.startsWith("de.audi.tghu.fwhmi.DisplayManager")) continue;
                String cls = c.substring(c.lastIndexOf('.') + 1);
                caller = cls + "." + st[i].getMethodName() + ":" + st[i].getLineNumber();
                break;
            }
        } catch (Throwable t) {
            /* J9 without stack traces: thread name only */
        }
        return " by " + caller + " [thread '" + threadName() + "']";
    }
}
