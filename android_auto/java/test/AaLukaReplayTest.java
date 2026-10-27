import com.luka.carplay.bus.CarplayBus;
import com.luka.carplay.core.ScreenModule;
import com.luka.carplay.framework.Log;
import com.luka.carplay.rgd.BAPBridge;
import com.luka.carplay.rgd.ManeuverMapper;
import com.luka.carplay.rgd.RendererMapper;
import com.luka.carplay.rgd.RouteGuidance;
import com.sq5.aa.luka.AaLog;
import com.sq5.aa.luka.AaManeuverMap;
import com.sq5.aa.luka.AaRgState;
import com.sq5.aa.luka.MirrorGate;
import de.audi.atip.interapp.combi.bap.navi.CombiBAPServiceNavi;
import de.audi.atip.interapp.combi.bap.navi.data.CombiBAPNaviManeuverDescriptor;

import java.io.BufferedReader;
import java.io.FileInputStream;
import java.io.InputStreamReader;
import java.lang.reflect.Field;
import java.lang.reflect.InvocationHandler;
import java.lang.reflect.Method;
import java.lang.reflect.Proxy;
import java.util.ArrayList;
import java.util.List;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * Host replay: a real Android Auto drive log (aa-cluster v2.x /tmp/sq5_aa.log) -> AaRgState ->
 * CarplayBus.injectLocal -> luka RouteGuidance.onFrame -> real BAPBridge (onStart/update/onRouteEnd)
 * -> CombiBAPServiceNavi.  Only the BAP service (a recording proxy) and the native-nav stop
 * (no Navigation singleton on a PC) are stubbed, as in luka's tests/RgiDeliveryRecoveryTest.
 *
 *   java -Xverify:none -cp build/test;build/classes;<lsd_ic.jar> AaLukaReplayTest <sq5_aa.log>
 */
public final class AaLukaReplayTest {
    static int failures = 0;
    static int checks = 0;

