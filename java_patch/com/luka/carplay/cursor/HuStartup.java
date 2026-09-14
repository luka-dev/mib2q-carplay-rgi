package com.luka.carplay.cursor;

import com.luka.carplay.bus.CarplayBus;
import com.luka.carplay.core.CarPlayApp;
import com.luka.carplay.framework.Log;
import com.luka.carplay.startup.HuMenuIdentity;
import de.audi.atip.base.IFrameworkAccess;
import de.audi.atip.hmi.modelaccess.ChoiceModelApp;

/** Main-HU startup scene, driven on the HMI dispatcher by NativeCursorOverlay.
 * After render ACK, publish UI identity and request its screen in one dispatcher
 * operation: TerminalMode's no-connection guard also reads model 4239. */
final class HuStartup implements CarplayBus.Listener {
    private static final int VIDEO_STATUS = CarplayBus.EVT_HU_VIDEO_STATUS;
    private static final int QUERY_VIDEO = CarplayBus.CMD_HU_VIDEO_STATUS;
    private int generation, queryNonce, videoEpoch, initialScreen;
    private boolean active, loading, prepared, entered, requested, videoReady;
    private boolean awaitingVideo, haveStatus;
    private long startedAt, requestedAt, lastQuery, lastReply, clock;

    synchronized void begin(int id) {
        generation = id;
        active = loading = true;
        prepared = entered = requested = videoReady = false;
        awaitingVideo = haveStatus = false;
        startedAt = -1; initialScreen = -1;
        requestedAt = lastQuery = lastReply = clock = 0;
        ++queryNonce; // Invalidate a reply still queued from the previous attempt.
        HuMenuIdentity.begin(id);
        CarplayBus.getInstance().on(VIDEO_STATUS, this);
    }
    synchronized void end() {
        active = loading = false; ++queryNonce;
        HuMenuIdentity.end(generation);
    }
    synchronized void failed(String reason) {
        if (!loading) return;
        loading = false; ++queryNonce;
        HuMenuIdentity.fallback(generation);
        Log.w("HuStartup", reason + "; released loader to stock HMI");
    }
    synchronized void posted(HuCursor.Frame frame, boolean complete) {
        if (!active || !loading || frame.generation != generation) return;
        prepared = true;
        if (complete && entered && videoReady && frame.flags == 1 && frame.revision == videoEpoch)
            loading = false; // Native reports completion; Java never times the reveal.
    }
    synchronized boolean running() { return active && loading; }

    synchronized HuCursor.Frame frame(IFrameworkAccess fw, int screen, int context,
            HuCursor.Frame geometry, boolean planeReady) {
        if (fw == null) return null;
        HuMenuIdentity.bind(fw.getHmiServiceApp());
        if (!active || !loading || !CarPlayApp.isActive()) return null;
        if (fw.getKombiType() == 4) { failed("G24 uses stock HU presentation"); return null; }
        long now = fw.getMonotonicTime();
        clock = now;
        if (startedAt < 0) { startedAt = now; lastQuery = now - 200; }
        if (initialScreen < 0 && screen >= 0) initialScreen = screen;
        if (now - startedAt > 15000) { failed("first main video frame timed out"); return null; }
        // A missing/old hook must not cover working stock video for 15 seconds.
        if (now - (haveStatus ? lastReply : startedAt) > 1000) {
            failed("main video observer unavailable"); return null;
        }
        boolean foreground = screen == 3200000 && context == 78;
        if (entered && !foreground) {
            // HOME/popups/context changes must win. Never re-enter on a timer.
            loading = false; ++queryNonce;
            return null;
        }
        if (!requested && screen >= 0 && initialScreen >= 0 && screen != initialScreen
                && screen != 3200000) {
            failed("user changed screen before loader entry"); return null;
        }
        if (requested && !planeReady) {
            failed("loader plane lease lost"); return null;
        }
        if (prepared && planeReady && !requested) {
            requested = true; requestedAt = now;
            // This is presentation identity, not DSISmartphoneIntegration STARTED.
            // It must precede screen entry, whose stock guard otherwise sees "no phone".
            HuMenuIdentity.shown(generation);
            if (!foreground && screen != 3200000) {
                ChoiceModelApp open = fw.getHmiServiceApp().getChoiceModel(4095);
                open.setValue(open.getValue() == 0 ? 1 : 0);
                Log.i("HuStartup", "loader prepared; requested TerminalMode generation=" + generation);
            }
        }
        if (prepared && planeReady && foreground && !entered) {
            entered = true;
            Log.i("HuStartup", "loader context confirmed generation=" + generation);
        }
        if (requested && !entered && now - requestedAt > 2500) {
            failed("HMI did not enter the loader context"); return null;
        }
        if (now - lastQuery >= (awaitingVideo ? 500 : 200)) {
            byte[] p = new byte[4];
            if (++queryNonce == 0) ++queryNonce;
            int n = queryNonce;
            p[0]=(byte)(n>>24); p[1]=(byte)(n>>16); p[2]=(byte)(n>>8); p[3]=(byte)n;
            awaitingVideo = CarplayBus.getInstance().send(QUERY_VIDEO, CarplayBus.FLAG_BINARY, p, p.length);
            lastQuery = now;
        }
        if (geometry.canvasW < 320 || geometry.canvasH < 200) {
            // Framework/session notification can precede DSI configuration.
            if (now - startedAt > 750) failed("main display geometry unavailable");
            return null;
        }
        return new HuCursor.Frame(0, 0, 0, 0, geometry.canvasW, geometry.canvasH,
            geometry.canvasW, geometry.canvasH, entered && videoReady ? 1 : 0,
            CursorWire.LOADER, generation, 0, videoEpoch);
    }
    public synchronized void onFrame(int type, int flags, byte[] p, int len) {
        if (type != VIDEO_STATUS || flags != CarplayBus.FLAG_BINARY || !active || !loading || p == null || len != 12
                || p.length < len || !awaitingVideo || CursorWire.get32(p, 0) != queryNonce
                || clock - lastQuery > 500) return;
        int epoch = CursorWire.get32(p, 4), phase = CursorWire.get32(p, 8);
        if (phase < 0 || phase > 2) return;
        awaitingVideo = false; haveStatus = true; lastReply = clock;
        videoEpoch = epoch; videoReady = phase == 2;
    }
}
