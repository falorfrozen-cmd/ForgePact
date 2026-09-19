"""Run the real auto-prospect core against its baseline and target.

Companion to auto_prospect_harness.cpp. ForgePact issue #9's Stage B is
auto-prospect on insert (the human's decision, 2026-09-18): each item moved
into the Prospect Cube's grid is prospected at once by the game's own Prospect
operation. What the mod decides - when an insert counts, when it invokes, when
it refuses and what it says - lives in
plugin/include/ForgePact/AutoProspectMod.hpp, a header that is game-independent
by contract, so it is spliced in whole with no runtime stub.

Baseline scenarios pin that the mod is off by default and, off, never invokes
whatever it is fed. Target scenarios pin one invoke per landed insert into the
ProspectGrid (whether the cells update in the same frame or later), nothing for
an insert elsewhere, a rearrangement, an insert the game makes inside our own
invoke or a pending insert on a replaced node, coalescing, the grid-full, no
window and no button refusals, the settled count following a new node and a
removal, `ran-no-effect` one frame later, each refusal reported once, and a
stat line that names what the mod did (Known Limitations item 7).

Stage C adds the move pass: with the `bag` sub-option on (its default), a
landed insert first sends the previous prospect's batch - the fingerprints the
core's own invoke produced, as recorded in that invoke's frame, and of those
only the cells the adapter flagged as materials through the SDK - to the
materials tab, then prospects in the same frame. Baselines pin that bag off,
or the parent off, never moves anything; targets pin one pass per landed
insert, before the invoke, naming batch materials only, the landed insert
staying landed across it, a refused move leaving the material and being named
once, `vanished`/`cell-kept` turning the pass off for the session, and the
player-log `first move to bag` line once.

Round 1 (after Phase 3 live on e63eed5 moved an inserted ore back to the tab
unprospected) pins that the insert is never moved, ore included: nothing moves
on the first prospect of a session, a hand-placed material never moves, the
batch is forgotten on a new node, the parent toggle, a removal or a
`not-landed` expiry, a batch is named at most once, and success with an
unreadable final cell is `cell-kept`.

Stage D (after the c27cdad re-run left every material of a type with no stack
yet in the grid) pins the new-type route: the stack route is unchanged, a
material the game placed through its preferred grid and cleared counts as
`moved` and `moved-new`, no named grid and an unconfirmed place each leave the
material and are named once, `vanished`/`cell-kept` on the new route turn the
pass off like the old one, and the stat line names the route (`not-stackable`
is gone).
"""
import os
import shutil
import subprocess
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "plugin/include/ForgePact/AutoProspectMod.hpp"


def spliceable(header_text):
    """The header without its #pragma/#include lines, which name the plugin's
    runtime headers; the harness supplies the standard library itself."""
    return "\n".join(
        line for line in header_text.split("\n")
        if not line.strip().startswith(("#pragma", "#include"))
    )


class AutoProspectBehaviorTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        out = ROOT / "build/auto-prospect-behavior"
        out.mkdir(parents=True, exist_ok=True)
        code = (ROOT / "tests/auto_prospect_harness.cpp").read_text(encoding="utf-8")
        code = code.replace("// PRODUCTION_AUTOPROSPECT", spliceable(HEADER.read_text(encoding="utf-8")))
        cpp = out / "autoprospect.cpp"
        cpp.write_text(code, encoding="utf-8")

        cls.binary = out / ("autoprospect.exe" if os.name == "nt" else "autoprospect")
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
                f'cl /nologo /std:c++20 /EHsc /O2 "{cpp}" /Fe:"{cls.binary}" /Fo:"{out / "autoprospect.obj"}"\n'
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
            self.assertTrue(label.startswith(("baseline/", "target/", "adapter/")), label)

    # ---- baseline: off is vanilla ------------------------------------------

    def test_baseline_off_by_default(self):
        self.assertScenario("baseline/off_by_default")

    def test_baseline_off_never_invokes(self):
        self.assertScenario("baseline/off_never_invokes")

    # ---- target: on --------------------------------------------------------

    def test_target_insert_into_prospect_grid_invokes_once(self):
        self.assertScenario("target/insert_into_prospect_grid_invokes_once")
        self.assertScenario("target/late_landing_invokes_when_it_lands")

    def test_target_insert_elsewhere_is_ignored(self):
        self.assertScenario("target/insert_elsewhere_ignored")

    def test_target_rearrangement_never_invokes_and_expires_not_landed(self):
        self.assertScenario("target/rearrangement_never_invokes_and_expires_not_landed")

    def test_target_insert_while_invoking_is_ignored_and_counted(self):
        self.assertScenario("target/insert_while_invoking_ignored_and_counted")

    def test_target_inserts_in_one_frame_coalesce(self):
        self.assertScenario("target/inserts_in_one_frame_coalesce")

    def test_target_grid_full_refuses_and_counts(self):
        self.assertScenario("target/grid_full_refuses_and_counts")

    def test_target_no_window_or_button_refuses_and_drops_the_insert(self):
        self.assertScenario("target/no_window_refuses_and_drops_pending")
        self.assertScenario("target/no_button_refuses_and_drops_pending")
        self.assertScenario("target/pending_on_a_replaced_node_is_dropped")

    def test_target_new_node_resets_the_settled_count(self):
        self.assertScenario("target/new_node_resets_settled_count")
        self.assertScenario("target/removal_lowers_settled_count")

    def test_target_ran_no_effect_is_counted_one_frame_later(self):
        self.assertScenario("target/ran_no_effect_counted_one_frame_later")

    def test_target_first_refusal_is_reported_once_per_reason(self):
        self.assertScenario("target/first_refusal_reported_once_per_reason")

    def test_target_stat_line_names_what_the_mod_did(self):
        self.assertScenario("target/statline_names_what_it_did")

    # ---- target: insert timing (Phase 1 measured contents=6->6 across a click-in)

    def test_target_an_insert_filled_before_its_hook_still_invokes_once(self):
        self.assertScenario("target/click_in_filled_in_the_same_frame_invokes_once")
        self.assertScenario("target/click_in_filled_a_frame_before_the_hook_invokes_once")
        self.assertScenario("target/insert_right_after_an_invoke_invokes_again")

    def test_target_what_is_already_settled_never_invokes_when_moved(self):
        self.assertScenario("target/rearrangement_right_after_an_invoke_never_invokes")
        self.assertScenario("target/refused_item_rearranged_never_invokes")

    def test_target_no_args_refuses_and_says_why(self):
        self.assertScenario("target/no_args_refuses_and_says_why")

    def test_target_first_prospect_is_reported_once_in_the_players_log(self):
        self.assertScenario("target/first_prospect_reported_once_in_the_players_log")

    def test_target_an_invoke_that_did_nothing_is_reported_once_in_the_players_log(self):
        # Phase 3 S7 counted ran-no-effect=1 that only the research-only stat
        # line could show; the negative control keeps a working prospect quiet.
        self.assertScenario("baseline/prospects_with_effect_log_no_nothing_happened_line")
        self.assertScenario("target/ran_no_effect_reported_once_in_the_players_log")
        self.assertScenario("target/unverified_reported_once_in_the_players_log")

    # ---- adapter: the shape Phase 1 recorded --------------------------------

    def test_adapter_recorded_shape(self):
        self.assertScenario("adapter/recorded_shape_exec_index_button_activation_args_self_found")
        self.assertScenario("adapter/measured_session_prospects_each_insert_once")

    # ---- Stage C: the previous batch goes to the materials tab first ----------

    def test_baseline_bag_off_never_moves(self):
        self.assertScenario("baseline/bag_off_never_moves")

    def test_baseline_parent_off_never_moves(self):
        self.assertScenario("baseline/parent_off_never_moves")

    def test_target_bag_on_by_default_and_kept_across_the_parent_toggle(self):
        self.assertScenario("target/bag_on_by_default_and_kept_across_the_parent_toggle")

    def test_target_move_pass_before_the_invoke(self):
        self.assertScenario("target/move_pass_before_the_invoke")

    def test_target_non_material_never_moved(self):
        self.assertScenario("target/non_material_never_moved")

    def test_target_move_pass_only_when_an_insert_lands(self):
        self.assertScenario("target/move_pass_only_when_an_insert_lands")

    def test_target_landed_insert_stays_landed_across_the_move_pass(self):
        self.assertScenario("target/landed_insert_stays_landed_across_the_move_pass")

    def test_target_refused_move_leaves_the_material_and_is_logged_once(self):
        self.assertScenario("target/refused_move_leaves_the_material_and_is_logged_once")

    def test_target_vanished_turns_the_move_pass_off_for_the_session(self):
        self.assertScenario("target/vanished_turns_the_move_pass_off_for_the_session")

    def test_target_cell_kept_after_add_turns_the_move_pass_off_for_the_session(self):
        self.assertScenario("target/cell_kept_after_add_turns_the_move_pass_off_for_the_session")

    def test_target_first_move_reported_once_in_the_players_log(self):
        self.assertScenario("target/first_move_reported_once_in_the_players_log")

    # ---- Stage C round 1: the move set is the recorded batch -------------------
    # Live on e63eed5 an inserted ore (itself a material) was moved to the tab
    # and never prospected; only what the core's own invoke produced moves now.

    def test_target_first_prospect_of_a_session_moves_nothing(self):
        self.assertScenario("target/first_prospect_of_a_session_moves_nothing")

    def test_target_ore_insert_moves_only_the_previous_batch(self):
        self.assertScenario("target/ore_insert_moves_only_the_previous_batch")

    def test_target_hand_placed_material_is_never_moved(self):
        self.assertScenario("target/hand_placed_material_is_never_moved")

    def test_target_batch_forgotten_on_a_new_node_or_the_parent_toggle(self):
        self.assertScenario("target/batch_forgotten_on_a_new_node_or_the_parent_toggle")

    def test_target_batch_forgotten_after_a_removal_or_an_unlanded_insert(self):
        self.assertScenario("target/batch_forgotten_after_a_removal_or_an_unlanded_insert")

    def test_target_a_batch_is_moved_at_most_once(self):
        self.assertScenario("target/a_batch_is_moved_at_most_once")

    def test_target_success_with_an_unreadable_cell_turns_the_move_pass_off(self):
        self.assertScenario("target/success_with_an_unreadable_cell_turns_the_move_pass_off")

    # ---- Stage D: a material whose type has no stack yet -----------------------
    # Live on c27cdad such a material was refused (`not-stackable`) and piled up;
    # the game's own click-move uses the preferred grid and a place instead.

    def test_baseline_existing_stack_route_is_unchanged(self):
        self.assertScenario("baseline/existing_stack_route_is_unchanged")

    def test_target_new_type_placed_and_cleared_counts_as_moved(self):
        self.assertScenario("target/new_type_placed_and_cleared_counts_as_moved")

    def test_target_new_type_without_a_preferred_grid_stays_and_is_logged_once(self):
        self.assertScenario("target/new_type_without_a_preferred_grid_stays_and_is_logged_once")

    def test_target_new_type_not_placed_stays_and_is_logged_once(self):
        self.assertScenario("target/new_type_not_placed_stays_and_is_logged_once")

    def test_target_new_type_vanished_or_kept_turns_the_move_pass_off(self):
        self.assertScenario("target/new_type_vanished_or_kept_turns_the_move_pass_off")

    def test_target_stat_line_names_the_new_type_route(self):
        self.assertScenario("target/stat_line_names_the_new_type_route")


if __name__ == "__main__":
    unittest.main()
