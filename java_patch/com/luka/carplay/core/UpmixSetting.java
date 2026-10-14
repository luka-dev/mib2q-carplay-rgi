/*
 * MMI Sound effects "CarPlay UpMix" level: Java owns it.
 *
 * Stored in the stock HMI persistence (IStorageAccess, the Sound app's namespace
 * 1009, our key 1000) and flushed to flash on every write through the shared
 * FlushValuesDict.register().  Java starts before dio_manager, so boot() copies the
 * stored level to TMP_FILE, which the hook reads at init and at every stream start
 * (spa_upmix_level_reload, hook/audio/audio_hook.c); a choice in
 * the row updates the store and the file and sends CMD_UPMIX for the running hook.
 *
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */
package com.luka.carplay.core;

import com.luka.carplay.bus.CarplayBus;
import com.luka.carplay.framework.Log;
import com.luka.carplay.tone.UpmixToneUi;
import de.audi.atip.base.IFrameworkAccess;
import de.audi.atip.storage.IStorageAccess;
import de.audi.tghu.storage.FlushValuesDict;
import java.io.FileOutputStream;

public final class UpmixSetting {
    private static final String TAG = "Upmix";
    private static final int NAMESPACE = 1009;     // the Sound app's own (WirelessChargingReminder: 1009/1)
    private static final int KEY = 1000;
    private static final int NORMAL = 2, COUNT = 5;
    static final String TMP_FILE = "/tmp/carplay_upmix_level";   // hook/audio/upmix_level.c

    private static volatile IFrameworkAccess fw;

    private UpmixSetting() {}

    /** HMI boot (CarPlayApp.startTransport): stored level -> TMP_FILE and the row. Never throws. */
    public static void boot(IFrameworkAccess framework) {
        try {
            fw = framework;
            try {
                FlushValuesDict.register(NAMESPACE, KEY);
            } catch (Throwable t) {
                /* an older, non-shared FlushValuesDict loaded first: still stored, flushed later */
                Log.w(TAG, "flush key not registered: " + t);
            }
            int level = read();
            writeTmp(level);
            UpmixToneUi.show(level);
            Log.i(TAG, "boot level " + level);
        } catch (Throwable t) {
            Log.w(TAG, "boot failed: " + t);
        }
    }

    /** The user picked a level in the row (HMI thread). */
    public static void select(int level) {
        if (level < 0 || level >= COUNT) return;
        try {
            IFrameworkAccess f = fw;
            if (f != null) f.getStorageMgr().setInt(NAMESPACE, KEY, level);
            else Log.w(TAG, "not stored: no framework yet (startTransport has not run)");
        } catch (Throwable t) {
            Log.w(TAG, "store failed: " + t);
        }
        writeTmp(level);
        boolean sent = CarplayBus.getInstance().sendBinary(CarplayBus.CMD_UPMIX, new byte[]{(byte) level});
        Log.i(TAG, "selected " + level + (sent ? "" : " (bus down: the hook takes it from the file at its next start)"));
    }

    private static int read() {
        try {
            IFrameworkAccess f = fw;
            IStorageAccess s = f != null ? f.getStorageMgr() : null;
            int v = s != null ? s.getInt(NAMESPACE, KEY, NORMAL) : NORMAL;
            return v >= 0 && v < COUNT ? v : NORMAL;
        } catch (Throwable t) {
            return NORMAL;                          // never written yet, or storage not up
        }
    }

    private static void writeTmp(int level) {
        FileOutputStream out = null;
        try {
            out = new FileOutputStream(TMP_FILE);
            out.write(new byte[]{(byte) ('0' + level), '\n'});
        } catch (Throwable t) {
            Log.w(TAG, "cannot write " + TMP_FILE + ": " + t);
        } finally {
            try { if (out != null) out.close(); } catch (Throwable t) { /* nothing to do */ }
        }
    }
}
