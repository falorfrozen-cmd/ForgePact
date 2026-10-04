// FindCallers.java - for each function name or 0xADDR argument, scan .text for
// direct `call rel32` (E8) and `jmp rel32` (E9) sites whose target is that
// address, and print each site with the named function or nearest preceding
// symbol that contains it. Read-only; needed because the project was imported
// with -noanalysis and so carries no cross-references. YYC compiles a script's
// direct call to another script as `call rel32`, which is what this finds; a
// call through the script table (a pointer) is not a direct site and is not
// reported here. The hits are candidate sites: this is a byte scan, not
// instruction-aligned, so confirm a site with DecompileAround.java before relying
// on it. A zero means no direct site was found, not "not called": run this in the
// same project on a function known to have a direct caller (a positive control)
// first, and remember that table, pointer-global and method-value calls are
// invisible to it. Usage: -postScript FindCallers.java <name|0xADDR> ...
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.mem.Memory;
import ghidra.program.model.mem.MemoryBlock;
import ghidra.program.model.symbol.Symbol;
import ghidra.program.model.symbol.SymbolIterator;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

public class FindCallers extends GhidraScript {
    @Override
    public void run() throws Exception {
        Memory mem = currentProgram.getMemory();
        MemoryBlock text = mem.getBlock(".text");
        Map<Long, String> targets = new HashMap<>();
        for (String a : getScriptArgs()) {
            if (a.toLowerCase().startsWith("0x")) {
                targets.put(Long.parseUnsignedLong(a.substring(2), 16), a);
            } else {
                SymbolIterator it = currentProgram.getSymbolTable().getSymbols(a);
                boolean found = false;
                while (it.hasNext()) { Symbol s = it.next(); targets.put(s.getAddress().getOffset(), a); found = true; }
                if (!found) println("FindCallers: no symbol named " + a);
            }
        }
        if (targets.isEmpty()) { println("FindCallers: nothing to look for"); return; }
        for (Map.Entry<Long, String> e : targets.entrySet())
            println("FindCallers: target " + e.getValue() + " @ 0x" + Long.toHexString(e.getKey()));

        // Sorted symbol addresses, for naming a site whose function was never created.
        List<Long> symAddrs = new ArrayList<>();
        Map<Long, String> symNames = new HashMap<>();
        SymbolIterator all = currentProgram.getSymbolTable().getAllSymbols(false);
        while (all.hasNext()) {
            Symbol s = all.next();
            long off = s.getAddress().getOffset();
            if (off < text.getStart().getOffset() || off > text.getEnd().getOffset()) continue;
            if (!symNames.containsKey(off)) { symAddrs.add(off); symNames.put(off, s.getName()); }
        }
        long[] sorted = new long[symAddrs.size()];
        for (int i = 0; i < sorted.length; i++) sorted[i] = symAddrs.get(i);
        Arrays.sort(sorted);

        long start = text.getStart().getOffset();
        long size = text.getSize();
        int chunk = 4 << 20, overlap = 16;
        byte[] buf = new byte[chunk + overlap];
        int hits = 0;
        for (long off = 0; off < size; off += chunk) {
            int n = (int) Math.min(buf.length, size - off);
            mem.getBytes(toAddr(start + off), buf, 0, n);
            int limit = Math.min(n, chunk);
            for (int i = 0; i + 5 <= limit; i++) {
                int op = buf[i] & 0xff;
                if (op != 0xE8 && op != 0xE9) continue;
                long ip = start + off + i;
                long disp = (long) ((buf[i + 1] & 0xff) | (buf[i + 2] & 0xff) << 8 | (buf[i + 3] & 0xff) << 16 | (buf[i + 4] & 0xff) << 24);
                long target = ip + 5 + disp;
                String name = targets.get(target);
                if (name == null) continue;
                hits++;
                Address site = toAddr(ip);
                String where;
                Function fn = getFunctionContaining(site);
                if (fn != null) {
                    where = fn.getName() + " (function, +0x" + Long.toHexString(ip - fn.getEntryPoint().getOffset()) + ")";
                } else {
                    int k = Arrays.binarySearch(sorted, ip);
                    if (k < 0) k = -k - 2;
                    where = k >= 0 ? symNames.get(sorted[k]) + " (nearest symbol, +0x" + Long.toHexString(ip - sorted[k]) + ")" : "?";
                }
                println("HIT " + name + " <- " + (op == 0xE8 ? "call" : "jmp") + " site=0x" + Long.toHexString(ip) + " in " + where);
                if (hits > 400) { println("FindCallers: stopping after 400 hits"); return; }
            }
        }
        println("FindCallers: " + hits + " hit(s)");
    }
}
