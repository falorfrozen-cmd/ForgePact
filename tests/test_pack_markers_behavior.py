"""Run the real ForgePact::PackMarkers class against a controlled game API.

The markers are map reveal's monster half since 1.4.5: one icon per spawner
that has not given birth, instead of creating the zone's monsters. These
scenarios pin what the class asks of the game (nothing while off or while the
map is loading, one enumeration per zone, a bounded rotating check per frame),
how births and destroyed spawners retire a marker, and that a marker is drawn
with the same placement formula the game uses for its own dots. Issue #181
adds the retirement policies (`timer`, `state`, `kind`), the birth memory
across zone revisits and the research build's census.
"""
import os
import re
import shutil
import subprocess
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class PackMarkersBehaviorTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        header = (ROOT / "plugin/include/ForgePact/PackMarkers.hpp").read_text(encoding="utf-8")
        klass = "\n".join(
            line for line in header.split("\n")
            if not line.strip().startswith("#pragma once")
            and '#include "Common.hpp"' not in line
        )
        out = ROOT / "build/pack-markers-behavior"
        out.mkdir(parents=True, exist_ok=True)
        code = (ROOT / "tests/pack_markers_harness.cpp").read_text(encoding="utf-8")
        code = code.replace("// PRODUCTION_PACKMARKERS", klass)
        cpp = out / "packmarkers.cpp"
        cpp.write_text(code, encoding="utf-8")
        cls.binary = out / ("packmarkers.exe" if os.name == "nt" else "packmarkers")
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
                f'cl /nologo /std:c++20 /EHsc /O2 /I "{ROOT / "plugin/include"}" "{cpp}" /Fe:"{cls.binary}" /Fo:"{out / "packmarkers.obj"}"\n'
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

    def test_a_rolling_density_copy_does_not_relist_the_zone(self):
        self.assertScenario("copy/no_relisting")
        self.assertScenario("copy/real_growth_still_lists")

    def test_nothing_is_asked_of_the_game_while_off_or_loading(self):
        self.assertScenario("off/no_calls")
        self.assertScenario("loading/no_enumeration")
        self.assertScenario("off/nothing_drawn")

    def test_one_enumeration_per_zone_and_a_bounded_rotating_check(self):
        self.assertScenario("ready/enumerated_all")
        self.assertScenario("ready/one_enumeration")
        self.assertScenario("steady/no_reenumeration")
        self.assertScenario("steady/bounded_checks_per_frame")

    def test_births_and_destroyed_spawners_retire_their_marker(self):
        self.assertScenario("birth/marker_dropped")
        self.assertScenario("birth/idempotent")
        self.assertScenario("rotation/spawned_dropped")
        self.assertScenario("rotation/destroyed_dropped")
        self.assertScenario("unarmed/dropped_after_give_up")
        self.assertScenario("growth/no_resurrection")

    def test_nearby_spawners_collapse_into_one_marker_with_a_count(self):
        self.assertScenario("cluster/nearby_spawners_collapse")
        self.assertScenario("cluster/one_marker_per_cluster")
        self.assertScenario("cluster/badge_per_collapsed_cluster")
        self.assertScenario("cluster/badge_off")

    def test_icons_load_once_and_fall_back_to_dots(self):
        self.assertScenario("icons/loaded_once_per_kind")
        self.assertScenario("icons/one_sprite_per_cluster")
        self.assertScenario("icons/not_reloaded_every_draw")
        self.assertScenario("icons/absolute_path_fallback")
        self.assertScenario("icons/failed_add_falls_back_to_dots")

    def test_markers_use_the_games_own_placement(self):
        self.assertScenario("draw/default_is_primitives")
        self.assertScenario("draw/one_call_per_marker")
        self.assertScenario("draw/placement_matches_game_formula")
        self.assertScenario("draw/kind_selects_icon")
        self.assertScenario("draw/bad_args_skip")

    def test_the_games_draw_state_is_put_back(self):
        # PR #67 review: the sprite path used to leave the badge colour and a
        # guessed alpha behind.
        self.assertScenario("draw/state_restored_on_sprite_path")
        self.assertScenario("draw/state_restored_on_primitive_path")
        # Second review: draw_get_font can answer with an asset reference.
        self.assertScenario("draw/font_restored_when_the_runner_answers_a_reference")

    # Issue #181 baseline: today's rules, pinned as they are.
    def test_baseline_a_timerless_special_spawner_is_given_up(self):
        # A spawner kind with no enemyCreatorTimer never arms, so the timer
        # policy drops its marker after kUnarmedGiveUpFrames, unborn.
        self.assertScenario("timerless/listed")
        self.assertScenario("timerless/kept_while_young")
        self.assertScenario("timerless/dropped_after_give_up")

    def test_baseline_an_attributed_create_retires_the_marker_at_once(self):
        self.assertScenario("attributed/retires_at_once")

    # Issue #181: the retirement policy, `timer` (today's rules) or `state`.
    def test_the_retirement_policy_defaults_to_timer_and_switches_at_runtime(self):
        self.assertScenario("retire/default_is_timer")
        self.assertScenario("retire/selectable_at_runtime")

    # Issue #181 target: under `state`, only a gone spawner or its own
    # enemyArray turning into an array retires a marker.
    def test_state_a_timerless_special_spawner_keeps_its_marker(self):
        self.assertScenario("state/timerless_kept_past_give_up")

    def test_state_a_born_pack_loses_its_marker_within_one_rotation(self):
        self.assertScenario("state/born_retired_within_one_rotation")

    def test_state_a_destroyed_spawner_loses_its_marker(self):
        self.assertScenario("state/destroyed_retired")

    def test_state_an_attributed_create_does_not_retire_an_unborn_marker(self):
        self.assertScenario("state/attributed_create_keeps_marker")
        self.assertScenario("state/attributed_then_born_retired")

    def test_state_an_absent_enemy_array_keeps_the_marker(self):
        self.assertScenario("state/absent_array_keeps_marker")

    def test_state_a_revisited_zones_born_spawners_get_no_marker(self):
        self.assertScenario("state/revisit_born_spawners_unmarked")

    def test_state_a_normal_spawner_behaves_the_same_way(self):
        # Control: the rule is not special-kind specific.
        self.assertScenario("state/normal_control")

    # Issue #181: the one-shot census over every creator of each kind.
    def test_census_tallies_the_timer_on_all_seven_kinds(self):
        self.assertScenario("census/creators_and_marked")
        self.assertScenario("census/timer_number")
        self.assertScenario("census/timer_undefined")
        self.assertScenario("census/timer_absent")
        self.assertScenario("census/timer_other")

    def test_census_tallies_the_enemy_array_on_all_seven_kinds(self):
        self.assertScenario("census/array_array")
        self.assertScenario("census/array_undefined")
        self.assertScenario("census/array_absent")
        self.assertScenario("census/array_other")

    def test_census_counts_lost_and_stale_markers_both_ways(self):
        self.assertScenario("census/lost_both_ways")
        self.assertScenario("census/stale_both_ways")

    def test_census_runs_only_when_asked(self):
        self.assertScenario("census/never_per_frame")

    # Issue #181: per-kind, per-zone retirement accounting.
    def test_accounting_each_rule_counts_under_its_own_reason_and_kind(self):
        self.assertScenario("accounting/listed")
        self.assertScenario("accounting/spawned")
        self.assertScenario("accounting/destroyed")
        self.assertScenario("accounting/timergone")
        self.assertScenario("accounting/givenup")
        self.assertScenario("accounting/stateborn")

    def test_accounting_records_ages_and_every_attributed_create(self):
        self.assertScenario("accounting/age_range")
        self.assertScenario("accounting/attributed_creates")
        self.assertScenario("accounting/top_creates")

    def test_accounting_resets_on_a_zone_change(self):
        self.assertScenario("accounting/reset_on_zone_change")

    def test_markers_held_now_are_counted_per_kind(self):
        self.assertScenario("stat/kinds_held_now")

    # Issue #181 replan 2: the `kind` policy. The fake runner holds the
    # creators' protected pack state as the runtime does: a `spawnPack` key
    # (or none) naming a store record that is unset or a number, read through
    # the getter by script name; a refused key reaching the getter fails.
    def test_the_fake_counts_a_refused_key_and_none_ever_reaches_the_getter(self):
        self.assertScenario("getter/fake_counts_a_refused_key")
        self.assertScenario("getter/no_refused_key_ever_reached_it")

    def test_kind_has_one_rule_per_kind_and_is_selectable(self):
        self.assertScenario("kind/one_rule_per_kind")
        self.assertScenario("kind/selectable_at_runtime")

    def test_kind_an_unborn_state_keeps_the_marker_past_the_give_up_window(self):
        self.assertScenario("kind/unborn_state_kept_past_give_up")

    def test_kind_state_2_retires_within_one_rotation(self):
        self.assertScenario("kind/state_2_retires_within_one_rotation")

    def test_kind_state_3_retires_within_one_rotation(self):
        self.assertScenario("kind/state_3_retires_within_one_rotation")

    def test_kind_a_spawner_born_when_listed_gets_no_marker(self):
        self.assertScenario("kind/born_when_listed_gets_no_marker")

    def test_kind_refused_keys_keep_the_marker_count_unread_and_never_reach_the_getter(self):
        self.assertScenario("kind/refused_keys_keep_the_marker_and_count_unread")
        self.assertScenario("kind/refused_keys_never_reach_the_getter")

    def test_kind_an_attributed_create_retires_nothing(self):
        self.assertScenario("kind/attributed_create_retires_nothing")

    def test_kind_normal_and_ambush_retire_on_enemy_array_as_under_state(self):
        self.assertScenario("kind/normal_and_ambush_retire_on_enemy_array")
        self.assertScenario("kind/absent_enemy_array_keeps_the_marker_and_counts_unread")
        self.assertScenario("state/absent_enemy_array_is_not_unread")

    def test_kind_a_destroyed_spawner_retires(self):
        self.assertScenario("kind/destroyed_retires")

    def test_kind_only_the_kind_policy_calls_the_getter_per_frame(self):
        self.assertScenario("kind/getter_only_under_kind")

    # The birth memory.
    def test_memory_a_remembered_id_gets_no_marker_after_a_zone_change(self):
        self.assertScenario("memory/remembered_id_gets_no_marker")

    def test_memory_a_new_id_at_a_remembered_position_gets_no_marker(self):
        self.assertScenario("memory/new_id_at_a_remembered_position_gets_no_marker")

    def test_memory_the_same_position_in_another_room_is_marked(self):
        # Negative control for the position key.
        self.assertScenario("memory/same_position_in_another_room_is_marked")

    def test_memory_an_unknown_room_matches_nothing_by_position(self):
        self.assertScenario("memory/unknown_room_matches_nothing_by_position")

    def test_memory_an_unborn_spawner_is_marked_again_on_return(self):
        # Positive control: the memory holds born packs only.
        self.assertScenario("memory/unborn_spawner_is_marked_again_on_return")

    def test_memory_sameid_counts_ids_from_an_earlier_visit_to_the_room(self):
        self.assertScenario("memory/sameid_counts_ids_from_an_earlier_visit")

    def test_memory_is_recorded_under_timer_and_state_but_applied_only_under_kind(self):
        self.assertScenario("memory/recorded_but_never_withholding_under_timer_and_state")
        self.assertScenario("memory/recorded_under_timer_and_state_applies_under_kind")

    def test_memory_stays_bounded(self):
        self.assertScenario("memory/stays_bounded")

    # `packgone` mode: miniboss, legion and champion keep their marker until
    # the pack is gone.
    def test_packgone_a_spawner_born_when_listed_gets_a_marker_held(self):
        self.assertScenario("packgone/born_when_listed_gets_a_marker_held")

    def test_packgone_with_no_member_recorded_the_marker_stays_unlinked(self):
        self.assertScenario("packgone/no_member_recorded_stays_unlinked")

    def test_packgone_the_state_turning_born_retires_nothing(self):
        self.assertScenario("packgone/state_turning_born_retires_nothing")

    def test_packgone_members_hold_the_marker_until_none_exists(self):
        self.assertScenario("packgone/members_hold_the_marker_until_none_exists")

    def test_packgone_members_recorded_before_a_zone_change_still_count(self):
        self.assertScenario("packgone/members_recorded_before_a_zone_change_still_count")

    def test_packgone_a_destroyed_spawner_retires_as_destroyed(self):
        self.assertScenario("packgone/destroyed_spawner_retires_as_destroyed")

    def test_packgone_legion_and_champion_behave_the_same(self):
        self.assertScenario("packgone/legion_and_champion_behave_the_same")

    def test_packgone_a_spawner_is_remembered_only_once_its_pack_is_gone(self):
        self.assertScenario("packgone/remembered_only_once_its_pack_is_gone")

    def test_baseline_members_never_hold_a_birth_mode_marker(self):
        self.assertScenario("packgone/birth_mode_ignores_living_members")

    def test_baseline_members_change_nothing_under_timer_and_state(self):
        self.assertScenario("packgone/members_change_nothing_under_timer_and_state")

    # The protected-state census and the second instrument's counts.
    def test_census_tallies_spawnpack_on_all_seven_kinds(self):
        self.assertScenario("census2/spawnpack_tally_on_all_seven_kinds")

    def test_census_counts_born_both_ways(self):
        self.assertScenario("census2/born_both_ways")

    def test_census_counts_attributed_unborn_both_ways(self):
        self.assertScenario("census2/attributed_unborn_both_ways")

    def test_census_reads_the_pack_state_only_when_asked(self):
        self.assertScenario("census2/pack_state_read_only_when_asked")

    def test_census_lists_members_both_ways(self):
        self.assertScenario("census2/members_both_ways")
        self.assertScenario("creator/members_by_object")

    def test_census_lists_one_kind_per_spawner(self):
        self.assertScenario("census2/per_spawner_listing")

    def test_non_enemy_creates_are_counted_per_kind(self):
        self.assertScenario("other/non_enemy_creates_counted_per_kind")

    def test_the_unread_count_lasts_the_session(self):
        self.assertScenario("stat/unread_survives_a_zone_change")

    def test_kind_names_match_the_icon_names(self):
        # `packmarks` lines name a kind by its icon; the two tables must agree.
        root = ROOT / "plugin/include/ForgePact"
        markers = (root / "PackMarkers.hpp").read_text(encoding="utf-8")
        icons = (root / "PackMarkerIcons.hpp").read_text(encoding="utf-8")
        start = markers.index("kKindNames[KindCount] = {")
        names = re.findall(r'"([a-z_]+)"', markers[start:markers.index("};", start)])
        self.assertEqual(names, re.findall(r'^\s*\{ "([a-z_]+)", k_', icons, re.M))
        self.assertEqual(len(names), 7)


if __name__ == "__main__":
    unittest.main()
