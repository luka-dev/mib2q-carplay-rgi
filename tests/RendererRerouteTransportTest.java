package com.luka.carplay.rgd;

import com.luka.carplay.framework.Log;
import java.io.DataInputStream;
import java.io.IOException;
import java.io.OutputStream;
import java.lang.reflect.Field;
import java.net.ServerSocket;
import java.net.Socket;
import java.util.ArrayList;
import java.util.List;

/**
 * Field bug: after a CarPlay reroute on a very short turn the cluster pill stays open
 * but black.  BAPBridge always sends CLEAR then MANEUVER (ShortTurnRerouteBlinkTest
 * proves that against a mock renderer).  This test drives the REAL RendererServer
 * (the Java transport) with a fake maneuver_render peer over a real loopback socket,
 * and checks that the transport delivers exactly what was accepted, in order, and
 * that its FRAME_READY/FRAME_CLEARED gate ends up open:
 *
 *  - CLEAR/MANEUVER/PROGRESS/VISIBLE_AREA sequences and reroute bursts,
 *  - a blocked writer (queue backpressure) during a CLEAR/MANEUVER storm,
 *  - the peer processing immediately, in drained batches, with a stale FRAME_READY
 *    in flight, and ACKing (FRAME_CLEARED) late or out of order.
 *
 * Asserted per case:
 *  1. the CLEAR/MANEUVER subsequence the peer receives equals the accepted
 *     sequence (nothing dropped, coalesced, or reordered), so the final MANEUVER
 *     after the last CLEAR is delivered;
 *  2. progress sent after the last MANEUVER/CLEAR arrives after it, latest value;
 *  3. the latest VISIBLE_AREA arrives;
 *  4. before the peer ACKs, the frame gate is closed; afterwards, if the last
 *     accepted command was a MANEUVER, isFrameReady() becomes true, and
 *     pendingClears returns to 0.
 *
 * Prints "RendererRerouteTransportTest: REPRODUCED <case>: <why>" per failure or
 * "... PASS".  Exit code 1 on any failure.
 */
public final class RendererRerouteTransportTest {
    private static int failures;
    private static int cases;

    private static void fail(String c, String why) {
        failures++;
        if (failures <= 15) System.out.println("RendererRerouteTransportTest: REPRODUCED " + c + ": " + why);
    }

    private static Field field(String name) throws Exception {
        Field f = RendererServer.class.getDeclaredField(name);
        f.setAccessible(true);
        return f;
    }
    private static Object get(RendererServer s, String name) throws Exception { return field(name).get(s); }
    private static void set(RendererServer s, String name, Object v) throws Exception { field(name).set(s, v); }

    /* ---------------------------------------------------------------- */
    /* Fake maneuver_render peer                                        */
    /* ---------------------------------------------------------------- */
    static final int MODE_IMMEDIATE = 0;   /* ACK CLEAR at once, FRAME_READY on first MANEUVER */
    static final int MODE_BATCHED = 1;     /* read a drain, ACK each CLEAR, FRAME_READY once at the end */
    static final int MODE_STALE_READY = 2; /* a pre-clear FRAME_READY is in flight before every ACK */
    static final int MODE_LATE_ACK = 3;    /* hold every ACK until the test releases them */
    static final int MODE_OUT_OF_ORDER = 4;/* like LATE_ACK, but a stale FRAME_READY precedes the ACKs */
    static final String[] MODE_NAMES = { "immediate", "batched", "stale-ready", "late-ack", "ack-out-of-order" };

    static final class Peer implements Runnable {
        final Socket sock;
        final DataInputStream in;
        final OutputStream out;
        final int mode;
        final List<byte[]> received = new ArrayList<byte[]>();
        int clears;
        boolean announced;
        boolean sawManeuver;
        volatile boolean stop;
        Thread thread;

        Peer(Socket s, int mode) throws IOException {
            this.sock = s; this.mode = mode;
            this.in = new DataInputStream(s.getInputStream());
            this.out = s.getOutputStream();
        }
        void start() {
            thread = new Thread(this, "fake-renderer");
            thread.setDaemon(true);
            thread.start();
        }
        synchronized void ev(int id) {
            byte[] p = new byte[48];
            p[0] = (byte) id;
            try { out.write(p); out.flush(); } catch (IOException e) { /* server went away */ }
        }
        synchronized int count() { return received.size(); }
        synchronized List<byte[]> snapshot() { return new ArrayList<byte[]>(received); }

