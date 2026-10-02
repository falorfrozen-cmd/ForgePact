"""Headhunter and Tyrant's Crown from the game's own Angelic roll (#74), run natively.

The real hook functions are taken out of `plugin/ModuleMain.cpp` (or
`FORGEPACT_TEST_PLUGIN_SOURCE`, so the same scenarios can be run against the
pre-#74 source) and compiled, as the player build (`FORGEPACT_RELEASE`), into
`angelic_hit_harness.cpp`, which models the game: the Angelic roll returns
undefined on a hit as on a miss, and only a hit calls `CreateDefaultParams`,
by a direct call that reaches ForgePact only through a hook on that function.

`test_baseline` passes against the pre-#74 source too: with both switches off
the roll is the game's own. `test_target` is the change; every scenario in it
fails against the pre-#74 source. Each production function's presence is
announced as `#define HAS_<NAME>`, so the harness compiles against either.
"""
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]

PRODUCTION = (
    'struct SignatureRollScope {',
    'static bool SignatureSwitchOn(',
    'static double SignatureShare(',
    'static void SignatureDropOnAngelicHit(',
    'static RValue& Hook_CreateDefaultParams(',
    'static RValue& HookAngelicChance(',
    'static void InstallSignatureAngelicHooks(',
    'static void SigDropStatus(',
    'static void AngelicHitStatus(',
)


def implementation(source, signature):
    """Brace-matched definition; `rfind` so a forward declaration is skipped."""
    start = source.rfind(signature)
    if start < 0:
        return ''
    brace = source.index('{', start)
    depth = 0
    for index in range(brace, len(source)):
        if source[index] == '{':
            depth += 1
        elif source[index] == '}':
            depth -= 1
            if depth == 0:
                text = source[start:index + 1]
                return text + ';' if signature.startswith('struct ') else text
    raise AssertionError(f'Unterminated definition: {signature}')


def has_define(signature):
    name = re.match(r'(?:static\s+\S+\s+|struct\s+)([A-Za-z_]\w*)', signature).group(1)
    return '#define HAS_' + name.upper()


class AngelicHitBehaviorTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        source_path = Path(os.environ.get('FORGEPACT_TEST_PLUGIN_SOURCE', ROOT / 'plugin/ModuleMain.cpp'))
        source = source_path.read_text(encoding='utf-8').replace('\r\n', '\n')
        parts, defines = [], []
        for signature in PRODUCTION:
            body = implementation(source, signature)
            if body:
                parts.append(body)
                defines.append(has_define(signature))
        functions = '\n'.join(defines) + '\n\n' + '\n\n'.join(parts)
        # One directory per run: the negative control compiles the pre-#74 source while the
        # current one may be compiling beside it (run_criteria --jobs), and a shared
        # directory let one run execute the other's binary.
        (ROOT / 'build').mkdir(exist_ok=True)
        output = Path(tempfile.mkdtemp(prefix='angelic-hit-', dir=ROOT / 'build'))
        cls.output = output
        try:
            cls.compile(output, functions)
        except unittest.SkipTest:
            shutil.rmtree(output, ignore_errors=True)
            raise

    @classmethod
    def compile(cls, output, functions):
        code = (ROOT / 'tests/angelic_hit_harness.cpp').read_text(encoding='utf-8')
        cpp = output / 'angelic_hit.cpp'
        cpp.write_text(code.replace('// PRODUCTION_FUNCTIONS', functions), encoding='utf-8')
        cls.binary = output / ('angelic_hit.exe' if os.name == 'nt' else 'angelic_hit')
        if os.name == 'nt':
            vswhere = Path(os.environ.get('ProgramFiles(x86)', r'C:\Program Files (x86)')) / 'Microsoft Visual Studio/Installer/vswhere.exe'
            if not vswhere.is_file():
                raise unittest.SkipTest('Visual Studio C++ compiler is required for native angelic-hit tests')
            install = subprocess.check_output([str(vswhere), '-latest', '-products', '*', '-requires', 'Microsoft.VisualStudio.Component.VC.Tools.x86.x64', '-property', 'installationPath'], text=True).strip()
            if not install:
                raise unittest.SkipTest('Visual Studio C++ toolchain not installed')
            vcvars = Path(install) / 'VC/Auxiliary/Build/vcvars64.bat'
            batch = output / 'compile.cmd'
            batch.write_text(f'@echo off\ncall "{vcvars}" >nul\nif errorlevel 1 exit /b 1\ncl /nologo /std:c++20 /EHsc /W4 /O2 "{cpp}" /Fe:"{cls.binary}" /Fo:"{output / "angelic_hit.obj"}"\nexit /b %errorlevel%\n', encoding='utf-8')
            command = ['cmd', '/d', '/c', str(batch)]
        else:
            compiler = shutil.which('c++')
            if not compiler:
                raise unittest.SkipTest('A C++20 compiler is required for native angelic-hit tests')
            command = [compiler, '-std=c++20', '-O2', str(cpp), '-o', str(cls.binary)]
        # The compiler speaks the machine's locale (/W4 notes included); decode
        # leniently so a localized line cannot crash the test itself.
        result = subprocess.run(command, cwd=output, capture_output=True, text=True, encoding='utf-8', errors='replace')
        (output / 'compile.log').write_text(result.stdout + result.stderr, encoding='utf-8')
        if result.returncode:
            raise AssertionError(result.stdout + result.stderr)

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.output, ignore_errors=True)

    def run_scenarios(self, scenarios):
        for scenario in scenarios:
            with self.subTest(scenario=scenario):
                result = subprocess.run([str(self.binary), scenario], capture_output=True, text=True)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_baseline(self):
        # Both switches off: the game's roll runs once per call (x3 extra rolls included),
        # its chance untouched and its own return handed back, and nothing spawns.
        self.run_scenarios((
            'both_off_miss_passthrough', 'both_off_hit_passthrough',
            'default_params_outside_roll_not_a_hit', 'extra_rolls_still_run',
        ))

    def test_target(self):
        # A hit is a CreateDefaultParams call while the roll runs; on each one the enabled
        # items roll one pool entry's share, k / (N + k), and drop beside the game's item.
        self.run_scenarios((
            'headhunter_on_hit_spawns_only_belt', 'tyrant_on_hit_spawns_only_crown',
            'both_on_equal_share', 'miss_never_spawns',
            'spawn_at_roll_position_with_monster_self', 'hit_in_extra_roll_counts',
            'switch_off_after_on_passes_through', 'original_throw_lowers_roll_flag',
            'install_is_idempotent', 'detection_not_detoured_never_arms',
            'status_reports_detect_route',
            # The panel switch is the gate (owner, 2026-10-02): a forged item's auto-arm
            # turns the mechanic on but never the drop.
            'autoarm_enabled_not_forced_no_drop', 'forced_hit_spawns',
        ))


if __name__ == '__main__':
    unittest.main()
