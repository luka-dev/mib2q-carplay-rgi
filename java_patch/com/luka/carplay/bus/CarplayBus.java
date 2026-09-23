/*
 * CarplayBus — bidirectional localhost link to the C hook (libcarplay_hook.so).
 *
 * Topology: Java is the long-lived TCP SERVER on 127.0.0.1:19810; the hook is the
 * client and (re)connects once per CarPlay session.  This half both RECEIVES hook
 * events (EVT_*) and SENDS commands (CMD_*, e.g. the altScreen CMD_ALT_* path).
 *
 * Wire frame (matches hook/framework/bus.c, all multi-byte big-endian):
 *   [u32 MAGIC][u32 seq][u16 type][u8 flags][u8 reserved][u32 len][payload]
 *
 * Java 1.4 / Foundation 1.1: no generics/autoboxing/enhanced-for.
 *
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */
package com.luka.carplay.bus;

import com.luka.carplay.framework.Log;

import java.io.DataInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.InetAddress;
import java.net.InetSocketAddress;
import java.net.ServerSocket;
import java.net.Socket;

public final class CarplayBus {

    private static final String TAG = "Bus";

    public static final String HOST = "127.0.0.1";
    public static final int    PORT = 19810;
    public static final int    MAGIC = 0x43504842;   /* "CPHB" */
    public static final int    HEADER_SIZE = 16;
    public static final int    MAX_PAYLOAD = 128 * 1024;
    public static final int    MAX_TYPES = 0x0120;

    /* flags */
    public static final int FLAG_STICKY = 0x01;
    public static final int FLAG_BINARY = 0x02;
    public static final int FLAG_REPLAY = 0x04;

    /* events hook -> Java */
    public static final int EVT_HELLO       = 0x0001;
    public static final int EVT_SYNC_BEGIN  = 0x0002;
    public static final int EVT_SYNC_END    = 0x0003;
    public static final int EVT_COVERART    = 0x0010;
    public static final int EVT_RGD_UPDATE  = 0x0020;

    /* commands Java -> hook (mirror bus_protocol.h) */
    public static final int CMD_SYNC_REQ       = 0x0100;   /* replay all sticky state       */
    public static final int CMD_ALT_ZOOM       = 0x0110;   /* [i8 signed MapScale step]     */
    public static final int CMD_ALT_ZONE       = 0x0111;   /* [u8 0=full/1=sport/2=classic][u16 LE durationMs] */
    public static final int CMD_ALT_ZONE_ACK   = 0x0112;   /* hook->Java [u8 mode][i32 LE status]       */
    public static final int CMD_ALT_RGI        = 0x0116;   /* [u8 0=off 1=on] confirmed RGI presentation */

    public interface Listener {
        void onFrame(int type, int flags, byte[] payload, int len);
    }

    private static final CarplayBus INSTANCE = new CarplayBus();
    public static CarplayBus getInstance() { return INSTANCE; }
    private CarplayBus() { this(PORT); }
    /* Package-local endpoint override keeps host lifecycle tests off the HU port. */
    CarplayBus(int port) { this.port = port; }
    private final int port;

    private final Object lock = new Object();
    private final Object writeLock = new Object();
    /* Serialize callbacks across reader incarnations without blocking accept
     * or holding the connection lock while calling a module. */
    private final Object dispatchLock = new Object();
    private final Listener[] listeners = new Listener[MAX_TYPES];
    private static final int WRITE_QUEUE_CAPACITY = 32;

    private volatile boolean running = false;
    private volatile int lifecycleGeneration;
    private Thread ioThread;
    private Thread writerThread;
    private ServerSocket serverSocket;
    private Socket sock;
    private InputStream in;
    private OutputStream out;
    private int txSeq = 1;
    private int connectionGeneration;

    private static final class PendingWrite {
        byte[] packet;
        int generation;
        int type;
    }
    private final PendingWrite[] writeQueue = new PendingWrite[WRITE_QUEUE_CAPACITY];
    private int writeHead;
    private int writeTail;
    private int writeCount;

    /* ---- lifecycle ---- */
    public void start() {
        synchronized (lock) {
            if (running) return;
            final int lifecycle = ++lifecycleGeneration;
            running = true;
            ioThread = new Thread(new Runnable() { public void run() { ioLoop(lifecycle); } }, "carplay-bus");
            ioThread.setDaemon(true);
            ioThread.start();
        }
        Log.i(TAG, "started (server " + HOST + ":" + port + ")");
    }

