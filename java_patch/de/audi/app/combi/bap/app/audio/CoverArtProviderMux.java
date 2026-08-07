/*
 * Preserve stock Media cover-art requests while adding CarPlay TerminalMode cover art.
 * AbstractPictureManager has one provider slot (type 0) for every audio source; the stock
 * AppConnectorMedia registers first and AppConnectorTerminalMode must therefore multiplex,
 * never overwrite, that slot.
 */
package de.audi.app.combi.bap.app.audio;

import de.audi.app.combi.bap.app.kombipictures.IPictureManager;
import de.audi.app.combi.bap.app.kombipictures.IPictureProvider;
import java.lang.reflect.Field;
import java.util.Map;

final class CoverArtProviderMux implements IPictureProvider {
    private static final int PICTURE_TYPE_COVER_ART = 0;

    private final IPictureProvider stockMedia;
    private final AppConnectorTerminalMode$CoverArtProvider carPlay;

    private CoverArtProviderMux(IPictureProvider stockMedia,
            AppConnectorTerminalMode$CoverArtProvider carPlay) {
        this.stockMedia = stockMedia;
        this.carPlay = carPlay;
    }

    static boolean install(IPictureManager manager,
            AppConnectorTerminalMode$CoverArtProvider carPlay) {
        if (manager == null || carPlay == null) return false;
        IPictureProvider stock = findProvider(manager, PICTURE_TYPE_COVER_ART);
        if (stock == null) return false;
        if (stock instanceof CoverArtProviderMux) return true;
        manager.registerPictureProvider(PICTURE_TYPE_COVER_ART,
            new CoverArtProviderMux(stock, carPlay));
        return true;
    }

    private static IPictureProvider findProvider(IPictureManager manager, int type) {
        Class c = manager.getClass();
        while (c != null) {
            try {
                Field f = c.getDeclaredField("pictureProviders");
                f.setAccessible(true);
                Object value = f.get(manager);
                if (!(value instanceof Map)) return null;
                Object provider = ((Map) value).get(new Integer(type));
                return provider instanceof IPictureProvider ? (IPictureProvider) provider : null;
            } catch (NoSuchFieldException e) {
                c = c.getSuperclass();
            } catch (Throwable t) {
                return null;
            }
        }
        return null;
    }

    public void requestPicture(long entryID) {
        if (carPlay.hasPicture(entryID)) carPlay.requestPicture(entryID);
        else stockMedia.requestPicture(entryID);
    }

    public void requestPicture(long entryID, int sourceType) {
        /* BAP source 21 is Bluetooth audio, not CarPlay (Apple Link is 37).
         * Route by the entry-ID mapping we actually own; otherwise preserve the
         * stock Media provider for every source, including Bluetooth. */
        if (carPlay.hasPicture(entryID)) {
            carPlay.requestPicture(entryID, sourceType);
        } else {
            stockMedia.requestPicture(entryID, sourceType);
        }
    }
}
