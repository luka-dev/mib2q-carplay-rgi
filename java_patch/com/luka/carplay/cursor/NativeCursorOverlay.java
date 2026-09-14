package com.luka.carplay.cursor;

import com.luka.carplay.core.CarPlayApp;
import de.audi.atip.base.IFrameworkAccess;
import de.audi.atip.hmi.HMIService;
import de.audi.atip.hmi.event.RunnableEvent;
import de.audi.atip.hmi.view.Screen;

/** HMI context gate and coalesced input/state transport to native displayable 103.
 * Creates no HMI widget, EAL node or texture. cursor_render owns the window.
 */
final class NativeCursorOverlay implements Runnable, CursorTransport.Listener {
    private final HuCursor owner;
    private final CursorTransport transport;
    private boolean queued, needsHide = true, presented;
    private int scheduleTicket;
    private long scheduledAt;
    private int seq, inFlight, inFlightMode, token, connection;
    private long sentAt, tokenUntil, firstShowAt, lastSend, pointerUntil;
    private HuCursor.Frame sentFrame, lastFrame;
    private int presentedMode;

    NativeCursorOverlay(HuCursor cursor) {
        owner = cursor;
        transport = new CursorTransport(this);
    }
    void request() { schedule(0); }
    // Rotary takeover bypasses a queued input/ACK. HIDE also revokes its token.
    void forceHide() { hide(); request(); }
    void start() { transport.start(); }
    synchronized boolean presented() {
        return presentedMode == CursorWire.SHOW && livePresentation() && now() < pointerUntil;
    }
    synchronized boolean loaderPresented() {
        return (presentedMode == CursorWire.LOADER || presentedMode == CursorWire.LOADER_DONE) && livePresentation();
    }
    private boolean livePresentation() {
        return presented && token != 0 && connection != 0 && connection == transport.connection()
            && now() < tokenUntil;
    }
    private static long now() {
        IFrameworkAccess fw = CarPlayApp.framework();
        return fw == null ? System.currentTimeMillis() : fw.getMonotonicTime();
    }
    static boolean isForeground() {
        try {
            IFrameworkAccess fw = CarPlayApp.framework();
            HMIService hmi = fw == null ? null : fw.getHMIService();
            Screen screen = hmi == null ? null : hmi.getRootWindow(0).getCurrentScreen();
            return CarPlayApp.isActive() && fw.getKombiType() != 4 && screen != null && screen.getID() == 3200000
                && hmi.getDisplayManager().getCurrentContextID(0) == 78;
        } catch (Throwable unavailable) { return false; }
    }
    void stop() {
        transport.stop();
        synchronized (this) {
            inFlight = token = connection = 0; tokenUntil = firstShowAt = 0;
            needsHide = true; presented = false; lastFrame = sentFrame = null;
        }
    }

    private void schedule(long delay) {
        final int ticket;
        synchronized (this) {
            long time = now();
            if (queued && (delay != 0 || scheduledAt <= time)) return;
            queued = true; scheduledAt = time + delay; ticket = ++scheduleTicket;
        }
        try {
            IFrameworkAccess fw = CarPlayApp.framework();
            if (fw == null || fw.getHMIService() == null) {
                synchronized (this) { queued = false; }
                return;
            }
            fw.getHMIService().getEventDispatcher().postEvent(new RunnableEvent(new Runnable() {
                public void run() {
                    synchronized (NativeCursorOverlay.this) {
                        if (ticket != scheduleTicket) return;
                        queued = false;
                    }
                    NativeCursorOverlay.this.run();
                }
            }), delay);
        } catch (Throwable t) {
            synchronized (this) { queued = false; }
            owner.failed(t);
        }
    }

    public void run() {
        boolean again = false;
        try {
            IFrameworkAccess fw = CarPlayApp.framework();
            HMIService hmi = fw == null ? null : fw.getHMIService();
            Screen screen = hmi == null ? null : hmi.getRootWindow(0).getCurrentScreen();
            boolean foreground = CarPlayApp.isActive() && screen != null && screen.getID() == 3200000
                && hmi.getDisplayManager().getCurrentContextID(0) == 78;
            HuCursor.Frame frame = owner.startupFrame(fw, screen == null ? -1 : screen.getID(),
                hmi == null ? -1 : hmi.getDisplayManager().getCurrentContextID(0));
            boolean loader = frame != null;
            if (!foreground) owner.screenLeft();
            if (frame == null) frame = owner.frame();
            if ((!foreground && !loader) || !owner.current(frame) || frame.canvasW < 320 || frame.canvasH < 200) {
                hide();
            } else {
                // Session teardown uses this same owner lock. An old dispatcher
                // frame must never restart the socket after deactivate stopped it.
                synchronized (owner) {
                    if (owner.current(frame)) { publish(frame); again = true; }
                }
            }
        } catch (Throwable t) {
            owner.failed(t);
            hide();
        }
        // Only screen/session observation and lease keepalive, never animation frames.
        if (again || owner.starting()) schedule(100);
    }

