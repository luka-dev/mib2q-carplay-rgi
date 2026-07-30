/*
 * SmartphoneTabProbe — MINIMAL standalone probe (no hook, no bus, no renderers).
 *
 * Purpose: on CarPlay connect, subscribe to DSIKombiSync and send ONLY the tab-
 * announcement, to test on-unit whether the A5 VC shows the Smartphone/CarPlay cluster
 * tab via the DSIKombiSync channel.  Does NOT touch the video stream, does NOT switch
 * any DisplayManager context, does NOT run any other module.
 *
 *   setMenuState(smartPhone = AVAILABLE)               -> tab exists / selectable
 *   setMMIDisplayStatus(LVDS_DM_ACTIVE, mainContext=117) -> "the current LVDS stream is CarPlay"
 *
 * Logs are appended to /tmp/carplay_tabprobe.log (same file-logging approach as the
 * old hook), each line prefixed "[TabProbe]".
 * Java 1.4 target.  Subscription rides the stock AbstractDSIController plumbing.
 */
package com.luka.carplay.probe;

import java.io.FileWriter;

import de.audi.app.terminalmode.IContext;
import de.audi.app.terminalmode.dsi.AbstractDSIController;

import org.dsi.ifc.base.DSIBase;
import org.dsi.ifc.base.DSIListener;
import org.dsi.ifc.kombisync2.DSIKombiSync;
import org.dsi.ifc.kombisync2.DSIKombiSyncListener;
import org.dsi.ifc.kombisync2.MenuState;
import org.dsi.ifc.kombisync2.DisplayStatus;
import org.dsi.ifc.kombisync2.DisplayStatusFlags;
import org.dsi.ifc.kombisync2.DisplayRequestResponse;
import org.dsi.ifc.kombisync2.DisplayIdentification;
import org.dsi.ifc.kombisync2.PopupRegisterRequestResponse;
import org.dsi.ifc.kombisync2.PopupActionRequestResponse;
import org.dsi.ifc.kombisync2.PopupStatus;

public final class SmartphoneTabProbe extends AbstractDSIController {

    private static final int SMARTPHONE_MAINCONTEXT = 117;   /* 0x75 */
    private static final String LOG_PATH = "/tmp/carplay_tabprobe.log";

    private static SmartphoneTabProbe instance;
    private volatile DSIKombiSync svc;
    private final Listener listener = new Listener();

    /* Append one line to /tmp/carplay_tabprobe.log (best-effort; also echoes to stdout). */
    static void log(String msg) {
        String line = "[TabProbe] " + msg;
        System.out.println(line);
        FileWriter fw = null;
        try { fw = new FileWriter(LOG_PATH, true); fw.write(line); fw.write("\n"); }
        catch (Throwable t) { /* ignore */ }
        finally { if (fw != null) try { fw.close(); } catch (Throwable t) { } }
    }

    private SmartphoneTabProbe(IContext ctx) {
        super(0, ctx.getServiceManager(), ctx.getFramework(), ctx.getDispatcher(), true,
              ctx.getLogger().dsi());
    }

    /* Dump the coding/sysConst that license the SmartPhone MMI_Status slot.
     * isAppleDIO (CarPlay) / isGoogleGAL (AA) live in the ANP Adaptation channel — NOT in the
     * head-unit long coding — and drive sysConst 4237/4238, which the native DSIKombiSync composer
     * reads to set MMI_Status.SmartPhone = AVAILABLE(2) at boot.  If both are false here, the
     * native side won't raise the tab regardless of our setMenuState. */
    static void dumpCoding(IContext ctx) {
        try {
            de.audi.atip.sysapp.carcoding.ISysConstManager scm = ctx.getFramework().getSysConstManager();
            boolean appleDIO  = scm.getAdaptationANP().isAppleDIO();
            boolean googleGAL = scm.getAdaptationANP().isGoogleGAL();
            int sc4237 = scm.getSysConst(4237);   /* = isAppleDIO  -> MMI_Status SmartPhone (CarPlay) */
            int sc4238 = scm.getSysConst(4238);   /* = isGoogleGAL -> MMI_Status SmartPhone (AndroidAuto) */
            log("coding: isAppleDIO=" + appleDIO + " isGoogleGAL=" + googleGAL
                + " | sysConst[4237]=" + sc4237 + " sysConst[4238]=" + sc4238
                + "  (SmartPhone slot licensed when any = 1/true)");
        } catch (Throwable t) {
            log("coding dump FAILED: " + t);
        }
    }

