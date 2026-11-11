package com.luka.carplay.rgd;

import com.luka.carplay.bus.CarplayBus;
import com.luka.carplay.framework.Log;
import de.audi.app.combi.bap.app.navi.AppConnectorNavi;
import de.audi.atip.interapp.combi.bap.navi.data.CombiBAPDestinationInfo;
import de.audi.atip.interapp.combi.bap.navi.data.CombiBAPNaviLaneGuidanceData;
import de.audi.atip.interapp.combi.bap.navi.data.CombiBAPNaviManeuverDescriptor;
import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileReader;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.util.Arrays;
import sun.misc.Unsafe;

/** Lane guidance end to end, Java half: replays the bus frames the real native
 * hook published (tests/lane_guidance_e2e.c) through RouteGuidance.parse and
 * BAPBridge.update, and checks what the HUD and the cluster renderer show at
 * every expectation the native side recorded. */
public final class LaneGuidanceE2ETest {
    static final int[] ANGLES = {-90, -45, 0, 45, 90};
    /** Stock BAP boundary: capture lane guidance and current-maneuver distance. */
    public static final class HUD extends AppConnectorNavi {
        boolean showing, barOn; int writes, distance, turnMain; String turnStreet;
        HUD() { super(null); } // skipped: allocated without the native framework
        public void updateLaneGuidance(boolean on, CombiBAPNaviLaneGuidanceData[] d) { showing = on; writes++; }
        public void updateRGStatus(int v) { }
        public void updateActiveRGType(int v) { }
        public void updateDistanceToNextManeuver(int a, int b, boolean c, int d) { distance = a; barOn = c; }
        public void updateCurrentPositionInfo(String v) { turnStreet = v; }
        public void updateTurnToInfo(String a, String b) { }
        public void updateDistanceToDestination(int a, int b, boolean c) { }
        public void updateTimeToDestination(int a, int b, long c) { }
        public void updateManeuverDescriptor(CombiBAPNaviManeuverDescriptor[] d) {
            turnMain = (d != null && d.length > 0 && d[0] != null) ? d[0].mainElement : 0;
        }
        public void updateDestinationInfo(CombiBAPDestinationInfo d) { }
        public void updateExitView(int a, int b) { }
        public void updateManeuverState(int v) { }
    }
    public static final class Renderer extends RendererServer {
        LaneGuidanceSnapshot lanes; int writes, progressMode, lastIcon;
        public boolean sendVisibleArea(int x, int y, int w, int h) { return true; }
        public boolean sendProgress(int level, int mode, int state) { progressMode = mode; return true; }
        public boolean sendBapProgressManeuver(int icon, int direction, int angle, int side, int[] roads,
                int level, int mode, int perspective, boolean refresh, boolean snap, int progress) {
            lastIcon = icon; progressMode = mode; return true;
        }
        boolean sendLaneGuidance(LaneGuidanceSnapshot value) { lanes = value; writes++; return true; }
    }
    static void check(boolean b, String message) { if (!b) throw new AssertionError(message); }
    static void set(Object o, String name, Object value) throws Exception {
        Field f = o.getClass().getDeclaredField(name); f.setAccessible(true); f.set(o, value);
    }
    static byte[] read(File f) throws Exception {
        byte[] b = new byte[(int) f.length()];
        FileInputStream in = new FileInputStream(f);
        int n = 0;
        while (n < b.length) { int r = in.read(b, n, b.length - n); if (r < 0) break; n += r; }
        in.close();
        return b;
    }

    static String slotOf(RouteGuidance.State s, int event) {
        for (int i = 0; i < s.lgIndex.length; i++)
            if (s.lgIndex[i] == event) return i + "/" + s.lgLaneCount[i];
        return "none";
    }

    static String maneuverSlot(RouteGuidance.State s, int i) {
        return Arrays.deepToString(new Object[]{s.mType[i], s.mTurnAngle[i], s.mTurnAnglePresent[i],
            s.mExitAngle[i], s.mJunctionType[i], s.mDrivingSide[i], s.mDistance[i], s.mZLevel[i],
            s.mName[i], s.mAfterRoad[i], s.mExitInfo[i], s.mJunctionAngles[i], s.mVer[i]});
    }

