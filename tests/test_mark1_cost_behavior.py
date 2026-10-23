"""Run the real ForgePact Mark1Cost core against controlled stat calls.

On the proc route the Mark 1 automation fires its skills the way the game
fires an item proc, which costs no mana, so the owner asked for a price on it
(2026-10-22). Mana itself is never written: the set bonus's consumer answers
the local player's ReturnSpecificStat calls for id 42 (the mana replenish)
with a drain of kMark1DrainPctPerSecond % of maximum mana a second per proc
column, divided by kMark1ReplenishUnit (0.745 mana a second per drained unit of
the answer, Stats live 3's S7 minus S8), never below minus the maximum, the cost
that ships (the owner's
choice after Stats live 2, whose `drain-applies` saw the lowered answer reach
the regen), or, the alternative still built behind `mark1probe autocost
maxmana`, id 37 (maximum mana) cut by kMark1MaxManaCutPct % per proc column,
the cost Stats live 1 measured.

Baseline: no proc column, the cost off, another self, another stat id, no
readable id: the answer reaches the caller as the game gave it and nothing is
counted; at unit 1.0 the drain is R - n x 1% x M. Target: at the shipped unit
the drain is R - n x 1% x M / 0.745, scales with the proc columns and stops at
minus the maximum, the cut takes 20% a column, and every `drain` and `maxmana` case of
the hub's hs-game-sdk/curated/mark1_automation_cases.json (computed by
hs_game_sdk.mark1_automation_model) comes out of the C++ core unchanged. The
consumer in ModuleMain.cpp (the shared ReturnSpecificStat detour, the local
player, no mana write) is pinned by test_mark1_set_contract.py.
"""
import json
import os
import shutil
import subprocess
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HUB = ROOT.parent
HEADER = ROOT / "plugin/include/ForgePact/Mark1Cost.hpp"
CASES = HUB / "hs-game-sdk/curated/mark1_automation_cases.json"


