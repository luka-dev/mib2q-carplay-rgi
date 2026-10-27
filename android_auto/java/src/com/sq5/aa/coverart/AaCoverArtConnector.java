/*
 * Combi (BAP audio) side of the Android Auto cover art, owned by the rebuilt MU0918
 * AppConnectorTerminalMode (three one-line hooks there; everything else lives here).
 *
 * luka's CarPlay port does this in two stock classes: TerminalModeBapCombi merges the picture
 * into the now-playing CombiBAPCurrentStationInfo (listRef forced to 0, "VC rejects non-zero
 * listRef"; late art re-sends the now-playing info) and AppConnectorTerminalMode pushes it to
 * the picture manager (responseCoverArt, source 21) before the stock station update, with a
 * provider for re-requests.  On Android Auto the now-playing info already arrives at
 * AppConnectorTerminalMode.updateCurrentStation through the untouched stock
 * TerminalModeBapCombi, so both halves are done here and TerminalModeBapCombi stays stock:
 *
 *   beforeStation (terminal-mode dispatcher, every stock now-playing update): during an Android
 *     Auto session set listRef 0, attach the current picture (only to a real track: title,
 *     artist or album), push it (or, without art, the stock default picture - exactly what
 *     stock AppConnectorMedia does) while TerminalMode has the audio focus.
 *   coverArtChanged (terminal-mode dispatcher, posted by AaCoverArt): art that arrives or is
 *     cleared after the track info re-runs the last now-playing update through the connector,
 *     so the VC gets the new picture id (luka's late-art re-send).
 *   focusChanged / session end: drop the provider mapping (handle 0 is shared with Media).
 *
 * Outside an Android Auto session (CarPlay, Media, radio) and with the SD kill switch, every
 * method is a no-op: stock MU0918 behaviour.
 */
package com.sq5.aa.coverart;

import de.audi.app.combi.bap.app.kombipictures.IPictureManager;
import de.audi.atip.interapp.combi.bap.audio.data.CombiBAPCurrentStationInfo;
import org.dsi.ifc.global.ResourceLocator;

public final class AaCoverArtConnector implements AaCoverArt.Host {
    /** Path the VC picture server (on the RCC) can open: MMX /dev/shmem files as /net/mmx/dev/shmem/...
     *  (stock OnlineMediaComponent also hands the cockpit /net/mmx/ paths). SD flag sq5_coverart_localpath
     *  = the previous local path. Pure except for the flag; host-tested. */
    public static String vcUrl(String path) {
        if (path == null || !path.startsWith("/dev/shmem/")) return path;
        String[] roots = {"/fs/sda0/", "/net/mmx/fs/sda0/"};
        for (int i = 0; i < roots.length; i++) {
            try { if (new java.io.File(roots[i] + "sq5_coverart_localpath").exists()) return path; } catch (Throwable t) { /* ignore */ }
        }
        return "/net/mmx" + path;
    }

    /** luka's verified push source type for terminal-mode art (AbstractPictureManager maps 21 -> 21). */
    public static final int SOURCE_TYPE_VC = 21;
    static final int TM_LIST_REF = 0;

    /** Implemented by AppConnectorTerminalMode (anonymous inner class). */
    public interface Owner {
        boolean inFocus();

        void resend(CombiBAPCurrentStationInfo info);
    }

    private final IPictureManager manager;
    private final Owner owner;
    private final AaCoverArtProvider provider;
    private final boolean enabled;
    private CombiBAPCurrentStationInfo last;   /* guarded by this */
    private String lastPush = "";              /* guarded by this: log de-dup */

    /** Never throws; returns null if the connector cannot be built (caller keeps stock behaviour). */
    public static AaCoverArtConnector create(IPictureManager manager, Owner owner) {
        try {
            return new AaCoverArtConnector(manager, owner, !AaCoverArt.killed(), true);
        } catch (Throwable t) {
            AaCoverArt.log("connector init failed: " + t);
            return null;
        }
    }

