import java.io.*;
import java.nio.file.*;
import java.security.MessageDigest;
import java.util.Arrays;
import java.util.jar.*;
import org.objectweb.asm.*;
import org.objectweb.asm.tree.*;

/** Two narrow inserts into pinned stock classes. No regenerated FSM/source or
 * consent bypass: the ordinary stock Yes/No, checkbox and persistence run on. */
public final class TransportDevicePatch {
    static final String ROOT = "de/audi/app/terminalmode/";
    static final String POLICY = "com/luka/carplay/core/TransportDevicePolicy";
    static final String CONTEXT = "L" + ROOT + "IContext;";
    static final String DEVICE = "L" + ROOT + "device/TMDevice;";
    static final String LIST = "Lde/audi/atip/utils/generics/GList;";
    static final String DSI = "Lorg/dsi/ifc/smartphoneintegration/Device;";

    static byte[] read(JarFile jar, String name) throws IOException {
        try (InputStream in = jar.getInputStream(jar.getJarEntry(name + ".class"));
             ByteArrayOutputStream out = new ByteArrayOutputStream()) {
            byte[] chunk = new byte[8192]; int n;
            while ((n = in.read(chunk)) != -1) out.write(chunk, 0, n);
            return out.toByteArray();
        }
    }
    static String sha(byte[] bytes) throws Exception {
        StringBuilder s = new StringBuilder();
        for (byte b : MessageDigest.getInstance("SHA-256").digest(bytes)) s.append(String.format("%02x", b & 255));
        return s.toString();
    }
    static void require(boolean condition, String message) {
        if (!condition) throw new IllegalArgumentException(message);
    }
    static InsnList prepare(String owner, String method, String arg) {
        InsnList code = new InsnList();
        code.add(new VarInsnNode(Opcodes.ALOAD, 0));
        code.add(new FieldInsnNode(Opcodes.GETFIELD, owner, "context", CONTEXT));
        code.add(new VarInsnNode(Opcodes.ALOAD, 1));
        code.add(new MethodInsnNode(Opcodes.INVOKESTATIC, POLICY, method,
                                   "(" + CONTEXT + arg + ")V", false));
        return code;
    }
    static byte[] canonical(ClassNode c) {
        ClassWriter writer = new ClassWriter(0); c.accept(writer); return writer.toByteArray();
    }
    static void verifyPreserved(byte[] input, byte[] output, int expected) {
        ClassNode original = new ClassNode(), changed = new ClassNode();
        new ClassReader(input).accept(original, 0); new ClassReader(output).accept(changed, 0);
        int removed = 0;
        for (MethodNode m : changed.methods) {
            for (AbstractInsnNode n : m.instructions.toArray()) {
                if (!(n instanceof MethodInsnNode) || !((MethodInsnNode)n).owner.equals(POLICY)) continue;
                MethodInsnNode call = (MethodInsnNode)n;
                int count = call.name.equals("presentation") ? 1 : 4;
                AbstractInsnNode cursor = n;
                for (int i = 0; i < count; i++) {
                    AbstractInsnNode previous = cursor.getPrevious();
                    m.instructions.remove(cursor); cursor = previous;
                }
                removed++;
            }
            for (MethodNode old : original.methods) if (old.name.equals(m.name) && old.desc.equals(m.desc)) {
                m.maxStack = old.maxStack; // The only recomputed method metadata.
            }
        }
        changed.version = original.version;
        require(removed == expected && Arrays.equals(canonical(original), canonical(changed)),
                "Stock bytecode changed outside the reviewed inserts");
    }
    static byte[] patch(String name, byte[] input) throws Exception {
        String hash = sha(input);
        boolean physical = name.endsWith("/PhysicalDevice");
        require(physical
            ? hash.equals("06fe11f9ecc012a1e3db1628cd6745c045434020760bc01c0f0fda3a9904b846")
              || hash.equals("7cf7cc1d551382a8ee466a48d8c55ea586ef69bb3ed8460587b15822063b5091")
            : hash.equals("b951153589dff05e9ba3711d383a9772a5b4f3a1a3df763105365984078801cf"),
            "Unrecognized transport UI class: " + name + " sha256=" + hash);
        ClassNode c = new ClassNode();
        new ClassReader(input).accept(c, 0);
        require(c.name.equals(name), "Wrong class name");
        // The JXE analysis exporter marks reconstructed stock classes 49.
        // Shipping replacements, including these stock bodies, target J9/1.4.
        c.version = Opcodes.V1_4;
        int edits = 0;
        for (MethodNode m : c.methods) {
            if (physical && m.name.equals("<init>")) {
                for (AbstractInsnNode n : m.instructions.toArray()) {
                    if (!(n instanceof FieldInsnNode)) continue;
                    FieldInsnNode f = (FieldInsnNode)n;
                    if (f.getOpcode() == Opcodes.PUTFIELD && f.owner.equals(name)
                        && f.name.equals("device") && f.desc.equals(DSI)) {
                        m.instructions.insertBefore(n, new MethodInsnNode(Opcodes.INVOKESTATIC,
                            POLICY, "presentation", "(" + DSI + ")" + DSI, false));
                        edits++;
                    }
                }
            } else if (!physical && m.name.equals("setDisclaimerDevice") && m.desc.equals("(" + DEVICE + ")V")) {
                m.instructions.insert(prepare(name, "prepareDisclaimer", DEVICE));
                edits++;
            } else if (!physical && m.name.equals("showScreen") && m.desc.equals("(" + LIST + "Ljava/lang/String;)V")) {
                for (AbstractInsnNode n : m.instructions.toArray()) {
                    if (!(n instanceof MethodInsnNode)) continue;
                    MethodInsnNode call = (MethodInsnNode)n;
                    if (call.owner.equals(name + "$IShowScreenStrategy") && call.name.equals("showScreen")
                        && call.desc.equals("()V")) {
                        // The original strategy receiver remains below our temporary operands.
                        m.instructions.insertBefore(n, prepare(name, "preparePopup", LIST));
                        edits++;
                    }
                }
            }
        }
        require(edits == (physical ? 1 : 2), "Unexpected hook count for " + name + ": " + edits);
        ClassWriter out = new ClassWriter(ClassWriter.COMPUTE_MAXS);
        c.accept(out);
        byte[] result = out.toByteArray();
        verifyPreserved(input, result, edits);
        return result;
    }
    public static void main(String[] args) throws Exception {
        require(args.length == 2, "usage: TransportDevicePatch stock.jar output-classes");
        try (JarFile jar = new JarFile(args[0])) {
            for (String name : new String[] {ROOT + "device/PhysicalDevice", ROOT + "hmi/NewDeviceHMIHandler"}) {
                Path output = Paths.get(args[1], name + ".class");
                byte[] bytes = patch(name, read(jar, name));
                Files.createDirectories(output.getParent());
                Files.write(output, bytes);
                System.out.println("Transport UI hooks: " + name + " sha256=" + sha(bytes));
            }
        }
    }
}