class Mark1CostBehaviorTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        header = HEADER.read_text(encoding="utf-8")
        cls.header = header
        klass = "\n".join(line for line in header.split("\n") if not line.strip().startswith("#pragma once"))
        out = ROOT / "build/mark1-cost-behavior"
        out.mkdir(parents=True, exist_ok=True)
        code = (ROOT / "tests/mark1_cost_harness.cpp").read_text(encoding="utf-8")
        code = code.replace("// PRODUCTION_MARK1COST", klass)
        cpp = out / "mark1cost.cpp"
        cpp.write_text(code, encoding="utf-8")
        cls.binary = out / ("mark1cost.exe" if os.name == "nt" else "mark1cost")
        if os.name == "nt":
            vswhere = Path(os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)")) / "Microsoft Visual Studio/Installer/vswhere.exe"
            if not vswhere.is_file():
                raise unittest.SkipTest("Visual Studio C++ compiler is required for native behavior tests")
            install = subprocess.check_output(
                [str(vswhere), "-latest", "-products", "*", "-requires",
                 "Microsoft.VisualStudio.Component.VC.Tools.x86.x64", "-property", "installationPath"],
                text=True).strip()
            if not install:
                raise unittest.SkipTest("Visual Studio C++ toolchain not installed")
            vcvars = Path(install) / "VC/Auxiliary/Build/vcvars64.bat"
            batch = out / "compile.cmd"
            batch.write_text(
                f'@echo off\ncall "{vcvars}" >nul\nif errorlevel 1 exit /b 1\n'
                f'cl /nologo /std:c++20 /EHsc /O2 /W4 /I "{ROOT / "plugin/include"}" "{cpp}" /Fe:"{cls.binary}" /Fo:"{out / "mark1cost.obj"}"\n'
                f'exit /b %errorlevel%\n', encoding="utf-8")
            command = ["cmd", "/d", "/c", str(batch)]
        else:
            compiler = shutil.which("c++")
            if not compiler:
                raise unittest.SkipTest("A C++20 compiler is required for native behavior tests")
            command = [compiler, "-std=c++20", "-O2", "-I", str(ROOT / "plugin/include"), str(cpp), "-o", str(cls.binary)]
        # The compiler speaks the machine's locale; decode leniently so a
        # localized diagnostic cannot itself crash the test.
        result = subprocess.run(command, cwd=out, capture_output=True, text=True, encoding="utf-8", errors="replace")
        (out / "compile.log").write_text(result.stdout + result.stderr, encoding="utf-8")
        if result.returncode:
            raise AssertionError(result.stdout + result.stderr)
        run = subprocess.run([str(cls.binary)], capture_output=True, text=True, encoding="utf-8", errors="replace")
        cls.output = run.stdout
        (out / "run.log").write_text(run.stdout + run.stderr, encoding="utf-8")

    def line(self, label):
        for line in self.output.split("\n"):
            if line.split(" ")[1:2] == [label]:
                return line
        raise AssertionError(f"scenario {label!r} not in harness output:\n{self.output}")

    def assertScenario(self, label):
        self.assertTrue(self.line(label).startswith("PASS "), self.line(label))

    def run_cases(self, lines):
        """The core's answers for `lines`, one case a line, through the
        harness's `cases` mode."""
        run = subprocess.run([str(self.binary), "cases"], input="\n".join(lines) + "\n",
                             capture_output=True, text=True, encoding="utf-8", errors="replace")
        self.assertEqual(run.returncode, 0, run.stdout + run.stderr)
        answers = [line.split() for line in run.stdout.splitlines() if line.strip()]
        self.assertEqual(len(answers), len(lines), run.stdout)
        return answers

    def test_all_scenarios_pass(self):
        self.assertIn("RESULT OK", self.output, self.output)

    # ---- baseline: vanilla stays vanilla ------------------------------------
    def test_cost_baseline_no_proc_column_answers_originals(self):
        self.assertScenario("baseline/off_no_column_answers_originals")
        self.assertScenario("baseline/no_proc_column_answers_originals")
        self.assertScenario("baseline/off_with_columns_answers_originals")
        self.assertScenario("baseline/other_self_untouched")
        self.assertScenario("baseline/other_stat_ids_untouched")
        self.assertScenario("baseline/no_stat_id_untouched")
        self.assertScenario("baseline/scope_control")
        self.assertScenario("baseline/one_cost_at_a_time")

    def test_cost_baseline_unit_one_is_mana_a_second(self):
        # At unit 1.0 id 42 is answered R - n x 1% x M; a unit that is not a
        # positive finite number leaves the answer alone.
        self.assertScenario("baseline/unit_one_is_mana_a_second")
        self.assertScenario("baseline/bad_unit_answers_original")

    def test_cost_baseline_no_unit_override_divides_by_the_constant(self):
        # Before `mark1probe autounit`: the drain divides by kMark1ReplenishUnit,
        # id 42 answered R - n x 1% x M / 0.745 for one column and for four.
        self.assertScenario("baseline/no_unit_override_divides_by_the_constant")

    # ---- target -----------------------------------------------------------------
    def test_cost_target_drain_scales_with_proc_columns(self):
        self.assertScenario("target/drain_scales_with_proc_columns")
        self.assertScenario("target/drain_linear_in_columns")
        self.assertScenario("target/repeated_calls_never_compound")
        self.assertScenario("target/columns_capped_at_four")
        self.assertScenario("target/not_a_number_untouched_and_counted")

    def test_cost_target_unit_divides_the_drain(self):
        # The owner's choice (2026-10-23): Stats live 3's S7 minus S8 read one
        # drained unit of id 42's answer as 0.745 mana a second, so the drain
        # divides by it and each automated skill takes about 1% of maximum mana
        # a second (not measured in a game session yet).
        self.assertIn("inline constexpr double kMark1ReplenishUnit = 0.745;", self.header)
        self.assertScenario("target/unit_divides_the_drain")
        cases = json.loads(CASES.read_text(encoding="utf-8"))
        scaled = [c for c in cases["drain"] if c["unit"] == 0.745]
        self.assertTrue({1, 4} <= {c["columns"] for c in scaled}, "no shared case at the shipped unit")

    def test_cost_target_drain_never_below_minus_max_mana(self):
        self.assertScenario("target/drain_never_below_minus_max_mana")
        self.assertScenario("target/drain_needs_a_maximum")

    def test_cost_target_maxmana_cuts_twenty_percent_each(self):
        self.assertScenario("target/maxmana_cuts_twenty_percent_each")
        self.assertScenario("target/maxmana_never_below_zero")

    def test_cost_target_matches_the_shared_cases(self):
        cases = json.loads(CASES.read_text(encoding="utf-8"))
        drain, maxmana = cases["drain"], cases["maxmana"]
        self.assertTrue(drain and maxmana, "the cases file carries no drain or maxmana case")
        answers = self.run_cases(
            [f"drain {c['columns']} {c['percent']} {c['max_mana']} {c['unit']} {c['original']}" for c in drain]
            + [f"maxmana {c['columns']} {c['cut_pct']} {c['original']}" for c in maxmana])
        for case, got in zip(drain, answers[:len(drain)]):
            with self.subTest(case["label"]):
                self.assertEqual(got[0], "drain", got)
                self.assertAlmostEqual(float(got[1]), case["expected_drain"], places=6)
                self.assertAlmostEqual(float(got[2]), case["expected_answer"], places=6)
        for case, got in zip(maxmana, answers[len(drain):]):
            with self.subTest(case["label"]):
                self.assertEqual(got[0], "maxmana", got)
                self.assertAlmostEqual(float(got[1]), case["expected_answer"], places=6)

    # ---- the research overrides and the shipped values -------------------------
    def test_cost_research_overrides(self):
        self.assertScenario("override/percent_and_cost")
        self.assertScenario("override/words")

    def test_cost_target_unit_override(self):
        # Amendment 11 (`mark1probe autounit`, the owner 2026-10-23: tune the
        # drain in the next build without a rebuild): the unit in force divides
        # the next answer, `off` and the overrides' reset restore
        # kMark1ReplenishUnit, and what Mark1ReplenishUnitUsable refuses (0, a
        # negative, NaN, infinity) changes nothing. The constant stays 0.745.
        self.assertScenario("override/unit_in_force_until_off_or_reset")
        self.assertScenario("override/unit_refuses_what_cannot_scale")
        self.assertIn("inline constexpr double kMark1ReplenishUnit = 0.745;", self.header)

    def test_cost_shipped_values(self):
        self.assertScenario("route/constants")
        self.assertScenario("route/shipped_cost_drains_replenish")
        self.assertNotIn("inline constexpr Mark1ManaCost kMark1ManaCost = Mark1ManaCost::MaxMana;", self.header)

    def test_cost_target_drain_ships(self):
        # The owner's choice (2026-10-22): the drain ships, and the cut stays
        # built, still answering id 37 when `mark1probe autocost maxmana` sets it.
        self.assertIn("inline constexpr Mark1ManaCost kMark1ManaCost = Mark1ManaCost::Drain;", self.header)
        self.assertScenario("route/shipped_cost_drains_replenish")
        self.assertScenario("route/cut_still_answers_max_mana")

    def test_the_core_is_game_independent(self):
        # Numbers in, numbers out: no instance, no RValue, no game call, and
        # never a mana write.
        for token in ("RValue", "CInstance", "g_Yytk", "HookOneScript", "#include \"Common.hpp\"",
                      "windows.h", "PlayerManaUpdate", "SetVariable"):
            self.assertNotIn(token, self.header, token)


if __name__ == "__main__":
    unittest.main()
