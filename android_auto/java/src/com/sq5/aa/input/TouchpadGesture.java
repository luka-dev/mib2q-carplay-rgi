/*
 * MMI touchpad gesture -> discrete navigation steps for Android Auto (SQ5 MHI2Q MU0918).
 *
 * Algorithm = luka-dev's TouchpadController (mib2q-carplay-rgi, java_patch/com/luka/carplay/input):
 * a single-finger drag accumulates signed dx/dy since the last step; when |dx| or |dy| crosses a
 * speed-adaptive threshold (fast finger 150, slow finger 200 units; per-sample speed in units per
 * 100 ms, dt clamped to 200 ms) one step is emitted and the threshold is subtracted.  A 30-unit
 * dead zone swallows jitter before the first step.  Differences to luka:
 *   - all distances are in a 1024-unit reference space and scaled to the pad's real resolution
 *     (MU0918 AbstractTerminalModeConfiguration.getSquareTouchPadResolution: 256 or 1024);
 *   - input is the stock touch-state stream (PRESSED 0 / RELEASED 1 / MOVED 2) with timestamps,
 *     so the engine is host-testable with a synthetic clock;
 *   - when one axis steps, the other axis' sub-threshold drift is dropped (a diagonal wobble on a
 *     vertical swipe must not later produce a stray left/right step in Android Auto);
 *   - at most MAX_STEPS_PER_SAMPLE steps per sample (a coordinate glitch cannot flood the phone);
 *   - a short tap (press+release, no step, little travel) emits SELECT (Android DPAD_CENTER).
 *
 * Java 1.4 (IBM J9): no generics/autoboxing/foreach/String.format.  Thread-safe (synchronized).
 */
package com.sq5.aa.input;

public final class TouchpadGesture {

    /** Receives one step; the caller turns it into a press+release pair. */
    public interface Sink {
        void step(int dir);
    }

    /* step directions */
    public static final int UP = 1;
    public static final int DOWN = 2;
    public static final int LEFT = 3;
    public static final int RIGHT = 4;
    public static final int SELECT = 5;

    /* de.audi.app.terminalmode.keyevents.TouchEvent states (MU0918: see TouchEvent constructors) */
    public static final int STATE_PRESSED = 0;
    public static final int STATE_RELEASED = 1;
    public static final int STATE_MOVED = 2;

    /* luka tuning, in reference units (1024 x 1024 pad) */
    static final int REF_RES = 1024;
    static final int THRESHOLD_FAST = 150;
    static final int THRESHOLD_SLOW = 200;
    static final int SPEED_FAST = 300;          /* ref units / 100 ms */
    static final int SPEED_SLOW = 100;
    static final int DEAD_ZONE = 30;
    /* additions */
    static final int MAX_STEPS_PER_SAMPLE = 4;
    static final long TAP_MAX_MS = 300;
    static final long GAP_REANCHOR_MS = 400;    /* sample gap => treat as a new touch-down */

    private Sink sink;
    private boolean tapEnabled = true;
    private int configuredRes;                  /* 0 = unknown */
    private int maxCoordSeen;

    /* per-gesture state */
    private boolean down;
    private boolean pressSeen;                  /* gesture started with a real PRESSED sample */
    private int lastX, lastY, downX, downY;
    private long downMs, lastMs;
    private int accX, accY;
    private boolean armed;
    private int maxTravel;
    private int gestureSteps;

    /* statistics for the caller's log line */
    private int nUp, nDown, nLeft, nRight, nSelect;

    public synchronized void setSink(Sink s) {
        reset();
        sink = s;
    }

    public synchronized void setTapEnabled(boolean on) {
        tapEnabled = on;
    }

    /** Touchpad resolution from ITerminalModeConfiguration (256 or 1024); <= 0 = auto. */
    public synchronized void setResolution(int res) {
        configuredRes = res > 0 ? res : 0;
    }

    /** Forgets the auto-detected coordinate range (host tests). */
    public synchronized void resetCalibration() {
        maxCoordSeen = 0;
    }

    /**
     * Effective resolution: the configured one, raised to 1024 as soon as a coordinate proves the
     * pad is larger; without configuration 256 until a coordinate above 255 is seen.
     */
    public synchronized int resolution() {
        int r = configuredRes > 0 ? configuredRes : 256;
        /* MU0918 only knows 256 and 1024; a larger coordinate is a glitch, not a bigger pad */
        if (maxCoordSeen >= r && r < REF_RES) r = REF_RES;
        return r;
    }

    private int scale(int refUnits) {
        int v = refUnits * resolution() / REF_RES;
        return v < 1 ? 1 : v;
    }

    /** Resolution auto-detection for samples that bypass onSample (non-Maps stroke path). */
    public synchronized void noteCoords(int x, int y) {
        if (x > maxCoordSeen) maxCoordSeen = x;
        if (y > maxCoordSeen) maxCoordSeen = y;
    }

