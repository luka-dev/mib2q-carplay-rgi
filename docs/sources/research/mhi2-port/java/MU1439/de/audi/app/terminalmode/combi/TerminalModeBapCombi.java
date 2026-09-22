/*
 * Decompiled with CFR 0.152.
 * 
 * Could not load the following classes:
 *  de.audi.app.terminalmode.IContext
 *  de.audi.app.terminalmode.ITerminalModeComponent
 *  de.audi.app.terminalmode.audio.IAudioStateListener
 *  de.audi.app.terminalmode.combi.NullCombiBAPService
 *  de.audi.app.terminalmode.combi.TerminalModeBapCombi$ActiveDeviceStateListener
 *  de.audi.app.terminalmode.combi.TerminalModeBapCombi$AudioStateListener
 *  de.audi.app.terminalmode.combi.TerminalModeBapCombi$CachingEventListener
 *  de.audi.app.terminalmode.combi.TerminalModeBapCombi$CombiBapServiceTrackerCustomizer
 *  de.audi.app.terminalmode.combi.TerminalModeBapCombi$EventListener
 *  de.audi.app.terminalmode.device.IActiveDeviceStateListener
 *  de.audi.app.terminalmode.events.IEventListener
 *  de.audi.app.terminalmode.osgi.IServiceManager
 *  de.audi.app.terminalmode.osgi.IServiceTracker
 *  de.audi.atip.interapp.combi.bap.audio.CombiBAPServiceTerminalMode
 *  de.audi.atip.log.LogChannel
 *  org.osgi.framework.BundleContext
 *  org.osgi.util.tracker.ServiceTrackerCustomizer
 */
package de.audi.app.terminalmode.combi;

import de.audi.app.terminalmode.IContext;
import de.audi.app.terminalmode.ITerminalModeComponent;
import de.audi.app.terminalmode.audio.IAudioStateListener;
import de.audi.app.terminalmode.combi.NullCombiBAPService;
import de.audi.app.terminalmode.combi.TerminalModeBapCombi;
import de.audi.app.terminalmode.device.IActiveDeviceStateListener;
import de.audi.app.terminalmode.events.IEventListener;
import de.audi.app.terminalmode.osgi.IServiceManager;
import de.audi.app.terminalmode.osgi.IServiceTracker;
import de.audi.atip.interapp.combi.bap.audio.CombiBAPServiceTerminalMode;
import de.audi.atip.log.LogChannel;
import org.osgi.framework.BundleContext;
import org.osgi.util.tracker.ServiceTrackerCustomizer;

public class TerminalModeBapCombi
implements ITerminalModeComponent {
    private static final String LOGCLASS = "TerminalModeBapCombi";
    private final IContext context;
    private final BundleContext bundleContext;
    private final LogChannel lc;
    private final IServiceTracker serviceTracker;
    private final IServiceManager serviceManager;
    private final IEventListener eventListener;
    private final CachingEventListener cachingEventListener;
    private volatile CombiBAPServiceTerminalMode bapService;
    private ActiveDeviceStateListener activeDeviceListener;
    private final IAudioStateListener audioStateListener;
    static Class class$de$audi$atip$interapp$combi$bap$audio$CombiBAPServiceTerminalMode;

    public TerminalModeBapCombi(IContext iContext) {
        this.context = iContext;
        this.bundleContext = this.context.getFramework().getBundleCxt();
        this.lc = this.context.getLogger().main();
        this.bapService = new NullCombiBAPService(this.lc);
        this.serviceManager = this.context.getServiceManager();
        this.serviceTracker = this.serviceManager.createServiceTracker(class$de$audi$atip$interapp$combi$bap$audio$CombiBAPServiceTerminalMode == null ? (class$de$audi$atip$interapp$combi$bap$audio$CombiBAPServiceTerminalMode = TerminalModeBapCombi.class$("de.audi.atip.interapp.combi.bap.audio.CombiBAPServiceTerminalMode")) : class$de$audi$atip$interapp$combi$bap$audio$CombiBAPServiceTerminalMode, (ServiceTrackerCustomizer)new CombiBapServiceTrackerCustomizer(this, null));
        this.eventListener = new EventListener(this, null);
        this.cachingEventListener = new CachingEventListener(this, null);
        this.activeDeviceListener = new ActiveDeviceStateListener(this, null);
        this.audioStateListener = new AudioStateListener(this, null);
    }

    public void init() {
        this.serviceTracker.open();
        this.context.getEventBus().registerListener((IEventListener)this.cachingEventListener);
        this.context.getDeviceManager().addActiveDeviceListener((IActiveDeviceStateListener)this.activeDeviceListener);
        this.context.getAudioManager().addAudioContextListener(this.audioStateListener);
    }

    public void deinit() {
        this.serviceTracker.close();
        this.context.getEventBus().unregisterListener(this.eventListener);
        this.context.getEventBus().unregisterListener((IEventListener)this.cachingEventListener);
        this.context.getDeviceManager().removeActiveDeviceListener((IActiveDeviceStateListener)this.activeDeviceListener);
        this.context.getAudioManager().removeAudioContextListener(this.audioStateListener);
    }

    static /* synthetic */ Class class$(String string) {
        try {
            return Class.forName(string);
        }
        catch (ClassNotFoundException classNotFoundException) {
            throw new NoClassDefFoundError().initCause(classNotFoundException);
        }
    }

    static /* synthetic */ CombiBAPServiceTerminalMode access$500(TerminalModeBapCombi terminalModeBapCombi) {
        return terminalModeBapCombi.bapService;
    }

    static /* synthetic */ IEventListener access$600(TerminalModeBapCombi terminalModeBapCombi) {
        return terminalModeBapCombi.eventListener;
    }

    static /* synthetic */ IContext access$700(TerminalModeBapCombi terminalModeBapCombi) {
        return terminalModeBapCombi.context;
    }

    static /* synthetic */ LogChannel access$800(TerminalModeBapCombi terminalModeBapCombi) {
        return terminalModeBapCombi.lc;
    }

    static /* synthetic */ CombiBAPServiceTerminalMode access$502(TerminalModeBapCombi terminalModeBapCombi, CombiBAPServiceTerminalMode combiBAPServiceTerminalMode) {
        terminalModeBapCombi.bapService = combiBAPServiceTerminalMode;
        return terminalModeBapCombi.bapService;
    }

    static /* synthetic */ BundleContext access$900(TerminalModeBapCombi terminalModeBapCombi) {
        return terminalModeBapCombi.bundleContext;
    }

    static /* synthetic */ CachingEventListener access$1000(TerminalModeBapCombi terminalModeBapCombi) {
        return terminalModeBapCombi.cachingEventListener;
    }
}
