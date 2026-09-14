package com.luka.carplay.cursor;
public final class HuCursor {
    static final class Frame {
        final int x, y, offsetX, offsetY, width, height, canvasW, canvasH;
        final int mode, generation, flags, activity, revision;
        Frame(int px, int py, int ox, int oy, int w, int h, int cw, int ch,
                int flags, int mode, int generation, int activity, int revision) {
            this.mode = mode; this.generation = generation;
            this.flags = flags; this.activity = activity; this.revision = revision;
            x = px; y = py; offsetX = ox; offsetY = oy;
            width = w; height = h; canvasW = cw; canvasH = ch;
        }
    }
}
