package com.luka.carplay.connectivity;

import de.audi.atip.base.IFrameworkAccess;

/** Lifecycle-test stand-in: CarPlayApp.startTransport calls boot(); the settings store is not under test here. */
public final class CarPlaySettingsUi {
    private CarPlaySettingsUi() {}
    public static void boot(IFrameworkAccess framework) {}
}
