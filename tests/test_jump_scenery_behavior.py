"""Run jumpprobe's real decision core against a controlled object table.

`jumpprobe pass` (research build only, ForgePact #16) answers "no collision"
to the local player's collision queries against a scenery family while a
jump's window is open, so the live session can see whether the jump then
crosses a prop and what the game does with a landing inside one. These
scenarios pin the decision ModuleMain.cpp's adapter takes from
plugin/include/ForgePact/JumpSceneryProbe.hpp, compiled whole.

Baseline: the lever off, every query runs the game's own function whatever
the window; the lever on, a query outside the window does too; and inside the
window a query from a self that is not the player, or against
Enemy_Parent_obj, does too. Target: inside the window a Collision_Prop_obj
descendant query from the player is answered no-collision and counted once,
with each builtin's own answer kind; `all` widens to Invisible_Wall_obj where
`props` does not; place_free and tilemap_get_at_pixel pass through under
`props` and are answered under `all`; the window closes at exactly `frames`;
and the `scripts` flag rewrites only its three rows.
"""
import os
import shutil
import subprocess
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class JumpSceneryBehaviorTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        header = (ROOT / "plugin/include/ForgePact/JumpSceneryProbe.hpp").read_text(encoding="utf-8")
        core = "\n".join(line for line in header.split("\n") if not line.strip().startswith("#pragma once"))
        out = ROOT / "build/jump-scenery-behavior"
        out.mkdir(parents=True, exist_ok=True)
        code = (ROOT / "tests/jump_scenery_harness.cpp").read_text(encoding="utf-8")
        code = code.replace("// PRODUCTION_JUMPSCENERY", core)
        cpp = out / "jumpscenery.cpp"
        cpp.write_text(code, encoding="utf-8")
        cls.binary = out / ("jumpscenery.exe" if os.name == "nt" else "jumpscenery")
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
                f'cl /nologo /std:c++20 /EHsc /O2 /W4 /I "{ROOT / "plugin/include"}" "{cpp}" /Fe:"{cls.binary}" /Fo:"{out / "jumpscenery.obj"}"\n'
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

    def test_all_scenarios_pass(self):
        self.assertIn("RESULT OK", self.output, self.output)
        self.assertNotIn("FAIL ", self.output, self.output)

    def test_the_rows_and_their_answer_kinds(self):
        for label in ("table/object_arguments", "table/answer_kinds", "table/noone_is_minus_four"):
            self.assertScenario(label)

    # ---- baseline: what the game does without the lever ------------------

    def test_lever_off_every_query_passes_through(self):
        for label in ("baseline/jump_with_lever_off_opens_nothing", "baseline/lever_off_passes_through",
                      "baseline/lever_off_counted_as_passthrough", "baseline/lever_off_scripts_untouched",
                      "baseline/lever_off_reads_no_object"):
            self.assertScenario(label)

    def test_lever_on_outside_the_window_passes_through(self):
        for label in ("baseline/outside_window_passes_through", "baseline/outside_window_reads_no_object",
                      "baseline/other_self_jump_opens_nothing"):
            self.assertScenario(label)

    def test_inside_the_window_other_selves_and_families_pass_through(self):
        for label in ("baseline/other_self_passes_through", "baseline/other_self_reads_no_object",
                      "baseline/enemy_family_passes_through", "baseline/unknown_object_passes_through"):
            self.assertScenario(label)

    # ---- target: what the lever does --------------------------------------

    def test_a_prop_query_from_the_player_is_answered_once_per_kind(self):
        for label in ("target/player_jump_opens_window", "target/prop_answered_per_builtin", "target/counted_once",
                      "target/descendants_and_the_family_itself", "target/window_is_the_players_only",
                      "target/ancestry_asked_once_per_object"):
            self.assertScenario(label)

    def test_all_widens_to_the_walls_and_the_objectless_rows(self):
        for label in ("target/props_leaves_invisible_wall", "target/props_objectless_rows_pass_through",
                      "target/all_widens_to_invisible_wall", "target/all_answers_objectless_rows",
                      "target/all_leaves_enemies_and_other_selves"):
            self.assertScenario(label)

    def test_the_window_closes_at_exactly_frames(self):
        for label in ("target/window_closes_at_frames", "target/tick_closes_the_window", "target/next_jump_reopens",
                      "target/jump_inside_window_restarts_it", "target/pass0_closes_window", "target/one_frame_window",
                      "target/frames_out_of_range_refused", "target/pass1_starts_clean"):
            self.assertScenario(label)

    def test_the_scripts_flag_rewrites_only_its_three_rows(self):
        for label in ("scripts/flag_off_runs_the_scripts", "scripts/three_rows_answered",
                      "scripts/only_inside_the_window_for_the_player", "scripts/builtins_unchanged"):
            self.assertScenario(label)

    def test_hold_keeps_the_window_open_without_a_jump(self):
        """The lever's own route: `hold` does not depend on a jump script firing."""
        for label in ("hold/window_held_open_without_a_jump", "hold/still_the_players_family_only",
                      "hold/jump_entry_counted_window_stays_held", "hold/pass0_releases_it",
                      "hold/default_is_not_held", "hold/all_and_scripts_answer_without_a_jump"):
            self.assertScenario(label)

    def test_a_lever_no_window_ever_opened_for_is_named_inert(self):
        for label in ("inert/no_window_opened_is_named", "inert/a_window_ends_it",
                      "inert/hold_lever_off_and_other_selves_are_not"):
            self.assertScenario(label)

    def test_the_probe_is_idle_unless_armed_tracing_or_on(self):
        self.assertScenario("active/only_when_armed_tracing_or_lever_on")
        self.assertScenario("trace/at_most_600_lines")

    def test_an_unresolved_family_answers_for_nothing(self):
        self.assertScenario("failclosed/unresolved_family_or_no_callback")


if __name__ == "__main__":
    unittest.main()
