// Export selected pseudocode with the verified V850 runtime GP/TP context.
// Args: output-directory. FD51 must already be loaded at 0x20000.
// Analysis-only context changes: close the source program without saving.
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.listing.Function;
import java.nio.file.*;
import java.nio.charset.StandardCharsets;
import java.math.BigInteger;

public class HudManeuverEvidence extends GhidraScript {
    public void run() throws Exception {
        if (currentProgram.getImageBase().getOffset() != 0x20000L)
            throw new Exception("Expected V850 FD51 at 0x20000");
        Path out = Paths.get(getScriptArgs()[0]);
        Files.createDirectories(out);
        currentProgram.getProgramContext().setValue(currentProgram.getRegister("gp"),
            toAddr(0x20000L), toAddr(0xb5fffL), new BigInteger("fedff8b4", 16));
        currentProgram.getProgramContext().setValue(currentProgram.getRegister("tp"),
            toAddr(0x20000L), toAddr(0xb5fffL), new BigInteger("28018", 16));
        long[] entries = {0x5ec3c,0x5fa7e,0x5e12a,0x65b2a,0x8ab76,0x8acb0,0x8adac,
            0x8b32e,0x8b3b2,0x8b486,0x8b536,0x8b6fa,0x8b798,0x8b86e,
            0x8ac90,0x8c340,0x974e8,0x97b96,0x98378,0x99a64,
            0x86534,0x89036,0x890f0,0x8910c,0x85286,0x8508e,
            0x8a984,0x81b9e,0x81dea,0x827d0,0x8277e,0x82a8e,0x83154};
        DecompInterface decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);
        try {
            for (long entry : entries) {
                Function function = getFunctionAt(toAddr(entry));
                if (function == null) {
                    disassemble(toAddr(entry));
                    function = createFunction(toAddr(entry), null);
                }
                if (function == null) throw new Exception("Missing function " + Long.toHexString(entry));
                DecompileResults result = decompiler.decompileFunction(function, 60, monitor);
                if (!result.decompileCompleted()) throw new Exception(result.getErrorMessage());
                String header = "// Original V850 FD51 G1952; linked VA 0x" + Long.toHexString(entry)
                    + "; file offset 0x" + Long.toHexString(entry - 0x20000L)
                    + "; GP=fedff8b4, TP=28018.\n"
                    + "// Analysis pseudocode: inferred prototypes/stack variables are not source code.\n";
                Files.write(out.resolve(String.format("0x%x.c", entry)),
                    (header + result.getDecompiledFunction().getC()).getBytes(StandardCharsets.UTF_8));
            }
        } finally { decompiler.dispose(); }
        StringBuilder startup = new StringBuilder();
        var instructions = currentProgram.getListing().getInstructions(toAddr(0x41068L), true);
        while (instructions.hasNext()) {
            var instruction = instructions.next();
            if (instruction.getAddress().getOffset() > 0x4110aL) break;
            startup.append(instruction.getAddress()).append("  ").append(instruction).append("\n");
        }
        Files.write(out.resolve("startup-registers.asm"), startup.toString().getBytes(StandardCharsets.UTF_8));
        println("MCU_EVIDENCE_PASS functions=" + entries.length + " gp=fedff8b4 tp=28018");
    }
}
