/*
 * GatedPhoneService - wrapper around the stock CombiBAPServicePhone (HU PHONE 0x28)
 * that shows CarPlay calls on the cluster telephone tab.
 *
 * Gate = stock's own CarPlay verdict: FctID 14 arrives with mobileConnectionType 5
 * (BAPPropertyTelFSGSetup: TerminalModeDevice active && connection method 1).
 * While the gate is on, the VC sees an HFP phone instead (type 3, Tel_State 15 =
 * phone on without SIM), and the call functions 22/23/24 carry CarPlay's calls from
 * DSICarplay.updateCallState.  Stock's values for the gated functions are cached and
 * replayed when the gate goes off.  Every other method passes straight through.
 *
 * VC actions (accept, hangup, hold/resume/swap, dial, mic mute) come back through
 * GatedPhoneListener and go to hook/telephony (iAP2 AcceptCall, EndCall,
 * SwapCalls, Hold/MuteStatusUpdate) and to callNumber (dial).  Recents/favorites come from hook/telephony (iAP2
 * ListUpdate, EVT_TEL_LISTS), with the missed-call indication derived from them;
 * signal and carrier come from the stock CommunicationsUpdate (EVT_TEL_COMM), the iPhone's
 * battery from PowerUpdate (EVT_TEL_COMM battery/charging, hook/telephony).
 *
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */
package com.luka.carplay.telephony;

import com.luka.carplay.bus.CarplayBus;
import com.luka.carplay.framework.Log;
import de.audi.atip.interapp.combi.bap.phone.CombiBAPServicePhone;
import de.audi.atip.interapp.combi.bap.phone.data.CallStartTime;
import de.audi.atip.interapp.combi.bap.phone.data.CombiBAPCallInfo;
import de.audi.atip.interapp.combi.bap.phone.data.CombiBAPCallStackEntry;
import de.audi.atip.interapp.combi.bap.phone.data.CombiBAPCallState;
import de.audi.atip.interapp.combi.bap.phone.data.CombiBAPFavoriteNumberEntry;
import de.audi.atip.interapp.combi.bap.phone.data.CombiBAPhoneMobileBatteryLevel;
import de.audi.atip.interapp.combi.bap.phone.data.FsgSetup;
import de.audi.atip.interapp.phone.TelAdbEntryStruct;
import java.util.Calendar;
import org.dsi.ifc.carplay.CallState;

public class GatedPhoneService implements CombiBAPServicePhone {
    private static final String TAG = "Telephony";

    private static final int MCT_HFP = 3, MCT_APPLE_LINK = 5;
    private static final int TEL_STATE_PHONE_ON_NO_SIM = 15;
    private static final int SLOTS = 7;
    private static final int NO_DURATION = CALL_DURATION_NOT_AVAILABLE;

    /* DSICarplay CALLSTATE_* / CALLDIRECTION_* */
    private static final int DSI_DISCONNECTED = 0, DSI_ACTIVE = 1, DSI_HOLDING = 2, DSI_CONNECTING = 3;
    private static final int DSI_INCOMING = 0;

    /* BAP CallState values, as stock TelBAPCall.getCallState produces them. */
    private static final int BAP_RINGING = 1, BAP_ACTIVE = 2, BAP_DIALING = 3, BAP_HELD = 5;
    private static final int BAP_CALL_TYPE_VOICE = 1;

    /* VC actions handled for CarPlay (GatedPhoneListener). */
    static final int ACT_ACCEPT = 1, ACT_HANGUP = 2, ACT_HOLD = 3, ACT_RESUME = 4, ACT_SWAP = 5, ACT_DIAL = 6,
                     ACT_MUTE_ON = 7, ACT_MUTE_OFF = 8, ACT_RESET_MISSED = 9;
    /* hook/telephony CMD_TEL actions: MuteStatusUpdate, HoldStatusUpdate, AcceptCall, EndCall, SwapCalls. */
    private static final int TEL_CMD_MUTE = 1, TEL_CMD_HOLD = 2, TEL_CMD_ACCEPT = 3, TEL_CMD_END = 4,
                             TEL_CMD_SWAP = 5;
    /* BAP callMode, as stock BAPPropertyTelCombinedNumbers.getCallMode maps DSI stacks. */
    private static final int CALLMODE_MISSED = 1, CALLMODE_RECEIVED = 2, CALLMODE_DIALED = 3;
    /* DSICarplay BUTTON_*: CarPlay's HID telephony keys, as stock MFLPhoneButtonsListener
     * posts them.  We send none of them: on the car they reached the phone neither
     * from the cluster nor from the wheel (2026-09-24), so every call action is iAP2. */
    private static final int BUTTON_ACCEPT_CALL = 13, BUTTON_END_CALL = 14, BUTTON_FLASH_CALL = 20;
    private static final int RESULT_OK = 0, RESULT_FAILED = 1;

