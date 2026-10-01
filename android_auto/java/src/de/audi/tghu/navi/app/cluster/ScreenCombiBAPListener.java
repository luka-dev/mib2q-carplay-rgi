package de.audi.tghu.navi.app.cluster;

import de.audi.atip.log.LogChannel;
import de.audi.atip.mmicombi.IViewSizeManager;
import de.audi.tghu.command.ICommandListFactory;
import de.audi.tghu.navi.app.NavigationEnv;
import de.audi.tghu.navi.app.OperationManager;
import de.audi.tghu.navi.app.SpeechManager;
import de.audi.tghu.navi.app.audio.AudioStateMachine;
import de.audi.tghu.navi.app.map.MapManager;

/**
 * Stock BAP boundary for CarPlay KDK composition.
 *
 * VC's Fct44 (KDK visibility) and Fct54 (map presentation/stage) are forwarded to
 * the layer controller before stock acknowledges them.  The steering-wheel roller
 * (BAP MapScale -> setMapScale) keeps zooming the head unit's native map exactly as
 * stock (luka 80 / stock 74) - EXCEPT while the cockpit shows the Android Auto mirror
 * (ScreenModule.isMirrorActive(): ctx 81/82 during a phone session): then the steps go
 * to Android Auto as rotary events (com.sq5.aa.input.AaRollerInput, SD kill flag
 * sq5_roller_off) and the request is acknowledged with the unchanged scale.
 */
public final class ScreenCombiBAPListener extends CombiBAPListener {
    public ScreenCombiBAPListener(
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
    }

    /** Apply the accepted stock state before its Status acknowledgement. This boundary
     * also covers internal supplementary visibility changes and initial Status replay,
     * which bypass the two-argument BAP request setter. */
    protected void updateMapVisibility() {
        com.luka.carplay.cluster.ClusterLayerController.onVcVisibility(this.supplementaryMapViewVisible);
        /* SQ5 AA port: full map view (log-only for the mirror) */
        com.luka.carplay.cluster.ClusterLayerController.onVcMapVisibility(this.lvdsMapVisible);
        super.updateMapVisibility();
    }

    /** SQ5 AA port: steering-wheel roller -> Android Auto while the AA mirror is in the cockpit.
     * Stock MU0918 (CombiBAPListener l.1664-1675) zooms the native kombi map, then updateMapScale()
     * sends the MapScale Status; when AA takes the steps only that Status acknowledgement is sent. */
    public void setMapScale(int steps) {
        com.sq5.aa.input.AaWheelProbe.log("bap setMapScale steps=" + steps);
        /* Cockpit options menu: while it is open the roller moves its selection (no zoom anywhere). */
        boolean menu = false;
        try {
            menu = com.sq5.aa.input.AaClusterMenu.onRoller(steps, System.currentTimeMillis());
        } catch (Throwable t) {
            menu = false;
        }
        if (menu) {
            try { this.updateMapScale(); } catch (Throwable t) { /* ack only */ }
            return;
        }
        boolean toAa = false;
        try {
            toAa = com.sq5.aa.input.AaRollerInput.onRoller(steps,
                com.luka.carplay.core.ScreenModule.isMirrorActive());
        } catch (Throwable t) {
            toAa = false;
        }
        if (!toAa) {
            super.setMapScale(steps);
            return;
        }
        try {
            this.updateMapScale();
        } catch (Throwable t) {
            com.sq5.aa.luka.AaLog.log("roller: MapScale status ack failed: " + t);
        }
    }

    public void setMapPresentation(boolean largeMapView, boolean leftMenu, boolean rightMenu) {
        com.sq5.aa.input.AaWheelProbe.log("bap setMapPresentation large=" + largeMapView + " leftMenu=" + leftMenu + " rightMenu=" + rightMenu);
        com.luka.carplay.cluster.ClusterLayerController.onVcPresentation(largeMapView);
        super.setMapPresentation(largeMapView, leftMenu, rightMenu);
    }

    /* Cockpit options menu, stage A (2026-09-29): log the native map-menu actions, stock handling unchanged. */
    public void setMapColor(int n) {
        com.sq5.aa.input.AaWheelProbe.log("bap setMapColor " + n);
        super.setMapColor(n);
    }

    public void setMapType(int n, int n2) {
        com.sq5.aa.input.AaWheelProbe.log("bap setMapType " + n + " " + n2);
        super.setMapType(n, n2);
    }

    public void setMapView(int n, int n2) {
        com.sq5.aa.input.AaWheelProbe.log("bap setMapView " + n + " " + n2);
        super.setMapView(n, n2);
    }

    public void setMapOrientation(int n) {
        com.sq5.aa.input.AaWheelProbe.log("bap setMapOrientation " + n);
        super.setMapOrientation(n);
    }

    public void setMapScaleSetting(int n) {
        com.sq5.aa.input.AaWheelProbe.log("bap setMapScaleSetting " + n);
        super.setMapScaleSetting(n);
    }
}
