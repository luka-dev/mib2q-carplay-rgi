/*
 * Read-only Virtual Cockpit preset/layout probe.
 *
 * MU1316 runs the legacy KombiSync protocol, whose status has no style field.
 * DSICarKombi is the authoritative source instead: attribute 99 publishes the
 * active preset ID, attribute 97 maps preset positions to semantic types
 * (CLASSIC=2, DRIVINGPROFILE=3), and attribute 100 publishes the independent
 * fullscreen/smallscreen display-view configuration.
 *
 * Java 1.4 / Foundation 1.1.
 */
package com.luka.carplay.core;

import com.luka.carplay.framework.Log;

import de.audi.atip.base.IFrameworkAccess;
import de.audi.mib.jdsi.DSIActivator;
import de.audi.mib.jdsi.IDSIClient;
import de.audi.remotehmi.util.dsi.DSICarKombiListenerAdapter;

import org.dsi.ifc.base.DSIBase;
import org.dsi.ifc.carkombi.DCDisplayPresetsListRecord;
import org.dsi.ifc.carkombi.DCDisplayViewConfiguration;
import org.dsi.ifc.carkombi.DSICarKombi;
import org.dsi.ifc.carkombi.DSICarKombiListener;
import org.dsi.ifc.global.CarArrayListUpdateInfo;

import org.osgi.framework.BundleContext;

public final class VcStyleLogger extends DSICarKombiListenerAdapter implements IDSIClient {
    private static final String TAG = "VCStyle";
    private static final Object LOCK = new Object();

    private static VcStyleLogger instance;

    private DSIActivator activator;
    private BundleContext bundleContext;
    private volatile int activePresetId = -1;
    private volatile int activePresetType = -1;
    private int activeDisplayView = -1;
    private DCDisplayPresetsListRecord[] presets;

    private VcStyleLogger() {}

    public static void start(IFrameworkAccess framework, BundleContext context) {
        if (framework == null || context == null) return;
        synchronized (LOCK) {
            if (instance != null) return;
            VcStyleLogger logger = new VcStyleLogger();
            try {
                logger.bundleContext = context;
                logger.activator = new DSIActivator(
                    framework,
                    DSICarKombi.class.getName(),
                    DSICarKombiListener.class.getName(),
                    new Integer(0),
                    logger,
                    logger
                );
                logger.activator.start(context);
                instance = logger;
                Log.i(TAG, "DSICarKombi listener started; switch VC Sport/Classic");
            } catch (Throwable t) {
                try {
                    if (logger.activator != null) logger.activator.stop(context);
                } catch (Throwable ignored) { }
                Log.w(TAG, "listener start failed", t);
            }
        }
    }

    public static void stop() {
        synchronized (LOCK) {
            VcStyleLogger logger = instance;
            instance = null;
            if (logger == null) return;
            try {
                if (logger.activator != null && logger.bundleContext != null) {
                    logger.activator.stop(logger.bundleContext);
                }
            } catch (Throwable t) {
                Log.w(TAG, "listener stop failed", t);
            }
            logger.activator = null;
            logger.bundleContext = null;
        }
    }

    public static int getActivePresetType() {
        VcStyleLogger logger = instance;
        return logger != null ? logger.activePresetType : -1;
    }

    public void setDSI(DSIBase service) {
        Log.i(TAG, "DSICarKombi service ready");
    }

    public int[] getAutoNotifications() {
        return new int[]{
            DSICarKombi.ATTR_DCDISPLAYPRESETSLISTTOTALNUMBEROFELEMENTS,
            DSICarKombi.ATTR_DCDISPLAYPRESETSLISTUPDATEINFO,
            DSICarKombi.ATTR_DCACTIVEDISPLAYPRESET,
            DSICarKombi.ATTR_DCDISPLAYVIEWCONFIGURATION
        };
    }

    public synchronized void updateDCActiveDisplayPreset(int presetId, int validFlag) {
        activePresetId = validFlag == 1 ? presetId : -1;
        activePresetType = typeForPreset(activePresetId);
        Log.i(TAG, "active presetID=" + activePresetId
            + " type=" + presetTypeName(activePresetType)
            + "(" + activePresetType + ") valid=" + validFlag);
    }

    public synchronized void updateDCDisplayViewConfiguration(DCDisplayViewConfiguration config, int validFlag) {
        activeDisplayView = validFlag == 1 && config != null ? config.getActiveDisplayView() : -1;
        Log.i(TAG, "displayView=" + displayViewName(activeDisplayView)
            + "(" + activeDisplayView + ") valid=" + validFlag);
    }

    public void updateDCDisplayPresetsListTotalNumberOfElements(int count, int validFlag) {
        Log.i(TAG, "preset count=" + count + " valid=" + validFlag);
    }

    public synchronized void responseDCDisplayPresetsList(
        CarArrayListUpdateInfo header, DCDisplayPresetsListRecord[] records
    ) {
        acceptPresets("response", records, 1);
    }

    public synchronized void updateDCDisplayPresetsListUpdateInfo(
        CarArrayListUpdateInfo header, DCDisplayPresetsListRecord[] records, int validFlag
    ) {
        acceptPresets("update", records, validFlag);
    }

    private void acceptPresets(String source, DCDisplayPresetsListRecord[] records, int validFlag) {
        if (validFlag == 1 && records != null) presets = records;
        StringBuffer text = new StringBuffer(source).append(" presets valid=").append(validFlag).append(" [");
        if (records != null) {
            for (int i = 0; i < records.length; i++) {
                if (i != 0) text.append(", ");
                DCDisplayPresetsListRecord record = records[i];
                if (record == null) {
                    text.append("null");
                } else {
                    text.append("pos=").append(record.getPos())
                        .append(" type=").append(presetTypeName(record.getType()))
                        .append('(').append(record.getType()).append(')')
                        .append(" instance=").append(record.getInstance())
                        .append(" state=").append(record.getState());
                }
            }
        }
        text.append(']');
        Log.i(TAG, text.toString());
        activePresetType = typeForPreset(activePresetId);
        if (activePresetId >= 0) {
            Log.i(TAG, "resolved presetID=" + activePresetId
                + " -> " + presetTypeName(activePresetType) + "(" + activePresetType + ")");
        }
    }

    private int typeForPreset(int presetId) {
        if (presetId < 0 || presets == null) return -1;
        for (int i = 0; i < presets.length; i++) {
            DCDisplayPresetsListRecord record = presets[i];
            if (record != null && record.getPos() == presetId) return record.getType();
        }
        return -1;
    }

    private static String presetTypeName(int type) {
        switch (type) {
            case DSICarKombi.DCDISPLAYPRESETSTYPE_INDIVIDUAL: return "INDIVIDUAL";
            case DSICarKombi.DCDISPLAYPRESETSTYPE_CLASSIC: return "CLASSIC";
            case DSICarKombi.DCDISPLAYPRESETSTYPE_DRIVINGPROFILE: return "DRIVINGPROFILE";
            case DSICarKombi.DCDISPLAYPRESETSTYPE_UNKNOWN: return "UNKNOWN";
            default: return "UNRESOLVED";
        }
    }

    private static String displayViewName(int view) {
        if (view == DSICarKombi.DCDISPLAYVIEWCONFIGURATION_NORMAL) return "SMALLSCREEN";
        if (view == DSICarKombi.DCDISPLAYVIEWCONFIGURATION_LARGE) return "FULLSCREEN";
        if (view == DSICarKombi.DCDISPLAYVIEWCONFIGURATION_UNKNOWN) return "UNKNOWN";
        return "DISPLAY_VIEW";
    }
}
