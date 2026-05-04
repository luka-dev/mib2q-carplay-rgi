/*
 * RendererServer - non-blocking TCP server for the c_render process (port 19800).
 *
 * Topology: Java is the long-lived server, c_render is the short-lived
 * client.  We open the listen socket once and a dedicated background
 * thread accepts connections forever — caller (BAPBridge) never blocks.
 *
 * Lifecycle:
 *   - connect() opens the listen socket + starts accept thread.  Returns
 *     immediately whether the renderer is up or not.
 *   - Accept thread loops accept() forever, replacing the current socket
 *     when a new connection comes in.  If the renderer process is killed
 *     and respawned, it just reconnects and we pick up the new socket.
 *   - sendXxx() returns false if no current connection — caller (BAPBridge)
 *     uses that signal for crash-recovery counter (3 fails -> respawn).
 *   - disconnect() stops the accept thread + closes everything.
 *
 * No timing dependencies, no blocking accepts, no Thread.sleep hacks.
 *
 * Class name kept for compatibility with all call sites in BAPBridge /
 * RouteGuidance.
 *
 * Sends 48-byte cr_cmd_t packets matching c_render/protocol.h.
 * All methods are non-fatal (swallow exceptions, log only).
 *
 * Java 1.2 compatible (no generics, no autoboxing, no enhanced for).
 */
package com.luka.carplay.routeguidance;

import com.luka.carplay.framework.Log;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.InetSocketAddress;
import java.net.ServerSocket;
import java.net.Socket;

public class RendererServer {

    private static final String TAG = "RendererServer";
    private static final String HOST = "127.0.0.1";
    private static final int PORT = 19800;
    private static final int PKT_SIZE = 48;

    /* Read timeout on the accepted renderer socket.  Renderer sends
     * EVT_HEARTBEAT (cmd=0x80) every 1 s; if no inbound for 5 s,
     * read throws SocketTimeoutException and we drop the dead client.
     * Catches force-killed renderer whose TCP state lingers. */
    private static final int SOCKET_READ_TIMEOUT_MS = 5000;
    private static final byte EVT_HEARTBEAT   = (byte) 0x80;
    private static final byte EVT_READY       = (byte) 0x81;
    private static final byte EVT_FRAME_READY = (byte) 0x82;

    /* Command IDs -- must match protocol.h */
    private static final byte CMD_MANEUVER    = 0x01;
    private static final byte CMD_SHUTDOWN    = 0x03;
    private static final byte CMD_PERSPECTIVE = 0x04;
    private static final byte CMD_BARGRAPH    = 0x06;

    /* CMD_MANEUVER flags */
    private static final byte MAN_FLAG_SET_PERSP = 0x01;
    private static final byte MAN_FLAG_BARGRAPH  = 0x02;

    private final Object lock = new Object();
    private ServerSocket server;
    private Socket sock;             /* protected by lock */
    private OutputStream out;        /* protected by lock */
    private Thread acceptThread;
    private volatile boolean running;
    private volatile boolean rendererReady;
    private volatile boolean frameReady;

    /* True once we've successfully accepted at least one renderer
     * connection.  Lets the caller distinguish "still starting up,
     * sends are expected to fail" vs "had a working renderer that
     * just died, respawn is appropriate". */
    private volatile boolean everConnected;
    private volatile DeathListener deathListener;

    public interface DeathListener {
        void onRendererDied(String reason);
    }

    public void setDeathListener(DeathListener listener) {
        deathListener = listener;
    }

    /**
     * Open the listen socket and start the background accept thread.
     * Non-blocking — returns immediately whether the renderer is up
     * or not.  Method name kept for BAPBridge call-site compatibility.
     */
    public boolean connect() {
        synchronized (lock) {
            if (server != null && !server.isClosed()) return true;
            try {
                ServerSocket ss = new ServerSocket();
                ss.setReuseAddress(true);
                ss.bind(new InetSocketAddress(HOST, PORT));
                server = ss;
                running = true;
                Log.i(TAG, "listening on " + HOST + ":" + PORT);
            } catch (IOException e) {
                Log.w(TAG, "bind failed: " + e.getMessage());
                return false;
            }

            acceptThread = new Thread(new Runnable() {
                public void run() { acceptLoop(); }
            }, "RendererServer-Accept");
            acceptThread.setDaemon(true);
            acceptThread.start();
        }
        return true;
    }

