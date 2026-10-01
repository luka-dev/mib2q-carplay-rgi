/*
 * Android Auto next-turn stream -> luka RouteGuidance frame (EVT_RGD_UPDATE text payload).
 *
 * Pure Java 1.4, no Audi classes, host-testable.  Every call that returns true changed the
 * route picture; snapshot() then yields a FULL frame (not a delta) in the "key:type:value"
 * format CarplayBus.parseText reads.  Full frames are idempotent for RouteGuidance.parse()
 * (unchanged values are not dirty), so a coalescing/sticky transport may drop any frame
 * except the latest without losing state.
 *
 * Route model (what luka's C hook would produce for an iPhone):
 *  - one maneuver slot (maneuver_list "0", maneuver_count 1); every new Android Auto turn
 *    bumps m0_ver, which makes RouteGuidance clear the slot and BAPBridge treat it as a
 *    new primary maneuver (approach/bargraph reset, renderer push animation);
 *  - route_generation increments at every guidance start, so a new route never inherits
 *    the previous route's slot contents;
 *  - active:   route_state 1, visible_in_app 1, source_supports_rg 1
 *  - inactive: route_state 0, visible_in_app 0, maneuver_count 0, maneuver_list "" (explicit
 *    empty) -> RouteGuidance wantActive=false -> bap.onStop()+onRouteEnd(), ctx 74.
 *  - m0_distance (step length, the bargraph denominator / highway detector) = the first
 *    positive distance Android Auto reports for that maneuver.
 *  - lanes (onLanes, from the gal hook's /tmp/sq5_aa_lanes, AaLaneFeed): one lane event in lg
 *    cache slot 0, as luka's C hook publishes an iOS 0x5204 event: lane_guidance_showing 1,
 *    lane_guidance_index/lg0_index = lane event id (bumped on every lane change),
 *    lane_guidance_slot 0, lg0_lane_count/positions (0..n-1 in Android Auto order)/directions/
 *    status/angles/complete.  Per lane: angles = every known shape (AaLaneFeed.angle), direction =
 *    the highlighted shape's angle and status 2 (PREFERRED), else direction 1000 (unknown, drawn
 *    grey from its angles) and status 0.  No lanes / no route: showing 0 + the C hook's lg0 clear.
 *    Lanes live only while a route is active and survive maneuver changes (independent event,
 *    like iOS); the route end clears them.
 *  - not sent (Android Auto MU0918 DSI has no such data): current_road, destination,
 *    eta_seconds, time_remaining_seconds, dist_dest_m.
 *
 * Android Auto quirks handled (drive log 21_19700101_002420):
 *  - Google Maps sends "depart, 0 m" at connect / free drive with no route: ignored.
 *  - mid-route it sends a transient "depart toward X, 0 m" ~0.5 s before the next real turn:
 *    DEPART / NAME_CHANGE / UNKNOWN are held ("soft") and only shown once a distance > 0
 *    arrives for them; a hard turn replaces a held one.
 *  - end of guidance = turn event with valid == 2 (or nav focus returning to native).
 */
package com.sq5.aa.luka;

public final class AaRgState {
    public static final int VALID = 1;
    public static final int NO_GUIDANCE = 2;
    public static final int NAVFOCUS_NATIVE = 1;

    private boolean routeActive;
    private long routeGeneration;      /* 0 = none yet */
    private int ver;
    /* current (published) maneuver */
    private AaManeuverMap.Result cur;
    private String curRoad = "";
    private String curKey;
    private int distM = -1;
    private int stepM = -1;
    /* held soft maneuver (depart / name change) waiting for a positive distance */
    private AaManeuverMap.Result pending;
    private String pendingRoad;
    private String pendingKey;
    /* lanes of the current step ("" = none) and the lane event id */
    private AaLaneFeed.Lanes lanes = AaLaneFeed.NONE;
    private int laneEvent;
    /* counters for the log / tests */
    private int nStarts, nEnds, nManeuvers;
    private String lastAction = "";

    public synchronized boolean isRouteActive() { return routeActive; }
    public synchronized long routeGeneration() { return routeGeneration; }
    public synchronized int maneuverVersion() { return ver; }
    public synchronized int distance() { return distM; }
    public synchronized int starts() { return nStarts; }
    public synchronized int ends() { return nEnds; }
    public synchronized int maneuvers() { return nManeuvers; }
    public synchronized String lastAction() { return lastAction; }
    public synchronized AaManeuverMap.Result current() { return cur; }
    public synchronized String lanes() { return lanes.key; }
    public synchronized int laneEvent() { return laneEvent; }

