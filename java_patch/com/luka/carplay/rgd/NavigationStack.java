package com.luka.carplay.rgd;

import java.util.Locale;

/** Source selection is separate from compensation. Everything unrecognised,
 * including Apple/Google, uses the unchanged default mapper. Names are
 * display-name aliases from the inspected IPAs, not authenticated bundle IDs. */
final class NavigationStack {
    static final NavigationStack DEFAULT = new NavigationStack("default", new ManeuverCompensation[0]);
    private static final ManeuverCompensation ROUNDABOUT_JUNCTION = new RoundaboutJunctionCompensation();
    static final NavigationStack BAIDU = new NavigationStack("baidu", new ManeuverCompensation[] {
        ROUNDABOUT_JUNCTION, new BaiduRoundaboutCompensation()
    });
    static final NavigationStack AMAP = new NavigationStack("amap", new ManeuverCompensation[] {
        new AmapRoundaboutCompensation()
    });
    static final NavigationStack YANDEX = new NavigationStack("yandex", new ManeuverCompensation[] {
        ROUNDABOUT_JUNCTION, new YandexRoundaboutCompensation()
    });

    final String name;
    private final ManeuverCompensation[] modules;

    private NavigationStack(String name, ManeuverCompensation[] modules) {
        this.name = name;
        this.modules = modules;
    }

    static NavigationStack forSource(String source) {
        String key = sourceKey(source);
        if ("百度地图".equals(key)) return BAIDU;
        if ("amap".equals(key) || "高德地图".equals(key) || "高德地圖".equals(key)
                || "amapiphone".equals(key)) return AMAP;
        if ("yandex navi".equals(key) || "yandex navigator".equals(key)
                || "yandexnavimaps".equals(key) || "навигатор".equals(key)
                || "навігатор".equals(key) || "yandex maps".equals(key)
                || "yandexmaps".equals(key) || "яндекс карты".equals(key)
                || "яндекс карти".equals(key)) return YANDEX;
        return DEFAULT;
    }

    /** Locale-independent matching; localized Yandex names contain U+00A0. Do not use
     * substring guesses such as "map", "nav" or a maneuver signature. */
    static String sourceKey(String source) {
        if (source == null) return "";
        return source.replace('\u00a0', ' ').trim().toLowerCase(Locale.ENGLISH);
    }

    ManeuverSemantics resolve(int type, int junction, int side, int angle, boolean present) {
        return resolve(type, junction, side, ManeuverMapper.DRIVING_SIDE_RIGHT, angle, present);
    }

    ManeuverSemantics resolve(int type, int junction, int side, int fallbackSide,
                              int angle, boolean present) {
        ManeuverSemantics value = new ManeuverSemantics(
            type, junction, side, fallbackSide, angle, present);
        for (int i = 0; i < modules.length; i++) modules[i].apply(value);
        return value;
    }
}
