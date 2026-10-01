/*
 * MMI touchpad -> Android Auto navigation (SQ5 MHI2Q MU0918).
 *
 * Event path on MU0918 (stock):
 *   HMI VirtualButtonModel 634662912 (terminal-mode screen)
 *     -> TMTouchscreenVBListener.touchPadPressed/-PositionMoved/-Released   (EvoTMConfiguration
 *        .isTouchScreenInputWidget() == true, so this listener is the bound TMVirtualButtonListener)
 *     -> ITerminalModeDSIKeyEventsController.updateTouchEvents(TouchEvent[])  (one pad event per call)
 *     -> AndroidAuto2KeyEventsController.updateTouchEvents -> DSIAndroidAuto2.postTouchEvent(0 = pad,..)
 * gal.json declares the touchpad with "uiNavigation":false, so the phone ignores those raw pad
 * events and the touchpad does nothing in Android Auto.
 *
 * The rebuilt AndroidAuto2KeyEventsController (aa-luka/input/src) calls onTouchEvents() first;
 * this class turns single-finger swipes into the same DSI key events the stock controller already
 * sends for the MMI joystick (postButtonEvent Android KEYCODE_DPAD_UP 19 / DOWN 20 / LEFT 21 /
 * RIGHT 22 / CENTER 23, state 0 = pressed, 1 = released), and optionally vertical swipes into
 * rotary steps (postRotaryEvent).  The stock raw postTouchEvent forwarding is left unchanged.
 *
 * Foreground: the terminal-mode VirtualButtonModel only receives the touchpad while the
 * terminal-mode (Android Auto) screen owns the MMI input, so native screens (handwriting speller,
 * native nav) never reach this code.  An optional session gate (setSessionActive) additionally
 * drops events outside an Android Auto session.
 *
 * Screen-dependent mode (car 2026-09-28, handwriting regression), latched per stroke at touch-down
 * from com.sq5.aa.luka.MirrorGate.isMapsOnScreen() (fresh /tmp/sq5_mirror_ready from the maps-gated
 * sq5_mirror; resolved by reflection so this package still builds standalone):
 *   Google Maps on screen: swipes -> knob steps incrementally (TouchpadGesture) and the raw pad
 *     batches are NOT forwarded (Maps panned the map with them);
 *   any other AA screen, or unsure (no/stale marker, mirror off, old mirror): the raw pad stream is
 *     forwarded exactly as stock (AA handwriting needs it) and only a clear flick produces knob
 *     steps, decided at release (StrokeClassifier) - letters never step.
 *
 * SD-card switches (root of SD1, re-read every 5 s):
 *   sq5_tp_off      touchpad bridge disabled (stock behaviour: raw forwarding only)
 *   sq5_tp_raw      always forward the raw pad stream (also on Maps)
 *   sq5_tp_mapsonly knob steps only on the Maps screen (elsewhere pure stock)
 *   sq5_tp_tap      touch-tap -> SELECT (default off: the physical click selects)
 *   sq5_tp_dpad     swipes -> DPAD keys (default: knob steps; this HU declares no D-pad)
 *
 * Every entry point catches Throwable: input handling must never break the HMI key path.
 */
package com.sq5.aa.input;

import com.sq5.aa.luka.AaLog;

import de.audi.app.terminalmode.IContext;
import de.audi.app.terminalmode.ITerminalModeConfiguration;
import de.audi.app.terminalmode.keyevents.TouchEvent;
import java.io.File;
import java.lang.reflect.Method;
import org.dsi.ifc.androidauto2.DSIAndroidAuto2;

public final class AaTouchpadInput {
    public static final String VERSION = "aa-touchpad 1.1";

    /* Android key codes, exactly as AndroidAuto2KeyEventsController.getKeyId() sends them */
    public static final int KEYCODE_DPAD_UP = 19;
    public static final int KEYCODE_DPAD_DOWN = 20;
    public static final int KEYCODE_DPAD_LEFT = 21;
    public static final int KEYCODE_DPAD_RIGHT = 22;
    public static final int KEYCODE_DPAD_CENTER = 23;
    /* AndroidAuto2KeyEventsController.getKeyState(): PRESSED -> 0, RELEASED -> 1 */
    public static final int BUTTON_PRESSED = 0;
    public static final int BUTTON_RELEASED = 1;

