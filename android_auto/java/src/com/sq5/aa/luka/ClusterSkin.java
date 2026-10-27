/*
 * SQ5 AA port: VC skin (gauge style) for the cockpit map layout (owner 2026-09-30).
 *
 * The small cockpit view comes in two different shapes: CLASSIC (two large dials, map centred between them)
 * and SPORT (one large centre dial, map on the left). One small layout cannot suit both, so the cluster
 * hook sends a different Android Auto layout (0x8009) for each. The skin is HMITerminalEvo.getSkin() on
 * terminal 0 (1 = sport, else classic; the same test the MIBSI package uses). Published to
 * /tmp/sq5_cluster_skin as a fixed 8-byte record rewritten in place ("sport  \n" / "classic\n"); /tmp is
 * /dev/shmem (no truncate-safe rename). Polled every 500 ms: a skin change does not always arrive with a
 * view-size event.
 */
package com.sq5.aa.luka;

import java.io.RandomAccessFile;

public final class ClusterSkin {
    public static final String PATH = "/tmp/sq5_cluster_skin";
    private static volatile Object source;          /* IHMIServiceEvo from CombiMapController */
    private static Thread poller;
    private static int lastSkin = -2;

    private ClusterSkin() {
    }

    /** CombiMapController.syncViewSize: remember the HMI service and start polling once. */
    public static synchronized void setSource(Object hmiService) {
        if (hmiService == null) return;
        source = hmiService;
        if (poller != null) return;
        poller = new Thread("sq5-cluster-skin") {
            public void run() {
                while (true) {
                    try { poll(); } catch (Throwable t) { /* keep polling */ }
                    try { Thread.sleep(500L); } catch (InterruptedException e) { return; }
                }
            }
        };
        poller.setDaemon(true);
        poller.start();
    }

    static int readSkin() {
        Object s = source;
        if (!(s instanceof de.audi.tghu.hmi.evo.IHMIServiceEvo)) return -1;
        Object term = ((de.audi.tghu.hmi.evo.IHMIServiceEvo) s).getHMITerminal(0);
        if (term instanceof de.audi.tghu.hmi.evo.HMITerminalEvo) return ((de.audi.tghu.hmi.evo.HMITerminalEvo) term).getSkin();
        return -1;
    }

    /** Pure mapping (host-tested): 1 = sport, anything else = classic. */
    public static String record(int skin) {
        return skin == 1 ? "sport  \n" : "classic\n";
    }

    /* Run 120 (owner photo): getSkin() stayed 0 on this car in the Sport skin, so the phone got the Classic layout.
     * The skin switch swaps the VC Layout object (LayoutMIB2HighB9Sport, small stage -476 vs LayoutMIB2HighB9,
     * small stage 0; ClusterLayerController logs it) -> that class name decides; getSkin() only before it. */
    private static volatile int layoutSkin = -1;

    /** ClusterLayerController on every Layout change. Pure part: skinOfLayout (host-tested). */
    public static void setLayout(String layoutName) {
        int s = skinOfLayout(layoutName);
        if (s < 0) return;
        layoutSkin = s;
        try { poll(); } catch (Throwable t) { /* the poller retries */ }
    }

    public static int skinOfLayout(String n) {
        if (n == null || n.indexOf("LayoutMIB2HighB9") < 0) return -1;
        return n.endsWith("Sport") ? 1 : 0;
    }

    private static synchronized void poll() {
        int skin = layoutSkin >= 0 ? layoutSkin : readSkin();
        if (skin == lastSkin) return;
        RandomAccessFile f = null;
        try {
            f = new RandomAccessFile(PATH, "rw");
            f.seek(0);
            f.write(record(skin).getBytes());
            lastSkin = skin;
            AaLog.log("cluster skin " + skin + " -> " + record(skin).trim());
        } catch (Throwable t) {
            /* retried on the next poll */
        } finally {
            if (f != null) try { f.close(); } catch (Throwable t) { /* ignore */ }
        }
    }
}
