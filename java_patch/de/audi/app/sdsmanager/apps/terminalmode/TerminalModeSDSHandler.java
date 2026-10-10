/*
 * Replaces the stock TerminalModeSDSHandler (byte-identical on MU1316, MU1329 and MU1003):
 * while a CarPlay phone is the active terminal-mode device, a short press of the wheel voice
 * key goes to Siri instead of the car's own voice control.
 *
 * Stock SDSHMIListener.handlePTTShort() starts the native speech dialog unless this handler
 * reports terminal-mode speech as active; only then does it hand the short press to
 * TerminalMode (notifyExternalSDSKeyListeners(1) -> SDSAppHandler).  Reporting speech as
 * active for every CarPlay connection routes the short press there: SDSAppHandler ends an
 * active Siri session or, with speech free, calls CarPlaySpeechRequestHandler.cancelPrewarm(),
 * which the patched handler turns into a Siri start.  The long press already went to Siri.
 * The only callers of isTerminalModeSpeechActive() are the three PTT branches of
 * SDSHMIListener; Android Auto and CarLife keep the stock answer.
 *
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */
package de.audi.app.sdsmanager.apps.terminalmode;

import de.audi.app.sdsmanager.common.Logger;
import de.audi.atip.interapp.terminalmode.DefaultTerminalModeUpdateListener;
import de.audi.atip.interapp.terminalmode.TerminalModeAppState;
import de.audi.atip.interapp.terminalmode.TerminalModeDevice;
import de.audi.atip.log.LogChannel;

public class TerminalModeSDSHandler extends DefaultTerminalModeUpdateListener implements ITerminalModeSDSHandler {
    private final LogChannel lc = Logger.getHMILog();
    private volatile boolean terminalModeDeviceActive = false;
    private volatile boolean terminalModeSpeechActive = false;
    private volatile boolean carPlayActive = false;

    public void updateActiveDeviceState(TerminalModeDevice terminalmodedevice) {
        boolean flag = terminalmodedevice.isActive();
        this.lc.log(10000000, "TerminalModeSDSHandler#updateActiveDeviceState: terminal mode device active: %1!", flag);
        if (!flag) {
            this.terminalModeSpeechActive = false;
        }

        this.carPlayActive = flag
            && terminalmodedevice.getConnectionMethod() == TerminalModeDevice.CONN_METHOD_CARPLAY;
        this.terminalModeDeviceActive = flag;
    }

    public boolean isTerminalModeDeviceActive() {
        return this.terminalModeDeviceActive;
    }

    public void updateAppStates(TerminalModeAppState[] aterminalmodeappstate) {
        for (int i = 0; i < aterminalmodeappstate.length; i++) {
            TerminalModeAppState terminalmodeappstate = aterminalmodeappstate[i];
            if (terminalmodeappstate != null) {
                this.lc.log(10000000, "TerminalModeSDSHandler#updateAppStates: %1!", terminalmodeappstate.toString());
                if (terminalmodeappstate.getAppId() == 3 && terminalmodeappstate.isRunningOnDevice()) {
                    this.terminalModeSpeechActive = true;
                    return;
                }
            }
        }

        this.lc
            .log(10000000, "TerminalModeSDSHandler#updateAppStates: Currently no SDS running on terminal mode device!");
        this.terminalModeSpeechActive = false;
    }

    /** Stock: the phone's speech app is running.  Here also: any CarPlay connection, so the
     *  short voice-key press goes to Siri (see the class comment). */
    public boolean isTerminalModeSpeechActive() {
        return this.terminalModeDeviceActive && (this.terminalModeSpeechActive || this.carPlayActive);
    }
}
