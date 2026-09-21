/*
 * ClusterLayerController — the single owner of the CarPlay cluster plane geometry.
 *
 * Extracted out of the stock CombiMapController so the stock class carries only a one-line call-out
 * (patch footprint = minimal, survives a firmware re-decompile).  All CarPlay cluster-plane policy
 * lives here:
 *
 *   98  = maneuver_render window (Software, 328x181)   — our RGI maneuver
 *   101 = stock 987 KDK backing  (Image, 328x180)      — sport/full size
 *   102 = stock 987 KDK backing  (Image, 210x153)      — popup size
 *
 * Visibility is gated on AltScreenModule.isNavActive() (== the BAP nav-active message, NOT the stock
 * KDK-visible hint): nav off -> all three transparent, so the panel never shows a stale maneuver or
 * backing.  ctx 80/81 (AltScreenModule) already removes the layers from composition on nav-off; the
 * opacity=0 here is the belt to that suspenders.
 *
 * Geometry comes from the terminal's active stock Layout.  This is important on B9: Classic and
 * Sport use different in-tube anchors/crops, and the stock skin switch changes the Layout object at
 * runtime.  We cache primitive values rather than the Layout itself so reapply() remains safe after
 * a context transition.
 *
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */
package com.luka.carplay.cluster;

import com.luka.carplay.framework.Log;
import de.audi.tghu.fwhmi.IDisplayManagerKombiControl;
import de.esolutions.hmi.widgets.audi.base.Layout;

public final class ClusterLayerController {

    /* CarPlay cluster displayables (see dc[80] = {98,101,102,99} in DisplayManagerMIB2High). */
    private static final int MANEUVER      = 98;    /* maneuver_render (Software) */
    private static final int BACKING_SPORT = 101;   /* 987 KDK backing, 328x180 */
    private static final int BACKING_POPUP = 102;   /* 987 KDK backing, 210x153 */
    /* Stock CombiMapController/Layout slots.  Names describe the KDK stage, not the skin: the
     * in-tube values themselves differ between LayoutMIB2HighB9 and LayoutMIB2HighB9Sport. */
    private static final int LC_IN_TUBE_X = 58, LC_IN_TUBE_Y = 59;
    private static final int LC_POPUP_X = 60, LC_POPUP_Y = 61;
    private static final int LC_POPUP_CROP_X = 118, LC_POPUP_CROP_Y = 119;
    private static final int LC_POPUP_CROP_W = 120, LC_POPUP_CROP_H = 121;
    private static final int LC_IN_TUBE_CROP_X = 122, LC_IN_TUBE_CROP_Y = 123;
    private static final int LC_IN_TUBE_CROP_W = 124, LC_IN_TUBE_CROP_H = 125;
    /* Stock's small-stage (singlescreen) offset.  CombiMapController.positionMap() adds it to the
     * map planes 33/58 when NAV_VIEW_SIZE_CHOICE == 1.  Our CarPlay map is plane 99, a full
     * 1440x542 window that cannot be translated, so the offset is applied to OUR maneuver plane
     * 98 and its backing instead — they are what has to follow the map.  Sport = (-476,0),
     * Classic = (0,0) (LayoutMIB2HighQ7 fallback), so Classic is unaffected for free. */
    private static final int LC_SMALL_STAGE_DX = 80, LC_SMALL_STAGE_DY = 81;
    private static final Object LOCK = new Object();
    private static IDisplayManagerKombiControl lastDm;
    private static int lastTerminal;
    private static boolean lastPopup;
    private static boolean lastStockVisible;
    private static int lastStockOpacity;
    /* Safe fallback used only before stock CombiMapController publishes its live Layout. */
    private static Geometry lastGeometry = new Geometry(
        984, 139, 1091, 110,
        59, 27, 210, 153,
        0, 0, 328, 180,
        -476, 0,
        "fallback-sport");
    private static boolean haveLayout;
    private static boolean errorLogged;
    private static String lastAppliedSignature;
    private static volatile ViewportListener viewportListener;

    public interface ViewportListener {
        void onManeuverViewportChanged();
    }

    public static void setViewportListener(ViewportListener listener) { viewportListener = listener; }
    public static void clearViewportListener(ViewportListener listener) {
        if (viewportListener == listener) viewportListener = null;
    }

    /** Source pixels actually visible on VC, in the renderer's 328x181 frame.
     * Use the same stage selection and crop as applyNow(), without moving the plane. */
    public static int[] maneuverViewport() {
        boolean popup = !com.luka.carplay.core.AltScreenModule.isSmallScreenViewArea();
        synchronized (LOCK) {
            Geometry g = lastGeometry;
            return popup ? new int[]{g.popupCropX, g.popupCropY, g.popupCropW, g.popupCropH}
                : new int[]{g.inTubeCropX, g.inTubeCropY, g.inTubeCropW, g.inTubeCropH};
        }
    }

