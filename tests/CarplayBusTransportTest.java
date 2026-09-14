package com.luka.carplay.bus;

import com.luka.carplay.framework.Log;
import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.InputStream;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.net.ServerSocket;
import java.net.Socket;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

/** Actual bus on ephemeral localhost ports plus deterministic delayed old reads. */
public final class CarplayBusTransportTest {
    private static void check(boolean value, String why) {
        if (!value) throw new AssertionError(why);
    }
    private static Field field(String name) throws Exception {
        Field f = CarplayBus.class.getDeclaredField(name); f.setAccessible(true); return f;
    }
    private static Object get(CarplayBus bus, String name) throws Exception { return field(name).get(bus); }
    private static void set(CarplayBus bus, String name, Object value) throws Exception { field(name).set(bus, value); }
    private static Object call(CarplayBus bus, String name, Class[] types, Object[] args) throws Exception {
        Method m = CarplayBus.class.getDeclaredMethod(name, types); m.setAccessible(true); return m.invoke(bus, args);
    }
    private static void await(CountDownLatch latch, String why) throws Exception {
        check(latch.await(3, TimeUnit.SECONDS), why);
    }
    private static int port(CarplayBus bus) throws Exception {
        long end = System.nanoTime() + TimeUnit.SECONDS.toNanos(3);
        while (System.nanoTime() < end) {
            synchronized (get(bus, "lock")) {
                ServerSocket s = (ServerSocket)get(bus, "serverSocket");
                if (s != null) return s.getLocalPort();
            }
            Thread.sleep(1);
        }
        throw new AssertionError("server did not start");
    }
    private static byte[] packet(byte value) throws Exception {
        ByteArrayOutputStream bytes = new ByteArrayOutputStream();
        DataOutputStream out = new DataOutputStream(bytes);
        out.writeInt(CarplayBus.MAGIC); out.writeInt(1);
        out.writeShort(CarplayBus.EVT_RGD_UPDATE); out.writeByte(0); out.writeByte(0);
        out.writeInt(1); out.writeByte(value);
        return bytes.toByteArray();
    }

    private static void malformedLists() throws Exception {
        byte[] text = "lane_directions:s:bad,2,,2147483648,-3,\n".getBytes("UTF-8");
        int[] values = CarplayBus.parseText(text, text.length).intList("lane_directions");
        int[] expected = {0, 2, 0, 0, -3, 0};
        check(java.util.Arrays.equals(values, expected), "malformed list must retain item positions");
        check(CarplayBus.parseText(text, text.length + 1).size() == 0, "invalid text length");
    }

    private static void realWireAndRestart() throws Exception {
        final CarplayBus bus = new CarplayBus(0);
        for (int cycle = 0; cycle < 30; ++cycle) {
            final CountDownLatch got = new CountDownLatch(1);
            bus.on(CarplayBus.EVT_RGD_UPDATE, new CarplayBus.Listener() {
                public void onFrame(int type, int flags, byte[] bytes, int len) {
                    if (len == 1 && bytes[0] == 7) got.countDown();
                }
            });
            bus.start();
            Socket peer = new Socket("127.0.0.1", port(bus));
            peer.setSoTimeout(3000);
            byte[] p = packet((byte)7);
            for (int i = 0; i < p.length; ++i) peer.getOutputStream().write(p, i, 1);
            await(got, "fragmented frame after restart");
            check(!bus.send(0xffff, 0, null, 0), "out-of-range type");
            check(!bus.send(CarplayBus.CMD_ALT_ZONE, 0, new byte[1], 2), "out-of-range payload");
            check(bus.sendBinary(CarplayBus.CMD_ALT_ZOOM, new byte[] {3}), "send after restart");
            DataInputStream in = new DataInputStream(peer.getInputStream());
            check(in.readInt() == CarplayBus.MAGIC, "outbound magic"); in.readInt();
            check(in.readUnsignedShort() == CarplayBus.CMD_ALT_ZOOM, "outbound type");
            check(in.readUnsignedByte() == CarplayBus.FLAG_BINARY, "outbound flags"); in.readByte();
            check(in.readInt() == 1 && in.readByte() == 3, "outbound body");
            Thread io = (Thread)get(bus, "ioThread"), reader = (Thread)get(bus, "readerThread"), writer = (Thread)get(bus, "writerThread");
            bus.stop(); peer.close();
            check(!io.isAlive() && !reader.isAlive() && !writer.isAlive(), "old workers still alive");
            check(get(bus, "serverSocket") == null && !bus.isConnected(), "stop leaked socket");
        }
        /* A delayed bind completion for a stopped run cannot publish a listener. */
        int stopped = ((Integer)get(bus, "lifecycleGeneration")).intValue();
        check(Boolean.FALSE.equals(call(bus, "openServerSocket", new Class[] {Integer.TYPE}, new Object[] {Integer.valueOf(stopped - 1)})), "stale bind accepted");
        check(get(bus, "serverSocket") == null, "stale bind leaked server");
    }

    private static final class DelayedRead extends ByteArrayInputStream {
        final CountDownLatch ready = new CountDownLatch(1), release = new CountDownLatch(1);
        DelayedRead(byte[] bytes) { super(bytes); }
        public synchronized int read(byte[] bytes, int off, int len) {
            int n = super.read(bytes, off, len);
            if (available() == 0 && n > 0) {
                ready.countDown();
                try { if (!release.await(3, TimeUnit.SECONDS)) throw new AssertionError("release old read"); }
                catch (InterruptedException e) { throw new AssertionError(e); }
            }
            return n;
        }
    }

    private static void delayedOldRead() throws Exception {
        final CarplayBus bus = new CarplayBus(0);
        final DelayedRead old = new DelayedRead(packet((byte)1));
        final CountDownLatch fresh = new CountDownLatch(1);
        final int[] oldCalls = {0};
        final Socket oldSocket = new Socket(), newSocket = new Socket();
        bus.on(CarplayBus.EVT_RGD_UPDATE, new CarplayBus.Listener() {
            public void onFrame(int type, int flags, byte[] bytes, int len) {
                if (bytes[0] == 1) oldCalls[0]++;
                if (bytes[0] == 2) fresh.countDown();
            }
        });
        set(bus, "running", Boolean.TRUE); set(bus, "lifecycleGeneration", Integer.valueOf(1));
        set(bus, "sock", oldSocket); set(bus, "in", old);
        final Throwable[] failure = {null};
        Thread reader = new Thread(new Runnable() {
            public void run() {
                try { call(bus, "readerLoop", new Class[] {Integer.TYPE}, new Object[] {Integer.valueOf(1)}); }
                catch (Throwable t) { failure[0] = t; }
            }
        });
        set(bus, "readerThread", reader); reader.start();
        await(old.ready, "old read never paused");
        synchronized (get(bus, "lock")) {
            set(bus, "sock", newSocket); set(bus, "in", new ByteArrayInputStream(packet((byte)2)));
        }
        old.release.countDown();
        await(fresh, "new connection not read");
        bus.stop(); oldSocket.close();
        check(!reader.isAlive() && failure[0] == null, "reader failure");
        check(oldCalls[0] == 0, "preempted old frame dispatched");
    }

    public static void main(String[] args) throws Exception {
        Log.setLevel(-1);
        malformedLists(); realWireAndRestart(); delayedOldRead();
        System.out.println("CarplayBusTransportTest: malformed lists, 30 restarts, fragmented wire, stale bind/read and outbound bounds PASS");
    }
}
