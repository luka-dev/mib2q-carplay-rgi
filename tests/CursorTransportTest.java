package com.luka.carplay.cursor;
import java.net.ServerSocket;
import java.net.Socket;
import java.net.InetAddress;
import java.net.SocketTimeoutException;
import java.io.DataInputStream;

public final class CursorTransportTest implements CursorTransport.Listener {
    private int ackSeq, ackConnection, count;
    public synchronized void cursorAck(int connection, byte[] p) {
        ackSeq = CursorWire.get32(p, 0); ackConnection = connection; count++; notifyAll();
    }
    private synchronized void waitAck(int seq, int connection) throws Exception {
        long until = System.currentTimeMillis() + 2000;
        while (ackSeq != seq && System.currentTimeMillis() < until) wait(10);
        check(ackSeq == seq && ackConnection == connection, "wrong/stale ACK");
    }
    private static void check(boolean condition, String text) {
        if (!condition) throw new AssertionError(text);
    }
    private static int ready(CursorTransport t) throws Exception {
        long until = System.currentTimeMillis() + 2000;
        while (t.connection() == 0 && System.currentTimeMillis() < until) Thread.sleep(5);
        check(t.connection() != 0, "no direct cursor connection"); return t.connection();
    }
    public static void main(String[] ignored) throws Exception {
        ServerSocket server = new ServerSocket(0, 2, InetAddress.getByName("127.0.0.1"));
        server.setSoTimeout(2000);
        CursorTransportTest receiver = new CursorTransportTest();
        CursorTransport t = new CursorTransport(receiver, server.getLocalPort());
        int previous = 0;
        try {
            for (int i = 1; i <= 30; i++) {
                t.start();
                Socket peer = server.accept(); peer.setSoTimeout(1000);
                int owner = ready(t);
                check(owner != previous, "reused connection generation");
                byte[] frame = CursorWire.encode(1, i, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
                check(!t.offer(previous, frame), "retired connection accepted a SHOW");
                check(t.offer(owner, frame), "current connection rejected state");
                byte[] received = new byte[CursorWire.FRAME_SIZE]; new DataInputStream(peer.getInputStream()).readFully(received);
                check(CursorWire.get32(received, 4) == i, "state replayed/corrupted");
                byte[] reply = new byte[CursorWire.ACK_SIZE]; reply[3] = (byte)i;
                peer.getOutputStream().write(reply, 0, 5); Thread.sleep(25);
                peer.getOutputStream().write(reply, 5, reply.length-5);
                receiver.waitAck(i, owner);
                // Retire during an incomplete next ACK; it must not cross reconnect.
                peer.getOutputStream().write(reply, 0, 7);
                t.stop();
                try { check(peer.getInputStream().read() < 0, "retired socket left open"); }
                catch (java.net.SocketException reset) { }
                peer.close(); previous = owner;
            }
            t.start();
            Socket peer = server.accept(); ready(t); peer.setSoTimeout(100);
            try { peer.getInputStream().read(); throw new AssertionError("old frame replayed after reconnect"); }
            catch (SocketTimeoutException expected) { }
            peer.close();
            synchronized (receiver) { check(receiver.count == 30, "retired partial ACK was delivered"); }
        } finally { t.stop(); server.close(); }
        System.out.println("CursorTransportTest: direct link, fragmented ACKs, 30 stop/start generations and no stale replay PASS");
    }
}
