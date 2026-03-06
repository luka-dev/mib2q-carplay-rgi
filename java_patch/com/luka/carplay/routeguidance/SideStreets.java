/*
 * Side street computation for MHI3-like TBT rendering.
 *
 * Mirrors tbt_renderer logic:
 * - occupied sidestreets are derived from junction element angles + exit angle
 * - angles are mapped to 16-step direction codes (0..240 step 16)
 * - payload is a byte[] of direction codes (Int8Array-style, one byte per entry)
 *
 */
package com.luka.carplay.routeguidance;

final class SideStreets {

    private SideStreets() {}

    /* Exit angle bins used by tbt_renderer (from init tables) */
    private static final float[] EXITS_FULL_17 = new float[]{
        -180.0f, -157.5f, -135.0f, -112.5f, -90.0f, -67.5f, -45.0f, -22.5f,
           0.0f,
          22.5f,  45.0f,  67.5f,  90.0f, 112.5f, 135.0f, 157.5f, 180.0f
    };

    /* Same bins without +/-180 */
    private static final float[] EXITS_15 = new float[]{
        -157.5f, -135.0f, -112.5f, -90.0f, -67.5f, -45.0f, -22.5f,
           0.0f,
          22.5f,  45.0f,  67.5f,  90.0f, 112.5f, 135.0f, 157.5f
    };

    /* Intersection bins (7 entries) */
    private static final float[] EXITS_7 = new float[]{
        135.0f, 90.0f, 45.0f, 0.0f, -45.0f, -90.0f, -135.0f
    };

    /* Sentinel used by MHI3 when exit angle field is missing */
    private static final int EXIT_ANGLE_MISSING = 1000;

    /*
     * Some maneuver types use fixed single-entry side street patterns in MHI3.
     * These are read via sub_2B6C00 from small tables:
     * - 0x5310E0 = 192
     * - 0x5310E8 = 64
     * - 0x5310F0/0x5310F8/0x531100/0x531108 = 0
     */
    private static final int SIDE_FIXED_LEFT = 192;
    private static final int SIDE_FIXED_RIGHT = 64;
    private static final int SIDE_FIXED_ZERO = 0;

    static byte[] calcSideStreetsBytes(int maneuverType, int junctionType, int drivingSide, int[] junctionAngles, int exitAngle) {
        int[] dirs = calcSideStreetDirections(maneuverType, junctionType, drivingSide, junctionAngles, exitAngle);
        return toByteArray(dirs);
    }

    /**
     * Returns direction codes (0..240 step 16) to be packed into sideStreets.
     * If there is nothing to show, returns an empty array.
     */
    static int[] calcSideStreetDirections(int maneuverType, int junctionType, int drivingSide, int[] junctionAngles, int exitAngle) {
        /*
         * Fixed sidestreet patterns (from MHI3 sub_2B9C90).
         */
        if (maneuverType == 7) {
            /* EXIT_ROUNDABOUT: always empty (no sub_2B97D0 call in native) */
            return new int[0];
        }
        if (maneuverType == 6) {
            /* ENTER_ROUNDABOUT: fixed 1-entry table depending on driving side */
            return new int[]{(drivingSide == 1) ? SIDE_FIXED_LEFT : SIDE_FIXED_RIGHT};
        }
        if (maneuverType == 8 || maneuverType == 22 || maneuverType == 23) {
            /* OFF_RAMP / HWY_OFF_RAMP: fixed table containing 0 */
            return new int[]{SIDE_FIXED_ZERO};
        }

        /*
         * MHI3 only computes dynamic sidestreets for specific maneuver types.
         * All other types get empty sidestreets (e.g. ON_RAMP=9, UTurn=4/18/26,
         * ExitRoundaboutTo=24/25, NoTurn=0, Arrive=10/12/27, Ferry=15/17, etc.).
         */
        if (!computesSidestreets(maneuverType, junctionType)) return new int[0];

        if (junctionAngles == null || junctionAngles.length == 0) return new int[0];
        if (exitAngle == EXIT_ANGLE_MISSING) return new int[0];

        /* Unknown junction types: native treats as "no info" and does not compute side streets. */
        if (junctionType != 0 && junctionType != 1) return new int[0];

        if (junctionType == 1) {
            return occupiedSidestreetsRoundabout(drivingSide, junctionAngles, exitAngle);
        }
        return occupiedSidestreetsIntersection(junctionAngles, exitAngle);
    }

    /* ============================
     * Intersection (junctionType=0)
     * ============================ */

