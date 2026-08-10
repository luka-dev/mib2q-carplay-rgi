package de.audi.tghu.navi.app.cluster;

import com.luka.carplay.core.AltScreenModule;

import de.audi.atip.log.LogChannel;
import de.audi.atip.mmicombi.IViewSizeManager;
import de.audi.tghu.command.ICommandListFactory;
import de.audi.tghu.navi.app.NavigationEnv;
import de.audi.tghu.navi.app.OperationManager;
import de.audi.tghu.navi.app.SpeechManager;
import de.audi.tghu.navi.app.audio.AudioStateMachine;
import de.audi.tghu.navi.app.map.MapManager;

/**
 * Narrow interception point for the steering-wheel roller.
 *
 * The instrument cluster sends wheel rotation as Navigation BAP MapScale.steps.
 * Stock CombiBAPListener.setMapScale() applies it to mapKombi via
 * activeContext.increment(400476, steps).  While altScreen owns the cluster we
 * translate the signed step to CarPlay's virtual knob and acknowledge the BAP
 * request with the unchanged native scale.  Outside altScreen, the stock method
 * runs unchanged.
 */
public final class AltScreenCombiBAPListener extends CombiBAPListener {
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