    /** CarPlay-side commands, installed by CarplayDSILifecycleController while DSI is up. */
    public interface Commands {
        /** Dial through CarPlay (requestUI2 "tel:<number>", stock callNumber). */
        void dial(String number);
    }

    private static volatile Commands commands;

    /* The steering-wheel Tel key reaches us twice: stock MFLPhoneButtonsListener
     * posts a HID key (FLASH 20, or HOOK 13 / HANGUP 14 on two-key wheels) and the
     * VC sends AcceptCall/HangupCall over BAP (App::handleKeyEvent, event 479).
     * During a call the BAP copy is answered over iAP2, so the stock HID copy is
     * dropped: it did nothing on the car, and on a unit where it works it would act
     * a second time (a FLASH right after an accept).  Without a call the VC sends no
     * BAP action (it opens the list itself), so stock's key is left alone. */
    public static boolean dropStockPhoneKey(int id) {
        if (id != BUTTON_ACCEPT_CALL && id != BUTTON_END_CALL && id != BUTTON_FLASH_CALL) return false;
        GatedPhoneService g;
        synchronized (GatedPhoneService.class) { g = instance; }
        return g != null && g.gatedWithCall();
    }

    private synchronized boolean gatedWithCall() { return gated && liveCalls() > 0; }

    public static void setCommands(Commands c) { commands = c; }

    private static GatedPhoneService instance;

    final CombiBAPServicePhone real;
    private boolean gated;

    /* Last stock values of the gated functions, replayed on gate-off. */
    private FsgSetup stockFsg;
    private boolean haveStockOp;
    private int stockTelState;
    private boolean stockPrivacy, stockEnhancedPrivacy;
    private boolean[] stockSupport;
    private CombiBAPCallState[] stockStates;
    private boolean stockEcallPending;
    private CombiBAPCallInfo[] stockInfos;
    private CallStartTime[] stockDurations;
    private CombiBAPCallStackEntry[] stockRecents;
    private CombiBAPFavoriteNumberEntry[] stockFavorites;
    private boolean haveStockMute, stockMute;
    private int stockSignal = -1, stockRegState, stockNetType, stockDataType;
    private boolean haveStockRegState, haveStockProvider, haveStockMissed;
    private int stockProvNameValid, stockProvIdValid, stockMissedCalls, stockMissedNumbers;
    private CombiBAPhoneMobileBatteryLevel stockBattery;
    private String stockProvName, stockProvId;

    /* CarPlay signal/carrier from the stock CommunicationsUpdate (hook EVT_TEL_COMM). */
    private int signalPct, regState;
    private String carrier = "";
    /* iPhone battery 0..100 from PowerUpdate, -1 = not reported. */
    private int batteryPct = -1, loggedBatteryPct = -2;
    /* Missed-call indication: unreturned missed calls newer than the last VC reset. */
    private long missedResetTs, newestRecentTs;   /* phone clock, s */
    private int missedCalls, missedNumbers;

    /* CarPlay recents/favorites from hook/telephony (iAP2 ListUpdate) and our mic mute. */
    private CombiBAPCallStackEntry[] recents = new CombiBAPCallStackEntry[0];
    private CombiBAPFavoriteNumberEntry[] favorites = new CombiBAPFavoriteNumberEntry[0];
    private boolean muted;

    /* CarPlay calls: slot order follows DSICarplay's array.
     * dio_manager's CDIOManager::OnCallStateUpdate (0x15def8) keys its m_calls RB-tree
     * by uniqueCallID when non-empty or phoneNumber when uniqueCallID is empty (0x15df40),
     * and only erases nodes with status == DSI_DISCONNECTED (0).  If an initial setup
     * update arrives without a CallUUID and later updates carry a CallUUID (or vice versa),
     * the disconnect event erases only one key while the other node stays in dio_manager's
     * RB-tree for the rest of the session.  ignoredCallKeys filters those orphaned nodes. */
    private CallState[] calls = new CallState[0];
    private final java.util.HashSet ignoredCallKeys = new java.util.HashSet();
    private int hangupSeq;
    private final String[] activeIds = new String[SLOTS];
    private final long[] activeSince = new long[SLOTS];

    public GatedPhoneService(CombiBAPServicePhone r) {
        this.real = r;
        synchronized (GatedPhoneService.class) { instance = this; }
        CarplayBus.getInstance().on(CarplayBus.EVT_TEL_LISTS, new CarplayBus.Listener() {
            public void onFrame(int type, int flags, byte[] payload, int len) {
                setLists(CarplayBus.parseText(payload, len));
            }
        });
        CarplayBus.getInstance().on(CarplayBus.EVT_TEL_COMM, new CarplayBus.Listener() {
            public void onFrame(int type, int flags, byte[] payload, int len) {
                setComm(CarplayBus.parseText(payload, len));
            }
        });
    }

    /** Wraps once: a second registration must not stack wrappers. */
    public static Object wrap(Object service) {
        if (service instanceof GatedPhoneService || !(service instanceof CombiBAPServicePhone)) return service;
        return new GatedPhoneService((CombiBAPServicePhone)service);
    }

