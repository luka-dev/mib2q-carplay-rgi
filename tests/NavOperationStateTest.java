package com.luka.carplay.core;

import com.luka.carplay.rgd.GatedCombiService;
import de.audi.app.bap.IPowerState;
import de.audi.app.bap.fw.functiontypes.BAPFunctionPropertyFSG;
import de.audi.app.combi.bap.app.navi.CombiModuleNavi;
import de.audi.app.combi.bap.app.navi.InitializationManagerNavi;
import de.audi.atip.hmi.view.IDisplayManager;
import de.audi.atip.interapp.combi.bap.navi.CombiBAPServiceNavi;
import de.audi.atip.log.LogChannel;
import de.vw.mib.bap.generated.navsd.serializer.FSG_OperationState_Status;
import java.lang.reflect.Field;
import java.lang.reflect.InvocationHandler;
import java.lang.reflect.Method;
import java.lang.reflect.Proxy;
import sun.misc.Unsafe;

/** Run the shipping NAVSD manager and real stock superclass state computation.
 * Only the external BAP sender, power input and DisplayManager are substitutes.
 * Unsafe bypasses framework/OSGi construction, as in the other stock-chain tests. */
public final class NavOperationStateTest {
    static final Unsafe U;
    static {
        try {
            Field f=Unsafe.class.getDeclaredField("theUnsafe");f.setAccessible(true);
            U=(Unsafe)f.get(null);
        } catch (Exception e) { throw new AssertionError(e); }
    }
    static final LogChannel LOG=new LogChannel() {
        public void log(int a,String b,Object c,Object d,Object e,Object f,long g,long h,long i,int j,Throwable k) { }
        public void log(int a,int b,Object c,Object d,Object e,Object f,long g,long h,long i,int j,Throwable k) { }
    };
    public static final class Sender extends BAPFunctionPropertyFSG {
        FSG_OperationState_Status status;
        int sends;
        Sender() { super(null,15); } // not invoked
        public de.vw.mib.bap.requests.StatusProperty getLastStatus() { return status; }
        public boolean isStatusRequestTransmissionPending() { return false; }
        public boolean isDataValid() { return true; }
        public void resendLastStatus() { sends++; }
    }
    static final class Power implements IPowerState {
        boolean on=true;
        public boolean isPowerOn() { return on; }
    }
    static Field field(Class<?> c,String name) throws Exception {
        for (;c!=null;c=c.getSuperclass()) {
            try { Field f=c.getDeclaredField(name);f.setAccessible(true);return f; }
            catch (NoSuchFieldException e) { }
        }
        throw new NoSuchFieldException(name);
    }
    static void set(Object obj,String name,Object value) throws Exception {
        field(obj.getClass(),name).set(obj,value);
    }
    static void alt(String name,Object value) throws Exception { field(AltScreenModule.class,name).set(null,value); }
    static void check(boolean ok,String why) { if (!ok) throw new AssertionError(why); }
    static final class Fixture {
        final InitializationManagerNavi manager;
        final Sender sender;
        final Power power=new Power();
        final AltScreenModule alt=new AltScreenModule();
        boolean failSwitch;
        Fixture() throws Exception {
            sender=(Sender)U.allocateInstance(Sender.class);
            sender.status=new FSG_OperationState_Status();
            manager=(InitializationManagerNavi)U.allocateInstance(InitializationManagerNavi.class);
            set(manager,"fsgOperationStateFunction",sender);
            set(manager,"module",U.allocateInstance(CombiModuleNavi.class));
            set(manager,"powerState",power);set(manager,"logChannel",LOG);
            set(manager,"appIsReady",Boolean.TRUE);set(manager,"bapStackState",Integer.valueOf(1));
            field(InitializationManagerNavi.class,"instance").set(null,manager);
            alt("platformSupported",Boolean.TRUE);alt("connected",Boolean.TRUE);
            alt("videoAvailable",Boolean.FALSE);alt("rendererReady",Boolean.FALSE);
            alt("clusterActive",Boolean.FALSE);alt("currentCtx",Integer.valueOf(74));
            alt("desiredCtx",Integer.valueOf(81));
            manager.updateOperationState();expect(3,"cold stock state");
        }
        void expect(int state,String why) {
            check(sender.status.op_State==state,why+": expected "+state+", got "+sender.status.op_State);
        }
        void compose() throws Exception {
            IDisplayManager dm=(IDisplayManager)Proxy.newProxyInstance(IDisplayManager.class.getClassLoader(),
                new Class[]{IDisplayManager.class},new InvocationHandler() {
                    public Object invoke(Object p,Method m,Object[] args) {
                        if (failSwitch && m.getName().equals("switchContext"))
                            throw new IllegalStateException("injected context failure");
                        return null;
                    }
                });
            set(alt,"dm",dm);
            Method apply=AltScreenModule.class.getDeclaredMethod("applySwitch",Integer.TYPE,IDisplayManager.class);
            apply.setAccessible(true);apply.invoke(alt,Integer.valueOf(81),dm);
        }
        void readyAndCompose() throws Exception { AltScreenModule.setRendererReady(true);compose(); }
    }
    static void fuzz(Fixture f) throws Exception {
        Method nativeState=Class.forName("de.audi.app.bap.fw.AbstractBAPModuleInitializationManagerFSG")
            .getDeclaredMethod("getGeneralOperationState");
        nativeState.setAccessible(true);
        java.util.Random random=new java.util.Random(47426351L);
        int[] apps={0,1,256,512,1024},contexts={-1,72,74,80,81};
        for (int i=0;i<10000;i++) {
            set(f.manager,"appState",Integer.valueOf(apps[random.nextInt(apps.length)]));
            set(f.manager,"initState",Integer.valueOf(random.nextInt(12)));
            set(f.manager,"appIsReady",Boolean.valueOf(random.nextBoolean()));
            boolean swdl=random.nextBoolean();set(f.manager,"swdlRunning",Boolean.valueOf(swdl));
            int bap=random.nextInt(2);set(f.manager,"bapStackState",Integer.valueOf(bap));
            f.power.on=random.nextBoolean();
            boolean connected=random.nextBoolean(),video=random.nextBoolean();
            boolean ready=random.nextBoolean(),supported=random.nextBoolean();
            int context=contexts[random.nextInt(contexts.length)];
            alt("connected",Boolean.valueOf(connected));alt("videoAvailable",Boolean.valueOf(video));
            alt("rendererReady",Boolean.valueOf(ready));alt("platformSupported",Boolean.valueOf(supported));
            alt("currentCtx",Integer.valueOf(context));
            // The oracle for native state is the unmodified stock superclass.
            int raw=((Integer)nativeState.invoke(f.manager)).intValue();
            int requested=bap==0 ? 2 : raw;
            boolean presentation=supported && ready && (connected || video) && (context==80 || context==81);
            boolean override=requested==2 && bap!=0 && f.power.on && !swdl && presentation;
            int[] wire={0,1,3,15};
            f.manager.updateOperationState();f.expect(override ? 0 : wire[requested],"mutation "+i);
            check(f.manager.isOpStateNormalOperation()==(requested==0),"native init oracle diverged at "+i);
            de.vw.mib.bap.stream.ByteArrayStream bytes=new de.vw.mib.bap.stream.ByteArrayStream();
            f.sender.status.serialize(bytes);
            check(bytes.toByteArray().length==1 && (bytes.toByteArray()[0]&255)==f.sender.status.op_State,
                "actual Fct15 wire serializer diverged at "+i);
        }
    }

