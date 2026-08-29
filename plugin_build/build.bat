@echo off
REM ForgePact plugin compiler.  Source and build environment live in the same
REM folder tree; no dependency on Downloads.
REM   build.bat release  -> BloodPactPlugin_ship.dll (the one players get)
REM   build.bat          -> BloodPactPlugin_rel.dll  (research, all commands)
setlocal
set "VS=C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"
if not exist "%VS%" (
    echo ERROR: Visual Studio not found -^> %VS%
    exit /b 1
)
call "%VS%" >nul
cd /d "%~dp0"
set "SOURCE=%~dp0..\plugin\ModuleMain.cpp"
if /I "%~1"=="release" (
    set "FLAGS=/DFORGEPACT_RELEASE"
    set "OUTPUT=BloodPactPlugin_ship.dll"
    set "OBJDIR=obj_ship"
) else (
    set "FLAGS="
    set "OUTPUT=BloodPactPlugin_rel.dll"
    set "OBJDIR=obj_dev"
)
if not exist "%OBJDIR%" mkdir "%OBJDIR%"
cl /nologo /std:c++20 /EHsc /MD /LD /O2 %FLAGS% /I "include" "%SOURCE%" "include\YYToolkit\YYTK_Shared_Types.cpp" /Fe:%OUTPUT% /Fo:%OBJDIR%\ /link /DLL user32.lib
if errorlevel 1 ( echo BUILD FAILED & exit /b 1 )
echo.
echo DONE -^> %OUTPUT%
