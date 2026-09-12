import com.luka.carplay.rgd.*;
import com.luka.carplay.framework.Log;
import de.audi.app.combi.bap.app.navi.AppConnectorNavi;
import de.audi.app.combi.bap.app.navi.CombiModuleNavi;
import de.audi.app.bap.fw.functiontypes.BAPFunctionPropertyFSG;
import de.audi.app.bap.fw.functionsync.IFunctionSynchronizationHandler;
import de.vw.mib.bap.requests.StatusProperty;
import de.vw.mib.bap.generated.navsd.serializer.ManeuverDescriptor_Status;
import de.vw.mib.bap.stream.ByteArrayStream;
import de.audi.atip.interapp.combi.bap.navi.data.CombiBAPNaviManeuverDescriptor;
import java.io.*;
import java.lang.reflect.*;
import sun.misc.Unsafe;

/** Offline audit of shipping Java mapping and reconstructed stock serialization.
 * No network/HU access. Only BAP transport, sync service and framework constructors
 * are stubbed; both bridge paths, SideStreets and RendererServer packing execute.
 * IMPORTANT: reconstructed JXE strings can be corrupt; retain raw evidence.
 */
public final class ManeuverChainAudit {
    public static final class Sender extends AppConnectorNavi {
        CombiBAPNaviManeuverDescriptor[] input;
        Sender() { super(null); } // skipped by Unsafe
        public void updateManeuverDescriptor(CombiBAPNaviManeuverDescriptor[] data) {
            input=data; super.updateManeuverDescriptor(data);
        }
    }
    public static final class Capture extends BAPFunctionPropertyFSG {
        StatusProperty last;
        Capture() { super(null, 23); }
        public void sendStatusIfChanged(StatusProperty status) { last = status; }
    }
    public static final class Module extends CombiModuleNavi {
        Capture capture;
        IFunctionSynchronizationHandler sync;
        Module() { super(null, null); }
        public BAPFunctionPropertyFSG getBAPFunctionPropertyFSG(int id) {
            check(id == 23, "unexpected function " + id); return capture;
        }
        public IFunctionSynchronizationHandler getFunctionSynchronizationHandler() { return sync; }
    }
    static void check(boolean ok, String why) { if (!ok) throw new AssertionError(why); }
    static Field field(Object o, String name) throws Exception {
        for (Class<?> c = o.getClass(); c != null; c = c.getSuperclass()) {
            try { Field f = c.getDeclaredField(name); f.setAccessible(true); return f; }
            catch (NoSuchFieldException e) { }
        }
        throw new NoSuchFieldException(name);
    }
    static void set(Object o, String name, Object value) throws Exception { field(o,name).set(o,value); }
    static Object get(Object o, String name) throws Exception { return field(o,name).get(o); }
    static String hex(byte[] data) {
        StringBuilder b = new StringBuilder();
        for (byte v : data) { if (b.length()>0) b.append(' '); b.append(String.format("%02x",v&255)); }
        return b.toString();
    }
    static String ints(int[] a) {
        StringBuilder b = new StringBuilder();
        if (a != null) for (int v:a) { if (b.length()>0) b.append(';'); b.append(v); }
        return b.toString();
    }
    static byte[] encoded(Object bapString) throws Exception {
        Method m=bapString.getClass().getDeclaredMethod("getEncodedBytes"); m.setAccessible(true);
        return (byte[])m.invoke(bapString);
    }
    static String typeName(int mt) throws Exception {
        for (Field f : ManeuverMapper.class.getFields())
            if (f.getName().startsWith("MT_") && f.getType()==Integer.TYPE && f.getInt(null)==mt) return f.getName();
        return "MISSING";
    }
    static int exampleAngle(int mt) {
        switch (mt) {
            case 1: case 20: case 24: return -90;
            case 2: case 21: case 25: return 90;
            case 13: case 49: return -45;
            case 14: case 50: return 45;
            case 47: return -135; case 48: return 135;
            case 4: case 18: case 26: return -180; case 19: return 180;
            case 8: case 9: case 23: case 53: return 30;
            case 22: case 52: return -30;
            default: return mt>=28 && mt<=46 ? 90 : 0;
        }
    }
    final Capture capture;
    final Sender sender;
    final BAPBridge bridge = new BAPBridge();
    final RendererServer renderer = new RendererServer();
    final Method sendBap, sendRenderer;
    ManeuverChainAudit() throws Exception {
        Field f=Unsafe.class.getDeclaredField("theUnsafe"); f.setAccessible(true);
        Unsafe u=(Unsafe)f.get(null);
        capture=(Capture)u.allocateInstance(Capture.class);
        Module module=(Module)u.allocateInstance(Module.class); module.capture=capture;
        module.sync=(IFunctionSynchronizationHandler)Proxy.newProxyInstance(getClass().getClassLoader(),
            new Class<?>[]{IFunctionSynchronizationHandler.class},new InvocationHandler(){
                public Object invoke(Object proxy,Method method,Object[] args) {
                    if(method.getReturnType()==Boolean.TYPE) return Boolean.TRUE;
                    if(method.getReturnType()==Integer.TYPE) return Integer.valueOf(0);
                    return null;
                }
            });
        sender=(Sender)u.allocateInstance(Sender.class);
        Constructor<?> ctor=Class.forName("com.luka.carplay.rgd.BAPBridge$SilentLogChannel").getDeclaredConstructor();
        ctor.setAccessible(true); set(sender,"logChannel",ctor.newInstance()); set(sender,"moduleFsg",module);
        set(bridge,"appConnectorNavi",sender); set(bridge,"rendererClient",renderer);
        set(bridge,"customRendererStarted",Boolean.TRUE);
        set(renderer,"running",Boolean.TRUE); set(renderer,"out",new ByteArrayOutputStream());
        sendBap=BAPBridge.class.getDeclaredMethod("sendManeuvers",RouteGuidance.State.class); sendBap.setAccessible(true);
        sendRenderer=BAPBridge.class.getDeclaredMethod("updateRendererIfChanged",RouteGuidance.State.class,Integer.TYPE); sendRenderer.setAccessible(true);
    }
    RouteGuidance.State state(int mt,int angle,int junction,int side,int z,int[] streets) {
        RouteGuidance.State s=new RouteGuidance.State(); s.maneuverCount=1; s.maneuverOrder=new int[]{0};
        s.mType[0]=mt; s.mTurnAnglePresent[0]=true; s.mTurnAngle[0]=angle; s.mExitAngle[0]=angle;
        s.mJunctionType[0]=junction; s.mDrivingSide[0]=side; s.mZLevel[0]=z;
        s.mJunctionAngles[0]=streets; s.mVer[0]=1; return s;
    }
    byte[] pendingPacket() throws Exception {
        Object[] q=(Object[])get(renderer,"writeQueue");
        check(((Integer)get(renderer,"writeCount")).intValue()==1,"expected one renderer command");
        return (byte[])get(q[0],"packet");
    }
    void reset() throws Exception {
        capture.last=null; set(bridge,"lastCrIcon",Integer.valueOf(-100));
        Object[] q=(Object[])get(renderer,"writeQueue"); for(int i=0;i<q.length;i++)q[i]=null;
        set(renderer,"writeHead",Integer.valueOf(0));set(renderer,"writeTail",Integer.valueOf(0));set(renderer,"writeCount",Integer.valueOf(0));
    }
    void row(PrintWriter out,String kind,int mt,int angle,int junction,int side,int z,int[] streets) throws Exception {
        reset(); RouteGuidance.State s=state(mt,angle,junction,side,z,streets);
        sendBap.invoke(bridge,s); check(capture.last!=null,"BAP missing");
        ManeuverDescriptor_Status status=(ManeuverDescriptor_Status)capture.last;
        check(status.maneuver_2.mainElement==0 && status.maneuver_3.mainElement==0,"trailing BAP slots active");
        check(status.maneuver_1.mainElement==sender.input[0].mainElement &&
              status.maneuver_1.direction==sender.input[0].direction &&
              status.maneuver_1.zLevelGuidance==sender.input[0].zLevelGuidance,"stock numeric mapping changed");
        ByteArrayStream stream=new ByteArrayStream(); status.serialize(stream);
        check(Boolean.TRUE.equals(sendRenderer.invoke(bridge,s,Integer.valueOf(0))),"renderer missing");
        byte[] cmd=pendingPacket(); check(cmd.length==48 && cmd[0]==1,"renderer command format");
        double exit=(short)(((cmd[4]&255)<<8)|(cmd[5]&255));
        if ((cmd[1]&4)!=0) exit *= 0.5;
        out.println(kind+","+mt+","+typeName(mt)+","+angle+","+junction+","+side+","+z+","+ints(streets)+","+
            status.maneuver_1.mainElement+","+status.maneuver_1.direction+","+status.maneuver_1.zLevelGuidance+","+
            hex(sender.input[0].sideStreets)+","+hex(encoded(status.maneuver_1.sidestreets))+","+hex(stream.toByteArray())+","+(cmd[2]&255)+","+cmd[3]+","+exit+","+hex(cmd));
    }
    public static void main(String[] args) throws Exception {
        Log.setLevel(-1); ManeuverChainAudit audit=new ManeuverChainAudit();
        PrintWriter out=new PrintWriter(new OutputStreamWriter(new FileOutputStream(args[0]),"UTF-8"));
        out.println("case,mt,type,angle,junction,traffic,z_input,junction_angles,bap_main,bap_direction,bap_z,bap_sidestreets,stock_sidestreets,stock_fct23_hex,renderer_icon,renderer_direction,renderer_angle,renderer_hex");
        int count=0;
        for(int mt=0;mt<=53;mt++) for(int side=0;side<2;side++) {
            audit.row(out,"canonical",mt,exampleAngle(mt),(mt==6||mt==7||mt==19||mt>=28&&mt<=46)?1:0,side,0,new int[]{-90,0,90}); count++;
        }
        int[] angles={-1000,-180,-157,-135,-113,-112,-90,-68,-67,-45,-23,-1,0,23,45,67,68,90,112,113,135,157,180,1000};
        for(int mt=0;mt<=53;mt++)for(int j=-1;j<=1;j++)for(int side=0;side<2;side++)for(int a:angles) {
            audit.row(out,"matrix",mt,a,j,side,0,new int[]{-135,-90,-45,0,45,90,135});count++;
        }
        for(int z:new int[]{-1,0,1,2,3,255}) { audit.row(out,"z_level",1,-90,0,0,z,new int[]{-90,0,90});count++; }
        out.close();
        audit.reset(); RouteGuidance.State s=audit.state(1,-90,0,0,0,new int[]{-90,0});
        check(Boolean.TRUE.equals(audit.sendRenderer.invoke(audit.bridge,s,Integer.valueOf(0))),"initial update");
        s.mJunctionAngles[0]=new int[]{-90,0,90};
        boolean streetsSent=((Boolean)audit.sendRenderer.invoke(audit.bridge,s,Integer.valueOf(0))).booleanValue();
        s.mVer[0]++;
        boolean versionSent=((Boolean)audit.sendRenderer.invoke(audit.bridge,s,Integer.valueOf(0))).booleanValue();
        check(streetsSent && versionSent,"junction-only or version update suppressed");
        audit.reset(); s=audit.state(1,-90,0,0,0,null);
        s.maneuverCount=2; s.maneuverOrder=new int[]{0,1}; s.mType[0]=-1;
        s.mType[1]=2; s.mTurnAngle[1]=90; s.mJunctionType[1]=0; s.mDrivingSide[1]=0; s.mVer[1]=1;
        audit.sendBap.invoke(audit.bridge,s);
        boolean invalidHeadBap=audit.capture.last!=null;
        boolean invalidHeadRenderer=((Boolean)audit.sendRenderer.invoke(audit.bridge,s,Integer.valueOf(0))).booleanValue();
        check(!invalidHeadBap && !invalidHeadRenderer,"invalid primary slot must not borrow next maneuver");
        audit.reset(); s=new RouteGuidance.State(); s.maneuverCount=0;
        audit.sendBap.invoke(audit.bridge,s);
        check(((ManeuverDescriptor_Status)audit.capture.last).maneuver_1.mainElement==0,"empty route must send NO_SYMBOL");
        PrintWriter checks=new PrintWriter(new FileOutputStream(args[0]+".checks.json"));
        checks.println("{\"rows\":"+count+",\"junction_only_renderer_update\":"+streetsSent+
            ",\"version_renderer_update\":"+versionSent+",\"invalid_head_bap_update\":"+invalidHeadBap+
            ",\"invalid_head_renderer_update\":"+invalidHeadRenderer+",\"empty_route_bap_main\":0}"); checks.close();
        System.err.println("ManeuverChainAudit: "+count+" rows; junction-only="+streetsSent+
            "; version="+versionSent+"; invalid-head BAP="+invalidHeadBap+" renderer="+invalidHeadRenderer+"; empty-route=NO_SYMBOL");
    }
}
