/*
 * Minimal logger — writes to BOTH stdout (captured by the lsd runtime into the HMI slog) and
 * an own file /tmp/carplay_java.log (append; matches the C modules so everything is SSH-tailable
 * from /tmp).  Java 1.4 / Foundation 1.1 (no varargs, no String.format in hot paths).
 */
package com.luka.carplay.framework;

import java.io.FileOutputStream;
import java.io.PrintStream;

public final class Log {
    public static final int E = 0, W = 1, I = 2, D = 3;
    private static int level = D;

    private static final String FILE = "/tmp/carplay_java.log";
    private static PrintStream file;        /* lazily opened; stays null if it can't be created */
    private static boolean fileTried;

    private Log() {}

    public static void setLevel(int l) { level = l; }

    public static void e(String tag, String msg) { out(E, "E", tag, msg); }
    public static void w(String tag, String msg) { out(W, "W", tag, msg); }
    public static void i(String tag, String msg) { out(I, "I", tag, msg); }
    public static void d(String tag, String msg) { out(D, "D", tag, msg); }

    /* throwable overloads (ports pass an exception) */
    public static void e(String tag, String msg, Throwable t) { out(E, "E", tag, msg + " :: " + t); }
    public static void w(String tag, String msg, Throwable t) { out(W, "W", tag, msg + " :: " + t); }

    private static void out(int l, String p, String tag, String msg) {
        if (l > level) return;
        String line = "[CP/" + p + "][" + tag + "] " + msg;
        System.out.println(line);                       /* HMI slog */
        PrintStream f = logFile();
        if (f != null) f.println(line);                 /* /tmp/carplay_java.log (autoflush) */
    }

    /* Open the file once, best-effort; append + autoflush so a crash still leaves the tail. */
    private static synchronized PrintStream logFile() {
        if (!fileTried) {
            fileTried = true;
            try { file = new PrintStream(new FileOutputStream(FILE, true), true); }
            catch (Throwable t) { file = null; }        /* fall back to stdout only */
        }
        return file;
    }
}
