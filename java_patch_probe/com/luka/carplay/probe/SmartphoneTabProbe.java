/*
 * SmartphoneTabProbe — MINIMAL standalone probe (no hook, no bus, no renderers).
 *
 * Purpose: on CarPlay connect, raise the cluster Smartphone/CarPlay tab and test on-unit
 * whether the A5 VC shows it.  Stock never calls setMenuState as a CLIENT (only the
 * DSIKombiSyncProvider implements it), so nobody natively raises the cluster tab — we do.
 *
 *   setMenuState(smartPhone = AVAILABLE)                  -> tab exists / selectable
 *   setMMIDisplayStatus(LVDS_DM_ACTIVE, mainContext=117)  -> "current LVDS stream is CarPlay"
 *
 * IMPORTANT — how we get DSIKombiSync:
 *   The old version subscribed via AbstractDSIController + startDSI() with a hardcoded
 *   instanceID=0.  DSIKombiSync is NOT a per-device DSI (that tracker is for DSICarplay);
 *   it is a singleton cluster service registered by DSIKombiSyncProvider under a fixed
 *   DEVICE_INSTANCE that is almost certainly not 0 -> addingService rejected it silently
 *   (rejection logs go to the stock DSI channel, not our file) so setMenuState never sent.
 *   Now we look it up directly via IServiceManager.getServiceReferences(DSIKombiSync.class)
 *   and log every registered instance, so the real DEVICE_INSTANCE is visible on-unit.
 *
 * Logs -> /tmp/carplay_tabprobe.log, each line "[TabProbe] ...".  Java 1.4 target.
 */
package com.luka.carplay.probe;

import java.io.FileWriter;

import de.audi.app.terminalmode.IContext;
import de.audi.app.terminalmode.osgi.IServiceManager;

import org.osgi.framework.ServiceReference;

import org.dsi.ifc.base.DSIBase;
import org.dsi.ifc.kombisync2.DSIKombiSync;
import org.dsi.ifc.kombisync2.MenuState;
import org.dsi.ifc.kombisync2.DisplayStatus;
import org.dsi.ifc.kombisync2.DisplayStatusFlags;


public final class SmartphoneTabProbe {

    private static final int SMARTPHONE_MAINCONTEXT = 117;   /* 0x75 = MAIN_CONTEXT_SMARTPHONE_CARPLAY */
    private static final String LOG_PATH = "/tmp/carplay_tabprobe.log";

    private SmartphoneTabProbe() {}

    /* Append one line to /tmp/carplay_tabprobe.log (best-effort; also echoes to stdout). */
    static void log(String msg) {
        String line = "[TabProbe] " + msg;
        System.out.println(line);
        FileWriter fw = null;
        try { fw = new FileWriter(LOG_PATH, true); fw.write(line); fw.write("\n"); }
        catch (Throwable t) { /* ignore */ }
        finally { if (fw != null) try { fw.close(); } catch (Throwable t) { } }
    }

    /* Dump the coding/sysConst that license the SmartPhone MMI_Status slot (4237=CarPlay,
     * 4238=AndroidAuto).  If both are false the native side won't license the slot. */
    static void dumpCoding(IContext ctx) {
        try {
            de.audi.atip.sysapp.carcoding.ISysConstManager scm = ctx.getFramework().getSysConstManager();
            boolean appleDIO  = scm.getAdaptationANP().isAppleDIO();
            boolean googleGAL = scm.getAdaptationANP().isGoogleGAL();
            log("coding: isAppleDIO=" + appleDIO + " isGoogleGAL=" + googleGAL
                + " | sysConst[4237]=" + scm.getSysConst(4237) + " sysConst[4238]=" + scm.getSysConst(4238)
                + "  (SmartPhone slot licensed when any = 1/true)");
        } catch (Throwable t) {
            log("coding dump FAILED: " + t);
        }
    }

    /* Look up the registered DSIKombiSync service directly (any DEVICE_INSTANCE) and log what
     * exists.  Returns the first usable service, or null. */
    private static DSIKombiSync findKombiSync(IServiceManager sm) {
        ServiceReference[] refs = sm.getServiceReferences(DSIKombiSync.class);
        if (refs == null || refs.length == 0) {
            log("findKombiSync: NO DSIKombiSync (v2) service registered — dumping kombi-related services:");
            dumpKombiServices(sm);
            return null;
        }
        DSIKombiSync first = null;
        for (int i = 0; i < refs.length; i++) {
            Object name = refs[i].getProperty(DSIBase.DEVICE_NAME);
            Object inst = refs[i].getProperty(DSIBase.DEVICE_INSTANCE);
            Object svc = sm.getService(refs[i]);
            log("findKombiSync: ref[" + i + "] DEVICE_NAME=" + name + " DEVICE_INSTANCE=" + inst
                + " svc=" + (svc == null ? "null" : svc.getClass().getName()));
            if (first == null && svc instanceof DSIKombiSync) first = (DSIKombiSync) svc;
        }
        return first;
    }

