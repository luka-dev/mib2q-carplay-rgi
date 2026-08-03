package de.audi.app.terminalmode.combi;

import de.audi.app.terminalmode.IContext;
import de.audi.app.terminalmode.ITerminalModeComponent;
import de.audi.app.terminalmode.audio.AudioConnectionState;
import de.audi.app.terminalmode.audio.IAudioStateListener;
import de.audi.app.terminalmode.device.IActiveDeviceStateListener;
import de.audi.app.terminalmode.device.TMDevice;
import de.audi.app.terminalmode.events.DefaultEventListener;
import de.audi.app.terminalmode.events.IEventListener;
import de.audi.app.terminalmode.events.TrackDataChangedEvent;
import de.audi.app.terminalmode.events.TrackPlayPositionEvent;
import de.audi.app.terminalmode.osgi.IServiceManager;
import de.audi.app.terminalmode.osgi.IServiceTracker;
import de.audi.atip.interapp.bap.data.TimeStamp;
import de.audi.atip.interapp.combi.bap.audio.CombiBAPServiceTerminalMode;
import de.audi.atip.interapp.combi.bap.audio.data.CombiBAPCurrentStationInfo;
import de.audi.atip.interapp.combi.bap.audio.data.PlayPosition;
import de.audi.atip.interapp.def.DefaultServiceTrackerCustomizer;
import de.audi.atip.log.LogChannel;
import org.osgi.framework.BundleContext;
import org.osgi.framework.ServiceReference;

public class TerminalModeBapCombi implements ITerminalModeComponent {
    private static final String LOGCLASS = "TerminalModeBapCombi";
    private final IContext context;
    private final BundleContext bundleContext;
    private final LogChannel lc;
    private final IServiceTracker serviceTracker;
    private final IServiceManager serviceManager;
    private final IEventListener eventListener;
    private final TerminalModeBapCombi.CachingEventListener cachingEventListener;
    private volatile CombiBAPServiceTerminalMode bapService;
    private TerminalModeBapCombi.ActiveDeviceStateListener activeDeviceListener;
    private final IAudioStateListener audioStateListener;
    public TerminalModeBapCombi(IContext icontext) {
        this.context = icontext;
        this.bundleContext = this.context.getFramework().getBundleCxt();
        this.lc = this.context.getLogger().main();
        this.bapService = new NullCombiBAPService(this.lc);
        this.serviceManager = this.context.getServiceManager();
        this.serviceTracker = this.serviceManager
            .createServiceTracker(
                de.audi.atip.interapp.combi.bap.audio.CombiBAPServiceTerminalMode.class,
                new TerminalModeBapCombi.CombiBapServiceTrackerCustomizer(this)
            );
        this.eventListener = new TerminalModeBapCombi.EventListener(this);
        this.cachingEventListener = new TerminalModeBapCombi.CachingEventListener(this);
        this.activeDeviceListener = new TerminalModeBapCombi.ActiveDeviceStateListener(this);
        this.audioStateListener = new TerminalModeBapCombi.AudioStateListener(this);
    }

    public void init() {
        this.serviceTracker.open();
        this.context.getEventBus().registerListener(this.cachingEventListener);
        this.context.getDeviceManager().addActiveDeviceListener(this.activeDeviceListener);
        this.context.getAudioManager().addAudioContextListener(this.audioStateListener);
    }

    public void deinit() {
        this.serviceTracker.close();
        this.context.getEventBus().unregisterListener(this.eventListener);
        this.context.getEventBus().unregisterListener(this.cachingEventListener);
        this.context.getDeviceManager().removeActiveDeviceListener(this.activeDeviceListener);
        this.context.getAudioManager().removeAudioContextListener(this.audioStateListener);
    }

    private final class ActiveDeviceStateListener implements IActiveDeviceStateListener {
        private final TerminalModeBapCombi this$0;

