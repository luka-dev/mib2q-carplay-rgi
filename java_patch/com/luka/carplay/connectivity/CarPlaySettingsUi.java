/*
 * "CarPlay settings" entry in Connection manager -> Options (drawer 2500001) and
 * submenu screen 2500140 (cMOPTABOUTCARPLAY) with:
 *   1. Spatial Audio      (checkbox -> KEY_SPATIAL 1001, spatial=0|1, CMD_SPATIAL; external amp only)
 *   2. UpMix              (3-choice: Off | Normal | Intense -> KEY_UPMIX 1000, upmix=0..2, CMD_UPMIX; external amp only)
 *   3. CarPlay on Cluster (checkbox: On | Off -> KEY_ALTSCREEN 1002, altscreen=0|1, CMD_ALT_CFG)
 *   4. Position           (3-choice: Left | Center | Right -> KEY_ALT_LAYOUT 1004, alt_layout=0..2, CMD_ALT_CFG)
 *   5. Route Guidance     (checkbox: On | Off -> KEY_RGI_ENABLED 1005, rgi=0|1; unlocks rows 6..8)
 *   6. CarPlay Map Cards  (2-choice: Hide when guiding | Always show -> KEY_ALT_URL 1003, alt_url=0|1, CMD_ALT_CFG)
 *   7. Turn Preview       (2-choice: Always next turn | Straight arrow when far -> KEY_RGI_FAR 1006, rgi_far=0|1)
 *   8. Cockpit Turn Card  (2-choice: Always visible | Auto-hide when far -> KEY_RGI_VC 1007, rgi_vc=0|1)
 * Each row carries a native MMI Infoline hint shown on focus.
 *
 * Stored in the stock HMI persistence (IStorageAccess, namespace 1009) and flushed
 * to flash on every write through FlushValuesDict.register().
 *
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */
package com.luka.carplay.connectivity;

import com.luka.carplay.bus.CarplayBus;
import com.luka.carplay.core.AltScreenModule;
import com.luka.carplay.framework.Log;
import com.luka.carplay.rgd.RouteGuidance;
import com.luka.carplay.tone.SoundControlUi;
import de.audi.atip.base.IFrameworkAccess;
import de.audi.atip.hmi.IDrawerContextConnectivity;
import de.audi.atip.hmi.model.ChoiceModel;
import de.audi.atip.hmi.model.HMIModel;
import de.audi.atip.hmi.model.listener.DefaultChoiceListener;
import de.audi.atip.hmi.view.HMIView;
import de.audi.atip.hmi.view.IDrawerController;
import de.audi.atip.hmi.view.Screen;
import de.audi.atip.interapp.audio.AmplifierListener;
import de.audi.atip.model.ConnectivityModelBank;
import de.audi.atip.storage.IStorageAccess;
import de.audi.tghu.connectivity.hmi.evohigh.ConnectivityScreenFactory;
import de.audi.tghu.connectivity.hmi.evohigh.FontFactory;
import de.audi.tghu.storage.FlushValuesDict;
import de.esolutions.hmi.widgets.audi.base.AbstractScreenWidget;
import de.esolutions.hmi.widgets.audi.base.AbstractWidget;
import de.esolutions.hmi.widgets.audi.base.widgets.AbstractWidgetController;
import de.esolutions.hmi.widgets.audi.evo.PropertyObject;
import de.esolutions.hmi.widgets.audi.evo.high.widgets.AbstractRendererHigh;
import de.esolutions.hmi.widgets.audi.evo.high.widgets.CompositeRendererHigh;
import de.esolutions.hmi.widgets.audi.base.StringUtility;
import de.esolutions.hmi.widgets.audi.evo.widgets.DrawerController;
import de.esolutions.hmi.widgets.audi.evo.widgets.LabelController;
import de.esolutions.hmi.widgets.audi.evo.widgets.menu.InfolineController;
import de.esolutions.hmi.widgets.audi.evo.widgets.menu.InfolineTimerController;
import de.esolutions.hmi.widgets.audi.evo.widgets.menu.MenuController;
import de.esolutions.hmi.widgets.audi.evo.widgets.menu.MenuItemController;
import de.esolutions.hmi.widgets.audi.evo.widgets.menu.MultiLineMenuItemController;
import java.io.FileOutputStream;
import java.util.List;

public final class CarPlaySettingsUi {
    private static final String TAG = "CarPlaySettings";
    private static final int NAMESPACE = 1009;     // the Sound app's own (WirelessChargingReminder: 1009/1)
    private static final int KEY_UPMIX       = 1000;
    private static final int KEY_SPATIAL     = 1001;
    private static final int KEY_ALTSCREEN   = 1002;
    private static final int KEY_ALT_URL     = 1003;
    private static final int KEY_ALT_LAYOUT  = 1004;
    private static final int KEY_RGI_ENABLED = 1005;
    private static final int KEY_RGI_FAR     = 1006;

    public static final int LEVEL_OFF = 0;
    public static final int LEVEL_NORMAL = 1;
    public static final int LEVEL_INTENSE = 2;
    private static final int NORMAL = LEVEL_NORMAL, COUNT = 3;
    public static final int CHOICE_OFF = 0;
    public static final int CHOICE_NORMAL = 1;
    public static final int CHOICE_INTENSE = 2;
    public static final int SPATIAL_OFF = 0;
    public static final int SPATIAL_ON = 1;
    private static final int SPATIAL_DEFAULT = SPATIAL_ON;

    public static final int ALTSCREEN_OFF = 0;
    public static final int ALTSCREEN_ON = 1;
    public static final int ALTSCREEN_DEFAULT = ALTSCREEN_ON;

    public static final int ALT_URL_MAP_WHEN_RGI = 0;
    public static final int ALT_URL_CARDS_ALWAYS = 1;
    public static final int ALT_URL_DEFAULT = ALT_URL_MAP_WHEN_RGI;

    public static final int ALT_LAYOUT_LEFT = 0;
    public static final int ALT_LAYOUT_CENTER = 1;
    public static final int ALT_LAYOUT_RIGHT = 2;
    public static final int ALT_LAYOUT_DEFAULT = ALT_LAYOUT_RIGHT;

