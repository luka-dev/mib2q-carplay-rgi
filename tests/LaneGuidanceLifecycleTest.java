package com.luka.carplay.rgd;

import com.luka.carplay.bus.CarplayBus;
import com.luka.carplay.framework.Log;
import de.audi.app.combi.bap.app.navi.AppConnectorNavi;
import de.audi.atip.interapp.combi.bap.navi.data.CombiBAPNaviLaneGuidanceData;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import sun.misc.Unsafe;

/** Real bus-delta parser and BAPBridge.update; captures only the two output
 * boundaries, without starting native HU services or changing maneuvers. */
public final class LaneGuidanceLifecycleTest {
    public static final class HUD extends AppConnectorNavi {
        boolean showing; int writes;
        CombiBAPNaviLaneGuidanceData[] lanes;
        HUD() { super(null); } // bypass the native framework constructor
        public void updateLaneGuidance(boolean enabled, CombiBAPNaviLaneGuidanceData[] data) {
            showing=enabled; lanes=data; writes++;
        }
    }
    public static final class Renderer extends RendererServer {
        LaneGuidanceSnapshot lanes; int writes;
        boolean sendLaneGuidance(LaneGuidanceSnapshot value) { lanes=value; writes++; return true; }
    }
    static void check(boolean b,String message) { if(!b)throw new AssertionError(message); }
    static void set(Object o,String name,Object value) throws Exception {
        Field f=o.getClass().getDeclaredField(name);f.setAccessible(true);f.set(o,value);
    }
    static int code(int raw) {
        int bucket=raw>=0?(raw+22)/45:-((-raw+22)/45);
        return new int[]{0x72,0x60,0x40,0x20,0,0xe0,0xc0,0xa0,0x92}[bucket+4];
    }
    public static void main(String[] args) throws Exception {
        Log.setLevel(-1);
        Field uf=Unsafe.class.getDeclaredField("theUnsafe");uf.setAccessible(true);
        HUD hud=(HUD)((Unsafe)uf.get(null)).allocateInstance(HUD.class);
        Renderer renderer=new Renderer();BAPBridge bridge=new BAPBridge();
        set(bridge,"initialized",Boolean.TRUE);set(bridge,"appConnectorNavi",hud);
        set(bridge,"routeTextPublished",Boolean.TRUE);
        set(bridge,"customRendererStarted",Boolean.TRUE);set(bridge,"rendererClient",renderer);
        RouteGuidance rg=new RouteGuidance();
        Field sf=RouteGuidance.class.getDeclaredField("state");sf.setAccessible(true);
        RouteGuidance.State s=(RouteGuidance.State)sf.get(rg);
        Method parse=RouteGuidance.class.getDeclaredMethod("parse",CarplayBus.Data.class);parse.setAccessible(true);
        s.routeState=1;s.maneuverCount=1;s.maneuverOrder=new int[]{0};s.mType[0]=4;
        String[] deltas={
            "lane_guidance_showing:n:1\nlane_guidance_index:n:73\nlane_guidance_slot:n:-1\n",
            "lg3_index:n:73\nlg3_lane_count:n:2\nlg3_lane_positions:s:0,1\nlg3_lane_directions:s:0,45\nlg3_lane_status:s:0,2\nlg3_lane_angles:s:0|0,45\n",
            "lane_guidance_showing:n:0\n",
            "lane_guidance_showing:n:1\n",
            "lg3_lane_status:s:2,0\n",
            "lg4_index:n:99\nlg4_lane_count:n:1\nlg4_lane_directions:s:-90\nlg4_lane_status:s:2\nlg4_lane_angles:s:-90\n",
            "lane_guidance_index:n:99\nlane_guidance_slot:n:4\n",
            "lg4_lane_count:n:0\n",
            "lane_guidance_index:n:73\nlane_guidance_slot:n:3\n",
            "lg3_index:n:101\n",
            "lane_guidance_showing:n:0\n",
            "lane_guidance_showing:n:1\nlane_guidance_index:n:101\n",
            "lg3_lane_count:n:1\nlg3_lane_directions:s:-45\nlg3_lane_status:s:2\nlg3_lane_angles:s:-45\n"
        };
        boolean[] shown={false,true,false,true,true,true,true,false,true,false,false,false,true};
        // Legacy mirror deliberately contains stale data; modern missing/empty
        // events must never fall back to this maneuver's lanes.
        s.mLaneCount[0]=1;s.mLaneDirections[0]=new int[]{90};
        for(int i=0;i<deltas.length;i++) {
            s.dirtyMask=0;byte[] text=deltas[i].getBytes("UTF-8");
            parse.invoke(rg,CarplayBus.parseText(text,text.length));
            check((s.dirtyMask & RouteGuidance.State.DIRTY_LANE_GUIDANCE)!=0,"lane delta not dirty "+i);
            check((s.dirtyMask & RouteGuidance.State.DIRTY_MANEUVER_ICON)==0,"lane delta dirtied maneuver "+i);
            int writes=renderer.writes;
            check(bridge.update(s),"bridge rejected lane-only delta "+i);
            check(hud.showing==shown[i],"HUD stale/missing guidance "+i);
            boolean visible=renderer.lanes.showing && renderer.lanes.count>0;
            check(visible==shown[i],"renderer visibility differs from HUD "+i);
            check(s.mType[0]==4 && s.maneuverOrder[0]==0,"lane delta changed maneuver "+i);
            if(i==5)check(renderer.writes==writes,"future cache event replaced active lanes");
            if(i==4)check(hud.lanes[0].guidanceInfo==2 && renderer.lanes.status[0]==2,"recommendation delta lost");
            if(i==9)check(s.lgLaneDirections[3]==null && s.lgLaneCount[3]==-1,"LRU remap retained old lane content");
        }
        // Route-only changes must reevaluate the same visibility predicate.
        s.maneuverCount=0;s.routeState=0;s.dirtyMask=RouteGuidance.State.DIRTY_ROUTE_STATE;
        check(bridge.update(s) && !hud.showing && !renderer.lanes.showing,"route-only clear missed lanes");
        s.routeState=1;s.dirtyMask=RouteGuidance.State.DIRTY_ROUTE_STATE;
        check(bridge.update(s) && hud.showing && renderer.lanes.showing,"lanes require a maneuver count");

        Method send=BAPBridge.class.getDeclaredMethod("sendLaneGuidance",RouteGuidance.State.class);send.setAccessible(true);
        for(int raw=-180;raw<=180;raw++)for(int status=0;status<=2;status++) {
            s.lgLaneDirections[3]=new int[]{raw};s.lgLaneStatus[3]=new int[]{status};
            s.lgLaneAngles[3]=new int[][]{{raw}};send.invoke(bridge,s);
            check(hud.showing && hud.lanes.length==1 && hud.lanes[0].laneDirection==code(raw),"HUD/renderer quantization "+raw);
            check(hud.lanes[0].guidanceInfo==status,"status mapping "+status);
        }
        for(int raw:new int[]{1000,-1000,181,-181,32767,-32768}) {
            s.lgLaneDirections[3]=new int[]{raw};s.lgLaneStatus[3]=new int[]{2};
            s.lgLaneAngles[3]=new int[][]{{raw}};send.invoke(bridge,s);
            check(!hud.showing,"invalid angle became HUD turn "+raw);
            s.lgLaneAngles[3]=new int[][]{{raw,45,0}};send.invoke(bridge,s);
            check(hud.showing && hud.lanes[0].laneDirection==0xe0 && hud.lanes[0].guidanceInfo==0,
                "unknown primary fabricated a highlighted branch "+raw);
            check(hud.lanes[0].laneSideStreets.length==1 && hud.lanes[0].laneSideStreets[0]==0,
                "valid alternative branch lost "+raw);
        }
        s.lgLaneDirections[3]=null;s.lgLaneAngles[3]=null;send.invoke(bridge,s);
        check(!hud.showing,"count-only event enabled an empty HUD panel");
        s.lgLaneAngles[3]=new int[][]{{45}};send.invoke(bridge,s);
        check(hud.showing && hud.lanes[0].laneDirection==0xe0 && hud.lanes[0].guidanceInfo==0,
            "missing primary lost a valid grey fallback branch");
        s.lgLaneCount[3]=2;s.lgLaneDirections[3]=new int[]{45,1000};
        s.lgLaneStatus[3]=new int[]{2,2};send.invoke(bridge,s);
        check(hud.lanes.length==1,"missing lane angles borrowed the first lane's direction");
        System.out.println("Lane lifecycle: real delta -> HUD/renderer, show/hide/reappear within one maneuver, late cache, prefetch, event switch, zero count, LRU reset, route-only gate, 1083 angle/status cases and safety sentinels PASS");
    }
}
