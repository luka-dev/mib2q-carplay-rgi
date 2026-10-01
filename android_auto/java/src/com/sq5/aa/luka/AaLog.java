/*
 * Small file log for the SQ5 Android Auto -> luka adapter: /tmp/sq5_aa.log (RAM, cleared on
 * reboot).  Same line format as aa-cluster's com.sq5.aa.AaLog ("<millis> <message>").
 *
 * Asynchronous: callers (Android Auto DSI thread, luka's cluster-switch worker, and the stock
 * DisplayManager/HMI threads for blocked context switches) only enqueue; one daemon thread owns
 * the file I/O.  Bounded queue (oldest dropped), size-capped file.  Never throws.
 * luka's own stack keeps logging to /tmp/carplay_java.log.
 */
package com.sq5.aa.luka;

import java.io.File;
import java.io.FileOutputStream;

public final class AaLog {
    private static final String PATH = "/tmp/sq5_aa.log";
    private static final long MAX_BYTES = 512 * 1024;
    private static final int QUEUE = 256;
    private static volatile boolean enabled = true;

    private static final Object LOCK = new Object();
    private static final String[] queue = new String[QUEUE];
    private static int head, tail, count, dropped;
    private static Thread writer;

    private AaLog() {
    }

    /** Host tests switch file output off. */
    public static void setEnabled(boolean on) {
        enabled = on;
    }

    public static void log(String msg) {
        if (!enabled) return;
        try {
            String line = System.currentTimeMillis() + " " + msg + "\n";
            synchronized (LOCK) {
                if (writer == null) {
                    Thread t = new Thread("sq5-aa-log") {
                        public void run() { writeLoop(); }
                    };
                    t.setDaemon(true);
                    t.start();
                    writer = t;
                }
                if (count == QUEUE) {
                    queue[head] = null;
                    head = (head + 1) % QUEUE;
                    count--;
                    dropped++;
                }
                queue[tail] = line;
                tail = (tail + 1) % QUEUE;
                count++;
                LOCK.notifyAll();
            }
        } catch (Throwable t) {
            /* logging must never affect Android Auto or the HMI */
        }
    }

    private static void writeLoop() {
        while (true) {
            String line;
            int lost;
            try {
                synchronized (LOCK) {
                    while (count == 0) LOCK.wait();
                    line = queue[head];
                    queue[head] = null;
                    head = (head + 1) % QUEUE;
                    count--;
                    lost = dropped;
                    dropped = 0;
                }
                if (lost > 0) write(System.currentTimeMillis() + " [log] dropped " + lost + " lines\n");
                write(line);
            } catch (Throwable t) {
                /* keep the writer alive */
            }
        }
    }

    private static void write(String line) {
        FileOutputStream out = null;
        try {
            File f = new File(PATH);
            boolean append = !(f.exists() && f.length() > MAX_BYTES);
            out = new FileOutputStream(PATH, append);
            out.write(line.getBytes());
        } catch (Throwable t) {
            /* ignore */
        } finally {
            if (out != null) {
                try { out.close(); } catch (Throwable t) { /* ignore */ }
            }
        }
    }
}