    /**
     * Lanes of the current step (null = none).  Ignored without an active route.  Returns true when
     * the lane picture changed.  Does not touch lastAction (the turn/distance log reads it).
     */
    public synchronized boolean onLanes(AaLaneFeed.Lanes l) {
        AaLaneFeed.Lanes n = l == null || !routeActive ? AaLaneFeed.NONE : l;
        if (n.key.equals(lanes.key)) return false;
        lanes = n;
        if (n.count > 0) laneEvent++;
        return true;
    }

    private static String key(int event, int side, int angle, int number, String road) {
        return event + "|" + side + "|" + angle + "|" + number + "|" + road;
    }

    public synchronized boolean onTurn(String road, int turnSide, int event, int turnAngle,
                                       int turnNumber, int valid) {
        if (valid == NO_GUIDANCE) return endRoute("Android Auto reports no active guidance (valid=2)");
        if (valid != VALID) { lastAction = "ignored turn valid=" + valid; return false; }
        String rd = road == null ? "" : road;
        String k = key(event, turnSide, turnAngle, turnNumber, rd);
        if (k.equals(curKey) && pending == null) { lastAction = "same maneuver repeated"; return false; }
        if (k.equals(pendingKey)) { lastAction = "same held maneuver repeated"; return false; }
        AaManeuverMap.Result m = AaManeuverMap.map(event, turnSide, turnAngle, turnNumber);
        if (m.soft) {
            pending = m;
            pendingRoad = rd;
            pendingKey = k;
            lastAction = routeActive ? "held soft maneuver (" + m + ") until distance > 0"
                                     : "no route yet (soft maneuver, wait for distance > 0)";
            return false;
        }
        pending = null;
        pendingRoad = null;
        pendingKey = null;
        return show(m, rd, k);
    }

    public synchronized boolean onDistance(int meters, int seconds, int valid) {
        if (valid != VALID) { lastAction = "ignored distance valid=" + valid; return false; }
        if (pending != null) {
            if (meters <= 0) { lastAction = "held soft maneuver, distance " + meters + " ignored"; return false; }
            AaManeuverMap.Result m = pending;
            String rd = pendingRoad, k = pendingKey;
            pending = null;
            pendingRoad = null;
            pendingKey = null;
            show(m, rd, k);
        } else if (!routeActive) {
            lastAction = "no route: distance " + meters + " ignored";
            return false;
        }
        if (meters == distM) { lastAction = "distance unchanged"; return false; }
        distM = meters;
        if (stepM <= 0 && meters > 0) stepM = meters;
        lastAction = "distance " + meters + " m";
        return true;
    }

    public synchronized boolean onNavFocus(int focus, int valid) {
        if (valid == VALID && focus == NAVFOCUS_NATIVE) return endRoute("native navigation took focus");
        lastAction = "navFocus " + focus;
        return false;
    }

    /** Guidance ended (valid=2, native focus, watchdog, session end). */
    public synchronized boolean endRoute(String why) {
        pending = null;
        pendingRoad = null;
        pendingKey = null;
        if (!routeActive) { lastAction = "no route to end (" + why + ")"; return false; }
        routeActive = false;
        cur = null;
        curRoad = "";
        curKey = null;
        distM = -1;
        stepM = -1;
        lanes = AaLaneFeed.NONE;
        nEnds++;
        lastAction = "route end: " + why;
        return true;
    }

    /** Phone session start/end: forget everything except the monotonic counters. */
    public synchronized void resetSession() {
        endRoute("session reset");
        lastAction = "session reset";
    }

    private boolean show(AaManeuverMap.Result m, String road, String k) {
        if (!routeActive) {
            routeActive = true;
            routeGeneration++;
            nStarts++;
        }
        ver++;
        nManeuvers++;
        cur = m;
        curRoad = road;
        curKey = k;
        distM = -1;
        stepM = -1;
        lastAction = "maneuver v" + ver + " gen " + routeGeneration + ": " + m + " road='" + road + "'";
        return true;
    }

