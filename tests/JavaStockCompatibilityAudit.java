import java.io.*;
import java.util.*;
import java.util.jar.*;
import org.objectweb.asm.*;
import org.objectweb.asm.tree.*;

/** A shared patch JAR must not discard firmware-specific stock behavior.
 * Compare only stock classes that the patch actually replaces. Normalization
 * removes differences introduced by versions of the JXE analysis exporter:
 * debug data, frames, trailing/padding NOPs, classfile version, synthetic flags
 * and InnerClasses ordering. Instructions, members, constants, access rights
 * and exception handlers otherwise remain part of the comparison. */
public final class JavaStockCompatibilityAudit {
    static byte[] normalized(JarFile jar, String name) throws IOException {
        ClassNode c = new ClassNode();
        try (InputStream input = jar.getInputStream(jar.getJarEntry(name))) {
            new ClassReader(input).accept(c, ClassReader.SKIP_DEBUG | ClassReader.SKIP_FRAMES);
        }
        c.version = Opcodes.V1_5;
        c.access &= ~Opcodes.ACC_SYNTHETIC;
        for (FieldNode f : c.fields) f.access &= ~Opcodes.ACC_SYNTHETIC;
        for (MethodNode m : c.methods) {
            m.access &= ~Opcodes.ACC_SYNTHETIC;
            for (AbstractInsnNode n : m.instructions.toArray())
                if (n.getOpcode() == Opcodes.NOP) m.instructions.remove(n);
        }
        for (InnerClassNode i : c.innerClasses) i.access &= ~Opcodes.ACC_SYNTHETIC;
        Collections.sort(c.innerClasses, new Comparator<InnerClassNode>() {
            public int compare(InnerClassNode a, InnerClassNode b) { return a.name.compareTo(b.name); }
        });
        ClassWriter writer = new ClassWriter(0);
        c.accept(writer);
        return writer.toByteArray();
    }

    public static void main(String[] args) throws Exception {
        if (args.length != 3)
            throw new IllegalArgumentException("reference-stock.jar candidate-stock.jar patch.jar");
        int compared = 0, errors = 0;
        try (JarFile reference = new JarFile(args[0]); JarFile candidate = new JarFile(args[1]);
             JarFile patch = new JarFile(args[2])) {
            Enumeration<JarEntry> entries = patch.entries();
            while (entries.hasMoreElements()) {
                String name = entries.nextElement().getName();
                if (!name.endsWith(".class") || reference.getEntry(name) == null) continue;
                compared++;
                if (candidate.getEntry(name) == null ||
                    !Arrays.equals(normalized(reference, name), normalized(candidate, name))) {
                    System.err.println("Stock behavior differs: " + name);
                    errors++;
                }
            }
        }
        if (compared == 0 || errors != 0)
            throw new AssertionError("Stock compatibility: compared=" + compared + " errors=" + errors);
        System.out.println("Stock compatibility: " + compared + " replaced classes match PASS");
    }
}
