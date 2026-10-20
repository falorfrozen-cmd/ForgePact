"""The capacity router is armed by every raised population lever (ForgePact#232).

The game's protected store in ac_dll_gm.dll holds 262,144 records. With density
x5 and Special Content x100 it filled, InitNewVariableFast returned -1, and the
game passed that -1 to SetVariable, which faulted (two player crash dumps,
2026-10-19/20). The router that adds pools behind the game's own existed, but
nothing called PreparePopulationCapacity(), so modstate read "Not requested".

Baseline: with no lever raised, the router is not armed, so vanilla play never
loads an extra pool or detours the store. Target: each raised lever arms it,
once per process, from FrameCallback after setup. Nothing caps the population.
"""
import re
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from test_release_hook_contract import PLUGIN_PATH, function_body, strip_comments  # noqa: E402


class PopulationCapacityArmingTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.plugin = PLUGIN_PATH.read_text(encoding="utf-8", errors="replace")
        cls.wanted = strip_comments(function_body(cls.plugin, "static bool PopulationCapacityWanted()"))
        cls.frame = strip_comments(function_body(cls.plugin, "void FrameCallback(FWFrame& FrameContext)"))

    def test_baseline_vanilla_values_do_not_want_capacity(self):
        # Every lever is compared against its vanilla value, so density 1,
        # enemyall 1, no Special Content and no spawn pass want nothing.
        for lever in ("DensityManager::Instance().Mult > 1.0", "g_EnemyMultAll > 1", "!g_ObjMult.empty()",
                      "reveal.IsEnabled() && reveal.PacksEnabled()"):
            self.assertIn(lever, self.wanted)
        self.assertNotIn(">=", self.wanted, "a vanilla value must not count as raised")

    def test_target_frame_callback_arms_capacity_once_after_setup(self):
        call = re.search(r"if \(([^{]*)\) \{\s*PreparePopulationCapacity\(\);\s*\}", self.frame)
        self.assertIsNotNone(call, "FrameCallback must call PreparePopulationCapacity()")
        gate = call.group(1)
        for part in ("g_Setup", "!ForgePact::ProtectedPool::Runtime::attempted", "PopulationCapacityWanted()"):
            self.assertIn(part, gate)

    def test_it_is_the_only_caller(self):
        callers = [m.start() for m in re.finditer(r"(?<!bool )PreparePopulationCapacity\(\);",
                                                  strip_comments(self.plugin))]
        self.assertEqual(len(callers), 1, "one arming point keeps Install() single-threaded on the frame")

    def test_nothing_caps_a_raised_population(self):
        # The owner's rule (2026-10-20): a raised lever gets its monsters. The
        # only capacity gate on density copies stays the reveal spawn pass's
        # existing one; plain density copies never wait on headroom.
        body = strip_comments(function_body(self.plugin, "static void DensityCopiesTick(){"))
        self.assertEqual(body.count("PopulationCapacityAvailable()"), 1)
        self.assertIn("PacksEnabled() && !PopulationCapacityAvailable()", body)


if __name__ == "__main__":
    unittest.main()