    /** From the patched CarplayDSILifecycleController (DSICarplay.updateCallState). */
    public static void onCarPlayCalls(CallState[] cs) {
        GatedPhoneService g;
        synchronized (GatedPhoneService.class) { g = instance; }
        if (g != null) g.setCalls(cs);
    }

    /** A VC telephone action. Returns false when the gate is off, so stock handles it. */
    static boolean onVcAction(int action, String number) {
        GatedPhoneService g;
        synchronized (GatedPhoneService.class) { g = instance; }
        return g != null && g.handle(action, number);
    }

    private synchronized boolean handle(int action, String number) {
        if (!gated) return false;
        Commands c = commands;
        int result = RESULT_FAILED;
        switch (action) {
            case ACT_ACCEPT:
                /* iAP2 AcceptCall, action 0, no UUID: iOS answers the incoming call. */
                if (has(BAP_RINGING) && tel(TEL_CMD_ACCEPT, false)) result = RESULT_OK;
                real.acceptCallResult(result);
                break;
            case ACT_HANGUP:
                /* iAP2 EndCall, action 0, no UUID: iOS ends the frontmost call,
                 * dialing included.  Always return RESULT_OK so the VC never stays
                 * stuck on "Ending call", and arm a 1 s watchdog that evicts any
                 * orphaned dio_manager call entry if no CallStateUpdate follows. */
                if (liveCalls() > 0) {
                    tel(TEL_CMD_END, false);
                    scheduleHangupWatchdog();
                }
                result = RESULT_OK;
                real.hangupCallResult(result);
                if (liveCalls() == 0) {
                    publishCalls();
                }
                break;
            case ACT_HOLD:
            case ACT_RESUME:
                /* Explicit iAP2 HoldStatusUpdate, not the HID Flash toggle. */
                if (liveCalls() > 0 && tel(TEL_CMD_HOLD, action == ACT_HOLD)) result = RESULT_OK;
                if (action == ACT_HOLD) real.callHoldResult(result);
                else real.resumeCallResult(result);
                break;
            case ACT_SWAP:
                /* iAP2 SwapCalls (no params): iOS swaps the active and the held call. */
                if (liveCalls() > 1 && tel(TEL_CMD_SWAP, false)) result = RESULT_OK;
                real.swapCallsResult(result);
                break;
            case ACT_MUTE_ON:
            case ACT_MUTE_OFF: {
                boolean on = action == ACT_MUTE_ON;
                if (liveCalls() > 0 && tel(TEL_CMD_MUTE, on)) {
                    /* ponytail: optimistic; iOS mute changes made on the phone are not
                     * read back (CommunicationsUpdate is stock's subscription). */
                    muted = on;
                }
                real.updateMicMuteState(muted);
                result = muted == on ? RESULT_OK : RESULT_FAILED;
                break;
            }
            case ACT_RESET_MISSED:
                missedResetTs = newestRecentTs;   /* phone clock: HU and phone clocks may differ */
                missedCalls = missedNumbers = 0;
                real.updateMissedCallIndication(0, 0);
                result = RESULT_OK;
                break;
            case ACT_DIAL:
                if (c != null && number != null && number.length() > 0) { c.dial(number); result = RESULT_OK; }
                real.dialNumberResult(result);
                break;
            default:
                return false;
        }
        Log.i(TAG, "VC action " + action + " -> " + (result == RESULT_OK ? "CarPlay" : "failed"));
        return true;
    }

    private static boolean tel(int action, boolean on) {
        return CarplayBus.getInstance().sendBinary(CarplayBus.CMD_TEL,
            new byte[] { (byte)action, (byte)(on ? 1 : 0) });
    }

    private boolean has(int bapState) {
        for (int i = 0; i < calls.length; i++)
            if (calls[i] != null && bapState(calls[i]) == bapState) return true;
        return false;
    }

    private int liveCalls() {
        int n = 0;
        for (int i = 0; i < calls.length; i++) if (calls[i] != null && bapState(calls[i]) != 0) n++;
        return n;
    }

