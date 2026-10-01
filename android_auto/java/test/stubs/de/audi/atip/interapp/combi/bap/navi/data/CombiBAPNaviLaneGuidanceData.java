package de.audi.atip.interapp.combi.bap.navi.data;

/** Host-test stub (the J9 original recurses in <init> on HotSpot): same public fields/constructors as MU0918. */
public final class CombiBAPNaviLaneGuidanceData {
    public short posID;
    public short laneDirection;
    public byte[] laneSideStreets;
    public short laneType;
    public byte laneMarkingLeft;
    public byte laneMarkingRight;
    public byte laneDescription;
    public byte guidanceInfo;
    public CombiBAPNaviLaneGuidanceData() { }
    public CombiBAPNaviLaneGuidanceData(short pos, short dir, byte[] side, short type, byte ml, byte mr, byte desc, byte gi) {
        posID = pos; laneDirection = dir; laneSideStreets = side; laneType = type;
        laneMarkingLeft = ml; laneMarkingRight = mr; laneDescription = desc; guidanceInfo = gi;
    }
    public int getPosID() { return posID; }
    public short getLaneDirection() { return laneDirection; }
    public byte[] getLaneSideStreets() { return laneSideStreets; }
    public short getLaneType() { return laneType; }
    public byte getLaneMarkingLeft() { return laneMarkingLeft; }
    public byte getLaneMarkingRight() { return laneMarkingRight; }
    public byte getLaneDescription() { return laneDescription; }
    public byte getGuidanceInfo() { return guidanceInfo; }
}
