/*
 * ConsoleKeyCarPlay - a tap on a console hard key jumps to CarPlay while it is connected.
 *
 * NAV/MAP (button model 116) opens the CarPlay map, MEDIA (114) Now Playing, TEL (118) the
 * phone and RADIO (119) the media app, through DSICarplay.requestUI and the stock
 * SWITCH_TO_TERMINAL_MODE flip that CarPlayDisclaimerSkip already uses for the TEL key.  The
 * HMI fires keyTyped on press (and never keyLongTyped for these keys), so the tap is diverted
 * there; keyPressed / keyReleased still reach stock.  The stock listener is wrapped on CarPlay
 * connect and restored on disconnect, so with CarPlay off the keys behave as standard.  The
 * feature is gated by the "Console Keys Open CarPlay" setting (default off).
 * Java 1.4 / Foundation 1.1.
 */
package com.luka.carplay.input;

import com.luka.carplay.core.CarPlayApp;
import com.luka.carplay.framework.Log;
import com.luka.carplay.telephony.CarPlayDisclaimerSkip;
import de.audi.atip.base.IFrameworkAccess;
import de.audi.atip.hmi.model.ButtonListener;
import de.audi.atip.hmi.model.ButtonModel;
import de.audi.atip.hmi.model.RgiButtonAccess;

public final class ConsoleKeyCarPlay {

    private static final String TAG = "ConsoleKey";

    // DSICarplay UI ids: PHONE 2, NAVIGATION 3, MEDIA 4, MEDIA_NOWPLAYING 5.
    private static final int UI_PHONE = 2, UI_NAVIGATION = 3, UI_MEDIA = 4, UI_MEDIA_NOWPLAYING = 5;

    private static final int[] MODEL  = { 116, 114, 118, 119 };                 // NAV, MEDIA, TEL, RADIO
    private static final int[] SCREEN = { UI_NAVIGATION, UI_MEDIA_NOWPLAYING, UI_PHONE, UI_MEDIA };

    /** Supplied by CarplayDSILifecycleController; forwards to DSICarplay.requestUI. */
    public interface UiRequester { void open(int ui); }
    private static volatile UiRequester requester;

    private static final Object LOCK = new Object();
    private static final Wrapper[] installed = new Wrapper[MODEL.length];

    private ConsoleKeyCarPlay() {}

    public static void setUiRequester(UiRequester r) { requester = r; }

    public static void install(IFrameworkAccess fw) {
        if (fw == null) return;
        synchronized (LOCK) {
            for (int i = 0; i < MODEL.length; i++) {
                try {
                    Object model = fw.getHMIService().getModel(MODEL[i]);
                    if (!(model instanceof ButtonModel)) continue;
                    ButtonModel bm = (ButtonModel) model;
                    ButtonListener stock = RgiButtonAccess.getListener(bm);
                    if (stock instanceof Wrapper) { installed[i] = (Wrapper) stock; continue; }
                    Wrapper w = new Wrapper(stock, SCREEN[i], fw);
                    bm.setButtonListener(w);
                    installed[i] = w;
                } catch (Throwable t) { Log.w(TAG, "wrap " + MODEL[i] + " failed: " + t); }
            }
        }
    }

    public static void uninstall(IFrameworkAccess fw) {
        synchronized (LOCK) {
            for (int i = 0; i < MODEL.length; i++) {
                Wrapper w = installed[i];
                installed[i] = null;
                if (w == null || fw == null) continue;
                try {
                    Object model = fw.getHMIService().getModel(MODEL[i]);
                    if (model instanceof ButtonModel
                            && RgiButtonAccess.getListener((ButtonModel) model) == w) {
                        ((ButtonModel) model).setButtonListener(w.stock);
                    }
                } catch (Throwable t) { Log.w(TAG, "unwrap " + MODEL[i] + " failed: " + t); }
            }
        }
    }

    static final class Wrapper implements ButtonListener {
        final ButtonListener stock;
        final int screen;
        final IFrameworkAccess fw;

        Wrapper(ButtonListener stock, int screen, IFrameworkAccess fw) {
            this.stock = stock; this.screen = screen; this.fw = fw;
        }

        public void keyPressed(int id, int key, int terminal)   { if (stock != null) stock.keyPressed(id, key, terminal); }
        public void keyReleased(int id, int key, int terminal)  { if (stock != null) stock.keyReleased(id, key, terminal); }
        public void keyLongTyped(int id, int key, int terminal) { if (stock != null) stock.keyLongTyped(id, key, terminal); }

        public void keyTyped(int id, int key, int terminal) {
            try {
                if (terminal == 0 && CarPlayApp.isActive()) {
                    UiRequester r = requester;
                    if (r != null) r.open(screen);
                    CarPlayDisclaimerSkip.press(fw, "console key");
                    return;
                }
            } catch (Throwable t) { Log.w(TAG, "key divert failed: " + t); }
            if (stock != null) stock.keyTyped(id, key, terminal);
        }
    }
}