    public static final int RGI_ENABLED_OFF = 0;
    public static final int RGI_ENABLED_ON = 1;
    public static final int RGI_ENABLED_DEFAULT = RGI_ENABLED_ON;

    public static final int RGI_FAR_NEXT = 0;
    public static final int RGI_FAR_STRAIGHT = 1;
    public static final int RGI_FAR_DEFAULT = RGI_FAR_NEXT;

    public static final String SETTINGS_FILE = "/tmp/carplay_settings"; // hook/audio/audio_hook.h (SPA_SETTINGS_FILE)

    public static final int MODEL_SPATIAL     = 2500500;       // module 25, index 500
    public static final int MODEL_UPMIX       = 2500501;       // module 25, index 501
    public static final int MODEL_ALTSCREEN   = 2500502;       // module 25, index 502
    public static final int MODEL_ALT_URL     = 2500503;       // module 25, index 503
    public static final int MODEL_ALT_LAYOUT  = 2500504;       // module 25, index 504
    public static final int MODEL_RGI_ENABLED = 2500505;       // module 25, index 505
    public static final int MODEL_RGI_FAR     = 2500506;       // module 25, index 506

    private static final int INDEX_SPATIAL     = MODEL_SPATIAL % 100000;
    private static final int INDEX_UPMIX       = MODEL_UPMIX % 100000;
    private static final int INDEX_ALTSCREEN   = MODEL_ALTSCREEN % 100000;
    private static final int INDEX_ALT_URL     = MODEL_ALT_URL % 100000;
    private static final int INDEX_ALT_LAYOUT  = MODEL_ALT_LAYOUT % 100000;
    private static final int INDEX_RGI_ENABLED = MODEL_RGI_ENABLED % 100000;
    private static final int INDEX_RGI_FAR     = MODEL_RGI_FAR % 100000;

    private static final int DRAWER_CM_OPT        = 2500001;
    private static final int SCREEN_ABOUT_CARPLAY = 2500140;
    private static final int EVENT_ABOUT_CARPLAY  = 2500228; // SMEventConnectivity.RD_CM_OPT_ABOUT_CAR_PLAY
    private static final int EVENT_WLAN_SETTINGS  = 2500073; // SMEventConnectivity.RD_CM_OPT_WLAN_SETTINGS

    private static final int INTERNAL_ID_OPT_CARPLAY = 2500271;
    private static final int INTERNAL_ID_SPATIAL     = 2509910;
    private static final int INTERNAL_ID_UPMIX       = 2509911;
    private static final int INTERNAL_ID_ALTSCREEN   = 2509912;
    private static final int INTERNAL_ID_ALT_LAYOUT  = 2509913;
    private static final int INTERNAL_ID_RGI_ENABLED = 2509914;
    private static final int INTERNAL_ID_ALT_URL     = 2509915;
    private static final int INTERNAL_ID_RGI_FAR     = 2509916;

    // HMIConnectivityEvoHigh string table slots 1500..1524 (text ids 2501500..2501524)
    public static final int TEXT_TITLE                  = 2501500;
    public static final int TEXT_SPATIAL                = 2501501;
    public static final int TEXT_SPATIAL_HINT           = 2501502;
    public static final int TEXT_UPMIX                  = 2501503;
    public static final int TEXT_UPMIX_OFF              = 2501504;
    public static final int TEXT_UPMIX_NORMAL           = 2501505;
    public static final int TEXT_UPMIX_INTENSE          = 2501506;
    public static final int TEXT_UPMIX_HINT             = 2501507;
    public static final int TEXT_ALTSCREEN              = 2501508;
    public static final int TEXT_ALTSCREEN_HINT         = 2501509;
    public static final int TEXT_ALT_LAYOUT             = 2501510;
    public static final int TEXT_ALT_LAYOUT_LEFT        = 2501511;
    public static final int TEXT_ALT_LAYOUT_CENTER      = 2501512;
    public static final int TEXT_ALT_LAYOUT_RIGHT       = 2501513;
    public static final int TEXT_ALT_LAYOUT_HINT        = 2501514;
    public static final int TEXT_RGI_ENABLED            = 2501515;
    public static final int TEXT_RGI_ENABLED_HINT       = 2501516;
    public static final int TEXT_ALT_URL                = 2501517;
    public static final int TEXT_ALT_URL_MAP_WHEN_RGI   = 2501518;
    public static final int TEXT_ALT_URL_CARDS_ALWAYS   = 2501519;
    public static final int TEXT_ALT_URL_HINT           = 2501520;
    public static final int TEXT_RGI_FAR                = 2501521;
    public static final int TEXT_RGI_FAR_NEXT           = 2501522;
    public static final int TEXT_RGI_FAR_STRAIGHT       = 2501523;
    public static final int TEXT_RGI_FAR_HINT           = 2501524;

    private static final String[] FALLBACK_TEXTS = new String[]{
        "CarPlay settings",
        "Spatial Audio",
        "Play multichannel 5.1 audio in supported apps",
        "UpMix",
        "Off",
        "Normal",
        "Intense",
        "Expand stereo audio into 5.1 surround sound",
        "Maps on Cluster",
        "Show CarPlay maps on the instrument cluster",
        "Maneuver Cards Position",
        "Left",
        "Center",
        "Right",
        "Choose where turn cards and arrival time appear on the map",
        "Route Guidance",
        "Show turn-by-turn directions on the cluster and Head-Up Display",
        "Map Cards during Guidance",
        "Hidden",
        "Shown",
        "Hide CarPlay turn cards on the map when Route Guidance is active",
        "Guidance Maneuver Preview",
        "Always next maneuver",
        "Follow road when far",
        "Show the next turn in advance, or a straight arrow until you get closer"
    };

    private static final int[] SUBMENU_BITMAPS = new int[]{39, 39, 39, 39};
    private static final int[] COMBO_BITMAPS = createComboBitmaps();

