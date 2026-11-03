package de.audi.app.terminalmode.statemachine.commands;

import de.audi.app.terminalmode.IContext;
import de.audi.app.terminalmode.audio.IAudioManager;
import de.audi.app.terminalmode.device.TMDevice;
import de.audi.app.terminalmode.dsi.carplay.CarplayDSILifecycleController;

public class ReleaseAudioFocus extends AbstractCommand {
    private static final String LOGCLASS = "ReleaseAudioFocus";
    private final IContext context;
    private final IAudioManager audioManager;

    public ReleaseAudioFocus(IContext icontext) {
        super(icontext.getLogger().main(), LOGCLASS, icontext);
        this.context = icontext;
        this.audioManager = icontext.getAudioManager();
    }

    public void execute() {
        this.logger.log(1000000, "[%1.execute]", LOGCLASS);
        if (this.audioManager.hasAudioFocus()) {
            TMDevice activeDevice = this.context.getDeviceManager().getActiveDevice();
            if (activeDevice != null
                    && activeDevice.isCarplayDevice()
                    && activeDevice.isActive()
                    && CarplayDSILifecycleController.shouldKeepAudioFocusOnRelease()) {
                this.logger.log(
                    1000000,
                    "[%1.execute] keep entertainment audio focus on CarPlay during call/speech",
                    LOGCLASS
                );
            } else {
                this.audioManager.releaseAudioFocus();
            }
        }
        this.commandList.commandFinished();
    }
}
