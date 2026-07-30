/*
 * CarPlay Touchpad Input Controller
 *
 * (Class name is legacy — this was originally an on-screen cursor state
 *  machine.  The cursor rendering was dropped after MU1316 turned out
 *  to ghost it through H.264 motion compensation, and the two-finger
 *  PAN/PINCH code was dropped after the log confirmed the Audi MMI
 *  touchpad on this unit only ever emits single-finger events via
 *  updateTouchEvents(TouchEvent[]).  File / class / package kept the
 *  same to minimise ripple through the DSI patch imports.  See memory
 *  `project_cursor_ghost_architectural_block` for the cursor ghost
 *  post-mortem.)
 *
 * Input model — single finger only
 *
 *   One finger  → DPAD ticks.  Accumulate Δx, Δy since the last emit;
 *                 whenever |Δx| or |Δy| crosses the (speed-adaptive)
 *                 threshold, emit a DDS_LEFT/RIGHT/UP/DOWN press+release
 *                 pair through TouchSink.postDpad and subtract the
 *                 threshold from the accumulator.  A long drag emits
 *                 multiple ticks so the user can traverse several list
 *                 items in one gesture.  Axes are independent — a
 *                 diagonal drag emits both X and Y ticks when both
 *                 accumulators cross threshold.
 *
 *   Knob (DDS_SELECT) — stock passthrough: press = focus-confirm in
 *                 whatever CarPlay element the dpad navigated to.  Not
 *                 intercepted by this controller.
 *
 * Multi-finger events are ignored (treated as touch-end).  If the
 * MMI touchpad ever starts reporting them, the DSI patch forwards
 * them here and we simply reset the single-finger state so a spurious
 * two-finger sample can't arm a dpad emission.
 *
 * Java 1.2 compatible: no generics, no lambdas, no java.util.Timer.
 */
package com.luka.carplay.cursor;

import com.luka.carplay.framework.Log;

public class CursorController {

    private static final String TAG = "InputCtrl";

    /* --- DPAD tuning (touchpad units) ---
     *
     * The MMI touchpad reports roughly 0..1023 × 0..767.  Threshold
     * adapts to finger speed so the user gets
     *   — fast swipe  → ~150 u/tick (wide sweep, many items/stroke)
     *   — slow drag   → ~200 u/tick (precise, one item per deliberate
     *                                 finger motion; avoids double-
     *                                 jumping when the user is aiming).
     * Linear interpolation between the two regimes based on the
     * instantaneous per-sample speed (units per 100 ms).
     * DEAD_ZONE=30 swallows tiny wiggles before the first tick fires;
     * after arm, only the current adaptive threshold matters. */
    private static final int DPAD_THRESHOLD_FAST = 150;
    private static final int DPAD_THRESHOLD_SLOW = 200;
    /* Speed axis is "touchpad units per 100 ms" (integer to stay
     * Java-1.2 friendly; no doubles in the hot path).  Samples arrive
     * every ~30 ms so speeds in the 30..400 range are typical. */
    private static final int SPEED_FAST          = 300;  /* ≥ → full sweep */
    private static final int SPEED_SLOW          = 100;  /* ≤ → full precision */
    private static final int DPAD_DEAD_ZONE      = 30;

    /* Key codes the TouchSink knows how to translate back to Audi Key
     * objects.  Values are intentionally not the internal getKeyId()
     * numbers — the DSI patch owns the mapping, we just name the
     * semantic direction. */
    public static final int KEY_DPAD_LEFT  = 1;
    public static final int KEY_DPAD_RIGHT = 2;
    public static final int KEY_DPAD_UP    = 3;
    public static final int KEY_DPAD_DOWN  = 4;

    /* ============================================================
     * TouchSink — implemented by the DSI patch.  Routes dpad button
     * events back through the stock DSI bridge's postButtonEvent.
     * ============================================================ */
    public interface TouchSink {
        /** DPAD tick — KEY_DPAD_* constant above.  The sink emits a
         *  press/release pair back-to-back; the controller never
         *  distinguishes the two phases, they're implementation detail
         *  of the DSI bridge's postButtonEvent. */
        void postDpad(int keyCode);
    }

    /* Legacy accessor kept for any stale call sites; always on. */
    public static boolean isFeatureEnabled() { return true; }

    /* ============================================================
     * Singleton
     * ============================================================ */
    private static final CursorController INSTANCE = new CursorController();
    public static CursorController getInstance() { return INSTANCE; }

    private CursorController() { }

    /* ============================================================
     * State (access synchronised on 'this')
     * ============================================================ */
    private TouchSink sink;

    /* touchpad anchor for relative dpad accumulation */
    private int     lastTpX = 0;
    private int     lastTpY = 0;
    private int     prevCount = 0;       /* previous finger count */

    /* dpad accumulators — signed; |accum| > threshold → emit tick */
    private int     accumDx = 0;
    private int     accumDy = 0;
    private boolean dpadArmed = false;   /* true after we've left DEAD_ZONE */
    private long    lastSampleMs = 0;    /* timestamp of prev one-finger sample */

    /* ============================================================
     * Wiring
     * ============================================================ */
    public synchronized void setTouchSink(TouchSink s) {
        this.sink = s;
        if (s != null) warnedDpad = false;
    }

    /* ============================================================
     * Public API — called from DSI patch
     * ============================================================ */

