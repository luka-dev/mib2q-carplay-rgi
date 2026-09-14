package com.luka.carplay.cursor;

/** Input coordinates and gesture ordering only. Native code owns visibility/time. */
final class PointerState {
    private int width = 1024, height = 480, padWidth = 1024;
    private float x = 512, y = 240;
    private int anchorX, anchorY, activity, revision, input;
    private boolean touching, suppressed;

    void configure(int w, int h, int pad) {
        width = w; height = h; padWidth = pad;
        x = (w - 1) / 2.0f; y = (h - 1) / 2.0f;
        reset();
    }
    void touch(int px, int py) {
        if (!touching) {
            touching = true; anchorX = px; anchorY = py;
            if (!suppressed) moved();
            return;
        }
        int dx = px - anchorX, dy = py - anchorY;
        anchorX = px; anchorY = py;
        if (suppressed || (dx == 0 && dy == 0)) return;
        float gain = (float)width / padWidth;
        x = clamp(x + dx * gain, width - 1);
        y = clamp(y + dy * gain, height - 1);
        moved();
    }
    private void moved() {
        if (++activity == 0) ++activity;
        ++revision; input = CursorWire.TOUCH;
    }
    void end() {
        if (touching && !suppressed) { input = CursorWire.RELEASED; ++revision; }
        touching = suppressed = false;
    }
    void hide() { hide(false); }
    void forceHide() { hide(true); }
    private void hide(boolean force) {
        int next = force || input == CursorWire.FORCE_HIDE ? CursorWire.FORCE_HIDE : CursorWire.DISMISS;
        if (input != next) { input = next; ++revision; }
        // Rotary/select during a held contact suppresses its remaining motion.
        suppressed = touching;
    }
    void reset() { touching = suppressed = false; input = CursorWire.DISMISS; ++revision; }
    int x() { return Math.round(x); }
    int y() { return Math.round(y); }
    int input() { return input; }
    int activity() { return activity; }
    int revision() { return revision; }
    private static float clamp(float v, int max) { return v < 0 ? 0 : v > max ? max : v; }
}
