/*
 * Reflection-injected wrapper for stock DSIKombiSyncProxy.
 *
 * The class-replacement approach (java_patch/de/esolutions/fw/dsi/...
 * DSIKombiSyncProvider.java) did not shadow stock lsd.jxe — observed
 * "no provider instance" in carplay_hook.log, confirming OSGi bundle
 * isolation kept our class out.  This wrapper sidesteps classloader
 * politics entirely: KombiInjector takes the LIVE stock provider, reads
 * its private `proxy` field, and replaces it with an instance of THIS
 * class that filters then delegates to the original proxy.
 *
 * We extend DSIKombiSyncProxy because the provider's field is typed as
 * the concrete class (not an interface).  Calling super(stockInstance,
 * stockReply) produces a duplicate inner core.Proxy that is never used
 * (we override getProxy() to return the delegate's), so the cost is
 * one stale Proxy object per session.
 */
package com.luka.carplay.kombi;

import com.luka.carplay.CarPlayHook;
import com.luka.carplay.framework.Log;

import de.esolutions.fw.comm.core.Proxy;
import de.esolutions.fw.comm.dsi.kombisync2.DSIKombiSyncReply;
import de.esolutions.fw.comm.dsi.kombisync2.impl.DSIKombiSyncProxy;

import org.dsi.ifc.kombisync2.DisplayIdentification;
import org.dsi.ifc.kombisync2.DisplayRequestResponse;
import org.dsi.ifc.kombisync2.DisplayStatus;
import org.dsi.ifc.kombisync2.DisplayStatusFlags;
import org.dsi.ifc.kombisync2.MenuContext;
import org.dsi.ifc.kombisync2.MenuState;
import org.dsi.ifc.kombisync2.PopupActionRequestResponse;
import org.dsi.ifc.kombisync2.PopupRegisterRequestResponse;
import org.dsi.ifc.kombisync2.PopupStatus;

public class FilteringKombiProxy extends DSIKombiSyncProxy {

    private static final String TAG = "FilteringKombiProxy";

    /* ----- Tunables ----- */

    /* Phone tab repurposing: rewrite mainContext 3->1 during CarPlay so VC
     * renders the Map scene (with HU MOST-video → TVMRCapture → Surface1)
     * when user selects Phone.  Phone tab is otherwise dead during CarPlay
     * ("use CarPlay" placeholder), so this turns it into a second Map. */
    private static final boolean REPURPOSE_PHONE_AS_MAP = true;

    /* Flap filter: zeroes LEFT_FLAP/RIGHT_FLAP bits + MenuContext states
     * on Map tab so the side drawer doesn't animate in during CarPlay.
     * Disabled while we focus on the Phone→Map rewrite path. */
    private static final boolean FILTER_FLAPS = false;

    /* ----- Constants (mirror DSIKombiSync interface) ----- */

    private static final int CONTEXT_MAP = 1;        // POPUPCONTEXT_NAV
    private static final int CONTEXT_PHONE = 3;      // POPUPCONTEXT_PHONE
    private static final int FLAP_BITS = 128 | 64;   // LEFT_FLAP | RIGHT_FLAP
    private static final int MENU_STATE_CLOSED = 1;  // MENUCONTEXTSTATE_CLOSED

    /* ----- Diagnostic log budgets (avoid HMI-tick spam) ----- */

    private static volatile int probeLogBudget = 5;     // unconditional first-N
    private static volatile int rewriteLogBudget = 10;  // Phone→Map rewrites
    private static volatile int filterLogBudget = 20;   // flap filter hits
    private static volatile int errorLogBudget = 3;     // swallowed exceptions

    /* ----- Static state for forcePushLast() ----- */

    private static volatile FilteringKombiProxy INSTANCE = null;
    private static volatile DisplayStatus lastDisplayStatus = null;

    /* ----- Instance state ----- */

    private final DSIKombiSyncProxy delegate;

