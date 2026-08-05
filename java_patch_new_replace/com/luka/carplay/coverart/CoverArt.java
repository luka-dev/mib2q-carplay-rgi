/*
 * CarPlay Cover Art Module  (ported from java_patch, foundation-swapped)
 *
 * Subscribes to EVT_COVERART on CarplayBus, pushes to BAP service via a callback.
 * Bus: EVT_COVERART carries text payload "crc:n:<uint>\npath:s:<path>\n"
 * PNG file: /var/app/icab/tmp/37/coverart.png (path echoed in payload for sanity).
 */
package com.luka.carplay.coverart;

import com.luka.carplay.bus.CarplayBus;
import com.luka.carplay.framework.Log;

public class CoverArt implements CarplayBus.Listener {

    private static final String TAG = "CoverArt";
    public static final String COVERART_PATH = "/var/app/icab/tmp/37/coverart.png";

    private static CoverArt instance;

    private volatile boolean running;
    private volatile long lastCrc = 0;
    private volatile int artId = 0;
    private volatile String currentPath = COVERART_PATH;

    private Callback callback;

    public interface Callback {
        void onNewCoverArt(String path, int artId);
    }

    public static synchronized CoverArt getInstance() {
        if (instance == null) instance = new CoverArt();
        return instance;
    }

    private CoverArt() {}

    public synchronized void start(Callback cb) {
        callback = cb;
        if (running) return;
        running = true;

        CarplayBus bus = CarplayBus.getInstance();
        bus.on(CarplayBus.EVT_COVERART, this);
        bus.start();   /* idempotent */

        Log.i(TAG, "Started (subscribed to EVT_COVERART)");
    }

    public synchronized void stop(Callback owner) {
        if (owner != null && callback != owner) return;
        callback = null;
        if (!running) return;
        running = false;
        CarplayBus.getInstance().off(CarplayBus.EVT_COVERART);
        lastCrc = 0;
        artId = 0;
        currentPath = COVERART_PATH;
        Log.i(TAG, "Stopped");
    }

    public boolean isRunning() { return running; }

    /** Projection-session boundary: do not let an identical CRC or stale path from the previous
     * phone/source suppress the first artwork event of the new CarPlay session. */
    public synchronized void resetSession() {
        lastCrc = 0;
        artId = 0;
        currentPath = COVERART_PATH;
    }

    public boolean hasCoverArt() { return lastCrc != 0; }
    public String getPath() { return currentPath; }
    public int getArtId() { return artId; }

    public void onFrame(int type, int flags, byte[] payload, int len) {
        if (type != CarplayBus.EVT_COVERART) return;

        CarplayBus.Data d = CarplayBus.parseText(payload, len);
        long crc = d.num64("crc", 0);
        String path = d.str("path", COVERART_PATH);
        Callback cb;
        int newArtId;
        String newPath;
        synchronized (this) {
            if (!running || crc == 0 || crc == lastCrc) return;
            if (path != null && path.length() > 0) currentPath = path;
            artId = (int) crc;  /* CRC doubles as picture ID (matches reference impl) */
            /* Publish last: hasCoverArt() seeing the new volatile CRC also sees matching path/id. */
            lastCrc = crc;
            cb = callback;
            newArtId = artId;
            newPath = currentPath;
        }

        Log.i(TAG, "New cover art: crc=" + Long.toHexString(crc) + " artId=" + newArtId
                + " path=" + newPath);

        if (cb != null) {
            try {
                cb.onNewCoverArt(newPath, newArtId);
            } catch (Exception e) {
                Log.e(TAG, "Callback error", e);
            }
        }
    }
}
