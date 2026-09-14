package com.luka.carplay.cursor;

import java.io.InputStream;
import java.io.OutputStream;
import java.io.IOException;
import java.io.InterruptedIOException;
import java.net.InetSocketAddress;
import java.net.Socket;

/** Direct loopback link to cursor_render. No socket I/O on the HMI/input thread.
 * A connection owns its queued state; reconnect never replays a previous SHOW.
 */
final class CursorTransport implements Runnable {
    interface Listener { void cursorAck(int connection, byte[] ack); }
    private final Listener listener;
    private final int port;
    private final Object lock = new Object();
    private boolean enabled;
    private Thread worker;
    private Socket socket;
    private int serial, connection;
    private byte[] pending;

    CursorTransport(Listener receiver) { this(receiver, 19840); }
    CursorTransport(Listener receiver, int endpoint) { listener = receiver; port = endpoint; }

    void start() {
        synchronized (lock) {
            enabled = true;
            if (worker == null) {
                worker = new Thread(this, "CarPlay-Cursor");
                worker.setDaemon(true); worker.start();
            }
            lock.notifyAll();
        }
    }
    void stop() {
        Socket old;
        synchronized (lock) {
            enabled = false; pending = null; connection = 0;
            old = socket; socket = null; lock.notifyAll();
        }
        close(old); // Interrupt connect/read/write owned by the retiring connection.
    }
    int connection() { synchronized (lock) { return connection; } }
    boolean offer(int owner, byte[] frame) {
        synchronized (lock) {
            if (!enabled || owner == 0 || owner != connection || frame == null || frame.length != CursorWire.FRAME_SIZE) return false;
            pending = frame; lock.notifyAll(); return true;
        }
    }
    private boolean owns(Socket owned) {
        synchronized (lock) { return enabled && socket == owned; }
    }
    private static void close(Socket owned) {
        if (owned != null) try { owned.close(); } catch (IOException ignored) { }
    }
    public void run() {
        for (;;) {
            Socket owned;
            synchronized (lock) {
                while (!enabled) try { lock.wait(); } catch (InterruptedException ignored) { }
                owned = new Socket(); socket = owned;
            }
            try {
                owned.connect(new InetSocketAddress("127.0.0.1", port), 300);
                owned.setTcpNoDelay(true); owned.setSoTimeout(20);
                InputStream input = owned.getInputStream(); OutputStream output = owned.getOutputStream();
                int incarnation;
                synchronized (lock) {
                    if (!owns(owned)) continue;
                    if (++serial == 0) ++serial;
                    incarnation = connection = serial;
                }
                byte[] ack = new byte[CursorWire.ACK_SIZE]; int used = 0;
                while (owns(owned)) {
                    byte[] next;
                    synchronized (lock) { next = pending; pending = null; }
                    if (next != null) { output.write(next); output.flush(); }
                    int n;
                    try { n = input.read(ack, used, ack.length - used); }
                    catch (InterruptedIOException timeout) { continue; }
                    if (n < 0) break;
                    used += n;
                    if (used == ack.length) {
                        if (owns(owned)) {
                            try { listener.cursorAck(incarnation, ack); }
                            catch (RuntimeException badReceiver) { throw new IOException("cursor ACK receiver failed"); }
                        }
                        ack = new byte[CursorWire.ACK_SIZE]; used = 0;
                    }
                }
            } catch (IOException ignored) {
                // The supervisor may start/restart the service later. No old state is replayed.
            } finally {
                close(owned);
                synchronized (lock) {
                    if (socket == owned) { socket = null; connection = 0; pending = null; }
                }
            }
            synchronized (lock) {
                if (enabled) try { lock.wait(300); } catch (InterruptedException ignored) { }
            }
        }
    }
}
