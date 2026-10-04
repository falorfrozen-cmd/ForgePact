// FindWrites.java - for each address argument, scan .text for rip-relative
// stores into it (mov [rip+d],r32/r64, mov [rip+d],imm32) and rip-relative
// leas of it, and print each site with the 72 bytes before and 24 after in
// hex, so the initialisation shape can be read. Read-only.
// Usage: -postScript FindWrites.java 0xADDR ...
import ghidra.app.script.GhidraScript;
import ghidra.program.model.mem.Memory;
import ghidra.program.model.mem.MemoryBlock;

import java.util.HashSet;
import java.util.Set;

public class FindWrites extends GhidraScript {
    @Override
    public void run() throws Exception {
        Memory mem = currentProgram.getMemory();
        MemoryBlock text = mem.getBlock(".text");
        Set<Long> targets = new HashSet<>();
        for (String a : getScriptArgs()) targets.add(Long.parseUnsignedLong(a.replaceFirst("^0[xX]", ""), 16));
        long start = text.getStart().getOffset();
        long size = text.getSize();
        int chunk = 4 << 20, overlap = 128;
        byte[] buf = new byte[chunk + overlap];
        int hits = 0;
        for (long off = 0; off < size; off += chunk) {
            int n = (int) Math.min(buf.length, size - off);
            mem.getBytes(toAddr(start + off), buf, 0, n);
            int limit = Math.min(n, chunk);
            for (int i = 0; i + 10 <= limit; i++) {
                long ip = start + off + i;
                int len = 0; String kind = null;
                // modrm 05 = [rip+disp32] with reg field 0; other reg fields: 0D,15,1D,25,2D,35,3D
                if (isRipModrm(buf[i + 1]) && (buf[i] == (byte) 0x89 || buf[i] == (byte) 0x8B || buf[i] == (byte) 0x8D)) { len = 6; kind = opname(buf[i]); }
                else if ((buf[i] == 0x48 || buf[i] == 0x4C || buf[i] == 0x44 || buf[i] == 0x49) && isRipModrm(buf[i + 2])
                        && (buf[i + 1] == (byte) 0x89 || buf[i + 1] == (byte) 0x8B || buf[i + 1] == (byte) 0x8D)) { len = 7; kind = "rex " + opname(buf[i + 1]); }
                else if (buf[i] == (byte) 0xC7 && buf[i + 1] == 0x05) { len = 10; kind = "mov imm32"; }
                else if (buf[i] == 0x48 && buf[i + 1] == (byte) 0xC7 && buf[i + 2] == 0x05) { len = 11; kind = "rex mov imm32"; }
                if (len == 0) continue;
                int dispAt = (len == 6) ? i + 2 : (len == 7) ? i + 3 : (len == 10) ? i + 2 : i + 3;
                long disp = (long) ((buf[dispAt] & 0xff) | (buf[dispAt + 1] & 0xff) << 8 | (buf[dispAt + 2] & 0xff) << 16 | (buf[dispAt + 3] & 0xff) << 24);
                long target = ip + len + disp;
                if (!targets.contains(target)) continue;
                if (kind.contains("reg,[rip]")) continue;
                hits++;
                StringBuilder sb = new StringBuilder();
                sb.append("HIT target=0x").append(Long.toHexString(target)).append(" site=0x").append(Long.toHexString(ip)).append(" kind=").append(kind).append("\n  before:");
                for (int k = Math.max(0, i - 72); k < i; k++) sb.append(' ').append(String.format("%02x", buf[k] & 0xff));
                sb.append("\n  at+after:");
                for (int k = i; k < Math.min(n, i + 24); k++) sb.append(' ').append(String.format("%02x", buf[k] & 0xff));
                println(sb.toString());
                if (hits > 60) { println("FindWrites: stopping after 60 hits"); return; }
            }
        }
        println("FindWrites: " + hits + " hit(s)");
    }

    private static boolean isRipModrm(byte m) {
        int v = m & 0xff;
        return (v & 0xC7) == 0x05;
    }

    private static String opname(byte op) {
        switch (op & 0xff) { case 0x89: return "mov [rip],reg"; case 0x8B: return "mov reg,[rip]"; case 0x8D: return "lea reg,[rip]"; default: return "?"; }
    }
}
