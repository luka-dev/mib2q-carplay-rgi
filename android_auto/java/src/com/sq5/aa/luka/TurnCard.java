/*
 * SQ5 AA port: turn card in the Virtual Cockpit's full/large map view while the Android Auto mirror
 * (displayable 99) is shown.
 *
 * Problem (owner drive 2026-09-28): ctx 81 = {98 arrow, 101/102 backing, 99 mirror}.  luka makes 98 and
 * its backing opaque only while the VC reports its KDK tile visible (FctID 44 supplementaryMapViewVisible)
 * and places them at the stock KDK anchor.  In the VC's large map view the VC shows the whole HU frame
 * and reports KDK visible=false, so 98 stayed at opacity 0: no turn guidance in the cockpit.
 *
 * Rule (pure, host-tested in TurnCardTest), evaluated by ClusterLayerController on every reapply:
 *   not owner / no route                        -> OFF     (luka: 98/101/102 at 0)
 *   no mirror ctx, or turn card disabled         -> STOCK   (luka unchanged)
 *   VC KDK tile visible (Fct44)                  -> STOCK   (luka: stage anchor + crop, opacity 100)
 *   VC map shown (Fct44 lvdsMapVisible) and
 *     large map view (Fct54 largeMapView)        -> CARD    (98 at our card rect, opacity 100)
 *   VC reports no map at all (lvdsMapVisible=0)  -> PREARM  (luka geometry, opacity 100: only a KDK hole
 *                                                           can show these pixels, so a tile that opens
 *                                                           before its Fct44 shows the arrow + backing,
 *                                                           never the mirror behind it)
 *   otherwise (small map view, KDK hidden;
 *     map-view state never reported)             -> STOCK   (opacity follows Fct44 = 0)
 *
 * Card geometry: maneuver_render's full in-tube frame (0,0 328x180 = Layout 122-125 on B9Sport, the
 * renderer's own sport-stage framing) placed at terminal (1064,40): the top-right of the map band
 * (band = 1440x455 at y 26), right of the Audi top bar (owner photos / AltScreen field photo: top bar
 * items end left of x ~970) and above the right dial of the large view (dial top estimated >= y 260 in
 * the field photo), 48 px in from the right edge (bezel curve).  Override on the card with
 * SD:/sq5_turncard_pos containing "x,y" (terminal pixels), read at attach.
 *
 * SD:/sq5_turncard_off disables everything here: the context rule returns to the legacy 81/82 split
 * and the layer rule to luka's (ClusterLayerController), and the mirror is told "no route, no card".
 */
package com.sq5.aa.luka;

import java.io.File;
import java.io.FileInputStream;

public final class TurnCard {
    public static final int MODE_OFF = 0;
    public static final int MODE_STOCK = 1;
    public static final int MODE_CARD = 2;
    public static final int MODE_PREARM = 3;

    /** Card = the stock VC tile's crop of the renderer frame at the stock tile's place (run 108: parked, the VC
     *  shows its own tile at popup (1091,110) crop (59,27 210x153); driving switched to a 328x180 card at
     *  another place that clipped the bezel - now both look and sit the same; the player draws the frame). */
    public static final int CARD_W = 210;
    public static final int CARD_H = 153;
    public static final int CROP_X = 59, CROP_Y = 27;
    public static final int DEFAULT_X = 1091;   /* = stock popup tile (ClusterLayerController fallback geometry) */
    public static final int DEFAULT_Y = 110;
    /** HU cockpit terminal (VC full map view shows all of it). */
    public static final int TERMINAL_W = 1440;
    public static final int TERMINAL_H = 542;

    private static volatile boolean enabled;
    private static volatile int cardX = DEFAULT_X;
    private static volatile int cardY = DEFAULT_Y;

    private TurnCard() {
    }

