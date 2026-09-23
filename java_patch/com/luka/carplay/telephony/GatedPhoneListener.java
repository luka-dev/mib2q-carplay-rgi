/*
 * GatedPhoneListener - wrapper around the stock CombiBAPServicePhoneListener
 * (TelBAPManagerTel1) for the cluster telephone tab.
 *
 * While GatedPhoneService shows CarPlay calls, accept / hangup / hold / resume /
 * swap / dial / mic mute from the VC go to CarPlay instead of the HFP stack, which has no
 * device and would only answer "not successful".  Everything else, and every
 * action while the gate is off, goes to stock unchanged.
 *
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */
package com.luka.carplay.telephony;

import de.audi.atip.interapp.combi.bap.phone.CombiBAPServicePhoneListener;
import de.audi.atip.interapp.combi.bap.phone.data.CombiBAPCallStackEntry;

public class GatedPhoneListener implements CombiBAPServicePhoneListener {
    final CombiBAPServicePhoneListener real;

    private GatedPhoneListener(CombiBAPServicePhoneListener r) { this.real = r; }

    /** Wraps once; null stays null (listener removed). */
    public static CombiBAPServicePhoneListener wrap(CombiBAPServicePhoneListener l) {
        if (l == null || l instanceof GatedPhoneListener) return l;
        return new GatedPhoneListener(l);
    }

    /* A list entry carries the number in its call-stack record; the first
     * argument is the fallback stock also logs. */
    private static String adbNumber(String s, CombiBAPCallStackEntry e) {
        String n = e != null ? e.getTelNumber() : null;
        return n != null && n.length() > 0 ? n : s;
    }

    public void dialNumber(String s, String s1) { if (!GatedPhoneService.onVcAction(GatedPhoneService.ACT_DIAL, s)) real.dialNumber(s, s1); }
    public void dialNumberFromAdbEntry(String s, String s1, CombiBAPCallStackEntry combibapcallstackentry) { if (!GatedPhoneService.onVcAction(GatedPhoneService.ACT_DIAL, adbNumber(s, combibapcallstackentry))) real.dialNumberFromAdbEntry(s, s1, combibapcallstackentry); }
    public void dialService(int i) { real.dialService(i); }
    public void confirmEmergencyCall(boolean flag) { real.confirmEmergencyCall(flag); }
    public void hangupCall(int i) { if (!GatedPhoneService.onVcAction(GatedPhoneService.ACT_HANGUP, null)) real.hangupCall(i); }
    public void acceptCall() { if (!GatedPhoneService.onVcAction(GatedPhoneService.ACT_ACCEPT, null)) real.acceptCall(); }
    public void callHold() { if (!GatedPhoneService.onVcAction(GatedPhoneService.ACT_HOLD, null)) real.callHold(); }
    public void resumeCall() { if (!GatedPhoneService.onVcAction(GatedPhoneService.ACT_RESUME, null)) real.resumeCall(); }
    public void setMicMuteState(boolean flag) { if (!GatedPhoneService.onVcAction(flag ? GatedPhoneService.ACT_MUTE_ON : GatedPhoneService.ACT_MUTE_OFF, null)) real.setMicMuteState(flag); }
    public void releaseActiveCallAcceptWaitingCall() { real.releaseActiveCallAcceptWaitingCall(); }
    public void swapCalls() { if (!GatedPhoneService.onVcAction(GatedPhoneService.ACT_SWAP, null)) real.swapCalls(); }
    public void callHoldAcceptWaitingCall() { real.callHoldAcceptWaitingCall(); }
    public void releaseAllCallsAcceptWaitingCall() { real.releaseAllCallsAcceptWaitingCall(); }
    public void setWaitingCallOnHold() { real.setWaitingCallOnHold(); }
    public void joinCalls() { real.joinCalls(); }
    public void splitCall(int i) { real.splitCall(i); }
    public void setRingToneMuteState(boolean flag) { real.setRingToneMuteState(flag); }
    public void setAutomaticRedialActive(boolean flag) { real.setAutomaticRedialActive(flag); }
    public void resetMissedCallsIndicator() { if (!GatedPhoneService.onVcAction(GatedPhoneService.ACT_RESET_MISSED, null)) real.resetMissedCallsIndicator(); }
}
