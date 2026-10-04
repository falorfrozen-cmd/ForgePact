// FindRvaTable.java - (1) find ASCII strings given as name= arguments in the
// initialised non-code blocks and print their addresses; (2) for each address
// given as 0x..., scan the same blocks for 32-bit RVAs (addr - image base) and
// 64-bit pointers to it and print the neighbouring dwords/qwords with any
// string they point at. Read-only.
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.mem.Memory;
import ghidra.program.model.mem.MemoryBlock;
import java.util.*;

public class FindRvaTable extends GhidraScript {
    private Memory mem; private long base;
    @Override public void run() throws Exception {
        mem = currentProgram.getMemory(); base = currentProgram.getImageBase().getOffset();
        List<String> names = new ArrayList<>(); Set<Long> targets = new HashSet<>();
        for (String a : getScriptArgs()) { if (a.startsWith("name=")) names.add(a.substring(5)); else targets.add(Long.parseUnsignedLong(a.replaceFirst("^0[xX]", ""), 16)); }
        Map<String, List<Long>> nameHits = new HashMap<>();
        for (MemoryBlock b : mem.getBlocks()) {
            if (!b.isInitialized() || b.isExecute()) continue;
            long start = b.getStart().getOffset(), size = b.getSize(); int chunk = 4 << 20; byte[] buf = new byte[chunk + 128];
            for (long off = 0; off < size; off += chunk) {
                int n = (int) Math.min(buf.length, size - off); mem.getBytes(toAddr(start + off), buf, 0, n); int limit = Math.min(n, chunk);
                for (String nm : names) {
                    byte[] pat = (nm + "\0").getBytes("US-ASCII");
                    outer: for (int i = 0; i + pat.length <= n && i < limit; i++) {
                        if (i > 0 && buf[i - 1] != 0) continue;
                        for (int k = 0; k < pat.length; k++) if (buf[i + k] != pat[k]) continue outer;
                        nameHits.computeIfAbsent(nm, x -> new ArrayList<>()).add(start + off + i);
                    }
                }
                for (int i = 0; i + 4 <= limit; i += 4) {
                    long d = (buf[i] & 0xffL) | (buf[i + 1] & 0xffL) << 8 | (buf[i + 2] & 0xffL) << 16 | (buf[i + 3] & 0xffL) << 24;
                    if (targets.contains(d + base)) println(describe("RVA", start + off + i, b.getName(), 4));
                    if (i % 8 == 0 && i + 8 <= limit) {
                        long q = d | (long) ((buf[i + 4] & 0xffL) | (buf[i + 5] & 0xffL) << 8 | (buf[i + 6] & 0xffL) << 16 | (buf[i + 7] & 0xffL) << 24) << 32;
                        if (targets.contains(q)) println(describe("PTR", start + off + i, b.getName(), 8));
                    }
                }
            }
        }
        for (Map.Entry<String, List<Long>> e : nameHits.entrySet()) { StringBuilder sb = new StringBuilder("STRING \"" + e.getKey() + "\" at"); for (Long a : e.getValue()) sb.append(" 0x" + Long.toHexString(a)); println(sb.toString()); }
        println("FindRvaTable: done");
    }
    private String describe(String kind, long at, String block, int width) throws Exception {
        StringBuilder sb = new StringBuilder("HIT " + kind + " at 0x" + Long.toHexString(at) + " (" + block + ")");
        for (int d = -6; d <= 6; d++) {
            long na = at + (long) d * width; long v;
            try { v = width == 4 ? (mem.getInt(toAddr(na)) & 0xffffffffL) : mem.getLong(toAddr(na)); } catch (Exception ex) { continue; }
            sb.append("\n   [" + (d >= 0 ? "+" : "") + d + "] 0x" + Long.toHexString(v));
            String s = ascii(width == 4 ? v + base : v); if (s != null) sb.append("  -> \"" + s + "\"");
        }
        return sb.toString();
    }
    private String ascii(long v) { try { Address ad = toAddr(v); if (mem.getBlock(ad) == null) return null; StringBuilder sb = new StringBuilder(); for (int i = 0; i < 80; i++) { byte c = mem.getByte(ad.add(i)); if (c == 0) return sb.length() >= 2 ? sb.toString() : null; if (c < 0x20 || c > 0x7e) return null; sb.append((char) c); } } catch (Exception e) { } return null; }
}