    private static volatile IFrameworkAccess fw;
    private static volatile int amplifier = -1;
    private static volatile ChoiceModel spatialModel;
    private static volatile ChoiceModel upmixModel;
    private static volatile ChoiceModel altScreenModel;
    private static volatile ChoiceModel altUrlModel;
    private static volatile ChoiceModel altLayoutModel;
    private static volatile ChoiceModel rgiEnabledModel;
    private static volatile ChoiceModel rgiFarModel;

    private static volatile MenuController settingsMenu;
    private static volatile MenuItemController spatialMenuItem;
    private static volatile MenuItemController upmixMenuItem;
    private static volatile MenuItemController altLayoutMenuItem;
    private static volatile MenuItemController altUrlMenuItem;
    private static volatile MenuItemController rgiFarMenuItem;

    private static volatile int pendingSpatial    = SPATIAL_DEFAULT;
    private static volatile int pendingUpmixChoice = CHOICE_NORMAL;
    private static volatile int pendingAltScreen  = ALTSCREEN_DEFAULT;
    private static volatile int pendingAltUrl     = ALT_URL_DEFAULT;
    private static volatile int pendingAltLayout  = ALT_LAYOUT_DEFAULT;
    private static volatile int pendingRgiEnabled = RGI_ENABLED_DEFAULT;
    private static volatile int pendingRgiFar     = RGI_FAR_DEFAULT;

    private static volatile int currentSpatial    = SPATIAL_DEFAULT;
    private static volatile int currentLevel      = LEVEL_NORMAL;
    private static volatile int currentAltScreen  = ALTSCREEN_DEFAULT;
    private static volatile int currentAltUrl     = ALT_URL_DEFAULT;
    private static volatile int currentAltLayout  = ALT_LAYOUT_DEFAULT;
    private static volatile int currentRgiEnabled = RGI_ENABLED_DEFAULT;
    private static volatile int currentRgiFar     = RGI_FAR_DEFAULT;

    private CarPlaySettingsUi() {}

    /** Map 3-state hook level (0..2) to 3-choice UI index (0=Off, 1=Normal, 2=Intense). */
    public static int levelToChoice(int level) {
        if (level <= LEVEL_OFF) return CHOICE_OFF;
        if (level >= LEVEL_INTENSE) return CHOICE_INTENSE;
        return CHOICE_NORMAL;
    }

    /** Map 3-choice UI index (0=Off, 1=Normal, 2=Intense) to 3-state hook level (0, 1, 2). */
    public static int choiceToLevel(int choice) {
        if (choice <= CHOICE_OFF) return LEVEL_OFF;
        if (choice >= CHOICE_INTENSE) return LEVEL_INTENSE;
        return LEVEL_NORMAL;
    }

    /** BASIC_2CH, BASIC_4CH, STANDARD_6CH: the head unit's internal amplifier, stereo route only. */
    public static boolean internalAmp(int type) {
        return type == 1 || type == 2 || type == 3;
    }

    /** Spatial Audio and UpMix apply only on external MOST amplifiers. */
    public static boolean available() {
        return !internalAmp(amplifier);
    }

    public static boolean isAltScreenEnabled() {
        return currentAltScreen == ALTSCREEN_ON;
    }

    public static int getAltUrlMode() {
        return currentAltUrl;
    }

    public static int getAltLayout() {
        return currentAltLayout;
    }

    public static boolean isRgiEnabled() {
        return currentRgiEnabled == RGI_ENABLED_ON;
    }

    public static boolean isRgiFarStraight() {
        return currentRgiFar == RGI_FAR_STRAIGHT;
    }

    /** Send active AltScreen enable, URL policy, and card placement to the C hook. */
    public static boolean sendAltConfigToHook() {
        byte[] payload = new byte[]{
            (byte) currentAltScreen,
            (byte) currentAltUrl,
            (byte) currentAltLayout
        };
        return CarplayBus.getInstance().sendBinary(CarplayBus.CMD_ALT_CFG, payload);
    }

    /** HMI boot (CarPlayApp.startTransport): stored settings -> /tmp files and UI models. Never throws. */
    public static void boot(IFrameworkAccess framework) {
        try {
            fw = framework;
            try {
                FlushValuesDict.register(NAMESPACE, KEY_UPMIX);
                FlushValuesDict.register(NAMESPACE, KEY_SPATIAL);
                FlushValuesDict.register(NAMESPACE, KEY_ALTSCREEN);
                FlushValuesDict.register(NAMESPACE, KEY_ALT_URL);
                FlushValuesDict.register(NAMESPACE, KEY_ALT_LAYOUT);
                FlushValuesDict.register(NAMESPACE, KEY_RGI_ENABLED);
                FlushValuesDict.register(NAMESPACE, KEY_RGI_FAR);
            } catch (Throwable t) {
                Log.w(TAG, "flush key not registered: " + t);
            }
            try {
                framework.getBundleCxt().registerService(AmplifierListener.class.getName(), new AmplifierListener() {
                    public void updateAmplifier(int type) {
                        amplifier = type;
                        SoundControlUi.updateAmplifier(type);
                        syncAudioRowsVisibility();
                        Log.i(TAG, "amplifier " + type + (internalAmp(type) ? " (head unit amp: audio rows hidden)" : ""));
                    }
                }, null);
            } catch (Throwable t) {
                Log.w(TAG, "amplifier not watched, CarPlay settings always shown: " + t);
            }
            currentLevel = read();
            currentSpatial = readSpatial();
            currentAltScreen = readBounded(KEY_ALTSCREEN, ALTSCREEN_DEFAULT, 0, 1);
            currentAltUrl = readBounded(KEY_ALT_URL, ALT_URL_DEFAULT, 0, 1);
            currentAltLayout = readBounded(KEY_ALT_LAYOUT, ALT_LAYOUT_DEFAULT, 0, 2);
            currentRgiEnabled = readBounded(KEY_RGI_ENABLED, RGI_ENABLED_DEFAULT, 0, 1);
            currentRgiFar = readBounded(KEY_RGI_FAR, RGI_FAR_DEFAULT, 0, 1);
            writeSettingsFile();
            show(currentSpatial, levelToChoice(currentLevel));
            showAltScreen(currentAltScreen);
            showAltUrl(currentAltUrl);
            showAltLayout(currentAltLayout);
            showRgiEnabled(currentRgiEnabled);
            showRgiFar(currentRgiFar);
            syncMenuEnabledStates();
            sendAltConfigToHook();
            Log.i(TAG, "boot level=" + currentLevel + " (choice=" + levelToChoice(currentLevel)
                + "), spatial=" + currentSpatial + ", altscreen=" + currentAltScreen
                + ", alt_url=" + currentAltUrl + ", alt_layout=" + currentAltLayout
                + ", rgi=" + currentRgiEnabled + ", rgi_far=" + currentRgiFar);
        } catch (Throwable t) {
            Log.w(TAG, "boot failed: " + t);
        }
    }

