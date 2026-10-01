import com.sq5.aa.luka.AaLog;
import com.sq5.aa.luka.VcMapViewGate;

import java.util.ArrayList;
import java.util.List;

/**
 * Host test of the VC map-view gate: stock ClusterService.updateGALState(true) (Android Auto took
 * navigation focus -> BAP InfoStates 6) is replaced by false only while the mirror context is shown,
 * and mirror edges re-forward the right value through the sink.
 */
public final class VcMapViewGateTest {
    static int failures, checks;
    static void check(boolean ok, String what) { checks++; if (!ok) { failures++; System.out.println("FAIL " + what); } }

    /** Records scheduled forwards; run() executes them like the Navigation dispatcher would. */
    static final class Sink implements VcMapViewGate.Sink {
        int scheduled;
        final List applied = new ArrayList();
        public void forwardLater() { scheduled++; }
        void run() { while (scheduled > 0) { scheduled--; applied.add(Boolean.valueOf(VcMapViewGate.takeEffective())); } }
        String take() { run(); String s = applied.toString(); applied.clear(); return s; }
    }

    public static void main(String[] args) {
        AaLog.setEnabled(false);

        /* pure rule */
        check(!VcMapViewGate.effective(false, false, true), "no phone nav -> false");
        check(VcMapViewGate.effective(true, false, true), "phone nav, no mirror -> stock true");
        check(!VcMapViewGate.effective(true, true, true), "phone nav + mirror shown -> suppressed");
        check(VcMapViewGate.effective(true, true, false), "gate disabled -> stock true");

        /* 1. no mirror: stock passes through untouched */
        VcMapViewGate.resetForTest();
        Sink s = new Sink();
        check(VcMapViewGate.onStockGalState(s, true), "no mirror: setGALState(true) forwarded as true");
        check(!VcMapViewGate.onStockGalState(s, false), "no mirror: false forwarded");

        /* 2. mirror shown first, then Android Auto takes navigation focus (the car-log order) */
        VcMapViewGate.onMirrorActive(true);
        check(s.take().equals("[]"), "mirror edge with stock false: nothing to re-forward");
        check(!VcMapViewGate.onStockGalState(s, true), "mirror shown: setGALState(true) -> false (InfoStates 6 suppressed)");
        check(s.take().equals("[]"), "stock path applies inline, no async forward");

        /* 3. mirror leaves (fallback to 80/74 or session end) -> stock value restored */
        VcMapViewGate.onMirrorActive(false);
        check(s.take().equals("[true]"), "mirror left: re-forward stock true");
        VcMapViewGate.onMirrorActive(false);
        check(s.take().equals("[]"), "repeated edge is a no-op");

        /* 4. mirror comes back while phone navigation already owns focus -> suppress again */
        VcMapViewGate.onMirrorActive(true);
        check(s.take().equals("[false]"), "mirror shown after focus: re-forward false");

        /* 5. focus returns to native nav while mirror shown -> false stays false, no extra forward */
        check(!VcMapViewGate.onStockGalState(s, false), "native focus while mirror: false");
        VcMapViewGate.onMirrorActive(false);
        check(s.take().equals("[]"), "mirror left with stock false: nothing to re-forward");

        /* 6. late runnable reads the value at execution time, never a stale one */
        VcMapViewGate.onStockGalState(s, true);          /* mirror off -> true forwarded inline */
        VcMapViewGate.onMirrorActive(true);              /* schedules false */
        VcMapViewGate.onMirrorActive(false);             /* forwarded is still true -> no new schedule */
        check(s.take().equals("[true]"), "stale scheduled forward applies the current value (true): ");

        /* 7. SD flag sq5_vcmap_nogate: stock behaviour even with the mirror shown */
        VcMapViewGate.resetForTest();
        VcMapViewGate.configure(false, false);
        Sink s2 = new Sink();
        VcMapViewGate.onMirrorActive(true);
        check(VcMapViewGate.onStockGalState(s2, true), "gate off: true forwarded while mirror shown");
        VcMapViewGate.onMirrorActive(false);
        check(s2.take().equals("[]"), "gate off: no re-forward");

        /* 8. ActiveRGType: luka default unless sq5_rgtype4 */
        VcMapViewGate.resetForTest();
        check(VcMapViewGate.activeRgType(0) == 0, "rgType default = luka 0");
        VcMapViewGate.configure(true, true);
        check(VcMapViewGate.activeRgType(0) == 4, "sq5_rgtype4 -> stock FPK 4");

        /* 9. edge before any stock value / sink: nothing forwarded, no NPE */
        VcMapViewGate.resetForTest();
        VcMapViewGate.onMirrorActive(true);
        VcMapViewGate.onMirrorActive(false);
        check(true, "edges without stock state are safe");

        System.out.println((failures == 0 ? "PASS" : "FAILED") + ": VcMapViewGateTest " + checks + " checks, " + failures + " failures");
        System.exit(failures == 0 ? 0 : 1);
    }
}
