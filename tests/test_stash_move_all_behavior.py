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
by its count, and the bag's Materials view feeds the Materials tab only. Live
1f and 1g decided the Socketable tab (socketMergeRoute: byname): fed from the
bag's Socket view only, a socketable whose identity has a node on the tab
merges and a new kind stays in the bag. And the in-game
Move all button (buttonRoute: poll): its node exists only while the switch is
on and the stash and Sort are listed, a left press inside its bbox is a press,
taken once under the key's guard, and a node that cannot be made is reported
once without turning the mod off.

ForgePact #131 (the owner's report of 2026-09-30): the route is decided per
stack, not per sum. The game's merge (the static reading of StashAddToStack)
takes the first stack of the kind whose count plus the item's stays at or
below the cap, 999, or 999999 with the sixth argument's flag 8 (the Socketable
tab's merge); the baseline pins that model. So on the Materials tab and a
stash page a stackable joins a stack with room for its whole count, and when
every stack of its kind is too full it starts a new stack in a free cell of
the same tab (no free cell: it stays in the bag); the Socketable tab, one
stack per kind, takes a socketable of any count onto that stack
(socketWholeStackMerge on, confirmed by Live procedure 3's socket-whole; the
flag off is kept as a negative control) and never starts a second stack; a
true answer on the cell route is decided as a merge by the sum. The button's
origin comes from Sort's box and the node's own extents: its right edge 8 GUI
units left of Sort, centred on it (UI_Button_Small_obj's origin is its bbox
centre, the Sort node's its top-left, Live 1f and 1g).
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

    # ---- the Socketable tab (socketMergeRoute, Live 1f and 1g) ---------------

    def test_baseline_socketable_tab_takes_only_the_bag_socket_view(self):
        # A bag page or the Materials view feeding the Socketable tab is
        # refused, and the Socket view feeds no other tab. Negative control:
        # the Socket view into the Socketable tab is a run.
        self.assertScenario("baseline/socketable_tab_takes_only_the_bag_socket_view")

    def test_target_socketable_merges_an_identity_with_a_node_by_its_whole_count(self):
        # Negative control: with socketMergeRoute not-observed the tab is
        # refused, as before Live 1f.
        self.assertScenario("target/socketable_merges_an_identity_with_a_node_by_its_whole_count")

    def test_target_socketable_new_kind_stays_in_the_bag(self):
        # socketRoute new: not-observed. Negative control: a measured
        # placement would go into a cell.
        self.assertScenario("target/socketable_new_kind_stays_in_the_bag")

    def test_target_socketable_whole_stack_merges_into_its_one_stack(self):
        # #131: socketWholeStackMerge is on; [81] + 3 merges by the whole
        # count, confirmed on the node rising by exactly 3. Negative control:
        # with the flag off, the same stack is a planned skip.
        self.assertScenario("target/socketable_whole_stack_merges_into_its_one_stack")

    def test_target_full_socketable_stack_never_starts_a_second_stack(self):
        # [999999] + 1 stays with the full-stack reason, even were the new
        # kind's placement measured. Negative control: [999998] + 1 merges.
        self.assertScenario("target/full_socketable_stack_never_starts_a_second_stack")

    # ---- #131: the game's merge rule and the per-stack route ------------------

    def test_baseline_game_merge_takes_a_stack_only_while_the_sum_stays_at_the_cap(self):
        # The static reading of StashAddToStack: cap 999, 999999 with flag 8;
        # the first stack that fits the whole count; unread is unknown.
        self.assertScenario("baseline/game_merge_takes_a_stack_only_while_the_sum_stays_at_the_cap")

    def test_target_full_materials_stack_overflows_into_a_free_cell(self):
        # The owner's report: [999] + 1 with room is a new stack in a cell.
        # Negative control: the sum-only rule's merge was refused and skipped.
        self.assertScenario("target/full_materials_stack_overflows_into_a_free_cell")

    def test_target_materials_merge_skips_the_full_stack_for_one_with_room(self):
        # [999, 400] + 500 merges; negative control [999, 600] + 500 is a cell.
        self.assertScenario("target/materials_merge_skips_the_full_stack_for_one_with_room")

    def test_target_merge_at_exactly_the_cap_is_a_merge(self):
        # [949] + 50 merges; negative control [950] + 50 is a cell.
        self.assertScenario("target/merge_at_exactly_the_cap_is_a_merge")

    def test_target_no_stack_fits_and_no_free_cell_stays_in_the_bag(self):
        # Never overflow holds for a new stack: no room calls nothing, an
        # unread list is a skip. Negative control: with room, it is placed.
        self.assertScenario("target/no_stack_fits_and_no_free_cell_stays_in_the_bag")

    def test_target_full_key_stack_on_a_page_overflows_into_a_free_cell(self):
        # A stash page too. Negative control: [998] + 1 merges.
        self.assertScenario("target/full_key_stack_on_a_page_overflows_into_a_free_cell")

    def test_target_unexpected_merge_on_the_cell_route_is_confirmed_as_a_merge(self):
        # The cell route passes the whole count; a true answer there is
        # decided as a merge by the sum. Negative control: decided on the
        # cell route it cannot be confirmed.
        self.assertScenario("target/unexpected_merge_on_the_cell_route_is_confirmed_as_a_merge")

    # ---- #131: the button's origin ---------------------------------------------

    def test_baseline_button_small_origin_is_its_centre_and_sort_origin_its_top_left(self):
        # Live 1f and 1g's geometry, and the old formula reproducing the
        # measured origin. Negative control: a box that did not read.
        self.assertScenario("baseline/button_small_origin_is_its_centre_and_sort_origin_its_top_left")

    def test_target_button_right_edge_sits_the_gap_left_of_sort_centred_on_it(self):
        # ButtonOrigin gives 2196.7, 1230.25 from the measured extents; a node
        # of another size is still placed right; the off-target line is said
        # once. Negative controls: unread boxes.
        self.assertScenario("target/button_right_edge_sits_the_gap_left_of_sort_centred_on_it")

    def test_target_old_button_origin_put_its_corner_inside_the_target_box(self):
        # The report reproduced: the old box's bottom-right corner lies inside
        # the target box. Negative control: its top-left does not.
        self.assertScenario("target/old_button_origin_put_its_corner_inside_the_target_box")

    def test_target_lines_name_what_moved_and_what_stayed(self):
        self.assertScenario("target/lines_name_what_moved_and_what_stayed")

    # ---- the route at the point of use (round-2 review) ----------------------

    def test_target_second_item_of_one_identity_merges_at_the_point_of_use_on_materials(self):
        # Two bag items of one identity the Materials tab lacked when the run
        # was planned: the first is placed, the second re-reads the sum at its
        # call and merges by its whole count. Negative control: the planned
        # cell route cannot confirm the one-unit merge the game did in its
        # place (the round-2 duplicate).
        self.assertScenario("target/second_item_of_one_identity_merges_at_the_point_of_use_on_materials")

    def test_target_second_item_of_one_identity_merges_at_the_point_of_use_on_a_page(self):
        self.assertScenario("target/second_item_of_one_identity_merges_at_the_point_of_use_on_a_page")

    def test_target_unreadable_stack_sum_at_the_point_of_use_skips(self):
        # With its negative controls: a sum of 0 is a cell, a non-stackable
        # needs no sum, a planned skip keeps its reason.
        self.assertScenario("target/unreadable_stack_sum_at_the_point_of_use_skips")

    def test_target_a_held_modifier_is_no_key_edge(self):
        # Alt+F4 closes the game; it must not start a run. Negative control:
        # the same press with no modifier is one run.
        self.assertScenario("target/a_held_modifier_is_no_key_edge")

    def test_target_owner_step_that_did_not_take_is_unconfirmed(self):
        # With its negative control: the personal page asks for no owner step.
        self.assertScenario("target/owner_step_that_did_not_take_is_unconfirmed")

    # ---- the in-game button (the core's side) --------------------------------

    def test_baseline_button_off_creates_nothing(self):
        # With its negative control: on, the same scene creates the node.
        self.assertScenario("baseline/button_off_creates_nothing")

    def test_target_button_exists_only_with_the_stash_and_sort_listed(self):
        self.assertScenario("target/button_exists_only_with_the_stash_and_sort_listed")

    def test_target_button_press_runs_once_under_the_key_guard(self):
        self.assertScenario("target/button_press_runs_once_under_the_key_guard")

    def test_target_button_press_is_a_left_press_inside_the_node_bbox(self):
        # buttonRoute: poll. Negative controls: a press just outside each
        # side, on Sort beside it, or on the panel background; a side that
        # did not read.
        self.assertScenario("target/button_press_is_a_left_press_inside_the_node_bbox")

    def test_target_button_refusal_is_reported_once_and_keeps_the_mod_on(self):
        # The fail-safe: F4 keeps working. Negative control: a node that
        # exists is kept.
        self.assertScenario("target/button_refusal_is_reported_once_and_keeps_the_mod_on")

    def test_target_button_counters_name_where_a_press_went(self):
        # A click that moved nothing is told apart on the state line:
        # poll-blind (presses=0 with the node held), a bbox miss (outside or
        # unread), a poll that threw, or a guard drop with its reason.
        # Negative control: a readable point outside the box still reads.
        self.assertScenario("target/button_counters_name_where_a_press_went")

    def test_target_off_for_this_session_state_line_keeps_the_button_fields(self):
        # After a loss the state line keeps the button's fields (a click that
        # ended in a loss reads from one line), the state word first and the
        # free-text reason last. Negative control: off by hand is plain off
        # with no reason.
        self.assertScenario("target/off_for_this_session_state_line_keeps_the_button_fields")


if __name__ == "__main__":
    unittest.main()
