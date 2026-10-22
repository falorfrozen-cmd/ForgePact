"""Run the Mark 1 set's real core against controlled worn reads.

Mark 1 (ForgePact #166 phase 2) is a set of four built-in Satanic pieces:
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
transitions; the 30-frame refresh period; the dressing, and its tier and
rarity (`tier_baseline`/`rarity_baseline`: an item that is not a piece keeps
both, a stand-in's build its own; `tier_target`/`rarity_target`: every piece
comes out SS 5 and Satanic 6, pinned against hs-game-sdk's curated
item_info.json; the owner, 2026-10-20); the stat line, fresh
(`mark1set: bonus=off` first) and filled, and the command's forms.
`failure_counts`: the automation's four per-column failures, the gate's
entries and throws, and the jump's failed reads and per-reason rejections
each reach the line under their own name (a bar column that could not be read
never lands in `skip-no-state=`). `art_rule`: the pieces' art is armed by the
session's first piece dressed and stays armed; a dressing writes our icon only
when armed, with both copies loaded and a ground detour not known to be
table-only; the ground consumer is wanted only when armed, in game.
`drops_arm`: `mark1set drops 1|0` parses, and a drops switch-on arms the same
art flag (one flag, a piece or the drops) while the first stat line and the
bonus reply stay byte for byte.

The owner's playtest (2026-10-21): pieces placed before `mark1set bonus 1`
kept their base art. `art_baseline`: a fresh core is not armed, and
`mark1set bonus 1` alone no longer arms it. `art_target`: a piece's dressing
and the drops' switch arm it, both sticky, and armed the ground install is
pending. `tooltip_baseline`: what is not a piece gets no block and keeps its
type line, and the core's colours are the game's own set display and skill
grant (hs-game-sdk's curated item_info.json, `tooltip.set_section` and
`tooltip.skill_grant`). `tooltip_target`: the set block's rows exactly, grey
and green by what is worn, its bonus line lit only with all four worn, the
switch on and the bonus installed, nothing green on a failed read, the type
line `Satanic Set Gloves`, and the stat line's `tip=0 tip-unread=0
tip-armed=0 typeline=0 typeline-miss=0 tip-cached=0 typeline-hook=not-tried`
(round 0's instrument-blindness findings and Live 1: the on-screen call runs
on every frame a tooltip shows, so each call armed for a piece is drawn, and
then swapped its type line or not, or cached). The type line's install waits
for the art to be armed, never for embedded art (`art_baseline`/`art_target`).
"""
import json
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
        self.assertScenarios("dressing/display_names", "dressing/satanic_rarity_and_mechanic",
                             "dressing/the_set_name_reads_mark_1")
        # No affix rows: the set block says what the set does.
        header = (ROOT / "plugin/include/ForgePact/Mark1Set.hpp").read_text(encoding="utf-8")
        self.assertNotIn("kAffixRows", header)

    # ---- the dressing's tier and rarity (owner, 2026-10-20) ---------------------------

    def curated(self):
        return json.loads((HUB / "hs-game-sdk/curated/item_info.json").read_text(encoding="utf-8"))

    def dressing_constants(self):
        for line in self.output.split("\n"):
            if line.startswith("DRESSING "):
                return {k: int(v) for k, v in (p.split("=") for p in line.split()[1:])}
        raise AssertionError(f"no DRESSING line in harness output:\n{self.output}")

    def test_tier_baseline_an_item_that_is_not_a_piece_keeps_its_tier(self):
        self.assertScenario("tier_baseline/an_item_that_is_not_a_piece_keeps_its_tier")

    def test_tier_baseline_a_pieces_info_before_dressing_holds_the_bases_value(self):
        self.assertScenario("tier_baseline/a_pieces_info_before_dressing_holds_the_bases_value")

    def test_tier_target_every_piece_comes_out_at_tier_5(self):
        self.assertScenario("tier_target/every_piece_comes_out_at_tier_5")

    def test_tier_target_the_cores_tier_is_the_sdks_ss_code(self):
        ss = [t["code"] for t in self.curated()["tiers"] if t["letter"] == "SS"]
        self.assertEqual(len(ss), 1)
        self.assertEqual(self.dressing_constants()["tier"], ss[0])
        self.assertEqual(ss[0], 5)

    def test_rarity_baseline_an_item_that_is_not_a_piece_keeps_its_rarity(self):
        self.assertScenario("rarity_baseline/an_item_that_is_not_a_piece_keeps_its_rarity")

    def test_rarity_baseline_a_stand_ins_build_before_the_rewrite_keeps_its_own_rarity(self):
        self.assertScenario("rarity_baseline/a_stand_ins_build_before_the_rewrite_keeps_its_own_rarity")

    def test_rarity_target_every_piece_comes_out_at_rarity_6(self):
        self.assertScenario("rarity_target/every_piece_comes_out_at_rarity_6")

    def test_rarity_target_the_cores_rarity_is_the_sdks_satanic_code_and_no_other(self):
        codes = {r["name"]: r["code"] for r in self.curated()["rarities"]}
        rarity = self.dressing_constants()["rarity"]
        self.assertEqual(rarity, codes["Satanic"])
        self.assertNotEqual(rarity, codes["Heroic"])
        self.assertEqual([name for name, code in codes.items() if code == rarity], ["Satanic"])

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

    # ---- turn_fields: the turn rule's counters (Live procedure 1 reads them) ----

    def test_stat_line_turn_fields_under_their_own_names_after_threw(self):
        self.assertScenarios("turn_fields/each_under_its_own_name", "turn_fields/after_threw_in_order",
                             "turn_fields/no_turn_count_folded_into_an_old_field", "turn_fields/fresh_zero")

    # ---- art_rule: the pieces' art follows the switch ------------------------

    def test_art_rule_baseline_unarmed_session_writes_and_installs_nothing(self):
        self.assertScenarios("art_rule/baseline_fresh_session_not_armed_writes_and_installs_nothing",
                             "art_rule/baseline_fresh_stat_reads_art_off")

    def test_art_rule_the_first_piece_arms_for_the_session(self):
        self.assertScenarios("art_rule/a_piece_arms_and_the_switch_does_not_disarm",
                             "art_rule/armed_stat_reads_art_armed")

    def test_art_rule_a_write_needs_both_copies_and_the_route(self):
        self.assertScenarios("art_rule/a_write_needs_both_copies_and_no_known_other_route", "art_rule/route_names")

    def test_art_rule_ground_install_only_armed_and_in_game(self):
        self.assertScenarios("art_rule/ground_install_wanted_only_when_armed_embedded_untried_and_in_game",
                             "art_rule/pending_is_the_tick_gate_before_any_game_call")

    def test_command_forms(self):
        self.assertScenarios("parse/accepted_forms", "parse/refused_forms_print_the_usage",
                             "parse/usage_names_every_form", "parse/bonus_reply_names_the_switch_and_the_install")

    # ---- drops_arm: the drops switch arms the same art flag (phase 3) ----------

    def test_drops_arm_the_drops_form_parses_and_is_in_the_usage(self):
        self.assertScenarios("drops_arm/parse_drops_form")

    def test_drops_arm_a_drops_switch_on_arms_the_art_and_leaves_the_set_off(self):
        self.assertScenarios("drops_arm/drops_on_arms_the_art_and_leaves_the_set_off",
                             "drops_arm/baseline_drops_off_arms_nothing",
                             "drops_arm/one_flag_a_piece_or_the_drops")

    def test_drops_arm_the_first_line_and_the_bonus_reply_stay_byte_for_byte(self):
        self.assertScenarios("drops_arm/first_line_and_bonus_reply_unchanged", "stat/fresh_line_exact")

    # ---- the art armed by a piece (owner playtest, 2026-10-21) ------------------

    def test_art_baseline_a_fresh_core_is_unarmed_and_bonus_1_alone_leaves_it_so(self):
        self.assertScenarios("art_baseline/a_fresh_core_is_not_armed_and_nothing_is_pending",
                             "art_baseline/bonus_1_alone_leaves_it_unarmed")

    def test_art_baseline_the_switch_writes_no_art_flag(self):
        header = (ROOT / "plugin/include/ForgePact/Mark1Set.hpp").read_text(encoding="utf-8")
        body = header[header.index("void SetEnabled(bool on)"):]
        body = body[:body.index("\n    }\n")]
        self.assertNotIn("artArmed_", body)

    def test_art_target_a_piece_and_the_drops_switch_arm_it_sticky(self):
        self.assertScenarios("art_target/a_piece_dressed_arms_and_stays_armed",
                             "art_target/a_piece_leaves_the_switch_and_install_alone",
                             "art_target/the_drops_switch_arms_and_stays_armed",
                             "art_target/armed_the_ground_install_is_pending",
                             "art_target/the_line_reads_art_armed_with_the_switch_off")

    # ---- the type line's install, apart from the icons (round 0's findings) -----

    def test_type_line_baseline_unarmed_nothing_is_pending(self):
        self.assertScenario("art_baseline/unarmed_the_type_line_install_is_not_pending")

    def test_type_line_target_armed_it_waits_for_no_embedded_art(self):
        self.assertScenario("art_target/the_type_line_install_waits_for_no_embedded_art")

    # ---- the set block in a piece's tooltip (owner playtest, 2026-10-21) --------

    def colours(self):
        for line in self.output.split("\n"):
            if line.startswith("COLOURS "):
                return {k: int(v) for k, v in (p.split("=") for p in line.split()[1:])}
        raise AssertionError(f"no COLOURS line in harness output:\n{self.output}")

    def test_tooltip_baseline_what_is_not_a_piece_gets_no_block(self):
        self.assertScenarios("tooltip_baseline/not_a_piece_gets_no_block",
                             "tooltip_baseline/not_a_piece_keeps_its_type_line",
                             "tooltip_baseline/a_piece_keeps_every_other_text")

    def test_tooltip_baseline_the_cores_colours_are_the_games_set_display(self):
        # The game's own display is the reference: the curated file's measured
        # set section and skill grant, never a value of our own.
        tooltip = self.curated()["tooltip"]
        section, grant = tooltip["set_section"], tooltip["skill_grant"]
        self.assertEqual(section["status"], "measured")
        self.assertEqual(grant["status"], "measured")
        core = self.colours()
        for name in ("bonus_active", "bonus_inactive", "set_name", "piece_worn", "piece_not_worn"):
            with self.subTest(colour=name):
                self.assertEqual(core[name], section["colours"][name])
        self.assertEqual(core["description"], grant["colours"]["description"])

    def test_tooltip_target_the_block_by_what_is_worn_the_switch_and_the_read(self):
        self.assertScenarios("tooltip_target/none_worn_four_grey_names_and_a_grey_bonus",
                             "tooltip_target/two_worn_those_two_green",
                             "tooltip_target/four_worn_switch_off_all_green_and_a_grey_bonus",
                             "tooltip_target/four_worn_switch_on_a_gold_bonus",
                             "tooltip_target/a_failed_read_gives_nothing_green",
                             "tooltip_target/an_unreadable_slot_leaves_that_piece_grey_and_counts_unread")

    def test_tooltip_target_the_type_line_reads_satanic_set_gloves(self):
        self.assertScenario("tooltip_target/a_pieces_type_line_reads_satanic_set_gloves")
        self.assertIn("Satanic Set Gloves", self.line("tooltip_target/a_pieces_type_line_reads_satanic_set_gloves"))

    def test_tooltip_target_the_fresh_line_carries_tip_and_tip_unread(self):
        self.assertScenarios("tooltip_target/the_line_carries_tip_and_tip_unread", "stat/fresh_line_exact")

    def test_tooltip_target_the_line_counts_armed_passes_and_the_type_line(self):
        # Round 0's instrument-blindness findings: `tip-armed=` counts the
        # on-screen passes armed for a piece, `typeline=` those whose type line
        # was swapped and `typeline-miss=` those that ended with none.
        self.assertScenarios("tooltip_target/the_line_carries_tip_armed_typeline_and_typeline_miss",
                             "stat/fresh_line_exact", "failure_counts/every_field_once_in_order")
        self.assertIn(" tip=0 tip-unread=0 tip-armed=0 typeline=0 typeline-miss=0 ",
                      self.line("stat/fresh_line_exact"))

    def test_tooltip_target_counts_drawn_and_cached_calls(self):
        # Live 1 (replan 1): the on-screen call runs on every frame a tooltip
        # shows and the game draws the text on a hover's first frame only, so
        # each call is classified: drawn (a stat call or a type-line swap
        # during it) raises `tip-armed=` and one of `typeline=` /
        # `typeline-miss=`, nothing drawn raises `tip-cached=`, and a call not
        # armed for a piece raises nothing. `typeline-hook=` separates "the
        # hook is not in" from "it ran and matched nothing".
        self.assertScenarios("tooltip_target/a_drawn_call_that_swapped_raises_tip_armed_and_typeline",
                             "tooltip_target/a_drawn_call_with_no_swap_raises_tip_armed_and_typeline_miss",
                             "tooltip_target/a_swap_with_no_stat_call_is_a_drawn_call",
                             "tooltip_target/a_call_that_drew_nothing_is_cached",
                             "tooltip_target/a_call_not_armed_for_a_piece_raises_nothing",
                             "tooltip_target/a_hover_and_its_rest_count_one_drawn_and_the_rest_cached",
                             "tooltip_target/typeline_hook_names_not_tried_in_and_failed",
                             "tooltip_target/the_line_carries_tip_cached_and_typeline_hook",
                             "stat/fresh_line_exact", "failure_counts/every_field_once_in_order")
        # The fresh line: the two new fields appended after `typeline-miss=`.
        self.assertIn(" tip=0 tip-unread=0 tip-armed=0 typeline=0 typeline-miss=0 tip-cached=0 typeline-hook=not-tried | speed=",
                      self.line("stat/fresh_line_exact"))
        self.assertIn(" typeline-miss=0 tip-cached=0 typeline-hook=not-tried | speed=", self.line("stat/fresh_line_exact"))

    # ---- the 2026-10-22 playtest: the pieces' stats, the set bonus, the second line ----

    def test_tooltip_target_bonus_lines_by_pieces_worn(self):
        # Each bonus line lit while at least its threshold of distinct pieces
        # is worn (2: Defense; 3: All Resistances, Replenish Life; 4: the rest),
        # the switch on and the bonus installed: 0, 2, 3 and 4 worn, the switch
        # off, not installed, and a piece worn twice counting once.
        self.assertScenarios("tooltip_target/bonus_lines_lit_by_the_pieces_worn",
                             "tooltip_target/none_worn_four_grey_names_and_a_grey_bonus",
                             "tooltip_target/two_worn_those_two_green",
                             "tooltip_target/four_worn_switch_on_a_gold_bonus",
                             "tooltip_target/four_worn_switch_off_all_green_and_a_grey_bonus",
                             "tooltip_target/the_bonus_line_needs_the_switch_and_the_install")

    def test_tooltip_target_movement_line_is_its_own_row(self):
        # The owner's finding 2: `-50% Movement Speed` is a stat line of its
        # own, not part of Automation's description.
        self.assertScenario("tooltip_target/movement_line_is_its_own_row")
        header = (ROOT / "plugin/include/ForgePact/Mark1Set.hpp").read_text(encoding="utf-8")
        description = re.search(r"kAutomationDescription =\s*((?:\"[^\"]*\"\s*)+);", header)
        self.assertIsNotNone(description)
        self.assertNotIn("half speed", description.group(1))

    def test_tooltip_target_block_anchors_after_the_pieces_own_lines(self):
        # A piece now has stats of its own, so its block anchors after the last
        # of its own lines, never at its first present stat (between the
        # `Defense: N` header and the stats).
        self.assertScenarios("tooltip_target/block_anchors_after_the_pieces_own_lines",
                             "tooltip_target/block_anchors_only_at_a_row_call")

    def test_seed_codec_band_seeds_are_pieces(self):
        self.assertScenarios("seed_codec/band_and_legacy_seeds_are_pieces",
                             "seed_codec/another_bands_seed_or_a_natural_seed_is_no_piece")

    def test_stat_line_target_second_line_fresh(self):
        expected = ("mark1set bonus: pieces=0 lit=0/0/0 answered=0/0/0/0/0/0 not-number=0 gear-reads=0 gear-unread=0"
                    " | auto talents=-/-/-/- free=-/-/-/- masked=0 held-seen=0 hold=4 yield=61 rest=24"
                    " mask=0,1,2,7,8,9,10,11,27,40 route=proc procs=0 proc-fail=0 locked=0 cost=drain drain=1.00 drained=0"
                    " cast-base=0.86 fcr=- haste=- stat-fail=0 | tip desc-font=not-drawn grey=0 grey-box=0"
                    " | rolls dressed=0 legacy=0 drop-rolls=0")
        # The criterion's prefix, as one string:
        # mark1set bonus: pieces=0 lit=0/0/0 answered=0/0/0/0/0/0 not-number=0 gear-reads=0 gear-unread=0 | auto talents=-/-/-/- free=-/-/-/- masked=0 held-seen=0
        self.assertScenarios("stat/second_line_fresh_exact", "stat/second_line_filled")
        self.assertEqual(self.line("stat/second_line_fresh_exact")[len("PASS stat/second_line_fresh_exact "):], expected)

    def test_tooltip_target_bonus_line_needs_the_install(self):
        # Round 1's NON-BLOCKING finding: the bonus line is lit only while the
        # bonus is installed too. On and not installed grey, on and installed
        # gold, off and installed grey.
        self.assertScenarios("tooltip_target/the_bonus_line_needs_the_switch_and_the_install",
                             "tooltip_target/four_worn_switch_on_a_gold_bonus",
                             "tooltip_target/four_worn_switch_off_all_green_and_a_grey_bonus")


if __name__ == "__main__":
    unittest.main()
