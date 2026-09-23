"""The local performance build must remain bounded and absent from player code."""
from pathlib import Path
import unittest
import test_adaptive_population as compiler

ROOT=Path(__file__).resolve().parents[1]

class PopulationProfileTests(unittest.TestCase):
    compile_and_run=compiler.AdaptivePopulationTests.compile_and_run

    def test_player_profile_macros_do_not_evaluate_arguments(self):
        code=(ROOT/'tests/population_profile.cpp').read_text(encoding='utf-8')
        self.compile_and_run('profile-disabled',code,'profile disabled PASS')

    def test_sampling_expiry_and_inactive_path(self):
        code='#define FORGEPACT_POPULATION_PROFILE\n'+(ROOT/'tests/population_profile.cpp').read_text(encoding='utf-8')
        self.compile_and_run('profile-enabled',code,'profile bounded sampling PASS')

    def test_native_script_wrappers_forward_unchanged_and_report_coverage(self):
        code='#define FORGEPACT_POPULATION_PROFILE\n'+(ROOT/'tests/population_script_profile.cpp').read_text(encoding='utf-8')
        self.compile_and_run('script-profile-enabled',code,'script timing forwarding PASS')

    def test_player_build_has_no_script_measurement_hooks(self):
        code=(ROOT/'tests/population_script_profile.cpp').read_text(encoding='utf-8')
        self.compile_and_run('script-profile-disabled',code,'script timing disabled PASS')
