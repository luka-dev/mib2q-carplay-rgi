/*
 * SQ5 AA port: VcMapViewGate's re-forward target.  Holds the live stock ClusterService and applies
 * the gate's current GAL value to its CombiBAPListener on the Navigation dispatcher (NavigationJobs),
 * the thread that owns CombiBAPListener.combiservice (see ScreenNavStatusGate).
 */
package com.sq5.aa.luka;

import de.audi.tghu.navi.app.Navigation;
import de.audi.tghu.navi.app.cluster.ClusterService;

public final class ClusterGalSink implements VcMapViewGate.Sink {
    private static ClusterGalSink last;

    private final ClusterService cs;

    private ClusterGalSink(ClusterService cs) {
        this.cs = cs;
    }

    /** One sink per ClusterService instance (the stock service is replaced only on nav restart). */
    public static synchronized ClusterGalSink of(ClusterService cs) {
        if (last == null || last.cs != cs) last = new ClusterGalSink(cs);
        return last;
    }

    public void forwardLater() {
        Runnable r = new Runnable() {
            public void run() {
                try {
                    boolean v = VcMapViewGate.takeEffective();
                    cs.sq5ForwardGALState(v);
                    AaLog.log("vcmap: CombiBAPListener.setGALState(" + v + ") applied");
                } catch (Throwable t) {
                    AaLog.log("vcmap: setGALState re-forward failed: " + t);
                }
            }
        };
        Navigation navigation = null;
        try {
            navigation = Navigation.getInstance();
        } catch (Throwable t) {
            /* fall through: run inline */
        }
        if (navigation != null && navigation.getDispatcher() != null) {
            navigation.getDispatcher().execute(r);
        } else {
            AaLog.log("vcmap: no Navigation dispatcher, re-forwarding inline");
            r.run();
        }
    }
}
