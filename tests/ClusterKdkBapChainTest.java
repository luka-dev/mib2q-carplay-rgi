import com.luka.carplay.cluster.ClusterLayerController;
import com.luka.carplay.core.AltScreenModule;
import com.luka.carplay.rgd.GatedCombiService;
import com.luka.carplay.framework.Log;
import de.audi.tghu.navi.app.cluster.AltScreenCombiBAPListener;
import de.audi.tghu.navi.app.cluster.ClusterService;
import de.audi.atip.interapp.combi.bap.navi.CombiBAPServiceNavi;
import de.audi.atip.mmicombi.IViewSizeManager;
import de.audi.tghu.fwhmi.IDisplayManagerKombiControl;
import java.lang.reflect.*;
import sun.misc.Unsafe;

/** Real patched listener -> stock MU1316 super -> real RG gate -> Status sink.
 * Checks layer ordering at the exact outgoing acknowledgement boundary. */
public final class ClusterKdkBapChainTest {
    public static final class Service extends ClusterService {
        Service() { super(null,null,null,null,null,null); }
        public void setSupplementaryMap(int view, boolean visible) { }
    }
    static void set(Object object,String name,Object value) throws Exception {
        for(Class c=object.getClass();c!=null;c=c.getSuperclass()) {
            try {Field f=c.getDeclaredField(name);f.setAccessible(true);f.set(object,value);return;}
            catch(NoSuchFieldException e) { }
        }
        throw new NoSuchFieldException(name);
    }
    public static void main(String[] args) throws Exception {
        Log.setLevel(-1);
        Field uf=Unsafe.class.getDeclaredField("theUnsafe");uf.setAccessible(true);
        Unsafe u=(Unsafe)uf.get(null);
        final ClusterKdkSyncTest capture=new ClusterKdkSyncTest();
        IDisplayManagerKombiControl dm=(IDisplayManagerKombiControl)Proxy.newProxyInstance(
            ClusterKdkBapChainTest.class.getClassLoader(),new Class[]{IDisplayManagerKombiControl.class},capture);
        ClusterKdkSyncTest.field("platformSupported",Boolean.TRUE);
        ClusterKdkSyncTest.field("connected",Boolean.TRUE);ClusterKdkSyncTest.field("navActive",Boolean.TRUE);
        ClusterLayerController.bind(dm,1);
        final int[] acknowledgements={0,0};
        CombiBAPServiceNavi sink=(CombiBAPServiceNavi)Proxy.newProxyInstance(
            ClusterKdkBapChainTest.class.getClassLoader(),new Class[]{CombiBAPServiceNavi.class},new InvocationHandler(){
                public Object invoke(Object p,Method m,Object[] a) {
                    if(m.getName().equals("updateMapVisibility")) {
                        boolean visible=((Boolean)a[1]).booleanValue();
                        ClusterKdkSyncTest.check(capture.opacity(98)==(visible?100:0),"Status sent before layer visibility applied");
                        acknowledgements[0]++;
                    }
                    if(m.getName().equals("updateMapPresentation")) {
                        boolean popup=((Boolean)a[0]).booleanValue();
                        ClusterKdkSyncTest.check(ClusterLayerController.maneuverViewport()[2]==(popup?210:328),"Status sent before VC stage applied");
                        acknowledgements[1]++;
                    }
                    return null;
                }
            });
        GatedCombiService gate=new GatedCombiService(sink);gate.setRouteGuidanceBlocked(true);
        AltScreenCombiBAPListener listener=(AltScreenCombiBAPListener)u.allocateInstance(AltScreenCombiBAPListener.class);
        Constructor log=Class.forName("com.luka.carplay.rgd.BAPBridge$SilentLogChannel").getDeclaredConstructor();log.setAccessible(true);
        set(listener,"logChannel",log.newInstance());set(listener,"combiservice",gate);
        set(listener,"clusterservice",u.allocateInstance(Service.class));
        set(listener,"viewSizeManagerFPK",Proxy.newProxyInstance(ClusterKdkBapChainTest.class.getClassLoader(),
            new Class[]{IViewSizeManager.class},new InvocationHandler(){
                public Object invoke(Object p,Method m,Object[] a) {return null;}
            }));
        listener.setMapPresentation(true,false,false);
        listener.setMapVisibility(false,true);capture.visible(100,0,100);
        listener.setMapVisibility(false,false);capture.visible(0,0,0);
        listener.setMapPresentation(false,false,false);capture.visible(0,0,0);
        listener.setMapVisibility(false,true);capture.visible(100,100,0);
        listener.setMapVisibility(false,true);capture.visible(100,100,0);
        // Internal stock changes bypass setMapVisibility(boolean, boolean).
        listener.setSupplementaryMapVisibility(false,true);capture.visible(0,0,0);
        listener.setMapVisibility(false,true,true);capture.visible(100,100,0);
        ClusterKdkSyncTest.check(acknowledgements[0]==6 && acknowledgements[1]==2,"stock Status acknowledgements lost through RG gate");
        System.out.println("ClusterKdkBapChainTest: stock listener + RG gate, apply-before-Status, duplicate requests PASS");
    }
}