    /* Enumerate ALL OSGi services and log the ones whose registered interfaces mention "kombi"
     * — tells us on-unit whether the cluster-sync service exists at all and under what name
     * (v1 org.dsi.ifc.kombisync vs v2 kombisync2, or something else entirely). */
    private static void dumpKombiServices(IServiceManager sm) {
        try {
            org.osgi.framework.ServiceReference[] all =
                sm.getBundleContext().getServiceReferences((String) null, (String) null);
            if (all == null) { log("  (getServiceReferences returned null)"); return; }
            int hits = 0;
            for (int i = 0; i < all.length; i++) {
                Object oc = all[i].getProperty("objectClass");
                String s = (oc instanceof String[]) ? join((String[]) oc) : String.valueOf(oc);
                String ls = s.toLowerCase();
                if (ls.indexOf("kombi") >= 0 || ls.indexOf("combi") >= 0 || ls.indexOf("actionproxy") >= 0) {
                    log("  kombi svc: " + s + "  DEVICE_INSTANCE=" + all[i].getProperty(DSIBase.DEVICE_INSTANCE));
                    hits++;
                }
            }
            log("  (" + all.length + " services total, " + hits + " kombi-related)");
        } catch (Throwable t) {
            log("  dumpKombiServices FAILED: " + t);
        }
    }

    private static String join(String[] a) {
        StringBuffer b = new StringBuffer();
        for (int i = 0; i < a.length; i++) { if (i > 0) b.append(","); b.append(a[i]); }
        return b.toString();
    }

    /* The REAL lever: MMICombiActionProxy.setMMICombiContext(terminalID, mainContext) is what the
     * stock nav/car/system state machines use to push the cluster mainContext over BAP — nav sends
     * (0, 20)=NAVIGATION_MAP and it works.  So (0, 117)=SMARTPHONE_CARPLAY should tell the cluster
     * the MMI is on CarPlay.  The impl is a native proxy; we just need the registered service. */
    private static boolean tryMMICombiContext(IServiceManager sm) {
        /* ActionProxies register under the BASE interface de.audi.atip.statemachine.ActionProxy,
         * not their sub-type — enumerate all of them and instanceof-check for MMICombiActionProxy.
         * Look up by class-NAME string (BundleContext), so this probe carries no compile/link
         * dependency on the ActionProxy class itself (avoids a bundle-wiring NoClassDefFoundError). */
        org.osgi.framework.BundleContext bc = sm.getBundleContext();
        ServiceReference[] refs;
        try { refs = bc.getServiceReferences("de.audi.atip.statemachine.ActionProxy", (String) null); }
        catch (Throwable t) { log("tryMMICombiContext: getServiceReferences threw " + t); return false; }
        if (refs == null || refs.length == 0) {
            log("tryMMICombiContext: NO ActionProxy services at all");
            return false;
        }
        /* Reflect by METHOD NAME, not instanceof — OSGi bundle classloaders give each bundle its own
         * MMICombiActionProxy Class object, so instanceof across bundles is false even for the real
         * proxy.  A method-name probe is classloader-agnostic; log every proxy's class so we see them. */
        Class[] sig = new Class[]{ Integer.TYPE, Integer.TYPE };
        for (int i = 0; i < refs.length; i++) {
            Object svc = bc.getService(refs[i]);          /* raw object, not the ServiceManager wrapper */
            if (svc == null) continue;
            String cn = svc.getClass().getName();
            java.lang.reflect.Method m = null;
            try { m = svc.getClass().getMethod("setMMICombiContext", sig); } catch (Throwable t) { /* not this one */ }
            log("  actionproxy[" + i + "] " + cn + (m != null ? "  <-- has setMMICombiContext" : ""));
            if (m != null) {
                try {
                    m.invoke(svc, new Object[]{ new Integer(0), new Integer(SMARTPHONE_MAINCONTEXT) });
                    log("SENT: setMMICombiContext(0, 117) via " + cn);
                    return true;
                } catch (Throwable t) { log("  invoke setMMICombiContext failed: " + t); }
            }
        }
        log("tryMMICombiContext: " + refs.length + " ActionProxy, none has setMMICombiContext");
        return false;
    }

    /* Called from the patched TerminalModeBapCombi seam on CarPlay connect. */
    public static synchronized void onConnect(IContext ctx) {
        log("onConnect ENTER");   /* heartbeat: if this is missing, the seam never called us / class didn't load */
        try {
            dumpCoding(ctx);
            IServiceManager sm = ctx.getServiceManager();
            /* Primary path: push the cluster mainContext via MMICombiActionProxy (BAP, what stock uses). */
            if (tryMMICombiContext(sm)) return;
            /* DSIKombiSync path (v2 was found absent on-unit — kept as a diagnostic + dumps services). */
            DSIKombiSync svc = findKombiSync(sm);
            if (svc == null) { log("onConnect: no MMICombiActionProxy and no DSIKombiSync -> cannot raise tab"); return; }
            MenuState ms = new MenuState();
            ms.smartPhone = DSIKombiSync.MENUSTATE_AVAILABLE;                                 /* 2 */
            svc.setMenuState(ms);
            DisplayStatus ds = new DisplayStatus();
            ds.statusFlags = new DisplayStatusFlags(DSIKombiSync.DISPLAYSTATUS_LVDS_DM_ACTIVE); /* 8192 */
            ds.mainContext = SMARTPHONE_MAINCONTEXT;                                            /* 117 */
            svc.setMMIDisplayStatus(ds);
            log("SENT (DSIKombiSync): tab AVAILABLE + stream claimed CarPlay");
        } catch (Throwable t) {
            log("onConnect FAILED: " + t);
        }
    }

    /* Called on CarPlay disconnect — retract the tab. */
    public static synchronized void onDisconnect(IContext ctx) {
        try {
            DSIKombiSync svc = findKombiSync(ctx.getServiceManager());
            if (svc == null) return;
            MenuState ms = new MenuState();
            ms.smartPhone = DSIKombiSync.MENUSTATE_NOT_EXISTING;                                /* 3 */
            svc.setMenuState(ms);
            log("retracted (NOT_EXISTING)");
        } catch (Throwable t) {
            log("onDisconnect FAILED: " + t);
        }
    }
}
