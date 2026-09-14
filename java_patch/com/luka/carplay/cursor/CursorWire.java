package com.luka.carplay.cursor;

/** Fixed-size, big-endian messages on the private cursor_render TCP link. */
public final class CursorWire {
    public static final int DISPLAYABLE = 103;
    static final int HIDE = 0, PROBE = 1, SHOW = 2, LOADER = 3;
    static final int LOADER_DONE = 4, TOUCH = 1, RELEASED = 2, DISMISS = 3, FORCE_HIDE = 4;
    static final int FRAME_SIZE = 40, ACK_SIZE = 32;
    private CursorWire() { }

    static byte[] encode(int mode, int seq, int token, int canvasW, int canvasH,
            int vx, int vy, int vw, int vh, int x, int y, int flags, int activity, int revision) {
        byte[] p = new byte[FRAME_SIZE];
        p[0] = 2; p[1] = (byte)mode; put16(p, 2, DISPLAYABLE);
        put32(p, 4, seq); put32(p, 8, token);
        put16(p, 12, canvasW); put16(p, 14, canvasH);
        put16(p, 16, vx); put16(p, 18, vy); put16(p, 20, vw); put16(p, 22, vh);
        put16(p, 24, x); put16(p, 26, y); p[28] = (byte)flags;
        put32(p, 32, activity); put32(p, 36, revision);
        return p;
    }
    static int get16(byte[] p, int offset) { return ((p[offset] & 255) << 8) | (p[offset+1] & 255); }
    static int get32(byte[] p, int offset) {
        return ((p[offset] & 255) << 24) | ((p[offset + 1] & 255) << 16)
            | ((p[offset + 2] & 255) << 8) | (p[offset + 3] & 255);
    }
    private static void put16(byte[] p, int offset, int v) { p[offset] = (byte)(v >> 8); p[offset + 1] = (byte)v; }
    private static void put32(byte[] p, int offset, int v) { put16(p, offset, v >> 16); put16(p, offset + 2, v); }
}
