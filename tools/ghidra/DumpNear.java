// DumpNear.java - read-only: for each 0xADDR argument print the qword stored
// there, and the bytes from ADDR-96 to ADDR+16 as ASCII (dots for non-printable),
// to identify a GameMaker builtin-table entry (name[64] precedes the function
// pointer) or an RValue constant. Local research only.
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.mem.Memory;

public class DumpNear extends GhidraScript {
    @Override
    public void run() throws Exception {
        Memory mem = currentProgram.getMemory();
        for (String a : getScriptArgs()) {
            long off = Long.parseUnsignedLong(a.substring(2), 16);
            Address addr = toAddr(off);
            StringBuilder sb = new StringBuilder();
            try {
                long q = mem.getLong(addr);
                sb.append("qword=0x").append(Long.toHexString(q)).append(' ');
                if (q > 0x140000000L && q < 0x160000000L) {
                    StringBuilder s2 = new StringBuilder();
                    for (int i = 0; i < 48; i++) { byte b = mem.getByte(toAddr(q + i)); if (b == 0) break; s2.append((b >= 32 && b < 127) ? (char) b : '.'); }
                    sb.append("-> \"").append(s2).append("\" ");
                }
            } catch (Exception e) { sb.append("qword=? "); }
            StringBuilder asc = new StringBuilder();
            for (long i = off - 96; i < off + 16; i++) {
                try { byte b = mem.getByte(toAddr(i)); asc.append((b >= 32 && b < 127) ? (char) b : '.'); } catch (Exception e) { asc.append('?'); }
            }
            println("DumpNear " + a + ": " + sb + " | " + asc);
        }
    }
}