    private synchronized void setCalls(CallState[] cs) {
        hangupSeq++;
        if (cs == null || cs.length == 0) {
            ignoredCallKeys.clear();
            calls = new CallState[0];
            if (gated) publishCalls();
            return;
        }

        /* 1. If a keyless setup entry (empty uniqueCallID) coexists with a keyed entry
         *    (non-empty uniqueCallID) for the same call, or if any entry is disconnecting
         *    (DSI_DISCONNECTED = 0), evict matching orphan nodes that dio_manager's
         *    RB-tree left behind under the alternate key. */
        boolean anyDisconnected = false;
        for (int i = 0; i < cs.length; i++) {
            CallState a = cs[i];
            if (a == null) continue;
            if (a.status == DSI_DISCONNECTED) {
                anyDisconnected = true;
                ignoredCallKeys.remove(callKey(a));
                for (int j = 0; j < cs.length; j++) {
                    CallState b = cs[j];
                    if (b == null || b == a || b.status == DSI_DISCONNECTED) continue;
                    if (sameOrOrphanedCall(b, a)) {
                        ignoredCallKeys.add(callKey(b));
                    }
                }
            } else if (str(a.uniqueCallID).length() > 0) {
                for (int j = 0; j < cs.length; j++) {
                    CallState b = cs[j];
                    if (b == null || b == a || b.status == DSI_DISCONNECTED) continue;
                    if (str(b.uniqueCallID).length() == 0 && sameOrOrphanedCall(b, a)) {
                        ignoredCallKeys.add(callKey(b));
                    }
                }
            }
        }

        /* 2. If no entry is disconnecting on this update and an ignored key transitions
         *    to DSI_ACTIVE with a new uniqueCallID, allow it again; if all remaining
         *    non-ignored entries are gone, keep orphans suppressed. */
        java.util.ArrayList filtered = new java.util.ArrayList(cs.length);
        for (int i = 0; i < cs.length; i++) {
            CallState c = cs[i];
            if (c == null || c.status == DSI_DISCONNECTED) continue;
            String key = callKey(c);
            if (!anyDisconnected && c.status == DSI_ACTIVE && str(c.uniqueCallID).length() > 0) {
                ignoredCallKeys.remove(key);
            }
            if (!ignoredCallKeys.contains(key)) {
                filtered.add(c);
            }
        }
        calls = (CallState[])filtered.toArray(new CallState[filtered.size()]);
        if (gated) publishCalls();
    }

    private void scheduleHangupWatchdog() {
        final int seq = ++hangupSeq;
        final java.util.ArrayList snapshotKeys = new java.util.ArrayList(calls.length);
        for (int i = 0; i < calls.length; i++) {
            if (calls[i] != null && bapState(calls[i]) != 0) {
                snapshotKeys.add(callKey(calls[i]));
            }
        }
        if (snapshotKeys.isEmpty()) return;
        Thread t = new Thread(new Runnable() {
            public void run() {
                try {
                    Thread.sleep(1000L);
                } catch (InterruptedException e) {
                    return;
                }
                synchronized (GatedPhoneService.this) {
                    if (!gated || hangupSeq != seq) return;
                    boolean changed = false;
                    java.util.ArrayList remaining = new java.util.ArrayList(calls.length);
                    for (int i = 0; i < calls.length; i++) {
                        CallState c = calls[i];
                        if (c == null) continue;
                        String k = callKey(c);
                        if (snapshotKeys.contains(k)) {
                            ignoredCallKeys.add(k);
                            changed = true;
                        } else {
                            remaining.add(c);
                        }
                    }
                    if (changed) {
                        calls = (CallState[])remaining.toArray(new CallState[remaining.size()]);
                        Log.i(TAG, "VC hangup watchdog cleared stale call entry");
                        publishCalls();
                    }
                }
            }
        }, "CPHangupWatchdog");
        t.setDaemon(true);
        t.start();
    }

    private static String callKey(CallState c) {
        String id = str(c.uniqueCallID);
        if (id.length() > 0) return "id:" + id;
        return "num:" + str(c.phoneNumber) + ":" + c.direction;
    }

    private static String normDigits(String s) {
        if (s == null || s.length() == 0) return "";
        StringBuffer sb = new StringBuffer(s.length());
        for (int i = 0; i < s.length(); i++) {
            char ch = s.charAt(i);
            if (ch >= '0' && ch <= '9') sb.append(ch);
        }
        String d = sb.toString();
        return d.length() > 7 ? d.substring(d.length() - 7) : d;
    }

    private static boolean sameOrOrphanedCall(CallState candidate, CallState ref) {
        String d1 = normDigits(candidate.phoneNumber);
        String d2 = normDigits(ref.phoneNumber);
        if (d1.length() > 0 && d1.equals(d2)) return true;
        boolean id1Empty = str(candidate.uniqueCallID).length() == 0;
        boolean id2Empty = str(ref.uniqueCallID).length() == 0;
        if (id1Empty != id2Empty && candidate.direction == ref.direction) return true;
        if (candidate.status == DSI_CONNECTING && candidate.direction == ref.direction
                && (d1.length() == 0 || d2.length() == 0)) {
            return true;
        }
        return false;
    }

    /* ---------------- gated functions ---------------- */

    public synchronized void updateFsgSetup(FsgSetup f) {
        stockFsg = f;
        boolean carplay = f != null && f.getMobileConnectionType() == MCT_APPLE_LINK;
        if (carplay != gated) {
            gated = carplay;
            Log.i(TAG, "cluster phone tab " + (gated ? "-> CarPlay calls" : "-> stock"));
            GatedAddressBook.setCarPlay(gated);                /* Directory: no phonebook */
            if (gated) publishCarPlay();
            else { ignoredCallKeys.clear(); calls = new CallState[0]; replayStock(); }  /* no stale call on the next session */
            return;
        }
        real.updateFsgSetup(gated ? asHfp(f) : f);
    }

