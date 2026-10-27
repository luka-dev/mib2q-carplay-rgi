import com.luka.carplay.rgd.*;
import com.luka.carplay.core.AltScreenModule;
import com.luka.carplay.cluster.ClusterLayerController;
import com.luka.carplay.framework.Log;
import java.lang.reflect.*;

/** Real stage source -> bridge -> queued 48-byte command, without iOS updates. */
public final class RendererViewportTest {
    static void check(boolean value,String why) { if(!value) throw new AssertionError(why); }
    static int u16(byte[] b,int offset) { return ((b[offset]&255)<<8)|(b[offset+1]&255); }
    static int count(ManeuverChainAudit a) throws Exception { return ((Integer)a.get(a.renderer,"writeCount")).intValue(); }
    static void clear(ManeuverChainAudit a) throws Exception {
        Method m=RendererServer.class.getDeclaredMethod("clearWriteQueue");m.setAccessible(true);m.invoke(a.renderer);
    }
    public static void main(String[] args) throws Exception {
        Log.setLevel(-1);
        ManeuverChainAudit a=new ManeuverChainAudit();
        a.set(a.renderer,"rendererReady",Boolean.TRUE);
        a.set(a.renderer,"sock",new java.net.Socket());
        Field stage=AltScreenModule.class.getDeclaredField("desiredViewAreaMode");stage.setAccessible(true);
        int saved=stage.getInt(null);
        try {
            stage.setInt(null,AltScreenModule.VIEWAREA_FULLSCREEN);
            ClusterLayerController.onVcPresentation(true);
            int[] area=ClusterLayerController.maneuverViewport();
            check(area[0]==59 && area[1]==27 && area[2]==210 && area[3]==153,"big-stage red frame crop");
            check(a.bridge.refreshRendererViewport(),"initial crop");
            byte[] b=a.pendingPacket();
            check(b[0]==8 && u16(b,2)==59 && u16(b,4)==27 && u16(b,6)==210 && u16(b,8)==153,"crop command layout");
            check(a.bridge.refreshRendererViewport() && count(a)==1,"unchanged crop suppressed");

            stage.setInt(null,AltScreenModule.VIEWAREA_SMALLSCREEN);
            check(ClusterLayerController.maneuverViewport()[2]==210,"requested View must not move KDK before VC Fct54");
            ClusterLayerController.onVcPresentation(false);
            RouteGuidance rg=new RouteGuidance();
            a.set(rg,"running",Boolean.TRUE);a.set(rg,"rgActive",Boolean.TRUE);a.set(rg,"bap",a.bridge);
            Method request=RouteGuidance.class.getDeclaredMethod("requestViewAreaRefresh",Integer.TYPE);
            request.setAccessible(true);request.invoke(rg,Integer.valueOf(1));
            check(((Boolean)a.get(rg,"rendererViewportRefreshPending")).booleanValue(),"View button schedules crop without iOS delta");
            Method drive=RouteGuidance.class.getDeclaredMethod("driveRendererViewport");drive.setAccessible(true);
            check(((Boolean)drive.invoke(rg)).booleanValue(),"serialized viewport worker");
            b=ManeuverParityTest.latest(a);
            check(count(a)==1 && b[0]==8 && u16(b,2)==0 && u16(b,4)==0 && u16(b,6)==328 && u16(b,8)==180,"split-stage replaces pending crop only");
            a.set(rg,"rendererViewportRefreshPending",Boolean.FALSE);
            a.set(rg,"infoPresentationRefreshPending",Boolean.FALSE);
            ClusterLayerController.ViewportListener listener=(ClusterLayerController.ViewportListener)a.get(rg,"viewportListener");
            listener.onManeuverViewportChanged();
            check(((Boolean)a.get(rg,"rendererViewportRefreshPending")).booleanValue()
                && !((Boolean)a.get(rg,"infoPresentationRefreshPending")).booleanValue(),"layout-only change updates crop without resetting route info");
            for(int i=0;i<40;i++) check(a.renderer.sendProgress(i%17,1,RendererServer.PROGRESS_FILL),"ordinary state enqueue");
            Object[] q=(Object[])a.get(a.renderer,"writeQueue");
            int n=0;
            for(Object p:q) if(p!=null && ((byte[])a.get(p,"packet"))[0]==8) n++;
            check(n==1,"queue pressure retains the current crop");
            clear(a);
            check(a.bridge.refreshRendererViewport() && count(a)==1,"reconnect/queue reset replays same crop");
            clear(a);
            for(int i=0;i<32;i++) check(a.renderer.sendClear(),"barrier enqueue");
            check(!a.bridge.refreshRendererViewport(),"barrier-only backpressure is reported");
            clear(a);
            check(a.bridge.refreshRendererViewport(),"failed crop enqueue remains retryable");
            check(!a.renderer.sendVisibleArea(-1,0,328,180),"invalid unsigned wire coordinate rejected");
        } finally { stage.setInt(null,saved); }
        System.out.println("RendererViewportTest: stage change, wire geometry, coalescing, reconnect and backpressure PASS");
    }
}
