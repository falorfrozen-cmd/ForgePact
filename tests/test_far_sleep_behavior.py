"""Run the real ForgePact::FarSleep class against a controlled game API.

Far sleep puts a zone's far scenery props to sleep (instance_deactivate_object)
and wakes them near a player. These scenarios pin what the class asks of the
game (nothing while off, a bounded number of runner calls a frame), which
objects it may touch (three scenery families, never shrines, piles, traps,
monsters, walls or objects with per-frame code), which rooms it leaves alone,
and that no prop inside the wake radius is ever left asleep: walking, after a
teleport, with two players, after the game woke one itself, after a restart,
and when it is switched off.
"""
import os
import shutil
import subprocess
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class FarSleepBehaviorTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        header = (ROOT / "plugin/include/ForgePact/FarSleep.hpp").read_text(encoding="utf-8")
        klass = "\n".join(
            line for line in header.split("\n")
            if not line.strip().startswith("#pragma once")
            and '#include "Common.hpp"' not in line
        )
        out = ROOT / "build/far-sleep-behavior"
        out.mkdir(parents=True, exist_ok=True)
        code = (ROOT / "tests/far_sleep_harness.cpp").read_text(encoding="utf-8")
        code = code.replace("// PRODUCTION_FARSLEEP", klass)
        cpp = out / "farsleep.cpp"
        cpp.write_text(code, encoding="utf-8")
        cls.binary = out / ("farsleep.exe" if os.name == "nt" else "farsleep")
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
                f'cl /nologo /std:c++20 /EHsc /O2 /I "{ROOT / "plugin/include"}" "{cpp}" /Fe:"{cls.binary}" /Fo:"{out / "farsleep.obj"}"\n'
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

    def test_nothing_is_asked_of_the_game_while_off(self):
        self.assertScenario("off/no_calls")
        self.assertScenario("off/quiet_after_drain")

    def test_object_table_is_classified_in_bounded_slices(self):
        self.assertScenario("classify/finished")
        self.assertScenario("classify/bounded_per_frame")
        self.assertScenario("classify/past_the_gap")
        self.assertScenario("classify/asks_the_code_table")

    def test_only_scenery_sleeps(self):
        self.assertScenario("zone/never_the_others")

    def test_nothing_happens_while_a_room_settles(self):
        self.assertScenario("settle/no_scan_while_settling")

    def test_far_props_sleep_and_near_props_stay_awake(self):
        for label in ("zone/running", "zone/radii_follow_view", "zone/everything_in_place", "zone/some_asleep"):
            self.assertScenario(label)

    def test_runner_calls_stay_inside_the_budget(self):
        for label in ("zone/budget", "steady/cheap", "walk/budget", "jump/urgent_budget", "off/drain_budget"):
            self.assertScenario(label)

    def test_props_wake_as_a_player_comes_near(self):
        self.assertScenario("walk/props_wake_ahead")
        self.assertScenario("hysteresis/few_flips")
        self.assertScenario("jump/woken_at_once")
        self.assertScenario("players/second_player_neighbourhood")

    def test_a_prop_that_moved_is_read_again_before_it_sleeps(self):
        self.assertScenario("moving/stale_position_reread")

    def test_a_broken_prop_is_asked_only_whether_it_exists(self):
        self.assertScenario("broken/never_read")

    def test_solid_props_stay_awake_where_monsters_hunt(self):
        self.assertScenario("hunt/solid_radius")
        self.assertScenario("hunt/whole_map_keeps_solid_awake")

    def test_what_the_game_does_meanwhile_is_noticed(self):
        self.assertScenario("external/noticed")
        self.assertScenario("topup/late_prop_asleep")
        self.assertScenario("restart/rescanned")
        self.assertScenario("restart/old_ids_barely_addressed")

    def test_switching_off_wakes_everything(self):
        self.assertScenario("off/everything_awake")

    def test_a_room_change_forgets_the_old_room(self):
        self.assertScenario("room/old_ids_never_addressed")
        self.assertScenario("room/new_room_managed")

    def test_rooms_it_leaves_alone(self):
        for label in ("skip/town", "skip/menu", "skip/dev", "skip/persistent"):
            self.assertScenario(label)

    def test_a_refusing_runner_gives_the_zone_up(self):
        self.assertScenario("errors/zone_given_up")


if __name__ == "__main__":
    unittest.main()
