package de.audi.app.terminalmode.audio;

/** COMPILE-ONLY stand-in for the listener registration methods referenced by the
 * MU0918 and MU1316 stock ExternalEventsListener implementations. */
public interface IAudioManager {
    void addAudioContextListener(IAudioStateListener listener);
    void removeAudioContextListener(IAudioStateListener listener);
}
