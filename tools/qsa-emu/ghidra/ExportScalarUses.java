// Headless Ghidra helper for locating structure-offset and immediate uses.
// Args: <output-file> <hex-scalar> [...]

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.scalar.Scalar;

import java.io.BufferedWriter;
import java.io.FileWriter;
import java.util.LinkedHashSet;
import java.util.Set;

public class ExportScalarUses extends GhidraScript {
    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length < 2) {
            println("usage: ExportScalarUses.java <output-file> <hex-scalar> [...]");
            return;
        }

        Set<Long> targets = new LinkedHashSet<>();
        for (int i = 1; i < args.length; i++) {
            targets.add(Long.parseUnsignedLong(args[i].replaceFirst("^0[xX]", ""), 16));
        }

        Set<Function> functions = new LinkedHashSet<>();
        try (BufferedWriter out = new BufferedWriter(new FileWriter(args[0]))) {
            Instruction instruction = currentProgram.getListing().getInstructions(true).next();
            while (instruction != null) {
                boolean matched = false;
                for (int operand = 0; operand < instruction.getNumOperands(); operand++) {
                    for (Object object : instruction.getOpObjects(operand)) {
                        if (object instanceof Scalar &&
                            targets.contains(((Scalar)object).getUnsignedValue())) {
                            matched = true;
                        }
                    }
                }
                if (matched) {
                    Function function = getFunctionContaining(instruction.getAddress());
                    out.write(instruction.getAddress() + "  " + instruction + "  function=" +
                        (function == null ? "<none>" : function.getName() + "@" + function.getEntryPoint()) +
                        "\n");
                    if (function != null) {
                        functions.add(function);
                    }
                }
                instruction = instruction.getNext();
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
