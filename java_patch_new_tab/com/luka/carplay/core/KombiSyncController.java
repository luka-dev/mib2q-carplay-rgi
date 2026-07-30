/*
 * KombiSyncController — subscribes to DSIKombiSync (v2) and tracks whether the VC's
 * Smartphone/CarPlay cluster tab (mainContext 0x75 = 117) currently has INPUT FOCUS.
 *
 * This is the correct gate for redirecting the MMI roller to the cluster
 * (CMD_ALT_HID) instead of the main-screen CarPlay: only while the user has focused
 * the altScreen tab do we steal the knob.  "cluster is rendering our content"
 * (AltScreenModule.isClusterActive) is NOT the same — that is true during route
 * guidance while the user still navigates the main screen.
 *
 * mainContext arrives asynchronously via DSIKombiSyncListener.responseKombiDisplayRequestResponse.
 * Subscription rides the stock AbstractDSIController service-tracker plumbing (same as
 * CarplayDSILifecycleController subscribes to DSICarplay).
 */
package com.luka.carplay.core;

import com.luka.carplay.framework.Log;

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
import org.dsi.ifc.kombisync2.DisplayStatus;
import org.dsi.ifc.kombisync2.DisplayIdentification;
import org.dsi.ifc.kombisync2.PopupRegisterRequestResponse;
import org.dsi.ifc.kombisync2.PopupActionRequestResponse;
import org.dsi.ifc.kombisync2.PopupStatus;

public final class KombiSyncController extends AbstractDSIController {

    private static final String TAG = "KombiSync";
    private static final int SMARTPHONE_MAINCONTEXT = 117;   /* 0x75 = SMARTPHONE_CARPLAY on the VC */

    private static volatile boolean smartphoneFocused = false;
    public static boolean isSmartphoneTabFocused() { return smartphoneFocused; }

    private final Listener listener = new Listener();
    private volatile DSIKombiSync svc;

    public KombiSyncController(IContext ctx) {
        super(0, ctx.getServiceManager(), ctx.getFramework(), ctx.getDispatcher(), true,
              ctx.getLogger().dsi());
    }

    /* Stock AbstractDSIController.init() is a no-op; startDSI() is what actually
     * creates the service tracker + registers our DSIKombiSyncListener (mirrors
     * CarplayDSILifecycleController.init()).  Without this the listener never
     * attaches and mainContext focus is never observed. */
    public void init() {
        super.init();
        this.startDSI();
    }

    protected Class getDSIServiceClass()   { return DSIKombiSync.class; }
    protected DSIListener getDSIListener()  { return this.listener; }
    protected Class getDSIListenerClass()   { return DSIKombiSyncListener.class; }
    protected void addDSIService(DSIBase b) {
        this.svc = (DSIKombiSync) b;
        Log.i(TAG, "DSIKombiSync attached");
        announceAndClaim();   /* PROBE: on connect, announce the tab + claim the current stream as CarPlay */
    }
    protected void removeDSIService() {
        retract();
        this.svc = null;
        smartphoneFocused = false;
        /* Focus is now false — re-select the cluster context so an active ctx 82/83 falls back
         * to 80/74 (else losing the DSI service would strand the cluster on a video context). */
        try { AltScreenModule.refreshClusterContext(); } catch (Throwable t) { /* ignore */ }
    }

    /* PROBE (does NOT touch the actual video stream):
     *   1. setMenuState(smartPhone=AVAILABLE) — the Smartphone/CarPlay cluster tab exists/selectable.
     *   2. setMMIDisplayStatus(LVDS_DM_ACTIVE, mainContext=117) — declare that the LVDS/MOST stream
     *      currently on the cluster IS the CarPlay smartphone context (we relabel it, not re-render).
     * Purpose: resolve on-unit whether the A5 VC honours the DSIKombiSync channel for the tab. */
    private void announceAndClaim() {
        DSIKombiSync s = this.svc;
        if (s == null) return;
        try {
            MenuState ms = new MenuState();
            ms.smartPhone = DSIKombiSync.MENUSTATE_AVAILABLE;            /* 2 */
            s.setMenuState(ms);

            DisplayStatus ds = new DisplayStatus();
            ds.statusFlags = new DisplayStatusFlags(DSIKombiSync.DISPLAYSTATUS_LVDS_DM_ACTIVE);  /* 8192 */
            ds.mainContext = SMARTPHONE_MAINCONTEXT;                     /* 117 = 0x75 */
            s.setMMIDisplayStatus(ds);

            Log.i(TAG, "PROBE: tab AVAILABLE + stream claimed as CarPlay (ctx 117, LVDS_DM_ACTIVE)");
        } catch (Throwable t) {
            Log.w(TAG, "announceAndClaim failed: " + t);
        }
    }

    private void retract() {
        DSIKombiSync s = this.svc;
        if (s == null) return;
        try {
            MenuState ms = new MenuState();
            ms.smartPhone = DSIKombiSync.MENUSTATE_NOT_EXISTING;         /* 3 */
            s.setMenuState(ms);
            Log.i(TAG, "PROBE: tab retracted (NOT_EXISTING)");
        } catch (Throwable t) { /* best-effort */ }
    }

    private final class Listener implements DSIKombiSyncListener {
        public void responseKombiDisplayRequestResponse(DisplayRequestResponse r) {
            boolean f = (r != null && r.mainContext == SMARTPHONE_MAINCONTEXT);
            if (f != smartphoneFocused) {
                Log.i(TAG, "smartphone tab focus=" + f + " (mainContext=" + (r != null ? r.mainContext : -1) + ")");
                smartphoneFocused = f;
                /* Focus change re-selects the cluster context (80 popup <-> 82 video) if the
                 * CarPlay-nav renderer is up. */
                try { AltScreenModule.refreshClusterContext(); } catch (Throwable t) { /* ignore */ }
            } else {
                smartphoneFocused = f;
            }
        }

        /* --- unused DSIKombiSyncListener + DSIListener callbacks --- */
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
