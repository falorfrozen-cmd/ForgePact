@echo off
rem Build with MSVC toolset 14.29 (matches the YYToolkit.dll the game uses).
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" -vcvars_ver=14.29.30133 >nul
cd /d "C:\Users\falor\Downloads\bp_plugin"
cl /nologo /std:c++20 /EHsc /MD /LD /O2 ^
   /I "include" ^
   source\ModuleMain.cpp ^
   "include\YYToolkit\YYTK_Shared_Types.cpp" ^
   /Fe:BloodPactPlugin.dll ^
   /link /DLL user32.lib
echo EXITCODE=%ERRORLEVEL%
