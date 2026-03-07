/*
 * CarPlay Hook Framework - Unified Logging
 *
 * Single logging solution for all modules:
 * - Configurable log levels (DEBUG, INFO, WARN, ERROR)
 * - Single log file output
 * - Thread-safe operation
 * - Enable/disable via static flag
 *
 */
package com.luka.carplay.framework;

import java.io.FileWriter;

public class Log {

    /* Log levels */
    public static final int LEVEL_DEBUG = 0;
    public static final int LEVEL_INFO = 1;
    public static final int LEVEL_WARN = 2;
    public static final int LEVEL_ERROR = 3;
    public static final int LEVEL_NONE = 4;

    /* Configuration */
    private static String logPath = "/tmp/carplay_hook.log";
    private static int minLevel = LEVEL_DEBUG;
    private static boolean enabled = true;
    private static boolean includeTimestamp = true;


    /* Prevent instantiation */
    private Log() {}

    /* ============================================================
     * Configuration
     * ============================================================ */

    /**
     * Set log file path.
     */
    public static void setPath(String path) {
        logPath = path;
    }

    /**
     * Set minimum log level.
     */
    public static void setLevel(int level) {
        minLevel = level;
    }

    /**
     * Enable or disable logging globally.
     */
    public static void setEnabled(boolean enabled) {
        Log.enabled = enabled;
    }

    /**
     * Include timestamps in log output.
     */
    public static void setIncludeTimestamp(boolean include) {
        includeTimestamp = include;
    }


    /* ============================================================
     * Logging Methods
     * ============================================================ */

    /**
     * Log debug message.
     */
    public static void d(String tag, String msg) {
        write(LEVEL_DEBUG, tag, msg);
    }

    /**
     * Log info message.
     */
    public static void i(String tag, String msg) {
        write(LEVEL_INFO, tag, msg);
    }

    /**
     * Log warning message.
     */
    public static void w(String tag, String msg) {
        write(LEVEL_WARN, tag, msg);
    }

    /**
     * Log error message.
     */
    public static void e(String tag, String msg) {
        write(LEVEL_ERROR, tag, msg);
    }

    /**
     * Log error message with exception.
     */
    public static void e(String tag, String msg, Throwable t) {
        write(LEVEL_ERROR, tag, msg + ": " + t.getClass().getName() + ": " + t.getMessage());
    }

    /**
     * Core logging method.
     */
    public static synchronized void write(int level, String tag, String msg) {
        if (!enabled || level < minLevel) {
            return;
        }

        StringBuffer sb = new StringBuffer();

        /* Timestamp */
        if (includeTimestamp) {
            sb.append(System.currentTimeMillis());
            sb.append(" ");
        }

        /* Level */
        sb.append("[");
        sb.append(levelStr(level));
        sb.append("] ");

        /* Tag */
        if (tag != null) {
            sb.append("[");
            sb.append(tag);
            sb.append("] ");
        }

        /* Message */
        sb.append(msg);

        String line = sb.toString();

        /* File */
        if (logPath != null) {
            FileWriter fw = null;
            try {
                fw = new FileWriter(logPath, true);
                fw.write(line);
                fw.write("\n");
            } catch (Exception e) {
                // ignore
            } finally {
                if (fw != null) {
                    try { fw.close(); } catch (Exception e) {}
                }
            }
        }
    }

    /**
     * Log hex dump of binary data.
     */
    public static void hexdump(String tag, String prefix, byte[] data, int offset, int len, int maxBytes) {
        if (!enabled || LEVEL_DEBUG < minLevel) {
            return;
        }

        if (data == null || len <= 0) {
            return;
        }

        int dumpLen = (maxBytes > 0 && len > maxBytes) ? maxBytes : len;

        StringBuffer sb = new StringBuffer();
        if (prefix != null) {
            sb.append(prefix);
            sb.append(" ");
        }
        sb.append("len=");
        sb.append(len);
        sb.append(" bytes=");

        for (int i = 0; i < dumpLen; i++) {
            int b = data[offset + i] & 0xFF;
            sb.append(hexChar(b >> 4));
            sb.append(hexChar(b & 0x0F));
            sb.append(" ");
        }

        if (dumpLen < len) {
            sb.append("...");
        }

        write(LEVEL_DEBUG, tag, sb.toString());
    }

    private static String levelStr(int level) {
        switch (level) {
            case LEVEL_DEBUG: return "DBG";
            case LEVEL_INFO:  return "INF";
            case LEVEL_WARN:  return "WRN";
            case LEVEL_ERROR: return "ERR";
            default:          return "???";
        }
    }

    private static char hexChar(int nibble) {
        return (nibble < 10) ? (char)('0' + nibble) : (char)('A' + nibble - 10);
    }
}