    /** The user picked a 3-choice option (0=Off, 1=Normal, 2=Intense) in the UpMix combo box. */
    public static void selectChoice(int choice) {
        select(choiceToLevel(choice));
    }

    /** The user picked a level (0..2) for UpMix (HMI thread). */
    public static void select(int level) {
        if (level < 0 || level >= COUNT) return;
        currentLevel = level;
        storeKey(KEY_UPMIX, level);
        writeSettingsFile();
        boolean sent = CarplayBus.getInstance().sendBinary(CarplayBus.CMD_UPMIX, new byte[]{(byte) level});
        Log.i(TAG, "selected upmix " + level + (sent ? "" : " (bus down: the hook takes it from the file at its next start)"));
    }

    /** The user toggled Spatial Audio (0=Off, 1=On) in the CarPlay settings checkbox (HMI thread). */
    public static void selectSpatial(int enable) {
        if (enable != SPATIAL_OFF && enable != SPATIAL_ON) return;
        currentSpatial = enable;
        storeKey(KEY_SPATIAL, enable);
        writeSettingsFile();
        boolean sent = CarplayBus.getInstance().sendBinary(CarplayBus.CMD_SPATIAL, new byte[]{(byte) enable});
        Log.i(TAG, "selected spatial " + enable + (sent ? "" : " (bus down: the hook takes it from the file at its next start)"));
    }

    /** The user toggled CarPlay on Cluster (0=Off, 1=On). */
    public static void selectAltScreen(int enable) {
        if (enable != ALTSCREEN_OFF && enable != ALTSCREEN_ON) return;
        currentAltScreen = enable;
        storeKey(KEY_ALTSCREEN, enable);
        writeSettingsFile();
        syncMenuEnabledStates();
        boolean sent = sendAltConfigToHook();
        AltScreenModule.onSettingsChanged();
        Log.i(TAG, "selected altscreen " + enable + (sent ? "" : " (bus down)"));
    }

    /** The user picked CarPlay Map Cards policy (0=Hide when guiding, 1=Always show). */
    public static void selectAltUrl(int mode) {
        if (mode != ALT_URL_MAP_WHEN_RGI && mode != ALT_URL_CARDS_ALWAYS) return;
        currentAltUrl = mode;
        storeKey(KEY_ALT_URL, mode);
        writeSettingsFile();
        boolean sent = sendAltConfigToHook();
        Log.i(TAG, "selected alt_url " + mode + (sent ? "" : " (bus down)"));
    }

    /** The user picked Position (0=Left, 1=Center, 2=Right). */
    public static void selectAltLayout(int layout) {
        if (layout < ALT_LAYOUT_LEFT || layout > ALT_LAYOUT_RIGHT) return;
        currentAltLayout = layout;
        storeKey(KEY_ALT_LAYOUT, layout);
        writeSettingsFile();
        boolean sent = sendAltConfigToHook();
        Log.i(TAG, "selected alt_layout " + layout + (sent ? "" : " (bus down)"));
    }

    /** The user toggled Route Guidance (0=Off, 1=On), unlocking/locking the RGI sub-settings. */
    public static void selectRgiEnabled(int enable) {
        if (enable != RGI_ENABLED_OFF && enable != RGI_ENABLED_ON) return;
        currentRgiEnabled = enable;
        storeKey(KEY_RGI_ENABLED, enable);
        writeSettingsFile();
        syncMenuEnabledStates();
        RouteGuidance.onRgiSettingsChanged();
        AltScreenModule.onSettingsChanged();
        Log.i(TAG, "selected rgi " + enable);
    }

    /** The user picked Guidance Maneuver Preview (0=Always next maneuver, 1=Follow road when far). */
    public static void selectRgiFar(int mode) {
        if (mode != RGI_FAR_NEXT && mode != RGI_FAR_STRAIGHT) return;
        currentRgiFar = mode;
        storeKey(KEY_RGI_FAR, mode);
        writeSettingsFile();
        RouteGuidance.onRgiSettingsChanged();
        Log.i(TAG, "selected rgi_far " + mode);
    }

    static void syncMenuEnabledStates() {
        try {
            boolean clusterOn = (currentAltScreen == ALTSCREEN_ON);
            boolean rgiOn = (currentRgiEnabled == RGI_ENABLED_ON);
            MenuItemController layoutItem = altLayoutMenuItem;
            if (layoutItem != null) layoutItem.setEnabled(clusterOn);
            MenuItemController urlItem = altUrlMenuItem;
            if (urlItem != null) urlItem.setEnabled(clusterOn && rgiOn);
            MenuItemController farItem = rgiFarMenuItem;
            if (farItem != null) farItem.setEnabled(rgiOn);
        } catch (Throwable t) { /* ignore */ }
    }

    static void syncAudioRowsVisibility() {
        try {
            if (!available()) {
                MenuController m = settingsMenu;
                if (m != null) {
                    if (spatialMenuItem != null) m.remove(spatialMenuItem);
                    if (upmixMenuItem != null) m.remove(upmixMenuItem);
                }
            }
        } catch (Throwable t) { /* ignore */ }
    }

    private static void storeKey(int key, int val) {
        try {
            IFrameworkAccess f = fw;
            if (f != null) f.getStorageMgr().setInt(NAMESPACE, key, val);
            else Log.w(TAG, "key " + key + " not stored: no framework yet");
        } catch (Throwable t) {
            Log.w(TAG, "store key " + key + " failed: " + t);
        }
    }

