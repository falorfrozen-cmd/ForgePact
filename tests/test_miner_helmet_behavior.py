"""Native selection/gating rules; never starts a game or touches game files."""
import os
import shutil
import subprocess
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class MinerHelmetRulesTests(unittest.TestCase):
    def test_compiled_rules(self):
        self.compile_and_run('miner_helmet_model')

    def test_real_equipment_reader(self):
        self.compile_and_run('miner_helmet_equipment')

    def test_full_runtime(self):
        self.compile_and_run('miner_helmet_runtime')

    def compile_and_run(self, name):
        output = ROOT / 'build/miner-helmet-tests'
        output.mkdir(parents=True, exist_ok=True)
        cpp = ROOT / 'tests' / (name + '.cpp')
        if name == 'miner_helmet_equipment':
            header = (ROOT / 'plugin/include/ForgePact/MinerHelmetMod.hpp').read_text()
            reader = header[header.index('inline bool Index('):header.index('inline int RewardMultiplier(')]
            template = cpp.read_text()
            cpp = output / (name + '.cpp')
            cpp.write_text(template.replace('// PRODUCTION_EQUIPMENT', reader))
        include = ROOT / 'plugin/include'
        sdk_include = ROOT.parent / 'hs-game-sdk/cpp/include'
        baseline = bool(os.environ.get('MINER_HELMET_BASELINE'))
        define = '/DMINER_HELMET_BASELINE' if baseline else ''
        binary = output / ('model.exe' if os.name == 'nt' else 'model')
        if os.name == 'nt':
            finder = Path(os.environ.get('ProgramFiles(x86)', r'C:\Program Files (x86)')) / 'Microsoft Visual Studio/Installer/vswhere.exe'
            if not finder.exists(): self.skipTest('MSVC not installed')
            install = subprocess.check_output([str(finder), '-latest', '-products', '*', '-requires',
                'Microsoft.VisualStudio.Component.VC.Tools.x86.x64', '-property', 'installationPath'], text=True).strip()
            if not install: self.skipTest('MSVC not installed')
            script = output / 'compile.cmd'
            script.write_text(f'@echo off\ncall "{install}\\VC\\Auxiliary\\Build\\vcvars64.bat" >nul\n'
                'if errorlevel 1 exit /b 1\n'
                f'cl /nologo /std:c++20 /EHsc /O2 /DFORGEPACT_RELEASE {define} /I "{include}" /I "{sdk_include}" "{cpp}" /Fe:"{binary}" /Fo:"{output / "model.obj"}"\nexit /b %errorlevel%\n')
            command = ['cmd','/d','/c',str(script)]
        else:
            compiler = shutil.which('c++')
            if not compiler: self.skipTest('C++20 compiler not installed')
            command=[compiler,'-std=c++20','-O2','-DFORGEPACT_RELEASE','-I',str(include),'-I',str(sdk_include),str(cpp),'-o',str(binary)]
            if baseline: command.append('-DMINER_HELMET_BASELINE')
        result=subprocess.run(command,cwd=output,capture_output=True,text=True)
        self.assertEqual(0,result.returncode,result.stdout+result.stderr)
        result=subprocess.run([str(binary)],capture_output=True,text=True)
        (output/(name + ('-baseline.log' if baseline else '-run.log'))).write_text(result.stdout+result.stderr)
        self.assertEqual(0,result.returncode,result.stdout+result.stderr)


if __name__ == '__main__': unittest.main()
