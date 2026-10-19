"""Run the Mark 1 set's real core against controlled worn reads.

Mark 1 (ForgePact #166 phase 2) is a set of four built-in Heroic pieces:
helmet, body armor, boots and gloves. While all four are worn, each in
whichever of gear slots 0-9 holds it, and the "Mark 1 set" switch is on, the
set bonus is active. These
scenarios pin the decisions ModuleMain.cpp's adapter takes from
plugin/include/ForgePact/Mark1Set.hpp, compiled whole.

Baseline (`baseline` in the name): a vanilla item, one piece worn twice in
place of another, three pieces, an unreadable slot, a failed read and the
switch off all leave the bonus inactive, and with the switch off nothing is
read. `seed_codec`: the table's classes (pinned against hs-game-sdk's
ItemType), seeds and bases; the seeds distinct, inside ForgePact's built-in
band and clear of every band in use; recognition ignores `b` and `c` and needs
`j` 0. Targets: the four pieces with the switch on make the bonus active
wherever among gear slots 0-9 each is worn (Mod live 2's `equip-detect`: the
gloves were not at slot 3), and the line's `worn=` names each one's slot; the
install state's
transitions; the 30-frame refresh period; the dressing; the stat line, fresh
(`mark1set: bonus=off` first) and filled, and the command's forms.
`failure_counts`: the automation's four per-column failures, the gate's
entries and throws, and the jump's failed reads and per-reason rejections
each reach the line under their own name (a bar column that could not be read
never lands in `skip-no-state=`). `art_rule`: the pieces' art is armed by the
session's first switch-on and stays armed; a dressing writes our icon only
when armed, with both copies loaded and a ground detour not known to be
table-only; the ground consumer is wanted only when armed, in game.
"""
import os
import re
import shutil
import subprocess
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HUB = ROOT.parent