        public void run() {
            try {
                while (!stop) {
                    byte[] first = new byte[48];
                    in.readFully(first);
                    List<byte[]> batch = new ArrayList<byte[]>();
                    batch.add(first);
                    if (mode == MODE_BATCHED) {
                        Thread.sleep(25);
                        while (in.available() >= 48) {
                            byte[] more = new byte[48];
                            in.readFully(more);
                            batch.add(more);
                        }
                    }
                    for (int i = 0; i < batch.size(); i++) {
                        byte[] p = batch.get(i);
                        synchronized (this) { received.add(p); }
                        process(p, mode == MODE_BATCHED);
                    }
                    if (mode == MODE_BATCHED && sawManeuver && !announced) {
                        announced = true;
                        ev(0x82);
                    }
                }
            } catch (Exception e) {
                /* socket closed */
            }
        }

        private void process(byte[] p, boolean deferReady) {
            int cmd = p[0];
            if (cmd == 7) {
                synchronized (this) { clears++; }
                announced = false;
                sawManeuver = false;
                if (mode == MODE_STALE_READY) ev(0x82);
                if (mode == MODE_IMMEDIATE || mode == MODE_BATCHED || mode == MODE_STALE_READY) ev(0x83);
            } else if (cmd == 1) {
                sawManeuver = true;
                if (!deferReady && (mode == MODE_IMMEDIATE || mode == MODE_STALE_READY) && !announced) {
                    announced = true;
                    ev(0x82);
                }
            }
        }

        /** LATE_ACK / OUT_OF_ORDER: deliver every held ACK now. */
        void release() {
            int n;
            synchronized (this) { n = clears; }
            if (mode == MODE_OUT_OF_ORDER) ev(0x82);
            for (int i = 0; i < n; i++) ev(0x83);
            if (sawManeuver && !announced) { announced = true; ev(0x82); }
        }
        void close() {
            stop = true;
            try { sock.close(); } catch (IOException e) { }
        }
    }

    /** Output stream whose writes block until opened: simulates a stalled peer/backpressure. */
    static final class GateOut extends OutputStream {
        private final OutputStream real;
        private boolean open;
        GateOut(OutputStream real) { this.real = real; }
        synchronized void open() { open = true; notifyAll(); }
        public synchronized void write(int b) throws IOException { real.write(b); }
        public void write(byte[] b, int off, int len) throws IOException {
            synchronized (this) {
                while (!open) {
                    try { wait(); } catch (InterruptedException e) { return; }
                }
            }
            real.write(b, off, len);
        }
        public void flush() throws IOException { real.flush(); }
        public void close() throws IOException { real.close(); }
    }

    /* ---------------------------------------------------------------- */
    /* Wire classification                                              */
    /* ---------------------------------------------------------------- */
    private static char classify(byte[] p) {
        if (p[0] == 7) return 'C';
        if (p[0] == 1) {
            if (p[3] == -1) return 'B';
            if ((p[1] & 8) != 0) return 'r';
            if (p[44] == RendererServer.PROGRESS_BLINK_LOW) return 'a';
            return 'A';
        }
        if (p[0] == 6) return 'p';
        if (p[0] == 8) return 'V';
        return '?';
    }
    private static boolean isManeuver(char c) { return c == 'A' || c == 'a' || c == 'B' || c == 'r'; }