    private static int[] occupiedSidestreetsIntersection(int[] junctionAngles, int exitAngle) {
        /*
         * Mirrors tbt_renderer sub_2B97D0:
         * - split junction angles into >exit and <exit buckets (input is expected in descending order)
         * - choose closest 45deg exit bin (EXITS_7)
         * - build two exit lists around that pivot (excluding pivot):
         *   - prefix before pivot (keeps original order) for angles > exit
         *   - reversed suffix after pivot for angles < exit
         * - compare counts to available exit bins (too many => empty)
         * - map each sidestreet to best-matching exits, then sort sidestreets by their best-match diff
         *   and emit unique direction codes in that processing order
         */
        SplitAngles split = splitAnglesNative(junctionAngles, exitAngle);
        int[] below = split.below; /* < exit */
        int[] above = split.above; /* > exit */

        float exitMatch = closestExit((float) exitAngle, EXITS_7);
        int idx = indexOf(EXITS_7, exitMatch);
        if (idx < 0) return new int[0];

        float[] exitsForAbove = slice(EXITS_7, 0, idx);
        float[] exitsForBelow = reverseSlice(EXITS_7, idx + 1, EXITS_7.length);

        if (above.length > exitsForAbove.length || below.length > exitsForBelow.length) {
            return new int[0];
        }

        EntryList entries = new EntryList(16);
        appendEntries(entries, above, exitsForAbove);
        appendEntries(entries, below, exitsForBelow);
        return emitUniqueDirectionCodes(entries);
    }

    /* ============================
     * Roundabout (junctionType=1)
     * ============================ */

    private static int[] occupiedSidestreetsRoundabout(int drivingSide, int[] junctionAngles, int exitAngle) {
        /*
         * Mirrors tbt_renderer sub_2B9030:
         * - choose closest 22.5deg exit bin (from full 17)
         * - split 15-bin list around that exit (excluding the exit itself); for +/-180 uses whole 15-bin list
         * - split junction angles into >exit and <exit, then swap buckets based on driving side
         * - if too many before => empty
         * - if too many after => omit after (keep before)
         * - map each sidestreet to best-matching exits, then sort sidestreets by their best-match diff
         *   and emit unique direction codes in that processing order
         */
        float exitMatch = closestExit((float) exitAngle, EXITS_FULL_17);

        SplitAngles split = splitAnglesNative(junctionAngles, exitAngle);
        int[] below = split.below;
        int[] above = split.above;

        /* Swap "before/after" based on driving side (matches native) */
        int[] sidBefore = (drivingSide == 1) ? below : above;
        int[] sidAfter = (drivingSide == 1) ? above : below;

        float[] exitsBefore;
        float[] exitsAfter;

        if (exitMatch == 180.0f || exitMatch == -180.0f) {
            /*
             * Native special-case: uses the full 15-bin list as a single partition (always "before"),
             * independent of driving side. "After" remains empty.
             */
            exitsBefore = copyFloats(EXITS_15);
            exitsAfter = new float[0];
        } else {
            int idx = indexOf(EXITS_15, exitMatch);
            if (idx < 0) {
                /* Should not happen (exitMatch is 22.5deg quantized); be conservative. */
                exitsBefore = copyFloats(EXITS_15);
                exitsAfter = new float[0];
            } else {
                float[] exitsAbove = reverseSlice(EXITS_15, idx + 1, EXITS_15.length);
                float[] exitsBelow = slice(EXITS_15, 0, idx);
                exitsBefore = (drivingSide == 1) ? exitsBelow : exitsAbove;
                exitsAfter = (drivingSide == 1) ? exitsAbove : exitsBelow;
            }
        }

        if (sidBefore.length > exitsBefore.length) {
            return new int[0];
        }

        EntryList entries = new EntryList(24);
        appendEntries(entries, sidBefore, exitsBefore);

        /* Too many sidestreets after exit -> omit after (native logs + keeps before) */
        if (sidAfter.length <= exitsAfter.length) {
            appendEntries(entries, sidAfter, exitsAfter);
        }

        return emitUniqueDirectionCodes(entries);
    }

    /* ============================
     * Helpers
     * ============================ */

    private static final class Entry {
        final float minDiff;
        final float[] pairs; /* [diff0, exit0, diff1, exit1, ...] */

        Entry(float minDiff, float[] pairs) {
            this.minDiff = minDiff;
            this.pairs = (pairs != null) ? pairs : new float[0];
        }
    }

    private static final class EntryList {
        private Entry[] a;
        private int n;

        EntryList(int cap) {
            if (cap < 0) cap = 0;
            a = new Entry[cap];
            n = 0;
        }

