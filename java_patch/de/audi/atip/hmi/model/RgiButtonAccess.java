/*
 * ButtonModel.buttonListener is protected; this same-package accessor reads it so the
 * CarPlay console-key wrapper can keep and delegate to the stock listener.  RGI already
 * adds helper classes to stock packages (e.g. CoverArtProviderMux).
 */
package de.audi.atip.hmi.model;

public final class RgiButtonAccess {
    private RgiButtonAccess() {}

    public static ButtonListener getListener(ButtonModel m) {
        return m.buttonListener;
    }
}
