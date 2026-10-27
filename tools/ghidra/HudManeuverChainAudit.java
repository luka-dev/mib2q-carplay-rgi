// Offline Ghidra script: execute original V850 BAP -> 3D HUD navigation code.
// Args: input.csv output.csv [FD51 override]. Loaded image base must be 0x20000.
// An override is written only into emulator memory, never into the program/file.
// The BAP cache accessor is the only substituted function. All memory helpers,
// mapping functions, model transfer and selector packing execute original code.
import ghidra.app.script.GhidraScript;
import ghidra.app.emulator.EmulatorHelper;
import java.nio.file.*;
import java.nio.charset.StandardCharsets;
import java.io.*;
import java.util.*;
import java.security.MessageDigest;

public class HudManeuverChainAudit extends GhidraScript {
    private EmulatorHelper emu;
    private final long input = 0x1000000L, scratch = 0x1002000L;
    private final long stop = 0x1004000L, stack = 0x1020000L;
    private long totalSteps;
    private int cacheReads;

    private long reg(String name) {
        return emu.readRegister(name).longValue() & 0xffffffffL;
    }
    private void put(long address, long value, int size) throws Exception {
        byte[] bytes = new byte[size];
        for (int i = 0; i < size; i++) bytes[i] = (byte)(value >>> (8 * i));
        emu.writeMemory(toAddr(address), bytes);
    }
    private long get(long address, int size) throws Exception {
        byte[] bytes = emu.readMemory(toAddr(address), size);
        long value = 0;
        for (int i = 0; i < size; i++) value |= (long)(bytes[i] & 255) << (8 * i);
        return value;
    }
    private String hex(byte[] bytes) {
        StringBuilder text = new StringBuilder();
        for (byte b : bytes) text.append(String.format("%02x", b & 255));
        return text.toString();
    }
    private void require(boolean condition, String message) throws Exception {
        if (!condition) throw new Exception(message);
    }
    private void call(long address, long... arguments) throws Exception {
        for (int i = 0; i < arguments.length; i++) emu.writeRegister("r" + (6 + i), arguments[i]);
        emu.writeRegister("sp", stack);
        emu.writeRegister("lp", stop);
        emu.writeRegister("pc", address);
        int steps = 0;
        while (reg("pc") != stop && steps++ < 100000) {
            if (reg("pc") == 0xa5ed6L) {
                require(reg("r6") == 0x32 && reg("r7") == 0x17, "unexpected BAP cache request");
                put(reg("r8"), input, 4);
                put(reg("r9"), 60, 2);
                emu.writeRegister("r10", 0);
                emu.writeRegister("pc", reg("lp"));
                cacheReads++;
                continue;
            }
            require(emu.step(monitor), "MCU error at " + emu.getExecutionAddress() + ": " + emu.getLastError());
        }
        require(reg("pc") == stop, "step cap at " + Long.toHexString(address));
        totalSteps += steps;
    }

    public void run() throws Exception {
        String[] args = getScriptArgs();
        require(args.length == 2 || args.length == 3, "args: input.csv output.csv [FD51 override]");
        require(currentProgram.getImageBase().getOffset() == 0x20000L, "FD51 must be linked at 0x20000");
        byte[] source = new byte[638976];
        currentProgram.getMemory().getBytes(toAddr(0x20000L), source);
        if (args.length == 3) source = Files.readAllBytes(Paths.get(args[2]));
        String sha = hex(MessageDigest.getInstance("SHA-256").digest(source));
        require(source.length == 638976 && (sha.equals("138351c2f12cd8e45d9f50fd4833775371ec46401b6838f04c24a9a4baaabae2")
            || sha.equals("6e59b62c982873a778bf72b749bf6dc113fe3fe14ee4bb3ae3855d0c624f6fe5")), "unsupported FD51 fingerprint");
        List<String> lines = Files.readAllLines(Paths.get(args[0]), StandardCharsets.UTF_8);
        emu = new EmulatorHelper(currentProgram);
        emu.writeMemory(toAddr(0x20000L), source);
        int cases = 0;
        try (PrintWriter out = new PrintWriter(args[1], "UTF-8")) {
            out.println("case,main,direction,z,streets_hex,stored_main,stored_direction,model,selector,marker,road_count,roads_hex,group_mask");
            for (String line : lines.subList(1, lines.size())) {
                monitor.checkCancelled();
                String[] fields = line.split(",", -1);
                int main = Integer.parseInt(fields[1]), direction = Integer.parseInt(fields[2]);
                int z = Integer.parseInt(fields[3]), count = fields[4].length() / 2;
                require(count <= 16, "input exceeds MCU side-street capacity");
                emu.writeMemory(toAddr(0xfedf0000L), new byte[65536]);
                emu.writeMemory(toAddr(input), new byte[0x3000]);
                emu.writeMemory(toAddr(stack - 0x4000), new byte[0x4000]);
                emu.writeRegister("gp", 0xfedff8b4L);
                emu.writeRegister("tp", 0x28018L);
                emu.writeRegister("CTBP", 0xb59c0L);
                put(input, main, 1); put(input + 1, direction, 1);
                put(input + 2, z, 1); put(input + 3, count, 1);
                for (int i = 0; i < count; i++) put(input + 4 + i, Integer.parseInt(fields[4].substring(i * 2, i * 2 + 2), 16), 1);
                int beforeReads = cacheReads;
                call(0x5ec3cL, scratch); // FctID 23 import, all slot-2/3 bytes zero
                require(cacheReads == beforeReads + 1, "BAP import did not request exactly one property");
                long storedMain = get(0xfedfadd4L, 2), storedDirection = get(0xfedfadd6L, 2);
                put(scratch + 2, 1, 1); // navigation descriptor is available
                call(0x8adacL, scratch); // original main/direction/z/street conversion
                long model = get(0xfedfc270L, 4);
                call(0x974e8L, 3); // transfer active 3D navigation model to display model
                require(get(0xfedfc600L, 4) == model, "display model changes main icon");
                put(0xfedfc170L, 3, 4); // isolate active 3D navigation mode
                call(0x86534L, scratch + 0x100, scratch + 0x200, 4);
                long selector = get(scratch + 0x100, 1), marker = get(scratch + 0x101, 1);
                long roadCount = get(0xfedfc648L, 1), mask = get(scratch + 0x200, 4);
                require(roadCount <= 17 && selector <= 120 && mask == 4, "invalid packed navigation fields");
                String roads = hex(emu.readMemory(toAddr(scratch + 0x102), 17));
                out.println(line + "," + storedMain + "," + storedDirection + "," + model + "," + selector + "," + marker + "," + roadCount + "," + roads + "," + mask);
                cases++;
            }
        } finally {
            emu.dispose();
        }
        println("MCU_CHAIN_PASS cases=" + cases + " steps=" + totalSteps
            + " original_instructions=" + (totalSteps - cacheReads)
            + " substituted_cache_reads=" + cacheReads + " sha256=" + sha);
    }
}
