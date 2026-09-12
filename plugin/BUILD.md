# Building BloodPactPlugin

`ModuleMain.cpp` is the source of **BloodPactPlugin** — the C++ mod that hooks Hero Siege
(via YYToolkit) and exposes density / special-content / drop-rate / key-gate / map-reveal
controls over a small file-based IPC channel that the ForgePact panel writes to.

## What you need
- **MSVC** (Visual Studio Build Tools) with the C++ desktop workload.
- **YYToolkit headers** (the `YYToolkit`, `Aurie`, and `FunctionWrapper` include trees)
  plus `YYToolkit/YYTK_Shared_Types.cpp`. These come from YYToolkit upstream —
  https://github.com/AurieFramework/YYToolkit (AGPL-3.0). Place them under an `include/`
  folder inside `plugin_build/`, next to `build.bat`.

The plugin must be built against the **same** YYToolkit headers as the `YYToolkit.dll`
you ship, or you will get crashes at load time — the interface is a raw vtable.

### `YYTK_DEFINE_INTERNAL` is required

`build.bat` passes `/DYYTK_DEFINE_INTERNAL=1`. This is not optional and not a debug
flag: it exposes YYToolkit's real struct bodies (`CScriptRef`, `YYObjectBase`,
`CInstance`) in place of the opaque stand-ins. `InvokeMethodValue` — the Pet Quest
Collector's collect call — reads a GML method value's own callable off its
`CScriptRef`, which is what lets it invoke `m_Questpickup` **without a hardcoded game
address** (see `agents.md`, "Never Call an Address You Resolved by Hand"). Without the
define the plugin does not compile.

It must be on the **whole** compile command, not a `#define` in `ModuleMain.cpp`:
`YYTK_Shared_Types.cpp` is a separate translation unit, and if the two disagree about
`sizeof(CInstance)` the link is quietly wrong rather than loudly broken. The header's
own `static_assert(sizeof(YYObjectBase) == 0x88)` is the compile-time check that the
headers still match the runtime.

## Build

```
plugin_build\build.bat
```
or
```
plugin_build\build.bat release
```

Both are equivalent (`build.bat`'s only special-cased argument is `dev`; anything else,
including no argument at all, takes this branch). Output:
`plugin_build\BloodPactPlugin_ship.dll` — the player build. Copy it to
`modfiles_shipped\BloodPactPlugin.dll`; `build_release.py` refuses to package if those
two files differ, so a stale plugin cannot ship by accident.

```
plugin_build\build.bat dev
```

Output: `plugin_build\BloodPactPlugin_rel.dll` — the research build. Same code, but
without `/DFORGEPACT_RELEASE`, so the runtime-inspection commands (`structdump`,
`readmem`, `census`, `enemylog`, `probestruct`, …) are compiled in. Use this one for
analysis; never ship it. **Bare `build.bat` (no argument) does NOT produce this build —
that was the behavior in an earlier version of the script and this doc was never updated;
you need the literal `dev` argument.**

The exact compile line is in `build.bat`; the only difference between the two builds is
the `/DFORGEPACT_RELEASE` define.

## The IPC channel

The plugin reads `<game>\bin\bp_ipc\cmd.txt` once per frame and appends replies to
`out.txt` in the same folder. One command per line. The panel writes commands there;
you can also just write to the file by hand while the game runs, which is how most of
the research in `docs/` was done.