        void add(Entry e) {
            if (e == null) return;
            if (n >= a.length) {
                int newCap = (a.length == 0) ? 8 : (a.length * 2);
                Entry[] na = new Entry[newCap];
                for (int i = 0; i < n; i++) na[i] = a[i];
                a = na;
            }
            a[n++] = e;
        }

        int size() { return n; }

        Entry get(int i) { return a[i]; }

        void set(int i, Entry e) { a[i] = e; }
    }

    private static void appendEntries(EntryList out, int[] sidestreets, float[] exits) {
        if (out == null) return;
        if (sidestreets == null || sidestreets.length == 0) return;
        if (exits == null || exits.length == 0) return;

        int matchCount = sidestreets.length;
        for (int i = 0; i < sidestreets.length; i++) {
            float[] pairs = bestMatchingExitPairs(sidestreets[i], matchCount, exits);
            float minDiff = (pairs.length >= 2) ? pairs[0] : Float.MAX_VALUE;
            out.add(new Entry(minDiff, pairs));
        }
    }

    private static int[] emitUniqueDirectionCodes(EntryList entries) {
        if (entries == null || entries.size() == 0) return new int[0];

        /*
         * Mirrors tbt_renderer sub_2B71C0:
         * - sort mapping entries by their best-match diff (ascending)
         * - then emit unique direction codes in that order
         */
        sortEntriesByMinDiff(entries);

        IntList out = new IntList(16);
        for (int i = 0; i < entries.size(); i++) {
            Entry e = entries.get(i);
            if (e == null || e.pairs == null) continue;
            float[] p = e.pairs;
            for (int j = 1; j < p.length; j += 2) {
                int code = angleToDirectionCode(p[j]);
                if (!out.contains(code)) {
                    out.add(code);
                    break; /* one direction code per side street */
                }
            }
        }
        return out.toArray();
    }

    private static void sortEntriesByMinDiff(EntryList entries) {
        /* Insertion sort (small lists, Java 1.2, stable). */
        for (int i = 1; i < entries.size(); i++) {
            Entry key = entries.get(i);
            float kd = (key != null) ? key.minDiff : Float.MAX_VALUE;
            int j = i - 1;
            while (j >= 0) {
                Entry cur = entries.get(j);
                float cd = (cur != null) ? cur.minDiff : Float.MAX_VALUE;
                if (cd <= kd) break;
                entries.set(j + 1, cur);
                j--;
            }
            entries.set(j + 1, key);
        }
    }

    /*
     * Implements bestMatchingExits (sub_2B31A0):
     * - computes abs diff between angle and each exit
     * - sorts by diff ascending
     * - returns up to topN pairs as [diff0, exit0, diff1, exit1, ...]
     */
    private static float[] bestMatchingExitPairs(int angle, int topN, float[] exits) {
        if (exits == null || exits.length == 0 || topN <= 0) return new float[0];
        if (topN > exits.length) topN = exits.length;

        float target = (float) angle;
        float[] diffs = new float[exits.length];
        int[] idx = new int[exits.length];
        for (int i = 0; i < exits.length; i++) {
            float d = exits[i] - target;
            if (d < 0) d = -d;
            diffs[i] = d;
            idx[i] = i;
        }

        /* Stable insertion sort by diff ascending (matches native tie behavior for int angles). */
        for (int i = 1; i < idx.length; i++) {
            int k = idx[i];
            float kd = diffs[k];
            int j = i - 1;
            while (j >= 0) {
                int pj = idx[j];
                float pd = diffs[pj];
                if (pd <= kd) break;
                idx[j + 1] = pj;
                j--;
            }
            idx[j + 1] = k;
        }

        float[] out = new float[topN * 2];
        for (int i = 0; i < topN; i++) {
            int k = idx[i];
            out[i * 2] = diffs[k];
            out[i * 2 + 1] = exits[k];
        }
        return out;
    }

    private static float closestExit(float angle, float[] exits) {
        if (exits == null || exits.length == 0) return 0.0f;
        float best = exits[0];
        float bestDiff = abs(best - angle);
        for (int i = 1; i < exits.length; i++) {
            float d = abs(exits[i] - angle);
            if (d < bestDiff) {
                bestDiff = d;
                best = exits[i];
            }
        }
        return best;
    }

    private static float abs(float v) {
        return (v < 0.0f) ? -v : v;
    }

    private static int indexOf(float[] a, float v) {
        if (a == null) return -1;
        for (int i = 0; i < a.length; i++) {
            if (a[i] == v) return i;
        }
        return -1;
    }

