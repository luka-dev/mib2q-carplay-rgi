package com.luka.carplay.rgd;

import com.luka.carplay.bus.CarplayBus;
import com.luka.carplay.framework.Log;
import de.audi.atip.interapp.combi.bap.navi.CombiBAPServiceNavi;
import de.audi.atip.interapp.combi.bap.navi.data.CombiBAPNaviManeuverDescriptor;
import java.lang.reflect.*;

/** Actual parser, onFrame, cached replay and bridge; only the two output
 * endpoints and native presentation readiness are controlled on the host. */
public final class RgiDeliveryRecoveryTest {
    static void check(boolean value,String why){if(!value)throw new AssertionError(why);}
    static Field field(Object o,String name)throws Exception {
        for(Class c=o.getClass();c!=null;c=c.getSuperclass())try {
            Field f=c.getDeclaredField(name);f.setAccessible(true);return f;
        }catch(NoSuchFieldException e){}
        throw new NoSuchFieldException(name);
    }
    static Object get(Object o,String name)throws Exception{return field(o,name).get(o);}
    static void set(Object o,String name,Object v)throws Exception{field(o,name).set(o,v);}
    static Method method(Class c,String name,Class... args)throws Exception {
        Method m=c.getDeclaredMethod(name,args);m.setAccessible(true);return m;
    }
    static class HUD implements InvocationHandler {
        String failMethod;int failures;boolean lanes;int direction;int laneWrites;int bar;
        public Object invoke(Object proxy,Method m,Object[] args) {
            if(m.getName().equals(failMethod) && failures-->0)throw new IllegalStateException("injected BAP failure");
            if(m.getName().equals("updateLaneGuidance")){lanes=(Boolean)args[0];laneWrites++;}
            if(m.getName().equals("updateManeuverDescriptor"))
                direction=((CombiBAPNaviManeuverDescriptor[])args[0])[0].direction;
            if(m.getName().equals("updateDistanceToNextManeuver"))bar=(Integer)args[3];
            return null;
        }
    }
    static class Renderer extends RendererServer {
        volatile int failManeuvers,attempts,writes;
        int failProgress,angle,disconnects,progressWrites,lastLevel,lastMode,lastProgress;
        boolean lanes,ready=true;
        public boolean isReady(){return ready;}
        public boolean isFrameReady(){return ready;}
        public boolean isConnected(){return ready;}
        public boolean everConnected(){return true;}
        public void disconnectClient(){disconnects++;}
        public boolean sendVisibleArea(int x,int y,int w,int h){return true;}
        boolean sendLaneGuidance(LaneGuidanceSnapshot s){lanes=s.showing && s.count>0;return true;}
        public boolean sendProgress(int level,int mode,int state){
            if(failProgress-->0)return false;
            progressWrites++;lastLevel=level;lastMode=mode;lastProgress=state;return true;
        }
        public boolean sendBapProgressManeuver(int icon,int dir,int exit,int side,int[] roads,
                int level,int mode,int perspective,boolean refresh,boolean snap,int progress) {
            attempts++;if(failManeuvers-->0)return false;writes++;angle=exit;
            lastLevel=level;lastMode=mode;lastProgress=progress;return true;
        }
    }
    static class Bridge extends BAPBridge {
        final Renderer renderer;
        Bridge(Renderer r){renderer=r;}
        public boolean preparePresentation(){return renderer.ready;}
        public boolean isPresentationReady(){return renderer.ready;}
    }
    static class Fixture {
        final RouteGuidance rg=new RouteGuidance();final HUD hud=new HUD();final Renderer renderer=new Renderer();
        final Bridge bridge=new Bridge(renderer);final RouteGuidance.State state;
        Fixture()throws Exception {
            set(bridge,"initialized",true);set(bridge,"bapSessionStarted",true);set(bridge,"routeTextPublished",true);
            set(bridge,"customRendererStarted",true);set(bridge,"rendererClient",renderer);
            set(bridge,"appConnectorNavi",Proxy.newProxyInstance(getClass().getClassLoader(),new Class[]{CombiBAPServiceNavi.class},hud));
            set(rg,"bap",bridge);set(rg,"running",true);set(rg,"rgActive",true);
            set(rg,"routeWantsActive",true);set(rg,"presentationConfirmed",true);
            bridge.setPresentationListener((BAPBridge.PresentationListener)get(rg,"bapPresentationListener"));
            state=(RouteGuidance.State)get(rg,"state");
            feed("route_generation:n:100\nroute_state:n:1\nmaneuver_count:n:1\nmaneuver_list:s:0\n"
                +"m0_ver:n:2\nm0_type:n:1\nm0_exit_angle:n:-90\nm0_junction_type:n:0\ndist_maneuver_m:n:5000\n"
                +"lane_guidance_showing:n:1\nlane_guidance_index:n:73\nlane_guidance_slot:n:3\n"
                +"lg3_index:n:73\nlg3_lane_count:n:1\nlg3_lane_directions:s:-90\nlg3_lane_status:s:2\nlg3_lane_angles:s:-90\n");
            check(state.dirtyMask==0 && hud.lanes && renderer.lanes,"initial shared snapshot");
        }
        void feed(String text)throws Exception{byte[] p=text.getBytes("UTF-8");rg.onFrame(CarplayBus.EVT_RGD_UPDATE,0,p,p.length);}
        boolean replay()throws Exception{return (Boolean)method(RouteGuidance.class,"driveCachedPresentation").invoke(rg);}
        boolean confirmed()throws Exception{return (Boolean)get(rg,"presentationConfirmed");}
        boolean pending()throws Exception{return (Boolean)get(rg,"presentationDrivePending");}
    }
    static void failedHudHide()throws Exception {
        Fixture f=new Fixture();int writes=f.renderer.writes;
        f.hud.failMethod="updateLaneGuidance";f.hud.failures=2;
        f.feed("lane_guidance_showing:n:0\n");
        check(f.state.dirtyMask!=0 && f.pending() && f.confirmed(),"lost dirty hide/retry or flickered context");
        check(f.hud.lanes && f.renderer.lanes,"failed HUD update changed only renderer");
        check(f.replay() && f.confirmed(),"failed cached retry lost presentation");
        check(!f.replay() && !f.hud.lanes && !f.renderer.lanes && f.state.dirtyMask==0,"shared hide did not converge");
        check(f.renderer.writes==writes,"retry replayed an already accepted transition");
    }
    static void failedManeuverAndSupersession()throws Exception {
        Fixture f=new Fixture();f.renderer.failManeuvers=1;int progressWrites=f.renderer.progressWrites;
        f.feed("m0_ver:n:3\nm0_type:n:2\nm0_exit_angle:n:90\nm0_junction_type:n:0\n");
        check(f.hud.direction==ManeuverMapper.DIR_RIGHT && f.renderer.angle==-180,"failure fixture");
        check(f.state.dirtyMask!=0 && f.pending() && f.confirmed(),"renderer failure reported successful publication");
        check(f.renderer.progressWrites==progressWrites,"next maneuver progress painted previous arrow before enqueue");
        f.feed("dist_maneuver_m:n:4900\n");
        check(f.renderer.angle==180 && f.state.dirtyMask==0,"distance-only update lost pending maneuver");
        f.renderer.failManeuvers=1;
        f.feed("m0_ver:n:4\nm0_type:n:1\nm0_exit_angle:n:-90\nm0_junction_type:n:0\n");
        f.feed("m0_ver:n:5\nm0_type:n:50\nm0_exit_angle:n:45\nm0_junction_type:n:0\n");
        check(f.hud.direction==ManeuverMapper.DIR_SLIGHT_RIGHT && f.renderer.angle==90,"replayed stale desired maneuver");
        int writes=f.renderer.writes;check(!f.replay() && f.renderer.writes==writes,"shared replay restarted transition");
        // Fail a send outside update (the blink/output path) and require a wakeup.
        set(f.rg,"presentationDrivePending",false);f.renderer.failProgress=4;
        Method progress=method(BAPBridge.class,"sendDistanceToManeuverRaw",int.class,boolean.class,int.class);
        for(int i=0;i<4;i++)progress.invoke(f.bridge,5000,false,0);
        check(f.pending() && f.renderer.disconnects==0,"backpressure lost retry or disconnected healthy renderer");
    }
    static void blinkPhase()throws Exception {
        Fixture f=new Fixture();
        Method context=method(BAPBridge.class,"updateActionBlinkContext",boolean.class,int.class,int.class);
        Method tick=method(BAPBridge.class,"sendActionBlinkTick",int.class);
        context.invoke(f.bridge,true,30,225);
        tick.invoke(f.bridge,0);
        check(f.hud.bar==100 && f.renderer.lastLevel==16 && f.renderer.lastMode==1
            && f.renderer.lastProgress==RendererServer.PROGRESS_BLINK_HIGH,"HUD/renderer high phase diverged");
        tick.invoke(f.bridge,0);
        check(f.hud.bar==0 && f.renderer.lastLevel==0 && f.renderer.lastMode==1
            && f.renderer.lastProgress==RendererServer.PROGRESS_BLINK_LOW,"HUD/renderer low phase diverged");
        set(f.bridge,"rendererManeuverPending",true);int writes=f.renderer.progressWrites;
        tick.invoke(f.bridge,0);
        check(f.hud.bar==100 && f.renderer.progressWrites==writes,"blink crossed an unaccepted maneuver");
    }
    static void workerRetry()throws Exception {
        final Fixture f=new Fixture();set(f.rg,"presentationConfirmed",false);
        f.renderer.failManeuvers=100;
        f.feed("m0_ver:n:3\nm0_type:n:2\nm0_exit_angle:n:90\nm0_junction_type:n:0\n");
        check(!f.confirmed(),"cold presentation confirmed before maneuver delivery");
        final Throwable[] failure={null};
        final Method loop=method(RouteGuidance.class,"presentationLoop",int.class);
        Thread worker=new Thread(new Runnable(){public void run(){
            try{loop.invoke(f.rg,0);}catch(Throwable e){failure[0]=e;}
        }},"RgiRecoveryTest");
        int attempts=f.renderer.attempts;worker.start();
        try {
            Thread.sleep(1100);
            int retries=f.renderer.attempts-attempts;
            check(retries>=2 && retries<=4,"failed replay spun or stopped retrying: "+retries);
            f.renderer.failManeuvers=0;
            long end=System.nanoTime()+2000000000L;
            while(f.renderer.writes<2 && System.nanoTime()<end)Thread.sleep(10);
            synchronized(f.rg) {
                check(f.renderer.writes==2 && f.confirmed() && f.state.dirtyMask==0,"worker did not converge without another iOS delta");
            }
        } finally {
            set(f.rg,"running",false);Object lock=get(f.rg,"presentationLock");
            synchronized(lock){lock.notifyAll();}worker.join(2000);
        }
        check(!worker.isAlive() && failure[0]==null,"recovery worker leaked or failed");
    }
    static void generationTransition()throws Exception {
        Fixture f=new Fixture();int writes=f.renderer.writes;
        f.feed("route_generation:n:101\nroute_state:n:1\nmaneuver_count:n:1\nmaneuver_list:s:0\n"
            +"m0_ver:n:2\nm0_type:n:1\nm0_exit_angle:n:-90\nm0_junction_type:n:0\ndist_maneuver_m:n:5000\n");
        check(f.renderer.writes==writes+1,"same slot/version in new route suppressed transition");
        check(!f.hud.lanes && !f.renderer.lanes,"new route inherited old lane event");
    }
    static void sourceTimeAge()throws Exception {
        Fixture f=new Fixture();f.feed("time_remaining_seconds:n:600\n");
        long sampled=f.state.timeRemainingSampleSeconds;check(sampled>0,"missing source sample timestamp");
        f.state.timeRemainingSampleSeconds=sampled-120;
        check(!f.replay(),"time replay failed");
        Method remaining=method(BAPBridge.class,"currentRemainingSeconds");
        long before=(Long)remaining.invoke(f.bridge);
        check(before>=479 && before<=480,"replay reset source age");
        check(!f.replay() && Math.abs((Long)remaining.invoke(f.bridge)-before)<=1,"second replay reset age");
        f.feed("time_remaining_seconds:n:600\n");
        check(f.state.timeRemainingSampleSeconds==sampled-120,"full C snapshot became fresh estimate");
        f.feed("time_remaining_seconds:n:601\n");
        check((Long)remaining.invoke(f.bridge)>=600 && f.state.timeRemainingSampleSeconds>=sampled,"fresh estimate kept old age");
    }
    public static void main(String[] args)throws Exception {
        Log.setLevel(-1);failedHudHide();failedManeuverAndSupersession();blinkPhase();workerRetry();generationTransition();sourceTimeAge();
        System.out.println("RgiDeliveryRecoveryTest: shared HUD/renderer retry, latest state, no duplicate travel/flicker, backpressure, generation and source-time age PASS");
    }
}
