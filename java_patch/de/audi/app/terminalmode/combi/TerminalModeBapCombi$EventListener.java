/*
 * TerminalModeBapCombi$EventListener
 *
 * Handles track data events and cover art updates for CarPlay.
 * Delegates cover art logic to com.luka.carplay.coverart.CoverArt.
 *
 *
 */
package de.audi.app.terminalmode.combi;

import com.luka.carplay.coverart.CoverArt;
import com.luka.carplay.framework.Log;
import de.audi.app.terminalmode.events.DefaultEventListener;
import de.audi.app.terminalmode.events.TrackDataChangedEvent;
import de.audi.app.terminalmode.events.TrackPlayPositionEvent;
import de.audi.atip.interapp.bap.data.TimeStamp;
import de.audi.atip.interapp.combi.bap.audio.CombiBAPServiceTerminalMode;
import de.audi.atip.interapp.combi.bap.audio.data.CombiBAPCurrentStationInfo;
import de.audi.atip.interapp.combi.bap.audio.data.PlayPosition;
import de.audi.atip.log.LogChannel;
import java.lang.reflect.Field;

class TerminalModeBapCombi$EventListener extends DefaultEventListener implements CoverArt.Callback {

    private static final String TAG = "TMEventListener";

    private final TerminalModeBapCombi this$0;

    /* Track state */
    private volatile String lastTrackKey = null;
    private volatile String currentTitle = "";
    private volatile String currentArtist = "";
    private volatile String currentAlbum = "";
    private volatile boolean sentWithoutCover = false;

    private TerminalModeBapCombi$EventListener(TerminalModeBapCombi outer) {
        this.this$0 = outer;
    }

    /* Synthetic constructor */
    TerminalModeBapCombi$EventListener(TerminalModeBapCombi outer, TerminalModeBapCombi$1 unused) {
        this(outer);
        startCoverArt();
    }

    /* ============================================================
     * Cover Art Integration
     * ============================================================ */

    private void startCoverArt() {
        CoverArt.getInstance().start(this);
    }

    public void onNewCoverArt(String path, int artId) {
        Log.i(TAG, "Cover art updated: " + path + " id=" + artId);

        /* Send update if we have track info */
        if (currentTitle.length() > 0 || currentArtist.length() > 0 || currentAlbum.length() > 0) {
            int effectiveArtId = artId;
            if (sentWithoutCover) {
                /* Use different ID to force VC to recognize as new picture */
                effectiveArtId = artId + 1;
                sentWithoutCover = false;
                Log.i(TAG, "Late cover art - using modified id=" + effectiveArtId);
            }
            sendNowPlaying(currentTitle, currentArtist, currentAlbum, path, effectiveArtId);
        }
    }

    /* ============================================================
     * Track Events
     * ============================================================ */

    public void updateNowPlayingData(TrackDataChangedEvent track) {
        String rawTitle = track.getTitle();
        String rawArtist = track.getArtist();
        String rawAlbum = track.getAlbum();

        if (rawTitle == null) rawTitle = "";
        if (rawArtist == null) rawArtist = "";
        if (rawAlbum == null) rawAlbum = "";

        /* Skip empty or placeholder */
        if (rawTitle.length() == 0 && rawArtist.length() == 0 && rawAlbum.length() == 0) {
            return;
        }
        if ("%".equals(rawTitle) && "%".equals(rawArtist) && "%".equals(rawAlbum)) {
            return;
        }

        String title = normalizeField(rawTitle);
        String artist = normalizeField(rawArtist);
        String album = normalizeField(rawAlbum);

        if (title.length() == 0 && artist.length() == 0 && album.length() == 0) {
            return;
        }

        currentTitle = title;
        currentArtist = artist;
        currentAlbum = album;

        String trackKey = title + "|" + artist + "|" + album;
        lastTrackKey = trackKey;

        Log.d(TAG, "Track: " + title + " - " + artist);

        /* Send with cover art if available */
        CoverArt ca = CoverArt.getInstance();
        if (ca.hasCoverArt()) {
            sentWithoutCover = false;
            sendNowPlaying(title, artist, album, ca.getPath(), ca.getArtId());
        } else {
            sentWithoutCover = true;
            sendNowPlaying(title, artist, album, null, 0);
        }
    }

