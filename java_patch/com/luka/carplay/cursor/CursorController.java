/*
 * CarPlay Cursor Controller
 *
 * State machine for the on-screen cursor driven by the MMI touchpad.
 *
 *   updateTouchEvent(count=1, x, y) → onOneFinger(x, y)
 *     → delta-driven cursor move, CursorClient.show(cx, cy)
 *   updateTouchEvent(count=2, ...) → onTwoFingers(x1,y1,x2,y2)
 *     → gesture-type arbitration (PAN vs PINCH, see below)
 *   updateTouchEvent(count=0, ...) → onTouchEnd()
 *     → arm grace timer; after 5 s → CursorClient.hide() (C-side fades out)
 *
 *   updateKey(DDS_SELECT, PRESSED)  while visible → armForKnobPress()
 *     (DSI patch then emits postTouchEvent tap itself, using getX()/getY())
 *   updateKey(DDS_SELECT, RELEASED) while armed   → afterKnobRelease()
 *
 * Two-finger gesture arbitration:
 *   When the user first puts two fingers down we enter the UNDECIDED
 *   sub-state and emit nothing on the bus.  Per sample we accumulate
 *   |Δspan| (span = distance between fingers) and |Δcenter| (midpoint
 *   drift).  Once either exceeds PINCH_COMMIT, we commit to whichever
 *   is larger:
 *     PAN  — synthesise a single virtual finger on the CarPlay screen
 *            that drifts by the center delta.  All-axis (vertical,
 *            horizontal, diagonal) — iOS's scroll view decides what
 *            to do with the motion.
 *     PINCH — synthesise TWO fingers on screen, anchored at the cursor
 *            position, separation = touchpad-span * PINCH_GAIN.  iOS
 *            CarPlay uses this for Apple Maps zoom; elsewhere it's a
 *            no-op (Apple silently drops count>1 outside Maps).
 *
 * Inertia/momentum: delegated to iOS.  UIKit derives flick velocity
 * from the last ~100 ms of touch-moves before touch-up and kicks off
 * its own inertia scroll.  We must:
 *   - Emit every touchpad sample (no subsampling / rate-limiting).
 *   - Emit the final touch-up the moment count drops to 0, with no
 *     debounce, carrying the last known (x,y) so velocity is preserved.
 *
 * Cold-start position: screen centre.  After a fade-out the position is
 * preserved, so the next finger-down starts the cursor where it was last
 * seen.  Screen size comes from EVT_SCREEN_INFO (sticky) published by
 * cursor_hook.c; until that arrives we use a 1024×480 fallback.
 *
 * Java 1.2 compatibility: no java.util.Timer, no generics, no lambdas.
 */
package com.luka.carplay.cursor;

import com.luka.carplay.framework.CarplayBus;
import com.luka.carplay.framework.Log;

public class CursorController implements CarplayBus.Listener {

    private static final String TAG = "CursorCtrl";

    /* Tuning */
    private static final float GAIN = 2.0f;
    private static final int   GRACE_MS = 5000;
    /* Observed from dmdt gs on MHI2Q (DISPLAYABLE_HMI / LVDS1 Main Display).
     * EVT_SCREEN_INFO from native overrides this as soon as the first OMX
     * frame is decoded. */
    private static final int   DEFAULT_SCREEN_W = 1024;
    private static final int   DEFAULT_SCREEN_H = 480;

    /* Two-finger gesture arbitration.  Thresholds are in touchpad units;
     * the MMI touchpad reports roughly 0..1023 × 0..767, so 30 is ≈3 %
     * of width — small enough to fire on a clear gesture, large enough
     * to ignore wiggle from settling fingers. */
    private static final int   PINCH_COMMIT = 30;
    /* Screen-side span of the synthetic pinch = touchpad-span × GAIN.
     * GAIN > 1 amplifies so the user doesn't need to stretch fingers
     * further than they can comfortably reach on a ~5 cm touchpad. */
    private static final float PINCH_GAIN = 2.0f;

    private static final int   MODE_UNDECIDED = 0;
    private static final int   MODE_PAN       = 1;
    private static final int   MODE_PINCH     = 2;

    /* ============================================================
     * Hook for synthesising screen-space touch events.  DSI patch
     * supplies this — it owns the TouchEvent class + DSI bridge.
     * ============================================================ */
    public interface TouchSink {
        /**
         * Single-finger touch.
         * @param count 1 = touch-down/move, 0 = touch-up
         */
        void postScreenTouch(int count, int x, int y);

