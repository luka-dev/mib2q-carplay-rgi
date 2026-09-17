package com.luka.carplay.rgd;

import com.luka.carplay.framework.Log;

/** Actual presentation worker: Time is temporary even without incoming RGI. */
public final class RouteInfoTimeoutTest {
    private static final String NEXT = "International Airport via Washington Boulevard and Main Street";
    private static void check(boolean ok, String why) { RgiDeliveryRecoveryTest.check(ok, why); }
    private static Object get(Object obj, String field) throws Exception { return RgiDeliveryRecoveryTest.get(obj, field); }
    private static void set(Object obj, String field, Object value) throws Exception { RgiDeliveryRecoveryTest.set(obj, field, value); }
    private static void ok(CurrentPositionDeliveryTest.Fixture f) throws Exception {
        RgiDeliveryRecoveryTest.method(RouteGuidance.class, "requestInfoModeToggle").invoke(f.rg);
    }
    private static long deadline(CurrentPositionDeliveryTest.Fixture f) throws Exception {
        synchronized (get(f.rg, "presentationLock")) { return (Long) get(f.rg, "infoReturnDeadline"); }
    }
    private static void waitFor(CurrentPositionDeliveryTest.Fixture f, boolean time) throws Exception {
        long until = System.nanoTime() + 1800000000L;
        while (System.nanoTime() < until) {
            synchronized (f.rg) {
                if (f.out.text.startsWith("◌ ") == time
                    && ((Integer) get(f.bridge, "infoPhase")) == (time ? 1 : 0)) return;
            }
            Thread.sleep(5);
        }
        throw new AssertionError("worker did not select " + (time ? "Time" : "road text"));
    }
    private static void shortenDeadline(CurrentPositionDeliveryTest.Fixture f) throws Exception {
        Object lock = get(f.rg, "presentationLock");
        synchronized (lock) {
            // Keep a real timed wait, without making each host test sleep 20 s.
            set(f.rg, "infoReturnDeadline", System.currentTimeMillis() + 100);
            set(f.rg, "presentationWake", true);
            lock.notifyAll();
        }
    }
    private static void autoReturn(boolean failReturn) throws Exception {
        CurrentPositionDeliveryTest.Fixture f = new CurrentPositionDeliveryTest.Fixture();
        f.feed("time_remaining_seconds:n:600\n");
        long before = System.currentTimeMillis();
        ok(f);
        CurrentPositionDeliveryTest.Worker worker = new CurrentPositionDeliveryTest.Worker(f);
        worker.start();
        try {
            waitFor(f, true);
            long expiry = deadline(f);
            check(expiry >= before + 20000 && expiry <= System.currentTimeMillis() + 20000,
                "Time hold is not 20 seconds after publication");
            // The latest exit is cached while Time is showing; it must be the
            // text restored later. Neither fresh ETA nor replay extends Time.
            synchronized (f.rg) {
                f.feed("m0_exit_info:s:" + NEXT + "\ntime_remaining_seconds:n:599\n");
                check(!f.replay(), "cached Time replay failed");
                check(deadline(f) == expiry, "route delta/replay extended Time");
                if (failReturn) f.out.failures = 1;
            }
            shortenDeadline(f);
            waitFor(f, false);
            synchronized (f.rg) {
                check(f.out.text.startsWith("‹International"), "timeout restored obsolete route text");
                check(deadline(f) == 0L && ((Integer) get(f.rg, "desiredInfoPhase")) == 0,
                    "timeout left Time selected/armed");
                check(f.deadline() >= System.currentTimeMillis() + 1000,
                    "automatic return did not restart scroll at its first hold");
            }
        } finally { worker.finish(); }
    }
    private static void manualReturnAndView() throws Exception {
        CurrentPositionDeliveryTest.Fixture f = new CurrentPositionDeliveryTest.Fixture();
        f.feed("time_remaining_seconds:n:600\n");
        CurrentPositionDeliveryTest.Worker worker = new CurrentPositionDeliveryTest.Worker(f);
        ok(f); worker.start();
        try {
            waitFor(f, true);
            ok(f); waitFor(f, false);
            check(deadline(f) == 0L, "manual OK return retained timeout");
            long before = System.currentTimeMillis();
            ok(f); waitFor(f, true);
            check(deadline(f) >= before + 20000, "second Time entry reused old timeout");
            RgiDeliveryRecoveryTest.method(RouteGuidance.class, "requestViewAreaRefresh", int.class).invoke(f.rg, 0);
            waitFor(f, false);
            check(deadline(f) == 0L, "View retained timeout");
        } finally { worker.finish(); }
    }
    private static void failedTimeEntry() throws Exception {
        CurrentPositionDeliveryTest.Fixture f = new CurrentPositionDeliveryTest.Fixture();
        f.feed("time_remaining_seconds:n:600\n");
        f.out.failures = 1;
        int attempts = f.out.attempts;
        ok(f);
        CurrentPositionDeliveryTest.Worker worker = new CurrentPositionDeliveryTest.Worker(f);
        worker.start();
        try {
            long until = System.nanoTime() + 1000000000L;
            while (f.out.attempts == attempts && System.nanoTime() < until) Thread.sleep(5);
            synchronized (f.rg) {
                check(f.out.attempts == attempts + 1 && deadline(f) == 0L,
                    "failed Time publication started its hold");
            }
            long beforeRetry = System.currentTimeMillis();
            waitFor(f, true);
            check(deadline(f) >= beforeRetry + 20000, "retry lost part of the Time hold");
        } finally { worker.finish(); }
    }
    private static void clockAndBoundary() throws Exception {
        CurrentPositionDeliveryTest.Fixture f = new CurrentPositionDeliveryTest.Fixture();
        java.lang.reflect.Method wait = RgiDeliveryRecoveryTest.method(RouteGuidance.class, "infoReturnWait", long.class);
        Object lock = get(f.rg, "presentationLock");
        synchronized (lock) {
            set(f.rg, "desiredInfoPhase", 1); set(f.rg, "infoReturnDeadline", 120000L);
            check((Long) wait.invoke(f.rg, 100000L) == 20000L, "full hold boundary");
            check((Long) wait.invoke(f.rg, 119999L) == 1L, "early auto-return");
            check((Long) wait.invoke(f.rg, 120000L) == 0L, "missed deadline");
            check((Long) wait.invoke(f.rg, 50000L) == 20000L, "clock correction stranded Time");
            set(f.rg, "desiredInfoPhase", 0);
            check((Long) wait.invoke(f.rg, 50000L) == -1L, "normal mode kept a timer");
        }
    }
    public static void main(String[] args) throws Exception {
        Log.setLevel(-1);
        autoReturn(false); autoReturn(true); manualReturnAndView(); failedTimeEntry(); clockAndBoundary();
        System.out.println("RouteInfoTimeoutTest: 20-second Time hold, idle-worker auto-return, latest route, scroll restart, failures, OK/View cancellation and clock correction PASS");
    }
}
