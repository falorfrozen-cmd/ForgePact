# Ghidra scripts

Our own Ghidra scripts for reading the game's executable locally. `ImportSymbols.java`
names the stripped `Hero_Siege.exe` once; every other script here only reads the
named project and prints, or writes files to a directory you give it. Run them with
`-readOnly`, and none of them saves anything to the project: `DecompileTo*` and
`DecompileAround` create functions in the session, which `-readOnly` discards.

Until 2026-10-04 the reading scripts lived in one researcher's `ghidra_scripts`
folder, so nobody else could repeat a reading. They are here so that a second
researcher can run the same reads.

## The scripts

| Script | What it does |
|---|---|
| `ImportSymbols.java` | Names the functions of a stripped `Hero_Siege.exe` from the game's own runtime script table (`symbols.csv`, written by `citrace symdump`). Run once, at import; the only script here that changes the project. Its header has the full procedure. |
| `DecompileTo.java` | Decompiles named functions (or addresses) into one local `.c` file each, with a header naming the function, the strings its instructions reference and its named callees. A leading `+` also decompiles the unnamed direct callees. 180 s per function. |
| `DecompileToLong.java` | `DecompileTo.java` with a 1800 s timeout per function. |
| `DecompileToHuge.java` | `DecompileToLong.java` with the decompiler's instruction and payload limits raised too. Prefer the defaults: see "Limits" below. |
| `DecompileAround.java` | Decompiles the function that contains each given address (a call site inside a region the `-noanalysis` import left without a function), by trying function starts back from it. |
| `ListCallsIn.java` | For a region the decompiler cannot finish, lists every call target in it (by name where the project has one) and the strings its instructions reference, to a file. Produces no decompiled text. |
| `FindCallers.java` | Lists the direct `call rel32` and `jmp rel32` sites that reach a named function or address, with the function that contains each. The hits are candidate sites: it scans bytes, not decoded instructions, so a hit may fall inside another instruction; confirm one with `DecompileAround.java` before relying on it. A zero means no direct site was found, not that the function is never called: before reading it as "not called", run `FindCallers.java` in the same project on a function known to have a direct caller (a positive control), and remember that calls through the script table, a pointer global or a method value are invisible to it. |
| `FindSlotNames.java` | Recovers the variable-slot and builtin-pointer globals from the startup code that registers them by name, and writes global, name and site to a CSV. |
| `FindWrites.java` | Lists the instructions that store to, or take the address of, a given global, with the bytes around each site. |
| `FindPointers.java` | Finds where an address is stored in the initialised data (a registration table entry) and prints the neighbouring entries. |
| `FindRvaTable.java` | Finds named strings in the data, and the 32-bit image-relative offsets and 64-bit pointers that refer to a given address. |
| `DerefNear.java` | Prints the eight-byte values around an address and, for each that points into the image, the text it points at, one level deep. |
| `DumpNear.java` | Prints the value stored at an address and the bytes just before it as text, to read a builtin-table entry or a constant. |
| `MemInfo.java` | Lists the memory blocks, then says which block holds each given address and what is stored there. |
| `PrintSymbolAddrs.java` | Prints the address of each named function, without decompiling. |

## Before you run one

- Ghidra and a JDK 21. The headless launcher is `support\analyzeHeadless.bat` in the
  Ghidra install.
- A project holding `Hero_Siege.exe` named by `ImportSymbols.java`. Its header gives the
  import command and the `citrace symdump` step that writes the symbols it reads. The
  launcher cannot take a path containing `(x86)`, so import a copy of the executable
  from a plain path.

## Running one headless

Run every reading script against the named project with `-readOnly`, so the session is
never saved, and point `-scriptPath` at this folder in a toolkit checkout:

```bat
set JAVA_HOME=C:\Program Files\Eclipse Adoptium\jdk-21.0.12.101-hotspot
"%GHIDRA%\support\analyzeHeadless.bat" C:\Users\<you>\ghidra_projects HeroSiege ^
    -process Hero_Siege.exe -noanalysis -readOnly ^
    -scriptPath <checkout>\ForgePact\tools\ghidra ^
    -postScript DecompileTo.java <outdir> SaveStash
```

Each script's header gives its own arguments. Keep the command in a small `.cmd`
wrapper on your own machine; the machine paths in it are yours, which is why no
wrapper is committed here.

## Recording the output

The four decompiling scripts (`DecompileTo`, `DecompileToLong`, `DecompileToHuge`,
`DecompileAround`) end their run by printing the line that records what they wrote in
the hub's local decompile index, with the output directory filled in. Run it from the
toolkit checkout's root, or add it to your wrapper after `analyzeHeadless`:

```bat
py -3 tools/decomp_index.py scan "<outdir>"
```

Then `py -3 tools/decomp_index.py has <name>` answers, before the next Ghidra run,
whether that script has already been decompiled for this build and where the file is.
The scripts only print the line; they never run the index or read a hub path, so they
behave the same in a standalone ForgePact clone. The hub's `docs/tools/decomp-index.md`
describes the index.

## The output never enters a repository

Decompiled text is research material. The hub's `AGENTS.md` section "Legal: Decompiled
Output Never Reaches Any Origin" applies to this repository's own remote as much as to
the hub's: no decompiled or disassembled body goes into a tracked file, a commit message,
an issue or a pull request. Write what a reading established in your own words, in
`docs/RUNTIME_DATA_MODELS.md` in the hub or a research doc here.

So give every script an output directory outside every repository, such as
`C:\Users\<you>\tools\hs-decomp`. `.gitignore` ignores a `.c` file anywhere under
`tools/ghidra/` as a backstop, and the hub's index tool refuses to write its index
inside a git tree.

## Why some scripts compare against fixed constants

`DecompileTo.java`, `DecompileToLong.java`, `DecompileToHuge.java` and `DumpNear.java`
keep a value only when it lies in a range around the default 64-bit image base of a
Windows executable, which is where any pointer into the image falls. That range is a
property of the file format, not an address in a game build, and it stays right when the
game is rebuilt. No script carries an address of this game; every address is an argument.

## Limits

Keep the decompiler's default limits. `LoadAllModifiers` does not decompile within them,
and raising them (`DecompileToHuge.java`) still ended in "Decompiler process died", with
`decompile.exe` observed past 22 GB. When a function exceeds the defaults, read the call
sites around it (`FindCallers.java`, `ListCallsIn.java`) or measure it live instead. The
hub's `docs/agents/static-model-workflow.md`, "Tooling findings", has the details.
