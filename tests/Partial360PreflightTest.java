import de.audi.app.car.common.app.ICarApplication;
import de.audi.app.earlyfunc.core.parking.*;
import de.audi.app.earlyfunc.core.parking.ops.OPSViewModeHandler;
import de.audi.app.earlyfunc.core.parking.vps.AbstractParkingSystemVPSComponent;
import de.audi.app.earlyfunc.evo.parking.ParkingSystemVPSComponentEvo;
import de.audi.atip.base.IFrameworkAccess;
import de.audi.atip.displaymanager.*;
import de.audi.atip.hmi.IHMIServiceApp;
import de.audi.atip.hmi.modelaccess.ChoiceModelApp;
import de.audi.atip.interapp.displaymanager.Cropping;
import de.audi.atip.model.ICoreEarlyAppsModelBank;
import de.audi.tghu.fwhmi.DisplayManagerMIB2High;
import org.dsi.ifc.carparkingsystem.*;
import org.dsi.ifc.displaymanagement.*;
import org.objectweb.asm.*;
import org.objectweb.asm.tree.*;
import java.lang.reflect.*;
import java.util.*;

/** Characterization of actual MU1316 bytecode, not a partial-camera implementation.
 * Native DSI, HMI model endpoints and constructors that start native services are
 * replaced at the boundary. Expected limitations are explicit passing contracts. */
