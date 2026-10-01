/*
 * SQ5 AA port: keep the Virtual Cockpit in its map view while the Android Auto mirror is shown.
 *
 * Stock mechanism (MU0918 lsd.jar; CombiBAPListener/ClusterService lines refer to re/src):
 *   Android Auto navFocusRequestNotification(PROJECTED)
 *     -> AndroidAuto2NavHandler.requestDSIUpdate(NAVI, DEVICE)       (terminal mode owner NAVI = phone)
 *     -> TerminalModeAppState{appId 1, runningOnDevice}              (TMInterAppHandler -> listeners)
 *     -> de.audi.tghu.navi.app.gal.GALHandler.updateNaviAppState(true)
 *     -> ClusterService.updateGALState(true) -> CombiBAPListener.setGALState(true)
 *     -> CombiBAPListener.updateInfoStates(): naviIsRunningOnSmartphone ? 6
 *     -> AppConnectorNavi.updateInfoStates(6) = BAP NavSD FctID 38 InfoStates = 6 ("mobile device")
 *   and the VC leaves its map content for the compass / mobile-device layout, hiding plane 99.
 *
 * This gate suppresses exactly that one input (setGALState(true)) while terminal 1 physically
 * carries a mirror context (81/82, ScreenModule.isMirrorActive()).  Everything else in the
 * InfoStates computation (GPS / navi / splash states) stays stock.  Outside the mirror contexts,
 * or with the SD flag sq5_vcmap_nogate, stock behaviour is unchanged.  Every suppression and every
 * re-forward is written to /tmp/sq5_aa.log.
 *
 * Threading: the stock path runs on the caller's thread exactly as stock does (the value is simply
 * replaced).  Mirror edges come from ScreenModule's switch worker; their re-forward goes through
 * Sink.forwardLater() (production: the Navigation dispatcher, like ScreenNavStatusGate), and the
 * value is read at execution time (takeEffective) so a late runnable never applies a stale value.
 */
package com.sq5.aa.luka;

public final class VcMapViewGate {
    /** Re-forward target: production = ClusterGalSink (ClusterService on NavigationJobs). */
    public interface Sink {
        /** Schedule takeEffective() -> CombiBAPListener.setGALState(value). */
        void forwardLater();
    }

    /** Stock FPK ActiveRGType (CombiBAPListener.updateActiveRGType: isClusterMapFPK -> 4). */
    public static final int STOCK_FPK_RG_TYPE = 4;

    private static final Object LOCK = new Object();
    private static boolean enabled = true;          /* SD flag sq5_vcmap_nogate turns it off */
    private static volatile boolean rgTypeStock;    /* SD flag sq5_rgtype4: luka BAPBridge sends 4 */
    private static boolean haveStock;
    private static boolean stockGal;
    private static boolean mirror;
    private static int forwarded = -1;              /* last value handed to CombiBAPListener */
    private static Sink sink;

    private VcMapViewGate() {
    }

    /** The one rule (pure): the phone-navigation flag reaches the VC unless the mirror is shown. */
    public static boolean effective(boolean stockGalState, boolean mirrorShown, boolean gateEnabled) {
        return stockGalState && !(gateEnabled && mirrorShown);
    }

    /** ActiveRGType for luka's BAPBridge (FctID 39): luka default unless sq5_rgtype4 asks for stock FPK. */
    public static int activeRgType(int lukaDefault) {
        return rgTypeStock ? STOCK_FPK_RG_TYPE : lukaDefault;
    }

    public static void configure(boolean gateEnabled, boolean useStockRgType) {
        synchronized (LOCK) {
            enabled = gateEnabled;
        }
        rgTypeStock = useStockRgType;
        AaLog.log("vcmap: gate " + (gateEnabled ? "on" : "OFF (sq5_vcmap_nogate)")
            + ", BAP ActiveRGType " + (useStockRgType ? "4 (sq5_rgtype4, stock FPK)" : "luka default"));
    }

    /**
     * Stock path: ClusterService.updateGALState(flag), on the stock caller's thread.
     * Returns the value to hand to CombiBAPListener.setGALState in place of flag.
     */
    public static boolean onStockGalState(Sink s, boolean flag) {
        boolean eff, m, repeat;
        synchronized (LOCK) {
            if (s != null) sink = s;
            m = mirror;
            eff = effective(flag, m, enabled);
            /* GALHandler re-sends on every terminal-mode app-state change: log changes only */
            repeat = haveStock && stockGal == flag && forwarded == (eff ? 1 : 0);
            haveStock = true;
            stockGal = flag;
            forwarded = eff ? 1 : 0;
        }
        if (repeat) {
            return eff;
        }
        if (eff != flag) {
            AaLog.log("vcmap: SUPPRESSED stock setGALState(true) (Android Auto took navigation focus;"
                + " would send BAP InfoStates=6 and hide the VC map) - mirror ctx shown, forwarding false");
        } else {
            AaLog.log("vcmap: stock setGALState(" + flag + ") forwarded (mirror=" + m + ")");
        }
        return eff;
    }

    /** ScreenModule worker, after every completed context switch on terminal 1. */
    public static void onMirrorActive(boolean active) {
        Sink s;
        boolean eff, sg;
        synchronized (LOCK) {
            if (mirror == active) return;
            mirror = active;
            sg = stockGal;
            if (!haveStock || sink == null) {
                s = null;
                eff = false;
            } else {
                eff = effective(stockGal, active, enabled);
                s = (forwarded == (eff ? 1 : 0)) ? null : sink;
            }
        }
        if (s == null) {
            AaLog.log("vcmap: mirror ctx " + (active ? "shown" : "left") + " (stock GAL state "
                + (haveStockUnlocked() ? String.valueOf(sg) : "not seen yet") + ", nothing to re-forward)");
            return;
        }
        AaLog.log("vcmap: mirror ctx " + (active ? "shown" : "left") + " -> re-forward setGALState(" + eff
            + ") (stock GAL state " + sg + ")" + (active ? ": SUPPRESSING InfoStates=6" : ": stock restored"));
        try {
            s.forwardLater();
        } catch (Throwable t) {
            AaLog.log("vcmap: re-forward failed: " + t);
        }
    }

    /** Called by the Sink when its scheduled forward runs: the value to apply now. */
    public static boolean takeEffective() {
        synchronized (LOCK) {
            boolean eff = effective(stockGal, mirror, enabled);
            forwarded = eff ? 1 : 0;
            return eff;
        }
    }

    private static boolean haveStockUnlocked() {
        synchronized (LOCK) {
            return haveStock;
        }
    }

    /** Host tests only. */
    public static void resetForTest() {
        synchronized (LOCK) {
            enabled = true;
            haveStock = false;
            stockGal = false;
            mirror = false;
            forwarded = -1;
            sink = null;
        }
        rgTypeStock = false;
    }
}
