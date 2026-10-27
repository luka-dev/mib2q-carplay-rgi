import com.luka.carplay.bus.CarplayBus;
import com.luka.carplay.framework.Log;
import com.luka.carplay.rgd.BAPBridge;
import com.luka.carplay.rgd.RouteGuidance;
import com.sq5.aa.luka.AaLaneFeed;
import com.sq5.aa.luka.AaLog;
import com.sq5.aa.luka.AaRgState;
import de.audi.atip.interapp.combi.bap.navi.CombiBAPServiceNavi;
import de.audi.atip.interapp.combi.bap.navi.data.CombiBAPNaviLaneGuidanceData;

import java.io.File;
import java.io.FileOutputStream;
import java.lang.reflect.Field;
import java.lang.reflect.InvocationHandler;
import java.lang.reflect.Method;
import java.lang.reflect.Proxy;

/**
 * Android Auto lanes: the gal hook's /tmp/sq5_aa_lanes record (AaLaneFeed) -> AaRgState frame ->
 * real luka RouteGuidance.onFrame -> real BAPBridge -> FctID 24 (recording CombiBAPServiceNavi) and the
 * renderer lane snapshot (BAPBridge.rendererLaneGuidance, the value sent to maneuver_render).
 * Run from AaLukaReplayTest (build.sh) or alone:
 *   java -Xverify:none -Xint -cp build/test;build/test-stubs;build/sq5_aa_luka.jar;<lsd_ic.jar> AaLaneGuidanceTest
 */
public final class AaLaneGuidanceTest {
    static int failures = 0;
    static int checks = 0;

    static void check(boolean ok, String what) {
        checks++;
        if (!ok) {
            failures++;
            System.out.println("FAIL lanes: " + what);
        }
    }

    static Field field(Object o, String name) throws Exception {
        for (Class c = o.getClass(); c != null; c = c.getSuperclass()) {
            try { Field f = c.getDeclaredField(name); f.setAccessible(true); return f; }
            catch (NoSuchFieldException e) { }
        }
        throw new NoSuchFieldException(name);
    }
    static Object get(Object o, String n) throws Exception { return field(o, n).get(o); }
    static void set(Object o, String n, Object v) throws Exception { field(o, n).set(o, v); }

    /** Records FctID 24 (updateLaneGuidance); everything else is accepted silently. */
    static final class Hud implements InvocationHandler {
        boolean laneOn;
        CombiBAPNaviLaneGuidanceData[] lanes;
        int laneSends;

        public synchronized Object invoke(Object proxy, Method m, Object[] a) {
            if (m.getName().equals("updateLaneGuidance")) {
                laneOn = ((Boolean) a[0]).booleanValue();
                lanes = (CombiBAPNaviLaneGuidanceData[]) a[1];
                laneSends++;
            }
            if (m.getReturnType() == Boolean.TYPE) return Boolean.TRUE;
            if (m.getReturnType() == Integer.TYPE) return new Integer(0);
            return null;
        }
    }

    /** The record exactly as navxlate.c nav_lanes_record writes it (256 bytes, space padded, '\n' last). */
    static byte[] record(String head, String lanes, String body, String end) {
        byte[] b = new byte[256];
        for (int i = 0; i < b.length; i++) b[i] = (byte) ' ';
        String s = head + "\nlanes=" + lanes + "\n" + body + "\n" + end + "\n";
        for (int i = 0; i < s.length(); i++) b[i] = (byte) s.charAt(i);
        b[255] = (byte) '\n';
        return b;
    }
    static byte[] record(int seq, int n, String body) {
        return record("SQ5L1 " + seq, String.valueOf(n), body, "end=" + seq);
    }
    static AaLaneFeed.Lanes parse(byte[] b) { return AaLaneFeed.parse(b, b.length); }

