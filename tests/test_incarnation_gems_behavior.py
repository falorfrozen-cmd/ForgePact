"""Compile the real Gems of Incarnation core into its harness and run it; never opens Hero Siege.

INCARNATION_GEMS_BASELINE=1 swaps the two decisions (a drop's seed, a finished
gem's affixes) for "leave it alone": the baseline checks still pass and every
drop, filter and dress target fails - the red-first record of what the mod adds.
"""
import os
import shutil
import subprocess
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / 'plugin/include/ForgePact/IncarnationGemsMod.hpp'
BASELINE_SWAPS = (
    ('    return tables.Pick(nKey, roll, state.filter, missed);\n', '    return std::nullopt;\n'),
    ('    return MaxRoll(affixes, tables.best);\n', '    return false;\n'),
)


def production_source(baseline):
    text = HEADER.read_text(encoding='utf8')
    if baseline:
        for old, new in BASELINE_SWAPS:
            if old not in text:
                raise AssertionError(f'baseline swap target moved: {old!r}')
            text = text.replace(old, new)
    return '\n'.join(line for line in text.splitlines() if not line.startswith(('#include', '#pragma')))


def run_harness(baseline):
    out = ROOT / ('build/incarnation-gems-baseline' if baseline else 'build/incarnation-gems-behavior')
    out.mkdir(parents=True, exist_ok=True)
    cpp = out / 'gems.cpp'
    cpp.write_text((ROOT / 'tests/incarnation_gems_harness.cpp').read_text(encoding='utf8')
                   .replace('// PRODUCTION_INCARNATION_GEMS', production_source(baseline)), encoding='utf8')
    binary = out / ('gems.exe' if os.name == 'nt' else 'gems')
    if os.name == 'nt':
        vswhere = Path(os.environ.get('ProgramFiles(x86)', r'C:\Program Files (x86)')) / 'Microsoft Visual Studio/Installer/vswhere.exe'
        if not vswhere.is_file():
            raise unittest.SkipTest('MSVC is needed for native behavior tests')
        install = subprocess.check_output([str(vswhere), '-latest', '-products', '*', '-requires',
                                           'Microsoft.VisualStudio.Component.VC.Tools.x86.x64', '-property',
                                           'installationPath'], text=True).strip()
        if not install:
            raise unittest.SkipTest('MSVC not installed')
        batch = out / 'compile.cmd'
        batch.write_text(f'@echo off\ncall "{install}\\VC\\Auxiliary\\Build\\vcvars64.bat" >nul\n'
                         'if errorlevel 1 exit /b 1\n'
                         f'cl /nologo /std:c++20 /EHsc /O2 /W4 "{cpp}" /Fe:"{binary}" /Fo:"{out / "gems.obj"}"\n'
                         'exit /b %errorlevel%\n', encoding='utf8')
        command = ['cmd', '/d', '/c', str(batch)]
    else:
        compiler = shutil.which('c++')
        if not compiler:
            raise unittest.SkipTest('C++20 compiler needed')
        command = [compiler, '-std=c++20', '-O2', '-Wall', str(cpp), '-o', str(binary)]
    # A localised MSVC writes its messages in the console code page; never let
    # decoding them hide the compile error itself.
    build = subprocess.run(command, cwd=out, capture_output=True, text=True, errors='replace')
    (out / 'compile.log').write_text(build.stdout + build.stderr, encoding='utf8')
    if build.returncode != 0:
        raise AssertionError(build.stdout + build.stderr)
    run = subprocess.run([str(binary)], capture_output=True, text=True, errors='replace')
    (out / 'run.log').write_text(run.stdout + run.stderr, encoding='utf8')
    return run


class IncarnationGemsBehaviorTests(unittest.TestCase):
    def test_core_scenarios(self):
        run = run_harness(baseline=bool(os.environ.get('INCARNATION_GEMS_BASELINE')))
        self.assertEqual(0, run.returncode, run.stdout + run.stderr)
        self.assertIn('RESULT OK', run.stdout)

    def test_baseline_stub_fails_every_drop_filter_and_dress_target(self):
        # The instrument proves itself: without the two decisions the baseline
        # still holds and the targets that need them fail.
        run = run_harness(baseline=True)
        lines = run.stdout.splitlines()
        self.assertTrue(all(line.startswith('PASS') for line in lines if ' baseline/' in line), run.stdout)
        failed = {line.split(' ', 1)[1] for line in lines if line.startswith('FAIL')}
        self.assertTrue({'target/drop_takes_mythic_seed_for_its_n', 'target/drop_without_n_uses_its_own_key',
                         'target/filter_prefers_most_wanted_mods', 'target/filter_ties_share_the_drops',
                         'target/filter_without_match_takes_any_mythic_and_says_so',
                         'target/best_tier_top_value', 'target/unknown_best_takes_own_top'} <= failed, run.stdout)
        self.assertIn('RESULT FAILED', run.stdout)


if __name__ == '__main__':
    unittest.main()
