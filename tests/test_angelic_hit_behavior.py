"""Headhunter and Tyrant's Crown from the game's own Angelic roll (#74, list injection), run natively.

The real types and hook functions are taken out of `plugin/ModuleMain.cpp` (or
`FORGEPACT_TEST_PLUGIN_SOURCE`, so the same scenarios can be run against an
older source) and compiled, as the player build (`FORGEPACT_RELEASE`), into
`angelic_hit_harness.cpp`, which models the game: its Angelic list on the
first `Controller_obj` instance, the builtins that read and write it, the roll
picking an entry from it (returning undefined on a hit as on a miss),
`CreateDefaultParams` (reached only through a hook on that function, since the
roll calls it directly), the placement that builds one item from the
parameters, and the Custom Forge hook that recognises a built Headhunter or
Tyrant's Crown.

`test_baseline` passes against `forgepact-74-inject-base` (the beside design)
too: with both switches off the roll and its list are the game's own.
`test_target` is the change, and every scenario in it fails against that
source: for the length of a roll the list carries one stand-in entry per
enabled item, a hit on it is ours at one entry's share, the game builds ours
from rewritten parameters, and nothing is spawned beside. `test_detection`
keeps the beside design's detection and gate scenarios, which still hold.
`test_identity` is replan 1's change, and fails by its own assertions against
`forgepact-74-replan1-base` (round 0's plugin, which attributed a hit on the
`CreateDefaultParams` pair alone): a hit is the item's only when the whole
entry (type, sub, b) the roll read through `GetUniqueRepoStruct` is the
stand-in's, a hit with no agreeing read is untyped and stays the game's, the
coin is m·k in n + m·k with k copies, and a push that a fresh read of the list
does not show is taken off again before the roll can carry it.
`test_layout` is replan 2's change, and fails by its own assertions against
`forgepact-74-replan2-base` (round 1's plugin, which took the Controller_obj
variable itself for a flat array of triples): the variable is an array of six
`ds_list` ids and the roll draws from element 5, so the push lands in that
`ds_list` alone and comes off it again, `n` counts that element only, every
step of the nested resolution refuses with its own reason, a fresh read whose
element 5 is another list is a held miss, and `list=` reads `<name>[5]:<size>`.
The model's fixture is that nested layout for every test, so the earlier
tests' scenarios now run on it too.
Each production name's presence is announced as `#define HAS_<NAME>`, so the
harness compiles against any of these sources.
"""
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]

# Inserted at `// PRODUCTION_TYPES`, ahead of the harness globals that use them.
PRODUCTION_TYPES = (
    'struct SignatureRollScope {',
    'struct SignatureItem {',
    'static constexpr SignatureItem kSignatureItems[]',
    'struct SignatureStandIn {',
)

# Inserted at `// PRODUCTION_FUNCTIONS`, callees before callers.
PRODUCTION = (
    'static bool SignatureSwitchOn(',
    'static double SignatureShare(',              # the beside design only
    'static void SignatureDropOnAngelicHit(',     # the beside design only
    'static bool SigNumber(',
    'static bool SigListHandle(',                 # replan 2: a ds_list handle, number or reference
    'static bool SigEntry(',
    'static bool SignatureController(',
    'static bool SignatureListShape(',
    'static std::string SignatureStandInsText(',
    'static std::string SignatureListText(',
    'static std::string SignatureListLine(',
    'static bool SignatureListResolve(',
    'static bool SignatureTailHolds(',
    'static bool SignatureHeldReadBack(',
    'static bool SigUniqueDropBase(',
    'static void SignatureResolveStandIns(',
    'static void SignatureInjectPush(',
    'static void SignatureInjectRemove(',
    'struct SignatureInjectGuard {',
    'static std::string SigJson(',
    'static bool SignatureRewriteParams(',
    'static void SignatureAttributeHit(',
    'static void SignatureNoteBuilt(',
    'static void SignatureHitReset(',
    'static void SignatureAfterHit(',
    'static void SigDropStatus(',
    'static RValue& Hook_CreateDefaultParams(',
    'static RValue& Hook_GetUniqueRepoStruct(',
    'static RValue& HookAngelicChance(',
    'static void InstallSignatureAngelicHooks(',
    'static void AngelicHitStatus(',
    'static std::string ApRollKindName(',        # the research dump's kind names
    'static void SigListDump(',                   # replan 2, research build: `angelicprobe list dump`
    'static void SigInjectStatus(',               # research build: `angelicprobe inject status`, for its list=
)


def implementation(source, signature):
    """Brace-matched definition; `rfind` so a forward declaration is skipped.

    A struct, or a table whose closing brace the source follows with `;`, keeps the `;`.
    """
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
                closed = signature.startswith('struct ') or source[index + 1:index + 2] == ';'
                return text + ';' if closed else text
    raise AssertionError(f'Unterminated definition: {signature}')


def has_define(signature):
    name = re.match(r'(?:static\s+(?:constexpr\s+|const\s+)?\S+\s+|struct\s+)([A-Za-z_]\w*)', signature).group(1)
    return '#define HAS_' + name.upper()


class AngelicHitBehaviorTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        source_path = Path(os.environ.get('FORGEPACT_TEST_PLUGIN_SOURCE', ROOT / 'plugin/ModuleMain.cpp'))
        source = source_path.read_text(encoding='utf-8').replace('\r\n', '\n')
        defines, blocks = [], {}
        for marker, signatures in (('types', PRODUCTION_TYPES), ('functions', PRODUCTION)):
            parts = []
            for signature in signatures:
                body = implementation(source, signature)
                if body:
                    parts.append(body)
                    defines.append(has_define(signature))
            blocks[marker] = '\n\n'.join(parts)
        types = '\n'.join(defines) + '\n\n' + blocks['types']
        # One directory per run: the negative control compiles the beside source while the
        # current one may be compiling beside it (run_criteria --jobs), and a shared
        # directory let one run execute the other's binary.
        (ROOT / 'build').mkdir(exist_ok=True)
        output = Path(tempfile.mkdtemp(prefix='angelic-hit-', dir=ROOT / 'build'))
        cls.output = output
        try:
            cls.compile(output, types, blocks['functions'])
        except unittest.SkipTest:
            shutil.rmtree(output, ignore_errors=True)
            raise

    @classmethod
    def compile(cls, output, types, functions):
        code = (ROOT / 'tests/angelic_hit_harness.cpp').read_text(encoding='utf-8')
        cpp = output / 'angelic_hit.cpp'
        cpp.write_text(code.replace('// PRODUCTION_TYPES', types).replace('// PRODUCTION_FUNCTIONS', functions), encoding='utf-8')
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
        # Both switches off: the game's roll runs once per call (x3 extra rolls included), its
        # chance, its list and its return untouched, and only the game's own item is built.
        self.run_scenarios((
            'both_off_miss_passthrough', 'both_off_hit_passthrough',
            'default_params_outside_roll_not_a_hit', 'extra_rolls_still_run',
        ))

    def test_target(self):
        # For the length of a roll the game's list carries one stand-in entry per enabled item
        # and is the game's own again after it, a throw included; a hit on a stand-in is ours at
        # one entry's share; the game builds ours from rewritten parameters, one item per hit,
        # and nothing is spawned beside it; a list that does not resolve or a missing field
        # refuses; a list the game changed mid-roll is left as found. (Round 0's
        # `ambiguous_standin_never_arms` moved out: the pool-based refusal is gone, and
        # test_identity's other-type scenario covers what it guarded.)
        self.run_scenarios((
            'switch_on_injects_for_the_call', 'original_throw_removes_entries',
            'standin_hit_is_ours_one_entry_share', 'our_hit_rewrites_and_the_game_builds_once',
            'roll_path_never_spawns', 'off_after_on_pushes_nothing',
            'both_on_pushes_two_and_builds_both', 'list_refusals',
            'list_changed_during_roll_left_as_found', 'extra_rolls_carry_the_entries',
            'missing_field_refuses_and_leaves_vanilla', 'sigdrop_status_tokens',
        ))

    def test_identity(self):
        # A hit is typed from the GetUniqueRepoStruct read the roll made before its die and is
        # the item's only on the whole triple; an untyped hit is vanilla and counted; n counts
        # the whole triple and k copies make the coin k in n + k, all of them removed again; a
        # push a fresh read does not show is taken off and the roll carries nothing; the
        # instance resolves as VALUE_REF or VALUE_OBJECT, and a wrong shape still refuses
        # (negative control); the typing hook is the third by-name detour the gate needs.
        self.run_scenarios((
            'other_type_same_pair_never_attributed', 'no_agreeing_record_is_untyped',
            'standin_listed_twice_coin_one_in_three', 'copies_coin_and_tail',
            'copied_list_held_read_back', 'value_ref_and_value_object_resolve_alike',
            'wrong_shape_still_refuses', 'typing_hook_installed_once_by_name',
            'typing_hook_not_detoured_never_arms',
        ))

    def test_layout(self):
        # Replan 2: the list is element 5 of the Controller_obj variable, a ds_list of triples.
        # The push lands in that ds_list alone and comes off it again (same entries, same order),
        # n counts it alone, each nested step refuses with its own reason and leaves the variable
        # as found, a fresh read holding another list there is a held miss, list= names the index,
        # a hit on a pushed entry is ours at k in n + k, a handle held as a reference works the
        # same, and the research dump reads the layout two levels down without writing.
        self.run_scenarios((
            'layout_push_lands_in_element_five_only', 'layout_n_counts_element_five_only',
            'layout_refusals', 'layout_held_miss_on_another_id', 'layout_status_tokens',
            'layout_hit_on_pushed_entry_k_in_n_plus_k', 'layout_handle_as_reference',
            'layout_dump_two_levels',
        ))

    def test_detection(self):
        # Kept from the beside design: the detection is installed once, by name, as two inline
        # detours or not at all, both status lines end with its route, and the panel switch -
        # not a forged item's auto-arm - is the gate (owner, 2026-10-02).
        self.run_scenarios((
            'install_is_idempotent', 'detection_not_detoured_never_arms',
            'status_reports_detect_route', 'autoarm_enabled_not_forced_no_drop',
        ))


if __name__ == '__main__':
    unittest.main()