    static void parser() throws Exception {
        AaLaneFeed.Lanes l = parse(record(7, 4, "1,0,4,0|1,1|5,1,1,0|8,1"));
        check(l != null && l.count == 4, "4-lane record parses");
        check(l.shape[0].length == 2 && l.shape[0][0] == 1 && !l.hl[0][0] && l.shape[0][1] == 4 && !l.hl[0][1],
            "lane 0 = straight, left, none highlighted");
        check(l.shape[1].length == 1 && l.shape[1][0] == 1 && l.hl[1][0], "lane 1 = straight highlighted");
        check(l.shape[2][0] == 5 && l.hl[2][0] && l.shape[2][1] == 1 && !l.hl[2][1], "lane 2 = right highlighted, straight");
        check(l.shape[3][0] == 8 && l.hl[3][0], "lane 3 = U-turn left highlighted");
        check(parse(record(8, 4, "1,0,4,0|1,1|5,1,1,0|8,1")).key.equals(l.key), "same lanes, new seq -> same key");
        check(parse(record(9, 0, "")) == AaLaneFeed.NONE, "lanes=0 -> no lanes");
        check(parse(record(9, 2, "1,0|")).count == 2 && parse(record(9, 2, "1,0|")).shape[1].length == 0,
            "empty lane (no directions) kept");
        check(parse(record("SQ5L1 7", "1", "1,1", "end=6")) == null, "torn record (seq differs) rejected");
        check(parse(record("SQ5L2 7", "1", "1,1", "end=7")) == null, "wrong magic rejected");
        check(parse(record(7, 9, "1,1|1,1|1,1|1,1|1,1|1,1|1,1|1,1|1,1")) == null, "more than 8 lanes rejected");
        check(parse(record(7, 2, "1,1")) == null, "fewer groups than lanes rejected");
        check(parse(record(7, 1, "1,1|1,1")) == null, "more groups than lanes rejected");
        check(parse(record(7, 1, "1,2")) == null, "highlighted not 0/1 rejected");
        check(parse(record(7, 1, "a,1")) == null, "bad shape rejected");
        check(parse(record(7, 1, "1,1,")) == null, "dangling comma rejected");
        check(parse(record(7, 1, "1,0,2,0,3,0,4,0,5,0")) == null, "more than 4 directions rejected");
        check(parse(record(7, 0, "1,1")) == null, "lanes=0 with a body rejected");
        check(parse(record("SQ5L1 7", "-1", "", "end=7")) == null, "negative count rejected");
        check(parse(new byte[256]) == null && parse(new byte[0]) == null && AaLaneFeed.parse(null, 0) == null,
            "empty / zero-filled record rejected");
        byte[] cut = record(7, 1, "1,1");
        check(AaLaneFeed.parse(cut, 12) == null, "truncated record rejected");

        File f = File.createTempFile("sq5_aa_lanes", ".rec");
        f.deleteOnExit();
        FileOutputStream o = new FileOutputStream(f);
        o.write(record(3, 1, "3,1"));
        o.close();
        AaLaneFeed.Lanes r = AaLaneFeed.read(f.getPath());
        check(r.count == 1 && r.shape[0][0] == 3 && r.hl[0][0], "read() from file");
        o = new FileOutputStream(f);
        o.write(record("SQ5L1 4", "1", "3,1", "end=3"));
        o.close();
        check(AaLaneFeed.read(f.getPath()) == AaLaneFeed.NONE, "persistently torn file -> no lanes");
        check(AaLaneFeed.read(f.getPath() + ".missing") == AaLaneFeed.NONE, "missing file -> no lanes");

        int[] want = {1000, 0, -45, 45, -90, 90, -135, 135, -180, 180, 1000};
        for (int s = 0; s <= 10; s++) check(AaLaneFeed.angle(s) == want[s], "shape " + s + " -> angle " + want[s]);
    }

    static String keyLine(String frame, String key) {
        int i = frame.indexOf("\n" + key + ":");
        if (i < 0) return null;
        int e = frame.indexOf('\n', i + 1);
        return frame.substring(i + 1, e);
    }

