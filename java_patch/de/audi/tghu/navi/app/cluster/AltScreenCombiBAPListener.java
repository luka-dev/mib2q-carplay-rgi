package de.audi.tghu.navi.app.cluster;

import com.luka.carplay.core.AltScreenModule;
import com.luka.carplay.framework.Log;

import de.audi.atip.hmi.model.BaseListRow;
import de.audi.atip.hmi.model.IntegerListCell;
import de.audi.atip.hmi.model.ListCell;
import de.audi.atip.hmi.modelaccess.ListModelApp;

import de.audi.atip.log.LogChannel;
import de.audi.atip.mmicombi.IViewSizeManager;
import de.audi.tghu.command.ICommandListFactory;
import de.audi.tghu.navi.app.NavigationEnv;
import de.audi.tghu.navi.app.OperationManager;
import de.audi.tghu.navi.app.SpeechManager;
import de.audi.tghu.navi.app.audio.AudioStateMachine;
import de.audi.tghu.navi.app.map.MapManager;

/**
 * Stock BAP boundary for CarPlay KDK composition and the steering-wheel roller.
 *
 * The instrument cluster sends wheel rotation as Navigation BAP MapScale.steps.
 * Stock CombiBAPListener.setMapScale() applies it to mapKombi via
 * activeContext.increment(400476, steps).  While altScreen owns the cluster we
 * translate the signed step to CarPlay's virtual knob and acknowledge the BAP
 * request with the unchanged native scale.  Outside altScreen, the stock method
 * runs unchanged.
 *
 * Diagnostics only: logScreenLayout() writes the navigation's own cockpit-map layout table
 * (list model 176, read by LayoutProviderG22Kombi / GUIKombiFPK) to the CarPlay log whenever it
 * changes.  It is the car's own record of where the gauge tubes, the tab line and the info line
 * sit, so it can confirm or replace the hand-measured CarPlay safe areas
 * (docs/cluster-and-rgi/VC_LAYOUT_AND_FONTS.md#layout-options).  Read only.
 */
public final class AltScreenCombiBAPListener extends CombiBAPListener {
    /* list model 176 row columns, as AbstractLayoutProvider names them */
    private static final int SL_WIDTH = 0, SL_HEIGHT = 1, SL_INFOLINE_BOTTOM = 8, SL_TOPLINE = 9;
    private static final int SL_TUBE_LEFT = 11, SL_TUBE_RIGHT = 12, SL_TUBE_LEFT_KB = 13, SL_TUBE_RIGHT_KB = 14;
    private static final int SL_MAP_OFFSET_LEFT = 24, SL_MAP_OFFSET_TOP = 25;
    private static String lastScreenLayout;
    private static boolean screenLayoutFailed;

    public AltScreenCombiBAPListener(
        ClusterService service,
        LogChannel logChannel,
        NavigationEnv env,
        SpeechManager speechManager,
        OperationManager operationManager,
        AudioStateMachine audioStateMachine,
        MapManager mapManager,
        ICommandListFactory commandListFactory,
        IViewSizeManager viewSizeManager
    ) {
        super(
            service,
            logChannel,
            env,
            speechManager,
            operationManager,
            audioStateMachine,
            mapManager,
            commandListFactory,
            viewSizeManager
        );
        logScreenLayout("init", -1);
    }

