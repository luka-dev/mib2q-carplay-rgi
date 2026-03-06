/*
 * CarPlay Cover Art Module
 *
 * Watches PPS notification for cover art updates, pushes to BAP service.
 * Used by TerminalModeBapCombi$EventListener.
 *
 * PPS notify: /ramdisk/pps/iap2/coverart_notify
 * PNG file:   /tmp/coverart.png
 *
 */
package com.luka.carplay.coverart;

import com.luka.carplay.framework.Log;
import com.luka.carplay.framework.PPS;

public class CoverArt implements PPS.Listener {

    private static final String TAG = "CoverArt";
    private static final String PPS_NOTIFY = "/ramdisk/pps/iap2/coverart_notify";
    public static final String COVERART_PATH = "/var/app/icab/tmp/37/coverart.png";

    /* Singleton */
    private static CoverArt instance;

    /* State */
    private PPS pps;
    private volatile boolean running;
    private volatile long lastCrc = 0;
    private volatile int artId = 0;

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

        pps = new PPS(PPS_NOTIFY, PPS.TEXT, 1024, this);
        pps.start();

        Log.i(TAG, "Started");
    }

    public void stop() {
        if (!running) return;
        running = false;

        if (pps != null) {
            pps.stop();
            pps = null;
        }

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
        return COVERART_PATH;
    }

    public int getArtId() {
        return artId;
    }

    /* ============================================================
     * PPS Listener
     * ============================================================ */

    public void onData(byte[] raw, int len, PPS.Data parsed) {
        if (parsed == null) return;

        long crc = parsed.num64("crc", 0);
        if (crc == 0 || crc == lastCrc) return;

        lastCrc = crc;
        artId = (int) crc;  /* Use CRC as picture ID like reference implementation */

        Log.i(TAG, "New cover art: crc=" + Long.toHexString(crc) + " artId=" + artId);

        /* Notify callback */
        if (callback != null) {
            try {
                callback.onNewCoverArt(COVERART_PATH, artId);
            } catch (Exception e) {
                Log.e(TAG, "Callback error", e);
            }
        }
    }

    public void onError(String reason) {
        Log.w(TAG, "PPS error: " + reason);
    }
}
