/*
 * Replaces the stock CarPlaySpeechRequestHandler (identical on MU1316, MU1329 and MU1003):
 * with CarPlay connected, a short press of the steering-wheel voice key starts Siri.
 *
 * Stock TerminalMode (SDSAppHandler) handles the PTT key as: press -> prewarm(); long press
 * detected -> startSpeechSession() plus SPEECH owner = DEVICE (that state change is what hands
 * the microphone to the phone); long release -> SIRI STOP; short release -> abort an active
 * Siri, otherwise cancelPrewarm().  The patched TerminalModeSDSHandler routes the short press
 * here instead of to the car's own voice control.
 *
 * cancelPrewarm() (whose only caller is that short-release branch) replays the stock LONG-press
 * path on SDSAppHandler, found as the TerminalMode ISDSKeyInterceptor service:
 * pttLongDetected() then pttReleasedAfterLongPress().  Stock thereby sets the SPEECH owner (mic
 * routing) and sends SIRI START followed by STOP, a button tap, after which iOS listens until
 * the speaker stops.  Car test 2026-10-10 of the first version (a bare SIRI START): Siri showed
 * its animation but heard nothing and never got the button release.
 *
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */
package de.audi.app.terminalmode.smartphone.carplay;

import com.luka.carplay.framework.Log;
import de.audi.app.terminalmode.IContext;
import de.audi.app.terminalmode.dsi.carplay.ICarplayDSIController;
import de.audi.app.terminalmode.dsi.carplay.SiriAction;
import de.audi.app.terminalmode.osgi.IServiceManager;
import de.audi.app.terminalmode.smartphone.ISpeechRequestHandler;
import de.audi.atip.interapp.ISDSKeyInterceptor;
import org.osgi.framework.ServiceReference;

public class CarPlaySpeechRequestHandler implements ISpeechRequestHandler {
    private static final String TM_SDS_HANDLER = "de.audi.app.terminalmode.SDSAppHandler";
    private final IContext context;
    private final ICarplayDSIController dioDSIController;

    public CarPlaySpeechRequestHandler(IContext icontext, ICarplayDSIController icarplaydsicontroller) {
        this.context = icontext;
        this.dioDSIController = icarplaydsicontroller;
    }

    public void prewarm() {
        this.dioDSIController.requestSIRIAction(SiriAction.SIRIACTION_PREWARM);
    }

    /** Short press released while speech is free: a Siri tap through the stock long-press path. */
    public void cancelPrewarm() {
        if (tapThroughStockLongPress()) return;
        Log.w("Siri", "voice key short press: TerminalMode SDS handler not found, plain Siri tap");
        this.dioDSIController.requestSIRIAction(SiriAction.SIRIACTION_START);
        this.dioDSIController.requestSIRIAction(SiriAction.SIRIACTION_STOP);
    }

    private boolean tapThroughStockLongPress() {
        try {
            IServiceManager sm = this.context != null ? this.context.getServiceManager() : null;
            ServiceReference[] refs = sm != null ? sm.getServiceReferences(ISDSKeyInterceptor.class) : null;
            for (int i = 0; refs != null && i < refs.length; i++) {
                Object s = sm.getService(refs[i]);
                if (s == null) continue;
                try {
                    if (s instanceof ISDSKeyInterceptor && TM_SDS_HANDLER.equals(s.getClass().getName())) {
                        ISDSKeyInterceptor tm = (ISDSKeyInterceptor)s;
                        Log.i("Siri", "voice key short press -> Siri tap (stock long-press path)");
                        tm.pttLongDetected();
                        tm.pttReleasedAfterLongPress();
                        return true;
                    }
                } finally {
                    sm.releaseService(refs[i]);
                }
            }
        } catch (Throwable t) {
            Log.w("Siri", "voice key short press: stock long-press path failed: " + t);
        }
        return false;
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
