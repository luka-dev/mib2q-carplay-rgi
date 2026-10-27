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
 * CarPlay ownership/navigation gates eligibility; VC FctID44 gates actual visibility,
 * and VC FctID54 selects the KDK stage. The requested View mode never reveals a layer.
 *
 * SQ5 AA port, turn card (com.sq5.aa.luka.TurnCard.planeMode): while the Android Auto mirror
 * context (81) is shown and the VC is in its large map view with the KDK tile hidden, 98 is placed
 * as a turn card (full 328x180 renderer frame at TurnCard's rect) and made opaque; with no VC map
 * at all it is pre-armed opaque at the KDK anchor.  Every other case is luka's rule unchanged.
 * Every decision is logged ("layers: apply ... mode=...").
 * Stock hints are retained for restoration when CarPlay releases the cluster.  ctx 80/81 (ScreenModule) already removes the layers from composition on nav-off; the
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
    /* Stock map-only offset, recorded for diagnostics; never applied to KDK. */
    private static final int LC_SMALL_STAGE_DX = 80, LC_SMALL_STAGE_DY = 81;
    /* SQ5 AA port: Android Auto mirror plane (sq5_mirror) and the stock map-plane anchor it
     * shares with 33/58 (CombiMapController.positionMap: Layout 108/109 + 80/81 in small view). */
    private static final int MIRROR = 99;
    private static final int NATIVE_MAP = 33;
    private static final int LC_MAP_X = 108, LC_MAP_Y = 109;
    private static boolean mirrorShown;          /* guarded by APPLY_LOCK */
    /* SQ5: last apply decided "no arrow box" (full view, TurnCard map card off). The stock KDK path in
     * DisplayManagerMIB2High.setKDKOpacity bypasses this controller, so it asks here (run 84/85: the stock
     * backing re-appeared as an empty frame). */
    private static volatile boolean arrowHidden;
    public static boolean isArrowHidden() { return arrowHidden; }
    private static String lastMirrorSignature;   /* guarded by APPLY_LOCK */
    /* SQ5 AA port, turn card (com.sq5.aa.luka.TurnCard): the plane mode of the last apply and whether
     * the renderer must frame the card crop.  lastPlaneMode is guarded by APPLY_LOCK. */
    private static int lastPlaneMode = -1;
    private static volatile boolean cardViewport;
    private static boolean haveVcMapView, vcMapView;
    private static final Object LOCK = new Object();
    private static final Object APPLY_LOCK = new Object();
    private static IDisplayManagerKombiControl lastDm;
    private static int lastTerminal;
    private static boolean lastStockPopup = true;
    private static boolean vcPopup = true;
    private static boolean haveVcStage;
    private static boolean vcVisible;
    private static boolean haveVcVisibility;
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
    /* SQ5 AA port: the Java DisplayManager's opacity write-through cache starts at 100, so a first
     * setOpacity(98, 1, 100) would be silently skipped.  Write 0 once per DM before any non-zero. */
    private static IDisplayManagerKombiControl maneuverOpacityPrimedDm;

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
        if (cardViewport) return com.sq5.aa.luka.TurnCard.cardCrop();   /* SQ5: turn card (full frame) */
        synchronized (LOCK) {
            boolean popup = haveVcStage ? vcPopup : lastStockPopup;
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
        int mapX, mapY = 26;                       /* SQ5: fallback = MU1316 table (0,26) */
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
                && smallStageDX == other.smallStageDX && smallStageDY == other.smallStageDY
                && mapX == other.mapX && mapY == other.mapY;
        }
    }

    private static Geometry geometryFrom(Layout layout) {
        Geometry g = new Geometry(
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
        g.mapX = layout.getIntegerConstant(LC_MAP_X);
        g.mapY = layout.getIntegerConstant(LC_MAP_Y);
        return g;
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
                + " smallStage=(" + geometry.smallStageDX + "," + geometry.smallStageDY + ")"
                + " map=(" + geometry.mapX + "," + geometry.mapY + ")");
            com.sq5.aa.luka.AaLog.log("layers: layout=" + geometry.layoutName + " map(108/109)=(" + geometry.mapX
                + "," + geometry.mapY + ") smallStage(80/81)=(" + geometry.smallStageDX + "," + geometry.smallStageDY + ")");
            com.sq5.aa.luka.ClusterSkin.setLayout(geometry.layoutName);   /* SQ5: B9Sport = sport skin, B9 = classic */
        }
    }

    /** Cold-boot fallback before the first stock KDK model update. The first real update replaces
     * the conservative popup geometry with the authoritative popup/in-tube layout. */
    public static void bind(IDisplayManagerKombiControl dm, int terminal) {
        synchronized (LOCK) {
            if (lastDm != dm) {
                lastStockVisible = false;
                lastStockOpacity = 0;
            }
            lastDm = dm;
            lastTerminal = terminal;
            haveLayout = true;
        }
    }

    /** Single source for acknowledged KDK visibility, independent of CarPlay sessions. */
    public static boolean isKdkVisible() {
        synchronized (LOCK) {
            return haveVcVisibility ? vcVisible : lastStockVisible && lastStockOpacity > 0;
        }
    }

    /** Receive accepted FctID44 state before the stock listener sends its Status response. */
    public static void onVcVisibility(boolean visible) {
        synchronized (LOCK) {
            vcVisible = visible;
            haveVcVisibility = true;
        }
        Log.i("ClusterLayers", "VC Fct44 KDK visible=" + visible);
        com.sq5.aa.luka.AaLog.log("layers: VC Fct44 KDK visible=" + visible);
        reapply();
        com.luka.carplay.core.ScreenModule.onVcKdkVisibility(visible);
    }

    /** SQ5 AA port: VC FctID44 lvdsMapVisible (the VC shows the HU map frame).  The mirror context
     *  rule does not gate on it (mirror_context_report 3.4); the turn-card layer rule does
     *  (TurnCard.planeMode: card in the large map view, prearm while no map is shown). */
    public static void onVcMapVisibility(boolean lvdsMapVisible) {
        boolean changed;
        synchronized (LOCK) {
            changed = !haveVcMapView || vcMapView != lvdsMapVisible;
            vcMapView = lvdsMapVisible;
            haveVcMapView = true;
        }
        if (changed) {
            com.sq5.aa.luka.AaLog.log("layers: VC Fct44 lvdsMapVisible=" + lvdsMapVisible
                + " mirror=" + com.luka.carplay.core.ScreenModule.isMirrorActive());
            reapply();
        }
    }

    /** Receive VC FctID54, emitted at the stage animation midpoint. */
    public static void onVcPresentation(boolean largeMapView) {
        boolean changed;
        synchronized (LOCK) {
            changed = !haveVcStage || vcPopup != largeMapView;
            vcPopup = largeMapView;
            haveVcStage = true;
        }
        Log.i("ClusterLayers", "VC Fct54 KDK stage=" + (largeMapView ? "popup" : "inTube"));
        if (changed) com.sq5.aa.luka.AaLog.log("layers: VC Fct54 KDK stage=" + (largeMapView ? "popup" : "inTube"));
        reapply();
        if (changed) {
            ViewportListener listener = viewportListener;
            if (listener != null) {
                try { listener.onManeuverViewportChanged(); }
                catch (Throwable t) { Log.w("ClusterLayers", "viewport listener failed: " + t); }
            }
        }
    }

    /** Cache stock hints for normal-navigation restoration. CarPlay uses the same VC
     * visibility/presentation inputs, captured before stock availability can mask them. */
    public static void apply(IDisplayManagerKombiControl dm, int terminal, Layout layout,
                             boolean stockVisible, int stockOpacity, boolean inTube) {
        updateLayout(dm, terminal, layout);
        synchronized (LOCK) {
            lastDm = dm;
            lastTerminal = terminal;
            lastStockPopup = !inTube;
            lastStockVisible = stockVisible;
            lastStockOpacity = stockOpacity;
            haveLayout = true;
        }
        reapply();
    }

    /** Re-apply the last stock KDK geometry after ScreenModule changes ctx 80/81. */
    public static void reapply() {
        boolean viewportChanged;
        synchronized (APPLY_LOCK) { viewportChanged = reapplySerialized(); }
        /* SQ5 AA port: turn-card edges move the renderer's visible area; notify outside APPLY_LOCK
         * (the listener takes RouteGuidance's presentation lock). */
        if (viewportChanged) {
            ViewportListener listener = viewportListener;
            if (listener != null) {
                try { listener.onManeuverViewportChanged(); }
                catch (Throwable t) { Log.w("ClusterLayers", "viewport listener failed: " + t); }
            }
        }
    }

    /** @return true when the turn-card viewport changed (caller notifies the listener). */
    private static boolean reapplySerialized() {
        IDisplayManagerKombiControl dm;
        int terminal;
        boolean stockVisible;
        int stockOpacity;
        Geometry geometry;
        synchronized (LOCK) {
            if (!haveLayout || lastDm == null) return false;
            dm = lastDm;
            terminal = lastTerminal;
            stockVisible = lastStockVisible;
            stockOpacity = lastStockOpacity;
            geometry = lastGeometry;
        }
        return applyNow(dm, terminal, geometry, stockVisible, stockOpacity);
    }

    private static boolean applyNow(IDisplayManagerKombiControl dm, int terminal, Geometry geometry,
                                    boolean stockVisible, int stockOpacity) {
        boolean popup;
        boolean carplayOwnsCluster = com.luka.carplay.core.ScreenModule.isConnected();
        int permittedOpacity;
        boolean mapKnown, mapShown;
        synchronized (LOCK) {
            popup = carplayOwnsCluster && haveVcStage ? vcPopup : lastStockPopup;
            permittedOpacity = haveVcVisibility ? (vcVisible ? 100 : 0)
                : (stockVisible ? stockOpacity : 0);
            mapKnown = haveVcMapView;
            mapShown = vcMapView;
        }
        /* Do NOT apply the layout's small-stage offset (80/81) here.  Stock adds it to the map
         * planes 33/58 only; the KDK panel and its backing have no view-size dependency at all
         * (positionKDKBackgrounds / handleKdkDualTerminal read no view size).  Moving the panel
         * by -476 in Sport singlescreen was measured on the car to break a view that stock keeps
         * correct.  The offset is logged below for diagnosis, never applied. */
        boolean navActive = com.luka.carplay.core.ScreenModule.isNavActive();
        boolean mirrorActive = carplayOwnsCluster && com.luka.carplay.core.ScreenModule.isMirrorActive();
        /* SQ5 AA port: turn card.  popup == VC Fct54 largeMapView while we own the cluster. */
        int mode = com.sq5.aa.luka.TurnCard.planeMode(carplayOwnsCluster, navActive, mirrorActive,
            permittedOpacity > 0, mapKnown, mapShown, popup);
        /* SQ5 (owner 2026-09-29, drive photos): no arrow box over the FULL cockpit view (Google's card shows the
         * same turn), in any mode - near a turn the VC also opens its KDK tile (STOCK mode). Keep it in the Sport
         * view: the VC's large-map flag (popup) is set there too on this car, so the full/small decision is
         * ScreenModule.isSmallScreenViewArea(), the same signal the cluster player uses. HUD (BAP) unchanged.
         * TurnCard.isMapCard (sq5_turncard_map_on / menu) shows it in the full view again. */
        /* Run 84/85: at the switch to the full view the VC reports largeMapView ~150 ms before ScreenModule's
         * small flag clears, and luka placed the CARD then. CARD only exists in the large map view, so it is
         * hidden whatever the small flag says; the Sport tile (STOCK, inTube) keys on the small flag. */
        boolean hideArrow = mirrorActive && com.sq5.aa.luka.TurnCard.isEnabled() && !com.sq5.aa.luka.TurnCard.isMapCard()
            && mode == com.sq5.aa.luka.TurnCard.MODE_CARD;   /* 2026-09-30: "Audi" = only our fill-in card is dropped; Audi's own tile is never blanked */
        arrowHidden = hideArrow;
        if (hideArrow && mode == com.sq5.aa.luka.TurnCard.MODE_CARD) mode = com.sq5.aa.luka.TurnCard.MODE_STOCK;
        int carplayOpacity = mode == com.sq5.aa.luka.TurnCard.MODE_CARD || mode == com.sq5.aa.luka.TurnCard.MODE_PREARM
            ? 100 : (carplayOwnsCluster && navActive ? permittedOpacity : 0);
        if (hideArrow) carplayOpacity = 0;
        boolean card = mode == com.sq5.aa.luka.TurnCard.MODE_CARD;
        boolean cardBacking = card && !com.sq5.aa.luka.MirrorGate.drawsCardPanel();
        logDecision(geometry, popup, carplayOpacity, mode, cardBacking);
        int[] mirrorPos = applyMirror(dm, terminal, geometry, mirrorActive);
        publishTurnCard(mirrorActive && navActive, card, mirrorPos);
        boolean viewportChanged = false;
        if (mode != lastPlaneMode) {
            viewportChanged = card != cardViewport;
            cardViewport = card;
            lastPlaneMode = mode;
        }
        applyPlanes(dm, terminal, geometry, stockVisible, stockOpacity, popup, carplayOwnsCluster, navActive,
            card, cardBacking, carplayOpacity);
        return viewportChanged;
    }

    private static void applyPlanes(IDisplayManagerKombiControl dm, int terminal, Geometry geometry,
                                    boolean stockVisible, int stockOpacity, boolean popup,
                                    boolean carplayOwnsCluster, boolean navActive, boolean card,
                                    boolean cardBacking, int carplayOpacity) {
        try {
            /* 101/102 are shared with the stock KDK renderer.  Restore the last stock model
             * when CarPlay releases terminal 1; otherwise a disconnect can leave
             * Audi navigation's backing permanently transparent until an unrelated KDK delta. */
            if (!carplayOwnsCluster) {
                dm.setOpacity(MANEUVER, terminal, 0);
                maneuverOpacityPrimedDm = dm;
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
                maneuverOpacityPrimedDm = dm;
                dm.setOpacity(BACKING_SPORT, terminal, 0);
                dm.setOpacity(BACKING_POPUP, terminal, 0);
                return;
            }
            if (card) {
                /* SQ5 AA port: VC large map view shows the whole HU frame and hides its KDK tile ->
                 * 98 is our turn card (full 328x180 renderer frame) at TurnCard's rect.  The mirror
                 * draws the card panel behind it when it advertises tc1; otherwise the 328x180
                 * sport backing (101) goes under the card. */
                int[] crop = com.sq5.aa.luka.TurnCard.cardCrop();
                int cx = com.sq5.aa.luka.TurnCard.cardX(), cy = com.sq5.aa.luka.TurnCard.cardY();
                dm.setCropping(MANEUVER, terminal, crop[0], crop[1], crop[2], crop[3], cx, cy, crop[2], crop[3]);
                if (maneuverOpacityPrimedDm != dm) {
                    dm.setOpacity(MANEUVER, terminal, 0);      /* SQ5: defeat the 100 cache default */
                    maneuverOpacityPrimedDm = dm;
                }
                dm.setOpacity(MANEUVER, terminal, 100);
                if (cardBacking) {
                    dm.setPosition(BACKING_SPORT, terminal, cx, cy);
                    dm.setOpacity(BACKING_SPORT, terminal, 100);
                } else {
                    dm.setOpacity(BACKING_SPORT, terminal, 0);
                }
                dm.setOpacity(BACKING_POPUP, terminal, 0);
                errorLogged = false;
                return;
            }
            // One composition path for both stock stages; visibility is independent.
            int cx = popup ? geometry.popupCropX : geometry.inTubeCropX;
            int cy = popup ? geometry.popupCropY : geometry.inTubeCropY;
            int cw = popup ? geometry.popupCropW : geometry.inTubeCropW;
            int ch = popup ? geometry.popupCropH : geometry.inTubeCropH;
            int dx = popup ? geometry.popupX : geometry.inTubeX;
            int dy = popup ? geometry.popupY : geometry.inTubeY;
            int backing = popup ? BACKING_POPUP : BACKING_SPORT;
            int otherBacking = popup ? BACKING_SPORT : BACKING_POPUP;
            dm.setCropping(MANEUVER, terminal, cx, cy, cw, ch, dx, dy, cw, ch);
            if (carplayOpacity != 0 && maneuverOpacityPrimedDm != dm) {
                dm.setOpacity(MANEUVER, terminal, 0);          /* SQ5: defeat the 100 cache default */
                maneuverOpacityPrimedDm = dm;
            }
            dm.setOpacity(MANEUVER, terminal, carplayOpacity);
            dm.setPosition(backing, terminal, dx, dy);
            dm.setOpacity(backing, terminal, carplayOpacity);
            dm.setOpacity(otherBacking, terminal, 0);
            errorLogged = false;
        } catch (Throwable t) {
            /* Never throw into HMI/DM threads, but keep the first failure diagnosable. */
            if (!errorLogged) {
                errorLogged = true;
                Log.w("ClusterLayers", "apply failed: " + t);
            }
        }
    }

    /** SQ5 AA port: plane 99 goes exactly where stock puts the native map 33 (positionMap rule),
     *  opaque only while ctx 81/82 is physically selected.  On every entry write 0 then 100: the
     *  Java DisplayManager's opacity cache starts at 100, so a lone 100 could be skipped. */
    private static int[] applyMirror(IDisplayManagerKombiControl dm, int terminal, Geometry g, boolean active) {
        int[] pos = null;
        try {
            if (active) {
                boolean small = com.luka.carplay.core.ScreenModule.isSmallScreenViewArea();
                int x = g.mapX + (small ? g.smallStageDX : 0);
                int y = g.mapY + (small ? g.smallStageDY : 0);
                pos = new int[]{x, y};
                dm.setPosition(MIRROR, terminal, x, y);
                if (!mirrorShown) dm.setOpacity(MIRROR, terminal, 0);
                dm.setOpacity(MIRROR, terminal, 100);
                String sig = "pos=(" + x + "," + y + ") view=" + (small ? "single" : "full");
                if (!mirrorShown || !sig.equals(lastMirrorSignature)) {
                    int[] ext = null;
                    try { ext = dm.getExtends(NATIVE_MAP); } catch (Throwable t) { /* diagnostics only */ }
                    com.sq5.aa.luka.AaLog.log("layers: mirror 99 shown " + sig + " opacity 0->100; extents(33)="
                        + (ext == null ? "n/a" : intList(ext)));
                }
                mirrorShown = true;
                lastMirrorSignature = sig;
            } else {
                dm.setOpacity(MIRROR, terminal, 0);
                if (mirrorShown) com.sq5.aa.luka.AaLog.log("layers: mirror 99 hidden (opacity 0)");
                mirrorShown = false;
            }
        } catch (Throwable t) {
            if (!errorLogged) {
                errorLogged = true;
                Log.w("ClusterLayers", "mirror apply failed: " + t);
                com.sq5.aa.luka.AaLog.log("layers: mirror apply failed: " + t);
            }
        }
        return pos;
    }

    /** SQ5 AA port: tell sq5_mirror (TurnCardFeed) whether a route is shown and where the card sits in
     *  ITS frame (terminal rect minus the mirror plane position), so its panel/text line up with 98. */
    private static void publishTurnCard(boolean routeShown, boolean card, int[] mirrorPos) {
        if (card && mirrorPos != null) {
            com.sq5.aa.luka.TurnCardFeed.setLayer(routeShown, true,
                com.sq5.aa.luka.TurnCard.cardX() - mirrorPos[0], com.sq5.aa.luka.TurnCard.cardY() - mirrorPos[1],
                com.sq5.aa.luka.TurnCard.CARD_W, com.sq5.aa.luka.TurnCard.CARD_H);
        } else {
            com.sq5.aa.luka.TurnCardFeed.setLayer(routeShown, false, 0, 0, 0, 0);
        }
    }

    private static String intList(int[] a) {
        StringBuffer b = new StringBuffer("[");
        for (int i = 0; i < a.length; i++) b.append(i == 0 ? "" : ",").append(a[i]);
        return b.append(']').toString();
    }

    /** One line per distinct geometry decision — the exact numbers written to the DM.
     *  Every Classic/Sport/singlescreen bug so far was a guess about which branch ran; this makes
     *  it readable in /tmp/carplay_java.log instead. Logged only when the tuple changes. */
    private static void logDecision(Geometry g, boolean popup, int opacity, int mode, boolean cardBacking) {
        int cropX = popup ? g.popupCropX : g.inTubeCropX;
        int cropY = popup ? g.popupCropY : g.inTubeCropY;
        int cropW = popup ? g.popupCropW : g.inTubeCropW;
        int cropH = popup ? g.popupCropH : g.inTubeCropH;
        int dstX  = popup ? g.popupX     : g.inTubeX;
        int dstY  = popup ? g.popupY     : g.inTubeY;
        boolean card = mode == com.sq5.aa.luka.TurnCard.MODE_CARD;
        if (card) {
            int[] c = com.sq5.aa.luka.TurnCard.cardCrop();
            cropX = c[0]; cropY = c[1]; cropW = c[2]; cropH = c[3];
            dstX = com.sq5.aa.luka.TurnCard.cardX();
            dstY = com.sq5.aa.luka.TurnCard.cardY();
        }
        boolean mapKnown, mapShown, kdk;
        synchronized (LOCK) {
            mapKnown = haveVcMapView;
            mapShown = vcMapView;
            kdk = haveVcVisibility && vcVisible;
        }

        String line = "apply " + g.layoutName
            + " mode=" + com.sq5.aa.luka.TurnCard.modeName(mode)
            + " kdk=" + kdk + " lvdsMap=" + (mapKnown ? String.valueOf(mapShown) : "?")
            + " mirror=" + com.luka.carplay.core.ScreenModule.isMirrorActive()
            + " view=" + (com.luka.carplay.core.ScreenModule.isSmallScreenViewArea()
                          ? "single" : "full")
            + " stage=" + (popup ? "popup" : "inTube")
            + " backing=" + (card ? (cardBacking ? String.valueOf(BACKING_SPORT) : "none(mirror panel)")
                : String.valueOf(popup ? BACKING_POPUP : BACKING_SPORT))
            + " opacity=" + opacity
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
        com.sq5.aa.luka.AaLog.log("layers: " + line + " owner=" + com.luka.carplay.core.ScreenModule.isConnected()
            + " nav=" + com.luka.carplay.core.ScreenModule.isNavActive());
    }

}
