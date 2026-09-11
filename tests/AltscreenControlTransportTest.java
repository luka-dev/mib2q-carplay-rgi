package com.luka.carplay.core;

import com.luka.carplay.framework.Log;
import java.io.InputStream;
import java.io.ByteArrayInputStream;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.net.Socket;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

public final class AltscreenControlTransportTest {
    private static void check(boolean yes, String why) { if (!yes) throw new AssertionError(why); }
    private static void await(CountDownLatch c) throws Exception { check(c.await(3, TimeUnit.SECONDS), "latch timeout"); }
    private static Field field(String name) throws Exception {
        Field f = AltscreenControlServer.class.getDeclaredField(name); f.setAccessible(true); return f;
    }
    private static final class Peer extends Socket {
        final boolean delayedClose;
        final CountDownLatch closing = new CountDownLatch(1), release = new CountDownLatch(1), closed = new CountDownLatch(1);
        final InputStream input;
        Peer(boolean old) {
            delayedClose = old;
            input = old ? new ByteArrayInputStream(new byte[0]) : new InputStream() {
                int at;
                public int read() {
                    if (at < 4) return at++ == 0 ? 0x81 : 0;
                    try { if (!closed.await(3, TimeUnit.SECONDS)) throw new AssertionError("peer tail timeout"); }
                    catch (InterruptedException e) { throw new AssertionError(e); }
                    return -1;
                }
            };
        }
        public void setTcpNoDelay(boolean on) { }
        public void setKeepAlive(boolean on) { }
        public InputStream getInputStream() { return input; }
        public void close() {
            closing.countDown();
            if (delayedClose) {
                try { await(release); } catch (Exception e) { throw new AssertionError(e); }
            }
            closed.countDown();
        }
    }
    private static Thread serve(final Peer peer, final int gen) {
        Thread t = new Thread(new Runnable() {
            public void run() {
                try {
                    Method m = AltscreenControlServer.class.getDeclaredMethod("serveClient", Socket.class, Integer.TYPE);
                    m.setAccessible(true); m.invoke(null, peer, Integer.valueOf(gen));
                } catch (Exception e) { throw new AssertionError(e); }
            }
        });
        t.start(); return t;
    }
    public static void main(String[] args) throws Exception {
        Log.setLevel(-1);
        Peer old = new Peer(true), fresh = new Peer(false);
        Object lock = field("LOCK").get(null);
        field("run").setBoolean(null, true);
        synchronized (lock) { field("client").set(null, old); field("clientGeneration").setInt(null, 1); }
        Thread oldReader = serve(old, 1); await(old.closing);
        synchronized (lock) { field("client").set(null, fresh); field("clientGeneration").setInt(null, 2); }
        Thread newReader = serve(fresh, 2); await(AltScreenModule.shown);
        old.release.countDown(); oldReader.join(3000);
        check(!oldReader.isAlive() && AltScreenModule.video, "old disconnect hid new video");
        fresh.close(); newReader.join(3000);
        check(!newReader.isAlive() && !AltScreenModule.video, "current disconnect did not hide video");
        field("run").setBoolean(null, false);
        System.out.println("AltscreenControlTransportTest: delayed old disconnect cannot override fresh VIDEO_ON PASS");
    }
}
