import java.lang.reflect.Constructor;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.lang.reflect.Proxy;
import java.util.ArrayList;
import java.util.List;
import sun.misc.Unsafe;
import com.luka.carplay.core.CarPlayApp;
import de.audi.atip.interapp.combi.bap.audio.CombiBAPServiceTerminalMode;
import de.audi.atip.interapp.combi.bap.audio.data.CombiBAPCurrentStationInfo;
import de.audi.atip.log.LogChannel;
import de.esolutions.fw.util.commons.job.DispatcherBase;
import de.esolutions.fw.util.commons.job.Job;
import de.audi.app.terminalmode.combi.TerminalModeBapCombi;
import de.audi.app.terminalmode.dsi.IDSIResource;
import de.audi.app.terminalmode.dsi.carplay.CarplayDSILifecycleController.DSICarplayListenerImpl;
import de.audi.app.terminalmode.dsi.carplay.ICarplayDSIControllerListener;
import de.audi.app.terminalmode.events.IEventBus;
import de.audi.app.terminalmode.events.TrackDataChangedEvent;
import de.audi.app.terminalmode.events.TrackPlayPositionEvent;
import org.dsi.ifc.carplay.TrackData;

/**
 * Verifies the YouTube Music NowPlaying sequence on MMI (DSICarplayListenerImpl)
 * and Virtual Cockpit (TerminalModeBapCombi):
 *   1. upcomingPlaybackLockScreenRenderer (new title/artist/album, duration = 0)
 *   2. clearNowPlayingInfo while singleVideo lifecycleState < 2 (empty strings, duration = 0)
 *   3. activeVideo lifecycleState == 2 (new title/artist/album, duration > 0)
 */
public final class NowPlayingClearTest {
    static final Unsafe U;
    static {
        try {
            Field f = Unsafe.class.getDeclaredField("theUnsafe");
            f.setAccessible(true);
            U = (Unsafe)f.get(null);
        } catch (Exception e) { throw new AssertionError(e); }
    }
    static final LogChannel LOG = new LogChannel() {
        public void log(int a,String b,Object c,Object d,Object e,Object f,long g,long h,long i,int j,Throwable k) { }
        public void log(int a,int b,Object c,Object d,Object e,Object f,long g,long h,long i,int j,Throwable k) { }
    };

    static void setCarPlayActive(boolean active) throws Exception {
        Field f = CarPlayApp.class.getDeclaredField("active");
        f.setAccessible(true);
        f.setBoolean(null, active);
    }

    static void check(boolean ok, String message) {
        if (!ok) throw new AssertionError(message);
    }

