package com.luka.carplay.cursor;

/** Latches the destination for the entire physical press/release pair. */
final class PointerClick {
    interface Sink { void touch(int count, int x, int y); }
    private int route; // 0 idle, 1 stock, 2 synthetic touch, 3 cancelled touch
    private int x, y;
    private Sink sink;

    boolean press(boolean visible, int px, int py, Sink destination) {
        if (route != 0) return route != 1;
        if (!visible || destination == null) { route = 1; return false; }
        route = 2;
        x = px; y = py; sink = destination;
        sink.touch(1, x, y);
        return true;
    }

    boolean release() {
        int previous = route;
        route = 0;
        Sink old = sink;
        sink = null;
        if (previous == 2) old.touch(0, x, y);
        return previous == 2 || previous == 3;
    }

    void cancel() {
        if (route != 2) return;
        route = 3; // Eat the eventual physical release, even after switching modes.
        Sink old = sink;
        sink = null;
        old.touch(0, x, y);
    }

    void reset() { cancel(); route = 0; sink = null; }

    boolean ownsPress() { return route == 2 || route == 3; }

    // The CarPlay session is already ending. Do not inject through its proxy,
    // which might have rebound to another phone by the time a queued job runs.
    void abandon() { if (route == 2) route = 3; sink = null; }
}
