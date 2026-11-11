package com.luka.carplay.rgd;

import com.luka.carplay.bus.CarplayBus;
import com.luka.carplay.core.CarPlayApp;
import com.luka.carplay.core.FrameworkRef;
import com.luka.carplay.framework.Log;
import de.audi.app.terminalmode.IContext;
import de.audi.atip.base.IFrameworkAccess;
import java.lang.reflect.Field;
import java.lang.reflect.InvocationHandler;
import java.lang.reflect.Method;
import java.lang.reflect.Proxy;
import java.util.Arrays;
import java.util.Locale;

/** Source identity/lifecycle and pure compensation through the production resolver. */
public final class NavigationStackTest {
    private static int checks;
    private static final String[] YANDEX_NAMES = {
        "Навигатор", "Навігатор", "Yandex Navi", "Yandex\u00a0Navi",
        "Yandex Navigator", "YandexNaviMaps", "YandexMaps", "Yandex Maps",
        "Yandex\u00a0Maps", "Яндекс Карты", "Яндекс\u00a0Карты",
        "Яндекс Карти", "Яндекс\u00a0Карти"
    };
    private static void check(boolean value, String why) {
        checks++; if (!value) throw new AssertionError(why);
    }
    private static RouteGuidance.State sample(String name, int type, int junction, int angle, boolean present) {
        RouteGuidance.State s = new RouteGuidance.State();
        s.sourceName = name;
        s.maneuverOrder = new int[]{0};
        s.mType[0] = type;
        s.mJunctionType[0] = junction;
        s.mDrivingSide[0] = 0;
        s.mTurnAngle[0] = angle;
        s.mTurnAnglePresent[0] = present;
        s.mExitAngle[0] = present ? angle : 1000;
        return s;
    }
    private static void aliases() {
        Locale original = Locale.getDefault();
        try {
            Locale.setDefault(new Locale("tr", "TR"));
            for (int i = 0; i < YANDEX_NAMES.length; i++)
                check(NavigationStack.forSource(YANDEX_NAMES[i]) == NavigationStack.YANDEX,
                    "Yandex alias " + YANDEX_NAMES[i]);
            check(NavigationStack.forSource(" YANDEX NAVI ") == NavigationStack.YANDEX, "locale-independent alias");
            check(NavigationStack.forSource(" ЯНДЕКС\u00a0КАРТЫ ") == NavigationStack.YANDEX,
                "localized Maps name normalization");
        } finally { Locale.setDefault(original); }
        check(NavigationStack.forSource("百度地图") == NavigationStack.BAIDU, "Baidu alias");
        String[] amap = {"Amap", "高德地图", "高德地圖", "AMapiPhone"};
        for (int i = 0; i < amap.length; i++)
            check(NavigationStack.forSource(amap[i]) == NavigationStack.AMAP, "AMap alias " + amap[i]);
        String[] defaults = {null, "", "Apple Maps", "Google Maps", "Waze",
            "Gaode", "Other Navigator", "Not Yandex Maps", "Not Yandex Navi", "Not Amap", "com.autonavi.amap",
            "com.baidu.map", "Baidu Maps", "ru.yandex.traffic", "ru.yandex.mobile.navigator"};
        for (int i = 0; i < defaults.length; i++)
            check(NavigationStack.forSource(defaults[i]) == NavigationStack.DEFAULT, "default source " + defaults[i]);
    }
    private static void defaultParity() {
        int[] angles = {-1000, -181, -180, -135, -90, -45, -1, 0, 45, 90, 180, 181, 1000};
        for (int type = -1; type <= 54; type++) for (int junction = -1; junction <= 2; junction++)
            for (int side = 0; side <= 1; side++) for (int a = 0; a < angles.length; a++) {
                RouteGuidance.State s = sample("Google Maps", type, junction, angles[a], true);
                s.mDrivingSide[0] = side;
                s.mJunctionAngles[0] = new int[]{-90, 0, 90};
                int[] old = ManeuverMapper.map(type, angles[a], junction, side, true);
                ManeuverPresentation p = ManeuverPresentation.resolve(s, true);
                if (!ManeuverMapper.isValidType(type) || old[0] == ManeuverMapper.NO_INFO
                        || old[0] == ManeuverMapper.NO_SYMBOL) {
                    check(p == ManeuverPresentation.FOLLOW, "default neutral parity");
                } else {
                    check(p.main == old[0] && p.direction == old[1], "default HUD parity");
                    RendererMapper.Mapping before = RendererMapper.map(old[0], old[1], side, type,
                        angles[a], true, s.mJunctionAngles[0]);
                    RendererMapper.Mapping after = RendererMapper.map(p, s.mJunctionAngles[0]);
                    check(before.icon == after.icon && before.direction == after.direction
                        && before.exitAngle == after.exitAngle && before.drivingSide == after.drivingSide
                        && before.snapToRoad == after.snapToRoad
                        && Arrays.equals(before.junctionAngles, after.junctionAngles), "default renderer parity");
                }
            }
    }
    private static void compensation() {
        int[] baidu = {90, 0, -45, -90, -135};
        for (int side = 0; side <= 1; side++) for (int i = 0; i < baidu.length; i++) {
            RouteGuidance.State s = sample("百度地图", 28 + i, -1, -1, false);
            s.mDrivingSide[0] = side;
            s.mJunctionAngles[0] = new int[]{-90, 0, 45};
            ManeuverPresentation p = ManeuverPresentation.resolve(s, true);
            check(p.hasManeuver() && p.semantics.junctionInferred, "Baidu missing junction");
            check(p.semantics.angle == baidu[i] && p.semantics.angleSource == ManeuverSemantics.ANGLE_DIRECTION_HINT
                && p.semantics.exitNumber == 0, "Baidu direction is not an ordinal/measured angle");
            check(p.direction == ManeuverMapper.directionFromAngle16(baidu[i]), "Baidu HUD direction");
            RendererMapper.Mapping render = RendererMapper.map(p, s.mJunctionAngles[0]);
            check(render.icon == RendererMapper.ICON_ROUNDABOUT && render.exitAngle == baidu[i] * 2
                && render.drivingSide == side && !render.snapToRoad, "Baidu renderer coarse direction");
            check(s.mJunctionType[0] == -1 && s.mTurnAngle[0] == -1 && !s.mTurnAnglePresent[0]
                && s.mExitAngle[0] == 1000, "compensation mutated received fields");
        }
        int[] measured = {-180, -1, 0, 46, 180};
        for (int i = 0; i < measured.length; i++) {
            ManeuverPresentation p = ManeuverPresentation.resolve(sample("百度地图", 28, -1, measured[i], true), true);
            check(p.semantics.angle == measured[i] && p.semantics.angleSource == ManeuverSemantics.ANGLE_RECEIVED,
                "received angle always wins (including zero and -1)");
        }
        int[] invalid = {-1000, 1000, -181, 181};
        for (int i = 0; i < invalid.length; i++)
            check(ManeuverPresentation.resolve(sample("百度地图", 31, -1, invalid[i], true), true)
                .semantics.angle == -90, "invalid geometry permits coarse direction");
        for (int junction = 0; junction <= 3; junction += 2)
            check(ManeuverPresentation.resolve(sample("百度地图", 28, junction, -1, false), true)
                == ManeuverPresentation.FOLLOW, "explicit conflicting junction is not overwritten");
        for (int type = 28; type <= 46; type++) {
            ManeuverPresentation y = ManeuverPresentation.resolve(sample("Yandex Navi", type, 1, -1, false), true);
            check(y.semantics.exitNumber == type - 27 && !y.semantics.hasAngle(), "Yandex ordinal without angle");
            check(y.semantics.angleSource != ManeuverSemantics.ANGLE_DIRECTION_HINT, "ordinal invented a direction");
        }
        ManeuverPresentation y = ManeuverPresentation.resolve(sample("Yandex Navi", 33, 1, -90, true), true);
        check(y.semantics.exitNumber == 6 && y.semantics.angle == -90, "independent exit 6 and left angle");
        check(ManeuverPresentation.resolve(sample("Yandex Navi", 7, 1, -1, false), true).semantics.exitNumber == 0,
            "Yandex generic exit is not necessarily exit 20");
        check(ManeuverPresentation.resolve(sample("百度地图", 33, 1, -1, false), true).semantics.angleSource
            == ManeuverSemantics.ANGLE_UNKNOWN, "unobserved Baidu type guessed direction");
        check(ManeuverPresentation.resolve(sample("百度地图", 28, -1, -1, false), false)
            == ManeuverPresentation.NONE, "inactive provider produces guidance");
    }
    private static void feed(Method parse, RouteGuidance rg, String text) throws Exception {
        byte[] b = text.getBytes("UTF-8"); parse.invoke(rg, CarplayBus.parseText(b, b.length));
    }
    private static void amapCompensation() {
        for (int side = 0; side <= 1; side++) for (int junction = -1; junction <= 1; junction++) {
            for (int type : new int[]{28, 30}) {
                RouteGuidance.State s = sample("Amap", type, junction, -1, false);
                s.mDrivingSide[0] = side;
                int expected = type == 28 ? -90 : 90;
                ManeuverPresentation p = ManeuverPresentation.resolve(s, true);
                check(p.main == (side == 0 ? ManeuverMapper.ROUNDABOUT_TRS_RIGHT : ManeuverMapper.ROUNDABOUT_TRS_LEFT)
                    && p.direction == (type == 28 ? ManeuverMapper.DIR_LEFT : ManeuverMapper.DIR_RIGHT),
                    "AMap direction/traffic side");
                check(p.semantics.angle == expected && p.semantics.angleSource == ManeuverSemantics.ANGLE_DIRECTION_HINT
                    && p.semantics.exitNumber == 0 && !p.semantics.roundaboutDeparture,
                    "AMap hint is neither Baidu direction nor Yandex ordinal");
                RendererMapper.Mapping r = RendererMapper.map(p, new int[]{-45, 0, 45});
                check(r.icon == RendererMapper.ICON_ROUNDABOUT && r.exitAngle == expected * 2
                    && !r.snapToRoad, "AMap renderer must not snap inferred direction");
                check(s.mType[0] == type && s.mJunctionType[0] == junction && s.mExitAngle[0] == 1000,
                    "AMap compensation edited raw state");
            }
            RouteGuidance.State s = sample("高德地圖", 29, junction, -1, false);
            s.mDrivingSide[0] = side;
            ManeuverPresentation p = ManeuverPresentation.resolve(s, true);
            RendererMapper.Mapping r = RendererMapper.map(p, new int[]{-90, 0, 90});
            check(p.main == (side == 0 ? ManeuverMapper.EXIT_ROUNDABOUT_TRS_RIGHT : ManeuverMapper.EXIT_ROUNDABOUT_TRS_LEFT)
                && p.semantics.roundaboutDeparture && p.semantics.mappingType() == ManeuverMapper.MT_EXIT_ROUNDABOUT,
                "AMap CP29 means generic departure");
            check(p.semantics.type == 29 && !p.semantics.hasAngle() && p.semantics.exitNumber == 0,
                "departure invented an angle/ordinal or lost received enum");
            check(r.icon == RendererMapper.ICON_ROUNDABOUT_EXIT && r.drivingSide == side
                && r.junctionAngles.length == 0 && !r.snapToRoad,
                "unknown departure must use the existing departure glyph");
        }
        for (int type : new int[]{28, 29, 30}) {
            for (int angle : new int[]{-180, -1, 0, 46, 180}) {
                ManeuverPresentation p = ManeuverPresentation.resolve(sample("Amap", type, 0, angle, true), true);
                RendererMapper.Mapping r = RendererMapper.map(p, null);
                check(p.semantics.angleSource == ManeuverSemantics.ANGLE_RECEIVED && p.semantics.angle == angle
                    && r.icon == RendererMapper.ICON_ROUNDABOUT && r.exitAngle == angle * 2,
                    "AMap received geometry must win, including 0 and -1");
            }
            for (int angle : new int[]{-1000, 1000, -181, 181}) {
                ManeuverPresentation p = ManeuverPresentation.resolve(sample("Amap", type, 0, angle, true), true);
                check(p.semantics.angleSource == (type == 29 ? ManeuverSemantics.ANGLE_UNKNOWN
                    : ManeuverSemantics.ANGLE_DIRECTION_HINT), "invalid AMap angle treated as measured");
            }
            for (int junction : new int[]{2, 255})
                check(ManeuverPresentation.resolve(sample("Amap", type, junction, -1, false), true)
                    == ManeuverPresentation.FOLLOW, "AMap must not overwrite an unknown junction");
        }
        for (int type : new int[]{1, 2, 3, 6, 19, 31, 46}) {
            ManeuverPresentation a = ManeuverPresentation.resolve(sample("Amap", type, 0, -1, false), true);
            ManeuverPresentation d = ManeuverPresentation.resolve(sample(null, type, 0, -1, false), true);
            check(a.main == d.main && a.direction == d.direction, "AMap changed an unrelated/ambiguous type");
        }
        for (int type : new int[]{28, 29, 30}) {
            ManeuverPresentation b = ManeuverPresentation.resolve(sample("百度地图", type, 1, -1, false), true);
            ManeuverPresentation a = ManeuverPresentation.resolve(sample("Amap", type, 1, -1, false), true);
            ManeuverPresentation y = ManeuverPresentation.resolve(sample("Yandex Navi", type, 1, -1, false), true);
            check(b.main != a.main || b.direction != a.direction, "AMap reused Baidu meaning for " + type);
            check(y.semantics.exitNumber == type - 27 && !y.semantics.hasAngle()
                && !y.semantics.roundaboutDeparture, "Yandex inherited a directional/departure compensation");
            check(ManeuverPresentation.resolve(sample(null, type, 0, -1, false), true)
                == ManeuverPresentation.FOLLOW, "AMap override escaped to default");
        }
    }
    private static void lifecycle() throws Exception {
        RouteGuidance rg = new RouteGuidance();
        Method parse = RouteGuidance.class.getDeclaredMethod("parse", CarplayBus.Data.class); parse.setAccessible(true);
        Field f = RouteGuidance.class.getDeclaredField("state"); f.setAccessible(true);
        RouteGuidance.State s = (RouteGuidance.State) f.get(rg);
        String man = "route_state:n:1\nmaneuver_count:n:1\nmaneuver_list:s:0\nm0_ver:n:1\nm0_type:n:31\n";
        feed(parse, rg, "route_generation:n:100\n" + man);
        check(ManeuverPresentation.resolve(s, true) == ManeuverPresentation.FOLLOW, "unknown source is default");
        s.clearDirty();
        feed(parse, rg, "source_name:s:百度地图\n");
        check(s.mType[0] == 31 && ManeuverPresentation.resolve(s, true).direction == ManeuverMapper.DIR_LEFT,
            "late source enriches existing maneuver");
        check((s.dirtyMask & RouteGuidance.State.DIRTY_MANEUVER_ICON) != 0, "late source did not request repaint");
        long revision = s.sourceRevision;
        s.clearDirty(); feed(parse, rg, "source_name:s:百度地图\n");
        check(s.sourceRevision == revision && s.dirtyMask == 0, "cached source forces repeated repaint");
        feed(parse, rg, "dist_maneuver_m:n:50\nlg0_index:n:7\nlg0_lane_count:n:2\n");
        check("百度地图".equals(s.sourceName), "missing source discarded provider");
        feed(parse, rg, "source_name:s:Yandex Navi\n" + man + "m0_junction_type:n:1\n");
        check(s.distManeuverM == -1 && s.lgIndex[0] == -1 && s.sourceRevision > revision,
            "source switch inherited distance/lanes or identity");
        check(ManeuverPresentation.resolve(s, true).semantics.exitNumber == 4
            && !ManeuverPresentation.resolve(s, true).semantics.hasAngle(), "Yandex inherited Baidu direction");
        feed(parse, rg, "source_name:s:\n");
        check(NavigationStack.forSource(s.sourceName) == NavigationStack.DEFAULT && s.mType[0] == -1,
            "explicit empty retained old provider/data");
        feed(parse, rg, "source_name:s:百度地图\n" + man);
        feed(parse, rg, "route_generation:n:101\n" + man);
        check(s.sourceName == null && ManeuverPresentation.resolve(s, true) == ManeuverPresentation.FOLLOW,
            "new generation inherited absent provider");
        feed(parse, rg, "route_generation:n:100\nsource_name:s:百度地图\n");
        check(s.sourceName == null, "stale frame changed provider");
        feed(parse, rg, "source_name:s:百度地图\n");
        feed(parse, rg, "source_supports_rg:n:0\n");
        check(s.sourceName == null && s.mType[0] == -1, "unsupported source retained stack");
        s.sourceName = "百度地图"; s.reset();
        check(s.sourceName == null && s.sourceRevision == 0, "disconnect/reset retained source");
        feed(parse, rg, man.replace("m0_type:n:31", "m0_type:n:2"));
        s.clearDirty(); revision = s.sourceRevision;
        feed(parse, rg, "source_name:s:Google Maps\n");
        check(s.dirtyMask == 0 && s.sourceRevision == revision && s.mType[0] == 2,
            "late default name restarted unchanged guidance");
        s.reset();
        feed(parse, rg, man + "m0_junction_type:n:1\n");
        s.clearDirty();
        feed(parse, rg, "source_name:s:Яндекс\u00a0Карты\n");
        ManeuverPresentation maps = ManeuverPresentation.resolve(s, true);
        check(s.mType[0] == 31 && s.mDrivingSide[0] == -1
            && maps.semantics.drivingSide == ManeuverMapper.DRIVING_SIDE_RIGHT
            && maps.semantics.drivingSideInferred
            && maps.semantics.exitNumber == 4 && maps.schematic != null,
            "late Maps name must enable numbered roundabout even when TLV 0x08 is omitted");
        check((s.dirtyMask & RouteGuidance.State.DIRTY_MANEUVER_ICON) != 0,
            "late Maps name did not request repaint");
    }
    private static void vehicleDrivingSideFallback() throws Exception {
        final int[] rhd = {0};
        final IFrameworkAccess fw = (IFrameworkAccess) Proxy.newProxyInstance(
            NavigationStackTest.class.getClassLoader(), new Class[]{IFrameworkAccess.class},
            new InvocationHandler() {
                public Object invoke(Object proxy, Method method, Object[] args) {
                    if ("getSysConst".equals(method.getName())) {
                        int id = ((Integer) args[0]).intValue();
                        return Integer.valueOf(id == 464 ? rhd[0] : 0);
                    }
                    Class<?> rt = method.getReturnType();
                    if (rt == Boolean.TYPE) return Boolean.FALSE;
                    if (rt == Integer.TYPE) return Integer.valueOf(0);
                    return null;
                }
            });
        IContext ctx = (IContext) Proxy.newProxyInstance(
            NavigationStackTest.class.getClassLoader(), new Class[]{IContext.class},
            new InvocationHandler() {
                public Object invoke(Object proxy, Method method, Object[] args) {
                    if ("getFramework".equals(method.getName())) return fw;
                    Class<?> rt = method.getReturnType();
                    if (rt == Boolean.TYPE) return Boolean.FALSE;
                    if (rt == Integer.TYPE) return Integer.valueOf(0);
                    return null;
                }
            });
        Field fwRefField = CarPlayApp.class.getDeclaredField("fwRef");
        fwRefField.setAccessible(true);
        Object savedFwRef = fwRefField.get(null);
        try {
            fwRefField.set(null, null);
            RouteGuidance.State uninit = sample("Яндекс\u00a0Карты", 30, 1, -1, false);
            uninit.mDrivingSide[0] = ManeuverMapper.DRIVING_SIDE_NOT_SET;
            ManeuverPresentation up = ManeuverPresentation.resolve(uninit, true);
            check(up.semantics.drivingSide == ManeuverMapper.DRIVING_SIDE_RIGHT
                && up.semantics.drivingSideInferred && up.schematic != null,
                "uninitialized framework ref must default fallback to RHT");

            fwRefField.set(null, new FrameworkRef(ctx));
            for (int carRhd = 0; carRhd <= 1; carRhd++) {
                rhd[0] = carRhd;
                int expectedSide = carRhd == 1
                    ? ManeuverMapper.DRIVING_SIDE_LEFT : ManeuverMapper.DRIVING_SIDE_RIGHT;
                RouteGuidance.State y = sample("Яндекс\u00a0Карты", 30, 1, -1, false);
                y.mDrivingSide[0] = ManeuverMapper.DRIVING_SIDE_NOT_SET;
                ManeuverPresentation yp = ManeuverPresentation.resolve(y, true);
                RendererMapper.Mapping yr = RendererMapper.map(yp, null);
                check(yp.semantics.drivingSide == expectedSide && yp.semantics.drivingSideInferred
                    && yp.schematic != null && yr.drivingSide == expectedSide
                    && yr.exitAngle == (carRhd == 1 ? 90 : -90)
                    && y.mDrivingSide[0] == ManeuverMapper.DRIVING_SIDE_NOT_SET,
                    "absent TLV 0x08 must follow vehicle sysConst(464) without mutating raw state");

                // Explicit phone drivingSide must always override vehicle RHD/LHD coding (e.g. border crossing).
                int opposite = 1 - expectedSide;
                y.mDrivingSide[0] = opposite;
                ManeuverPresentation explicit = ManeuverPresentation.resolve(y, true);
                RendererMapper.Mapping er = RendererMapper.map(explicit, null);
                check(explicit.semantics.drivingSide == opposite && !explicit.semantics.drivingSideInferred
                    && explicit.schematic != null && er.drivingSide == opposite
                    && er.exitAngle == (opposite == 1 ? 90 : -90),
                    "explicit phone drivingSide must override vehicle fallback");

                RouteGuidance.State a = sample("Amap", 29, 0, -1, false);
                a.mDrivingSide[0] = ManeuverMapper.DRIVING_SIDE_NOT_SET;
                ManeuverPresentation ap = ManeuverPresentation.resolve(a, true);
                check(ap.main == (expectedSide == 1
                        ? ManeuverMapper.EXIT_ROUNDABOUT_TRS_LEFT : ManeuverMapper.EXIT_ROUNDABOUT_TRS_RIGHT),
                    "AMap absent drivingSide must follow vehicle fallback");
            }
        } finally {
            fwRefField.set(null, savedFwRef);
        }
    }
    private static void yandexSchematic() throws Exception {
        Method streets = BAPBridge.class.getDeclaredMethod("maneuverSideStreets",
            RouteGuidance.State.class, ManeuverPresentation.class);
        streets.setAccessible(true);
        int[][] sideCases = {
            {ManeuverMapper.DRIVING_SIDE_NOT_SET, ManeuverMapper.DRIVING_SIDE_RIGHT, 0},
            {ManeuverMapper.DRIVING_SIDE_NOT_SET, ManeuverMapper.DRIVING_SIDE_LEFT, 1},
            {ManeuverMapper.DRIVING_SIDE_RIGHT, ManeuverMapper.DRIVING_SIDE_LEFT, 0},
            {ManeuverMapper.DRIVING_SIDE_LEFT, ManeuverMapper.DRIVING_SIDE_RIGHT, 1}
        };
        for (int name = 0; name < YANDEX_NAMES.length; name++)
        for (int sc = 0; sc < sideCases.length; sc++) for (int ordinal = 1; ordinal <= 19; ordinal++) {
            int rawSide = sideCases[sc][0], fallbackSide = sideCases[sc][1], side = sideCases[sc][2];
            RouteGuidance.State s = sample(YANDEX_NAMES[name], 27 + ordinal, 1, -1, false);
            s.mDrivingSide[0] = rawSide;
            ManeuverPresentation p = ManeuverPresentation.resolve(s, true, fallbackSide);
            check(!p.semantics.hasAngle() && p.semantics.exitNumber == ordinal
                && p.semantics.drivingSide == side
                && p.semantics.drivingSideInferred == (rawSide == ManeuverMapper.DRIVING_SIDE_NOT_SET),
                "synthetic geometry must not become received evidence");
            if (ordinal > 14) {
                check(p.schematic == null, "too many exits must not be silently merged");
                continue;
            }
            RendererMapper.Mapping r = RendererMapper.map(p, null);
            byte[] hud = (byte[]) streets.invoke(null, s, p);
            check(r.icon == RendererMapper.ICON_ROUNDABOUT && r.drivingSide == side && !r.snapToRoad,
                "schematic must remain a fixed circle on resolved traffic side");
            check(r.junctionAngles.length == ordinal + 1 && hud.length == ordinal,
                "selected exit plus one AFTER, excluding entry");
            check(r.exitAngle == r.junctionAngles[ordinal - 1]
                && r.exitAngle == RendererMapper.directionAngleHalfDegrees(p.direction),
                "same selected exit on HUD and renderer");
            int before = 0, after = 0;
            boolean[] occupied = new boolean[16];
            for (int i = 0; i < r.junctionAngles.length; i++) {
                int a = r.junctionAngles[i];
                check(a > -360 && a < 360 && a % 45 == 0, "exact non-entry HUD bin");
                int bin = ((-a / 45) + 16) % 16;
                check(!occupied[bin], "distinct exits survive HUD quantization");
                occupied[bin] = true;
                if (i == ordinal - 1) continue;
                boolean isBefore = side == 0 ? a > r.exitAngle : a < r.exitAngle;
                if (isBefore) before++; else after++;
                boolean found = false;
                for (int j = 0; j < hud.length; j++)
                    if ((hud[j] & 255) == bin * 16) found = true;
                check(found, "every renderer side road reaches HUD");
            }
            check(before == ordinal - 1 && after == 1, "ordinal must survive mirroring");
            if (ordinal == 6) check(r.exitAngle == (side == 0 ? -180 : 180), "sixth-exit layout");
            check(s.mDrivingSide[0] == rawSide && s.mTurnAngle[0] == -1 && s.mExitAngle[0] == 1000
                && s.mJunctionAngles[0] == null, "raw state changed");
            for (int a = -180; a <= 180; a++) {
                s.mTurnAngle[0] = a; s.mTurnAnglePresent[0] = true;
                p = ManeuverPresentation.resolve(s, true, fallbackSide);
                check(p.schematic == null && RendererMapper.map(p, null).exitAngle == a * 2,
                    "every valid received angle supersedes schematic, including -1/0");
            }
        }
        RouteGuidance.State s = sample("Yandex Navi", 33, 1, -1, false);
        s.mJunctionAngles[0] = new int[]{1000, 0};
        check(ManeuverPresentation.resolve(s, true).schematic == null, "do not mix real and synthetic roads");
        s.mJunctionAngles[0] = new int[]{1000, -1000};
        check(ManeuverPresentation.resolve(s, true).schematic != null, "sentinels are not geometry");
        for (int badSide : new int[]{-2, 2, 255}) {
            s.mDrivingSide[0] = badSide;
            check(ManeuverPresentation.resolve(s, true).schematic == null,
                "explicit invalid circulation side must not synthesize schematic");
        }
        s.mDrivingSide[0] = ManeuverMapper.DRIVING_SIDE_NOT_SET;
        check(ManeuverPresentation.resolve(s, true).schematic != null,
            "absent circulation side must use vehicle fallback");
        String[] sources = {null, "Google Maps", "百度地图", "Amap", "Not Yandex Maps"};
        for (int i = 0; i < sources.length; i++) {
            s.sourceName = sources[i];
            check(ManeuverPresentation.resolve(s, true).schematic == null, "Yandex-only presentation");
        }
    }

    public static void main(String[] args) throws Exception {
        Log.setLevel(-1);
        aliases(); defaultParity(); compensation(); amapCompensation(); lifecycle();
        vehicleDrivingSideFallback(); yandexSchematic();
        System.out.println("Navigation stacks: " + checks + " checks PASS");
    }
}
