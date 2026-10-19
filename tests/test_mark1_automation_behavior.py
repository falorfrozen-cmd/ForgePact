"""Run the real ForgePact::Mark1Automation core against a controlled bar.

The Mark 1 set's bonus casts the skills on the bar's Q, E, R and Y slots (bar
columns 3 to 6) whenever the game's own gate allows, by putting the press
value into the profile slot's `inputState` entry of the same index for the
length of one `CheckTalentUse` call and putting the original back afterwards.
These scenarios pin the game-independent part of that, before any adapter
exists: which column is pressed, the toggle guard (a toggle is pressed only
while it reads Off and outside its own settle window, never while Unreadable),
which columns wear the border, the press value and the column-to-entry
mapping, and that the original value is restored after the call in every
path, a throw included. The adapter in ModuleMain.cpp (reading the bar, the
toggle state and `inputState` by name) is pinned by its own contract tests.
"""
import os
import shutil
import subprocess
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class Mark1AutomationBehaviorTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        header = (ROOT / "plugin/include/ForgePact/Mark1Automation.hpp").read_text(encoding="utf-8")
        cls.header = header
        klass = "\n".join(
            line for line in header.split("\n")
            if not line.strip().startswith("#pragma once")
            and '#include "Common.hpp"' not in line
        )
        out = ROOT / "build/mark1-automation-behavior"
        out.mkdir(parents=True, exist_ok=True)
        code = (ROOT / "tests/mark1_automation_harness.cpp").read_text(encoding="utf-8")
        code = code.replace("// PRODUCTION_MARK1AUTOMATION", klass)
        cpp = out / "mark1automation.cpp"
        cpp.write_text(code, encoding="utf-8")
        cls.binary = out / ("mark1automation.exe" if os.name == "nt" else "mark1automation")
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
                f'cl /nologo /std:c++20 /EHsc /O2 /W4 /I "{ROOT / "plugin/include"}" "{cpp}" /Fe:"{cls.binary}" /Fo:"{out / "mark1automation.obj"}"\n'
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

    # ---- baseline: the bonus inactive ------------------------------------
    def test_baseline_inactive_bonus_presses_nothing_and_counts_nothing(self):
        self.assertScenario("baseline/no_column_read")
        self.assertScenario("baseline/no_column_pressed")
        self.assertScenario("baseline/nothing_counted")
        self.assertScenario("baseline/no_border")
        self.assertScenario("baseline/press_scope_writes_nothing")

    # ---- which columns ----------------------------------------------------
    def test_empty_and_aura_columns_are_never_pressed_or_bordered(self):
        self.assertScenario("columns/empty_never_pressed")
        self.assertScenario("columns/aura_never_pressed")
        self.assertScenario("columns/empty_and_aura_counted")
        self.assertScenario("border/empty_and_aura_unbordered")
        self.assertScenario("columns/unreadable_never_pressed")
        self.assertScenario("columns/press_scope_untouched")
        self.assertScenario("columns/throwing_reads_contained")

    def test_an_ordinary_column_is_pressed_on_every_gate_call(self):
        self.assertScenario("columns/ordinary_every_call")
        self.assertScenario("columns/ordinary_same_frame_twice")
        self.assertScenario("columns/ordinary_counted")

    def test_the_border_marks_every_automated_column(self):
        self.assertScenario("border/ordinary_and_toggle_bordered")
        self.assertScenario("border/unreadable_unbordered")

    # ---- the toggle guard -------------------------------------------------
    def test_toggle_guard_presses_only_while_off(self):
        self.assertScenario("toggle/off_pressed")
        self.assertScenario("toggle/on_never_pressed")

    def test_toggle_guard_waits_out_its_own_settle_window(self):
        self.assertScenario("toggle/settle_window_blocks")
        self.assertScenario("toggle/settle_window_ends")
        self.assertScenario("toggle/settle_is_per_column")
        self.assertScenario("toggle/settle_survives_frame_reset")

    def test_toggle_guard_never_presses_an_unreadable_toggle(self):
        self.assertScenario("toggle/unreadable_never_pressed")
        self.assertScenario("toggle/unreadable_counted")

    def test_toggle_guard_holds_a_toggle_on_across_600_frames_with_one_press(self):
        self.assertScenario("toggle/held_on_600_frames_pressed_once")
        self.assertScenario("toggle/held_on_600_frames_never_off")

    def test_toggle_guard_settle_window_is_a_named_constant(self):
        # Provisional 30 frames until `live1-constants` sets it from Live 1's
        # `toggle-settle:` (measured + 10).
        self.assertIn("inline constexpr long long kMark1ToggleSettleFrames = 30;", self.header)
        self.assertScenario("toggle/settle_constant_is_30")

    # ---- the press itself -------------------------------------------------
    def test_press_value_is_4(self):
        self.assertIn("inline constexpr double kMark1PressValue = 4.0;", self.header)
        self.assertScenario("press/value_4_written")

    def test_columns_3_to_6_map_to_entries_3_to_6(self):
        self.assertScenario("press/column_entry_mapping")
        self.assertScenario("press/outside_columns_rejected")
        self.assertScenario("press/only_planned_entries_written")

    def test_original_value_is_restored_after_the_call(self):
        self.assertScenario("press/restored_after_return")
        self.assertScenario("press/original_return_passed_through")
        self.assertScenario("press/value_seen_by_the_call")
        self.assertScenario("press/void_original")

    def test_original_value_is_restored_when_the_call_throws(self):
        self.assertScenario("press/restored_after_throw")
        self.assertScenario("press/throw_propagates")

    def test_an_entry_whose_original_cannot_be_read_is_never_pressed(self):
        self.assertScenario("press/unreadable_original_untouched")
        self.assertScenario("press/unreadable_original_counted")

    def test_a_failed_restore_is_counted_not_hidden(self):
        self.assertScenario("press/restore_failure_counted")


if __name__ == "__main__":
    unittest.main()