        private ActiveDeviceStateListener(TerminalModeBapCombi terminalmodebapcombi) {
            this.this$0 = terminalmodebapcombi;
        }

        public void updateActiveDeviceState(TMDevice tmdevice) {
            if (tmdevice.connectionState().is(TMDevice.ConnectionState.ACTIVATING)) {
                this.this$0.bapService.updateActiveInfoState(9);
                int i = this.getBapSourceType(tmdevice);
                this.this$0.bapService.updateActiveSource(i, 0, 0, false, false, 0);
                this.this$0.bapService.updateCurrentStation(new CombiBAPCurrentStationInfo());
                this.this$0
                    .bapService
                    .updatePlayPosition(
                        PlayPosition.builder()
                            .setTimePosition(TimeStamp.getInstanceFromSeconds(65535))
                            .setTotalPlayTime(TimeStamp.getInstanceFromSeconds(65535))
                            .build()
                    );
                this.this$0.bapService.updateActiveInfoState(9);

                /* ===== PROBE: CarPlay connect → announce the Smartphone tab ===== */
                if (tmdevice.isCarplayDevice()) {
                    try { com.luka.carplay.probe.SmartphoneTabProbe.onConnect(this.this$0.context); }
                    catch (Throwable t) { /* never crash stock flow */ }
                }
            }

            /* ===== PROBE: CarPlay disconnect → retract the tab ===== */
            if (tmdevice.connectionState().is(TMDevice.ConnectionState.NOT_ATTACHED)) {
                if (tmdevice.isCarplayDevice()) {
                    try { com.luka.carplay.probe.SmartphoneTabProbe.onDisconnect(this.this$0.context); }
                    catch (Throwable t) { }
                }
            }
        }

        private int getBapSourceType(TMDevice tmdevice) {
            if (tmdevice.isCarplayDevice()) {
                return 37;
            } else {
                return tmdevice.isAndroidAutoDevice() ? 39 : 40;
            }
        }
    }

    private final class AudioStateListener implements IAudioStateListener {
        private final TerminalModeBapCombi this$0;

        private AudioStateListener(TerminalModeBapCombi terminalmodebapcombi) {
            this.this$0 = terminalmodebapcombi;
        }

        public void audioStateChanged(AudioConnectionState audioconnectionstate) {
        }

        public void audioFocusChanged(boolean flag) {
            this.this$0.lc.log(10000000, "[%1.audioFocusChanged] %2", "TerminalModeBapCombi", new Boolean(flag));
            if (flag) {
                this.this$0.context.getEventBus().registerListener(this.this$0.eventListener);
                this.this$0.cachingEventListener.sendCachedEvents();
            } else {
                this.this$0.context.getEventBus().unregisterListener(this.this$0.eventListener);
            }
        }
    }

    private class CachingEventListener extends DefaultEventListener {
        private final TrackPlayPositionEvent INVALID_TIME;
        private final TrackDataChangedEvent INVALID_TRACK;
        private TrackPlayPositionEvent lastPlayPositionEvent;
        private TrackDataChangedEvent lastTrackData;
        private final TerminalModeBapCombi this$0;

        private CachingEventListener(TerminalModeBapCombi terminalmodebapcombi) {
            this.this$0 = terminalmodebapcombi;
            this.INVALID_TIME = new TrackPlayPositionEvent(-1, 0);
            this.INVALID_TRACK = new TrackDataChangedEvent.Builder().build();
            this.lastPlayPositionEvent = this.INVALID_TIME;
            this.lastTrackData = this.INVALID_TRACK;
        }

        public void updateNowPlayingData(TrackDataChangedEvent trackdatachangedevent) {
            this.lastTrackData = trackdatachangedevent;
        }

        public void updatePlayPosition(TrackPlayPositionEvent trackplaypositionevent) {
            this.lastPlayPositionEvent = trackplaypositionevent;
        }

