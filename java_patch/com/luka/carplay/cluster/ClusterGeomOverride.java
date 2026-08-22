/*
 * ClusterGeomOverride - live tuning knob for the CarPlay cluster panel geometry.
 *
 * The authoritative numbers come from the stock Layout table (see
 * docs/reference/CLUSTER_KDK_GEOMETRY.md).  They are correct for Sport, but Classic inherits its
 * in-tube crop from LayoutMIB2HighQ7 and the result has to be eyeballed on the car.  Rebuilding
 * the jar for every nudge costs a full unit reboot, so the values are overridable from a file.
 *
 *   /tmp/cluster_geom.cfg      key=value per line, '#' comments, missing keys keep the layout value
 *
 *     stage        = auto | popup | intube     (auto = stock rule: singlescreen -> intube)
 *     inTubeX      inTubeY                     in-tube (singlescreen) anchor
 *     inTubeCropX  inTubeCropY  inTubeCropW  inTubeCropH
 *     popupX       popupY                      popup (fullscreen) anchor
 *     popupCropX   popupCropY   popupCropW   popupCropH
 *
 * Values are absolute, not deltas - copy what the ClusterLayers log prints, then edit.
 * The file is re-read whenever its size or mtime changes; the 250 ms connected-context worker
 * polls it, so a save takes effect within a quarter second with no restart.
 *
 * Absent file = zero overrides = pure stock layout, which is the production state.
 */
package com.luka.carplay.cluster;

import com.luka.carplay.framework.Log;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileReader;

public final class ClusterGeomOverride {

    public static final int STAGE_AUTO = 0;
    public static final int STAGE_POPUP = 1;
    public static final int STAGE_IN_TUBE = 2;

    private static final String PATH = "/tmp/cluster_geom.cfg";
    private static final String TAG = "ClusterGeom";
    private static final Object LOCK = new Object();

    /* Unset is Integer.MIN_VALUE so a legitimate 0 or a negative offset still overrides. */
    private static final int UNSET = Integer.MIN_VALUE;

    private static long lastModified = -1L;
    private static long lastLength = -1L;
    private static int stage = STAGE_AUTO;
    private static int inTubeX = UNSET, inTubeY = UNSET;
    private static int popupX = UNSET, popupY = UNSET;
    private static int inTubeCropX = UNSET, inTubeCropY = UNSET;
    private static int inTubeCropW = UNSET, inTubeCropH = UNSET;
    private static int popupCropX = UNSET, popupCropY = UNSET;
    private static int popupCropW = UNSET, popupCropH = UNSET;

    private ClusterGeomOverride() {}

    /** @return true when the effective override set changed and the caller should re-apply. */
    public static boolean poll() {
        File f = new File(PATH);
        long mod, len;
        boolean exists;
        try {
            exists = f.exists();
            mod = exists ? f.lastModified() : -1L;
            len = exists ? f.length() : -1L;
        } catch (Throwable t) {
            return false;
        }
        synchronized (LOCK) {
            if (mod == lastModified && len == lastLength) return false;
            lastModified = mod;
            lastLength = len;
        }
        if (!exists) {
            synchronized (LOCK) { clearLocked(); }
            Log.i(TAG, "no " + PATH + " - stock layout geometry");
            return true;
        }
        return load(f);
    }

    private static boolean load(File f) {
        BufferedReader r = null;
        synchronized (LOCK) { clearLocked(); }
        try {
            r = new BufferedReader(new FileReader(f));
            String line;
            while ((line = r.readLine()) != null) {
                int hash = line.indexOf('#');
                if (hash >= 0) line = line.substring(0, hash);
                int eq = line.indexOf('=');
                if (eq <= 0) continue;
                String key = line.substring(0, eq).trim();
                String val = line.substring(eq + 1).trim();
                if (key.length() == 0 || val.length() == 0) continue;
                apply(key, val);
            }
        } catch (Throwable t) {
            Log.w(TAG, "read failed: " + t);
            return true;
        } finally {
            if (r != null) try { r.close(); } catch (Throwable t) { }
        }
        Log.i(TAG, "loaded " + PATH + " " + describe());
        return true;
    }

