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

    public String name() { return "rgd"; }

    public boolean start(FrameworkRef fw) {
        if (fw == null || !fw.isReady()) return false;             /* framework not up → retry */

        Object navi = fw.getService(CombiBAPServiceNavi.class);    /* appears ~a beat after activate */
        if (navi == null) {
            Log.d(TAG, "CombiBAPServiceNavi not up yet — retry");
            return false;
        }

        if (rg == null) rg = new RouteGuidance();
        if (!rg.init(navi)) {                                      /* BAPBridge init (ClusterService) not ready */
            Log.d(TAG, "RouteGuidance.init not ready — retry");
            return false;
        }
        rg.start();                                               /* subscribe to EVT_RGD_UPDATE */
        return true;
    }

    public void stop() {
        if (rg != null) { rg.stop(); rg = null; }
    }
}
