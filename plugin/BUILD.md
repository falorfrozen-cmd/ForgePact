# Building BloodPactPlugin

`ModuleMain.cpp` is the source of **BloodPactPlugin** — the C++ mod that hooks Hero Siege
(via YYToolkit) and exposes density / special-content / drop-rate / key-gate / map-reveal
controls over a small file-based IPC channel that the ForgePact panel writes to.

## What you need
- **MSVC** (Visual Studio Build Tools) with the C++ desktop workload.
- **YYToolkit headers** (the `YYToolkit`, `Aurie`, and `FunctionWrapper` include trees)
  plus `YYToolkit/YYTK_Shared_Types.cpp`. These come from YYToolkit upstream —
  https://github.com/AurieFramework/YYToolkit (AGPL-3.0). Place them under an `include/`
  folder inside `plugin_build/`, next to `derle.bat`.

The plugin must be built against the **same** YYToolkit headers as the `YYToolkit.dll`
you ship, or you will get crashes at load time — the interface is a raw vtable.

## Build

```
plugin_build\derle.bat yayin
```

Output: `plugin_build\BloodPactPlugin_ship.dll` — the player build. Copy it to
`modfiles_shipped\BloodPactPlugin.dll`; `build_release.py` refuses to package if those
two files differ, so a stale plugin cannot ship by accident.

```
plugin_build\derle.bat
```

Output: `plugin_build\BloodPactPlugin_rel.dll` — the research build. Same code, but
without `/DFORGEPACT_RELEASE`, so the runtime-inspection commands (`structdump`,
`readmem`, `census`, `enemylog`, `probestruct`, …) are compiled in. Use this one for
analysis; never ship it.

The exact compile line is in `derle.bat`; the only difference between the two builds is
the `/DFORGEPACT_RELEASE` define.

## The IPC channel

The plugin reads `<game>\bin\bp_ipc\cmd.txt` once per frame and appends replies to
`out.txt` in the same folder. One command per line. The panel writes commands there;
you can also just write to the file by hand while the game runs, which is how most of
the research in `docs/` was done.
