/*
 * CockpitMapSetting - which map the Virtual Cockpit shows while CarPlay is connected.
 *
 *   PHONE        the phone's cluster map (luka's behaviour, the default)
 *   AUDI_ARROWS  the stock Audi map, with CarPlay's 3D maneuver box over it while CarPlay
 *                guides (the arrow-over-stock-map mode of Allemon's fork); CarPlay keeps the
 *                route text, distance and HUD, the stock route is cancelled at connect as today
 *   AUDI         the stock Audi navigation, untouched: its own map, route, maneuver box, route
 *                text and HUD.  CarPlay keeps the main screen, calls, music and the phone tab.
 *                luka's navignore seam (CarplayDSILifecycleController) already stops the HMI
 *                from handing navigation to CarPlay, and in this mode RgdModule does not take
 *                over, so a stock route keeps guiding next to Waze or Maps on the main screen.
 *
 * Stored in the stock HMI persistence (Sound app namespace 1009, our key 1001, next to the
 * UpMix level's 1000) and flushed to flash on every write.  Changed by the steering-wheel
 * shortcut (long press on the left roller, SteeringWheelInputModule) or select().  A change
 * applies at once: AltScreenModule recomputes the cluster context and CarPlayApp rebinds the
 * route-guidance module.
 *
 * Java 1.4 source level.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
package com.luka.carplay.core;

import com.luka.carplay.framework.Log;

import de.audi.atip.base.IFrameworkAccess;
import de.audi.atip.storage.IStorageAccess;
import de.audi.tghu.storage.FlushValuesDict;

public final class CockpitMapSetting {
    private static final String TAG = "CockpitMap";
    public static final int PHONE = 0;
    public static final int AUDI_ARROWS = 1;
    public static final int AUDI = 2;
    public static final int COUNT = 3;

    static final int NAMESPACE = 1009;
    static final int KEY = 1001;

    private static volatile IFrameworkAccess fw;
    private static volatile IStorageAccess storageForTest;
    private static volatile int mode = PHONE;

    private CockpitMapSetting() {}

    public static int mode() { return mode; }

    public static String name(int m) {
        return m == AUDI_ARROWS ? "audi-map+carplay-arrows" : (m == AUDI ? "audi-navigation" : "phone-map");
    }

    /** HMI boot (CarPlayApp.startTransport): load the stored mode.  Never throws. */
    public static void boot(IFrameworkAccess framework) {
        try {
            fw = framework;
            try {
                FlushValuesDict.register(NAMESPACE, KEY);
            } catch (Throwable t) {
                Log.w(TAG, "flush key not registered: " + t);
            }
            int m = read();
            set(m, "boot", false);
        } catch (Throwable t) {
            Log.w(TAG, "boot failed: " + t);
        }
    }

    /** User choice (shortcut or menu). Stores it and applies it.  Never throws. */
    public static void select(int m, String reason) {
        if (m < 0 || m >= COUNT) return;
        set(m, reason, true);
    }

    /** Next mode in the shortcut cycle PHONE -> AUDI_ARROWS -> AUDI -> PHONE. */
    public static int next(int m) {
        return (m + 1) % COUNT;
    }

    private static void set(int m, String reason, boolean store) {
        int old;
        synchronized (CockpitMapSetting.class) {
            old = mode;
            mode = m;
        }
        if (store) write(m);
        Log.w(TAG, "mode=" + name(m) + " (was " + name(old) + ") reason=" + reason);
        if (old == m) return;
        /* Outside our lock: the cluster context first, then the route-guidance ownership. */
        try { AltScreenModule.onCockpitMapModeChanged(m); }
        catch (Throwable t) { Log.w(TAG, "cluster update failed: " + t); }
        /* Route-guidance ownership changes only into or out of AUDI; PHONE <-> AUDI_ARROWS keeps
         * the running CarPlay guidance untouched. */
        if ((old == AUDI) != (m == AUDI)) {
            try { CarPlayApp.onCockpitMapModeChanged(reason); }
            catch (Throwable t) { Log.w(TAG, "route-guidance rebind failed: " + t); }
        }
    }

    private static IStorageAccess storage() {
        IStorageAccess s = storageForTest;
        if (s != null) return s;
        IFrameworkAccess f = fw;
        return f != null ? f.getStorageMgr() : null;
    }

    private static int read() {
        try {
            IStorageAccess s = storage();
            int v = s != null ? s.getInt(NAMESPACE, KEY, PHONE) : PHONE;
            return v >= 0 && v < COUNT ? v : PHONE;
        } catch (Throwable t) {
            return PHONE;                       // never written yet, or storage not up
        }
    }

    private static void write(int m) {
        try {
            IStorageAccess s = storage();
            if (s != null) s.setInt(NAMESPACE, KEY, m);
            else Log.w(TAG, "not stored: no framework yet");
        } catch (Throwable t) {
            Log.w(TAG, "store failed: " + t);
        }
    }

    /** Host tests only. */
    static void resetForTest(IStorageAccess s) {
        storageForTest = s;
        mode = PHONE;
    }
}