    private static final String[] SD_ROOTS = {"/fs/sda0/", "/net/mmx/fs/sda0/"};
    private static final long FLAG_CHECK_MS = 5000L;
    private static final String MAPS_CLASS = "com.sq5.aa.luka.MirrorGate";
    /* a stream silent this long starts a new stroke even without PRESSED (lost release) */
    private static final long STROKE_GAP_MS = 1000L;

    static final int SESSION_UNKNOWN = 0;
    static final int SESSION_ACTIVE = 1;
    static final int SESSION_INACTIVE = 2;

    private static final Object LOCK = new Object();
    private static final TouchpadGesture GESTURE = new TouchpadGesture();
    private static final StrokeClassifier STROKE = new StrokeClassifier();
    private static DSIAndroidAuto2 target;              /* guarded by LOCK */
    private static int session = SESSION_UNKNOWN;       /* guarded by LOCK */
    private static boolean knobInverted;                /* guarded by LOCK */
    private static boolean off, noTap = true, vRotary = true;         /* guarded by LOCK */
    private static boolean rawAlways, mapsOnly;                        /* guarded by LOCK */
    private static boolean strokeOpen;          /* guarded by LOCK: a stroke is in progress */
    private static boolean strokeMaps;          /* guarded by LOCK: mode latched at stroke start */
    private static int lastMode = -1;           /* guarded by LOCK: for the mode-change log */
    private static long lastSampleMs;           /* guarded by LOCK */
    private static long rejectLogMs = -1000000L;
    /* MirrorGate probe: 0 unresolved, 1 resolved, 2 missing (guarded by LOCK) */
    private static int probeState;
    private static Method probe;
    private static int mapsForTest = -1;        /* host tests: -1 real probe, 0 not Maps, 1 Maps */
    private static long flagsReadMs = -FLAG_CHECK_MS - 1;
    private static boolean firstEventLogged;
    private static boolean gatedLogged;
    private static long gestureStartMs;
    private static int nPosted;

    /* host tests: flags come from here instead of the SD card when non-null */
    private static String[] testFlagRoots;

    static {
        GESTURE.setSink(new TouchpadGesture.Sink() {
            public void step(int dir) {
                post(dir);
            }
        });
    }

    private AaTouchpadInput() {
    }

    /* ------------------------------------------------------------------ optional hooks */

    /**
     * Optional, from AaLukaBridge.attach(): reads the touchpad resolution and knob direction from
     * the terminal-mode configuration.  Without it the resolution is auto-detected. Never throws.
     */
    public static void configure(IContext context) {
        try {
            ITerminalModeConfiguration c = context == null ? null : context.getConfiguration();
            int res = 0;
            boolean hasPad = false;
            boolean inv = false;
            if (c != null) {
                hasPad = c.hasTouchpad();
                res = c.getTouchPadResolutionX();
                inv = c.isKnobDirectionInverted();
            }
            GESTURE.setResolution(res);
            synchronized (LOCK) {
                knobInverted = inv;
            }
            AaLog.log("tp: configured (" + VERSION + ") hasTouchpad=" + hasPad + " res=" + res
                + " knobInverted=" + inv);
        } catch (Throwable t) {
            AaLog.log("tp: configure failed: " + t);
        }
    }

    /** Optional, from AaLukaBridge.updateActiveDeviceState() on luka activate/deactivate. */
    public static void setSessionActive(boolean active) {
        try {
            synchronized (LOCK) {
                session = active ? SESSION_ACTIVE : SESSION_INACTIVE;
                gatedLogged = false;
                strokeOpen = false;
                strokeMaps = false;
            }
            GESTURE.reset();
            STROKE.resetAll();
            AaLog.log("tp: session " + (active ? "active" : "inactive"));
        } catch (Throwable t) {
            /* ignore */
        }
    }

    /* ------------------------------------------------------------------ main entry */

