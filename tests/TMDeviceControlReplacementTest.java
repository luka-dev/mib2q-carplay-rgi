import java.util.*;
import java.util.jar.*;
import java.io.*;
import org.objectweb.asm.*;
import org.objectweb.asm.tree.*;
import org.objectweb.asm.tree.analysis.*;

/** Audit full source replacement: all 30 classes, stock ABI and inflator constructors. */
public final class TMDeviceControlReplacementTest {
    static final String PREFIX = "de/audi/app/terminalmode/device/TMDeviceControl";
    static ClassNode read(byte[] b) {
        ClassNode c = new ClassNode(); new ClassReader(b).accept(c, ClassReader.SKIP_DEBUG); return c;
    }
    static byte[] bytes(InputStream in) throws IOException {
        try (InputStream stream = in) {
            ByteArrayOutputStream out = new ByteArrayOutputStream(); byte[] b = new byte[4096]; int n;
            while ((n = stream.read(b)) >= 0) out.write(b, 0, n);
            return out.toByteArray();
        }
    }
    static Map<String,ClassNode> classes(String path) throws IOException {
        Map<String,ClassNode> result = new TreeMap<>();
        try (JarFile jar = new JarFile(path)) {
            for (Enumeration<JarEntry> e = jar.entries(); e.hasMoreElements();) {
                JarEntry entry = e.nextElement();
                if (entry.getName().startsWith(PREFIX) && entry.getName().endsWith(".class")) {
                    ClassNode c = read(bytes(jar.getInputStream(entry))); result.put(c.name, c);
                }
            }
        }
        return result;
    }
    static byte[] withoutMenu(ClassNode c) {
        c.methods.removeIf(m -> m.name.equals("updateMainWizardModel") && m.desc.equals("(ZI)V"));
        ClassWriter w = new ClassWriter(0); c.accept(w); return w.toByteArray();
    }
    static Set<String> abi(ClassNode c) {
        Set<String> result = new TreeSet<>();
        for (MethodNode m : c.methods) if (!m.name.startsWith("access$") && !m.name.startsWith("class$")
                && !m.name.equals("<clinit>")) result.add(m.name + m.desc);
        return result;
    }
    public static void main(String[] args) throws Exception {
        Map<String,ClassNode> stock = classes(args[0]), replacement = classes(args[1]), reference = classes(args[2]);
        if (stock.size() != 30 || !stock.keySet().equals(replacement.keySet())) throw new AssertionError("stock class family changed");
        int states = 0, methods = 0, calls = 0;
        for (String name : stock.keySet()) {
            ClassNode original = stock.get(name), current = replacement.get(name);
            if (current.version != Opcodes.V1_4 || !original.superName.equals(current.superName)
                    || !original.interfaces.equals(current.interfaces) || !abi(original).equals(abi(current)))
                throw new AssertionError("stock ABI changed: " + name + "\n" + abi(original) + "\n" + abi(current));
            if (name.contains("$SM$") && !name.matches(".*\\$[0-9]+$")) {
                String parent = name.substring(0, name.lastIndexOf('$'));
                if (!abi(current).contains("<init>(L" + parent + ";)V")) throw new AssertionError("inflator parent: " + name);
                states++;
            }
            for (MethodNode m : current.methods) {
                new Analyzer<BasicValue>(new BasicVerifier()).analyze(name, m); methods++;
                for (AbstractInsnNode n : m.instructions) {
                    if (n instanceof LdcInsnNode && "isNativeLittleEndian".equals(((LdcInsnNode)n).cst))
                        throw new AssertionError("unrecovered JXE empty string");
                    if (n instanceof MethodInsnNode && ((MethodInsnNode)n).owner.equals("com/luka/carplay/startup/HuMenuIdentity")) {
                        if (!m.name.equals("updateMainWizardModel")) throw new AssertionError("UI hook escaped menu method");
                        calls++;
                    }
                }
            }
            if (!Arrays.equals(withoutMenu(reference.get(name)), withoutMenu(current)))
                throw new AssertionError("changed code outside menu: " + name);
        }
        if (calls != 2) throw new AssertionError("menu hook calls=" + calls);
        System.out.println("TMDeviceControlReplacementTest: 30 complete classes, " + states + " state constructors, "
            + methods + " methods verified; only menu differs from recovered stock PASS");
    }
}
