@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d "C:\Users\falor\Downloads\bp_plugin"
cl /nologo /std:c++20 /EHsc /MD /LD /O2 ^
   /I "include" ^
   source\ModuleMain.cpp ^
   "include\YYToolkit\YYTK_Shared_Types.cpp" ^
   /Fe:BloodPactPlugin.dll ^
   /link /DLL user32.lib
echo EXITCODE=%ERRORLEVEL%
