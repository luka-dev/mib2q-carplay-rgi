package de.audi.atip.interapp.combi.bap.navi.data;

/** Host-test stub (see test/stubs/README.txt): same public API as MU0918. */
public final class CombiBAPNaviManeuverDescriptor {
    public int mainElement;
    public int direction;
    public int zLevelGuidance;
    public byte[] sideStreets;
    public CombiBAPNaviManeuverDescriptor() { }
    public CombiBAPNaviManeuverDescriptor(int m, int d, int z, byte[] s) { mainElement = m; direction = d; zLevelGuidance = z; sideStreets = s; }
    public int getMainElement() { return mainElement; }
    public int getDirection() { return direction; }
    public int getZLevelGuidance() { return zLevelGuidance; }
    public byte[] getSideStreets() { return sideStreets; }
}
