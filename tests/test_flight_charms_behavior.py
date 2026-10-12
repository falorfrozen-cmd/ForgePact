"""Run the flight charms' real decision core against a controlled world.

ForgePact #158 phase 2 adds three built-in charms: Pocket Elf, Gabriel Wings
and Witch's Broomstick. While one is worn the local player walks through
scenery (the Collision_Parent_obj family) but not through walls, gates,
locks, monsters or the room's edge, and is drawn in front of scenery. These
scenarios pin the decisions ModuleMain.cpp's adapter takes from
plugin/include/ForgePact/FlightCharms.hpp, compiled whole, together with
FlightCharmProbe.hpp, which shares its namespace.

Test names carry the substrings the workorder's criteria select with `-k`:
`seed_codec` (the table and the seeds, a save format), `exclusion` (no charm
worn: every query real, the baseline; worn: a blocked family query answered,
walls, locks, monsters, the room's edge and other selves kept, the target),
`residual`, `size_guard` (outside a load scope nothing changes, the baseline;
inside it a dressed charm waits for the vanilla items, the target;
`size_guard_two_charms`: two charms in one load both stay on the grid, in
either save order, read either way round and with a cell unknown;
`size_guard_counters`: the guard's own line), `trace_sums` (the trace
line's counters add up as FlightCounters says), `front_layer` (the flight
layer's depth, its census once per room visit and its re-choice at most once
a second, validated where it is written; no write while off, residual or for
another self, the baseline), `front_layer_filter` (never past a filter: the
rearmost filter decides, on the data file's room stacks), `tooltip_rows`
(a forged item's gold rows anchor at its first present format-2/3 stat call,
the baseline, or at the first format-2/3 call on an item with no stats,
Pocket Elf, the target; ForgedTooltipRows.hpp, which the harness includes),
`stand_in`
(the even-count rule and the coin) and `stat_line` (the fresh session's line,
byte for byte).
"""
import os
import shutil
import subprocess
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class FlightCharmsCoreBehaviorTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        header = (ROOT / "plugin/include/ForgePact/FlightCharms.hpp").read_text(encoding="utf-8")
        core = "\n".join(line for line in header.split("\n") if not line.strip().startswith("#pragma once"))
        # One directory per process: the criteria runner starts several `-k`
        # selections at once, and two compiles into one .obj fail with
        # "Permission denied". A failure carries the compiler's output in its
        # message and every scenario assertion quotes the harness's output, so
        # the directory goes when the class does.
        out = ROOT / "build/flight-charms-behavior" / f"pid-{os.getpid()}"
        out.mkdir(parents=True, exist_ok=True)
        cls.addClassCleanup(shutil.rmtree, out, ignore_errors=True)
        code = (ROOT / "tests/flight_charms_harness.cpp").read_text(encoding="utf-8")
        code = code.replace("// PRODUCTION_FLIGHTCHARMS", core)
        cpp = out / "flightcharmscore.cpp"
        cpp.write_text(code, encoding="utf-8")
        cls.binary = out / ("flightcharmscore.exe" if os.name == "nt" else "flightcharmscore")
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
                f'cl /nologo /std:c++20 /EHsc /O2 /W4 /WX /I "{ROOT / "plugin/include"}" "{cpp}" /Fe:"{cls.binary}" /Fo:"{out / "flightcharmscore.obj"}"\n'
                f'exit /b %errorlevel%\n', encoding="utf-8")
            command = ["cmd", "/d", "/c", str(batch)]
        else:
            compiler = shutil.which("c++")
            if not compiler:
                raise unittest.SkipTest("A C++20 compiler is required for native behavior tests")
            command = [compiler, "-std=c++20", "-O2", "-Wall", "-I", str(ROOT / "plugin/include"), str(cpp), "-o", str(cls.binary)]
        # The compiler speaks the machine's locale; decode leniently so a
        # localized diagnostic cannot itself crash the test.
        result = subprocess.run(command, cwd=out, capture_output=True, text=True, encoding="utf-8", errors="replace")
        if result.returncode:
            raise AssertionError(result.stdout + result.stderr)
        run = subprocess.run([str(cls.binary)], capture_output=True, text=True, encoding="utf-8", errors="replace")
        cls.output = run.stdout

    def line(self, label):
        for line in self.output.split("\n"):
            if line.split(" ")[1:2] == [label]:
                return line
        raise AssertionError(f"scenario {label!r} not in harness output:\n{self.output}")

    def assertScenarios(self, *labels):
        for label in labels:
            self.assertTrue(self.line(label).startswith("PASS "), self.line(label))

    def test_all_scenarios_pass(self):
        self.assertIn("RESULT OK", self.output, self.output)
        self.assertNotIn("FAIL ", self.output, self.output)

    # ---- the item table and the seed codec (a save format) ----------------

    def test_seed_codec_the_table(self):
        self.assertScenarios("seed_codec/the_table", "seed_codec/keys_and_stats")

    def test_seed_codec_round_trips_and_range_edges(self):
        self.assertScenarios("seed_codec/round_trips_every_roll", "seed_codec/every_range_edge_and_one_past_it",
                             "seed_codec/an_out_of_range_roll_has_no_seed",
                             "seed_codec/no_collision_with_seeds_in_use")

    def test_seed_codec_t_b_c_or_j_off_by_one_is_not_a_flight_charm(self):
        self.assertScenarios("seed_codec/t_b_c_or_j_off_by_one_is_not_a_flight_charm")

    def test_seed_codec_a_uniform_draw(self):
        self.assertScenarios("seed_codec/a_uniform_draw_covers_each_roll_once")

    # ---- flight's answers: baseline, then target --------------------------

    def test_exclusion_baseline_no_charm_every_query_real(self):
        self.assertScenarios("exclusion/the_six_rows", "exclusion/baseline_no_charm_every_query_real")

    def test_exclusion_target_family_answered(self):
        self.assertScenarios("exclusion/target_a_blocked_family_query_is_answered",
                             "exclusion/target_block_dynamic_sprite_is_family_not_a_wall",
                             "exclusion/target_a_free_answer_stands")

    def test_exclusion_target_walls_locks_monsters_and_the_room_edge_keep_their_answer(self):
        self.assertScenarios("exclusion/target_a_wall_or_lock_named_query_keeps_its_answer",
                             "exclusion/target_a_query_whose_rerun_a_wall_blocks_keeps_its_answer",
                             "exclusion/target_a_monster_named_query_keeps_its_answer",
                             "exclusion/target_a_point_outside_the_room_keeps_its_answer")

    def test_exclusion_target_another_self_is_never_touched(self):
        self.assertScenarios("exclusion/target_another_self_is_never_touched")

    def test_exclusion_instance_place_tallest(self):
        self.assertScenarios("exclusion/instance_place_tallest_family_result_noone_wall_result_stays")

    def test_exclusion_point_of_use_and_fail_closed(self):
        self.assertScenarios("exclusion/the_worn_charms_are_read_once_a_frame",
                             "exclusion/an_unresolved_family_or_exclusion_answers_for_nothing",
                             "exclusion/each_object_is_classified_once")

    # ---- residual -----------------------------------------------------------

    def test_residual_taken_off_inside_a_prop(self):
        self.assertScenarios("residual/taken_off_inside_a_prop_until_the_first_free_read",
                             "residual/worn_again_goes_back_on")

    def test_residual_taken_off_outside_a_prop_and_baseline(self):
        self.assertScenarios("residual/taken_off_outside_a_prop_goes_straight_to_off",
                             "residual/baseline_no_charm_is_never_residual",
                             "residual/an_unreadable_read_fails_closed")

    # ---- the size guard -----------------------------------------------------

    def test_size_guard_baseline(self):
        self.assertScenarios("size_guard/baseline_outside_the_scope_or_for_another_item_nothing_changes")

    def test_size_guard_target_deferral_and_order(self):
        self.assertScenarios("size_guard/target_deferred_to_the_end_of_the_load_whatever_the_order",
                             "size_guard/target_a_vanilla_item_places_first",
                             "size_guard/target_a_full_footprint_that_fits_is_placed_whole",
                             "size_guard/target_a_1x1_charm_is_never_deferred")

    def test_size_guard_target_kept_and_failed(self):
        self.assertScenarios("size_guard/target_a_refused_full_footprint_becomes_1x1_at_its_own_cell",
                             "size_guard/target_a_1x5_charm_is_deferred_and_kept",
                             "size_guard/target_a_refused_1x1_counts_failed",
                             "size_guard/flushing_defers_nothing_and_a_throw_counts_failed",
                             "size_guard/the_log_line")

    def test_size_guard_target_nested_scopes(self):
        self.assertScenarios("size_guard/target_nested_scopes_flush_their_own_lists")

    def test_size_guard_two_charms_in_one_load_both_stay_on_the_grid(self):
        # The single-charm baselines first: a full footprint that fits goes in
        # whole, one that does not becomes 1x1 at its own cell.
        self.assertScenarios("size_guard/target_a_full_footprint_that_fits_is_placed_whole",
                             "size_guard/target_a_refused_full_footprint_becomes_1x1_at_its_own_cell",
                             "size_guard/baseline_two_charms_unguarded_the_first_covers_the_second",
                             "size_guard/target_two_charms_whose_footprint_covers_the_others_cell_keep_both",
                             "size_guard/target_a_transposed_footprint_still_yields",
                             "size_guard/target_an_unknown_cell_yields_when_another_charm_is_held",
                             "size_guard/target_one_charm_with_an_unknown_cell_is_not_shrunk")

    def test_size_guard_counters_say_how_far_a_load_got(self):
        self.assertScenarios("size_guard/saw_counts_only_inside_a_load_and_outside_a_flush",
                             "size_guard/the_guard_stat_line")

    # ---- the trace line -----------------------------------------------------

    def test_trace_sums_as_flight_counters_documents(self):
        self.assertScenarios("trace/the_counters_sum_as_documented")

    # ---- drawing in front ---------------------------------------------------

    def test_front_layer_no_write_for_off_residual_or_another_self(self):
        self.assertScenarios("front_layer/no_write_for_off_residual_or_another_self")

    def test_front_layer_one_in_front_of_the_lowest_drawn_layer(self):
        self.assertScenarios("front_layer/the_game_front_is_the_lower_end",
                             "front_layer/one_in_front_of_the_lowest_drawn_layer",
                             "front_layer/hidden_empty_and_the_flight_layer_itself_never_count")

    def test_front_layer_an_unreadable_game_front_writes_nothing(self):
        self.assertScenarios("front_layer/an_unreadable_game_front_writes_nothing")

    def test_front_layer_census_once_per_visit_rechoice_once_a_second(self):
        self.assertScenarios("front_layer/the_census_once_per_visit_the_rechoice_once_a_second",
                             "front_layer/a_census_that_yields_nothing_retries_once_a_second")

    def test_front_layer_validated_where_it_is_written_and_found_by_name(self):
        self.assertScenarios("front_layer/validated_where_it_is_written_and_found_by_name")

    # ---- never past a filter (the owner, 2026-10-13) --------------------------

    def test_front_layer_filter_none(self):
        self.assertScenarios("front_layer_filter/no_filter")

    def test_front_layer_filter_in_front_of_every_drawn_layer(self):
        self.assertScenarios("front_layer_filter/a_filter_in_front_of_every_drawn_layer")

    def test_front_layer_filter_behind_the_frontmost_drawn_layer(self):
        self.assertScenarios("front_layer_filter/a_filter_behind_the_frontmost_drawn_layer")

    def test_front_layer_filter_several_the_rearmost_decides(self):
        self.assertScenarios("front_layer_filter/several_filters_the_rearmost_decides")

    def test_front_layer_filter_hidden_or_behind_the_game_front_is_ignored(self):
        self.assertScenarios("front_layer_filter/a_hidden_filter_and_one_behind_the_game_front_change_nothing")

    def test_front_layer_filter_an_empty_effect_layer_that_fills(self):
        self.assertScenarios("front_layer_filter/an_empty_effect_layer_that_fills")

    def test_front_layer_filter_a_failed_effect_read_is_no_filter(self):
        self.assertScenarios("front_layer_filter/a_failed_effect_read_is_no_filter")

    # ---- the gold row on an item with no stats (ForgedTooltipRows.hpp) --------

    def test_tooltip_rows_baseline_an_item_with_stats(self):
        self.assertScenarios("tooltip_rows/baseline_an_item_with_stats_anchors_at_its_first_present_row_call")

    def test_tooltip_rows_target_an_item_with_no_stats(self):
        self.assertScenarios("tooltip_rows/target_an_item_with_no_stats_anchors_at_the_first_row_call")

    def test_tooltip_rows_formats_once_a_pass_and_no_rows(self):
        self.assertScenarios("tooltip_rows/formats_other_than_2_and_3_never_anchor",
                             "tooltip_rows/a_pass_anchors_at_most_once",
                             "tooltip_rows/with_no_rows_nothing_anchors")

    # ---- drops: the stand-ins and the coin ----------------------------------

    def test_stand_in_table_and_even_count_rule(self):
        self.assertScenarios("stand_in/the_table_and_its_order",
                             "stand_in/the_first_even_candidate_that_passes_the_filter",
                             "stand_in/counting_and_the_unique_filter")

    def test_stand_in_refusal(self):
        self.assertScenarios("stand_in/no_candidate_refuses_and_names_the_counts")

    def test_stand_in_coin(self):
        self.assertScenarios("stand_in/the_coin_c_over_n_plus_c")

    # ---- the install and the command ----------------------------------------

    def test_install_states(self):
        self.assertScenarios("install/armed_by_a_charm_installed_once_a_character_exists",
                             "install/a_refusal_is_final_for_the_session",
                             "install/looks_for_the_player_every_60_frames")

    def test_command_forms(self):
        self.assertScenarios("command/accepted_forms", "command/refused_forms_name_the_usage")

    # ---- the stat line ------------------------------------------------------

    def test_stat_line_fresh_session_byte_for_byte(self):
        self.assertScenarios("stat_line/the_fresh_session_line_byte_for_byte")

    def test_stat_line_fields_after_use(self):
        self.assertScenarios("stat_line/fields_after_use_and_their_names")


if __name__ == "__main__":
    unittest.main()
