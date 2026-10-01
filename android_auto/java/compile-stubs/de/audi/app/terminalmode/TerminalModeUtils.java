package de.audi.app.terminalmode;

import de.audi.app.terminalmode.keyevents.Key;

/**
 * COMPILE-ONLY stand-in - never shipped (see compile-stubs/README.txt).  Superset of
 * aa-luka/input/compile-stubs/de/audi/app/terminalmode/TerminalModeUtils.java.
 */
public final class TerminalModeUtils {
    private TerminalModeUtils() {
    }

    public static boolean isJoystickMiddleposition(Key key) {
        return key != null && key.is(Key.JS_MIDDLE);
    }

    public static boolean isJoystick(Key key) {
        return key != null && (key.isOneOf(Key.JS_NORTH, Key.JS_SOUTH, Key.JS_EAST, Key.JS_WEST));
    }

    public static int mapActiveSmartphoneTypeToEntertainmentAudioModelType(SmartphoneManager.SmartphoneType type) {
        return 0;
    }
}
