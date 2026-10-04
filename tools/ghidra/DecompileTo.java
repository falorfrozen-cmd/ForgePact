// DecompileTo.java - decompile named functions of the imported Hero_Siege.exe
// into local files, for reading only.
//
// The project is the ImportSymbols.java result (functions named with the bare
// script names from symbols.csv, '@' mapped to '_'). This script never writes
// to the project (run it with -readOnly) and never writes under a repository:
// its output directory is the first argument and must be somewhere local such
// as C:\Users\<you>\tools\hs-decomp. Decompiled text is research material and
// stays out of every tracked file (hub AGENTS.md, Legal section).
//
// USAGE (headless, through a small .cmd wrapper because the launcher rejects
// paths containing "(x86)"):
//
// 1. Decompile, with -scriptPath at this folder in a toolkit checkout:
//
//   set JAVA_HOME=C:\Program Files\Eclipse Adoptium\jdk-21.0.12.101-hotspot
//   "%GHIDRA%\support\analyzeHeadless.bat" C:\Users\<you>\ghidra_projects HeroSiege ^
//       -process Hero_Siege.exe -noanalysis -readOnly ^
//       -scriptPath <checkout>\ForgePact\tools\ghidra ^
//       -postScript DecompileTo.java <outdir> <name|+name|0xADDR> ...
//
// 2. Record the output in the local decompile index, from the toolkit
//    checkout's root. The run prints this line last, <outdir> filled in, so a
//    wrapper can carry it as its next line:
//
//   py -3 tools/decomp_index.py scan "<outdir>"
//
// Each argument after <outdir> is a function name as ImportSymbols applied it
// (SaveStash, ___struct___359_SaveStash_SaveStashFunc, ...) or a hex address.
// A leading '+' also decompiles the function's direct callees that carry no
// name (the runtime helpers YYC calls), into callee_<addr>.c, so a call chain
// can be followed one level without a second run.
//
// Per function the file holds: a header (name, address, size), the strings
// its instructions reference (rip-relative operands that point at printable
// NUL-terminated ASCII - YYC passes variable and script names this way), the
// callees found in the decompiled text, then the decompiled C.
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileOptions;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.app.cmd.disassemble.DisassembleCommand;
import ghidra.app.cmd.function.CreateFunctionCmd;
import ghidra.program.model.address.Address;
import ghidra.program.model.address.AddressSet;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.InstructionIterator;
import ghidra.program.model.mem.Memory;
import ghidra.program.model.mem.MemoryBlock;
import ghidra.program.model.symbol.Symbol;
import ghidra.program.model.symbol.SymbolIterator;
import ghidra.util.task.ConsoleTaskMonitor;

import java.io.File;
import java.io.FileWriter;
import java.io.PrintWriter;
import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

public class DecompileTo extends GhidraScript {

    private static final Pattern CALLEE = Pattern.compile("\\b(?:FUN_|func_0x)([0-9a-fA-F]{6,16})\\b");
    private static final Pattern NAMED_CALL = Pattern.compile("\\b([A-Za-z_][A-Za-z0-9_]*)\\s*\\(");

