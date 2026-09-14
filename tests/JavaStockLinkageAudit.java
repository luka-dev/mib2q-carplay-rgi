import java.io.*;
import java.util.*;
import java.util.jar.*;
import org.objectweb.asm.*;
import org.objectweb.asm.tree.*;

/** Check both patch->stock links and remaining stock callers of replaced classes. */
public final class JavaStockLinkageAudit {
    static final Map<String,ClassNode> cache = new HashMap<>();
    static final Set<String> patched = new TreeSet<>(), errors = new TreeSet<>();
    static final List<JarFile> jars = new ArrayList<>();
    static final Map<String,Boolean> affected = new HashMap<>();
    static int patchLinks, stockLinks;
    static final class Member {
        final String owner;
        final int access;
        Member(String owner, int access) { this.owner=owner; this.access=access; }
    }
    static ClassNode read(InputStream input) throws IOException {
        return read(input, ClassReader.SKIP_DEBUG);
    }
    static ClassNode read(InputStream input, int flags) throws IOException {
        try (InputStream in = input) {
            ClassNode c = new ClassNode(); new ClassReader(in).accept(c, flags); return c;
        }
    }
    static ClassNode get(String name) throws IOException {
        if (name.startsWith("[")) name = "java/lang/Object";
        if (cache.containsKey(name)) return cache.get(name);
        for (JarFile jar : jars) {
            JarEntry e = jar.getJarEntry(name + ".class");
            if (e != null) {
                ClassNode c = read(jar.getInputStream(e), ClassReader.SKIP_DEBUG | ClassReader.SKIP_CODE);
                cache.put(name,c); return c;
            }
        }
        // Never let the host Java 8 library mask an API absent from the HU J9 library.
        cache.put(name,null); return null;
    }
    static Member member(String owner, String name, String desc, boolean method, Set<String> seen) throws IOException {
        if (!seen.add(owner)) return null;
        ClassNode c = get(owner); if (c == null) return null;
        if (method) {
            for (MethodNode m : c.methods) if (m.name.equals(name) && m.desc.equals(desc)) return new Member(c.name,m.access);
        } else {
            for (FieldNode f : c.fields) if (f.name.equals(name) && f.desc.equals(desc)) return new Member(c.name,f.access);
        }
        if (name.equals("<init>")) return null;
        // Field resolution visits interfaces before the superclass (JVMS 5.4.3.2).
        if (method && c.superName != null) {
            Member found=member(c.superName,name,desc,true,seen); if (found != null) return found;
        }
        for (String i : c.interfaces) {
            Member found=member(i,name,desc,method,seen); if (found != null) return found;
        }
        return !method && c.superName != null ? member(c.superName,name,desc,false,seen) : null;
    }
    static boolean affected(String owner) throws IOException {
        if (affected.containsKey(owner)) return affected.get(owner);
        affected.put(owner,false);
        ClassNode c=get(owner);
        boolean result=patched.contains(owner);
        if (!result && c != null) {
            if (c.superName != null) result=affected(c.superName);
            for (String i : c.interfaces) result |= affected(i);
        }
        affected.put(owner,result); return result;
    }
    static String pkg(String name) { return name.substring(0,name.lastIndexOf('/')+1); }
    static boolean accessible(String caller, Member member) throws IOException {
        int flags=member.access;
        if ((flags & Opcodes.ACC_PUBLIC) != 0 || caller.equals(member.owner)) return true;
        if ((flags & Opcodes.ACC_PRIVATE) != 0) return false;
        if (pkg(caller).equals(pkg(member.owner))) return true;
        if ((flags & Opcodes.ACC_PROTECTED) == 0) return false;
        for (ClassNode c=get(caller); c != null; c=c.superName == null ? null : get(c.superName))
            if (c.name.equals(member.owner)) return true;
        return false;
    }
    static void check(ClassNode c, boolean patch) throws IOException {
        for (MethodNode m : c.methods) for (AbstractInsnNode n : m.instructions) {
            String owner, name, desc; boolean method;
            if (n instanceof MethodInsnNode) {
                MethodInsnNode x=(MethodInsnNode)n; owner=x.owner; name=x.name; desc=x.desc; method=true;
            } else if (n instanceof FieldInsnNode) {
                FieldInsnNode x=(FieldInsnNode)n; owner=x.owner; name=x.name; desc=x.desc; method=false;
            } else continue;
            if (!patch && !affected(owner)) continue;
            // Arrays provide clone() intrinsically; no declaring class-file method.
            if (owner.startsWith("[") && name.equals("clone")) continue;
            if (patch) patchLinks++; else stockLinks++;
            Member target=member(owner,name,desc,method,new HashSet<>());
            String link=(patch ? "PATCH" : "STOCK") + " " + c.name + "." + m.name + m.desc
                + " -> " + owner + "." + name + desc;
            if (target == null) { errors.add("MISSING " + link); continue; }
            boolean staticUse=n.getOpcode()==Opcodes.INVOKESTATIC || n.getOpcode()==Opcodes.GETSTATIC
                || n.getOpcode()==Opcodes.PUTSTATIC;
            if (staticUse != ((target.access & Opcodes.ACC_STATIC) != 0)) errors.add("STATIC " + link);
            // Final.jar inlines access$ into otherwise illegal private accesses for decompilation.
            // Its stock callers are checked for existence only; the combined.jar pass checks access.
            if ((patch || !jars.get(1).getName().endsWith("-final.jar")) && !accessible(c.name,target))
                errors.add("ACCESS " + link);
        }
    }
    public static void main(String[] args) throws Exception {
        for (String arg : args) jars.add(new JarFile(arg));
        for (Enumeration<JarEntry> e=jars.get(0).entries();e.hasMoreElements();) {
            JarEntry entry=e.nextElement();
            if (entry.getName().endsWith(".class")) patched.add(entry.getName().replace(".class",""));
        }
        for (String name : patched)
            check(read(jars.get(0).getInputStream(jars.get(0).getJarEntry(name + ".class"))),true);
        int stockClasses=0;
        for (Enumeration<JarEntry> e=jars.get(1).entries();e.hasMoreElements();) {
            JarEntry entry=e.nextElement();
            if (!entry.getName().endsWith(".class") || patched.contains(entry.getName().replace(".class",""))) continue;
            check(read(jars.get(1).getInputStream(entry)),false); stockClasses++;
        }
        for (String error : errors) System.out.println(error);
        System.out.println("JavaStockLinkageAudit: " + patched.size() + " patch classes / " + stockClasses
            + " remaining stock classes; links patch=" + patchLinks + " stock=" + stockLinks + "; errors=" + errors.size());
        for (JarFile jar : jars) jar.close();
        if (!errors.isEmpty()) throw new AssertionError("Unresolved stock/patch member links");
    }
}