    private ClusterLayerController() {}

    private static final class Geometry {
        final int inTubeX, inTubeY, popupX, popupY;
        final int popupCropX, popupCropY, popupCropW, popupCropH;
        final int inTubeCropX, inTubeCropY, inTubeCropW, inTubeCropH;
        final int smallStageDX, smallStageDY;
        final String layoutName;

        Geometry(int inTubeX, int inTubeY, int popupX, int popupY,
                 int popupCropX, int popupCropY, int popupCropW, int popupCropH,
                 int inTubeCropX, int inTubeCropY, int inTubeCropW, int inTubeCropH,
                 int smallStageDX, int smallStageDY,
                 String layoutName) {
            this.inTubeX = inTubeX;
            this.inTubeY = inTubeY;
            this.popupX = popupX;
            this.popupY = popupY;
            this.popupCropX = popupCropX;
            this.popupCropY = popupCropY;
            this.popupCropW = popupCropW;
            this.popupCropH = popupCropH;
            this.inTubeCropX = inTubeCropX;
            this.inTubeCropY = inTubeCropY;
            this.inTubeCropW = inTubeCropW;
            this.inTubeCropH = inTubeCropH;
            this.smallStageDX = smallStageDX;
            this.smallStageDY = smallStageDY;
            this.layoutName = layoutName;
        }

        boolean sameValues(Geometry other) {
            return other != null
                && inTubeX == other.inTubeX && inTubeY == other.inTubeY
                && popupX == other.popupX && popupY == other.popupY
                && popupCropX == other.popupCropX && popupCropY == other.popupCropY
                && popupCropW == other.popupCropW && popupCropH == other.popupCropH
                && inTubeCropX == other.inTubeCropX && inTubeCropY == other.inTubeCropY
                && inTubeCropW == other.inTubeCropW && inTubeCropH == other.inTubeCropH
                && smallStageDX == other.smallStageDX && smallStageDY == other.smallStageDY;
        }
    }

    private static Geometry geometryFrom(Layout layout) {
        return new Geometry(
            layout.getIntegerConstant(LC_IN_TUBE_X),
            layout.getIntegerConstant(LC_IN_TUBE_Y),
            layout.getIntegerConstant(LC_POPUP_X),
            layout.getIntegerConstant(LC_POPUP_Y),
            layout.getIntegerConstant(LC_POPUP_CROP_X),
            layout.getIntegerConstant(LC_POPUP_CROP_Y),
            layout.getIntegerConstant(LC_POPUP_CROP_W),
            layout.getIntegerConstant(LC_POPUP_CROP_H),
            layout.getIntegerConstant(LC_IN_TUBE_CROP_X),
            layout.getIntegerConstant(LC_IN_TUBE_CROP_Y),
            layout.getIntegerConstant(LC_IN_TUBE_CROP_W),
            layout.getIntegerConstant(LC_IN_TUBE_CROP_H),
            layout.getIntegerConstant(LC_SMALL_STAGE_DX),
            layout.getIntegerConstant(LC_SMALL_STAGE_DY),
            layout.getClass().getName());
    }

    /** Seed/cache the exact OEM geometry even before the first KDK model delta. */
    public static void updateLayout(IDisplayManagerKombiControl dm, int terminal, Layout layout) {
        if (dm == null || layout == null) return;
        Geometry geometry;
        try {
            geometry = geometryFrom(layout);
        } catch (Throwable t) {
            Log.w("ClusterLayers", "layout read failed: " + t);
            return;
        }
        boolean changed;
        synchronized (LOCK) {
            changed = !geometry.sameValues(lastGeometry)
                || !geometry.layoutName.equals(lastGeometry.layoutName);
            lastDm = dm;
            lastTerminal = terminal;
            lastGeometry = geometry;
            haveLayout = true;
        }
        if (changed) {
            ViewportListener listener = viewportListener;
            if (listener != null) {
                try { listener.onManeuverViewportChanged(); }
                catch (Throwable t) { Log.w("ClusterLayers", "viewport listener failed: " + t); }
            }
            Log.i("ClusterLayers", "layout=" + geometry.layoutName
                + " inTube=(" + geometry.inTubeX + "," + geometry.inTubeY + ") crop=("
                + geometry.inTubeCropX + "," + geometry.inTubeCropY + ","
                + geometry.inTubeCropW + "x" + geometry.inTubeCropH + ") popup=("
                + geometry.popupX + "," + geometry.popupY + ") crop=("
                + geometry.popupCropX + "," + geometry.popupCropY + ","
                + geometry.popupCropW + "x" + geometry.popupCropH + ")"
                + " smallStage=(" + geometry.smallStageDX + "," + geometry.smallStageDY + ")");
        }
    }

