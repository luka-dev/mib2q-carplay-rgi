package com.luka.carplay.core;

import de.audi.atip.base.IFrameworkAccess;

/** Lifecycle-test stand-in: CarPlayApp.startTransport calls boot(); the mode store is not under test here. */
public final class CockpitMapSetting {
    private CockpitMapSetting() {}
    public static void boot(IFrameworkAccess framework) {}
}
