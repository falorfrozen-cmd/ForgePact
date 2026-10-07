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

ForgePact #173: a research-build session at x100 ended with no `dropmult gold
coin` line written. The baseline `coin_line_precedes_original` pins why that
says no DropGold call reached the hook: the first coin line after a multiplier
change is logged before the game's original runs. The `gold_crumbs` targets
pin the breadcrumb points around both originals, through the harness's
recording sink. The harness is built a second time without a sink, where the
points must compile to nothing and every other scenario print the same.
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
        # The recording sink build, and the one with no sink (#173): there the
        # breadcrumb points must compile to nothing, as in the player build.
        cls.output = cls.build_and_run(out, cpp, "dropgold", None)
        cls.output_no_sink = cls.build_and_run(out, cpp, "dropgold-nosink", "DROP_GOLD_HARNESS_NO_SINK")

    @staticmethod
    def build_and_run(out, cpp, name, define):
        binary = out / (name + ".exe" if os.name == "nt" else name)
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
            batch = out / f"compile-{name}.cmd"
            flag = f" /D{define}" if define else ""
            batch.write_text(
                f'@echo off\ncall "{vcvars}" >nul\nif errorlevel 1 exit /b 1\n'
                f'cl /nologo /std:c++20 /EHsc /O2{flag} "{cpp}" /Fe:"{binary}" /Fo:"{out / (name + ".obj")}"\n'
                f'exit /b %errorlevel%\n', encoding="utf-8")
            command = ["cmd", "/d", "/c", str(batch)]
        else:
            compiler = shutil.which("c++")
            if not compiler:
                raise unittest.SkipTest("A C++20 compiler is required for native behavior tests")
            command = [compiler, "-std=c++20", "-O2", *([f"-D{define}"] if define else []),
                       str(cpp), "-o", str(binary)]

        result = subprocess.run(command, cwd=out, capture_output=True, text=True)
        (out / f"compile-{name}.log").write_text(result.stdout + result.stderr, encoding="utf-8")
        if result.returncode:
            raise AssertionError(result.stdout + result.stderr)

        run = subprocess.run([str(binary)], capture_output=True, text=True)
        (out / f"run-{name}.log").write_text(run.stdout + run.stderr, encoding="utf-8")
        return run.stdout

    def line(self, label, output=None):
        output = self.output if output is None else output
        for line in output.split("\n"):
            if line.split(" ")[1:2] == [label]:
                return line
        raise AssertionError(f"scenario {label!r} not in harness output:\n{output}")

    def assertScenario(self, label, output=None):
        self.assertTrue(self.line(label, output).startswith("PASS "), self.line(label, output))

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

    # ---- #173: the ordering, and the breadcrumbs ----------------------------------

    def test_baseline_coin_line_precedes_original(self):
        # At x10 and x100 the first `dropmult gold coin 1/8 at x<n>` line is
        # logged before the game's DropGold runs, so a session that died with
        # no such line had no DropGold call reach the hook. With and without
        # a sink: the breadcrumbs change nothing about it.
        for output in (self.output, self.output_no_sink):
            self.assertScenario("baseline/coin_line_precedes_original", output)

    def test_target_gold_crumbs_x100_monster_drop(self):
        # MonsterGold enter, Gold enter (51 handed, 5100 passed), Gold done,
        # MonsterGold done, each immediately around its original.
        self.assertScenario("target/gold_crumbs_x100_monster_drop")
        self.assertIn("[DropGold enter #", self.line("target/gold_crumbs_x100_monster_drop"))
        self.assertIn(" x100 a4 51 -> 5100]", self.line("target/gold_crumbs_x100_monster_drop"))

    def test_target_gold_crumbs_x1_handed_equals_passed(self):
        self.assertScenario("target/gold_crumbs_x1_handed_equals_passed")

    def test_target_gold_crumbs_non_numeric_unchanged(self):
        self.assertScenario("target/gold_crumbs_non_numeric_unchanged")

    def test_target_gold_crumbs_direct_drop_gold_only_gold_pair(self):
        self.assertScenario("target/gold_crumbs_direct_drop_gold_only_gold_pair")

    def test_target_gold_crumbs_reward_scope_records_x1(self):
        self.assertScenario("target/gold_crumbs_reward_scope_records_x1")

    def test_target_gold_crumbs_ordinal_rises_per_call(self):
        self.assertScenario("target/gold_crumbs_ordinal_rises_per_call")

    def test_without_a_sink_the_points_compile_to_nothing(self):
        # The player build defines no sink. There each point is ((void)0), its
        # arguments never evaluated, and the harness built that way prints
        # exactly what the sink build prints, less the crumb scenarios.
        header = HEADER.read_text(encoding="utf-8").replace("\r\n", "\n")
        no_sink = header[header.index("#ifdef FP_GOLD_CRUMB_SINK"):]
        no_sink = no_sink[no_sink.index("#else"):no_sink.index("#endif")]
        for point in ("FP_GOLD_CRUMB_ORDINAL", "FP_GOLD_CRUMB_ENTER", "FP_GOLD_CRUMB_DONE"):
            self.assertRegex(no_sink, r"#define " + point + r"\([^)\n]*\)\s+\(\(void\)0\)\s*\n")
        self.assertIn("RESULT OK", self.output_no_sink, self.output_no_sink)
        crumb = ("PASS target/gold_crumbs_", "FAIL target/gold_crumbs_")
        sink_lines = [l for l in self.output.split("\n") if not l.startswith(crumb)]
        self.assertEqual(self.output_no_sink.split("\n"), sink_lines)
        # Negative control: the crumb scenarios exist in the sink build only.
        self.assertTrue(any(l.startswith(crumb) for l in self.output.split("\n")))
        self.assertFalse(any(l.startswith(crumb) for l in self.output_no_sink.split("\n")))


if __name__ == "__main__":
    unittest.main()
