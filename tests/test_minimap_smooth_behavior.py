"""Run the real MinimapSmoothManager against a modelled minimap refresh/draw loop.

The core of ForgePact issue #19 (minimap smoothing in the game's performance
mode). The header is pure C++ with no runtime dependency, so the harness
compiles it verbatim - spliced in at `// PRODUCTION_MINIMAPSMOOTH` - and plays
the game's part itself: a marker container refilled every N frames and drawn
from every frame.

The game-facing adapter does not exist yet: which routines refill and draw the
markers, and what a marker record looks like, are the open questions
docs/minimap-smoothing-research.md records. These scenarios pin the decisions
that do not depend on those answers.

Baseline scenarios (`test_baseline_*`) pin the unmodified path: toggle off,
and a game that refreshes every frame, both write nothing. Target scenarios
(`test_target_*`) pin what smoothing must do once engaged.
"""
import os
import shutil
import subprocess
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "plugin/include/ForgePact/MinimapSmoothManager.hpp"
HARNESS = ROOT / "tests/minimap_smooth_harness.cpp"
MARKER = "// PRODUCTION_MINIMAPSMOOTH"


class MinimapSmoothBehaviorTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        header = HEADER.read_text(encoding="utf-8")
        # The real class, verbatim, minus the include guard pragma.
        klass = "\n".join(
            line for line in header.split("\n") if not line.strip().startswith("#pragma once")
        )
        code = HARNESS.read_text(encoding="utf-8")
        if MARKER not in code:
            raise AssertionError(f"{HARNESS.name} lost its splice marker {MARKER}")
        code = code.replace(MARKER, klass)

        out = ROOT / "build/minimap-smooth-behavior"
        out.mkdir(parents=True, exist_ok=True)
        cpp = out / "minimapsmooth.cpp"
        cpp.write_text(code, encoding="utf-8")

        cls.binary = out / ("minimapsmooth.exe" if os.name == "nt" else "minimapsmooth")
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
                f'cl /nologo /std:c++20 /EHsc /O2 "{cpp}" /Fe:"{cls.binary}" /Fo:"{out / "minimapsmooth.obj"}"\n'
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

    def test_baseline_unmodified_path_writes_nothing(self):
        # Toggle off (the default) must be vanilla, and so must a game that
        # already refreshes the minimap every frame: the gate is the measured
        # cadence, so performance mode off costs no write at all.
        self.assertScenario("off/no_writes")
        self.assertScenario("fast_cadence/passthrough")

    def test_target_a_steady_cadence_glides_to_the_reported_position(self):
        # Half a period in, half way; a full period in, exactly where the game
        # reported it; a late refresh leaves the marker waiting there rather
        # than carrying it past (no extrapolation - out of scope by design).
        self.assertScenario("steady/linear_progress")
        self.assertScenario("steady/arrives_at_cur")
        self.assertScenario("late_refresh/clamped_at_cur")

    def test_target_an_early_refresh_does_not_pop(self):
        # The glide restarts from where markers were DISPLAYED, so an
        # irregular cadence never makes the minimap jump or run backwards.
        self.assertScenario("no_pop/monotonic_across_refreshes")

    def test_target_markers_that_appear_or_disappear(self):
        # Out of scope: fading. A new marker is shown where reported; a gone
        # one is simply not written.
        self.assertScenario("new_marker/shown_at_cur")
        self.assertScenario("dropped_marker/not_written")

    def test_target_the_write_budget_is_capped_and_counted(self):
        self.assertScenario("cap/truncated_counted")

    def test_target_index_identity_does_not_pair_unrelated_markers(self):
        self.assertScenario("index_keys_count_change/all_new")


if __name__ == "__main__":
    unittest.main()
