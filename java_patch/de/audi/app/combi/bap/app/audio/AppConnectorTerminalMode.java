/*
 * AppConnectorTerminalMode - Patched for CarPlay cover art
 *
 * Mirrors native AppConnectorMedia cover art pattern:
 * - Registers IPictureProvider for cluster re-requests
 * - Pushes cover art BEFORE station info (native order)
 * - setNotification(0) called once in constructor
 *
 *
 */
package de.audi.app.combi.bap.app.audio;

import de.audi.app.bap.fw.functiontypes.BAPFunctionArrayFSG;
import de.audi.app.bap.fw.functiontypes.BAPFunctionPropertyFSG;
import de.audi.app.combi.bap.app.audio.list.SourceListHandler;
import de.audi.app.combi.bap.fw.AbstractCombiModule;
import de.audi.atip.interapp.combi.bap.audio.CombiBAPServiceTerminalMode;
import de.audi.atip.interapp.combi.bap.audio.data.CombiBAPAudioSource;
import de.audi.atip.interapp.combi.bap.audio.data.CombiBAPCurrentStationInfo;
import de.audi.atip.interapp.combi.bap.audio.data.PlayPosition;
import de.vw.mib.bap.generated.audiosd.serializer.PlayPosition_Status;
import org.dsi.ifc.global.ResourceLocator;

public class AppConnectorTerminalMode extends AbstractAppConnectorAudio implements CombiBAPServiceTerminalMode {

    private static final int SOURCE_TYPE_VC = 21;
    private final AppConnectorTerminalMode$CoverArtProvider pictureProvider;

    protected AppConnectorTerminalMode(CombiModuleAudio combiModule) {
        super(combiModule);
        this.pictureProvider = new AppConnectorTerminalMode$CoverArtProvider(this);
        combiModule.getPictureManager().registerPictureProvider(0, this.pictureProvider);
        combiModule.getPictureManager().setNotification(0);
    }

    /* Package-private accessor for CoverArtProvider (mirrors synthetic access$000) */
    AbstractCombiModule getCombiModule() {
        return (AbstractCombiModule) this.moduleFsg;
    }

    protected int getAudioApplication() {
        return 3;
    }

    protected String getAudioApplicationName() {
        return "TerminalMode";
    }

    protected boolean isAudioApplicationInFocus() {
        return ((CombiModuleAudio)this.moduleFsg).getAudioApplicationFocusHandler().isTerminalModeInFocus();
    }

    public void updateActiveSource(int sourceType, int slotNumber, int partitionNumber,
                                   boolean mediaBrowserAvailable, boolean presetListAvailable, int listState) {
        CombiBAPAudioSource audioSource = new CombiBAPAudioSource();
        audioSource.setSourceType(sourceType);
        audioSource.setSlotNumber(slotNumber);
        audioSource.setPartitionNumber(partitionNumber);
        CombiBAPAudioListStates listStates = new CombiBAPAudioListStates();
        listStates.setMediaBrowserListAvailable(mediaBrowserAvailable);
        listStates.setPresetListAvailable(presetListAvailable);
        listStates.setListState(listState);
        this.updateActiveSource(audioSource, listStates);
    }

    public void updateCurrentStation(CombiBAPCurrentStationInfo stationInfo) {
        this.pushCoverArt(stationInfo);
        super.updateCurrentStation(stationInfo);
    }

    private void pushCoverArt(CombiBAPCurrentStationInfo stationInfo) {
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
            this.logChannel.log(1000000,
                "[AppConnectorTerminalMode#pushCoverArt] error: " + e.getMessage());
        }
    }

    public void updateSourceListTerminalMode(int[] sourceTypes, CombiBAPAudioSource[][] audioSources) {
        this.logChannel.log(1000000, "[AppConnectorTerminalMode#updateSourceListTerminalMode] called");
        BAPFunctionArrayFSG arrayFSG = this.moduleFsg.getBAPFunctionArrayFSG(32);
        ((SourceListHandler)arrayFSG.getArrayHandler()).updateSources(3, sourceTypes, audioSources);
    }

    public void updatePlayPosition(PlayPosition playPosition) {
        this.logChannel.log(10000000, "[AppConnectorTerminalMode#updatePlayPosition] %1", playPosition);
        if (this.isAudioApplicationInFocus()) {
            PlayPosition_Status positionStatus = new PlayPosition_Status();
            positionStatus.timePosition = playPosition.getTimePosition().getTimeInSeconds();
            positionStatus.totalPlayTime = playPosition.getTotalPlayTime().getTimeInSeconds();
            positionStatus.attributes.variableBitRateActive = playPosition.isVariableBitRate();
            this.moduleFsg.getBAPFunctionPropertyFSG(52).sendStatusIfChanged(positionStatus);
        } else {
            this.logChannel.log(10000000, "[AppConnectorTerminalMode#updatePlayPosition] not in focus");
        }
    }
}