    public static void main(String[] args) throws Exception {
        /* 1. DSICarplayListenerImpl stays stock: events pass immediately and duplicate
         *    (0, 0) play positions on upcomingPlayback -> clearNowPlayingInfo are
         *    deduplicated so MMI's ModelGroup is never flushed during the empty state. */
        final List<TrackDataChangedEvent> busTracks = new ArrayList<TrackDataChangedEvent>();
        final List<TrackPlayPositionEvent> busPositions = new ArrayList<TrackPlayPositionEvent>();
        DispatcherBase dummyDispatcher = (DispatcherBase)U.allocateInstance(DispatcherBase.class);
        IEventBus bus = (IEventBus)Proxy.newProxyInstance(
            IEventBus.class.getClassLoader(), new Class[]{IEventBus.class}, (p, m, a) -> {
                if ("updateNowPlayingData".equals(m.getName())) busTracks.add((TrackDataChangedEvent)a[0]);
                if ("updatePlayPosition".equals(m.getName())) busPositions.add((TrackPlayPositionEvent)a[0]);
                return m.getReturnType() == boolean.class ? Boolean.TRUE : null;
            });
        DSICarplayListenerImpl dsiListener = new DSICarplayListenerImpl(LOG, dummyDispatcher, bus, null);
        dsiListener.updateNowPlayingData(new TrackData("Song B", 0, "Album B", "Artist B", "", ""), 1);
        dsiListener.updateNowPlayingData(new TrackData("", 0, "", "", "", ""), 1);
        dsiListener.updateNowPlayingData(new TrackData("Song B", 215000, "Album B", "Artist B", "", ""), 1);
        check(busTracks.size() == 3, "DSICarplayListenerImpl must remain stock pass-through");
        check(busPositions.size() == 2,
            "notifyPlayPosition must deduplicate (0,0) across upcomingPlayback -> clearNowPlayingInfo");
        check(busPositions.get(0).getTotalTimeOfTrack() == 0 && busPositions.get(1).getTotalTimeOfTrack() == 215,
            "unexpected play position sequence");

        /* 2. TerminalModeBapCombi ignores transient empty clearNowPlayingInfo on VC
         *    while CarPlay is active, preserving pi_Type=6 and late cover art merge. */
        final List<CombiBAPCurrentStationInfo> stations = new ArrayList<CombiBAPCurrentStationInfo>();
        TerminalModeBapCombi combi = (TerminalModeBapCombi)U.allocateInstance(TerminalModeBapCombi.class);
        Field lcField = TerminalModeBapCombi.class.getDeclaredField("lc");
        lcField.setAccessible(true);
        lcField.set(combi, LOG);
        CombiBAPServiceTerminalMode bapProxy = (CombiBAPServiceTerminalMode)Proxy.newProxyInstance(
            CombiBAPServiceTerminalMode.class.getClassLoader(),
            new Class[]{CombiBAPServiceTerminalMode.class},
            (p, m, a) -> {
                if ("updateCurrentStation".equals(m.getName())) {
                    stations.add((CombiBAPCurrentStationInfo)a[0]);
                }
                return null;
            });
        Field bapField = TerminalModeBapCombi.class.getDeclaredField("bapService");
        bapField.setAccessible(true);
        bapField.set(combi, bapProxy);

        Class<?> evClass = Class.forName("de.audi.app.terminalmode.combi.TerminalModeBapCombi$EventListener");
        Constructor<?> evCtor = evClass.getDeclaredConstructors()[0];
        evCtor.setAccessible(true);
        Object[] ctorArgs = new Object[evCtor.getParameterTypes().length];
        for (int i = 0; i < ctorArgs.length; i++) {
            if (evCtor.getParameterTypes()[i] == TerminalModeBapCombi.class) ctorArgs[i] = combi;
        }
        Object evListener = evCtor.newInstance(ctorArgs);
        Method updateTrack = evClass.getDeclaredMethod("updateNowPlayingData", new Class[]{TrackDataChangedEvent.class});
        updateTrack.setAccessible(true);
        Method applyCoverArt = evClass.getDeclaredMethod("applyCoverArt", new Class[]{String.class, int.class});
        applyCoverArt.setAccessible(true);
        Method resetCoverMerge = evClass.getDeclaredMethod("resetCoverMerge", new Class[0]);
        resetCoverMerge.setAccessible(true);
        Field currentTitleField = evClass.getDeclaredField("currentTitle");
        currentTitleField.setAccessible(true);

        setCarPlayActive(true);
        try {
            for (int i = 0; i < busTracks.size(); i++) {
                updateTrack.invoke(evListener, new Object[]{busTracks.get(i)});
                if (i == 1) {
                    /* Transient cover-art clear during YT Music buffering must not re-send without picture */
                    applyCoverArt.invoke(evListener, new Object[]{"", Integer.valueOf(0)});
                    /* Late cover art arriving during YT Music's clearNowPlayingInfo buffering window */
                    applyCoverArt.invoke(evListener, new Object[]{"/tmp/cover.jpg", Integer.valueOf(7)});
                }
            }
            check(stations.size() == 3,
                "VC must receive upcomingPlayback, cover-art refresh, and activeVideo — never the empty clear");
            check("Song B".equals(currentTitleField.get(evListener)),
                "currentTitle was lost during clearNowPlayingInfo");

            /* Session reset clears state so a new session starts clean */
            resetCoverMerge.invoke(evListener, new Object[0]);
            check("".equals(currentTitleField.get(evListener)), "resetCoverMerge did not clear currentTitle");
        } finally {
            setCarPlayActive(false);
        }

        /* 3. YouTube Music tears down its realtime AirPlay stream (updateMainAudioType(0))
         *    on every track skip and opens a new stream (updateMainAudioType(2)) 250-710ms
         *    later. Verify that transient updateMainAudioType(0) is deferred and cancelled
         *    when updateMainAudioType(2) arrives within 1500ms, whereas a genuine stop
         *    forwards 0 after the timer expires. */
        final List<Runnable> delayedTasks = new ArrayList<Runnable>();
        final List<Long> delayedMs = new ArrayList<Long>();
        DispatcherBase testDispatcher = new DispatcherBase(null, null, null, null) {
            public Job execute(Object job) {
                ((Runnable)job).run();
                return null;
            }
            public Job execute(Object job, long delay) {
                delayedTasks.add((Runnable)job);
                delayedMs.add(Long.valueOf(delay));
                return null;
            }
        };
        final List<Integer> forwardedAudioTypes = new ArrayList<Integer>();
        ICarplayDSIControllerListener dioListener = (ICarplayDSIControllerListener)Proxy.newProxyInstance(
            ICarplayDSIControllerListener.class.getClassLoader(),
            new Class[]{ICarplayDSIControllerListener.class},
            (p, m, a) -> {
                if ("updateMainAudioType".equals(m.getName())) {
                    forwardedAudioTypes.add((Integer)a[0]);
                }
                return null;
            });
        DSICarplayListenerImpl audioListener = new DSICarplayListenerImpl(LOG, testDispatcher, bus, null);
        Field dioField = DSICarplayListenerImpl.class.getDeclaredField("dioDSIControllerListener");
        dioField.setAccessible(true);
        dioField.set(audioListener, dioListener);

        /* Track A starts media stream (type 2) */
        audioListener.updateMainAudioType(2, 1);
        check(forwardedAudioTypes.size() == 1 && forwardedAudioTypes.get(0).intValue() == 2,
            "initial updateMainAudioType(2) must be forwarded immediately");

        /* User skips to Track B in YouTube Music: stream torn down (0), then reopened (2) 300ms later */
        audioListener.updateMainAudioType(0, 1);
        check(forwardedAudioTypes.size() == 1,
            "transient updateMainAudioType(0) on track skip must not be forwarded immediately");
        check(delayedTasks.size() == 1 && delayedMs.get(0).longValue() == 1500L,
            "updateMainAudioType(0) must schedule 1500ms deferred task");

        audioListener.updateMainAudioType(2, 1);
        check(forwardedAudioTypes.size() == 2 && forwardedAudioTypes.get(1).intValue() == 2,
            "reopened stream updateMainAudioType(2) must be forwarded immediately for ACK");

        /* When the 1500ms timer later fires, the deferred 0 must be cancelled */
        delayedTasks.remove(0).run();
        check(forwardedAudioTypes.size() == 2,
            "cancelled deferred updateMainAudioType(0) must not fire after stream reopened");

        /* Genuine playback stop: updateMainAudioType(0) with no new stream forwards after 1500ms */
        audioListener.updateMainAudioType(0, 1);
        check(delayedTasks.size() == 1, "second stop must schedule deferred task");
        delayedTasks.remove(0).run();
        check(forwardedAudioTypes.size() == 3 && forwardedAudioTypes.get(2).intValue() == 0,
            "genuine stop must forward updateMainAudioType(0) after 1500ms");

        /* Explicit MMI borrow/take of RESOURCE_MAIN_AUDIO (id=1, owner=1) forwards 0 immediately */
        audioListener.updateMainAudioType(2, 1);
        Method trackReq = DSICarplayListenerImpl.class.getDeclaredMethod(
            "trackRequestedScreenOwner", new Class[]{IDSIResource[].class});
        trackReq.setAccessible(true);
        IDSIResource mmiAudioTake = (IDSIResource)Proxy.newProxyInstance(
            IDSIResource.class.getClassLoader(),
            new Class[]{IDSIResource.class},
            (p, m, a) -> {
                if ("getDSIResourceId".equals(m.getName())) return Integer.valueOf(1);
                if ("getOwner".equals(m.getName())) return Integer.valueOf(1);
                return Integer.valueOf(0);
            });
        trackReq.invoke(audioListener, new Object[]{new IDSIResource[]{mmiAudioTake}});
        audioListener.updateMainAudioType(0, 1);
        check(forwardedAudioTypes.size() == 5 && forwardedAudioTypes.get(4).intValue() == 0 && delayedTasks.isEmpty(),
            "updateMainAudioType(0) after explicit MMI audio borrow must be forwarded immediately");

        System.out.println("NowPlayingClearTest: stock MMI pass-through, VC clear filter, and mainAudioType skip guard PASS");
    }
}