    public FilteringKombiProxy(int stockInstance, DSIKombiSyncReply stockReply,
                               DSIKombiSyncProxy delegate) {
        /* The super constructor allocates a stale duplicate Proxy that we
         * never expose (getProxy() below returns delegate's instead).
         * Cost: ~1 idle object.  Saves us from null-arg crashes inside
         * DSIKombiSyncReplyService and Proxy's constructor chain. */
        super(stockInstance, stockReply);
        this.delegate = delegate;
        INSTANCE = this;
        Log.i(TAG, "wrapper installed (delegate=" + delegate.getClass().getName() + ")");
    }

    /* Return the STOCK inner Proxy so the OSGi framework keeps talking to
     * the same wire — our duplicate from super() is dead memory. */
    public Proxy getProxy() {
        return delegate.getProxy();
    }

    /* ============================================================
     * The interesting one: setMMIDisplayStatus
     * ============================================================ */

    public void setMMIDisplayStatus(DisplayStatus displayStatus) {
        synchronized (this) {
            setMMIDisplayStatusLocked(displayStatus);
        }
    }

    private void setMMIDisplayStatusLocked(DisplayStatus displayStatus) {
        lastDisplayStatus = displayStatus;

        if (probeLogBudget > 0) {
            probeLogBudget--;
            try {
                int sf = (displayStatus != null && displayStatus.statusFlags != null)
                        ? displayStatus.statusFlags.statusFlags : -1;
                int lm = (displayStatus != null && displayStatus.menuContext != null)
                        ? displayStatus.menuContext.leftMenuState : -1;
                int rm = (displayStatus != null && displayStatus.menuContext != null)
                        ? displayStatus.menuContext.rightMenuState : -1;
                int mc = (displayStatus != null) ? displayStatus.mainContext : -1;
                Log.i(TAG, "PROBE[" + probeLogBudget + "] carplay="
                        + CarPlayHook.isCarplayRunning()
                        + " mainContext=" + mc
                        + " flags=0x" + Integer.toHexString(sf)
                        + " menuL=" + lm + " menuR=" + rm);
            } catch (Throwable ignore) { }
        }

        int savedFlags = 0;
        int savedLeftMenu = 0;
        int savedRightMenu = 0;
        int savedMainContext = 0;
        boolean mutatedFlags = false;
        boolean mutatedMenu = false;
        boolean mutatedMainContext = false;

        /* Phone → Map rewrite (BEFORE the flap filter so the latter
         * naturally applies to the rewritten Map context). */
        try {
            if (REPURPOSE_PHONE_AS_MAP
                    && CarPlayHook.isCarplayRunning()
                    && displayStatus != null
                    && displayStatus.mainContext == CONTEXT_PHONE) {
                savedMainContext = displayStatus.mainContext;
                displayStatus.mainContext = CONTEXT_MAP;
                mutatedMainContext = true;
                if (rewriteLogBudget > 0) {
                    rewriteLogBudget--;
                    Log.i(TAG, "Phone->Map rewrite: mainContext "
                            + savedMainContext + "->" + CONTEXT_MAP);
                }
            }
        } catch (Throwable t) {
            if (errorLogBudget > 0) {
                errorLogBudget--;
                Log.e(TAG, "Phone->Map rewrite threw", t);
            }
        }

        /* Flap filter (disabled with FILTER_FLAPS=false). */
        try {
            if (FILTER_FLAPS
                    && CarPlayHook.isCarplayRunning()
                    && displayStatus != null
                    && displayStatus.mainContext == CONTEXT_MAP) {

                DisplayStatusFlags f = displayStatus.statusFlags;
                if (f != null) {
                    savedFlags = f.statusFlags;
                    if ((savedFlags & FLAP_BITS) != 0) {
                        f.statusFlags = savedFlags & ~FLAP_BITS;
                        mutatedFlags = true;
                    }
                }

                MenuContext mc = displayStatus.menuContext;
                if (mc != null) {
                    savedLeftMenu = mc.leftMenuState;
                    savedRightMenu = mc.rightMenuState;
                    if (mc.leftMenuState != MENU_STATE_CLOSED
                            || mc.rightMenuState != MENU_STATE_CLOSED) {
                        mc.leftMenuState = MENU_STATE_CLOSED;
                        mc.rightMenuState = MENU_STATE_CLOSED;
                        mutatedMenu = true;
                    }
                }

                if ((mutatedFlags || mutatedMenu) && filterLogBudget > 0) {
                    filterLogBudget--;
                    Log.i(TAG, "MAP+CarPlay -> filtered: flags 0x"
                            + Integer.toHexString(savedFlags) + "->0x"
                            + Integer.toHexString(f == null ? 0 : f.statusFlags)
                            + " menu L=" + savedLeftMenu + "->" + MENU_STATE_CLOSED
                            + " R=" + savedRightMenu + "->" + MENU_STATE_CLOSED);
                }
            }
        } catch (Throwable t) {
            if (errorLogBudget > 0) {
                errorLogBudget--;
                Log.e(TAG, "flap filter threw", t);
            }
        }

        try {
            delegate.setMMIDisplayStatus(displayStatus);
        } finally {
            try {
                if (mutatedMainContext) {
                    displayStatus.mainContext = savedMainContext;
                }
                if (mutatedFlags && displayStatus.statusFlags != null) {
                    displayStatus.statusFlags.statusFlags = savedFlags;
                }
                if (mutatedMenu && displayStatus.menuContext != null) {
                    displayStatus.menuContext.leftMenuState = savedLeftMenu;
                    displayStatus.menuContext.rightMenuState = savedRightMenu;
                }
            } catch (Throwable ignore) { }
        }
    }

