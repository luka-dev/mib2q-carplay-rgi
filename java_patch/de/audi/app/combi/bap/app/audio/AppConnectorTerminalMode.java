package de.audi.app.combi.bap.app.audio;

import de.audi.app.bap.fw.functiontypes.BAPFunctionArrayFSG;
import de.audi.app.combi.bap.app.audio.list.SourceListHandler;
import de.audi.atip.base.AppNameConst;
import de.audi.atip.interapp.combi.bap.audio.CombiBAPServiceTerminalMode;
import de.audi.atip.interapp.combi.bap.audio.data.CombiBAPAudioSource;
import de.audi.atip.interapp.combi.bap.audio.data.CombiBAPCurrentStationInfo;
import de.audi.atip.interapp.combi.bap.audio.data.PlayPosition;
import de.audi.app.combi.bap.fw.AbstractCombiModule;
import de.vw.mib.bap.generated.audiosd.serializer.PlayPosition_Status;
import org.dsi.ifc.global.ResourceLocator;

public class AppConnectorTerminalMode extends AbstractAppConnectorAudio implements CombiBAPServiceTerminalMode {
    /* CarPlay cover art (ported from the working old impl; the stock class delivers no CarPlay
     * artwork): register an IPictureProvider for cluster re-requests + push the artwork to the VC
     * on each station update. */
    private static final int SOURCE_TYPE_VC = 21;
    private final AppConnectorTerminalMode$CoverArtProvider pictureProvider;

    protected AppConnectorTerminalMode(CombiModuleAudio combimoduleaudio) {
        super(combimoduleaudio);
        this.pictureProvider = new AppConnectorTerminalMode$CoverArtProvider(this);
        /* AppConnectorMedia has already registered stock cover-art provider type 0.  Replacing
         * that Map entry breaks later Media re-requests, so preserve it and route CarPlay IDs
         * through a small mux instead.  Direct push below remains the fallback if a variant hides
         * the provider map from reflection. */
        if (!CoverArtProviderMux.install(combimoduleaudio.getPictureManager(), this.pictureProvider)) {
            this.logChannel.log(100000, "[AppConnectorTerminalMode] cover-art mux unavailable; direct push only");
        }
        combimoduleaudio.getPictureManager().setNotification(0);
    }

    /* Package-private accessor for CoverArtProvider (mirrors the synthetic access$000). */
    AbstractCombiModule getCombiModule() {
        return (AbstractCombiModule) this.moduleFsg;
    }

    protected int getAudioApplication() {
        return 3;
    }

    protected String getAudioApplicationName() {
        return AppNameConst.APP_NAME_TERMINAL_MODE;
    }

    protected boolean isAudioApplicationInFocus() {
        return ((CombiModuleAudio)this.moduleFsg).getAudioApplicationFocusHandler().isTerminalModeInFocus();
    }

    public void updateActiveSource(int i, int j, int k, boolean flag, boolean flag1, int l) {
        CombiBAPAudioSource combibapaudiosource = new CombiBAPAudioSource();
        combibapaudiosource.setSourceType(i);
        combibapaudiosource.setSlotNumber(j);
        combibapaudiosource.setPartitionNumber(k);
        CombiBAPAudioListStates combibapaudioliststates = new CombiBAPAudioListStates();
        combibapaudioliststates.setMediaBrowserListAvailable(flag);
        combibapaudioliststates.setPresetListAvailable(flag1);
        combibapaudioliststates.setListState(l);
        this.updateActiveSource(combibapaudiosource, combibapaudioliststates);
    }

    /* Intercept the now-playing station update to push CarPlay artwork to the VC first (native order),
     * then run the stock update.  The picture URL/ID is set on the stationInfo by our now-playing send. */
    public void updateCurrentStation(CombiBAPCurrentStationInfo stationInfo) {
        this.pushCoverArt(stationInfo);
        super.updateCurrentStation(stationInfo);
    }

    private void pushCoverArt(CombiBAPCurrentStationInfo stationInfo) {
        if (!com.luka.carplay.core.CarPlayApp.isActive()) return;
        if (!this.isAudioApplicationInFocus()) return;
        String url = stationInfo.getPictureURL();
        if (url == null || url.length() == 0) return;
        try {
            ResourceLocator resource = new ResourceLocator(stationInfo.getPictureID(), url);
            this.pictureProvider.clearMapping();
            this.pictureProvider.addToMapping(stationInfo.getListRef(), resource);
            ((AbstractCombiModule) this.moduleFsg).getPictureManager()
                .responseCoverArt(stationInfo.getListRef(), SOURCE_TYPE_VC, resource, true);
        } catch (Exception e) {
            this.logChannel.log(1000000, "[AppConnectorTerminalMode#pushCoverArt] error: " + e.getMessage());
        }
    }

    public void updateSourceListTerminalMode(int[] aint, CombiBAPAudioSource[][] acombibapaudiosource) {
        this.logChannel
            .log(1000000, "[AppConnectorTerminalMode#updateSourceListTerminalMode] called (specified sourceTypes)");
        BAPFunctionArrayFSG bapfunctionarrayfsg = this.moduleFsg.getBAPFunctionArrayFSG(32);
        ((SourceListHandler)bapfunctionarrayfsg.getArrayHandler()).updateSources(3, aint, acombibapaudiosource);
    }

    public void updatePlayPosition(PlayPosition playposition) {
        this.logChannel.log(10000000, "[AppConnectorTerminalMode#updatePlayPosition] %1", playposition);
        if (this.isAudioApplicationInFocus()) {
            PlayPosition_Status playposition_status = new PlayPosition_Status();
            playposition_status.timePosition = playposition.getTimePosition().getTimeInSeconds();
            playposition_status.totalPlayTime = playposition.getTotalPlayTime().getTimeInSeconds();
            playposition_status.attributes.variableBitRateActive = playposition.isVariableBitRate();
            this.moduleFsg.getBAPFunctionPropertyFSG(52).sendStatusIfChanged(playposition_status);
        } else {
            this.logChannel
                .log(
                    10000000,
                    "[AppConnectorTerminalMode#updatePlayPosition] application not in focus; don't send playPosition"
                );
        }
    }
}