    /** Cold-boot fallback before the first stock KDK model update. The first real update replaces
     * the conservative popup geometry with the authoritative popup/in-tube layout. */
    public static void bind(IDisplayManagerKombiControl dm, int terminal) {
        synchronized (LOCK) {
            if (lastDm != dm) {
                lastPopup = true;
                lastStockVisible = false;
                lastStockOpacity = 0;
            }
            lastDm = dm;
            lastTerminal = terminal;
            haveLayout = true;
        }
    }

    /**
     * Apply the CarPlay cluster plane geometry for one KDK model update.
     * @param popup true = popup stage (hint&8 == 0); false = in-tube stage (hint&8 != 0).
     *               Exact crop and size come from the current Classic/Sport Layout.
     * Reads the nav-active gate itself; never throws into the HMI thread.
     */
    public static void apply(IDisplayManagerKombiControl dm, int terminal, Layout layout,
                             boolean popup, boolean stockVisible, int stockOpacity) {
        updateLayout(dm, terminal, layout);
        Geometry geometry;
        synchronized (LOCK) {
            lastDm = dm;
            lastTerminal = terminal;
            lastPopup = popup;
            lastStockVisible = stockVisible;
            lastStockOpacity = stockOpacity;
            haveLayout = true;
            geometry = lastGeometry;
        }
        applyNow(dm, terminal, geometry, popup, stockVisible, stockOpacity);
    }

    /** Re-apply the last stock KDK geometry after AltScreenModule changes ctx 80/81. */
    public static void reapply() {
        IDisplayManagerKombiControl dm;
        int terminal;
        boolean popup;
        boolean stockVisible;
        int stockOpacity;
        Geometry geometry;
        synchronized (LOCK) {
            if (!haveLayout || lastDm == null) return;
            dm = lastDm;
            terminal = lastTerminal;
            popup = lastPopup;
            stockVisible = lastStockVisible;
            stockOpacity = lastStockOpacity;
            geometry = lastGeometry;
        }
        applyNow(dm, terminal, geometry, popup, stockVisible, stockOpacity);
    }