    private static int read() {
        return readBounded(KEY_UPMIX, NORMAL, 0, COUNT - 1);
    }

    private static int readSpatial() {
        return readBounded(KEY_SPATIAL, SPATIAL_DEFAULT, 0, 1);
    }

    private static int readBounded(int key, int def, int min, int max) {
        try {
            IFrameworkAccess f = fw;
            IStorageAccess s = f != null ? f.getStorageMgr() : null;
            int v = s != null ? s.getInt(NAMESPACE, key, def) : def;
            return (v >= min && v <= max) ? v : def;
        } catch (Throwable t) {
            return def;
        }
    }

    /** Format /tmp/carplay_settings as PPS-style key=value lines. */
    public static String formatSettings(int spatial, int upmix, int altscreen, int altUrl,
                                        int altLayout, int rgiEnabled, int rgiFar) {
        return "spatial=" + spatial
             + "\nupmix=" + upmix
             + "\naltscreen=" + altscreen
             + "\nalt_url=" + altUrl
             + "\nalt_layout=" + altLayout
             + "\nrgi=" + rgiEnabled
             + "\nrgi_far=" + rgiFar + "\n";
    }

    public static String formatSettings(int spatial, int upmix) {
        return "spatial=" + spatial + "\nupmix=" + upmix + "\n";
    }

    private static void writeSettingsFile() {
        FileOutputStream out = null;
        try {
            out = new FileOutputStream(SETTINGS_FILE);
            String content = formatSettings(currentSpatial, currentLevel, currentAltScreen,
                currentAltUrl, currentAltLayout, currentRgiEnabled, currentRgiFar);
            out.write(content.getBytes("UTF-8"));
        } catch (Throwable t) {
            Log.w(TAG, "cannot write " + SETTINGS_FILE + ": " + t);
        } finally {
            try { if (out != null) out.close(); } catch (Throwable t) { /* nothing to do */ }
        }
    }

    private static int[] createComboBitmaps() {
        int[] b = new int[44];
        for (int i = 0; i < 42; i++) b[i] = 127;
        b[42] = 129;
        b[43] = 332;
        return b;
    }

    public static ConnectivityModelBank newBank() { return new Bank(); }

    public static ConnectivityScreenFactory newFactory(IFrameworkAccess fw) { return new Factory(fw); }

    /** Reflect active Spatial Audio (0/1) and UpMix choice (0..2) in the UI models. */
    public static void show(int spatial, int upmixChoice) {
        showSpatial(spatial);
        showUpmixChoice(upmixChoice);
    }

    public static void showSpatial(int spatial) {
        try {
            int v = spatial != 0 ? 1 : 0;
            pendingSpatial = v;
            ChoiceModel m = spatialModel;
            if (m != null) m.setValue(v);
        } catch (Throwable t) { /* UI mirror only */ }
    }

    public static void showUpmixChoice(int choice) {
        try {
            int c = (choice >= 0 && choice <= 2) ? choice : 1;
            pendingUpmixChoice = c;
            ChoiceModel m = upmixModel;
            if (m != null) m.setValue(c);
        } catch (Throwable t) { /* UI mirror only */ }
    }

    public static void showAltScreen(int enable) {
        try {
            int v = enable != 0 ? 1 : 0;
            pendingAltScreen = v;
            ChoiceModel m = altScreenModel;
            if (m != null) m.setValue(v);
        } catch (Throwable t) { /* UI mirror only */ }
    }

    public static void showAltUrl(int mode) {
        try {
            int v = mode != 0 ? 1 : 0;
            pendingAltUrl = v;
            ChoiceModel m = altUrlModel;
            if (m != null) m.setValue(v);
        } catch (Throwable t) { /* UI mirror only */ }
    }

    public static void showAltLayout(int layout) {
        try {
            int v = (layout >= 0 && layout <= 2) ? layout : ALT_LAYOUT_DEFAULT;
            pendingAltLayout = v;
            ChoiceModel m = altLayoutModel;
            if (m != null) m.setValue(v);
        } catch (Throwable t) { /* UI mirror only */ }
    }

    public static void showRgiEnabled(int enable) {
        try {
            int v = enable != 0 ? 1 : 0;
            pendingRgiEnabled = v;
            ChoiceModel m = rgiEnabledModel;
            if (m != null) m.setValue(v);
        } catch (Throwable t) { /* UI mirror only */ }
    }

    public static void showRgiFar(int mode) {
        try {
            int v = mode != 0 ? 1 : 0;
            pendingRgiFar = v;
            ChoiceModel m = rgiFarModel;
            if (m != null) m.setValue(v);
        } catch (Throwable t) { /* UI mirror only */ }
    }

    static final class Bank extends ConnectivityModelBank {
        private static final int[] EXTRA_MODELS = new int[]{
            MODEL_SPATIAL, MODEL_UPMIX, MODEL_ALTSCREEN, MODEL_ALT_URL,
            MODEL_ALT_LAYOUT, MODEL_RGI_ENABLED, MODEL_RGI_FAR
        };

        Bank() {
            int need = INDEX_RGI_FAR + 1;
            if (this.models.length < need) {
                HMIModel[] grown = new HMIModel[need];
                System.arraycopy(this.models, 0, grown, 0, this.models.length);
                this.models = grown;
            }
            int origLen = this.modelIDs.length;
            int[] ids = new int[origLen + EXTRA_MODELS.length];
            System.arraycopy(this.modelIDs, 0, ids, 0, origLen);
            for (int k = 0; k < EXTRA_MODELS.length; k++) {
                ids[origLen + k] = EXTRA_MODELS[k];
            }
            this.modelIDs = ids;
        }

