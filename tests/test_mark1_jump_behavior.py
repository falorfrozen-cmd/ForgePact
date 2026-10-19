"""Run the Mark 1 set's real jump core against a controlled world.

With all four Mark 1 pieces worn and the "Mark 1 set" switch on (ForgePact
#166), the local player's jump always goes three times its natural maximum M
toward the cursor, passes scenery, and lands only somewhere walkable. These
scenarios pin the decision ModuleMain.cpp's adapter takes from
plugin/include/ForgePact/Mark1Jump.hpp, compiled whole.

Baseline (`baseline` in the name): with the bonus inactive nothing is read,
decided or rewritten and every query keeps its real answer; another self's
jump is not the player's; no cursor, or one within 16 px of the take-off, is a
vanilla jump. Target: the landing is 3M along the cursor's direction whether
the cursor is inside M (`cursor_near_full_distance`: 100 px still lands at
3M), between M and 3M or beyond 3M; f = L / min(d, M), with M from the cap,
the reach or the 175 px start. `walkable`: the landing and the points 16 px
either side need room, no family and a free path-grid cell (a room with no
grid counts free), a gate or lock on the segment refuses, and an unread
answer counts as blocked. `back_off`: a refused 3M point backs off in 16 px
steps to the farthest candidate past M that passes the same test, never one
whose gate check did not run; with none the jump is vanilla, refused-gate=
when only gates refused; backed-off= is counted apart from the refusals. The
clamp at L, the natural length handed to the learner, a new jump after a
two-frame gap, and a granted window's answers.
"""
import os
import shutil
import subprocess
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class Mark1JumpBehaviorTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        header = (ROOT / "plugin/include/ForgePact/Mark1Jump.hpp").read_text(encoding="utf-8")
        core = "\n".join(line for line in header.split("\n") if not line.strip().startswith("#pragma once"))
        # One directory per process, so two runs at once (a criteria runner's
        # parallel checks) do not race on one .obj.
        out = ROOT / "build/mark1-jump-behavior" / f"pid-{os.getpid()}"
        out.mkdir(parents=True, exist_ok=True)
        cls.addClassCleanup(shutil.rmtree, out, ignore_errors=True)
        code = (ROOT / "tests/mark1_jump_harness.cpp").read_text(encoding="utf-8")
        code = code.replace("// PRODUCTION_MARK1JUMP", core)
        cpp = out / "mark1jump.cpp"
        cpp.write_text(code, encoding="utf-8")
        cls.binary = out / ("mark1jump.exe" if os.name == "nt" else "mark1jump")
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
                f'cl /nologo /std:c++20 /EHsc /O2 /W4 /WX /I "{ROOT / "plugin/include"}" "{cpp}" /Fe:"{cls.binary}" /Fo:"{out / "mark1jump.obj"}"\n'
                f'exit /b %errorlevel%\n', encoding="utf-8")
            command = ["cmd", "/d", "/c", str(batch)]
        else:
            compiler = shutil.which("c++")
            if not compiler:
                raise unittest.SkipTest("A C++20 compiler is required for native behavior tests")
            command = [compiler, "-std=c++20", "-O2", "-Wall", "-Wextra", "-Werror", "-I", str(ROOT / "plugin/include"),
                       str(cpp), "-o", str(cls.binary)]
        # The compiler speaks the machine's locale; decode leniently so a
        # localized diagnostic cannot itself crash the test.
        result = subprocess.run(command, cwd=out, capture_output=True, text=True, encoding="utf-8", errors="replace")
        if result.returncode:
            raise AssertionError(result.stdout + result.stderr)
        run = subprocess.run([str(cls.binary)], capture_output=True, text=True, encoding="utf-8", errors="replace")
        cls.output = run.stdout + run.stderr

    def line(self, label):
        for line in self.output.split("\n"):
            if line.split(" ")[1:2] == [label]:
                return line
        raise AssertionError(f"scenario {label!r} not in harness output:\n{self.output}")

    def assertScenarios(self, *labels):
        for label in labels:
            self.assertTrue(self.line(label).startswith("PASS "), self.line(label))

    def test_all_scenarios_pass(self):
        self.assertIn("RESULT OK", self.output, self.output)
        self.assertNotIn("FAIL ", self.output, self.output)

    def test_the_named_constants(self):
        self.assertScenarios("table/named_constants")

    # ---- baseline: what the game does without the bonus -------------------

    def test_baseline_inactive_reads_decides_and_rewrites_nothing(self):
        self.assertScenarios("baseline/inactive_no_decision", "baseline/inactive_no_scaling",
                             "baseline/inactive_answers_real", "baseline/other_self_is_not_the_players_jump")

    def test_baseline_no_usable_cursor_is_a_vanilla_jump(self):
        self.assertScenarios("baseline/no_cursor_is_vanilla", "baseline/near_cursor_is_vanilla",
                             "baseline/cursor_at_the_threshold_is_decided")

    def test_baseline_a_refused_jump_keeps_its_real_answers(self):
        self.assertScenarios("baseline/refused_jump_keeps_real_answers")

    # ---- target: the landing is always 3M toward the cursor ---------------

    def test_cursor_near_full_distance(self):
        """A cursor 100 px away, well inside M, still lands at 3M."""
        self.assertScenarios("target/cursor_near_full_distance")

    def test_target_lands_at_3m_for_a_cursor_between_m_and_3m_and_beyond(self):
        self.assertScenarios("target/cursor_between_m_and_3m", "target/cursor_beyond_3m",
                             "target/direction_follows_the_cursor")

    def test_target_factor_with_m_from_the_cap_the_reach_and_the_start(self):
        self.assertScenarios("target/max_from_cap_then_reach_then_start",
                             "target/factor_for_each_cursor_and_max_source",
                             "target/an_implausible_max_falls_back_to_start")

    # ---- walkable: the landing test ---------------------------------------

    def test_walkable_landing_and_band_need_room_family_and_grid_free(self):
        self.assertScenarios("walkable/open_floor_passes_and_asks_the_band",
                             "walkable/landing_and_band_need_room",
                             "walkable/landing_and_band_need_no_family",
                             "walkable/landing_and_band_need_a_free_grid_cell_and_a_missing_grid_counts_free")

    def test_walkable_refuses_a_gate_or_lock_on_the_segment(self):
        self.assertScenarios("walkable/a_gate_or_lock_on_the_segment_refuses")

    def test_walkable_unread_answers_count_as_blocked(self):
        self.assertScenarios("walkable/an_unread_answer_counts_as_blocked",
                             "walkable/an_unreadable_room_is_a_vanilla_jump",
                             "walkable/an_unreadable_take_off_is_a_vanilla_jump")

    # ---- back_off: a refused 3M point ----------------------------------------

    def test_back_off_to_the_farthest_walkable_candidate_past_m(self):
        self.assertScenarios("back_off/steps_to_the_farthest_walkable_candidate",
                             "back_off/a_gate_beyond_backs_off_to_its_near_side")

    def test_back_off_never_accepts_a_skipped_gate_check(self):
        self.assertScenarios("back_off/a_skipped_gate_check_is_not_accepted")

    def test_back_off_with_no_candidate_past_m_is_a_vanilla_jump(self):
        self.assertScenarios("back_off/none_past_m_is_a_vanilla_jump",
                             "back_off/every_candidate_behind_a_gate_is_refused_gate")

    def test_back_off_counted_apart_from_a_refusal(self):
        self.assertScenarios("back_off/counted_apart_from_a_refusal")

    # ---- the scaled, clamped displacement and the learner -------------------

    def test_clamp_at_the_landing_distance(self):
        self.assertScenarios("clamp/ends_at_the_landing_distance_and_holds_there",
                             "clamp/a_large_factor_reaches_the_landing_on_the_first_call",
                             "clamp/a_call_that_did_not_move_is_left_alone")

    def test_the_learner_gets_the_natural_length(self):
        self.assertScenarios("learner/natural_length_handed_while_scaled",
                             "learner/natural_length_includes_held_calls_and_vanilla_jumps",
                             "learner/a_dropped_jump_is_not_learned")

    def test_a_new_jump_after_a_two_frame_gap(self):
        self.assertScenarios("window/a_new_jump_after_a_two_frame_gap")

    def test_a_granted_window_answers_the_family_only(self):
        self.assertScenarios("answers/a_granted_window_answers_the_family_only")


if __name__ == "__main__":
    unittest.main()
