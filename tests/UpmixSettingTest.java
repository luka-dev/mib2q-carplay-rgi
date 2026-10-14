package com.luka.carplay.core;

/** UpmixSetting.internalAmp: the UpMix row hides exactly on the head unit's own amplifier. */
public final class UpmixSettingTest {
    public static void main(String[] args) {
        // org.dsi.ifc.audio DEVICETYPE_*: BASIC_2CH 1, BASIC_4CH 2, STANDARD_6CH 3 = internal
        for (int t = 1; t <= 3; t++)
            if (!UpmixSetting.internalAmp(t)) throw new AssertionError("internal type " + t);
        // unknown (-1), audio management (0), ALPINE_BO_AUDI 4, PREMIUM_BOSE_AUDI 17, ADVANCED_BO_AUDI 21 ...
        int[] external = {-1, 0, 4, 5, 17, 18, 19, 20, 21, 22, 23};
        for (int i = 0; i < external.length; i++)
            if (UpmixSetting.internalAmp(external[i])) throw new AssertionError("external type " + external[i]);
        System.out.println("UpmixSettingTest: internal 1-3 hidden, external and unreported shown PASS");
    }
}