    public void stop() {
        Thread t, r, w;
        synchronized (lock) {
            running = false;
            ++lifecycleGeneration;
            closeConnectionLocked("stop");
            if (serverSocket != null) { try { serverSocket.close(); } catch (IOException e) {} serverSocket = null; }
            lock.notifyAll();                 /* wake the reader thread out of its wait() */
            synchronized (writeLock) { clearWriteQueueLocked(); writeLock.notifyAll(); }
            t = ioThread; ioThread = null;
            r = readerThread; readerThread = null;
            w = writerThread; writerThread = null;
        }
        joinOther(t); joinOther(r); joinOther(w);
    }

    private static void joinOther(Thread t) {
        if (t != null && t != Thread.currentThread()) {
            try { t.join(1000); } catch (InterruptedException e) { Thread.currentThread().interrupt(); }
        }
    }

    private boolean isCurrentRun(int lifecycle) {
        return running && lifecycleGeneration == lifecycle;
    }

    private Thread readerThread;   /* dedicated reader — decoupled from accept (see ioLoop) */

    public boolean isConnected() {
        synchronized (lock) { return sock != null; }
    }

    /* ---- listeners ---- */
    public void on(int type, Listener l) {
        if (type < 0 || type >= MAX_TYPES) return;
        synchronized (lock) { listeners[type] = l; }
    }
    public void off(int type) {
        if (type < 0 || type >= MAX_TYPES) return;
        synchronized (lock) { listeners[type] = null; }
    }

    /* ---- writer (Java -> hook) ---- */
    public boolean send(int type, int flags, byte[] payload, int len) {
        if (type < 0 || type >= MAX_TYPES || flags < 0 || flags > 255) return false;
        if (len < 0 || len > MAX_PAYLOAD) return false;
        if (len > 0 && (payload == null || len > payload.length)) return false;

        int generation;
        synchronized (lock) {
            if (!running || out == null) {
                Log.w(TAG, "send dropped (no connection) type=0x" + Integer.toHexString(type));
                return false;
            }
            generation = connectionGeneration;
        }

        byte[] packet = new byte[HEADER_SIZE + len];
        putBE32(packet, 0, MAGIC);
        putBE16(packet, 8, type);
        packet[10] = (byte) flags;
        packet[11] = 0;
        putBE32(packet, 12, len);
        if (len > 0) System.arraycopy(payload, 0, packet, HEADER_SIZE, len);

        /* Never perform a socket write on a stock HMI/BAP callback.  The sole
         * writer owns blocking I/O; connection generations discard commands
         * queued for a preempted hook instance. */
        synchronized (lock) {
            if (!running || out == null || generation != connectionGeneration) return false;
            synchronized (writeLock) {
                PendingWrite pending = new PendingWrite();
                putBE32(packet, 4, txSeq++);
                if (writeCount == WRITE_QUEUE_CAPACITY) {
                    PendingWrite dropped = writeQueue[writeHead];
                    writeQueue[writeHead] = null;
                    writeHead = (writeHead + 1) % WRITE_QUEUE_CAPACITY;
                    writeCount--;
                    Log.w(TAG, "writer queue full; dropped oldest type=0x"
                        + Integer.toHexString(dropped == null ? -1 : dropped.type));
                }
                pending.packet = packet;
                pending.generation = generation;
                pending.type = type;
                writeQueue[writeTail] = pending;
                writeTail = (writeTail + 1) % WRITE_QUEUE_CAPACITY;
                writeCount++;
                writeLock.notifyAll();
                return true;
            }
        }
    }

    private void writerLoop(int lifecycle) {
        while (true) {
            PendingWrite pending;
            synchronized (writeLock) {
                while (isCurrentRun(lifecycle) && writeCount == 0) {
                    try { writeLock.wait(); } catch (InterruptedException e) { }
                }
                if (!isCurrentRun(lifecycle)) return;
                pending = writeQueue[writeHead];
                writeQueue[writeHead] = null;
                writeHead = (writeHead + 1) % WRITE_QUEUE_CAPACITY;
                writeCount--;
            }
            OutputStream ownedOut;
            synchronized (lock) {
                if (!isCurrentRun(lifecycle) || pending == null ||
                    pending.generation != connectionGeneration || out == null)
                    continue;
                ownedOut = out;
            }
            try {
                ownedOut.write(pending.packet);
                ownedOut.flush();
            } catch (IOException e) {
                Log.w(TAG, "writer failed type=0x" + Integer.toHexString(pending.type)
                    + ": " + e.getMessage());
                synchronized (lock) {
                    if (out == ownedOut) closeConnectionLocked("writer error");
                }
            }
        }
    }