    private static void applyNow(IDisplayManagerKombiControl dm, int terminal, Geometry geometry,
                                 boolean popup, boolean stockVisible, int stockOpacity) {
        /* The caller's stage is the stock KDK hint BITFIELD_KDK_POSITION_IN_TUBE, and during
         * CarPlay the stock nav sends no KDK model updates at all — so it is a stale cache stuck
         * at its bind() default (popup).  That is why fullscreen always looked right and every
         * small-stage view was wrong.
         *
         * Derive it the way stock does.  ClusterKDKHandlerImpl.setKDKPositionHints():
         *     isSmallStageActive() -> addHint(8)     "small stage - kdk in tube"
         *     else                 -> removeHint(8)  "big stage - kdk in flap"
         * Small stage is the same axis as NAV_VIEW_SIZE_CHOICE == 1, which we already track.
         * This is skin-independent: Classic does have an in-tube stage, it just inherits the
         * 210x153 crop from LayoutMIB2HighQ7 instead of overriding it like Sport's 328x180. */
        popup = !com.luka.carplay.core.AltScreenModule.isSmallScreenViewArea();
        logDecision(geometry, popup);
        /* Do NOT apply the layout's small-stage offset (80/81) here.  Stock adds it to the map
         * planes 33/58 only; the KDK panel and its backing have no view-size dependency at all
         * (positionKDKBackgrounds / handleKdkDualTerminal read no view size).  Moving the panel
         * by -476 in Sport singlescreen was measured on the car to break a view that stock keeps
         * correct.  The offset is logged below for diagnosis, never applied. */
        boolean carplayOwnsCluster = com.luka.carplay.core.AltScreenModule.isConnected();
        boolean navActive = com.luka.carplay.core.AltScreenModule.isNavActive();
        int carplayOpacity = resolveCarPlayOpacity(carplayOwnsCluster, navActive);
        try {
            /* 101/102 are shared with the stock KDK renderer.  Restore the last stock model
             * atomically when CarPlay releases terminal 1; otherwise a disconnect can leave
             * Audi navigation's backing permanently transparent until an unrelated KDK delta. */
            if (!carplayOwnsCluster) {
                dm.setOpacity(MANEUVER, terminal, 0);
                if (stockVisible) {
                    dm.setOpacity(popup ? BACKING_POPUP : BACKING_SPORT,
                                  terminal, stockOpacity);
                    dm.setOpacity(popup ? BACKING_SPORT : BACKING_POPUP,
                                  terminal, 0);
                } else {
                    dm.setOpacity(BACKING_SPORT, terminal, 0);
                    dm.setOpacity(BACKING_POPUP, terminal, 0);
                }
                errorLogged = false;
                return;
            }
            if (!navActive) {
                dm.setOpacity(MANEUVER, terminal, 0);
                dm.setOpacity(BACKING_SPORT, terminal, 0);
                dm.setOpacity(BACKING_POPUP, terminal, 0);
                return;
            }
            if (popup) {
                /* Popup crop/anchor from the active OEM Layout. */
                int cx = geometry.popupCropX;
                int cy = geometry.popupCropY;
                int cw = geometry.popupCropW;
                int ch = geometry.popupCropH;
                int dx = geometry.popupX;
                int dy = geometry.popupY;
                dm.setCropping(MANEUVER, terminal, cx, cy, cw, ch, dx, dy, cw, ch);
                dm.setOpacity(MANEUVER, terminal, carplayOpacity);
                dm.setPosition(BACKING_POPUP, terminal, dx, dy);
                dm.setOpacity(BACKING_POPUP, terminal, carplayOpacity);
                dm.setOpacity(BACKING_SPORT, terminal, 0);
            } else {
                /* In-tube crop/anchor differs between Classic and Sport layouts. */
                int cx = geometry.inTubeCropX;
                int cy = geometry.inTubeCropY;
                int cw = geometry.inTubeCropW;
                int ch = geometry.inTubeCropH;
                int dx = geometry.inTubeX;
                int dy = geometry.inTubeY;
                dm.setCropping(MANEUVER, terminal, cx, cy, cw, ch, dx, dy, cw, ch);
                dm.setOpacity(MANEUVER, terminal, 100);
                dm.setPosition(BACKING_SPORT, terminal, dx, dy);
                dm.setOpacity(BACKING_SPORT, terminal, 100);
                dm.setOpacity(BACKING_POPUP, terminal, 0);
            }
            errorLogged = false;
        } catch (Throwable t) {
            /* Never throw into HMI/DM threads, but keep the first failure diagnosable. */
            if (!errorLogged) {
                errorLogged = true;
                Log.w("ClusterLayers", "apply failed: " + t);
            }
        }
    }

    /** One line per distinct geometry decision — the exact numbers written to the DM.
     *  Every Classic/Sport/singlescreen bug so far was a guess about which branch ran; this makes
     *  it readable in /tmp/carplay_java.log instead. Logged only when the tuple changes. */
    private static void logDecision(Geometry g, boolean popup) {
        int cropX = popup ? g.popupCropX : g.inTubeCropX;
        int cropY = popup ? g.popupCropY : g.inTubeCropY;
        int cropW = popup ? g.popupCropW : g.inTubeCropW;
        int cropH = popup ? g.popupCropH : g.inTubeCropH;
        int dstX  = popup ? g.popupX     : g.inTubeX;
        int dstY  = popup ? g.popupY     : g.inTubeY;

        String line = "apply " + g.layoutName
            + " style=" + com.luka.carplay.core.AltScreenModule.styleLabel()
            + " view=" + (com.luka.carplay.core.AltScreenModule.isSmallScreenViewArea()
                          ? "single" : "full")
            + " stage=" + (popup ? "popup" : "inTube")
            + " backing=" + (popup ? BACKING_POPUP : BACKING_SPORT)
            + " src=(" + cropX + "," + cropY + " " + cropW + "x" + cropH + ")"
            + " dst=(" + dstX + "," + dstY + ")"
            + " smallStageOffset=(" + g.smallStageDX + "," + g.smallStageDY + ") [not applied]";

        /* Every value that can change the picture is in the line, so comparing the line itself
         * is the dedup key. */
        synchronized (LOCK) {
            if (line.equals(lastAppliedSignature)) return;
            lastAppliedSignature = line;
        }
        Log.i("ClusterLayers", line);
    }

    /** One shared opacity for the maneuver and its backing.  No HU-side reveal hold: the VC
     *  itself fades the KDK area in over 200 ms (docs/reference/CLUSTER_KDK_ANIMATION_RU.md), so
     *  the backing must already be opaque when that fade starts, or the hole opens over the map
     *  and the black backing pops in afterwards.  Removal keeps AltScreenModule's 250 ms hold. */
    private static int resolveCarPlayOpacity(boolean owns, boolean navActive) {
        return (owns && navActive) ? 100 : 0;
    }
}
