/*
 * gal hook -> Java lane feed: /tmp/sq5_aa_lanes, written by aa-cluster-live src/navxlate.c from the lanes
 * of steps[0] of every Android Auto 0x8006 NavigationState (the legacy 0x8004 turn event the DSI delivers
 * has no lanes).  Same in-place contract as TurnCardFeed in the other direction: one fixed-length record
 * (256 bytes) rewritten at offset 0, never truncated or renamed (/tmp is /dev/shmem), only when the
 * lanes change.
 *
 * Record (ASCII, '\n'-separated, space-padded, last byte '\n'):
 *   SQ5L1 <seq>
 *   lanes=<n>                              0..8; 0 = the step has no lanes
 *   <shape>,<hl>[,<shape>,<hl>...]|...     one group per lane, in the order Android Auto sends them;
 *                                          shape = NavigationLane.LaneDirection.Shape 0..9, hl = is_highlighted
 *   end=<seq>
 * A record whose two seq values differ (read racing the write) or that does not parse is rejected.
 *
 * Pure parse + one small read; never throws.
 */
package com.sq5.aa.luka;

import java.io.RandomAccessFile;

public final class AaLaneFeed {
    public static final String PATH = "/tmp/sq5_aa_lanes";
    public static final int MAX_LANES = 8;
    public static final int MAX_DIRS = 4;
    static final int READ_MAX = 1024;

    /** Lanes of the current step.  shape[i][j] / hl[i][j] = direction j of lane i (lane 0 first as sent). */
    public static final class Lanes {
        public final int count;
        public final int[][] shape;
        public final boolean[][] hl;
        /** canonical text ("" = no lanes), equal content <=> equal key */
        public final String key;

        Lanes(int[][] shape, boolean[][] hl, String key) {
            this.count = shape.length;
            this.shape = shape;
            this.hl = hl;
            this.key = key;
        }
    }

    public static final Lanes NONE = new Lanes(new int[0][], new boolean[0][], "");

    private AaLaneFeed() {
    }

    /** Parses one record; null = torn, garbled or not a record. */
    public static Lanes parse(byte[] buf, int len) {
        try {
            if (buf == null || len <= 0) return null;
            StringBuffer sb = new StringBuffer(len);
            for (int i = 0; i < len && i < buf.length; i++) sb.append((char) (buf[i] & 0xff));
            String s = sb.toString();
            int a = s.indexOf('\n'), b = a < 0 ? -1 : s.indexOf('\n', a + 1);
            int c = b < 0 ? -1 : s.indexOf('\n', b + 1), d = c < 0 ? -1 : s.indexOf('\n', c + 1);
            if (d < 0) return null;
            String head = s.substring(0, a), count = s.substring(a + 1, b), body = s.substring(b + 1, c);
            String end = s.substring(c + 1, d);
            if (!head.startsWith("SQ5L1 ") || !end.startsWith("end=")) return null;
            String seq = head.substring(6);
            if (seq.length() == 0 || !seq.equals(end.substring(4)) || !digits(seq)) return null;
            if (!count.startsWith("lanes=") || !digits(count.substring(6)) || count.length() > 8) return null;
            int n = Integer.parseInt(count.substring(6));
            if (n > MAX_LANES) return null;
            if (n == 0) return body.length() == 0 ? NONE : null;
            int[][] shape = new int[n][];
            boolean[][] hl = new boolean[n][];
            int start = 0;
            for (int i = 0; i < n; i++) {
                int bar = body.indexOf('|', start);
                if ((bar < 0) != (i == n - 1)) return null;          /* exactly n groups */
                String lane = body.substring(start, bar < 0 ? body.length() : bar);
                start = bar + 1;
                int k = lane.length() == 0 ? 0 : (lane.length() + 1) / 4;
                if (lane.length() != (k == 0 ? 0 : 4 * k - 1) || k > MAX_DIRS) return null;
                shape[i] = new int[k];
                hl[i] = new boolean[k];
                for (int j = 0; j < k; j++) {
                    char sh = lane.charAt(4 * j), comma = lane.charAt(4 * j + 1), h = lane.charAt(4 * j + 2);
                    if (sh < '0' || sh > '9' || comma != ',' || (h != '0' && h != '1')
                            || (j + 1 < k && lane.charAt(4 * j + 3) != ',')) return null;
                    shape[i][j] = sh - '0';
                    hl[i][j] = h == '1';
                }
            }
            return new Lanes(shape, hl, n + ":" + body);
        } catch (Throwable t) {
            return null;
        }
    }

    private static boolean digits(String s) {
        if (s.length() == 0 || s.length() > 10) return false;
        for (int i = 0; i < s.length(); i++) {
            if (s.charAt(i) < '0' || s.charAt(i) > '9') return false;
        }
        return true;
    }

    /** Reads and parses the record; a torn read is retried once.  Missing/unreadable/garbled -> NONE. */
    public static Lanes read(String path) {
        for (int attempt = 0; attempt < 2; attempt++) {
            byte[] buf = readFile(path);
            if (buf == null) return NONE;
            Lanes l = parse(buf, buf.length);
            if (l != null) return l;
            try { Thread.sleep(20L); } catch (Throwable t) { /* ignore */ }
        }
        return NONE;
    }

    private static byte[] readFile(String path) {
        RandomAccessFile f = null;
        try {
            f = new RandomAccessFile(path, "r");
            byte[] b = new byte[READ_MAX];
            int n = 0, r;
            while (n < b.length && (r = f.read(b, n, b.length - n)) > 0) n += r;
            byte[] out = new byte[n];
            System.arraycopy(b, 0, out, 0, n);
            return out;
        } catch (Throwable t) {
            return null;
        } finally {
            if (f != null) try { f.close(); } catch (Throwable t) { /* ignore */ }
        }
    }

    /**
     * LaneDirection.Shape -> luka lane angle (signed degrees, negative = left, 0 = straight, +/-180 =
     * U-turn: the iAP2 laneAngles convention BAPBridge quantizes to the FctID 24 direction), or 1000 for
     * UNKNOWN / out of range (luka's "unknown" sentinel).
     */
    public static int angle(int shape) {
        switch (shape) {
            case 1: return 0;        /* STRAIGHT */
            case 2: return -45;      /* SLIGHT_LEFT */
            case 3: return 45;       /* SLIGHT_RIGHT */
            case 4: return -90;      /* NORMAL_LEFT */
            case 5: return 90;       /* NORMAL_RIGHT */
            case 6: return -135;     /* SHARP_LEFT */
            case 7: return 135;      /* SHARP_RIGHT */
            case 8: return -180;     /* U_TURN_LEFT */
            case 9: return 180;      /* U_TURN_RIGHT */
            default: return 1000;
        }
    }
}
