package com.luka.carplay.cursor;
import com.luka.carplay.startup.*;
import com.luka.carplay.bus.CarplayBus;
import de.audi.atip.base.IFrameworkAccess;
import de.audi.atip.hmi.IHMIServiceApp;
import de.audi.atip.hmi.modelaccess.ChoiceModelApp;

public final class HuStartupTest {
    static int checks;
    static void eq(int a, int b) { checks++; if (a!=b) throw new AssertionError(a + " != " + b); }
    static void ok(boolean b) { checks++; if (!b) throw new AssertionError("check " + checks); }
    static final class Model implements ChoiceModelApp {
        int value, writes; public int getValue() { return value; }
        public void setValue(int v) { value=v; writes++; }
    }
    static final class Framework implements IFrameworkAccess, IHMIServiceApp {
        long now=1000;
        final Model identity=new Model(), selected=new Model(), open=new Model();
        public int getKombiType() { return 0; }
        public long getMonotonicTime() { return now; }
        public IHMIServiceApp getHmiServiceApp() { return this; }
        public ChoiceModelApp getChoiceModel(int id) { return id==4239 ? identity : id==138 ? selected : open; }
    }
    static byte[] ready() {
        byte[] p=new byte[12]; System.arraycopy(CarplayBus.getInstance().lastQuery,0,p,0,4);
        p[7]=1; p[11]=2; return p;
    }
    public static void main(String[] args) {
        MenuIdentityState s=new MenuIdentityState();
        s.stock(1,1); eq(s.value(),1); // baseline, feature has never taken ownership
        s.begin(1); s.stock(1,1); eq(s.value(),1); eq(s.selection(18),16);
        s.shown(0); eq(s.value(),1); s.shown(1); eq(s.value(),1); eq(s.selection(18),16);
        s.stock(0,1); eq(s.value(),1); // stock update cannot flash generic during loader
        s.end(1); eq(s.value(),0); s.stock(1,1); eq(s.value(),0); s.shown(1); eq(s.value(),0);
        s.begin(2); s.shown(2); s.end(1); eq(s.value(),1); // old teardown
        s.stock(2,2); eq(s.value(),2); s.stock(0,1); eq(s.value(),2); eq(s.selection(16),17);
        s.end(2); eq(s.value(),2); s.stock(0,2); eq(s.value(),0);
        s.begin(3); s.stock(1,1); s.fallback(3); eq(s.value(),1); eq(s.selection(8),8);

        Framework fw=new Framework(); fw.selected.value=18;
        HuMenuIdentity.bind(fw); HuStartup loader=new HuStartup();
        HuCursor.Frame geometry=new HuCursor.Frame(0,0,100,20,800,440,1024,480,0,CursorWire.SHOW,0,0,0);
        loader.begin(10); HuMenuIdentity.stockIdentity(fw.identity,1,1);
        HuCursor.Frame first=loader.frame(fw,1000,0,geometry,true);
        ok(first!=null && first.mode==CursorWire.LOADER); eq(fw.identity.value,1); eq(fw.open.writes,0);
        eq(first.offsetX,0); eq(first.offsetY,0); eq(first.width,1024); eq(first.height,480);
        // Stock identity is retained, but our early HMI request still requires a posted frame.
        loader.frame(fw,3200000,78,geometry,true); eq(fw.identity.value,1);
        loader.posted(first,false); fw.now+=33;
        loader.frame(fw,1000,0,geometry,true); eq(fw.open.writes,1); eq(fw.identity.value,1);
        loader.frame(fw,1000,0,geometry,true); eq(fw.open.writes,1);
        fw.now+=33; loader.frame(fw,3200000,78,geometry,true);
        eq(fw.identity.value,1); eq(fw.selected.value,16);
        HuMenuIdentity.stockIdentity(fw.identity,0,1); eq(fw.identity.value,1);
        HuMenuIdentity.stockSelection(fw.selected,18); eq(fw.selected.value,16);
        fw.selected.value=8; HuMenuIdentity.stockSelection(fw.selected,18); eq(fw.selected.value,8);
        byte[] reply=ready(); loader.onFrame(0x31,2,reply,reply.length);
        fw.now+=33; first=loader.frame(fw,3200000,78,geometry,true); eq(first.flags,1);
        // Delayed native completion cannot be guessed from elapsed Java time.
        for (int i=0;i<20;i++) {
            fw.now+=100; first=loader.frame(fw,3200000,78,geometry,true);
            ok(first!=null && first.flags==1 && loader.running());
            loader.onFrame(0x31,2,ready(),12);
        }
        loader.posted(first,true); ok(!loader.running()); eq(fw.identity.value,1);
        loader.end(); eq(fw.identity.value,0);
        HuMenuIdentity.stockIdentity(fw.identity,1,1); eq(fw.identity.value,0);

        loader.begin(11); fw.now+=200; first=loader.frame(fw,1000,0,geometry,true);
        loader.onFrame(0x31,2,reply,reply.length); // previous request nonce
        loader.posted(first,false); loader.frame(fw,3200000,78,geometry,true);
        fw.now+=300; first=loader.frame(fw,3200000,78,geometry,true); ok(first!=null && first.flags==0);
        int opened=fw.open.writes;
        ok(loader.frame(fw,1000,0,geometry,true)==null); ok(!loader.running());
        fw.now+=300; loader.frame(fw,1000,0,geometry,true); eq(fw.open.writes,opened); // HOME wins
        loader.end();
        loader.begin(12); fw.now+=200; first=loader.frame(fw,1000,0,geometry,true); loader.posted(first,false);
        loader.frame(fw,1000,0,geometry,true);
        HuMenuIdentity.stockIdentity(fw.identity,1,1);
        for (int i=0;i<13;i++) { fw.now+=200; byte[] pending=ready(); pending[11]=1;
            loader.onFrame(0x31,2,pending,12); loader.frame(fw,1000,0,geometry,true); }
        ok(!loader.running()); eq(fw.identity.value,1); // fallback
        loader.end(); eq(fw.identity.value,0);
        // A stale ACK/expired native lease cannot request the screen.
        loader.begin(13); fw.now+=200; first=loader.frame(fw,1000,0,geometry,false);
        loader.posted(first,false); opened=fw.open.writes;
        loader.frame(fw,1000,0,geometry,false); eq(fw.open.writes,opened);
        loader.frame(fw,1000,0,geometry,true); eq(fw.open.writes,opened+1);
        ok(loader.frame(fw,3200000,78,geometry,false)==null); ok(!loader.running()); loader.end();

        // Missing observer, including an old hook, releases the scene in 1s.
        loader.begin(14); fw.now+=200; loader.frame(fw,1000,0,geometry,false);
        fw.now+=1001; ok(loader.frame(fw,1000,0,geometry,false)==null); ok(!loader.running()); loader.end();

        // User navigation before adoption wins; no delayed auto-entry from an old frame.
        loader.begin(15); fw.now+=200; first=loader.frame(fw,1000,0,geometry,false);
        loader.posted(first,false); opened=fw.open.writes;
        ok(loader.frame(fw,2000,0,geometry,true)==null); eq(fw.open.writes,opened); loader.end();

        // DSI geometry can arrive after activation.
        HuCursor.Frame empty=new HuCursor.Frame(0,0,0,0,0,0,0,0,0,CursorWire.SHOW,0,0,0);
        loader.begin(16); fw.now+=200; ok(loader.frame(fw,1000,0,empty,false)==null); ok(loader.running());
        fw.now+=100; ok(loader.frame(fw,1000,0,geometry,false)!=null); loader.end();

        // Continue observing while native reveals video. A native reconfigure cancels old readiness.
        loader.begin(17); fw.now+=200; first=loader.frame(fw,1000,0,geometry,true); loader.posted(first,false);
        loader.onFrame(0x31,2,ready(),12);
        loader.frame(fw,3200000,78,geometry,true); fw.now+=200;
        first=loader.frame(fw,3200000,78,geometry,true); ok(first.flags==1);
        byte[] reset=ready(); reset[7]=2; reset[11]=1; loader.onFrame(0x31,2,reset,12);
        fw.now+=33; first=loader.frame(fw,3200000,78,geometry,true); ok(first.flags==0);
        loader.onFrame(0x31,2,ready(),12); // duplicate response cannot re-arm video
        fw.now+=33; first=loader.frame(fw,3200000,78,geometry,true); ok(first.flags==0); loader.end();
        System.out.println("HuStartupTest: " + checks + " ownership/ACK/context/native-completion/disconnect/late-event checks PASS");
    }
}
