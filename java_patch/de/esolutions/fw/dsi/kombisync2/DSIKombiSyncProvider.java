/*
 * Class-replacement of stock DSIKombiSyncProvider in lsd.jxe.
 *
 * Mutates DisplayStatus in flight: when CarPlay is active, clears
 * LEFT_FLAP (bit 128) and RIGHT_FLAP (bit 64) from statusFlags before the
 * call is forwarded to the proxy and thus out over DSI/MOST to the
 * instrument cluster. With these bits cleared, the cluster's Kanzi
 * scene-graph Trigger (in AU491-*_Tubes.kzb) never fires the
 * t_flapStatus_left/right write and the Selection Drawer stays hidden.
 *
 * All other methods and the constructor are byte-for-byte identical to
 * the stock provider from the MU1316 lsd.jxe decompile.
 */
package de.esolutions.fw.dsi.kombisync2;

import com.luka.carplay.CarPlayHook;
import com.luka.carplay.framework.Log;

import de.esolutions.fw.comm.agent.Agent;
import de.esolutions.fw.comm.core.Proxy;
import de.esolutions.fw.comm.core.method.MethodException;
import de.esolutions.fw.comm.dsi.kombisync2.DSIKombiSyncReply;
import de.esolutions.fw.comm.dsi.kombisync2.impl.DSIKombiSyncProxy;
import de.esolutions.fw.dsi.base.AbstractProvider;
import de.esolutions.fw.dsi.base.IDispatcher;
import org.dsi.ifc.kombisync2.DSIKombiSync;
import org.dsi.ifc.kombisync2.DisplayIdentification;
import org.dsi.ifc.kombisync2.DisplayRequestResponse;
import org.dsi.ifc.kombisync2.DisplayStatus;
import org.dsi.ifc.kombisync2.DisplayStatusFlags;
import org.dsi.ifc.kombisync2.MenuContext;
import org.dsi.ifc.kombisync2.MenuState;
import org.dsi.ifc.kombisync2.PopupActionRequestResponse;
import org.dsi.ifc.kombisync2.PopupRegisterRequestResponse;
import org.dsi.ifc.kombisync2.PopupStatus;
import org.osgi.framework.BundleContext;

public class DSIKombiSyncProvider extends AbstractProvider implements DSIKombiSync {

    private static final String TAG = "DSIKombiSyncProvider";

    /* Bit constants copied from DSIKombiSync interface — kept local so the
     * filter body is self-explanatory. */
    private static final int FLAP_BITS = 128 | 64;  // LEFT_FLAP | RIGHT_FLAP

    /* mainContext values. Same numbering as POPUPCONTEXT_* constants. */
    private static final int CONTEXT_MAP = 1;    // POPUPCONTEXT_NAV
    private static final int CONTEXT_PHONE = 3;  // POPUPCONTEXT_PHONE

    /* Phone-tab repurposing: during CarPlay the Phone tab only shows a
     * "use CarPlay" placeholder — useless. Rewrite mainContext 3→1 so
     * VC renders Map scene (with HU MOST-video stream → TVMRCapture →
     * Surface1 → visible) whenever user selects Phone. Net effect:
     * Phone tab turns into a second Map view during CarPlay.
     *
     * Side effect: the flap filter below automatically applies to the
     * Phone→Map-masked traffic as well, so we don't show flap chrome on
     * the repurposed Phone tab either. */
    private static final boolean REPURPOSE_PHONE_AS_MAP = true;

    /* MenuContext state: cluster also receives per-menu state ints that
     * separately indicate "is the left/right side menu open" — independent
     * of the statusFlags bit. Zeroing the bit alone isn't guaranteed to
     * keep the flap closed if the cluster reads menuContext first. */
    private static final int MENU_STATE_CLOSED = 1;  // MENUCONTEXTSTATE_CLOSED

    /* Counter to suppress log spam — log only first N filtered calls per session. */
    private static volatile int filterLogBudget = 20;

    /* Unconditional log of first N incoming setMMIDisplayStatus calls so we
     * can diagnose the "is the patch even being called?" question and read
     * real mainContext values (our CONTEXT_MAP=1 is a hypothesis that needs
     * live verification). */
    private static volatile int probeLogBudget = 5;

    /* Log budget for swallowed exceptions in the filter path. Catching
     * Throwable without any output makes a malformed DisplayStatus silently
     * bypass the filter — hard to debug. */
    private static volatile int errorLogBudget = 3;

