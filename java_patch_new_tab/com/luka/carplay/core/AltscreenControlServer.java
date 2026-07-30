/*
 * AltscreenControlServer — always-on control server for altscreen_render.
 *
 * The H.264 DATA path is separate (libairplay hook -> altscreen_render :19831).  This is the
 * CONTROL path: altscreen_render connects here (client) and sends 4-byte events reporting whether
 * the CarPlay 111 cluster video is flowing:
 *   0x81 VIDEO_ON  -> AltScreenModule.setVideoAvailable(true)   (cluster can switch to ctx 82/83)
 *   0x82 VIDEO_OFF -> AltScreenModule.setVideoAvailable(false)  (fall back to 80/74)
 * altscreen disconnecting also counts as VIDEO_OFF.
 *
 * Started always-on from CarPlayApp.startTransport() (boot) — altscreen_render is a framework
 * service that comes up before CarPlay, so the server must already be listening.  Java 1.4.
 */
package com.luka.carplay.core;

import com.luka.carplay.framework.Log;

import java.io.InputStream;
import java.net.InetSocketAddress;
import java.net.ServerSocket;
import java.net.Socket;

final class AltscreenControlServer {

    private static final String TAG  = "AltCtl";
    private static final int    PORT = 19832;            /* must match altscreen protocol.h ALTCTL_PORT */
    private static final int    EVT_VIDEO_ON  = 0x81;
    private static final int    EVT_VIDEO_OFF = 0x82;

    private static final Object LOCK = new Object();
    private static Thread   thread;
    private static volatile boolean run;

    private AltscreenControlServer() {}

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
                    Socket c = ss.accept();
                    Log.i(TAG, "altscreen_render connected");
                    try { serve(c); }
                    catch (Throwable t) { Log.d(TAG, "client dropped: " + t); }
                    finally { try { c.close(); } catch (Throwable t) { /* ignore */ } }
                    /* altscreen gone → nothing streaming */
                    try { AltScreenModule.setVideoAvailable(false); } catch (Throwable t) { /* ignore */ }
                }
            } catch (Throwable t) {
                Log.w(TAG, "server error: " + t);
                try { Thread.sleep(1000); } catch (InterruptedException ie) { Thread.currentThread().interrupt(); }
            } finally {
                if (ss != null) { try { ss.close(); } catch (Throwable t) { /* ignore */ } }
            }
        }
    }

    private static void serve(Socket c) throws Exception {
        c.setTcpNoDelay(true);
        InputStream in = c.getInputStream();
        byte[] pkt = new byte[4];
        while (run) {
            int off = 0;
            while (off < 4) {
                int n = in.read(pkt, off, 4 - off);
                if (n < 0) return;                       /* EOF → altscreen gone */
                off += n;
            }
            int evt = pkt[0] & 0xFF;
            if (evt == EVT_VIDEO_ON) {
                Log.i(TAG, "VIDEO_ON");
                AltScreenModule.setVideoAvailable(true);
            } else if (evt == EVT_VIDEO_OFF) {
                Log.i(TAG, "VIDEO_OFF");
                AltScreenModule.setVideoAvailable(false);
            }
        }
    }
}
