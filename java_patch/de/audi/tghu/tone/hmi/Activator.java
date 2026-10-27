/*
 * Patched de.audi.tghu.tone.hmi.Activator: identical to stock except that the model bank and
 * screen factory come from com.luka.carplay.tone (adds the Sound effects "UpMix" row).
 * Any failure there falls back to the stock classes, so the worst case is "no UpMix row".
 */
package de.audi.tghu.tone.hmi;

import com.luka.carplay.tone.UpmixToneUi;
import de.audi.atip.activator.AbstractHMIActivator;
import de.audi.atip.hmi.HMIConditionBank;
import de.audi.atip.hmi.view.AbstractScreenFactory;
import de.audi.atip.model.ToneModelBank;
import de.audi.tghu.tone.hmi.evohigh.ToneConditionBank;
import de.audi.tghu.tone.hmi.evohigh.ToneScreenFactory;

public class Activator extends AbstractHMIActivator {
    private ToneScreenFactory instance = null;
    private ToneConditionBank conditionBank;

    public Activator() {
        /* the stock constants as the stock bytecode inlines them (AppNameConst.APP_NAME_TONE,
         * RemoteHMIParameters.PARAMETER_VARIANT): no reference to either class */
        super(10, "Tone", System.getProperty("variant.skin", "EvoHigh"), newBank());
    }

    private static ToneModelBank newBank() {
        try {
            return UpmixToneUi.newBank();
        } catch (Throwable t) {
            return new ToneModelBank();
        }
    }

    protected synchronized AbstractScreenFactory getScreenFactory() {
        if (this.instance == null) {
            try {
                this.instance = UpmixToneUi.newFactory(this.getFramework());
            } catch (Throwable t) {
                this.instance = null;
            }
            if (this.instance == null) {
                this.instance = new ToneScreenFactory(this.getFramework());
            }
        }
        return this.instance;
    }

    public HMIConditionBank getConditionBank() {
        if (this.conditionBank == null) {
            this.conditionBank = new ToneConditionBank((ToneScreenFactory) this.getScreenFactory());
        }
        return this.conditionBank;
    }
}
