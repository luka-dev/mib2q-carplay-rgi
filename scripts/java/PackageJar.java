import java.io.*;
import java.nio.file.*;
import java.util.*;
import java.util.jar.*;

/** Build-tool code, never shipped: stable JAR entries and Java 1.4 validation. */
public final class PackageJar {
    public static void main(String[] args) throws Exception {
        Path root = Paths.get(args[0]);
        TreeMap<String, Path> files = new TreeMap<String, Path>();
        try (java.util.stream.Stream<Path> paths = Files.walk(root)) {
            paths.filter(Files::isRegularFile).forEach(p ->
                files.put(root.relativize(p).toString().replace(File.separatorChar, '/'), p));
        }
        int count = 0;
        for (Map.Entry<String, Path> item : files.entrySet()) {
            if (!item.getKey().endsWith(".class")) continue;
            try (DataInputStream in = new DataInputStream(Files.newInputStream(item.getValue()))) {
                if (in.readInt() != 0xcafebabe) throw new IOException("Invalid class: " + item.getKey());
                in.readUnsignedShort();
                if (in.readUnsignedShort() != 48) throw new IOException("Not Java 1.4: " + item.getKey());
            }
            count++;
        }
        for (String required : new String[] {"com/luka/carplay/core/CarPlayApp.class",
                "com/luka/carplay/rgd/VCTextData.class", "com/luka/carplay/rgd/vc-text.bin"}) {
            if (!files.containsKey(required)) throw new IOException("Missing JAR entry: " + required);
        }
        if (count == 0) throw new IOException("No compiled classes");
        // No host/JDK-specific Created-By field. UTC is set by the container runner.
        try (JarOutputStream out = new JarOutputStream(new FileOutputStream(args[1]))) {
            JarEntry manifest = new JarEntry("META-INF/MANIFEST.MF");
            manifest.setTime(315532800000L);
            out.putNextEntry(manifest);
            out.write("Manifest-Version: 1.0\r\n\r\n".getBytes("UTF-8"));
            out.closeEntry();
            for (Map.Entry<String, Path> item : files.entrySet()) {
                if (item.getKey().equals("META-INF/MANIFEST.MF"))
                    throw new IOException("Resources must not replace the generated manifest");
                JarEntry entry = new JarEntry(item.getKey());
                entry.setTime(315532800000L);
                out.putNextEntry(entry);
                Files.copy(item.getValue(), out);
                out.closeEntry();
            }
        }
        System.out.println("Java 1.4: " + count + " classes; complete resources");
    }
}
