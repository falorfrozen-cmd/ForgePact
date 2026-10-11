"""Run the Always show Chaos Pillar mods core against controlled frames.

`pillartips 1` (ForgePact #7) shows every on-screen Chaos Pillar's box of
mods without the pillar being hovered, by setting the built-in `visible` flag
on the pillar's Chaos_Pillar_Tooltip_obj that the game's own mouse-enter event
sets (docs/chaos-pillar-tips-research.md). These scenarios pin the decision
ModuleMain.cpp's adapter takes from
plugin/include/ForgePact/ChaosPillarTips.hpp, compiled whole with /W4 and no
warning.

Baseline: with the switch off the core reads nothing and writes nothing, and
switching off writes exactly one hide per tooltip the core showed, then
nothing.

Target: switching on installs and writes nothing until the next frame. On,
each pillar on screen (the view grown by a quarter of its size on every side,
edges included), not activated, with a readable `activated` and a tooltip
gets a show write every frame; any other pillar gets nothing. A shown tooltip
whose pillar leaves the screen, is activated, becomes unreadable, loses its
tooltip or leaves the census gets one hide. A tooltip the core never showed is
never written. A view of zero size decides nothing. The census runs on the
first frame, at most once every 30 frames after, and at once in a new room,
which empties the shown set without writing. The stat line counts it all.
"""
import os
import shutil
import subprocess
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

_BUILD = {}


def setUpModule():
    header = (ROOT / "plugin/include/ForgePact/ChaosPillarTips.hpp").read_text(encoding="utf-8")
    core = "\n".join(line for line in header.split("\n") if not line.strip().startswith("#pragma once"))
    # One directory per process: two runs at once (a criteria runner's
    # parallel checks) otherwise race on one .obj. Every assertion quotes the
    # compiler's or the harness's output, so the directory goes afterwards.
    out = ROOT / "build/chaos-pillar-tips-behavior" / f"pid-{os.getpid()}"
    out.mkdir(parents=True, exist_ok=True)
    unittest.addModuleCleanup(shutil.rmtree, out, ignore_errors=True)
    code = (ROOT / "tests/chaos_pillar_tips_harness.cpp").read_text(encoding="utf-8")
    if "// PRODUCTION_CHAOS_PILLAR_TIPS" not in code:
        raise AssertionError("harness lost its injection marker")
    code = code.replace("// PRODUCTION_CHAOS_PILLAR_TIPS", core)
    cpp = out / "chaospillartips.cpp"
    cpp.write_text(code, encoding="utf-8")
    binary = out / ("chaospillartips.exe" if os.name == "nt" else "chaospillartips")
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
            f'cl /nologo /std:c++20 /EHsc /O2 /W4 /WX /I "{ROOT / "plugin/include"}" "{cpp}" /Fe:"{binary}" /Fo:"{out / "chaospillartips.obj"}"\n'
            f'exit /b %errorlevel%\n', encoding="utf-8")
        command = ["cmd", "/d", "/c", str(batch)]
    else:
        compiler = shutil.which("c++")
        if not compiler:
            raise unittest.SkipTest("A C++20 compiler is required for native behavior tests")
        command = [compiler, "-std=c++20", "-O2", "-Wall", "-Wextra", "-Werror",
                   "-I", str(ROOT / "plugin/include"), str(cpp), "-o", str(binary)]
    # The compiler speaks the machine's locale; decode leniently so a
    # localized diagnostic cannot itself crash the test.
    result = subprocess.run(command, cwd=out, capture_output=True, text=True, encoding="utf-8", errors="replace")
    compile_log = result.stdout + result.stderr
    if result.returncode:
        raise AssertionError(compile_log)
    run = subprocess.run([str(binary)], capture_output=True, text=True, encoding="utf-8", errors="replace")
    _BUILD.update(compile_log=compile_log, output=run.stdout, returncode=run.returncode)


