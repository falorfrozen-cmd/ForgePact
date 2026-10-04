// DerefNear.java - for each 0xADDR argument print the qwords from -0x40 to +0x40 in 8-byte
// steps, and for each qword that points into the image, the ASCII at it and at the qword it
// points to (one level), so a {fnptr, name-record} table entry can be read. Read-only.
// Usage: -postScript DerefNear.java 0xADDR ...
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.mem.Memory;

public class DerefNear extends GhidraScript {
    private Memory mem;
    @Override
    public void run() throws Exception {
        mem = currentProgram.getMemory();
        for (String a : getScriptArgs()) {
            long at = Long.parseUnsignedLong(a.replaceFirst("^0[xX]", ""), 16);
            println("DerefNear 0x" + Long.toHexString(at));
            for (long d = -0x40; d <= 0x40; d += 8) {
                long na = at + d;
                try {
                    long v = mem.getLong(toAddr(na));
                    StringBuilder sb = new StringBuilder("  [" + off(d) + "] 0x" + Long.toHexString(v));
                    String s = ascii(v); if (s != null) sb.append("  -> \"" + s + "\"");
                    else if (mem.getBlock(toAddr(v)) != null) {
                        try { long v2 = mem.getLong(toAddr(v)); sb.append("  -> q=0x" + Long.toHexString(v2)); String s2 = ascii(v2); if (s2 != null) sb.append(" -> \"" + s2 + "\"");
                              long v3 = mem.getLong(toAddr(v + 8)); sb.append(" q+8=0x" + Long.toHexString(v3)); String s3 = ascii(v3); if (s3 != null) sb.append(" -> \"" + s3 + "\""); } catch (Exception e) {}
                    }
                    println(sb.toString());
                } catch (Exception e) { println("  [" + off(d) + "] unreadable"); }
            }
        }
    }
    private static String off(long d) { return (d >= 0 ? "+" : "-") + Long.toHexString(Math.abs(d)); }

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
