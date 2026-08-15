/*
 * Read-only bridge from the stock DSICarKombi display-preset feed to the
 * altscreen renderer.  Attribute 99 identifies the active preset position;
 * attribute 97 supplies pos -> type records (CLASSIC=2, DRIVINGPROFILE=3).
 *
 * This class owns no DSIActivator.  The stock NaviDSICarKombiListener is already
 * attached to DSICarKombi instance 0 and forwards its callbacks here, avoiding
 * the second-activator lifecycle which never became ready on MU1316.
 */
package com.luka.carplay.core;

import com.luka.carplay.framework.Log;

import org.dsi.ifc.carkombi.DCDisplayPresetsListRecord;
import org.dsi.ifc.carkombi.DCDisplayViewConfiguration;
import org.dsi.ifc.carkombi.DSICarKombi;

public final class VcLayoutState {
    private static final String TAG = "VCLayout";
    private static final Object LOCK = new Object();
    private static final int CACHE_SIZE = 32;

    private static final int[] presetTypes = new int[CACHE_SIZE];
    private static final boolean[] presetKnown = new boolean[CACHE_SIZE];
    private static int activePreset = -1;
    private static int publishedStyle = AltscreenControlServer.STYLE_UNKNOWN;

    private VcLayoutState() {}

    public static void onPresetCount(int count, int validFlag) {
        Log.i(TAG, "preset count=" + count + " valid=" + validFlag);
    }

    public static void onPresetRecords(String source,
                                       DCDisplayPresetsListRecord[] records,
                                       int validFlag) {
        synchronized (LOCK) {
            if (validFlag != 1 || records == null) {
                Log.i(TAG, source + " presets ignored valid=" + validFlag);
                return;
            }
            StringBuffer text = new StringBuffer(source).append(" presets [");
            for (int i = 0; i < records.length; i++) {
                DCDisplayPresetsListRecord record = records[i];
                if (i != 0) text.append(", ");
                if (record == null) {
                    text.append("null");
                    continue;
                }
                int pos = record.getPos();
                int type = record.getType();
                if (pos >= 0 && pos < CACHE_SIZE) {
                    presetTypes[pos] = type;
                    presetKnown[pos] = true;
                }
                text.append("pos=").append(pos)
                    .append(" type=").append(typeName(type))
                    .append('(').append(type).append(')');
            }
            text.append(']');
            Log.i(TAG, text.toString());
            publishResolvedLocked("preset-list");
        }
    }

    public static void onActivePreset(int preset, int validFlag) {
        synchronized (LOCK) {
            if (validFlag != 1) {
                Log.i(TAG, "active preset invalid value=" + preset
                    + " valid=" + validFlag + "; keeping current style");
                return;
            }
            activePreset = preset;
            publishResolvedLocked("active-preset");
        }
    }

    public static void onDisplayViewConfiguration(DCDisplayViewConfiguration config,
                                                   int validFlag) {
        int view = validFlag == 1 && config != null
            ? config.getActiveDisplayView() : -1;
        /* Diagnostic only.  The Audi View-button model remains authoritative
         * for the CarPlay fullscreen/smallscreen command. */
        Log.i(TAG, "DSI displayView=" + view + " valid=" + validFlag);
    }

    private static void publishResolvedLocked(String reason) {
        int type = typeForActiveLocked();
        int style = styleForType(type);
        Log.i(TAG, "active preset=" + activePreset + " type=" + typeName(type)
            + " style=" + styleName(style) + " reason=" + reason);
        if (style == AltscreenControlServer.STYLE_UNKNOWN) return;
        if (style != publishedStyle) {
            publishedStyle = style;
            AltScreenModule.setClusterStyle(style, activePreset);
        }
    }

    private static int typeForActiveLocked() {
        if (activePreset < 0 || activePreset >= CACHE_SIZE ||
            !presetKnown[activePreset]) return -1;
        return presetTypes[activePreset];
    }

    private static int styleForType(int type) {
        if (type == DSICarKombi.DCDISPLAYPRESETSTYPE_CLASSIC)
            return AltscreenControlServer.STYLE_CLASSIC;
        if (type == DSICarKombi.DCDISPLAYPRESETSTYPE_DRIVINGPROFILE)
            return AltscreenControlServer.STYLE_SPORT;
        return AltscreenControlServer.STYLE_UNKNOWN;
    }

    private static String typeName(int type) {
        if (type == DSICarKombi.DCDISPLAYPRESETSTYPE_CLASSIC) return "CLASSIC";
        if (type == DSICarKombi.DCDISPLAYPRESETSTYPE_DRIVINGPROFILE) return "DRIVINGPROFILE";
        if (type == DSICarKombi.DCDISPLAYPRESETSTYPE_INDIVIDUAL) return "INDIVIDUAL";
        if (type == DSICarKombi.DCDISPLAYPRESETSTYPE_UNKNOWN) return "UNKNOWN";
        return "UNRESOLVED";
    }

    private static String styleName(int style) {
        if (style == AltscreenControlServer.STYLE_CLASSIC) return "classic";
        if (style == AltscreenControlServer.STYLE_SPORT) return "sport";
        return "unknown";
    }
}
