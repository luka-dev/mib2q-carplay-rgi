/*
 * VehicleLayoutProfile - per-car CarPlay safe areas, chosen from the stock Layout class.
 *
 * The stock HMI picks its cluster Layout by car (HMITerminalImplMIB2High.generateLayoutClassic /
 * generateLayoutSport): LayoutMIB2HighB9(Sport) for A4/A5/Q5/Q2, LayoutMIB2HighQ7(Sport) for the
 * Q7, others for A3/R8/TT.  The hook's built-in safe areas were measured on an A5 (B9), so a B9
 * gets no profile.  For the Q7 this writes the three Q7 rectangles to PROFILE_FILE, which the
 * hook reads when it builds the cluster display for the phone (hook/altscreen/cluster_layout.h,
 * cl_layout_build): built-in < this profile < /mnt/app/carplay_layout.cfg, and that file's
 * "car_profile off" switches the profile off for an A/B comparison.
 *
 * The Q7 values are the ones used on a Q7 4M in the aberbic issue #5 review (a commercial
 * A4/A5/Q5/Q7 package ships the same three, so they are one source, not a Q7 measurement of
 * ours).  They are a starting point until our Q7 confirms them: docs/cluster-and-rgi/
 * VC_LAYOUT_AND_FONTS.md#layout-options.
 *
 * Java 1.4 source level.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
package com.luka.carplay.cluster;

import com.luka.carplay.framework.Log;

import java.io.File;
import java.io.FileOutputStream;

public final class VehicleLayoutProfile {
    private static final String TAG = "ClusterLayout";
    public static final String PROFILE_FILE = "/tmp/carplay_layout.vehicle";

    /* full, sport, classic: x y w h on the 1440x542 canvas. */
    static final String Q7_PROFILE =
        "# written by VehicleLayoutProfile for the Q7 Layout classes\n"
        + "full    360 87 720 297\n"
        + "sport   114 88 350 296\n"
        + "classic 520 87 400 297\n";

    private static String path = PROFILE_FILE;
    private static String applied;   /* profile name last written, "" = none */

    private VehicleLayoutProfile() {}

    /** "q7" for the Q7 Layout classes, "" for every other car (built-in areas). */
    static String profileFor(String layoutClass) {
        if (layoutClass == null) return "";
        int dot = layoutClass.lastIndexOf('.');
        String simple = dot >= 0 ? layoutClass.substring(dot + 1) : layoutClass;
        if (simple.startsWith("LayoutMIB2HighQ7")) return "q7";
        return "";
    }

    /** Called with the live stock Layout class name; writes or removes the profile file on a
     *  change.  Never throws. */
    public static void apply(String layoutClass) {
        String profile = profileFor(layoutClass);
        synchronized (VehicleLayoutProfile.class) {
            if (profile.equals(applied)) return;
            try {
                File f = new File(path);
                if (profile.length() == 0) {
                    if (f.exists() && !f.delete()) {
                        Log.w(TAG, "car profile: cannot remove " + path);
                        return;
                    }
                } else {
                    write(f, "# layout " + layoutClass + "\n" + Q7_PROFILE);
                }
                applied = profile;
                Log.w(TAG, "car profile=" + (profile.length() == 0 ? "none (built-in A5 areas)" : profile)
                    + " layout=" + layoutClass + " file=" + path);
            } catch (Throwable t) {
                Log.w(TAG, "car profile write failed: " + t);
            }
        }
    }

    private static void write(File f, String text) throws Exception {
        File tmp = new File(f.getPath() + ".new");
        FileOutputStream out = new FileOutputStream(tmp);
        try {
            out.write(text.getBytes("US-ASCII"));
        } finally {
            out.close();
        }
        if (!tmp.renameTo(f)) {
            /* rename over an existing file can fail on some filesystems: replace in place */
            f.delete();
            if (!tmp.renameTo(f)) {
                tmp.delete();
                out = new FileOutputStream(f);
                try { out.write(text.getBytes("US-ASCII")); } finally { out.close(); }
            }
        }
    }

    /** Host tests only. */
    static synchronized void resetForTest(String p) {
        path = p == null ? PROFILE_FILE : p;
        applied = null;
    }
}
