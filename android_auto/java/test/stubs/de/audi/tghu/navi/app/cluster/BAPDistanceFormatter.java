package de.audi.tghu.navi.app.cluster;

import de.audi.atip.log.LogChannel;

/** Host-test stub (see test/stubs/README.txt): exact metres, unit 0. */
public class BAPDistanceFormatter {
    public static class BAPDistance {
        private final int value, unit;
        BAPDistance(int value, int unit) { this.value = value; this.unit = unit; }
        public int getValue() { return value; }
        public int getUnit() { return unit; }
    }
    public BAPDistanceFormatter(LogChannel lc) { }
    public BAPDistance formatDistanceToTurn(int m, boolean metric) { return m > 0 ? new BAPDistance(m * 10, 0) : new BAPDistance(-1, 255); }
    public BAPDistance formatDistanceToDestination(int m, boolean metric) { return formatDistanceToTurn(m, metric); }
}
