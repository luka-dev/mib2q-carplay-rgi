package com.luka.carplay.startup;

import de.audi.atip.hmi.modelaccess.ChoiceModelApp;
import de.audi.atip.hmi.IHMIServiceApp;

/** The two stock MainWizard writes and loader callbacks share one lock/owner. */
public final class HuMenuIdentity {
    private static final MenuIdentityState state = new MenuIdentityState();
    private static ChoiceModelApp identity, selection;
    private HuMenuIdentity() { }

    public static synchronized void bind(IHMIServiceApp hmi) {
        if (hmi == null) return;
        identity = hmi.getChoiceModel(4239); selection = hmi.getChoiceModel(138);
    }
    public static synchronized void begin(int generation) { state.begin(generation); apply(); }
    public static synchronized void shown(int generation) { state.shown(generation); apply(); }
    public static synchronized void fallback(int generation) { state.fallback(generation); apply(); }
    public static synchronized void end(int generation) { state.end(generation); apply(); }

    /** Called only from stock Active.updateMainWizardModel(ZI)V. */
    public static synchronized void stockIdentity(ChoiceModelApp model, int value, int deviceType) {
        identity = model;
        state.stock(value, deviceType);
        apply();
    }
    public static synchronized void stockSelection(ChoiceModelApp model, int value) {
        selection = model;
        // Re-check current focus inside the lock; never steal a newer non-phone selection.
        int current = model.getValue();
        if (current >= 16 && current <= 19) write(model, state.selection(value));
    }
    private static void apply() {
        if (identity != null) write(identity, state.value());
        if (selection != null) write(selection, state.selection(selection.getValue()));
    }
    private static void write(ChoiceModelApp model, int value) {
        if (model.getValue() != value) model.setValue(value);
    }
}
