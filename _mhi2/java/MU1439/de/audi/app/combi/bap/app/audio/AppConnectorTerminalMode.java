/*
 * Decompiled with CFR 0.152.
 * 
 * Could not load the following classes:
 *  de.audi.app.bap.fw.functiontypes.BAPFunctionArrayFSG
 *  de.audi.app.combi.bap.app.audio.AbstractAppConnectorAudio
 *  de.audi.app.combi.bap.app.audio.CombiBAPAudioListStates
 *  de.audi.app.combi.bap.app.audio.CombiModuleAudio
 *  de.audi.app.combi.bap.app.audio.list.SourceListHandler
 *  de.audi.atip.interapp.combi.bap.audio.CombiBAPServiceTerminalMode
 *  de.audi.atip.interapp.combi.bap.audio.data.CombiBAPAudioSource
 *  de.audi.atip.interapp.combi.bap.audio.data.PlayPosition
 *  de.vw.mib.bap.generated.audiosd.serializer.PlayPosition_Status
 *  de.vw.mib.bap.requests.StatusProperty
 */
package de.audi.app.combi.bap.app.audio;

import de.audi.app.bap.fw.functiontypes.BAPFunctionArrayFSG;
import de.audi.app.combi.bap.app.audio.AbstractAppConnectorAudio;
import de.audi.app.combi.bap.app.audio.CombiBAPAudioListStates;
import de.audi.app.combi.bap.app.audio.CombiModuleAudio;
import de.audi.app.combi.bap.app.audio.list.SourceListHandler;
import de.audi.atip.interapp.combi.bap.audio.CombiBAPServiceTerminalMode;
import de.audi.atip.interapp.combi.bap.audio.data.CombiBAPAudioSource;
import de.audi.atip.interapp.combi.bap.audio.data.PlayPosition;
import de.vw.mib.bap.generated.audiosd.serializer.PlayPosition_Status;
import de.vw.mib.bap.requests.StatusProperty;

public class AppConnectorTerminalMode
extends AbstractAppConnectorAudio
implements CombiBAPServiceTerminalMode {
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

    public void updateActiveSource(int n, int n2, int n3, boolean bl, boolean bl2, int n4) {
        CombiBAPAudioSource combiBAPAudioSource = new CombiBAPAudioSource();
        combiBAPAudioSource.setSourceType(n);
        combiBAPAudioSource.setSlotNumber(n2);
        combiBAPAudioSource.setPartitionNumber(n3);
        CombiBAPAudioListStates combiBAPAudioListStates = new CombiBAPAudioListStates();
        combiBAPAudioListStates.setMediaBrowserListAvailable(bl);
        combiBAPAudioListStates.setPresetListAvailable(bl2);
        combiBAPAudioListStates.setListState(n4);
        this.updateActiveSource(combiBAPAudioSource, combiBAPAudioListStates);
    }

    public void updateSourceListTerminalMode(int[] nArray, CombiBAPAudioSource[][] combiBAPAudioSourceArray) {
        this.logChannel.log(1000000, "[AppConnectorTerminalMode#updateSourceListTerminalMode] called (specified sourceTypes)");
        BAPFunctionArrayFSG bAPFunctionArrayFSG = this.moduleFsg.getBAPFunctionArrayFSG(32);
        ((SourceListHandler)bAPFunctionArrayFSG.getArrayHandler()).updateSources(3, nArray, combiBAPAudioSourceArray);
    }

    public void updatePlayPosition(PlayPosition playPosition) {
        this.logChannel.log(10000000, "[AppConnectorTerminalMode#updatePlayPosition] %1", (Object)playPosition);
        if (this.isAudioApplicationInFocus()) {
            PlayPosition_Status playPosition_Status = new PlayPosition_Status();
            playPosition_Status.timePosition = playPosition.getTimePosition().getTimeInSeconds();
            playPosition_Status.totalPlayTime = playPosition.getTotalPlayTime().getTimeInSeconds();
            playPosition_Status.attributes.variableBitRateActive = playPosition.isVariableBitRate();
            this.moduleFsg.getBAPFunctionPropertyFSG(52).sendStatusIfChanged((StatusProperty)playPosition_Status);
        } else {
            this.logChannel.log(10000000, "[AppConnectorTerminalMode#updatePlayPosition] application not in focus; don't send playPosition");
        }
    }
}