    private static boolean sendOp(RendererServer s, char op, boolean[] toggle) {
        switch (op) {
        case 'A': return s.sendBapProgressManeuver(2, 1, 90, 0, null, 3, 1, 1, false, false, RendererServer.PROGRESS_FILL);
        case 'a': return s.sendBapProgressManeuver(2, 1, 90, 0, null, 0, 1, 1, false, false, RendererServer.PROGRESS_BLINK_LOW);
        case 'B': return s.sendBapProgressManeuver(2, -1, -45, 0, new int[] { 30 }, 3, 1, 1, false, false, RendererServer.PROGRESS_FILL);
        case 'r': return s.sendBapProgressManeuver(2, 1, 90, 0, null, 3, 1, 1, true, false, RendererServer.PROGRESS_FILL);
        case 'f': return s.sendProgress(1, 1, RendererServer.PROGRESS_FILL);
        case 'l': return s.sendProgress(0, 1, RendererServer.PROGRESS_BLINK_LOW);
        case 'h': return s.sendProgress(0, 1, RendererServer.PROGRESS_BLINK_HIGH);
        case 'o': return s.sendProgress(16, 0, RendererServer.PROGRESS_OFF);
        case 'V':
            toggle[0] = !toggle[0];
            return toggle[0] ? s.sendVisibleArea(59, 27, 210, 153) : s.sendVisibleArea(0, 0, 328, 180);
        default: return s.sendClear();
        }
    }
    private static boolean isProgressOp(char op) { return op == 'f' || op == 'l' || op == 'h' || op == 'o'; }

    private static void waitReady(RendererServer s) throws Exception {
        long end = System.nanoTime() + 3000000000L;
        while (!s.isReady() && System.nanoTime() < end) Thread.sleep(2);
    }

    private static void quiet(RendererServer s, Peer p) throws Exception {
        long end = System.nanoTime() + 4000000000L;
        int last = -1, stable = 0;
        while (System.nanoTime() < end) {
            Thread.sleep(40);
            int wc = ((Integer) get(s, "writeCount")).intValue();
            int n = p.count();
            if (wc == 0 && n == last) { if (++stable >= 2) return; } else stable = 0;
            last = n;
        }
    }

    /* ---------------------------------------------------------------- */
    /* One case                                                         */
    /* ---------------------------------------------------------------- */
    private static void run(String ops, int mode, boolean gated) throws Exception {
        String name = (gated ? "gated:" : "") + MODE_NAMES[mode] + ":"
            + (ops.length() > 24 ? ops.substring(0, 24) + "...(" + ops.length() + ")" : ops);
        cases++;
        RendererServer server = new RendererServer(0);
        Peer peer = null;
        try {
            if (!server.connect()) { fail(name, "listen failed"); return; }
            int port = ((ServerSocket) get(server, "server")).getLocalPort();
            Socket ps = new Socket("127.0.0.1", port);
            ps.setTcpNoDelay(true);
            peer = new Peer(ps, mode);
            peer.start();
            peer.ev(0x81);
            waitReady(server);
            if (!server.isReady()) { fail(name, "renderer handshake"); return; }

            GateOut gate = null;
            if (gated) {
                Object lock = get(server, "lock");
                synchronized (lock) {
                    gate = new GateOut((OutputStream) get(server, "out"));
                    set(server, "out", gate);
                }
            }

            StringBuffer accepted = new StringBuffer();     /* C and maneuver tokens */
            boolean[] toggle = new boolean[1];
            int[] expProg = null;                            /* {level, state} sent after last M/C */
            int[] expVisible = null;
            boolean anyClear = false;
            char lastGate = ' ';
            int rejected = 0;
            for (int i = 0; i < ops.length(); i++) {
                char op = ops.charAt(i);
                boolean ok = sendOp(server, op, toggle);
                if (!ok) { rejected++; continue; }
                if (op == 'C' || isManeuver(op)) {
                    accepted.append(op);
                    expProg = null;
                    lastGate = op;
                    if (op == 'C') anyClear = true;
                } else if (isProgressOp(op)) {
                    int state = op == 'f' ? 1 : op == 'l' ? 2 : op == 'h' ? 3 : 0;
                    expProg = new int[] { op == 'f' ? 1 : op == 'o' ? 16 : 0, state };
                } else if (op == 'V') {
                    expVisible = toggle[0] ? new int[] { 59, 27, 210, 153 } : new int[] { 0, 0, 328, 180 };
                }
            }
            if (gated) {
                /* Backpressure only ever applies once the 32-entry queue is full. */
                if (ops.length() <= 32 && rejected > 0) fail(name, "command rejected although queue not full");
                gate.open();
            } else if (rejected > 0) {
                fail(name, "ungated send rejected (" + rejected + ")");
            }
            quiet(server, peer);

            /* 1. CLEAR/MANEUVER subsequence delivered exactly, in order. */
            List<byte[]> got = peer.snapshot();
            StringBuffer seen = new StringBuffer();
            int lastManPos = -1;
            for (int i = 0; i < got.size(); i++) {
                char c = classify(got.get(i));
                if (c == 'C' || isManeuver(c)) { seen.append(c); lastManPos = i; }
            }
            if (!seen.toString().equals(accepted.toString()))
                fail(name, "peer saw CLEAR/MANEUVER order '" + seen + "' but accepted '" + accepted + "'");

            /* 2. progress sent after the last MANEUVER/CLEAR arrives after it, latest value. */
            if (expProg != null) {
                byte[] lastProg = null;
                for (int i = lastManPos + 1; i < got.size(); i++)
                    if (classify(got.get(i)) == 'p') lastProg = got.get(i);
                if (lastProg == null)
                    fail(name, "progress sent after last MANEUVER never delivered after it");
                else if ((lastProg[2] & 255) != expProg[0] || (lastProg[4] & 255) != expProg[1])
                    fail(name, "stale progress delivered last (level=" + (lastProg[2] & 255)
                        + " state=" + (lastProg[4] & 255) + ")");
            }

            /* 3. latest visible area delivered. */
            if (expVisible != null) {
                byte[] lastV = null;
                for (int i = 0; i < got.size(); i++) if (classify(got.get(i)) == 'V') lastV = got.get(i);
                if (lastV == null || (((lastV[2] & 255) << 8) | (lastV[3] & 255)) != expVisible[0]
                        || (((lastV[8] & 255) << 8) | (lastV[9] & 255)) != expVisible[3])
                    fail(name, "latest VISIBLE_AREA not delivered");
            }

            /* 4. frame gate. */
            if (mode == MODE_LATE_ACK || mode == MODE_OUT_OF_ORDER) {
                if (anyClear && server.isFrameReady())
                    fail(name, "gate open although CLEAR ACKs are still outstanding");
                peer.release();
            }
            boolean expectReady = lastGate != ' ' && lastGate != 'C';
            long end = System.nanoTime() + 2500000000L;
            while (System.nanoTime() < end) {
                boolean pendingZero = ((Integer) get(server, "pendingClears")).intValue() == 0;
                if (pendingZero && server.isFrameReady() == expectReady) break;
                Thread.sleep(10);
            }
            int pending = ((Integer) get(server, "pendingClears")).intValue();
            if (pending != 0) fail(name, "pendingClears stuck at " + pending + " after all ACKs");
            else if (server.isFrameReady() != expectReady)
                fail(name, expectReady
                    ? "final MANEUVER delivered but ctx80 gate never opened (isFrameReady=false)"
                    : "gate open although last command was CLEAR");
        } finally {
            if (peer != null) peer.close();
            server.dispose();
        }
    }

