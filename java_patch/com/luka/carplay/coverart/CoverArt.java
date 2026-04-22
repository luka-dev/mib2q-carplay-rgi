/*
 * CarPlay Cover Art Module
 *
 * Subscribes to EVT_COVERART on CarplayBus, pushes to BAP service.
 * Used by TerminalModeBapCombi$EventListener.
 *
 * Bus: EVT_COVERART carries text payload "crc:n:<uint>\npath:s:<path>\n"
 * PNG file:   /var/app/icab/tmp/37/coverart.png (path always the same,
 *             comes through in payload for sanity).
 */
package com.luka.carplay.coverart;

import com.luka.carplay.framework.CarplayBus;
import com.luka.carplay.framework.Log;

public class CoverArt implements CarplayBus.Listener {

    private static final String TAG = "CoverArt";
    public static final String COVERART_PATH = "/var/app/icab/tmp/37/coverart.png";

    /* Singleton */
    private static CoverArt instance;

    /* State */
    private volatile boolean running;
    private volatile long lastCrc = 0;
    private volatile int artId = 0;
    private volatile String currentPath = COVERART_PATH;

    /* Callback for new cover art */
    private Callback callback;

    public interface Callback {
        void onNewCoverArt(String path, int artId);
    }

    /* ============================================================
     * Singleton
     * ============================================================ */

    public static synchronized CoverArt getInstance() {
        if (instance == null) {
            instance = new CoverArt();
        }
        return instance;
    }

    private CoverArt() {
    }

    /* ============================================================
     * Lifecycle
     * ============================================================ */

    public void start(Callback cb) {
        if (running) return;
        running = true;
        callback = cb;

        CarplayBus bus = CarplayBus.getInstance();
        bus.on(CarplayBus.EVT_COVERART, this);
        bus.start();   /* idempotent */

        Log.i(TAG, "Started (subscribed to EVT_COVERART)");
    }

    public void stop() {
        if (!running) return;
        running = false;
        CarplayBus.getInstance().off(CarplayBus.EVT_COVERART);
        Log.i(TAG, "Stopped");
    }

    public boolean isRunning() {
        return running;
    }

    /* ============================================================
     * State Access
     * ============================================================ */

    public boolean hasCoverArt() {
        return lastCrc != 0;
    }

    public String getPath() {
        return currentPath;
    }

    public int getArtId() {
        return artId;
    }

    /* ============================================================
     * CarplayBus Listener
     * ============================================================ */

    public void onFrame(int type, int flags, byte[] payload, int len) {
        if (type != CarplayBus.EVT_COVERART) return;

        CarplayBus.Data d = CarplayBus.parseText(payload, len);
        long crc = d.num64("crc", 0);
        if (crc == 0 || crc == lastCrc) return;

        lastCrc = crc;
        artId = (int) crc;  /* Use CRC as picture ID like reference implementation */
        String path = d.str("path", COVERART_PATH);
        if (path != null && path.length() > 0) currentPath = path;

        Log.i(TAG, "New cover art: crc=" + Long.toHexString(crc) + " artId=" + artId
                + " path=" + currentPath);

        if (callback != null) {
            try {
                callback.onNewCoverArt(currentPath, artId);
            } catch (Exception e) {
                Log.e(TAG, "Callback error", e);
            }
        }
    }
}
