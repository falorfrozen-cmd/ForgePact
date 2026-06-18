# Modified YYToolkit — corresponding source notice

The `YYToolkit.dll` shipped in the ForgePact release is **YYToolkit, modified**, and is
covered by **AGPL-3.0** (https://github.com/AurieFramework/YYToolkit).

## What we changed
We modified exactly **one** source file:

- `YYToolkit/source/YYTK/Module Internals/GameMaker/Generic/Generic-RunnerInterfaceNew.cpp`
  (included here)

The change adds a **disk cache** for the runner-interface lookup: stock YYToolkit
disassembles the whole game `.text` (~1 minute) on every launch to find the
RunnerInterface init point; our version caches that offset to `<exe>.yytkcache` (keyed by
exe size, so it auto-invalidates on a game update) and skips the scan on later launches.

Everything else is **unmodified upstream YYToolkit**.

## Corresponding source (how to reproduce the DLL)
1. Clone upstream YYToolkit: https://github.com/AurieFramework/YYToolkit
2. Replace `Generic-RunnerInterfaceNew.cpp` with the copy in this folder.
3. Build with YYToolkit's normal build process (we used MSVC toolset 14.50 / `cl /std:c++latest /MD /LD`).

The upstream repository plus this one modified file together form the complete
corresponding source for the distributed `YYToolkit.dll`, as required by AGPL-3.0.
