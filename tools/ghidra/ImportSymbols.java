// ImportSymbols.java - name a stripped Hero_Siege.exe from the game's own
// runtime script table.
//
// Hero_Siege.exe is a YYC build: every GML script and object event is native
// x86-64 compiled into a ~280 MB executable with no symbols, so a decompiler
// shows FUN_14xxxxxxx everywhere and nothing is readable. Three separate
// research sessions stalled on exactly that (see
// ForgePact/docs/pet-quest-collector-research.md, "Native decompilation": 16
// functions decompiled, "as unreadable as expected without symbols").
//
// The fix does not need a disassembler at all. The running game already knows
// every script's name and address:
//
//     script_get_name(i)                    -> the script's name
//     GetNamedRoutinePointer("gml_Script_" + name) -> its live address
//
// ForgePact's `citrace symdump` walks the index space doing exactly that and
// writes bp_ipc\symbols.csv. This script reads that CSV and applies it to the
// Ghidra program: disassemble + create a function at each RVA if one is not
// already there, then name it. The result is a named binary, reusable by every
// future session and every other tool in this toolkit.
//
// This is our own code operating on our own generated data. Per agents.md, the
// decompiler's OUTPUT stays local and out of version control; a name/RVA table
// is an interoperability fact ("object/script names and their numeric
// indices", "function offsets needed to hook or read memory") and the script
// itself is original work.
//
// USAGE
//   1. In-game (research build):  citrace symdump
//      -> <game>\bin\bp_ipc\symbols.csv
//   2. Copy that CSV somewhere local, then run headless:
//
//      set JAVA_HOME=C:\Program Files\Eclipse Adoptium\jdk-21.0.12.101-hotspot
//      "%GHIDRA%\support\analyzeHeadless.bat" C:\Users\<you>\ghidra_projects HeroSiege ^
//          -process Hero_Siege.exe -noanalysis ^
//          -scriptPath C:\Users\<you>\ghidra_scripts -postScript ImportSymbols.java <csv path>
//
//      -noanalysis matters: a full auto-analysis pass on a 280 MB image takes
//      hours and is not needed - this creates functions only at the addresses
//      the CSV names.
//
// The CSV is index,name,rva,module with a header line; rows whose rva is blank
// (name known, address not resolvable) are skipped rather than treated as an
// error, and rows for modules other than the exe are skipped too.
import ghidra.app.script.GhidraScript;
import ghidra.app.cmd.disassemble.DisassembleCommand;
import ghidra.app.cmd.function.CreateFunctionCmd;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionManager;
import ghidra.program.model.symbol.SourceType;
import ghidra.util.task.ConsoleTaskMonitor;

import java.io.BufferedReader;
import java.io.FileReader;
import java.util.ArrayList;
import java.util.List;

public class ImportSymbols extends GhidraScript {

    // Ghidra rejects some characters that are perfectly normal in GameMaker
    // routine names - notably '@', which every anon closure uses
    // (anon@2786@gml_Object_Quest_Object_Parent_obj_Create_0). Mapping them to
    // '_' keeps the name searchable and still unique.
    private static String sanitize(String name) {
        return name.replace('@', '_').replace(' ', '_');
    }

    // Minimal CSV field splitter: handles the quoted name field, which is the
    // only one that can contain a comma.
    private static List<String> splitCsv(String line) {
        List<String> out = new ArrayList<>();
        StringBuilder cur = new StringBuilder();
        boolean inQuotes = false;
        for (int i = 0; i < line.length(); i++) {
            char c = line.charAt(i);
            if (c == '"') { inQuotes = !inQuotes; continue; }
            if (c == ',' && !inQuotes) { out.add(cur.toString()); cur.setLength(0); continue; }
            cur.append(c);
        }
        out.add(cur.toString());
        return out;
    }

    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        String csvPath = (args.length > 0)
            ? args[0]
            : "C:\\Program Files (x86)\\Steam\\steamapps\\common\\HeroSiege\\bin\\bp_ipc\\symbols.csv";

        long imageBase = currentProgram.getImageBase().getOffset();
        FunctionManager fm = currentProgram.getFunctionManager();
        ConsoleTaskMonitor mon = new ConsoleTaskMonitor();

        int rows = 0, skipped = 0, named = 0, created = 0, failed = 0;

        try (BufferedReader r = new BufferedReader(new FileReader(csvPath))) {
            String line = r.readLine();   // header
            if (line == null) { println("ImportSymbols: empty CSV at " + csvPath); return; }
            while ((line = r.readLine()) != null) {
                if (line.trim().isEmpty()) continue;
                rows++;
                List<String> f = splitCsv(line);
                if (f.size() < 3) { skipped++; continue; }
                String name = f.get(1).trim();
                String rvaStr = f.get(2).trim();
                String module = f.size() > 3 ? f.get(3).trim() : "exe";
                // Blank address = the name exists but did not resolve to a
                // live function; a non-exe module is a runtime helper DLL and
                // is not part of this program.
                if (name.isEmpty() || rvaStr.isEmpty() || !module.equals("exe")) { skipped++; continue; }

                long rva;
                try {
                    rva = Long.parseLong(rvaStr.startsWith("0x") ? rvaStr.substring(2) : rvaStr, 16);
                } catch (NumberFormatException e) { skipped++; continue; }

                Address addr = currentProgram.getAddressFactory()
                        .getDefaultAddressSpace().getAddress(imageBase + rva);

                Function fn = fm.getFunctionAt(addr);
                if (fn == null) {
                    try {
                        new DisassembleCommand(addr, null, true).applyTo(currentProgram, mon);
                        new CreateFunctionCmd(addr).applyTo(currentProgram, mon);
                        fn = fm.getFunctionAt(addr);
                        if (fn != null) created++;
                    } catch (Exception e) {
                        failed++;
                        continue;
                    }
                }
                if (fn == null) { failed++; continue; }

                try {
                    fn.setName(sanitize(name), SourceType.USER_DEFINED);
                    named++;
                } catch (Exception e) { failed++; }

                if ((rows % 500) == 0) println("  ... " + rows + " rows, " + named + " named");
            }
        }

        println("ImportSymbols: " + rows + " rows | named " + named
                + " (created " + created + " new functions) | skipped " + skipped + " | failed " + failed);
        println("  source: " + csvPath);
    }
}