    static void state() {
        AaRgState s = new AaRgState();
        AaLaneFeed.Lanes l = parse(record(7, 4, "1,0,4,0|1,1|5,1,1,0|8,1"));
        check(!s.onLanes(l) && s.lanes().length() == 0, "lanes ignored without a route");
        check(s.onTurn("Main St", 2, 4, 0, 0, 1), "route start");
        check("lane_guidance_showing:n:0".equals(keyLine(s.snapshot(), "lane_guidance_showing")), "route without lanes: showing 0");
        check(s.onLanes(l), "lanes on");
        String f = s.snapshot();
        check("lane_guidance_showing:n:1".equals(keyLine(f, "lane_guidance_showing")), "showing 1");
        check("lane_guidance_index:n:1".equals(keyLine(f, "lane_guidance_index")) && "lg0_index:n:1".equals(keyLine(f, "lg0_index")),
            "lane event 1 in lg slot 0");
        check("lane_guidance_slot:n:0".equals(keyLine(f, "lane_guidance_slot")), "slot 0");
        check("lg0_lane_count:n:4".equals(keyLine(f, "lg0_lane_count")), "count 4");
        check("lg0_lane_complete:n:1".equals(keyLine(f, "lg0_lane_complete")), "complete");
        check("lg0_lane_positions:s:0,1,2,3".equals(keyLine(f, "lg0_lane_positions")), "positions");
        check("lg0_lane_directions:s:1000,0,90,-180".equals(keyLine(f, "lg0_lane_directions")),
            "directions = highlighted angle or 1000: " + keyLine(f, "lg0_lane_directions"));
        check("lg0_lane_status:s:0,2,2,2".equals(keyLine(f, "lg0_lane_status")), "status 2 = highlighted");
        check("lg0_lane_angles:s:0,-90|0|90,0|-180".equals(keyLine(f, "lg0_lane_angles")),
            "angles: " + keyLine(f, "lg0_lane_angles"));
        check(!s.onLanes(parse(record(8, 4, "1,0,4,0|1,1|5,1,1,0|8,1"))), "same lanes, new seq: no change");
        check(s.onTurn("Elm St", 1, 4, 0, 0, 1) && s.snapshot().indexOf("lg0_lane_count:n:4\n") >= 0,
            "new maneuver keeps the lane event");
        check(s.onLanes(parse(record(9, 1, "4,1"))) && s.laneEvent() == 2
            && "lg0_lane_directions:s:-90".equals(keyLine(s.snapshot(), "lg0_lane_directions")), "lane change -> event 2");
        check(s.onLanes(null) && "lane_guidance_showing:n:0".equals(keyLine(s.snapshot(), "lane_guidance_showing"))
            && "lg0_index:n:-1".equals(keyLine(s.snapshot(), "lg0_index")), "garbled/missing -> lanes cleared");
        check(s.onLanes(parse(record(10, 1, "0,1"))) && "lg0_lane_directions:s:1000".equals(keyLine(s.snapshot(), "lg0_lane_directions"))
            && "lg0_lane_status:s:0".equals(keyLine(s.snapshot(), "lg0_lane_status")), "UNKNOWN shape highlighted -> unknown, not preferred");
        s.onLanes(l);
        check(s.onTurn("", 0, 0, 0, 0, 2) && s.lanes().length() == 0
            && "lane_guidance_showing:n:0".equals(keyLine(s.snapshot(), "lane_guidance_showing")), "route end clears lanes");
    }

