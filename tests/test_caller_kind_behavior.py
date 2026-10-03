"""The enemy-born guard's caller reads must never hand the runner a kind it cannot convert.

Live 1 rerun (issue #44, 2026-10-02): a `cb` spawn of a boss reached HookICD
with a `self` that has no `object_index`. `CallerObjectIndex` converted the
undefined answer with `RValue::ToDouble()`, which is the runner's REAL_RValue:
it raised the runner's own error ("REAL argument incorrect type undefined",
YYToolkit full report #2) instead of throwing, so the surrounding
`catch (...)` never saw it. The frame mapping is in
`docs/boss-rarity-research.md` ("The plugin's runner error and the traced
kills").

`caller_kind_harness.cpp` compiles the production `CallerObjectIndex`,
`InstanceIdOf` and `RarInstanceIsBoss` (the shared rarity hook's boss check,
which runs for every enemy while the Bosses control is on), and the kind check
they share, against a stub whose
conversion records a runner error for every kind it cannot convert, as the
real one does, and keeps the unchecked shape beside them as the negative
control. The harness is compiled once per run and each test reads its case.
"""
from pathlib import Path
import unittest

import test_adaptive_population as compiler
from test_map_reveal_behavior import implementation

ROOT = Path(__file__).resolve().parents[1]
KIND_CHECK = 'static bool IsNumericInstanceRead('


class CallerKindBehaviorTests(unittest.TestCase):
    compile_and_run = compiler.AdaptivePopulationTests.compile_and_run
    _output = None

    def harness_output(self):
        cls = type(self)
        if cls._output is None:
            source = (ROOT / 'plugin/ModuleMain.cpp').read_text(encoding='utf-8-sig')
            harness = (ROOT / 'tests/caller_kind_harness.cpp').read_text(encoding='utf-8')
            # Before the fix there is no kind check to compile; the targets then
            # fail on behaviour (a recorded runner error), not on a missing name.
            check = implementation(source, KIND_CHECK) if KIND_CHECK in source else ''
            harness = harness.replace('// PRODUCTION_KIND_CHECK', check)
            harness = harness.replace('// PRODUCTION_CALLER_OBJECT_INDEX', implementation(source, 'static int CallerObjectIndex('))
            harness = harness.replace('// PRODUCTION_INSTANCE_ID_OF', implementation(source, 'static double InstanceIdOf('))
            harness = harness.replace('// PRODUCTION_RAR_INSTANCE_IS_BOSS', implementation(source, 'static bool RarInstanceIsBoss('))
            self.compile_and_run('caller-kind', harness, 'caller kind harness DONE')
            cls._output = (ROOT / 'build/adaptive-population/caller-kind.log').read_text(encoding='utf-8')
        return cls._output

    def assertCasePasses(self, case):
        out = self.harness_output()
        self.assertIn(case + ' PASS', out, out)

    def test_numeric_caller_object_index_is_read(self):
        """Baseline: REAL, INT32, INT64 and REF reads come back unchanged, with no runner error."""
        self.assertCasePasses('numeric_reads')

    def test_undefined_caller_object_index_raises_no_runner_error(self):
        """Target: an undefined (or any non-numeric) object_index reads as -1 and is never converted."""
        self.assertCasePasses('undefined_caller_object_index')

    def test_undefined_instance_id_raises_no_runner_error(self):
        """Target: the same for the `id` read."""
        self.assertCasePasses('undefined_instance_id')

    def test_undefined_boss_object_index_raises_no_runner_error(self):
        """Target: the rarity hook's boss check never converts a non-numeric object_index, and still finds a boss."""
        self.assertCasePasses('undefined_boss_object_index')

    def test_harness_sees_the_unchecked_conversion(self):
        """Negative control: the pre-fix shape, in the same harness, records the runner error."""
        self.assertCasePasses('unchecked_conversion_seen')


if __name__ == '__main__':
    unittest.main()