    public synchronized void updateFSGOperationState(int telState, boolean privacy, boolean enhanced) {
        haveStockOp = true;
        stockTelState = telState; stockPrivacy = privacy; stockEnhancedPrivacy = enhanced;
        if (!gated) real.updateFSGOperationState(telState, privacy, enhanced);
    }

    public synchronized void updateMobileServiceSupport(boolean[] b) {
        stockSupport = b;
        if (!gated) real.updateMobileServiceSupport(b);
    }

    public synchronized void updateCallStates(CombiBAPCallState[] s, boolean ecallPending) {
        stockStates = s; stockEcallPending = ecallPending;
        if (!gated) real.updateCallStates(s, ecallPending);
    }

    public synchronized void updateCallInfo(CombiBAPCallInfo[] i) {
        stockInfos = i;
        if (!gated) real.updateCallInfo(i);
    }

    public synchronized void updateCallDurations(CallStartTime[] d) {
        stockDurations = d;
        if (!gated) real.updateCallDurations(d);
    }

    public synchronized void updateMicMuteState(boolean m) {
        haveStockMute = true; stockMute = m;
        if (!gated) real.updateMicMuteState(m);
    }

    public synchronized void updateSignalQuality(int q) {
        stockSignal = q;
        if (!gated) real.updateSignalQuality(q);
    }

    public synchronized void updateRegisterState(int reg, int net, int data) {
        haveStockRegState = true; stockRegState = reg; stockNetType = net; stockDataType = data;
        if (!gated) real.updateRegisterState(reg, net, data);
    }

    public synchronized void updateNetworkProvider(int nameValid, String name, int idValid, String id) {
        haveStockProvider = true;
        stockProvNameValid = nameValid; stockProvName = name; stockProvIdValid = idValid; stockProvId = id;
        if (!gated) real.updateNetworkProvider(nameValid, name, idValid, id);
    }

    public synchronized void updateMissedCallIndication(int calls, int numbers) {
        haveStockMissed = true; stockMissedCalls = calls; stockMissedNumbers = numbers;
        if (!gated) real.updateMissedCallIndication(calls, numbers);
    }

    public synchronized void updateCombinedNumbers(CombiBAPCallStackEntry[] e) {
        stockRecents = e;
        if (!gated) real.updateCombinedNumbers(e);
    }

    public synchronized void updateFavoriteList(CombiBAPFavoriteNumberEntry[] e) {
        stockFavorites = e;
        if (!gated) real.updateFavoriteList(e);
    }

    /* ---------------- CarPlay lists ---------------- */

    private synchronized void setLists(CarplayBus.Data d) {
        int rn = Math.max(0, d.num("r.n", 0)), fn = Math.max(0, d.num("f.n", 0));
        CombiBAPCallStackEntry[] r = new CombiBAPCallStackEntry[rn];
        /* Missed calls at the top of the list (newest first), stopping at the first
         * answered or dialed call or at the last VC reset: the unreturned ones. */
        int mCalls = 0, mNumbers = 0;
        boolean countingMissed = true;
        for (int i = 0; i < rn; i++) {
            if (!countingMissed) break;
            long ts = d.num64("r." + i + ".ts", 0);
            if (d.num("r." + i + ".type", -1) != 3 || (ts > 0 && ts <= missedResetTs)) {
                countingMissed = false;
            } else {
                mNumbers++;
                mCalls += Math.max(1, d.num("r." + i + ".occ", 1));
            }
        }
        missedCalls = mCalls;
        missedNumbers = mNumbers;
        newestRecentTs = rn > 0 ? d.num64("r.0.ts", 0) : 0;
        for (int i = 0; i < rn; i++) {
            /* iAP2 recents and favorites: param 1 = RemoteID (number), 2 = DisplayName,
             * which is the formatted number when there is no contact (accessoryd
             * CoreAccessories_18; car capture). */
            String num = d.str("r." + i + ".a", ""), name = d.str("r." + i + ".b", "");
            r[i] = recent(i, isNumber(name) ? "" : name, num,
                          callMode(d.num("r." + i + ".type", -1)), d.num64("r." + i + ".ts", 0));
        }
        CombiBAPFavoriteNumberEntry[] f = new CombiBAPFavoriteNumberEntry[fn];
        for (int i = 0; i < fn; i++) {
            String num = d.str("f." + i + ".a", ""), name = d.str("f." + i + ".b", "");
            f[i] = new CombiBAPFavoriteNumberEntry(i, isNumber(name) ? "" : name, num);
        }
        recents = r;
        favorites = f;
        Log.i(TAG, "CarPlay lists: recents=" + rn + " favorites=" + fn);
        if (gated) publishLists();
    }

    private void publishLists() {
        real.updateCombinedNumbers(recents);
        real.updateFavoriteList(favorites);
        real.updateMissedCallIndication(missedCalls, missedNumbers);
    }