    private DecompInterface decomp;
    private File outDir;

    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length < 2) {
            println("DecompileTo: usage -> <outdir> <name|+name|0xADDR> ...");
            return;
        }
        outDir = new File(args[0]);
        outDir.mkdirs();

        decomp = new DecompInterface();
        DecompileOptions opts = new DecompileOptions();
        decomp.setOptions(opts);
        decomp.toggleCCode(true);
        decomp.toggleSyntaxTree(false);
        decomp.setSimplificationStyle("decompile");
        if (!decomp.openProgram(currentProgram)) {
            println("DecompileTo: decompiler failed to open the program: " + decomp.getLastMessage());
            return;
        }

        Set<Long> done = new LinkedHashSet<>();
        for (int i = 1; i < args.length; i++) {
            String a = args[i];
            boolean withCallees = a.startsWith("+");
            if (withCallees) a = a.substring(1);
            Function fn = resolve(a);
            if (fn == null) { println("MISSING " + a); continue; }
            List<Long> callees = emit(fn, fn.getName(), done);
            if (withCallees) {
                for (Long addr : callees) {
                    if (done.contains(addr)) continue;
                    Address ca = toAddr(addr);
                    Function cf = getFunctionAt(ca);
                    if (cf == null) cf = createAt(ca);
                    if (cf == null) { println("  callee " + ca + ": no function could be created"); continue; }
                    String label = cf.getSymbol().getSource().toString().equals("DEFAULT")
                            ? "callee_" + ca.toString() : cf.getName();
                    emit(cf, label, done);
                }
            }
        }
        decomp.dispose();
        println("DecompileTo: done, " + done.size() + " function(s) written under " + outDir);
        // Printed only: the record step runs from the toolkit checkout, not from here.
        println("DecompileTo: record it, from the toolkit checkout's root: py -3 tools/decomp_index.py scan \""
                + outDir.getAbsolutePath() + "\"");
    }

    private Function resolve(String a) {
        if (a.toLowerCase().startsWith("0x")) {
            Address ad = toAddr(Long.parseUnsignedLong(a.substring(2), 16));
            Function fn = getFunctionAt(ad);
            if (fn == null) fn = createAt(ad);
            return fn;
        }
        SymbolIterator it = currentProgram.getSymbolTable().getSymbols(a);
        while (it.hasNext()) {
            Symbol s = it.next();
            Function fn = getFunctionAt(s.getAddress());
            if (fn != null) return fn;
        }
        return null;
    }

    // Read-only session: the function exists for this run only, never saved.
    private Function createAt(Address ad) {
        try {
            if (getInstructionAt(ad) == null) {
                DisassembleCommand dis = new DisassembleCommand(ad, null, true);
                dis.applyTo(currentProgram, new ConsoleTaskMonitor());
            }
            CreateFunctionCmd cmd = new CreateFunctionCmd(ad);
            cmd.applyTo(currentProgram, new ConsoleTaskMonitor());
            return getFunctionAt(ad);
        } catch (Exception e) {
            println("  createAt " + ad + " failed: " + e.getMessage());
            return null;
        }
    }

    private List<Long> emit(Function fn, String label, Set<Long> done) throws Exception {
        done.add(fn.getEntryPoint().getOffset());
        String safe = label.replaceAll("[^A-Za-z0-9_.-]", "_");
        File out = new File(outDir, safe + ".c");
        DecompileResults res = decomp.decompileFunction(fn, 180, new ConsoleTaskMonitor());
        String c = (res != null && res.decompileCompleted() && res.getDecompiledFunction() != null)
                ? res.getDecompiledFunction().getC()
                : "/* decompile failed: " + (res == null ? "null" : res.getErrorMessage()) + " */\n";

        Map<String, String> strings = referencedStrings(fn);
        List<Long> callees = new ArrayList<>();
        Set<String> named = new LinkedHashSet<>();
        Matcher m = CALLEE.matcher(c);
        while (m.find()) {
            long v = Long.parseUnsignedLong(m.group(1), 16);
            if (!callees.contains(v)) callees.add(v);
        }
        Matcher n = NAMED_CALL.matcher(c);
        while (n.find()) {
            String name = n.group(1);
            if (name.startsWith("FUN_") || name.startsWith("func_0x")) continue;
            if (name.equals("if") || name.equals("while") || name.equals("for") || name.equals("switch")
                    || name.equals("return") || name.equals("sizeof")) continue;
            if (currentProgram.getSymbolTable().getSymbols(name).hasNext()) named.add(name);
        }

        try (PrintWriter w = new PrintWriter(new FileWriter(out))) {
            w.println("// " + label + " @ " + fn.getEntryPoint() + " size=" + fn.getBody().getNumAddresses());
            w.println("// program: " + currentProgram.getName() + " (local decompiled research material - never copy into a tracked file)");
            w.println("// strings referenced (" + strings.size() + "):");
            for (Map.Entry<String, String> e : strings.entrySet()) w.println("//   " + e.getKey() + " \"" + e.getValue() + "\"");
            w.println("// named callees (" + named.size() + "): " + String.join(", ", named));
            w.print("// unnamed callees (" + callees.size() + "):");
            for (Long v : callees) w.print(" 0x" + Long.toHexString(v));
            w.println();
            w.println();
            w.print(c);
        }
        println("WROTE " + out.getName() + " (" + fn.getEntryPoint() + ", " + strings.size() + " strings, "
                + named.size() + " named, " + callees.size() + " unnamed callees)");
        return callees;
    }

    private Map<String, String> referencedStrings(Function fn) {
        Map<String, String> found = new LinkedHashMap<>();
        Memory mem = currentProgram.getMemory();
        InstructionIterator it = currentProgram.getListing().getInstructions(fn.getBody(), true);
        while (it.hasNext()) {
            Instruction ins = it.next();
            for (int op = 0; op < ins.getNumOperands(); op++) {
                for (Object o : ins.getOpObjects(op)) {
                    Address ad = null;
                    if (o instanceof Address) ad = (Address) o;
                    else if (o instanceof ghidra.program.model.scalar.Scalar) {
                        long v = ((ghidra.program.model.scalar.Scalar) o).getUnsignedValue();
                        if (v > 0x140000000L && v < 0x180000000L) ad = toAddr(v);
                    }
                    if (ad == null) continue;
                    MemoryBlock b = mem.getBlock(ad);
                    if (b == null || b.isExecute()) continue;
                    String s = readAscii(mem, ad, 160);
                    if (s != null && s.length() >= 2) found.put(ad.toString(), s);
                }
            }
        }
        return found;
    }

    private String readAscii(Memory mem, Address ad, int max) {
        StringBuilder sb = new StringBuilder();
        try {
            for (int i = 0; i < max; i++) {
                byte bch = mem.getByte(ad.add(i));
                if (bch == 0) return sb.length() > 0 ? sb.toString() : null;
                if (bch < 0x20 || bch > 0x7e) return null;
                sb.append((char) bch);
            }
        } catch (Exception e) { return null; }
        return null;
    }
}
