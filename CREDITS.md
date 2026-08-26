# Credits & License Notices

ForgePact loads Aurie Framework and YYToolkit at runtime; both are AGPL-3.0, so
this project is released under the same license. The `native_s10/` tree is an
earlier self-contained runtime that did not use them - it is no longer the
shipping path and remains here for its research notes and tools.

## Aurie Framework
- Repository: https://github.com/AurieFramework/Aurie
- License: AGPL-3.0
- Used as: the mod loader / framework. The compiled `AurieCore.dll` and the
  `AuriePatcher.exe` shipped in the release come from Aurie Framework, **unmodified**.
  Their corresponding source is available at the repository above.

## YYToolkit (YYTK)
- Repository: https://github.com/AurieFramework/YYToolkit
- License: AGPL-3.0
- Used as: the GameMaker runtime interface our plugin links against. The
  `YYToolkit.dll` shipped in the release is built from YYToolkit with **one modified
  source file** (a startup-time disk cache for the runner-interface lookup). That
  modification and build/upstream notes are in `yytoolkit-modified/`.

## This project (ForgePact + BloodPactPlugin)
- The control panel (`src/forgepact.py`) and the mod plugin (`plugin/ModuleMain.cpp`)
  are original work, released under **AGPL-3.0** to satisfy the copyleft of the
frameworks above.

## MinHook
- Repository: https://github.com/TsudaKageyu/minhook
- License: BSD 2-Clause
- Used by: the patcherless Season 10 native runtime for targeted x64 trampolines.
- Vendored license: `native_s10/third_party/minhook/LICENSE.txt`.

---
The project continues to be distributed under AGPL-3.0. The full license text
is in `LICENSE`; MinHook retains its separate BSD 2-Clause notice.

ForgePact is an independent, fan-made project. It is **not affiliated with, sponsored
by, or endorsed by** AurieFramework, Panic Art Studios, or Hero Siege. All trademarks
belong to their respective owners.
