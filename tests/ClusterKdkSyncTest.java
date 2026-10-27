import com.luka.carplay.cluster.ClusterLayerController;
import com.luka.carplay.core.AltScreenModule;
import com.luka.carplay.framework.Log;
import de.audi.tghu.fwhmi.IDisplayManagerKombiControl;
import java.lang.reflect.*;
import java.util.*;

/** Exercise real layer writes/context decisions with the VC's recovered event sequence. */
public final class ClusterKdkSyncTest implements InvocationHandler {
    final Map opacity = new HashMap();
    int cropWidth;
    static void check(boolean b, String message) { if (!b) throw new AssertionError(message); }
    static void field(String name, Object value) throws Exception {
        Field f=AltScreenModule.class.getDeclaredField(name); f.setAccessible(true);f.set(null,value);
    }
    public Object invoke(Object proxy, Method method, Object[] args) {
        if (method.getName().equals("setOpacity")) opacity.put(args[0],args[2]);
        if (method.getName().equals("setCropping")) cropWidth=((Integer)args[4]).intValue();
        Class type=method.getReturnType();
        if(type==Boolean.TYPE) return Boolean.FALSE;
        if(type==Integer.TYPE) return Integer.valueOf(0);
        return null;
    }
    int opacity(int id) { return ((Integer)opacity.get(Integer.valueOf(id))).intValue(); }
    void visible(int maneuver,int sport,int popup) {
        check(opacity(98)==maneuver && opacity(101)==sport && opacity(102)==popup,
            "unexpected opacity: "+opacity);
    }
    public static void main(String[] args) throws Exception {
        Log.setLevel(-1);
        ClusterKdkSyncTest capture=new ClusterKdkSyncTest();
        IDisplayManagerKombiControl dm=(IDisplayManagerKombiControl)Proxy.newProxyInstance(
            ClusterKdkSyncTest.class.getClassLoader(),new Class[]{IDisplayManagerKombiControl.class},capture);
        field("platformSupported",Boolean.TRUE);field("connected",Boolean.TRUE);
        field("navActive",Boolean.TRUE);
        ClusterLayerController.bind(dm,1);
        ClusterLayerController.onVcPresentation(true);
        ClusterLayerController.onVcVisibility(true);capture.visible(100,0,100);
        // View request is not the stage event; it must not expose the destination rectangle.
        field("desiredViewAreaMode",Integer.valueOf(1));
        ClusterLayerController.reapply();capture.visible(100,0,100);
        ClusterLayerController.onVcVisibility(false);capture.visible(0,0,0);
        check(AltScreenModule.isNavActive(),"View fade must preserve active-route context");
        ClusterLayerController.onVcPresentation(false);capture.visible(0,0,0);
        check(capture.cropWidth==328,"VC midpoint moves hidden inTube crop");
        ClusterLayerController.onVcVisibility(true);capture.visible(100,100,0);
        // Duplicate callbacks and replays must not reveal the other backing.
        ClusterLayerController.onVcPresentation(false);ClusterLayerController.reapply();capture.visible(100,100,0);
        ClusterLayerController.onVcVisibility(false);
        ClusterLayerController.onVcPresentation(true);capture.visible(0,0,0);
        ClusterLayerController.onVcVisibility(true);capture.visible(100,0,100);
        AltScreenModule.setNavActive(false);
        check(AltScreenModule.isNavActive(),"route end retains context until VC hide");
        ClusterLayerController.reapply();capture.visible(100,0,100);
        // A restarted route cancels pending release; later View hide cannot stop it.
        AltScreenModule.setNavActive(true);
        ClusterLayerController.onVcVisibility(false);
        check(AltScreenModule.isNavActive(),"restart cancels old release");
        ClusterLayerController.onVcVisibility(true);
        AltScreenModule.setNavActive(false);
        ClusterLayerController.onVcVisibility(false);capture.visible(0,0,0);
        check(!AltScreenModule.isNavActive(),"VC hide releases ended route");
        ClusterLayerController.onVcVisibility(true);capture.visible(0,0,0);
        check(!AltScreenModule.isNavActive(),"late visibility cannot resurrect ended route");
        // Release restores the stock hint-selected backing, not the CarPlay destination.
        field("connected",Boolean.FALSE);field("videoAvailable",Boolean.FALSE);
        ClusterLayerController.apply(dm,1,null,true,100,true);capture.visible(0,100,0);
        System.out.println("ClusterKdkSyncTest: VC hide/midpoint/show, route end/restart, stale show, stock restore PASS");
    }
}
