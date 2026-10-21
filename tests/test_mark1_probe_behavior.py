"""Run mark1probe's real core against controlled calls.

`mark1probe` (research build only, ForgePact #166 phase 1) wraps nine game
scripts, from the cast gate to the jump, and holds one lever at a time on
them, so the live sessions can find which value automates a cast, which one
carries the movement penalty and the speed, and where the jump's distance
lives. These scenarios pin the decisions ModuleMain.cpp's adapter takes from
plugin/include/ForgePact/Mark1Probe.hpp, compiled whole.

Baseline (`baseline` in the name): with no lever on, every row's call, for the
local player and for any other self, runs the original once, returns its
result unchanged and reads or writes nothing; a lever on one row leaves the
others alone. Targets (`lever`): `set` and `scale` write before the original
and restore after it, also when it throws, only for the local player, once
per nested call and only within their frame budget; `move` halves and triples
a call's own displacement and leaves a call that did not move alone; `stat`
changes only a number returned for the matching id and the local player; a
second lever replaces the first. `walkable`: the verdict line for each
field's values, each field's raw builtin answer printed beside it (and what a
blocked family met), place_meeting's answer read as a meeting (false or 0
free, true or 1 blocked) apart from collision_line's instance-or-`noone`, an
unrecognised kind left unreadable, and `scan`'s point list. `parse`: every form, accepted and
refused. The status, `hook`, `show`, `args`, `watch`, `count` and `pos` lines.

Phase 2's walk research (the Mark 1 set's Live procedure 1) adds, each with its
baseline (off, every row passes its call through unchanged): the builtin rows
(`builtin`: a pass-through that counts, `move` on one, `pos` naming one, and a
name the runtime lacks listed `not-found`, never failed); `statlog` (one line
per local answer for the id with the rows around it, the getter's direct calls
apart, an end line with min and max, a line cap, nothing written); `statpin`
(the last local answer before it, on both routes, once per call, the reply
naming it); `hold` (written before each outer local call, restored once when
it ends, is replaced or is turned off, nothing written without a start value);
the `effect`, `pst` array and `builtin` watch scopes, whose names parse back
as targets; `pos`'s `mode=` and `ms=` from the frame samples alone, with no
row hooked; `base`'s key, definition and line; and every lever reply's
`from f=`.

The Mark 1 set's adapter adds `spawn`: the pieces one word places, each
piece's definition from Mark1Set.hpp's own row, the form's parse and refusals,
and its line (placed, refused at the ground, not built), with the tier
it read after `dressed=`.

The drops (phase 3) add `heroic <n>`, `pool [<k>]` and `copies <k>` for their
live session: `heroic`'s per-call line (the tier after `rarity=`) and summary, `pool`'s summary of one
sub-list as the Heroic roll keeps it with each piece's stand-in by
Mark1Drops.hpp's rule, `copies`' line, and each form's parse and refusals.
The kill route adds `unique <n>` (the same lines under its own name) and `pool
kill` (the five lengths and each piece's kill stand-in with its counts).

Mod live procedure 6 adds `killgate [0|1]`, a lever that opens LoadDrops' type-45
gate once per kill inside the game's own call. Baseline (`test_killgate_baseline_*`):
the lever off passes every call through with the chances array untouched and
nothing re-entered, only the counters moving, and a hook other than the inline
detour refuses it. Target (`test_killgate_target_*`): the game's own type-45 call
raised and put back, the first call on a fresh array re-entered once at type 45
and put back, later calls untouched, a throw putting the chance back and counted,
and the stat line's exact text.

Mod live procedure 7 adds `draws [on|off]`, a watch on DropItem's own unique draw,
`draws push <c>|off`, its lever, and `pool draw`. Baseline (`test_draws_baseline_*`):
the watch off counts nothing and takes no DropItem call, the lever pushes nothing, and
a DropItem route other than the inline detour refuses the watch. Target
(`test_draws_target_*`): each scope classes a build by the innermost one open; k,
on-k and off-k follow the recorded tier; the lever pushes c onto each of [0..5] and
takes exactly that off; the picks and pick-builds; the status and built lines exact.
`test_pool_draw_*`: the candidate rule and the line. Live 1b adds the lever's push
counters and its side (`draws push <c> [ss|below]|off`): the baselines that the
counters stay 0 with the push off and that no side picks as built, SS first; the
targets that push-held= counts once per outermost call, push-skipped= once per silent
path, pushed-reads= only draw-scope reads of a pushed triple, and that `below` picks
only below-SS candidates and refuses a class without one.
"""
import os
import shutil
import subprocess
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class Mark1ProbeBehaviorTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        header = (ROOT / "plugin/include/ForgePact/Mark1Probe.hpp").read_text(encoding="utf-8")
        core = "\n".join(line for line in header.split("\n") if not line.strip().startswith("#pragma once"))
        out = ROOT / "build/mark1-probe-behavior"
        out.mkdir(parents=True, exist_ok=True)
        code = (ROOT / "tests/mark1_probe_harness.cpp").read_text(encoding="utf-8")
        code = code.replace("// PRODUCTION_MARK1PROBE", core)
        cpp = out / "mark1probe.cpp"
        cpp.write_text(code, encoding="utf-8")
        cls.binary = out / ("mark1probe.exe" if os.name == "nt" else "mark1probe")
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
                f'cl /nologo /std:c++20 /EHsc /O2 /W4 /WX /I "{ROOT / "plugin/include"}" "{cpp}" /Fe:"{cls.binary}" /Fo:"{out / "mark1probe.obj"}"\n'
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

    def test_all_scenarios_pass(self):
        self.assertIn("RESULT OK", self.output, self.output)
        self.assertNotIn("FAIL ", self.output, self.output)

    def test_the_script_and_builtin_rows(self):
        self.assertScenario("rows/names_round_trip")

    # ---- baseline: no lever ----------------------------------------------------

    def test_baseline_every_row_calls_the_original_and_writes_nothing(self):
        self.assertScenarios("baseline/every_row_calls_the_original_once_and_writes_nothing",
                             "baseline/calls_are_counted_local_and_all", "baseline/nothing_applied")

    def test_baseline_a_throwing_original_propagates_untouched(self):
        self.assertScenarios("baseline/a_throwing_original_propagates_untouched", "baseline/depth_recovers_after_a_throw")

    def test_baseline_a_lever_leaves_other_rows_alone(self):
        self.assertScenario("baseline/a_lever_leaves_other_rows_alone")

    # ---- the levers ------------------------------------------------------------

    def test_lever_set_writes_before_and_restores_after(self):
        self.assertScenarios("lever/set_writes_before_the_original_and_restores_after", "lever/set_a_bool_and_restore_it",
                             "lever/set_restores_when_the_original_throws")

    def test_lever_set_only_for_the_local_player_and_within_its_budget(self):
        self.assertScenarios("lever/set_only_for_the_local_player", "lever/set_only_within_its_frame_budget",
                             "lever/the_budget_ends_with_a_line", "lever/set_an_unreadable_member_writes_nothing",
                             "lever/a_nested_call_of_the_row_applies_once")

    def test_lever_scale_multiplies_and_restores(self):
        self.assertScenarios("lever/scale_multiplies_before_and_restores_after", "lever/scale_restores_when_the_original_throws",
                             "lever/scale_only_for_the_local_player", "lever/scale_leaves_a_bool_alone")

    def test_lever_move_scales_the_calls_own_displacement(self):
        self.assertScenarios("lever/move_halves_the_calls_displacement", "lever/move_triples_the_calls_displacement",
                             "lever/move_leaves_a_call_that_did_not_move_alone", "lever/move_only_for_the_local_player",
                             "lever/move_only_on_its_row")

    def test_lever_stat_changes_only_the_matching_id_for_the_local_player(self):
        self.assertScenarios("lever/stat_scales_the_matching_ids_number", "lever/stat_leaves_another_id_alone",
                             "lever/stat_only_for_the_local_player", "lever/stat_counts_a_non_number_as_skip",
                             "lever/stat_leaves_a_call_without_an_id_alone", "lever/stat_only_on_ReturnSpecificStat")

    def test_lever_a_second_replaces_the_first(self):
        self.assertScenarios("lever/a_second_lever_replaces_the_first", "lever/off_turns_every_lever_off",
                             "lever/text_has_no_spaces", "lever/reply_names_scope_value_and_restored_value")

    def test_lever_every_reply_names_its_start_frame(self):
        self.assertScenario("lever/every_reply_names_its_start_frame")

    # ---- phase 2: the walk research additions ------------------------------------

    def test_builtin_rows_are_rows(self):
        self.assertScenarios("builtin/a_builtin_row_passes_through_and_counts",
                             "builtin/move_scales_a_builtin_rows_displacement", "builtin/pos_names_a_builtin_mover",
                             "hook/a_builtin_not_found_is_listed_not_failed")

    def test_statlog_lines_and_baseline(self):
        self.assertScenarios("statlog/one_line_per_local_answer_with_the_rows_around_it",
                             "statlog/end_line_counts_min_and_max", "statlog/writes_nothing",
                             "statlog/no_line_before_after_or_once_off", "statlog/a_line_cap_counts_the_rest")

    def test_statpin_pins_the_last_answer(self):
        self.assertScenarios("statpin/answers_with_the_last_answer_before_it", "statpin/reply_names_the_pinned_value",
                             "statpin/ends_with_its_counts")

    def test_hold_restores_once(self):
        self.assertScenarios("hold/writes_before_each_call_and_restores_once_at_the_end",
                             "hold/a_value_once_per_outer_call_and_only_for_the_local_player",
                             "hold/a_new_lever_restores_it_first", "hold/off_ends_it_and_the_restore_follows",
                             "hold/an_unread_start_or_a_bool_multiple_writes_nothing")

    def test_watch_scopes_effect_pst_array_and_builtin(self):
        self.assertScenarios("watch/effect_pst_array_and_builtin_name_members_as_targets",
                             "watch/a_watched_name_is_a_hold_target")

    def test_pos_mode_and_ms_without_a_hook(self):
        self.assertScenarios("pos/samples_with_no_row_hooked_give_mode_and_ms",
                             "pos/ms_counts_every_frame_and_mode_a_vertical_step")

    def test_base_key_definition_and_line(self):
        self.assertScenarios("base/key_and_definition", "base/line_built_and_not_built")

    def test_spawn_pieces_definition_parse_and_line(self):
        # The Mark 1 set's adapter: `spawn` builds each piece from
        # Mark1Set.hpp's own row (seed, base, class) and prints one line per
        # piece - placed, refused at the ground, or not built.
        self.assertScenarios("spawn/pieces_by_word", "spawn/definition_is_the_rows_seed_and_base", "spawn/parse",
                             "spawn/line_placed_refused_and_not_built")

    # ---- the drops' forms (Mark1Drops.hpp's live session) -------------------------

    def test_heroic_call_lines_summary_and_parse(self):
        # `heroic <n>`: one line per call (placed with its class, base, c, rarity, tier, name and
        # whether it is a piece, or nothing placed and why), then the summary: rarities rising,
        # the unread ones apart, and the pieces by their own recognition, never by name.
        self.assertScenarios("heroic/parse", "heroic/call_lines_placed_and_nothing",
                             "heroic/summary_rarities_rising_and_pieces_by_recognition", "heroic/a_fresh_summary_reads_none")

    def test_heroic_and_spawn_lines_carry_the_tier(self):
        # Every Mark 1 piece is SS (the owner, 2026-10-20): `heroic`'s placed line carries
        # `tier=<info 32>` after `rarity=` (a piece reads rarity=6 tier=5, Satanic SS), and
        # `spawn`'s line after `dressed=`; `?` when the key could not be read, and a line that
        # built or placed nothing carries none.
        self.assertScenarios("heroic/call_line_tier_after_rarity_question_mark_when_unread",
                             "heroic/call_lines_placed_and_nothing",
                             "spawn/line_tier_after_dressed_question_mark_when_unread",
                             "spawn/line_placed_refused_and_not_built")

    def test_pool_summary_standins_and_parse(self):
        # `pool [<k>]`: 5 by default; T_H (`droppable=`) counts the entries the Heroic roll keeps,
        # repeats included, by rarity, apart from those it draws past (rated 7 or 10, hidden,
        # unread, not a triple); each piece's stand-in is Mark1Drops.hpp's rule (Heroic preferred,
        # none for a class with no kept triple), and each distinct triple is asked about once.
        self.assertScenarios("pool/parse_default_five_and_a_named_list", "pool/summary_droppable_histograms_and_what_is_drawn_past",
                             "pool/each_piece_standin_by_the_drops_rule", "pool/each_distinct_triple_asked_once")

    def test_copies_line(self):
        self.assertScenario("copies/line")

    def test_unique_call_lines_and_summary_for_the_kill_route(self):
        # `unique <n>` (the kill route, the ordinary unique roll by name): the same per-call line
        # and summary as `heroic`'s, under its own name, with the tier after the rarity.
        self.assertScenarios("unique/parse_kill", "unique/call_lines_for_the_kill_route",
                             "unique/summary_under_its_own_name_for_the_kill_route")

    def test_pool_kill_lengths_standins_and_counts(self):
        # `pool kill`: [0..4]'s lengths, then per piece class the kill stand-in by Mark1Drops.hpp's
        # equal-count rule (n 0 first), that triple's count on each of [0..4], the candidates with
        # one count on all five and the class's kept triples; each distinct triple asked once.
        self.assertScenarios("pool/kill_parse", "pool/kill_lengths_standins_and_counts", "pool/kill_each_distinct_triple_asked_once")

    # ---- killgate (Mod live procedure 6) -------------------------------------------

    def test_killgate_baseline_lever_off_passes_every_call_through(self):
        # The lever off: every LoadDrops call is the game's own, once, with the chances array
        # untouched and nothing re-entered; only the counters move, type 45's vanilla chance
        # read once per array (an id in a later frame is a new array). On and then off again
        # is vanilla once more.
        self.assertScenarios("killgate/baseline_off_every_call_passes_through_untouched",
                             "killgate/baseline_off_only_the_counters_move",
                             "killgate/baseline_off_vanilla_45_read_once_per_array",
                             "killgate/baseline_lever_off_again_is_vanilla")

    def test_killgate_baseline_a_hook_not_inline_refuses(self):
        # A table-only, missing or refused LoadDrops hook refuses the lever with one line and
        # nothing changed; the inline detour (the negative control) is accepted.
        self.assertScenarios("killgate/baseline_a_hook_not_inline_refuses_nothing_changed",
                             "killgate/baseline_hook_route_from_the_install")

    def test_killgate_target_raise_and_reenter_once_per_array(self):
        # The lever on: the game's own type-45 call on a fresh array runs with chances[45]
        # opened and put back after; the first call of another type re-enters LoadDrops once at
        # type 45, opened and put back; later calls on that array, a game type-45 call
        # included, pass untouched; each new array is opened once.
        self.assertScenarios("killgate/target_game_45_call_raised_and_put_back",
                             "killgate/target_first_call_reenters_45_once_and_puts_back",
                             "killgate/target_later_calls_on_a_handled_array_pass_untouched",
                             "killgate/target_each_new_array_opened_once")

    def test_killgate_target_throw_nesting_and_unreadable(self):
        # A throw inside the opened call puts the chance back and counts threw= (the game's own
        # call's throw passed on, the re-entry's not); a call nested in an opened one, and an
        # array whose chance cannot be read or written, pass untouched.
        self.assertScenarios("killgate/target_a_throw_puts_the_chance_back_and_counts_threw",
                             "killgate/target_not_left_nested_after_a_throw",
                             "killgate/target_a_call_nested_in_an_opened_one_passes_untouched",
                             "killgate/target_unreadable_or_unwritable_chance_passes_through")

    def test_killgate_target_stat_line_and_parse(self):
        self.assertScenarios("killgate/stat_line", "killgate/off_reply", "killgate/parse")

    # ---- draws (Mod live procedure 7) ----------------------------------------------

    def test_draws_baseline_watch_off_counts_nothing_and_takes_no_call(self):
        # The watch off: no DropItem call is taken (the adapter then calls the game's own
        # DropItem as it is), no scope opens, no tier, read, pick, build or ReturnRandomSatanic
        # call is counted, and the status line is the fresh one, exactly.
        self.assertScenarios("draws/baseline_off_nothing_counted", "draws/baseline_off_status_line_exact")

    def test_draws_baseline_the_push_does_nothing_with_the_watch_off(self):
        # The lever set while the watch is off pushes nothing; a push with nothing planned or a
        # list that cannot be read writes nothing.
        self.assertScenarios("draws/baseline_off_the_lever_pushes_nothing",
                             "draws/baseline_push_with_nothing_planned_or_a_list_unread_writes_nothing")

    def test_draws_baseline_a_route_not_inline_refuses_the_watch(self):
        # Only DropManager's inline DropItem detour sees the game's calls: a table-only route or
        # a probe holding the slot refuses the watch, named, and it stays off.
        self.assertScenarios("draws/baseline_a_route_not_inline_refuses_the_watch")

    def test_draws_target_each_scope_classes_a_build(self):
        # draw inside a DropItem call; dropunique, angelic and heroic inside a nested roll, the
        # innermost one winning; outside with none; normal= for c 0; a nested DropItem is its
        # own draw scope, and ending the outer call closes a roll left open.
        self.assertScenarios("draws/target_each_scope_classes_a_build", "draws/target_nested_call_and_a_roll_left_open")

    def test_draws_target_k_on_k_and_off_k_follow_the_tier(self):
        # k is the tier the call's LoadSatanicDropTier answered (`-` when unread or outside a
        # call); on-k and off-k count the typed draw builds listed on list k or not; an untyped
        # pick has no lists and counts on neither.
        self.assertScenarios("draws/target_k_on_k_and_off_k_follow_the_tier", "draws/target_query_names_the_picked_triple")

    def test_draws_target_the_lever_pushes_c_on_each_list_and_takes_exactly_that_off(self):
        # c copies of each kill stand-in on each of [0..5] for the outermost call only, read back
        # on every tail, and exactly that taken off; a list the game changed is left as found and
        # named, an add that throws takes the partial push off again.
        self.assertScenarios("draws/target_push_plan_c_of_each_stand_in",
                             "draws/target_push_onto_each_list_and_exactly_that_off",
                             "draws/target_a_list_changed_during_the_call_is_left_as_found",
                             "draws/target_an_add_that_throws_comes_off_again")

    def test_draws_target_picks_and_pick_builds(self):
        # A typed draw-scope pick of a pushed stand-in counts picks= and, built, pick-builds=,
        # never on-k; the same pair picked inside a nested roll is no draw pick.
        self.assertScenarios("draws/target_picks_and_pick_builds_of_a_pushed_stand_in")

    def test_draws_target_the_lever_pushes_pool_draws_candidates(self):
        # The lever's stand-in per piece is `pool draw`'s candidate of its class (rarity 6,
        # fields 40 and 41 clear), SS first and the below-SS side only as a named fallback; an
        # entry the draw would send back is never planned, a class with no candidate is named as
        # refused, the reply is exact, and the stand-ins are resolved again when a list changed.
        self.assertScenarios("draws/target_lever_stand_ins_are_pool_draws_candidates_ss_first",
                             "draws/target_lever_plan_pushes_only_what_the_draw_keeps",
                             "draws/target_lever_line_exact",
                             "draws/target_lever_pushes_the_draw_stand_in_onto_each_list_and_off",
                             "draws/target_lever_stand_ins_resolved_again_when_a_list_changed")

    def test_draws_target_lines_refusals_and_parse(self):
        self.assertScenarios("draws/target_built_lines_exact", "draws/target_status_line_and_off_reply",
                             "draws/target_push_refused_while_mark1_or_pocket_elf_is_on",
                             "draws/target_built_lines_capped_counters_not", "draws/parse")

    # ---- draws: the lever's push counters and side (Live 1b) -----------------------

    def test_draws_baseline_push_counters_stay_zero_without_a_push(self):
        # The watch on and the push off: calls (nested ones too), reads, picks and builds leave
        # push-held=, push-skipped= and pushed-reads= at 0, and push-side= reads `-`.
        self.assertScenarios("draws/baseline_push_counters_stay_zero_without_a_push")

    def test_draws_target_push_held_once_per_outermost_call(self):
        # A push that reads back Held is one push-held per outermost DropItem call; a nested call
        # riding on its entries adds none, even when noted.
        self.assertScenarios("draws/target_push_held_once_per_outermost_call")

    def test_draws_target_push_skipped_on_every_silent_path(self):
        # Each silent way a call that was to push ends without a Held push (lists unread, no
        # stand-ins, nothing planned, a length unread, NotHeld, Threw, a throw the adapter
        # swallows) is one push-skipped and no push-held; a call that may not push notes nothing.
        self.assertScenarios("draws/target_push_skipped_on_every_silent_path")

    def test_draws_target_pushed_reads_count_only_draw_reads_of_a_pushed_triple(self):
        # A draw-scope read of a triple the call pushed counts (a nested DropItem's too); a read
        # of a game triple, inside a nested roll, unreadable, or outside the call does not.
        self.assertScenarios("draws/target_pushed_reads_only_draw_reads_of_a_pushed_triple")

    def test_draws_baseline_push_without_a_side_keeps_ss_first(self):
        # No side word picks exactly as built: SS first, the below-SS best a named fallback,
        # the same line, plan and reply, and push-side=ss-first.
        self.assertScenarios("draws/baseline_push_without_a_side_keeps_ss_first")

    def test_draws_target_push_below_picks_only_below_ss_and_refuses_a_class_without_one(self):
        # `below` (the keep control) takes only below-SS candidates and `ss` only SS ones, with
        # no fallback: a class with none on that side pushes nothing and is named as refused.
        # The side word parses, is refused with its reason otherwise, and shows in the reply,
        # on push-side= and in the stand-ins cache's key.
        self.assertScenarios("draws/target_push_below_picks_only_below_ss_and_refuses_a_class_without_one",
                             "draws/target_push_side_parse_reply_and_status")

    # ---- pool draw ----------------------------------------------------------------

    def test_pool_draw_candidate_rule_and_lines(self):
        # Per class (the four pieces and Pocket Elf's class 10): the distinct triples on any of
        # [0..5] reading rarity 6 with fields 40 and 41 clear, each asked once; ss= the tier-5
        # ones; the best per tier side by the smallest largest count, then the lowest (sub, b).
        self.assertScenarios("pool/draw_candidate_rule", "pool/draw_lines_exact", "pool/draw_classes_are_the_pieces_and_pocket_elf")

    # ---- the lines -------------------------------------------------------------

    def test_status_hook_show_and_args_lines(self):
        self.assertScenarios("hook/line_and_held_rows", "status/first_lines", "show/casts_per_talent_id", "show/lines",
                             "show/reset_clears_counts_and_casts", "args/line", "args/takes_n_then_stops")

    def test_watch_count_and_pos_lines(self):
        self.assertScenarios("watch/changes_only_from_the_second_snapshot", "watch/after_a_row_starts_at_its_next_local_call",
                             "watch/a_line_cap_ends_it_with_a_dropped_count", "count/one_line_per_change",
                             "pos/start_line", "pos/lines_per_moving_call_and_a_total")

    # ---- walkable --------------------------------------------------------------

    def test_walkable_verdict_line_for_each_field(self):
        self.assertScenarios("walkable/line_with_every_field_free", "walkable/line_with_every_field_blocked",
                             "walkable/grid_unreadable_and_four_tile_layers_at_most", "walkable/room_and_grid_cell")

    def test_walkable_scan_point_list(self):
        self.assertScenario("walkable/scan_point_list")

    def test_walkable_reads_a_meeting_as_a_boolean_and_a_line_as_an_instance(self):
        self.assertScenarios("walkable/meeting_false_or_zero_is_free_true_or_one_blocked",
                             "walkable/meeting_of_an_unrecognised_kind_is_unreadable",
                             "walkable/wall_line_instance_crossed_noone_clear", "walkable/grid_cell_minus_one_blocked")

    def test_walkable_prints_each_fields_raw_result(self):
        self.assertScenarios("walkable/a_point_with_nothing_read_is_unreadable_throughout",
                             "walkable/tiles_not_enumerated_print_unreadable", "walkable/raw_fields_follow_the_line",
                             "walkable/raw_error_and_what_a_blocked_family_met", "walkable/met_names_object_and_id")

    # ---- the parsers -----------------------------------------------------------

    def test_parse_accepted_forms(self):
        self.assertScenarios("parse/bare_hook_off_show_args", "parse/watch_forms", "parse/set_and_scale_forms",
                             "parse/move_and_stat_forms", "parse/count_pos_and_walkable_forms", "parse/phase2_forms")

    def test_parse_refused_forms_name_their_reason(self):
        self.assertScenario("parse/refused_forms_name_their_reason")

    def test_parse_usage_and_text(self):
        self.assertScenarios("parse/usage_lists_every_form", "parse/number_and_value_text")


if __name__ == "__main__":
    unittest.main()