    public synchronized void onOneFinger(int tpX, int tpY) {
        long now = System.currentTimeMillis();

        if (prevCount != 1) {
            /* Entering one-finger.  Anchor without emit; reset
             * accumulators so previous drift doesn't leak in. */
            lastTpX = tpX;
            lastTpY = tpY;
            prevCount = 1;
            accumDx = 0;
            accumDy = 0;
            dpadArmed = false;
            lastSampleMs = now;
            return;
        }

        int dx = tpX - lastTpX;
        int dy = tpY - lastTpY;
        lastTpX = tpX;
        lastTpY = tpY;

        accumDx += dx;
        accumDy += dy;

        /* DEAD_ZONE gate: suppress emits until the cumulative drift
         * since touch-down exceeds it on either axis.  Once armed,
         * subsequent ticks use only the adaptive threshold. */
        if (!dpadArmed) {
            int absX = accumDx < 0 ? -accumDx : accumDx;
            int absY = accumDy < 0 ? -accumDy : accumDy;
            if (absX < DPAD_DEAD_ZONE && absY < DPAD_DEAD_ZONE) {
                lastSampleMs = now;
                return;
            }
            dpadArmed = true;
        }

        /* Adaptive threshold: compute speed from THIS sample's delta
         * and time.  Per-sample only — no EMA, no history window —
         * because CarPlay element-picking needs responsive feedback
         * to finger speed change; a smoothed speed would lag the
         * user's intent by one or two samples.
         *
         * Units-per-100-ms is chosen to keep integer arithmetic in
         * the same scale as the thresholds (150..200), so the linear
         * interpolation reads naturally. */
        int dtMs = (int)(now - lastSampleMs);
        if (dtMs <= 0) dtMs = 1;
        /* Clamp long gaps: if the touchpad driver held a sample back
         * (e.g. during OS jank), we don't want that to register as
         * "very slow" and force max precision on a fast gesture. */
        if (dtMs > 200) dtMs = 200;
        int stepDist = (dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy);
        int speed = stepDist * 100 / dtMs;
        lastSampleMs = now;

        int threshold;
        if (speed >= SPEED_FAST) {
            threshold = DPAD_THRESHOLD_FAST;
        } else if (speed <= SPEED_SLOW) {
            threshold = DPAD_THRESHOLD_SLOW;
        } else {
            /* Linear interp: threshold = FAST + (SLOW−FAST) * (1 − t)
             * where t = (speed − SLOW) / (FAST − SLOW). */
            int range = SPEED_FAST - SPEED_SLOW;
            int pos   = speed - SPEED_SLOW;
            int bonus = (range - pos) * (DPAD_THRESHOLD_SLOW - DPAD_THRESHOLD_FAST) / range;
            threshold = DPAD_THRESHOLD_FAST + bonus;
        }

        /* Emit as many ticks as the accumulators earned.  Loop so a
         * fast swipe across the whole pad can traverse several items
         * in one sample.  Threshold is fixed for this loop iteration
         * even though each emit subtracts from the accumulator — the
         * speed belongs to THIS sample, not the stride we're walking. */
        while (true) {
            int absX = accumDx < 0 ? -accumDx : accumDx;
            int absY = accumDy < 0 ? -accumDy : accumDy;
            if (absX < threshold && absY < threshold) break;
            if (absX >= threshold) {
                if (accumDx < 0) { emitDpad(KEY_DPAD_LEFT);  accumDx += threshold; }
                else             { emitDpad(KEY_DPAD_RIGHT); accumDx -= threshold; }
            }
            if (absY >= threshold) {
                if (accumDy < 0) { emitDpad(KEY_DPAD_UP);    accumDy += threshold; }
                else             { emitDpad(KEY_DPAD_DOWN);  accumDy -= threshold; }
            }
        }
    }

    /** Multi-finger is ignored — the MMI touchpad on this unit never
     *  reports it (all live events have active-count == 1).  Kept as
     *  a callable so the DSI patch's dispatch doesn't need a separate
     *  branch; treats multi-finger arrival as a defensive "touch end"
     *  to prevent a stray two-finger sample from arming a dpad emit. */
    public synchronized void onTwoFingers(int x1, int y1, int x2, int y2) {
        resetTouchState();
    }

    public synchronized void onTouchEnd() {
        resetTouchState();
    }

    /**
     * Called from CarPlayHook.onDeactivate BEFORE CarplayBus.stop().
     * No bus traffic — just clears soft state and drops the sink.
     */
    public synchronized void shutdown() {
        resetTouchState();
        /* Drop sink — next session's DSI patch constructor installs
         * a fresh one pointing at the new outer-class instance. */
        sink = null;
    }

    /* ============================================================
     * Private helpers
     * ============================================================ */

    private void resetTouchState() {
        lastTpX = 0; lastTpY = 0;
        prevCount = 0;
        accumDx = 0; accumDy = 0;
        dpadArmed = false;
        lastSampleMs = 0;
    }

    private void emitDpad(int key) {
        if (sink != null) {
            sink.postDpad(key);
        } else {
            warnNullSinkOnce();
        }
    }

    /* Null-sink warning — once per sink reset, so a silent drop isn't
     * invisible in the log. */
    private boolean warnedDpad = false;
    private void warnNullSinkOnce() {
        if (!warnedDpad) {
            warnedDpad = true;
            Log.w(TAG, "TouchSink not installed yet; dropping dpad event. "
                    + "DSI patch constructor may not have run.");
        }
    }
}
