"""ForgePact::ItemTruth compiled and run without a game (tests/item_truth_harness.cpp).

Writes only under the system temp folder and build/item-truth-tests."""
import os
import subprocess
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class ItemTruthBehaviorTests(unittest.TestCase):
    def test_harness(self):
        if os.name != 'nt':
            self.skipTest('ItemTruth uses the Win32 API')
        output = ROOT / 'build/item-truth-tests'
        output.mkdir(parents=True, exist_ok=True)
        finder = Path(os.environ.get('ProgramFiles(x86)', r'C:\Program Files (x86)')) / 'Microsoft Visual Studio/Installer/vswhere.exe'
        if not finder.exists():
            self.skipTest('MSVC not installed')
        install = subprocess.check_output([str(finder), '-latest', '-products', '*', '-requires',
            'Microsoft.VisualStudio.Component.VC.Tools.x86.x64', '-property', 'installationPath'], text=True).strip()
        if not install:
            self.skipTest('MSVC not installed')
        cpp = ROOT / 'tests/item_truth_harness.cpp'
        include = ROOT / 'plugin/include'
        binary = output / 'item_truth.exe'
        script = output / 'compile.cmd'
        script.write_text(f'@echo off\ncall "{install}\\VC\\Auxiliary\\Build\\vcvars64.bat" >nul\n'
            'if errorlevel 1 exit /b 1\n'
            f'cl /nologo /std:c++20 /EHsc /O2 /W4 /DFORGEPACT_RELEASE /I "{include}" "{cpp}" '
            f'/Fe:"{binary}" /Fo:"{output / "item_truth.obj"}"\nexit /b %errorlevel%\n')
        result = subprocess.run(['cmd', '/d', '/c', str(script)], cwd=output, capture_output=True, text=True)
        self.assertEqual(0, result.returncode, result.stdout + result.stderr)
        result = subprocess.run([str(binary)], capture_output=True, text=True)
        (output / 'item_truth-run.log').write_text(result.stdout + result.stderr)
        self.assertEqual(0, result.returncode, result.stdout + result.stderr)
        self.assertIn('RESULT OK', result.stdout)


if __name__ == '__main__':
    unittest.main()
