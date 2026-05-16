/*
 * CarPlay Hook Bus - Java server
 *
 * One-way TCP event stream from libcarplay_hook.so to Java.
 * Java is the long-lived server on 127.0.0.1:19810; the hook is the
 * short-lived client inside dio_manager.  Touchpad input stays entirely
 * in the Java/DSI path and does not use this socket.
 *
 * Java 1.2 compatible: no generics, no lambdas, no try-with-resources.
 */

package com.luka.carplay.framework;

import java.io.DataInputStream;
import java.io.InputStream;
import java.io.IOException;
import java.net.InetSocketAddress;
import java.net.ServerSocket;
import java.net.Socket;

public class CarplayBus {

    private static final String TAG = "CarplayBus";

    public static final String HOST = "127.0.0.1";
    public static final int    PORT = 19810;
    public static final int    MAGIC = 0x43504842;
    public static final int    HEADER_SIZE = 16;
    public static final int    MAX_PAYLOAD = 128 * 1024;

    public static final int FLAG_STICKY  = 0x01;
    public static final int FLAG_BINARY  = 0x02;
    public static final int FLAG_REPLAY  = 0x04;

    public static final int EVT_HELLO       = 0x0001;
    public static final int EVT_SYNC_BEGIN  = 0x0002;
    public static final int EVT_SYNC_END    = 0x0003;
    public static final int EVT_PONG        = 0x0004;  /* legacy/debug only */
    public static final int EVT_COVERART    = 0x0010;
    public static final int EVT_RGD_UPDATE  = 0x0020;
    public static final int EVT_DEVICE_STATE= 0x0030;

    /* Legacy constants kept so old debug callers still compile. */
    public static final int CMD_SYNC_REQ    = 0x0100;
    public static final int CMD_PING        = 0x0101;

    public interface Listener {
        void onFrame(int type, int flags, byte[] payload, int len);
    }

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

    private static final CarplayBus INSTANCE = new CarplayBus();
    public static CarplayBus getInstance() { return INSTANCE; }
    private CarplayBus() {}

    private final Object lock = new Object();
    private volatile boolean running;
    private Thread ioThread;
    private ServerSocket serverSocket;
    private Socket sock;
    private InputStream in;

    private static final int MAX_TYPES = 0x0400;
    private final Listener[] listeners = new Listener[MAX_TYPES];

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
            if (serverSocket != null) {
                try { serverSocket.close(); } catch (Exception e) {}
            }
            lock.notifyAll();
        }
        if (ioThread != null) {
            try { ioThread.join(1000); } catch (InterruptedException e) {}
            ioThread = null;
        }
        Log.i(TAG, "CarplayBus stopped");
    }

    public void flush(long timeoutMs) {
        /* One-way bus: Java has no outbound queue. */
    }

    public boolean isConnected() {
        synchronized (lock) {
            return sock != null && sock.isConnected() && !sock.isClosed();
        }
    }

    public void on(int type, Listener listener) {
        if (type < 0 || type >= MAX_TYPES) {
            Log.w(TAG, "on: type 0x" + Integer.toHexString(type) + " out of range");
            return;
        }
        synchronized (lock) { listeners[type] = listener; }
    }

    public void off(int type) {
        if (type < 0 || type >= MAX_TYPES) return;
        synchronized (lock) { listeners[type] = null; }
    }

    public void send(int type, int flags, byte[] payload, int len) {
        Log.w(TAG, "send ignored on one-way bus type=0x" + Integer.toHexString(type));
    }

    public void sendText(int type, int flags, String text) {
        send(type, flags & ~FLAG_BINARY, null, 0);
    }

    public void sendBare(int type, int flags) {
        send(type, flags, null, 0);
    }

    private void ioLoop() {
        if (!openServerSocket()) {
            Log.e(TAG, "ServerSocket bind failed; bus disabled");
            return;
        }

        while (running) {
            if (!acceptOne()) {
                sleepMs(500);
                continue;
            }
            readerLoop();
        }

        try { if (serverSocket != null) serverSocket.close(); }
        catch (Exception e) {}
        serverSocket = null;
    }

    private boolean openServerSocket() {
        try {
            ServerSocket ss = new ServerSocket();
            ss.setReuseAddress(true);
            ss.bind(new InetSocketAddress(HOST, PORT));
            synchronized (lock) { serverSocket = ss; }
            Log.i(TAG, "listening on " + HOST + ":" + PORT);
            return true;
        } catch (IOException e) {
            Log.e(TAG, "ServerSocket bind " + HOST + ":" + PORT + " failed: " + e.getMessage());
            return false;
        }
    }

    private boolean acceptOne() {
        ServerSocket ss;
        synchronized (lock) {
            ss = serverSocket;
            if (ss == null) return false;
        }
        try {
            Socket s = ss.accept();
            s.setTcpNoDelay(true);
            s.setKeepAlive(true);
            InputStream iin = s.getInputStream();

            synchronized (lock) {
                if (sock != null) {
                    Log.w(TAG, "stale socket present at accept time - cleaning up before re-init");
                }
                closeConnectionLocked("re-accept");
                sock = s;
                in = iin;
            }
            Log.i(TAG, "hook connected from " + s.getInetAddress() + ":" + s.getPort());
            return true;
        } catch (IOException e) {
            if (running) Log.w(TAG, "accept() failed: " + e.getMessage());
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
                if (running) {
                    String msg = e.getMessage();
                    Log.i(TAG, "reader IO closed: "
                            + e.getClass().getName()
                            + (msg == null ? "" : ": " + msg));
                }
                break;
            } catch (Throwable t) {
                Log.w(TAG, "reader error: " + t);
                break;
            }
        }

        synchronized (lock) {
            if (sock == owned) closeConnectionLocked("reader exit");
            lock.notifyAll();
        }
    }

    private void dispatch(int type, int flags, byte[] payload, int len, int seq) {
        if (type < 0 || type >= MAX_TYPES) {
            Log.w(TAG, "dispatch: type 0x" + Integer.toHexString(type) + " out of range");
            return;
        }
        Listener l;
        synchronized (lock) { l = listeners[type]; }
        if (l == null) {
            if (type == EVT_PONG || type == EVT_HELLO
                || type == EVT_SYNC_BEGIN || type == EVT_SYNC_END) return;
            Log.d(TAG, "no listener for type 0x" + Integer.toHexString(type));
            return;
        }
        try {
            l.onFrame(type, flags, payload, len);
        } catch (Throwable t) {
            Log.w(TAG, "listener for type 0x" + Integer.toHexString(type) + " threw: " + t);
        }
    }

    private void closeConnectionLocked(String reason) {
        if (sock != null) {
            try { sock.close(); } catch (Exception e) {}
            sock = null;
        }
        in = null;
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

    public static void main(String[] args) throws Exception {
        final CarplayBus bus = CarplayBus.getInstance();
        Listener dump = new Listener() {
            public void onFrame(int type, int flags, byte[] payload, int len) {
                System.out.println("RX type=0x" + Integer.toHexString(type)
                        + " (" + typeName(type) + ") flags=" + flags + " len=" + len);
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
            EVT_COVERART, EVT_RGD_UPDATE, EVT_DEVICE_STATE
        };
        for (int i = 0; i < allTypes.length; i++) bus.on(allTypes[i], dump);
        bus.start();
        while (true) {
            Thread.sleep(2000);
            System.out.println(bus.isConnected() ? "(connected)" : "(not connected)");
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
            case CMD_SYNC_REQ:     return "SYNC_REQ";
            case CMD_PING:         return "PING";
            default:               return "?";
        }
    }
}
