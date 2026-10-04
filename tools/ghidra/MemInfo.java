// MemInfo.java - list memory blocks, then for each address argument say which
// block holds it, print the qword stored there (if initialized) and, if that
// qword points at printable ASCII inside the image, the text. Read-only.
// Usage: -postScript MemInfo.java 0xADDR ...
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.mem.Memory;
import ghidra.program.model.mem.MemoryBlock;

public class MemInfo extends GhidraScript {
    @Override
    public void run() throws Exception {
        Memory mem = currentProgram.getMemory();
        for (MemoryBlock b : mem.getBlocks()) {
            println("BLOCK " + b.getName() + " " + b.getStart() + "-" + b.getEnd() + " init=" + b.isInitialized()
                    + " r=" + b.isRead() + " w=" + b.isWrite() + " x=" + b.isExecute());
        }
        for (String a : getScriptArgs()) {
            Address ad = toAddr(Long.parseUnsignedLong(a.replaceFirst("^0[xX]", ""), 16));
            MemoryBlock b = mem.getBlock(ad);
            String line = "ADDR " + ad + " block=" + (b == null ? "none" : b.getName() + " init=" + b.isInitialized());
            try {
                long q = mem.getLong(ad);
                line += " qword=0x" + Long.toHexString(q);
                Address t = toAddr(q);
                if (mem.getBlock(t) != null) {
                    StringBuilder sb = new StringBuilder();
                    for (int i = 0; i < 120; i++) {
                        byte c = mem.getByte(t.add(i));
                        if (c == 0) break;
                        if (c < 0x20 || c > 0x7e) { sb.setLength(0); sb.append("<non-ascii>"); break; }
                        sb.append((char) c);
                    }
                    line += " -> \"" + sb + "\"";
                }
            } catch (Exception e) { line += " (unreadable: " + e.getMessage() + ")"; }
            println(line);
        }
    }
}
