"""Run the real `dropmult gold` hook bodies against their baseline and target.

Companion to drop_gold_harness.cpp. ForgePact issue #77: `dropmult gold 100`
froze the game. DropManager's count hook ran the DropMonsterGold original 100
times, and each of those called DropGold directly - a call the inline detour
intercepts - whose hook ran its own original 100 times: 10,000 coins for one
monster's gold. Live 1 (2026-09-27) measured it (the DropGold hook count
100 times the DropMonsterGold count, 10,000 coins per drop, an 8.4 s stall at
spawn and 6.5 s at pickup) and found DropGold's argument 4 varying per coin
(51, 59, 31, 29) while the others stayed constant: the coin's amount.

Owner's decision (2026-09-27, during Live 1): the multiplier scales the value
of the coin, not the number of coins.

The harness splices the real plugin/include/ForgePact/DropManager.hpp with
stand-ins for HookOneScript, RValue, RewardScope, Out and the BP_* macros.

Baseline scenarios pin that the other drop targets keep their count semantics
and that one hooked DropMonsterGold call at x100 runs each gold original once.
That second scenario is the documented "before": run red against the header
before the fix, it printed `monster_originals=100 gold_originals=10000`.
Target scenarios pin one coin whose amount is scaled, arguments untouched at
x1 and inside AFK FARM's reward scope, a non-number or infinite amount left
alone and counted, and one log line per session.
"""
import os
import shutil
import subprocess
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "plugin/include/ForgePact/DropManager.hpp"


def spliceable(header_text):
    """The header without its #pragma/#include lines; the harness supplies
    the standard library and the runtime stand-ins itself."""
    return "\n".join(
        line for line in header_text.split("\n")
        if not line.strip().startswith(("#pragma", "#include"))
    )


class DropGoldBehaviorTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        out = ROOT / "build/drop-gold-behavior"
        out.mkdir(parents=True, exist_ok=True)
        code = (ROOT / "tests/drop_gold_harness.cpp").read_text(encoding="utf-8")
        code = code.replace("// PRODUCTION_DROPMANAGER", spliceable(HEADER.read_text(encoding="utf-8")))
        cpp = out / "dropgold.cpp"
        cpp.write_text(code, encoding="utf-8")

        cls.binary = out / ("dropgold.exe" if os.name == "nt" else "dropgold")
        if os.name == "nt":
            vswhere = Path(os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)")) / "Microsoft Visual Studio/Installer/vswhere.exe"
            if not vswhere.is_file():
                raise unittest.SkipTest("Visual Studio C++ compiler is required for native behavior tests")
            install = subprocess.check_output(
                [str(vswhere), "-latest", "-products", "*", "-requires",
                 "Microsoft.VisualStudio.Component.VC.Tools.x86.x64", "-property", "installationPath"],
                text=True).strip()
            if not install:
                raise unittest.SkipTest("Visual Studio C++ compiler is required for native behavior tests")
            vcvars = Path(install) / "VC/Auxiliary/Build/vcvars64.bat"
            batch = out / "compile.cmd"
            batch.write_text(
                f'@echo off\ncall "{vcvars}" >nul\nif errorlevel 1 exit /b 1\n'
                f'cl /nologo /std:c++20 /EHsc /O2 "{cpp}" /Fe:"{cls.binary}" /Fo:"{out / "dropgold.obj"}"\n'
                f'exit /b %errorlevel%\n', encoding="utf-8")
            command = ["cmd", "/d", "/c", str(batch)]
        else:
            compiler = shutil.which("c++")
            if not compiler:
                raise unittest.SkipTest("A C++20 compiler is required for native behavior tests")
            command = [compiler, "-std=c++20", "-O2", str(cpp), "-o", str(cls.binary)]

        result = subprocess.run(command, cwd=out, capture_output=True, text=True)
        (out / "compile.log").write_text(result.stdout + result.stderr, encoding="utf-8")
        if result.returncode:
            raise AssertionError(result.stdout + result.stderr)

        run = subprocess.run([str(cls.binary)], capture_output=True, text=True)
        cls.output = run.stdout
        (out / "run.log").write_text(run.stdout + run.stderr, encoding="utf-8")

    def line(self, label):
        for line in self.output.split("\n"):
            if line.split(" ")[1:2] == [label]:
                return line
        raise AssertionError(f"scenario {label!r} not in harness output:\n{self.output}")

    def assertScenario(self, label):
        self.assertTrue(self.line(label).startswith("PASS "), self.line(label))

    def test_all_scenarios_pass(self):
        self.assertIn("RESULT OK", self.output, self.output)

    def test_every_scenario_is_a_baseline_or_a_target(self):
        labels = [line.split(" ")[1] for line in self.output.split("\n") if line.startswith(("PASS ", "FAIL "))]
        self.assertTrue(any(l.startswith("baseline/") for l in labels), labels)
        self.assertTrue(any(l.startswith("target/") for l in labels), labels)
        for label in labels:
            self.assertTrue(label.startswith(("baseline/", "target/")), label)

    # ---- baseline --------------------------------------------------------------

    def test_baseline_other_targets_keep_count_semantics(self):
        # The control: `dropmult item 3` still runs DropItem's original three
        # times. Only gold changed meaning.
        self.assertScenario("baseline/other_targets_keep_count_semantics")

    def test_baseline_x100_compounds_to_ten_thousand(self):
        # #77's "before". Red, against the header before the fix, this printed
        # `monster_originals=100 gold_originals=10000`: 10,000 coins for one
        # monster's gold. It asserts the fixed count, one of each.
        self.assertScenario("baseline/x100_compounds_to_ten_thousand")
        self.assertIn("monster_originals=1 gold_originals=1",
                      self.line("baseline/x100_compounds_to_ten_thousand"))

    # ---- target ----------------------------------------------------------------

    def test_target_x100_scales_one_coin(self):
        # One coin, DropGold's argument 4 x100 (51 -> 5100), every other
        # argument as the game passed it, the caller's own value unwritten.
        self.assertScenario("target/x100_scales_one_coin")
        self.assertScenario("target/direct_drop_gold_is_one_coin")

    def test_target_x1_passes_through(self):
        self.assertScenario("target/x1_passes_through")

    def test_target_reward_scope_passes_through(self):
        # AFK FARM's reward delivery keeps the game's own amount.
        self.assertScenario("target/reward_scope_passes_through")

    def test_target_non_numeric_amount_is_left_alone(self):
        # A non-number (or an infinite number) at the index is passed on
        # untouched, one call, and the gold-unscaled counter rises.
        self.assertScenario("target/non_numeric_amount_is_left_alone")

    def test_target_first_scaling_logs_once(self):
        self.assertScenario("target/first_scaling_logs_once")
        self.assertScenario("target/scaling_line_is_not_repeated")
        self.assertIn("LOG dropmult gold: x100 applied to the coin's amount (one coin per drop)",
                      self.output)


if __name__ == "__main__":
    unittest.main()
