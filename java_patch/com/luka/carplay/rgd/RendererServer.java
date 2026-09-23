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
 *     when a new connection comes in.  If carplay_monitor.sh restarts the
 *     renderer, it just reconnects and we pick up the new socket.
 *   - sendXxx() returns false if no current connection — caller (BAPBridge)
 *     counts failures to drop the view and wait for a reconnect; Java never
 *     kills or starts the renderer.
 *   - disconnect() stops the accept thread + closes everything.
 *
 * No timing dependencies, no blocking accepts, no Thread.sleep hacks.
 *
 * Sends 48-byte cr_cmd_t packets matching maneuver_render/protocol.h.
 * All methods are non-fatal (swallow exceptions, log only).
 *
 * Java 1.4 / Foundation 1.1 (no generics, no autoboxing, no enhanced for).
 *
 * Copyright (c) 2026 LuKa (@LuKa_dev)
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
    private final int port;
    public RendererServer() { this(PORT); }
    RendererServer(int port) { this.port = port; }
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
    private static final byte CMD_PROGRESS    = 0x06;
    private static final byte CMD_VISIBLE_AREA = 0x08;
    private static final byte CMD_CLEAR       = 0x07;
    private static final byte CMD_LANES_BEGIN = 0x0c;
    private static final byte CMD_LANES_LANE = 0x0d;
    private static final byte CMD_LANES_COMMIT = 0x0e;
    private int laneToken;

    /* CMD_MANEUVER flags */
    private static final byte MAN_FLAG_SET_PERSP = 0x01;
    private static final byte MAN_FLAG_PROGRESS  = 0x02;
    private static final byte MAN_FLAG_BAP_GEOMETRY = 0x04;
    private static final byte MAN_FLAG_REFRESH = 0x08;
    private static final byte MAN_FLAG_SNAP_TO_ROAD = 0x10;

    private final Object lock = new Object();
    private final Object writeLock = new Object();
    private ServerSocket server;
    private Socket sock;             /* protected by lock */
    private OutputStream out;        /* protected by lock */
    private Thread acceptThread;
    private Thread writerThread;
    private volatile boolean running;
    private volatile int lifecycleGeneration;
    private volatile boolean rendererReady;
    private volatile boolean frameReady;
    /* True from enqueueing CMD_CLEAR until the renderer processes it and returns
     * EVT_FRAME_CLEARED.  Any older FRAME_READY received in this interval belongs
     * to pre-clear pixels and must not expose ctx80. */
    private volatile boolean clearPending;
    private int pendingClears; /* queued or sent CLEARs awaiting their ordered ACKs */

    /* True once we've successfully accepted at least one renderer
     * connection.  Lets the caller distinguish "still starting up,
     * sends are expected to fail" vs "had a working renderer that
     * just died, drop the view until it reconnects". */
    private volatile boolean everConnected;
    private volatile StateListener stateListener;
    private long lastBindFailureLogMs;
    private int suppressedBindFailures;
    private int connectionGeneration;
    private boolean haveVisibleArea;
    private int visibleX, visibleY, visibleW, visibleH;

    private static final class PendingWrite {
        byte[] packet;
        int generation;
        boolean clear;
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
                ss.bind(new InetSocketAddress(HOST, port));
                server = ss;
                running = true;
                Log.i(TAG, "listening on " + HOST + ":" + port);
            } catch (IOException e) {
                if (ss != null) try { ss.close(); } catch (IOException closeError) { }
                logBindFailure(e);
                return false;
            }

            final int lifecycle = ++lifecycleGeneration;
            final ServerSocket ownedServer = server;
            acceptThread = new Thread(new Runnable() {
                public void run() { acceptLoop(ownedServer, lifecycle); }
            }, "RendererServer-Accept");
            acceptThread.setDaemon(true);
            acceptThread.start();

            writerThread = new Thread(new Runnable() {
                public void run() { writerLoop(lifecycle); }
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
    private boolean isCurrentRun(int lifecycle) {
        return running && lifecycleGeneration == lifecycle;
    }

    private void acceptLoop(ServerSocket ss, int lifecycle) {
        while (isCurrentRun(lifecycle) && ss != null && !ss.isClosed()) {
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
                        if (!isCurrentRun(lifecycle) || server != ss) {
                            s.close(); break;
                        }
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
    private void writerLoop(int lifecycle) {
        while (true) {
            PendingWrite pending;
            synchronized (writeLock) {
                while (isCurrentRun(lifecycle) && writeCount == 0) {
                    try { writeLock.wait(); } catch (InterruptedException e) { }
                }
                if (!isCurrentRun(lifecycle)) break;
                pending = writeQueue[writeHead];
                writeQueue[writeHead] = null;
                writeHead = (writeHead + 1) % WRITE_QUEUE_CAPACITY;
                writeCount--;
            }

            OutputStream ownedOut;
            synchronized (lock) {
                if (!isCurrentRun(lifecycle) || out == null || pending == null ||
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
            haveVisibleArea = false;
            for (int i = 0; i < WRITE_QUEUE_CAPACITY; i++) writeQueue[i] = null;
            writeHead = 0;
            writeTail = 0;
            writeCount = 0;
            pendingClears = 0;
            clearPending = false;
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
                if (pendingClears > 0) --pendingClears;
                clearPending = pendingClears > 0;
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
             * had a working renderer; later send failures still count as
             * a lost renderer, not start-up. */
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
        Thread accept, writer;
        synchronized (lock) {
            running = false;
            ++lifecycleGeneration;
            everConnected = false;
            closeClientLocked();
            if (server != null) {
                try { server.close(); } catch (Exception e) {}
                server = null;
            }
            clearWriteQueue();
            accept = acceptThread; acceptThread = null;
            writer = writerThread; writerThread = null;
        }
        if (accept != null && accept != Thread.currentThread()) {
            try { accept.join(500); } catch (InterruptedException e) { Thread.currentThread().interrupt(); }
        }
        if (writer != null && writer != Thread.currentThread()) {
            try { writer.join(500); } catch (InterruptedException e) { Thread.currentThread().interrupt(); }
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
     * from "had a connection that died" (drop the view, wait for reconnect).
     */
    public boolean everConnected() {
        return everConnected;
    }

    /**
     * Send CMD_MANEUVER — push a new maneuver with transition.
     *
     * @param icon           ICON_* constant (0-9)
     * @param direction      -1=left, 0=center, +1=right
     * @param exitAngle      signed degrees
     * @param drivingSide    0=RHT, 1=LHT
     * @param junctionAngles signed degree array (may be null)
     * @param remainingLevel  remaining segments 0-16 (16=far, 0=at maneuver)
     * @param progressMode   legacy progress mode; blink phases use sendBapProgressManeuver
     * @param perspective    0=2D, 1=3D after transition (-1=don't change)
     */
    public boolean sendManeuver(int icon, int direction, int exitAngle,
                                int drivingSide, int[] junctionAngles,
                                int remainingLevel, int progressMode,
                                int perspective) {
        return sendManeuverPacket(icon, direction, exitAngle, drivingSide, junctionAngles,
            remainingLevel, progressMode, perspective, false, false, false, -1);
    }

    public static final int PROGRESS_OFF=0, PROGRESS_FILL=1,
        PROGRESS_BLINK_LOW=2, PROGRESS_BLINK_HIGH=3;
    private static final int PROGRESS_FLAG=0x20;

    /** Same legacy packet, with an explicit progress snapshot in unused byte 42. */
    public boolean sendBapProgressManeuver(int icon, int direction, int exitAngle,
            int drivingSide, int[] junctionAngles, int level, int mode, int perspective,
            boolean refresh, boolean snapToRoad, int progressState) {
        return sendManeuverPacket(icon,direction,exitAngle,drivingSide,junctionAngles,
            level,mode,perspective,true,refresh,snapToRoad,progressState);
    }

    /** Raw lane guidance is an independent snapshot, never maneuver geometry. */
    boolean sendLaneGuidance(LaneGuidanceSnapshot lanes) {
        synchronized (lock) {
            if (++laneToken == 0) ++laneToken;
            return sendPacket(lanePacket(lanes, laneToken));
        }
    }
    static byte[] lanePacket(LaneGuidanceSnapshot lanes, int token) {
        byte[] batch = new byte[(lanes.count + 2) * PKT_SIZE];
        batch[0] = CMD_LANES_BEGIN; putLaneInt(batch, 2, token);
        batch[6] = (byte)lanes.count; batch[7] = (byte)(lanes.complete ? 1 : 0);
        batch[8] = (byte)(lanes.showing ? 1 : 0); putLaneInt(batch, 10, lanes.eventIndex);
        for (int i = 0; i < lanes.count; ++i) {
            int p = (i + 1) * PKT_SIZE;
            batch[p] = CMD_LANES_LANE; putLaneInt(batch, p + 2, token);
            batch[p + 6] = (byte)i; putLaneShort(batch, p + 7, lanes.positions[i]);
            batch[p + 9] = (byte)lanes.status[i]; batch[p + 10] = (byte)lanes.angles[i].length;
            putLaneShort(batch, p + 11, lanes.primary[i]);
            for (int j = 0; j < lanes.angles[i].length; ++j)
                putLaneShort(batch, p + 13 + j * 2, lanes.angles[i][j]);
        }
        int end = (lanes.count + 1) * PKT_SIZE;
        batch[end] = CMD_LANES_COMMIT; putLaneInt(batch, end + 2, token);
        return batch; // One writer queue entry: records cannot interleave.
    }
    private static void putLaneInt(byte[] b, int p, int v) {
        b[p]=(byte)(v>>>24); b[p+1]=(byte)(v>>>16); b[p+2]=(byte)(v>>>8); b[p+3]=(byte)v;
    }
    private static void putLaneShort(byte[] b, int p, int v) { b[p]=(byte)(v>>>8); b[p+1]=(byte)v; }

    private boolean sendManeuverPacket(int icon, int direction, int exitAngle,
                                      int drivingSide, int[] junctionAngles,
                                      int remainingLevel, int progressMode,
                                      int perspective, boolean bapGeometry, boolean refresh, boolean snapToRoad, int progressState) {
        byte[] pkt = new byte[PKT_SIZE];
        pkt[0] = CMD_MANEUVER;
        byte flags = 0;
        if (bapGeometry) flags |= MAN_FLAG_BAP_GEOMETRY;
        if (refresh) flags |= MAN_FLAG_REFRESH;
        if (snapToRoad) flags |= MAN_FLAG_SNAP_TO_ROAD;
        if (progressMode > 0 || progressState >= 0) flags |= MAN_FLAG_PROGRESS;
        if (progressState >= 0) {
            flags |= PROGRESS_FLAG;
            pkt[44]=(byte)progressState;
        }
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
            if (jCount > 18) jCount = 18;  /* max 18: payload[6..41], keeps [42..45] for phase/perspective/progress */
        }
        pkt[7] = (byte) (jCount & 0xFF);
        /* payload[6..41]: junction_angles (big-endian i16 each) */
        for (int i = 0; i < jCount; i++) {
            int off = 8 + i * 2;
            int a = junctionAngles[i];
            pkt[off]     = (byte) ((a >> 8) & 0xFF);
            pkt[off + 1] = (byte) (a & 0xFF);
        }

        /* perspective in payload[43] when flag set */
        if (perspective >= 0) {
            pkt[45] = (byte) (perspective & 0xFF);
        }
        /* remaining progress in payload[44..45] when flag set */
        if (progressMode > 0 || progressState >= 0) {
            pkt[46] = (byte) (remainingLevel & 0xFF);
            pkt[47] = (byte) (progressMode & 0xFF);
        }

        return sendPacket(pkt);
    }

    /** Arrow progress; state -1 preserves legacy unmarked packets for old clients. */
    public boolean sendProgress(int level, int mode, int progressState) {
        byte[] pkt = new byte[PKT_SIZE];
        pkt[0] = CMD_PROGRESS;
        if(progressState >= 0) { pkt[1]=(byte)PROGRESS_FLAG; pkt[4]=(byte)progressState; }
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

    /** Stage/layout updates are independent of the current maneuver and distance.
     * Cache only a successful enqueue; reconnect always replays the visible area. */
    public boolean sendVisibleArea(int x, int y, int w, int h) {
        synchronized (lock) {
            if (haveVisibleArea && visibleX == x && visibleY == y && visibleW == w && visibleH == h)
                return true;
            byte[] pkt = new byte[PKT_SIZE];
            pkt[0] = CMD_VISIBLE_AREA;
            int[] values = new int[]{x, y, w, h};
            for (int i = 0; i < 4; i++) {
                if (values[i] < 0 || values[i] > 65535) return false;
                pkt[2 + 2*i] = (byte)(values[i] >>> 8);
                pkt[3 + 2*i] = (byte)values[i];
            }
            if (!sendPacket(pkt)) return false;
            visibleX = x; visibleY = y; visibleW = w; visibleH = h;
            haveVisibleArea = true;
            return true;
        }
    }

    /**
     * Non-blocking send.  Returns false if no current connection or
     * queue is full. Accepted maneuvers remain queued until written or the
     * connection is lost, which wakes the shared RGI snapshot replay.
     */
    private boolean sendPacket(byte[] pkt) {
        return sendPacket(pkt, false);
    }

    /** writeLock held. Preserve the order of every retained command. */
    private void removePendingWrite(int offset) {
        for (int i = offset; i + 1 < writeCount; i++)
            writeQueue[(writeHead + i) % WRITE_QUEUE_CAPACITY] =
                writeQueue[(writeHead + i + 1) % WRITE_QUEUE_CAPACITY];
        writeTail = (writeTail + WRITE_QUEUE_CAPACITY - 1) % WRITE_QUEUE_CAPACITY;
        writeQueue[writeTail] = null;
        writeCount--;
    }

    private boolean sendPacket(byte[] pkt, boolean beginClear) {
        synchronized (lock) {
            if (!running || out == null) return false;
            synchronized (writeLock) {
                PendingWrite pending = new PendingWrite();
                pending.packet = pkt;
                pending.generation = connectionGeneration;
                pending.clear = beginClear;
                // Keep the latest independent state as a whole queue entry.
                // In particular, a lane hide must survive progress traffic.
                // Replacing an older snapshot across CLEAR is safe: the new
                // snapshot stays after that barrier at its original TCP order.
                if (pkt[0] == CMD_VISIBLE_AREA || pkt[0] == CMD_LANES_BEGIN) {
                    for (int n = 0; n < writeCount; n++) {
                        if (writeQueue[(writeHead + n) % WRITE_QUEUE_CAPACITY].packet[0] != pkt[0])
                            continue;
                        removePendingWrite(n);
                        break;
                    }
                }
                if (pkt[0] == CMD_PROGRESS) {
                    // Only replace progress belonging to this maneuver. A new
                    // maneuver embeds its own initial progress; CLEAR is a barrier.
                    for (int n = writeCount - 1; n >= 0; n--) {
                        byte command = writeQueue[(writeHead + n) % WRITE_QUEUE_CAPACITY].packet[0];
                        if (command == CMD_MANEUVER || command == CMD_CLEAR) break;
                        if (command == CMD_PROGRESS) {
                            removePendingWrite(n);
                            break;
                        }
                    }
                }
                if (writeCount == WRITE_QUEUE_CAPACITY) {
                    /* Never discard an accepted maneuver: BAPBridge has already
                     * cached its identity and cannot detect such a loss. Only
                     * obsolete progress can yield space; otherwise report backpressure. */
                    int drop = -1;
                    boolean superseded = pkt[0] == CMD_MANEUVER
                        || pkt[0] == CMD_CLEAR || pkt[0] == CMD_PROGRESS;
                    for (int n = writeCount - 1; n >= 0; n--) {
                        byte command = writeQueue[(writeHead + n) % WRITE_QUEUE_CAPACITY].packet[0];
                        if (command == CMD_PROGRESS && superseded) drop = n;
                        if (command == CMD_MANEUVER || command == CMD_CLEAR) superseded = true;
                    }
                    if (drop < 0) return false;
                    removePendingWrite(drop);
                }
                if (beginClear) {
                    frameReady = false;
                    ++pendingClears;
                    clearPending = true;
                    lock.notifyAll();
                }
                writeQueue[writeTail] = pending;
                writeTail = (writeTail + 1) % WRITE_QUEUE_CAPACITY;
                writeCount++;
                writeLock.notifyAll();
            }
        }
        return true;
    }
}
