/*
 * CarPlay Cursor Client
 *
 * Java-side API for driving the on-screen cursor overlay rendered by the
 * hook.  Commands travel over CarplayBus (TCP 127.0.0.1:19810) as binary
 * frames.
 *
 * Binary payload layout (CMD_CURSOR_POS):
 *   0..3  i32 x (big-endian)  screen pixels
 *   4..7  i32 y (big-endian)  screen pixels
 *
 * CMD_CURSOR_HIDE carries no payload.
 *
 * Typical wiring (from TerminalModeDSIKeyEventsController$ patch):
 *
 *   CursorClient c = CursorClient.getInstance();
 *   c.show(x, y);                 // touchpad: finger moved, move cursor
 *   c.hide();                     // grace timer expired
 *
 * Clicks from the knob are still handled on the Java side — when the
 * cursor is visible, the DDS_SELECT press is converted into a touch
 * event at (x, y) via the existing postTouchEvent() path instead of
 * being sent to the hook.  The cursor overlay itself keeps running
 * independent of click handling.
 */
package com.luka.carplay.cursor;

import com.luka.carplay.framework.CarplayBus;
import com.luka.carplay.framework.Log;

public class CursorClient {

    private static final String TAG = "CursorClient";

    private static final CursorClient INSTANCE = new CursorClient();
    public static CursorClient getInstance() { return INSTANCE; }
    private CursorClient() {}

    private volatile boolean lastVisible = false;
    private volatile int lastX = 0, lastY = 0;

    /** Show cursor at (x, y) in screen pixel coordinates. */
    public void show(int x, int y) {
        byte[] payload = new byte[8];
        putBE32(payload, 0, x);
        putBE32(payload, 4, y);
        CarplayBus.getInstance().send(CarplayBus.CMD_CURSOR_POS,
                                      CarplayBus.FLAG_BINARY, payload, 8);
        if (!lastVisible) Log.d(TAG, "cursor SHOW at " + x + "," + y);
        lastVisible = true;
        lastX = x;
        lastY = y;
    }

    /** Hide cursor. */
    public void hide() {
        if (lastVisible) {
            Log.d(TAG, "cursor HIDE");
        }
        lastVisible = false;
        CarplayBus.getInstance().sendBare(CarplayBus.CMD_CURSOR_HIDE, 0);
    }

    public boolean isVisible() { return lastVisible; }
    public int getX() { return lastX; }
    public int getY() { return lastY; }

    private static void putBE32(byte[] b, int o, int v) {
        b[o]   = (byte)((v >> 24) & 0xFF);
        b[o+1] = (byte)((v >> 16) & 0xFF);
        b[o+2] = (byte)((v >> 8) & 0xFF);
        b[o+3] = (byte)(v & 0xFF);
    }
}
