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
    private final TerminalModeBapCombi.EventListener eventListener;
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
                CombiBAPServiceTerminalMode.class,
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

        /* ===== CarPlay: bring the transport (bus :19810) up ALWAYS-ON at component
         * init, independent of any CarPlay session — the C hook connects once at boot
         * and stays connected.  onActivate/onDeactivate only gate the modules. ===== */
        try {
            com.luka.carplay.core.CarPlayApp.startTransport(this.context);
        } catch (Throwable t) { /* never crash the stock component init */ }
        try { com.luka.carplay.coverart.CoverArt.getInstance().start(this.eventListener); }
        catch (Throwable t) { /* optional */ }
    }

    public void deinit() {
        try { com.luka.carplay.core.CarPlayApp.onDeactivateAndWait(); }
        catch (Throwable t) { /* optional lifecycle cleanup */ }
        try { com.luka.carplay.coverart.CoverArt.getInstance().stop(this.eventListener); }
        catch (Throwable t) { /* optional */ }
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

                /* ===== CarPlay hook: device activating ===== */
                if (tmdevice.isCarplayDevice()) {
                    try {
                        com.luka.carplay.coverart.CoverArt.getInstance().resetSession();
                        this.this$0.eventListener.resetCoverMerge();
                        com.luka.carplay.core.CarPlayApp.onActivate(this.this$0.context);
                    } catch (Throwable t) { /* never crash the stock flow */ }
                }
            }

            /* dio can bounce through INVALID and then return directly as ACTIVE,
             * without a second ACTIVATING callback.  INVALID stops the modules;
             * re-arm them from ACTIVE when CarPlay is again authoritative. */
            if (tmdevice.connectionState().is(TMDevice.ConnectionState.ACTIVE)
                    && tmdevice.isCarplayDevice()
                    && !com.luka.carplay.core.CarPlayApp.isActive()) {
                try {
                    com.luka.carplay.coverart.CoverArt.getInstance().resetSession();
                    this.this$0.eventListener.resetCoverMerge();
                    com.luka.carplay.core.CarPlayApp.onActivate(this.this$0.context);
                } catch (Throwable t) { /* never crash the stock flow */ }
            }

            /* ===== CarPlay hook: device disconnected =====
             * The REAL disconnect state (from the TMDev diagnostic log) is INVALID with carplay=false —
             * NOT NOT_ATTACHED — because a detached device loses its CarPlay identity.  Matching a specific
             * ConnectionState name is therefore fragile; instead deactivate whenever the device is no longer
             * a CarPlay device while we still think CarPlay is active.  All connect states (ATTACHED /
             * ACTIVATING / ACTIVE) report carplay=true, so this only fires on a genuine drop.  onDeactivate()
             * is idempotent (no-op unless active), so it is safe.  This also unsticks the cluster: without it
             * clusterActive stayed true → CombiMapController kept blocking the stock map → "cluster overlay stayed
             * after disconnect" and "View won't return to stock". */
            if (!tmdevice.isCarplayDevice() && com.luka.carplay.core.CarPlayApp.isActive()) {
                try {
                    com.luka.carplay.core.CarPlayApp.onDeactivate();
                    com.luka.carplay.coverart.CoverArt.getInstance().resetSession();
                    this.this$0.eventListener.resetCoverMerge();
                } catch (Throwable t) { /* never crash the stock flow */ }
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

    private class EventListener extends DefaultEventListener implements com.luka.carplay.coverart.CoverArt.Callback {
        private final TerminalModeBapCombi this$0;

        /* CarPlay cover-art: last track (to re-send when late artwork arrives). */
        private String currentTitle = "";
        private String currentArtist = "";
        private String currentAlbum = "";
        private boolean sentWithoutCover = false;
        private boolean metadataFromCarPlay = false;

        private EventListener(TerminalModeBapCombi terminalmodebapcombi) {
            this.this$0 = terminalmodebapcombi;
        }

        /* Bus callbacks run on CarplayBus's reader. Marshal the BAP mutation to TerminalMode's
         * stock dispatcher and re-check the source after the handoff. */
        public void onNewCoverArt(final String path, final int artId) {
            try {
                this.this$0.context.getDispatcher().execute(new Runnable() {
                    public void run() { applyCoverArt(path, artId); }
                });
            } catch (Throwable t) { /* component is shutting down */ }
        }

        private synchronized void applyCoverArt(String path, int artId) {
            if (!com.luka.carplay.core.CarPlayApp.isActive() || !metadataFromCarPlay) return;
            if (currentTitle.length() > 0 || currentArtist.length() > 0 || currentAlbum.length() > 0) {
                int effId = artId;
                if (sentWithoutCover) { effId = artId + 1; sentWithoutCover = false; }  /* new id forces VC refresh */
                sendNowPlaying(currentTitle, currentArtist, currentAlbum, path, effId);
            }
        }

        public synchronized void updateNowPlayingData(TrackDataChangedEvent trackdatachangedevent) {
            String title = trackdatachangedevent.getTitle();
            String artist = trackdatachangedevent.getArtist();
            String album = trackdatachangedevent.getAlbum();
            if (title == null) title = "";
            if (artist == null) artist = "";
            if (album == null) album = "";
            currentTitle = title; currentArtist = artist; currentAlbum = album;
            metadataFromCarPlay = com.luka.carplay.core.CarPlayApp.isActive();

            com.luka.carplay.coverart.CoverArt ca = com.luka.carplay.coverart.CoverArt.getInstance();
            if (com.luka.carplay.core.CarPlayApp.isActive() && ca.hasCoverArt()) {
                sentWithoutCover = false;
                sendNowPlaying(title, artist, album, ca.getPath(), ca.getArtId());
            } else {
                sentWithoutCover = com.luka.carplay.core.CarPlayApp.isActive();
                sendNowPlaying(title, artist, album, null, 0);
            }
        }

        /** Never merge a new session's artwork with metadata cached for the previous CP/AA source. */
        private synchronized void resetCoverMerge() {
            currentTitle = "";
            currentArtist = "";
            currentAlbum = "";
            sentWithoutCover = false;
            metadataFromCarPlay = false;
        }

        /* Stock now-playing send + optional CarPlay cover-art picture. */
        private void sendNowPlaying(String title, String artist, String album, String coverPath, int coverId) {
            int i = title.length() > 0 ? 6 : 0;
            int j = artist.length() > 0 ? 73 : 0;
            int k = album.length() > 0 ? 74 : 0;
            CombiBAPCurrentStationInfo combibapcurrentstationinfo = new CombiBAPCurrentStationInfo();
            combibapcurrentstationinfo.setPrimaryInformation(title, i, 0);
            combibapcurrentstationinfo.setSecondaryInformation(artist, j);
            combibapcurrentstationinfo.setTertiaryInformation(album, k);
            combibapcurrentstationinfo.setListRef(0);                 /* VC rejects non-zero listRef */
            combibapcurrentstationinfo.setListAbsolutePosition(0);
            if (coverPath != null && coverPath.length() > 0 && coverId != 0) {
                combibapcurrentstationinfo.setPicture(coverId, coverPath);
            }
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
