/*
 * MU0918 stock AudioDrawerContextImpl, rebuilt from the lsd.jar decompile (log level restored),
 * plus luka-dev's APS-column policy for the SQ5 Android Auto PDC port: on the front unit the
 * APS column (0) of drawer model 538 goes through OpsAudioDrawerPolicy, which publishes INACTIVE
 * only while PdcSmallStageGuard keeps Android Auto beside the right OPS popup and republishes
 * the last stock value on release.  Every other column is stock.
 */
package de.audi.audio.context;

import com.sq5.aa.pdc.OpsAudioDrawerPolicy;
import de.audi.atip.hmi.model.IntegerListCell;
import de.audi.atip.hmi.model.ListCell;
import de.audi.atip.hmi.modelaccess.ListModelApp;
import de.audi.atip.interapp.audio.drawer.AudioDrawerContext;
import de.audi.audio.AudioEnv;
import java.util.Arrays;

public class AudioDrawerContextImpl
implements AudioDrawerContext {
    private final AudioEnv env;
    private final ListModelApp listModel;
    private final int ROW_1;
    private final OpsAudioDrawerPolicy opsDrawer;

    public AudioDrawerContextImpl(AudioEnv audioEnv) {
        this.ROW_1 = 0;
        this.env = audioEnv;
        this.listModel = audioEnv.getListModel(538);
        this.listModel.setMaxRows(1);
        this.listModel.setMaxColumns(23);
        ListCell[] listCellArray = new ListCell[23];
        Arrays.fill(listCellArray, LIST_CELL_INACTIVE);
        this.listModel.addRow(listCellArray);
        // SQ5 PDC: only the front MU owns the projection/side-OPS presentation policy.
        OpsAudioDrawerPolicy policy = null;
        try {
            if (audioEnv.isFrontUnit()) policy = new OpsAudioDrawerPolicy(this.listModel);
        } catch (Throwable t) {
            policy = null;
        }
        this.opsDrawer = policy;
    }

    public void setContext(AudioDrawerContext.Source source, AudioDrawerContext.SourceAudioState sourceAudioState) {
        if (source == null || sourceAudioState == null) {
            this.env.lcMain.log(10000, "[AudioDrawerContextImpl.setContext] Illegal args! source:%1 state:%2", source, sourceAudioState);
            return;
        }
        this.env.lcMain.log(10000000, "[AudioDrawerContextImpl.setContext] %1 -> %2", source, sourceAudioState);
        this.updateListModel(source.getColumn(), sourceAudioState.getCell());
    }

    private void updateListModel(int n, IntegerListCell integerListCell) {
        if (n < 0 || n >= 23) {
            this.env.lcMain.log(10000, "[AudioDrawerContextImpl.updateListModel] Illegal args! column:%1 ", (long)n);
            return;
        }
        if (n == OpsAudioDrawerPolicy.PRIO_IDX_APS && this.opsDrawer != null) {
            this.opsDrawer.setRequested(integerListCell);
            return;
        }
        if (this.listModel.getCell(0, n) != integerListCell) {
            this.listModel.setCell(0, n, integerListCell);
        }
    }
}
