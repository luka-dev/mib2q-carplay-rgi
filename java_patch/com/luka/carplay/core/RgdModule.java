/*
 * RgdModule — CarPlay route-guidance feature as a CarPlayApp Module.
 *
 * Adapter over the ported RouteGuidance/BAPBridge chain.  start() implements the
 * proven retry-until-ready gate: CombiBAPServiceNavi (and the ClusterService BAP
 * listener it drives) appears a beat after CarPlay activate, so we return false
 * until the service is registered — CarPlayApp.startRetry() calls us again.
 */
package com.luka.carplay.core;

import com.luka.carplay.framework.Log;
import com.luka.carplay.rgd.RouteGuidance;

import de.audi.atip.interapp.combi.bap.navi.CombiBAPServiceNavi;

final class RgdModule implements Module {

    private static final String TAG = "RgdModule";
    private RouteGuidance rg;
    private FrameworkRef.ServiceHandle naviHandle;

    public String name() { return "rgd"; }

    public boolean start(FrameworkRef fw) {
        if (fw == null || !fw.isReady()) return false;             /* framework not up → retry */
        if (!ScreenModule.isPlatformSupported(fw)) {
            Log.w(TAG, "disabled on unsupported G24 cluster");
            return true;
        }

        if (naviHandle == null) naviHandle = fw.getServiceHandle(CombiBAPServiceNavi.class);
        Object navi = naviHandle != null ? naviHandle.service() : null;
        if (navi == null) {
            return false;
        }

        /* Init + subscribe ONCE.  Guard against re-running rg.init() on a retry (it builds a
         * fresh BAPBridge each call → would re-wrap the gate every retry).  rg.start() is
         * idempotent (returns early if already running). */
        if (rg == null) {
            RouteGuidance r = new RouteGuidance();
            if (!r.init(navi)) {                                  /* BAPBridge init (ClusterService) not ready */
                return false;
            }
            rg = r;
        }
        /* REPLACE: don't report started until the RG gate is actually shut.  engageTakeover
         * returns false while ClusterService isn't up yet → CarPlayApp keeps retrying, so a
         * connected session with no CarPlay navigation still gets stock RG blocked. */
        if (!rg.engageTakeover()) {
            return false;
        }
        if (!rg.isRunning()) rg.start();                           /* subscribe only after gate is shut */
        return true;
    }

    public void stop() {
        if (rg != null) { rg.stop(); rg.disengageTakeover(); rg = null; }
        if (naviHandle != null) { naviHandle.release(); naviHandle = null; }
    }
}