public final class Partial360PreflightTest {
    static int groups;
    static void check(boolean value, String why) { PdcResourcePolicyTest.check(value, why); }
    static void pass(String text) { groups++; System.out.println("  PASS " + text); }
    static Field field(Class<?> type, String name) throws Exception {
        for (Class<?> c = type; c != null; c = c.getSuperclass()) {
            try { Field f = c.getDeclaredField(name); f.setAccessible(true); return f; }
            catch (NoSuchFieldException ignored) { }
        }
        throw new NoSuchFieldException(type.getName() + "." + name);
    }
    static void put(Object target, String name, Object value) throws Exception {
        field(target.getClass(), name).set(target, value);
    }
    static Object get(Object target, String name) throws Exception {
        return field(target.getClass(), name).get(target);
    }
    static <T> T allocate(Class<T> type) throws Exception {
        return type.cast(PdcResourcePolicyTest.UNSAFE.allocateInstance(type));
    }
    static <T> T proxy(Class<T> type, PdcResourcePolicyTest.Call call) {
        return PdcResourcePolicyTest.proxy(type, call);
    }
    static MethodNode method(String type, String name) throws Exception {
        ClassNode node = new ClassNode(); new ClassReader(type).accept(node, 0);
        MethodNode found = null;
        for (MethodNode m : node.methods) if (m.name.equals(name)) {
            check(found == null, "ambiguous method " + type + "." + name); found = m;
        }
        check(found != null, "missing " + type + "." + name); return found;
    }
    static boolean contains(int[] values, int value) {
        for (int item : values) if (item == value) return true;
        return false;
    }
    static final Map<String,Map<String,Integer>> constantFields = new HashMap<>();
    static Integer literal(AbstractInsnNode node) throws Exception {
        // The final JAR's uninline pass restores named model constants; combined
        // retains integer literals. Resolve ConstantValue without class initialization.
        if (node.getOpcode() == Opcodes.GETSTATIC && node instanceof FieldInsnNode) {
            FieldInsnNode field = (FieldInsnNode)node;
            Map<String,Integer> constants = constantFields.get(field.owner);
            if (constants == null) {
                constants = new HashMap<>(); ClassNode owner = new ClassNode();
                new ClassReader(field.owner).accept(owner,ClassReader.SKIP_CODE);
                for (FieldNode f : owner.fields) if (f.value instanceof Integer)
                    constants.put(f.name,(Integer)f.value);
                constantFields.put(field.owner,constants);
            }
            return constants.get(field.name);
        }
        if (node instanceof IntInsnNode) return ((IntInsnNode)node).operand;
        if (node instanceof LdcInsnNode && ((LdcInsnNode)node).cst instanceof Integer)
            return (Integer)((LdcInsnNode)node).cst;
        int op = node.getOpcode();
        return op >= Opcodes.ICONST_M1 && op <= Opcodes.ICONST_5 ? op - Opcodes.ICONST_0 : null;
    }
    static void screenGeometry() throws Exception {
        String bag = "de.audi.tghu.earlyapps.hmi.evohigh.EarlyAppsScreenBag1";
        boolean rightBounds = false;
        int backgrounds = 0;
        for (AbstractInsnNode node : method(bag,"cARPOPUPPARKINGOPS").instructions) {
            if (Integer.valueOf(ICoreEarlyAppsModelBank.PARKING_OPS_BACKGROUND_CHOICE).equals(literal(node)))
                backgrounds++;
            if (!(node instanceof MethodInsnNode)) continue;
            MethodInsnNode call = (MethodInsnNode)node;
            if (!call.name.equals("setBounds") || !call.owner.endsWith("PartialPopupController")) continue;
            Integer[] bounds = new Integer[4]; AbstractInsnNode prev = node.getPrevious();
            for (int i = 3; i >= 0; i--) {
                while (prev != null && prev.getOpcode() < 0) prev = prev.getPrevious();
                check(prev != null,"missing partial bounds"); bounds[i] = literal(prev); prev = prev.getPrevious();
            }
            rightBounds |= Arrays.equals(bounds,new Integer[]{720,0,304,480});
        }
        check(rightBounds && backgrounds >= 2,"right OPS geometry/background wiring changed");
        boolean videoContext = false;
        for (AbstractInsnNode node : method(bag,"cARPOPUPPARKINGAPSOPSVPSRVC").instructions)
            videoContext |= Integer.valueOf(ICoreEarlyAppsModelBank.PARKING_VPS_VIDEO_CONTEXT_CHOICE).equals(literal(node));
        check(videoContext,"stock fullscreen parking video context wiring changed");
        pass("stock right OPS bounds 720,0,304,480; background and fullscreen video-context wiring");
    }
    static DisplayManagerMIB2High displayManager(final int kombi) throws Exception {
        DisplayManagerMIB2High dm = allocate(DisplayManagerMIB2High.class);
        put(dm, "framework", proxy(IFrameworkAccess.class, (name,args) -> {
            if (name.equals("getKombiType")) return kombi;
            if (name.equals("getSysConst")) return 2;
            if (name.equals("isSimulator")) return false;
            throw new AssertionError("unexpected framework " + name);
        }));
        put(dm, "log", PdcResourcePolicyTest.LOG);
        put(dm, "displayableExtents", new HashMap<Integer,int[]>());
        return dm;
    }
    static void contexts() throws Exception {
        for (int kombi : new int[]{0, 4}) {
            DisplayManagerMIB2High dm = displayManager(kombi);
            Method define = DisplayManagerMIB2High.class.getDeclaredMethod("defineContexts");
            define.setAccessible(true); define.invoke(dm);
            DisplayContext[] contexts = (DisplayContext[])get(dm, "dc");
            check(contexts.length == (kombi == 4 ? 158 : 82), "context allocation changed: revisit ID audit");
            check(Arrays.equals(contexts[3].getDisplayableList(), new int[]{16,17}), "stock RVC context changed");
            check(Arrays.equals(contexts[78].getDisplayableList(), new int[]{16,59}), "ordinary CarPlay changed");
            List<Integer> holes = new ArrayList<>();
            for (int i = 0; i < contexts.length; i++) {
                DisplayContext context = contexts[i];
                if (context == null) { holes.add(i); continue; }
                int[] list = context.getDisplayableList();
                check(!(contains(list,17) && contains(list,59)), "new mixed camera context: update preflight assumptions");
            }
            if (kombi == 4) {
                check(Arrays.equals(contexts[91].getDisplayableList(), new int[]{20,16,22,19}),
                    "G24 91 is no longer stock 12+79");
            } else check(contexts.length <= 91, "91 unexpectedly fits A5 table");
            System.out.println("    " + (kombi == 4 ? "G24" : "A5") + " unassigned slots (NOT permission to reuse): " + holes);
        }
        for (String name : new String[]{"switchContext", "processEvent"}) {
            MethodNode m = method("de.audi.tghu.fwhmi.DisplayManager", name);
            boolean modulo79 = false;
            for (AbstractInsnNode n : m.instructions) if (n.getOpcode() == Opcodes.IREM) {
                AbstractInsnNode prev = n.getPrevious();
                while (prev != null && prev.getOpcode() < 0) prev = prev.getPrevious();
                modulo79 |= prev instanceof IntInsnNode && ((IntInsnNode)prev).operand == 79;
            }
            check(modulo79, "callback normalization changed: " + name);
        }
        pass("context 91 rejected: outside A5, G24 collision; both stock callback paths use %79");
    }
    static void extentsAndExplicitCrop() throws Exception {
        DisplayManagerMIB2High dm = displayManager(0);
        List<String> calls = new ArrayList<>();
        put(dm, "dsiDispMgmt", proxy(DSIDisplayManagement.class, (name,args) -> {
            calls.add(name + Arrays.toString(args)); return null;
        }));
        put(dm, "initialized", true);
        check(dm.getExtends(17) == null, "cold extents unexpectedly synchronous");
        check(calls.equals(Arrays.asList("getExtents[17]")), "cold extents did not request DSI");
        int[] cached = {1024,480};
        ((Map<Integer,int[]>)get(dm,"displayableExtents")).put(17,cached);
        int[] borrowed = dm.getExtends(17);
        check(borrowed == cached && calls.size() == 1, "cached extents ownership changed");
        int[] snapshot = borrowed.clone(); cached[0] = 720;
        check(borrowed[0] == 720 && snapshot[0] == 1024, "snapshot must not alias cache");
        calls.clear();
        dm.setCropping(17,0,12,24,300,400,720,0,304,480);
        check(calls.equals(Arrays.asList("setCropping[0, 17, 12, 24, 300, 400, 720, 0, 304, 480]")),
            "HU crop argument order/routing changed: " + calls);
        check(Arrays.equals(dm.getPosition(17,0), new int[]{720,0}), "target position not cached");
        check(dm.getExtends(17)[0] == 720, "crop must not be mistaken for an extents update");
        calls.clear();
        dm.setCropping(17,1,0,0,100,200,3,4,50,100);
        check(calls.get(0).startsWith("setCropping[4, 17,"), "external cluster terminal must map to 4");
        calls.clear(); put(dm,"initialized",false);
        dm.setCropping(17,0,0,0,100,200,720,0,304,480);
        check(calls.isEmpty(), "uninitialized crop reached DSI");
        pass("cold/cache extents, borrowed array, explicit crop routing and uninitialized guard");
    }
    static void cameraServiceAndGenericCrop() throws Exception {
        DisplayManagerServiceImpl service = allocate(DisplayManagerServiceImpl.class);
        put(service,"logger",PdcResourcePolicyTest.LOG);
        put(service,"stateMutex",new Object());
        put(service,"state",new DisplayManagerState(59));
        List<String> calls = new ArrayList<>();
        IDSIDisplayManagerController controller = proxy(IDSIDisplayManagerController.class, (name,args) -> {
            calls.add(name + Arrays.toString(args)); return null;
        });
        put(service,"displayManagerController",controller);
        service.activateComponentRVC(17); service.deactivateComponentRVC(17);
        check(calls.equals(Arrays.asList("startComponent[17, 0, 0]", "stopComponent[17, 0, 0]")),
            "special RVC route changed: " + calls);
        check(service.getCurrentDisplayable() == 59, "RVC request unexpectedly changed generic state");
        Constructor<Cropping> ctor = Cropping.class.getDeclaredConstructor(int.class,int.class);
        ctor.setAccessible(true);
        Cropping crop = ctor.newInstance(1024,480).fixed(720,0,304,480);
        calls.clear();
        new SetCroppingCommand(service,PdcResourcePolicyTest.LOG,controller,crop,12,24,300,400,null).execute();
        check(calls.equals(Arrays.asList("setCropping[0, 59, 12, 24, 300, 400, 720, 0, 304, 480]")),
            "generic crop no longer uses generic displayable state");
        ClassNode node = new ClassNode(); new ClassReader(DisplayManagerServiceImpl.class.getName()).accept(node,0);
        int overloads = 0;
        for (MethodNode m : node.methods) if (m.name.equals("setCropping")) {
            overloads++;
            for (AbstractInsnNode n : m.instructions) if (n instanceof VarInsnNode)
                check(((VarInsnNode)n).var != 1, "first generic crop argument is now used: reassess API");
        }
        check(overloads == 2, "generic cropping overloads changed");
        pass("RVC start/stop do not change generic state; interapp crop can still target 59");
    }
    static void birdViewRequest() throws Exception {
        ParkingSystemVPSComponentEvo vps = allocate(ParkingSystemVPSComponentEvo.class);
        put(vps,"logChannel",PdcResourcePolicyTest.LOG);
        final DisplayContent[] current = {new DisplayContent(DSICarParkingSystem.POPUP_OPS,2,1,1)};
        List<DisplayContent> requests = new ArrayList<>();
        put(vps,"controller",proxy(IParkingSystemController.class, (name,args) -> {
            if (name.equals("getCurrentDisplayContent")) return current[0];
            if (name.equals("changeDisplayContent")) { requests.add((DisplayContent)args[0]); return null; }
            throw new AssertionError("unexpected parking controller " + name);
        }));
        Method change = AbstractParkingSystemVPSComponent.class.getDeclaredMethod("switchVPSView",int.class);
        change.setAccessible(true); change.invoke(vps,4);
        check(requests.size() == 1, "missing Bird View request");
        DisplayContent bird = requests.get(0);
        check(bird.view == DSICarParkingSystem.VPSCURRENTVIEW_BIRDVIEW && bird.mode == DSICarParkingSystem.VPSMODE_BIRDVIEW,
            "Bird View must be view=4 mode=9");
        check(bird.popup == DSICarParkingSystem.POPUP_OPS && bird.screen == 2,
            "switchVPSView changed popup/screen; reassess activation plan");
        check(current[0].view == 1 && current[0] != bird, "stock request mutated current content");
        current[0] = bird; change.invoke(vps,4); change.invoke(vps,99);
        check(requests.size() == 1, "duplicate/invalid view not ignored");
        for (int rawPopup : new int[]{2,3,8,12,13,14,16,18}) {
            ParkingPopupIdentifier id = vps.getHMIPopupID(rawPopup);
            check(id.getPopupType() == 0 && id.getHmiPopupID() == 2100000,
                "stock VPS no longer requests fullscreen popup");
        }
        PdcResourcePolicyTest.callsBefore(AbstractParkingSystemVPSComponent.class.getName(),"setActive",
            "setDisplayManagerComponentActive","addPopupRequest");
        PdcResourcePolicyTest.callsBefore(AbstractParkingSystemVPSComponent.class.getName(),"setActive",
            "addPopupRequest","notifyParkingSystemActive");
        pass("Bird View 4/9 preserves OPS popup; stock VPS source -> fullscreen popup -> active notification");
    }
    static final class Models {
        final Map<Integer,Integer> values = new HashMap<>();
        final ICarApplication app;
        Models() {
            IHMIServiceApp hmi = proxy(IHMIServiceApp.class, (name,args) -> {
                if (!name.equals("getChoiceModel")) throw new AssertionError("unexpected model API " + name);
                final int id = (Integer)args[0];
                return proxy(ChoiceModelApp.class, (n,a) -> {
                    if (n.equals("getValue")) return value(id);
                    if (n.equals("getID")) return id;
                    if (n.equals("setValue")) { values.put(id,(Integer)a[0]); return null; }
                    throw new AssertionError("unexpected choice " + n);
                });
            });
            IFrameworkAccess framework = proxy(IFrameworkAccess.class,(n,a) -> {
                if (n.equals("getHmiServiceApp")) return hmi;
                throw new AssertionError("unexpected framework " + n);
            });
            app = proxy(ICarApplication.class,(n,a) -> {
                if (n.equals("getFrameworkAccess")) return framework;
                throw new AssertionError("unexpected car app " + n);
            });
        }
        int value(int id) { Integer v = values.get(id); return v == null ? 0 : v; }
        void expect(int mode, int background) {
            check(value(ICoreEarlyAppsModelBank.PARKING_OPS_VIEW_MODE_CHOICE) == mode, "wrong OPS mode");
            check(value(ICoreEarlyAppsModelBank.PARKING_OPS_BACKGROUND_CHOICE) == background, "wrong OPS background");
        }
    }
    static void opsOverlayModels() throws Exception {
        for (int system : new int[]{0,1,2}) {
            Models models = new Models(); models.values.put(ICoreEarlyAppsModelBank.PARKING_VPS_SYSTEM_TYPE_CHOICE,system);
            OPSViewModeHandler ops = new OPSViewModeHandler(models.app,PdcResourcePolicyTest.LOG);
            put(ops,"vpsScreenID",2100001);
            ops.updateOPSActive(true); models.expect(2,3); // no VPS view
            ops.updateVPSView(4); models.expect(system == 0 ? 2 : 0,system == 0 ? 3 : 1);
            if (system == 2) {
                ops.updateVPSView(0); models.expect(1,1); // reduced, but stock screen is absent
                ops.notifyScreenConnected(999); models.expect(1,1);
                ops.notifyScreenConnected(2100001); models.expect(1,2);
                ops.notifyScreenFadedOut(2100001); models.expect(1,1);
                models.values.put(ICoreEarlyAppsModelBank.PARKING_VPS_TOPVIEW_AVAILABLE_CHOICE,1);
                ops.updateOPSViewMode(); models.expect(0,1); // topview unavailable
            }
        }
        pass("OPS background matrix: system type, topview support, view and actual screen connection");
    }
    public static void main(String[] args) throws Exception {
        contexts(); extentsAndExplicitCrop(); cameraServiceAndGenericCrop(); birdViewRequest(); opsOverlayModels(); screenGeometry();
        System.out.println("Partial 360 preflight: " + groups + " contract groups passed; native coexistence NOT tested");
    }
}