    /* ---------------- CarPlay signal / carrier ---------------- */

    private synchronized void setComm(CarplayBus.Data d) {
        int sig = d.num("signal", -1), reg = d.num("registration", -1);
        /* iAP2 SignalStrength 0..5 -> BAP percent (stock DSI scale, 20 per bar). */
        signalPct = sig < 0 ? 0 : Math.min(sig, 5) * 20;
        regState = bapRegisterState(reg, d.bool("airplane", false));
        carrier = d.str("carrier", "");
        batteryPct = d.num("battery", -1);
        if (gated) publishComm();
    }

    /* BAP charge level buckets, as stock BAPPropertyTelMobileBatteryLevel maps HFP bars;
     * the lowest bucket (below 20 %) is critical there too. */
    public static CombiBAPhoneMobileBatteryLevel batteryLevel(int pct) {
        int level;
        if (pct < 0) level = CombiBAPhoneMobileBatteryLevel.BATTERY_LEVEL_UNKNOWN_LEVEL_NOT_RECEIVED;
        else if (pct == 0) level = CombiBAPhoneMobileBatteryLevel.BATTERY_LEVEL_CHARGE_LEVEL_0;
        else if (pct < 20) level = CombiBAPhoneMobileBatteryLevel.BATTERY_LEVEL_CHARGE_LEVEL_0_19;
        else if (pct < 40) level = CombiBAPhoneMobileBatteryLevel.BATTERY_LEVEL_CHARGE_LEVEL_20_39;
        else if (pct < 60) level = CombiBAPhoneMobileBatteryLevel.BATTERY_LEVEL_CHARGE_LEVEL_40_59;
        else if (pct < 80) level = CombiBAPhoneMobileBatteryLevel.BATTERY_LEVEL_CHARGE_LEVEL_60_79;
        else level = CombiBAPhoneMobileBatteryLevel.BATTERY_LEVEL_CHARGE_LEVEL_80_100;
        return CombiBAPhoneMobileBatteryLevel.createOnlyMobileChargeLevel1(
            new CombiBAPhoneMobileBatteryLevel.ChargeLevel(level, pct >= 0 && pct < 20));
    }

    private void publishComm() {
        real.updateSignalQuality(signalPct);
        real.updateRegisterState(regState, 0, 0);
        int valid = carrier.length() > 0 ? 1 : 0;
        real.updateNetworkProvider(valid, carrier, valid, carrier);
        real.updateMobileBatteryLevel(batteryLevel(batteryPct));
        if (batteryPct != loggedBatteryPct) {
            loggedBatteryPct = batteryPct;
            Log.i(TAG, "iPhone battery " + batteryPct + "% -> VC level "
                + batteryLevel(batteryPct).getMobileChargeLevel1().getChargeLevelPercent());
        }
    }

    /* iAP2 RegistrationStatus (0 unknown, 1 not registered, 2 searching, 3 denied,
     * 4 home, 5 roaming, 6 emergency only) -> BAP RegisterState. */
    private static int bapRegisterState(int iap2, boolean airplane) {
        if (airplane) return 0;
        switch (iap2) {
            case 2: return 2;
            case 3: return 3;
            case 4: return 1;
            case 5: return 4;
            default: return 0;
        }
    }

    private static boolean isNumber(String s) {
        if (s == null || s.length() == 0) return false;
        for (int i = 0; i < s.length(); i++)
            if ("0123456789+*#-() ".indexOf(s.charAt(i)) < 0) return false;
        return true;
    }

    /* iAP2 recents Type 1 incoming / 2 outgoing / 3 missed, 0 other (iOS
     * ACCCommunicationsFeaturePlugin currentRecentsListWithCoalescing; car capture 2). */
    private static int callMode(int iap2Type) {
        switch (iap2Type) {
            case 1: return CALLMODE_RECEIVED;
            case 2: return CALLMODE_DIALED;
            case 3: return CALLMODE_MISSED;
            default: return 0;
        }
    }

    private static CombiBAPCallStackEntry recent(int pos, String name, String num, int mode, long ts) {
        if (ts <= 0) return new CombiBAPCallStackEntry(pos, name, num, 0, mode);
        Calendar c = Calendar.getInstance();
        c.setTime(new java.util.Date(ts > 100000000000L ? ts : ts * 1000L));  /* s or ms */
        return new CombiBAPCallStackEntry(pos, name, num, 0, mode,
            (short)c.get(Calendar.DAY_OF_MONTH), (short)(c.get(Calendar.MONTH) + 1),
            (short)c.get(Calendar.YEAR), (short)c.get(Calendar.HOUR_OF_DAY),
            (short)c.get(Calendar.MINUTE), (short)c.get(Calendar.SECOND),
            new TelAdbEntryStruct());
    }

    /* ---------------- publishing ---------------- */