    /**
     * Called by the rebuilt AndroidAuto2KeyEventsController.updateTouchEvents before its stock
     * body.  Never throws; never alters what the stock body does afterwards.
     */
    public static void onTouchEvents(TouchEvent[] events, DSIAndroidAuto2 dsi) {
        try {
            handle(events, dsi, System.currentTimeMillis());
        } catch (Throwable t) {
            try {
                AaLog.log("tp: handler failed: " + t);
            } catch (Throwable t2) {
                /* ignore */
            }
        }
    }

    /**
     * Car 2026-09-28: Google Maps pans the map with the raw touchpad stream that stock forwards via
     * postTouchEvent, so each swipe was a knob step AND a map drag (recenter then needed). The raw
     * pad batch is therefore not forwarded - but ONLY while the current stroke started with Google
     * Maps on screen: every other AA screen needs the raw stream (handwriting). SD flag sq5_tp_raw
     * always forwards. Touchscreen batches are never touched. Called after onTouchEvents for the
     * same batch (the stroke mode is latched there). Never throws.
     */
    public static boolean suppressRawPad(TouchEvent[] events) {
        try {
            if (events == null || events.length == 0) return false;
            for (int i = 0; i < events.length; i++) {
                if (events[i] == null || events[i].isTouchScreen()) return false;
            }
            synchronized (LOCK) {
                if (off || session == SESSION_INACTIVE || rawAlways) return false;
                return strokeMaps;
            }
        } catch (Throwable t) {
            return false;
        }
    }

    static void handle(TouchEvent[] events, DSIAndroidAuto2 dsi, long now) {
        if (events == null || events.length == 0) return;
        int pads = 0;
        TouchEvent pad = null;
        for (int i = 0; i < events.length; i++) {
            TouchEvent e = events[i];
            if (e != null && !e.isTouchScreen()) {
                pads++;
                if (pad == null) pad = e;
            }
        }
        if (pads == 0) return;                      /* touchscreen only: stock path */

        String gate = null;
        synchronized (LOCK) {
            readFlags(now);
            if (off) gate = "sq5_tp_off";
            else if (session == SESSION_INACTIVE) gate = "no Android Auto session";
            else if (dsi == null) gate = "no DSI";
            if (gate != null) {
                if (!gatedLogged) {
                    gatedLogged = true;
                    AaLog.log("tp: touchpad ignored (" + gate + ")");
                }
            } else {
                gatedLogged = false;
                target = dsi;
            }
        }
        if (gate != null) {
            GESTURE.reset();
            STROKE.reset();
            synchronized (LOCK) {
                strokeOpen = false;
                strokeMaps = false;
            }
            return;
        }
        int state = pad.getTouchState();
        int x = pad.getCurrentX();
        int y = pad.getCurrentY();

        /* stroke mode, latched at the stroke's first sample (the Maps probe runs outside LOCK) */
        boolean start;
        synchronized (LOCK) {
            start = state == TouchpadGesture.STATE_PRESSED || !strokeOpen
                || now - lastSampleMs > STROKE_GAP_MS || now < lastSampleMs;
            lastSampleMs = now;
        }
        if (start) {
            boolean m = mapsOnScreen();
            synchronized (LOCK) {
                strokeMaps = m;
                if (lastMode != (m ? 1 : 0)) {
                    lastMode = m ? 1 : 0;
                    AaLog.log(m ? "tp: AA screen = Google Maps (swipe -> knob steps, raw pad suppressed)"
                        : "tp: AA screen not Maps (raw pad forwarded, flick -> knob step at release)");
                }
            }
        }
        boolean maps, tapOn, onlyMaps;
        synchronized (LOCK) {
            strokeOpen = state != TouchpadGesture.STATE_RELEASED;
            maps = strokeMaps;
            tapOn = !noTap;
            onlyMaps = mapsOnly;
        }
        if (!maps) {
            GESTURE.reset();
            if (onlyMaps) STROKE.reset();
            else stroke(pads, state, x, y, now, start, tapOn);
            return;
        }
        STROKE.reset();

        GESTURE.setTapEnabled(tapOn);           /* outside LOCK: lock order is GESTURE -> LOCK */
        if (pads > 1) {                             /* multi-finger: not a navigation swipe */
            GESTURE.reset();
            return;
        }
        if (!firstEventLogged) {
            firstEventLogged = true;
            AaLog.log("tp: first touchpad event state=" + state + " x=" + x + " y=" + y
                + " res=" + GESTURE.resolution());
        }
        if (state == TouchpadGesture.STATE_PRESSED) gestureStartMs = now;
        boolean ended = GESTURE.onSample(state, x, y, now);
        if (ended) {
            String stats = GESTURE.takeStats();
            if (GESTURE.gestureSteps() > 0) {
                AaLog.log("tp: gesture " + stats + " (" + (now - gestureStartMs) + " ms, res="
                    + GESTURE.resolution() + (vRotary ? ", vrotary" : "") + ")");
            }
        }
    }