    /**
     * Accept loop running on a dedicated thread.  Each accept replaces
     * any current socket, so renderer respawn -> new accept -> we
     * automatically pick up the new connection.
     */
    private void acceptLoop() {
        ServerSocket ss;
        synchronized (lock) {
            ss = server;
        }
        while (running && ss != null && !ss.isClosed()) {
            try {
                final Socket s = ss.accept();    /* blocks until renderer connects */
                s.setTcpNoDelay(true);
                /* SO_TIMEOUT on the read side — heartbeat dead-detection.
                 * Renderer sends EVT_HEARTBEAT every 1 s; 5 s silence =
                 * dead, reader thread gets SocketTimeoutException and
                 * drops the socket. */
                s.setSoTimeout(SOCKET_READ_TIMEOUT_MS);
                OutputStream o = s.getOutputStream();
                final InputStream i = s.getInputStream();

                synchronized (lock) {
                    /* Drop any stale socket from a previous renderer instance. */
                    if (sock != null) {
                        try { sock.close(); } catch (Exception e) {}
                    }
                    sock = s;
                    out = o;
                    everConnected = true;
                    rendererReady = false;
                    frameReady = false;
                    lock.notifyAll();
                }
                Log.i(TAG, "renderer connected from " + s.getInetAddress() + ":" + s.getPort());

                /* Spawn a per-connection reader.  It just drains inbound
                 * heartbeats (and discards their content) — the only
                 * point is to detect EOF / SO_TIMEOUT so we know the
                 * renderer died.  When it exits, we close the dead
                 * socket and the next accept() loops back to wait for
                 * the respawned renderer. */
                Thread reader = new Thread(new Runnable() {
                    public void run() { readerLoop(s, i); }
                }, "RendererServer-Read");
                reader.setDaemon(true);
                reader.start();
            } catch (IOException e) {
                if (running) {
                    Log.d(TAG, "accept ended: " + e.getMessage());
                }
                /* If the listen socket was closed (dispose()), exit. */
                synchronized (lock) {
                    if (server == null || server.isClosed()) break;
                }
            }
        }
        Log.i(TAG, "accept thread exiting");
    }

    /**
     * Per-connection reader.  Drains heartbeat frames, exits on
     * EOF / SO_TIMEOUT / any IO error.  On exit closes the
     * client socket if it's still ours.
     */
    private void readerLoop(Socket owned, InputStream is) {
        byte[] buf = new byte[PKT_SIZE];
        try {
            while (running) {
                int total = 0;
                while (total < PKT_SIZE) {
                    int n = is.read(buf, total, PKT_SIZE - total);
                    if (n < 0) throw new IOException("EOF");
                    total += n;
                }
                handleRendererEvent(owned, buf[0]);
            }
        } catch (IOException e) {
            if (running) {
                Log.i(TAG, "reader exit: " + e.getClass().getSimpleName()
                        + (e.getMessage() == null ? "" : " " + e.getMessage()));
                notifyDeath(e.getClass().getSimpleName()
                        + (e.getMessage() == null ? "" : " " + e.getMessage()));
            }
        } finally {
            synchronized (lock) {
                if (sock == owned) {
                    closeClientLocked();
                }
            }
        }
    }

    private void notifyDeath(String reason) {
        DeathListener l = deathListener;
        if (l == null) return;
        try {
            l.onRendererDied(reason);
        } catch (Throwable t) {
            Log.w(TAG, "death listener failed: " + t.getMessage());
        }
    }

