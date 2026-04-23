/*
 * CarPlay Hook Bus - Java client
 *
 * Full-duplex TCP bus to libcarplay_hook.so (hook = server on 127.0.0.1:19810).
 * Replaces PPS-file polling for every hook<->Java channel.  See bus_protocol.h
 * for wire format.
 *
 * Usage:
 *   CarplayBus.getInstance().start();
 *   CarplayBus.getInstance().on(EVT_COVERART, new CarplayBus.Listener() {
 *       public void onFrame(int type, int flags, byte[] payload, int len) {
 *           ...parse text or binary payload...
 *       }
 *   });
 *   CarplayBus.getInstance().sendText(CMD_CURSOR_POS, 0, payloadText);
 *
 * Threading:
 *   - single I/O thread handles socket connect, frame read, dispatch.
 *   - listener callbacks run on the I/O thread; keep them short or hand off
 *     to another executor.
 *   - send() is non-blocking: enqueues and wakes the I/O thread.  On reconnect
 *     the queue is flushed (hook will also replay sticky state after CMD_SYNC_REQ).
 *
 * Java 1.2 compatible: no generics, no lambdas, no try-with-resources.
 */

package com.luka.carplay.framework;

import java.io.DataInputStream;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.IOException;
import java.net.InetSocketAddress;
import java.net.Socket;

public class CarplayBus {

    private static final String TAG = "CarplayBus";

    /* ============================================================
     * Protocol constants - keep in sync with bus_protocol.h
     * ============================================================ */
    public static final String HOST = "127.0.0.1";
    public static final int    PORT = 19810;
    public static final int    MAGIC = 0x43504842;
    public static final int    HEADER_SIZE = 16;
    public static final int    MAX_PAYLOAD = 128 * 1024;

    /* Flags */
    public static final int FLAG_STICKY  = 0x01;
    public static final int FLAG_BINARY  = 0x02;
    public static final int FLAG_REPLAY  = 0x04;

    /* Event types (Hook -> Java) */
    public static final int EVT_HELLO       = 0x0001;
    public static final int EVT_SYNC_BEGIN  = 0x0002;
    public static final int EVT_SYNC_END    = 0x0003;
    public static final int EVT_PONG        = 0x0004;
    public static final int EVT_COVERART    = 0x0010;
    public static final int EVT_RGD_UPDATE  = 0x0020;
    public static final int EVT_DEVICE_STATE= 0x0030;
    public static final int EVT_SCREEN_INFO = 0x0040;   /* text: width:n:W\nheight:n:H */

    /* Command types (Java -> Hook) */
    public static final int CMD_SYNC_REQ    = 0x0100;
    public static final int CMD_PING        = 0x0101;
    public static final int CMD_CURSOR_POS  = 0x0200;
    public static final int CMD_CURSOR_HIDE = 0x0201;

    /* ============================================================
     * Listener callback
     * ============================================================ */
    public interface Listener {
        void onFrame(int type, int flags, byte[] payload, int len);
    }

    /* ============================================================
     * Text payload parser (PPS-style key:type:value)
     *
     * Identical to the legacy PPS.Data class so consumers can drop in
     * CarplayBus.parseText() wherever they previously used PPS.Data.
     * ============================================================ */
    public static class Data {
        private static final int MAX = 512;
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

        public String str(String key, String def) {
            for (int i = 0; i < count; i++) {
                if (key.equals(keys[i])) return vals[i];
            }
            return def;
        }

        public String str(String key) { return str(key, null); }

        public int num(String key, int def) {
            String v = str(key, null);
            if (v == null) return def;
            try { return Integer.parseInt(v.trim()); } catch (Exception e) { return def; }
        }

        public long num64(String key, long def) {
            String v = str(key, null);
            if (v == null) return def;
            try { return Long.parseLong(v.trim()); } catch (Exception e) { return def; }
        }

        public boolean bool(String key, boolean def) {
            String v = str(key, null);
            if (v == null) return def;
            v = v.trim();
            if ("true".equals(v) || "1".equals(v)) return true;
            if ("false".equals(v) || "0".equals(v)) return false;
            return def;
        }

        public int[] intList(String key) {
            String v = str(key, null);
            if (v == null || v.length() == 0) return null;
            int commas = 0;
            for (int i = 0; i < v.length(); i++) if (v.charAt(i) == ',') commas++;
            int[] out = new int[commas + 1];
            int start = 0, idx = 0;
            for (int i = 0; i <= v.length(); i++) {
                if (i == v.length() || v.charAt(i) == ',') {
                    try { out[idx++] = Integer.parseInt(v.substring(start, i).trim()); }
                    catch (Exception e) { out[idx++] = 0; }
                    start = i + 1;
                }
            }
            return out;
        }