    /** Log list model 176 once per distinct content, with the visible map rectangle the stock
     *  navigation derives from it (LayoutProviderG22Kombi.getVisibleArea): x = tubeLeft -
     *  mapOffsetLeft, y = topLine - mapOffsetTop, w = width - tubeLeft - tubeRight,
     *  h = height - topLine - infoLine, for the normal ("large") and the Kb tube columns. */
    private void logScreenLayout(String reason, int largeMapView) {
        try {
            ListModelApp m = this.env == null ? null : this.env.getListModel(176);
            if (m == null) return;
            int rows = m.getLength();
            StringBuffer sb = new StringBuffer();
            sb.append("rows=").append(rows).append(" maxColumns=").append(m.getMaxColumns());
            int[] row1 = null;
            for (int r = 0; r < rows && r < 8; r++) {
                BaseListRow row = m.getRow(r);
                sb.append(" row").append(r).append('=');
                if (row == null) { sb.append("null"); continue; }
                int n = row.getColumnCount();
                int[] values = new int[n];
                sb.append('[');
                for (int c = 0; c < n && c < 48; c++) {
                    ListCell cell = row.getCell(c);
                    if (c > 0) sb.append(',');
                    if (cell instanceof IntegerListCell) {
                        values[c] = ((IntegerListCell) cell).getValue();
                        sb.append(values[c]);
                    } else {
                        sb.append(cell == null ? "-" : "?");
                    }
                }
                sb.append(']');
                if (r == 1) row1 = values;
            }
            String table = sb.toString();
            synchronized (AltScreenCombiBAPListener.class) {
                if (table.equals(lastScreenLayout)) return;
                lastScreenLayout = table;
            }
            String visible = "";
            if (row1 != null && row1.length > SL_MAP_OFFSET_TOP) {
                visible = " audiVisible normal=" + visibleArea(row1, SL_TUBE_LEFT, SL_TUBE_RIGHT)
                    + " kb=" + visibleArea(row1, SL_TUBE_LEFT_KB, SL_TUBE_RIGHT_KB);
            }
            Log.w("ClusterLayout", "screen layout list176 reason=" + reason
                + " largeMap=" + largeMapView
                + " style=" + AltScreenModule.styleLabel()
                + " smallView=" + AltScreenModule.isSmallScreenViewArea()
                + visible + " " + table);
        } catch (Throwable t) {
            synchronized (AltScreenCombiBAPListener.class) {
                if (screenLayoutFailed) return;
                screenLayoutFailed = true;
            }
            Log.w("ClusterLayout", "screen layout list176 unreadable: " + t);
        }
    }

    private static String visibleArea(int[] v, int left, int right) {
        int x = v[left] - v[SL_MAP_OFFSET_LEFT];
        int y = v[SL_TOPLINE] - v[SL_MAP_OFFSET_TOP];
        int w = v[SL_WIDTH] - v[left] - v[right];
        int h = v[SL_HEIGHT] - v[SL_TOPLINE] - v[SL_INFOLINE_BOTTOM];
        return "(" + x + "," + y + " " + w + "x" + h + ")";
    }

    /** Apply the accepted stock state before its Status acknowledgement. This boundary
     * also covers internal supplementary visibility changes and initial Status replay,
     * which bypass the two-argument BAP request setter. */
    protected void updateMapVisibility() {
        com.luka.carplay.cluster.ClusterLayerController.onVcVisibility(this.supplementaryMapViewVisible);
        logScreenLayout("visibility", -1);
        super.updateMapVisibility();
    }

    public void setMapPresentation(boolean largeMapView, boolean leftMenu, boolean rightMenu) {
        com.luka.carplay.cluster.ClusterLayerController.onVcPresentation(largeMapView);
        /* The same cockpit stage also drives the CarPlay full/compact view area when
         * NAV_VIEW_SIZE_CHOICE stays silent (view_source in /mnt/app/carplay_layout.cfg). */
        AltScreenModule.onVcMapPresentation(largeMapView);
        logScreenLayout("presentation", largeMapView ? 1 : 0);
        super.setMapPresentation(largeMapView, leftMenu, rightMenu);
    }

    public void setMapScale(int steps) {
        if (AltScreenModule.isConnected()) {
            AltScreenModule.consumeSteeringWheelScale(steps);
            /* Complete the BAP SetGet with the unchanged native-map status. */
            this.updateMapScale();
            return;
        }
        super.setMapScale(steps);
    }

    public void setMapScaleSetting(int setting) {
        if (AltScreenModule.isConnected()) {
            /* This is autoZoom, not the steering-wheel OK key.  Do not mutate
             * the hidden Audi map; setMapScale() sends the one Status ACK. */
            return;
        }
        super.setMapScaleSetting(setting);
    }
}
