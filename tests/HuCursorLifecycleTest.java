package com.luka.carplay.cursor;

import com.luka.carplay.core.CarPlayApp;
import com.luka.carplay.framework.Log;
import java.lang.reflect.Field;

/** Shipping owner/overlay/transport, with only the external app lifecycle stubbed. */
public final class HuCursorLifecycleTest {
    static int checks;
    static void check(boolean b, String why) { checks++; if (!b) throw new AssertionError(why); }
    static Field field(Object o, String name) throws Exception {
        Field f=o.getClass().getDeclaredField(name); f.setAccessible(true); return f;
    }
    static Object get(Object o, String name) throws Exception { return field(o,name).get(o); }
    static void set(Object o, String name, Object value) throws Exception { field(o,name).set(o,value); }
    static void ack(NativeCursorOverlay overlay, HuCursor.Frame frame, int status) throws Exception {
        CursorTransport transport=(CursorTransport)get(overlay,"transport");
        set(transport,"connection",7); set(overlay,"connection",7);
        set(overlay,"inFlight",42); set(overlay,"inFlightMode",frame.mode);
        set(overlay,"sentAt",System.currentTimeMillis()); set(overlay,"sentFrame",frame);
        byte[] ack=new byte[CursorWire.ACK_SIZE]; ack[3]=42;
        for (int i=0;i<4;i++) ack[4+i]=(byte)(status >>> (24-8*i));
        ack[11]=99; ack[14]=1; ack[15]=(byte)244;
        ack[19]=(byte)frame.mode; ack[26]=1; ack[27]=(byte)244;
        ack[21]=77; ack[23]=88;
        for (int i=0;i<4;i++) ack[28+i]=(byte)(frame.revision >>> (24-8*i));
        overlay.cursorAck(7,ack);
    }
    public static void main(String[] args) throws Exception {
        Log.setLevel(-1);
        HuCursor owner=HuCursor.getInstance();
        NativeCursorOverlay overlay=(NativeCursorOverlay)get(owner,"overlay");
        owner.activate(10);
        HuCursor.Frame old=owner.frame(); check(old.generation==10,"frame carries lifecycle generation");
        CarPlayApp.generation=11; owner.activate(11);
        check(!owner.current(old),"old frame cannot republish after activation");
        owner.deactivateSession(10); check(owner.starting(),"old deactivation cannot cancel new loader");
        owner.activate(10); check(owner.frame().generation==11,"stale activation rejected");
        owner.rejected(old,-1); check(!((Boolean)get(owner,"failed")),"old native failure cannot poison new session");
        owner.rendered(old, 0, 0); check(!((Boolean)get(owner,"rendered")),"old cursor ACK cannot enable clicking");
        overlay.stop(); // No socket worker races while ACKs are delivered deterministically.
        HuCursor.Frame loader=new HuCursor.Frame(0,0,0,0,1024,480,1024,480,1,CursorWire.LOADER,11,0,0);
        ack(overlay,loader,0);
        check(overlay.loaderPresented(),"loader post has live readiness");
        check(!overlay.presented(),"loader ACK never means pointer is visible");
        owner.onRotary(); check(overlay.loaderPresented() && owner.starting(),"rotary cursor hide does not tear down PRISM startup");
        check(!((Boolean)get(owner,"rendered")),"loader ACK cannot arm pointer coordinates");
        ack(overlay,loader,-2); check(!overlay.loaderPresented(),"revoked token clears loader readiness");
        ack(overlay,loader,0); set(overlay,"tokenUntil",System.currentTimeMillis()-1);
        check(!overlay.loaderPresented(),"expired native lease cannot open HMI screen");
        set(overlay,"tokenUntil",System.currentTimeMillis()+500); set(overlay,"token",0);
        check(!overlay.loaderPresented(),"zero native token is not a presentation");
        ack(overlay,old,-1); check(!((Boolean)get(owner,"failed")),"delayed error callback is generation gated");
        ((HuStartup)get(owner,"startup")).end();
        owner.configure(1024,480,1024,0,0,1024,480,null); overlay.stop();
        PointerState input=(PointerState)get(owner,"state"); input.touch(10,10);
        HuCursor.Frame pointer=owner.frame(); ack(overlay,pointer,0);
        check(((Boolean)get(owner,"rendered")) && overlay.presented(),"native visible ACK arms click");
        check(((Integer)get(owner,"drawnX"))==77 && ((Integer)get(owner,"drawnY"))==88,
            "click uses native confirmed coordinates, not requested target");
        set(overlay,"pointerUntil",System.currentTimeMillis()-1);
        check(!overlay.presented(),"native fade deadline expires click before the connection lease");
        owner.hideForStockInput(); ack(overlay,pointer,0);
        check(!((Boolean)get(owner,"rendered")),"late visible ACK cannot re-arm after stock input");
        input.end(); input.touch(20,20); pointer=owner.frame(); ack(overlay,pointer,0);
        check(((Boolean)get(owner,"rendered")),"new gesture can present again");
        owner.transportReset(); ack(overlay,pointer,0);
        check(input.input()==CursorWire.DISMISS && !((Boolean)get(owner,"rendered")),
            "reconnection retires old activity and delayed coordinates");
        input.end(); input.touch(30,30); pointer=owner.frame(); ack(overlay,pointer,0);
        owner.onRotary();
        check(input.input()==CursorWire.FORCE_HIDE && !overlay.presented(),"rotary immediately revokes native click/presentation lease");
        ack(overlay,pointer,0); check(!((Boolean)get(owner,"rendered")),"late ACK after rotary cannot restore clicking");
        CarPlayApp.active=false; owner.deactivateSession(11);
        check(!owner.starting() && !overlay.presented() && !overlay.loaderPresented(),"deactivate revokes both modes");
        System.out.println("HuCursorLifecycleTest: " + checks + " shipping owner/ACK/generation/lease checks PASS");
    }
}
