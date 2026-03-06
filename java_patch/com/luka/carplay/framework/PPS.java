/*
 * CarPlay Hook Framework - Unified PPS Watcher
 *
 * Watches QNX PPS files with support for:
 * - Text mode: key:type:value parsing with typed accessors
 * - Binary mode: raw bytes with helper methods
 *
 * Usage:
 *   PPS pps = new PPS("/ramdisk/pps/iap2/routeguidance", listener);
 *   pps.start();
 *   // ... listener receives callbacks ...
 *   pps.stop();
 *
 *
 */
package com.luka.carplay.framework;

import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;

public class PPS {

    private static final String TAG = "PPS";

    /* Mode */
    public static final int TEXT = 0;
    public static final int BINARY = 1;

    /* Config */
    private final String path;
    private final int mode;
    private final int bufferSize;
    private final Listener listener;
    private final String label;

    /* State */
    private volatile boolean running;
    private Thread thread;
    private volatile FileInputStream stream;
    /* Chunk accumulation: C hook sends _more:b:true when buffer overflows */
    private Data pendingChunk;

    /* ============================================================
     * Listener
     * ============================================================ */

    public interface Listener {
        /** Called when data is received. For TEXT mode, data is also parsed. */
        void onData(byte[] raw, int len, Data parsed);

        /** Called on disconnect or error. */
        void onError(String reason);
    }

    /* ============================================================
     * Parsed Data (for TEXT mode)
     * ============================================================ */

    public static class Data {
        private static final int MAX = 256;
        private String[] keys = new String[MAX];
        private String[] vals = new String[MAX];
        private int count = 0;

        void put(String key, String value) {
            if (count < MAX) {
                keys[count] = key;
                vals[count] = value;
                count++;
            }
        }

        /** Merge all entries from another Data (for chunk accumulation). */
        void mergeFrom(Data other) {
            if (other == null) return;
            for (int i = 0; i < other.count; i++) {
                if ("_more".equals(other.keys[i])) continue;
                put(other.keys[i], other.vals[i]);
            }
        }

        /** Get string value. */
        public String str(String key, String def) {
            for (int i = 0; i < count; i++) {
                if (key.equals(keys[i])) return vals[i];
            }
            return def;
        }

        /** Get string value, null if not found. */
        public String str(String key) {
            return str(key, null);
        }

        /** Get int value. */
        public int num(String key, int def) {
            String v = str(key, null);
            if (v == null) return def;
            try {
                return Integer.parseInt(v.trim());
            } catch (Exception e) {
                return def;
            }
        }

        /** Get long value. */
        public long num64(String key, long def) {
            String v = str(key, null);
            if (v == null) return def;
            try {
                return Long.parseLong(v.trim());
            } catch (Exception e) {
                return def;
            }
        }

        /** Get boolean value. */
        public boolean bool(String key, boolean def) {
            String v = str(key, null);
            if (v == null) return def;
            v = v.trim();
            if ("true".equals(v) || "1".equals(v)) return true;
            if ("false".equals(v) || "0".equals(v)) return false;
            return def;
        }

        /** Get comma-separated int array. */
        public int[] intList(String key) {
            String v = str(key, null);
            if (v == null || v.length() == 0) return null;

            int commas = 0;
            for (int i = 0; i < v.length(); i++) {
                if (v.charAt(i) == ',') commas++;
            }

            int[] result = new int[commas + 1];
            int start = 0, idx = 0;
            for (int i = 0; i <= v.length(); i++) {
                if (i == v.length() || v.charAt(i) == ',') {
                    try {
                        result[idx++] = Integer.parseInt(v.substring(start, i).trim());
                    } catch (Exception e) {
                        result[idx++] = 0;
                    }
                    start = i + 1;
                }
            }
            return result;
        }

        /** Get delimited string array. */
        public String[] strList(String key, char delim) {
            String v = str(key, null);
            if (v == null || v.length() == 0) return null;

            int count = 1;
            for (int i = 0; i < v.length(); i++) {
                if (v.charAt(i) == delim) count++;
            }

            String[] result = new String[count];
            int start = 0, idx = 0;
            for (int i = 0; i <= v.length(); i++) {
                if (i == v.length() || v.charAt(i) == delim) {
                    result[idx++] = v.substring(start, i);
                    start = i + 1;
                }
            }
            return result;
        }

