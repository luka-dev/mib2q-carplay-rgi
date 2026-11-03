package de.audi.app.terminalmode.statemachine.commands;

import de.audi.app.terminalmode.IContext;
import de.audi.app.terminalmode.audio.IAudioManager;
import de.audi.app.terminalmode.device.TMDevice;
import de.audi.app.terminalmode.dsi.AbstractDispatcherRunnable;
import de.audi.app.terminalmode.dsi.carplay.CarplayDSILifecycleController;
import de.audi.app.terminalmode.statemachine.IStateHandler;
import de.audi.app.terminalmode.statemachine.Resource;
import de.audi.atip.log.LogChannel;

public class ReleaseAudioFocus extends AbstractCommand {
    private static final String LOGCLASS = "ReleaseAudioFocus";
    private static volatile int deferredReleaseGeneration;
    private final IContext context;
    private final IAudioManager audioManager;

    public ReleaseAudioFocus(IContext icontext) {
        super(icontext.getLogger().main(), LOGCLASS, icontext);
        this.context = icontext;
        this.audioManager = icontext.getAudioManager();
    }

    public void execute() {
        this.logger.log(1000000, "[%1.execute]", LOGCLASS);
        final int gen = ++deferredReleaseGeneration;
        if (this.audioManager.hasAudioFocus()) {
            TMDevice activeDevice = this.context.getDeviceManager().getActiveDevice();
            long delayMs = (activeDevice != null && activeDevice.isCarplayDevice() && activeDevice.isActive())
                ? CarplayDSILifecycleController.getAudioFocusReleaseDelayMs()
                : 0L;
            if (delayMs > 0L) {
                this.logger.log(
                    1000000,
                    "[%1.execute] deferring entertainment audio focus release during call/speech",
                    LOGCLASS
                );
                scheduleDeferredRelease(this.context, this.audioManager, this.logger, gen, delayMs);
            } else {
                this.audioManager.releaseAudioFocus();
            }
        }
        this.commandList.commandFinished();
    }

    private static void scheduleDeferredRelease(
        final IContext ctx,
        final IAudioManager audioMgr,
        final LogChannel log,
        final int gen,
        long delayMs
    ) {
        ctx.getDispatcher().execute(
            new AbstractDispatcherRunnable("ReleaseAudioFocus.deferred") {
                public void run() {
                    if (gen != deferredReleaseGeneration || !audioMgr.hasAudioFocus()) {
                        return;
                    }
                    IStateHandler stateHandler = (IStateHandler)ctx.get(IStateHandler.class);
                    TMDevice activeDevice = ctx.getDeviceManager().getActiveDevice();
                    boolean carplayActive = activeDevice != null
                        && activeDevice.isCarplayDevice()
                        && activeDevice.isActive();
                    if (carplayActive
                            && stateHandler != null
                            && stateHandler.getCurrentState().isResourceOwnerDevice(Resource.AUDIO_MEDIA)) {
                        return;
                    }
                    long remainingMs = carplayActive
                        ? CarplayDSILifecycleController.getAudioFocusReleaseDelayMs()
                        : 0L;
                    if (remainingMs > 0L) {
                        scheduleDeferredRelease(ctx, audioMgr, log, gen, remainingMs);
                        return;
                    }
                    log.log(
                        1000000,
                        "[%1.execute] releasing deferred audio focus after call/speech guard",
                        LOGCLASS
                    );
                    audioMgr.releaseAudioFocus();
                }
            },
            delayMs
        );
    }
}
