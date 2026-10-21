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
`test_build_id` is Session 5's change, and fails by its own assertions against
`forgepact-74-live2-base` (the plugin Live 2 ran, which refused every hit with
`no field a`): the model's `CreateDefaultParams` returns the struct Live 2
measured, `{j, b, c}` with no `a`; `LootGroundCreate` stores its own `a` into
that record and `CreateItemNew` builds from it, so the plugin writes the item's
`a`, `b`, `c`, `j` at `CreateItemNew`'s entry, creating a field the record
lacks. `test_refusal_latch` holds the refusal latch: a refused rewrite turns its
item off for the session (`rewrite refused: <why>`, `refused=`).
`test_kind_gate` is report#2's change, and fails by its own assertion against
`forgepact-74-kindgate-base` (the plugin before the change, which converted every
element to a number first): the list gate refuses an array, a string, a
struct, an undefined or a null as `never a handle` before any conversion, and
still accepts a live list whose handle is a real or a ref.
`test_pool_retry` is the PR review's change, and fails by its own assertions
against the plugin before it (ForgePact `b1f7cce`, which latched the first pool
build even when it came back empty): the plugin's own `BuildAngelicPool`, run
against the model's unique data, keeps only a build that found candidates, so a
later roll or switch-on resolves the stand-ins once the data is there.
`test_flight_baseline` and `test_flight_target` are the flight charms' drop
switches (#158), on the same machinery: the model adds `DropUniqueItems`, the
ordinary unique roll (one draw over `lootListUnique[k]`, k 0..4, kept when the
repository's rarity is below 7 and the entry is not hidden), and the
repository's rarity and hidden flag for the triples a scenario names. The
baseline holds against the source before #158 too; every target scenario
fails there (no `FlightDropSwitch`).
`test_mark1_heroic_baseline` and `test_mark1_heroic_target` are the Mark 1 drops
(#166), on the same machinery: the model adds `DropItemHeroic`, the Heroic roll
(element k of the same variable, k its third argument and 5 when that is absent
or undefined, drawn again until it keeps an entry that is not hidden and is
rated neither 7 nor 10, one `GetUniqueRepoStruct` read per draw, then
`CreateDefaultParams` once and the placement under the entry's type), and the
Mark 1 set's art state the drops arm. The baseline holds against the source
before the drop adapter too (no switch: the roll is the game's own and nothing
is hooked); the switch-off scenario and every target scenario fail there (no
`Mark1DropState`). Every #74 and flight scenario stays as it was.
`test_mark1_kill_baseline` and `test_mark1_kill_target` are the Mark 1 drops' kill
route (#166, Amendment 3), on the same model of `DropUniqueItems`: the pieces
join the ordinary unique roll through the one detour Pocket Elf already holds
(`Hook_FlightDropUniqueItems`, one install for both), a kill stand-in per
piece on each of `lootListUnique[0..4]`, and each feature's push only with its
own switch. The untouched scenario holds against the source before the kill
route; the switch-off scenario and every target scenario fail there by their
own assertions (its drops' install hooks no `DropUniqueItems`).
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
# The plugin's own headers: the flight charms' decision core (FlightCharms.hpp) compiles in whole.
INCLUDE = ROOT / 'plugin' / 'include'

# Inserted at `// PRODUCTION_TYPES`, ahead of the harness globals that use them.
PRODUCTION_TYPES = (
    'struct AngelicBase {',                      # the pool's rows, the model's unique data
    'static const AngelicBase kAngelicBases[]',
    'struct SignatureRollScope {',
    'struct SignatureItem {',
    'static constexpr SignatureItem kSignatureItems[]',
    'struct SignatureStandIn {',
    'struct FlightDropState {',                  # #158: the flight charms' drop adapter's state
    'struct Mark1DropState {',                   # #166: the Mark 1 drop adapter's state
    'struct KillDrawRollAside {',                # #166's kill draw: a nested roll's set-aside
    'struct KillDrawHit {',                      # #166's kill draw: its state
    'struct KillDrawState {',
)

# Inserted at `// PRODUCTION_FUNCTIONS`, callees before callers.
PRODUCTION = (
    'static bool TryStructNumber(',               # the pool build's reads
    'static std::string StructKey(',
    'static bool SignatureSwitchOn(',
    'static std::string SignatureOffReason(',      # `rewrite refused: <why>` once an item is latched off
    'static double SignatureShare(',             # the beside design only
    'static void SignatureDropOnAngelicHit(',     # the beside design only
    'static bool SigNumber(',
    'static bool SigNeverAHandle(',               # report#2: the kinds refused before any conversion
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
    'static void SignatureRefuse(',               # a refused rewrite: its line, refused=, the latch
    'static void SignatureAttributeHit(',
    'static void SignatureBeforeCreate(',         # Session 5: the rewrite at CreateItemNew's entry
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
    # #158: the flight charms' drop adapter, after everything it calls.
    'struct FlightDropOutsideRoll {',
    'static const FcNs::StandIn* FlightDropStandIn(',
    'static FcNs::Gate FlightDropGate(',
    'static bool FlightDropOn(',
    'static std::string FlightDropOffReason(',
    'static void FlightDropResolveAngelic(',
    'static void FlightDropCountAngelic(',
    'static bool FlightDropReadUniqueLists(',
    'static bool FlightDropUniquePasses(',
    'static void FlightDropResolveElf(',
    'static bool FlightDropTailHolds(',
    'static bool FlightDropHeldReadBack(',
    'static void FlightDropRemove(',
    'static void FlightDropPushOnto(',
    'static void FlightDropHitReset(',
    'static void FlightDropAngelicPush(',
    'static void FlightDropAngelicRemove(',
    'static void FlightDropUniquePush(',
    'static void FlightDropAttributeHit(',
    'static void FlightDropRefuse(',
    'static void FlightDropBeforeCreate(',
    'static void FlightDropNoteBuilt(',
    'static void FlightDropAfterHit(',
    'struct FlightUniqueInjectGuard {',
    'struct FlightUniqueCounterScope {',
    'struct Mark1KillInjectGuard {',              # #166: the kill route's part of the shared detour
    'struct FlightUniqueDepthScope {',
    'static RValue& Hook_FlightDropUniqueItems(',
    'static void FlightDropInstallUnique(',
    'static std::string FlightDropUniqueRoute(',
    'static void FlightDropSwitch(',
    'static void FlightDropFillStat(',
    'static std::string FlightDropTraceLine(',
    # #166: the Mark 1 drop adapter (Mark1Drops.hpp's core compiles in whole), after everything it calls.
    'static bool Mark1DropArtReady(',
    'static M1dNs::ArgKind Mark1DropArgOf(',
    'static bool Mark1DropReadList(',
    'static bool Mark1DropVerdict(',
    'static std::vector<M1dNs::ListEntry> Mark1DropEntries(',
    'static void Mark1DropResolve(',
    'static bool Mark1ListTailHolds(',
    'static bool Mark1DropTailHolds(',
    'static void Mark1DropAddEntry(',
    'static int Mark1DropDeleteLast(',
    'static bool Mark1DropHeldReadBack(',
    'static void Mark1DropClearRoll(',
    'static void Mark1DropRemove(',
    'static void Mark1DropPush(',
    # #166's kill route: the same push onto lootListUnique[0..4], for the shared detour.
    'static bool Mark1KillReadLists(',
    'static void Mark1KillResolve(',
    'static bool Mark1KillTailHolds(',
    'static bool Mark1KillHeldReadBack(',
    'static void Mark1KillClearRoll(',
    'static void Mark1KillRemove(',
    'static void Mark1KillPush(',
    'static bool Mark1KillBeginCall(',
    'static void Mark1KillEnter(',
    'static void Mark1KillLeave(',
    'static void Mark1DropHitReset(',
    'static void Mark1DropAttributeHit(',
    'static void Mark1DropRefuse(',
    'static bool Mark1DropBeforeCreate(',
    'static void Mark1DropNoteBuilt(',
    'static void Mark1DropAfterHit(',
    'struct Mark1DropInjectGuard {',
    'static RValue& Hook_Mark1DropHeroic(',
    'static std::string Mark1DropInstall(',
    'static void Mark1DropInstallIfReady(',
    'static void Mark1DropsTick(',
    'static void Mark1DropsSwitch(',
    'static std::string Mark1DropStatLine(',
    # #166's kill draw: raredrop's one LoadSatanicDropTier detour and its install, then the adapter on
    # DropManager's DropItem detour (DropManager.hpp is spliced in whole, at PRODUCTION_DROPMANAGER).
    'static RValue& SatanicTierLift(',
    'static RValue& HookSatanicTier(',
    'static bool InstallSatanicTierHook(',
    'static std::string KillDrawRoute(',
    'static bool KillDrawHoldsDropItem(',
    'static bool KillDrawScope(',
    'static bool KillDrawElfOn(',
    'struct KillDrawBusyScope {',
    'static bool KillDrawDefinition(',
    'static bool KillDrawPieceVerdict(',
    'static bool KillDrawElfVerdict(',
    'static void KillDrawAnomaly(',
    'static bool KillDrawRemove(',
    'static bool KillDrawHeldReadBack(',
    'static bool KillDrawPut(',
    'static void KillDrawPush(',
    'static void KillDrawCloseHit(',
    'static std::string KillDrawWho(',
    'static void KillDrawNoteRead(',
    'static bool KillDrawTypePick(',
    'static void KillDrawSettleHit(',
    'static bool KillDrawBeforeCreate(',
    'static bool KillDrawNoteBuiltPiece(',
    'static bool KillDrawNoteBuiltElf(',
    'static void KillDrawNoteTier(',
    'static void KillDrawEnterRoll(',
    'static void KillDrawLeaveRoll(',
    'static void KillDrawBeginCall(',
    'static void KillDrawEndCall(',
    'struct KillDrawCall {',
    'static RValue& KillDrawDropItem(',
    'static std::string KillDrawInstall(',
    'static void KillDrawElfFillStat(',
)

# The real DropManager.hpp, spliced at `// PRODUCTION_DROPMANAGER` without its #pragma and #include
# lines (as tests/test_drop_gold_behavior.py splices it): the kill draw's DropItem detour is its
# Hook_DropItem, and the scenarios' `dropmult item` copies are its own loop.
DROP_MANAGER = ROOT / 'plugin' / 'include' / 'ForgePact' / 'DropManager.hpp'


def spliceable(header_text):
    return '\n'.join(line for line in header_text.replace('\r\n', '\n').split('\n')
                     if not line.strip().startswith(('#pragma', '#include')))

# The plugin's own pool build, appended to the functions under another name: the harness's
# BuildAngelicPool keeps its fixed pool for every scenario but `test_pool_retry`'s, which hand
# the call to this one against the model's unique data.
POOL_BUILD = ('static void BuildAngelicPool(', 'static void ProductionBuildAngelicPool(')


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
        pool = implementation(source, POOL_BUILD[0])
        if pool:
            blocks['functions'] += '\n\n' + pool.replace(POOL_BUILD[0], POOL_BUILD[1], 1)
            defines.append(has_define(POOL_BUILD[1]))
        types ='\n'.join(defines) + '\n\n' + blocks['types']
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
        code = code.replace('// PRODUCTION_DROPMANAGER', spliceable(DROP_MANAGER.read_text(encoding='utf-8')))
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
            batch.write_text(f'@echo off\ncall "{vcvars}" >nul\nif errorlevel 1 exit /b 1\ncl /nologo /std:c++20 /EHsc /W4 /O2 /I "{INCLUDE}" "{cpp}" /Fe:"{cls.binary}" /Fo:"{output / "angelic_hit.obj"}"\nexit /b %errorlevel%\n', encoding='utf-8')
            command = ['cmd', '/d', '/c', str(batch)]
        else:
            compiler = shutil.which('c++')
            if not compiler:
                raise unittest.SkipTest('A C++20 compiler is required for native angelic-hit tests')
            command = [compiler, '-std=c++20', '-O2', '-I', str(INCLUDE), str(cpp), '-o', str(cls.binary)]
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
            'missing_field_is_created_and_a_refusal_restores', 'sigdrop_status_tokens',
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

    def test_kind_gate(self):
        # report#2: the list gate refuses an array, a string, a struct, an undefined or a null at
        # [5] as `never a handle` before converting it (the stub counts each conversion it
        # refuses, as the runner counts a raised error, and the count must not move), and a live
        # list whose handle is a real or a ref is still accepted. Fails by its own assertion
        # against forgepact-74-kindgate-base, which converted first and refused `id unreadable`.
        self.run_scenarios(('kind_gate_refuses_before_converting',))

    def test_build_id(self):
        # Session 5: the game model is Live 2's measurement - CreateDefaultParams returns {j, b, c},
        # no field a - and the static reading of the build: LootGroundCreate stores its own a into
        # that record, and CreateItemNew builds from it. The target (ours built once with t 8,
        # a 777002, b 2, c 0, j 0) fails by `no field a` against forgepact-74-live2-base, the
        # plugin Live 2 ran; the baseline (switches off, the game's own record untouched) holds
        # there too. The rewrite point is reached only through a detoured CreateItemNew hook.
        self.run_scenarios((
            'measured_params_vanilla_untouched', 'measured_params_build_our_item',
            'item_hook_route_decides_the_gate',
        ))

    def test_refusal_latch(self):
        # A refused rewrite latches its item off for the session: no more copies pushed, the gate
        # off with `rewrite refused: <why>`, `refused=` counting it, the other item still on.
        self.run_scenarios(('refusal_latches_item_off',))

    def test_pool_retry(self):
        # The #74 review: the plugin's own BuildAngelicPool runs against the model's unique data,
        # absent at first (a switch applied at the main menu). An empty build is not kept: it is
        # logged once and tried again, at most once per 5 s, by a later roll or switch-on, which
        # then resolves the stand-ins and pushes them; a pool that found candidates is built once,
        # and `angeliclist` still rebuilds at once. Both fail against the plugin before the fix,
        # which latched the first, empty, build for the session.
        self.run_scenarios(('empty_pool_retried_on_a_later_roll', 'empty_pool_retried_on_a_later_switch_on'))

    def test_detection(self):
        # Kept from the beside design: the detection is installed once, by name, as two inline
        # detours or not at all, both status lines end with its route, and the panel switch -
        # not a forged item's auto-arm - is the gate (owner, 2026-10-02).
        self.run_scenarios((
            'install_is_idempotent', 'detection_not_detoured_never_arms',
            'status_reports_detect_route', 'autoarm_enabled_not_forced_no_drop',
        ))

    def test_flight_baseline(self):
        # #158, the flight charms' drop switches: with no switch ever touched both rolls are the
        # game's own and the plugin calls nothing; with every switch on and then off again (the
        # hooks installed, passing through) the rows the Angelic roll and the unique roll read,
        # and what they build, are the game's own. Holds against the source before #158 too.
        self.run_scenarios(('flight_untouched_installs_nothing', 'flight_switches_off_rows_vanilla'))

    def test_flight_target(self):
        # #158: Gabriel Wings and Witch's Broomstick join the Angelic roll's lootListUnique[5]
        # after #74's rows, typed, coined (c in n + c) and rewritten to their own seed, built by
        # the game once per hit; Pocket Elf joins the ordinary unique roll (a model of
        # DropUniqueItems: one draw over lootListUnique[k], no second draw), its stand-in on each
        # of lootListUnique[0..4] by the even-count rule, refused when no candidate has one count
        # in all five, counted again when a sub-list changes; an untyped or vanilla pick stays the
        # game's, and #74's `sigdrop status` does not move; a table-only DropUniqueItems hook and
        # a refused rewrite keep the charm off; the fresh `flightcharm stat` line is byte-exact.
        # Every scenario fails against the source before #158 (no FlightDropSwitch).
        self.run_scenarios((
            'flight_stat_line_fresh', 'flight_wings_angelic_hit_built', 'flight_broom_coin_one_in_two',
            'flight_angelic_rows_after_74s', 'flight_elf_unique_roll_built', 'flight_elf_coin_one_in_two',
            'flight_elf_even_count_rule', 'flight_elf_untyped_and_not_ours_are_vanilla',
            'flight_elf_table_only_never_arms', 'flight_refused_rewrite_latches',
            'flight_unique_throw_removes_entries',
        ))

    def test_flight_partial_push(self):
        # PR #214 review round 1: ds_list_add refused partway through a flight push. The entries
        # that did go on come off again before the roll, on the Angelic list (where they sat
        # after #74's row, whose own tail-checked removal they used to block) and on each of the
        # unique roll's sub-lists, and the refused push counts one anomaly, not an injection.
        # Fails against 9b12f86, which left a partial push on the lists as found.
        self.run_scenarios(('flight_partial_push_angelic_removes_its_entries', 'flight_partial_push_unique_removes_its_entries'))

    def test_mark1_heroic_baseline(self):
        # #166, the Mark 1 drops: with the switch never touched the Heroic roll is the game's own
        # and the plugin calls nothing (holds against the source before the drop adapter too);
        # switched on and off again, the hook stays in and passes every call through, counted,
        # the list each call reads and what it builds the game's own.
        self.run_scenarios(('mark1_heroic_untouched_installs_nothing', 'mark1_heroic_switch_off_is_vanilla'))

    def test_mark1_heroic_target(self):
        # #166: each piece's stand-in (a triple of its class the roll keeps, Heroic preferred) joins
        # the sub-list the call reads for its length, typed, coined (c in n + c) and rewritten to the
        # piece's own seed and base, built by the game once per hit and counted built; the third
        # argument names the list; a nested call, a throw, a class with no stand-in, a refused
        # rewrite, a table-only hook, #74's detection not inline and an art not ready each keep the
        # game's own roll as it is; #74's `sigdrop status` does not move; nothing installs before a
        # character exists; the fresh `mark1set drops:` line is the core's, byte for byte.
        self.run_scenarios((
            'mark1_heroic_stat_line_fresh', 'mark1_heroic_hit_builds_the_piece', 'mark1_heroic_coin_one_in_n_plus_c',
            'mark1_heroic_list_index_from_the_argument', 'mark1_heroic_nested_in_the_angelic_roll_passes_through',
            'mark1_heroic_throw_leaves_the_list_as_found', 'mark1_heroic_class_with_no_standin',
            'mark1_heroic_74s_counters_unchanged', 'mark1_heroic_installs_with_a_character',
            'mark1_heroic_table_only_and_no_detection_refuse', 'mark1_heroic_refused_rewrite_latches',
            'mark1_heroic_art_not_ready_pushes_nothing',
        ))

    def test_mark1_kill_baseline(self):
        # #166's kill route (Amendment 3): with neither switch touched the ordinary unique roll is
        # the game's own and nothing is hooked (holds against the source before the kill route too);
        # the drops switched on and off again leave the shared detour in, passing every call through,
        # each counted, the lists and what they build the game's own (fails there: no detour).
        self.run_scenarios(('mark1_kill_untouched_installs_nothing', 'mark1_kill_switch_off_is_vanilla'))

    def test_mark1_kill_target(self):
        # #166's kill route: the pieces join the ordinary unique roll through the one DropUniqueItems
        # detour Pocket Elf already holds - one install for both, whichever switch comes first, and a
        # table-only hook refuses the drops whole on a line of their own. Each call carries one entry
        # per kill stand-in on each of lootListUnique[0..4] (the equal-count rule, n 0 first), a hit
        # on one is the piece's at c in n + c, built by the game and counted on the kill route's
        # tallies; with Pocket Elf on too each hit goes to its own item, and each switch pushes only
        # its own; a throw leaves all five lists as found; a call nested in the Angelic roll passes
        # through; #74's `sigdrop status` does not move; an art not ready pushes nothing. Every
        # Pocket Elf and Heroic scenario above stays as it was. Each fails by its own assertions
        # against the source before the kill route, whose drops' install hooks no DropUniqueItems.
        self.run_scenarios((
            'mark1_kill_hit_builds_the_piece', 'mark1_kill_coin_one_in_n_plus_c', 'mark1_kill_and_pocket_elf_each_hit_their_own',
            'mark1_kill_each_switch_pushes_only_its_own', 'mark1_kill_throw_leaves_the_lists_as_found',
            'mark1_kill_nested_in_the_angelic_roll_passes_through', 'mark1_kill_74s_counters_unchanged',
            'mark1_kill_shares_the_elfs_detour', 'mark1_kill_art_not_ready_pushes_nothing',
        ))

    def test_mark1_draw_baseline(self):
        # #166's kill draw (DropItem's own unique draw): with neither switch touched DropItem, its tier
        # and its draw are the game's own and nothing is hooked (holds against the source before the
        # kill draw too); both switches on and off again leave DropManager's DropItem detour and
        # raredrop's tier detour in, and Hook_DropItem calls the game's DropItem exactly as before -
        # once per call, the caller's own arguments, its return handed back, no runtime call - each
        # call counted (fails there: no draw hooks, no draw fields).
        self.run_scenarios(('mark1_draw_untouched_installs_nothing', 'mark1_draw_switch_off_is_vanilla'))

    def test_mark1_draw_target(self):
        # #166's kill draw: the drops' install hooks DropItem through DropManager's one detour and
        # LoadSatanicDropTier through raredrop's, refused whole unless both are inline. Each DropItem
        # call (each `dropmult item` copy too) carries one entry per draw stand-in on each of
        # lootListUnique[0..5]; a typed draw pick of one is the piece's at c in n_k + c, k the tier the
        # call recorded, rewritten and built, on the draw's own tally; no tier recorded refuses the hit;
        # two hits in one call are settled apart; #74's Angelic roll nested in DropItem sees the lists
        # as on its own and types its hit the same way, the shared DropUniqueItems detour nested in it
        # pushes and hits as today, and the draw after either reads its stand-ins back on; a throw
        # leaves every list as found. Every scenario fails against the source before the kill draw.
        self.run_scenarios((
            'mark1_draw_hit_builds_the_piece', 'mark1_draw_coin_one_in_n_plus_c_at_k', 'mark1_draw_no_tier_is_refused',
            'mark1_draw_two_hits_in_one_call', 'mark1_draw_nested_angelic_sees_the_lists_as_before',
            'mark1_draw_nested_unique_pushes_and_hits_as_today', 'mark1_draw_throw_leaves_the_lists_as_found',
            'mark1_draw_dropmult_copies_each_carry_the_draw', 'mark1_draw_table_only_dropitem_refuses',
            'mark1_draw_table_only_tier_refuses',
        ))

    def test_flight_draw_baseline(self):
        # #166's kill draw for Pocket Elf: its switch on and off again leaves the draw's hooks in,
        # passing every DropItem call through, counted (` elf-draw-calls=`).
        self.run_scenarios(('flight_draw_switch_off_is_vanilla',))

    def test_flight_draw_target(self):
        # #166's kill draw for Pocket Elf: its SS stand-in (Aimbot.exe's rarity and tier) on each of
        # [0..5] for the length of a DropItem call, a typed pick of it Pocket Elf's at c in n_k + c,
        # rewritten, built and counted on `elf-draw=`; with the Mark 1 drops on too, each pick goes to
        # its own item; #74's Angelic roll nested in the call does not see it; a table-only DropItem
        # hook refuses Pocket Elf whole and turns its switch off.
        self.run_scenarios((
            'flight_draw_hit_builds_pocket_elf', 'flight_draw_and_mark1_each_hit_their_own',
            'flight_draw_nested_angelic_sees_the_lists_as_before', 'flight_draw_refuses_whole_when_not_inline',
        ))


if __name__ == '__main__':
    unittest.main()
