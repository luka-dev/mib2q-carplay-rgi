/*
 * CarPlay Hook - Device State Handler
 *
 * Direct implementation of IActiveDeviceStateListener.
 * Routes device state updates to CarPlayHook.
 *
 */
package com.luka.carplay;

import de.audi.app.terminalmode.device.IActiveDeviceStateListener;
import de.audi.app.terminalmode.device.TMDevice;

public class DeviceStateHandler implements IActiveDeviceStateListener {

    public void updateActiveDeviceState(TMDevice device) {
        CarPlayHook.onDeviceStateUpdate(device);
    }
}
