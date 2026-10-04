"""Run the Loot announcements mod's real decision core against controlled drops.

`lootann 1` (ForgePact #17) announces in chat a ground item of Heroic (9),
Angelic (7) or Unholy (10) rarity, read from `itemInfoStruct["27"]`, once per
item (the ground instance id and the item's `itemTimeStamp`), and never an
item the player drops from the bag (the `LootGroundDrop` detour's window).
These scenarios pin the decision ModuleMain.cpp's adapter takes from
plugin/include/ForgePact/LootAnnounceMod.hpp, compiled whole.

Baseline: with the switch off a Heroic item, and every other rarity, is not
announced, and nothing is counted or remembered. Target: with it on, Heroic,
Angelic and Unholy announce once; Satanic, Mythic, Common and every other
code, and an unreadable rarity, do not; a second sight of the same item does
not; a bag drop does not; the memory is kept across off/on and capped; the
stat line counts what happened.
"""
import os
import shutil
import subprocess
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class LootAnnounceBehaviorTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        header = (ROOT / "plugin/include/ForgePact/LootAnnounceMod.hpp").read_text(encoding="utf-8")
        core = "\n".join(line for line in header.split("\n") if not line.strip().startswith("#pragma once"))
        out = ROOT / "build/loot-announce-behavior"
        out.mkdir(parents=True, exist_ok=True)
        code = (ROOT / "tests/loot_announce_harness.cpp").read_text(encoding="utf-8")
        code = code.replace("// PRODUCTION_LOOT_ANNOUNCE_MOD", core)
        cpp = out / "lootannounce.cpp"
        cpp.write_text(code, encoding="utf-8")
        cls.binary = out / ("lootannounce.exe" if os.name == "nt" else "lootannounce")
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
                f'cl /nologo /std:c++20 /EHsc /O2 /W4 /I "{ROOT / "plugin/include"}" "{cpp}" /Fe:"{cls.binary}" /Fo:"{out / "lootannounce.obj"}"\n'
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

    def test_the_announced_set_is_heroic_angelic_unholy(self):
        self.assertScenarios("table/announced_rarities_are_heroic_angelic_unholy", "table/is_announced_rarity_exact",
                             "table/unread_is_not_a_rarity", "table/shipped_sink_and_names")

    # ---- baseline: what the game does without the mod ---------------------

    def test_switch_off_a_heroic_item_is_not_announced(self):
        self.assertScenarios("baseline/starts_off", "baseline/off_heroic_not_announced",
                             "baseline/off_every_rarity_not_announced", "baseline/off_counts_and_remembers_nothing",
                             "baseline/item_seen_while_off_is_new_when_on")

    # ---- target: what the mod does ----------------------------------------

    def test_heroic_angelic_and_unholy_announce_once(self):
        self.assertScenarios("target/on_status", "target/heroic_announced", "target/angelic_announced",
                             "target/unholy_announced", "target/three_announced_counted")

    def test_satanic_mythic_and_common_do_not(self):
        self.assertScenarios("target/satanic_not_announced", "target/mythic_not_announced",
                             "target/common_not_announced", "target/other_codes_not_announced",
                             "target/unread_rarity_not_announced_and_counted",
                             "target/held_counted_never_announced_never_remembered")

    def test_a_second_sight_of_the_same_item_does_not(self):
        self.assertScenarios("target/second_sight_not_announced", "target/key_is_ground_id_and_time_stamp",
                             "target/memory_kept_across_off_on")

    def test_a_bag_drop_does_not(self):
        self.assertScenarios("target/bag_drop_window_closed_at_start", "target/bag_drop_window_open_in_scope",
                             "target/bag_drop_not_announced", "target/bag_drop_window_nests",
                             "target/bag_drop_window_closes", "target/bag_drop_not_remembered",
                             "target/after_bag_drop_a_game_drop_announces", "target/bag_drop_window_never_negative")

    def test_the_memory_is_capped_oldest_first(self):
        self.assertScenarios("memory/capped", "memory/oldest_forgotten_first")

    def test_the_stat_line(self):
        self.assertScenarios("stat/fresh_off", "stat/counts", "stat/verdict_names")


if __name__ == "__main__":
    unittest.main()
