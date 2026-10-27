/*
 * luka-dev's CoverArtProviderMux, for the Android Auto picture.  MU0918 AbstractPictureManager
 * keeps ONE provider per picture type (map field "pictureProviders", type 0 = cover art) for all
 * audio sources; stock AppConnectorMedia registers it first (CombiModuleAudio builds Media before
 * TerminalMode).  Overwriting it would break Media cover-art re-requests, so the Media provider
 * is wrapped: entries the Android Auto provider owns go to it, everything else to Media.
 * If the map cannot be read (other variant), nothing is installed and the direct push in
 * AaCoverArtConnector still delivers the art (the VC's re-requests then get Media's answer).
 */
package com.sq5.aa.coverart;

import de.audi.app.combi.bap.app.kombipictures.IPictureManager;
import de.audi.app.combi.bap.app.kombipictures.IPictureProvider;
import java.lang.reflect.Field;
import java.util.Map;

public final class AaCoverArtMux implements IPictureProvider {
    static final int PICTURE_TYPE_COVER_ART = 0;

    private final IPictureProvider stock;
    private final AaCoverArtProvider aa;

    AaCoverArtMux(IPictureProvider stock, AaCoverArtProvider aa) {
        this.stock = stock;
        this.aa = aa;
    }

    static boolean install(IPictureManager manager, AaCoverArtProvider aa) {
        if (manager == null || aa == null) return false;
        IPictureProvider stock = findProvider(manager, PICTURE_TYPE_COVER_ART);
        if (stock == null) return false;
        if (stock instanceof AaCoverArtMux) return true;
        manager.registerPictureProvider(PICTURE_TYPE_COVER_ART, new AaCoverArtMux(stock, aa));
        return true;
    }

    static IPictureProvider findProvider(IPictureManager manager, int type) {
        Class c = manager.getClass();
        while (c != null) {
            try {
                Field f = c.getDeclaredField("pictureProviders");
                f.setAccessible(true);
                Object value = f.get(manager);
                if (!(value instanceof Map)) return null;
                Object provider = ((Map) value).get(new Integer(type));
                return provider instanceof IPictureProvider ? (IPictureProvider) provider : null;
            } catch (Throwable t) {
                /* lsd.jxe's Class.getDeclaredField lost its throws clause, so javac rejects a
                 * checked catch of NoSuchFieldException; it is still thrown at run time */
                if (!(t instanceof NoSuchFieldException)) return null;
                c = c.getSuperclass();
            }
        }
        return null;
    }

    public void requestPicture(long entryID) {
        if (aa.owns(entryID)) aa.requestPicture(entryID);
        else stock.requestPicture(entryID);
    }

    public void requestPicture(long entryID, int sourceType) {
        if (aa.owns(entryID)) aa.requestPicture(entryID, sourceType);
        else stock.requestPicture(entryID, sourceType);
    }
}