    private void clearWriteQueueLocked() {
        for (int i = 0; i < WRITE_QUEUE_CAPACITY; i++) writeQueue[i] = null;
        writeHead = writeTail = writeCount = 0;
    }

    private void clearWriteQueue() {
        synchronized (writeLock) { clearWriteQueueLocked(); }
    }

    public boolean sendBinary(int type, byte[] payload) {
        return send(type, FLAG_BINARY, payload, payload == null ? 0 : payload.length);
    }

    /* ---- text payload parser (hook -> Java "key:type:val\n" frames) ---- */
    public static class Data {
        private static final int MAX = 1024;
        private String[] keys = new String[MAX];
        private String[] vals = new String[MAX];
        private int count = 0;
        private boolean overflow;

        void put(String key, String value) {
            if (count < MAX) { keys[count] = key; vals[count] = value; count++; }
            else overflow = true;
        }

        public String str(String key, String def) {
            for (int i = 0; i < count; i++) if (key.equals(keys[i])) return vals[i];
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
                    int parsed;
                    try { parsed = Integer.parseInt(v.substring(start, i).trim()); }
                    catch (NumberFormatException e) { parsed = 0; }
                    out[idx++] = parsed;
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
        public boolean overflowed() { return overflow; }
    }

    public static Data parseText(byte[] buf, int len) {
        Data d = new Data();
        if (buf == null || len <= 0 || len > buf.length) return d;
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
        if (d.overflowed()) Log.w(TAG, "text frame truncated at " + Data.MAX + " keys");
        return d;
    }

    /* ---- io loop ----
     * ioThread does ACCEPT ONLY.  A dedicated reader thread (started here) owns readFully(), so a
     * blocked read on a dead / half-open peer can NEVER stop us accepting the next (live) connection:
     * the new connect preempts the stale one (acceptOne closes the old socket → the reader's readFully
     * throws → it switches to the new socket).  This makes the bus un-wedgeable no matter how many
     * hook instances connect and die — no reboot needed to recover a stuck connection. */
    private void ioLoop(final int lifecycle) {
        if (!openServerSocket(lifecycle)) {
            Log.e(TAG, "bind failed; bus disabled");
            synchronized (lock) {
                if (lifecycleGeneration == lifecycle) running = false;
            }
            return;
        }
        synchronized (lock) {
            if (!isCurrentRun(lifecycle)) return;
            writerThread = new Thread(new Runnable() { public void run() { writerLoop(lifecycle); } }, "carplay-bus-writer");
            writerThread.setDaemon(true);
            writerThread.start();
            readerThread = new Thread(new Runnable() { public void run() { readerLoop(lifecycle); } }, "carplay-bus-reader");
            readerThread.setDaemon(true);
            readerThread.start();
        }
        while (isCurrentRun(lifecycle)) {
            if (!acceptOne(lifecycle)) { if (isCurrentRun(lifecycle)) sleep(200); }
        }
    }

    private boolean openServerSocket(int lifecycle) {
        /* Bind and publication share the lifecycle lock: an obsolete binder
         * must not briefly occupy the port and make the replacement fail. */
        synchronized (lock) {
            if (!isCurrentRun(lifecycle)) return false;
            ServerSocket ss = null;
            try {
                ss = new ServerSocket();
                ss.setReuseAddress(true);
                ss.bind(new InetSocketAddress(InetAddress.getByName(HOST), port));
                serverSocket = ss;
                return true;
            } catch (IOException e) {
                if (ss != null) try { ss.close(); } catch (IOException closeError) { }
                Log.e(TAG, "bind " + HOST + ":" + port + " failed: " + e.getMessage());
                return false;
            }
        }
    }

