package com.luka.carplay.cursor;

import com.luka.carplay.core.CarPlayApp;
import com.luka.carplay.framework.Log;
import de.audi.atip.base.IFrameworkAccess;

/** HU pointer state. NativeCursorOverlay gates commands; cursor_render owns plane 103. */
public final class HuCursor {
    public interface TouchSink { void postScreenTouch(int count, int x, int y); }
    private static final HuCursor INSTANCE = new HuCursor();
    public static HuCursor getInstance() { return INSTANCE; }

    private final PointerState state = new PointerState();
    private final PointerClick click = new PointerClick();
    private final NativeCursorOverlay overlay = new NativeCursorOverlay(this);
    private final HuStartup startup = new HuStartup();
    private TouchSink sink;
    private boolean configured, failed, rendered;
    private int lifecycleGeneration;
    private int offsetX, offsetY, drawnX, drawnY;
    private int width, height, canvasW, canvasH;

    private HuCursor() { }

    public synchronized void activate(int generation) {
        if (!CarPlayApp.isCurrentSession(generation)) return;
        // Retire commands/ACKs from the previous activation before reusing 103.
        overlay.stop(); click.abandon(); state.reset(); rendered = failed = false;
        lifecycleGeneration = generation;
        startup.begin(generation);
        overlay.start(); overlay.request();
    }
    public void frameworkReady() { overlay.request(); }
    public synchronized void deactivateSession(int generation) {
        if (generation == lifecycleGeneration) deactivate();
    }

    synchronized Frame startupFrame(IFrameworkAccess fw, int screen, int context) {
        return startup.frame(fw, screen, context, frame(), overlay.loaderPresented());
    }
    synchronized boolean starting() { return startup.running(); }
    synchronized void loaderPosted(Frame frame, boolean complete) { startup.posted(frame, complete); }
    synchronized boolean current(Frame frame) {
        return frame != null && frame.generation == lifecycleGeneration && !failed
            && CarPlayApp.isCurrentSession(lifecycleGeneration);
    }
    synchronized void rejected(Frame frame, int status) {
        if (!current(frame)) return;
        if (status == -1) failed(new IllegalStateException("native plane creation/presentation failed"));
        else detached();
    }

    public synchronized void configure(int width, int height, int padWidth,
            int ox, int oy, int fullWidth, int fullHeight, TouchSink destination) {
        click.reset();
        configured = width >= 320 && height >= 200 && padWidth >= 64
            && ox >= 0 && oy >= 0 && fullWidth >= ox + width && fullHeight >= oy + height
            && fullWidth <= 1920 && fullHeight <= 1080;
        state.configure(configured ? width : 1024, configured ? height : 480,
            configured ? padWidth : 1024);
        offsetX = ox; offsetY = oy;
        this.width = width; this.height = height; canvasW = fullWidth; canvasH = fullHeight;
        sink = destination;
        failed = false; rendered = false;
        overlay.start();
        overlay.request();
    }

    public synchronized boolean onOneFinger(int x, int y) {
        if (!configured || failed || startup.running() || sink == null || !CarPlayApp.isActive()
                || !NativeCursorOverlay.isForeground()) return false;
        state.touch(x, y);
        overlay.request();
        return !failed;
    }

    public synchronized void onTouchEnd() {
        state.end();
        overlay.request();
    }

    /** Use only for the centre-console select, after the existing MFW filter. */
    public synchronized boolean onSelect(boolean pressed) {
        boolean consumed;
        if (pressed) {
            final TouchSink destination = sink;
            consumed = click.press(CarPlayApp.isActive() && !startup.running() && rendered
                    && overlay.presented() && NativeCursorOverlay.isForeground() && !failed, drawnX, drawnY,
                destination == null ? null : new PointerClick.Sink() {
                    public void touch(int count, int x, int y) {
                        send(destination, count, x, y);
                    }
                });
            // Retire clicking now; native fades the visible ring. Release stays latched.
            state.hide(); rendered = false;
            overlay.request();
        } else {
            consumed = click.release();
        }
        return consumed;
    }

    public synchronized boolean ownsSelect() { return click.ownsPress(); }

    public synchronized void hideForStockInput() {
        click.cancel();
        state.hide();
        rendered = false;
        overlay.request();
    }

    public synchronized void onRotary() {
        click.cancel(); state.forceHide(); rendered = false;
        // Rotary dismisses only the pointer; a startup PRISM scene has its own lifecycle.
        if (startup.running()) overlay.request(); else overlay.forceHide();
    }

    public synchronized void deactivate() {
        lifecycleGeneration = 0;
        startup.end();
        click.abandon();
        state.reset();
        rendered = false; failed = false;
        overlay.stop();
        overlay.request();
    }

    public synchronized void uninstall() {
        click.cancel();
        deactivate();
        sink = null;
        configured = false;
    }

    synchronized Frame frame() {
        return new Frame(state.x(), state.y(), offsetX, offsetY, width, height, canvasW, canvasH,
            configured && !failed && CarPlayApp.isActive() ? state.input() : CursorWire.DISMISS,
            CursorWire.SHOW, lifecycleGeneration, state.activity(), state.revision());
    }

    synchronized void rendered(Frame frame, int x, int y) {
        if (frame.generation != lifecycleGeneration || !CarPlayApp.isCurrentSession(lifecycleGeneration)
                || failed || startup.running() || state.input() >= CursorWire.DISMISS
                || frame.revision != state.revision() || x < 0 || y < 0 || x >= width || y >= height) return;
        rendered = true; drawnX = x; drawnY = y;
    }

    synchronized void transportReset() { click.cancel(); state.hide(); rendered = false; }

    synchronized void detached() { rendered = false; }

    synchronized void screenLeft() {
        click.cancel();
        state.hide();
        rendered = false;
    }

    synchronized void failed(Throwable problem) {
        startup.failed(String.valueOf(problem));
        if (!failed) Log.w("HuCursor", "native cursor unavailable; using touchpad DPAD: " + problem);
        failed = true;
        click.cancel();
        state.hide();
        rendered = false;
    }

    private static void send(TouchSink destination, int count, int x, int y) {
        try { destination.postScreenTouch(count, x, y); }
        catch (Throwable t) { Log.w("HuCursor", "touch " + count + " failed: " + t); }
    }

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
