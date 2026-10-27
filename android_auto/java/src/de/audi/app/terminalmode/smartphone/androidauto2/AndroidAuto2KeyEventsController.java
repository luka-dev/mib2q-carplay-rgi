/*
 * Rebuilt MU0918 de.audi.app.terminalmode.smartphone.androidauto2.AndroidAuto2KeyEventsController
 * (source: CFR decompile of re/lsd_ic.jar, MHI2Q_US_AUG22_P3639 MU0918 - NOT luka's MU1316).
 *
 * Stock behaviour is unchanged, byte for byte in intent: same DSI calls, same key-id mapping, same
 * log strings.  Additions (marked SQ5): updateTouchEvents first hands the event batch to
 * com.sq5.aa.input.AaTouchpadInput, which turns MMI touchpad swipes into Android Auto DPAD /
 * rotary key events; the constructor hands the DSI proxy to com.sq5.aa.input.AaRollerInput
 * (steering-wheel roller -> rotary while the cockpit shows the AA mirror).  The hooks are wrapped
 * in catch(Throwable) so a missing or failing adapter can never break the stock path (the raw pad
 * events are still forwarded to postTouchEvent as stock).
 *
 * Java 1.4 source (IBM J9).  Public/protected members must match the MU0918 original (gate in
 * aa-luka/input/build.sh).
 */
package de.audi.app.terminalmode.smartphone.androidauto2;

import de.audi.app.terminalmode.TerminalModeUtils;
import de.audi.app.terminalmode.keyevents.ITerminalModeDSIKeyEventsController;
import de.audi.app.terminalmode.keyevents.Key;
import de.audi.app.terminalmode.keyevents.KeyState;
import de.audi.atip.log.LogChannel;
import de.esolutions.fw.util.commons.Buffer;
import org.dsi.ifc.androidauto2.DSIAndroidAuto2;
import org.dsi.ifc.androidauto2.TouchEvent;

public final class AndroidAuto2KeyEventsController implements ITerminalModeDSIKeyEventsController {
    private static final String LOGCLASS = "TerminalModeDSIKeyEventsController";
    private volatile Key lastJoystickkey;
    private final boolean[] downState = new boolean[3];
    private final DSIAndroidAuto2 dsi;
    private final LogChannel lc;

    public AndroidAuto2KeyEventsController(DSIAndroidAuto2 dSIAndroidAuto2, LogChannel logChannel) {
        this.dsi = dSIAndroidAuto2;
        this.lc = logChannel;
        /* SQ5: the steering-wheel roller bridge posts rotary steps through this same DSI proxy */
        try {
            com.sq5.aa.input.AaRollerInput.setDsi(dSIAndroidAuto2);
        } catch (Throwable t) {
            /* never let the adapter break stock input */
        }
    }

    public void updateKey(Key key, KeyState keyState) {
        /* SQ5 cockpit menu: a roller press used by our menu arrives here as DDS_SELECT (the keyboard stack
         * collapses the wheel OK and the centre knob into one key); do not click the centre screen with it. */
        try {
            if (key.is(Key.DDS_SELECT) && com.sq5.aa.input.AaClusterMenu.consumeSelect(System.currentTimeMillis())) {
                return;
            }
        } catch (Throwable t) {
            /* menu unavailable: stock handling */
        }
        if (TerminalModeUtils.isJoystickMiddleposition(key)) {
            if (null == this.lastJoystickkey) {
                return;
            }
            this.lc.log(1000000, "[%1.updateKey] joystick middle position, release button %2", (Object) LOGCLASS,
                (Object) this.lastJoystickkey);
            this.dsi.postButtonEvent(this.getKeyId(this.lastJoystickkey), 1);
            this.lastJoystickkey = null;
            return;
        }
        int n = this.getKeyId(key);
        if (0 == n) {
            this.lc.log(1000000, "[%1.updateKey] unknown key id", (Object) LOGCLASS);
            return;
        }
        this.lc.log(1000000, "[%1.updateKey] %2", (Object) LOGCLASS, (long) n);
        if (TerminalModeUtils.isJoystick(key)) {
            this.lastJoystickkey = key;
        }
        this.dsi.postButtonEvent(n, this.getKeyState(keyState));
    }