        public void sendCachedEvents() {
            if (!this.INVALID_TIME.equals(this.lastPlayPositionEvent)) {
                this.this$0.eventListener.updatePlayPosition(this.lastPlayPositionEvent);
            }

            if (!this.INVALID_TRACK.equals(this.lastTrackData)) {
                this.this$0.eventListener.updateNowPlayingData(this.lastTrackData);
            }
        }

        public void clearCachedEvents() {
            this.lastPlayPositionEvent = this.INVALID_TIME;
            this.lastTrackData = this.INVALID_TRACK;
        }
    }

    private final class CombiBapServiceTrackerCustomizer extends DefaultServiceTrackerCustomizer {
        private final TerminalModeBapCombi this$0;

        private CombiBapServiceTrackerCustomizer(TerminalModeBapCombi terminalmodebapcombi) {
            this.this$0 = terminalmodebapcombi;
        }

        public Object addingService(ServiceReference servicereference) {
            CombiBAPServiceTerminalMode combibapserviceterminalmode = (CombiBAPServiceTerminalMode)this.this$0
                .context
                .getServiceManager()
                .getService(servicereference);
            if (combibapserviceterminalmode != null) {
                this.this$0.lc.log(1000000, "[TerminalModeCombi.addingService] %1", combibapserviceterminalmode);
                this.this$0.bapService = combibapserviceterminalmode;
            }

            return combibapserviceterminalmode;
        }

        public void removedService(ServiceReference servicereference, Object object) {
            this.this$0.lc.log(1000000, "[TerminalModeCombi.removedService] %1", object);
            this.this$0.bapService = new NullCombiBAPService(this.this$0.lc);
            this.this$0.bundleContext.ungetService(servicereference);
        }
    }

    private class EventListener extends DefaultEventListener {
        private final TerminalModeBapCombi this$0;

        private EventListener(TerminalModeBapCombi terminalmodebapcombi) {
            this.this$0 = terminalmodebapcombi;
        }

        public void updateNowPlayingData(TrackDataChangedEvent trackdatachangedevent) {
            int i = trackdatachangedevent.getTitle().length() > 0 ? 6 : 0;
            int j = trackdatachangedevent.getArtist().length() > 0 ? 73 : 0;
            int k = trackdatachangedevent.getAlbum().length() > 0 ? 74 : 0;
            CombiBAPCurrentStationInfo combibapcurrentstationinfo = new CombiBAPCurrentStationInfo();
            combibapcurrentstationinfo.setPrimaryInformation(trackdatachangedevent.getTitle(), i, 0);
            combibapcurrentstationinfo.setSecondaryInformation(trackdatachangedevent.getArtist(), j);
            combibapcurrentstationinfo.setTertiaryInformation(trackdatachangedevent.getAlbum(), k);
            this.this$0.lc.log(1000000, "[TerminalModeCombi.updateNowPlayingData] STATES_NO_ERROR!");
            this.this$0.bapService.updateCurrentStation(combibapcurrentstationinfo);
            this.this$0.bapService.updateActiveInfoState(0);
        }

        public void updatePlayPosition(TrackPlayPositionEvent trackplaypositionevent) {
            if (trackplaypositionevent.getTotalTimeOfTrack() == 0) {
                this.this$0
                    .bapService
                    .updatePlayPosition(
                        PlayPosition.builder()
                            .setTimePosition(TimeStamp.getInstanceFromSeconds(65535))
                            .setTotalPlayTime(TimeStamp.getInstanceFromSeconds(65535))
                            .build()
                    );
            } else {
                this.this$0
                    .bapService
                    .updatePlayPosition(
                        PlayPosition.builder()
                            .setTimePosition(TimeStamp.getInstanceFromSeconds(trackplaypositionevent.getPlayTime()))
                            .setTotalPlayTime(
                                TimeStamp.getInstanceFromSeconds(trackplaypositionevent.getTotalTimeOfTrack())
                            )
                            .build()
                    );
            }
        }
    }
}
