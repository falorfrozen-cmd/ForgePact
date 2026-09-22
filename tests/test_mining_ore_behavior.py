"""Compile the real adapter against fake game calls; never opens Hero Siege."""
import os
import shutil
import subprocess
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class MiningOreBehaviorTests(unittest.TestCase):
    def test_adapter_scenarios(self):
        out = ROOT / 'build/mining-ore-behavior'
        out.mkdir(parents=True, exist_ok=True)
        header = ROOT / 'plugin/include/ForgePact/MiningOreMod.hpp'
        if os.environ.get('MINING_ORE_BASELINE'):
            production = '''namespace ForgePact::MiningOre {
int multiplier=1; bool installTried=false, ready=false, unavailable=false, loggedReward=false, loggedFailure=false;
CInstance* activeNode=nullptr; bool inReward=false;
PFUNC_YYGMLScript originalStep=nullptr, originalLoot=nullptr;
void Command(const std::string&){}
RValue& HookStep(CInstance* s,CInstance* o,RValue& r,int n,RValue** a){return originalStep(s,o,r,n,a);}
RValue& HookLoot(CInstance* s,CInstance* o,RValue& r,int n,RValue** a){return originalLoot(s,o,r,n,a);}
}'''
        else:
            production = '\n'.join(line for line in header.read_text(encoding='utf8').splitlines()
                                   if not line.startswith(('#include', '#pragma')))
        cpp = out / 'mining.cpp'
        cpp.write_text((ROOT / 'tests/mining_ore_harness.cpp').read_text(encoding='utf8')
                       .replace('// PRODUCTION_MINING_ORE', production), encoding='utf8')
        binary = out / ('mining.exe' if os.name == 'nt' else 'mining')
        include = ROOT.parent / 'hs-game-sdk/cpp/include'
        if os.name == 'nt':
            vswhere = Path(os.environ.get('ProgramFiles(x86)', r'C:\Program Files (x86)')) / 'Microsoft Visual Studio/Installer/vswhere.exe'
            if not vswhere.is_file():
                raise unittest.SkipTest('MSVC is needed for native behavior tests')
            install = subprocess.check_output([str(vswhere), '-latest', '-products', '*', '-requires',
                       'Microsoft.VisualStudio.Component.VC.Tools.x86.x64', '-property', 'installationPath'], text=True).strip()
            if not install: raise unittest.SkipTest('MSVC not installed')
            batch = out / 'compile.cmd'
            batch.write_text(f'@echo off\ncall "{install}\\VC\\Auxiliary\\Build\\vcvars64.bat" >nul\n'
                             'if errorlevel 1 exit /b 1\n'
                             f'cl /nologo /std:c++20 /EHsc /O2 /DFORGEPACT_RELEASE /I "{include}" "{cpp}" /Fe:"{binary}" /Fo:"{out / "mining.obj"}"\n'
                             'exit /b %errorlevel%\n', encoding='utf8')
            command = ['cmd', '/d', '/c', str(batch)]
        else:
            compiler = shutil.which('c++')
            if not compiler: raise unittest.SkipTest('C++20 compiler needed')
            command = [compiler, '-std=c++20', '-O2', '-DFORGEPACT_RELEASE', '-I', str(include), str(cpp), '-o', str(binary)]
        build = subprocess.run(command, cwd=out, capture_output=True, text=True)
        (out/'compile.log').write_text(build.stdout+build.stderr, encoding='utf8')
        self.assertEqual(0, build.returncode, build.stdout+build.stderr)
        run = subprocess.run([str(binary)], capture_output=True, text=True)
        (out/('baseline.log' if os.environ.get('MINING_ORE_BASELINE') else 'run.log')).write_text(run.stdout+run.stderr, encoding='utf8')
        self.assertEqual(0, run.returncode, run.stdout+run.stderr)
        self.assertIn('RESULT OK', run.stdout)


if __name__ == '__main__':
    unittest.main()
