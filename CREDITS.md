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

### Panel artwork
The bronze anvil in the panel is the original vector artwork from Falor's
Hero Siege Toolkit UI source package, `hub/src/ToolIcon.svelte` (`anvil` variant).
It is adapted inline in `src/forgepact.py`, retaining the stroke and gradient
colors. The body outline is closed and its top face is drawn continuously to
correct the missing upper-left surface. No download is needed.

The 69 setting icons in `src/panel_icons.py` are original vector illustrations
created for ForgePact/Falor and distributed under this project's AGPL-3.0
license. They depict the corresponding gameplay concepts; they are not
extracted game sprites. The export command produces standalone SVG files and
a local gallery from the same source used by the panel.

## HS Offline Launcher
- Repository: https://github.com/falorfrozen-cmd/HS-Offline-Launcher
- Source revision: `59108803f776e7dcbd9488b21b0428ea26de5617`.
- The embedded engine in `src/offline_launcher.py` reuses its Steam discovery,
  PE/runtime validation and protection/process checks. The explicit-path API,
  ForgePact plugin preflight, isolated Win32 wrapper and cached launch result
  are integration adaptations. No separate launcher application is required.
- These portions retain their MIT license, reproduced below.

MIT License

Copyright (c) 2026 falorfrozen-cmd

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

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
