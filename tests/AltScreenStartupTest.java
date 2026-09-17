package com.luka.carplay.core;

import com.luka.carplay.framework.Log;
import de.audi.atip.hmi.view.IDisplayManager;
import java.lang.reflect.Field;
import java.lang.reflect.InvocationHandler;
import java.lang.reflect.Method;
import java.lang.reflect.Proxy;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.LinkedBlockingQueue;
import java.util.concurrent.TimeUnit;

/** Exercise the shipping context worker with a cold/unavailable renderer. */
public final class AltScreenStartupTest {
    private static void check(boolean value, String why) {
        if (!value) throw new AssertionError(why);
    }
    private static Field field(String name) throws Exception {
        Field f = AltScreenModule.class.getDeclaredField(name);
        f.setAccessible(true);
        return f;
    }
    private static final class Display implements InvocationHandler {
        final LinkedBlockingQueue<Integer> writes = new LinkedBlockingQueue<Integer>();
        final CountDownLatch bounce = new CountDownLatch(1);
        final CountDownLatch release = new CountDownLatch(1);
        volatile boolean holdBounce;
        volatile int current = 74;
        volatile int rate = 30;

        public Object invoke(Object proxy, Method method, Object[] args) throws Exception {
            String name = method.getName();
            if ("switchContext".equals(name)) {
                current = ((Integer)args[0]).intValue();
                writes.add(Integer.valueOf(current));
                if (current == 72 && holdBounce) {
                    bounce.countDown();
                    check(release.await(3, TimeUnit.SECONDS), "bounce release timeout");
                }
            } else if ("setUpdateRate".equals(name)) {
                rate = ((Integer)args[1]).intValue();
            } else if ("getCurrentContextID".equals(name)) {
                return Integer.valueOf(current);
            }
            return null;
        }
        IDisplayManager manager() {
            return (IDisplayManager)Proxy.newProxyInstance(
                IDisplayManager.class.getClassLoader(), new Class[]{IDisplayManager.class}, this);
        }
        void expect(int context) throws Exception {
            Integer next = writes.poll(3, TimeUnit.SECONDS);
            check(next != null && next.intValue() == context,
                "expected context " + context + ", got " + next);
        }
        void expectQuiet() throws Exception {
            check(writes.poll(400, TimeUnit.MILLISECONDS) == null,
                "physical context changed before a renderer frame was ready");
        }
    }
    private static void publishStop() throws Exception {
        Object lock = field("LOCK").get(null);
        synchronized (lock) {
            field("connected").setBoolean(null, false);
            field("desiredCtx").setInt(null, 74);
            lock.notifyAll();
        }
    }
    public static void main(String[] args) throws Exception {
        Log.setLevel(-1);
        final AltScreenModule module = new AltScreenModule();
        Display display = new Display();
        String scenario = args[0];
        display.holdBounce = "bounce-loss".equals(scenario);
        field("dm").set(module, display.manager());
        field("connected").setBoolean(null, true);
        field("desiredCtx").setInt(null, 81);
        if ("warm".equals(scenario) || display.holdBounce)
            AltScreenModule.setRendererReady(true);
        final CountDownLatch started = new CountDownLatch(1);
        Thread worker = new Thread(new Runnable() {
            public void run() {
                try {
                    Method m = AltScreenModule.class.getDeclaredMethod("switchLoop");
                    m.setAccessible(true);
                    started.countDown();
                    m.invoke(module);
                } catch (Exception e) { throw new AssertionError(e); }
            }
        });
        worker.setDaemon(true);
        worker.start();
        check(started.await(3, TimeUnit.SECONDS), "worker start timeout");
        check(AltScreenModule.isConnected(), "ownership intent must precede readiness");
        check(AltScreenModule.consumeSteeringWheelScale(0), "roller pin lost during startup");

        if ("stop".equals(scenario)) {
            display.expectQuiet();
            publishStop();
            display.expect(74);
            display.expectQuiet();
            check(display.rate == 30, "stock frame rate was not restored");
        } else {
            if ("cold".equals(scenario)) {
                display.expectQuiet();
                check(!field("videoAvailable").getBoolean(null), "unexpected video");
                AltScreenModule.setRendererReady(true);
            } else if (display.holdBounce) {
                check(display.bounce.await(3, TimeUnit.SECONDS), "bounce did not start");
                display.expect(72);
                AltScreenModule.setRendererReady(false);
                display.holdBounce = false;
                display.release.countDown();
                display.expectQuiet();
                AltScreenModule.setRendererReady(true);
            }
            display.expect(72);
            display.expect(81);
            check(!field("videoAvailable").getBoolean(null), "PRISM required decoded video");
            display.expectQuiet();
            AltScreenModule.setRendererReady(false);
            publishStop();
            display.expect(74);
            display.expectQuiet();
            check(!AltScreenModule.isConnected() && display.rate == 30, "release failed");
        }
        System.out.println("AltScreenStartupTest: " + scenario + " PASS");
    }
}
