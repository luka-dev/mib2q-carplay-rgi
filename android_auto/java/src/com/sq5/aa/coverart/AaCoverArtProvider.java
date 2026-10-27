/*
 * Answers the VC's cover-art re-requests for the Android Auto picture (luka's
 * AppConnectorTerminalMode$CoverArtProvider, moved out of the stock package).  Owns exactly one
 * entry (the TerminalMode now-playing handle, listRef 0) and only while an Android Auto session
 * is active and the connector holds the audio focus; everything else goes to stock Media
 * through AaCoverArtMux, so a Media track that also uses handle 0 never gets Android Auto art.
 */
package com.sq5.aa.coverart;

import de.audi.app.combi.bap.app.kombipictures.IPictureManager;
import de.audi.app.combi.bap.app.kombipictures.IPictureProvider;
import org.dsi.ifc.global.ResourceLocator;

public final class AaCoverArtProvider implements IPictureProvider {
    private final IPictureManager manager;
    private long entry;
    private ResourceLocator locator;   /* null = nothing owned */

    public AaCoverArtProvider(IPictureManager manager) {
        this.manager = manager;
    }

    synchronized void set(long entryID, ResourceLocator rl) {
        entry = entryID;
        locator = rl;
    }

    synchronized void clear() {
        locator = null;
    }

    synchronized boolean owns(long entryID) {
        return locator != null && entry == entryID && AaCoverArt.isSessionActive();
    }

    public void requestPicture(long entryID) {
        requestPicture(entryID, 255);
    }

    public void requestPicture(long entryID, int sourceType) {
        ResourceLocator rl;
        synchronized (this) {
            if (!owns(entryID)) return;
            rl = locator;
        }
        try {
            manager.responseCoverArt(entryID, sourceType, rl, false);
            AaCoverArt.log("VC re-request entry=" + entryID + " source=" + sourceType + " -> " + rl.getUrl());
        } catch (Throwable t) {
            AaCoverArt.log("re-request failed: " + t);
        }
    }
}
