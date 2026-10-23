"""The Angelic / Unholy Drops slider's own pool, run natively (#229).

The real table (`kAngelicBases`), pool build (`BuildAngelicPool`), kill die
(`AngelicDropOnKill`), spawn (`SpawnAngelicItem`) and status line
(`AngelicDropStatus`) are taken out of `plugin/ModuleMain.cpp` (or
`FORGEPACT_TEST_PLUGIN_SOURCE`, so the same scenarios can be run against an
older source) and compiled, as the player build (`FORGEPACT_RELEASE`), into
`angelic_pool_harness.cpp`, which models the game's answers per (type, sub, b):
the definition's name key and hidden flag, the rarity `InitItemFromJson` builds
the item at, and a recording `LootGroundCreateFromItem`.

`test_baseline` holds against `forgepact-229-gold-inlaid-potion-base` (the
plugin that rejected every potion for its type) too: a flagged row, a row the
game builds at another rarity and a row whose key the game does not agree with
are rejected, an ordinary Angelic row validates and a hit on it lands at the
enemy, and with the slider off nothing rolls. `test_target` is the change, and
fails by its own assertions against that source: a potion row goes through the
same three tests as every row (the Gold Inlaid Mysterious Potion validates, the
Elixir of Unworldly Cognition built as Heroic is rejected for its rarity, a
potion that fails any test is rejected for that test), a hit on the potion is
built and placed like any other, and with every row answered as passing the
pool holds every row.
"""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]

# A compile under load was measured at about 2 minutes (guide, "A new test module"); a
# scenario is a few milliseconds, so its bound only has to catch a hang.
COMPILE_TIMEOUT_SECONDS = 600
RUN_TIMEOUT_SECONDS = 30

# Inserted at `// PRODUCTION_TYPES`, ahead of the harness globals that use them.
PRODUCTION_TYPES = (
    'struct AngelicBase {',
    'static const AngelicBase kAngelicBases[]',
    'struct AngelicCandidate {',
)

# Inserted at `// PRODUCTION_FUNCTIONS`, callees before callers.
PRODUCTION = (
    'static bool TryStructNumber(',
    'static std::string StructKey(',
    'static void BuildAngelicPool(',
    'static bool SpawnAngelicItem(',
    'static void AngelicDropOnKill(',
    'static void AngelicDropStatus(',
)


def implementation(source, signature):
    """Brace-matched definition; `rfind` so a forward declaration is skipped.

    A struct, or a table whose closing brace the source follows with `;`, keeps the `;`.
    """
    start = source.rfind(signature)
    if start < 0:
        raise AssertionError(f'{signature} is not in the plugin source')
    brace = source.index('{', start)
    depth = 0
    for index in range(brace, len(source)):
        if source[index] == '{':
            depth += 1
        elif source[index] == '}':
            depth -= 1
            if depth == 0:
                text = source[start:index + 1]
                closed = signature.startswith('struct ') or source[index + 1:index + 2] == ';'
                return text + ';' if closed else text
    raise AssertionError(f'Unterminated definition: {signature}')


class AngelicPoolBehaviorTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        source_path = Path(os.environ.get('FORGEPACT_TEST_PLUGIN_SOURCE', ROOT / 'plugin/ModuleMain.cpp'))
        source = source_path.read_text(encoding='utf-8').replace('\r\n', '\n')
        types = '\n\n'.join(implementation(source, s) for s in PRODUCTION_TYPES)
        functions = '\n\n'.join(implementation(source, s) for s in PRODUCTION)
        # One directory per run: criterion runs compile the base source beside the current one.
        (ROOT / 'build').mkdir(exist_ok=True)
        output = Path(tempfile.mkdtemp(prefix='angelic-pool-', dir=ROOT / 'build'))
        cls.output = output
        try:
            cls.compile(output, types, functions)
        except BaseException:
            shutil.rmtree(output, ignore_errors=True)
            raise

    @classmethod
    def compile(cls, output, types, functions):
        code = (ROOT / 'tests/angelic_pool_harness.cpp').read_text(encoding='utf-8')
        cpp = output / 'angelic_pool.cpp'
        cpp.write_text(code.replace('// PRODUCTION_TYPES', types).replace('// PRODUCTION_FUNCTIONS', functions), encoding='utf-8')
        cls.binary = output / ('angelic_pool.exe' if os.name == 'nt' else 'angelic_pool')
        if os.name == 'nt':
            vswhere = Path(os.environ.get('ProgramFiles(x86)', r'C:\Program Files (x86)')) / 'Microsoft Visual Studio/Installer/vswhere.exe'
            if not vswhere.is_file():
                raise unittest.SkipTest('Visual Studio C++ compiler is required for native angelic-pool tests')
            install = subprocess.check_output([str(vswhere), '-latest', '-products', '*', '-requires', 'Microsoft.VisualStudio.Component.VC.Tools.x86.x64', '-property', 'installationPath'], text=True).strip()
            if not install:
                raise unittest.SkipTest('Visual Studio C++ toolchain not installed')
            vcvars = Path(install) / 'VC/Auxiliary/Build/vcvars64.bat'
            batch = output / 'compile.cmd'
            batch.write_text(f'@echo off\ncall "{vcvars}" >nul\nif errorlevel 1 exit /b 1\ncl /nologo /std:c++20 /EHsc /W4 /O2 "{cpp}" /Fe:"{cls.binary}" /Fo:"{output / "angelic_pool.obj"}"\nexit /b %errorlevel%\n', encoding='utf-8')
            command = ['cmd', '/d', '/c', str(batch)]
        else:
            compiler = shutil.which('c++')
            if not compiler:
                raise unittest.SkipTest('A C++20 compiler is required for native angelic-pool tests')
            command = [compiler, '-std=c++20', '-O2', str(cpp), '-o', str(cls.binary)]
        # The compiler speaks the machine's locale; decode leniently so a localized line
        # cannot crash the test itself.
        result = subprocess.run(command, cwd=output, capture_output=True, text=True, encoding='utf-8', errors='replace',
                                timeout=COMPILE_TIMEOUT_SECONDS)
        (output / 'compile.log').write_text(result.stdout + result.stderr, encoding='utf-8')
        if result.returncode:
            raise AssertionError(result.stdout + result.stderr)

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.output, ignore_errors=True)

    def run_scenarios(self, scenarios):
        for scenario in scenarios:
            with self.subTest(scenario=scenario):
                result = subprocess.run([str(self.binary), scenario], capture_output=True, text=True,
                                        timeout=RUN_TIMEOUT_SECONDS)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_baseline(self):
        # The pool's three tests on rows that are not potions, a hit on an ordinary Angelic,
        # and the slider off: the same before #229 and after it.
        self.run_scenarios((
            'flagged_row_rejected', 'wrong_rarity_rejected', 'key_mismatch_rejected',
            'ordinary_angelic_validates_and_spawns', 'slider_off_nothing_rolls',
        ))

    def test_target(self):
        # #229: no row is rejected for its type. The potion validates as a real Angelic, the
        # Elixir answered as Heroic is rejected for its rarity, a potion failing any of the
        # three tests is rejected for that test (negative control), a hit on the potion is
        # built and placed at the enemy, and a game that passes every row fills the pool.
        self.run_scenarios((
            'potion_validates_as_angelic', 'elixir_rejected_by_its_rarity_not_its_type',
            'potion_still_validated', 'potion_hit_builds_and_places',
            'every_row_passing_fills_the_pool',
        ))


if __name__ == '__main__':
    unittest.main()
