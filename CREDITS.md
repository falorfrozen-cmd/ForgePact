# Credits & License Notices

ForgePact is built on the following open-source projects, both licensed under the
**GNU Affero General Public License, version 3 (AGPL-3.0)**:

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

---
Because both frameworks are AGPL-3.0 (strong copyleft), the entire ForgePact
distribution is AGPL-3.0. The full license text is in `LICENSE`.

ForgePact is an independent, fan-made project. It is **not affiliated with, sponsored
by, or endorsed by** AurieFramework, Panic Art Studios, or Hero Siege. All trademarks
belong to their respective owners.
