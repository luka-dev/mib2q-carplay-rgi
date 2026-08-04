/*
 * PhoneTabGate — EXPERIMENTAL cluster telephone-tab existence gate.
 *
 * Rule: the cluster telephone tab should exist ONLY when a Bluetooth/HFP phone is connected AND no
 * projection (CarPlay / Android Auto) owns telephony.  That exact condition is already computed by the
 * stock phone/core code: BAPPropertyTelFSGSetup.getMobileConnectionType() returns 3
 * (MOBILE_CONNECTION_TYPE_HANDS_FREE_PROFILE) ONLY when HFP is connected+active and NOT CarPlay(5)/
 * AndroidAuto(6).  So the whole rule collapses to `mobileConnectionType == 3`, published from
 * updateAsync() (which runs on every telephony/projection transition).
 *
 * The lever: when the tab must be hidden, InitializationManagerPhone2 forces PHONE2 HMIState=NOT_READY
 * (re-pushed via the registered resend), which de-publishes the PHONE2 tel domain on the cluster ->
 * App::setPhoneAvailable(0) -> tab avail=0 -> icon index 0 (nothing drawn).  (op_State was the WRONG
 * lever — the cluster routes it into a separate OpState field that only swaps the ICON, never avail.)
 * Whether avail=0 collapses the tab SLOT or just leaves an empty gap is UNVERIFIED from hmi_app.elf —
 * this is the on-unit experiment.
 *
 * Default = visible: a missing signal (before the first telephony update) never hides the tab.
 */
package com.luka.carplay.phone;

public final class PhoneTabGate {

    private static volatile boolean tabVisible = true;
    private static volatile Runnable resend;   /* -> InitializationManagerPhone2.updateOperationState() */

    private PhoneTabGate() {}

    /** Read by InitializationManagerPhone2.isRequiredDataForOpStateNormalAvailable(). */
    public static boolean isTabVisible() { return tabVisible; }

    /** Called from BAPPropertyTelFSGSetup.updateAsync with (mobileConnectionType == 3). */
    public static void setTabVisible(boolean visible) {
        if (tabVisible == visible) return;
        tabVisible = visible;
        Runnable r = resend;
        if (r != null) {
            /* never let the op-state re-push throw back into the phone-state thread */
            try { r.run(); } catch (Throwable t) { /* ignore */ }
        }
    }

    /** InitializationManagerPhone2 registers a resend that re-pushes the PHONE2 FSG op-state. */
    public static void registerResend(Runnable r) { resend = r; }
}
