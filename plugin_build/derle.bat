@echo off
REM ForgePact eklenti derleyicisi.  Kaynak ve derleme ortami ayni klasor
REM agacinda; Downloads'a bagimlilik yok.
REM   derle.bat yayin  -> BloodPactPlugin_ship.dll (oyunculara giden)
REM   derle.bat        -> BloodPactPlugin_rel.dll  (arastirma, tum komutlar)
setlocal
set "VS=C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"
if not exist "%VS%" (
    echo HATA: Visual Studio bulunamadi -^> %VS%
    exit /b 1
)
call "%VS%" >nul
cd /d "%~dp0"
set "KAYNAK=%~dp0..\plugin\ModuleMain.cpp"
if /I "%~1"=="yayin" (
    set "BAYRAK=/DFORGEPACT_RELEASE"
    set "CIKTI=BloodPactPlugin_ship.dll"
    set "ARA=obj_ship"
) else (
    set "BAYRAK="
    set "CIKTI=BloodPactPlugin_rel.dll"
    set "ARA=obj_dev"
)
if not exist "%ARA%" mkdir "%ARA%"
cl /nologo /std:c++20 /EHsc /MD /LD /O2 %BAYRAK% /I "include" "%KAYNAK%" "include\YYToolkit\YYTK_Shared_Types.cpp" /Fe:%CIKTI% /Fo:%ARA%\ /link /DLL user32.lib
if errorlevel 1 ( echo DERLEME BASARISIZ & exit /b 1 )
echo.
echo TAMAM -^> %CIKTI%
