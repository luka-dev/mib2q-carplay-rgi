/*
 * Minimal logger — stdout with a level prefix + tag.  Java 1.2 (no varargs,
 * no String.format in hot paths).  The lsd runtime captures stdout to the HMI log.
 */
package com.luka.carplay.framework;

public final class Log {
    public static final int E = 0, W = 1, I = 2, D = 3;
    private static int level = D;

    private Log() {}

    public static void setLevel(int l) { level = l; }

    public static void e(String tag, String msg) { out(E, "E", tag, msg); }
    public static void w(String tag, String msg) { out(W, "W", tag, msg); }
    public static void i(String tag, String msg) { out(I, "I", tag, msg); }
    public static void d(String tag, String msg) { out(D, "D", tag, msg); }

    private static void out(int l, String p, String tag, String msg) {
        if (l > level) return;
        System.out.println("[CP/" + p + "][" + tag + "] " + msg);
    }
}
