/*
 * CarPlay Cursor Client
 *
 * Java-side API for driving the on-screen cursor overlay rendered by the
 * hook.  Commands travel over CarplayBus (TCP 127.0.0.1:19810) as binary
 * frames.
 *
 * Binary payload layout (CMD_CURSOR_POS, 12 bytes, big-endian):
 *   0..3  i32  x       (screen pixels)
 *   4..7  i32  y       (screen pixels)
 *   8     u8   alpha   (0..255, 255=opaque)
 *   9..11 u8   rsvd    (must be 0)
 *
 * CMD_CURSOR_HIDE carries no payload — on the C side it triggers a short
 * native fade-out animation (~300 ms) before the cursor disappears.
 *
 * Typical wiring (from TerminalModeDSIKeyEventsController$ patch):
 *
 *   CursorClient c = CursorClient.getInstance();
 *   c.show(x, y);              // touchpad: finger moved — full opacity
 *   c.hide();                  // grace timer expired — C fades out
 *
 * Clicks from the knob are still handled on the Java side — when the
 * cursor is visible, the DDS_SELECT press is converted into a touch
 * event at (x, y) via the existing postTouchEvent() path instead of
 * being sent to the hook.  The cursor overlay keeps running independent
 * of click handling.
 */
package com.luka.carplay.cursor;

import com.luka.carplay.framework.CarplayBus;
import com.luka.carplay.framework.Log;

public class CursorClient {

    private static final String TAG = "CursorClient";
    private static final int POS_PAYLOAD_LEN = 12;

    private static final CursorClient INSTANCE = new CursorClient();
    public static CursorClient getInstance() { return INSTANCE; }
    private CursorClient() {}

    private volatile boolean lastVisible = false;
    private volatile int lastX = 0, lastY = 0;
    private volatile int lastAlpha = 255;

    /** Show cursor at (x, y), fully opaque. */
    public void show(int x, int y) {
        show(x, y, 255);
    }

    /**
     * Show cursor at (x, y) with the given alpha (0..255).  Normal tracking
     * sends alpha=255; the C side handles fade-out animation on its own
     * when it receives CMD_CURSOR_HIDE, so Java does not usually need to
     * send intermediate alpha values.  Provided for completeness.
     */
    public void show(int x, int y, int alpha) {
        if (alpha < 0)   alpha = 0;
        if (alpha > 255) alpha = 255;

        byte[] payload = new byte[POS_PAYLOAD_LEN];
        putBE32(payload, 0, x);
        putBE32(payload, 4, y);
        payload[8]  = (byte)alpha;
        payload[9]  = 0;
        payload[10] = 0;
        payload[11] = 0;
        CarplayBus.getInstance().send(CarplayBus.CMD_CURSOR_POS,
                                      CarplayBus.FLAG_BINARY,
                                      payload, POS_PAYLOAD_LEN);

        if (!lastVisible) Log.d(TAG, "cursor SHOW at " + x + "," + y + " a=" + alpha);
        lastVisible = true;
        lastX = x;
        lastY = y;
        lastAlpha = alpha;
    }

    /** Hide cursor — triggers native fade-out on the C side. */
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
    public int getAlpha() { return lastAlpha; }

    private static void putBE32(byte[] b, int o, int v) {
        b[o]   = (byte)((v >> 24) & 0xFF);
        b[o+1] = (byte)((v >> 16) & 0xFF);
        b[o+2] = (byte)((v >> 8) & 0xFF);
        b[o+3] = (byte)(v & 0xFF);
    }
}
