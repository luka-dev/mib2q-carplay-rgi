/*
 * GatedAddressBook - wrapper around the stock CombiBAPServiceAddressBook (VC telephone
 * tab "Directory", BAP FctID 51 PbState / 52 phonebook / 53 speller).
 *
 * CarPlay gives the car no phonebook: iAP2 carries only the recents and favorites
 * lists (GatedPhoneService), and the HU's own phonebook comes over Bluetooth PBAP,
 * which is not connected during wired CarPlay.  While GatedPhoneService's gate is on
 * the VC is told DOWNLOAD_STATE_NO_PHONE_BOOK_AVAILABLE with 0 entries instead of an
 * empty list.  Stock's PbState and service support are cached and replayed on gate-off;
 * every other method passes straight through.
 *
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */
package com.luka.carplay.telephony;

import com.luka.carplay.framework.Log;
import de.audi.atip.interapp.combi.bap.phone.CombiBAPServiceAddressBook;
import de.audi.atip.interapp.combi.bap.phone.data.CombiBAPPhonebookEntry;
import de.audi.atip.interapp.combi.bap.phone.data.CombiBAPPhonebookEntryDetails;

public class GatedAddressBook implements CombiBAPServiceAddressBook {
    private static final String TAG = "Telephony";
    private static final int DOWNLOAD_STATE_NO_PHONE_BOOK_AVAILABLE = 0;

    private static GatedAddressBook instance;

    private final CombiBAPServiceAddressBook real;
    private boolean gated;
    private boolean haveStockState, haveStockSupport;
    private int stockState, stockEntries;
    private boolean stockPbState, stockPhonebook, stockSpeller;

    public GatedAddressBook(CombiBAPServiceAddressBook r) {
        this.real = r;
        synchronized (GatedAddressBook.class) { instance = this; }
    }

    /** Wraps once: a second registration must not stack wrappers. */
    public static Object wrap(Object service) {
        if (service instanceof GatedAddressBook || !(service instanceof CombiBAPServiceAddressBook)) return service;
        return new GatedAddressBook((CombiBAPServiceAddressBook)service);
    }

    /** From GatedPhoneService when its CarPlay gate flips. */
    static void setCarPlay(boolean on) {
        GatedAddressBook g;
        synchronized (GatedAddressBook.class) { g = instance; }
        if (g != null) g.setGated(on);
    }

    private synchronized void setGated(boolean on) {
        if (on == gated) return;
        gated = on;
        Log.i(TAG, "cluster directory " + (on ? "-> no phonebook (CarPlay)" : "-> stock"));
        if (on) {
            /* PbState must stay supported so the VC reads the "no phonebook" state. */
            real.updateMobileServiceSupport(true, stockPhonebook, false);
            real.updatePbState(DOWNLOAD_STATE_NO_PHONE_BOOK_AVAILABLE, 0);
        } else {
            if (haveStockSupport) real.updateMobileServiceSupport(stockPbState, stockPhonebook, stockSpeller);
            real.updatePbState(haveStockState ? stockState : DOWNLOAD_STATE_NO_PHONE_BOOK_AVAILABLE,
                               haveStockState ? stockEntries : 0);
        }
    }

    /* ---------------- gated ---------------- */

    public synchronized void updateMobileServiceSupport(boolean pbState, boolean phonebook, boolean speller) {
        haveStockSupport = true;
        stockPbState = pbState; stockPhonebook = phonebook; stockSpeller = speller;
        if (!gated) real.updateMobileServiceSupport(pbState, phonebook, speller);
    }

    public synchronized void updatePbState(int state, int entries) {
        haveStockState = true;
        stockState = state; stockEntries = entries;
        if (!gated) real.updatePbState(state, entries);
    }

    public synchronized void updatePhonebookDownloadProgress(int a, int b, int c, int d) {
        if (!gated) real.updatePhonebookDownloadProgress(a, b, c, d);
    }

    /* ---------------- pass-through ---------------- */

    public void responsePhonebookListElements(int i, CombiBAPPhonebookEntry[] e) { real.responsePhonebookListElements(i, e); }
    public void responsePhonebookEntryDetails(long l, CombiBAPPhonebookEntryDetails[] d) { real.responsePhonebookEntryDetails(l, d); }
    public void phonebookChanged(int i) { real.phonebookChanged(i); }
    public void pbSpellerResult(int i, int j, int k) { real.pbSpellerResult(i, j, k); }
}
