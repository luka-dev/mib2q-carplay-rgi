/*
 * Patch: CarplayDSILifecycleController$TerminalModeDSIKeyEventsController
 *
 * Overlays the stock Audi touchpad/knob event handler to route through
 * com.luka.carplay.cursor.CursorController.  Other input paths
 * (joystick, softkeys, rotary, character) keep their stock behaviour.
 *
 * Changes vs stock:
 *   - updateTouchEvent(int,int,int,int,int,int,int) no longer calls
 *     postTouchEvent directly; it hands the finger count + positions to
 *     CursorController.  The controller moves the on-screen cursor
 *     (one finger) or synthesises a screen-space swipe (two fingers) via
 *     a TouchSink callback we supply at construction.
 *   - updateTouchEvents(TouchEvent[]) same: routed through CursorController
 *     unless the event originates from a real touchscreen
 *     (isTouchScreen()==true), in which case we keep stock behaviour.
 *   - updateKey(DDS_SELECT): when the cursor is currently visible, the
 *     knob press/release is converted into a touch at cursor coords,
 *     suppressing the stock DDS_SELECT postButtonEvent.  All other keys
 *     and the cursor-not-visible DDS_SELECT case fall through unchanged.
 *
 * Why reflection on the outer class' private fields:
 *   The stock nested class reaches the outer's private dsiCarplaySafe /
 *   configuration / lastJoystickkey via javac-synthesised access$300 /
 *   access$600 / access$2900 / access$2902 accessors.  Those methods are
 *   marked ACC_SYNTHETIC — the JLS forbids resolving them from Java
 *   source code.  Since our patch is compiled from source, we can't
 *   reference them by name.  Instead we obtain the three Field handles
 *   once at class-load time via reflection and use them for every call.
 *   One setAccessible(true) per field is enough; no per-event reflection
 *   cost beyond a single Field.get()/set().
 */
package de.audi.app.terminalmode.dsi.carplay;

import com.luka.carplay.cursor.CursorController;
import com.luka.carplay.framework.Log;

import de.audi.app.terminalmode.ITerminalModeConfiguration;
import de.audi.app.terminalmode.TerminalModeUtils;
import de.audi.app.terminalmode.keyevents.ITerminalModeDSIKeyEventsController;
import de.audi.app.terminalmode.keyevents.Key;
import de.audi.app.terminalmode.keyevents.KeyState;

import org.dsi.ifc.carplay.TouchEvent;

import java.lang.reflect.Field;