    private boolean acceptOne(int lifecycle) {
        ServerSocket ss;
        synchronized (lock) { ss = isCurrentRun(lifecycle) ? serverSocket : null; }
        if (ss == null) return false;
        Socket s = null;
        try {
            s = ss.accept();
            s.setTcpNoDelay(true);
            /* KEEPALIVE — the proven-working old impl set this and it regressed out (defence in depth;
             * the preempt below is the primary un-wedge mechanism). */
            s.setKeepAlive(true);
            InputStream iin = s.getInputStream();
            OutputStream oout = s.getOutputStream();
            synchronized (lock) {
                if (!isCurrentRun(lifecycle) || serverSocket != ss) {
                    s.close(); return false;
                }
                /* A NEW connection PREEMPTS the current one: closing the old socket makes the reader's
                 * blocked readFully() throw → it drops the stale/dead peer and picks up this live one.
                 * Multiple hook instances or a half-open peer can therefore never wedge the bus. */
                if (sock != null) closeConnectionLocked("preempted by new connection");
                sock = s; in = iin; out = oout;
                connectionGeneration++;
                clearWriteQueue();
                lock.notifyAll();               /* wake the reader onto the new socket */
            }
            Log.i(TAG, "hook connected");
            return true;
        } catch (IOException e) {
            if (s != null) try { s.close(); } catch (IOException closeError) { }
            if (isCurrentRun(lifecycle)) Log.w(TAG, "accept failed: " + e.getMessage());
            return false;
        }
    }

    /* Own thread: read the CURRENT socket; on break/preempt, wait for acceptOne to install the next. */
    private void readerLoop(int lifecycle) {
        byte[] hdr = new byte[HEADER_SIZE];
        while (isCurrentRun(lifecycle)) {
            DataInputStream din;
            Socket owned;
            synchronized (lock) {
                while (isCurrentRun(lifecycle) && (in == null || sock == null)) {
                    try { lock.wait(); } catch (InterruptedException e) { /* re-check predicate */ }
                }
                if (!isCurrentRun(lifecycle)) return;
                din = new DataInputStream(in);
                owned = sock;
            }
            while (isCurrentRun(lifecycle)) {
                try {
                    din.readFully(hdr);
                    if (getBE32(hdr, 0) != MAGIC) { Log.w(TAG, "bad magic"); break; }
                    int type  = getBE16(hdr, 8);
                    int flags = hdr[10] & 0xFF;
                    int len   = getBE32(hdr, 12);
                    if (len < 0 || len > MAX_PAYLOAD) { Log.w(TAG, "bad len " + len); break; }
                    byte[] payload = (len > 0) ? new byte[len] : new byte[0];
                    if (len > 0) din.readFully(payload);
                    if (!dispatch(lifecycle, owned, type, flags, payload, len)) break;
                } catch (IOException e) {
                    if (running) Log.i(TAG, "reader closed: " + e.getMessage());
                    break;
                } catch (Throwable t) {
                    Log.w(TAG, "reader error: " + t);
                    break;
                }
            }
            /* Only tear down if we still own the socket — if acceptOne preempted us, `sock` is already
             * the new one and must stay open for the next iteration. */
            synchronized (lock) { if (sock == owned) closeConnectionLocked("reader exit"); }
        }
    }

    private boolean dispatch(int lifecycle, Socket owned, int type, int flags, byte[] payload, int len) {
        synchronized (dispatchLock) {
            Listener l;
            synchronized (lock) {
                if (!isCurrentRun(lifecycle) || sock != owned) return false;
                if (type < 0 || type >= MAX_TYPES) return true;
                l = listeners[type];
            }
            if (l != null) {
                try { l.onFrame(type, flags, payload, len); }
                catch (Throwable t) { Log.w(TAG, "listener 0x" + Integer.toHexString(type) + " threw: " + t); }
            }
            return true;
        }
    }

    /* caller holds lock */
    private void closeConnectionLocked(String why) {
        if (sock != null) Log.i(TAG, "closing connection (" + why + ")");
        if (in   != null) { try { in.close();   } catch (IOException e) {} in = null; }
        if (out  != null) { try { out.close();  } catch (IOException e) {} out = null; }
        if (sock != null) { try { sock.close(); } catch (IOException e) {} sock = null; }
    }

    /* ---- byte helpers ---- */
    private static void putBE32(byte[] b, int o, int v) {
        b[o] = (byte)(v >>> 24); b[o+1] = (byte)(v >>> 16); b[o+2] = (byte)(v >>> 8); b[o+3] = (byte) v;
    }
    private static void putBE16(byte[] b, int o, int v) {
        b[o] = (byte)(v >>> 8); b[o+1] = (byte) v;
    }
    private static int getBE32(byte[] b, int o) {
        return ((b[o] & 0xFF) << 24) | ((b[o+1] & 0xFF) << 16) | ((b[o+2] & 0xFF) << 8) | (b[o+3] & 0xFF);
    }
    private static int getBE16(byte[] b, int o) {
        return ((b[o] & 0xFF) << 8) | (b[o+1] & 0xFF);
    }
    private static void sleep(long ms) { try { Thread.sleep(ms); } catch (InterruptedException e) {} }
}
