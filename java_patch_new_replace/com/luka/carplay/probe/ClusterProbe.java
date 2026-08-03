/*
 * ClusterProbe — one-shot cluster diagnostics (gated, inert by default).
 *
 * Answers the gating questions for the CarPlay-cluster view-mode design
 * (docs/CLUSTER_CARPLAY_DESIGN.md §10) on ONE instrumented run, with NO reflection —
 * the patched ClusterViewMode/ClusterService/DisplayManagerMIB2High call in directly:
 *   R1  getKombiType()                       (is ctx 80 even valid? != 4)
 *   R2  isClusterMapFPK/MOST/MMI + gfx/mapReady + which mode setFavoredViewMode(3) yields
 *   R3  every switchDisplayContextKombi(8/9) caller + the FINAL terminal-1 context/displayables
 *
 * Enable on-unit (no relaunch-arg needed): `touch /tmp/carplay_clusterprobe` then reboot/reconnect.
 * Output goes to the shared /tmp/carplay_java.log (via Log). ~20 s after the cluster service
 * starts it force-drives setFavoredViewMode(MAP=3) once and lets the patched classes log the outcome.
 *
 * Java 1.4 / Foundation 1.1: no generics/autoboxing; StringBuffer; no reflection.
 */
package com.luka.carplay.probe;

import com.luka.carplay.framework.Log;
import de.audi.atip.base.IFrameworkAccess;
import de.audi.tghu.navi.app.cluster.ClusterInputListener;
import de.audi.tghu.navi.app.util.Util;

import java.io.ByteArrayOutputStream;
import java.io.PrintStream;

public final class ClusterProbe {
    private static final String TAG = "ClusterProbe";

    private ClusterProbe() {}

    /* TEMPORARY: always-on while we characterize the cluster (no /tmp marker needed).
     * Revert to a gate (or delete the probe) once R1/R2/R3 are answered — the call sites
     * are all `if (ClusterProbe.on()) ...` so flipping this one method disables everything.
     * NOTE: with this true, the one-shot forces setFavoredViewMode(MAP) ~20 s after every
     * cluster start — intended for the diagnostic build; remove with the logging later. */
    public static boolean on() { return true; }

    public static void log(String s) { Log.i(TAG, s); }

    /* top of ClusterService.switchDisplayContextKombi(i) — who asked for 8/9 */
    public static void onSwitchKombi(int i) {
        StringBuffer sb = new StringBuffer("switchDisplayContextKombi(");
        sb.append(i).append(") caller:");
        try {
            ByteArrayOutputStream bos = new ByteArrayOutputStream();
            new Throwable().printStackTrace(new PrintStream(bos));
            sb.append('\n').append(bos.toString());
        } catch (Throwable t) { sb.append(" <stack n/a>"); }
        log(sb.toString());
    }

    /* inside DisplayManagerMIB2High.switchContext — requested vs final ctx + planes */
    public static void onSwitchContext(int requested, int finalCtx, int terminal, int visibleKDK, int[] displ) {
        StringBuffer sb = new StringBuffer("switchContext req=");
        sb.append(requested).append(" final=").append(finalCtx).append(" term=").append(terminal)
          .append(" visibleKDK=").append(visibleKDK).append(" displayables=[");
        if (displ != null) {
            for (int k = 0; k < displ.length; k++) { if (k > 0) sb.append(','); sb.append(displ[k]); }
        }
        sb.append(']');
        log(sb.toString());
    }

    public static void logConfig(String tag, IFrameworkAccess fw) {
        StringBuffer sb = new StringBuffer("CONFIG[").append(tag).append("] ");
        try { sb.append("kombiType=").append(fw.getKombiType()); } catch (Throwable t) { sb.append("kombiType=?"); }
        sb.append(" mapAvail=").append(Util.isClusterMapAvailable(fw));
        sb.append(" mapFPK=").append(Util.isClusterMapFPK(fw));
        sb.append(" mapMOST=").append(Util.isClusterMapMOST(fw));
        sb.append(" mmi=").append(Util.isClusterMMI(fw));
        sb.append(" mostAlwaysOn=").append(Util.isClusterMapMOSTAlwaysOn());
        sb.append(" mapAlwaysOn=").append(Util.isClusterMapAlwaysOn());
        log(sb.toString());
    }

    /* Fire once ~20 s after the cluster starts: dump config, force MAP(3), let the patched
     * ClusterViewMode/ClusterService/DisplayManager log whether it reaches ctx 8 = {98,102,101,99}. */
    public static void armOneShot(final IFrameworkAccess fw, final ClusterInputListener cil) {
        Thread t = new Thread(new Runnable() {
            public void run() {
                try { Thread.sleep(20000L); } catch (InterruptedException e) {}
                log("=== one-shot BEGIN (forcing MAP view mode to probe ctx-80 reachability) ===");
                logConfig("preMAP", fw);
                try {
                    log("calling ClusterInputListener.setFavoredViewMode(3=MAP, false)");
                    cil.setFavoredViewMode(3, false);
                } catch (Throwable ex) {
                    log("setFavoredViewMode(3) THREW: " + ex);
                }
                try { Thread.sleep(2000L); } catch (InterruptedException e) {}
                log("=== one-shot END (see switchDisplayContextKombi / switchContext lines above) ===");
            }
        }, "ClusterProbe-oneShot");
        t.setDaemon(true);
        t.start();
    }
}
