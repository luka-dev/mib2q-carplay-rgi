package com.luka.carplay.core;

import de.audi.atip.base.IFrameworkAccess;

/** Lifecycle-test stand-in: CarPlayApp.startTransport calls boot(); the level store is not under test here. */
public final class UpmixSetting {
    private UpmixSetting() {}
    public static void boot(IFrameworkAccess framework) {}
}
