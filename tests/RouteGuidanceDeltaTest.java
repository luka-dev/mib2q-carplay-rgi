import com.luka.carplay.bus.CarplayBus;
import com.luka.carplay.rgd.RouteGuidance;
import com.luka.carplay.framework.Log;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.lang.reflect.Modifier;
import java.util.Arrays;

/** Real delta parser; never starts the bus, presentation worker or HU services. */
public final class RouteGuidanceDeltaTest {
    private static void check(boolean value, String why) { if (!value) throw new AssertionError(why); }
    private static void feed(Method parse, RouteGuidance rg, String text) throws Exception {
        byte[] bytes = text.getBytes("UTF-8");
        parse.invoke(rg, CarplayBus.parseText(bytes, bytes.length));
    }
    /** Compare all public snapshot fields, including nested slot/lane arrays and pending dirty bits. */
    static String snapshot(RouteGuidance rg, RouteGuidance.State state) throws Exception {
        StringBuilder text = new StringBuilder();
        for (Field f : RouteGuidance.State.class.getFields()) {
            if (!Modifier.isStatic(f.getModifiers()))
                text.append(f.getName()).append('=').append(Arrays.deepToString(new Object[]{f.get(state)})).append('\n');
        }
        Field updates = RouteGuidance.class.getDeclaredField("hasRouteUpdate");
        updates.setAccessible(true);
        return text.append("hasRouteUpdate=").append(updates.getBoolean(rg)).toString();
    }
    private static void staleGenerations(Method parse, RouteGuidance rg, RouteGuidance.State state) throws Exception {
        String before = snapshot(rg, state);
        feed(parse, rg, "route_generation:n:100\nroute_state:n:0\nsource_supports_rg:n:0\n"
            + "disconnect_reason:s:stale disconnect\ncurrent_road:s:Stale road\ndist_maneuver_m:n:1\n"
            + "m0_ver:n:99\nm0_type:n:2\nm0_exit_angle:n:90\nlg3_index:n:99\nlg3_lane_count:n:9\n");
        check(before.equals(snapshot(rg, state)), "older generation mutated route, slots, lanes or dirty flags");
        feed(parse, rg, "route_generation:n:101\ncurrent_road:s:Current road\ndist_maneuver_m:n:200\n");
        check(state.routeGeneration == 101 && "Current road".equals(state.currentRoad)
            && state.distManeuverM == 200, "equal generation rejected a live delta");
        state.clearDirty();
        before = snapshot(rg, state);
        feed(parse, rg, "route_generation:n:0\ncurrent_road:s:Oldest route\n");
        check(before.equals(snapshot(rg, state)), "generation zero rolled back an active generation");
        feed(parse, rg, "route_generation:n:bad\ncurrent_road:s:Legacy road\n");
        check(state.routeGeneration == 101 && "Legacy road".equals(state.currentRoad), "invalid generation broke legacy deltas");
        feed(parse, rg, "route_generation:n:4294967297\nroute_state:n:1\nmaneuver_count:n:1\nmaneuver_list:s:0\n"
            + "m0_type:n:2\nm0_exit_angle:n:90\n");
        check(state.routeGeneration == 4294967297L && state.currentRoad == null && state.mAfterRoad[0] == null,
            "64-bit generation did not clear old content");
        state.clearDirty();
        before = snapshot(rg, state);
        Field running = RouteGuidance.class.getDeclaredField("running"); running.setAccessible(true);
        running.setBoolean(rg, true);
        try {
            byte[] stale = "route_generation:n:101\ndisconnect_reason:s:old peer\n".getBytes("UTF-8");
            rg.onFrame(CarplayBus.EVT_RGD_UPDATE, 0, stale, stale.length);
            check(before.equals(snapshot(rg, state)), "stale onFrame triggered lifecycle teardown");
            byte[] disconnect = "route_generation:n:4294967297\ndisconnect_reason:s:current peer\n".getBytes("UTF-8");
            rg.onFrame(CarplayBus.EVT_RGD_UPDATE, 0, disconnect, disconnect.length);
            check(state.routeGeneration == -1L && state.mType[0] == -1, "real disconnect did not reset generation");
        } finally { running.setBoolean(rg, false); }
        feed(parse, rg, "route_generation:n:0\nroute_state:n:1\nmaneuver_count:n:1\nmaneuver_list:s:0\nm0_type:n:1\n");
        check(state.routeGeneration == 0L && state.mType[0] == 1, "fresh session rejected its first generation");
    }
    public static void main(String[] args) throws Exception {
        Log.setLevel(-1);
        RouteGuidance rg = new RouteGuidance();
        Method parse = RouteGuidance.class.getDeclaredMethod("parse", CarplayBus.Data.class); parse.setAccessible(true);
        Field sf = RouteGuidance.class.getDeclaredField("state"); sf.setAccessible(true);
        RouteGuidance.State state = (RouteGuidance.State)sf.get(rg);
        feed(parse, rg, "source_supports_rg:n:1\nroute_state:n:1\nmaneuver_count:n:1\nmaneuver_list:s:0\nm0_type:n:3\nm0_turn_angle:n:-90\n");
        check(state.mType[0] == 3 && state.maneuverOrder[0] == 0, "initial maneuver");
        feed(parse, rg, "current_road:s:Невский проспект\nvisible_in_app:n:0\n");
        check(state.mType[0] == 3 && state.routeState == 1 && state.visibleInApp == 0, "metadata delta discarded route");
        feed(parse, rg, "route_state:n:0\n");
        check(state.mType[0] == 3 && state.maneuverOrder == null && state.maneuverCount == 0, "transient zero lost cached icon");
        feed(parse, rg, "route_state:n:5\nmaneuver_count:n:1\nmaneuver_list:s:0\n");
        check(state.mType[0] == 3 && state.maneuverOrder[0] == 0, "reroute did not restore cached icon");
        feed(parse, rg, "maneuver_list:s:\n");
        check(state.maneuverOrder != null && state.maneuverOrder.length == 0, "empty list confused with missing list");
        feed(parse, rg, "source_supports_rg:n:0\n");
        check(state.routeState == 0 && state.mType[0] == -1 && state.maneuverOrder == null, "source hard clear retained stale slot");
        feed(parse, rg, "source_supports_rg:n:1\nroute_state:n:1\nmaneuver_count:n:1\nmaneuver_list:s:0\n");
        check(state.mType[0] == -1, "source resume resurrected old icon");
        feed(parse, rg, "maneuver_list:s:bad,2,,2147483648\n");
        check(state.maneuverOrder.length == 4 && state.maneuverOrder[1] == 2, "malformed numeric list aborted delta");
        check(!state.mTurnAnglePresent[0], "clear must reset angle presence");
        feed(parse, rg, "m0_turn_angle:n:-1\n");
        check(state.mTurnAnglePresent[0] && state.mTurnAngle[0] == -1 && state.mExitAngle[0] == -1,
            "real -1 degree angle lost against the empty-slot default");
        feed(parse, rg, "m0_exit_angle:n:67\n");
        check(state.mTurnAngle[0] == 67 && state.mExitAngle[0] == 67, "exit-only delta not shared");
        feed(parse, rg, "m0_turn_angle:n:-45\nm0_exit_angle:n:90\n");
        check(state.mTurnAngle[0] == 90 && state.mExitAngle[0] == 90, "explicit exit alias must win");
        feed(parse, rg, "m0_turn_angle:n:bad\n");
        check(state.mTurnAngle[0] == 1000 && state.mExitAngle[0] == 1000, "invalid angle must be safe");
        feed(parse, rg, "source_supports_rg:n:0\n");
        check(!state.mTurnAnglePresent[0] && state.mExitAngle[0] == 1000, "angle survives source reset");
        feed(parse,rg,"lg3_index:n:42\nlg3_lane_count:n:2\nlg3_lane_complete:n:1\n");
        check(state.lgLaneComplete[3]==1,"native completeness bit lost");
        feed(parse,rg,"lg3_lane_complete:n:0\n");check(state.lgLaneComplete[3]==0,"truncation flag stale");
        feed(parse,rg,"source_supports_rg:n:1\nroute_state:n:1\nmaneuver_count:n:1\nmaneuver_list:s:0\n"
            +"m0_ver:n:20\nm0_type:n:1\nm0_turn_angle:n:-90\nm0_junction_angles:s:-90,0,90\n"
            +"m0_distance:n:6000\nm0_name:s:Old instruction\nm0_after_road:s:Old road\n"
            +"m0_lane_count:n:1\nm0_lane_directions:s:-90\nm0_lane_status:s:2\n");
        check(state.mJunctionAngles[0].length==3 && state.mDistance[0]==6000
            && "Old road".equals(state.mAfterRoad[0]),"slot fixture");
        state.clearDirty();
        feed(parse,rg,"m0_ver:n:20\nm0_exit_angle:n:-45\n");
        check(state.mJunctionAngles[0].length==3 && state.mName[0]!=null,"same-slot delta discarded omitted fields");
        feed(parse,rg,"m0_ver:n:21\nm0_type:n:2\nm0_exit_angle:n:90\n");
        check(state.mTurnAngle[0]==90 && state.mTurnAnglePresent[0],"new slot lost supplied angle");
        check(state.mJunctionAngles[0]==null && state.mAfterRoad[0]==null && state.mName[0]==null
            && state.mDistance[0]==-1,
            "new slot inherited omitted roads, text or distance");
        check(state.lgIndex[3]==42 && state.lgLaneCount[3]==2,"maneuver reassignment cleared independent lane event");
        feed(parse,rg,"m0_ver:n:22\nm0_type:n:2\nm0_exit_angle:n:1000\n");
        check(state.mExitAngle[0]==1000,"CarPlay positive safety sentinel lost");
        feed(parse,rg,"m0_exit_angle:n:-1000\n");
        check(state.mExitAngle[0]==-1000,"CarPlay negative safety sentinel lost");
        feed(parse,rg,"route_generation:n:100\nroute_state:n:1\nmaneuver_state:n:3\nmaneuver_count:n:1\nmaneuver_list:s:0\n"
            +"m0_ver:n:2\nm0_type:n:1\nm0_exit_angle:n:-90\nm0_after_road:s:Old route road\n"
            +"lg3_index:n:73\nlg3_lane_count:n:1\nlane_guidance_showing:n:1\nlane_guidance_slot:n:3\n");
        state.clearDirty();
        feed(parse,rg,"route_generation:n:101\nroute_state:n:1\nmaneuver_count:n:1\nmaneuver_list:s:0\n"
            +"m0_ver:n:2\nm0_type:n:1\nm0_exit_angle:n:-90\n");
        check(state.maneuverState==-1 && state.mAfterRoad[0]==null && state.lgIndex[3]==-1 && state.laneGuidanceShowing==-1,
            "new route reused old slot version and inherited route/lane data");
        check(state.dirtyMask!=0 && state.mVer[0]==2 && state.routeGeneration==101,"new generation failed to publish");
        staleGenerations(parse, rg, state);
        System.out.println("RouteGuidanceDeltaTest: delta retention, lifecycle, slot reuse, stale/equal/64-bit generations, disconnect reset, independent lanes and +/-1000 sentinels PASS");
    }
}
