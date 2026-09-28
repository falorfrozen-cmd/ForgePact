"""Run the real ForgePact::HiddenLootMod class against a controlled game API.

Hidden loot sleep puts a ground item the player's own loot filter hides to
sleep (instance_deactivate_object) at the end of the frame it dropped in, and
shows the slept items while a key is held. These scenarios pin what the class
asks of the game (nothing while off, not even the key; nothing inside the
game's own drop call), what it acts on (only a Loot_Ground_obj whose verdict
reads hidden, found among the call's arguments and `self` whatever their
kind), what it counts instead of acting on, the hold key and its foreground
guard, switching off and on, room changes and persistent rooms, and the
fallback pass that takes over when the drop hook is table-only.
"""
import os
import shutil
import subprocess
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class HiddenLootBehaviorTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        header = (ROOT / "plugin/include/ForgePact/HiddenLootMod.hpp").read_text(encoding="utf-8")
        klass = "\n".join(
            line for line in header.split("\n")
            if not line.strip().startswith("#pragma once")
            and '#include "Common.hpp"' not in line
        )
        out = ROOT / "build/hidden-loot-behavior"
        out.mkdir(parents=True, exist_ok=True)
        code = (ROOT / "tests/hidden_loot_harness.cpp").read_text(encoding="utf-8")
        code = code.replace("// PRODUCTION_HIDDENLOOT", klass)
        cpp = out / "hiddenloot.cpp"
        cpp.write_text(code, encoding="utf-8")
        cls.binary = out / ("hiddenloot.exe" if os.name == "nt" else "hiddenloot")
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
                f'cl /nologo /std:c++20 /EHsc /O2 /I "{ROOT / "plugin/include"}" "{cpp}" /Fe:"{cls.binary}" /Fo:"{out / "hiddenloot.obj"}"\n'
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
        # Also covers what no single method below names: an unreadable verdict
        # and an unidentifiable call are counted and not acted on, keys 1 and
        # 2 are refused, key 0 polls nothing, a refusing runner is counted,
        # and no builtin outside the contract's list is called.
        self.assertIn("RESULT OK", self.output, self.output)
        for label in ("nofilter/counted_not_acted", "unidentified/counted_not_acted",
                      "key/default_left_alt", "key/refuses_mouse_buttons", "key/range", "key/zero_accepted",
                      "key/zero_polls_nothing", "errors/counted", "calls/only_listed_builtins"):
            self.assertScenario(label)

    def test_baseline_off_asks_the_game_nothing(self):
        for label in ("off/starts_off", "off/init_and_tick_no_calls", "off/key_not_read",
                      "off/drop_stays_awake", "off/inits_not_counted"):
            self.assertScenario(label)

    def test_hidden_drop_sleeps_at_the_end_of_its_frame(self):
        for label in ("drop/no_call_inside_init", "drop/one_deactivate_at_next_tick", "drop/never_twice",
                      "drop/identified_from_number", "drop/identified_from_self", "drop/same_item_once"):
            self.assertScenario(label)

    def test_visible_drop_is_never_touched(self):
        self.assertScenario("visible/never_touched")

    def test_holding_the_key_shows_and_release_hides(self):
        for label in ("hold/some_asleep", "hold/shows", "hold/steady_while_held", "release/hides_and_sleeps",
                      "release/gone_counted_without_call", "hold/visible_refusal_tolerated", "hold/refusal_rehides"):
            self.assertScenario(label)

    def test_drop_while_held_is_shown_then_hidden(self):
        self.assertScenario("held_drop/shown")
        self.assertScenario("held_drop/hidden_on_release")

    def test_key_ignored_when_game_not_in_front(self):
        self.assertScenario("front/key_ignored")

    def test_off_wakes_everything(self):
        for label in ("off/wakes_everything", "off/verdict_hidden", "off/forgotten", "on/walk_capped_off_wakes"):
            self.assertScenario(label)

    def test_switch_on_sleeps_what_is_on_the_ground(self):
        for label in ("on/walk_sleeps_hidden_ground", "on/walk_leaves_visible", "on/slept_counted", "on/walk_capped"):
            self.assertScenario(label)

    def test_room_change_forgets_without_calls(self):
        for label in ("room/walk_in_new_room", "room/forgets_without_calls", "room/old_ids_never_addressed",
                      "room/persistent_sleeps_nothing"):
            self.assertScenario(label)

    def test_table_only_route_uses_the_pass(self):
        for label in ("pass/never_with_both_routes", "pass/every_18_frames", "pass/visible_counted_once",
                      "pass/judged_items_not_reread", "pass/route_none_uses_it"):
            self.assertScenario(label)


if __name__ == "__main__":
    unittest.main()
