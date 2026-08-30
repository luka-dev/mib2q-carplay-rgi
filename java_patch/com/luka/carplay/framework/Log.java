/*
 * Minimal file-only logger.  Never write to System.out/System.err: lsd routes
 * them through the shared HMI slog path, adding boot-time contention and noisy
 * duplicate records.  /tmp/carplay_java.log is bounded and rotated so it stays
 * independently SSH-tailable.  Java 1.4 / Foundation 1.1.
 */
package com.luka.carplay.framework;

import java.io.FileOutputStream;
import java.io.File;
import java.io.PrintStream;

public final class Log {
    public static final int E = 0, W = 1, I = 2, D = 3;
    /* Production emits WARN and ERROR only.  INFO carries the state-transition trace that is
     * worth having while diagnosing (session start, module ready, context/geometry decisions);
     * it stays compiled in and is switched on without a rebuild - and without a unit reboot -
     * by `touch /mnt/app/carplay_verbose` or /tmp/carplay_verbose. */
    private static final String VERBOSE_MARKERS = "/mnt/app/carplay_verbose:/tmp/carplay_verbose";
    private static int level = resolveInitialLevel();

    private static int resolveInitialLevel() {
        try {
            int from = 0;
            while (from < VERBOSE_MARKERS.length()) {
                int sep = VERBOSE_MARKERS.indexOf(':', from);
                String path = sep < 0 ? VERBOSE_MARKERS.substring(from)
                                      : VERBOSE_MARKERS.substring(from, sep);
                if (new File(path).exists()) return I;
                if (sep < 0) break;
                from = sep + 1;
            }
        } catch (Throwable t) {
            /* Never let logging setup break startup. */
        }
        return W;
    }

    private static final String FILE = "/tmp/carplay_java.log";
    private static final String FILE_OLD = "/tmp/carplay_java.log.1";
    private static final long MAX_FILE_BYTES = 512L * 1024L;
    private static PrintStream file;        /* lazily opened; stays null if it can't be created */
    private static boolean fileTried;
    private static long fileBytes;
    private static final Object LOCK = new Object();
    private static final int QUEUE_CAPACITY = 256;
    private static final String[] queue = new String[QUEUE_CAPACITY];
    private static int queueHead;
    private static int queueTail;
    private static int queueCount;
    private static Thread writerThread;

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
        synchronized (LOCK) {
            ensureWriterLocked();
            if (writerThread == null) return;
            if (queueCount == QUEUE_CAPACITY) {
                queue[queueHead] = null;
                queueHead = (queueHead + 1) % QUEUE_CAPACITY;
                queueCount--;
            }
            queue[queueTail] = line;
            queueTail = (queueTail + 1) % QUEUE_CAPACITY;
            queueCount++;
            LOCK.notifyAll();
        }
    }

    /* No stock HMI/BAP callback ever performs file I/O. A single daemon owns
     * open/write/flush/rotation; overload drops the oldest diagnostics rather
     * than blocking lsd. */
    private static void ensureWriterLocked() {
        if (writerThread != null) return;
        Thread t = new Thread(new Runnable() {
            public void run() { writerLoop(); }
        }, "carplay-log-writer");
        t.setDaemon(true);
        try {
            t.start();
            writerThread = t;
        } catch (Throwable ignored) {
            writerThread = null;
        }
    }

    private static void writerLoop() {
        while (true) {
            String line;
            synchronized (LOCK) {
                while (queueCount == 0) {
                    try { LOCK.wait(); } catch (InterruptedException ignored) { }
                }
                line = queue[queueHead];
                queue[queueHead] = null;
                queueHead = (queueHead + 1) % QUEUE_CAPACITY;
                queueCount--;
            }
            /* Logging must remain best-effort even if the old Foundation/QNX
             * file stack throws something other than IOException.  Never let
             * the sole writer die while writerThread remains non-null: that
             * would make every later diagnostic queue forever and churn the
             * bounded ring for no useful output. */
            try { writeLine(line); }
            catch (Throwable ignored) { resetFileAfterFailure(); }
        }
    }

    private static void writeLine(String line) {
        if (line == null) return;
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
    private static PrintStream logFile() {
        if (!fileTried) {
            fileTried = true;
            try {
                File current = new File(FILE);
                if (current.length() >= MAX_FILE_BYTES) rotateFiles(current);
                fileBytes = current.exists() ? current.length() : 0L;
                file = new PrintStream(new FileOutputStream(FILE, true), true);
            }
            catch (Throwable t) { file = null; }        /* logging is best-effort */
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

    private static void resetFileAfterFailure() {
        if (file != null) {
            try { file.close(); } catch (Throwable ignored) { }
            file = null;
        }
        fileTried = false;
        fileBytes = 0L;
    }

    private static void rotateFiles(File current) {
        File old = new File(FILE_OLD);
        if (old.exists()) old.delete();
        if (current.exists()) current.renameTo(old);
    }
}
