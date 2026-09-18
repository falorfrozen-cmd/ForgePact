@echo off
setlocal
cd /d "%~dp0"
echo Preparing ForgePact's local plugin files...
echo This needs Python, Visual Studio C++ Build Tools, and the toolkit checkout.
if not exist "..\hs-game-sdk\cpp\include\hs_game_sdk\hs_game_sdk.hpp" (
    echo ERROR: hs-game-sdk is missing. Use ForgePact inside the full hero-siege-offline-toolkit checkout.
    goto :failed
)
py tools\fetch_toolchain.py
if errorlevel 1 goto :failed
call plugin_build\build.bat release
if errorlevel 1 goto :failed
echo.
echo Ready. Open src\forgepact.py and click Install Mod Plugin with the game closed.
pause
exit /b 0
:failed
echo.
echo Preparation failed. No game files were changed. See the error above.
pause
exit /b 1
