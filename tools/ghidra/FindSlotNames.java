// FindSlotNames.java - recover the variable-slot and builtin-pointer table of
// the YYC build from its own initialisation code, read-only.
//
// YYC reads every instance/struct/global variable through a slot number the
// runtime hands out at startup (Code_Variable_Find_Slot_From_Name), stored in
// a global the file initialises to -1; builtin functions are called through a
// pointer global the file initialises to 0. So such reads show an unnamed
// global where the variable's name should be. The startup
// code that fills those globals is a run of
//     lea rcx,[rip+"name"]   (48 8D 0D disp32)
//     call <find>            (E8 rel32)
//     mov [rip+global],eax   (89 05 disp32)  or  mov [rip+global],rax (48 89 05)
// or, for functions, a name in rdx and the output global's address in rcx.
// This script scans .text for those shapes and writes global,name,site to a
// CSV. Usage: -postScript FindSlotNames.java <out csv>
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.mem.Memory;
import ghidra.program.model.mem.MemoryBlock;

import java.io.FileWriter;
import java.io.PrintWriter;
import java.util.HashMap;
import java.util.Map;

public class FindSlotNames extends GhidraScript {

    private Memory mem;
    private long rdataStart, rdataEnd;
    private Map<Long, String> stringCache = new HashMap<>();

    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length < 1) { println("FindSlotNames: usage -> <out csv>"); return; }
        mem = currentProgram.getMemory();
        MemoryBlock text = mem.getBlock(".text");
        MemoryBlock rdata = mem.getBlock(".rdata");
        MemoryBlock data = mem.getBlock(".data");
        rdataStart = rdata.getStart().getOffset();
        rdataEnd = rdata.getEnd().getOffset();
        long dataStart = data.getStart().getOffset(), dataEnd = data.getEnd().getOffset();

        long start = text.getStart().getOffset();
        long size = text.getSize();
        int chunk = 4 << 20;
        int overlap = 64;
        byte[] buf = new byte[chunk + overlap];
        int found = 0;
        try (PrintWriter w = new PrintWriter(new FileWriter(args[0]))) {
            w.println("global,name,site,shape");
            for (long off = 0; off < size; off += chunk) {
                int n = (int) Math.min(buf.length, size - off);
                mem.getBytes(toAddr(start + off), buf, 0, n);
                int limit = Math.min(n, chunk);
                for (int i = 0; i < limit && i + 7 <= n; i++) {
                    // shape A: lea rcx,[rip+str] ; (up to 16 bytes) ; call ; mov [rip+g],eax|rax
                    // No `continue` out of shape A: a lea rcx that is not shape A
                    // may still start shape B's reverse order (rcx = global first).
                    if (buf[i] == 0x48 && buf[i + 1] == (byte) 0x8D && buf[i + 2] == 0x0D) {
                        long ip = start + off + i;
                        long strAddr = ip + 7 + rel32(buf, i + 3);
                        String s = stringAt(strAddr);
                        int callAt = -1;
                        if (s != null) {
                            for (int k = i + 7; k <= i + 7 + 16 && k + 5 <= n; k++) {
                                if (buf[k] == (byte) 0xE8) { callAt = k; break; }
                            }
                        }
                        if (callAt >= 0) {
                            int m = callAt + 5;
                            long g = -1; String shape = null;
                            if (m + 6 <= n && buf[m] == (byte) 0x89 && buf[m + 1] == 0x05) {
                                g = (start + off + m) + 6 + rel32(buf, m + 2); shape = "A-eax";
                            } else if (m + 7 <= n && buf[m] == 0x48 && buf[m + 1] == (byte) 0x89 && buf[m + 2] == 0x05) {
                                g = (start + off + m) + 7 + rel32(buf, m + 3); shape = "A-rax";
                            }
                            if (g >= dataStart && g <= dataEnd) {
                                w.println("0x" + Long.toHexString(g) + "," + csv(s) + ",0x" + Long.toHexString(ip) + "," + shape);
                                found++;
                            }
                        }
                    }
                    // shape B: lea rdx,[rip+str] then lea rcx,[rip+g] (or the reverse) then call
                    if (buf[i] == 0x48 && buf[i + 1] == (byte) 0x8D && (buf[i + 2] == 0x15 || buf[i + 2] == 0x0D)
                            && i + 14 <= n && buf[i + 7] == 0x48 && buf[i + 8] == (byte) 0x8D
                            && (buf[i + 9] == 0x0D || buf[i + 9] == 0x15) && buf[i + 9] != buf[i + 2]) {
                        long ip1 = start + off + i, ip2 = ip1 + 7;
                        long t1 = ip1 + 7 + rel32(buf, i + 3);
                        long t2 = ip2 + 7 + rel32(buf, i + 10);
                        long strAddr = (buf[i + 2] == 0x15) ? t1 : t2;
                        long g = (buf[i + 2] == 0x15) ? t2 : t1;
                        String s = stringAt(strAddr);
                        if (s == null || g < dataStart || g > dataEnd) continue;
                        boolean call = false;
                        for (int k = i + 14; k <= i + 14 + 8 && k + 5 <= n; k++) if (buf[k] == (byte) 0xE8) { call = true; break; }
                        if (!call) continue;
                        w.println("0x" + Long.toHexString(g) + "," + csv(s) + ",0x" + Long.toHexString(ip1) + ",B");
                        found++;
                    }
                }
            }
        }
        println("FindSlotNames: " + found + " (global, name) pairs written to " + args[0]);
    }

    private static long rel32(byte[] b, int at) {
        return (long) ((b[at] & 0xff) | (b[at + 1] & 0xff) << 8 | (b[at + 2] & 0xff) << 16 | (b[at + 3] & 0xff) << 24);
    }

    private static String csv(String s) { return "\"" + s.replace("\"", "\"\"") + "\""; }

    private String stringAt(long a) {
        if (a < rdataStart || a > rdataEnd) return null;
        if (stringCache.containsKey(a)) return stringCache.get(a);
        String r = null;
        try {
            StringBuilder sb = new StringBuilder();
            Address ad = toAddr(a);
            for (int i = 0; i < 96; i++) {
                byte c = mem.getByte(ad.add(i));
                if (c == 0) { r = sb.length() >= 1 ? sb.toString() : null; break; }
                if (c < 0x20 || c > 0x7e) { r = null; break; }
                sb.append((char) c);
            }
        } catch (Exception e) { r = null; }
        if (r != null && !r.matches("[A-Za-z_@][A-Za-z0-9_@.$]*")) r = null;
        stringCache.put(a, r);
        return r;
    }
}