    static void luka() throws Exception {
        RouteGuidance rgd = new RouteGuidance();
        BAPBridge bridge = new BAPBridge();
        Hud hud = new Hud();
        set(bridge, "initialized", Boolean.TRUE);
        set(bridge, "appConnectorNavi", Proxy.newProxyInstance(AaLaneGuidanceTest.class.getClassLoader(),
            new Class[]{CombiBAPServiceNavi.class}, hud));
        set(bridge, "nativeStopAttempted", Boolean.TRUE);
        set(rgd, "bap", bridge);
        set(rgd, "running", Boolean.TRUE);
        bridge.setPresentationListener((BAPBridge.PresentationListener) get(rgd, "bapPresentationListener"));
        RouteGuidance.State st = (RouteGuidance.State) get(rgd, "state");
        Method rend = BAPBridge.class.getDeclaredMethod("rendererLaneGuidance", new Class[]{RouteGuidance.State.class, Boolean.TYPE});
        rend.setAccessible(true);

        AaRgState aa = new AaRgState();
        aa.onTurn("Main St", 2, 4, 0, 0, 1);
        frame(rgd, aa);
        check(((Boolean) get(rgd, "rgActive")).booleanValue(), "luka guidance active");
        check(!hud.laneOn, "no lanes yet -> FctID 24 off");

        aa.onLanes(parse(record(7, 4, "1,0,4,0|1,1|5,1,1,0|8,1")));
        int sends = hud.laneSends;
        frame(rgd, aa);
        check(hud.laneSends > sends && hud.laneOn && hud.lanes != null && hud.lanes.length == 4, "FctID 24 with 4 lanes");
        if (hud.laneOn && hud.lanes != null && hud.lanes.length == 4) {
            CombiBAPNaviLaneGuidanceData[] d = hud.lanes;
            int[] dir = {0x00, 0x00, 0xC0, 0x72};
            int[] gi = {0, 2, 2, 2};
            for (int i = 0; i < 4; i++) {
                check(d[i].posID == i, "lane " + i + " position");
                check((d[i].laneDirection & 0xff) == dir[i], "lane " + i + " direction 0x" + Integer.toHexString(d[i].laneDirection));
                check(d[i].guidanceInfo == gi[i], "lane " + i + " guidanceInfo " + d[i].guidanceInfo);
            }
            check(d[0].laneSideStreets.length == 1 && (d[0].laneSideStreets[0] & 0xff) == 0x40, "lane 0 also left (side street)");
            check(d[2].laneSideStreets.length == 1 && d[2].laneSideStreets[0] == 0, "lane 2 also straight (side street)");
        }
        Object snap = rend.invoke(null, new Object[]{st, Boolean.TRUE});
        check(((Boolean) get(snap, "showing")).booleanValue() && ((Integer) get(snap, "count")).intValue() == 4
            && ((Boolean) get(snap, "complete")).booleanValue() && ((Integer) get(snap, "eventIndex")).intValue() == 1,
            "renderer lane snapshot: showing, 4 lanes, complete, event 1");
        int[] status = (int[]) get(snap, "status"), primary = (int[]) get(snap, "primary");
        check(status[0] == 0 && status[1] == 2 && status[2] == 2 && status[3] == 2, "renderer status (2 = highlighted)");
        check(primary[0] == 1000 && primary[1] == 0 && primary[2] == 90 && primary[3] == -180, "renderer primary angles");
        check(((int[][]) get(snap, "angles"))[0].length == 2, "renderer lane 0 has both angles");

        /* (no renderer here, so bap.update never reports "published" and luka keeps the lane bit dirty:
         * FctID 24 may be resent, it must stay the same) */
        aa.onDistance(300, 20, 1);
        frame(rgd, aa);
        check(hud.laneOn && hud.lanes.length == 4 && hud.lanes[1].guidanceInfo == 2, "distance frame keeps the lanes");

        aa.onTurn("Elm St", 1, 4, 0, 0, 1);
        frame(rgd, aa);
        check(hud.laneOn && hud.lanes.length == 4, "lanes survive a new maneuver");

        aa.onLanes(parse(record(9, 1, "4,1")));
        frame(rgd, aa);
        check(hud.laneOn && hud.lanes.length == 1 && (hud.lanes[0].laneDirection & 0xff) == 0x40 && hud.lanes[0].guidanceInfo == 2,
            "lane change -> 1 lane, left, recommended");

        aa.onLanes(AaLaneFeed.NONE);
        frame(rgd, aa);
        check(!hud.laneOn, "lanes gone -> FctID 24 off");
        snap = rend.invoke(null, new Object[]{st, Boolean.TRUE});
        check(((Integer) get(snap, "count")).intValue() == 0, "renderer: no lanes");

        aa.onLanes(parse(record(10, 2, "1,1|5,0")));
        frame(rgd, aa);
        check(hud.laneOn && hud.lanes.length == 2, "lanes back");
        aa.onTurn("", 0, 0, 0, 0, 2);
        frame(rgd, aa);
        check(!((Boolean) get(rgd, "rgActive")).booleanValue() && !hud.laneOn, "route end -> guidance and FctID 24 off");

        /* next route: no lanes inherited */
        aa.onTurn("Oak St", 2, 4, 0, 0, 1);
        frame(rgd, aa);
        check(((Boolean) get(rgd, "rgActive")).booleanValue() && !hud.laneOn, "new route starts without lanes");
        aa.onTurn("", 0, 0, 0, 0, 2);
        frame(rgd, aa);
        rgd.stop();
    }

    static void frame(RouteGuidance rgd, AaRgState aa) throws Exception {
        byte[] p = aa.snapshot().getBytes("UTF-8");
        rgd.onFrame(CarplayBus.EVT_RGD_UPDATE, CarplayBus.FLAG_STICKY, p, p.length);
    }

    /** Returns the number of failures. */
    public static int run() throws Exception {
        parser();
        state();
        luka();
        System.out.println("-- lanes: " + (failures == 0 ? "PASS" : "FAILED") + ": " + checks + " checks, " + failures + " failures");
        return failures;
    }

    public static void main(String[] args) throws Exception {
        AaLog.setEnabled(false);
        Log.setLevel(-1);
        System.exit(run() == 0 ? 0 : 1);
    }
}