    /**
     * Re-push the last-seen DisplayStatus through the filter pipeline.
     * Called from CarPlayHook on activate/deactivate so the cluster
     * re-evaluates state immediately rather than waiting for HMI's
     * next tick.
     */
    public static void forcePushLast(String reason) {
        FilteringKombiProxy inst = INSTANCE;
        if (inst == null) {
            Log.w(TAG, "forcePushLast(" + reason + "): no wrapper instance");
            return;
        }
        synchronized (inst) {
            DisplayStatus ds = lastDisplayStatus;
            if (ds == null) {
                Log.w(TAG, "forcePushLast(" + reason + "): no cached DisplayStatus yet");
                return;
            }
            Log.i(TAG, "forcePushLast(" + reason + "): mainContext=" + ds.mainContext
                    + " statusFlags=0x" + (ds.statusFlags == null ? "null"
                            : Integer.toHexString(ds.statusFlags.statusFlags)));
            inst.setMMIDisplayStatusLocked(ds);
        }
    }

    /* ============================================================
     * Pass-through delegates for everything else
     * ============================================================ */

    public void setMMIDisplayRequestResponse(DisplayRequestResponse r) {
        delegate.setMMIDisplayRequestResponse(r);
    }

    public void setMenuState(MenuState m) {
        delegate.setMenuState(m);
    }

    public void setMMIPopupRegisterRequest(PopupRegisterRequestResponse r) {
        delegate.setMMIPopupRegisterRequest(r);
    }

    public void setMMIPopupActionResponse(PopupActionRequestResponse r) {
        delegate.setMMIPopupActionResponse(r);
    }

    public void setMMIPopupStatus(PopupStatus s) {
        delegate.setMMIPopupStatus(s);
    }

    public void setMMIDisplayIdentification(DisplayIdentification id) {
        delegate.setMMIDisplayIdentification(id);
    }

    public void setHMIIsReady(boolean bl) {
        delegate.setHMIIsReady(bl);
    }

    public void setNotification(int[] i) {
        delegate.setNotification(i);
    }

    public void setNotification(int i) {
        delegate.setNotification(i);
    }

    public void setNotification() {
        delegate.setNotification();
    }

    public void clearNotification(int[] i) {
        delegate.clearNotification(i);
    }

    public void clearNotification(int i) {
        delegate.clearNotification(i);
    }

    public void clearNotification() {
        delegate.clearNotification();
    }

    public void yySet(String s1, String s2) {
        delegate.yySet(s1, s2);
    }
}
