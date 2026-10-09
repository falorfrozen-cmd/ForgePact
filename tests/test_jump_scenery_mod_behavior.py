"""Run the Jump through scenery mod's real decision core against a controlled world.

`jumpscenery 1` (ForgePact #16) lets the local player's universal jump through
scenery: while the jump is airborne, a really-blocked collision query of the
player against the Collision_Parent_obj family is answered "no collision", but
only when the jump would land on open ground inside the room, and never for a
gate or a lock. These scenarios pin the decision ModuleMain.cpp's adapter takes
from plugin/include/ForgePact/JumpScenery.hpp, compiled whole.

Baseline: with the mod off every query keeps its real answer; with it on, so
does a query outside the window, from another self, against another family
(Enemy_Parent_obj), or one that was really free; a refused jump keeps its real
answers for its whole window; and the window closes two frames after the last
skillsLeap entry. Target: a granted jump answers each of the five builtins with
its measured value and is decided once; the landing guard refuses a blocked
landing, a landing (or only the far end of its band) outside the room, and a
jump with no walk direction; the reach is learned from a clear jump of at
least 32 px only, kept across a new player instance and across off/on;
landed-inside=, before-open= and excluded= count what they say.

The landing distance (v2.2.1, from Live 1's measured jumps): with no cursor
the learned reach decides, and puts Live 1's two prop jumps inside their
scenery (baseline). With a cursor the landing is checked at the cursor's
distance, no further than the cap a clean jump well short of its cursor set;
with neither a cursor nor a learned reach, at the 175 px starting reach
(target).

The install: with a character loaded, `jumpscenery 1` installs at once
(baseline). At character select, as the panel's launch commands send it, it
only arms; the tick looks for the player every 60 frames, installs once it
resolves and tries once per session; a refusal turns the switch off (target,
`install=` on the stat line).
"""
import os
import shutil
import subprocess
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class JumpSceneryModBehaviorTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        header = (ROOT / "plugin/include/ForgePact/JumpScenery.hpp").read_text(encoding="utf-8")
        core = "\n".join(line for line in header.split("\n") if not line.strip().startswith("#pragma once"))
        # One directory per process: two runs at once (a criteria runner's
        # parallel checks) otherwise race on one .obj and fail with
        # "Permission denied". A failure carries the compiler's output in its
        # message and every scenario assertion quotes the harness's output,
        # so the directory goes when the class does.
        out = ROOT / "build/jump-scenery-mod-behavior" / f"pid-{os.getpid()}"
        out.mkdir(parents=True, exist_ok=True)
        cls.addClassCleanup(shutil.rmtree, out, ignore_errors=True)
        code = (ROOT / "tests/jump_scenery_mod_harness.cpp").read_text(encoding="utf-8")
        code = code.replace("// PRODUCTION_JUMPSCENERY_MOD", core)
        cpp = out / "jumpscenerymod.cpp"
        cpp.write_text(code, encoding="utf-8")
        cls.binary = out / ("jumpscenerymod.exe" if os.name == "nt" else "jumpscenerymod")
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
                f'cl /nologo /std:c++20 /EHsc /O2 /W4 /I "{ROOT / "plugin/include"}" "{cpp}" /Fe:"{cls.binary}" /Fo:"{out / "jumpscenerymod.obj"}"\n'
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

    def assertScenarios(self, *labels):
        for label in labels:
            self.assertScenario(label)

    def test_all_scenarios_pass(self):
        self.assertIn("RESULT OK", self.output, self.output)
        self.assertNotIn("FAIL ", self.output, self.output)

    def test_the_five_rows_their_answers_and_the_named_constants(self):
        self.assertScenarios("table/five_rows_and_object_arguments", "table/measured_answers",
                             "table/named_constants")

    # ---- baseline: what the game does without the mod ---------------------

    def test_mod_off_every_query_gets_its_real_answer(self):
        self.assertScenarios("baseline/off_every_query_real", "baseline/off_opens_no_window",
                             "baseline/off_tick_reads_nothing")

    def test_mod_on_queries_it_does_not_own_get_their_real_answer(self):
        self.assertScenarios("baseline/outside_window_real", "baseline/other_self_real",
                             "baseline/other_self_opens_no_window", "baseline/enemy_family_real",
                             "baseline/free_answer_never_rewritten", "baseline/non_object_argument_real")

    def test_a_refused_jump_gets_real_answers_for_its_whole_window(self):
        self.assertScenario("baseline/refused_jump_real_whole_window")

    def test_the_window_closes_two_frames_after_the_last_entry(self):
        self.assertScenarios("baseline/window_closes_two_frames_after_last_entry",
                             "baseline/entry_after_a_gap_starts_a_new_jump")

    def test_baseline_without_a_cursor_the_learned_reach_decides(self):
        # Live 1's two prop jumps on the shipped plugin: the band checked at
        # the learned reach (89 in town, 113 outside) lies inside the scenery.
        self.assertScenarios("baseline/no_cursor_learned_reach_decides_town",
                             "baseline/no_cursor_learned_reach_decides_outdoor")

    # ---- target: what the mod does ----------------------------------------

    def test_a_granted_jump_answers_each_builtin_with_its_measured_value(self):
        self.assertScenarios("target/granted_answers_each_builtin", "target/decided_once_per_jump",
                             "target/next_jump_decides_anew", "target/landing_from_reach_and_walk_direction")

    def test_the_landing_guard_refusals(self):
        self.assertScenarios("target/refused_landing", "target/refused_landing_band",
                             "target/refused_room", "target/refused_room_far_band_end_only",
                             "target/refused_room_unreadable", "target/no_reach_refused_at_the_starting_reach",
                             "target/no_direction", "target/no_direction_zero_length",
                             "target/unreadable_takeoff_refused")

    # ---- target: the landing checked where the jump goes (v2.2.1) -----------
    # Live 1 (2026-10-09) measured a jump ending near its cursor, and every
    # prop jump refused because the band was checked at the last clean jump's
    # length instead.

    def test_landing_checked_at_the_cursor_town_live1(self):
        self.assertScenario("target/cursor_town_live1")

    def test_landing_checked_at_the_cursor_outdoor_live1(self):
        self.assertScenario("target/cursor_outdoor_live1")

    def test_landing_checked_at_the_cursor_refuses_a_cursor_inside_the_prop(self):
        self.assertScenario("target/cursor_inside_the_prop_refused")

    def test_landing_checked_at_the_cursor_up_to_the_cap(self):
        self.assertScenario("target/cursor_up_to_the_cap")

    def test_cap_learned_only_from_a_jump_short_of_its_cursor(self):
        self.assertScenario("cap/learned_only_short_of_its_cursor")

    def test_first_jump_lands_at_the_cursor_with_no_learned_reach(self):
        self.assertScenario("first/lands_at_the_cursor_with_no_learned_reach")

    def test_first_jump_without_a_cursor_uses_the_starting_reach(self):
        self.assertScenarios("first/no_cursor_uses_the_starting_reach",
                             "first/unreadable_cursor_uses_the_starting_reach")

    def test_reach_survives_a_new_player_instance(self):
        self.assertScenario("reach/survives_a_new_player_instance")

    def test_the_reach_is_learned_from_a_clear_jump_only(self):
        self.assertScenarios("reach/learned_from_clear_jump", "reach/not_from_stationary_hop",
                             "reach/thirty_two_px_is_enough", "reach/not_from_answered_jump",
                             "reach/not_from_blocked_takeoff", "reach/not_from_blocked_before_open",
                             "reach/kept_on_new_player", "reach/kept_across_off_on")

    def test_landed_inside_counts_a_granted_jump_ending_in_the_family(self):
        self.assertScenario("target/landed_inside")

    def test_before_open_counts_blocked_queries_before_the_window(self):
        self.assertScenario("target/before_open")

    def test_wall_parent_is_in_the_family_through_its_ancestry(self):
        self.assertScenarios("target/wall_parent_in_family", "target/family_table_built_once_per_object")

    def test_gates_and_locks_keep_blocking(self):
        self.assertScenarios("excluded/blocked_by_gate_or_lock", "excluded/query_naming_gate_or_lock",
                             "excluded/asked_only_for_blocked_queries_in_a_granted_window")

    # ---- the install: armed at launch, hooked once a character exists ------
    # (Known Limitations item 8: a hook installed at character select stalls
    # the runner, and the panel's launch commands carry `jumpscenery 1` there.)

    def test_baseline_install_is_immediate_with_a_character(self):
        self.assertScenario("baseline/install_is_immediate_with_a_character")

    def test_the_install_waits_for_a_character(self):
        self.assertScenarios("target/waits_for_a_character_before_installing",
                             "target/a_refusal_turns_the_switch_off_and_the_tick_tries_once",
                             "target/switched_off_never_installs_the_hooks")

    def test_the_stat_line(self):
        self.assertScenarios("stat/on_off_lines", "stat/line_names_every_counter", "stat/room_unknown",
                             "stat/cursor_cap_and_last_check_fields")

    def test_an_unresolved_family_answers_for_nothing(self):
        self.assertScenario("failclosed/unresolved_family_or_exclusion")


if __name__ == "__main__":
    unittest.main()