    private void publish(HuCursor.Frame frame) {
        String error = null;
        transport.start();
        boolean replaced;
        synchronized (this) {
            int current = transport.connection();
            replaced = current != connection;
            if (replaced) {
                connection = current; inFlight = token = 0; tokenUntil = 0;
                presented = false; lastFrame = sentFrame = null; firstShowAt = 0;
            }
        }
        if (replaced) { owner.transportReset(); return; }
        synchronized (this) {
            long now = now();
            if (firstShowAt == 0) firstShowAt = now;
            if (inFlight != 0 && now - sentAt > 650) {
                error = "native cursor acknowledgement timed out";
            } else if (firstShowAt != 0 && !presented && now - firstShowAt > 1200) {
                error = "native plane did not present its first frame";
            } else if (connection != 0 && inFlight == 0) {
                if (token == 0 || now >= tokenUntil) {
                    if (!send(CursorWire.PROBE, frame, now)) error = "cursor probe could not be queued";
                } else if (!same(frame, lastFrame) || now - lastSend >= 150) {
                    if (!send(frame.mode, frame, now)) error = "overlay state could not be queued";
                }
            }
        }
        // Never take the owner lock while holding this object's lock (input does the reverse).
        if (error != null) throw new IllegalStateException(error);
    }

    private static boolean same(HuCursor.Frame a, HuCursor.Frame b) {
        return a != null && b != null && a.mode == b.mode && a.generation == b.generation && a.x == b.x && a.y == b.y
            && a.canvasW == b.canvasW && a.canvasH == b.canvasH
            && a.offsetX == b.offsetX && a.offsetY == b.offsetY && a.width == b.width && a.height == b.height
            && a.flags == b.flags && a.activity == b.activity && a.revision == b.revision;
    }

    private boolean send(int mode, HuCursor.Frame frame, long now) {
        if (++seq == 0) ++seq;
        inFlight = seq; inFlightMode = mode; sentAt = now; sentFrame = frame;
        byte[] packet = CursorWire.encode(mode, seq, token, frame.canvasW, frame.canvasH,
            frame.offsetX, frame.offsetY, frame.width, frame.height,
            frame.x, frame.y, frame.flags, frame.activity, frame.revision);
        needsHide = true;
        boolean ok = transport.offer(connection, packet);
        if (!ok) inFlight = 0;
        if (mode == CursorWire.SHOW || mode == CursorWire.LOADER) { lastFrame = frame; lastSend = now; }
        return ok;
    }

    private void hide() {
        synchronized (this) {
            if (needsHide && transport.connection() != 0) {
                if (++seq == 0) ++seq;
                transport.offer(transport.connection(),
                    CursorWire.encode(CursorWire.HIDE, seq, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0));
            }
            needsHide = false; inFlight = 0; token = 0; tokenUntil = 0;
            firstShowAt = 0; presented = false; lastFrame = null; sentFrame = null;
        }
        owner.detached();
    }

    public void cursorAck(int incarnation, byte[] p) {
        if (p == null || p.length != CursorWire.ACK_SIZE) return;
        HuCursor.Frame shown = null;
        HuCursor.Frame completed;
        int status, phase = CursorWire.HIDE;
        synchronized (this) {
            if (incarnation != connection || incarnation != transport.connection()
                    || inFlight == 0 || CursorWire.get32(p, 0) != inFlight) return;
            status = CursorWire.get32(p, 4);
            completed = sentFrame;
            int ttl = CursorWire.get32(p, 12);
            if (ttl < 1 || ttl > 500) return;
            if (now() - sentAt > 650) return;
            token = status >= 0 ? CursorWire.get32(p, 8) : 0;
            if (status < 0) { presented = false; lastFrame = null; }
            // Conservative deadline: measured from send, not delayed acknowledgement arrival.
            tokenUntil = sentAt + ttl;
            if (status == 0 && (inFlightMode == CursorWire.SHOW || inFlightMode == CursorWire.LOADER)) {
                shown = sentFrame;
                phase = CursorWire.get32(p, 16);
                presentedMode = phase;
                presented = true; // A hidden native scene still renews a healthy session.
                int visible = CursorWire.get32(p, 24);
                pointerUntil = sentAt + (visible >= 0 && visible <= ttl ? visible : 0);
            }
            inFlight = 0; sentFrame = null;
        }
        if (shown != null) {
            if (shown.mode == CursorWire.LOADER && (phase == CursorWire.LOADER || phase == CursorWire.LOADER_DONE))
                owner.loaderPosted(shown, phase == CursorWire.LOADER_DONE);
            else if (phase == CursorWire.SHOW && CursorWire.get32(p, 28) == shown.revision)
                owner.rendered(shown, CursorWire.get16(p, 20), CursorWire.get16(p, 22));
            else owner.detached();
        }
        else if (status < 0) owner.rejected(completed, status);
        request();
    }
}