    private static FsgSetup asHfp(FsgSetup f) {
        return FsgSetup.builder()
            .setInternalSimCardReaderPresent(f.isInternalSimCardReaderPresent())
            .setCableConnectionPossible(f.isCableConnectionPossible())
            .setHfpConnectionPossible(true)
            .setRsapConnectionPossible(f.isRsapConnectionPossible())
            .setAppleLinkConnectionPossible(f.isAppleLinkConnectionPossible())
            .setGoogleLinkConnectionPossible(f.isGoogleLinkConnectionPossible())
            .setMobileConnectionType(MCT_HFP)
            .build();
    }

    private void publishCarPlay() {
        real.updateFsgSetup(asHfp(stockFsg));
        real.updateFSGOperationState(TEL_STATE_PHONE_ON_NO_SIM, false, true);
        boolean[] support = new boolean[MOBILE_SERVICE_SUPPORT_MAX];
        support[MOBILE_SERVICE_SUPPORT_CALL_STATE] = true;
        support[MOBILE_SERVICE_SUPPORT_CALL_INFO] = true;
        support[MOBILE_SERVICE_SUPPORT_CALL_DURATION_SYNC] = true;
        support[MOBILE_SERVICE_SUPPORT_DIAL_NUMBER] = true;
        support[MOBILE_SERVICE_SUPPORT_HANGUP_CALL] = true;
        support[MOBILE_SERVICE_SUPPORT_ACCEPT_CALL] = true;
        support[MOBILE_SERVICE_SUPPORT_CALL_HOLD] = true;
        support[MOBILE_SERVICE_SUPPORT_RESUME_CALL] = true;
        support[MOBILE_SERVICE_SUPPORT_MICRO_MUTE_ON_OFF] = true;
        support[MOBILE_SERVICE_SUPPORT_COMBINED_NUMBERS] = true;
        support[MOBILE_SERVICE_SUPPORT_FAVORITE_LIST] = true;
        support[MOBILE_SERVICE_SUPPORT_MISSED_CALL_INDICATION] = true;
        support[MOBILE_SERVICE_SUPPORT_REGISTER_STATE] = true;
        support[MOBILE_SERVICE_SUPPORT_NETWORK_PROVIDER] = true;
        support[MOBILE_SERVICE_SUPPORT_SIGNAL_QUALITY] = true;
        support[MOBILE_SERVICE_SUPPORT_MOBILE_BATTERY_LEVEL] = true;
        real.updateMobileServiceSupport(support);
        muted = false;
        real.updateMicMuteState(false);
        publishCalls();
        publishLists();
        publishComm();
    }

    private void replayStock() {
        if (stockFsg != null) real.updateFsgSetup(stockFsg);
        if (haveStockOp) real.updateFSGOperationState(stockTelState, stockPrivacy, stockEnhancedPrivacy);
        if (stockSupport != null) real.updateMobileServiceSupport(stockSupport);
        real.updateCallStates(stockStates, stockEcallPending);  /* null = stock's EMPTY_CALL_STATES */
        real.updateCallInfo(stockInfos);
        real.updateCallDurations(stockDurations != null ? stockDurations : noDurations());
        real.updateMicMuteState(haveStockMute && stockMute);
        real.updateCombinedNumbers(stockRecents != null ? stockRecents : new CombiBAPCallStackEntry[0]);
        real.updateFavoriteList(stockFavorites != null ? stockFavorites : new CombiBAPFavoriteNumberEntry[0]);
        real.updateSignalQuality(stockSignal >= 0 ? stockSignal : 0);
        if (haveStockRegState) real.updateRegisterState(stockRegState, stockNetType, stockDataType);
        else real.updateRegisterState(0, 0, 0);
        if (haveStockProvider) real.updateNetworkProvider(stockProvNameValid, stockProvName, stockProvIdValid, stockProvId);
        else real.updateNetworkProvider(0, "", 0, "");
        if (haveStockMissed) real.updateMissedCallIndication(stockMissedCalls, stockMissedNumbers);
        else real.updateMissedCallIndication(0, 0);
        real.updateMobileBatteryLevel(stockBattery != null ? stockBattery
            : CombiBAPhoneMobileBatteryLevel.createOnlyMobileChargeLevel1(new CombiBAPhoneMobileBatteryLevel.ChargeLevel(
                CombiBAPhoneMobileBatteryLevel.BATTERY_LEVEL_UNKNOWN_DEVICE_NOT_CONNECTED, false)));
    }

