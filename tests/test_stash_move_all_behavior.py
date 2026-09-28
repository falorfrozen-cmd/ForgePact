"""Run the real "Move all into the stash" core against its baseline and target.

Companion to stash_move_all_harness.cpp. ForgePact #68 asks for one key that,
with the stash open, moves every item on the bag tab on show into the stash
tab on show, each item by the game's own per-item move, called by name. Every
decision the player build's adapter acts on lives in
plugin/include/ForgePact/StashMoveAllMod.hpp - a header that is
game-independent by contract, spliced in whole with no runtime stub.

Owner's decisions (2026-09-27, docs/stash-move-research.md): call the game's
own per-item move once per item; never write a container or a map entry; a
refusal the game gives (no room, not taken) skips the item and the run goes
on; an outcome the re-read cannot confirm stops the run and turns the mod off
for the session.

Baseline scenarios pin that the mod is off by default, that off plans nothing
whatever the bag holds, and that a key press with the switch off is nothing.
Target scenarios pin that every item of a mixed tab is planned once, in
row-major order by its first cell; that a tab answering "no room" leaves the
rest in the bag; that a stackable plans a stack when a stack of its identity is
there and a special tab takes only its own class; that a refused item is
skipped and the next continues; that an item is moved only when both sides
confirm it and anything else stops the run and turns the mod off; and that the
lines name what moved and what stayed. The owner's 2026-09-28 rule, never
overflow: an item the shown stash tab has no room for is skipped with nothing
called, so it stays in the bag and every other tab is unchanged, and a shown
tab that changed during the move, or could not be re-read, is unconfirmed.

The routes Live 1e decided (docs/stash-move-research.md § Decision): a new
material identity goes into a cell of the Materials tab, a whole stack merges
by its count, and the Socketable tab and the bag's Socket view are refused;
the bag's Materials view feeds the Materials tab only.
"""
import os
import shutil
import subprocess
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "plugin/include/ForgePact/StashMoveAllMod.hpp"


def spliceable(header_text):
    """The header without its #pragma/#include lines; the harness supplies
    the standard library itself."""
    return "\n".join(
        line for line in header_text.split("\n")
        if not line.strip().startswith(("#pragma", "#include"))
    )


class StashMoveAllBehaviorTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        out = ROOT / "build/stash-move-all-behavior"
        out.mkdir(parents=True, exist_ok=True)
        code = (ROOT / "tests/stash_move_all_harness.cpp").read_text(encoding="utf-8")
        code = code.replace("// PRODUCTION_STASHMOVEALL", spliceable(HEADER.read_text(encoding="utf-8")))
        cpp = out / "stashmoveall.cpp"
        cpp.write_text(code, encoding="utf-8")

        cls.binary = out / ("stashmoveall.exe" if os.name == "nt" else "stashmoveall")
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
                f'cl /nologo /std:c++20 /EHsc /O2 "{cpp}" /Fe:"{cls.binary}" /Fo:"{out / "stashmoveall.obj"}"\n'
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

    def test_baseline_off_plans_nothing_whatever_the_bag_holds(self):
        # With its negative control: the same view, on, plans every item.
        self.assertScenario("baseline/off_plans_nothing_whatever_the_bag_holds")

    def test_baseline_key_press_with_the_switch_off_is_nothing(self):
        # With its negative control: on, a press in the foreground with the
        # stash listed starts one run per press, and neither a held key, a
        # background window nor a missing stash starts one.
        self.assertScenario("baseline/key_press_with_the_switch_off_is_nothing")

    # ---- target: the plan ----------------------------------------------------

    def test_target_mixed_tab_every_item_planned_once_in_order(self):
        self.assertScenario("target/mixed_tab_every_item_planned_once_in_order")

    def test_target_refused_runs_name_the_reason_and_plan_nothing(self):
        self.assertScenario("target/refused_runs_name_the_reason_and_plan_nothing")

    def test_target_stackable_plans_a_stack_when_its_identity_is_there(self):
        self.assertScenario("target/stackable_plans_a_stack_when_its_identity_is_there")

    # ---- target: the outcome -------------------------------------------------

    def test_target_no_room_skips_and_the_rest_stay_in_the_bag(self):
        self.assertScenario("target/no_room_skips_and_the_rest_stay_in_the_bag")

    def test_target_refused_item_is_skipped_and_the_next_continues(self):
        self.assertScenario("target/refused_item_is_skipped_and_the_next_continues")

    def test_target_moved_only_when_both_sides_confirm(self):
        self.assertScenario("target/moved_only_when_both_sides_confirm")

    def test_target_unconfirmed_item_stops_the_run_and_turns_the_mod_off(self):
        self.assertScenario("target/unconfirmed_item_stops_the_run_and_turns_the_mod_off")

    # ---- target: never overflow (D4) -----------------------------------------

    def test_full_shown_tab_keeps_item_in_bag_and_other_tabs_unchanged(self):
        # The owner's 2026-09-28 rule: only what the shown stash tab has room
        # for moves. Against a stand-in routine that spills into the next tab
        # with room, a full shown tab calls nothing, the items stay in the bag
        # and every other tab is unchanged; with one free cell, one item moves
        # and the rest stay. An unreadable room check calls nothing either.
        self.assertScenario("target/full_shown_tab_keeps_item_in_bag_and_other_tabs_unchanged")

    def test_target_shown_tab_changed_or_unread_is_unconfirmed(self):
        # Only the shown tab is re-read (the others have no container readable
        # by name; owner, 2026-09-28, "Accept"). With its negative control: the
        # tab unchanged and the key at the answer's cell is moved; the tab on
        # show moved off the planned one, or unreadable, is a loss.
        self.assertScenario("target/shown_tab_changed_or_unread_is_unconfirmed")

    def test_target_room_is_a_free_block_of_the_items_footprint_on_the_shown_tab(self):
        self.assertScenario("target/room_is_a_free_block_of_the_items_footprint_on_the_shown_tab")

    # ---- the routes Live 1e decided ------------------------------------------

    def test_baseline_grid_tab_destination_from_a_bag_sub_tab_is_refused(self):
        # With its negative control: a bag page is a source for a stash page.
        self.assertScenario("baseline/grid_tab_destination_from_a_bag_sub_tab_is_refused")

    def test_target_bag_materials_view_feeds_the_materials_tab(self):
        self.assertScenario("target/bag_materials_view_feeds_the_materials_tab")

    def test_target_new_material_identity_is_placed_in_a_cell(self):
        # newMaterialRoute: byname. Negative control: the route not measured
        # makes it a skip that calls nothing.
        self.assertScenario("target/new_material_identity_is_placed_in_a_cell")

    def test_target_whole_stack_merges_by_its_count(self):
        # wholeStackMerge: byname. Negative control: not measured, more than
        # one unit is a skip and one unit still merges.
        self.assertScenario("target/whole_stack_merges_by_its_count")

    def test_target_socketable_tab_and_socket_view_are_refused(self):
        # socketRoute: neither path byname. Negative control: a measured path
        # would let class 15 in from the bag's Socket view, and nothing else.
        self.assertScenario("target/socketable_tab_and_socket_view_are_refused")

    def test_target_lines_name_what_moved_and_what_stayed(self):
        self.assertScenario("target/lines_name_what_moved_and_what_stayed")


if __name__ == "__main__":
    unittest.main()
