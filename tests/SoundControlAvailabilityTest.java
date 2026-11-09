import com.luka.carplay.tone.SoundControlUi;
import de.audi.atip.base.IFrameworkAccess;
import de.audi.atip.hmi.HMIService;
import de.audi.atip.hmi.event.EventDispatcher;
import de.audi.atip.hmi.event.RunnableEvent;
import de.audi.atip.hmi.model.AbstractModelBank;
import de.audi.atip.hmi.model.ChoiceModel;
import de.audi.atip.hmi.view.HMIView;
import de.audi.atip.hmi.view.Screen;
import de.audi.atip.log.LogChannel;
import de.audi.atip.log.LogChannelFactory;
import de.audi.atip.model.ICoreToneModelBank;
import de.esolutions.hmi.widgets.audi.base.AbstractWidget;
import de.esolutions.hmi.widgets.audi.evo.ScreenWidgetEVO;
import de.esolutions.hmi.widgets.audi.evo.high.widgets.CompositeRendererHigh;
import de.esolutions.hmi.widgets.audi.evo.widgets.ContainerController;
import de.esolutions.hmi.widgets.audi.evo.widgets.menu.MenuController;
import de.esolutions.hmi.widgets.audi.evo.widgets.menu.MenuItemController;
import java.lang.reflect.*;
import java.util.ArrayList;
import java.util.List;

/** Shipping tone builder/models and real US/EU/CN widgets; only the event loop and
 * framework services are substituted. No native graphics or installed settings. */
