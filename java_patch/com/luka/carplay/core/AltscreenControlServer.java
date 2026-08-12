/*
 * AltscreenControlServer — always-on control server for altscreen_render.
 *
 * The H.264 DATA path is separate (libairplay hook -> altscreen_render :19831).  This is the
 * CONTROL path: altscreen_render connects here (client) and sends 4-byte events reporting whether
 * the CarPlay 111 cluster video is flowing (0x81 VIDEO_ON / 0x82 VIDEO_OFF).
 *
 * VIDEO_ON is also a data-plane fallback for the cluster context.  TMDev can publish a transient
 * INVALID while dio restarts and fail to publish a matching activation afterwards even though the
 * new 111 stream is live.  In that case AltScreenModule must still expose displayable 99.
 *
 * Started always-on from CarPlayApp.startTransport() (boot) — altscreen_render is a framework
 * service that comes up before CarPlay, so the server must already be listening.  Java 1.4.
 */
package com.luka.carplay.core;

import com.luka.carplay.framework.Log;

import java.io.InputStream;
import java.io.OutputStream;
import java.net.InetSocketAddress;
import java.net.ServerSocket;
import java.net.Socket;

final class AltscreenControlServer {

    private static final String TAG  = "AltCtl";
    private static final int    PORT = 19832;            /* must match altscreen protocol.h ALTCTL_PORT */
    private static final int    EVT_VIDEO_ON  = 0x81;
    private static final int    EVT_VIDEO_OFF = 0x82;
    private static final int    CMD_VIEW_AREA = 0x01;

    private static final Object LOCK = new Object();
    private static Thread   thread;
    private static volatile boolean run;
    private static Socket client;
    private static int clientGeneration;
    private static int viewAreaMode;                /* 0=fullscreen, 1=smallscreen */

    private AltscreenControlServer() {}

    /** Cache and publish the current Audi View-button state. */
    static void setViewAreaMode(int mode) {
        synchronized (LOCK) {
            viewAreaMode = mode == 1 ? 1 : 0;
            sendViewAreaLocked("view-change");
        }
    }

    /* All writes use LOCK, so the initial state and a simultaneous View press
     * cannot interleave their fixed four-byte packets. */
    private static void sendViewAreaLocked(String reason) {
        if (client == null) return;
        try {
            byte[] pkt = new byte[] { (byte)CMD_VIEW_AREA, (byte)viewAreaMode, 0, 0 };
            OutputStream out = client.getOutputStream();
            out.write(pkt);
            out.flush();
            Log.i(TAG, "-> VIEW_AREA " + (viewAreaMode == 1 ? "smallscreen" : "fullscreen")
                + " reason=" + reason);
        } catch (Throwable t) {
            Log.d(TAG, "VIEW_AREA send failed: " + t);
            try { client.close(); } catch (Throwable ignored) { }
        }
    }

    static void start() {
        synchronized (LOCK) {
            if (run) return;
            run = true;
            thread = new Thread(new Runnable() { public void run() { loop(); } }, "carplay-altctl");
            thread.setDaemon(true);
            thread.start();
        }
    }

    private static void loop() {
        while (run) {
            ServerSocket ss = null;
            try {
                ss = new ServerSocket();
                ss.setReuseAddress(true);
                ss.bind(new InetSocketAddress("127.0.0.1", PORT));
                Log.i(TAG, "listening :" + PORT);
                while (run) {
                    final Socket c = ss.accept();
                    final int generation;
                    synchronized (LOCK) {
                        if (client != null) try { client.close(); } catch (Throwable t) { }
                        client = c;
                        generation = ++clientGeneration;
                        sendViewAreaLocked("renderer-connect");
                    }
                    Log.i(TAG, "altscreen_render connected generation=" + generation);
                    Thread reader = new Thread(new Runnable() {
                        public void run() { serveClient(c, generation); }
                    }, "carplay-altctl-reader");
                    reader.setDaemon(true);
                    reader.start();
                }
            } catch (Throwable t) {
                Log.w(TAG, "server error: " + t);
                try { Thread.sleep(1000); } catch (InterruptedException ie) { Thread.currentThread().interrupt(); }
            } finally {
                if (ss != null) { try { ss.close(); } catch (Throwable t) { /* ignore */ } }
            }
        }
    }

    private static void serveClient(Socket c, int generation) {
        try {
            c.setTcpNoDelay(true);
            c.setKeepAlive(true);
            InputStream in = c.getInputStream();
            byte[] pkt = new byte[4];
            while (run) {
                int off = 0;
                while (off < 4) {
                    int n = in.read(pkt, off, 4 - off);
                    if (n < 0) return;
                    off += n;
                }
                synchronized (LOCK) {
                    if (client != c || generation != clientGeneration) return;
                }
                int evt = pkt[0] & 0xFF;
                if (evt == EVT_VIDEO_ON) {
                    Log.i(TAG, "VIDEO_ON generation=" + generation);
                    AltScreenModule.setVideoAvailable(true);
                } else if (evt == EVT_VIDEO_OFF) {
                    Log.i(TAG, "VIDEO_OFF generation=" + generation);
                    AltScreenModule.setVideoAvailable(false);
                }
            }
        } catch (Throwable t) {
            if (run) Log.d(TAG, "client dropped: " + t);
        } finally {
            boolean owned = false;
            synchronized (LOCK) {
                if (client == c && generation == clientGeneration) {
                    client = null;
                    owned = true;
                }
            }
            try { c.close(); } catch (Throwable t) { }
            if (owned) {
                AltScreenModule.setVideoAvailable(false);
                Log.i(TAG, "altscreen_render disconnected generation=" + generation);
            }
        }
    }
}
