"""Run the real crafting-materials core against its baseline and target.

Companion to craft_mats_harness.cpp. ForgePact issue #14 asks that crafting at
the game's Crafting Cube can use materials held in the shared stash's
purchasable material tab. How the game counts and consumes a recipe's
materials is not measured yet (docs/crafting-materials-research.md, Phase 1
pending), so what is pinned here is the arithmetic every hypothesis shares,
which lives in plugin/include/ForgePact/CraftMatsMod.hpp - a header that is
game-independent by contract, spliced in whole with no runtime stub.

Owner's decisions (2026-09-22): count and consume; bag first, then the stash
tab, which supplies only the shortfall.

Baseline scenarios pin that the mod is off by default, that off never plans a
take whatever it is fed, and that a bag which covers the need plans nothing.
Target scenarios pin that the shortfall - and only the shortfall - is taken,
from the stash's material tab and no other source, never more than the tab
holds; that an unreadable count refuses the whole press and is named once;
that a take the game declined leaves the mod on and is named once; that a loss
signal (a tab that shrank without the game's success answer, or a success the
re-read cannot confirm) turns the mod off for the session; and that the stat
and first-take lines name what the mod did.

Phase 1e (the owner chose H-A) adds the kept stash map's currency rule,
CraftMatsKeptMap: the kept_map_baseline scenarios pin that nothing kept is not
current and that an index still held after a character load or a room change
is not current (GameMaker reuses map indices, so ds_exists alone proves
nothing); the kept_map_target scenarios pin that the game's own refresh makes
it current, again after an invalidation, and that a clear is not current.
"""
import os
import shutil
import subprocess
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "plugin/include/ForgePact/CraftMatsMod.hpp"


def spliceable(header_text):
    """The header without its #pragma/#include lines; the harness supplies
    the standard library itself."""
    return "\n".join(
        line for line in header_text.split("\n")
        if not line.strip().startswith(("#pragma", "#include"))
    )


class CraftMatsBehaviorTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        out = ROOT / "build/craft-mats-behavior"
        out.mkdir(parents=True, exist_ok=True)
        code = (ROOT / "tests/craft_mats_harness.cpp").read_text(encoding="utf-8")
        code = code.replace("// PRODUCTION_CRAFTMATS", spliceable(HEADER.read_text(encoding="utf-8")))
        cpp = out / "craftmats.cpp"
        cpp.write_text(code, encoding="utf-8")

        cls.binary = out / ("craftmats.exe" if os.name == "nt" else "craftmats")
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
                f'cl /nologo /std:c++20 /EHsc /O2 "{cpp}" /Fe:"{cls.binary}" /Fo:"{out / "craftmats.obj"}"\n'
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

    def test_every_scenario_is_a_baseline_or_a_target(self):
        labels = [line.split(" ")[1] for line in self.output.split("\n") if line.startswith(("PASS ", "FAIL "))]
        self.assertTrue(any(l.startswith("baseline/") for l in labels), labels)
        self.assertTrue(any(l.startswith("target/") for l in labels), labels)
        for label in labels:
            self.assertTrue(label.startswith(("baseline/", "target/")), label)

    # ---- baseline: off is vanilla ------------------------------------------

    def test_baseline_off_by_default(self):
        self.assertScenario("baseline/off_by_default")

    def test_baseline_off_never_plans_a_take(self):
        self.assertScenario("baseline/off_never_plans_a_take")

    def test_baseline_bag_covers_need_takes_nothing(self):
        # Includes the negative control for the unreadable rule: an unreadable
        # stash tab does not refuse a press the bag already covers.
        self.assertScenario("baseline/bag_covers_need_takes_nothing")

    # ---- target: on --------------------------------------------------------

    def test_target_deficit_is_taken_from_the_stash_tab_only(self):
        self.assertScenario("target/deficit_is_taken_from_the_stash_tab_only")
        self.assertScenario("target/duplicate_rows_of_one_material_are_summed")

    def test_target_never_takes_more_than_the_stash_holds(self):
        self.assertScenario("target/never_takes_more_than_the_stash_holds")

    def test_target_unreadable_source_refuses_and_is_named_once(self):
        self.assertScenario("target/unreadable_source_refuses_and_is_named_once")
        self.assertScenario("target/disagreeing_reads_of_one_material_refuse")

    def test_target_loss_signal_turns_the_mod_off_for_the_session(self):
        self.assertScenario("target/loss_signal_turns_the_mod_off_for_the_session")

    def test_target_a_declined_take_leaves_the_mod_on_and_is_named_once(self):
        self.assertScenario("target/not_taken_leaves_the_mod_on_and_is_named_once")
        self.assertScenario("target/report_while_off_is_ignored")

    def test_target_first_take_is_reported_once_and_the_stat_line_names_work_done(self):
        self.assertScenario("target/confirmed_take_counts_and_is_reported_once")
        self.assertScenario("target/statline_names_what_it_did")

    # ---- Phase 1e: the kept stash map's currency rule ------------------------
    #
    # A kept GetItemMap(9) return is current only when the game's own call
    # refreshed it after the latest character load or room change. GameMaker
    # reuses a destroyed map's index, so an index that still exists is not
    # evidence of currency (research doc, § Decision gate).

    def test_kept_map_baseline_nothing_kept_is_not_current(self):
        self.assertScenario("baseline/kept_map_nothing_kept_is_not_current")

    def test_kept_map_baseline_reused_index_is_not_current_after_an_invalidation(self):
        self.assertScenario("baseline/kept_map_reused_index_is_not_current_after_an_invalidation")

    def test_kept_map_target_refresh_makes_it_current(self):
        self.assertScenario("target/kept_map_refresh_makes_it_current")
        self.assertScenario("target/kept_map_reason_names_are_the_stat_tokens")

    def test_kept_map_target_refresh_after_an_invalidation_is_current_again(self):
        self.assertScenario("target/kept_map_refresh_after_an_invalidation_is_current_again")

    def test_kept_map_target_clear_is_not_current(self):
        self.assertScenario("target/kept_map_clear_is_not_current")


if __name__ == "__main__":
    unittest.main()
