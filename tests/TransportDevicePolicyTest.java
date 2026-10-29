import com.luka.carplay.core.TransportDevicePolicy;
import de.audi.app.terminalmode.*;
import de.audi.app.terminalmode.device.*;
import de.audi.app.terminalmode.dsi.smartphoneintegration.ISmartphoneIntegrationDSIController;
import de.audi.app.terminalmode.hmi.NewDeviceHMIHandler;
import de.audi.atip.hmi.modelaccess.*;
import de.audi.atip.log.LogChannel;
import de.audi.atip.utils.dispatching.IDispatcher;
import de.audi.atip.utils.generics.*;
import de.esolutions.fw.util.commons.timeout.ITimeSource;
import org.dsi.ifc.smartphoneintegration.Device;
import sun.misc.Unsafe;
import java.io.*;
import java.lang.reflect.*;
import java.util.*;

/** Executes patched stock constructors/popups, unmodified stock Yes/No,
 * TMDevice stream persistence, and the native Java rediscovery state method.
 * Hardware, model bank and dispatcher edges are substituted. */
public final class TransportDevicePolicyTest {
    static final Unsafe U;
    static {
        try { Field f=Unsafe.class.getDeclaredField("theUnsafe"); f.setAccessible(true); U=(Unsafe)f.get(null); }
        catch (Exception e) { throw new AssertionError(e); }
    }
    static final LogChannel LOG = new LogChannel() {
        public void log(int a,String b,Object c,Object d,Object e,Object f,long g,long h,long i,int j,Throwable k) { }
        public void log(int a,int b,Object c,Object d,Object e,Object f,long g,long h,long i,int j,Throwable k) { }
    };
    interface Call { Object run(String method,Object[] args) throws Exception; }
    static <T> T proxy(Class<T> type, Call call) {
        return type.cast(Proxy.newProxyInstance(type.getClassLoader(),new Class[]{type},(p,m,a)->{
            if (m.getName().equals("toString")) return type.getName();
            if (m.getName().equals("hashCode")) return System.identityHashCode(p);
            if (m.getName().equals("equals")) return p==a[0];
            return call.run(m.getName(),a);
        }));
    }
    static void check(boolean ok,String message) { if (!ok) throw new AssertionError(message); }
    static void put(Object obj,String name,Object value) throws Exception {
        Field field=obj.getClass().getDeclaredField(name); field.setAccessible(true); field.set(obj,value);
    }
    static Object get(Object obj,String name) throws Exception {
        Field field=obj.getClass().getDeclaredField(name); field.setAccessible(true); return field.get(obj);
    }
    static TMDevice phone(String address) {
        return new TMDevice(new TMDeviceID(address,SmartphoneManager.SmartphoneType.CARPLAY));
    }
    static final class Fixture {
        final Map<Integer,Integer> values=new HashMap<>();
        final IContext context;
        final NewDeviceHMIHandler handler;
        final TMDeviceControl control;
        final TMDevice device;
        int activates, deactivates;
        Fixture(TMDevice phone) throws Exception {
            device=phone;
            control=(TMDeviceControl)U.allocateInstance(TMDeviceControl.class);
            put(control,"tmDevice",device); put(control,"lc",LOG); put(control,"logTag","transport-test");
            put(control,"dispatcher",proxy(IDispatcher.class,(n,a)->{
                if (n.equals("execute")) { ((Runnable)a[0]).run(); return null; }
                if (n.equals("isDispatchThread")) return true;
                throw new AssertionError(n);
            }));
            put(control,"deviceControlToDeviceManager",proxy(TMDeviceControl.IDeviceControlToDeviceManager.class,(n,a)->{
                if (n.equals("requestActivation")) activates++;
                else if (n.equals("requestDeactivation")) deactivates++;
                else check(n.equals("notifyAboutChange"),n);
                return null;
            }));
            put(control,"timeSource",proxy(ITimeSource.class,(n,a)->1234L));
            context=proxy(IContext.class,(n,a)->{
                if (n.equals("getChoiceModel")) {
                    final int id=(Integer)a[0];
                    return proxy(ChoiceModelApp.class,(mn,ma)->{
                        if (mn.equals("setValue")) values.put(id,(Integer)ma[0]);
                        else if (mn.equals("getValue")) return values.containsKey(id)?values.get(id):0;
                        return null;
                    });
                }
                if (n.equals("getConfiguration")) return proxy(ITerminalModeConfiguration.class,(mn,ma)->false);
                throw new AssertionError(n);
            });
            handler=(NewDeviceHMIHandler)U.allocateInstance(NewDeviceHMIHandler.class);
            put(handler,"context",context); put(handler,"logger",LOG);
            put(handler,"newDevicesToBeActivated",Generics.newArrayList());
            put(handler,"deviceManager",proxy(IDeviceManager.class,(n,a)->{
                check(n.equals("control") && a[0]==device,"wrong phone control"); return control;
            }));
            Class strategy=Class.forName("de.audi.app.terminalmode.hmi.NewDeviceHMIHandler$IShowScreenStrategy");
            put(handler,"showScreenStrategy",proxy(strategy,(n,a)->null));
        }
        int remember() { return values.containsKey(3200034)?values.get(3200034):0; }
        void disclaimer() { handler.setDisclaimerDevice(device); }
        void popup(GList devices) throws Exception {
            Method m=NewDeviceHMIHandler.class.getDeclaredMethod("showScreen",GList.class,String.class);
            m.setAccessible(true); m.invoke(handler,devices,"test discovery");
        }
        void press(boolean yes) throws Exception {
            Class c=Class.forName("de.audi.app.terminalmode.hmi.NewDeviceHMIHandler$"+(yes?10:11));
            Constructor ctor=c.getDeclaredConstructor(NewDeviceHMIHandler.class); ctor.setAccessible(true);
            Object button=ctor.newInstance(handler);
            Method m=c.getDeclaredMethod("accept",ButtonModelApp.class); m.setAccessible(true);
            m.invoke(button,proxy(ButtonModelApp.class,(n,a)->n.equals("fireEvent")?Boolean.TRUE:null));
        }
        void rediscover(TMDevice loaded) throws Exception {
            put(control,"tmDevice",loaded);
            String base="de.audi.app.terminalmode.device.TMDeviceControl$SM";
            Object sm=U.allocateInstance(Class.forName(base)); put(sm,"this$0",control);
            Object state=U.allocateInstance(Class.forName(base+"$BaseState")); put(state,"this$1",sm);
            Object discovered=U.allocateInstance(Class.forName(base+"$BaseState$DiscoveredState"));
            put(discovered,"this$2",state);
            Method enter=discovered.getClass().getDeclaredMethod("enter",Class.forName("de.audi.atip.utils.statemachine.Message"));
            enter.invoke(discovered,new Object[]{null});
        }
    }
    static TMDevice persist(TMDevice device) throws Exception {
        ByteArrayOutputStream bytes=new ByteArrayOutputStream();
        device.writeToStream(new DataOutputStream(bytes));
        return (TMDevice)TMDevice.CREATOR.readFromStream(new DataInputStream(new ByteArrayInputStream(bytes.toByteArray())));
    }
    static void presentationAndStockOwnership() throws Exception {
        int[] disconnects={0};
        ISmartphoneIntegrationDSIController dsi=proxy(ISmartphoneIntegrationDSIController.class,(n,a)->{
            check(n.equals("disconnectDevice") && (Integer)a[0]==101,"wrong transport disconnected");
            disconnects[0]++; return null;
        });
        Device raw=new Device(101,"iPhone Луки",2,"wireless-hash",2);
        PhysicalDevice physical=new PhysicalDevice(raw,dsi);
        check(raw.deviceName.equals("iPhone Луки"),"mutated shared DSI object");
        Device display=physical.getDevice();
        check(display.deviceName.equals("Wireless - iPhone Луки"),"missing wireless label");
        check(display.deviceID==101 && display.deviceAddress.equals("wireless-hash") && display.connectionType==2,"changed identity/transport");
        Device wired=new PhysicalDevice(new Device(42,"iPhone Луки",1,"usb-hash",2),null).getDevice();
        check(wired.deviceName.equals("Wired - iPhone Луки"),"missing wired label");
        check(!phone(wired.deviceAddress).getUniqueID().equals(phone(display.deviceAddress).getUniqueID()),"transports merged");
        check(TransportDevicePolicy.presentation(display).deviceName.equals(display.deviceName),"duplicated prefix");
        check(TransportDevicePolicy.presentation(new Device(42,"Apple Inc.-iPhone",1,"usb-hash",2)).deviceName.equals("Apple Inc.-iPhone"),"broke native placeholder retention");
        check(TransportDevicePolicy.presentation(new Device(1,"Other",1,"other",1)).deviceName.equals("Other"),"renamed non-CarPlay device");
        TMDeviceControl first=(TMDeviceControl)U.allocateInstance(TMDeviceControl.class);
        TMDeviceControl second=(TMDeviceControl)U.allocateInstance(TMDeviceControl.class);
        physical.addTMDeviceControl(first); physical.addTMDeviceControl(second);
        check(!physical.disconnectDevice(first) && disconnects[0]==0,"lost conservative stock multi-mode ownership");
        physical.connectDevice(second);
        check(!physical.disconnectDevice(first) && disconnects[0]==0,"disconnected another active mode");
        check(physical.disconnectDevice(second) && disconnects[0]==1,"stock disconnect did not run");
    }
    static void consentAndPersistence() throws Exception {
        Fixture f=new Fixture(phone("wireless-hash"));
        f.popup(Generics.newArrayList(new TMDevice[]{f.device}));
        check(f.remember()==1,"wireless popup not default remembered");
        f.disclaimer();
        check(f.device.userAcceptState()==TMDevice.UserAcceptState.INITIAL && f.activates==0,"discovery manufactured consent");
        f.press(true); // Actual unmodified stock Yes consumer and control jobs.
        TMDevice restored=persist(f.device);
        check(restored.isStoreUserAcceptState() && restored.wasDisclaimerPreviouslyAccepted(),"Yes not persisted");
        check(restored.userAcceptState()==TMDevice.UserAcceptState.DISCLAIMER_ACCEPTED && f.activates==1,"stock Yes did not activate");
        f.rediscover(restored);
        check(restored.userAcceptState()==TMDevice.UserAcceptState.DISCLAIMER_ACCEPTED,"stock rediscovery reset remembered consent");
        check(restored.getUniqueID().equals(f.device.getUniqueID()),"persistence changed identity");

        f=new Fixture(phone("wireless-hash")); f.disclaimer(); f.values.put(3200034,0); f.press(true);
        restored=persist(f.device); f.rediscover(restored);
        check(!restored.isStoreUserAcceptState() && restored.userAcceptState()==TMDevice.UserAcceptState.INITIAL,"ignored explicit uncheck");
        f=new Fixture(phone("wireless-hash")); f.disclaimer(); f.press(false);
        check(f.activates==0 && f.deactivates==1 && !f.device.wasDisclaimerPreviouslyAccepted(),"No accepted/activated phone");

        Fixture wired=new Fixture(phone("usb-hash")); wired.disclaimer();
        check(wired.remember()==0,"changed wired consent default");
        Fixture mixed=new Fixture(phone("wireless-hash"));
        mixed.popup(Generics.newArrayList(new TMDevice[]{mixed.device,phone("usb-hash")}));
        check(mixed.remember()==0,"changed mixed popup default");
    }
    public static void main(String[] args) throws Exception {
        presentationAndStockOwnership(); consentAndPersistence();
        System.out.println("TransportDevicePolicyTest: labels, separate IDs, stock ownership, explicit Yes/No, persistence, rediscovery and opt-out PASS");
    }
}
