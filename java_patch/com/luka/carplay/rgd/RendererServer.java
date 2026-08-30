/*
 * RendererServer - non-blocking TCP server for the maneuver_render process (port 19800).
 *
 * Topology: Java is the long-lived server; the framework-owned maneuver_render
 * connects as a restartable client. We open the listen socket once and a dedicated background
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
 * Sends 48-byte cr_cmd_t packets matching maneuver_render/protocol.h.
 * All methods are non-fatal (swallow exceptions, log only).
 *
 * Java 1.2 compatible (no generics, no autoboxing, no enhanced for).
 */
package com.luka.carplay.rgd;

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
    private static final int WRITE_QUEUE_CAPACITY = 32;

    /* Read timeout on the accepted renderer socket.  Renderer sends
     * EVT_HEARTBEAT (cmd=0x80) every 1 s; if no inbound for 5 s,
     * read throws SocketTimeoutException and we drop the dead client.
     * Catches force-killed renderer whose TCP state lingers. */
    private static final int SOCKET_READ_TIMEOUT_MS = 5000;
    private static final byte EVT_HEARTBEAT   = (byte) 0x80;
    private static final byte EVT_READY       = (byte) 0x81;
    private static final byte EVT_FRAME_READY = (byte) 0x82;
    private static final byte EVT_FRAME_CLEARED = (byte) 0x83;

    /* Command IDs used by this always-on client protocol. */
    private static final byte CMD_MANEUVER    = 0x01;
    private static final byte CMD_BARGRAPH    = 0x06;
    private static final byte CMD_CLEAR       = 0x07;

    /* CMD_MANEUVER flags */
    private static final byte MAN_FLAG_SET_PERSP = 0x01;
    private static final byte MAN_FLAG_BARGRAPH  = 0x02;

    private final Object lock = new Object();
    private final Object writeLock = new Object();
    private ServerSocket server;
    private Socket sock;             /* protected by lock */
    private OutputStream out;        /* protected by lock */
    private Thread acceptThread;
    private Thread writerThread;
    private volatile boolean running;
    private volatile boolean rendererReady;
    private volatile boolean frameReady;
    /* True from enqueueing CMD_CLEAR until the renderer processes it and returns
     * EVT_FRAME_CLEARED.  Any older FRAME_READY received in this interval belongs
     * to pre-clear pixels and must not expose ctx80. */
    private volatile boolean clearPending;

    /* True once we've successfully accepted at least one renderer
     * connection.  Lets the caller distinguish "still starting up,
     * sends are expected to fail" vs "had a working renderer that
     * just died, respawn is appropriate". */
    private volatile boolean everConnected;
    private volatile StateListener stateListener;
    private long lastBindFailureLogMs;
    private int suppressedBindFailures;
    private int connectionGeneration;

    private static final class PendingWrite {
        byte[] packet;
        int generation;
    }

    private final PendingWrite[] writeQueue = new PendingWrite[WRITE_QUEUE_CAPACITY];
    private int writeHead;
    private int writeTail;
    private int writeCount;

    /** Edge notification only. The listener must not block the socket threads. */
    public interface StateListener {
        void onRendererStateChanged(String reason);
    }

    public void setStateListener(StateListener listener) {
        stateListener = listener;
    }

    /**
     * Open the listen socket and start the background accept thread.
     * Non-blocking — returns immediately whether the renderer is up
     * or not.  Method name kept for BAPBridge call-site compatibility.
     */
    public boolean connect() {
        synchronized (lock) {
            if (server != null && !server.isClosed()) return true;
            ServerSocket ss = null;
            try {
                ss = new ServerSocket();
                ss.setReuseAddress(true);
                ss.bind(new InetSocketAddress(HOST, PORT));
                server = ss;
                running = true;
                Log.i(TAG, "listening on " + HOST + ":" + PORT);
            } catch (IOException e) {
                if (ss != null) try { ss.close(); } catch (IOException closeError) { }
                logBindFailure(e);
                return false;
            }

            acceptThread = new Thread(new Runnable() {
                public void run() { acceptLoop(); }
            }, "RendererServer-Accept");
            acceptThread.setDaemon(true);
            acceptThread.start();

            writerThread = new Thread(new Runnable() {
                public void run() { writerLoop(); }
            }, "RendererServer-Write");
            writerThread.setDaemon(true);
            writerThread.start();
        }
        return true;
    }

    /** Presentation recovery may call connect every 500 ms while another owner has :19800.
     * Close every failed socket above and emit at most one warning per 10 seconds. */
    private void logBindFailure(IOException e) {
        long now = System.currentTimeMillis();
        if (lastBindFailureLogMs == 0 || now - lastBindFailureLogMs >= 10000L) {
            String suffix = suppressedBindFailures > 0
                ? " (" + suppressedBindFailures + " repeats suppressed)" : "";
            Log.w(TAG, "bind failed: " + e.getMessage() + suffix);
            lastBindFailureLogMs = now;
            suppressedBindFailures = 0;
        } else {
            suppressedBindFailures++;
        }
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
                try {
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
                        connectionGeneration++;
                        clearWriteQueue();
                        everConnected = true;
                        rendererReady = false;
                        frameReady = false;
                        clearPending = false;
                        lock.notifyAll();
                    }
                    Log.i(TAG, "renderer connected from " + s.getInetAddress() + ":" + s.getPort());
                    notifyStateChanged("connected");

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
                } catch (IOException setupEx) {
                    /* accept() succeeded but post-accept setup failed — close the
                     * orphan socket so its FD isn't leaked, then keep accepting. */
                    try { s.close(); } catch (Exception e2) {}
                }
            } catch (IOException e) {
                if (running) {
                }
                /* If the listen socket was closed (dispose()), exit. */
                synchronized (lock) {
                    if (server == null || server.isClosed()) break;
                }
            }
        }
        Log.i(TAG, "accept thread exiting");
    }

    /** The only thread allowed to perform the potentially blocking Java socket
     * write. BAP/HMI callers merely enqueue one fixed packet and return. */
    private void writerLoop() {
        while (true) {
            PendingWrite pending;
            synchronized (writeLock) {
                while (running && writeCount == 0) {
                    try { writeLock.wait(); } catch (InterruptedException e) { }
                }
                if (!running) break;
                pending = writeQueue[writeHead];
                writeQueue[writeHead] = null;
                writeHead = (writeHead + 1) % WRITE_QUEUE_CAPACITY;
                writeCount--;
            }

            OutputStream ownedOut;
            synchronized (lock) {
                if (out == null || pending == null ||
                        pending.generation != connectionGeneration) {
                    continue;
                }
                ownedOut = out;
            }

            try {
                ownedOut.write(pending.packet);
                ownedOut.flush();
            } catch (Exception e) {
                Log.w(TAG, "Writer failed: " + e.getMessage());
                boolean lostCurrent = false;
                synchronized (lock) {
                    if (out == ownedOut) {
                        closeClientLocked();
                        lostCurrent = true;
                    }
                }
                if (lostCurrent) notifyStateChanged("send-failed");
            }
        }
        Log.i(TAG, "writer thread exiting");
    }

    private void clearWriteQueue() {
        synchronized (writeLock) {
            for (int i = 0; i < WRITE_QUEUE_CAPACITY; i++) writeQueue[i] = null;
            writeHead = 0;
            writeTail = 0;
            writeCount = 0;
            writeLock.notifyAll();
        }
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
                Log.i(TAG, "reader exit: " + e.getClass().getName()
                        + (e.getMessage() == null ? "" : " " + e.getMessage()));
            }
        } finally {
            boolean lostCurrent = false;
            synchronized (lock) {
                if (sock == owned) {
                    closeClientLocked();
                    lostCurrent = true;
                }
            }
            /* A preempted old reader must never report the new socket dead. */
            if (lostCurrent) notifyStateChanged("disconnected");
        }
    }

    private void notifyStateChanged(String reason) {
        StateListener l = stateListener;
        if (l == null) return;
        try {
            l.onRendererStateChanged(reason);
        } catch (Throwable t) {
            Log.w(TAG, "state listener failed: " + t.getMessage());
        }
    }

    private void handleRendererEvent(Socket owned, byte event) {
        boolean changed = false;
        String reason = null;
        synchronized (lock) {
            if (sock != owned) return;
            if (event == EVT_READY) {
                changed = !rendererReady;
                rendererReady = true;
                lock.notifyAll();
                reason = "ready";
            } else if (event == EVT_FRAME_READY) {
                rendererReady = true;
                if (!clearPending) {
                    changed = !frameReady;
                    frameReady = true;
                    lock.notifyAll();
                    reason = "frame-ready";
                }
            } else if (event == EVT_FRAME_CLEARED) {
                changed = clearPending || frameReady;
                clearPending = false;
                frameReady = false;
                lock.notifyAll();
                reason = "frame-cleared";
            }
        }
        if (changed) {
            Log.i(TAG, "renderer " + reason);
            notifyStateChanged(reason);
        }
    }

    /**
     * Lightweight teardown: drop the accepted client socket, keeping the server listen socket
     * open. Does NOT shut down the renderer; its CarPlay supervisor owns that lifecycle. The
     * renderer sees the peer close, blanks itself and reconnects. Call on route stop/link loss.
     */
    public void disconnectClient() {
        boolean changed;
        synchronized (lock) {
            changed = sock != null || rendererReady || frameReady || clearPending;
            closeClientLocked();
            /* everConnected stays true — within this session we've already
             * had a working renderer; subsequent connect failures should
             * still trigger respawn correctly. */
        }
        if (changed) notifyStateChanged("disconnect-request");
    }

    /**
     * Full teardown: close everything (client + server + accept thread).
     * Call on CarPlay deactivate / HMI shutdown.
     *
     * After dispose() the instance is unusable.  Call connect() again
     * (or build a new instance) to reopen.
     */
    public void dispose() {
        /* Do NOT send CMD_SHUTDOWN. The CarPlay supervisor owns the renderer and may preserve it
         * across a fast dio_manager replacement. Close only our sockets. */
        synchronized (lock) {
            running = false;
            everConnected = false;
            closeClientLocked();
            if (server != null) {
                try { server.close(); } catch (Exception e) {}
                server = null;
            }
        }
        clearWriteQueue();
        if (acceptThread != null) {
            try { acceptThread.join(500); } catch (InterruptedException e) {}
            acceptThread = null;
        }
        synchronized (writeLock) { writeLock.notifyAll(); }
        if (writerThread != null) {
            try { writerThread.join(500); } catch (InterruptedException e) {}
            writerThread = null;
        }
        notifyStateChanged("disposed");
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
        connectionGeneration++;
        clearWriteQueue();
        rendererReady = false;
        frameReady = false;
        clearPending = false;
        lock.notifyAll();
    }

    public boolean isConnected() {
        synchronized (lock) {
            return sock != null && !sock.isClosed() && out != null;
        }
    }

    /** True only after the current renderer peer sent EVT_READY. */
    public boolean isReady() {
        synchronized (lock) {
            return sock != null && !sock.isClosed() && out != null && rendererReady;
        }
    }

    /** True only for the currently connected renderer after it has acknowledged a real swap. */
    public boolean isFrameReady() {
        synchronized (lock) {
            return sock != null && !sock.isClosed() && out != null
                && frameReady && !clearPending;
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

    /** Send CMD_CLEAR — blank the popup (route/CarPlay off) WITHOUT killing the renderer. */
    public boolean sendClear() {
        byte[] pkt = new byte[PKT_SIZE];
        pkt[0] = CMD_CLEAR;
        return sendPacket(pkt, true);
    }

    /**
     * Non-blocking send.  Returns false if no current connection or
     * write failed — BAPBridge uses the bool for crash-recovery counter
     * (3 consecutive false returns trigger a renderer respawn).
     */
    private boolean sendPacket(byte[] pkt) {
        return sendPacket(pkt, false);
    }

    private boolean sendPacket(byte[] pkt, boolean beginClear) {
        int generation;
        synchronized (lock) {
            if (out == null) return false;
            generation = connectionGeneration;
            if (beginClear) {
                frameReady = false;
                clearPending = true;
                lock.notifyAll();
            }
        }

        synchronized (writeLock) {
            PendingWrite pending = new PendingWrite();
            pending.packet = pkt;
            pending.generation = generation;
            if (writeCount == WRITE_QUEUE_CAPACITY) {
                /* Full state is resent frequently; discard the oldest stale
                 * command rather than ever blocking a BAP/HMI caller. */
                writeQueue[writeHead] = null;
                writeHead = (writeHead + 1) % WRITE_QUEUE_CAPACITY;
                writeCount--;
            }
            writeQueue[writeTail] = pending;
            writeTail = (writeTail + 1) % WRITE_QUEUE_CAPACITY;
            writeCount++;
            writeLock.notifyAll();
        }
        return true;
    }
}
