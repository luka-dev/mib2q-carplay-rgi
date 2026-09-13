import com.luka.carplay.rgd.*;
import com.luka.carplay.bus.CarplayBus;
import com.luka.carplay.framework.Log;
import java.lang.reflect.*;
import java.io.*;
import java.util.Arrays;
import sun.misc.Unsafe;
import de.audi.app.combi.bap.app.navi.AppConnectorNavi;
import de.audi.atip.interapp.combi.bap.navi.data.CombiBAPNaviLaneGuidanceData;

/** Observation probe for the audited revision, not a regression acceptance test. */
public class JavaRgiAuditProbe {
    static Field field(Object o, String name) throws Exception {
        for (Class c=o.getClass();c!=null;c=c.getSuperclass()) {
            try { Field f=c.getDeclaredField(name);f.setAccessible(true);return f; }
            catch(NoSuchFieldException e) { }
        }
        throw new NoSuchFieldException(name);
    }
    static Object get(Object o,String n) throws Exception { return field(o,n).get(o); }
    static void set(Object o,String n,Object v) throws Exception { field(o,n).set(o,v); }
    static Method method(Class c,String n,Class... types) throws Exception {
        Method m=c.getDeclaredMethod(n,types);m.setAccessible(true);return m;
    }
    static void check(boolean b,String text) { if(!b)throw new AssertionError(text); }
    static void parse(RouteGuidance rg,String text) throws Exception {
        byte[] b=text.getBytes("UTF-8");
        method(RouteGuidance.class,"parse",CarplayBus.Data.class).invoke(rg,CarplayBus.parseText(b,b.length));
    }
    static void queueLoss() throws Exception {
        RendererServer r=new RendererServer();
        set(r,"running",true);set(r,"out",new ByteArrayOutputStream());
        BAPBridge bridge=new BAPBridge();set(bridge,"rendererClient",r);set(bridge,"customRendererStarted",true);
        RouteGuidance.State s=new RouteGuidance.State();s.maneuverOrder=new int[]{0};s.maneuverCount=1;
        s.mType[0]=2;s.mTurnAngle[0]=90;s.mTurnAnglePresent[0]=true;s.mJunctionType[0]=0;s.mVer[0]=1;
        Method update=method(BAPBridge.class,"updateRendererIfChanged",RouteGuidance.State.class,int.class);
        check(Boolean.TRUE.equals(update.invoke(bridge,s,0)),"maneuver not initially accepted");
        for(int i=0;i<80;i++)check(r.sendProgress(i%17,1,RendererServer.PROGRESS_FILL),"progress not accepted");
        Object[] q=(Object[])get(r,"writeQueue");int head=(Integer)get(r,"writeHead"),count=(Integer)get(r,"writeCount");
        int maneuver=0;for(int i=0;i<count;i++)if(((byte[])get(q[(head+i)%q.length],"packet"))[0]==1)maneuver++;
        boolean resend=(Boolean)update.invoke(bridge,s,0);
        System.out.println("QUEUE: accepted maneuver; after 80 progress packets, queued maneuvers="+maneuver+", duplicate resend="+resend);
        check(maneuver==0 && !resend,"queue finding not reproduced");r.dispose();
    }
    public static class FailingBridge extends BAPBridge {
        int calls;
        boolean published;
        public boolean update(RouteGuidance.State s) { calls++;published=super.update(s);return published; }
        public boolean isPresentationReady() { return true; }
    }
    public static class FailingHud extends AppConnectorNavi {
        int attempts;
        FailingHud() { super(null); }
        public void updateLaneGuidance(boolean show,CombiBAPNaviLaneGuidanceData[] lanes) {
            attempts++;throw new IllegalStateException("injected transient BAP failure");
        }
    }
    static void retryLoss() throws Exception {
        RouteGuidance rg=new RouteGuidance();FailingBridge b=new FailingBridge();
        Field uf=Unsafe.class.getDeclaredField("theUnsafe");uf.setAccessible(true);
        FailingHud hud=(FailingHud)((Unsafe)uf.get(null)).allocateInstance(FailingHud.class);
        set(b,"initialized",true);set(b,"routeTextPublished",true);set(b,"appConnectorNavi",hud);
        set(rg,"bap",b);set(rg,"running",true);set(rg,"rgActive",true);
        set(rg,"routeWantsActive",true);set(rg,"presentationConfirmed",true);set(rg,"hasRouteUpdate",true);
        RouteGuidance.State s=(RouteGuidance.State)get(rg,"state");s.routeState=1;s.maneuverCount=1;s.maneuverOrder=new int[]{0};s.mType[0]=2;
        s.laneGuidanceShowing=1;s.clearDirty();
        byte[] data="lane_guidance_showing:n:0\n".getBytes("UTF-8");
        rg.onFrame(CarplayBus.EVT_RGD_UPDATE,0,data,data.length);
        boolean retry=(Boolean)get(rg,"presentationDrivePending");
        System.out.println("RETRY: real BAP lane hide threw; HUD attempts="+hud.attempts+", published="+b.published+", update calls="+b.calls+", dirtyMask="+s.dirtyMask+", retry pending="+retry+", still confirmed="+get(rg,"presentationConfirmed"));
        check(b.calls==1 && hud.attempts==1 && !b.published && s.dirtyMask==0 && !retry,"retry finding not reproduced");
    }
    static void slotReuse() throws Exception {
        RouteGuidance rg=new RouteGuidance();
        String old="route_state:n:1\nmaneuver_count:n:1\nmaneuver_list:s:0\nm0_type:n:14\nm0_ver:n:2\nm0_turn_angle:n:45\nm0_junction_type:n:0\nm0_junction_angles:s:0,45,90\nm0_after_road:s:Old road\nm0_distance:n:6000\n";
        parse(rg,old);
        parse(rg,"m0_type:n:2\nm0_ver:n:35\n");
        RouteGuidance.State s=(RouteGuidance.State)get(rg,"state");
        System.out.println("SLOT REUSE: new version="+s.mVer[0]+", inherited angle="+s.mTurnAngle[0]+", present="+s.mTurnAnglePresent[0]+", roads="+Arrays.toString(s.mJunctionAngles[0])+", road="+s.mAfterRoad[0]+", step="+s.mDistance[0]);
        check(s.mTurnAnglePresent[0] && s.mTurnAngle[0]==45 && "Old road".equals(s.mAfterRoad[0]),"slot finding not reproduced");
        parse(rg,"route_state:n:0\nmaneuver_count:n:0\n");
        parse(rg,"route_state:n:1\nmaneuver_count:n:1\nmaneuver_list:s:0\nm0_type:n:2\nm0_ver:n:2\n");
        System.out.println("NEW ROUTE: reused version="+s.mVer[0]+", inherited angle="+s.mTurnAngle[0]+", road="+s.mAfterRoad[0]);
        check(s.mTurnAnglePresent[0] && "Old road".equals(s.mAfterRoad[0]),"route-reset finding not reproduced");
    }
    static void timeReplay() throws Exception {
        BAPBridge b=new BAPBridge();RouteGuidance.State s=new RouteGuidance.State();s.timeRemainingSeconds=600;
        Method cache=method(BAPBridge.class,"cacheTravelInfo",RouteGuidance.State.class,int.class);
        Method remain=method(BAPBridge.class,"currentRemainingSeconds");
        cache.invoke(b,s,RouteGuidance.State.DIRTY_TIME_REMAINING);
        long sample=(Long)get(b,"lastTimeRemainingSampleSeconds");set(b,"lastTimeRemainingSampleSeconds",sample-120);
        long before=(Long)remain.invoke(b);
        s.markAllDirtyForReplay();cache.invoke(b,s,s.dirtyMask);long after=(Long)remain.invoke(b);
        System.out.println("TIME REPLAY: without fresh source sample, remaining "+before+" -> "+after+" seconds");
        check(after-before>=119,"time replay finding not reproduced");
    }
    public static void main(String[] args) throws Exception {
        Log.setLevel(-1);queueLoss();retryLoss();slotReuse();timeReplay();
    }
}
