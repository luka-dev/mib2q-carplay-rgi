package de.audi.app.combi.bap.app.audio;

import de.audi.app.bap.fw.functiontypes.BAPFunctionArrayFSG;
import de.audi.app.combi.bap.app.audio.list.SourceListHandler;
import de.audi.atip.interapp.combi.bap.audio.CombiBAPServiceTerminalMode;
import de.audi.atip.interapp.combi.bap.audio.data.CombiBAPAudioSource;
import de.audi.atip.interapp.combi.bap.audio.data.PlayPosition;
import de.vw.mib.bap.generated.audiosd.serializer.PlayPosition_Status;

public class AppConnectorTerminalMode extends AbstractAppConnectorAudio implements CombiBAPServiceTerminalMode {
    protected AppConnectorTerminalMode(CombiModuleAudio combiModuleAudio) {
        super(combiModuleAudio);
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

    public void updateActiveSource(int i, int j, int k, boolean bl, boolean bl1, int l) {
        CombiBAPAudioSource combiBAPAudioSource = new CombiBAPAudioSource();
        combiBAPAudioSource.setSourceType(i);
        combiBAPAudioSource.setSlotNumber(j);
        combiBAPAudioSource.setPartitionNumber(k);
        CombiBAPAudioListStates combiBAPAudioListStates = new CombiBAPAudioListStates();
        combiBAPAudioListStates.setMediaBrowserListAvailable(bl);
        combiBAPAudioListStates.setPresetListAvailable(bl1);
        combiBAPAudioListStates.setListState(l);
        this.updateActiveSource(combiBAPAudioSource, combiBAPAudioListStates);
    }

    public void updateSourceListTerminalMode(int[] i, CombiBAPAudioSource[][] combiBAPAudioSources) {
        this.logChannel
            .log(1000000, "[AppConnectorTerminalMode#updateSourceListTerminalMode] called (specified sourceTypes)");
        BAPFunctionArrayFSG bAPFunctionArrayFSG = this.moduleFsg.getBAPFunctionArrayFSG(32);
        ((SourceListHandler)bAPFunctionArrayFSG.getArrayHandler()).updateSources(3, i, combiBAPAudioSources);
    }

    public void updatePlayPosition(PlayPosition playPosition) {
        this.logChannel.log(10000000, "[AppConnectorTerminalMode#updatePlayPosition] %1", playPosition);
        if (this.isAudioApplicationInFocus()) {
            PlayPosition_Status playPosition_Status = new PlayPosition_Status();
            playPosition_Status.timePosition = playPosition.getTimePosition().getTimeInSeconds();
            playPosition_Status.totalPlayTime = playPosition.getTotalPlayTime().getTimeInSeconds();
            playPosition_Status.attributes.variableBitRateActive = playPosition.isVariableBitRate();
            this.moduleFsg.getBAPFunctionPropertyFSG(52).sendStatusIfChanged(playPosition_Status);
        } else {
            this.logChannel
                .log(
                    10000000,
                    "[AppConnectorTerminalMode#updatePlayPosition] application not in focus; don't send playPosition"
                );
        }
    }
}
