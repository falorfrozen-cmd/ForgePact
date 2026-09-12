@echo off
REM ForgePact plugin compiler.  Source and build environment live in the same
REM folder tree; no dependency on Downloads.
REM   build.bat release  -> BloodPactPlugin_ship.dll (the one players get)
REM   build.bat          -> BloodPactPlugin_rel.dll  (research, all commands)
setlocal
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
call "%VS%" >nul
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
if not exist "%OBJDIR%" mkdir "%OBJDIR%"
REM YYTK_DEFINE_INTERNAL exposes YYToolkit's real struct bodies (CScriptRef,
REM YYObjectBase, CInstance) instead of the opaque stand-ins.  InvokeMethodValue
REM needs CScriptRef to read a method value's own callable, which is what lets
REM the quest collect run without a hardcoded game address.  It MUST be on the
REM whole command line, not a #define in ModuleMain.cpp: YYTK_Shared_Types.cpp
REM is a separate translation unit and the two must agree on the layouts, or
REM they disagree about sizeof(CInstance) and the link is quietly wrong.
cl /nologo /std:c++20 /EHsc /MD /LD /O2 /DNDEBUG /DYYTK_DEFINE_INTERNAL=1 %FLAGS% /I "include" /I "%~dp0..\plugin\include" /I "%~dp0..\..\hs-game-sdk\cpp\include" "%SOURCE%" "include\YYToolkit\YYTK_Shared_Types.cpp" /Fe:%OUTPUT% /Fo:%OBJDIR%\ /link /DLL user32.lib
if errorlevel 1 ( echo BUILD FAILED & exit /b 1 )
if not "%OUTPUT%"=="BloodPactPlugin_rel.dll" (
    copy /y "%OUTPUT%" "..\modfiles_shipped\BloodPactPlugin.dll"
    if errorlevel 1 ( echo ERROR: could not stage BloodPactPlugin.dll & exit /b 1 )
    if exist "..\dist\ForgePact\modfiles" (
        copy /y "%OUTPUT%" "..\dist\ForgePact\modfiles\BloodPactPlugin.dll"
        if errorlevel 1 ( echo ERROR: could not update dist\ForgePact\modfiles\BloodPactPlugin.dll & exit /b 1 )
    )
)
echo.
echo DONE -^> %OUTPUT%
