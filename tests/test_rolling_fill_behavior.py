"""Run the real fill-the-map rule against a controlled, positional game API.

"Fill the map as you approach" (`fillroll`, issue #183) limits Map Reveal's
"Really spawn every pack on arrival" to the spawners near the local player.
tests/rolling_fill_harness.cpp compiles the real ForgePact::RollingFill and
ForgePact::MapRevealManager classes and the real Hook_distance_to_object body
against a fake runner in which every creator and the player have a position,
and distance_to_object's native answer is the real distance between them.

RollingFillBaselineTests pins today's fill with `fillroll` off; it passed
against the header before the rolling rule existed. RollingFillTargetTests
pins the rule. Both need a C++ compiler, and a skip is reported as a failure
by the workorder's criterion, so a machine without one cannot pass silently.
"""
import os
import shutil
import subprocess
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
INCLUDE = ROOT / "plugin/include/ForgePact"


def implementation(source, signature):
    """The full text of `signature`'s definition (last occurrence wins)."""
    start = source.rfind(signature)
    if start < 0:
        raise AssertionError(f"not found: {signature}")
    brace = source.index("{", start)
    depth = 0
    for index in range(brace, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[start:index + 1]
    raise AssertionError(f"unterminated: {signature}")


def injectable(header_text):
    """A header's text minus what the harness supplies itself: `#pragma once`,
    Common.hpp (the game-API stand-ins) and RollingFill.hpp (injected first)."""
    return "\n".join(
        line for line in header_text.split("\n")
        if not line.strip().startswith("#pragma once")
        and '#include "Common.hpp"' not in line
        and "#include <ForgePact/RollingFill.hpp>" not in line
    )


_OUTPUT = None


def harness_output():
    """Compile and run the harness once for both test classes."""
    global _OUTPUT
    if _OUTPUT is not None:
        return _OUTPUT
    plugin = (ROOT / "plugin/ModuleMain.cpp").read_text(encoding="utf-8")
    rolling_path = INCLUDE / "RollingFill.hpp"
    rolling = injectable(rolling_path.read_text(encoding="utf-8")) if rolling_path.is_file() else ""
    reveal = injectable((INCLUDE / "MapRevealManager.hpp").read_text(encoding="utf-8"))
    hook = implementation(plugin, "static void Hook_distance_to_object(")

    out = ROOT / "build/rolling-fill-behavior"
    out.mkdir(parents=True, exist_ok=True)
    code = (ROOT / "tests/rolling_fill_harness.cpp").read_text(encoding="utf-8")
    code = (code.replace("// PRODUCTION_ROLLINGFILL", rolling)
                .replace("// PRODUCTION_MAPREVEAL", reveal)
                .replace("// PRODUCTION_FUNCTIONS", hook))
    cpp = out / "rollingfill.cpp"
    cpp.write_text(code, encoding="utf-8")

    binary = out / ("rollingfill.exe" if os.name == "nt" else "rollingfill")
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
            f'cl /nologo /std:c++20 /EHsc /O2 /I "{ROOT / "plugin/include"}" "{cpp}" /Fe:"{binary}" /Fo:"{out / "rollingfill.obj"}"\n'
            f'exit /b %errorlevel%\n', encoding="utf-8")
        command = ["cmd", "/d", "/c", str(batch)]
    else:
        compiler = shutil.which("c++")
        if not compiler:
            raise unittest.SkipTest("A C++20 compiler is required for native behavior tests")
        command = [compiler, "-std=c++20", "-O2", "-I", str(ROOT / "plugin/include"), str(cpp), "-o", str(binary)]

    # The compiler speaks the machine's locale; decode leniently so a
    # localized diagnostic cannot itself crash the test.
    result = subprocess.run(command, cwd=out, capture_output=True, text=True, encoding="utf-8", errors="replace")
    (out / "compile.log").write_text(result.stdout + result.stderr, encoding="utf-8")
    if result.returncode:
        raise AssertionError(result.stdout + result.stderr)
    run = subprocess.run([str(binary)], capture_output=True, text=True, encoding="utf-8", errors="replace")
    (out / "run.log").write_text(run.stdout + run.stderr, encoding="utf-8")
    _OUTPUT = run.stdout
    return _OUTPUT