        protected synchronized void createModel(int i) {
            if (i == INDEX_SPATIAL) {
                if (this.models[i] != null) return;
                final ChoiceModel m = new ChoiceModel(MODEL_SPATIAL);
                m.setChoiceListener(new DefaultChoiceListener() {
                    public void itemSelected(int id, int value, int unused, int terminal) {
                        try {
                            int v = value != 0 ? 1 : 0;
                            m.setValue(v);
                            selectSpatial(v);
                        } catch (Throwable t) { /* never into the HMI thread */ }
                    }
                });
                m.setValue(pendingSpatial);
                this.models[i] = m;
                spatialModel = m;
                return;
            }
            if (i == INDEX_UPMIX) {
                if (this.models[i] != null) return;
                final ChoiceModel m = new ChoiceModel(MODEL_UPMIX);
                m.setChoiceListener(new DefaultChoiceListener() {
                    public void itemSelected(int id, int value, int unused, int terminal) {
                        try {
                            m.setValue(value);
                            selectChoice(value);
                        } catch (Throwable t) { /* never into the HMI thread */ }
                    }
                });
                m.setValue(pendingUpmixChoice);
                this.models[i] = m;
                upmixModel = m;
                return;
            }
            if (i == INDEX_ALTSCREEN) {
                if (this.models[i] != null) return;
                final ChoiceModel m = new ChoiceModel(MODEL_ALTSCREEN);
                m.setChoiceListener(new DefaultChoiceListener() {
                    public void itemSelected(int id, int value, int unused, int terminal) {
                        try {
                            int v = value != 0 ? 1 : 0;
                            m.setValue(v);
                            selectAltScreen(v);
                        } catch (Throwable t) { /* never into the HMI thread */ }
                    }
                });
                m.setValue(pendingAltScreen);
                this.models[i] = m;
                altScreenModel = m;
                return;
            }
            if (i == INDEX_ALT_URL) {
                if (this.models[i] != null) return;
                final ChoiceModel m = new ChoiceModel(MODEL_ALT_URL);
                m.setChoiceListener(new DefaultChoiceListener() {
                    public void itemSelected(int id, int value, int unused, int terminal) {
                        try {
                            int v = value != 0 ? 1 : 0;
                            m.setValue(v);
                            selectAltUrl(v);
                        } catch (Throwable t) { /* never into the HMI thread */ }
                    }
                });
                m.setValue(pendingAltUrl);
                this.models[i] = m;
                altUrlModel = m;
                return;
            }
            if (i == INDEX_ALT_LAYOUT) {
                if (this.models[i] != null) return;
                final ChoiceModel m = new ChoiceModel(MODEL_ALT_LAYOUT);
                m.setChoiceListener(new DefaultChoiceListener() {
                    public void itemSelected(int id, int value, int unused, int terminal) {
                        try {
                            int v = (value >= 0 && value <= 2) ? value : ALT_LAYOUT_DEFAULT;
                            m.setValue(v);
                            selectAltLayout(v);
                        } catch (Throwable t) { /* never into the HMI thread */ }
                    }
                });
                m.setValue(pendingAltLayout);
                this.models[i] = m;
                altLayoutModel = m;
                return;
            }
            if (i == INDEX_RGI_ENABLED) {
                if (this.models[i] != null) return;
                final ChoiceModel m = new ChoiceModel(MODEL_RGI_ENABLED);
                m.setChoiceListener(new DefaultChoiceListener() {
                    public void itemSelected(int id, int value, int unused, int terminal) {
                        try {
                            int v = value != 0 ? 1 : 0;
                            m.setValue(v);
                            selectRgiEnabled(v);
                        } catch (Throwable t) { /* never into the HMI thread */ }
                    }
                });
                m.setValue(pendingRgiEnabled);
                this.models[i] = m;
                rgiEnabledModel = m;
                return;
            }
            if (i == INDEX_RGI_FAR) {
                if (this.models[i] != null) return;
                final ChoiceModel m = new ChoiceModel(MODEL_RGI_FAR);
                m.setChoiceListener(new DefaultChoiceListener() {
                    public void itemSelected(int id, int value, int unused, int terminal) {
                        try {
                            int v = value != 0 ? 1 : 0;
                            m.setValue(v);
                            selectRgiFar(v);
                        } catch (Throwable t) { /* never into the HMI thread */ }
                    }
                });
                m.setValue(pendingRgiFar);
                this.models[i] = m;
                rgiFarModel = m;
                return;
            }
            super.createModel(i);
        }
    }

    static final class Factory extends ConnectivityScreenFactory {
        Factory(IFrameworkAccess fw) { super(fw); }

        public IDrawerController[] getOptionDrawers(int terminal) {
            IDrawerController[] drawers = super.getOptionDrawers(terminal);
            if (drawers != null) {
                for (int i = 0; i < drawers.length; i++) {
                    if (drawers[i] instanceof DrawerController
                            && ((DrawerController) drawers[i]).getID() == DRAWER_CM_OPT) {
                        try { patchOptionDrawer((DrawerController) drawers[i], terminal); }
                        catch (Throwable t) { /* leave stock drawer on error */ }
                    }
                }
            }
            return drawers;
        }

        protected Screen createScreen(int id, int terminal) {
            Screen s = super.createScreen(id, terminal);
            if (id == SCREEN_ABOUT_CARPLAY) {
                try { buildCarPlaySettingsScreen(s, terminal); }
                catch (Throwable t) { /* leave stock screen on error */ }
            }
            return s;
        }

        public String getText(int id) {
            String s = super.getText(id);
            if ((s == null || s.length() == 0) && id >= TEXT_TITLE && id <= TEXT_RGI_FAR_HINT) {
                return FALLBACK_TEXTS[id - TEXT_TITLE];
            }
            return s;
        }

        public int getTextStatus(int id) {
            int st = super.getTextStatus(id);
            if (st == 0 && id >= TEXT_TITLE && id <= TEXT_RGI_FAR_HINT) {
                return 8;
            }
            return st;
        }

