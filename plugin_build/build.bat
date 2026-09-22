@echo off
REM ForgePact plugin compiler.  Source and build environment live in the same
REM folder tree; no dependency on Downloads.
REM   build.bat release  -> BloodPactPlugin_ship.dll (the one players get)
REM   build.bat          -> BloodPactPlugin_ship.dll (player default)
REM   build.bat dev      -> BloodPactPlugin_rel.dll  (research, all commands)
REM   build.bat profile  -> BloodPactPlugin_profile.dll (bounded local CPU timings)
setlocal
REM Compiler discovery, in order:
REM   1. Already-initialised MSVC environment (a caller that already ran
REM      vcvars, e.g. this repo's own tests via vswhere).
REM   2. vswhere, asking for whichever VS install actually has the C++ tools
REM      component -- this is what finds VS 18.9 Enterprise on a GitHub
REM      windows-2025-vs2026 runner, which none of the four hardcoded paths
REM      below match (they were written against VS 2022 / VS 18 BuildTools).
REM   3. The four hardcoded paths, unchanged, as a last-resort fallback.
if defined VSCMD_VER (
    where cl >nul 2>nul
    if not errorlevel 1 (
        echo using already-initialised MSVC environment ^(VSCMD_VER=%VSCMD_VER%^)
        goto :have_vs
    )
)

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" goto :try_legacy_paths
for /f "usebackq tokens=* delims=" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
    set "VSWHERE_PATH=%%i"
)
if not defined VSWHERE_PATH goto :try_legacy_paths
if not exist "%VSWHERE_PATH%\VC\Auxiliary\Build\vcvars64.bat" goto :try_legacy_paths
set "VS=%VSWHERE_PATH%\VC\Auxiliary\Build\vcvars64.bat"
goto :call_vcvars

:try_legacy_paths
if exist "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat" (
    set "VS=C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
) else if exist "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" (
    set "VS=C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"
) else if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" (
    set "VS=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
) else if exist "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" (
    set "VS=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
) else (
    echo ERROR: Visual Studio not found
    exit /b 1
)

:call_vcvars
echo using %VS%
call "%VS%" >nul
:have_vs
cd /d "%~dp0"
set "SOURCE=%~dp0..\plugin\ModuleMain.cpp"
if /I "%~1"=="dev" (
    set "FLAGS="
    set "OUTPUT=BloodPactPlugin_rel.dll"
    set "OBJDIR=obj_dev"
) else (
    set "FLAGS=/DFORGEPACT_RELEASE"
    set "OUTPUT=BloodPactPlugin_ship.dll"
    set "OBJDIR=obj_ship"
)
if /I "%~1"=="profile" (
    set "FLAGS=/DFORGEPACT_RELEASE /DFORGEPACT_POPULATION_PROFILE"
    set "OUTPUT=BloodPactPlugin_profile.dll"
    set "OBJDIR=obj_profile"
)
if not exist "%OBJDIR%" mkdir "%OBJDIR%"
REM YYTK_DEFINE_INTERNAL exposes YYToolkit's real struct bodies (CScriptRef,
REM YYObjectBase, CInstance) instead of the opaque stand-ins.  InvokeMethodValue
REM needs CScriptRef to read a method value's own callable, which is what lets
REM the quest collect run without a hardcoded game address.  It MUST be on the
REM whole command line, not a #define in ModuleMain.cpp: YYTK_Shared_Types.cpp
REM is a separate translation unit and the two must agree on the layouts, or
REM they disagree about sizeof(CInstance) and the link is quietly wrong.
cl /nologo /std:c++20 /EHsc /MD /LD /O2 /bigobj /DNDEBUG /DYYTK_DEFINE_INTERNAL=1 %FLAGS% /I "include" /I "%~dp0..\plugin\include" /I "%~dp0..\..\hs-game-sdk\cpp\include" "%SOURCE%" "include\YYToolkit\YYTK_Shared_Types.cpp" /Fe:%OUTPUT% /Fo:%OBJDIR%\ /link /DLL user32.lib
if errorlevel 1 ( echo BUILD FAILED & exit /b 1 )
if "%OUTPUT%"=="BloodPactPlugin_ship.dll" (
    copy /y "%OUTPUT%" "..\modfiles_shipped\BloodPactPlugin.dll"
    if errorlevel 1 ( echo ERROR: could not stage BloodPactPlugin.dll & exit /b 1 )
    if exist "..\dist\ForgePact\modfiles" (
        copy /y "%OUTPUT%" "..\dist\ForgePact\modfiles\BloodPactPlugin.dll"
        if errorlevel 1 ( echo ERROR: could not update dist\ForgePact\modfiles\BloodPactPlugin.dll & exit /b 1 )
    )
)
echo.
echo DONE -^> %OUTPUT%
