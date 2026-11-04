import com.luka.carplay.connectivity.CarPlaySettingsUi;
import de.audi.atip.base.IFrameworkAccess;
import de.audi.atip.hmi.HMIService;
import de.audi.atip.hmi.event.EventDispatcher;
import de.audi.atip.hmi.event.RunnableEvent;
import de.audi.atip.hmi.model.AbstractModelBank;
import de.audi.atip.hmi.model.ChoiceModel;
import de.audi.atip.hmi.view.Screen;
import de.audi.atip.log.LogChannel;
import de.audi.atip.log.LogChannelFactory;
import de.esolutions.hmi.widgets.audi.base.AbstractWidget;
import de.esolutions.hmi.widgets.audi.base.eal.IWrappedFont;
import de.esolutions.hmi.widgets.audi.evo.ScreenWidgetEVO;
import de.esolutions.hmi.widgets.audi.evo.widgets.ContainerController;
import de.esolutions.hmi.widgets.audi.evo.widgets.menu.MenuController;
import de.esolutions.hmi.widgets.audi.evo.widgets.menu.MenuItemController;
import de.esolutions.hmi.widgets.audi.evo.widgets.menu.MenuItemIndex;
import java.lang.reflect.*;
import java.util.ArrayList;
import java.util.List;

/** Real shipping screen builder/models and stock CN/US/EU widgets, without native graphics.
 * The framework event queue and fonts are substituted; no installed settings are written. */
