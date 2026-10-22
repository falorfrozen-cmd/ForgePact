"""Run the real ForgePact::Mark1PriorityCore (and Mark1Automation's writes) against a controlled bar.

The Mark 1 set's bonus casts the skills on the bar's Q, E, R and Y slots by
priority (the owner's playtest, 2026-10-22): a skill with a cooldown is cast
first, in that order, as soon as it is ready; a skill with no cooldown is cast
continuously, as holding its key does; a held manual input no longer keeps an
automated skill from being cast; a skill under automation cannot be cast by
hand; and on the proc route each skill is fired straight into the skill, at its
own interval (its cooldown at the player's Skill Haste plus its cast time at
the player's Faster Cast Rate). These scenarios pin that core before any
adapter reads the game: the cycle, the yield, the rests, the mask, the
lockout, the route and its call shape, the intervals (held to the hub's
hs-game-sdk/curated/mark1_automation_cases.json, the cases the hub's
mark1_automation_model computes), and the grey icons. With the bonus inactive
nothing is read, pressed, masked, locked or fired.
"""
import json
import os
import shutil
import subprocess
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HUB = ROOT.parent
CASES = HUB / "hs-game-sdk/curated/mark1_automation_cases.json"


def interval_cases():
    return json.loads(CASES.read_text(encoding="utf-8"))["interval"]


