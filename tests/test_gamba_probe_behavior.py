"""Run gambaprobe's real decision core against controlled calls.

`gambaprobe` (research build only, ForgePact #134) hooks the gamba machine's
events, the scripts they call and the RNG and instance builtins, and its one
lever, `rng <builtin> <value> [count] [args <text>]`, answers the next `count`
calls of that RNG builtin whose self is a gamba machine (and, with `args`,
whose argument text matches). These scenarios pin the decision
ModuleMain.cpp's adapter takes from plugin/include/ForgePact/GambaProbe.hpp,
compiled whole.

Baseline: with the probe idle and the lever off, every RNG answer is the real
one, no self is read and no counter but `calls` moves; armed with the lever
off, calls are classified and still never answered. Target: with the lever on,
only a machine-self call of the target is answered, `count` calls and then the
lever is off; another object's call, another self inside a machine's event,
and a machine call of another builtin or with other arguments are untouched,
the last two counted as passed; a lever its target never reached is named
INERT; moving keys cannot spend a row's trace budget within a spin; the status
line reads back every counter.

Replan 1 (after Live 1 found every spawned machine removed before its first
Step_0): a call of instance_destroy, instance_change or
instance_deactivate_object whose first argument names a machine is counted
(`machine-arg=`) while its own self is anything else, and nothing else is; the
caller walk prints at most its budget per window and counts the rest; a
frame's text names the gml row, the runner, this plugin, another module or
nothing; and a script row's by-name route reads same, detoured, shared or
missing from its two name lookups.

Phase 4 (the explosion watch): without a sprite change, a vanished machine or
a `window` command, no window line is produced and the trace budgets count as
before, and a machine seen again with the same sprite prints nothing more.
A sprite change or a vanished machine prints one line and opens a window that
replays both rings' last two frames (build-row calls, and instance
create/destroy calls in a ring of their own that a burst cannot use to evict a
build) in call order, logs forward calls for its span, keeps two caps of its
own - build lines that instance lines can never spend, and instance lines per
object - counting what each dropped, tops both up when extended, and is
extended, never doubled, by a second transition; every line is the fixed text
the contract test and the live procedure read.
"""
import os
import shutil
import subprocess
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class GambaProbeBehaviorTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        header = (ROOT / "plugin/include/ForgePact/GambaProbe.hpp").read_text(encoding="utf-8")
        core = "\n".join(line for line in header.split("\n") if not line.strip().startswith("#pragma once"))
        out = ROOT / "build/gamba-probe-behavior"
        out.mkdir(parents=True, exist_ok=True)
        code = (ROOT / "tests/gamba_probe_harness.cpp").read_text(encoding="utf-8")
        code = code.replace("// PRODUCTION_GAMBAPROBE", core)
        cpp = out / "gambaprobe.cpp"
        cpp.write_text(code, encoding="utf-8")
        cls.binary = out / ("gambaprobe.exe" if os.name == "nt" else "gambaprobe")
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
                f'cl /nologo /std:c++20 /EHsc /O2 /W4 /I "{ROOT / "plugin/include"}" "{cpp}" /Fe:"{cls.binary}" /Fo:"{out / "gambaprobe.obj"}"\n'
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
        # /W4 is the bar: a warning in the decision core is a finding, not noise.
        cls.compile_output = result.stdout + result.stderr
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
        self.assertNotIn("FAIL ", self.output, self.output)

    def test_the_core_compiles_without_warnings(self):
        self.assertNotRegex(self.compile_output, r"warning C\d+", self.compile_output)

    def test_the_tables(self):
        for label in ("table/events", "table/rng_rows_and_kinds", "table/row_layout", "table/route_names",
                      "table/trace_budget_is_named", "table/builtin_by_name", "table/args_key_trims_and_folds_whitespace"):
            self.assertScenario(label)

    # ---- baseline: what the game does without the lever ------------------

    def test_idle_every_rng_answer_is_the_real_one_and_only_calls_moves(self):
        for label in ("baseline/idle_every_rng_answer_is_the_real_one", "baseline/idle_no_counter_but_calls_moves",
                      "baseline/idle_reads_no_self", "baseline/idle_events_and_scripts_only_count",
                      "baseline/idle_lever_and_status_off"):
            self.assertScenario(label)

    def test_armed_with_the_lever_off_answers_nothing(self):
        for label in ("baseline/armed_lever_off_answers_nothing", "baseline/armed_classifies_machine_in_event_other",
                      "baseline/armed_status_on"):
            self.assertScenario(label)

    def test_the_machine_self_predicate(self):
        for label in ("predicate/only_the_machines_own_index", "predicate/unresolved_machine_is_nothing",
                      "predicate/unresolved_machine_answers_nothing"):
            self.assertScenario(label)

    # ---- target: what the lever does --------------------------------------

    def test_only_a_machine_self_call_is_answered(self):
        for label in ("target/set_rng", "target/another_objects_call_is_untouched",
                      "target/another_self_inside_a_machine_event_is_untouched", "target/levered_reads_the_self",
                      "target/machine_self_answered_with_the_value", "target/instance_rows_never_answered",
                      "target/an_instance_row_is_no_target", "target/other_selves_are_not_counted_as_passed",
                      "target/another_builtins_machine_call_passes_untouched", "target/answered_counted_per_row"):
            self.assertScenario(label)

    def test_an_armed_lever_leaves_a_non_matching_machine_self_call_untouched(self):
        """The lever is aimed: an idle per-frame call or a reel roll cannot take the answer meant for the prize roll."""
        for label in ("aim/set_with_args", "aim/non_matching_machine_calls_are_untouched",
                      "aim/inert_line_counts_what_passed", "aim/args_text_is_read_only_for_the_targets_machine_calls",
                      "aim/the_matching_call_is_answered", "aim/no_args_filter_matches_any_arguments"):
            self.assertScenario(label)

    def test_count_calls_then_off(self):
        for label in ("target/count_calls_then_off", "target/finished_is_read_once",
                      "target/after_count_the_real_one_again", "target/out_of_range_set_refused_nothing_changed",
                      "target/set_rng_starts_clean"):
            self.assertScenario(label)

    def test_choose_answers_one_of_its_own_arguments(self):
        for label in ("target/choose_answers_an_argument_index", "target/choose_out_of_range_runs_the_original",
                      "target/choose_fraction_or_negative_runs_the_original", "target/value_rows_take_any_finite_value"):
            self.assertScenario(label)

    def test_a_lever_no_machine_call_reached_is_named_inert(self):
        for label in ("inert/armed_lever_no_machine_call_reached_is_named", "inert/an_answer_ends_it",
                      "inert/lever_off_or_never_set_is_not"):
            self.assertScenario(label)

    def test_the_trace_budget(self):
        for label in ("trace/at_most_the_budget_per_row", "trace/budget_is_per_row",
                      "trace/one_key_cannot_spend_the_row", "trace/budget_spent_is_named",
                      "trace/a_repeat_for_the_same_key_spends_nothing", "trace/hook_again_restores_the_budget",
                      "trace/reset_restores_each_keys_budget", "trace/bad_row_refused"):
            self.assertScenario(label)

    def test_moving_keys_cannot_spend_a_row_within_a_spin(self):
        """Six keys moving every frame, then a seventh key at the spin's end: its line is still logged."""
        for label in ("trace/moving_keys_cannot_spend_the_row_within_a_spin",
                      "trace/every_key_of_a_full_row_writes_its_lines"):
            self.assertScenario(label)

    def test_a_repeated_line_is_counted_and_named_never_silent(self):
        """A prize roll whose line repeats a reel roll's must not vanish from the trace."""
        for label in ("trace/a_same_shape_repeat_is_one_line_and_counted",
                      "trace/status_names_the_key_a_repeat_folded_into",
                      "trace/another_key_with_the_same_result_is_no_repeat",
                      "trace/the_keys_next_line_carries_its_repeats", "trace/carried_repeats_are_taken_once",
                      "trace/a_new_window_starts_repeats_over"):
            self.assertScenario(label)

    def test_the_status_line_reads_back_every_counter(self):
        for label in ("status/events_by_key", "status/row_reads_every_counter", "status/passed_is_counted_on_its_own_row",
                      "status/non_rng_rows_omit_the_lever_counters", "status/line_sums_every_counter",
                      "status/rng_line", "status/number_text"):
            self.assertScenario(label)

    def test_off_disarms_and_keeps_the_counts(self):
        self.assertScenario("off/disarms_and_lever_off_counts_stay")

    # ---- replan 1: who removes the machine, and the by-name route ----------

    def test_a_call_whose_first_argument_names_a_machine_is_counted(self):
        """instance_destroy(<machine>) from another self is a removal the self filter alone would miss."""
        for label in ("machinearg/only_the_target_taking_rows", "machinearg/names_a_machine",
                      "machinearg/names_nothing_else", "machinearg/unresolved_machine_names_nothing",
                      "machinearg/another_selfs_call_naming_a_machine_is_counted", "machinearg/status_reads_it_back",
                      "machinearg/bad_row_refused"):
            self.assertScenario(label)

    def test_the_caller_walk_budget(self):
        for label in ("walk/budgets", "walk/first_four_per_row_then_counted", "walk/status_reads_it_back",
                      "walk/a_new_window_starts_the_walks_over"):
            self.assertScenario(label)

    def test_the_frame_text(self):
        """A frame is a gml row, a runner frame, this plugin, another module or unknown."""
        for label in ("walk/rows_sorted_by_function", "walk/frame_in_a_gml_row", "walk/runner_frame_above_a_row_is_exe",
                      "walk/runner_frame_below_every_row_is_exe", "walk/plugin_frame", "walk/other_module_frame",
                      "walk/unknown_frame_and_no_rows"):
            self.assertScenario(label)

    def test_the_byname_route(self):
        for label in ("byname/words", "byname/index_kinds", "byname/both_names_the_script_is_same",
                      "byname/a_routine_that_is_the_rows_own_function_is_same", "byname/another_game_routine_is_detoured",
                      "byname/full_name_unresolved_is_missing", "byname/an_undetourable_routine_is_missing",
                      "byname/several_rows_on_one_routine_share_it", "byname/status_text_ends_with_the_word"):
            self.assertScenario(label)

    # ---- phase 4: the explosion watch -------------------------------------

    def test_the_watch_limits_and_fixed_lines(self):
        for label in ("watch/limits", "watch/window_builtin_rows", "watch/span_default_and_cap", "watch/fixed_lines",
                      "watch/status_line"):
            self.assertScenario(label)

    def test_without_a_transition_or_a_window_command_nothing_changes(self):
        """Baseline: no window line, the trace budgets count as before, one first-sight line per machine."""
        for label in ("watch/baseline_no_window_without_a_transition", "watch/baseline_trace_budgets_unchanged",
                      "watch/ring_is_bounded", "watch/baseline_same_sprite_one_first_sight_line"):
            self.assertScenario(label)

    def test_a_transition_opens_one_window_that_replays_and_logs_forward(self):
        """Target: change and gone lines open a window, which replays its look-back and stops at its own cap."""
        for label in ("watch/sprite_change_opens_a_window_and_replays_the_ring", "watch/forward_lines_until_the_span_ends",
                      "watch/a_machine_that_disappears_is_one_gone_line_and_a_window",
                      "watch/instance_lines_stop_per_object_and_at_their_cap",
                      "watch/build_lines_keep_their_own_cap_and_count_what_they_dropped", "watch/command_opens_the_same_window",
                      "watch/a_command_on_an_open_window_tops_up_both_caps", "watch/an_instance_burst_cannot_evict_a_build",
                      "watch/a_capped_object_is_named_once_per_window",
                      "watch/a_transition_while_open_extends_the_window", "watch/a_call_a_window_counted_is_not_replayed",
                      "watch/off_closes_and_forgets_the_machines"):
            self.assertScenario(label)

    def test_the_decision_keys(self):
        for label in ("decision/six_keys_in_order", "decision/listed_labels_valid", "decision/pending_only_before_live_1",
                      "decision/drop_route_takes_a_name", "decision/unlisted_label_or_key_refused"):
            self.assertScenario(label)


if __name__ == "__main__":
    unittest.main()