    public void updatePlayPosition(TrackPlayPositionEvent position) {
        CombiBAPServiceTerminalMode bapService = getBapService();
        if (bapService == null) return;

        if (position.getTotalTimeOfTrack() == 0) {
            bapService.updatePlayPosition(
                PlayPosition.builder()
                    .setTimePosition(TimeStamp.getInstanceFromSeconds(65535))
                    .setTotalPlayTime(TimeStamp.getInstanceFromSeconds(65535))
                    .build()
            );
        } else {
            bapService.updatePlayPosition(
                PlayPosition.builder()
                    .setTimePosition(TimeStamp.getInstanceFromSeconds(position.getPlayTime()))
                    .setTotalPlayTime(TimeStamp.getInstanceFromSeconds(position.getTotalTimeOfTrack()))
                    .build()
            );
        }
    }

    /* ============================================================
     * Now Playing Update
     * ============================================================ */

    private void sendNowPlaying(String title, String artist, String album, String coverPath, int coverId) {
        CombiBAPServiceTerminalMode bapService = getBapService();
        if (bapService == null) {
            Log.w(TAG, "No BAP service");
            return;
        }

        int titleType = title.length() > 0 ? 6 : 0;
        int artistType = artist.length() > 0 ? 73 : 0;
        int albumType = album.length() > 0 ? 74 : 0;

        CombiBAPCurrentStationInfo info = new CombiBAPCurrentStationInfo();
        info.setPrimaryInformation(title, titleType, 0);
        info.setSecondaryInformation(artist, artistType);
        info.setTertiaryInformation(album, albumType);
        info.setListRef(0);  /* Must be 0 - VC rejects non-zero listRef */
        info.setListAbsolutePosition(0);
        info.setPresetListRef(0);
        info.setPresetListAbsolutePosition(0);
        info.setCommonListRef(0);
        info.setCommonListAbsolutePosition(0);

        if (coverPath != null && coverPath.length() > 0 && coverId != 0) {
            info.setPicture(coverId, coverPath);
            Log.i(TAG, "With cover: " + coverPath + " id=" + coverId);
        }

        bapService.updateCurrentStation(info);
        bapService.updateActiveInfoState(0);

        Log.i(TAG, "Sent now playing");
        /* Cover art push handled by AppConnectorTerminalMode.updateStationArt() */
    }

    /* ============================================================
     * Utilities
     * ============================================================ */

    private String normalizeField(String value) {
        if (value == null) return "";
        String s = value.trim();
        if (s.length() == 0) return "";

        /* Remove "Lossless" suffix */
        String lower = s.toLowerCase();
        int idx = lower.indexOf("lossless");
        if (idx >= 0) {
            s = s.substring(0, idx).trim();
        }

        /* Remove trailing non-alphanumeric */
        int end = s.length();
        while (end > 0) {
            char c = s.charAt(end - 1);
            if (Character.isLetterOrDigit(c)) break;
            end--;
        }
        if (end != s.length()) {
            s = s.substring(0, end).trim();
        }

        return s;
    }

    private LogChannel getLogger() {
        try {
            Field field = this.this$0.getClass().getDeclaredField("lc");
            field.setAccessible(true);
            return (LogChannel) field.get(this.this$0);
        } catch (Exception e) {
            return null;
        }
    }

    private CombiBAPServiceTerminalMode getBapService() {
        try {
            Field field = this.this$0.getClass().getDeclaredField("bapService");
            field.setAccessible(true);
            return (CombiBAPServiceTerminalMode) field.get(this.this$0);
        } catch (Exception e) {
            return null;
        }
    }
}