    AaCoverArtConnector(IPictureManager manager, Owner owner, boolean enabled, boolean installMux) {
        this.manager = manager;
        this.owner = owner;
        this.enabled = enabled && manager != null;
        this.provider = new AaCoverArtProvider(manager);
        if (!this.enabled) {
            AaCoverArt.log("connector: disabled (" + (manager == null ? "no picture manager" : AaCoverArt.KILL_FLAG)
                + ") - stock TerminalMode cover art");
            return;
        }
        boolean mux = installMux && AaCoverArtMux.install(manager, provider);
        manager.setNotification(0);   /* IPictureManager.NOTIFICATION_COVER_ART: allow pushes */
        AaCoverArt.setHost(this);
        AaCoverArt.log("connector ready (mux=" + mux + (mux ? "" : ": direct push only") + ")");
    }

    AaCoverArtProvider provider() {
        return provider;
    }

    static boolean hasText(CombiBAPCurrentStationInfo s) {
        return nonEmpty(s.getPrimaryInformation()) || nonEmpty(s.getSecondaryInformation())
            || nonEmpty(s.getTertiaryInformation());
    }

    private static boolean nonEmpty(String s) {
        return s != null && s.length() > 0;
    }

    /** AppConnectorTerminalMode.updateCurrentStation, before the stock update. Never throws. */
    public void beforeStation(CombiBAPCurrentStationInfo s) {
        try {
            if (!enabled || s == null) return;
            if (!AaCoverArt.isSessionActive()) {
                forget();
                return;
            }
            s.setListRef(TM_LIST_REF);
            s.setListAbsolutePosition(0);
            AaCoverArt.Ref r = AaCoverArt.current();
            ResourceLocator rl = null;
            if (r != null && hasText(s)) {
                String url = vcUrl(r.path);
                s.setPicture(r.id, url);
                rl = new ResourceLocator(r.id, url);
            } else {
                s.setPicture(-1, null);
            }
            synchronized (this) {
                last = s;
            }
            if (!owner.inFocus()) {
                provider.clear();
                note("not pushed: TerminalMode not in audio focus");
                return;
            }
            provider.set(TM_LIST_REF, rl);
            manager.responseCoverArt(TM_LIST_REF, SOURCE_TYPE_VC, rl, true);
            note(rl == null ? "pushed default picture (no Android Auto art)"
                : "pushed art id=" + r.id + " " + r.path);
        } catch (Throwable t) {
            AaCoverArt.log("beforeStation failed: " + t);
        }
    }

    /** AaCoverArt: art arrived, changed or was cleared (terminal-mode dispatcher). */
    public void coverArtChanged() {
        try {
            if (!enabled) return;
            if (!AaCoverArt.isSessionActive()) {
                forget();
                note("session end: VC mapping cleared");
                return;
            }
            CombiBAPCurrentStationInfo s;
            synchronized (this) {
                s = last;
            }
            if (s == null || !hasText(s)) return;   /* next track update carries it */
            if (!owner.inFocus()) return;           /* focus return re-sends the cached track */
            AaCoverArt.Ref r = AaCoverArt.current();
            int id = r == null ? -1 : r.id;
            if (id == s.getPictureID()) return;
            AaCoverArt.log("re-sending now-playing with " + (r == null ? "no art" : "art id=" + id));
            owner.resend(s);
        } catch (Throwable t) {
            AaCoverArt.log("coverArtChanged failed: " + t);
        }
    }

    /** AppConnectorTerminalMode.notifyAudioApplicationInFocusChanged. Never throws. */
    public void focusChanged(boolean inFocus) {
        try {
            if (enabled && !inFocus) provider.clear();
        } catch (Throwable t) {
            /* ignore */
        }
    }

    private void forget() {
        synchronized (this) {
            last = null;
        }
        provider.clear();
    }

    private void note(String msg) {
        synchronized (this) {
            if (msg.equals(lastPush)) return;
            lastPush = msg;
        }
        AaCoverArt.log(msg);
    }
}
