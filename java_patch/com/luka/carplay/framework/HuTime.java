package com.luka.carplay.framework;

import com.luka.carplay.core.CarPlayApp;
import de.audi.atip.base.IFrameworkAccess;

/** Time conversion shared by CarPlay display consumers. */
public final class HuTime {
    private HuTime() { }

    /**
     * Shift UTC epoch milliseconds by the HU's current UTC offset (including
     * DST). The JVM default time zone on MHI2Q is UTC, so display consumers
     * decode this shifted value with their UTC Calendar. It is not a new UTC
     * instant and must not be used for timestamp comparisons.
     * If framework access is unavailable, retain the original UTC value.
     */
    public static long convertUtcToLocalMs(long utcMs) {
        try {
            IFrameworkAccess fw = CarPlayApp.framework();
            if (fw != null) return fw.convertUTCTimeToLocalTime(utcMs);
        } catch (Throwable t) {
            /* Keep display timestamps usable while framework access is unavailable. */
        }
        return utcMs;
    }
}