        private void patchOptionDrawer(DrawerController drawer, int terminal) {
            Object main = drawer.getDrawerMain();
            if (!(main instanceof AbstractWidgetController)) return;
            AbstractWidgetController mainCtrl = (AbstractWidgetController) main;
            if (mainCtrl.getChildrenSize() == 0) return;
            AbstractWidget first = mainCtrl.getChild(0);
            if (!(first instanceof MenuController)) return;
            MenuController menu = (MenuController) first;

            MenuItemController oldCarPlayItem = null;
            MenuItemController refSubmenuItem = null;
            List children = menu.getChildren();
            for (int i = 0; i < children.size(); i++) {
                Object child = children.get(i);
                if (child instanceof MenuItemController) {
                    MenuItemController mi = (MenuItemController) child;
                    if (mi.getEvent() == EVENT_ABOUT_CARPLAY) {
                        oldCarPlayItem = mi;
                    } else if (mi.getEvent() == EVENT_WLAN_SETTINGS) {
                        refSubmenuItem = mi;
                    }
                }
            }

            MenuItemController item = new MenuItemController();
            CompositeRendererHigh r = new CompositeRendererHigh(item);
            item.setRenderer(r);
            item.setBitmaps(refSubmenuItem != null && refSubmenuItem.getBitmapIndices() != null
                    ? refSubmenuItem.getBitmapIndices() : SUBMENU_BITMAPS);
            r.setFonts(refSubmenuItem != null && refSubmenuItem.getRenderer() instanceof AbstractRendererHigh
                    && ((AbstractRendererHigh) refSubmenuItem.getRenderer()).getFonts() != null
                    ? ((AbstractRendererHigh) refSubmenuItem.getRenderer()).getFonts()
                    : FontFactory.getFonts(0, terminal, this.getFramework()));
            item.setEvent(EVENT_ABOUT_CARPLAY);
            item.setLabelId(TEXT_TITLE);
            item.setGlassplateInsetsBottom(6);
            item.setGlassplateInsetsTop(7);
            item.setInternalID(INTERNAL_ID_OPT_CARPLAY);
            item.setType(MenuItemController.TYPE_SUBMENU);
            item.setWidgetID(1);

            if (refSubmenuItem != null) {
                int idx = menu.getChildren().indexOf(refSubmenuItem);
                menu.addWithPosition(item, idx + 1);
            } else {
                menu.add(item);
            }
            if (oldCarPlayItem != null) {
                menu.remove(oldCarPlayItem);
            }

            PropertyObject[] props = drawer.getPropertyObjects();
            if (props != null) {
                PropertyObject newProp = new PropertyObject(
                    item,
                    1,
                    -2,
                    -2,
                    new int[]{IDrawerContextConnectivity.CONTEXT_CONNECTIVITY_CONNECTIVITY},
                    null,
                    null,
                    null,
                    null
                );
                boolean replaced = false;
                PropertyObject[] updated = new PropertyObject[props.length];
                for (int i = 0; i < props.length; i++) {
                    if (oldCarPlayItem != null && props[i] != null && props[i].getWidget() == oldCarPlayItem) {
                        updated[i] = newProp;
                        replaced = true;
                    } else {
                        updated[i] = props[i];
                    }
                }
                if (!replaced) {
                    PropertyObject[] grown = new PropertyObject[props.length + 1];
                    System.arraycopy(updated, 0, grown, 0, updated.length);
                    grown[grown.length - 1] = newProp;
                    updated = grown;
                }
                drawer.setPropertyObjects(updated);
            }
        }

        private MenuItemController createCheckboxItem(int terminal, int modelId, int labelId,
                                                      int hintId, int internalId) {
            MenuItemController item = new MenuItemController();
            CompositeRendererHigh r = new CompositeRendererHigh(item);
            item.setRenderer(r);
            r.setFonts(FontFactory.getFonts(0, terminal, this.getFramework()));
            item.setModelID(modelId);
            item.setMapping(new int[][]{{0}, {1}});
            item.setLabelId(labelId);
            item.setInfolineTextIdEnabled(hintId);
            item.setHasInfolineDisclaimer(false);
            item.setGlassplateInsetsBottom(6);
            item.setGlassplateInsetsTop(7);
            item.setInternalID(internalId);
            item.setType(MenuItemController.TYPE_CHECKBOX);
            item.setWidgetID(1);
            return item;
        }

        private MenuItemController createComboItem(int terminal, int modelId, int labelId,
                                                   int[] choiceTextIds, int hintId, int internalId) {
            MenuItemController item = new MenuItemController();
            CompositeRendererHigh r = new CompositeRendererHigh(item);
            item.setRenderer(r);
            item.setBitmaps(COMBO_BITMAPS);
            r.setFonts(FontFactory.getFonts(3, terminal, this.getFramework()));
            item.setModelID(modelId);
            item.setLabelId(labelId);
            item.setTextIds(choiceTextIds);
            item.setInfolineTextIdEnabled(hintId);
            item.setHasInfolineDisclaimer(false);
            item.setGlassplateInsetsBottom(6);
            item.setGlassplateInsetsTop(7);
            item.setInternalID(internalId);
            item.setType(MenuItemController.TYPE_COMBO_BOX);
            item.setWidgetID(1);
            return item;
        }