    /** Drops any gesture in progress (session end, sink change, multi-finger). */
    public synchronized void reset() {
        down = false;
        pressSeen = false;
        accX = 0;
        accY = 0;
        armed = false;
        maxTravel = 0;
        gestureSteps = 0;
    }

    /** Steps emitted so far in the current/last gesture. */
    public synchronized int gestureSteps() {
        return gestureSteps;
    }

    /** "U/D/L/R/S" counters since the last call (for a compact per-gesture log line). */
    public synchronized String takeStats() {
        String s = "U" + nUp + " D" + nDown + " L" + nLeft + " R" + nRight + " S" + nSelect;
        nUp = nDown = nLeft = nRight = nSelect = 0;
        return s;
    }

    /**
     * One touchpad sample.
     * @return true when this sample ended a gesture (RELEASED), so the caller may log a summary.
     */
    public synchronized boolean onSample(int state, int x, int y, long now) {
        if (x > maxCoordSeen) maxCoordSeen = x;
        if (y > maxCoordSeen) maxCoordSeen = y;

        if (state == STATE_PRESSED) {
            anchor(x, y, now, true);
            return false;
        }
        if (state == STATE_RELEASED) {
            if (!down) {
                reset();
                return false;
            }
            move(x, y, now);
            boolean tap = tapEnabled && pressSeen && gestureSteps == 0
                && now - downMs <= TAP_MAX_MS && maxTravel < scale(DEAD_ZONE);
            if (tap) emit(SELECT);
            down = false;
            pressSeen = false;
            accX = 0;
            accY = 0;
            armed = false;
            return true;
        }
        /* MOVED (or any unknown state): anchor if no gesture or the stream stalled */
        if (!down || now - lastMs > GAP_REANCHOR_MS) {
            anchor(x, y, now, false);
            return false;
        }
        move(x, y, now);
        return false;
    }

    private void anchor(int x, int y, long now, boolean pressed) {
        down = true;
        pressSeen = pressed;
        lastX = downX = x;
        lastY = downY = y;
        downMs = lastMs = now;
        accX = 0;
        accY = 0;
        armed = false;
        maxTravel = 0;
        gestureSteps = 0;
    }

    private static int abs(int v) {
        return v < 0 ? -v : v;
    }

    private void move(int x, int y, long now) {
        int dx = x - lastX;
        int dy = y - lastY;
        lastX = x;
        lastY = y;
        int travel = abs(x - downX) > abs(y - downY) ? abs(x - downX) : abs(y - downY);
        if (travel > maxTravel) maxTravel = travel;
        accX += dx;
        accY += dy;

        if (!armed) {
            int dz = scale(DEAD_ZONE);
            if (abs(accX) < dz && abs(accY) < dz) {
                lastMs = now;
                return;
            }
            armed = true;
        }

        int dt = (int) (now - lastMs);
        if (dt <= 0) dt = 1;
        if (dt > 200) dt = 200;
        lastMs = now;
        int res = resolution();
        /* speed in reference units per 100 ms */
        int speed = (abs(dx) + abs(dy)) * 100 / dt * REF_RES / res;

        int thrRef;
        if (speed >= SPEED_FAST) {
            thrRef = THRESHOLD_FAST;
        } else if (speed <= SPEED_SLOW) {
            thrRef = THRESHOLD_SLOW;
        } else {
            int range = SPEED_FAST - SPEED_SLOW;
            int pos = speed - SPEED_SLOW;
            thrRef = THRESHOLD_FAST + (range - pos) * (THRESHOLD_SLOW - THRESHOLD_FAST) / range;
        }
        int thr = scale(thrRef);

        int steps = 0;
        while (steps < MAX_STEPS_PER_SAMPLE) {
            int ax = abs(accX);
            int ay = abs(accY);
            if (ax < thr && ay < thr) break;
            if (ax >= ay) {
                if (accX < 0) { emit(LEFT); accX += thr; } else { emit(RIGHT); accX -= thr; }
                if (abs(accY) < thr) accY = 0;
            } else {
                if (accY < 0) { emit(UP); accY += thr; } else { emit(DOWN); accY -= thr; }
                if (abs(accX) < thr) accX = 0;
            }
            steps++;
        }
        if (steps == MAX_STEPS_PER_SAMPLE) {
            /* burst cap reached: drop the rest instead of carrying a backlog */
            if (abs(accX) >= thr) accX = accX < 0 ? -(thr - 1) : thr - 1;
            if (abs(accY) >= thr) accY = accY < 0 ? -(thr - 1) : thr - 1;
        }
    }

    private void emit(int dir) {
        gestureSteps++;
        switch (dir) {
            case UP: nUp++; break;
            case DOWN: nDown++; break;
            case LEFT: nLeft++; break;
            case RIGHT: nRight++; break;
            default: nSelect++; break;
        }
        if (sink != null) sink.step(dir);
    }
}