class Mark1PriorityBehaviorTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        priority = (ROOT / "plugin/include/ForgePact/Mark1Priority.hpp").read_text(encoding="utf-8")
        automation = (ROOT / "plugin/include/ForgePact/Mark1Automation.hpp").read_text(encoding="utf-8")
        cls.header = priority
        # The automation core includes the priority core: both go in, the
        # priority first, with the include between them stripped.
        klass = "\n".join(
            line for line in (priority + "\n" + automation).split("\n")
            if not line.strip().startswith("#pragma once")
            and '#include "Common.hpp"' not in line
            and '#include "Mark1Priority.hpp"' not in line
        )
        cls.cases = interval_cases()
        rows = "\n".join(
            "    {{ {!r}, {!r}, {!r}, {!r}, {!r} }},".format(
                float(c["ability_cooldown"]), float(c["haste"]), float(c["fcr"]),
                float(c["base_cast_seconds"]), float(c["expected_ms"]))
            for c in cls.cases)
        out = ROOT / "build/mark1-priority-behavior"
        out.mkdir(parents=True, exist_ok=True)
        code = (ROOT / "tests/mark1_priority_harness.cpp").read_text(encoding="utf-8")
        code = code.replace("// PRODUCTION_MARK1PRIORITY", klass).replace("    // INTERVAL_CASES", rows)
        cpp = out / "mark1priority.cpp"
        cpp.write_text(code, encoding="utf-8")
        cls.binary = out / ("mark1priority.exe" if os.name == "nt" else "mark1priority")
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
                f'cl /nologo /std:c++20 /EHsc /O2 /W4 /I "{ROOT / "plugin/include"}" "{cpp}" /Fe:"{cls.binary}" /Fo:"{out / "mark1priority.obj"}"\n'
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

    def assertScenarios(self, *labels):
        for label in labels:
            with self.subTest(scenario=label):
                self.assertTrue(self.line(label).startswith("PASS "), self.line(label))

    def test_all_scenarios_pass(self):
        self.assertIn("RESULT OK", self.output, self.output)

    def test_priority_constants_and_their_sources(self):
        self.assertScenarios("constants/values", "constants/settings_start_at_the_constants")
        for line in ("inline constexpr long long kMark1YieldFrames = 56;",
                     "inline constexpr long long kMark1ProbeRestFrames = 24;",
                     "inline constexpr double kMark1FreePressValue = 4.0;",
                     "inline constexpr double kMark1FreeCooldownFloor = 0.25;",
                     "inline constexpr Mark1Route kMark1AutoRoute = Mark1Route::Press;",
                     'inline constexpr const char* kMark1ProcShape = "";',
                     "inline constexpr double kMark1BaseCastSeconds = 1.0;",
                     "inline constexpr long long kMark1StatRefreshMs = 1000;"):
            with self.subTest(line=line):
                self.assertIn(line, self.header)

    # ---- baseline: the bonus inactive ---------------------------------------------
    def test_priority_baseline_inactive_presses_and_masks_nothing(self):
        self.assertScenarios("baseline/inactive_reads_and_writes_nothing", "baseline/inactive_counts_nothing",
                             "baseline/inactive_input_plan_is_empty")

    # ---- the press route's cycle ---------------------------------------------------
    def test_priority_target_cooldown_columns_first_in_q_e_r_y_order(self):
        self.assertScenarios("cooldown/first_cooldown_column_before_any_free_column",
                             "cooldown/next_cooldown_column_while_the_first_rests",
                             "cooldown/free_column_only_when_every_cooldown_column_rests",
                             "cooldown/q_e_r_y_order", "cooldown/back_from_its_rest_before_the_free_column",
                             "cooldown/unreadable_cooldown_is_a_cooldown_column")

    def test_priority_target_free_column_pressed_until_it_lands(self):
        self.assertScenarios("free/pressed_on_every_call_until_it_lands", "free/patience_then_rest",
                             "free/pressed_with_the_hold_value")

    def test_priority_target_yield_after_a_landing(self):
        self.assertScenarios("yield/no_press_for_the_yield_window", "yield/decision_names_the_yield",
                             "yield/held_key_untouched_during_the_yield", "yield/a_yield_of_0_presses_on")

    def test_priority_target_failed_cooldown_press_rests_the_column(self):
        self.assertScenarios("rest/failed_press_rests_the_probe_rest", "rest/counted_yielded",
                             "rest/the_rest_in_force_is_used", "rest/frame_reset_ends_a_rest", "rest/a_landing_is_no_rest")

    def test_priority_target_free_columns_take_turns(self):
        self.assertScenarios("turns/one_cast_each_in_q_e_r_y_order", "turns/patience_passes_the_turn",
                             "turns/ineligible_free_column_skipped")

    def test_priority_target_mask_only_asking_entries_while_pressing(self):
        self.assertScenarios("mask/asking_entries_rested_during_the_press", "mask/put_back_after_the_call",
                             "mask/counted", "mask/nothing_masked_without_a_press",
                             "mask/off_reads_and_writes_nothing_but_the_press", "mask/entry_list_in_force",
                             "mask/put_back_after_a_throw", "mask/parse_refuses", "mask/plan_needs_a_press")

    # ---- the routes ------------------------------------------------------------------
    def test_priority_baseline_press_route_without_a_shape(self):
        self.assertScenarios("route/proc_without_a_shape_refused", "route/press_decisions_unchanged",
                             "route/shape_parsed", "route/shape_unknown_token_refused_by_name",
                             "route/clear_returns_to_the_press")

    def test_priority_target_proc_shape_player_token(self):
        # Live 1 (replan 1): the template had no token for the local player,
        # so P5 passed a1 as a number where the game passes a ref. `player`
        # parses, formats back and reaches the adapter as its own kind; a
        # literal instance number still parses as a number, and a near miss is
        # refused by name.
        self.assertScenarios("shape/player_token_parsed", "shape/literal_instance_number_is_a_number",
                             "shape/near_miss_refused", "shape/player_built_for_the_adapter",
                             "shape/literal_built_as_a_number", "shape/player_through_the_automation")

    def test_priority_target_proc_clocks_survive_a_room_change(self):
        # Round 0's review: every room change forgot the proc clocks, so a
        # cooldown skill procced again on entering a room. A room keeps them
        # (and still forgets the press route's state); a new character clears
        # them.
        self.assertScenarios("room/core_room_keeps_the_proc_clock", "room/core_room_forgets_the_press_state",
                             "room/core_new_character_clears_the_clock", "room/proc_clock_survives_a_room_change",
                             "room/new_character_clears_the_proc_clock", "room/bonus_again_keeps_the_proc_clock")

    def test_priority_target_proc_route_bypasses_the_gate(self):
        self.assertScenarios("proc/fired_never_pressed", "proc/never_masked", "proc/args_from_the_shape",
                             "proc/counted", "proc/a_throw_falls_back_to_the_press",
                             "proc/refused_counts_a_failure_not_a_call", "proc/refused_argument",
                             "proc/no_clock_no_proc", "proc/unreadable_cooldown_keeps_the_press")

    def test_priority_target_proc_cooldown_column_waits_its_interval(self):
        self.assertScenarios("interval/cooldown_at_haste_plus_cast_at_fcr", "interval/next_due_time_set_when_fired",
                             "interval/base_cast_seconds_in_force", "interval/clock_reset_is_due")

    def test_priority_target_proc_free_column_fires_each_cast_time(self):
        self.assertScenarios("procfree/each_on_its_own_clock", "procfree/q_first_when_both_are_due",
                             "procfree/never_pressed", "procfree/faster_cast_rate_halves_the_interval")

    def test_priority_target_proc_never_fires_a_toggle(self):
        self.assertScenarios("toggle/never_fired", "toggle/takes_the_press", "toggle/not_a_proc_column")

    def test_priority_target_interval_matches_the_shared_cases(self):
        # Every `interval` case of the hub's cases file, within 1 ms.
        self.assertTrue(self.cases, "the cases file has no interval case")
        self.assertIn(f"INTERVAL_CASES {len(self.cases)}", self.output)
        for k, case in enumerate(self.cases):
            with self.subTest(case=case["label"]):
                self.assertTrue(self.line(f"cases/interval_{k}").startswith("PASS "), self.line(f"cases/interval_{k}"))

    # ---- the lockout and the grey icons ------------------------------------------------
    def test_priority_baseline_lockout_inactive_touches_nothing(self):
        self.assertScenarios("lockbase/inactive_touches_nothing", "lockbase/lockout_off_leaves_the_key")

    def test_priority_target_lockout_rests_only_automated_held_entries(self):
        self.assertScenarios("lock/automated_held_and_pressed_rested", "lock/before_the_press",
                             "lock/not_automated_left_alone", "lock/put_back_after_the_call",
                             "lock/counted_once_per_call", "lock/resting_and_release_left_alone",
                             "lock/during_the_yield", "lock/on_the_proc_route", "lock/put_back_after_a_throw")

    def test_priority_target_grey_mask_is_the_border_mask(self):
        self.assertScenarios("grey/the_border_mask", "grey/inactive_none")

    def test_priority_research_overrides_refuse_out_of_range(self):
        self.assertScenarios("overrides/ranges", "overrides/stat_cache")

    def test_priority_header_is_pure(self):
        # No game object, no Common.hpp, no address: the core reads and writes
        # nothing itself.
        for banned in ('#include "Common.hpp"', "g_Yytk", "CallBuiltin", "Rva", "GetModuleHandle", "RValue"):
            with self.subTest(banned=banned):
                self.assertNotIn(banned, self.header)
        # Amendment 1's constants are gone.
        for gone in ("kMark1ProcFreeFrames", "kMark1ProcCooldownScale"):
            with self.subTest(gone=gone):
                self.assertNotIn(gone, self.header)


if __name__ == "__main__":
    unittest.main()