    /** AaLukaBridge.attach (SD flags read once).  pos = {x, y} or null for the default. */
    public static void configure(boolean on, int[] pos) {
        int x = DEFAULT_X, y = DEFAULT_Y;
        if (pos != null && pos.length == 2 && validPos(pos[0], pos[1])) {
            x = pos[0];
            y = pos[1];
        }
        cardX = x;
        cardY = y;
        enabled = on;
        AaLog.log("layers: turn card " + (on ? "ON" : "OFF (sq5_turncard_off)") + " rect=(" + x + "," + y + " "
            + CARD_W + "x" + CARD_H + ")" + (pos != null && (x != pos[0] || y != pos[1]) ? " (sq5_turncard_pos rejected)" : ""));
    }

    public static boolean isEnabled() { return enabled; }
    public static int cardX() { return cardX; }
    public static int cardY() { return cardY; }

    /** Source crop of the 328x181 renderer frame shown in the card (also the renderer's visible area). */
    public static int[] cardCrop() { return new int[]{CROP_X, CROP_Y, CARD_W, CARD_H}; }

    static boolean validPos(int x, int y) {
        return x >= 0 && y >= 0 && x + CARD_W <= TERMINAL_W && y + CARD_H <= TERMINAL_H;
    }

    /** "x,y" -> {x, y}; null when absent or malformed. */
    public static int[] parsePos(String s) {
        if (s == null) return null;
        s = s.trim();
        int c = s.indexOf(',');
        if (c <= 0) return null;
        try {
            return new int[]{Integer.parseInt(s.substring(0, c).trim()), Integer.parseInt(s.substring(c + 1).trim())};
        } catch (NumberFormatException e) {
            return null;
        }
    }

    /** First readable SD file among paths, trimmed, at most 64 bytes; null if none. */
    public static String readSmallFile(String[] paths) {
        for (int i = 0; i < paths.length; i++) {
            FileInputStream in = null;
            try {
                File f = new File(paths[i]);
                if (!f.exists()) continue;
                in = new FileInputStream(f);
                byte[] b = new byte[64];
                int n = in.read(b);
                if (n > 0) return new String(b, 0, n).trim();
            } catch (Throwable t) {
                /* next path */
            } finally {
                if (in != null) try { in.close(); } catch (Throwable t) { /* ignore */ }
            }
        }
        return null;
    }

    /* Owner 2026-09-29: with the Android Auto cluster map (Google's own turn card) or the full-screen mirror,
     * the arrow box in the large map view repeats the same turn and blocks the map's top-right. Default: no
     * box in the large map view (CARD -> OFF); the HUD and the Sport/classic arrow tiles are unchanged.
     * SD flag sq5_turncard_map_on (and later the cockpit menu) shows it again. */
    private static volatile boolean mapCard;

    public static void setMapCard(boolean on) {
        mapCard = on;
        AaLog.log("layers: turn card in the large map view " + (on ? "shown (sq5_turncard_map_on)" : "hidden"));
    }

    public static boolean isMapCard() { return mapCard; }

    /** The layer rule with the configured switches. */
    public static int planeMode(boolean owner, boolean navActive, boolean mirrorActive, boolean kdkVisible,
                                boolean haveMapView, boolean mapView, boolean largeMap) {
        /* mapCard is applied by ClusterLayerController (full view only, owner 2026-09-29: keep it in Sport). */
        return planeMode(enabled, owner, navActive, mirrorActive, kdkVisible, haveMapView, mapView, largeMap);
    }

    /** The layer rule (pure).  See the class comment for the table. */
    public static int planeMode(boolean on, boolean owner, boolean navActive, boolean mirrorActive, boolean kdkVisible,
                                boolean haveMapView, boolean mapView, boolean largeMap) {
        if (!owner || !navActive) return MODE_OFF;
        if (!on || !mirrorActive) return MODE_STOCK;
        if (kdkVisible) return MODE_STOCK;
        if (haveMapView && mapView && largeMap) return MODE_CARD;
        if (haveMapView && !mapView) return MODE_PREARM;
        return MODE_STOCK;
    }

    public static String modeName(int mode) {
        switch (mode) {
            case MODE_OFF: return "off";
            case MODE_STOCK: return "stock";
            case MODE_CARD: return "card";
            case MODE_PREARM: return "prearm";
            default: return "?" + mode;
        }
    }
}