        /**
         * Two-finger touch (for pinch gestures).
         * @param count 2 = both fingers down/moving, 0 = both lifted
         *              (on count=0 we still pass the last (x1,y1,x2,y2)
         *               so iOS gets a tap-up with positions — Stock
         *               Audi code caches lastFingers for the same
         *               reason; a release without a position is
         *               interpreted as a cancel instead of gesture-end.)
         */
        void postPinch(int count, int x1, int y1, int x2, int y2);
    }

    /* ============================================================
     * Singleton
     * ============================================================ */
    private static final CursorController INSTANCE = new CursorController();
    public static CursorController getInstance() { return INSTANCE; }

    private CursorController() {
        CarplayBus.getInstance().on(CarplayBus.EVT_SCREEN_INFO, this);
    }

    /* ============================================================
     * State (all access synchronised on 'this')
     * ============================================================ */
    private TouchSink sink;

    private int screenW = DEFAULT_SCREEN_W;
    private int screenH = DEFAULT_SCREEN_H;

    /* cursor position in screen pixels, persists across fades */
    private int cursorX = DEFAULT_SCREEN_W / 2;
    private int cursorY = DEFAULT_SCREEN_H / 2;

    /* touchpad anchor for relative motion */
    private int lastTpX = 0;
    private int lastTpY = 0;
    private int prevCount = 0;           /* previous finger count */

    /* two-finger gesture arbitration */
    private int     twoFingerMode = MODE_UNDECIDED;
    private int     spanChangeSum = 0;
    private int     centerChangeSum = 0;
    private int     lastSpan = 0;
    private int     lastCx = 0, lastCy = 0;

    /* single-finger synthetic swipe (PAN mode) */
    private boolean swipeActive = false;
    private int     swipeX = 0;
    private int     swipeY = 0;

    /* two-finger synthetic pinch (PINCH mode) — last emitted screen coords,
     * kept so we can replay them on touch-up (iOS needs the final position
     * for gesture completion). */
    private boolean pinchActive = false;
    private int     pinchF1X = 0, pinchF1Y = 0;
    private int     pinchF2X = 0, pinchF2Y = 0;

    /* knob press ownership */
    private boolean knobArmed = false;

    /* grace timer (Java 1.2 compatible) */
    private volatile long graceExpireAt = 0;   /* 0 = no pending */
    private Thread graceThread = null;

    /* whether the C-side cursor is currently visible.  Mirror kept here
     * so we can report isVisible() to the DSI patch quickly without an
     * IPC round-trip. */
    private boolean visible = false;

    /* ============================================================
     * Wiring
     * ============================================================ */
    public synchronized void setTouchSink(TouchSink s) {
        this.sink = s;
        /* Fresh sink → fresh telemetry: re-enable per-context null-sink
         * warnings so a subsequent silent drop isn't masked by a prior
         * episode from before reconnect. */
        if (s != null) {
            warnedKnobDrag = false;
            warnedSwipeStart = false;
            warnedSwipeMove = false;
            warnedPinch = false;
        }
    }

    /* ============================================================
     * Public API — called from DSI patch
     * ============================================================ */

    public synchronized void onOneFinger(int tpX, int tpY) {
        cancelGrace();
        endTwoFingerGestureIfActive();

        if (prevCount == 1) {
            int dx = (int)((tpX - lastTpX) * GAIN);
            int dy = (int)((tpY - lastTpY) * GAIN);
            cursorX = clamp(cursorX + dx, 0, screenW - 1);
            cursorY = clamp(cursorY + dy, 0, screenH - 1);
        }
        /* else: first sample after (0 or 2) fingers → just re-anchor,
         * no cursor delta this tick (prevents jerk). */
        lastTpX = tpX;
        lastTpY = tpY;
        prevCount = 1;
        twoFingerMode = MODE_UNDECIDED;

        CursorClient.getInstance().show(cursorX, cursorY);
        visible = true;

        /* If knob is held, we're doing a drag gesture.  Keep the iOS-side
         * touch stream alive by emitting a touch-move at the new cursor
         * position.  Without this, iOS sees a static tap and would reject
         * drag-oriented interactions (carousel scroll, slider drag). */
        if (knobArmed) {
            if (sink != null) {
                sink.postScreenTouch(1, cursorX, cursorY);
            } else {
                warnNullSinkOnce("knob-drag");
            }
        }
    }

