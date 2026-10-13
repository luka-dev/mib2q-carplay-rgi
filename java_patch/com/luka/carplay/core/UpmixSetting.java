/*
 * MMI Sound effects "UpMix" row <-> hook (CMD_UPMIX / EVT_UPMIX_LEVEL).
 *
 * The hook owns the level (it persists it and applies it to the scene upmix);
 * the row only asks for a level and mirrors the one the hook reports.  The row
 * calls select() on the HMI thread: CarplayBus.send() only queues the frame.
 *
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */
package com.luka.carplay.core;

import com.luka.carplay.bus.CarplayBus;
import com.luka.carplay.framework.Log;
import com.luka.carplay.tone.UpmixToneUi;

public final class UpmixSetting {
    private static final String TAG = "Upmix";
    private static boolean attached;

    private UpmixSetting() {}

    /** Subscribe to the hook's level once (the sticky frame is replayed if it came first). */
    public static synchronized void attach() {
        if (attached) return;
        attached = true;
        CarplayBus.getInstance().on(CarplayBus.EVT_UPMIX_LEVEL, new CarplayBus.Listener() {
            public void onFrame(int type, int flags, byte[] payload, int len) {
                if (len < 1) return;
                int level = payload[0] & 0xFF;
                Log.i(TAG, "hook level " + level);
                UpmixToneUi.show(level);
            }
        });
    }

    /** The user picked a level in the row (HMI thread, must not block). */
    public static void select(int level) {
        if (level < 0 || level > 4) return;
        boolean sent = CarplayBus.getInstance().sendBinary(CarplayBus.CMD_UPMIX, new byte[]{(byte) level});
        Log.i(TAG, "selected " + level + (sent ? "" : " (bus down, not applied)"));
    }
}
