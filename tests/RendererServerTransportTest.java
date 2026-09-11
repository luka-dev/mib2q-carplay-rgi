package com.luka.carplay.rgd;

import com.luka.carplay.framework.Log;
import java.io.ByteArrayOutputStream;
import java.io.DataInputStream;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.net.ServerSocket;
import java.net.Socket;

public final class RendererServerTransportTest {
    private static void check(boolean value, String why) { if (!value) throw new AssertionError(why); }
    private static Field field(String name) throws Exception {
        Field f = RendererServer.class.getDeclaredField(name); f.setAccessible(true); return f;
    }
    private static Object get(RendererServer s, String name) throws Exception { return field(name).get(s); }
    private static void set(RendererServer s, String name, Object v) throws Exception { field(name).set(s, v); }
    private static void event(RendererServer s, Socket peer, int event) throws Exception {
        Method m = RendererServer.class.getDeclaredMethod("handleRendererEvent", Socket.class, Byte.TYPE);
        m.setAccessible(true); m.invoke(s, peer, Byte.valueOf((byte)event));
    }
    private static void ready(RendererServer server) throws Exception {
        long end = System.nanoTime() + 3000000000L;
        while (!server.isReady() && System.nanoTime() < end) Thread.sleep(1);
        check(server.isReady(), "renderer handshake");
    }
    private static void multipleClears() throws Exception {
        RendererServer server = new RendererServer(0);
        check(server.connect(), "listen");
        int port = ((ServerSocket)get(server, "server")).getLocalPort();
        Socket peer = new Socket("127.0.0.1", port); peer.setSoTimeout(3000);
        byte[] ready = new byte[48]; ready[0] = (byte)0x81;
        peer.getOutputStream().write(ready); ready(server);
        Socket owned = (Socket)get(server, "sock");
        check(server.sendClear(), "clear 1");
        check(server.sendManeuver(3, -1, -90, 0, new int[] {-45, 90}, 8, 1, 1), "maneuver");
        check(server.sendClear(), "clear 2");
        DataInputStream in = new DataInputStream(peer.getInputStream());
        byte[] one = new byte[48], maneuver = new byte[48], two = new byte[48];
        in.readFully(one); in.readFully(maneuver); in.readFully(two);
        check(one[0] == 7 && two[0] == 7 && maneuver[0] == 1, "48-byte command ordering");
        check(maneuver[3] == -1 && maneuver[4] == -1 && (maneuver[5] & 255) == 166, "signed BE16 direction/angle");
        check(maneuver[7] == 2 && maneuver[45] == 1 && maneuver[46] == 8 && maneuver[47] == 1, "layout/bargraph wire fields");
        event(server, owned, 0x83);
        event(server, owned, 0x82);
        check(!server.isFrameReady(), "first CLEAR ACK exposed pixels before second CLEAR");
        event(server, owned, 0x83);
        event(server, owned, 0x82);
        check(server.isFrameReady(), "last CLEAR ACK did not release gate");
        server.dispose(); peer.close();
    }
    private static void queueOverflow() throws Exception {
        RendererServer server = new RendererServer(0);
        Socket fake = new Socket(); ByteArrayOutputStream bytes = new ByteArrayOutputStream();
        set(server, "running", Boolean.TRUE); set(server, "lifecycleGeneration", Integer.valueOf(2));
        set(server, "sock", fake); set(server, "out", bytes);
        check(server.sendClear(), "queued CLEAR");
        for (int i = 0; i < 32; ++i) check(server.sendBargraph(i % 17, 1), "queue fill");
        check(((Integer)get(server, "writeCount")).intValue() == 32, "queue limit");
        check(((Integer)get(server, "pendingClears")).intValue() == 1, "overflow lost CLEAR barrier");
        Object first = ((Object[])get(server, "writeQueue"))[((Integer)get(server, "writeHead")).intValue()];
        Field packet = first.getClass().getDeclaredField("packet"); packet.setAccessible(true);
        check(((byte[])packet.get(first))[0] == 7, "queue dropped or reordered CLEAR");
        event(server, fake, 0x82); check(!server.isFrameReady(), "overflow released CLEAR gate");
        Method oldWriter = RendererServer.class.getDeclaredMethod("writerLoop", Integer.TYPE);
        oldWriter.setAccessible(true); oldWriter.invoke(server, Integer.valueOf(1));
        check(((Integer)get(server, "writeCount")).intValue() == 32 && bytes.size() == 0, "old writer consumed new queue");
        // Replace every ordinary queued command with a CLEAR. With no droppable
        // command left, report backpressure without inventing another pending ACK.
        for (int i = 0; i < 31; ++i) check(server.sendClear(), "fill remaining barriers");
        check(!server.sendClear() && !server.sendBargraph(1, 1), "barrier-only queue accepted overflow");
        check(((Integer)get(server, "pendingClears")).intValue() == 32, "rejected command changed ACK count");
        server.dispose();
    }
    private static void restart() throws Exception {
        RendererServer server = new RendererServer(0);
        for (int i = 0; i < 30; ++i) {
            check(server.connect(), "restart bind");
            Thread accept = (Thread)get(server, "acceptThread"), writer = (Thread)get(server, "writerThread");
            server.dispose();
            check(!accept.isAlive() && !writer.isAlive(), "old server workers alive");
            check(get(server, "server") == null && !server.isConnected(), "disposed server retained socket");
        }
    }
    public static void main(String[] args) throws Exception {
        Log.setLevel(-1); multipleClears(); queueOverflow(); restart();
        System.out.println("RendererServerTransportTest: CLEAR ACK ordering, queue overflow, 48-byte wire, stale writer and 30 restarts PASS");
    }
}