    private void publishCalls() {
        CombiBAPCallState[] states = new CombiBAPCallState[SLOTS];
        CombiBAPCallInfo[] infos = new CombiBAPCallInfo[SLOTS];
        CallStartTime[] durations = noDurations();
        long now = System.currentTimeMillis();
        int n = 0;
        for (int i = 0; i < calls.length && n < SLOTS; i++) {
            CallState c = calls[i];
            int st = c != null ? bapState(c) : 0;
            if (st == 0) continue;
            states[n] = new CombiBAPCallState(st, BAP_CALL_TYPE_VOICE, 0);
            states[n].setCallOptionAcceptCallEnabled(st == BAP_RINGING);
            states[n].setCallOptionCallHoldEnabled(st == BAP_ACTIVE);
            states[n].setCallOptionResumeCallEnabled(st == BAP_HELD);
            infos[n] = new CombiBAPCallInfo(name(c), str(c.phoneNumber), BAP_CALL_TYPE_VOICE, 0);
            if (st == BAP_ACTIVE || st == BAP_HELD) {
                String id = str(c.uniqueCallID);
                if (!id.equals(activeIds[n])) { activeIds[n] = id; activeSince[n] = now; }
                durations[n] = elapsed(now - activeSince[n]);
            } else {
                activeIds[n] = null;
            }
            n++;
        }
        for (int i = n; i < SLOTS; i++) activeIds[i] = null;
        real.updateCallStates(states, false);
        real.updateCallInfo(infos);
        real.updateCallDurations(durations);
        if (n == 0 && muted) { muted = false; real.updateMicMuteState(false); }
    }

    private static int bapState(CallState c) {
        switch (c.status) {
            case DSI_ACTIVE: return BAP_ACTIVE;
            case DSI_HOLDING: return BAP_HELD;
            case DSI_CONNECTING: return c.direction == DSI_INCOMING ? BAP_RINGING : BAP_DIALING;
            case DSI_DISCONNECTED:
            default: return 0;
        }
    }

    /* Stock shows an empty name when it equals the number (TelBAPCall.getDisplayName). */
    private static String name(CallState c) {
        String n = str(c.callerName);
        return n.equals(str(c.phoneNumber)) ? "" : n;
    }

    private static String str(String s) { return s != null ? s : ""; }

    private static CallStartTime elapsed(long ms) {
        int s = (int)(ms / 1000);
        return new CallStartTime(s / 3600, (s / 60) % 60, s % 60);
    }

    private static CallStartTime[] noDurations() {
        CallStartTime[] d = new CallStartTime[SLOTS];
        for (int i = 0; i < SLOTS; i++) d[i] = new CallStartTime(NO_DURATION);
        return d;
    }

    /* ---------------- pass-through ---------------- */

    public void updateLockState(int i) { real.updateLockState(i); }
    public synchronized void updateDisconnectReason(int i) { if (!gated) real.updateDisconnectReason(i); }
    public void dialNumberResult(int i) { real.dialNumberResult(i); }
    public void dialNumberFromAdbEntryResult(int i) { real.dialNumberFromAdbEntryResult(i); }
    public void dialServiceResult(int i) { real.dialServiceResult(i); }
    public void confirmEmergencyCallResult(int i) { real.confirmEmergencyCallResult(i); }
    public void hangupCallResult(int i) { real.hangupCallResult(i); }
    public void acceptCallResult(int i) { real.acceptCallResult(i); }
    public void callHoldResult(int i) { real.callHoldResult(i); }
    public void resumeCallResult(int i) { real.resumeCallResult(i); }
    public void releaseActiveCallAcceptWaitingCallResult(int i) { real.releaseActiveCallAcceptWaitingCallResult(i); }
    public void swapCallsResult(int i) { real.swapCallsResult(i); }
    public void callHoldAcceptWaitingCallResult(int i) { real.callHoldAcceptWaitingCallResult(i); }
    public void releaseAllCallsAcceptWaitingCallResult(int i) { real.releaseAllCallsAcceptWaitingCallResult(i); }
    public void setWaitingCallOnHoldResult(int i) { real.setWaitingCallOnHoldResult(i); }
    public void joinCallsResult(int i) { real.joinCallsResult(i); }
    public void splitCallResult(int i) { real.splitCallResult(i); }
    public synchronized void updateMobileBatteryLevel(CombiBAPhoneMobileBatteryLevel b) {
        stockBattery = b;
        if (!gated) real.updateMobileBatteryLevel(b);
    }
    public void updateMissedCalls(CombiBAPCallStackEntry[] acombibapcallstackentry) { real.updateMissedCalls(acombibapcallstackentry); }
    public void updateReceivedCalls(CombiBAPCallStackEntry[] acombibapcallstackentry) { real.updateReceivedCalls(acombibapcallstackentry); }
    public void updateDialedNumbers(CombiBAPCallStackEntry[] acombibapcallstackentry) { real.updateDialedNumbers(acombibapcallstackentry); }
    public void updateRingToneMuteState(boolean flag) { real.updateRingToneMuteState(flag); }
    public void updateAutomaticRedialActive(boolean flag) { real.updateAutomaticRedialActive(flag); }
    public void updateAutomaticRedialExtendedInfo(int i, String s, String s1, int j) { real.updateAutomaticRedialExtendedInfo(i, s, s1, j); }
    public void updateSupportedServiceNumbers(boolean flag, boolean flag1, boolean flag2, boolean flag3) { real.updateSupportedServiceNumbers(flag, flag1, flag2, flag3); }
}
