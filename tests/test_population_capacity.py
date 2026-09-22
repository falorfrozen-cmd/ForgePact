"""Native baseline/target tests; no game process or installation is touched."""
import os
from pathlib import Path
import subprocess
import unittest

ROOT = Path(__file__).resolve().parents[1]

class PopulationCapacityTests(unittest.TestCase):
    def test_real_detours_and_existing_drop_getter_when_explicitly_supplied(self):
        library = os.environ.get('FORGEPACT_POOL_TEST_DLL')
        minhook = os.environ.get('FORGEPACT_MINHOOK_SOURCE')
        if not library or not minhook or os.name != 'nt':
            self.skipTest('Optional native library and MinHook sources not supplied')
        from test_map_reveal_behavior import implementation
        out = ROOT / 'build/population-capacity';out.mkdir(parents=True, exist_ok=True)
        (out/'population_drop_getter.inc').write_text(implementation(
            (ROOT/'plugin/ModuleMain.cpp').read_text(encoding='utf-8'),
            'static double __cdecl HookProtGet('),encoding='utf-8')
        vswhere = Path(os.environ.get('ProgramFiles(x86)', r'C:\Program Files (x86)')) / 'Microsoft Visual Studio/Installer/vswhere.exe'
        vs = subprocess.check_output([str(vswhere), '-latest', '-products', '*', '-requires',
            'Microsoft.VisualStudio.Component.VC.Tools.x86.x64', '-property', 'installationPath'], text=True).strip()
        mh = Path(minhook)
        sources = ' '.join(f'"{mh / name}"' for name in ('src/hook.c','src/buffer.c','src/trampoline.c','src/hde/hde64.c'))
        script = out/'detours.cmd'
        script.write_text(f'@echo off\ncall "{vs}\\VC\\Auxiliary\\Build\\vcvars64.bat" >nul\n'
            f'cl /nologo /std:c++20 /EHsc /O2 /DFORGEPACT_NATIVE_DETOURS /I "{ROOT / "plugin/include"}" /I "{mh / "include"}" /I "{out}" '
            f'"{ROOT / "tests/population_native_library.cpp"}" {sources} /Fe:"{out / "detours.exe"}" /Fo:"{out}/"\n',encoding='utf-8')
        build = subprocess.run(['cmd','/d','/c',str(script)],capture_output=True,text=True,timeout=90)
        self.assertEqual(build.returncode,0,build.stdout+build.stderr)
        for mode in (0,-1,10):
            with self.subTest(preexisting_getter=mode==-1,failed_hook=mode if mode>0 else None):
                run=subprocess.run([str(out/'detours.exe'),library,str(out/'local-native-cache'),str(mode)],capture_output=True,text=True,timeout=90)
                self.assertEqual(run.returncode,0,run.stdout+run.stderr)
                print(run.stdout.strip())

    def test_supported_native_library_when_explicitly_supplied(self):
        library = os.environ.get('FORGEPACT_POOL_TEST_DLL')
        if not library or os.name != 'nt':
            self.skipTest('Optional local native library not supplied')
        vswhere = Path(os.environ.get('ProgramFiles(x86)', r'C:\Program Files (x86)')) / 'Microsoft Visual Studio/Installer/vswhere.exe'
        vs = subprocess.check_output([str(vswhere), '-latest', '-products', '*', '-requires',
            'Microsoft.VisualStudio.Component.VC.Tools.x86.x64', '-property', 'installationPath'], text=True).strip()
        out = ROOT / 'build/population-capacity';out.mkdir(parents=True, exist_ok=True)
        script = out / 'native.cmd'
        script.write_text(f'@echo off\ncall "{vs}\\VC\\Auxiliary\\Build\\vcvars64.bat" >nul\n'
            f'cl /nologo /std:c++20 /EHsc /O2 /I "{ROOT / "plugin/include"}" "{ROOT / "tests/population_native_library.cpp"}" /Fe:"{out / "native.exe"}" /Fo:"{out / "native.obj"}"\n', encoding='utf-8')
        build = subprocess.run(['cmd','/d','/c',str(script)], capture_output=True,text=True,timeout=90)
        self.assertEqual(build.returncode,0,build.stdout+build.stderr)
        for fail in range(11):
            with self.subTest(failed_hook=fail):
                run = subprocess.run([str(out/'native.exe'), library,str(out/'local-native-cache'),str(fail)],capture_output=True,text=True,timeout=90)
                self.assertEqual(run.returncode,0,run.stdout+run.stderr)
                print(run.stdout.strip())

    def test_native_router_and_admission_queue(self):
        if os.name != 'nt':
            self.skipTest('Windows C++ toolchain required')
        vswhere = Path(os.environ.get('ProgramFiles(x86)', r'C:\Program Files (x86)')) / 'Microsoft Visual Studio/Installer/vswhere.exe'
        if not vswhere.exists():
            self.skipTest('MSVC unavailable')
        vs = subprocess.check_output([str(vswhere), '-latest', '-products', '*', '-requires',
            'Microsoft.VisualStudio.Component.VC.Tools.x86.x64', '-property', 'installationPath'], text=True).strip()
        if not vs:
            self.skipTest('MSVC unavailable')
        out = ROOT / 'build/population-capacity';out.mkdir(parents=True, exist_ok=True)
        script = out / 'test.cmd'
        script.write_text(f'@echo off\ncall "{vs}\\VC\\Auxiliary\\Build\\vcvars64.bat" >nul\n'
            f'cl /nologo /std:c++20 /EHsc /O2 /I "{ROOT / "plugin/include"}" "{ROOT / "tests/population_capacity.cpp"}" /Fe:"{out / "test.exe"}" /Fo:"{out / "test.obj"}"\n'
            'if errorlevel 1 exit /b 1\n'
            f'"{out / "test.exe"}"\n', encoding='utf-8')
        result = subprocess.run(['cmd','/d','/c',str(script)], capture_output=True, text=True, timeout=90)
        self.assertEqual(result.returncode,0,result.stdout+result.stderr)
        self.assertIn('queue: bounded, complete, stale entries, reset PASS',result.stdout)

if __name__ == '__main__':unittest.main()
