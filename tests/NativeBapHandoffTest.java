package com.luka.carplay.core;

import com.luka.carplay.rgd.*;
import com.luka.carplay.framework.Log;
import de.audi.atip.interapp.combi.bap.navi.CombiBAPServiceNavi;
import de.audi.tghu.navi.app.cluster.ClusterService;
import java.lang.reflect.*;
import java.util.*;
import sun.misc.Unsafe;

/** Real coordinator/gate/bridge cleanup; only dispatcher, stock bootstrap and
 * endpoint are controlled. Queue draining exposes stale-task and thread errors. */
public final class NativeBapHandoffTest {
    static int checks;
    static void check(boolean b,String why) { checks++; if(!b)throw new AssertionError(why); }
    static Field field(Class c,String n)throws Exception {Field f=c.getDeclaredField(n);f.setAccessible(true);return f;}
    static void state(String n,Object v)throws Exception {field(AltScreenNavStatusGate.class,n).set(null,v);}
    static Object state(String n)throws Exception {return field(AltScreenNavStatusGate.class,n).get(null);}
    static Unsafe unsafe()throws Exception {return (Unsafe)field(Unsafe.class,"theUnsafe").get(null);}
    static class Jobs implements AltScreenNavStatusGate.NavigationAccess {
        final LinkedList<Runnable> queue=new LinkedList<Runnable>();
        Service service; boolean available=true; boolean navigationThread;
        public ClusterService cluster(){return service;}
        public synchronized boolean execute(Runnable r){if(!available)return false;queue.add(r);return true;}
        void drain() {
            for (;;) {
                Runnable r; synchronized(this){if(queue.isEmpty())return;r=queue.removeFirst();}
                navigationThread=true;try{r.run();}finally{navigationThread=false;}
            }
        }
    }
    static class Sink implements InvocationHandler {
        Jobs jobs; List<String> calls=new ArrayList<String>(); String road; int failures;
        Throwable violation;
        public Object invoke(Object p,Method m,Object[] a) {
            if(!jobs.navigationThread)violation=new AssertionError("BAP outside NavigationJobs: "+m.getName());
            calls.add(m.getName());
            if(m.getName().equals("updateCurrentPositionInfo")) {
                if(failures-->0)throw new IllegalStateException("injected");road=(String)a[0];
            }
            return null;
        }
        CombiBAPServiceNavi endpoint(){return (CombiBAPServiceNavi)Proxy.newProxyInstance(getClass().getClassLoader(),new Class[]{CombiBAPServiceNavi.class},this);}
    }
    public static class Service extends ClusterService {
        CombiBAPServiceNavi output; int seeds, seedFailures;
        Service(){super(null,null,null,null,null,null);}
        public CombiBAPServiceNavi getCombiBAPListenerCombiService(){return output;}
        public void setCombiBAPListenerCombiService(CombiBAPServiceNavi value){output=value;seed();}
        void seed(){seeds++;if(seedFailures-->0)throw new IllegalStateException("seed failed after attach");output.updateCurrentPositionInfo("native-road");output.updateTurnToInfo("","");output.updateRGStatus(0);output.updateActiveRGType(4);}
    }
    static class Fixture {
        Jobs jobs=new Jobs(); Sink sink=new Sink(); Service service;
        Fixture()throws Exception {
            reset();sink.jobs=jobs;service=(Service)unsafe().allocateInstance(Service.class);
            service.output=sink.endpoint();jobs.service=service;state("navigationAccess",jobs);
            AltScreenNavStatusGate.setRouteGuidanceBlocked(true);
            AltScreenNavStatusGate.setCurrentPositionInfoBlocked(true);
            jobs.drain();check(service.seeds==1,"wrapper not seeded once");
            check(sink.violation==null,"bootstrap wrong thread");sink.calls.clear();
        }
        GatedCombiService gate(){return (GatedCombiService)service.output;}
        void nativeRoad(String s){jobs.navigationThread=true;try{gate().updateCurrentPositionInfo(s);}finally{jobs.navigationThread=false;}}
    }
    static void reset()throws Exception {
        synchronized(AltScreenNavStatusGate.class) {
            GatedCombiService old=(GatedCombiService)state("gate");if(old!=null)old.retire();
            state("gate",null);state("clusterService",null);
            state("desiredRouteBlocked",false);state("desiredCurrentPositionBlocked",false);
            state("appliedRouteBlocked",false);state("policyRevision",0);state("appliedRevision",0);
            state("installScheduled",false);state("failureLogged",false);
            state("seedPending",false);
            // Existing daemon may finish naturally; do not spawn a second one.
            field(AltScreenNavStatusGate.class,"installTicket").setInt(null,((Integer)state("installTicket"))+1);
        }
    }
    static void lifecycle()throws Exception {
        Fixture f=new Fixture();f.nativeRoad("latest");
        AltScreenNavStatusGate.setCurrentPositionInfoBlocked(false);
        check(f.sink.calls.isEmpty(),"release wrote on calling thread");
        f.jobs.drain();check("latest".equals(f.sink.road),"route-end restoration missing");
        check(!f.sink.calls.contains("updateRGStatus"),"route end opened session RG gate");
        check(f.service.seeds==1,"handoff invoked full stock updateAll");
        f.sink.calls.clear();AltScreenNavStatusGate.setCurrentPositionInfoBlocked(false);f.jobs.drain();
        check(f.sink.calls.contains("updateCurrentPositionInfo"),"repeated cleanup not repaired");
        f.sink.calls.clear();AltScreenNavStatusGate.setCurrentPositionInfoBlocked(false);
        AltScreenNavStatusGate.setCurrentPositionInfoBlocked(true);f.jobs.drain();
        check(f.sink.calls.isEmpty(),"queued release overwrote restarted CarPlay");
        AltScreenNavStatusGate.setRouteGuidanceBlocked(false);f.jobs.drain();
        check(f.sink.calls.contains("updateRGStatus") && "latest".equals(f.sink.road),"disconnect did not restore both groups");
        check(f.sink.violation==null,"handoff escaped NavigationJobs");
    }
    static void replacement()throws Exception {
        Fixture f=new Fixture();GatedCombiService old=f.gate();
        AltScreenNavStatusGate.setRouteGuidanceBlocked(false); // queued old task
        Runnable late; synchronized(f.jobs){late=f.jobs.queue.removeFirst();}
        Sink next=new Sink();next.jobs=f.jobs;
        f.jobs.navigationThread=true;
        try {
            f.service.output=AltScreenNavStatusGate.prepareCombiBAPService(f.service,next.endpoint());
            f.service.seed();
        }finally{f.jobs.navigationThread=false;}
        f.sink.calls.clear();next.calls.clear();old.replayPending();old.updateCurrentPositionInfo("obsolete");
        f.jobs.navigationThread=true;try{late.run();}finally{f.jobs.navigationThread=false;}
        check(f.sink.calls.isEmpty() && next.calls.isEmpty(),"retired snapshot / stale queued task sent data");
        f.jobs.navigationThread=true;try{AltScreenNavStatusGate.prepareCombiBAPService(f.service,null);}finally{f.jobs.navigationThread=false;}
        check(next.calls.isEmpty(),"service removal replayed to obsolete endpoint");
    }
    static void retryAfterDisconnect()throws Exception {
        Fixture f=new Fixture();f.sink.failures=1;
        AltScreenNavStatusGate.setRouteGuidanceBlocked(false);f.jobs.drain();
        check(f.gate().hasPendingReplay(),"handoff failure not pending");
        // Actual lazy worker must retry after disconnect, without CarPlay's module retry loop.
        long until=System.nanoTime()+3500000000L;
        while(System.nanoTime()<until && f.gate().hasPendingReplay()){Thread.sleep(20);f.jobs.drain();}
        check(!f.gate().hasPendingReplay() && "native-road".equals(f.sink.road),"disconnected retry never converged");
        check(f.sink.violation==null,"retry worker performed BAP I/O");
        until=System.nanoTime()+2500000000L;
        while(System.nanoTime()<until && state("retryThread")!=null)Thread.sleep(20);
        check(state("retryThread")==null,"idle recovery worker leaked");
    }
    static void dispatcherUnavailable()throws Exception {
        Fixture f=new Fixture();f.jobs.available=false;
        AltScreenNavStatusGate.setCurrentPositionInfoBlocked(false);
        check(f.sink.calls.isEmpty(),"release bypassed missing dispatcher");
        f.jobs.available=true;
        long until=System.nanoTime()+2500000000L;
        while(System.nanoTime()<until && f.sink.road==null){Thread.sleep(20);f.jobs.drain();}
        check("native-road".equals(f.sink.road),"missing-dispatcher release lost");
    }
    static void bootstrapFailure()throws Exception {
        Fixture f=new Fixture();
        // A new raw endpoint requires a seed even if wrapper attachment succeeds
        // before stock updateAll throws. Recovery must not forget the incomplete seed.
        f.service.output=f.sink.endpoint();f.service.seedFailures=1;
        AltScreenNavStatusGate.setCurrentPositionInfoBlocked(false);f.jobs.drain();
        check((Boolean)state("seedPending"),"interrupted bootstrap forgotten");
        long until=System.nanoTime()+2500000000L;
        while(System.nanoTime()<until && (Boolean)state("seedPending")){Thread.sleep(20);f.jobs.drain();}
        check(!(Boolean)state("seedPending") && "native-road".equals(f.sink.road),"bootstrap did not retry");
    }
    static void wrapperRecovery()throws Exception {
        Fixture f=new Fixture();GatedCombiService old=f.gate();
        // No policy change: a service edge revealing a missing wrapper must
        // still invalidate the previously applied policy and reinstall it.
        f.service.output=f.sink.endpoint();
        AltScreenNavStatusGate.onCombiBAPServiceChanged(f.service);f.jobs.drain();
        check(f.service.output instanceof GatedCombiService && f.gate()!=old,"service edge failed to recover wrapper");
        check(f.service.seeds==2,"recovered wrapper not seeded");
        f.sink.calls.clear();old.updateCurrentPositionInfo("obsolete");old.replayPending();
        check(f.sink.calls.isEmpty(),"recovery left old wrapper live");
    }
    static void bridgeCleanupOrder()throws Exception {
        final Fixture f=new Fixture();final List<String> order=new ArrayList<String>();
        BAPBridge bridge=new BAPBridge();
        field(BAPBridge.class,"initialized").setBoolean(bridge,true);
        field(BAPBridge.class,"takeoverEngaged").setBoolean(bridge,true);
        field(BAPBridge.class,"appConnectorNavi").set(bridge,Proxy.newProxyInstance(NativeBapHandoffTest.class.getClassLoader(),
            new Class[]{CombiBAPServiceNavi.class},new InvocationHandler(){public Object invoke(Object p,Method m,Object[] a){
                order.add(m.getName());
                // Even if NavigationJobs drains during each cleanup write, native
                // text is still owned by CarPlay until the last clear completes.
                f.jobs.drain();check(f.sink.calls.isEmpty(),"native replay interrupted CarPlay cleanup");return null;
            }}));
        bridge.onRouteEnd();check(order.size()==11,"real bridge cleanup incomplete");
        f.jobs.drain();check("native-road".equals(f.sink.road),"real bridge did not return text ownership");
        check(!f.sink.calls.contains("updateRGStatus"),"real bridge opened route gate before disconnect");
    }
    public static void main(String[] args)throws Exception {
        Log.setLevel(-1);
        try{lifecycle();replacement();retryAfterDisconnect();dispatcherUnavailable();bootstrapFailure();wrapperRecovery();bridgeCleanupOrder();}
        finally{reset();}
        System.out.println("NativeBapHandoffTest: PASS ("+checks+" checks)");
    }
}