    public synchronized void onTwoFingers(int x1, int y1, int x2, int y2) {
        /* If knob is held, ignore 2-finger events entirely — keep the
         * in-flight drag gesture intact until release. */
        if (knobArmed) return;

        cancelGrace();

        int span = isqrt(sq(x2 - x1) + sq(y2 - y1));
        int cx = (x1 + x2) >> 1;
        int cy = (y1 + y2) >> 1;

        if (prevCount < 2) {
            /* Entering 2-finger gesture.  Don't emit yet — wait for the
             * arbitration thresholds to commit to PAN or PINCH.  Cursor
             * sprite stays visible at the current position so the user
             * has immediate feedback that their fingers were registered. */
            twoFingerMode = MODE_UNDECIDED;
            spanChangeSum = 0;
            centerChangeSum = 0;
            lastSpan = span;
            lastCx = cx; lastCy = cy;
            prevCount = 2;
            CursorClient.getInstance().show(cursorX, cursorY);
            visible = true;
            return;
        }

        /* Accumulate motion since last sample. */
        int spanDelta = span - lastSpan;
        if (spanDelta < 0) spanDelta = -spanDelta;
        spanChangeSum += spanDelta;

        int dcx = cx - lastCx;
        int dcy = cy - lastCy;
        centerChangeSum += isqrt(dcx * dcx + dcy * dcy);

        lastSpan = span;
        lastCx = cx; lastCy = cy;

        /* ---------- UNDECIDED: wait for motion to disambiguate ---------- */
        if (twoFingerMode == MODE_UNDECIDED) {
            if (spanChangeSum < PINCH_COMMIT && centerChangeSum < PINCH_COMMIT) {
                /* Still below threshold — hold fire, cursor stays put. */
                CursorClient.getInstance().show(cursorX, cursorY);
                visible = true;
                return;
            }
            if (spanChangeSum > centerChangeSum) {
                twoFingerMode = MODE_PINCH;
                pinchActive = true;
                /* First pinch sample: emit touch-down (count=2) at the
                 * scaled finger positions. iOS logs this as the pinch
                 * start and uses current span as scale=1.0. */
                emitPinchSample(x1, y1, x2, y2, 2);
            } else {
                twoFingerMode = MODE_PAN;
                swipeActive = true;
                swipeX = cursorX;
                swipeY = cursorY;
                /* First pan sample: touch-down at cursor. iOS begins a
                 * single-finger pan gesture; the scroll view picks up
                 * whichever axis has dominant motion from here on. */
                if (sink != null) sink.postScreenTouch(1, swipeX, swipeY);
                else              warnNullSinkOnce("swipe-start");
            }
            CursorClient.getInstance().show(cursorX, cursorY);
            visible = true;
            return;
        }

        /* ---------- Committed PAN: drift swipe point by center delta ---------- */
        if (twoFingerMode == MODE_PAN) {
            int dx = (int)(dcx * GAIN);
            int dy = (int)(dcy * GAIN);
            swipeX = clamp(swipeX + dx, 0, screenW - 1);
            swipeY = clamp(swipeY + dy, 0, screenH - 1);
            if (sink != null) sink.postScreenTouch(1, swipeX, swipeY);
            else              warnNullSinkOnce("swipe-move");
        }
        /* ---------- Committed PINCH: emit both fingers around cursor ---------- */
        else if (twoFingerMode == MODE_PINCH) {
            emitPinchSample(x1, y1, x2, y2, 2);
        }

        /* Keep cursor sprite pinned at its frozen position so the user
         * sees their aim-point during the gesture. */
        CursorClient.getInstance().show(cursorX, cursorY);
        visible = true;
    }

    public synchronized void onTouchEnd() {
        endTwoFingerGestureIfActive();
        lastTpX = 0; lastTpY = 0;
        prevCount = 0;
        twoFingerMode = MODE_UNDECIDED;
        /* Do NOT touch cursorX/Y — position is preserved across fades. */
        /* Skip grace while knob is held: cursor must stay visible during a
         * drag gesture even if the user lifts the finger mid-hold.  When
         * the knob releases, afterKnobRelease re-arms the grace timer if
         * no finger is back. */
        if (visible && !knobArmed) {
            armGrace();
        }
    }

    /** Called by DSI patch on DDS_SELECT PRESSED while cursor visible. */
    public synchronized void armForKnobPress() {
        cancelGrace();
        endTwoFingerGestureIfActive();
        knobArmed = true;
    }

    /** Called by DSI patch on DDS_SELECT RELEASED while previously armed. */
    public synchronized void afterKnobRelease() {
        knobArmed = false;
        if (prevCount == 0) armGrace();
    }

