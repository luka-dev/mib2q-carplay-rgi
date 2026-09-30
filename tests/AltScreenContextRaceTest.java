package com.luka.carplay.core;

import com.luka.carplay.framework.Log;
import de.audi.atip.hmi.view.IDisplayManager;
import java.lang.reflect.Field;
import java.lang.reflect.InvocationHandler;
import java.lang.reflect.Method;
import java.lang.reflect.Proxy;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

/** Shipping context-switch code, with lifecycle changes while target IPC is in flight. */
public final class AltScreenContextRaceTest {
    private static void check(boolean value, String why) {
        if (!value) throw new AssertionError(why);
    }
    private static Field field(String name) throws Exception {
        Field f = AltScreenModule.class.getDeclaredField(name);
        f.setAccessible(true);
        return f;
    }
    private static final class Display implements InvocationHandler {
        final List<Integer> writes = new ArrayList<Integer>();
        final List<Integer> rates = new ArrayList<Integer>();
        final CountDownLatch entered = new CountDownLatch(1);
        final CountDownLatch release = new CountDownLatch(1);
        int heldContext = -1;
        int current;
        int rate = 30;

        public Object invoke(Object proxy, Method method, Object[] args) throws Exception {
            String name = method.getName();
            if ("switchContext".equals(name)) {
                current = ((Integer) args[0]).intValue();
                writes.add(Integer.valueOf(current));
                if (current == heldContext) {
                    entered.countDown();
                    check(release.await(3, TimeUnit.SECONDS), "target IPC release timeout");
                }
            } else if ("setUpdateRate".equals(name)) {
                rate = ((Integer) args[1]).intValue();
                rates.add(Integer.valueOf(rate));
            } else if ("getCurrentContextID".equals(name)) {
                return Integer.valueOf(current);
            }
            return null;
        }
        IDisplayManager manager() {
            return (IDisplayManager) Proxy.newProxyInstance(
                IDisplayManager.class.getClassLoader(), new Class[]{IDisplayManager.class}, this);
        }
        void expectWrites(int... contexts) {
            check(writes.size() == contexts.length, "unexpected context trace " + writes);
            for (int i = 0; i < contexts.length; i++)
                check(writes.get(i).intValue() == contexts[i], "context trace " + writes);
        }
    }
    private static final class Fixture {
        final AltScreenModule module = new AltScreenModule();
        final Display display = new Display();
        final IDisplayManager manager = display.manager();
        final Object lock;
        final Method apply;

        Fixture(int current, int desired) throws Exception {
            lock = field("LOCK").get(null);
            apply = AltScreenModule.class.getDeclaredMethod("applySwitch", Integer.TYPE, IDisplayManager.class);
            apply.setAccessible(true);
            display.current = current;
            synchronized (lock) {
                field("dm").set(module, manager);
                field("currentCtx").setInt(null, current);
                field("clusterActive").setBoolean(null, current == 80 || current == 81);
                field("connected").setBoolean(null, desired != 74);
                field("videoAvailable").setBoolean(null, false);
                field("rendererReady").setBoolean(null, true);
                field("desiredCtx").setInt(null, desired);
            }
        }
        void publish(int desired, boolean ready) throws Exception {
            synchronized (lock) {
                field("connected").setBoolean(null, desired != 74);
                field("desiredCtx").setInt(null, desired);
                field("rendererReady").setBoolean(null, ready);
                lock.notifyAll();
            }
        }
        void apply(int target, IDisplayManager dm) throws Exception {
            apply.invoke(module, Integer.valueOf(target), dm);
        }
        void expectState(int current, boolean active) throws Exception {
            synchronized (lock) {
                check(field("currentCtx").getInt(null) == current, "cached context is stale");
                check(field("clusterActive").getBoolean(null) == active, "cluster activity is stale");
            }
        }
        void supersede(final int target, String scenario) throws Exception {
            display.heldContext = target;
            final Throwable[] failure = new Throwable[1];
            Thread ipc = new Thread(new Runnable() {
                public void run() {
                    try { apply(target, manager); }
                    catch (Throwable t) { failure[0] = t; }
                }
            }, "context-race-IPC");
            ipc.setDaemon(true);
            ipc.start();
            try {
                check(display.entered.await(3, TimeUnit.SECONDS), "target IPC did not start");
                if ("peer".equals(scenario)) {
                    synchronized (lock) {
                        field("dm").set(module, new Display().manager());
                    }
                } else {
                    publish("stop".equals(scenario) ? 74 : 81, !"readiness".equals(scenario));
                }
            } finally {
                display.release.countDown();
                ipc.join(3000);
            }
            check(!ipc.isAlive() && failure[0] == null, "target IPC worker failed or leaked");
            expectState(-1, false);
        }
    }
    public static void main(String[] args) throws Exception {
        Log.setLevel(-1);
        String scenario = args[0];
        if ("direct".equals(scenario)) {
            Fixture f = new Fixture(81, 80);
            f.apply(80, f.manager);
            f.expectState(80, true);
            f.publish(81, true);
            f.apply(81, f.manager);
            f.expectState(81, true);
            f.display.expectWrites(80, 81);
        } else if ("reconnect".equals(scenario)) {
            Fixture f = new Fixture(81, 74);
            f.supersede(74, scenario);
            check(f.display.rate == 30, "superseded stock switch lost frame-rate restoration");
            f.apply(81, f.manager);
            f.expectState(81, true);
            f.display.expectWrites(74, 72, 81);
        } else {
            int target = "change".equals(scenario) ? 80 : 81;
            Fixture f = new Fixture("change".equals(scenario) ? 81 : 74, target);
            f.supersede(target, scenario);
            check(f.display.rates.isEmpty(), "superseded target published an active update rate");
            if ("peer".equals(scenario)) {
                Display replacement = new Display();
                IDisplayManager dm = replacement.manager();
                synchronized (f.lock) { field("dm").set(f.module, dm); }
                f.apply(81, dm);
                f.expectState(81, true);
                replacement.expectWrites(72, 81);
                f.display.expectWrites(72, 81);
            } else {
                int desired = "stop".equals(scenario) ? 74 : 81;
                f.publish(desired, true);
                f.apply(desired, f.manager);
                f.expectState(desired, desired != 74);
                if ("change".equals(scenario)) f.display.expectWrites(80, 72, 81);
                else if ("stop".equals(scenario)) f.display.expectWrites(72, 81, 74);
                else f.display.expectWrites(72, 81, 72, 81);
            }
        }
        System.out.println("AltScreenContextRaceTest: " + scenario + " PASS");
    }
}