        /** Check if key exists. */
        public boolean has(String key) {
            for (int i = 0; i < count; i++) {
                if (key.equals(keys[i])) return true;
            }
            return false;
        }

        /** Get number of entries. */
        public int size() {
            return count;
        }
    }

    /* ============================================================
     * Constructor
     * ============================================================ */

    /**
     * Create PPS watcher.
     * @param path PPS file path (without ?wait)
     * @param mode TEXT or BINARY
     * @param bufferSize max read size
     * @param listener callback
     */
    public PPS(String path, int mode, int bufferSize, Listener listener) {
        this.path = path;
        this.mode = mode;
        this.bufferSize = bufferSize;
        this.listener = listener;
        this.label = deriveLabel(path);
    }

    /** Create TEXT mode watcher with 8K buffer. */
    public PPS(String path, Listener listener) {
        this(path, TEXT, 8192, listener);
    }

    /* ============================================================
     * Lifecycle
     * ============================================================ */

    public synchronized void start() {
        if (running) return;

        ensureFile();
        running = true;

        thread = new Thread(new Runnable() {
            public void run() { loop(); }
        }, "PPS-" + path.substring(path.lastIndexOf('/') + 1));
        thread.setDaemon(true);
        thread.start();

        Log.i(TAG, "Started: " + path + " mode=" + (mode == TEXT ? "text" : "binary"));
    }

    public synchronized void stop() {
        if (!running) return;
        running = false;
        pendingChunk = null;

        if (stream != null) {
            try { stream.close(); } catch (Exception e) {}
            stream = null;
        }
        if (thread != null) {
            thread.interrupt();
            thread = null;
        }

        Log.i(TAG, "Stopped: " + path);
    }

    public boolean isRunning() {
        return running;
    }

    public String getPath() {
        return path;
    }

    /* ============================================================
     * Watch Loop
     * ============================================================ */

    private void loop() {
        byte[] buf = new byte[bufferSize];
        FileInputStream in = null;

        while (running) {
            try {
                if (in == null) {
                    in = new FileInputStream(path + "?wait");
                    stream = in;
                }

                int len = in.read(buf, 0, buf.length);
                if (!running) break;

                if (len <= 0) {
                    close(in);
                    in = null;
                    stream = null;
                    sleep(100);
                    continue;
                }

                String preview = (mode == TEXT)
                    ? previewText(buf, len, 96)
                    : previewHex(buf, len, 16);
                Log.d(TAG, "[PPS:" + label + "] Read " + len + " bytes from " + path + "?wait" +
                      " preview=\"" + preview + "\"");

                if (listener != null) {
                    Data parsed = (mode == TEXT) ? parse(buf, len) : null;
                    if (parsed != null && parsed.bool("_more", false)) {
                        /* Intermediate chunk — accumulate and wait for final */
                        if (pendingChunk == null) {
                            pendingChunk = new Data();
                        }
                        pendingChunk.mergeFrom(parsed);
                        Log.d(TAG, "[PPS:" + label + "] Chunk accumulated ("
                              + pendingChunk.size() + " keys so far)");
                    } else {
                        /*
                         * Final chunk or non-chunked write.
                         * Note: buf/len are from the last read only; the merged
                         * Data contains all keys.  RouteGuidance uses only the
                         * parsed Data, so this is safe.
                         */
                        if (pendingChunk != null) {
                            pendingChunk.mergeFrom(parsed);
                            parsed = pendingChunk;
                            pendingChunk = null;
                            Log.d(TAG, "[PPS:" + label + "] Chunks merged ("
                                  + parsed.size() + " keys total)");
                        }
                        listener.onData(buf, len, parsed);
                    }
                }

            } catch (Exception e) {
                if (running) {
                    Log.w(TAG, "Error: " + e.getMessage());
                    if (listener != null) listener.onError(e.getMessage());
                    sleep(500);
                }
                pendingChunk = null; /* discard stale partial chunk on reconnect */
                close(in);
                in = null;
                stream = null;
            }
        }

        close(in);
        stream = null;
    }