    private static String clean(String s) {
        if (s == null) return "";
        StringBuffer b = new StringBuffer(s.length());
        for (int i = 0; i < s.length(); i++) {
            char c = s.charAt(i);
            b.append(c == '\n' || c == '\r' ? ' ' : c);
        }
        return b.toString();
    }

    /** Lane keys: one lane event in lg cache slot 0 (see the class comment), or the clear set. */
    private void appendLanes(StringBuffer b, AaLaneFeed.Lanes l) {
        if (l.count <= 0) {
            b.append("lane_guidance_showing:n:0\n");
            b.append("lane_guidance_total:n:0\n");
            b.append("lane_guidance_index:n:-1\n");
            b.append("lane_guidance_slot:n:-1\n");
            b.append("lg0_index:n:-1\n");
            b.append("lg0_lane_count:n:-1\n");
            b.append("lg0_lane_complete:n:0\n");
            b.append("lg0_lane_positions:s:\n");
            b.append("lg0_lane_directions:s:\n");
            b.append("lg0_lane_status:s:\n");
            b.append("lg0_lane_angles:s:\n");
            return;
        }
        StringBuffer pos = new StringBuffer(), dir = new StringBuffer(), st = new StringBuffer();
        StringBuffer ang = new StringBuffer();
        for (int i = 0; i < l.count; i++) {
            int primary = 1000;
            if (i > 0) { pos.append(','); dir.append(','); st.append(','); ang.append('|'); }
            boolean first = true;
            for (int j = 0; j < l.shape[i].length; j++) {
                int a = AaLaneFeed.angle(l.shape[i][j]);
                if (a == 1000) continue;
                if (!first) ang.append(',');
                ang.append(a);
                first = false;
                if (l.hl[i][j] && primary == 1000) primary = a;
            }
            pos.append(i);
            dir.append(primary);
            st.append(primary == 1000 ? 0 : 2);
        }
        b.append("lane_guidance_showing:n:1\n");
        b.append("lane_guidance_total:n:1\n");
        b.append("lane_guidance_index:n:").append(laneEvent).append('\n');
        b.append("lane_guidance_slot:n:0\n");
        b.append("lg0_index:n:").append(laneEvent).append('\n');
        b.append("lg0_lane_count:n:").append(l.count).append('\n');
        b.append("lg0_lane_complete:n:1\n");
        b.append("lg0_lane_positions:s:").append(pos).append('\n');
        b.append("lg0_lane_directions:s:").append(dir).append('\n');
        b.append("lg0_lane_status:s:").append(st).append('\n');
        b.append("lg0_lane_angles:s:").append(ang).append('\n');
    }

    /** Full EVT_RGD_UPDATE text payload for the current picture. */
    public synchronized String snapshot() {
        StringBuffer b = new StringBuffer(512);
        b.append("@routeguidance\n");
        b.append("source_supports_rg:n:1\n");
        b.append("route_generation:n:").append(routeGeneration).append('\n');
        if (!routeActive || cur == null) {
            b.append("route_state:n:0\n");
            b.append("visible_in_app:n:0\n");
            b.append("maneuver_count:n:0\n");
            b.append("maneuver_list:s:\n");
            b.append("dist_maneuver_m:n:-1\n");
            appendLanes(b, AaLaneFeed.NONE);
            return b.toString();
        }
        b.append("route_state:n:1\n");
        b.append("visible_in_app:n:1\n");
        b.append("maneuver_state:n:1\n");
        b.append("maneuver_count:n:1\n");
        b.append("maneuver_list:s:0\n");
        b.append("dist_maneuver_m:n:").append(distM).append('\n');
        b.append("m0_ver:n:").append(ver).append('\n');
        b.append("m0_type:n:").append(cur.type).append('\n');
        if (cur.anglePresent) b.append("m0_turn_angle:n:").append(cur.angle).append('\n');
        b.append("m0_junction_type:n:").append(cur.junctionType).append('\n');
        b.append("m0_driving_side:n:").append(cur.drivingSide).append('\n');
        if (stepM > 0) b.append("m0_distance:n:").append(stepM).append('\n');
        if (curRoad.length() > 0) b.append("m0_after_road:s:").append(clean(curRoad)).append('\n');
        appendLanes(b, lanes);
        return b.toString();
    }
}
