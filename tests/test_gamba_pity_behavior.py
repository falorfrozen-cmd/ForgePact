"""Run gambapity's real decision core against controlled calls.

`gambapity` (ForgePact #134 phase 2, player build) guarantees Goburin's Head
from the gamba machine after the configured number of spins without it
dropping. The counter/threshold/reset state machine lives in
plugin/include/ForgePact/GambaPity.hpp, game-independent by contract, and
these scenarios pin the decision ModuleMain.cpp's adapter takes from it,
compiled whole.

Baseline: off, or on with a count not yet reached, every prize roll runs the
game's own roll, nothing but a spin moves the counter, and nothing is ever
forced. Target: with the threshold reached, the next prize roll forces the
charm and resets; a spin before the threshold never forces; a natural charm
drop resets; a spin on another object's call never counts; the gold equivalent
is count * 10000.
"""
import os
import shutil
import subprocess
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class GambaPityBehaviorTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        header = (ROOT / "plugin/include/ForgePact/GambaPity.hpp").read_text(encoding="utf-8")
        core = "\n".join(line for line in header.split("\n") if not line.strip().startswith("#pragma once"))
        out = ROOT / "build/gamba-pity-behavior"
        out.mkdir(parents=True, exist_ok=True)
        code = (ROOT / "tests/gamba_pity_harness.cpp").read_text(encoding="utf-8")
        code = code.replace("// PRODUCTION_GAMBAPITY", core)
        cpp = out / "gambapity.cpp"
        cpp.write_text(code, encoding="utf-8")
        cls.binary = out / ("gambapity.exe" if os.name == "nt" else "gambapity")
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
                f'cl /nologo /std:c++20 /EHsc /O2 /W4 /I "{ROOT / "plugin/include"}" "{cpp}" /Fe:"{cls.binary}" /Fo:"{out / "gambapity.obj"}"\n'
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
        # /W4 is the bar: a warning in the decision core is a finding, not noise.
        cls.compile_output = result.stdout + result.stderr
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

    def test_all_scenarios_pass(self):
        self.assertIn("RESULT OK", self.output, self.output)
        self.assertNotIn("FAIL ", self.output, self.output)

    def test_the_core_compiles_without_warnings(self):
        self.assertNotRegex(self.compile_output, r"warning C\d+", self.compile_output)

    def test_the_gold_equivalent_is_count_times_10000(self):
        for label in ("table/gold_per_spin", "core/gold_equivalent_is_count_times_10000",
                      "core/gold_equivalent_tracks_the_count"):
            self.assertScenario(label)

    # ---- baseline: off, nothing happens --------------------------------

    def test_off_never_counts_or_forces(self):
        for label in ("baseline/off_a_spin_never_counts", "baseline/off_a_prize_roll_never_forces",
                      "baseline/off_a_natural_drop_leaves_the_count", "baseline/off_status"):
            self.assertScenario(label)

    def test_on_below_the_threshold_runs_the_games_own_roll(self):
        for label in ("baseline/on_below_the_threshold_runs_the_games_own_roll",
                      "baseline/on_below_the_threshold_status"):
            self.assertScenario(label)

    # ---- the counter: nothing but a spin moves it ----------------------

    def test_only_a_machine_self_spin_counts(self):
        for label in ("counter/another_objects_spin_never_counts", "counter/only_a_machine_self_spin_counts",
                      "counter/a_natural_drop_resets", "counter/off_keeps_the_counter"):
            self.assertScenario(label)

    # ---- target: the threshold reached forces and resets ---------------

    def test_a_spin_before_the_threshold_never_forces(self):
        for label in ("target/a_spin_before_the_threshold_never_forces",
                      "target/the_next_prize_roll_forces_the_charm",
                      "target/after_the_reset_the_roll_is_the_games_own_again"):
            self.assertScenario(label)

    def test_another_selfs_roll_never_forces(self):
        for label in ("target/another_selfs_roll_never_forces",
                      "target/the_machines_own_roll_forces_at_the_threshold"):
            self.assertScenario(label)

    def test_the_count_past_the_threshold_forces_once(self):
        self.assertScenario("target/count_past_the_threshold_still_forces")

    def test_a_zero_threshold_never_forces(self):
        self.assertScenario("target/a_zero_threshold_never_forces")

    def test_a_natural_drop_resets_the_threshold_reached(self):
        self.assertScenario("target/a_natural_drop_resets_the_threshold_reached")

    # ---- the status line ------------------------------------------------

    def test_the_status_line_reads_back_the_state(self):
        for label in ("status/line_names_count_threshold_and_gold", "status/off_keeps_the_count_in_the_line"):
            self.assertScenario(label)


if __name__ == "__main__":
    unittest.main()