    /* ============================================================
     * Text Parsing
     * ============================================================ */

    private Data parse(byte[] buf, int len) {
        Data data = new Data();

        String content;
        try {
            content = new String(buf, 0, len, "UTF-8");
        } catch (Exception e) {
            content = new String(buf, 0, len);
        }

        int pos = 0;
        while (pos < content.length()) {
            int eol = content.indexOf('\n', pos);
            if (eol < 0) eol = content.length();

            String line = content.substring(pos, eol);
            pos = eol + 1;

            if (line.length() == 0 || line.charAt(0) == '@') continue;

            int c1 = line.indexOf(':');
            if (c1 < 0) continue;

            int c2 = line.indexOf(':', c1 + 1);

            String key = line.substring(0, c1);
            String val = (c2 >= 0 && c2 + 1 <= line.length())
                ? line.substring(c2 + 1)
                : "";

            data.put(key, val);
        }

        return data;
    }

    private static String previewText(byte[] buf, int len, int max) {
        int cap = len;
        if (cap > max) cap = max;

        String s;
        try {
            s = new String(buf, 0, cap, "UTF-8");
        } catch (Exception e) {
            s = new String(buf, 0, cap);
        }

        StringBuffer out = new StringBuffer();
        for (int i = 0; i < s.length(); i++) {
            char c = s.charAt(i);
            if (c == '\n') out.append("\\n");
            else if (c == '\r') out.append("\\r");
            else if (c == '\t') out.append("\\t");
            else if (c < 0x20) out.append('.');
            else out.append(c);
        }
        if (len > cap) out.append("...");
        return out.toString();
    }

    private static String previewHex(byte[] buf, int len, int max) {
        int cap = len;
        if (cap > max) cap = max;
        StringBuffer out = new StringBuffer();
        for (int i = 0; i < cap; i++) {
            int v = buf[i] & 0xFF;
            if (i > 0) out.append(' ');
            if (v < 16) out.append('0');
            out.append(Integer.toHexString(v).toUpperCase());
        }
        if (len > cap) out.append(" ...");
        return out.toString();
    }

    private static String deriveLabel(String path) {
        if (path == null || path.length() == 0) return "pps";
        int slash = path.lastIndexOf('/');
        if (slash >= 0 && slash + 1 < path.length()) {
            return path.substring(slash + 1);
        }
        return path;
    }

    /* ============================================================
     * Binary Helpers (static utilities)
     * ============================================================ */

    /** Read big-endian uint16. */
    public static int be16(byte[] d, int o) {
        return ((d[o] & 0xFF) << 8) | (d[o+1] & 0xFF);
    }

    /** Read big-endian uint32. */
    public static long be32(byte[] d, int o) {
        return ((long)(d[o] & 0xFF) << 24) | ((long)(d[o+1] & 0xFF) << 16) |
               ((long)(d[o+2] & 0xFF) << 8) | (d[o+3] & 0xFF);
    }

    /** Read little-endian uint16. */
    public static int le16(byte[] d, int o) {
        return (d[o] & 0xFF) | ((d[o+1] & 0xFF) << 8);
    }

    /** Read little-endian uint32. */
    public static long le32(byte[] d, int o) {
        return (d[o] & 0xFF) | ((long)(d[o+1] & 0xFF) << 8) |
               ((long)(d[o+2] & 0xFF) << 16) | ((long)(d[o+3] & 0xFF) << 24);
    }

    /* ============================================================
     * Utilities
     * ============================================================ */

    private void ensureFile() {
        try {
            File f = new File(path);
            if (!f.exists()) {
                File p = f.getParentFile();
                if (p != null && !p.exists()) p.mkdirs();
                f.createNewFile();
                Log.i(TAG, "Created: " + path);
            }
        } catch (Exception e) {
            Log.w(TAG, "Cannot create " + path + ": " + e.getMessage());
        }
    }

    private static void close(FileInputStream s) {
        if (s != null) try { s.close(); } catch (Exception e) {}
    }

    private static void sleep(int ms) {
        try { Thread.sleep(ms); } catch (Exception e) {}
    }
}
