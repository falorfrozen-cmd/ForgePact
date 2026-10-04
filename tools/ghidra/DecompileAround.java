// DecompileAround.java - decompile the function CONTAINING each 0xADDR argument
// (a call site FindCallers.java printed inside an unnamed region) into local
// files, for reading only. Same output shape as DecompileTo.java.
//
// The project was imported with -noanalysis, so an unnamed region has no
// function and getFunctionContaining() answers null. This script walks back
// from the site over 16-byte-aligned addresses preceded by int3 (0xCC) padding,
// the usual MSVC gap between functions, creates a function at each candidate
// (read-only session: never saved) and keeps the first whose body contains the
// site. Output never goes under a repository (hub AGENTS.md, Legal section).
//
// USAGE (headless, -scriptPath at this folder in a toolkit checkout,
// <checkout>\ForgePact\tools\ghidra, and -readOnly as for DecompileTo.java):
//
// 1. -postScript DecompileAround.java <outdir> 0xADDR ...
// 2. Record the output in the local decompile index, from the toolkit
//    checkout's root (the run prints this line last, <outdir> filled in):
//
//      py -3 tools/decomp_index.py scan "<outdir>"
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileOptions;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.app.cmd.disassemble.DisassembleCommand;
import ghidra.app.cmd.function.CreateFunctionCmd;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.mem.Memory;
import ghidra.program.model.mem.MemoryBlock;
import ghidra.util.task.ConsoleTaskMonitor;

import java.io.File;
import java.io.PrintWriter;

public class DecompileAround extends GhidraScript {
    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length < 2) { println("DecompileAround: usage -> <outdir> 0xADDR ..."); return; }
        File outDir = new File(args[0]);
        outDir.mkdirs();
        DecompInterface decomp = new DecompInterface();
        decomp.setOptions(new DecompileOptions());
        decomp.toggleCCode(true);
        decomp.openProgram(currentProgram);
        Memory mem = currentProgram.getMemory();
        for (int i = 1; i < args.length; i++) {
            long site = Long.parseUnsignedLong(args[i].substring(2), 16);
            Address siteAd = toAddr(site);
            Function fn = getFunctionContaining(siteAd);
            int tries = 0;
            long cand = site & ~0xFL;
            long lo = cand;
            byte[] blk = new byte[0];
            if (fn == null) {
                // One read of up to 8 MB before the site, clamped to the site's
                // memory block; YYC event bodies run to hundreds of KB.
                MemoryBlock mb = mem.getBlock(siteAd);
                if (mb == null) { println("DecompileAround: " + siteAd + " is in no memory block"); continue; }
                lo = Math.min(cand, Math.max(cand - (8L << 20), mb.getStart().getOffset()));
                blk = new byte[(int) (cand - lo)];
                mem.getBytes(toAddr(lo), blk);
            }
            // Test the site's own 16-byte boundary first, then step back; the
            // decrement sits in the for header so a `continue` still advances.
            for (; fn == null && tries < 200; cand -= 16) {
                int off = (int) (cand - lo);
                if (off < 2) break;
                if ((blk[off - 1] & 0xff) != 0xCC || (blk[off - 2] & 0xff) != 0xCC) continue;
                tries++;
                Address ad = toAddr(cand);
                if (getInstructionAt(ad) == null) {
                    new DisassembleCommand(ad, null, true).applyTo(currentProgram, new ConsoleTaskMonitor());
                }
                new CreateFunctionCmd(ad).applyTo(currentProgram, new ConsoleTaskMonitor());
                Function f = getFunctionAt(ad);
                if (f != null && f.getBody().contains(siteAd)) fn = f;
                else println("  candidate " + ad + " does not contain " + siteAd + (f == null ? " (no function)" : " (size " + f.getBody().getNumAddresses() + ")"));
            }
            if (fn == null) { println("DecompileAround: no containing function found for " + siteAd); continue; }
            String label = "around_" + Long.toHexString(site) + "_fn_" + fn.getEntryPoint().toString();
            File out = new File(outDir, label + ".c");
            DecompileResults res = decomp.decompileFunction(fn, 300, new ConsoleTaskMonitor());
            String c = (res != null && res.decompileCompleted() && res.getDecompiledFunction() != null)
                    ? res.getDecompiledFunction().getC()
                    : "/* decompile failed: " + (res == null ? "null" : res.getErrorMessage()) + " */\n";
            try (PrintWriter w = new PrintWriter(out, "UTF-8")) {
                w.println("// function containing " + siteAd + " @ " + fn.getEntryPoint() + " size=" + fn.getBody().getNumAddresses());
                w.println("// program: Hero_Siege.exe (local decompiled research material - never copy into a tracked file)");
                w.println();
                w.print(c);
            }
            println("WROTE " + out.getName() + " (entry " + fn.getEntryPoint() + ", size " + fn.getBody().getNumAddresses() + ")");
        }
        decomp.dispose();
        println("DecompileAround: done");
        // Printed only: the record step runs from the toolkit checkout, not from here.
        println("DecompileAround: record it, from the toolkit checkout's root: py -3 tools/decomp_index.py scan \""
                + outDir.getAbsolutePath() + "\"");
    }
}