        public String[] strList(String key, char delim) {
            String v = str(key, null);
            if (v == null || v.length() == 0) return null;
            int c = 1;
            for (int i = 0; i < v.length(); i++) if (v.charAt(i) == delim) c++;
            String[] out = new String[c];
            int start = 0, idx = 0;
            for (int i = 0; i <= v.length(); i++) {
                if (i == v.length() || v.charAt(i) == delim) {
                    out[idx++] = v.substring(start, i);
                    start = i + 1;
                }
            }
            return out;
        }

        public boolean has(String key) {
            for (int i = 0; i < count; i++) if (key.equals(keys[i])) return true;
            return false;
        }

        public int size() { return count; }
    }

    /** Parse a text payload (PPS-style key:type:value lines).  Discards @header lines. */
    public static Data parseText(byte[] buf, int len) {
        Data d = new Data();
        if (buf == null || len <= 0) return d;
        String content;
        try { content = new String(buf, 0, len, "UTF-8"); }
        catch (Exception e) { content = new String(buf, 0, len); }
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
            String val = (c2 >= 0 && c2 + 1 <= line.length()) ? line.substring(c2 + 1) : "";
            d.put(key, val);
        }
        return d;
    }

    /* ============================================================
     * Singleton
     * ============================================================ */
    private static final CarplayBus INSTANCE = new CarplayBus();
    public static CarplayBus getInstance() { return INSTANCE; }
    private CarplayBus() {}

    /* ============================================================
     * State
     * ============================================================ */
    private final Object lock = new Object();
    private volatile boolean running;
    private Thread ioThread;
    private Socket sock;
    private InputStream in;
    private OutputStream out;
    private int txSeq = 1;

    /* Listener table: direct-indexed by type.  Must cover the full
     * protocol range (see bus_protocol.h; currently through 0x02FF).
     * Out-of-range types are rejected by on()/dispatch to avoid silent
     * collisions. */
    private static final int MAX_TYPES = 0x0400;
    private final Listener[] listeners = new Listener[MAX_TYPES];

    /* Send queue (synchronized).  Frames are opaque byte arrays
     * already header-prefixed by enqueueing thread. */
    private byte[][] sendQueue = new byte[64][];
    private int sendHead = 0, sendTail = 0, sendCount = 0;

    /* ============================================================
     * Public API
     * ============================================================ */
    public void start() {
        synchronized (lock) {
            if (running) return;
            running = true;
            ioThread = new Thread(new Runnable() {
                public void run() { ioLoop(); }
            }, "CarplayBus-IO");
            ioThread.setDaemon(true);
            ioThread.start();
        }
        Log.i(TAG, "CarplayBus started");
    }

    public void stop() {
        synchronized (lock) {
            if (!running) return;
            running = false;
            closeConnectionLocked("stopping");
            lock.notifyAll();
        }
        if (ioThread != null) {
            try { ioThread.join(1000); } catch (InterruptedException e) {}
            ioThread = null;
        }
        Log.i(TAG, "CarplayBus stopped");
    }

    /**
     * Block up to {@code timeoutMs} until the outbound queue has drained.
     *
     * Necessary before a graceful stop() when the caller has just enqueued
     * teardown frames (e.g. CMD_CURSOR_HIDE) — stop() nukes sendQueue[]
     * inside closeConnectionLocked(), so any frame still sitting in the
     * queue when stop() runs is lost.  A short post-drain sleep lets the
     * writer's last socket.flush() settle the bytes into the kernel
     * buffer; 20 ms is ample for localhost TCP.
     *
     * Returns silently on timeout or interrupt.  Callers should treat
     * this as best-effort: pathological socket stalls cannot be
     * recovered here.
     */
    public void flush(long timeoutMs) {
        long deadline = System.currentTimeMillis() + timeoutMs;
        while (System.currentTimeMillis() < deadline) {
            int pending;
            synchronized (lock) { pending = sendCount; }
            if (pending == 0) {
                try { Thread.sleep(20); } catch (InterruptedException e) {}
                return;
            }
            try { Thread.sleep(10); }
            catch (InterruptedException e) { return; }
        }
        int leftover;
        synchronized (lock) { leftover = sendCount; }
        if (leftover > 0) {
            Log.w(TAG, "flush: timeout after " + timeoutMs + "ms, "
                     + leftover + " frame(s) still queued — will be discarded");
        }
    }

    public boolean isConnected() {
        synchronized (lock) {
            return sock != null && sock.isConnected() && !sock.isClosed();
        }
    }

    /** Register a listener for a given type. Overwrites previous. */
    public void on(int type, Listener listener) {
        if (type < 0 || type >= MAX_TYPES) {
            Log.w(TAG, "on: type 0x" + Integer.toHexString(type) + " out of range");
            return;
        }
        synchronized (lock) {
            listeners[type] = listener;
        }
    }

    public void off(int type) {
        if (type < 0 || type >= MAX_TYPES) return;
        synchronized (lock) {
            listeners[type] = null;
        }
    }

    /** Send a frame with binary payload. */
    public void send(int type, int flags, byte[] payload, int len) {
        if (len < 0 || len > MAX_PAYLOAD) {
            Log.w(TAG, "send: bad length " + len + " for type 0x" + Integer.toHexString(type));
            return;
        }
        byte[] frame = new byte[HEADER_SIZE + len];
        int seq;
        synchronized (lock) {
            seq = txSeq++;
        }
        putBE32(frame, 0, MAGIC);
        putBE32(frame, 4, seq);
        putBE16(frame, 8, type);
        frame[10] = (byte)(flags & 0xFF);
        frame[11] = 0;
        putBE32(frame, 12, len);
        if (len > 0 && payload != null) {
            System.arraycopy(payload, 0, frame, HEADER_SIZE, len);
        }
        enqueueFrame(frame);
    }

    /** Send a frame with UTF-8 text payload.  Always clears FLAG_BINARY. */
    public void sendText(int type, int flags, String text) {
        byte[] body;
        try {
            body = (text == null ? "" : text).getBytes("UTF-8");
        } catch (Exception e) {
            body = new byte[0];
        }
        send(type, flags & ~FLAG_BINARY, body, body.length);
    }

    /** Convenience: send an empty frame of this type. */
    public void sendBare(int type, int flags) {
        send(type, flags, null, 0);
    }

    /* ============================================================
     * Frame enqueue (lossy overflow: drop oldest)
     * ============================================================ */
    private void enqueueFrame(byte[] frame) {
        synchronized (lock) {
            if (sendCount == sendQueue.length) {
                /* grow up to 1024, then start dropping oldest */
                if (sendQueue.length < 1024) {
                    byte[][] bigger = new byte[sendQueue.length * 2][];
                    int i;
                    for (i = 0; i < sendCount; i++) {
                        bigger[i] = sendQueue[(sendHead + i) % sendQueue.length];
                    }
                    sendQueue = bigger;
                    sendHead = 0;
                    sendTail = sendCount;
                } else {
                    /* drop oldest */
                    sendHead = (sendHead + 1) % sendQueue.length;
                    sendCount--;
                    Log.w(TAG, "send queue overflow, dropped oldest");
                }
            }
            sendQueue[sendTail] = frame;
            sendTail = (sendTail + 1) % sendQueue.length;
            sendCount++;
            lock.notifyAll();
        }
    }

    private byte[] dequeueFrame() {
        synchronized (lock) {
            if (sendCount == 0) return null;
            byte[] frame = sendQueue[sendHead];
            sendQueue[sendHead] = null;
            sendHead = (sendHead + 1) % sendQueue.length;
            sendCount--;
            return frame;
        }
    }

    /* ============================================================
     * I/O thread main
     * ============================================================ */
    private void ioLoop() {
        int backoffMs = 200;
        while (running) {
            if (!connect()) {
                sleepMs(backoffMs);
                backoffMs = Math.min(backoffMs * 2, 2000);
                continue;
            }
            backoffMs = 200;

            /* CMD_SYNC_REQ already enqueued atomically inside connect() —
             * guaranteed to be the first frame on the wire. */

            /* Split I/O: reader blocks on in.read, writer drains queue.
             * Do both on this thread via ready checks: write if queue
             * non-empty, otherwise block on read.  Because reads block
             * on the socket but we may need to flush writes, use a
             * helper reader thread for the lifetime of the connection. */
            Thread readerThread = new Thread(new Runnable() {
                public void run() { readerLoop(); }
            }, "CarplayBus-Read");
            readerThread.setDaemon(true);
            readerThread.start();

            writerLoop();

            try { readerThread.join(500); } catch (InterruptedException e) {}
        }
    }

    private void readerLoop() {
        DataInputStream din;
        Socket owned;
        synchronized (lock) {
            if (in == null || sock == null) return;
            din = new DataInputStream(in);
            owned = sock;
        }
        byte[] hdr = new byte[HEADER_SIZE];
        while (running) {
            try {
                din.readFully(hdr);
                int magic = getBE32(hdr, 0);
                if (magic != MAGIC) {
                    Log.w(TAG, "bad magic 0x" + Integer.toHexString(magic));
                    break;
                }
                int seq  = getBE32(hdr, 4);
                int type = getBE16(hdr, 8);
                int flags = hdr[10] & 0xFF;
                int len = getBE32(hdr, 12);
                if (len < 0 || len > MAX_PAYLOAD) {
                    Log.w(TAG, "bad len " + len);
                    break;
                }
                byte[] payload = (len > 0) ? new byte[len] : new byte[0];
                if (len > 0) din.readFully(payload);
                dispatch(type, flags, payload, len, seq);
            } catch (IOException e) {
                if (running) Log.i(TAG, "reader IO closed: " + e.getMessage());
                break;
            } catch (Exception e) {
                Log.w(TAG, "reader error: " + e);
                break;
            }
        }
        /* Close ONLY if the current socket is still ours.  Prevents
         * a late-exiting reader from clobbering a fresh reconnect. */
        synchronized (lock) {
            if (sock == owned) closeConnectionLocked("reader exit");
            lock.notifyAll();
        }
    }

    private void writerLoop() {
        while (running) {
            byte[] frame = null;
            OutputStream o;
            synchronized (lock) {
                while (running && sendCount == 0 && sock != null && !sock.isClosed()) {
                    try { lock.wait(100); } catch (InterruptedException e) {}
                }
                if (!running) return;
                if (sock == null || sock.isClosed() || out == null) return;
                frame = dequeueFrame();
                o = out;  /* capture freshest reference under lock */
            }
            if (frame == null) continue;
            try {
                o.write(frame);
                o.flush();
            } catch (IOException e) {
                Log.i(TAG, "writer IO closed: " + e.getMessage());
                synchronized (lock) { closeConnectionLocked("writer exit"); }
                return;
            }
        }
    }

    /* ============================================================
     * Dispatch / connect helpers
     * ============================================================ */
    private void dispatch(int type, int flags, byte[] payload, int len, int seq) {
        if (type < 0 || type >= MAX_TYPES) {
            Log.w(TAG, "dispatch: type 0x" + Integer.toHexString(type) + " out of range");
            return;
        }
        Listener l;
        synchronized (lock) { l = listeners[type]; }
        if (l == null) {
            Log.d(TAG, "no listener for type 0x" + Integer.toHexString(type));
            return;
        }
        try {
            l.onFrame(type, flags, payload, len);
        } catch (Throwable t) {
            Log.w(TAG, "listener for type 0x" + Integer.toHexString(type) + " threw: " + t);
        }
    }

    private boolean connect() {
        Socket s = new Socket();
        try {
            s.connect(new InetSocketAddress(HOST, PORT), 1000);
            s.setTcpNoDelay(true);
            s.setKeepAlive(true);
            InputStream iin = s.getInputStream();
            OutputStream oout = s.getOutputStream();
            /* Atomic: drop anything that piled up while disconnected,
             * assign the new socket, and enqueue CMD_SYNC_REQ as the
             * very first outbound frame — so no concurrent send() from
             * CursorClient / other producer can slip ahead of it and
             * deliver stale state to the freshly-connected hook.
             * Java monitors are re-entrant, so sendBare() nested inside
             * runs under the same lock hold. */
            synchronized (lock) {
                closeConnectionLocked("reconnect");   /* flushes queue */
                sock = s;
                in = iin;
                out = oout;
                sendBare(CMD_SYNC_REQ, 0);            /* first frame guaranteed */
            }
            Log.i(TAG, "connected to " + HOST + ":" + PORT);
            return true;
        } catch (IOException e) {
            /* Silent — native hook bus may not be up yet during warm-up;
             * the reconnect loop retries every ~400 ms and drowns the log. */
            try { s.close(); } catch (Exception ex) {}
            return false;
        }
    }

    private void closeConnectionLocked(String reason) {
        if (sock != null) {
            try { sock.close(); } catch (Exception e) {}
            sock = null;
        }
        in = null;
        out = null;

        /* Discard any outbound frames that accumulated on the old
         * connection.  Cursor positions, acks, etc. are transient —
         * replaying them against a freshly-connected hook would leak
         * stale state.  CMD_SYNC_REQ is re-issued at the start of
         * every new connection by ioLoop(), so nothing is lost that
         * should survive a reconnect. */
        for (int i = 0; i < sendQueue.length; i++) sendQueue[i] = null;
        sendHead = 0;
        sendTail = 0;
        sendCount = 0;
    }

    /* ============================================================
     * Byte helpers
     * ============================================================ */
    private static void putBE16(byte[] buf, int off, int v) {
        buf[off]   = (byte)((v >> 8) & 0xFF);
        buf[off+1] = (byte)(v & 0xFF);
    }

    private static void putBE32(byte[] buf, int off, int v) {
        buf[off]   = (byte)((v >> 24) & 0xFF);
        buf[off+1] = (byte)((v >> 16) & 0xFF);
        buf[off+2] = (byte)((v >> 8) & 0xFF);
        buf[off+3] = (byte)(v & 0xFF);
    }

    private static int getBE16(byte[] buf, int off) {
        return ((buf[off] & 0xFF) << 8) | (buf[off+1] & 0xFF);
    }

    private static int getBE32(byte[] buf, int off) {
        return ((buf[off]   & 0xFF) << 24)
             | ((buf[off+1] & 0xFF) << 16)
             | ((buf[off+2] & 0xFF) << 8)
             | ((buf[off+3] & 0xFF));
    }

    private static void sleepMs(int ms) {
        try { Thread.sleep(ms); } catch (InterruptedException e) {}
    }

    /* ============================================================
     * Standalone echo test harness
     *
     *   java -cp carplay_hook.jar com.luka.carplay.framework.CarplayBus
     *
     * Connects to 127.0.0.1:19810, prints every inbound frame, sends a
     * CMD_PING every 2 seconds.  Meant for quick round-trip validation
     * of libcarplay_hook.so after deploy.
     * ============================================================ */
    public static void main(String[] args) throws Exception {
        final CarplayBus bus = CarplayBus.getInstance();

        Listener dump = new Listener() {
            public void onFrame(int type, int flags, byte[] payload, int len) {
                String kind = typeName(type);
                System.out.println("RX type=0x" + Integer.toHexString(type)
                        + " (" + kind + ") flags=" + flags + " len=" + len);
                if (len > 0 && (flags & FLAG_BINARY) == 0) {
                    try {
                        String s = new String(payload, 0, len, "UTF-8");
                        if (s.length() > 200) s = s.substring(0, 200) + "...";
                        System.out.println("   text: " + s.replace('\n', '|'));
                    } catch (Exception e) {}
                }
            }
        };

        int[] allTypes = {
            EVT_HELLO, EVT_SYNC_BEGIN, EVT_SYNC_END, EVT_PONG,
            EVT_COVERART, EVT_RGD_UPDATE, EVT_DEVICE_STATE, EVT_SCREEN_INFO
        };
        for (int i = 0; i < allTypes.length; i++) bus.on(allTypes[i], dump);

        bus.start();

        int n = 0;
        while (true) {
            Thread.sleep(2000);
            if (bus.isConnected()) {
                String p = "ping#" + (n++) + "@" + System.currentTimeMillis();
                bus.sendText(CMD_PING, 0, p);
                System.out.println("TX " + p);
            } else {
                System.out.println("(not connected)");
            }
        }
    }

    private static String typeName(int t) {
        switch (t) {
            case EVT_HELLO:        return "HELLO";
            case EVT_SYNC_BEGIN:   return "SYNC_BEGIN";
            case EVT_SYNC_END:     return "SYNC_END";
            case EVT_PONG:         return "PONG";
            case EVT_COVERART:     return "COVERART";
            case EVT_RGD_UPDATE:   return "RGD_UPDATE";
            case EVT_DEVICE_STATE: return "DEVICE_STATE";
            case EVT_SCREEN_INFO:  return "SCREEN_INFO";
            case CMD_SYNC_REQ:     return "SYNC_REQ";
            case CMD_PING:         return "PING";
            case CMD_CURSOR_POS:   return "CURSOR_POS";
            case CMD_CURSOR_HIDE:  return "CURSOR_HIDE";
            default:               return "?";
        }
    }
}
