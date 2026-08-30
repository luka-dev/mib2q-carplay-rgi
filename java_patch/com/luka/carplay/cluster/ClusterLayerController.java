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
 * Visibility is gated on ScreenModule.isNavActive() (== the BAP nav-active message, NOT the stock
 * KDK-visible hint): nav off -> all three transparent, so the panel never shows a stale maneuver or
 * backing.  ctx 80 (ScreenModule) already removes the layers from composition on nav-off; the
 * opacity=0 here is the belt to that suspenders.
 *
 * Geometry comes from the terminal's active stock Layout.  This is important on B9: Classic and
 * Sport use different in-tube anchors/crops, and the stock skin switch changes the Layout object at
 * runtime.  We cache primitive values rather than the Layout itself so reapply() remains safe after
 * a context transition.
 */
package com.luka.carplay.cluster;

import com.luka.carplay.framework.Log;
import de.audi.tghu.fwhmi.IDisplayManagerKombiControl;
import de.esolutions.hmi.widgets.audi.base.Layout;

public final class ClusterLayerController {

    /* CarPlay cluster displayables (see dc[80] = {98,101,102,33} in DisplayManagerMIB2High). */
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
     * map planes 33/58 when NAV_VIEW_SIZE_CHOICE == 1.  The stock native map (plane 33) is a full
     * 1440x542 window stock already translates, so the offset is applied to OUR maneuver plane
     * 98 and its backing instead — they are what has to follow the map.  Sport = (-476,0),
     * Classic = (0,0) (LayoutMIB2HighQ7 fallback), so Classic is unaffected for free. */
    private static final int LC_SMALL_STAGE_DX = 80, LC_SMALL_STAGE_DY = 81;
    /* The VC's KDK fade completion is not exported to HU Java.  Prefer the stock
     * BITFIELD_KDK_FADED_IN-derived opacity, but never strand a valid CarPlay
     * presentation invisible if that model delta is lost. */
    private static final long POPUP_REVEAL_FALLBACK_MS = 250L;
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
    private static boolean popupCycleActive;
    private static boolean popupRevealed;
    private static boolean popupRevealPending;
    private static String lastAppliedSignature;
    private static int popupRevealGeneration;

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
                resetPopupRevealLocked();
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

