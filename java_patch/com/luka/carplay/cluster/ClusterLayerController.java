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
 * Geometry is HARDCODED (tuned on-unit against the red popup-crop frame, CR_POPUP=(59,27,210,153) in
 * maneuver_render).  setCropping dst anchor != setPosition: crop dst = setPosition value + crop src
 * offset (pos 1032,83 => crop dst 1091,110 = +59,+27).  Values are final — the old
 * /tmp/cluster_planes.cfg live-tuning path is gone.
 */
package com.luka.carplay.cluster;

import com.luka.carplay.framework.Log;
import de.audi.tghu.fwhmi.IDisplayManagerKombiControl;

public final class ClusterLayerController {

    /* CarPlay cluster displayables (see dc[80] = {98,101,102,99} in DisplayManagerMIB2High). */
    private static final int MANEUVER      = 98;    /* maneuver_render (Software) */
    private static final int BACKING_SPORT = 101;   /* 987 KDK backing, 328x180 */
    private static final int BACKING_POPUP = 102;   /* 987 KDK backing, 210x153 */
    private static final Object LOCK = new Object();
    private static IDisplayManagerKombiControl lastDm;
    private static int lastTerminal;
    private static boolean lastPopup;
    private static boolean lastStockVisible;
    private static int lastStockOpacity;
    private static boolean haveLayout;
    private static boolean errorLogged;

    private ClusterLayerController() {}

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
     * @param popup true = 210x153 popup zone (hint&8 == 0); false = full 328x181 sport (hint&8 != 0).
     * Reads the nav-active gate itself; never throws into the HMI thread.
     */
    public static void apply(IDisplayManagerKombiControl dm, int terminal, boolean popup,
                             boolean stockVisible, int stockOpacity) {
        synchronized (LOCK) {
            lastDm = dm;
            lastTerminal = terminal;
            lastPopup = popup;
            lastStockVisible = stockVisible;
            lastStockOpacity = stockOpacity;
            haveLayout = true;
        }
        applyNow(dm, terminal, popup, stockVisible, stockOpacity);
    }

    /** Re-apply the last stock KDK geometry after AltScreenModule changes ctx 80/81. */
    public static void reapply() {
        IDisplayManagerKombiControl dm;
        int terminal;
        boolean popup;
        boolean stockVisible;
        int stockOpacity;
        synchronized (LOCK) {
            if (!haveLayout || lastDm == null) return;
            dm = lastDm;
            terminal = lastTerminal;
            popup = lastPopup;
            stockVisible = lastStockVisible;
            stockOpacity = lastStockOpacity;
        }
        applyNow(dm, terminal, popup, stockVisible, stockOpacity);
    }

    private static void applyNow(IDisplayManagerKombiControl dm, int terminal, boolean popup,
                                 boolean stockVisible, int stockOpacity) {
        boolean carplayOwnsCluster = com.luka.carplay.core.AltScreenModule.isConnected();
        boolean navActive = com.luka.carplay.core.AltScreenModule.isNavActive();
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
                /* popup: show only the 210x153 popup zone of 98, over the 210x153 backing 102 */
                dm.setCropping(MANEUVER, terminal, 59, 27, 210, 153, 1091, 110, 210, 153);
                dm.setOpacity(MANEUVER, terminal, 100);
                dm.setPosition(BACKING_POPUP, terminal, 1091, 110);
                dm.setOpacity(BACKING_POPUP, terminal, 100);
                dm.setOpacity(BACKING_SPORT, terminal, 0);
            } else {
                /* sport: full 328x181 of 98, over the 328x180 backing 101 */
                dm.setCropping(MANEUVER, terminal, 0, 0, 328, 181, 984, 139, 328, 181);
                dm.setOpacity(MANEUVER, terminal, 100);
                dm.setPosition(BACKING_SPORT, terminal, 984, 139);
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
}