    /** Non-Maps screen: raw stream goes to AA as stock; only a clear flick steps, at release. */
    private static void stroke(int pads, int state, int x, int y, long now, boolean start, boolean tapOn) {
        if (pads > 1) {
            STROKE.invalidate("multi-finger");
            return;
        }
        GESTURE.noteCoords(x, y);
        int res = GESTURE.resolution();
        if (state == TouchpadGesture.STATE_PRESSED) {
            STROKE.begin(x, y, now, true);
            return;
        }
        if (state == TouchpadGesture.STATE_RELEASED) {
            if (!STROKE.active()) return;
            int n = STROKE.end(x, y, now, res, tapOn);
            int dir = STROKE.dir();
            for (int i = 0; i < n; i++) post(dir);
            if (n > 0) {
                AaLog.log("tp: " + STROKE.reason() + " -> " + n + "x dir " + dir + " (" + STROKE.stats() + ")");
            } else if (!"still".equals(STROKE.reason()) && now - rejectLogMs >= 2000L) {
                rejectLogMs = now;                  /* handwriting: at most one line per 2 s */
                AaLog.log("tp: stroke kept raw only (" + STROKE.reason() + ", " + STROKE.stats() + ")");
            }
            return;
        }
        if (start || !STROKE.active()) STROKE.begin(x, y, now, false);
        else STROKE.add(x, y, now, res);
    }

    /** Google Maps on the AA screen? MirrorGate via reflection; missing/failing -> false. */
    private static boolean mapsOnScreen() {
        Method m;
        synchronized (LOCK) {
            if (mapsForTest >= 0) return mapsForTest == 1;
            if (probeState == 0) {
                try {
                    probe = Class.forName(MAPS_CLASS).getMethod("isMapsOnScreen", new Class[0]);
                    probeState = 1;
                } catch (Throwable t) {
                    probeState = 2;
                    AaLog.log("tp: " + MAPS_CLASS + ".isMapsOnScreen unavailable (" + t
                        + ") - every screen treated as not Maps");
                }
            }
            if (probeState != 1) return false;
            m = probe;
        }
        try {
            return Boolean.TRUE.equals(m.invoke(null, new Object[0]));
        } catch (Throwable t) {
            return false;
        }
    }

    /* ------------------------------------------------------------------ output */

    private static void post(int dir) {
        DSIAndroidAuto2 dsi;
        boolean rot, inv, tapOff;
        synchronized (LOCK) {
            dsi = target;
            rot = vRotary;
            inv = knobInverted;
            tapOff = noTap;
        }
        if (dsi == null) return;
        if (rot && dir != TouchpadGesture.SELECT) {
            /* Car 2026-09-28: AA ignored DPAD 19-22 (this HU declares a rotary controller, no D-pad),
             * so every swipe is a knob step by default. Same sign rule as TMVirtualButtonListener:
             * increment -> -1 (inverted: +1); swipe down/right = knob increment. */
            int inc = inv ? 1 : -1;
            boolean fwd = dir == TouchpadGesture.DOWN || dir == TouchpadGesture.RIGHT;
            dsi.postRotaryEvent(fwd ? inc : -inc);
            nPosted++;
            return;
        }
        int key;
        switch (dir) {
            case TouchpadGesture.UP: key = KEYCODE_DPAD_UP; break;
            case TouchpadGesture.DOWN: key = KEYCODE_DPAD_DOWN; break;
            case TouchpadGesture.LEFT: key = KEYCODE_DPAD_LEFT; break;
            case TouchpadGesture.RIGHT: key = KEYCODE_DPAD_RIGHT; break;
            case TouchpadGesture.SELECT:
                if (tapOff) return;
                key = KEYCODE_DPAD_CENTER;
                break;
            default: return;
        }
        dsi.postButtonEvent(key, BUTTON_PRESSED);
        dsi.postButtonEvent(key, BUTTON_RELEASED);
        nPosted++;
    }