    /** Re-apply the last stock KDK geometry after ScreenModule changes ctx 80. */
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
        popup = !com.luka.carplay.core.ScreenModule.isSmallScreenViewArea();
        int forced = ClusterGeomOverride.stage();
        if (forced == ClusterGeomOverride.STAGE_POPUP) popup = true;
        else if (forced == ClusterGeomOverride.STAGE_IN_TUBE) popup = false;
        logDecision(geometry, popup);
        /* Do NOT apply the layout's small-stage offset (80/81) here.  Stock adds it to the map
         * planes 33/58 only; the KDK panel and its backing have no view-size dependency at all
         * (positionKDKBackgrounds / handleKdkDualTerminal read no view size).  Moving the panel
         * by -476 in Sport singlescreen was measured on the car to break a view that stock keeps
         * correct.  The offset is logged below for diagnosis, never applied. */
        boolean carplayOwnsCluster = com.luka.carplay.core.ScreenModule.isConnected();
        boolean navActive = com.luka.carplay.core.ScreenModule.isNavActive();
        int carplayOpacity = resolveCarPlayOpacity(carplayOwnsCluster, navActive, popup,
                                                   stockVisible, stockOpacity);
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
                int cx = ClusterGeomOverride.popupCropX(geometry.popupCropX);
                int cy = ClusterGeomOverride.popupCropY(geometry.popupCropY);
                int cw = ClusterGeomOverride.popupCropW(geometry.popupCropW);
                int ch = ClusterGeomOverride.popupCropH(geometry.popupCropH);
                int dx = ClusterGeomOverride.popupX(geometry.popupX);
                int dy = ClusterGeomOverride.popupY(geometry.popupY);
                dm.setCropping(MANEUVER, terminal, cx, cy, cw, ch, dx, dy, cw, ch);
                dm.setOpacity(MANEUVER, terminal, carplayOpacity);
                dm.setPosition(BACKING_POPUP, terminal, dx, dy);
                dm.setOpacity(BACKING_POPUP, terminal, carplayOpacity);
                dm.setOpacity(BACKING_SPORT, terminal, 0);
            } else {
                /* In-tube crop/anchor differs between Classic and Sport layouts. */
                int cx = ClusterGeomOverride.inTubeCropX(geometry.inTubeCropX);
                int cy = ClusterGeomOverride.inTubeCropY(geometry.inTubeCropY);
                int cw = ClusterGeomOverride.inTubeCropW(geometry.inTubeCropW);
                int ch = ClusterGeomOverride.inTubeCropH(geometry.inTubeCropH);
                int dx = ClusterGeomOverride.inTubeX(geometry.inTubeX);
                int dy = ClusterGeomOverride.inTubeY(geometry.inTubeY);
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
        int cropX = popup ? ClusterGeomOverride.popupCropX(g.popupCropX)
                          : ClusterGeomOverride.inTubeCropX(g.inTubeCropX);
        int cropY = popup ? ClusterGeomOverride.popupCropY(g.popupCropY)
                          : ClusterGeomOverride.inTubeCropY(g.inTubeCropY);
        int cropW = popup ? ClusterGeomOverride.popupCropW(g.popupCropW)
                          : ClusterGeomOverride.inTubeCropW(g.inTubeCropW);
        int cropH = popup ? ClusterGeomOverride.popupCropH(g.popupCropH)
                          : ClusterGeomOverride.inTubeCropH(g.inTubeCropH);
        int dstX  = popup ? ClusterGeomOverride.popupX(g.popupX)
                          : ClusterGeomOverride.inTubeX(g.inTubeX);
        int dstY  = popup ? ClusterGeomOverride.popupY(g.popupY)
                          : ClusterGeomOverride.inTubeY(g.inTubeY);

        String line = "apply " + g.layoutName
            + " view=" + (com.luka.carplay.core.ScreenModule.isSmallScreenViewArea()
                          ? "single" : "full")
            + " stage=" + (popup ? "popup" : "inTube")
            + " backing=" + (popup ? BACKING_POPUP : BACKING_SPORT)
            + " src=(" + cropX + "," + cropY + " " + cropW + "x" + cropH + ")"
            + " dst=(" + dstX + "," + dstY + ")"
            + " smallStageOffset=(" + g.smallStageDX + "," + g.smallStageDY + ") [not applied]";

        /* Every value that can change the picture is in the line, so comparing the line itself
         * is both the dedup key and the guarantee that a /tmp/cluster_geom.cfg edit re-logs. */
        synchronized (LOCK) {
            if (line.equals(lastAppliedSignature)) return;
            lastAppliedSignature = line;
        }
        Log.i("ClusterLayers", line);
    }

    /** Resolve one shared opacity for the maneuver and its backing.
     *
     * Popup/fullscreen has the stock KDK slide/fade: wait until the stock model
     * reports FADED_IN, with a bounded fallback if that delta never arrives.
     * Sport/dual has no popup slide and is intentionally immediate.  Once a
     * popup has been revealed, keep it visible through the 250 ms removal hold;
     * a stock fade-out delta must not tear the backing away early. */
    private static int resolveCarPlayOpacity(boolean owns, boolean navActive, boolean popup,
                                             boolean stockVisible, int stockOpacity) {
        int startGeneration = -1;
        int opacity;
        boolean synchronizedToStock = false;
        synchronized (LOCK) {
            if (!owns || !navActive || !popup) {
                resetPopupRevealLocked();
                return (owns && navActive) ? 100 : 0;
            }

            if (!popupCycleActive) {
                popupCycleActive = true;
                popupRevealed = false;
                popupRevealPending = false;
                popupRevealGeneration++;
            }

            /* kdkOpacity is retained by stock when NO_KDK is selected, so the
             * opacity alone may be a stale 100 from the previous route. */
            if (!popupRevealed && stockVisible && stockOpacity > 0) {
                popupRevealed = true;
                popupRevealPending = false;
                popupRevealGeneration++;       /* cancel a pending fallback */
                synchronizedToStock = true;
            } else if (!popupRevealed && !popupRevealPending) {
                popupRevealPending = true;
                startGeneration = ++popupRevealGeneration;
            }
            opacity = popupRevealed ? 100 : 0;
        }

        if (synchronizedToStock)
            Log.i("ClusterLayers", "popup reveal synchronized to stock KDK opacity");
        if (startGeneration >= 0) startPopupRevealFallback(startGeneration);
        return opacity;
    }

    private static void resetPopupRevealLocked() {
        if (popupCycleActive || popupRevealPending || popupRevealed) popupRevealGeneration++;
        popupCycleActive = false;
        popupRevealed = false;
        popupRevealPending = false;
    }

    private static void startPopupRevealFallback(final int generation) {
        Thread reveal = new Thread(new Runnable() {
            public void run() {
                try { Thread.sleep(POPUP_REVEAL_FALLBACK_MS); }
                catch (InterruptedException e) { Thread.currentThread().interrupt(); return; }

                synchronized (LOCK) {
                    if (!popupCycleActive || !popupRevealPending
                            || generation != popupRevealGeneration || popupRevealed) return;
                    popupRevealPending = false;
                    popupRevealed = true;
                }
                Log.i("ClusterLayers", "popup reveal fallback after "
                    + POPUP_REVEAL_FALLBACK_MS + "ms");
                reapply();
            }
        }, "carplay-popup-reveal");
        reveal.setDaemon(true);
        try {
            reveal.start();
        } catch (Throwable t) {
            boolean apply = false;
            synchronized (LOCK) {
                if (popupCycleActive && popupRevealPending
                        && generation == popupRevealGeneration && !popupRevealed) {
                    popupRevealPending = false;
                    popupRevealed = true;
                    apply = true;
                }
            }
            Log.w("ClusterLayers", "popup reveal worker start failed; revealing immediately: " + t);
            if (apply) reapply();
        }
    }
}