    /**
     * Called from CarPlayHook.onDeactivate BEFORE CarplayBus.stop().
     *
     * Without this, a CarPlay session that ends mid-gesture leaves
     * state dangling:
     *   — iOS holds a "stuck finger" from the unclosed touch stream
     *     and the next session starts in an invalid gesture state.
     *   — the grace-timer thread keeps running and eventually fires
     *     CursorClient.hide() on a stopped bus, producing log spam
     *     until the thread exits.
     *   — native g_visible stays true until the next CMD_CURSOR_HIDE,
     *     so a stale sprite can flash on the first frame of the next
     *     session.
     *
     * shutdown() is best-effort and idempotent: safe to call even if
     * nothing is active.
     */
    public synchronized void shutdown() {
        /* 1. Drain the iOS-side gesture cleanly — touch-up carries the
         *    last known position so iOS registers as gesture-end
         *    (not cancel). */
        endTwoFingerGestureIfActive();

        /* 2. Kill the grace-timer thread so it can't fire on a
         *    stopped bus. */
        cancelGrace();

        /* 3. Force-hide cursor while the bus is still alive.  Native
         *    runs its 300 ms fade on its own frame timeline and flips
         *    g_visible=false at the end regardless of whether further
         *    OMX frames arrive. */
        if (visible) {
            visible = false;
            CursorClient.getInstance().hide();
        }

        /* 4. Reset soft state so a reactivation starts clean.  Note
         *    that cursorX/cursorY are deliberately preserved — if the
         *    user reconnects the same phone seconds later, the cursor
         *    should reappear where it was aiming. */
        prevCount = 0;
        twoFingerMode = MODE_UNDECIDED;
        knobArmed = false;
        lastTpX = 0; lastTpY = 0;
        spanChangeSum = 0;
        centerChangeSum = 0;

        /* 5. Drop sink reference.  The DSI patch constructor on the
         *    next activation installs a fresh one pointing at the new
         *    outer-class instance; holding the old reference would
         *    route synthetic touches into a torn-down DSI bridge. */
        sink = null;
    }

    public synchronized boolean isVisible() { return visible; }
    public synchronized boolean isKnobArmed() { return knobArmed; }
    public synchronized int getX() { return cursorX; }
    public synchronized int getY() { return cursorY; }
    public synchronized int getScreenW() { return screenW; }
    public synchronized int getScreenH() { return screenH; }

    /* ============================================================
     * CarplayBus listener — EVT_SCREEN_INFO
     * ============================================================ */
    public void onFrame(int type, int flags, byte[] payload, int len) {
        if (type != CarplayBus.EVT_SCREEN_INFO) return;
        CarplayBus.Data d = CarplayBus.parseText(payload, len);
        int w = d.num("width",  0);
        int h = d.num("height", 0);
        if (w <= 0 || h <= 0) return;

        synchronized (this) {
            if (w == screenW && h == screenH) return;
            screenW = w;
            screenH = h;
            cursorX = clamp(cursorX, 0, screenW - 1);
            cursorY = clamp(cursorY, 0, screenH - 1);
        }
        Log.i(TAG, "screen resolution: " + w + "x" + h);
    }

    /* ============================================================
     * Private helpers
     * ============================================================ */

    /**
     * Map raw touchpad finger positions to two screen-space positions
     * symmetric around the cursor, separation = touchpad-span × GAIN.
     *
     * Derivation: vector from touchpad-midpoint to each finger is
     * (±dx/2, ±dy/2) where dx,dy = x2-x1, y2-y1.  Amplifying by GAIN
     * and anchoring around (cursorX, cursorY):
     *
     *   f1_screen = (cursorX - dx·GAIN/2,  cursorY - dy·GAIN/2)
     *   f2_screen = (cursorX + dx·GAIN/2,  cursorY + dy·GAIN/2)
     *
     * Screen span = |dx|·GAIN, direction preserved.  The span term in
     * the normalisation conveniently cancels — no sqrt needed here.
     */
    private void emitPinchSample(int x1, int y1, int x2, int y2, int count) {
        int dx = x2 - x1;
        int dy = y2 - y1;
        int halfDx = (int)(dx * PINCH_GAIN / 2.0f);
        int halfDy = (int)(dy * PINCH_GAIN / 2.0f);

        /* Symmetric clamp: if either finger would fall off-screen, scale
         * BOTH halfs down proportionally so the pair stays symmetric
         * around the cursor.  Independent per-finger clamp would break
         * UIPinchGestureRecognizer's scale contract (ratio of current to
         * initial span), making zoom non-linear near the edge. */
        int absHx = (halfDx < 0) ? -halfDx : halfDx;
        int absHy = (halfDy < 0) ? -halfDy : halfDy;
        int maxHx = Math.min(cursorX, screenW - 1 - cursorX);
        int maxHy = Math.min(cursorY, screenH - 1 - cursorY);
        if (absHx > maxHx && absHx > 0) {
            halfDy = (int)((long)halfDy * maxHx / absHx);
            halfDx = (halfDx < 0) ? -maxHx : maxHx;
            absHx = maxHx;
            absHy = (halfDy < 0) ? -halfDy : halfDy;
        }
        if (absHy > maxHy && absHy > 0) {
            halfDx = (int)((long)halfDx * maxHy / absHy);
            halfDy = (halfDy < 0) ? -maxHy : maxHy;
        }

        pinchF1X = cursorX - halfDx;
        pinchF1Y = cursorY - halfDy;
        pinchF2X = cursorX + halfDx;
        pinchF2Y = cursorY + halfDy;
        if (sink != null) {
            sink.postPinch(count, pinchF1X, pinchF1Y, pinchF2X, pinchF2Y);
        } else {
            warnNullSinkOnce("pinch");
        }
    }