class _HarnessCase(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.output = harness_output()

    def line(self, label):
        for line in self.output.split("\n"):
            if line.split(" ")[1:2] == [label]:
                return line
        raise AssertionError(f"scenario {label!r} not in harness output:\n{self.output}")

    def assertScenario(self, label):
        self.assertTrue(self.line(label).startswith("PASS "), self.line(label))


class RollingFillBaselineTests(_HarnessCase):
    """Today's fill, `fillroll` off: what the switch must leave alone."""

    def test_the_entry_pass_answers_a_far_ready_creator(self):
        # Map Reveal's fill tells every ready spawner in the zone that the
        # player is adjacent, however far it really is.
        self.assertScenario("baseline/entry_pass_open")
        self.assertScenario("baseline/far_native_is_real")
        self.assertScenario("baseline/entry_pass_far_answered")

    def test_after_the_pass_closes_the_answer_is_native(self):
        # 900 frames with nothing deferred and the pass ends; later callers
        # get distance_to_object's own answer.
        self.assertScenario("baseline/pass_closed")
        self.assertScenario("baseline/after_pass_native")

    def test_with_the_fill_off_every_creator_gets_the_native_answer(self):
        for label in ("baseline/fill_off_near_native", "baseline/fill_off_far_native",
                      "baseline/reveal_off_native"):
            self.assertScenario(label)

    def test_with_the_fill_off_fillroll_on_changes_nothing(self):
        # Written with the target: the switch had to exist to be turned on.
        for label in ("baseline/fill_off_rolling_on_near_native", "baseline/fill_off_rolling_on_far_native",
                      "baseline/fill_off_rolling_on_no_player_read", "baseline/reveal_off_rolling_on_native"):
            self.assertScenario(label)


class RollingFillTargetTests(_HarnessCase):
    """`fillroll` on, with the fill on: only creators within reach are answered 0."""

    def test_all_scenarios_pass(self):
        self.assertIn("RESULT OK", self.output, self.output)

    def test_it_starts_off_at_the_default_reach(self):
        self.assertScenario("target/starts_off")
        self.assertScenario("target/default_reach")

    def test_a_far_creator_gets_the_native_answer_and_counts_as_held_back(self):
        self.assertScenario("target/far_native")
        self.assertScenario("target/far_held_back")

    def test_a_held_back_creator_never_takes_an_admission_slot(self):
        self.assertScenario("target/far_takes_no_slot")

    def test_a_near_creator_is_answered_and_counted(self):
        for label in ("target/near_answered", "target/near_counted", "target/near_admitted",
                      "target/many_held_back", "target/many_only_near_answered"):
            self.assertScenario(label)

    def test_the_pass_stays_armed_for_the_zone_visit(self):
        # After more than 900 frames with nothing deferred, today's pass has
        # ended; with the switch on, a creator the player has since come
        # within reach of is answered at its next check.
        for label in ("target/armed_far_first_native", "target/armed_after_900_frames",
                      "target/armed_window_steady", "target/approached_answered"):
            self.assertScenario(label)

    def test_an_unready_creator_within_reach_never_gets_zero(self):
        # Known Limitation 13: an uninitialised creator answered 0 comes out
        # inert. The positive control shows the same creator is answered once
        # it reports a real timer.
        self.assertScenario("target/unready_near_never_answered")
        self.assertScenario("target/unready_near_once_ready")

    def test_the_existing_guards_still_apply_within_reach(self):
        for label in ("target/admission_still_limits", "target/admission_defers_the_rest",
                      "target/capacity_still_gates", "target/capacity_recovered"):
            self.assertScenario(label)

    def test_no_local_player_means_the_native_answer_for_all(self):
        for label in ("target/no_player_near_native", "target/no_player_far_native",
                      "target/no_player_counts_nothing", "target/no_player_takes_no_slot",
                      "target/player_back_near_answered"):
            self.assertScenario(label)

    def test_turning_it_off_rearms_the_full_pass_for_the_current_zone(self):
        for label in ("target/rearm_far_held", "target/off_rearms_pending", "target/off_rearms_window",
                      "target/off_rest_of_zone_fills", "target/off_with_fill_off_arms_nothing"):
            self.assertScenario(label)

    def test_a_zone_change_resets_the_counters(self):
        for label in ("target/counters_before_zone_change", "target/zone_change_resets_answered",
                      "target/zone_change_resets_held_back"):
            self.assertScenario(label)

    def test_the_reach_bounds_and_one_gives_3000(self):
        for label in ("target/parse_1_on", "target/parse_1_gives_3000", "target/parse_0_off",
                      "target/parse_stat", "target/parse_1500_on", "target/parse_1500_reach",
                      "target/parse_20000_on", "target/parse_20000_reach",
                      "target/parse_out_of_bounds_is_usage", "target/usage_text",
                      "target/within_reach_edge", "target/within_reach_nan_is_out",
                      "target/custom_reach_inside", "target/custom_reach_outside",
                      "target/out_of_range_reach_refused"):
            self.assertScenario(label)

    def test_the_player_position_is_read_at_most_once_a_frame(self):
        # The counter's positive control is the second frame reading again.
        for label in ("target/player_resolved_once_a_frame", "target/player_position_read_once_a_frame",
                      "target/many_one_resolve", "target/many_player_reads",
                      "target/player_read_again_next_frame", "target/stat_player_known",
                      "target/stat_player_x"):
            self.assertScenario(label)

    def test_a_non_creator_caller_pays_nothing_new(self):
        self.assertScenario("target/non_creator_native")
        self.assertScenario("target/non_creator_pays_nothing")


if __name__ == "__main__":
    unittest.main()
