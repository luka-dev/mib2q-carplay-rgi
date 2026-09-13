import com.luka.carplay.rgd.*;
import com.luka.carplay.bus.CarplayBus;
import com.luka.carplay.framework.Log;
import java.lang.reflect.Method;
import java.util.Arrays;

/** Real parser -> BAP bridge/stock sender and renderer queue; no HU/network. */
public final class ManeuverParityTest {
    static int checks;
    static void check(boolean ok, String why) { checks++; if (!ok) throw new AssertionError(why); }
    static boolean send(ManeuverChainAudit a, RouteGuidance.State s) throws Exception {
        return ((Boolean)a.sendRenderer.invoke(a.bridge, s, Integer.valueOf(0))).booleanValue();
    }
    static int angle(byte[] packet, int offset) {
        return (short)(((packet[offset]&255)<<8)|(packet[offset+1]&255));
    }
    static byte[] latest(ManeuverChainAudit a) throws Exception {
        Object[] q=(Object[])a.get(a.renderer,"writeQueue");
        int tail=((Integer)a.get(a.renderer,"writeTail")).intValue();
        return (byte[])a.get(q[(tail+q.length-1)%q.length],"packet");
    }
    static byte[] packet(ManeuverChainAudit a, int mt, int angle, int j, int side) throws Exception {
        a.reset(); RouteGuidance.State s=a.state(mt,angle,j,side,0,new int[]{-90,0,90});
        check(send(a,s),"initial packet"); return a.pendingPacket();
    }
    public static void main(String[] args) throws Exception {
        Log.setLevel(-1); ManeuverChainAudit a=new ManeuverChainAudit();
        check(ManeuverMapper.map(4,-1,0,1,true)[1]==64,"real -1 must choose left U-turn in LHT");
        check(ManeuverMapper.map(4,-1,0,1,false)[1]==192,"missing U-turn must use traffic side");
        check(ManeuverMapper.map(8,-1,0,0,true)[1]==32,"real -1 left off-ramp");
        check(ManeuverMapper.map(8,-1,0,0,false)[1]==224,"missing RHT off-ramp");
        check(ManeuverMapper.map(20,-1,0,0,true)[1]==32,"real -1 left end-of-road");
        check(ManeuverMapper.map(20,-1,0,0,false)[1]==64,"missing left end-of-road");
        Method roads=Class.forName("com.luka.carplay.rgd.SideStreets").getDeclaredMethod(
            "calcSideStreetsBytes",Integer.TYPE,Integer.TYPE,Integer.TYPE,int[].class,Integer.TYPE);
        roads.setAccessible(true);
        for(int mt=0;mt<=53;mt++) for(int j=-1;j<=1;j++) for(int side=0;side<2;side++) {
            byte[] pos=(byte[])roads.invoke(null,mt,j,side,new int[]{-90,0,90},1000);
            byte[] neg=(byte[])roads.invoke(null,mt,j,side,new int[]{-90,0,90},-1000);
            check(Arrays.equals(pos,neg),"symmetric missing roads mt="+mt);
            byte[] clean=(byte[])roads.invoke(null,mt,j,side,new int[]{-90,0,90},45);
            byte[] dirty=(byte[])roads.invoke(null,mt,j,side,new int[]{-1000,-90,0,90,1000},45);
            check(Arrays.equals(clean,dirty),"sentinels must not consume road slots mt="+mt);
        }
        byte[] b=packet(a,1,90,0,0);
        check(b[2]==2 && angle(b,4)==-180,"LEFT type wins conflicting raw right angle");
        for(int mt:new int[]{11,16,51}) {
            b=packet(a,mt,-90,0,0);
            check(b[2]==2 && angle(b,4)==-180,"generic signed turn mt="+mt);
        }
        b=packet(a,9,35,0,0);
        check(b[2]==2 && angle(b,4)==90,"ON_RAMP shares slight TURN, not MERGE");
        for (int mt : new int[]{6,7,28,46}) for (int side=0;side<2;side++) {
            b=packet(a,mt,-160,1,side);
            check((b[1]&16)!=0 && b[2]==6 && angle(b,4)==-320 && b[6]==side,"full circle and exact phase exit retained");
            check(b[7]==3 && angle(b,8)==-180 && angle(b,10)==0 && angle(b,12)==180,"full circle uses actual roads");
            for (int missing : new int[]{-1000,1000}) {
                b=packet(a,mt,missing,1,side);
                check((b[1]&16)==0 && b[2]==6 && angle(b,4)==0,"missing circle phase exit does not invent a U-turn");
            }
        }
        for (int mt : new int[]{1,20,47}) {
            b=packet(a,mt,-160,0,0);check(angle(b,4)==-320,"exact left angle keeps overlap lift");
            b=packet(a,mt,160,0,0);check(angle(b,4)<0,"left type rejects right angle");
        }
        for (int mt : new int[]{2,21,48}) {
            b=packet(a,mt,160,0,0);check(angle(b,4)==320,"exact right angle keeps overlap lift");
            b=packet(a,mt,-160,0,0);check(angle(b,4)>0,"right type rejects left angle");
        }
        b=packet(a,1,-105,0,0);check(angle(b,4)==-210,"compatible left angle retains detail");
        b=packet(a,1,0,0,0);check(angle(b,4)==-180,"zero must not erase explicit left turn");
        b=packet(a,3,160,0,0);check(angle(b,4)==0,"explicit straight must remain straight");
        b=packet(a,19,90,1,0); check((b[1]&16)==0 && b[2]==6 && angle(b,4)==360,"typed circle U-turn stays 180 degrees");
        b=packet(a,52,-30,0,0); check(b[2]==8 && b[3]==-1,"left lane-change family");
        b=packet(a,23,135,0,0); check(b[2]==2 && b[3]==0 && angle(b,4)==90,"right single-bend off-ramp");
        int[][] rampRoads={null,new int[0],new int[]{-1000,1000},new int[]{-90,35,90},new int[]{0,35}};
        int[][] rampExpected={new int[]{0},new int[]{0},new int[]{0},new int[]{-180,70,180,0},new int[]{0,70}};
        for(int mt:new int[]{8,22,23}) for(int c=0;c<rampRoads.length;c++) {
            a.reset();RouteGuidance.State ramp=a.state(mt,35,0,0,0,rampRoads[c]);
            check(send(a,ramp),"off-ramp with incomplete road data");b=a.pendingPacket();
            check(b[7]==rampExpected[c].length,"continuing highway retained, zero not duplicated");
            for(int i=0;i<rampExpected[c].length;i++)
                check(angle(b,8+2*i)==rampExpected[c][i],"raw branches plus continuing highway");
            check(angle(b,4)==(mt==22?-90:90),"road correction keeps single-bend ramp");
        }
        for(int mt:new int[]{1,9,13,50}) {
            a.reset();check(send(a,a.state(mt,-45,0,0,0,null)),"non-off-ramp packet");
            check(a.pendingPacket()[7]==0,"do not invent a through-road for ordinary turns/merges");
        }
        int[] crowded=new int[18];for(int i=0;i<crowded.length;i++)crowded[i]=40+i;
        a.reset();check(send(a,a.state(8,30,0,0,0,crowded)),"bounded highway roads");b=a.pendingPacket();
        check(b[7]==18 && angle(b,42)==0,"reserve final wire slot for continuing highway");
        for(int i=0;i<17;i++)check(angle(b,8+2*i)==2*crowded[i],"keep original branch order");
        b=packet(a,1,-90,-1,0); check(b[2]==0 && b[7]==0,"NO_INFO must be empty in renderer");
        b=packet(a,28,-67,1,0);
        check((b[1]&4)!=0 && angle(b,4)==-134 && (b[1]&16)!=0,"raw circle exit retains exact degrees and enables snap");
        for(int dir=0;dir<256;dir+=16) {
            int half=RendererMapper.directionAngleHalfDegrees(dir);
            check(((720-half)%720)/45 == dir/16,"BAP compass bin "+dir);
        }

        for (int mt : new int[]{8,9,22,23}) for (int side=0;side<2;side++)
            for (int raw : new int[]{-1000,-180,-35,-1,0,35,180,1000}) {
                b=packet(a,mt,raw,0,side);
                boolean left=mt==22 || (mt!=23 && ((raw>=-180 && raw<0)
                    || ((raw==0 || raw==-1000 || raw==1000) && side==1)));
                check(b[2]==2 && angle(b,4)==(left?-90:90),"ramp one bend and safe side");
            }
        for (int mt : new int[]{1,5,28}) {
            a.reset();
            RouteGuidance.State raw=a.state(mt,-160,mt==28?1:0,0,0,
                new int[]{-1000,-181,-160,-67,-1,0,43,90,157,181,1000});
            check(send(a,raw),"raw roads packet");b=a.pendingPacket();
            check(b[7]==7,"invalid roads filtered without dropping valid roads");
            int[] expected={-320,-134,-2,0,86,180,314};
            for(int i=0;i<expected.length;i++) check(angle(b,8+i*2)==expected[i],"exact raw side road");
        }
        a.reset(); RouteGuidance.State circle=a.state(28,1000,1,0,0,new int[]{10,90});
        check(send(a,circle),"missing circle angle initial");
        circle.mTurnAngle[0]=0; circle.mExitAngle[0]=0;
        check(send(a,circle),"same numeric fallback but now valid must enable snap");
        check((latest(a)[1]&16)!=0 && (latest(a)[1]&8)==0,"snap policy change is new geometry");

        a.reset(); RouteGuidance.State s=a.state(1,-90,0,0,0,new int[]{-90,0});
        a.set(a.renderer,"running",Boolean.FALSE);
        check(!send(a,s),"disconnected send must fail");
        a.set(a.renderer,"running",Boolean.TRUE);
        check(send(a,s),"identical input must retry after failed send");
        check(!send(a,s),"successful unchanged input should suppress");
        s.mJunctionAngles[0]=new int[]{-90,0,90};
        check(send(a,s),"junction-only change must send");
        check(latest(a)[7]==3,"all original roads encoded, including active road");
        check((latest(a)[1]&8)!=0,"roads-only update must preserve current transition");
        s.mJunctionAngles[0][2]=45;
        check(send(a,s),"in-place source-array edit must send");
        s.mVer[0]++; check(send(a,s),"same shape, new version must send");
        check((latest(a)[1]&8)==0,"new version requires a maneuver transition");
        s.mType[1]=s.mType[0];s.mTurnAngle[1]=s.mTurnAngle[0];s.mExitAngle[1]=s.mExitAngle[0];
        s.mJunctionAngles[1]=s.mJunctionAngles[0];s.mJunctionType[1]=0;s.mDrivingSide[1]=0;
        s.mVer[1]=s.mVer[0];s.maneuverOrder=new int[]{1};
        check(send(a,s),"same shape/version, new primary slot must send");
        for(int bad:new int[]{-1,999}) {
            a.reset();s.maneuverOrder=new int[]{bad,1};s.maneuverCount=2;
            a.sendBap.invoke(a.bridge,s);
            check(a.capture.last==null && !send(a,s),"invalid primary index must not skip to next");
        }
        a.reset();s=a.state(10,0,0,0,0,null);
        check(send(a,s),"arrived initial");s.mVer[0]++;
        check(!send(a,s),"arrived parking updates must not restart animation");
        s.mType[0]=24;check(send(a,s),"arrived side change must send");

        RouteGuidance rg=new RouteGuidance();
        Method parse=RouteGuidance.class.getDeclaredMethod("parse",CarplayBus.Data.class);parse.setAccessible(true);
        String[] deltas={"m0_type:n:4\nm0_junction_type:n:0\nm0_driving_side:n:1\nmaneuver_count:n:1\nmaneuver_list:s:0\nm0_turn_angle:n:-1\n",
            "m0_exit_angle:n:90\n"};
        for(int i=0;i<deltas.length;i++) {
            byte[] text=deltas[i].getBytes("UTF-8");parse.invoke(rg,CarplayBus.parseText(text,text.length));
            s=(RouteGuidance.State)a.get(rg,"state");a.reset();a.sendBap.invoke(a.bridge,s);
            check(send(a,s),"parsed angle update");b=a.pendingPacket();
            check(a.sender.input[0].direction==(i==0?64:192) && b[3]==(i==0?-1:1),"parser aliases agree in BAP and renderer");
        }
        a.reset();check(a.renderer.sendManeuver(2,0,-67,0,new int[]{-1,90},16,2,1),"legacy packet send");
        b=a.pendingPacket();check((b[1]&4)==0 && angle(b,4)==-67 && angle(b,8)==-1,"legacy degrees unchanged");
        a.reset();check(a.renderer.sendBapProgressManeuver(6,0,-135,1,new int[22],16,1,1,false,false,RendererServer.PROGRESS_FILL),"BAP packet send");
        b=a.pendingPacket();check(b[1]==39 && b[7]==18 && b[45]==1 && b[46]==16 && b[47]==1,"BAP flags/count/presentation packing");
        for(int state=0;state<=3;state++) {
            int level=state==3?16:0, mode=state==0?0:1;
            a.reset(); check(a.renderer.sendProgress(level,mode,state),"progress send");
            b=latest(a); check(b.length==48 && b[0]==6 && b[1]==32 && b[2]==level && b[3]==mode && b[4]==state,
                "progress extension packing");
            a.reset(); check(a.renderer.sendBapProgressManeuver(6,0,-135,1,new int[22],level,mode,1,true,true,state),"progress maneuver");
            b=a.pendingPacket(); check((b[1]&32)!=0 && (b[1]&2)!=0 && b[7]==18 && b[44]==state && b[46]==level && b[47]==mode,
                "inline progress overwrote geometry or omitted off");
        }
        a.reset();check(a.renderer.sendProgress(0,1,-1),"legacy empty"); b=latest(a);
        check(b[1]==0 && b[4]==0,"legacy sender must stay unmarked");
        a.reset();RouteGuidance.State blinking=a.state(2,90,0,0,0,new int[]{-90,0,90});
        blinking.distManeuverM=10;
        a.set(a.bridge,"hasLastDistM",Boolean.TRUE);a.set(a.bridge,"lastDistM",Integer.valueOf(10));
        a.set(a.bridge,"lastBarOn",Boolean.TRUE);a.set(a.bridge,"lastBar",Integer.valueOf(0));
        a.set(a.bridge,"lastProgressState",Integer.valueOf(RendererServer.PROGRESS_BLINK_LOW));
        check(((Boolean)a.sendRenderer.invoke(a.bridge,blinking,Integer.valueOf(100))).booleanValue(),"blink snapshot send");
        b=a.pendingPacket();check(b[44]==2 && b[46]==0 && b[47]==1,"maneuver snapshot lost current HUD phase");
        for(int type:new int[]{4,52,53}) {
            a.reset(); RouteGuidance.State ordinary=a.state(type,45,0,0,0,new int[]{-90,0,90});
            check(send(a,ordinary),"ordinary family initial send");
            check(a.pendingPacket()[7]==0,"unused side streets entered renderer packet");
            ordinary.mJunctionAngles[0]=new int[]{-135,45,1000};
            check(!send(a,ordinary),"unused side streets triggered refresh");
            ++ordinary.mVer[0];check(send(a,ordinary),"new identical maneuver lost transition");
        }
        System.out.println("ManeuverParityTest: PASS ("+checks+" checks)");
    }
}
