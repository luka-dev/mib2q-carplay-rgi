package com.luka.carplay.rgd;

import com.luka.carplay.framework.Log;
import de.audi.atip.interapp.combi.bap.navi.CombiBAPServiceNavi;
import java.lang.reflect.InvocationHandler;
import java.lang.reflect.Method;
import java.lang.reflect.Proxy;

/** Real bridge + presentation worker, only BAP/renderer endpoints substituted. */
public final class CurrentPositionDeliveryTest {
    private static final String ROAD = "North Pennsylvania Avenue via Washington Boulevard and Main Street";
    private static void check(boolean ok, String why) { RgiDeliveryRecoveryTest.check(ok, why); }
    private static void set(Object obj, String key, Object value) throws Exception { RgiDeliveryRecoveryTest.set(obj, key, value); }
    private static Object get(Object obj, String key) throws Exception { return RgiDeliveryRecoveryTest.get(obj, key); }
    private static Object call(Object obj, String name) throws Exception {
        return RgiDeliveryRecoveryTest.method(obj.getClass(), name).invoke(obj);
    }
    static class Output implements InvocationHandler {
        volatile int textWrites, otherWrites, attempts, timeWrites, timeAttempts;
        int failures;
        String failMethod;
        int otherFailures;
        volatile String text;
        final java.util.List<String> texts = new java.util.ArrayList<String>();
        public Object invoke(Object p, Method m, Object[] args) {
            if (m.getName().equals("updateCurrentPositionInfo")) {
                attempts++;
                if (failures-- > 0) throw new IllegalStateException("injected text queue failure");
                text = (String) args[0];
                texts.add(text);
                textWrites++;
            } else {
                if (m.getName().equals("updateTimeToDestination")) timeAttempts++;
                if (m.getName().equals(failMethod) && otherFailures-- > 0)
                    throw new IllegalStateException("injected " + failMethod + " failure");
                if (m.getName().equals("updateTimeToDestination")) timeWrites++;
                otherWrites++;
            }
            return null;
        }
    }
    static final class Worker extends Thread {
        final Fixture f;
        volatile Throwable failure;
        Worker(Fixture fixture) { super("ScrollIntegrationTest"); f = fixture; }
        public void run() {
            try { RgiDeliveryRecoveryTest.method(RouteGuidance.class, "presentationLoop", int.class).invoke(f.rg, 0); }
            catch (Throwable t) { failure = t; }
        }
        void finish() throws Exception {
            Object lock = get(f.rg, "presentationLock");
            synchronized (lock) { set(f.rg, "running", false); lock.notifyAll(); }
            join(2000);
            check(!isAlive() && failure == null, "integration worker leaked/failed: " + failure);
        }
    }
    private static void blockedRetiredWorker() throws Exception {
        Fixture f = new Fixture();
        set(f.scroll(), "deadline", System.currentTimeMillis() - 1);
        Worker w = new Worker(f);
        int writes = f.out.textWrites;
        try {
            synchronized (f.rg) {
                w.start();
                long end = System.nanoTime() + 1000000000L;
                while (w.getState() != Thread.State.BLOCKED && System.nanoTime() < end) Thread.sleep(5);
                check(w.getState() == Thread.State.BLOCKED, "worker did not reach serialized publication");
                Object lock = get(f.rg, "presentationLock");
                synchronized (lock) { set(f.rg, "presentationGeneration", 1); }
            }
            w.join(1000);
            check(!w.isAlive() && f.out.textWrites == writes, "retired worker sent after waiting for route monitor");
        } finally { w.finish(); }
    }
    private static void infoFailureRecovery() throws Exception {
        // A failure in each part of the View/OK transaction must retry even
        // when the replacement text fits and no new iOS delta ever arrives.
        String[] failures = {"updateTurnToInfo", "updateCurrentPositionInfo", "updateTimeToDestination"};
        for (int i = 0; i < failures.length; i++) {
            Fixture f = new Fixture();
            f.feed("time_remaining_seconds:n:600\n");
            int timeWrites = f.out.timeWrites;
            if (i == 1) f.out.failures = 1;
            else { f.out.failMethod = failures[i]; f.out.otherFailures = 1; }
            call(f.rg, "requestInfoModeToggle");
            Worker w = new Worker(f);
            w.start();
            try {
                long end = System.nanoTime() + 1600000000L;
                while (f.out.timeWrites == timeWrites && System.nanoTime() < end) Thread.sleep(5);
                check(f.out.timeWrites > timeWrites && f.out.text.startsWith("◌ "),
                    "View/OK transaction did not recover after " + failures[i]);
                check(f.out.timeAttempts <= timeWrites + 2, "View/OK retry spun");
            } finally { w.finish(); }
        }
    }
    private static void routeEndResetsInfo() throws Exception {
        String[] ends = {"route_state:n:0\n", "disconnect_reason:s:test-disconnect\n"};
        for (int i = 0; i < ends.length; i++) {
            Fixture f = new Fixture();
            call(f.rg, "requestInfoModeToggle");
            check(((Integer) get(f.rg, "desiredInfoPhase")) == 1, "toggle fixture");
            set(f.rg, "infoReturnDeadline", System.currentTimeMillis() + 20000);
            // Avoid launching/stopping a real host renderer in this route-end probe.
            set(f.bridge, "customRendererStarted", false);
            f.feed(ends[i]);
            check(((Integer) get(f.rg, "desiredInfoPhase")) == 0
                && !((Boolean) get(f.rg, "infoPresentationRefreshPending"))
                && ((Long) get(f.rg, "infoReturnDeadline")) == 0L,
                "ended route retained queued OK phase: " + ends[i]);
            int writes = f.out.textWrites;
            f.bridge.tickPositionScroll(System.currentTimeMillis() + 3000);
            check(f.out.textWrites == writes && f.bridge.positionScrollWait(System.currentTimeMillis()) < 0,
                "ended route retained text timer");
        }
    }
    private static void queuedViewBeforeDueTick() throws Exception {
        Fixture f = new Fixture();
        String first = f.out.text;
        f.tick();
        set(f.scroll(), "deadline", System.currentTimeMillis() - 1);
        RgiDeliveryRecoveryTest.method(RouteGuidance.class, "requestViewAreaRefresh", int.class).invoke(f.rg, 0);
        int writes = f.out.textWrites;
        Worker w = new Worker(f); w.start();
        try {
            long end = System.nanoTime() + 1000000000L;
            while (f.out.textWrites == writes && System.nanoTime() < end) Thread.sleep(5);
            synchronized (f.rg) {
                check(f.out.textWrites == writes + 1 && first.equals(f.out.texts.get(writes)),
                    "due old fragment preceded queued View reset");
                check(f.deadline() > System.currentTimeMillis() + 1000, "View lost first-fragment hold");
            }
        } finally { w.finish(); }
    }
    private static void firstPublicationRecovery() throws Exception {
        Fixture f = new Fixture();
        set(f.bridge, "routeTextPublished", false);
        set(f.rg, "presentationConfirmed", false);
        int writes = f.out.textWrites;
        int attempts = f.out.attempts;
        f.out.failures = 2;
        f.feed("m0_exit_info:s:International Airport via Washington Boulevard and Main Street\n");
        Worker w = new Worker(f); w.start();
        try {
            long end = System.nanoTime() + 1600000000L;
            while (f.out.textWrites == writes && System.nanoTime() < end) Thread.sleep(5);
            synchronized (f.rg) {
                check(f.out.text.startsWith("International") && f.confirmed() && f.state.dirtyMask == 0,
                    "failed initial publication did not recover cached text");
                check(f.out.attempts - attempts == 3, "initial-publication retry spun");
                check(f.deadline() > System.currentTimeMillis() + 1000, "initial recovery lost first hold");
            }
        } finally { w.finish(); }
    }
    private static void coldReadyBeforeWorker() throws Exception {
        // FRAME_READY can arrive before the worker ever observes the not-ready
        // state. Its cached text was sent while context 80 was still hidden.
        for (int mode = 0; mode < 2; mode++) {
            Fixture f = new Fixture();
            String first = f.out.text;
            set(f.scroll(), "deadline", System.currentTimeMillis() - 1);
            set(f.rg, "presentationConfirmed", false);
            if (mode == 0) {
                set(f.rg, "presentationDrivePending", true);
                int writes = f.out.textWrites;
                Worker w = new Worker(f); w.start();
                try {
                    long end = System.nanoTime() + 1000000000L;
                    while (f.out.textWrites == writes && System.nanoTime() < end) Thread.sleep(5);
                    synchronized (f.rg) {
                        check(f.confirmed() && first.equals(f.out.text)
                            && f.deadline() > System.currentTimeMillis() + 1000,
                            "cached FRAME_READY replay consumed the hidden first hold");
                    }
                } finally { w.finish(); }
            } else {
                // -2 (was 4999 pre-fix): the fixture's baseline distance is now -1, not 5000
                // (see Fixture's constructor comment); -2 is a genuine bus-level change from -1
                // that still keeps the formatted distance text empty (both <=0, both take the
                // "distance unusable" branch so approach-zone state is untouched either way), so
                // this still exercises exactly what it always did -- a live distance-only delta
                // must not disturb the held/unconfirmed first fragment or its hold deadline.
                f.feed("dist_maneuver_m:n:-2\n");
                check(f.confirmed() && first.equals(f.out.text)
                    && f.deadline() > System.currentTimeMillis() + 1000,
                    "live FRAME_READY update consumed the hidden first hold");
            }
        }
    }
    private static void integrationAudit() throws Exception {
        int failures = 0;
        try { blockedRetiredWorker(); } catch (AssertionError e) { failures++; System.err.println(e); }
        try { infoFailureRecovery(); } catch (AssertionError e) { failures++; System.err.println(e); }
        try { routeEndResetsInfo(); } catch (AssertionError e) { failures++; System.err.println(e); }
        try { queuedViewBeforeDueTick(); } catch (AssertionError e) { failures++; System.err.println(e); }
        try { firstPublicationRecovery(); } catch (AssertionError e) { failures++; System.err.println(e); }
        try { coldReadyBeforeWorker(); } catch (AssertionError e) { failures++; System.err.println(e); }
        check(failures == 0, "scroll integration regressions: " + failures);
    }
    static class Fixture extends RgiDeliveryRecoveryTest.Fixture {
        final Output out = new Output();
        Fixture() throws Exception {
            set(bridge, "appConnectorNavi", Proxy.newProxyInstance(getClass().getClassLoader(),
                new Class[] {CombiBAPServiceNavi.class}, out));
            /* The base RgiDeliveryRecoveryTest.Fixture leaves dist_maneuver_m=5000 (from its own
             * constructor feed). Under the exit-arrow/truncation fix (BAPBridge.updateLatchedRouteText),
             * any POSITIVE distance alongside a signpost now adds a turn-arrow prefix and ellipsis-
             * truncates the name to fit one page -- so a positive distance here would never reach what
             * this fixture actually exists to exercise: the scrolling multi-page path for a long,
             * untruncated road name. Set the distance to -1 (not 0) in the same feed that sets the
             * signpost, so distStr stays "" (BAPBridge only adds the arrow/" | <dist>" suffix when
             * distManeuverM > 0) and this fixture keeps hitting plain scrolling exactly like before the
             * fix -- -1 specifically because distM=0 satisfies hasUsableDistance=(distM>=0) and would
             * force BAPBridge.update()'s approach-zone entry (0 <= the 457 m display threshold), which
             * the original 5000 m baseline never did; -1 takes the "distance unusable" branch instead,
             * which falls back to the (still-false) previous approach-zone state, so this fixture enters
             * approach zone exactly as often as the original one did: never.
             * distM<=0 does not clear the maneuver here: shouldClearManeuver/explicitClear both require
             * maneuverCount==0 (still 1 from the base fixture), so the maneuver and signpost stay latched
             * -- only the *displayed distance text* becomes empty. See roadSignpostWithDistanceSinglePage()
             * below for the companion case (signpost WITH a distance -> arrow + truncated single page). */
            feed("dist_maneuver_m:n:-1\nm0_exit_info:s:" + ROAD + "\n");
            check(out.text != null && out.text.startsWith("North"), "long-road fixture");
        }
        CurrentPositionScroll scroll() throws Exception { return (CurrentPositionScroll) get(bridge, "positionScroll"); }
        long deadline() throws Exception { return (Long) get(scroll(), "deadline"); }
        void tick() throws Exception { bridge.tickPositionScroll(deadline()); }
    }
    private static void directDelivery() throws Exception {
        Fixture f = new Fixture();
        String first = f.out.text;
        long deadline = f.deadline();
        int other = f.out.otherWrites;
        int renderer = f.renderer.writes;
        Object plan = get(f.scroll(), "starts");
        f.bridge.tickPositionScroll(deadline - 1);
        check(f.out.text.equals(first), "early text tick");
        f.tick();
        check(!f.out.text.equals(first), "overflow did not advance");
        check(f.out.otherWrites == other && f.renderer.writes == renderer, "text tick replayed unrelated output");
        String second = f.out.text;
        deadline = f.deadline();
        // -2 (was 4999 pre-fix, against a 5000 baseline in the same bucket): the fixture's
        // baseline distance is now -1 so the signpost line has no arrow/suffix (see Fixture's
        // constructor comment). -2 is a real value change (-1 -> -2) whose formatted distance
        // text is still empty both before and after, so the scroll plan/deadline must stay
        // byte-identical (same `starts` array instance) -- the same invariant the original
        // 5000->4999 same-bucket delta was checking, just relocated to the non-positive range
        // that now keeps this signpost's text free of a distance suffix.
        f.feed("dist_maneuver_m:n:-2\n");
        check(f.deadline() == deadline && get(f.scroll(), "starts") == plan, "distance changed scroll plan/deadline");
        f.feed("m0_exit_info:s:" + ROAD + "\n");
        check(f.deadline() == deadline && f.out.text.equals(second), "duplicate source restarted scroll");
        check(!f.replay() && f.deadline() == deadline && f.out.text.equals(second), "cached replay restarted scroll");
        f.out.failures = 1;
        int writes = f.out.textWrites;
        f.tick();
        check(f.out.textWrites == writes && f.deadline() == deadline + 500, "failed frame not retained for retry");
        f.bridge.tickPositionScroll(deadline + 499);
        check(f.out.textWrites == writes, "retry busy-loop");
        f.tick();
        check(f.out.textWrites == writes + 1, "retry did not converge");
        f.feed("m0_ver:n:3\nm0_type:n:1\nm0_exit_info:s:" + ROAD + "\n");
        check(f.out.text.equals(first), "same text in a new maneuver must restart");
        f.tick();
        f.renderer.ready = false;
        call(f.rg, "drivePositionScroll");
        writes = f.out.textWrites;
        f.bridge.tickPositionScroll(System.currentTimeMillis() + 3000);
        check(f.out.textWrites == writes && f.bridge.positionScrollWait(System.currentTimeMillis()) < 0,
            "renderer loss did not suspend text");
        f.renderer.ready = true;
        check(!f.replay() && f.out.text.equals(first), "renderer recovery did not restart first fragment");
        f.tick();
        f.bridge.refreshInfoPresentation(f.state, 0);
        check(f.out.text.equals(first), "View refresh did not restart");
        set(f.bridge, "lastEtaSeconds", System.currentTimeMillis() / 1000 + 300);
        f.bridge.refreshInfoPresentation(f.state, 1);
        check(f.out.text.startsWith("◌ "), "OK phase did not replace road");
        f.bridge.refreshInfoPresentation(f.state, 0);
        check(f.out.text.equals(first), "return from OK did not restart road");
        f.bridge.onStop();
        writes = f.out.textWrites;
        f.bridge.tickPositionScroll(System.currentTimeMillis() + 3000);
        check(f.out.textWrites == writes && f.bridge.positionScrollWait(System.currentTimeMillis()) < 0, "stale road after stop");
    }
    /**
     * Companion to the Fixture's plain-scrolling ROAD case above: the SAME long signpost,
     * but WITH a distance, must take the opposite path -- an exit-direction arrow prefix
     * (BAPBridge.getTurnArrowPrefix), the name ellipsis-truncated to fit one page
     * (CurrentPositionScroll.fitWithEllipsis), the pinned " | <dist>" suffix kept intact,
     * and no scrolling at all (single page, tick() never advances).
     */
    private static void roadSignpostWithDistanceSinglePage() throws Exception {
        Fixture f = new Fixture();
        // Base fixture's maneuver is mType[0]=1 (MT_LEFT_TURN), m0_junction_type=0, no
        // turn_angle present -> ManeuverMapper resolves DIR_LEFT -> BAPBridge.ARROW_LEFT.
        f.feed("dist_maneuver_m:n:800\n");
        String text = f.out.text;
        check(text.startsWith(BAPBridge.ARROW_LEFT),
            "signpost+distance line must carry the exit-direction arrow: " + text);
        check(text.indexOf("…") >= 0 || text.indexOf("...") >= 0,
            "signpost+distance line must be ellipsized: " + text);
        check(text.endsWith(" mi") || text.endsWith(" km") || text.endsWith(" m")
                || text.endsWith(" ft") || text.endsWith(" yd"),
            "signpost+distance line must end with the pinned \" | <dist>\" suffix: " + text);
        check(text.indexOf(" | ") > 0, "distance suffix separator present: " + text);
        int writes = f.out.textWrites;
        f.tick();
        check(f.out.textWrites == writes,
            "signpost+distance line must not advance on tick (single page, no scroll): " + text);

        // A same-bucket distance delta (800 -> 810 m; both format to "800 m" metric and
        // "0.5 mi" imperial) must not republish FctID 19 or disturb the single-page plan.
        String textBefore = f.out.text;
        int writesBefore = f.out.textWrites;
        Object planBefore = get(f.scroll(), "starts");
        long deadlineBefore = f.deadline();
        f.feed("dist_maneuver_m:n:810\n");
        check(f.out.text.equals(textBefore), "same-bucket distance delta changed the text: " + f.out.text);
        check(f.out.textWrites == writesBefore, "same-bucket distance delta republished FctID 19");
        check(get(f.scroll(), "starts") == planBefore, "same-bucket distance delta rebuilt the scroll plan");
        check(f.deadline() == deadlineBefore, "same-bucket distance delta moved the deadline");

        // A different-bucket distance (1500 m, still > 457 m so approach zone is unaffected;
        // "1.5 km" metric / "0.9 mi" imperial) MUST republish with new text -- guards against
        // the assertions above passing only because nothing is ever republished.
        f.feed("dist_maneuver_m:n:1500\n");
        String changed = f.out.text;
        check(!changed.equals(textBefore), "different-bucket distance delta did not change the text");
        check(changed.startsWith(BAPBridge.ARROW_LEFT), "different-bucket line lost the exit-direction arrow: " + changed);
        check(changed.endsWith(" mi") || changed.endsWith(" km") || changed.endsWith(" m")
                || changed.endsWith(" ft") || changed.endsWith(" yd"),
            "different-bucket line must still end with a distance-unit suffix: " + changed);
    }
    private static void worker() throws Exception {
        final Fixture f = new Fixture();
        // Expire the first hold without sleeping 1.8 s in a host test.
        set(f.scroll(), "deadline", System.currentTimeMillis() - 1);
        final Method loop = RgiDeliveryRecoveryTest.method(RouteGuidance.class, "presentationLoop", int.class);
        final Throwable[] failure = {null};
        Thread thread = new Thread(new Runnable() { public void run() {
            try { loop.invoke(f.rg, 0); } catch (Throwable t) { failure[0] = t; }
        }}, "CurrentPositionWorkerTest");
        int initial = f.out.textWrites, other = f.out.otherWrites;
        thread.start();
        try {
            long end = System.nanoTime() + 1500000000L;
            while (f.out.textWrites == initial && System.nanoTime() < end) Thread.sleep(5);
            check(f.out.textWrites > initial, "existing worker did not deliver text without incoming RGI");
            check(f.out.otherWrites == other, "worker text tick sent non-text BAP fields");
            synchronized (f.rg) {
                f.feed("m0_exit_info:s:Main Street\n");
            }
            Thread.sleep(300);
            int staticWrites = f.out.textWrites;
            Thread.sleep(300);
            check(f.out.textWrites == staticWrites, "fitting text kept publishing");
            check(thread.getState() == Thread.State.WAITING, "fitting text kept a timer awake");
        } finally {
            Object lock = get(f.rg, "presentationLock");
            synchronized (lock) { set(f.rg, "running", false); lock.notifyAll(); }
            thread.join(2000);
        }
        check(!thread.isAlive() && failure[0] == null, "text worker leaked/failed");
        // A retired worker generation must not send even an already-due frame.
        set(f.rg, "running", true);
        set(f.rg, "presentationGeneration", 1);
        f.feed("m0_exit_info:s:" + ROAD + "\n");
        set(f.scroll(), "deadline", System.currentTimeMillis() - 1);
        int writes = f.out.textWrites;
        loop.invoke(f.rg, 0);
        check(f.out.textWrites == writes, "retired generation emitted stale text");
    }
    public static void main(String[] args) throws Exception {
        Log.setLevel(-1);
        directDelivery(); roadSignpostWithDistanceSinglePage(); worker(); integrationAudit();
        System.out.println("CurrentPositionDeliveryTest: Fct19-only ticks, initial/View/OK retries, cached deltas, maneuver identity, View-before-tick ordering, readiness, route end/disconnect and blocked-worker retirement PASS");
    }
}