    /* ------------------------------------------------------------------ flags */

    private static void readFlags(long now) {
        if (now - flagsReadMs < FLAG_CHECK_MS && now >= flagsReadMs) return;
        flagsReadMs = now;
        boolean o = flag("sq5_tp_off");
        /* defaults from the car: swipes -> knob steps, touch-tap does NOT select (the pad's physical
         * click already selects via the stock path). sq5_tp_dpad -> DPAD keys, sq5_tp_tap -> tap selects */
        boolean nt = !flag("sq5_tp_tap");
        boolean vr = !flag("sq5_tp_dpad");
        boolean ra = flag("sq5_tp_raw");
        boolean mo = flag("sq5_tp_mapsonly");
        if (o != off || nt != noTap || vr != vRotary || ra != rawAlways || mo != mapsOnly) {
            AaLog.log("tp: flags off=" + o + " notap=" + nt + " vrotary=" + vr + " raw=" + ra + " mapsonly=" + mo);
        }
        off = o;
        noTap = nt;
        vRotary = vr;
        rawAlways = ra;
        mapsOnly = mo;
    }

    private static boolean flag(String name) {
        String[] roots = testFlagRoots != null ? testFlagRoots : SD_ROOTS;
        for (int i = 0; i < roots.length; i++) {
            try {
                if (new File(roots[i] + name).exists()) return true;
            } catch (Throwable t) {
                /* ignore */
            }
        }
        return false;
    }

    /* ------------------------------------------------------------------ host-test support */

    /** Host tests: flag directory (with trailing separator) replacing the SD roots; resets state. */
    public static void resetForTest(String flagRoot) {
        synchronized (LOCK) {
            testFlagRoots = flagRoot == null ? null : new String[]{flagRoot};
            target = null;
            session = SESSION_UNKNOWN;
            knobInverted = false;
            off = false;
            noTap = false;
            vRotary = false;
            rawAlways = false;
            mapsOnly = false;
            strokeOpen = false;
            strokeMaps = false;
            lastMode = -1;
            lastSampleMs = 0;
            probeState = 0;
            probe = null;
            flagsReadMs = -FLAG_CHECK_MS - 1;
            firstEventLogged = false;
            gatedLogged = false;
            nPosted = 0;
        }
        GESTURE.setResolution(0);
        GESTURE.resetCalibration();
        GESTURE.setTapEnabled(true);
        GESTURE.reset();
        GESTURE.takeStats();
        STROKE.resetAll();
    }

    /** Host tests: -1 = real MirrorGate probe, 0 = not Maps, 1 = Google Maps on screen. */
    public static void setMapsForTest(int mode) {
        synchronized (LOCK) {
            mapsForTest = mode;
        }
    }

    /** Host tests: force a flag re-read on the next event. */
    public static void rereadFlagsForTest() {
        synchronized (LOCK) {
            flagsReadMs = -FLAG_CHECK_MS - 1;
        }
    }

    /** Host tests: set resolution / knob direction without an IContext. */
    public static void configureForTest(int res, boolean inverted) {
        GESTURE.setResolution(res);
        synchronized (LOCK) {
            knobInverted = inverted;
        }
    }

    /** Host tests: synthetic clock entry. */
    public static void handleForTest(TouchEvent[] events, DSIAndroidAuto2 dsi, long now) {
        handle(events, dsi, now);
    }
}