    private static void apply(String key, String val) {
        if ("stage".equals(key)) {
            int s = "popup".equals(val) ? STAGE_POPUP
                  : ("intube".equals(val) || "inTube".equals(val)) ? STAGE_IN_TUBE
                  : STAGE_AUTO;
            synchronized (LOCK) { stage = s; }
            return;
        }
        int v;
        try { v = Integer.parseInt(val); }
        catch (NumberFormatException e) { Log.w(TAG, "bad value " + key + "=" + val); return; }
        synchronized (LOCK) {
            if ("inTubeX".equals(key)) inTubeX = v;
            else if ("inTubeY".equals(key)) inTubeY = v;
            else if ("popupX".equals(key)) popupX = v;
            else if ("popupY".equals(key)) popupY = v;
            else if ("inTubeCropX".equals(key)) inTubeCropX = v;
            else if ("inTubeCropY".equals(key)) inTubeCropY = v;
            else if ("inTubeCropW".equals(key)) inTubeCropW = v;
            else if ("inTubeCropH".equals(key)) inTubeCropH = v;
            else if ("popupCropX".equals(key)) popupCropX = v;
            else if ("popupCropY".equals(key)) popupCropY = v;
            else if ("popupCropW".equals(key)) popupCropW = v;
            else if ("popupCropH".equals(key)) popupCropH = v;
            else Log.w(TAG, "unknown key " + key);
        }
    }

    private static void clearLocked() {
        stage = STAGE_AUTO;
        inTubeX = inTubeY = popupX = popupY = UNSET;
        inTubeCropX = inTubeCropY = inTubeCropW = inTubeCropH = UNSET;
        popupCropX = popupCropY = popupCropW = popupCropH = UNSET;
    }

    public static int stage() { synchronized (LOCK) { return stage; } }

    /** @param layoutValue the stock Layout constant; returned unchanged when nothing overrides it. */
    public static int inTubeX(int layoutValue) { synchronized (LOCK) { return pick(inTubeX, layoutValue); } }
    public static int inTubeY(int layoutValue) { synchronized (LOCK) { return pick(inTubeY, layoutValue); } }
    public static int popupX(int layoutValue)  { synchronized (LOCK) { return pick(popupX, layoutValue); } }
    public static int popupY(int layoutValue)  { synchronized (LOCK) { return pick(popupY, layoutValue); } }
    public static int inTubeCropX(int v) { synchronized (LOCK) { return pick(inTubeCropX, v); } }
    public static int inTubeCropY(int v) { synchronized (LOCK) { return pick(inTubeCropY, v); } }
    public static int inTubeCropW(int v) { synchronized (LOCK) { return pick(inTubeCropW, v); } }
    public static int inTubeCropH(int v) { synchronized (LOCK) { return pick(inTubeCropH, v); } }
    public static int popupCropX(int v) { synchronized (LOCK) { return pick(popupCropX, v); } }
    public static int popupCropY(int v) { synchronized (LOCK) { return pick(popupCropY, v); } }
    public static int popupCropW(int v) { synchronized (LOCK) { return pick(popupCropW, v); } }
    public static int popupCropH(int v) { synchronized (LOCK) { return pick(popupCropH, v); } }

    private static int pick(int override, int layoutValue) {
        return override == UNSET ? layoutValue : override;
    }

    private static String describe() {
        StringBuffer b = new StringBuffer();
        b.append("stage=").append(stage == STAGE_POPUP ? "popup"
                                : stage == STAGE_IN_TUBE ? "intube" : "auto");
        add(b, "inTubeX", inTubeX); add(b, "inTubeY", inTubeY);
        add(b, "popupX", popupX);   add(b, "popupY", popupY);
        add(b, "inTubeCrop", inTubeCropX); add(b, ",", inTubeCropY);
        add(b, "x", inTubeCropW);          add(b, ",", inTubeCropH);
        add(b, "popupCrop", popupCropX);   add(b, ",", popupCropY);
        add(b, "x", popupCropW);           add(b, ",", popupCropH);
        return b.toString();
    }

    private static void add(StringBuffer b, String name, int v) {
        if (v != UNSET) b.append(' ').append(name).append('=').append(v);
    }
}
