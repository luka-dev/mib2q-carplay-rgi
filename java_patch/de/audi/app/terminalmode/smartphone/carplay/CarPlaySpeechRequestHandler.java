/*
 * Replaces the stock CarPlaySpeechRequestHandler (identical on MU1316, MU1329 and MU1003):
 * with CarPlay connected, a short press of the steering-wheel voice key starts Siri.
 *
 * Stock TerminalMode (SDSAppHandler) handles the PTT key as: press -> prewarm(); long press
 * detected -> startSpeechSession(); short release -> abortActiveSpeechSession() while Siri
 * owns speech, otherwise cancelPrewarm().  Stock never gets that short release while CarPlay
 * is idle: the car's own voice control takes it first.  The patched
 * de.audi.app.sdsmanager.apps.terminalmode.TerminalModeSDSHandler hands it to TerminalMode for
 * every CarPlay connection, and the only caller of cancelPrewarm() is that short-release
 * branch, so here it starts the session that the press prewarmed.  A second short press while
 * Siri is active still ends it; the long press keeps its stock hold-to-talk behaviour.
 *
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */
package de.audi.app.terminalmode.smartphone.carplay;

import com.luka.carplay.framework.Log;
import de.audi.app.terminalmode.IContext;
import de.audi.app.terminalmode.dsi.carplay.ICarplayDSIController;
import de.audi.app.terminalmode.dsi.carplay.SiriAction;
import de.audi.app.terminalmode.smartphone.ISpeechRequestHandler;

public class CarPlaySpeechRequestHandler implements ISpeechRequestHandler {
    private final ICarplayDSIController dioDSIController;

    public CarPlaySpeechRequestHandler(IContext icontext, ICarplayDSIController icarplaydsicontroller) {
        this.dioDSIController = icarplaydsicontroller;
    }

    public void prewarm() {
        this.dioDSIController.requestSIRIAction(SiriAction.SIRIACTION_PREWARM);
    }

    /** Short press released while speech is free: start Siri (stock: stop the prewarm). */
    public void cancelPrewarm() {
        Log.i("Siri", "voice key short press -> Siri start");
        this.dioDSIController.requestSIRIAction(SiriAction.SIRIACTION_START);
    }

    public void startSpeechSession() {
        this.dioDSIController.requestSIRIAction(SiriAction.SIRIACTION_START);
    }

    public void abortActiveSpeechSession() {
        this.dioDSIController.requestSIRIAction(SiriAction.SIRIACTION_START);
        this.dioDSIController.requestSIRIAction(SiriAction.SIRIACTION_STOP);
    }

    public void pttReleasedAfterLongPress() {
        this.dioDSIController.requestSIRIAction(SiriAction.SIRIACTION_STOP);
    }
}
