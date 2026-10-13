/*
 * de.audi.tghu.storage.FlushValuesDict, shared and extensible.
 *
 * Stock: StorageAccess / DSIStorageProvider flush the persistence database right
 * after a write only for the (namespace, key) pairs listed here; any other key is
 * written but reaches flash only at a later general flush.  Mods used to replace
 * this class with their own list, and with several jars on j9's bootclasspath only
 * the first copy counts (luka-collection.jar loads before carplay_hook.jar).
 *
 * This version keeps the stock lists byte for byte (US/EU/CN are identical) and adds
 * register()/registerSetup(): every mod ships this SAME class and registers its own
 * keys at start, so whichever copy loads first serves all of them.  Copy-on-write
 * arrays: the stock lookups read one array reference, never a half-grown list.
 *
 * Shared with luka-collection (java_patch/de/audi/tghu/storage/FlushValuesDict.java
 * there) - keep both copies identical.
 *
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */
package de.audi.tghu.storage;

public final class FlushValuesDict {
    public static int[][] values = new int[][]{
        {1101, 10, 0},
        {1002, 200, 0},
        {1002, 19, 0},
        {261, 20, 0},
        {1018, 20, 0},
        {1018, 40, 0},
        {259, 2, 0},
        {262, 10, 0},
        {1004, 30, 0},
        {1004, 10, 0},
        {1004, 290, 0},
        {1004, 860, 0},
        {1004, 410, 0},
        {1004, 420, 0},
        {1004, 510, 0},
        {1004, 880, 0},
        {1004, 800, 0},
        {1004, 890, 0},
        {257, 5003, 0},
        {257, 1, 0},
        {257, 2, 0},
        {257, 4, 0},
        {257, 5006, 0},
        {257, 6, 0},
        {257, 5007, 0},
        {257, 5001, 0},
        {257, 5002, 0},
        {1011, 1, 0},
        {1011, 2, 0},
        {1011, 3, 0},
        {1011, 4, 0},
        {1011, 5, 0},
        {1011, 6, 0},
        {1011, 7, 0},
        {1011, 8, 0}
    };
    public static int[][] valuesSetup = new int[][]{
        {1002, 110, 0},
        {1002, 120, 0},
        {1002, 220, 0},
        {1002, 29, 0},
        {1002, 30, 0},
        {1002, 40, 0},
        {1002, 50, 0},
        {1002, 60, 0},
        {1002, 70, 0},
        {1002, 80, 0},
        {1002, 90, 0},
        {1002, 100, 0},
        {1002, 130, 0},
        {1002, 140, 0},
        {1002, 150, 0},
        {1002, 160, 0},
        {1002, 170, 0},
        {1004, 100, 0},
        {1004, 110, 0},
        {1004, 120, 0},
        {1004, 130, 0},
        {1004, 140, 0},
        {1004, 150, 0},
        {1004, 850, 0},
        {1004, 840, 0},
        {1004, 900, 0},
        {1004, 200, 0},
        {1004, 210, 0},
        {1004, 220, 0},
        {1004, 230, 0},
        {1004, 240, 0},
        {1004, 250, 0},
        {1004, 260, 0},
        {1004, 270, 0},
        {1004, 280, 0},
        {1004, 300, 0},
        {1004, 310, 0},
        {1004, 320, 0},
        {1004, 330, 0},
        {1004, 360, 0},
        {1004, 370, 0},
        {1004, 380, 0},
        {1004, 390, 0},
        {1004, 400, 0},
        {1004, 430, 0},
        {1004, 440, 0}
    };

    /** Flush right after a write of (namespace, key). Idempotent; any thread. */
    public static synchronized void register(int namespace, int key) {
        values = add(values, namespace, key);
    }

    /** The same for the setup-screen dirty flag (StorageAccess.setDirtyFlag). */
    public static synchronized void registerSetup(int namespace, int key) {
        valuesSetup = add(valuesSetup, namespace, key);
    }

    private static int[][] add(int[][] list, int namespace, int key) {
        for (int k = 0; k < list.length; k++)
            if (list[k][0] == namespace && list[k][1] == key) return list;
        int[][] grown = new int[list.length + 1][];
        System.arraycopy(list, 0, grown, 0, list.length);
        grown[list.length] = new int[]{namespace, key, 0};
        return grown;
    }

    public static boolean isFlushRequired(int i, int j) {
        int[][] list = values;
        for (int k = 0; k < list.length; k++) {
            if (i == list[k][0] && j == list[k][1]) {
                list[k][2]++;
                return true;
            }
        }
        return false;
    }

    public static boolean isSetupScreenFlushRequired(int i, int j) {
        int[][] list = valuesSetup;
        for (int k = 0; k < list.length; k++) {
            if (i == list[k][0] && j == list[k][1]) {
                list[k][2]++;
                return true;
            }
        }
        return false;
    }
}