    /* Last DisplayStatus we saw.  Kept so CarPlayHook can request a
     * re-push when CarPlay activates while the Map tab (with flaps
     * already open) was on screen — otherwise we'd wait for the HMI's
     * next natural update and flaps would linger. */
    private static volatile DisplayStatus lastDisplayStatus = null;

    /* Live instance pointer so static helpers can reach `proxy`. */
    private static volatile DSIKombiSyncProvider INSTANCE = null;

    private static final int[] attributeIDs = new int[]{1, 2, 3, 4, 5, 6, 7};
    private DSIKombiSyncProxy proxy;

    public DSIKombiSyncProvider(int i, BundleContext bundleContext, Agent agent, IDispatcher iDispatcher) {
        super(bundleContext, agent, iDispatcher, i);
        this.createNewProxy();
        INSTANCE = this;
        Log.i(TAG, "provider installed (patched)");
    }

    protected int[] getAttributeIDs() {
        return attributeIDs;
    }

    public String getName() {
        return org.dsi.ifc.kombisync2.DSIKombiSync.class.getName();
    }

    protected Proxy getProxy() {
        return this.proxy.getProxy();
    }

    protected Proxy createNewProxy() {
        this.proxy = new DSIKombiSyncProxy(this.instance, (DSIKombiSyncReply) this.dispatcher);
        return this.proxy.getProxy();
    }

    public void setMMIDisplayRequestResponse(DisplayRequestResponse displayRequestResponse) {
        try {
            this.proxy.setMMIDisplayRequestResponse(displayRequestResponse);
        } catch (Exception methodException) {
            this.traceException(methodException);
        }
    }

    /**
     * PATCHED: clear LEFT_FLAP|RIGHT_FLAP bits while CarPlay is running
     * AND the cluster's main context is the Map tab. Other tabs
     * (Car/Media/Phone) keep their native flap behavior.
     *
     * The filter restores the original bits on the in-flight object after
     * the proxy call — so HMI's cached DisplayStatus reference stays
     * byte-identical to before the call. Without this restore we'd slowly
     * corrupt HMI's own state by stripping bits off objects it reuses.
     */
    public void setMMIDisplayStatus(DisplayStatus displayStatus) {
        /* `synchronized` prevents a race between the HMI-thread path and
         * the CarPlay-thread forcePushLast path — both may arrive with
         * the SAME DisplayStatus reference and otherwise clobber each
         * other's mutate/restore sequences. */
        synchronized (this) {
            setMMIDisplayStatusLocked(displayStatus);
        }
    }

