/*
 * Minimal logger — writes to BOTH stdout (captured by the lsd runtime into the HMI slog) and
 * an own file /tmp/carplay_java.log (append; matches the C modules so everything is SSH-tailable
 * from /tmp).  Java 1.4 / Foundation 1.1 (no varargs, no String.format in hot paths).
 */
package com.luka.carplay.framework;

import java.io.FileOutputStream;
import java.io.File;
import java.io.PrintStream;

public final class Log {
    public static final int E = 0, W = 1, I = 2, D = 3;
    /* Production default. DEBUG can still be enabled explicitly for a diagnostic build. */
    private static int level = I;

    private static final String FILE = "/tmp/carplay_java.log";
    private static final String FILE_OLD = "/tmp/carplay_java.log.1";
    private static final long MAX_FILE_BYTES = 512L * 1024L;
    private static PrintStream file;        /* lazily opened; stays null if it can't be created */
    private static boolean fileTried;
    private static long fileBytes;

    private Log() {}

    public static void setLevel(int l) { level = l; }

    public static void e(String tag, String msg) { out(E, "E", tag, msg); }
    public static void w(String tag, String msg) { out(W, "W", tag, msg); }
    public static void i(String tag, String msg) { out(I, "I", tag, msg); }
    public static void d(String tag, String msg) { out(D, "D", tag, msg); }

    /* throwable overloads (ports pass an exception) */
    public static void e(String tag, String msg, Throwable t) { out(E, "E", tag, msg + " :: " + t); }
    public static void w(String tag, String msg, Throwable t) { out(W, "W", tag, msg + " :: " + t); }

    private static synchronized void out(int l, String p, String tag, String msg) {
        if (l > level) return;
        String line = "[CP/" + p + "][" + tag + "] " + msg;
        System.out.println(line);                       /* HMI slog */
        PrintStream f = logFile();
        if (f != null) {
            int bytes = line.getBytes().length + 1;
            if (fileBytes + bytes > MAX_FILE_BYTES) {
                rotateFile();
                f = logFile();
            }
            if (f != null) {
                f.println(line);                        /* /tmp/carplay_java.log (autoflush) */
                fileBytes += bytes;
            }
        }
    }

    /* Open the file once, best-effort; append + autoflush so a crash still leaves the tail. */
    private static synchronized PrintStream logFile() {
        if (!fileTried) {
            fileTried = true;
            try {
                File current = new File(FILE);
                if (current.length() >= MAX_FILE_BYTES) rotateFiles(current);
                fileBytes = current.exists() ? current.length() : 0L;
                file = new PrintStream(new FileOutputStream(FILE, true), true);
            }
            catch (Throwable t) { file = null; }        /* fall back to stdout only */
        }
        return file;
    }

    private static void rotateFile() {
        if (file != null) {
            try { file.close(); } catch (Throwable t) { }
            file = null;
        }
        try { rotateFiles(new File(FILE)); }
        catch (Throwable t) { }
        fileTried = false;
        fileBytes = 0L;
    }

    private static void rotateFiles(File current) {
        File old = new File(FILE_OLD);
        if (old.exists()) old.delete();
        if (current.exists()) current.renameTo(old);
    }
}
