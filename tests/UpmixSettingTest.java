package com.luka.carplay.core;

import com.luka.carplay.connectivity.CarPlaySettingsUi;
import com.luka.carplay.tone.SoundControlUi;

/** CarPlaySettingsUi.internalAmp, 3-choice UpMix mapping, and SoundControlUi amplifier capabilities. */
public final class UpmixSettingTest {
    public static void main(String[] args) {
        // org.dsi.ifc.audio DEVICETYPE_*: BASIC_2CH 1, BASIC_4CH 2, STANDARD_6CH 3 = internal
        for (int t = 1; t <= 3; t++)
            if (!CarPlaySettingsUi.internalAmp(t)) throw new AssertionError("internal type " + t);
        // unknown (-1), audio management (0), ALPINE_BO_AUDI 4, PREMIUM_BOSE_AUDI 17, ADVANCED_BO_AUDI 21 ...
        int[] external = {-1, 0, 4, 5, 17, 18, 19, 20, 21, 22, 23};
        for (int i = 0; i < external.length; i++)
            if (CarPlaySettingsUi.internalAmp(external[i])) throw new AssertionError("external type " + external[i]);

        // 3 choices (1:1 with hook levels): Off (0) -> 0, Normal (1) -> 1, Intense (2) -> 2
        if (CarPlaySettingsUi.choiceToLevel(0) != 0) throw new AssertionError("choice 0");
        if (CarPlaySettingsUi.choiceToLevel(1) != 1) throw new AssertionError("choice 1");
        if (CarPlaySettingsUi.choiceToLevel(2) != 2) throw new AssertionError("choice 2");
        if (CarPlaySettingsUi.levelToChoice(0) != 0) throw new AssertionError("level 0");
        if (CarPlaySettingsUi.levelToChoice(1) != 1) throw new AssertionError("level 1 -> Normal");
        if (CarPlaySettingsUi.levelToChoice(2) != 2) throw new AssertionError("level 2 -> Intense");

        // Subwoofer is hidden on BASIC_2CH (1) / BASIC_4CH (2) and unlocked on >= 3 (9VD 6ch, Bose, B&O)
        if (SoundControlUi.subwooferSupported(1)) throw new AssertionError("2ch no sub");
        if (SoundControlUi.subwooferSupported(2)) throw new AssertionError("4ch no sub");
        if (!SoundControlUi.subwooferSupported(3)) throw new AssertionError("6ch 9VD has sub");
        for (int i = 0; i < external.length; i++) {
            if (!SoundControlUi.subwooferSupported(external[i])) throw new AssertionError("sub expected for " + external[i]);
        }

        // Surround is hidden on internal amps (1..3) and enabled on all external MOST amps (B&O 4/21, Bose 17/18/19, etc.)
        for (int t = 1; t <= 3; t++)
            if (SoundControlUi.surroundSupported(t)) throw new AssertionError("no surround on internal " + t);
        for (int i = 0; i < external.length; i++)
            if (!SoundControlUi.surroundSupported(external[i])) throw new AssertionError("surround expected for " + external[i]);

        // VNC / GALA / AudioPilot widget & range per amplifier hardware:
        //   - Bose (17, 18, 19): boolean AudioPilot checkbox (vncMaxLevel = 0, soundSystemChoice = 4)
        //   - Internal DSP (1..3) and Alpine B&O / Bentley (4, 5): 0..7 rotary (vncMaxLevel = 7)
        //   - B&O Adv / Bentley Adv / Burmester (21, 22, 23): 0..24 rotary (vncMaxLevel = 24)
        int[] boseAmps = {17, 18, 19};
        for (int i = 0; i < boseAmps.length; i++) {
            if (!SoundControlUi.boseAudioPilotAmp(boseAmps[i])) throw new AssertionError("bose expected for " + boseAmps[i]);
            if (SoundControlUi.vncMaxLevel(boseAmps[i]) != 0) throw new AssertionError("bose vncMax 0 for " + boseAmps[i]);
            if (SoundControlUi.vncDefaultLevel(boseAmps[i]) != 1) throw new AssertionError("bose default 1 for " + boseAmps[i]);
            if (SoundControlUi.soundSystemChoice(boseAmps[i], 3) != 4) throw new AssertionError("bose choice 4 for " + boseAmps[i]);
        }
        int[] rotary7Amps = {1, 2, 3, 4, 5};
        for (int i = 0; i < rotary7Amps.length; i++) {
            if (SoundControlUi.boseAudioPilotAmp(rotary7Amps[i])) throw new AssertionError("not bose " + rotary7Amps[i]);
            if (SoundControlUi.vncMaxLevel(rotary7Amps[i]) != 7) throw new AssertionError("vncMax 7 for " + rotary7Amps[i]);
            if (SoundControlUi.vncDefaultLevel(rotary7Amps[i]) != 2) throw new AssertionError("vncDefault 2 for " + rotary7Amps[i]);
        }
        int[] rotary24Amps = {21, 22, 23};
        for (int i = 0; i < rotary24Amps.length; i++) {
            if (SoundControlUi.boseAudioPilotAmp(rotary24Amps[i])) throw new AssertionError("not bose " + rotary24Amps[i]);
            if (SoundControlUi.vncMaxLevel(rotary24Amps[i]) != 24) throw new AssertionError("vncMax 24 for " + rotary24Amps[i]);
            if (SoundControlUi.vncDefaultLevel(rotary24Amps[i]) != 12) throw new AssertionError("vncDefault 12 for " + rotary24Amps[i]);
            if (SoundControlUi.soundSystemChoice(rotary24Amps[i], 10) != 6) throw new AssertionError("adv choice 6 for " + rotary24Amps[i]);
        }
        if (SoundControlUi.soundSystemChoice(-1, 0) != 0) throw new AssertionError("choice 0 preserved");
        if (SoundControlUi.soundSystemChoice(-1, 4) != 4 || SoundControlUi.soundSystemChoice(-1, 8) != 4)
            throw new AssertionError("Bose / Porsche Bose -> 4");
        if (SoundControlUi.soundSystemChoice(-1, 5) != 5 || SoundControlUi.soundSystemChoice(-1, 9) != 5)
            throw new AssertionError("Alpine B&O / Bentley -> 5");
        if (SoundControlUi.soundSystemChoice(-1, 6) != 6 || SoundControlUi.soundSystemChoice(-1, 7) != 6 || SoundControlUi.soundSystemChoice(-1, 10) != 6)
            throw new AssertionError("B&O Adv / Burmester / Bentley Adv -> 6");

        // Unified /tmp/carplay_settings key=value format
        if (!"/tmp/carplay_settings".equals(CarPlaySettingsUi.SETTINGS_FILE))
            throw new AssertionError("settings file path: " + CarPlaySettingsUi.SETTINGS_FILE);
        if (!"spatial=1\nupmix=2\n".equals(CarPlaySettingsUi.formatSettings(1, 2)))
            throw new AssertionError("formatSettings(1, 2): " + CarPlaySettingsUi.formatSettings(1, 2));
        String full = CarPlaySettingsUi.formatSettings(1, 2, 1, 0, 2, 1, 0);
        if (!"spatial=1\nupmix=2\naltscreen=1\nalt_url=0\nalt_layout=2\nrgi=1\nrgi_far=0\n".equals(full))
            throw new AssertionError("formatSettings full: " + full);

        // Cluster / RGI settings defaults
        if (!CarPlaySettingsUi.isAltScreenEnabled()) throw new AssertionError("altscreen default on");
        if (CarPlaySettingsUi.getAltUrlMode() != CarPlaySettingsUi.ALT_URL_MAP_WHEN_RGI)
            throw new AssertionError("alt_url default 0");
        if (CarPlaySettingsUi.getAltLayout() != CarPlaySettingsUi.ALT_LAYOUT_RIGHT)
            throw new AssertionError("alt_layout default right(2)");
        if (!CarPlaySettingsUi.isRgiEnabled()) throw new AssertionError("rgi default on");
        if (CarPlaySettingsUi.isRgiFarStraight()) throw new AssertionError("rgi_far default next maneuver");

        System.out.println("UpmixSettingTest: internal 1-3 hidden, external and unreported shown PASS");
    }
}