    public static void main(String[] args) throws Exception {
        Log.setLevel(System.getenv("LANE_E2E_LOG") != null ? Log.D : -1);
        File dir = new File(args[0]);
        Field uf = Unsafe.class.getDeclaredField("theUnsafe"); uf.setAccessible(true);
        HUD hud = (HUD) ((Unsafe) uf.get(null)).allocateInstance(HUD.class);
        Renderer renderer = new Renderer();
        BAPBridge bridge = new BAPBridge();
        set(bridge, "initialized", Boolean.TRUE); set(bridge, "appConnectorNavi", hud);
        set(bridge, "bapSessionStarted", Boolean.TRUE);
        set(bridge, "routeTextPublished", Boolean.TRUE);
        set(bridge, "customRendererStarted", Boolean.TRUE); set(bridge, "rendererClient", renderer);
        RouteGuidance rg = new RouteGuidance();
        Field sf = RouteGuidance.class.getDeclaredField("state"); sf.setAccessible(true);
        RouteGuidance.State s = (RouteGuidance.State) sf.get(rg);
        Method parse = RouteGuidance.class.getDeclaredMethod("parse", CarplayBus.Data.class);
        parse.setAccessible(true);

        BufferedReader steps = new BufferedReader(new FileReader(new File(dir, "steps")));
        String line;
        int frames = 0, checks = 0, shown = 0;
        while ((line = steps.readLine()) != null) {
            String[] p = line.split(" ");
            if (p[0].equals("F")) {
                byte[] text = read(new File(dir, p[1] + ".txt"));
                parse.invoke(rg, CarplayBus.parseText(text, text.length));
                // Every native snapshot must be sufficient on its own after reconnect.
                // Compare listed slots only: unlisted prefetch is intentionally not published.
                RouteGuidance fresh = new RouteGuidance();
                parse.invoke(fresh, CarplayBus.parseText(text, text.length));
                RouteGuidance.State freshState = (RouteGuidance.State) sf.get(fresh);
                check(s.distManeuverM == freshState.distManeuverM,
                    "frame " + frames + ": live/reconnect distance differs");
                if (s.maneuverOrder != null) for (int slot : s.maneuverOrder)
                    check(maneuverSlot(s, slot).equals(maneuverSlot(freshState, slot)),
                        "frame " + frames + " slot " + slot + ": live/reconnect snapshot differs: "
                        + maneuverSlot(s, slot) + " vs " + maneuverSlot(freshState, slot));
                if (bridge.update(s)) s.clearDirty(); // as RouteGuidance does after a publish
                if (s.routeState > 0 && s.maneuverOrder != null) {
                    parse.invoke(rg, CarplayBus.parseText(text, text.length));
                    check(s.dirtyMask == 0, "frame " + frames + ": identical snapshot dirtied state");
                }
                frames++;
                continue;
            }
            if (p[0].equals("D")) {
                int meters = Integer.parseInt(p[1]); boolean visible = p[2].equals("1");
                String at = "distance after frame " + (frames - 1);
                check(s.distManeuverM == meters, at + ": expected " + meters + ", got " + s.distManeuverM);
                check((hud.distance > 0) == visible && hud.barOn == visible, at + ": HUD distance/progress");
                check((renderer.progressMode != 0) == visible, at + ": renderer progress");
                checks++;
                continue;
            }
            if (p[0].equals("M")) {
                boolean expectTurn = p[1].equals("1");
                String road = p[2];
                String at = "maneuver after frame " + (frames - 1) + " (icon=" + renderer.lastIcon
                    + " hudMain=" + hud.turnMain + " street=" + hud.turnStreet + ")";
                if (expectTurn) {
                    check(renderer.lastIcon == RendererMapper.ICON_TURN, at + ": expected ICON_TURN");
                    check(hud.turnMain == ManeuverMapper.TURN, at + ": expected HUD TURN");
                    check(hud.turnStreet != null && hud.turnStreet.indexOf(road) >= 0,
                        at + ": expected street containing " + road);
                }
                checks++;
                continue;
            }
            int event = Integer.parseInt(p[1]), seed = Integer.parseInt(p[2]);
            int live = 0;
            for (int i = 0; i < s.lgIndex.length; i++) if (s.lgIndex[i] >= 0) live++;
            // Passed events are dropped, so a snapshot carries about one window
            // (6), twice that only while a renumbered reroute overlaps the old one.
            check(live <= 12, "check " + checks + ": " + live + " lane events ride in every snapshot");
            String at = "check " + checks + " after frame " + (frames - 1) + " (event " + event + "; state showing="
                + s.laneGuidanceShowing + " active=" + s.laneGuidanceIndex + " route=" + s.routeState
                + " maneuvers=" + s.maneuverCount + " slot=" + slotOf(s, s.laneGuidanceIndex)
                + " renderer writes=" + renderer.writes + " hud writes=" + hud.writes + ")";
            LaneGuidanceSnapshot lanes = renderer.lanes;
            boolean visible = lanes != null && lanes.showing && lanes.count > 0;
            if (event < 0) {
                check(!visible, at + ": renderer shows lanes of event " + (lanes == null ? -1 : lanes.eventIndex));
                check(!hud.showing, at + ": HUD shows lanes");
            } else {
                int count = 2 + seed % 5;
                check(visible, at + ": no lanes on the renderer");
                check(hud.showing, at + ": no lanes on the HUD");
                check(lanes.eventIndex == event, at + ": renderer event " + lanes.eventIndex);
                check(lanes.count == count, at + ": " + lanes.count + " lanes, want " + count);
                for (int i = 0; i < count; i++) {
                    check(lanes.status[i] == (seed + i) % 3, at + ": lane " + i + " status from another event");
                    check(lanes.primary[i] == ANGLES[(seed + i) % 5], at + ": lane " + i + " direction from another event");
                }
                shown++;
            }
            checks++;
        }
        steps.close();
        bridge.onStop();
        check(shown >= 92 && checks >= 185, "scenario too short: " + checks + " checks");
        System.out.println("Lane guidance end to end: native hook -> bus -> Java -> HUD/renderer, " + frames
            + " frames, " + checks + " checks (" + shown + " junctions with lanes, events pre-sent below the maneuver indices, "
            + "reroute renumbering without a reset, stale same-number events replaced, lane data without an index, new route) PASS");
    }
}
