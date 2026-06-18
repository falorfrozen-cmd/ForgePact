# Building BloodPactPlugin

`ModuleMain.cpp` is the source of **BloodPactPlugin** — the C++ mod that hooks Hero Siege
(via YYToolkit) and exposes density / drop / spawn / map-reveal / relic-gate controls over a
small file-based IPC channel that the ForgePact panel writes to.

## What you need
- **MSVC** (Visual Studio Build Tools). The build script pins toolset **14.29.30133** to
  match the YYToolkit DLL's ABI — use the matching toolset or you may get crashes.
- **YYToolkit headers** (the `YYToolkit`, `Aurie`, and `FunctionWrapper` include trees).
  These come from YYToolkit upstream — https://github.com/AurieFramework/YYToolkit
  (AGPL-3.0). Place them under an `include/` folder next to `ModuleMain.cpp`, or point the
  `/I "include"` flag in the build script at wherever you have them.

## Build
```
build_1429.bat
```
(or `build.bat` for the default toolset). Output: `BloodPactPlugin.dll`. The build script's
exact compile line:
```
cl /nologo /std:c++20 /EHsc /MD /LD /O2 /I "include" ModuleMain.cpp "include\YYToolkit\YYTK_Shared_Types.cpp" /Fe:BloodPactPlugin.dll /link /DLL user32.lib
```

## License
BloodPactPlugin is licensed **AGPL-3.0** (it links to AGPL-3.0 YYToolkit). See the top-level
`LICENSE`.