    static void check(boolean ok, String what) {
        checks++;
        if (!ok) {
            failures++;
            System.out.println("FAIL " + what);
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

    /** Recording CombiBAPServiceNavi: the stock AppConnectorNavi interface luka's BAPBridge writes. */
    static final class Hud implements InvocationHandler {
        final List calls = new ArrayList();
        int rgStatus = -1;
        int main = -1, dir = -1;
        int descriptorSends;
        int distValue = Integer.MIN_VALUE, distUnit, bar;
        boolean barOn;
        int distSends, distValidSends;
        int maneuverState = -1;
        String position = "";
        final Thread owner = Thread.currentThread();
        int asyncDistSends;

        public synchronized Object invoke(Object proxy, Method m, Object[] a) {
            String n = m.getName();
            if (n.equals("updateRGStatus")) { rgStatus = ((Integer) a[0]).intValue(); calls.add("RGStatus(" + rgStatus + ")"); }
            else if (n.equals("updateManeuverDescriptor")) {
                CombiBAPNaviManeuverDescriptor d = ((CombiBAPNaviManeuverDescriptor[]) a[0])[0];
                main = d.mainElement; dir = d.direction; descriptorSends++;
            } else if (n.equals("updateDistanceToNextManeuver") && Thread.currentThread() != owner) {
                asyncDistSends++;       /* BAPBridge action-blink worker (600 ms), not the frame path */
            } else if (n.equals("updateDistanceToNextManeuver")) {
                distValue = ((Integer) a[0]).intValue(); distUnit = ((Integer) a[1]).intValue();
                barOn = ((Boolean) a[2]).booleanValue(); bar = ((Integer) a[3]).intValue();
                distSends++;
                if (distValue > 0) distValidSends++;
            } else if (n.equals("updateManeuverState")) maneuverState = ((Integer) a[0]).intValue();
            else if (n.equals("updateCurrentPositionInfo")) position = (String) a[0];
            if (m.getReturnType() == Boolean.TYPE) return Boolean.TRUE;
            if (m.getReturnType() == Integer.TYPE) return new Integer(0);
            return null;
        }
    }

    static final Pattern TURN = Pattern.compile("^(\\d+) turn road='(.*)' side=(-?\\d+) event=(-?\\d+) angle=(-?\\d+) number=(-?\\d+) valid=(-?\\d+)$");
    static final Pattern DIST = Pattern.compile("^(\\d+) distance m=(-?\\d+) s=(-?\\d+) valid=(-?\\d+)$");
    static final Pattern FOCUS = Pattern.compile("^(\\d+) navFocus focus=(-?\\d+) valid=(-?\\d+)$");

    /* expected BAP (mainElement, direction) for an Android Auto hard turn, independent of the adapter */
    static int[] expectedBap(int event, int side) {
        boolean l = side == 1, r = side == 2;
        switch (event) {
            case 3: return new int[]{13, l ? 32 : r ? 224 : 0};
            case 4: return new int[]{13, l ? 64 : r ? 192 : 0};
            case 5: return new int[]{13, l ? 96 : r ? 160 : 0};
            case 6: return new int[]{25, r ? 192 : 64};
            case 7: case 8: return new int[]{13, l ? 32 : 224};
            case 9: return new int[]{13, l ? 32 : r ? 224 : 0};
            case 14: return new int[]{13, 0};
            case 19: return new int[]{3, l ? 64 : r ? 192 : 0};
            default: return null;
        }
    }

    static int bargraphDen(int stepM) {
        int cap = (stepM > 2000 ? 3000 : 1500) * 15 / 100;
        return (stepM <= 0 || stepM > cap) ? cap : stepM;
    }

    /* ------------------------------------------------------------------ mapping table */

    static void mappingTable() {
        System.out.println("-- AA event -> EManeuverType -> luka BAP (main,dir) / renderer icon");
        int[][] rows = {
            {1, 0, 0, 0}, {2, 0, 0, 0}, {3, 1, 0, 0}, {3, 2, 0, 0}, {4, 1, 0, 0}, {4, 2, 0, 0}, {4, 3, 0, 0},
            {5, 1, 0, 0}, {5, 2, 0, 0}, {6, 1, 0, 0}, {6, 2, 0, 0}, {6, 3, 0, 0}, {7, 1, 0, 0}, {7, 2, 0, 0},
            {8, 1, 0, 0}, {8, 2, 0, 0}, {8, 3, 0, 0}, {9, 1, 0, 0}, {9, 2, 0, 0}, {10, 1, 0, 0}, {10, 2, 0, 0},
            {11, 2, 90, 0}, {12, 2, 90, 0}, {13, 2, 90, 1}, {13, 2, 180, 2}, {13, 2, 270, 3}, {13, 2, 360, 4},
            {13, 1, 90, 1}, {13, 1, 270, 3}, {13, 2, 0, 0}, {14, 3, 0, 0}, {16, 0, 0, 0}, {19, 0, 0, 0},
            {19, 1, 0, 0}, {0, 0, 0, 0}};
        for (int i = 0; i < rows.length; i++) {
            int[] r = rows[i];
            AaManeuverMap.Result m = AaManeuverMap.map(r[0], r[1], r[2], r[3]);
            int[] bap = ManeuverMapper.map(m.type, m.anglePresent ? m.angle : 1000, m.junctionType, m.drivingSide,
                m.anglePresent);
            RendererMapper.Mapping rm = RendererMapper.map(bap[0], bap[1], m.drivingSide, m.type,
                m.anglePresent ? m.angle : 1000, m.anglePresent, null);
            System.out.println("  event=" + r[0] + " side=" + r[1] + " angle=" + r[2] + " n=" + r[3] + " -> " + m
                + " -> BAP(" + bap[0] + "," + bap[1] + ") renderer icon=" + rm.icon + " exit=" + rm.exitAngle);
            check(ManeuverMapper.isValidType(m.type), "valid EManeuverType for event " + r[0]);
            check(bap[0] != ManeuverMapper.NO_INFO && bap[0] != ManeuverMapper.NO_SYMBOL, "BAP icon for event " + r[0]);
            int[] exp = expectedBap(r[0], r[1]);
            if (exp != null) check(bap[0] == exp[0] && bap[1] == exp[1],
                "event " + r[0] + " side " + r[1] + " expected BAP(" + exp[0] + "," + exp[1] + ") got (" + bap[0] + "," + bap[1] + ")");
        }
        /* roundabouts, right-hand traffic: 90 = right exit, 180 = straight, 270 = left (DIR16 table) */
        check(bapOf(13, 2, 90, 1)[1] == ManeuverMapper.DIR_RIGHT, "RHT roundabout 90 -> right");
        check(bapOf(13, 2, 180, 2)[1] == ManeuverMapper.DIR_STRAIGHT, "RHT roundabout 180 -> straight");
        check(bapOf(13, 2, 270, 3)[1] == ManeuverMapper.DIR_LEFT, "RHT roundabout 270 -> left");
        check(bapOf(13, 2, 90, 1)[0] == ManeuverMapper.ROUNDABOUT_TRS_RIGHT, "RHT roundabout icon");
        check(bapOf(13, 1, 90, 1)[0] == ManeuverMapper.ROUNDABOUT_TRS_LEFT && bapOf(13, 1, 90, 1)[1] == ManeuverMapper.DIR_LEFT,
            "LHT roundabout 90 -> left, clockwise icon");
        check(bapOf(12, 2, 0, 0)[0] == ManeuverMapper.EXIT_ROUNDABOUT_TRS_RIGHT, "roundabout exit icon");
        check(bapOf(1, 0, 0, 0)[0] == ManeuverMapper.TURN, "depart -> START_ROUTE (TURN straight)");
    }

    static int[] bapOf(int event, int side, int angle, int n) {
        AaManeuverMap.Result m = AaManeuverMap.map(event, side, angle, n);
        return ManeuverMapper.map(m.type, m.anglePresent ? m.angle : 1000, m.junctionType, m.drivingSide, m.anglePresent);
    }

    /* ------------------------------------------------------------------ state machine unit checks */

    static void stateMachine() {
        AaRgState s = new AaRgState();
        check(!s.onTurn("toward X", 0, 1, 0, 0, 1), "depart before route is held");
        check(!s.onDistance(0, 0, 1), "depart 0 m before route ignored");
        check(!s.isRouteActive(), "no route after depart 0 m");
        check(s.onDistance(300, 30, 1) && s.isRouteActive(), "depart with distance > 0 starts guidance");
        check(s.snapshot().indexOf("m0_type:n:11\n") >= 0, "depart shown as START_ROUTE");
        int v = s.maneuverVersion();
        check(s.onTurn("Main St", 2, 4, 0, 0, 1) && s.maneuverVersion() == v + 1, "hard turn bumps version");
        check(!s.onTurn("Main St", 2, 4, 0, 0, 1), "repeated identical turn is not a new maneuver");
        check(!s.onTurn("toward Y", 0, 1, 0, 0, 1) && s.snapshot().indexOf("m0_type:n:2\n") >= 0,
            "mid-route depart held, previous turn stays");
        check(!s.onDistance(0, 0, 1), "held depart 0 m ignored");
        check(s.onTurn("Elm St", 1, 4, 0, 0, 1) && s.snapshot().indexOf("m0_type:n:1\n") >= 0, "held depart replaced by turn");
        check(s.onDistance(512, 60, 1) && s.snapshot().indexOf("m0_distance:n:512\n") >= 0, "first distance = step");
        check(s.onDistance(400, 50, 1) && s.snapshot().indexOf("m0_distance:n:512\n") >= 0
            && s.snapshot().indexOf("dist_maneuver_m:n:400\n") >= 0, "step kept, distance updated");
        long g = s.routeGeneration();
        check(s.onTurn("", 0, 0, 0, 0, 2) && !s.isRouteActive(), "valid=2 ends route");
        String end = s.snapshot();
        check(end.indexOf("route_state:n:0\n") >= 0 && end.indexOf("maneuver_list:s:\n") >= 0
            && end.indexOf("visible_in_app:n:0\n") >= 0 && end.indexOf("m0_") < 0, "clean route-end frame");
        check(s.onTurn("Oak St", 2, 4, 0, 0, 1) && s.routeGeneration() == g + 1, "new route -> new generation");
        check(s.onNavFocus(1, 1) && !s.isRouteActive(), "native nav focus ends route");
        check(s.onTurn("Oak St", 2, 4, 0, 0, 1), "restart");
        s.resetSession();
        check(!s.isRouteActive() && s.snapshot().indexOf("route_state:n:0\n") >= 0, "session reset ends route");
        check(s.onTurn("A\nB", 2, 4, 0, 0, 1) && s.snapshot().indexOf("m0_after_road:s:A B\n") >= 0, "newline sanitized");
    }

    /* ------------------------------------------------------------------ cockpit context rule + mirror marker */

    static void contextRule() {
        /* ScreenModule.contextFor(session, navActive, mirrorReady) */
        int[][] t = {
            {0, 0, 0, 74}, {0, 1, 0, 74}, {0, 0, 1, 74}, {0, 1, 1, 74},   /* no phone session: stock always */
            {1, 0, 0, 74}, {1, 1, 0, 80},                                 /* luka: arrow over native map */
            {1, 0, 1, 82}, {1, 1, 1, 81}};                                /* mirror alone / arrow over mirror */
        for (int i = 0; i < t.length; i++) {
            int got = ScreenModule.contextFor(t[i][0] != 0, t[i][1] != 0, t[i][2] != 0);
            check(got == t[i][3], "contextFor(session=" + t[i][0] + ",nav=" + t[i][1] + ",mirror=" + t[i][2]
                + ") = " + got + ", expected " + t[i][3]);
        }
        MirrorGate g = new MirrorGate();
        long now = 1000000L;
        check(!g.observe(null, now), "no marker -> not ready");
        check(g.observe("1 4242 1440x455", now), "first marker content -> ready");
        int polls = 0;
        boolean ready = true;
        for (long ms = 250; ms <= 2750; ms += 250) { polls++; ready = g.observe("1 4242 1440x455", now + ms); }
        check(ready, "unchanged marker still fresh before 3 s (" + polls + " polls)");
        check(g.observe("2 4242 1440x455", now + 3000), "heartbeat keeps it fresh");
        ready = true;
        for (long ms = 3250; ms <= 6250 && ready; ms += 250) ready = g.observe("2 4242 1440x455", now + ms);
        check(!ready, "marker unchanged for >= 3 s -> stale (fallback)");
        check(g.observe("3 4242 1440x455", now + 6500), "heartbeat resumes -> fresh again");
        ready = true;
        for (int i = 0; i < 20 && ready; i++) ready = g.observe("3 4242 1440x455", now + 6500 + 2999);
        check(ready, "many polls inside 3 s (worker wake-ups) do not make it stale");
        check(g.observe("3 4242 1440x455", now + 6500 + 3600 * 1000L) == false,
            "forward clock jump + enough polls -> stale (then next heartbeat recovers)");
        check(g.observe("4 4242 1440x455", now + 6500 + 3600 * 1000L + 1000), "recovers on next heartbeat");
        check(!g.observe("", now), "deleted/empty marker -> not ready at once");
    }

    /* ------------------------------------------------------------------ drive replay */

    public static void main(String[] args) throws Exception {
        AaLog.setEnabled(false);
        Log.setLevel(-1);
        mappingTable();
        stateMachine();
        contextRule();

        RouteGuidance rgd = new RouteGuidance();
        BAPBridge bridge = new BAPBridge();
        Hud hud = new Hud();
        set(bridge, "initialized", Boolean.TRUE);
        set(bridge, "appConnectorNavi", Proxy.newProxyInstance(AaLukaReplayTest.class.getClassLoader(),
            new Class[]{CombiBAPServiceNavi.class}, hud));
        /* No de.audi.tghu.navi.app.Navigation on a PC: treat the native route as already absent. */
        set(bridge, "nativeStopAttempted", Boolean.TRUE);
        set(rgd, "bap", bridge);
        set(rgd, "running", Boolean.TRUE);
        bridge.setPresentationListener((BAPBridge.PresentationListener) get(rgd, "bapPresentationListener"));
        RouteGuidance.State st = (RouteGuidance.State) get(rgd, "state");
        CarplayBus bus = CarplayBus.getInstance();
        final RouteGuidance target = rgd;
        final int[] errors = new int[1];
        CarplayBus.Listener checking = new CarplayBus.Listener() {
            public void onFrame(int type, int flags, byte[] payload, int len) {
                try { target.onFrame(type, flags, payload, len); }
                catch (Throwable t) { errors[0]++; System.out.println("onFrame threw " + t); }
            }
        };
        bus.on(CarplayBus.EVT_RGD_UPDATE, checking);

        AaRgState aa = new AaRgState();
        BufferedReader in = new BufferedReader(new InputStreamReader(new FileInputStream(args[0]), "UTF-8"));
        String line;
        int nTurn = 0, nDist = 0, nFocus = 0, frames = 0, activations = 0, deactivations = 0;
        int hardTurnsInLog = 0, listenerErrors = 0;
        int hardTurnsActive = 0, descriptorOk = 0, distChecked = 0, distOk = 0, blinkZone = 0, ignoredDepart0 = 0;
        boolean wasActive = false;
        int curEvent = -1, curSide = 0, stepM = -1, stepVer = -1;
        int maxDescriptorLag = 0;
        while ((line = in.readLine()) != null) {
            line = line.trim();
            Matcher t = TURN.matcher(line), d = DIST.matcher(line), f = FOCUS.matcher(line);
            boolean changed;
            int hudDistBefore = hud.distSends, hudDescBefore = hud.descriptorSends;
            int ev = -1, side = 0, meters = -1;
            if (t.matches()) {
                nTurn++;
                ev = Integer.parseInt(t.group(4));
                side = Integer.parseInt(t.group(3));
                int valid = Integer.parseInt(t.group(7));
                String road = t.group(2);
                if ("null".equals(road)) road = null;
                boolean activeBefore = aa.isRouteActive();
                changed = aa.onTurn(road, side, ev, Integer.parseInt(t.group(5)), Integer.parseInt(t.group(6)), valid);
                if (!activeBefore && valid == 1 && (ev == 1 || ev == 2 || ev == 0) && !changed) ignoredDepart0++;
                if (valid == 1 && expectedBap(ev, side) != null) hardTurnsInLog++;
            } else if (d.matches()) {
                nDist++;
                meters = Integer.parseInt(d.group(2));
                changed = aa.onDistance(meters, Integer.parseInt(d.group(3)), Integer.parseInt(d.group(4)));
            } else if (f.matches()) {
                nFocus++;
                changed = aa.onNavFocus(Integer.parseInt(f.group(2)), Integer.parseInt(f.group(3)));
            } else {
                continue;
            }
            if (!changed) continue;
            frames++;
            byte[] p = aa.snapshot().getBytes("UTF-8");
            check(bus.injectLocal(CarplayBus.EVT_RGD_UPDATE, CarplayBus.FLAG_STICKY, p), "frame reached RouteGuidance");

            boolean rgActive = ((Boolean) get(rgd, "rgActive")).booleanValue();
            if (rgActive && !wasActive) {
                activations++;
                check(hud.rgStatus == 1, "RGStatus(1) on activation (" + line + ")");
                check(ScreenModule.isNavActive(), "ScreenModule nav active (ctx 80 intent) on activation");
                System.out.println("  ACTIVATE  @" + line);
            }
            if (!rgActive && wasActive) {
                deactivations++;
                check(hud.rgStatus == 0, "RGStatus(0) on route end (" + line + ")");
                check(!ScreenModule.isNavActive(), "ScreenModule nav inactive (ctx 74) after route end");
                check(hud.main == 0, "NO_SYMBOL descriptor after route end");
                System.out.println("  DEACTIVATE@" + line);
            }
            check(rgActive == aa.isRouteActive(), "luka guidance state follows adapter (" + line + ")");
            wasActive = rgActive;

            if (t.matches() && rgActive) {
                int[] exp = expectedBap(ev, side);
                AaManeuverMap.Result cur = aa.current();
                if (exp != null && cur != null && !cur.soft) {
                    hardTurnsActive++;
                    boolean ok = hud.descriptorSends > hudDescBefore && hud.main == exp[0] && hud.dir == exp[1];
                    check(ok, "descriptor for " + line + " expected (" + exp[0] + "," + exp[1] + ") got ("
                        + hud.main + "," + hud.dir + ") sends=" + (hud.descriptorSends - hudDescBefore));
                    if (ok) descriptorOk++;
                    check(st.mVer[0] == aa.maneuverVersion() && st.mType[0] == cur.type, "slot 0 carries new maneuver version/type");
                    curEvent = ev; curSide = side; stepM = -1;
                }
            }
            if (d.matches() && rgActive && meters > 0 && curEvent >= 0) {
                if (stepVer != aa.maneuverVersion()) { stepVer = aa.maneuverVersion(); stepM = -1; }
                if (stepM <= 0) stepM = meters;
                check(st.distManeuverM == meters, "RouteGuidance distance == Android Auto distance (" + line + ")");
                int den = bargraphDen(stepM);
                boolean inBar = meters <= den;
                boolean blink = inBar && meters * 100 / den < 20;
                if (blink) {
                    blinkZone++;          /* FctID 18 comes from BAPBridge's 600 ms blink worker */
                } else {
                    distChecked++;
                    /* SQ5: BAP bargraph (hides the VC distance number) only within SQ5_BAP_BAR_MAX_M */
                    boolean bapBar = inBar && meters <= 50;
                    boolean ok = hud.distSends > hudDistBefore && hud.distValue == meters * 10 && hud.barOn == bapBar
                        && (!bapBar || hud.bar == meters * 100 / den);
                    check(ok, "FctID 18 for " + line + ": sent=" + (hud.distSends - hudDistBefore) + " value="
                        + hud.distValue + " unit=" + hud.distUnit + " barOn=" + hud.barOn + " bar=" + hud.bar
                        + " (den " + den + ")");
                    if (ok) distOk++;
                }
            }
        }
        in.close();
        listenerErrors = errors[0];

        /* phone disconnect while the last route is still active: adapter session end */
        boolean activeAtEnd = aa.isRouteActive();
        aa.resetSession();
        byte[] p = aa.snapshot().getBytes("UTF-8");
        bus.injectLocal(CarplayBus.EVT_RGD_UPDATE, CarplayBus.FLAG_STICKY, p);
        boolean rgActive = ((Boolean) get(rgd, "rgActive")).booleanValue();
        if (activeAtEnd) {
            check(!rgActive && hud.rgStatus == 0 && !ScreenModule.isNavActive(), "session end deactivates guidance");
            if (!rgActive) deactivations++;
        }
        /* sticky replay: a RouteGuidance registering later receives the latest frame */
        bus.off(CarplayBus.EVT_RGD_UPDATE);
        AaRgState late = new AaRgState();
        late.onTurn("Late St", 2, 4, 0, 0, 1);
        check(!bus.injectLocal(CarplayBus.EVT_RGD_UPDATE, CarplayBus.FLAG_STICKY, late.snapshot().getBytes("UTF-8")),
            "no listener -> frame held");
        bus.on(CarplayBus.EVT_RGD_UPDATE, rgd);
        for (int i = 0; i < 50 && !((Boolean) get(rgd, "rgActive")).booleanValue(); i++) Thread.sleep(20);
        check(((Boolean) get(rgd, "rgActive")).booleanValue() && hud.main == 13 && hud.dir == 192,
            "late RouteGuidance got the held sticky frame");

        System.out.println("-- replay " + args[0]);
        System.out.println("  AA events: turns=" + nTurn + " distances=" + nDist + " navFocus=" + nFocus
            + " -> frames to RouteGuidance=" + frames);
        System.out.println("  guidance activations=" + activations + " deactivations=" + deactivations
            + " (depart/0 m ignored before route: " + ignoredDepart0 + ")");
        System.out.println("  hard turns while active=" + hardTurnsActive + " descriptor correct=" + descriptorOk
            + " total descriptor sends=" + hud.descriptorSends);
        System.out.println("  distance frames checked=" + distChecked + " FctID18 correct=" + distOk
            + " blink-zone (async)=" + blinkZone + " FctID18 sends=" + hud.distSends + " valid=" + hud.distValidSends
            + " blink-worker sends=" + hud.asyncDistSends);
        System.out.println("  RGStatus calls: " + hud.calls);
        check(activations == 3, "3 guidance activations expected, got " + activations);
        check(deactivations == 3, "3 deactivations expected (2x valid=2 + session end), got " + deactivations);
        check(hardTurnsActive == hardTurnsInLog, "every hard turn in the log replayed while active: "
            + hardTurnsActive + " of " + hardTurnsInLog);
        check(hud.descriptorSends < frames / 2, "descriptor resent on distance-only frames: " + hud.descriptorSends);
        check(listenerErrors == 0, "RouteGuidance.onFrame threw " + listenerErrors + " times");
        check(distOk > 500, "more than 500 distances reached BAP, got " + distOk);
        rgd.stop();
        /* Android Auto lanes (gal hook record -> AaRgState -> RouteGuidance -> FctID 24 / renderer) */
        int laneFailures = AaLaneGuidanceTest.run();
        checks += AaLaneGuidanceTest.checks;
        failures += laneFailures;
        System.out.println((failures == 0 ? "PASS" : "FAILED") + ": " + checks + " checks, " + failures + " failures");
        System.exit(failures == 0 ? 0 : 1);
    }
}
