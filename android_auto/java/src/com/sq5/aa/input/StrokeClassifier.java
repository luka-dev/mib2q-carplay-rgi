/*
 * MMI touchpad stroke classifier for Android Auto screens other than Google Maps (SQ5 MU0918).
 *
 * Off the Maps screen the raw pad stream is forwarded to AA exactly as stock, because AA uses it
 * for handwriting (drawing letters on the touchpad).  A knob step would move the focus while the
 * owner writes, so a stroke is judged once, at release, and only a clear flick produces steps:
 *   - begun with a real PRESSED sample, lasts <= MAX_MS, no coordinate glitch, single finger;
 *   - net travel on the dominant axis >= MIN_MAJOR, average speed >= MIN_SPEED;
 *   - nearly straight: perpendicular extent <= MAX_OFFAXIS_PCT of the net travel, travel back
 *     along the axis <= MAX_BACKTRACK_PCT of it, net distance >= MIN_STRAIGHT_PCT of the path;
 *   - not within WRITING_HOLD_MS of a rejected (written) stroke - multi-stroke letters and the
 *     next letter's strokes stay silent.
 * Curvy / multi-direction / long / slow strokes (letters) never step.  A still tap (no travel)
 * may SELECT when enabled and not while writing.  All distances are in 1024-unit reference space,
 * scaled to the pad resolution.  Java 1.4 (IBM J9).  Thread-safe (synchronized).
 */
package com.sq5.aa.input;

final class StrokeClassifier {
    static final long MAX_MS = 400L;
    static final int MIN_MAJOR = 200;           /* ref units */
    static final int MIN_SPEED = 80;            /* ref units / 100 ms (net major / duration) */
    static final int MAX_OFFAXIS_PCT = 30;
    static final int MAX_BACKTRACK_PCT = 12;
    static final int MIN_STRAIGHT_PCT = 90;
    static final long WRITING_HOLD_MS = 1000L;
    static final int STEP_REF = 200;            /* one step per 200 ref units of net travel */
    static final int MAX_STEPS = 3;
    static final int TAP_TRAVEL = 30;           /* = TouchpadGesture.DEAD_ZONE */
    static final long TAP_MAX_MS = 300L;

    private boolean active;
    private boolean pressSeen;
    private String invalid;
    private int x0, y0, lastX, lastY, minX, maxX, minY, maxY;
    private long t0;
    private double path;
    private long holdUntil;
    private boolean holdSet;

    /* result of the last end() */
    private int dir;
    private String reason = "";
    private String stats = "";

    synchronized boolean active() {
        return active;
    }

    synchronized void reset() {
        active = false;
        pressSeen = false;
        invalid = null;
    }

    /** Forget the writing hold too (session change, host tests). */
    synchronized void resetAll() {
        reset();
        holdSet = false;
    }

    synchronized void begin(int x, int y, long now, boolean pressed) {
        active = true;
        pressSeen = pressed;
        invalid = null;
        x0 = lastX = minX = maxX = x;
        y0 = lastY = minY = maxY = y;
        t0 = now;
        path = 0.0;
    }

    synchronized void invalidate(String why) {
        if (active && invalid == null) invalid = why;
    }

    synchronized void add(int x, int y, long now, int res) {
        if (!active) return;
        if (x < 0 || y < 0 || x > 2 * res || y > 2 * res) {
            invalid = "glitch";
            return;
        }
        double dx = x - lastX;
        double dy = y - lastY;
        path += Math.sqrt(dx * dx + dy * dy);
        lastX = x;
        lastY = y;
        if (x < minX) minX = x;
        if (x > maxX) maxX = x;
        if (y < minY) minY = y;
        if (y > maxY) maxY = y;
    }

    private static int scale(int ref, int res) {
        int v = ref * res / TouchpadGesture.REF_RES;
        return v < 1 ? 1 : v;
    }

    /**
     * Release sample: classify the stroke.
     * @return number of steps in direction dir() (0 = none; SELECT counts 1)
     */
    synchronized int end(int x, int y, long now, int res, boolean tapEnabled) {
        dir = 0;
        if (!active) {
            reason = "no stroke";
            return 0;
        }
        add(x, y, now, res);
        active = false;
        long dur = now - t0;
        if (dur < 0) dur = 0;
        int dx = lastX - x0;
        int dy = lastY - y0;
        int adx = dx < 0 ? -dx : dx;
        int ady = dy < 0 ? -dy : dy;
        boolean horiz = adx > ady;
        int major = horiz ? adx : ady;
        int along = horiz ? maxX - minX : maxY - minY;
        int off = horiz ? maxY - minY : maxX - minX;
        int travel = (maxX - minX) > (maxY - minY) ? maxX - minX : maxY - minY;
        boolean writing = holdSet && t0 < holdUntil;
        int speed = (int) ((long) major * 100L / (dur > 0 ? dur : 1) * TouchpadGesture.REF_RES / res);
        double net = Math.sqrt((double) dx * dx + (double) dy * dy);
        stats = "d=" + dx + "," + dy + " path=" + (int) path + " off=" + off + " back=" + (along - major)
            + " " + dur + "ms v=" + speed;

        if (invalid == null && travel < scale(TAP_TRAVEL, res)) {
            if (tapEnabled && pressSeen && !writing && dur <= TAP_MAX_MS) {
                dir = TouchpadGesture.SELECT;
                reason = "tap";
                return 1;
            }
            reason = "still";
            return 0;                               /* rest / dot: no hold */
        }
        String why = null;
        if (invalid != null) why = invalid;
        else if (!pressSeen) why = "no press";
        else if (writing) why = "writing";
        else if (dur > MAX_MS) why = "long";
        else if (major < scale(MIN_MAJOR, res)) why = "short";
        else if (speed < MIN_SPEED) why = "slow";
        else if (off * 100 > MAX_OFFAXIS_PCT * major) why = "off-axis";
        else if ((along - major) * 100 > MAX_BACKTRACK_PCT * major) why = "backtrack";
        else if (net * 100.0 < MIN_STRAIGHT_PCT * path) why = "curved";
        if (why != null) {
            reason = why;
            holdSet = true;
            holdUntil = now + WRITING_HOLD_MS;
            return 0;
        }
        if (horiz) dir = dx < 0 ? TouchpadGesture.LEFT : TouchpadGesture.RIGHT;
        else dir = dy < 0 ? TouchpadGesture.UP : TouchpadGesture.DOWN;
        int n = major / scale(STEP_REF, res);
        if (n < 1) n = 1;
        if (n > MAX_STEPS) n = MAX_STEPS;
        reason = "flick";
        return n;
    }

    synchronized int dir() {
        return dir;
    }

    synchronized String reason() {
        return reason;
    }

    synchronized String stats() {
        return stats;
    }
}