    private void setMMIDisplayStatusLocked(DisplayStatus displayStatus) {
        /* keep reference for forcePushLast() on CarPlay activation. */
        lastDisplayStatus = displayStatus;

        /* Unconditional diagnostic log for the first N calls — verifies
         * the class-replacement actually shadows stock lsd.jxe and
         * reveals the real mainContext value (our CONTEXT_MAP guess
         * needs this to be verified on a live cluster). */
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
            } catch (Throwable ignore) { /* diagnostic only */ }
        }

        int savedFlags = 0;
        int savedLeftMenu = 0;
        int savedRightMenu = 0;
        int savedMainContext = 0;
        boolean mutatedFlags = false;
        boolean mutatedMenu = false;
        boolean mutatedMainContext = false;

        /* Phone tab repurposing: rewrite mainContext BEFORE the flap filter
         * so the filter naturally applies to the rewritten (Map) context. */
        try {
            if (REPURPOSE_PHONE_AS_MAP
                    && CarPlayHook.isCarplayRunning()
                    && displayStatus != null
                    && displayStatus.mainContext == CONTEXT_PHONE) {
                savedMainContext = displayStatus.mainContext;
                displayStatus.mainContext = CONTEXT_MAP;
                mutatedMainContext = true;
                if (filterLogBudget > 0) {
                    Log.i(TAG, "Phone->Map rewrite: mainContext "
                            + savedMainContext + "->" + CONTEXT_MAP
                            + " (flap filter will now apply too)");
                }
            }
        } catch (Throwable t) {
            if (errorLogBudget > 0) {
                errorLogBudget--;
                Log.e(TAG, "mainContext rewrite threw", t);
            }
        }

        try {
            if (CarPlayHook.isCarplayRunning()
                    && displayStatus != null
                    && displayStatus.mainContext == CONTEXT_MAP) {

                /* Zero statusFlags LEFT_FLAP / RIGHT_FLAP bits */
                DisplayStatusFlags f = displayStatus.statusFlags;
                if (f != null) {
                    savedFlags = f.statusFlags;
                    if ((savedFlags & FLAP_BITS) != 0) {
                        f.statusFlags = savedFlags & ~FLAP_BITS;
                        mutatedFlags = true;
                    }
                }

                /* Also force MenuContext left/right states to CLOSED so
                 * cluster doesn't re-open flaps from the menuContext
                 * signal path (independent of statusFlags bits). */
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
                Log.e(TAG, "filter threw (swallowing to keep stock path alive)", t);
            }
        }

        try {
            this.proxy.setMMIDisplayStatus(displayStatus);
        } catch (Exception methodException) {
            this.traceException(methodException);
        } finally {
            /* restore original values so HMI-owned DisplayStatus stays
             * consistent if HMI reuses it next tick. */
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

    /* ============================================================
     * Static helpers used by CarPlayHook for explicit re-push.
     * ============================================================ */

    /**
     * Called from CarPlayHook on CarPlay activation and deactivation.
     * Re-runs the last setMMIDisplayStatus with the currently-cached
     * DisplayStatus so the cluster re-evaluates flaps immediately:
     *   - on activation: filter kicks in, flaps hide
     *   - on deactivation: filter skipped, flaps restore to HMI truth
     *
     * Safe to call before any setMMIDisplayStatus ever arrives
     * (no-op if cache is empty).
     */
    public static void forcePushLast(String reason) {
        DSIKombiSyncProvider inst = INSTANCE;
        if (inst == null) {
            Log.w(TAG, "forcePushLast(" + reason + "): no provider instance");
            return;
        }
        /* Read-and-push under provider lock — so if HMI thread is
         * concurrently mid-setMMIDisplayStatus, we queue behind it and
         * avoid the mutate/restore race on a shared DisplayStatus. */
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

    public void setMenuState(MenuState menuState) {
        try {
            this.proxy.setMenuState(menuState);
        } catch (Exception methodException) {
            this.traceException(methodException);
        }
    }

    public void setMMIPopupRegisterRequest(PopupRegisterRequestResponse popupRegisterRequestResponse) {
        try {
            this.proxy.setMMIPopupRegisterRequest(popupRegisterRequestResponse);
        } catch (Exception methodException) {
            this.traceException(methodException);
        }
    }

    public void setMMIPopupActionResponse(PopupActionRequestResponse popupActionRequestResponse) {
        try {
            this.proxy.setMMIPopupActionResponse(popupActionRequestResponse);
        } catch (Exception methodException) {
            this.traceException(methodException);
        }
    }

    public void setMMIPopupStatus(PopupStatus popupStatus) {
        try {
            this.proxy.setMMIPopupStatus(popupStatus);
        } catch (Exception methodException) {
            this.traceException(methodException);
        }
    }

    public void setMMIDisplayIdentification(DisplayIdentification displayIdentification) {
        try {
            this.proxy.setMMIDisplayIdentification(displayIdentification);
        } catch (Exception methodException) {
            this.traceException(methodException);
        }
    }

    public void setHMIIsReady(boolean bl) {
        try {
            this.proxy.setHMIIsReady(bl);
        } catch (Exception methodException) {
            this.traceException(methodException);
        }
    }

    public void setNotification(int[] i) {
        try {
            this.proxy.setNotification(i);
        } catch (Exception methodException) {
            this.traceException(methodException);
        }
    }

    public void setNotification(int i) {
        try {
            this.proxy.setNotification(i);
        } catch (Exception methodException) {
            this.traceException(methodException);
        }
    }

    public void setNotification() {
        try {
            this.proxy.setNotification();
        } catch (Exception methodException) {
            this.traceException(methodException);
        }
    }

    public void clearNotification(int[] i) {
        try {
            this.proxy.clearNotification(i);
        } catch (Exception methodException) {
            this.traceException(methodException);
        }
    }

    public void clearNotification(int i) {
        try {
            this.proxy.clearNotification(i);
        } catch (Exception methodException) {
            this.traceException(methodException);
        }
    }

    public void clearNotification() {
        try {
            this.proxy.clearNotification();
        } catch (Exception methodException) {
            this.traceException(methodException);
        }
    }

    public void yySet(String string, String string1) {
        try {
            this.proxy.yySet(string, string1);
        } catch (Exception methodException) {
            this.traceException(methodException);
        }
    }
}