    private void handleRendererEvent(Socket owned, byte event) {
        synchronized (lock) {
            if (sock != owned) return;
            if (event == EVT_READY) {
                rendererReady = true;
                lock.notifyAll();
                Log.i(TAG, "renderer ready");
            } else if (event == EVT_FRAME_READY) {
                rendererReady = true;
                frameReady = true;
                lock.notifyAll();
                Log.i(TAG, "renderer frame ready");
            } else if (event == EVT_HEARTBEAT) {
                /* Backward compatibility for renderer builds before EVT_READY:
                 * the first heartbeat is sent from the main loop after render init. */
                if (!rendererReady) {
                    rendererReady = true;
                    lock.notifyAll();
                    Log.i(TAG, "renderer ready via heartbeat");
                }
            }
        }
    }

    public boolean waitForReady(long timeoutMs) {
        return waitForState(timeoutMs, true);
    }

    public boolean waitForFrameReady(long timeoutMs) {
        return waitForState(timeoutMs, false);
    }

    private boolean waitForState(long timeoutMs, boolean wantReady) {
        long deadline = System.currentTimeMillis() + timeoutMs;
        synchronized (lock) {
            while (running) {
                if (wantReady && rendererReady) return true;
                if (!wantReady && frameReady) return true;

                long remain = deadline - System.currentTimeMillis();
                if (remain <= 0) break;
                try {
                    lock.wait(remain);
                } catch (InterruptedException e) {
                    Thread.currentThread().interrupt();
                    break;
                }
            }
            return wantReady ? rendererReady : frameReady;
        }
    }

    /**
     * Lightweight teardown: send CMD_SHUTDOWN to renderer, drop the
     * accepted client socket — but **keep the server listen socket
     * open** so the next renderer process can connect immediately.
     *
     * Call on route stop (between routes within one CarPlay session).
     */
    public void disconnectClient() {
        sendShutdown();
        synchronized (lock) {
            closeClientLocked();
            /* everConnected stays true — within this session we've already
             * had a working renderer; subsequent connect failures should
             * still trigger respawn correctly. */
        }
    }

    /**
     * Full teardown: close everything (client + server + accept thread).
     * Call on CarPlay deactivate / HMI shutdown.
     *
     * After dispose() the instance is unusable.  Call connect() again
     * (or build a new instance) to reopen.
     */
    public void dispose() {
        sendShutdown();
        synchronized (lock) {
            running = false;
            everConnected = false;
            closeClientLocked();
            if (server != null) {
                try { server.close(); } catch (Exception e) {}
                server = null;
            }
        }
        if (acceptThread != null) {
            try { acceptThread.join(500); } catch (InterruptedException e) {}
            acceptThread = null;
        }
    }

    /**
     * Backward-compat shim — old call sites used disconnect() which
     * did full teardown.  Default to that behaviour, but new code
     * should prefer disconnectClient() (per-route) or dispose() (final).
     */
    public void disconnect() {
        dispose();
    }

    private void closeClientLocked() {
        if (out != null) {
            try { out.close(); } catch (Exception e) {}
            out = null;
        }
        if (sock != null) {
            try { sock.close(); } catch (Exception e) {}
            sock = null;
        }
        rendererReady = false;
        frameReady = false;
        lock.notifyAll();
    }

    public boolean isConnected() {
        synchronized (lock) {
            return sock != null && !sock.isClosed() && out != null;
        }
    }

    /**
     * True once the renderer has successfully connected at least once
     * during the current connect()/disconnect() lifecycle.  Caller uses
     * this to distinguish "still starting up" (sends fail naturally)
     * from "had a connection that died" (respawn appropriate).
     */
    public boolean everConnected() {
        return everConnected;
    }