        private void buildCarPlaySettingsScreen(Screen s, int terminal) {
            AbstractScreenWidget screen = (AbstractScreenWidget) s;
            updateScreenTitle(screen, TEXT_TITLE);

            Object mainArea = screen.getMainArea();
            if (!(mainArea instanceof AbstractWidgetController)) return;
            AbstractWidgetController mainCtrl = (AbstractWidgetController) mainArea;
            MenuController menu = null;
            for (int i = 0; i < mainCtrl.getChildrenSize(); i++) {
                if (mainCtrl.getChild(i) instanceof MenuController) {
                    menu = (MenuController) mainCtrl.getChild(i);
                    break;
                }
            }
            if (menu == null) return;
            settingsMenu = menu;

            // Remove stock static "About CarPlay" multiline text item
            List children = menu.getChildren();
            for (int i = children.size() - 1; i >= 0; i--) {
                Object c = children.get(i);
                if (c instanceof MultiLineMenuItemController || c instanceof MenuItemController) {
                    menu.remove((AbstractWidget) c);
                }
            }

            InfolineTimerController timer = menu.getInfolineTimer();
            if (timer != null) {
                timer.setStartTimerAutomatically(true);
            }
            InfolineController infoline = menu.getInfoline();
            if (infoline != null && infoline.getInfolineRenderer() != null) {
                infoline.getInfolineRenderer().setAutoWrap(StringUtility.WRAP_MODE_STANDARD);
            }

            int pos = 0;

            if (available()) {
                // Row 1: Spatial Audio (checkbox, external MOST amp only)
                MenuItemController spatialItem = createCheckboxItem(terminal, MODEL_SPATIAL,
                    TEXT_SPATIAL, TEXT_SPATIAL_HINT, INTERNAL_ID_SPATIAL);
                registerScreenView(screen, MODEL_SPATIAL, spatialItem);
                menu.addWithPosition(spatialItem, pos++);
                spatialMenuItem = spatialItem;

                // Row 2: UpMix (Off | Normal | Intense, external MOST amp only)
                MenuItemController upmixItem = createComboItem(terminal, MODEL_UPMIX,
                    TEXT_UPMIX, new int[]{TEXT_UPMIX_OFF, TEXT_UPMIX_NORMAL, TEXT_UPMIX_INTENSE},
                    TEXT_UPMIX_HINT, INTERNAL_ID_UPMIX);
                registerScreenView(screen, MODEL_UPMIX, upmixItem);
                menu.addWithPosition(upmixItem, pos++);
                upmixMenuItem = upmixItem;
            } else {
                spatialMenuItem = null;
                upmixMenuItem = null;
            }

            // Row 3: Maps on Cluster (checkbox)
            MenuItemController altScreenItem = createCheckboxItem(terminal, MODEL_ALTSCREEN,
                TEXT_ALTSCREEN, TEXT_ALTSCREEN_HINT, INTERNAL_ID_ALTSCREEN);
            registerScreenView(screen, MODEL_ALTSCREEN, altScreenItem);
            menu.addWithPosition(altScreenItem, pos++);

            // Row 4: Maneuver Cards Position (Left | Center | Right)
            MenuItemController altLayoutItem = createComboItem(terminal, MODEL_ALT_LAYOUT,
                TEXT_ALT_LAYOUT, new int[]{TEXT_ALT_LAYOUT_LEFT, TEXT_ALT_LAYOUT_CENTER, TEXT_ALT_LAYOUT_RIGHT},
                TEXT_ALT_LAYOUT_HINT, INTERNAL_ID_ALT_LAYOUT);
            registerScreenView(screen, MODEL_ALT_LAYOUT, altLayoutItem);
            menu.addWithPosition(altLayoutItem, pos++);
            altLayoutMenuItem = altLayoutItem;

            // Row 5: Route Guidance (checkbox — unlocks rows 6..7)
            MenuItemController rgiEnabledItem = createCheckboxItem(terminal, MODEL_RGI_ENABLED,
                TEXT_RGI_ENABLED, TEXT_RGI_ENABLED_HINT, INTERNAL_ID_RGI_ENABLED);
            registerScreenView(screen, MODEL_RGI_ENABLED, rgiEnabledItem);
            menu.addWithPosition(rgiEnabledItem, pos++);

            // Row 6: Map Cards during Guidance (Hidden | Shown)
            MenuItemController altUrlItem = createComboItem(terminal, MODEL_ALT_URL,
                TEXT_ALT_URL, new int[]{TEXT_ALT_URL_MAP_WHEN_RGI, TEXT_ALT_URL_CARDS_ALWAYS},
                TEXT_ALT_URL_HINT, INTERNAL_ID_ALT_URL);
            registerScreenView(screen, MODEL_ALT_URL, altUrlItem);
            menu.addWithPosition(altUrlItem, pos++);
            altUrlMenuItem = altUrlItem;

            // Row 7: Guidance Maneuver Preview (Always next maneuver | Follow road when far)
            MenuItemController rgiFarItem = createComboItem(terminal, MODEL_RGI_FAR,
                TEXT_RGI_FAR, new int[]{TEXT_RGI_FAR_NEXT, TEXT_RGI_FAR_STRAIGHT},
                TEXT_RGI_FAR_HINT, INTERNAL_ID_RGI_FAR);
            registerScreenView(screen, MODEL_RGI_FAR, rgiFarItem);
            menu.addWithPosition(rgiFarItem, pos++);
            rgiFarMenuItem = rgiFarItem;

            syncMenuEnabledStates();
        }

        private static void updateScreenTitle(AbstractScreenWidget screen, int titleTextId) {
            for (int i = 0; i < screen.getChildrenSize(); i++) {
                AbstractWidget child = screen.getChild(i);
                if (child.getRole() == 2 && child instanceof AbstractWidgetController) {
                    AbstractWidgetController titleArea = (AbstractWidgetController) child;
                    if (titleArea.getChildrenSize() > 0
                            && titleArea.getChild(0) instanceof AbstractWidgetController) {
                        AbstractWidgetController titleBar = (AbstractWidgetController) titleArea.getChild(0);
                        for (int j = 0; j < titleBar.getChildrenSize(); j++) {
                            AbstractWidget w = titleBar.getChild(j);
                            if (w instanceof LabelController && w.getRole() == 0) {
                                ((LabelController) w).setTextIds(new int[]{titleTextId});
                                return;
                            }
                        }
                    }
                }
            }
        }

        private static void registerScreenView(AbstractScreenWidget screen, int modelId, HMIView item) {
            int[] ids = screen.getViewIDs();
            if (ids == null) ids = new int[0];
            HMIView[][] views = new HMIView[ids.length + 1][];
            int[] nids = new int[ids.length + 1];
            int o = 0;
            boolean placed = false;
            for (int k = 0; k < ids.length; k++) {
                if (!placed && ids[k] > modelId) {
                    nids[o] = modelId;
                    views[o++] = new HMIView[]{item};
                    placed = true;
                }
                nids[o] = ids[k];
                views[o++] = screen.findViews(ids[k]);
            }
            if (!placed) {
                nids[o] = modelId;
                views[o] = new HMIView[]{item};
            }
            screen.setViews(nids, views);
        }
    }
}
