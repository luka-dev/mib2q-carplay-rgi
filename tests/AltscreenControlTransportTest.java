package com.luka.carplay.core;

import com.luka.carplay.framework.Log;
import java.io.InputStream;
import java.io.OutputStream;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.net.Socket;
import java.util.concurrent.LinkedBlockingQueue;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

/** Real module + real control reader/writer; only the socket and HMI lifecycle
 * entry dependencies are replaced. Keep one peer across stop/start. */
public final class AltscreenControlTransportTest {
    private static void check(boolean yes, String why) { if (!yes) throw new AssertionError(why); }
    private static Field field(Class type, String name) throws Exception {
        Field f = type.getDeclaredField(name); f.setAccessible(true); return f;
    }
    private static Object ctl(String name) throws Exception { return field(AltscreenControlServer.class, name).get(null); }
    private static boolean flag(String name) throws Exception { return field(AltScreenModule.class, name).getBoolean(null); }
    private static int epoch() throws Exception { return field(AltScreenModule.class, "rendererEpoch").getInt(null); }
    private static void state(boolean ready, boolean video) throws Exception {
        check(flag("rendererReady") == ready && flag("videoAvailable") == video,
            "unexpected state ready=" + flag("rendererReady") + " video=" + flag("videoAvailable"));
    }
    private static final class Peer extends Socket {
        final LinkedBlockingQueue<byte[]> incoming = new LinkedBlockingQueue<byte[]>();
        final LinkedBlockingQueue<byte[]> outgoing = new LinkedBlockingQueue<byte[]>();
        boolean closed;
        volatile boolean blockQuery;
        final CountDownLatch queryEntered = new CountDownLatch(1), releaseQuery = new CountDownLatch(1);
        int readPackets, processed;
        final InputStream input = new InputStream() {
            byte[] current; int at;
            public int read() { throw new AssertionError("use bounded reads"); }
            public int read(byte[] b, int off, int len) {
                try {
                    if (current != null && at == current.length) {
                        synchronized (Peer.this) { processed++; Peer.this.notifyAll(); }
                        current = null;
                    }
                    if (current == null) {
                        current = incoming.poll(5, TimeUnit.SECONDS); at = 0;
                        if (current == null) throw new AssertionError("socket timeout");
                    }
                    if (current.length == 0) return -1;
                    b[off] = current[at++]; // deliberately fragmented packets
                    if (at == current.length) synchronized (Peer.this) { readPackets++; Peer.this.notifyAll(); }
                    return 1;
                } catch (InterruptedException e) { throw new AssertionError(e); }
            }
        };
        public void setTcpNoDelay(boolean on) { }
        public void setKeepAlive(boolean on) { }
        public InputStream getInputStream() { return input; }
        public OutputStream getOutputStream() { return new OutputStream() {
            public void write(int b) { throw new AssertionError("whole packet expected"); }
            public void write(byte[] b) {
                if ((b[0] & 255) == 2 && blockQuery) {
                    blockQuery = false;
                    queryEntered.countDown();
                    try { check(releaseQuery.await(3,TimeUnit.SECONDS), "blocked writer timeout"); }
                    catch (InterruptedException e) { throw new AssertionError(e); }
                }
                outgoing.add((byte[])b.clone());
            }
        }; }
        public synchronized void close() { if (!closed) { closed = true; incoming.add(new byte[0]); } }
        void send(int event, int epoch) { incoming.add(new byte[]{(byte)event,(byte)(epoch>>>16),(byte)(epoch>>>8),(byte)epoch}); }
        synchronized void await(int count, boolean applied) throws Exception {
            long until = System.currentTimeMillis() + 3000;
            while ((applied ? processed : readPackets) < count) {
                long left = until - System.currentTimeMillis();
                check(left > 0, "reader barrier timeout"); wait(left);
            }
        }
        void expectQuery(int epoch) throws Exception {
            long until = System.currentTimeMillis() + 3000;
            while (System.currentTimeMillis() < until) {
                byte[] b = outgoing.poll(100, TimeUnit.MILLISECONDS);
                if (b == null || (b[0] & 255) != 2) continue;
                int got = ((b[1]&255)<<16)|((b[2]&255)<<8)|(b[3]&255);
                check(got == epoch, "wrong request epoch: " + got + " != " + epoch); return;
            }
            throw new AssertionError("missing readiness request");
        }
    }
    private static Thread serve(final Peer peer) throws Exception {
        Method install = AltscreenControlServer.class.getDeclaredMethod("installClient", Socket.class);
        install.setAccessible(true);
        final int gen = ((Integer)install.invoke(null, peer)).intValue();
        Thread t = new Thread(new Runnable() { public void run() {
            try {
                Method m = AltscreenControlServer.class.getDeclaredMethod("serveClient", Socket.class, Integer.TYPE);
                m.setAccessible(true); m.invoke(null, peer, Integer.valueOf(gen));
            } catch (Exception e) { throw new AssertionError(e); }
        }}); t.start(); return t;
    }
    private static void begin(AltScreenModule module) throws Exception {
        synchronized (field(AltScreenModule.class,"LOCK").get(null)) {
            field(AltScreenModule.class,"enabled").setBoolean(module,true);
            field(AltScreenModule.class,"connected").setBoolean(null,true);
            Method m = AltScreenModule.class.getDeclaredMethod("beginRendererSessionLocked");
            m.setAccessible(true); m.invoke(null);
        }
    }
    public static void main(String[] args) throws Exception {
        Log.setLevel(-1);
        field(AltscreenControlServer.class,"run").setBoolean(null,true);
        Thread writer = new Thread(new Runnable() { public void run() {
            try { Method m=AltscreenControlServer.class.getDeclaredMethod("writerLoop");m.setAccessible(true);m.invoke(null); }
            catch(Exception e){throw new AssertionError(e);}
        }}); writer.start();
        AltScreenModule module = new AltScreenModule();
        // Exercise all three epoch bytes and cache before a peer exists.
        field(AltScreenModule.class,"rendererEpoch").setInt(null,0x123455);
        begin(module); int first=epoch();
        Peer peer=new Peer(); Thread reader=serve(peer);
        peer.expectQuery(first);
        int n=0;
        peer.send(0x83,first);peer.await(++n,true);state(true,false);
        peer.send(0x81,first);peer.await(++n,true);state(true,true);
        module.stop();state(false,false);
        for(int event:new int[]{0x81,0x83}) {
            peer.send(event,first);peer.await(++n,true);state(false,false);
        }
        begin(module);int second=epoch();check(second!=first,"epoch reused");
        peer.expectQuery(second);state(false,false);
        // A delayed old reply after restart must not satisfy the new request.
        for(int event:new int[]{0x81,0x83,0x82}) {
            peer.send(event,first);peer.await(++n,true);state(false,false);
        }
        peer.send(0x81,0);peer.await(++n,true);state(false,false);
        peer.send(0x83,second);peer.await(++n,true);state(true,false);
        peer.send(0x81,second);peer.await(++n,true);state(true,true);
        peer.send(0x82,first);peer.await(++n,true);state(true,true); // stale OFF cannot hide new video
        peer.send(0x82,second);peer.await(++n,true);state(false,false);
        peer.send(0x81,second);peer.await(++n,true);state(true,true); // surviving stream bounce
        // A blocked write can finish after a newer start. The old response must
        // lose, and the coalesced latest request must still reach the peer.
        peer.blockQuery=true;begin(module);int blocked=epoch();
        check(peer.queryEntered.await(3,TimeUnit.SECONDS),"writer did not block");
        module.stop();begin(module);second=epoch();state(false,false);
        peer.releaseQuery.countDown();peer.expectQuery(blocked);peer.expectQuery(second);
        peer.send(0x81,blocked);peer.await(++n,true);state(false,false);
        peer.send(0x81,second);peer.await(++n,true);state(true,true);
        // Stop between socket read and publication, using the actual event lock.
        synchronized(ctl("EVENT_LOCK")) {
            peer.send(0x83,second);peer.await(++n,false);module.stop();
        }
        peer.await(n,true);state(false,false);
        begin(module);int third=epoch();peer.expectQuery(third);
        Peer fresh=new Peer();Thread freshReader;
        synchronized(ctl("EVENT_LOCK")) {
            peer.send(0x81,third);peer.await(++n,false);
            freshReader=serve(fresh); // same epoch, different peer: old callback must lose
        }
        fresh.expectQuery(third);
        fresh.send(0x83,third);fresh.await(1,true);state(true,false);
        reader.join(3000);check(!reader.isAlive(),"old reader leaked");state(true,false);
        fresh.close();freshReader.join(3000);check(!freshReader.isAlive(),"fresh reader leaked");state(false,false);
        synchronized(ctl("LOCK")) {
            field(AltscreenControlServer.class,"run").setBoolean(null,false);ctl("LOCK").notifyAll();
        }
        writer.join(3000);check(!writer.isAlive(),"writer leaked");
        System.out.println("AltscreenControlTransportTest: real lifecycle, same-peer stale ON/LOADER/OFF, atomic stop, request wire, late peer and replacement PASS");
    }
}