    /** Must be called while synchronized(this).  Cleanly terminates
     *  whichever 2-finger mode is currently active.  Safe to call when
     *  neither is active. */
    private void endTwoFingerGestureIfActive() {
        if (swipeActive) {
            swipeActive = false;
            if (sink != null) sink.postScreenTouch(0, swipeX, swipeY);
        }
        if (pinchActive) {
            pinchActive = false;
            if (sink != null) {
                sink.postPinch(0, pinchF1X, pinchF1Y, pinchF2X, pinchF2Y);
            }
        }
    }

    /* Grace timer — simple thread-per-schedule.  Java 1.2 has no
     * java.util.Timer, so we roll our own; GC copes with short-lived
     * daemons just fine. */
    private void armGrace() {
        graceExpireAt = System.currentTimeMillis() + GRACE_MS;
        if (graceThread == null) {
            graceThread = new Thread(new Runnable() {
                public void run() { graceLoop(); }
            }, "CursorGrace");
            graceThread.setDaemon(true);
            graceThread.start();
        } else {
            /* A thread is already running and will pick up the new
             * expiry time on its next wake cycle (volatile read). */
            graceThread.interrupt();
        }
    }

    private void cancelGrace() {
        graceExpireAt = 0;
        if (graceThread != null) {
            graceThread.interrupt();
            graceThread = null;
        }
    }

    private void graceLoop() {
        while (true) {
            long expire;
            synchronized (this) {
                expire = graceExpireAt;
                if (expire == 0) { graceThread = null; return; }
            }
            long now = System.currentTimeMillis();
            long sleep = expire - now;
            if (sleep <= 0) {
                /* Fire — send hide() INSIDE the sync block so a parallel
                 * onOneFinger can't race a show() between our state flip
                 * and the bus write, which would leave the cursor trapped
                 * in a fade the user didn't trigger. */
                synchronized (this) {
                    if (graceExpireAt != expire) continue;   /* rearmed — loop */
                    graceExpireAt = 0;
                    visible = false;
                    graceThread = null;
                    CursorClient.getInstance().hide();
                }
                return;
            }
            try { Thread.sleep(sleep); }
            catch (InterruptedException e) { /* rearmed or cancelled; loop */ }
        }
    }

    /* Log a null-sink warning at most once per context.  A silently-
     * dropped touch event is hard to debug without this signal. */
    private boolean warnedKnobDrag = false;
    private boolean warnedSwipeStart = false;
    private boolean warnedSwipeMove  = false;
    private boolean warnedPinch      = false;
    private void warnNullSinkOnce(String context) {
        boolean fire = false;
        if ("knob-drag".equals(context)) {
            if (!warnedKnobDrag) { warnedKnobDrag = true; fire = true; }
        } else if ("swipe-start".equals(context)) {
            if (!warnedSwipeStart) { warnedSwipeStart = true; fire = true; }
        } else if ("swipe-move".equals(context)) {
            if (!warnedSwipeMove) { warnedSwipeMove = true; fire = true; }
        } else if ("pinch".equals(context)) {
            if (!warnedPinch) { warnedPinch = true; fire = true; }
        }
        if (fire) {
            Log.w(TAG, "TouchSink not installed yet; dropping " + context
                    + " event. DSI patch constructor may not have run.");
        }
    }

    private static int clamp(int v, int lo, int hi) {
        if (v < lo) return lo;
        if (v > hi) return hi;
        return v;
    }

    private static int sq(int v) { return v * v; }

    /* Integer sqrt via Math.sqrt — double precision is fine for the
     * small touchpad distances we're measuring (<1500 units max).
     * Could be replaced with a Newton-iteration if Math.sqrt turns
     * out to be prohibitively slow on the MHI2Q SoC. */
    private static int isqrt(int v) {
        if (v <= 0) return 0;
        return (int)Math.sqrt((double)v);
    }
}
