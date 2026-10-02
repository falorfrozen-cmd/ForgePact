"""Run the real relic filter through the game's own relic pick, against controlled game API responses.

Companion to test_relic_filter_contract.py, which asserts on source text.

- ForgePact 2.0.1's filter logged "holding back 1 of 1" while the relic it
  named kept dropping (#125). Its lever wrote `droprate.base`, which no relic
  pick reads (hub docs/models/relic-pick-spec.md).
- Earlier, origin's review of PR #4 had shown a log line announcing a working
  filter on paths where nothing was applied.

These scenarios play the game's draw-again loop through the real
Hook_GetRelicQuest and report which relic actually dropped, beside what was
logged.
"""
import os
import re
import shutil
import subprocess
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def implementation(source, signature):
    """The full text of `signature`'s definition (last occurrence wins)."""
    start = source.rfind(signature)
    if start < 0:
        raise AssertionError(f"not found: {signature}")
    brace = source.index("{", start)
    depth = 0
    for index in range(brace, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[start:index + 1]
    raise AssertionError(f"unterminated: {signature}")


def statics(source):
    """The lever's own statics, from its trampoline to the skip-log constant."""
    match = re.search(
        r"^static PFUNC_YYGMLScript g_Orig_GetRelicQuest = nullptr;$.*?^static constexpr long kRelicSkipLinesLogged = \d+;$",
        source, re.S | re.M)
    if match is None:
        raise AssertionError("the GetRelicQuest statics were not found in ModuleMain.cpp")
    return match.group(0)


class RelicFilterBehaviorTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        plugin = (ROOT / "plugin/ModuleMain.cpp").read_text(encoding="utf-8").replace("\r\n", "\n")
        header = (ROOT / "plugin/include/ForgePact/RelicFilterMod.hpp").read_text(encoding="utf-8").replace("\r\n", "\n")

        # The real class, verbatim, minus the include of Common.hpp (the
        # harness supplies the stand-ins Common.hpp would have pulled in).
        klass = "\n".join(
            line for line in header.split("\n")
            if not line.strip().startswith("#pragma once")
            and '#include "Common.hpp"' not in line
        )
        functions = "\n\n".join(implementation(plugin, signature) for signature in (
            "static RValue& Hook_DropRelic(",
            "static RValue& Hook_GetRelicQuest(",
            "static std::string RelicFilterHookState(",
            "static void RelicFilterStatus(",
            # The arm-time report (#93, #125) lives beside the hooks.
            "static void RelicFilterReportArmScan(",
        ))

        out = ROOT / "build/relic-filter-behavior"
        out.mkdir(parents=True, exist_ok=True)
        code = (ROOT / "tests/relic_filter_harness.cpp").read_text(encoding="utf-8")
        code = (code.replace("// PRODUCTION_RELICFILTER", klass)
                    .replace("// PRODUCTION_STATICS", statics(plugin))
                    .replace("// PRODUCTION_FUNCTIONS", functions))
        cpp = out / "relicfilter.cpp"
        cpp.write_text(code, encoding="utf-8")

        cls.binary = out / ("relicfilter.exe" if os.name == "nt" else "relicfilter")
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
                f'cl /nologo /std:c++20 /EHsc /O2 "{cpp}" /Fe:"{cls.binary}" /Fo:"{out / "relicfilter.obj"}"\n'
                f'exit /b %errorlevel%\n', encoding="utf-8")
            command = ["cmd", "/d", "/c", str(batch)]
        else:
            compiler = shutil.which("c++")
            if not compiler:
                raise unittest.SkipTest("A C++20 compiler is required for native behavior tests")
            command = [compiler, "-std=c++20", "-O2", str(cpp), "-o", str(cls.binary)]

        # The compiler speaks the machine's locale; decode leniently so a
        # localized diagnostic cannot itself crash the test.
        result = subprocess.run(command, cwd=out, capture_output=True, text=True, encoding="utf-8", errors="replace")
        (out / "compile.log").write_text(result.stdout + result.stderr, encoding="utf-8")
        if result.returncode:
            raise AssertionError(result.stdout + result.stderr)

        run = subprocess.run([str(cls.binary)], capture_output=True, text=True, encoding="utf-8", errors="replace")
        cls.output = run.stdout
        (out / "run.log").write_text(run.stdout + run.stderr, encoding="utf-8")

    def scenario(self, label):
        for line in self.output.split("\n"):
            if line.startswith(f"SCENARIO {label} "):
                return line
        raise AssertionError(f"scenario {label!r} not in harness output:\n{self.output}")

    def logs(self, label):
        prefix = f"LOG {label} :: "
        return [line[len(prefix):] for line in self.output.split("\n") if line.startswith(prefix)]

    def counts(self, label):
        parts = dict(piece.split("=") for piece in self.scenario(label).split(" ")[2:] if "=" in piece)
        return {key: int(value) for key, value in parts.items()}

    def skip_lines(self, label):
        return [line for line in self.logs(label) if line.startswith("relicfilter: skipped maxed relic ")]

    def test_harness_ran(self):
        self.assertIn("HARNESS DONE", self.output, self.output)

    # ---- the lever, through the game's own pick (#125) ---------------------

    def test_baseline_the_filter_off_lets_the_maxed_relic_drop(self):
        counts = self.counts("baseline_off")
        self.assertEqual(counts["dropped"], 140, self.output)
        self.assertEqual(counts["scans"], 0, self.output)
        self.assertEqual(self.skip_lines("baseline_off"), [])

    def test_target_a_maxed_relic_is_skipped_and_another_drops(self):
        counts = self.counts("positive_control")
        self.assertEqual(counts["dropped"], 5, self.output)
        self.assertEqual(counts["skips"], 1, self.output)
        self.assertEqual(self.skip_lines("positive_control"),
                         ["relicfilter: skipped maxed relic 140, the game picks again (1 since armed)"])

    def test_a_quest_relic_is_the_games_own_reroll(self):
        counts = self.counts("quest_is_the_games")
        self.assertEqual(counts["dropped"], 3, self.output)
        self.assertEqual(counts["skips"], 0, self.output)

    def test_a_relic_that_is_not_maxed_drops_untouched(self):
        counts = self.counts("not_maxed")
        self.assertEqual(counts["dropped"], 9, self.output)
        self.assertEqual(counts["skips"], 0, self.output)

    def test_every_relic_maxed_stands_down_once_and_lets_the_game_pick(self):
        counts = self.counts("all_maxed")
        self.assertEqual(counts["dropped"], 140, self.output)
        self.assertEqual(counts["skips"], 0, self.output)
        logged = self.logs("all_maxed")
        self.assertEqual(logged.count(
            "relicfilter: every droppable relic is maxed, filter stands down (nothing left to drop instead)"), 1, logged)
        self.assertEqual(self.skip_lines("all_maxed"), [])

    def test_the_last_relic_left_drops(self):
        counts = self.counts("one_left")
        self.assertEqual(counts["dropped"], 77, self.output)
        self.assertEqual(counts["skips"], 3, self.output)

    def test_afk_farm_reward_scope_gets_the_games_answer(self):
        counts = self.counts("reward_scope")
        self.assertEqual(counts["dropped"], 140, self.output)
        self.assertEqual(counts["scans"], 0, self.output)

    def test_a_scan_that_did_not_run_holds_nothing_back(self):
        for label in ("no_player", "scan_throws"):
            counts = self.counts(label)
            self.assertEqual(counts["dropped"], 140, self.output)
            self.assertEqual(counts["skips"], 0, self.output)

    def test_one_scan_per_frame(self):
        one = self.counts("frame_cache_one_frame")
        self.assertEqual(one["dropped"], 9, self.output)
        self.assertEqual(one["skips"], 3, self.output)
        self.assertEqual(one["scans"], 1, self.output)
        self.assertEqual(self.counts("frame_cache_next_frame")["scans"], 2, self.output)

    def test_the_skip_log_thins_out(self):
        self.assertEqual(self.counts("log_thinning")["skips"], 100, self.output)
        lines = self.skip_lines("log_thinning")
        self.assertEqual(len(lines), 21, lines)
        self.assertEqual(lines[-1], "relicfilter: skipped maxed relic 140, the game picks again (100 since armed)")

    def test_research_test_ids_join_a_scan_that_ran(self):
        self.assertEqual(self.counts("testmaxed_joins")["dropped"], 3, self.output)
        self.assertEqual(self.counts("testmaxed_needs_a_scan")["dropped"], 7, self.output)

    def test_status_names_the_hook_and_the_count(self):
        self.assertEqual(self.logs("status_on"), [
            "relicfilter status: ON (GetRelicQuest, native detour) | skipped 1 maxed relic(s) since armed "
            "(last 140) | stands down: no | testmaxed=0"])
        (table_only,) = self.logs("status_table_only")
        self.assertIn("FAILED (GetRelicQuest is table-only", table_only)
        (off,) = self.logs("status_off")
        self.assertTrue(off.startswith("relicfilter status: OFF | "), off)

    def test_arming_again_starts_a_fresh_count(self):
        self.assertIn("SCENARIO rearm_resets before=2 after=0", self.output)

    def test_drop_relic_is_only_the_multiplier_now(self):
        self.assertEqual(self.counts("drop_x1")["dropcalls"], 1, self.output)
        self.assertEqual(self.counts("drop_x3")["dropcalls"], 3, self.output)
        self.assertEqual(self.counts("drop_reward_scope")["dropcalls"], 1, self.output)
        for label in ("drop_x1", "drop_x3", "drop_reward_scope"):
            self.assertEqual(self.counts(label)["scans"], 0, self.output)

    # ---- #93/#125: the lines the filter logs when it arms ------------------
    # The filter's other lines sit inside a relic roll, and relics roll only
    # in Satanic zones, so a live session cannot wait for one. These lines,
    # built from the scan's own set and reports, are what a live check and a
    # player's log read instead.

    def test_two_maxed_relics_are_named_by_count_and_id(self):
        self.assertIn("relicfilter: scan found 2 maxed relics (ids 7,42)",
                      self.logs("arm_scan_two"), self.output)

    def test_ids_are_in_numeric_order(self):
        self.assertIn("relicfilter: scan found 3 maxed relics (ids 9,124,135)",
                      self.logs("arm_scan_sorted"), self.output)

    def test_an_empty_scan_says_none(self):
        self.assertIn("relicfilter: scan found 0 maxed relics (ids none)",
                      self.logs("arm_scan_empty"), self.output)

    def test_a_missing_player_is_a_scan_that_did_not_run(self):
        logged = self.logs("arm_scan_no_player")
        self.assertIn("relicfilter: scan did not run (no player yet)", logged, self.output)
        self.assertFalse([line for line in logged if "scan found" in line], logged)

    def test_one_line_per_arm(self):
        """The report is due once per arm and the report itself clears it."""
        for label in ("arm_scan_two", "arm_scan_sorted", "arm_scan_empty", "arm_scan_no_player"):
            counts = self.counts(label)
            self.assertEqual(counts["due_before"], 1, self.output)
            self.assertEqual(counts["due_after"], 0, self.output)
            self.assertEqual(len([line for line in self.logs(label) if line.startswith("relicfilter: scan ")]), 1,
                             self.logs(label))

    def test_the_report_rolls_no_relic(self):
        self.assertEqual(self.counts("arm_scan_two")["questcalls"], 0, self.output)

    def test_switching_off_cancels_a_due_report(self):
        self.assertEqual(self.counts("arm_then_off")["due"], 0, self.output)

    def test_rearming_with_the_hook_already_in_still_reports(self):
        counts = self.counts("rearm_hooked")
        self.assertEqual(counts["pending"], 0, self.output)
        self.assertEqual(counts["due"], 1, self.output)

    def test_the_equipped_slot_and_relic_tab_reports_follow_the_scan_line(self):
        self.assertEqual(self.logs("arm_scan_two")[-3:], [
            "relicfilter: scan found 2 maxed relics (ids 7,42)",
            "relicfilter: equipped slots relic=2 stopped=none",
            "relicfilter: relic tab relic=14 stopped=none",
        ], self.output)
        self.assertEqual(self.counts("arm_scan_two")["reports"], 1, self.output)

    def test_a_stopped_read_names_its_stage_beside_the_zero(self):
        self.assertEqual(self.logs("arm_scan_stopped")[-3:], [
            "relicfilter: scan found 0 maxed relics (ids none)",
            "relicfilter: equipped slots relic=0 stopped=owner",
            "relicfilter: relic tab relic=0 stopped=controller",
        ], self.output)

    def test_a_scan_that_did_not_run_claims_no_stage(self):
        for label in ("arm_scan_no_player", "arm_scan_throws"):
            logged = self.logs(label)
            self.assertIn("relicfilter: scan did not run (no player yet)", logged, self.output)
            self.assertFalse([line for line in logged if "equipped slots" in line or "relic tab" in line], logged)

    def test_the_roll_itself_asks_for_no_report(self):
        """The hook's scan runs at relic rolls; the reports belong to the once-per-arm lines only."""
        for label in ("positive_control", "all_maxed", "frame_cache_one_frame", "log_thinning"):
            self.assertEqual(self.counts(label)["reports"], 0, self.output)


if __name__ == "__main__":
    unittest.main()
