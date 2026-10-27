// Headless Ghidra helper for the MU1316 LPASS TDM audit.
// Args: <output-file> <hex-address> [...]

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.mem.MemoryBlock;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceIterator;

import java.io.BufferedWriter;
import java.io.FileWriter;
import java.util.LinkedHashSet;
import java.util.Set;

public class ExportTdmXrefs extends GhidraScript {
    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length < 2) {
            println("usage: ExportTdmXrefs.java <output-file> <hex-address> [...]");
            return;
        }

        Set<Function> functions = new LinkedHashSet<>();
        try (BufferedWriter out = new BufferedWriter(new FileWriter(args[0]))) {
            for (MemoryBlock block : currentProgram.getMemory().getBlocks()) {
                out.write("BLOCK " + block.getName() + " " + block.getStart() + ".." +
                    block.getEnd() + " execute=" + block.isExecute() + "\n");
            }
            for (int i = 1; i < args.length; i++) {
                long value = Long.parseUnsignedLong(args[i].replaceFirst("^0[xX]", ""), 16);
                Address target = toAddr(value);
                out.write("TARGET " + target + "\n");
                Function targetFunction = getFunctionContaining(target);
                MemoryBlock targetBlock = currentProgram.getMemory().getBlock(target);
                if (targetFunction == null && targetBlock != null && targetBlock.isExecute()) {
                    disassemble(target);
                    targetFunction = createFunction(target, null);
                }
                if (targetFunction != null) {
                    out.write("  CONTAINING function=" + targetFunction.getName() + "@" +
                        targetFunction.getEntryPoint() + "\n");
                    functions.add(targetFunction);
                }
                ReferenceIterator refs = currentProgram.getReferenceManager().getReferencesTo(target);
                while (refs.hasNext()) {
                    Reference ref = refs.next();
                    Address from = ref.getFromAddress();
                    Function function = getFunctionContaining(from);
                    out.write("  REF " + from + " " + ref.getReferenceType());
                    if (function != null) {
                        out.write(" function=" + function.getName() + "@" + function.getEntryPoint());
                        functions.add(function);
                    }
                    out.write("\n");
                }
            }

            DecompInterface decompiler = new DecompInterface();
            decompiler.openProgram(currentProgram);
            for (Function function : functions) {
                out.write("\n==== " + function.getName() + " @ " + function.getEntryPoint() + " ====\n");
                DecompileResults result = decompiler.decompileFunction(function, 120, monitor);
                if (result.decompileCompleted() && result.getDecompiledFunction() != null) {
                    out.write(result.getDecompiledFunction().getC());
                } else {
                    out.write("DECOMPILE FAILED: " + result.getErrorMessage() + "\n");
                }
            }
            decompiler.dispose();
        }
    }
}