    /* Called from the patched TerminalModeBapCombi seam on CarPlay connect / disconnect. */
    public static synchronized void onConnect(IContext ctx) {
        if (instance != null) return;
        try {
            dumpCoding(ctx);
            instance = new SmartphoneTabProbe(ctx);
            instance.init();
            log("onConnect: subscribing DSIKombiSync");
        } catch (Throwable t) {
            log("onConnect FAILED: " + t);
            instance = null;
        }
    }
    public static synchronized void onDisconnect() {
        if (instance == null) return;
        try { instance.retract(); instance.deinit(); } catch (Throwable t) { }
        instance = null;
    }

    /* Stock init() is a no-op; startDSI() actually creates the tracker + registers us. */
    public void init() {
        super.init();
        this.startDSI();
    }

    protected Class getDSIServiceClass()   { return DSIKombiSync.class; }
    protected DSIListener getDSIListener()  { return this.listener; }
    protected Class getDSIListenerClass()   { return DSIKombiSyncListener.class; }
    protected void addDSIService(DSIBase b) {
        this.svc = (DSIKombiSync) b;
        log("DSIKombiSync attached");
        announceAndClaim();
    }
    protected void removeDSIService() { this.svc = null; }

    private void announceAndClaim() {
        DSIKombiSync s = this.svc;
        if (s == null) return;
        try {
            MenuState ms = new MenuState();
            ms.smartPhone = DSIKombiSync.MENUSTATE_AVAILABLE;                                  /* 2 */
            s.setMenuState(ms);

            DisplayStatus ds = new DisplayStatus();
            ds.statusFlags = new DisplayStatusFlags(DSIKombiSync.DISPLAYSTATUS_LVDS_DM_ACTIVE); /* 8192 */
            ds.mainContext = SMARTPHONE_MAINCONTEXT;                                            /* 117 */
            s.setMMIDisplayStatus(ds);

            log("SENT: tab AVAILABLE + stream claimed CarPlay (ctx 117, LVDS_DM_ACTIVE)");
        } catch (Throwable t) {
            log("announce FAILED: " + t);
        }
    }

    private void retract() {
        DSIKombiSync s = this.svc;
        if (s == null) return;
        try {
            MenuState ms = new MenuState();
            ms.smartPhone = DSIKombiSync.MENUSTATE_NOT_EXISTING;                                /* 3 */
            s.setMenuState(ms);
            log("retracted (NOT_EXISTING)");
        } catch (Throwable t) { }
    }

    private final class Listener implements DSIKombiSyncListener {
        public void responseKombiDisplayRequestResponse(DisplayRequestResponse r) {
            log("responseDisplayRequestResponse mainContext="
                + (r != null ? r.mainContext : -1) + " focus=" + (r != null ? r.focus : -1));
        }
        public void updateKombiCommunicationState(boolean flag, int i) {}
        public void updateKombiMessageStateDisplayIdentification(int i, int j) {}
        public void updateKombiMessageStateDisplayRequestResponse(int i, int j) {}
        public void updateKombiMessageStateDisplayStatus(int i, int j) {}
        public void updateKombiMessageStatePopupActionRequest(int i, int j) {}
        public void updateKombiMessageStatePopupRegisterResponse(int i, int j) {}
        public void updateKombiMessageStatePopupStatus(int i, int j) {}
        public void responseKombiDisplayStatus(DisplayStatus s) {}
        public void responseKombiDisplayIdentification(DisplayIdentification d) {}
        public void responseKombiPopupRegisterResponse(PopupRegisterRequestResponse p) {}
        public void responseKombiPopupActionRequest(PopupActionRequestResponse p) {}
        public void responseKombiPopupStatus(PopupStatus p) {}
        public void asyncException(int i, String s, int j) {}
    }
}
