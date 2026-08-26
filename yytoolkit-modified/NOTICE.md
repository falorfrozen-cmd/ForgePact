# Modified YYToolkit — corresponding source notice

The `YYToolkit.dll` shipped in the ForgePact release is **YYToolkit, modified**, and is
covered by **AGPL-3.0** (https://github.com/AurieFramework/YYToolkit).

## What we changed
Two source files, both included in this folder:

### 1. `YYToolkit/source/YYTK/Module Internals/GameMaker/Generic/Generic-RunnerInterfaceNew.cpp`
Adds a **disk cache** for the runner-interface lookup. Stock YYToolkit disassembles the
whole game `.text` (~1 minute) on every launch to find the RunnerInterface init point;
our version caches that offset to `<exe>.yytkcache` (keyed by exe size, so it
auto-invalidates on a game update) and skips the scan on later launches.

### 2. `YYToolkit/source/YYTK/Module Internals/Hooks/Hooks.cpp`
**Disables the `ExecuteIt` hook** (kept here as `source/YYTK/Hooks.cpp`).

On Hero Siege Season 10 that hook corrupted the game's instance references — asset refs
are `RValue` kind 15 with the index packed as `(tag << 32) | id`, and routing them through
the hook produced a fatal `Unable to find any instance for object index 'NNNNNN'`, which
showed up as infinite loading and hard crashes.

`ExecuteIt` only feeds YYToolkit's `EVENT_OBJECT_CALL`. `EVENT_FRAME`, which is what
BloodPactPlugin actually uses, comes from `HkPresent` and is unaffected — so nothing we
rely on is lost. The `m_CodeExecute` pointer is still populated for anything else that
reads it; only the hook installation is skipped.

Everything else is **unmodified upstream YYToolkit**.

## Corresponding source (how to reproduce the DLL)
To reproduce the shipped DLL:

1. Clone upstream YYToolkit: https://github.com/AurieFramework/YYToolkit
2. Replace these two files with the copies in this folder:
   - `YYToolkit/source/YYTK/Module Internals/GameMaker/Generic/Generic-RunnerInterfaceNew.cpp`
   - `YYToolkit/source/YYTK/Module Internals/Hooks/Hooks.cpp`
3. Build with YYToolkit's normal build process (we used MSVC toolset 14.50,
   `cl /std:c++latest /MD /LD`).

The upstream repository plus these two modified files together form the complete
corresponding source for the distributed `YYToolkit.dll`, as required by AGPL-3.0.

The shipped binary is `modfiles_shipped/YYToolkit.dll`, 904 192 bytes.
