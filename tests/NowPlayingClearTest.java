import java.lang.reflect.Field;
import java.lang.reflect.Proxy;
import java.util.ArrayList;
import java.util.List;
import sun.misc.Unsafe;
import de.audi.atip.log.LogChannel;
import de.esolutions.fw.util.commons.job.DispatcherBase;
import de.esolutions.fw.util.commons.job.Job;
import de.audi.app.terminalmode.events.IEventBus;
import de.audi.app.terminalmode.events.TrackDataChangedEvent;
import de.audi.app.terminalmode.events.TrackPlayPositionEvent;
import de.audi.app.terminalmode.dsi.carplay.CarplayDSILifecycleController.DSICarplayListenerImpl;
import org.dsi.ifc.carplay.TrackData;

/** Runs the shipping listener with a deterministic dispatcher clock. */
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
    static final class Scheduler extends DispatcherBase {
        long now;
        List<Object[]> tasks;
        Scheduler() { super("now-playing-test", null, null, null); }
        public Job execute(Object task, long delay) {
            tasks.add(new Object[]{Long.valueOf(now + delay), task});
            return null;
        }
        void advance(long time) {
            now = time;
            for (int i = 0; i < tasks.size();) {
                Object[] task = tasks.get(i);
                if (((Long)task[0]).longValue() <= now) {
                    tasks.remove(i);
                    ((Runnable)task[1]).run();
                } else { i++; }
            }
        }
    }
    static final class Fixture {
        final Scheduler scheduler;
        final DSICarplayListenerImpl listener;
        final List<TrackDataChangedEvent> tracks = new ArrayList<TrackDataChangedEvent>();
        final List<TrackPlayPositionEvent> positions = new ArrayList<TrackPlayPositionEvent>();
        Fixture() throws Exception {
            // Do not start stock worker threads: only scheduling is replaced.
            scheduler = (Scheduler)U.allocateInstance(Scheduler.class);
            scheduler.tasks = new ArrayList<Object[]>();
            IEventBus bus = (IEventBus)Proxy.newProxyInstance(
                IEventBus.class.getClassLoader(), new Class[]{IEventBus.class}, (p, m, a) -> {
                    if (m.getName().equals("updateNowPlayingData")) tracks.add((TrackDataChangedEvent)a[0]);
                    if (m.getName().equals("updatePlayPosition")) positions.add((TrackPlayPositionEvent)a[0]);
                    return m.getReturnType() == boolean.class ? Boolean.TRUE : null;
                });
            listener = new DSICarplayListenerImpl(LOG, scheduler, bus, null);
        }
        void send(String title, int duration) {
            listener.updateNowPlayingData(new TrackData(title, duration, "", "", "", ""), 1);
        }
        void reset() throws Exception {
            java.lang.reflect.Method m = DSICarplayListenerImpl.class.getDeclaredMethod("resetPlaybackState");
            m.setAccessible(true);
            m.invoke(listener);
        }
    }
    static void check(boolean ok, String message) {
        if (!ok) throw new AssertionError(message);
    }
    public static void main(String[] args) throws Exception {
        Fixture repeated = new Fixture();
        repeated.send("Old track", 180000);
        repeated.send("", 0);
        repeated.scheduler.advance(600);
        repeated.send("", 90000); // latest untagged track duration must survive coalescing
        repeated.scheduler.advance(799);
        check(repeated.tracks.size() == 1, "cleared before 800 ms");
        repeated.scheduler.advance(800);
        check(repeated.tracks.size() == 2, "repeated empty update postponed first clear deadline");
        check("".equals(repeated.tracks.get(1).getTitle()), "old title retained");
        check(repeated.positions.get(repeated.positions.size() - 1).getTotalTimeOfTrack() == 90,
            "clear published stale duration");
        repeated.scheduler.advance(2000);
        check(repeated.tracks.size() == 2, "duplicate delayed clear");

        Fixture replaced = new Fixture();
        replaced.send("Old track", 180000);
        replaced.send("", 0);
        replaced.scheduler.advance(600);
        replaced.send("New track", 120000);
        replaced.send("", 0); // independent second clear window, ending at 1400 ms
        replaced.scheduler.advance(800);
        check(replaced.tracks.size() == 2, "old clear erased the new track");
        replaced.scheduler.advance(1400);
        check(replaced.tracks.size() == 3, "new clear window was not armed");

        Fixture reset = new Fixture();
        reset.send("Old session", 180000);
        reset.send("", 0);
        reset.scheduler.advance(600);
        reset.reset();
        reset.send("New session", 120000);
        reset.send("", 0);
        reset.scheduler.advance(800);
        check(reset.tracks.size() == 2, "old session clear survived reset");
        reset.scheduler.advance(1400);
        check(reset.tracks.size() == 3, "reset left pending-clear state stuck");

        Fixture initial = new Fixture();
        initial.send("", 90000);
        check(initial.tracks.size() == 1 && initial.scheduler.tasks.isEmpty(),
            "initial untagged track was unnecessarily delayed");
        System.out.println("NowPlayingClearTest: bounded first-empty deadline, latest duration, replacement and session reset PASS");
    }
}
