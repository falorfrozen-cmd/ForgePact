"""Run gambapity's real decision core against controlled calls.

`gambapity` (ForgePact #134 phase 5, player build) guarantees Goburin's Head
from the gamba machine: the first machine that explodes after the configured
number of spins drops exactly one head, and the counter starts over. The
counter, the explosion watch and the deadline's decision live in
plugin/include/ForgePact/GambaPity.hpp, game-independent by contract, and
these scenarios pin the decisions ModuleMain.cpp's adapter takes from it,
compiled whole.

Baseline: off, nothing counts and nothing forces; only a machine-self spin
counts; the gold equivalent is count * 10000; `off` keeps the count; a natural
head resets it. Target: a live-to-destroyed sprite change is one explosion,
decided once its settle span has passed; at the threshold with no head signal
it forces, a confirmed force resets and a refused one keeps the count; below
the threshold the count is kept; a new ground head, a head build in the
look-back or settle span, or a machine-self (0, 98) build makes it natural at
any count; a room change abandons it; two machines in one span force at most
once; spins without an explosion never force; every line is fixed text.
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
        for label in ("baseline/off_a_spin_never_counts", "baseline/off_a_natural_drop_leaves_the_count",
                      "baseline/off_status", "baseline/off_an_explosion_never_forces",
                      "baseline/off_clears_pending_explosions"):
            self.assertScenario(label)

    # ---- the counter: nothing but a spin raises it ---------------------

    def test_only_a_machine_self_spin_counts(self):
        for label in ("counter/another_objects_spin_never_counts", "counter/only_a_machine_self_spin_counts",
                      "counter/a_natural_drop_resets", "counter/off_keeps_the_counter"):
            self.assertScenario(label)

    # ---- target: the machine watch -------------------------------------

    def test_first_sight_of_a_destroyed_machine_is_not_an_explosion(self):
        self.assertScenario("target/first_sight_destroyed_is_not_an_explosion")

    def test_a_live_to_destroyed_change_is_one_explosion_decided_after_the_settle_span(self):
        self.assertScenario("target/live_to_destroyed_is_one_explosion_decided_after_the_settle_span")

    def test_a_vanished_machine_is_not_an_explosion(self):
        self.assertScenario("target/a_vanished_machine_is_not_an_explosion")

    def test_the_settle_span_look_back_and_radius(self):
        self.assertScenario("table/settle_lookback_radius")

    # ---- target: the decision ------------------------------------------

    def test_at_the_threshold_with_no_signal_the_decision_is_force(self):
        self.assertScenario("target/at_the_threshold_with_no_signal_the_decision_is_force")

    def test_a_confirmed_force_resets_the_counter(self):
        self.assertScenario("target/a_confirmed_force_resets_the_counter")

    def test_a_refused_force_keeps_the_counter_and_the_next_explosion_forces(self):
        self.assertScenario("target/a_refused_force_keeps_the_counter_and_the_next_explosion_forces")

    def test_below_the_threshold_the_counter_is_kept(self):
        self.assertScenario("target/below_the_threshold_the_counter_is_kept")

    def test_a_zero_threshold_never_forces(self):
        self.assertScenario("target/a_zero_threshold_never_forces")

    # ---- target: the natural-head signals ------------------------------

    def test_a_new_ground_head_is_natural_at_any_count(self):
        self.assertScenario("target/a_new_ground_head_is_natural_at_any_count")

    def test_a_head_in_the_machines_baseline_is_not_natural(self):
        self.assertScenario("target/a_baseline_head_is_not_natural")

    def test_a_head_build_in_the_look_back_is_natural(self):
        self.assertScenario("target/a_head_build_in_the_look_back_is_natural")

    def test_a_head_build_in_the_settle_span_is_natural(self):
        self.assertScenario("target/a_head_build_in_the_settle_span_is_natural")

    def test_a_head_build_outside_every_span_does_not_reset(self):
        self.assertScenario("target/a_head_build_outside_every_span_does_not_reset")

    def test_a_machine_build_resets_at_once_and_makes_the_explosion_natural(self):
        self.assertScenario("target/a_machine_build_resets_at_once_and_makes_the_explosion_natural")

    def test_an_own_drop_build_counts_as_own_head_builds_not_a_signal(self):
        self.assertScenario("target/an_own_drop_build_counts_as_own_head_builds_not_a_signal")

    # ---- target: the room, two machines, payouts -----------------------

    def test_a_room_change_abandons_a_pending_explosion_and_keeps_the_counter(self):
        self.assertScenario("target/a_room_change_abandons_a_pending_explosion_and_keeps_the_counter")

    def test_a_room_change_clears_the_machine_records(self):
        self.assertScenario("target/a_room_change_clears_the_machine_records")

    def test_two_machines_in_one_settle_span_force_at_most_once(self):
        self.assertScenario("target/two_machines_in_one_settle_span_force_at_most_once")

    def test_spins_without_an_explosion_never_force(self):
        self.assertScenario("target/spins_without_an_explosion_never_force")

    # ---- the lines, byte for byte --------------------------------------

    def test_the_status_line_carries_every_counter(self):
        for label in ("status/line_names_every_counter", "status/off_keeps_the_count_in_the_line"):
            self.assertScenario(label)

    def test_every_action_line_is_fixed_text(self):
        for label in ("lines/machine_seen", "lines/explosion", "lines/forced", "lines/ground_after_drop",
                      "lines/natural_seen", "lines/below", "lines/refused", "lines/abandoned",
                      "lines/natural_build"):
            self.assertScenario(label)

if __name__ == "__main__":
    unittest.main()