public final class SoundControlAvailabilityTest {
    static final Class<?> UI = SoundControlUi.class;
    static final List<RunnableEvent> events = new ArrayList<RunnableEvent>();
    static boolean onHmiThread;
    static IFrameworkAccess framework;
    static Object factory;
    static ChoiceModel surround, threeD, stockBlock, stockIdle;
    static int checks;
    static final LogChannel LOG = new LogChannel() {
        public void log(int a,String b,Object c,Object d,Object e,Object f,long g,long h,long i,int j,Throwable k) {}
        public void log(int a,int b,Object c,Object d,Object e,Object f,long g,long h,long i,int j,Throwable k) {}
    };
    static void check(boolean b,String message) { checks++; if (!b) throw new AssertionError(message); }
    static Field field(Class<?> c,String name) throws Exception {
        while(c != null) {
            try { Field f=c.getDeclaredField(name); f.setAccessible(true); return f; }
            catch(NoSuchFieldException e) { c=c.getSuperclass(); }
        }
        throw new NoSuchFieldException(name);
    }
    static Object call(Object target,String name,Class<?>[] types,Object... args) throws Exception {
        Method m=(target == null ? UI : target.getClass()).getDeclaredMethod(name,types);
        m.setAccessible(true); return m.invoke(target,args);
    }
    @SuppressWarnings("unchecked")
    static <T> T proxy(Class<T> c,InvocationHandler h) {
        return (T)Proxy.newProxyInstance(c.getClassLoader(),new Class[]{c},h);
    }
    static void setup() throws Exception {
        EventDispatcher dispatcher=proxy(EventDispatcher.class,(o,m,a)-> {
            if(m.getName().equals("isDispatchThread")) return onHmiThread;
            if(m.getName().equals("postEvent")) { events.add((RunnableEvent)a[0]); return null; }
            throw new AssertionError(m.getName());
        });
        HMIService hmi=proxy(HMIService.class,(o,m,a)-> {
            if(m.getName().equals("getEventDispatcher")) return dispatcher;
            if(m.getName().equals("getModel")) return ((Integer)a[1])==4073 ? stockBlock : stockIdle;
            throw new AssertionError(m.getName());
        });
        framework=proxy(IFrameworkAccess.class,(o,m,a)-> {
            if(m.getName().equals("getLogChannel")) return LOG;
            if(m.getName().equals("getHMIService")) return hmi;
            if(m.getName().equals("getLanguageMgr")) return proxy(de.audi.atip.i18n.ILanguageManager.class,(x,method,args)->
                new de.audi.atip.i18n.Language(-1,"en_US","en","English",0,java.util.Locale.US));
            if(m.getReturnType()==int.class) return 1;
            if(m.getReturnType()==boolean.class) return false;
            throw new AssertionError(m.getName());
        });
        AbstractWidget.setFrameworkAccess(framework);
        AbstractModelBank.initLogging(proxy(LogChannelFactory.class,(o,m,a)->LOG));
        stockBlock=new ChoiceModel(4073); stockBlock.setValue(0);
        stockIdle=new ChoiceModel(11); stockIdle.setValue(0);
        factory=SoundControlUi.newFactory(framework);
        Object bank=SoundControlUi.newBank();
        call(bank,"createModel",new Class[]{int.class},SoundControlUi.MODEL_SURROUND%100000);
        call(bank,"createModel",new Class[]{int.class},SoundControlUi.MODEL_THREE_D%100000);
        surround=(ChoiceModel)field(UI,"surroundModel").get(null);
        threeD=(ChoiceModel)field(UI,"threeDModel").get(null);
    }
    static final class Fixture {
        final MenuController menu=new MenuController();
        final ScreenWidgetEVO screen=new ScreenWidgetEVO(1000006);
        final MenuItemController level=new MenuItemController(), effect=new MenuItemController();
        final MenuItemController item;
        Fixture(int amp) throws Exception {
            SoundControlUi.updateAmplifier(amp); drain();
            ContainerController main=new ContainerController(); main.add(menu); screen.add(main,1);
            menu.add(new MenuItemController()); menu.add(level); menu.add(effect);
            effect.setRenderer(new CompositeRendererHigh(effect));
            screen.setViews(new int[]{ICoreToneModelBank.DSP_SOURROUND_RANGE,SoundControlUi.MODEL_THREE_D},
                new HMIView[][]{new HMIView[]{level},new HMIView[]{effect}});
            call(factory,"addSurroundRow",new Class[]{Screen.class,int.class},screen,0);
            item=(MenuItemController)screen.findViews(SoundControlUi.MODEL_SURROUND)[0];
        }
        void verify(boolean supported) {
            check(item.isVisible()==supported,"wrong visibility");
            check(item.isEnabled()==supported,"wrong availability");
            check(menu.getChildrenSize()==4 && menu.getChild(1)==item,"duplicate or reordered row");
            check(item.getParent()==menu && screen.findViews(SoundControlUi.MODEL_SURROUND)[0]==item,"lost binding");
        }
    }
    static void drain() {
        onHmiThread=true;
        try { while(!events.isEmpty()) events.remove(0).dispatch(); }
        finally { onHmiThread=false; }
    }
    public static void main(String[] args) throws Exception {
        setup();
        for(int amp:new int[]{-1,0,1,2,3,4,5,17,18,19,21,22,23}) new Fixture(amp).verify(amp>3);
        Fixture f=new Fixture(-1); f.verify(false);
        SoundControlUi.updateAmplifier(3); drain(); f.verify(false);
        SoundControlUi.updateAmplifier(21);
        f.verify(false); check(events.size()==1,"amplifier callback bypassed HMI queue");
        drain(); f.verify(true);
        surround.setValue(0); drain();
        check(!f.level.isEnabled() && !f.effect.isEnabled(),"external surround gate lost");
        threeD.setValue(2);
        SoundControlUi.updateAmplifier(3);
        // A selection already queued before detection must not change either model.
        surround.itemSelected(1,0);
        check(surround.getValue()==0 && threeD.getValue()==2,"unsupported selection changed model");
        threeD.setValue(0);
        check(threeD.getValue()==0,"internal 3D update incorrectly suppressed");
        drain(); f.verify(false);
        check(f.level.isEnabled() && f.effect.isEnabled(),"old surround overlay retained on internal amp");
        // With no overlay, stock-disabled controls must remain untouched.
        f.level.setEnabled(false); f.effect.setEnabled(false);
        SoundControlUi.updateAmplifier(3); drain();
        check(!f.level.isEnabled() && !f.effect.isEnabled(),"internal stock state overwritten");
        SoundControlUi.updateAmplifier(21); SoundControlUi.updateAmplifier(-1);
        drain(); f.verify(false);
        SoundControlUi.updateAmplifier(3); SoundControlUi.updateAmplifier(17);
        drain(); f.verify(true);
        onHmiThread=true;
        try { SoundControlUi.updateAmplifier(0); f.verify(false); check(events.isEmpty(),"HMI callback unnecessarily queued"); }
        finally { onHmiThread=false; }
        // Returning to external restores the saved off state, without duplicate insertion.
        SoundControlUi.updateAmplifier(4); drain(); f.verify(true);
        check(surround.getValue()==0 && !f.level.isEnabled(),"saved off state lost");
        surround.setValue(1); drain();
        stockBlock.setValue(1);
        call(factory,"executeCondition",new Class[]{int.class,int.class,HMIView[].class,int.class},
            SoundControlUi.MODEL_SURROUND,1000006,new HMIView[]{f.item},0);
        check(f.item.isVisible() && !f.item.isEnabled(),"stock restriction overridden");
        check(!f.level.isEnabled() && !f.effect.isEnabled(),"stock child restriction overridden");
        SoundControlUi.updateAmplifier(3); drain(); f.verify(false);
        check(!f.level.isEnabled() && !f.effect.isEnabled(),"overlay release ignored stock restriction");
        System.out.println("SoundControlAvailabilityTest: PASS ("+checks+" checks; late detection, HMI queue, stable bindings, stock controls, stale selection)");
    }
}