class _Scenarios(unittest.TestCase):
    @property
    def output(self):
        return _BUILD["output"]

    def line(self, label):
        for line in self.output.split("\n"):
            if line.split(" ")[1:2] == [label]:
                return line
        raise AssertionError(f"scenario {label!r} not in harness output:\n{self.output}")

    def assertScenarios(self, *labels):
        for label in labels:
            self.assertTrue(self.line(label).startswith("PASS "), self.line(label))


class ChaosPillarTipsBaselineTests(_Scenarios):
    """The game's behaviour with the mod off: nothing read, nothing written."""

    def test_the_header_compiles_whole_with_no_warning(self):
        self.assertNotRegex(_BUILD["compile_log"], r"(?i)\bwarning\b", _BUILD["compile_log"])

    def test_switch_off_starts_off_and_says_so(self):
        self.assertScenarios("baseline/starts_off", "baseline/fresh_stat_line")

    def test_off_reads_nothing_and_writes_nothing(self):
        self.assertScenarios("baseline/off_reads_nothing", "baseline/off_writes_nothing",
                             "baseline/off_with_nothing_shown_writes_nothing")

    def test_switching_off_hides_each_shown_tooltip_once_then_nothing(self):
        self.assertScenarios("baseline/switch_off_hides_each_shown_once")


class ChaosPillarTipsTargetTests(_Scenarios):
    """What the mod does while on, every rule of the plan with its edges."""

    def test_every_scenario_passes(self):
        self.assertEqual(_BUILD["returncode"], 0, self.output)
        self.assertIn("RESULT OK", self.output, self.output)
        self.assertNotIn("FAIL ", self.output, self.output)

    def test_the_constants(self):
        self.assertScenarios("target/constants")

    def test_on_screen_is_the_view_grown_by_a_quarter_edges_included(self):
        self.assertScenarios("target/onscreen_edges_inclusive", "target/onscreen_just_outside",
                             "target/onscreen_margin_is_a_quarter_of_each_side",
                             "target/unusable_view_is_never_on_screen")

    def test_switching_on_arms_nothing(self):
        # No hook is installed: `pillartips 1` at launch only sets the switch.
        self.assertScenarios("target/switch_on_writes_nothing_until_a_frame", "target/first_frame_takes_a_census",
                             "target/switch_on_twice_changes_nothing", "target/switch_on_again_takes_a_census")

    def test_an_onscreen_inactive_readable_pillar_with_a_tooltip_is_shown_every_frame(self):
        self.assertScenarios("target/onscreen_pillar_shown", "target/show_written_every_frame",
                             "target/only_onscreen_inactive_readable_with_tooltip_shown")

    def test_offscreen_activated_unreadable_and_tooltipless_pillars_are_not(self):
        self.assertScenarios("target/offscreen_not_shown", "target/activated_not_shown",
                             "target/unreadable_not_shown", "target/census_counts")

    def test_a_shown_tooltip_whose_pillar_no_longer_qualifies_gets_one_hide(self):
        self.assertScenarios("target/leaves_screen_one_hide", "target/back_on_screen_shown_again",
                             "target/state_change_waits_for_the_census", "target/activated_after_shown_one_hide",
                             "target/unreadable_after_shown_one_hide", "target/tooltip_lost_one_hide",
                             "target/gone_from_census_one_hide", "target/each_hide_once_then_nothing")

    def test_a_tooltip_the_mod_did_not_show_is_never_written(self):
        self.assertScenarios("target/never_touches_a_tooltip_it_did_not_show")

    def test_a_view_of_zero_size_decides_nothing(self):
        self.assertScenarios("target/zero_view_decides_nothing", "target/zero_view_then_a_real_one")

    def test_the_census_cadence_and_the_room(self):
        self.assertScenarios("target/census_once_every_30_frames", "target/census_due_until_taken",
                             "target/new_room_census_at_once_no_hide", "target/room_without_pillars_writes_nothing")

    def test_refused_writes_and_the_stat_line(self):
        self.assertScenarios("target/refused_write_counted", "target/stat_line", "target/stat_line_after_off")


if __name__ == "__main__":
    unittest.main()