    public void updateTouchEvent(int n, int n2, int n3, int n4, int n5, int n6, int n7) {
        this.lc.log(1000000, "[%1.updateTouchEvent]", (Object) LOGCLASS);
        TouchEvent[] touchEventArray = new TouchEvent[n2];
        if (n2 >= 1) {
            touchEventArray[0] = new TouchEvent(n4, n5, 0);
        }
        if (n2 >= 2) {
            int[] corr = this.calcSecondCorr(n4, n5, n6, n7);
            touchEventArray[1] = new TouchEvent(corr[0], corr[1], 1);
        }
        if (this.lc.isDebug()) {
            if (n2 >= 1) {
                this.lc.log(1000000, "[%1.updateTouchEvent] %2/%3", (Object) LOGCLASS,
                    (long) touchEventArray[0].getX(), (long) touchEventArray[0].getY());
            } else if (n2 >= 2) {
                /* unreachable in the MU0918 original as well (n2 >= 2 implies n2 >= 1); kept */
                Buffer buffer = new Buffer(50);
                buffer.append(touchEventArray[0].getX());
                buffer.append('/');
                buffer.append(touchEventArray[0].getY());
                buffer.append(' ');
                buffer.append(touchEventArray[1].getX());
                buffer.append('/');
                buffer.append(touchEventArray[1].getY());
                this.lc.log(1000000, "[%1.updateTouchEvent] %2", (Object) LOGCLASS, (Object) buffer.toString());
            }
        }
        int n8 = 0;
        if (!this.downState[n]) {
            this.downState[n] = true;
            n8 = 0;
        } else if (n2 == 0) {
            this.downState[n] = false;
            n8 = 1;
        } else {
            n8 = 2;
        }
        this.dsi.postTouchEvent(this.getDSITouchInputId(n), touchEventArray, n8, 0);
    }

    public void updateTouchEvents(de.audi.app.terminalmode.keyevents.TouchEvent[] touchEventArray) {
        /* SQ5: MMI touchpad -> Android Auto DPAD/rotary (additive; stock body below unchanged) */
        try {
            com.sq5.aa.input.AaTouchpadInput.onTouchEvents(touchEventArray, this.dsi);
        } catch (Throwable t) {
            /* never let the adapter break stock input */
        }
        /* SQ5: the raw pad stream pans Google Maps; skip it while the bridge handles the pad */
        try {
            if (com.sq5.aa.input.AaTouchpadInput.suppressRawPad(touchEventArray)) return;
        } catch (Throwable t) {
            /* fall through to stock */
        }
        this.lc.log(1000000, "[%1.updateTouchEvents] %2 %3", (Object) LOGCLASS,
            (Object) Integer.toString(touchEventArray.length), (Object) touchEventArray[0]);
        TouchEvent[] touchEventArray2 = new TouchEvent[touchEventArray.length];
        int n = 0;
        for (int i2 = 0; i2 < touchEventArray.length; ++i2) {
            de.audi.app.terminalmode.keyevents.TouchEvent touchEvent = touchEventArray[i2];
            touchEventArray2[i2] = new TouchEvent(touchEvent.getCurrentX(), touchEvent.getCurrentY(), i2);
        }
        switch (touchEventArray[0].getTouchState()) {
            case 0: {
                n = 0;
                break;
            }
            case 1: {
                n = 1;
                break;
            }
            default: {
                n = 2;
            }
        }
        this.dsi.postTouchEvent(touchEventArray[0].isTouchScreen() ? 1 : 0, touchEventArray2, n, 0);
    }

    private int[] calcSecondCorr(int n, int n2, int n3, int n4) {
        if (n4 > 100) {
            n4 -= 100;
        }
        int n5 = (int) ((double) n + Math.acos(n4 / 100) * (double) n4);
        int n6 = (int) ((double) n2 + Math.asin(n4 / 100) * (double) n4);
        return new int[]{n5, n6};
    }

    public void updateRotary(int n) {
        this.dsi.postRotaryEvent(n);
    }

    private int getKeyState(KeyState keyState) {
        if (keyState.is(KeyState.PRESSED)) {
            return 0;
        }
        if (keyState.is(KeyState.RELEASED)) {
            return 1;
        }
        this.lc.log(1000000, "[%1.getKeyState] Unsupported key state %2", (Object) LOGCLASS, (Object) keyState);
        return -1;
    }

    private int getKeyId(Key key) {
        if (key.is(Key.BACK)) {
            return 4;
        }
        if (key.is(Key.DDS_SELECT)) {
            return 23;
        }
        if (key.isOneOf(Key.JS_NORTH, Key.SOFTKEY_SOUTHWEST)) {
            return 19;
        }
        if (key.isOneOf(Key.JS_EAST, Key.SOFTKEY_NORTHEAST)) {
            return 22;
        }
        if (key.isOneOf(Key.JS_SOUTH, Key.SOFTKEY_SOUTHEAST)) {
            return 20;
        }
        if (key.isOneOf(Key.JS_WEST, Key.SOFTKEY_NORTHWEST)) {
            return 21;
        }
        if (key.is(Key.SOFTKEY_EAST)) {
            return 2;
        }
        if (key.is(Key.SOFTKEY_WEST)) {
            return 1;
        }
        this.lc.log(1000000, "[%1.getKeyId] Unsupport %2", (Object) LOGCLASS, (Object) key);
        return 0;
    }

    private int getDSITouchInputId(int n) {
        switch (n) {
            case 2: {
                return 1;
            }
        }
        return 0;
    }

    public void updateCharacterEvent(String[] stringArray, int[] nArray) {
        this.lc.log(1000000, "[%1.updateCharacterEvent]", (Object) LOGCLASS);
    }
}
