/*
 * Patched de.audi.tghu.connectivity.hmi.Activator: identical to stock except that the model bank
 * and screen factory come from com.luka.carplay.connectivity (adds "CarPlay settings" to
 * Connection Manager -> Options). Any failure falls back to the stock classes.
 */
package de.audi.tghu.connectivity.hmi;

import com.luka.carplay.connectivity.CarPlaySettingsUi;
import de.audi.atip.activator.AbstractHMIActivator;
import de.audi.atip.hmi.HMIConditionBank;
import de.audi.atip.hmi.view.AbstractScreenFactory;
import de.audi.atip.model.ConnectivityModelBank;
import de.audi.tghu.connectivity.hmi.evohigh.ConnectivityConditionBank;
import de.audi.tghu.connectivity.hmi.evohigh.ConnectivityScreenFactory;

public class Activator extends AbstractHMIActivator {
    private ConnectivityScreenFactory instance = null;
    private ConnectivityConditionBank conditionBank;

    public Activator() {
        super(25, "Connectivity", System.getProperty("variant.skin", "EvoHigh"), newBank());
    }

    private static ConnectivityModelBank newBank() {
        try {
            return CarPlaySettingsUi.newBank();
        } catch (Throwable t) {
            return new ConnectivityModelBank();
        }
    }

    protected synchronized AbstractScreenFactory getScreenFactory() {
        if (this.instance == null) {
            try {
                this.instance = CarPlaySettingsUi.newFactory(this.getFramework());
            } catch (Throwable t) {
                this.instance = null;
            }
            if (this.instance == null) {
                this.instance = new ConnectivityScreenFactory(this.getFramework());
            }
        }
        return this.instance;
    }

    public HMIConditionBank getConditionBank() {
        if (this.conditionBank == null) {
            this.conditionBank = new ConnectivityConditionBank((ConnectivityScreenFactory) this.getScreenFactory());
        }
        return this.conditionBank;
    }
}
