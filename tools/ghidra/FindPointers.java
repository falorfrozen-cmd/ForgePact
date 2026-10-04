// FindPointers.java - for each address argument, scan the initialised data
// blocks for qwords holding that address (a registration table entry) and
// print the neighbouring qwords, each interpreted as a pointer to ASCII when
// it is one. Read-only. Usage: -postScript FindPointers.java 0xADDR ...
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.mem.Memory;
import ghidra.program.model.mem.MemoryBlock;

import java.util.HashSet;
import java.util.Set;

public class FindPointers extends GhidraScript {
    private Memory mem;

    @Override
    public void run() throws Exception {
        mem = currentProgram.getMemory();
        Set<Long> targets = new HashSet<>();
        for (String a : getScriptArgs()) targets.add(Long.parseUnsignedLong(a.replaceFirst("^0[xX]", ""), 16));
        int hits = 0;
        for (MemoryBlock b : mem.getBlocks()) {
            if (!b.isInitialized() || b.isExecute()) continue;
            long start = b.getStart().getOffset(), size = b.getSize();
            int chunk = 4 << 20;
            byte[] buf = new byte[chunk + 8];
            for (long off = 0; off < size; off += chunk) {
                int n = (int) Math.min(buf.length, size - off);
                mem.getBytes(toAddr(start + off), buf, 0, n);
                int limit = Math.min(n - 8, chunk);
                for (int i = 0; i + 8 <= limit; i += 8) {
                    long q = 0;
                    for (int k = 7; k >= 0; k--) q = (q << 8) | (buf[i + k] & 0xff);
                    if (!targets.contains(q)) continue;
                    long at = start + off + i;
                    StringBuilder sb = new StringBuilder("HIT 0x" + Long.toHexString(q) + " stored at 0x" + Long.toHexString(at) + " (" + b.getName() + ")");
                    for (int d = -4; d <= 4; d++) {
                        long na = at + d * 8L;
                        try {
                            long v = mem.getLong(toAddr(na));
                            sb.append("\n   [" + (d >= 0 ? "+" : "") + d + "] 0x" + Long.toHexString(v));
                            String s = ascii(v);
                            if (s != null) sb.append("  -> \"" + s + "\"");
                        } catch (Exception e) { /* edge */ }
                    }
                    println(sb.toString());
                    if (++hits > 40) { println("FindPointers: stopping after 40 hits"); return; }
                }
            }
        }
        println("FindPointers: " + hits + " hit(s)");
    }

    private String ascii(long v) {
        try {
            Address ad = toAddr(v);
            if (mem.getBlock(ad) == null) return null;
            StringBuilder sb = new StringBuilder();
            for (int i = 0; i < 80; i++) {
                byte c = mem.getByte(ad.add(i));
                if (c == 0) return sb.length() >= 2 ? sb.toString() : null;
                if (c < 0x20 || c > 0x7e) return null;
                sb.append((char) c);
            }
        } catch (Exception e) { return null; }
        return null;
    }
}
