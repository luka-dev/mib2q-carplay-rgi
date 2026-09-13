package com.luka.carplay.rgd;
import java.io.FileOutputStream;
import java.lang.reflect.Method;
public class LaneGuidanceTransportTest {
    static void check(boolean b,String message) {if(!b)throw new AssertionError(message);}
    static int shortAt(byte[] p,int i) {return (short)(((p[i]&255)<<8)|(p[i+1]&255));}
    public static void main(String[] args) throws Exception {
        RouteGuidance.State state=new RouteGuidance.State();
        state.laneGuidanceShowing=1;state.laneGuidanceIndex=73;state.laneGuidanceSlot=3;
        state.lgIndex[3]=73;state.lgLaneComplete[3]=0;state.lgLaneCount[3]=8;
        state.lgLanePositions[3]=new int[8];state.lgLaneDirections[3]=new int[8];
        state.lgLaneStatus[3]=new int[8];state.lgLaneAngles[3]=new int[8][];
        for(int i=0;i<8;++i) {
            state.lgLanePositions[3][i]=i;state.lgLaneStatus[3][i]=i%3;
            state.lgLaneDirections[3][i]=i%2==0?1000:-1000;
            state.lgLaneAngles[3][i]=new int[16];
            for(int j=0;j<16;++j)state.lgLaneAngles[3][i][j]=j%2==0?1000:-1000;
        }
        Method source=BAPBridge.class.getDeclaredMethod("rendererLaneGuidance",RouteGuidance.State.class,Boolean.TYPE);
        source.setAccessible(true);
        LaneGuidanceSnapshot a=(LaneGuidanceSnapshot)source.invoke(null,state,Boolean.TRUE);
        check(a.count==8 && a.showing && !a.complete && a.eventIndex==73,"incomplete raw event discarded");
        // Maneuver/link/direction changes must not choose another lane event.
        state.mType[0]=ManeuverMapper.MT_ROUNDABOUT_EXIT_1;state.mTurnAngle[0]=-180;
        state.mLinkedLaneGuidanceIndex[0]=99;
        check(a.same((LaneGuidanceSnapshot)source.invoke(null,state,Boolean.TRUE)),"maneuver changed independent guidance");
        state.lgLaneAngles[3][0][0]=90;
        check(a.angles[0][0]==1000,"snapshot aliases mutable input");
        byte[] wire=RendererServer.lanePacket(a,0x12345678);
        check(wire.length==480 && wire[0]==12 && wire[432]==14,"independent batch framing");
        for(int i=0;i<8;++i) {
            int p=(i+1)*48;check(wire[p]==13 && wire[p+6]==i,"record order");
            check(shortAt(wire,p+11)==(i%2==0?1000:-1000),"primary sentinel changed");
            for(int j=0;j<16;++j)check(shortAt(wire,p+13+j*2)==(j%2==0?1000:-1000),"angle changed");
        }
        LaneGuidanceSnapshot hidden=(LaneGuidanceSnapshot)source.invoke(null,state,Boolean.FALSE);
        check(!hidden.showing && hidden.count==0,"hide not transported");
        check(RendererServer.lanePacket(hidden,2).length==96,"hide needs empty atomic snapshot");
        if(args.length>0) {FileOutputStream out=new FileOutputStream(args[0]);out.write(wire);out.close();}
        System.out.println("Lane guidance transport: HUD active-event selection, maneuver independence, raw sentinels, incomplete metadata, ownership and hide PASS");
    }
}