    /**
     * Send CMD_MANEUVER — push a new maneuver with transition.
     *
     * @param icon           ICON_* constant (0-7)
     * @param direction      -1=left, 0=center, +1=right
     * @param exitAngle      signed degrees
     * @param drivingSide    0=RHT, 1=LHT
     * @param junctionAngles signed degree array (may be null)
     * @param bargraphLevel  0-16 (0=empty, 16=full)
     * @param bargraphMode   0=off, 1=on, 2=blink
     * @param perspective    0=2D, 1=3D after transition (-1=don't change)
     */
    public boolean sendManeuver(int icon, int direction, int exitAngle,
                                int drivingSide, int[] junctionAngles,
                                int bargraphLevel, int bargraphMode,
                                int perspective) {
        byte[] pkt = new byte[PKT_SIZE];
        pkt[0] = CMD_MANEUVER;
        byte flags = 0;
        if (bargraphMode > 0)  flags |= MAN_FLAG_BARGRAPH;
        if (perspective >= 0)  flags |= MAN_FLAG_SET_PERSP;
        pkt[1] = flags;

        /* payload[0]: icon */
        pkt[2] = (byte) (icon & 0xFF);
        /* payload[1]: direction (signed) */
        pkt[3] = (byte) direction;
        /* payload[2..3]: exit_angle (big-endian i16) */
        pkt[4] = (byte) ((exitAngle >> 8) & 0xFF);
        pkt[5] = (byte) (exitAngle & 0xFF);
        /* payload[4]: driving_side */
        pkt[6] = (byte) (drivingSide & 0xFF);
        /* payload[5]: junction_count */
        int jCount = 0;
        if (junctionAngles != null) {
            jCount = junctionAngles.length;
            if (jCount > 18) jCount = 18;  /* max 18: payload[6..41], keeps [42..45] for persp/bargraph */
        }
        pkt[7] = (byte) (jCount & 0xFF);
        /* payload[6..45]: junction_angles (big-endian i16 each) */
        for (int i = 0; i < jCount; i++) {
            int off = 8 + i * 2;
            if (off + 1 >= PKT_SIZE) break;
            int a = junctionAngles[i];
            pkt[off]     = (byte) ((a >> 8) & 0xFF);
            pkt[off + 1] = (byte) (a & 0xFF);
        }

        /* perspective in payload[43] when flag set */
        if (perspective >= 0) {
            pkt[45] = (byte) (perspective & 0xFF);
        }
        /* bargraph in payload[44..45] when flag set */
        if (bargraphMode > 0) {
            pkt[46] = (byte) (bargraphLevel & 0xFF);
            pkt[47] = (byte) (bargraphMode & 0xFF);
        }

        return sendPacket(pkt);
    }

    /**
     * Send standalone CMD_BARGRAPH update.
     *
     * @param level 0-16
     * @param mode  0=off, 1=on, 2=blink
     * @return true if sent successfully
     */
    public boolean sendBargraph(int level, int mode) {
        byte[] pkt = new byte[PKT_SIZE];
        pkt[0] = CMD_BARGRAPH;
        pkt[2] = (byte) (level & 0xFF);   /* payload[0] = level */
        pkt[3] = (byte) (mode & 0xFF);    /* payload[1] = mode */
        return sendPacket(pkt);
    }

    /**
     * Send CMD_PERSPECTIVE to switch 3D/2D mode.
     *
     * @param enabled 0=flat 2D, 1=perspective 3D
     */
    public void sendPerspective(int enabled) {
        byte[] pkt = new byte[PKT_SIZE];
        pkt[0] = CMD_PERSPECTIVE;
        pkt[2] = (byte) (enabled & 0xFF);   /* payload[0] = on/off */
        if (sendPacket(pkt)) {
            Log.i(TAG, "Sent CMD_PERSPECTIVE=" + enabled);
        }
    }

    /**
     * Send CMD_SHUTDOWN for graceful renderer exit.
     */
    public void sendShutdown() {
        if (!isConnected()) return;
        byte[] pkt = new byte[PKT_SIZE];
        pkt[0] = CMD_SHUTDOWN;
        sendPacket(pkt);
        Log.i(TAG, "Sent CMD_SHUTDOWN");
    }

    /**
     * Non-blocking send.  Returns false if no current connection or
     * write failed — BAPBridge uses the bool for crash-recovery counter
     * (3 consecutive false returns trigger a renderer respawn).
     */
    private boolean sendPacket(byte[] pkt) {
        OutputStream o;
        synchronized (lock) {
            if (out == null) return false;
            o = out;
        }
        try {
            o.write(pkt);
            o.flush();
            return true;
        } catch (Exception e) {
            Log.w(TAG, "Send failed: " + e.getMessage());
            synchronized (lock) {
                /* Drop the dead socket; next renderer connect will replace it. */
                closeClientLocked();
            }
            return false;
        }
    }
}