public final class CarPlaySettingsAvailabilityTest {
    static final Class<?> UI = CarPlaySettingsUi.class;
    static final LogChannel LOG = new LogChannel() {
        public void log(int a,String b,Object c,Object d,Object e,Object f,long g,long h,long i,int j,Throwable k) {}
        public void log(int a,int b,Object c,Object d,Object e,Object f,long g,long h,long i,int j,Throwable k) {}
    };
    static final List<RunnableEvent> events = new ArrayList<RunnableEvent>();
    static boolean onHmiThread;
    static IFrameworkAccess framework;
    static ChoiceModel spatial, upmix;
    static void check(boolean b, String message) { if (!b) throw new AssertionError(message); }
    static Field field(Class<?> c, String name) throws Exception {
        while (c != null) {
            try { Field f=c.getDeclaredField(name); f.setAccessible(true); return f; }
            catch (NoSuchFieldException e) { c=c.getSuperclass(); }
        }
        throw new NoSuchFieldException(name);
    }
    static void put(Class<?> c,Object target,String name,Object value) throws Exception { field(c,name).set(target,value); }
    static Object invoke(Object target, String name, Class<?>[] types, Object... args) throws Exception {
        Method m=(target == null ? UI : target.getClass()).getDeclaredMethod(name,types);
        m.setAccessible(true); return m.invoke(target,args);
    }
    @SuppressWarnings("unchecked")
    static <T> T proxy(Class<T> c, InvocationHandler h) {
        return (T)Proxy.newProxyInstance(c.getClassLoader(),new Class[]{c},h);
    }
    static void setup() throws Exception {
        EventDispatcher dispatcher=proxy(EventDispatcher.class,(o,m,a)-> {
            if (m.getName().equals("isDispatchThread")) return onHmiThread;
            if (m.getName().equals("postEvent")) { events.add((RunnableEvent)a[0]); return null; }
            throw new AssertionError(m.getName());
        });
        HMIService hmi=proxy(HMIService.class,(o,m,a)-> {
            if (m.getName().equals("getEventDispatcher")) return dispatcher;
            throw new AssertionError(m.getName());
        });
        framework=proxy(IFrameworkAccess.class,(o,m,a)-> {
            if (m.getName().equals("getLogChannel")) return LOG;
            if (m.getName().equals("getHMIService")) return hmi;
            if (m.getName().equals("getLanguageMgr")) return proxy(de.audi.atip.i18n.ILanguageManager.class,(x,method,args)->
                new de.audi.atip.i18n.Language(-1,"en_US","en","English",0,java.util.Locale.US));
            if (m.getReturnType()==int.class) return 1;
            if (m.getReturnType()==boolean.class) return false;
            throw new AssertionError(m.getName());
        });
        AbstractWidget.setFrameworkAccess(framework);
        AbstractModelBank.initLogging(proxy(LogChannelFactory.class,(o,m,a)->LOG));
        // Rendering is outside this host test; the actual screen builder still runs.
        IWrappedFont[][] fonts=new IWrappedFont[7][];
        for(int i=0;i<fonts.length;i++) fonts[i]=new IWrappedFont[0];
        put(Class.forName("de.audi.tghu.connectivity.hmi.evohigh.FontFactory"),null,"fontArrays",fonts);
        put(UI,null,"fw",framework);
        put(UI,null,"pendingSpatial",1);
        put(UI,null,"pendingUpmixChoice",2);
        Object bank=CarPlaySettingsUi.newBank();
        invoke(bank,"createModel",new Class[]{int.class},500);
        invoke(bank,"createModel",new Class[]{int.class},501);
        spatial=(ChoiceModel)field(UI,"spatialModel").get(null);
        upmix=(ChoiceModel)field(UI,"upmixModel").get(null);
    }
    static final class Fixture {
        final MenuController menu=new MenuController();
        final ScreenWidgetEVO screen=new ScreenWidgetEVO(2500140);
        final MenuItemController audio, mix;
        Fixture(int amp) throws Exception {
            put(UI,null,"amplifier",amp);
            ContainerController main=new ContainerController(); main.add(menu); screen.add(main,1);
            Object factory=CarPlaySettingsUi.newFactory(framework);
            invoke(factory,"buildCarPlaySettingsScreen",new Class[]{Screen.class,int.class},screen,0);
            audio=(MenuItemController)screen.findViews(2500500)[0];
            mix=(MenuItemController)screen.findViews(2500501)[0];
            check(menu.getChildrenSize()==7,"all seven settings must exist for amp "+amp);
            put(MenuController.class,menu,"focusedIndex",new MenuItemIndex(0,0));
        }
        void verify(boolean enabled) throws Exception {
            check(audio.isEnabled()==enabled && mix.isEnabled()==enabled,"wrong native enabled state");
            check(menu.getChildrenSize()==7 && menu.getChild(0)==audio && menu.getChild(1)==mix,"rows removed/reordered");
            check(audio.getParent()==menu && mix.getParent()==menu,"row detached");
            check(menu.hasFocusedItem() && menu.getFocusedIndex().equals(new MenuItemIndex(0,0)),"focus lost");
            check(screen.findViews(2500500)[0]==audio && screen.findViews(2500501)[0]==mix,"bindings changed");
            check(spatial.getValue()==1 && upmix.getValue()==2,"saved values overwritten");
        }
    }
    static void notifyAmp(int amp) throws Exception {
        put(UI,null,"amplifier",amp);
        invoke(null,"requestAudioRowsEnabledState",new Class[0]);
    }
    static void drain() {
        onHmiThread=true;
        try { while(!events.isEmpty()) events.remove(0).dispatch(); }
        finally { onHmiThread=false; }
    }
    public static void main(String[] args) throws Exception {
        setup();
        // Early detection, including repeated screen construction, keeps the same models.
        for(int amp:new int[]{-1,0,1,2,3,4,5,17,21}) new Fixture(amp).verify(amp>3);
        Fixture f=new Fixture(-1); f.verify(false);
        notifyAmp(21); f.verify(false); check(events.size()==1,"callback bypassed HMI queue");
        drain(); f.verify(true);
        notifyAmp(3); f.verify(true); drain(); f.verify(false);
        // Stale queued notifications must not re-enable after an internal/unknown result.
        notifyAmp(21); notifyAmp(-1); drain(); f.verify(false);
        notifyAmp(3); notifyAmp(21); drain(); f.verify(true);
        // Native disabled state must also reject a selection already queued before disable.
        notifyAmp(3);
        spatial.itemSelected(0,0); upmix.itemSelected(0,0);
        drain(); f.verify(false);
        onHmiThread=true;
        try { notifyAmp(21); check(events.isEmpty(),"HMI callback unnecessarily queued"); f.verify(true); }
        finally { onHmiThread=false; }
        new Fixture(3).verify(false);
        new Fixture(21).verify(true);
        System.out.println("CarPlaySettingsAvailabilityTest: PASS (native disabled, saved values, stable focus/bindings, HMI queue, late/repeated detection)");
    }
}
