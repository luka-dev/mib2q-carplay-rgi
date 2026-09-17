package de.audi.audio.context;

import com.luka.carplay.pdc.OpsAudioDrawerPolicy;
import de.audi.atip.hmi.model.IntegerListCell;
import de.audi.atip.hmi.model.ListCell;
import de.audi.atip.hmi.modelaccess.ListModelApp;
import de.audi.atip.interapp.audio.drawer.AudioDrawerContext;
import de.audi.audio.AudioEnv;
import java.util.Arrays;

public class AudioDrawerContextImpl implements AudioDrawerContext {
    private final AudioEnv env;
    private final ListModelApp listModel;
    private final OpsAudioDrawerPolicy opsDrawer;
    private final int ROW_1 = 0;

    public AudioDrawerContextImpl(AudioEnv audioenv) {
        this.env = audioenv;
        this.listModel = audioenv.getListModel(538);
        this.listModel.setMaxRows(1);
        this.listModel.setMaxColumns(23);
        ListCell[] alistcell = new ListCell[23];
        Arrays.fill(alistcell, LIST_CELL_INACTIVE);
        this.listModel.addRow(alistcell);
        // Only the front MU owns the CarPlay/side-OPS presentation policy.
        this.opsDrawer = audioenv.isFrontUnit() ? new OpsAudioDrawerPolicy(this.listModel) : null;
    }

    public void setContext(
        AudioDrawerContext.Source audiodrawercontext$source,
        AudioDrawerContext.SourceAudioState audiodrawercontext$sourceaudiostate
    ) {
        if (audiodrawercontext$source != null && audiodrawercontext$sourceaudiostate != null) {
            this.env
                .lcMain
                .log(
                    10000000,
                    "[AudioDrawerContextImpl.setContext] %1 -> %2",
                    audiodrawercontext$source,
                    audiodrawercontext$sourceaudiostate
                );
            this.updateListModel(audiodrawercontext$source.getColumn(), audiodrawercontext$sourceaudiostate.getCell());
        } else {
            this.env
                .lcMain
                .log(
                    10000,
                    "[AudioDrawerContextImpl.setContext] Illegal args! source:%1 state:%2",
                    audiodrawercontext$source,
                    audiodrawercontext$sourceaudiostate
                );
        }
    }

    private void updateListModel(int i, IntegerListCell integerlistcell) {
        if (i == PRIO_IDX_APS && this.opsDrawer != null) {
            this.opsDrawer.setRequested(integerlistcell);
        } else if (i >= 0 && i < 23) {
            if (this.listModel.getCell(0, i) != integerlistcell) {
                this.listModel.setCell(0, i, integerlistcell);
            }
        } else {
            this.env.lcMain.log(10000, "[AudioDrawerContextImpl.updateListModel] Illegal args! column:%1 ", i);
        }
    }
}