    public static void main(String[] args) throws Exception {
        com.luka.carplay.framework.Log.setLevel(-1);
        Fixture f=new Fixture();
        // Counterexample: the old high-level wrapper cannot change appState=0/initState=0.
        CombiBAPServiceNavi service=(CombiBAPServiceNavi)Proxy.newProxyInstance(
            CombiBAPServiceNavi.class.getClassLoader(),new Class[]{CombiBAPServiceNavi.class},
            new InvocationHandler() {
                public Object invoke(Object p,Method m,Object[] a) {
                    if (m.getName().equals("hideInitializingScreen")) f.manager.setAppIsReady(true);
                    return null;
                }
            });
        new GatedCombiService(service).setAltScreenTakeover(true);
        f.expect(3,"hideInitializingScreen alone must not fake native app readiness");
        AltScreenModule.setRendererReady(true);
        f.expect(3,"posted loader without applied CarPlay composition");
        f.compose();f.expect(0,"cold Navigation absent, applied CarPlay context");
        check(AltScreenModule.isClusterPresented(),"presentation not published");
        check(!f.manager.isOpStateNormalOperation(),"presentation must not complete native BAP initialization");
        check(((Integer)field(f.manager.getClass(),"appState").get(f.manager)).intValue()==0,"native app state changed");
        check(f.manager.getInitState()==0,"native BAP initialization state changed");
        set(f.manager,"appState",Integer.valueOf(1));
        for (int init=0;init<9;init++) {
            set(f.manager,"initState",Integer.valueOf(init));f.manager.updateOperationState();
            f.expect(0,"BAP init window "+init);
            check(!f.manager.isOpStateNormalOperation(),"native init falsely complete at "+init);
        }
        set(f.manager,"initState",Integer.valueOf(9));f.manager.updateOperationState();
        check(f.manager.isOpStateNormalOperation(),"real native NORMAL must remain NORMAL");
        f.expect(0,"native Navigation ready");
        set(f.manager,"appState",Integer.valueOf(0));
        set(f.manager,"swdlRunning",Boolean.TRUE);f.manager.updateOperationState();f.expect(3,"software download");
        set(f.manager,"swdlRunning",Boolean.FALSE);
        f.power.on=false;f.manager.updateOperationState();f.expect(1,"power off");
        f.power.on=true;
        set(f.manager,"bapStackState",Integer.valueOf(0));f.manager.updateOperationState();f.expect(3,"BAP down");
        set(f.manager,"bapStackState",Integer.valueOf(1));
        set(f.manager,"appState",Integer.valueOf(256));f.manager.updateOperationState();f.expect(15,"native defect");
        set(f.manager,"appState",Integer.valueOf(0));f.manager.updateOperationState();f.expect(0,"presentation restored");
        AltScreenModule.setRendererReady(false);f.expect(3,"renderer loss restores real INITIALIZING");
        f.readyAndCompose();f.expect(0,"renderer reconnect");
        alt("connected",Boolean.FALSE);alt("videoAvailable",Boolean.TRUE);
        InitializationManagerNavi.refreshCarPlayPresentation();f.expect(0,"live video bridges a TMDevice bounce");
        alt("videoAvailable",Boolean.FALSE);InitializationManagerNavi.refreshCarPlayPresentation();
        f.expect(3,"disconnect recomputes real Navigation state");
        alt("connected",Boolean.TRUE);alt("clusterActive",Boolean.FALSE);
        f.failSwitch=true;f.compose();f.expect(3,"failed physical context must not claim NORMAL");
        f.failSwitch=false;f.compose();f.expect(0,"context recovery");
        alt("platformSupported",Boolean.FALSE);InitializationManagerNavi.refreshCarPlayPresentation();
        f.expect(3,"unsupported cluster");
        fuzz(f);
        System.out.println("NavOperationStateTest: cold/init/normal/faults/reconnect/disconnect/context +10000 mutations seed=47426351 PASS sends="+f.sender.sends);
    }
}