class CarplayDSILifecycleController$TerminalModeDSIKeyEventsController
        implements ITerminalModeDSIKeyEventsController {

    private static final String TAG = "CarplayDSIKey";

    /* ============================================================
     * Reflection handles for outer-class private fields.
     * Names cross-checked via
     *   javap -p de.audi.app.terminalmode.dsi.carplay.CarplayDSILifecycleController
     * on the stock lsd.jar.  If the stock jar is a different rev and
     * field names have drifted, we fall back to type-based resolution
     * which survives single-name changes.
     *
     * Type-fallback note: CarplayDSILifecycleController has TWO fields
     * that would naïvely match DSICarplaySafe — `dsiCarplaySafe` (the
     * interface, wired to the real bridge) and `dsiCarplayProxy`
     * (DSICarplaySafeProxy, a notification wrapper around it).  The
     * stock access$300 accessor returns the former.  To disambiguate
     * without the name, we prefer the field whose declared type is
     * EXACTLY the DSICarplaySafe interface (not the concrete Proxy
     * subclass) — that's the plumbed-through one.
     * ============================================================ */
    private static final Field F_DSI;      /* DSICarplaySafe dsiCarplaySafe */
    private static final Field F_CFG;      /* ITerminalModeConfiguration configuration */
    private static final Field F_LAST_JS;  /* volatile Key lastJoystickkey */

    static {
        Field fDsi = null, fCfg = null, fJs = null;
        Throwable initErr = null;
        Class oc = CarplayDSILifecycleController.class;

        /* Primary: stock names. */
        fDsi = tryField(oc, "dsiCarplaySafe");
        fCfg = tryField(oc, "configuration");
        fJs  = tryField(oc, "lastJoystickkey");

        /* Fallback: resolve by exact declared type for any that didn't
         * match by name.  This handles jar revisions where stock fields
         * were renamed but types stayed stable. */
        if (fDsi == null || fCfg == null || fJs == null) {
            try {
                Field[] all = oc.getDeclaredFields();
                for (int i = 0; i < all.length; i++) {
                    Field f = all[i];
                    Class ft = f.getType();
                    if (fDsi == null && DSICarplaySafe.class.equals(ft)) {
                        f.setAccessible(true);
                        fDsi = f;
                    } else if (fCfg == null && ITerminalModeConfiguration.class.equals(ft)) {
                        f.setAccessible(true);
                        fCfg = f;
                    } else if (fJs == null && Key.class.equals(ft)) {
                        f.setAccessible(true);
                        fJs = f;
                    }
                }
            } catch (Throwable t) { initErr = t; }
        }

        F_DSI = fDsi;
        F_CFG = fCfg;
        F_LAST_JS = fJs;

        /* Loud diagnostic if the critical field is missing.  Without
         * F_DSI every key/touch/rotary/character event is a silent
         * drop — CarPlay UI appears alive but does not respond, which
         * is the worst possible debugging scenario.  At minimum this
         * message in /tmp/carplay_hook.log points the investigator
         * straight at the host-jar mismatch. */
        if (F_DSI == null) {
            Log.e(TAG, "CRITICAL: DSICarplaySafe bridge field not found on "
                + "CarplayDSILifecycleController. ALL CarPlay input events "
                + "(touchpad, knob, rotary, softkeys, character) will be "
                + "silently dropped. Likely cause: stock jar version drift "
                + "— expected field 'dsiCarplaySafe' of type DSICarplaySafe.",
                initErr);
        }
        if (F_CFG == null) {
            Log.w(TAG, "configuration field missing — screen-offset for "
                + "real-touchscreen path defaults to 0,0.");
        }
        if (F_LAST_JS == null) {
            Log.w(TAG, "lastJoystickkey field missing — joystick mid-release "
                + "repeat and JS_* state tracking will be broken.");
        }
    }

    private static Field tryField(Class oc, String name) {
        try {
            Field f = oc.getDeclaredField(name);
            f.setAccessible(true);
            return f;
        } catch (Throwable t) {
            return null;
        }
    }

    private static DSICarplaySafe dsi(CarplayDSILifecycleController outer) {
        if (F_DSI == null) return null;
        try { return (DSICarplaySafe) F_DSI.get(outer); }
        catch (Throwable t) { return null; }
    }

    private static ITerminalModeConfiguration cfg(CarplayDSILifecycleController outer) {
        if (F_CFG == null) return null;
        try { return (ITerminalModeConfiguration) F_CFG.get(outer); }
        catch (Throwable t) { return null; }
    }

    private static Key getJs(CarplayDSILifecycleController outer) {
        if (F_LAST_JS == null) return null;
        try { return (Key) F_LAST_JS.get(outer); }
        catch (Throwable t) { return null; }
    }

    private static void setJs(CarplayDSILifecycleController outer, Key k) {
        if (F_LAST_JS == null) return;
        try { F_LAST_JS.set(outer, k); }
        catch (Throwable t) { /* ignore */ }
    }

    /* ============================================================ */

    private final /* synthetic */ CarplayDSILifecycleController this$0;

    private CarplayDSILifecycleController$TerminalModeDSIKeyEventsController(
            CarplayDSILifecycleController outer) {
        this.this$0 = outer;
        installCursorTouchSink();
    }

    /* synthetic ctor called by the outer class's factory */
    CarplayDSILifecycleController$TerminalModeDSIKeyEventsController(
            CarplayDSILifecycleController outer,
            CarplayDSILifecycleController$1 unused) {
        this(outer);
    }

    /* ============================================================
     * Wire CursorController's TouchSink once — routes synthetic touch
     * events back through the stock DSI bridge.
     * ============================================================ */
    private void installCursorTouchSink() {
        final CarplayDSILifecycleController outer = this.this$0;
        CursorController.getInstance().setTouchSink(
            new CursorController.TouchSink() {
                public void postScreenTouch(int count, int x, int y) {
                    try {
                        /* Always carry (x,y) — even on touch-up (count==0).
                         * Stock code caches lastFingers for this same
                         * reason: iOS needs the release to land at the
                         * same coordinate as the press so it's interpreted
                         * as a tap / gesture-end, not a cancel. */
                        TouchEvent[] ev = new TouchEvent[] {
                            new TouchEvent(x, y)
                        };
                        DSICarplaySafe bridge = dsi(outer);
                        if (bridge != null) {
                            bridge.postTouchEvent(1 /*touchscreen id*/, count, ev);
                        }
                    } catch (Throwable t) {
                        /* Never let a sink failure kill the cursor. */
                    }
                }

                public void postPinch(int count, int x1, int y1, int x2, int y2) {
                    try {
                        /* Two-finger touch for pinch gestures.  iOS
                         * CarPlay consumes this only in Apple Maps; in
                         * any other app iOS silently drops count>1.
                         * The always-two-element array mirrors stock
                         * lastFingers behaviour — release carries both
                         * final positions so iOS interprets as
                         * gesture-end, not cancel. */
                        TouchEvent[] ev = new TouchEvent[] {
                            new TouchEvent(x1, y1),
                            new TouchEvent(x2, y2)
                        };
                        DSICarplaySafe bridge = dsi(outer);
                        if (bridge != null) {
                            bridge.postTouchEvent(1 /*touchscreen id*/, count, ev);
                        }
                    } catch (Throwable t) { }
                }
            });
    }

    /* ============================================================
     * updateTouchEvent — 7-arg stock form (touchpad)
     *
     * Stock signature: (id, count, ?, x1, y1, n6, n7)
     *   id     — touchpad surface id (2 = MMI touchpad)
     *   count  — finger count (0/1/2)
     *   x1/y1  — primary finger absolute coords (touchpad surface space)
     *   n6/n7  — second-finger angle+distance (decoded by calcSecondCorr)
     * ============================================================ */
    public void updateTouchEvent(int id, int count, int n3,
                                 int x1, int y1, int n6, int n7) {
        CursorController c = CursorController.getInstance();
        if (count >= 2) {
            int[] p2 = this.calcSecondCorr(x1, y1, n6, n7);
            c.onTwoFingers(x1, y1, p2[0], p2[1]);
        } else if (count == 1) {
            c.onOneFinger(x1, y1);
        } else {
            c.onTouchEnd();
        }
        /* Do NOT call through to postTouchEvent — CursorController owns
         * the touchpad and synthesises its own screen-space events. */
    }

    /* ============================================================
     * updateTouchEvents — array form
     *
     * If the events come from a real touchscreen (isTouchScreen==true —
     * e.g. passenger display prototype), forward to stock-style handling.
     * Otherwise redirect through CursorController.
     * ============================================================ */
    public void updateTouchEvents(
            de.audi.app.terminalmode.keyevents.TouchEvent[] touchEventArray) {
        if (touchEventArray != null && touchEventArray.length > 0
                && touchEventArray[0].isTouchScreen()) {
            /* Stock touchscreen path (passthrough).  Minimal reimplementation
             * — screen offset subtract, bulk post.  Kept intentionally simple;
             * MHI2 MMI does not have a real touchscreen in practice. */
            int n = 0;
            TouchEvent[] out = new TouchEvent[touchEventArray.length];
            ITerminalModeConfiguration conf = cfg(this.this$0);
            int offX = (conf != null) ? conf.getScreenOffsetX() : 0;
            int offY = (conf != null) ? conf.getScreenOffsetY() : 0;
            for (int i = 0; i < touchEventArray.length; i++) {
                out[i] = new TouchEvent(
                    touchEventArray[i].getCurrentX() - offX,
                    touchEventArray[i].getCurrentY() - offY);
                if (touchEventArray[i].getTouchState() != 1) n++;
            }
            DSICarplaySafe bridge = dsi(this.this$0);
            if (bridge != null) {
                bridge.postTouchEvent(1 /*touchscreen*/, n, out);
            }
            return;
        }

        /* Touchpad form: route through CursorController.
         *
         * CRITICAL: count ACTIVE fingers, not array length.  Stock Audi
         * convention (see the touchscreen branch above, and stock
         * `lastFingers` handling): getTouchState() == 1 means RELEASED
         * — the entry still carries coords (so iOS can see where the
         * finger lifted) but it is not an active finger.  Routing by
         * array length would make a release event look like "user is
         * still pressing", so the cursor never arms grace and any
         * in-flight pan/pinch never sees its synthetic touch-up. */
        CursorController c = CursorController.getInstance();
        if (touchEventArray == null || touchEventArray.length == 0) {
            c.onTouchEnd();
            return;
        }
        int active = 0;
        int a0 = -1, a1 = -1;
        for (int i = 0; i < touchEventArray.length; i++) {
            if (touchEventArray[i].getTouchState() != 1) {
                if (active == 0) a0 = i;
                else if (active == 1) a1 = i;
                active++;
            }
        }
        if (active == 0) {
            c.onTouchEnd();
        } else if (active == 1) {
            c.onOneFinger(touchEventArray[a0].getCurrentX(),
                          touchEventArray[a0].getCurrentY());
        } else {
            c.onTwoFingers(
                touchEventArray[a0].getCurrentX(), touchEventArray[a0].getCurrentY(),
                touchEventArray[a1].getCurrentX(), touchEventArray[a1].getCurrentY());
        }
    }

    /* ============================================================
     * updateKey — only DDS_SELECT with visible cursor is altered
     * ============================================================ */
    public void updateKey(Key key, KeyState keyState) {
        /* Gate DDS_SELECT → synthetic tap when cursor is up. */
        if (Key.DDS_SELECT.is(key)
                && CursorController.getInstance().isVisible()) {
            CursorController c = CursorController.getInstance();
            DSICarplaySafe bridge = dsi(this.this$0);
            if (keyState.is(KeyState.PRESSED)) {
                c.armForKnobPress();
                if (bridge != null) {
                    TouchEvent[] ev = new TouchEvent[] {
                        new TouchEvent(c.getX(), c.getY())
                    };
                    bridge.postTouchEvent(1 /*touchscreen*/, 1, ev);
                }
            } else if (keyState.is(KeyState.RELEASED)) {
                if (bridge != null) {
                    /* Preserve coords on release — see TouchSink for rationale. */
                    TouchEvent[] ev = new TouchEvent[] {
                        new TouchEvent(c.getX(), c.getY())
                    };
                    bridge.postTouchEvent(1 /*touchscreen*/, 0, ev);
                }
                c.afterKnobRelease();
            }
            return;
        }

        /* ---------- Verbatim stock body below ---------- */
        DSICarplaySafe bridge = dsi(this.this$0);
        if (TerminalModeUtils.isJoystickMiddleposition(key)) {
            Key last = getJs(this.this$0);
            if (null == last) {
                return;
            }
            if (bridge != null) {
                bridge.postButtonEvent(this.getKeyId(last), 1);
            }
            setJs(this.this$0, null);
            return;
        }
        int n = this.getKeyId(key);
        if (0 == n) {
            return;
        }
        int n2 = this.getKeyState(keyState);
        if (TerminalModeUtils.isJoystick(key)) {
            setJs(this.this$0, key);
        }
        if (bridge != null) {
            bridge.postButtonEvent(n, n2);
        }
    }

    /* ============================================================
     * updateRotary, updateCharacterEvent — verbatim stock
     * ============================================================ */
    public void updateRotary(int n) {
        DSICarplaySafe bridge = dsi(this.this$0);
        if (bridge != null) bridge.postRotaryEvent(n);
    }

    public void updateCharacterEvent(String[] stringArray, int[] nArray) {
        DSICarplaySafe bridge = dsi(this.this$0);
        if (bridge != null) bridge.postCharacterEvent(stringArray.length, stringArray);
    }

    /* ============================================================
     * Stock private helpers — copied verbatim
     * ============================================================ */
    private int getKeyState(KeyState keyState) {
        if (keyState.is(KeyState.PRESSED))  return 0;
        if (keyState.is(KeyState.RELEASED)) return 1;
        return -1;
    }

    private int getKeyId(Key key) {
        if (Key.BACK.is(key))        return 3;
        if (Key.DDS_SELECT.is(key))  return 1;
        if (key.isOneOf(Key.JS_NORTH, Key.SOFTKEY_SOUTHWEST)) return 7;
        if (key.isOneOf(Key.JS_EAST,  Key.SOFTKEY_NORTHEAST)) return 6;
        if (key.isOneOf(Key.JS_SOUTH, Key.SOFTKEY_SOUTHEAST)) return 8;
        if (key.isOneOf(Key.JS_WEST,  Key.SOFTKEY_NORTHWEST)) return 5;
        if (key.is(Key.SOFTKEY_EAST)) return 17;
        if (key.is(Key.SOFTKEY_WEST)) return 16;
        return 0;
    }

    /* Kept even though we don't call it from the cursor paths —
     * updateTouchEvent references it inline. */
    private int[] calcSecondCorr(int n, int n2, int n3, int n4) {
        if (n4 > 100) n4 -= 100;
        int n5 = (int)((double)n + Math.acos(n4 / 100.0) * (double)n4);
        int n6 = (int)((double)n2 + Math.asin(n4 / 100.0) * (double)n4);
        return new int[] { n5, n6 };
    }
}
