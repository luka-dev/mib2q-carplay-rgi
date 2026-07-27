/*
 * CarplayBus — bidirectional localhost link to the C hook (libcarplay_hook.so).
 *
 * Topology: Java is the long-lived TCP SERVER on 127.0.0.1:19810; the hook is the
 * client and (re)connects once per CarPlay session.  This half both RECEIVES hook
 * events (EVT_*) AND SENDS commands (CMD_*) — the old patch's send() was a stub;
 * here send() is a real writer (the altScreen CMD_ALT_* path needs it).
 *
 * Wire frame (matches c_hook/framework/bus.c, all multi-byte big-endian):
 *   [u32 MAGIC][u32 seq][u16 type][u8 flags][u8 reserved][u32 len][payload]
 *
 * Java 1.2 (MU1316 lsd): no generics/autoboxing/enhanced-for.
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
    public static final int EVT_DEVICE_STATE= 0x0030;

    /* commands Java -> hook (altScreen block, mirror bus_protocol.h) */
    public static final int CMD_ALT_HID        = 0x0110;   /* [i8 wheel][u8 buttons]        */
    public static final int CMD_ALT_ZONE       = 0x0111;   /* [u8 idx][u16 le durMs?]       */
    public static final int CMD_ALT_ZONE_ACK   = 0x0112;   /* hook->Java [u8 idx][i32 le st]*/
    public static final int CMD_ALT_APPEARANCE = 0x0114;   /* [u8 target][u8 mode][u8 set]  */
    public static final int CMD_ALT_UICTX      = 0x0115;   /* utf-8 url                     */

    public interface Listener {
        void onFrame(int type, int flags, byte[] payload, int len);
    }

    private static final CarplayBus INSTANCE = new CarplayBus();
    public static CarplayBus getInstance() { return INSTANCE; }
    private CarplayBus() {}

    private final Object lock = new Object();
    private final Object writeLock = new Object();
    private final Listener[] listeners = new Listener[MAX_TYPES];

    private volatile boolean running = false;
    private Thread ioThread;
    private ServerSocket serverSocket;
    private Socket sock;
    private InputStream in;
    private OutputStream out;
    private int txSeq = 1;

    /* ---- lifecycle ---- */
    public void start() {
        synchronized (lock) {
            if (running) return;
            running = true;
            ioThread = new Thread(new Runnable() { public void run() { ioLoop(); } }, "carplay-bus");
            ioThread.setDaemon(true);
            ioThread.start();
        }
        Log.i(TAG, "started (server " + HOST + ":" + PORT + ")");
    }

    public void stop() {
        synchronized (lock) {
            running = false;
            closeConnectionLocked("stop");
            if (serverSocket != null) { try { serverSocket.close(); } catch (IOException e) {} serverSocket = null; }
        }
        Thread t;
        synchronized (lock) { t = ioThread; ioThread = null; }
        if (t != null) { try { t.join(1000); } catch (InterruptedException e) {} }
    }

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
        if (len < 0 || len > MAX_PAYLOAD) return false;
        if (len > 0 && payload == null) return false;

        OutputStream os;
        int seq;
        synchronized (lock) {
            os = out;
            seq = txSeq++;
        }
        if (os == null) {
            Log.w(TAG, "send dropped (no connection) type=0x" + Integer.toHexString(type));
            return false;
        }

        byte[] hdr = new byte[HEADER_SIZE];
        putBE32(hdr, 0, MAGIC);
        putBE32(hdr, 4, seq);
        putBE16(hdr, 8, type);
        hdr[10] = (byte) flags;
        hdr[11] = 0;
        putBE32(hdr, 12, len);

        /* one frame at a time on the wire */
        synchronized (writeLock) {
            try {
                os.write(hdr, 0, HEADER_SIZE);
                if (len > 0) os.write(payload, 0, len);
                os.flush();
                return true;
            } catch (IOException e) {
                Log.w(TAG, "send failed type=0x" + Integer.toHexString(type) + ": " + e.getMessage());
                synchronized (lock) { if (out == os) closeConnectionLocked("send error"); }
                return false;
            }
        }
    }

    public boolean sendBinary(int type, byte[] payload) {
        return send(type, FLAG_BINARY, payload, payload == null ? 0 : payload.length);
    }

    /* ---- io loop ---- */
    private void ioLoop() {
        if (!openServerSocket()) { Log.e(TAG, "bind failed; bus disabled"); return; }
        while (running) {
            if (!acceptOne()) { if (running) sleep(200); continue; }
            readerLoop();
        }
    }

    private boolean openServerSocket() {
        try {
            ServerSocket ss = new ServerSocket();
            ss.setReuseAddress(true);
            ss.bind(new InetSocketAddress(InetAddress.getByName(HOST), PORT));
            synchronized (lock) { serverSocket = ss; }
            return true;
        } catch (IOException e) {
            Log.e(TAG, "bind " + HOST + ":" + PORT + " failed: " + e.getMessage());
            return false;
        }
    }

    private boolean acceptOne() {
        ServerSocket ss;
        synchronized (lock) { ss = serverSocket; }
        if (ss == null) return false;
        try {
            Socket s = ss.accept();
            s.setTcpNoDelay(true);
            InputStream iin = s.getInputStream();
            OutputStream oout = s.getOutputStream();
            synchronized (lock) {
                if (sock != null) closeConnectionLocked("re-accept");
                sock = s; in = iin; out = oout;
            }
            Log.i(TAG, "hook connected");
            return true;
        } catch (IOException e) {
            if (running) Log.w(TAG, "accept failed: " + e.getMessage());
            return false;
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
                if (getBE32(hdr, 0) != MAGIC) { Log.w(TAG, "bad magic"); break; }
                int seq   = getBE32(hdr, 4);
                int type  = getBE16(hdr, 8);
                int flags = hdr[10] & 0xFF;
                int len   = getBE32(hdr, 12);
                if (len < 0 || len > MAX_PAYLOAD) { Log.w(TAG, "bad len " + len); break; }
                byte[] payload = (len > 0) ? new byte[len] : new byte[0];
                if (len > 0) din.readFully(payload);
                dispatch(type, flags, payload, len);
            } catch (IOException e) {
                if (running) Log.i(TAG, "reader closed: " + e.getMessage());
                break;
            } catch (Throwable t) {
                Log.w(TAG, "reader error: " + t);
                break;
            }
        }
        synchronized (lock) { if (sock == owned) closeConnectionLocked("reader exit"); }
    }

    private void dispatch(int type, int flags, byte[] payload, int len) {
        if (type < 0 || type >= MAX_TYPES) { Log.w(TAG, "rx type 0x" + Integer.toHexString(type) + " oob"); return; }
        Listener l;
        synchronized (lock) { l = listeners[type]; }
        if (l == null) {
            if (type != EVT_HELLO && type != EVT_SYNC_BEGIN && type != EVT_SYNC_END)
                Log.d(TAG, "no listener for 0x" + Integer.toHexString(type));
            return;
        }
        try { l.onFrame(type, flags, payload, len); }
        catch (Throwable t) { Log.w(TAG, "listener 0x" + Integer.toHexString(type) + " threw: " + t); }
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
