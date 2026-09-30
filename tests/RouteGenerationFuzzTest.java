import com.luka.carplay.bus.CarplayBus;
import com.luka.carplay.framework.Log;
import com.luka.carplay.rgd.RouteGuidance;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.util.ArrayDeque;
import java.util.Arrays;
import java.util.Random;

/** Raw bus deltas checked against generation authority and a separate content model. */
public final class RouteGenerationFuzzTest {
    private static long checks;
    private static final ArrayDeque<String> history = new ArrayDeque<String>();
    private static void check(boolean value, String why) {
        checks++; if (!value) throw new AssertionError(why);
    }
    private static final class Model {
        long generation = -1;
        String road;
        int distance = -1, route = -1, source = -1, count;
        final int[] types, lanes;
        final String[] names;
        Model(int slots) { types = new int[slots]; lanes = new int[slots]; names = new String[slots]; clearContent(); }
        void clearContent() {
            road = null; distance = -1; count = 0;
            Arrays.fill(types, -1); Arrays.fill(lanes, -1); Arrays.fill(names, null);
        }
        void reset() { generation = -1; route = source = -1; clearContent(); }
        void verify(RouteGuidance.State s) {
            check(s.routeGeneration == generation, "generation authority diverged");
            check(s.distManeuverM == distance && equal(road, s.currentRoad), "road/distance content diverged");
            check(s.routeState == route && s.sourceSupportsRg == source && s.maneuverCount == count, "route lifecycle diverged");
            check(Arrays.equals(types, s.mType), "maneuver slots diverged");
            check(Arrays.equals(names, s.mName), "maneuver text diverged");
            check(Arrays.equals(lanes, s.lgIndex), "independent lane slots diverged");
        }
    }
    private static boolean equal(Object a, Object b) { return a == null ? b == null : a.equals(b); }
    private static void feed(Method parse, RouteGuidance rg, String frame) throws Exception {
        if (history.size() == 24) history.removeFirst();
        history.addLast(frame.replace('\n', ';'));
        byte[] bytes = frame.getBytes("UTF-8");
        parse.invoke(rg, CarplayBus.parseText(bytes, bytes.length));
    }
    public static void main(String[] args) throws Exception {
        Log.setLevel(-1);
        long seed = Long.decode(args[0]).longValue();
        int iterations = Integer.parseInt(args[1]);
        Random random = new Random(seed);
        RouteGuidance rg = new RouteGuidance();
        Method parse = RouteGuidance.class.getDeclaredMethod("parse", CarplayBus.Data.class); parse.setAccessible(true);
        Field stateField = RouteGuidance.class.getDeclaredField("state"); stateField.setAccessible(true);
        Field running = RouteGuidance.class.getDeclaredField("running"); running.setAccessible(true);
        RouteGuidance.State state = (RouteGuidance.State) stateField.get(rg);
        Model model = new Model(state.mType.length);
        long stale = 0, fresh = 0, equal = 0, legacy = 0, resets = 0, lifecycle = 0;
        int step = 0;
        try {
            for (; step < iterations; step++) {
                if (random.nextBoolean()) state.clearDirty();
                int action = random.nextInt(12);
                if (action == 0) {
                    // A real current-peer disconnect, through the full listener, permits a new epoch.
                    running.setBoolean(rg, true);
                    String frame = (model.generation < 0 ? "" : "route_generation:n:" + model.generation + "\n")
                        + "disconnect_reason:s:current peer\n";
                    byte[] bytes = frame.getBytes("UTF-8");
                    rg.onFrame(CarplayBus.EVT_RGD_UPDATE, 0, bytes, bytes.length);
                    running.setBoolean(rg, false);
                    history.addLast("RESET current peer");
                    if (history.size() > 24) history.removeFirst();
                    model.reset(); resets++;
                } else if (action == 1) {
                    feed(parse, rg, "route_state:n:0\n");
                    model.route = 0; model.count = 0; lifecycle++;
                } else if (action == 2) {
                    feed(parse, rg, "source_supports_rg:n:0\n");
                    if (model.source != 0) {
                        Arrays.fill(model.types, -1); Arrays.fill(model.names, null); Arrays.fill(model.lanes, -1);
                        model.route = 0; model.count = 0;
                    }
                    model.source = 0; lifecycle++;
                } else {
                    int kind = action <= 5 ? 0 : action <= 7 ? 1 : action <= 9 ? 2 : 3;
                    long generation;
                    String prefix;
                    if (kind == 0) {
                        generation = model.generation <= 0 ? -1 : model.generation - 1 - random.nextInt(3);
                        if (generation < 0 && model.generation > 0) generation = 0;
                        prefix = "route_generation:n:" + generation + "\n";
                    } else if (kind == 1) {
                        generation = model.generation < 0 ? random.nextInt(2) : model.generation;
                        prefix = "route_generation:n:" + generation + "\n";
                    } else if (kind == 2) {
                        generation = Math.max(0L, model.generation) + 1 + random.nextInt(100);
                        if (random.nextInt(20) == 0) generation += 4294967296L;
                        prefix = "route_generation:n:" + generation + "\n";
                    } else {
                        generation = -1;
                        prefix = random.nextBoolean() ? "" : "route_generation:n:bad\n";
                    }
                    boolean rejected = generation >= 0 && model.generation >= 0 && generation < model.generation;
                    if (rejected) {
                        String before = RouteGuidanceDeltaTest.snapshot(rg, state);
                        feed(parse, rg, prefix + "source_supports_rg:n:0\nroute_state:n:0\ndisconnect_reason:s:old peer\n"
                            + "current_road:s:STALE\ndist_maneuver_m:n:1\nm0_ver:n:999\nm0_type:n:2\n"
                            + "m0_junction_angles:s:90,180\nlg0_index:n:999\nlg0_lane_angles:s:1000|90\n");
                        check(before.equals(RouteGuidanceDeltaTest.snapshot(rg, state)), "stale frame changed the complete snapshot");
                        stale++;
                    } else {
                        boolean changed = generation >= 0 && generation != model.generation;
                        if (changed) { model.generation = generation; model.clearContent(); fresh++; }
                        else if (generation >= 0) equal++; else legacy++;
                        int slot = random.nextInt(model.types.length), lane = random.nextInt(model.lanes.length);
                        int type = 1 + random.nextInt(12), index = random.nextInt(10000), distance = random.nextInt(10000);
                        String road = "Road " + step + " Невский", name = "Turn " + step;
                        int route = random.nextBoolean() ? 1 : 5;
                        feed(parse, rg, prefix + "source_supports_rg:n:1\nroute_state:n:" + route + "\nmaneuver_count:n:1\n"
                            + "maneuver_list:s:" + slot + "\ncurrent_road:s:" + road + "\ndist_maneuver_m:n:" + distance
                            + "\nm" + slot + "_type:n:" + type + "\nm" + slot + "_name:s:" + name
                            + "\nlg" + lane + "_index:n:" + index + "\n");
                        model.source = 1; model.route = route; model.count = 1;
                        model.road = road; model.distance = distance; model.types[slot] = type;
                        model.names[slot] = name; model.lanes[lane] = index;
                    }
                }
                model.verify(state);
            }
            check(stale > 0 && fresh > 0 && equal > 0 && legacy > 0 && resets > 0 && lifecycle > 0, "missing fuzz event coverage");
        } catch (Throwable t) {
            System.err.println("RouteGenerationFuzzTest FAIL seed=" + seed + " step=" + step);
            for (String frame : history) System.err.println(frame);
            throw t;
        }
        System.out.println("RouteGenerationFuzzTest PASS seed=" + seed + " events=" + iterations + " checks=" + checks
            + " stale=" + stale + " fresh=" + fresh + " equal=" + equal + " legacy=" + legacy
            + " resets=" + resets + " lifecycle=" + lifecycle);
    }
}