class Mark1SetBehaviorTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        header = (ROOT / "plugin/include/ForgePact/Mark1Set.hpp").read_text(encoding="utf-8")
        core = "\n".join(line for line in header.split("\n") if not line.strip().startswith("#pragma once"))
        out = ROOT / "build/mark1-set-behavior"
        out.mkdir(parents=True, exist_ok=True)
        code = (ROOT / "tests/mark1_set_harness.cpp").read_text(encoding="utf-8")
        code = code.replace("// PRODUCTION_MARK1SET", core)
        cpp = out / "mark1set.cpp"
        cpp.write_text(code, encoding="utf-8")
        cls.binary = out / ("mark1set.exe" if os.name == "nt" else "mark1set")
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
                f'cl /nologo /std:c++20 /EHsc /O2 /W4 /WX /I "{ROOT / "plugin/include"}" "{cpp}" /Fe:"{cls.binary}" /Fo:"{out / "mark1set.obj"}"\n'
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

    def table(self):
        rows = {}
        for line in self.output.split("\n"):
            if line.startswith("TABLE "):
                parts = line.split()
                rows[parts[1]] = {k: int(v) for k, v in (p.split("=") for p in parts[2:])}
        return rows

    def test_all_scenarios_pass(self):
        self.assertIn("RESULT OK", self.output, self.output)
        self.assertNotIn("FAIL ", self.output, self.output)

    # ---- baseline: the bonus stays inactive -------------------------------------

    def test_baseline_a_vanilla_item_leaves_the_bonus_inactive(self):
        self.assertScenarios("baseline/a_vanilla_item_is_not_full", "baseline/a_vanilla_item_switch_on_inactive")

    def test_baseline_a_piece_worn_twice_in_place_of_another_leaves_the_bonus_inactive(self):
        self.assertScenarios("baseline/a_piece_twice_in_place_of_another_does_not_count",
                             "baseline/the_same_piece_in_every_slot_is_one",
                             "baseline/gear_without_a_or_j_is_read_and_not_a_piece",
                             "baseline/a_piece_twice_in_place_of_another_switch_on_inactive")

    def test_baseline_three_pieces_leave_the_bonus_inactive(self):
        self.assertScenarios("baseline/three_pieces_are_not_full", "baseline/three_pieces_switch_on_inactive",
                             "baseline/no_piece_reads_none")

    def test_baseline_an_unreadable_slot_leaves_the_bonus_inactive_and_is_counted(self):
        self.assertScenarios("baseline/an_unreadable_slot_is_not_full",
                             "baseline/an_unreadable_slot_switch_on_inactive_and_counted",
                             "baseline/a_failed_read_is_inactive_and_counted",
                             "baseline/no_read_callback_is_a_failed_read")

    def test_baseline_the_switch_off_reads_nothing_and_leaves_the_bonus_inactive(self):
        self.assertScenarios("baseline/switch_off_reads_nothing_and_is_inactive",
                             "baseline/switch_off_installed_still_inactive",
                             "baseline/switch_turned_off_ends_the_bonus_at_once")

    # ---- seed_codec: the table and recognition ---------------------------------

    def test_seed_codec_classes_slots_seeds_and_bases(self):
        self.assertScenarios("seed_codec/classes_in_piece_order_and_ten_gear_slots", "seed_codec/one_seed_per_piece",
                             "seed_codec/measured_bases", "seed_codec/keys_round_trip")

    def test_seed_codec_bases_are_the_research_docs_piece_bases(self):
        # Mod live 1's `base-check` (docs/mark1-set-research.md, `piece-bases:`
        # as `<class>:<b>` four times) is what each piece is built on.
        doc = (ROOT / "docs/mark1-set-research.md").read_text(encoding="utf-8")
        found = re.findall(r"^piece-bases: (\d+:\d+(?:,\d+:\d+){3})\s*$", doc, re.M)
        self.assertEqual(len(found), 1, found)
        measured = {int(c): int(b) for c, b in (pair.split(":") for pair in found[0].split(","))}
        table = {row["type"]: row["b"] for row in self.table().values()}
        self.assertEqual(table, measured, f"piece-bases: {found[0]}")

    def test_seed_codec_classes_match_the_sdk_item_types(self):
        sdk = (HUB / "hs-game-sdk/cpp/include/hs_game_sdk/item_type.hpp").read_text(encoding="utf-8")
        types = {name: int(value) for name, value in re.findall(r"^\s*(\w+) = (\d+),", sdk, re.M)}
        rows = self.table()
        self.assertEqual(sorted(rows), ["body", "boots", "gloves", "helmet"])
        for key, name in (("helmet", "Helmet"), ("body", "Body"), ("boots", "Boots"), ("gloves", "Gloves")):
            self.assertEqual(rows[key]["type"], types[name], key)
        # No worn slot per piece: RDM's "Equipment Slots Layout" rows 1-8 are
        # unmeasured, and Mod live 2 found the gloves elsewhere than slot 3.
        self.assertTrue(all("slot" not in row for row in rows.values()), rows)

    def test_seed_codec_seeds_are_distinct_in_the_band_and_clear_of_bands_in_use(self):
        self.assertScenarios("seed_codec/seeds_distinct_inside_the_built_in_band",
                             "seed_codec/seeds_collide_with_no_band_in_use")
        seeds = [row["seed"] for row in self.table().values()]
        self.assertEqual(len(set(seeds)), 4)
        self.assertTrue(all(777000 <= s <= 777999 for s in seeds), seeds)

    def test_seed_codec_header_asserts_its_seeds_at_compile_time(self):
        header = (ROOT / "plugin/include/ForgePact/Mark1Set.hpp").read_text(encoding="utf-8")
        self.assertRegex(header, r"static_assert\(SeedsDistinctInBand\(\)")

    def test_seed_codec_recognition(self):
        self.assertScenarios("seed_codec/each_piece_recognised", "seed_codec/b_and_c_do_not_matter",
                             "seed_codec/j_must_be_zero", "seed_codec/other_classes_seeds_and_reals_refused")

    def test_seed_codec_header_holds_no_runtime_value(self):
        # Numbers and strings only: the FlightCharms.hpp contract.
        header = (ROOT / "plugin/include/ForgePact/Mark1Set.hpp").read_text(encoding="utf-8")
        code = "\n".join(line.split("//")[0] for line in header.split("\n"))
        for name in ("RValue", "CInstance", "YYTK", "Aurie", "g_Yytk"):
            self.assertNotIn(name, code, name)
        self.assertNotRegex(code, r"#include\s*[<\"](YYToolkit|Aurie)")

    # ---- targets ---------------------------------------------------------------

    def test_four_matching_pieces_with_the_switch_on_make_the_bonus_active(self):
        self.assertScenarios("active/four_matching_pieces_are_full", "active/four_pieces_switch_on_is_active")

    def test_the_four_pieces_count_wherever_among_gear_slots_0_to_9_each_is_worn(self):
        # Mod live 2's `equip-detect`: the worn Mark 1 Gloves were not at slot 3.
        self.assertScenarios("active/gloves_in_another_gear_slot_are_full",
                             "active/gloves_in_another_gear_slot_switch_on_is_active",
                             "active/pieces_in_any_gear_slots_are_full",
                             "active/a_piece_twice_counts_once_at_its_first_slot",
                             "active/an_unreadable_other_slot_beside_four_pieces_is_full_and_counted",
                             "active/worn_names_each_pieces_slot",
                             "stat/worn_names_the_slot_each_piece_was_found_in")

    def test_dressing(self):
        self.assertScenarios("dressing/display_names", "dressing/heroic_rarity_and_mechanic",
                             "dressing/affix_rows_first_reads_mark_1")

    def test_install_state_transitions(self):
        self.assertScenarios("install/fresh_is_not_armed", "install/switch_off_never_installs",
                             "install/switch_on_without_a_character_waits", "install/not_before_setup",
                             "install/tick_looks_every_60_frames", "install/tick_installs_once_a_character_exists",
                             "install/installed", "install/switch_off_after_install_keeps_the_hooks",
                             "install/refused_turns_the_switch_off",
                             "install/after_a_refusal_the_tick_does_not_retry_the_command_does",
                             "install/a_later_install_clears_the_refusal",
                             "install/command_with_a_character_installs_at_once")

    def test_refresh_period(self):
        self.assertScenarios("refresh/period_is_30_frames", "refresh/at_most_one_read_in_30_frames",
                             "refresh/reads_again_on_frame_30", "refresh/many_calls_one_frame_one_read",
                             "refresh/a_piece_taken_off_ends_it_at_the_next_read",
                             "refresh/a_frame_counter_that_went_back_reads_at_once",
                             "refresh/switch_on_again_reads_at_once", "refresh/stat_reads_now",
                             "refresh/stat_reads_with_the_switch_off_and_stays_inactive")

    def test_stat_line_fresh_text(self):
        self.assertScenarios("stat/fresh_line_begins_bonus_off", "stat/fresh_line_exact", "stat/no_double_space")

    def test_stat_line_named_fields(self):
        self.assertScenarios("stat/named_fields", "stat/filled_fields", "stat/partial_set", "stat/full_set",
                             "stat/failed_read", "stat/waiting_for_character", "stat/number_text")

    # ---- failure_counts: what `presses=0` or a vanilla jump came from ----------

    def test_stat_line_failure_counts_every_field_once_in_order(self):
        self.assertScenarios("failure_counts/every_field_once_in_order", "failure_counts/same_names_filled",
                             "failure_counts/new_fields_after_each_groups_own")

    def test_stat_line_failure_counts_each_under_its_own_name(self):
        self.assertScenarios("failure_counts/automation_fields_under_their_own_names",
                             "failure_counts/jump_fields_under_their_own_names",
                             "failure_counts/icons_fields_under_their_own_names",
                             "failure_counts/no_failure_folded_into_an_old_field")

    def test_stat_line_failure_counts_column_unreadable_is_not_skip_no_state(self):
        self.assertScenarios("failure_counts/column_unreadable_never_in_skip_no_state")

    # ---- art_rule: the pieces' art follows the switch ------------------------

    def test_art_rule_baseline_unarmed_session_writes_and_installs_nothing(self):
        self.assertScenarios("art_rule/baseline_fresh_session_not_armed_writes_and_installs_nothing",
                             "art_rule/baseline_fresh_stat_reads_art_off")

    def test_art_rule_first_switch_on_arms_for_the_session(self):
        self.assertScenarios("art_rule/switch_on_arms_and_off_does_not_disarm",
                             "art_rule/armed_stat_reads_art_armed")

    def test_art_rule_a_write_needs_both_copies_and_the_route(self):
        self.assertScenarios("art_rule/a_write_needs_both_copies_and_no_known_other_route", "art_rule/route_names")

    def test_art_rule_ground_install_only_armed_and_in_game(self):
        self.assertScenarios("art_rule/ground_install_wanted_only_when_armed_embedded_untried_and_in_game",
                             "art_rule/pending_is_the_tick_gate_before_any_game_call")

    def test_command_forms(self):
        self.assertScenarios("parse/accepted_forms", "parse/refused_forms_print_the_usage",
                             "parse/usage_names_every_form", "parse/bonus_reply_names_the_switch_and_the_install")


if __name__ == "__main__":
    unittest.main()
