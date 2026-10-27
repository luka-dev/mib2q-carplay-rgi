package com.luka.carplay.rgd;

/** Owned raw lane event for transport. No recommendation selection, sorting,
 * maneuver-angle matching or road-layout assumptions belong here. */
final class LaneGuidanceSnapshot {
    static final LaneGuidanceSnapshot HIDDEN = new LaneGuidanceSnapshot(-1, false, false, 0);
    final int eventIndex, count;
    final boolean showing;
    boolean complete;
    final int[] positions, primary, status;
    final int[][] angles;
    private LaneGuidanceSnapshot(int event, boolean visible, boolean full, int n) {
        eventIndex=event; showing=visible; complete=full; count=n;
        positions=new int[n]; primary=new int[n]; status=new int[n]; angles=new int[n][];
    }
    private static int value(int[] a, int i, int unknown) {
        return a != null && i < a.length ? a[i] : unknown;
    }
    static LaneGuidanceSnapshot copy(int event, boolean visible, boolean full, int n,
            int[] positions, int[] primary, int[] status, int[][] angles) {
        if (!visible) return HIDDEN;
        int count=Math.max(0,Math.min(n,8));
        LaneGuidanceSnapshot out=new LaneGuidanceSnapshot(event,visible,full && count==n,count);
        for (int i=0;i<count;++i) {
            int p=value(positions,i,-1), a=value(primary,i,1000), s=value(status,i,-1);
            out.positions[i]=p>=0 && p<=65535 ? p : 65535;
            out.primary[i]=a>=-32768 && a<=32767 ? a : 1000;
            out.status[i]=s>=0 && s<=255 ? s : 255;
            int[] raw=angles!=null && i<angles.length ? angles[i] : null;
            int m=raw==null ? 0 : Math.min(raw.length,16);
            if (p<0 || s<0 || raw==null || raw.length>16 || a<-32768 || a>32767) out.complete=false;
            out.angles[i]=new int[m];
            for (int j=0;j<m;++j) {
                int v=raw[j];
                if (v<-32768 || v>32767) {v=1000;out.complete=false;}
                out.angles[i][j]=v;
            }
        }
        return out;
    }
    boolean same(LaneGuidanceSnapshot b) {
        if (b==null || eventIndex!=b.eventIndex || count!=b.count || showing!=b.showing || complete!=b.complete) return false;
        for (int i=0;i<count;++i) {
            if (positions[i]!=b.positions[i] || primary[i]!=b.primary[i] || status[i]!=b.status[i]
                    || angles[i].length!=b.angles[i].length) return false;
            for (int j=0;j<angles[i].length;++j) if (angles[i][j]!=b.angles[i][j]) return false;
        }
        return true;
    }
}
