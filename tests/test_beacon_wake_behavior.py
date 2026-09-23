from pathlib import Path
import unittest
import test_adaptive_population as compiler
from test_map_reveal_behavior import implementation

ROOT=Path(__file__).resolve().parents[1]

class BeaconWakeBehaviorTests(unittest.TestCase):
    compile_and_run=compiler.AdaptivePopulationTests.compile_and_run

    def test_activation_preserves_behavior_without_redundant_full_walks(self):
        source=(ROOT/'plugin/ModuleMain.cpp').read_text(encoding='utf-8')
        harness=(ROOT/'tests/beacon_wake_harness.cpp').read_text(encoding='utf-8')
        harness=harness.replace('// PRODUCTION_WAKE',implementation(source,'static long BeWakeObject('))
        self.compile_and_run('beacon-wake',harness,'wake redundant walks removed PASS')
