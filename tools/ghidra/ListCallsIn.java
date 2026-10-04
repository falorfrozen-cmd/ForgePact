// ListCallsIn.java - read-only: linear-disassemble a region that the decompiler
// cannot finish (a Step-sized unnamed host) and list every CALL target in it,
// by name where the project has one, with the offset of each call site, plus the
// printable strings its instructions reference. No decompiled text is produced.
// Output stays local (hub AGENTS.md, Legal section).
//
// USAGE: -postScript ListCallsIn.java <outfile> 0xSTART <maxBytes>
import ghidra.app.cmd.disassemble.DisassembleCommand;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.address.AddressSet;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.InstructionIterator;
import ghidra.program.model.mem.Memory;
import ghidra.program.model.symbol.Symbol;
import ghidra.util.task.ConsoleTaskMonitor;

import java.io.File;
import java.io.FileWriter;
import java.io.PrintWriter;

public class ListCallsIn extends GhidraScript {
    @Override
    public void run() throws Exception {
        String[] a = getScriptArgs();
        File out = new File(a[0]);
        long start = Long.parseUnsignedLong(a[1].substring(2), 16);
        long len = Long.parseLong(a[2]);
        Address s = toAddr(start);
        Address e = toAddr(start + len - 1);
        AddressSet set = new AddressSet(s, e);
        DisassembleCommand dis = new DisassembleCommand(set, set, true);
        dis.applyTo(currentProgram, new ConsoleTaskMonitor());
        Memory mem = currentProgram.getMemory();
        int calls = 0;
        try (PrintWriter w = new PrintWriter(new FileWriter(out))) {
            InstructionIterator it = currentProgram.getListing().getInstructions(set, true);
            while (it.hasNext()) {
                Instruction ins = it.next();
                long off = ins.getAddress().getOffset() - start;
                String mn = ins.getMnemonicString();
                if (mn.equalsIgnoreCase("CALL")) {
                    calls++;
                    Address[] flows = ins.getFlows();
                    String tgt = "indirect";
                    if (flows != null && flows.length > 0) {
                        Symbol sym = getSymbolAt(flows[0]);
                        tgt = sym != null ? sym.getName() : ("FUN_" + flows[0]);
                    }
                    w.println("call +0x" + Long.toHexString(off) + " " + tgt);
                }
                for (int op = 0; op < ins.getNumOperands(); op++) {
                    for (Object o : ins.getOpObjects(op)) {
                        if (!(o instanceof Address)) continue;
                        Address ad = (Address) o;
                        StringBuilder sb = new StringBuilder();
                        try {
                            for (int i = 0; i < 64; i++) {
                                byte b = mem.getByte(ad.add(i));
                                if (b == 0) break;
                                if (b < 32 || b >= 127) { sb.setLength(0); break; }
                                sb.append((char) b);
                            }
                        } catch (Exception ex) { sb.setLength(0); }
                        if (sb.length() >= 3) w.println("str  +0x" + Long.toHexString(off) + " \"" + sb + "\"");
                    }
                }
            }
        }
        println("ListCallsIn: " + calls + " calls written to " + out);
    }
}
