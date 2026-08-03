package de.audi.app.combi.bap.app.audio;

import de.audi.app.combi.bap.app.kombipictures.IPictureProvider;
import java.util.HashMap;
import java.util.Map;
import org.dsi.ifc.global.ResourceLocator;

class AppConnectorTerminalMode$CoverArtProvider implements IPictureProvider {

    private final Map id2pictureURL;
    private final AppConnectorTerminalMode this$0;

    AppConnectorTerminalMode$CoverArtProvider(AppConnectorTerminalMode outer) {
        this.this$0 = outer;
        this.id2pictureURL = new HashMap();
    }

    public void requestPicture(long entryID) {
        this.requestPicture(entryID, 255);
    }

    public void requestPicture(long entryID, int sourceType) {
        Long key = new Long(entryID);
        if (this.id2pictureURL.containsKey(key)) {
            this.this$0.getCombiModule()
                .getPictureManager()
                .responseCoverArt(entryID, sourceType,
                    (ResourceLocator) this.id2pictureURL.get(key), false);
        }
    }

    public void addToMapping(long entryID, ResourceLocator rl) {
        this.id2pictureURL.put(new Long(entryID), rl);
    }

    public void clearMapping() {
        this.id2pictureURL.clear();
    }
}