    private static String repeat(String s, int n) {
        StringBuffer b = new StringBuffer();
        for (int i = 0; i < n; i++) b.append(s);
        return b.toString();
    }

    public static void main(String[] args) throws Exception {
        Log.setLevel(-1);
        String[] seqs = {
            "AfCAlhl", "AfCAhlh", "ACCA", "ACCCA", "ACA", "ACACACA", "ACBCA", "ACBCACB",
            "AflCfAhl", "ACflAhl", "AChAlh", "aCahl", "ACrhl", "AfCrh", "CA", "CfA",
            "AVCVA", "AVhCVlA", "VACVhlCAVl"
        };
        for (int m = 0; m < MODE_NAMES.length; m++)
            for (int i = 0; i < seqs.length; i++) run(seqs[i], m, false);

        /* Reroute storm against a stalled writer: more commands than queue capacity. */
        String storm = repeat("CA", 20) + "hl";            /* 42 ops, capacity 32 */
        String stormShort = repeat("CA", 12) + "hlV";       /* fits: must all be accepted */
        for (int m = 0; m < MODE_NAMES.length; m++) {
            run(storm, m, true);
            run(stormShort, m, true);
        }

        if (failures > 0) {
            System.out.println("RendererRerouteTransportTest: REPRODUCED " + failures + " failures in " + cases + " cases");
            System.exit(1);
        }
        System.out.println("RendererRerouteTransportTest: PASS (" + cases
            + " cases: CLEAR/MANEUVER order, progress/viewport delivery, ACK gate under immediate/batched/stale/late/out-of-order peers and a stalled writer)");
    }
}
