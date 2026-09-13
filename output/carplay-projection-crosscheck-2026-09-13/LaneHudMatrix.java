import com.luka.carplay.rgd.*;
import com.luka.carplay.framework.Log;
import de.audi.app.combi.bap.app.navi.AppConnectorNavi;
import de.audi.atip.interapp.combi.bap.navi.data.CombiBAPNaviLaneGuidanceData;
import java.lang.reflect.*;
import java.io.*;
import java.util.Arrays;
import sun.misc.Unsafe;

/** Compare complete, valid lane records across the previous/current HUD sender. */
public class LaneHudMatrix {
    public static class HUD extends AppConnectorNavi {
        boolean showing;CombiBAPNaviLaneGuidanceData[] lanes;
        HUD(){super(null);}
        public void updateLaneGuidance(boolean show,CombiBAPNaviLaneGuidanceData[] data){showing=show;lanes=data;}
    }
    public static void main(String[] args)throws Exception {
        Log.setLevel(-1);
        Field f=Unsafe.class.getDeclaredField("theUnsafe");f.setAccessible(true);
        HUD hud=(HUD)((Unsafe)f.get(null)).allocateInstance(HUD.class);
        BAPBridge bridge=new BAPBridge();f=BAPBridge.class.getDeclaredField("appConnectorNavi");f.setAccessible(true);f.set(bridge,hud);
        Method send=BAPBridge.class.getDeclaredMethod("sendLaneGuidance",RouteGuidance.State.class);send.setAccessible(true);
        RouteGuidance.State s=new RouteGuidance.State();
        s.routeState=1;s.maneuverCount=1;s.maneuverOrder=new int[]{0};s.mType[0]=2;
        s.laneGuidanceShowing=1;s.laneGuidanceIndex=73;s.laneGuidanceSlot=3;
        s.lgIndex[3]=73;s.lgLaneCount[3]=1;s.lgLanePositions[3]=new int[]{0};
        PrintWriter out=new PrintWriter(new FileOutputStream(args[0]));
        for(int angle=-180;angle<=180;angle++)for(int status=0;status<=2;status++) {
            s.lgLaneDirections[3]=new int[]{angle};s.lgLaneStatus[3]=new int[]{status};
            s.lgLaneAngles[3]=new int[][]{{angle,0,45,90}};send.invoke(bridge,s);
            if(!hud.showing || hud.lanes.length!=1)throw new AssertionError("valid lane not emitted");
            CombiBAPNaviLaneGuidanceData d=hud.lanes[0];
            out.println(angle+"\t"+status+"\t"+d.laneDirection+"\t"+d.guidanceInfo+"\t"+Arrays.toString(d.laneSideStreets));
        }
        out.close();
    }
}