    private static float[] slice(float[] a, int from, int to) {
        if (a == null) return new float[0];
        if (from < 0) from = 0;
        if (to > a.length) to = a.length;
        if (to <= from) return new float[0];
        float[] out = new float[to - from];
        for (int i = 0; i < out.length; i++) out[i] = a[from + i];
        return out;
    }

    private static float[] reverseSlice(float[] a, int from, int to) {
        float[] s = slice(a, from, to);
        for (int i = 0; i < s.length / 2; i++) {
            float t = s[i];
            s[i] = s[s.length - 1 - i];
            s[s.length - 1 - i] = t;
        }
        return s;
    }

    private static float[] copyFloats(float[] a) {
        if (a == null) return new float[0];
        float[] out = new float[a.length];
        for (int i = 0; i < a.length; i++) out[i] = a[i];
        return out;
    }

    private static final class SplitAngles {
        final int[] below;
        final int[] above;
        SplitAngles(int[] below, int[] above) {
            this.below = (below != null) ? below : new int[0];
            this.above = (above != null) ? above : new int[0];
        }
    }

    /*
     * Mirrors tbt_renderer sub_2B7510:
     * - partitions angles into two vectors:
     *   - "above": values > exitAngle
     *   - "below": values < exitAngle
     * - values == exitAngle are ignored
     *
     * Native does not require ordering; preserve input order within each partition.
     */
    private static SplitAngles splitAnglesNative(int[] angles, int exitAngle) {
        if (angles == null || angles.length == 0) return new SplitAngles(new int[0], new int[0]);

        IntList below = new IntList(angles.length);
        IntList above = new IntList(angles.length);
        for (int i = 0; i < angles.length; i++) {
            int a = angles[i];
            if (a > exitAngle) above.add(a);
            else if (a < exitAngle) below.add(a);
        }
        return new SplitAngles(below.toArray(), above.toArray());
    }

    /*
     * MHI3 tbt_renderer uses a std::map<float,int> populated from a table:
     * angle->code with 22.5deg steps. This is the equivalent formula.
     */
    private static int angleToDirectionCode(float angle) {
        /* Quantize to 22.5deg steps (inputs are already exact bins in native). */
        int step = (int) (angle / 22.5f);
        if (angle < 0.0f && ((float) step) * 22.5f != angle) step -= 1; /* floor for negatives */

        /* Clamp to [-8..8] */
        if (step < -8) step = -8;
        if (step > 8) step = 8;

        int code;
        if (step <= 0) {
            code = (-step) * 16;
        } else {
            code = (16 - step) * 16;
        }
        return code;
    }

    /*
     * MHI3 sub_2B9C90 whitelist: only these maneuver types trigger dynamic
     * sidestreet computation via occupiedSidestreetsIntersection / Roundabout.
     *
     * - Types 11, 16, 19, 51 bypass the junctionType gate and always compute.
     * - Types 1-3, 13-14, 20-21, 47-50 compute only with junctionType=0 (intersection).
     * - Types 28-46 compute only with junctionType=1 (roundabout exits).
     * Everything else gets empty sidestreets.
     */
    private static boolean computesSidestreets(int maneuverType, int junctionType) {
        switch (maneuverType) {
            case 11: case 16: case 19: case 51:
                return true;
            case 1: case 2: case 3: case 13: case 14:
            case 20: case 21: case 47: case 48: case 49: case 50:
                return junctionType == 0;
            default:
                return maneuverType >= 28 && maneuverType <= 46 && junctionType == 1;
        }
    }

    private static byte[] toByteArray(int[] v) {
        if (v == null || v.length == 0) return new byte[0];
        byte[] out = new byte[v.length];
        for (int i = 0; i < v.length; i++) out[i] = (byte) (v[i] & 0xFF);
        return out;
    }

    private static final class IntList {
        private int[] a;
        private int n;

        IntList(int cap) {
            if (cap < 0) cap = 0;
            a = new int[cap];
            n = 0;
        }

        void add(int v) {
            if (n >= a.length) {
                int newCap = (a.length == 0) ? 4 : (a.length * 2);
                int[] na = new int[newCap];
                for (int i = 0; i < n; i++) na[i] = a[i];
                a = na;
            }
            a[n++] = v;
        }

        boolean contains(int v) {
            for (int i = 0; i < n; i++) if (a[i] == v) return true;
            return false;
        }

        int[] toArray() {
            if (n == 0) return new int[0];
            int[] out = new int[n];
            for (int i = 0; i < n; i++) out[i] = a[i];
            return out;
        }
    }
}
