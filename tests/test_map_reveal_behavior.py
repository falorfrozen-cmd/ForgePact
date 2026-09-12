"""Run the real MapRevealManager and the real distance hook against controlled game API responses.

Companion to test_map_reveal_contract.py, which asserts on source text. Origin's
review of PR #2 showed why that is not enough on its own: the pack window's
authorization is consumed by Hook_distance_to_object during creator *step*
events, while OnFrame is dispatched from HkPresent at the END of the frame.
Source-string assertions passed while that ordering was wrong. These scenarios
call the hook at the point in the order where it actually matters.
"""
import os
import shutil
import subprocess
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


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


class MapRevealBehaviorTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        plugin = (ROOT / "plugin/ModuleMain.cpp").read_text(encoding="utf-8")
        header = (ROOT / "plugin/include/ForgePact/MapRevealManager.hpp").read_text(encoding="utf-8")

        # The real class, verbatim, minus the include of Common.hpp (the
        # harness supplies the stand-ins Common.hpp would have pulled in).
        klass = "\n".join(
            line for line in header.split("\n")
            if not line.strip().startswith("#pragma once")
            and '#include "Common.hpp"' not in line
        )
        hook = implementation(plugin, "static void Hook_distance_to_object(")

        out = ROOT / "build/map-reveal-behavior"
        out.mkdir(parents=True, exist_ok=True)
        code = (ROOT / "tests/map_reveal_harness.cpp").read_text(encoding="utf-8")
        code = code.replace("// PRODUCTION_MAPREVEAL", klass).replace("// PRODUCTION_FUNCTIONS", hook)
        cpp = out / "mapreveal.cpp"
        cpp.write_text(code, encoding="utf-8")

        cls.binary = out / ("mapreveal.exe" if os.name == "nt" else "mapreveal")
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
                f'cl /nologo /std:c++20 /EHsc /O2 "{cpp}" /Fe:"{cls.binary}" /Fo:"{out / "mapreveal.obj"}"\n'
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

    def test_ready_zone_still_populates(self):
        # The feature must still work: a window opens and a ready creator is
        # answered 0 so the game's own creator logic runs.
        self.assertScenario("ready_zone/window")
        self.assertScenario("ready_zone/distance")

    def test_new_zone_creator_keeps_its_real_distance_before_the_next_present(self):
        # THE REGRESSION origin reported. The hook runs in the creator's step
        # event; OnFrame runs at Present, after it. An unready creator in a
        # newly-entered zone must keep the original distance on that first
        # call - closing the window afterwards is too late.
        self.assertScenario("new_room_before_present/unready_creator")
        self.assertScenario("new_room_after_present/window")
        self.assertScenario("new_room_after_present/distance")

    def test_a_ready_creator_is_still_served_across_the_transition(self):
        # The authorization is per creator, not per zone: the damage mechanism
        # is specific to uninitialised creators, so a ready one is safe. This
        # is what keeps the pass working rather than failing closed on every
        # transition.
        self.assertScenario("new_room_before_present/ready_creator")

    def test_replaced_or_missing_map_with_the_same_room_key(self):
        # Reported alongside the main issue: comparing only the room key let a
        # replaced or removed minimap keep an active window.
        for label in ("same_room_new_map/opened",
                      "same_room_new_map/unready_before_present",
                      "same_room_new_map/window_closed",
                      "map_lost/unready_before_present",
                      "map_lost/window_closed"):
            self.assertScenario(label)

    def test_a_window_is_never_opened_against_an_unreadable_room(self):
        # Reported: TryOpenSpawnWindow stored INT64_MIN and opened anyway, so a
        # later failed read compared equal to it and the window was never
        # invalidated. It must simply not open, and open later once the room
        # can be read.
        self.assertScenario("unreadable_room/never_opens")
        self.assertScenario("unreadable_room/distance")
        self.assertScenario("unreadable_room/opens_when_readable")

    def test_enabling_packs_applies_to_the_current_zone(self):
        self.assertScenario("packs_off/no_window")
        self.assertScenario("packs_on_current_zone/window")

    def test_a_zone_whose_creators_never_initialise_is_left_alone(self):
        self.assertScenario("never_ready/no_window")
        self.assertScenario("never_ready/distance")

    def test_the_beacon_lie_is_gated_by_the_same_invariant(self):
        # "Never answer 0 to an uninitialised creator" is a property of the
        # creator, not of whichever feature asked - the spawner comes out
        # inert either way. The Beacon's wake radius and continuous behaviour
        # are otherwise untouched; reveal is off in this scenario.
        self.assertScenario("beacon/reveal_is_off")
        self.assertScenario("beacon/unready_creator")
        self.assertScenario("beacon/ready_creator")


if __name__ == "__main__":
    unittest.main()
