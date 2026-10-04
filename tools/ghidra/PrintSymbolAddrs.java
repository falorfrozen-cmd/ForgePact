// PrintSymbolAddrs.java - print the address of each named function (args), no bodies.
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.Symbol;
import ghidra.program.model.symbol.SymbolIterator;

public class PrintSymbolAddrs extends GhidraScript {
    @Override
    public void run() throws Exception {
        long base = currentProgram.getImageBase().getOffset();
        println("PrintSymbolAddrs: functions in program = " + currentProgram.getFunctionManager().getFunctionCount());
        for (String name : getScriptArgs()) {
            boolean found = false;
            SymbolIterator it = currentProgram.getSymbolTable().getSymbols(name);
            while (it.hasNext()) {
                Symbol s = it.next();
                Function fn = getFunctionAt(s.getAddress());
                println("FOUND " + name + " @ " + s.getAddress() + " (rva 0x"
                        + Long.toHexString(s.getAddress().getOffset() - base) + ")"
                        + (fn != null ? " function" : " NOT-a-function"));
                found = true;
            }
            if (!found) println("MISSING " + name);
        }
    }
}
