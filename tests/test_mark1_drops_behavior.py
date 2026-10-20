"""Run the Mark 1 drops' real core against controlled lists and verdicts.

Mark 1's phase 3 (ForgePact #166): while the "Mark 1 drops" switch is on, each
of the four pieces joins the game's own Heroic roll (`DropItemHeroic`) with one
entry's share, through #74's stand-in, coin and rewrite machinery. These
scenarios pin the decisions ModuleMain.cpp's drop adapter takes from
plugin/include/ForgePact/Mark1Drops.hpp, compiled whole (it includes
Mark1Set.hpp, the pieces' table).

Baseline (`baseline` in the name): the switch off pushes nothing and
attributes nothing, the hook that stays in passes through, and a hit on a
would-be stand-in with nothing pushed stays the game's own. Targets: the
sub-list a call reads (`list_index`: 5 when the argument is absent or
undefined, a whole number 0 to 5 itself, anything else refused and counted);
the stand-in rule (`standin`: a kept triple of the piece's own class, Heroic
preferred, a class with none leaving the others, ties by the lowest sub and b,
n counted with repeats, resolved again when the list changes); the coin c /
(n + c), the attribution and the rewrite target per piece from Mark1Set.hpp's
`kPieces` (`coin`); the gate and install words; the art-ready test
(`art_ready`); and the fresh `mark1set drops:` line with its named fields
(`fresh_line`).
"""
import os
import re
import shutil
import subprocess
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "plugin/include/ForgePact/Mark1Drops.hpp"


class Mark1DropsBehaviorTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        header = HEADER.read_text(encoding="utf-8")
        core = "\n".join(line for line in header.split("\n") if not line.strip().startswith("#pragma once"))
        out = ROOT / "build/mark1-drops-behavior"
        out.mkdir(parents=True, exist_ok=True)
        code = (ROOT / "tests/mark1_drops_harness.cpp").read_text(encoding="utf-8")
        code = code.replace("// PRODUCTION_MARK1DROPS", core)
        cpp = out / "mark1drops.cpp"
        cpp.write_text(code, encoding="utf-8")
        cls.binary = out / ("mark1drops.exe" if os.name == "nt" else "mark1drops")
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
                f'cl /nologo /std:c++20 /EHsc /O2 /W4 /WX /I "{ROOT / "plugin/include"}" "{cpp}" /Fe:"{cls.binary}" /Fo:"{out / "mark1drops.obj"}"\n'
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

    def assertScenarios(self, *labels):
        for label in labels:
            self.assertTrue(self.line(label).startswith("PASS "), self.line(label))

    def test_all_scenarios_pass(self):
        self.assertIn("RESULT OK", self.output, self.output)
        self.assertNotIn("FAIL ", self.output, self.output)

    # ---- baseline: the switch off is the game's own roll -----------------------

    def test_baseline_the_switch_off_pushes_nothing_and_only_counts_the_call(self):
        self.assertScenarios("baseline/switch_off_passes_and_only_counts_the_call",
                             "baseline/installed_switch_off_passes_through")

    def test_baseline_nothing_pushed_attributes_nothing(self):
        self.assertScenarios("baseline/nothing_pushed_attributes_nothing",
                             "baseline/a_stand_in_hit_with_nothing_pushed_stays_the_games")

    # ---- the core's contract ---------------------------------------------------

    def test_baseline_header_holds_no_runtime_value(self):
        # Numbers and strings only: the Mark1Set.hpp contract.
        code = "\n".join(line.split("//")[0] for line in HEADER.read_text(encoding="utf-8").split("\n"))
        for name in ("RValue", "CInstance", "YYTK", "Aurie", "g_Yytk"):
            self.assertNotIn(name, code, name)
        self.assertNotRegex(code, r"#include\s*[<\"](YYToolkit|Aurie)")
        self.assertNotRegex(code, r"(?i)\brva\b|0x[0-9a-f]{5,}")

    def test_list_index_and_copies_are_named_constants(self):
        header = HEADER.read_text(encoding="utf-8")
        self.assertRegex(header, r"inline constexpr int kHeroicDefaultListIndex\s*=\s*5\s*;")
        self.assertRegex(header, r"inline constexpr int kPlayerCopies\s*=\s*1\s*;")

    # ---- list_index ------------------------------------------------------------

    def test_list_index_absent_or_undefined_reads_5_and_whole_0_to_5_themselves(self):
        self.assertScenarios("list_index/absent_or_undefined_reads_5", "list_index/whole_0_to_5_read_themselves")

    def test_list_index_anything_else_is_refused_and_counted(self):
        self.assertScenarios("list_index/anything_else_refused",
                             "list_index/a_refused_index_pushes_nothing_and_is_counted")

    # ---- standin ---------------------------------------------------------------

    def test_standin_one_per_piece_of_its_own_class(self):
        self.assertScenarios("standin/one_per_piece_of_its_own_class", "standin/only_piece_classes_asked_each_once")

    def test_standin_heroic_preferred_then_any_kept_rarity(self):
        self.assertScenarios("standin/heroic_preferred", "standin/no_heroic_takes_the_lowest_kept",
                             "standin/refused_rarities_and_hidden_never_stand_in")

    def test_standin_ties_by_the_lowest_sub_and_b(self):
        self.assertScenarios("standin/ties_by_lowest_sub_then_b")

    def test_standin_n_counted_with_repeats(self):
        self.assertScenarios("standin/n_counted_with_repeats")

    def test_standin_a_class_with_none_leaves_the_other_pieces(self):
        self.assertScenarios("standin/a_class_with_none_leaves_the_others")

    def test_standin_unreadable_verdicts_are_counted_never_chosen(self):
        self.assertScenarios("standin/unreadable_counted_and_never_a_stand_in", "standin/no_verdict_reader_is_unread")

    def test_standin_plan_and_cache(self):
        self.assertScenarios("standin/plan_copies_per_piece_latched_left_out", "standin/plan_copies_bounded",
                             "standin/cache_resolves_again_when_id_or_length_changes")

    # ---- coin ------------------------------------------------------------------

    def test_coin_share_is_c_over_n_plus_c(self):
        self.assertScenarios("coin/share_is_c_over_n_plus_c", "coin/ours_below_the_share")

    def test_coin_a_hit_on_a_pushed_stand_in_is_the_piece(self):
        self.assertScenarios("coin/a_hit_on_the_whole_triple_is_the_piece_on_the_coin")

    def test_coin_rewrite_target_per_piece_is_mark1set_kpieces(self):
        self.assertScenarios("coin/rewrite_target_is_the_pieces_own_definition")
        # The pieces' table as Mark1Set.hpp writes it, against what the core
        # rewrites a hit to: a = the seed, b = the base, c 0, j 0.
        table = (ROOT / "plugin/include/ForgePact/Mark1Set.hpp").read_text(encoding="utf-8")
        rows = re.findall(r'\{\s*Piece::\w+,\s*"(\w+)",\s*"[^"]+",\s*(\d+),\s*(\d+),\s*(\d+)\s*\}', table)
        self.assertEqual(len(rows), 4, rows)
        rewrites = {}
        for line in self.output.split("\n"):
            if line.startswith("REWRITE "):
                parts = line.split()
                rewrites[parts[1]] = {k: int(v) for k, v in (p.split("=") for p in parts[2:])}
        for key, item_type, seed, base in rows:
            with self.subTest(piece=key):
                self.assertEqual(rewrites[key], {"type": int(item_type), "a": int(seed), "b": int(base), "c": 0, "j": 0})

    # ---- gate and install --------------------------------------------------------

    def test_gate_and_install_words(self):
        self.assertScenarios("gate/fresh_is_not_armed_and_off", "gate/switch_on_without_a_character_waits",
                             "gate/refused_turns_the_switch_off_and_a_later_install_clears_it",
                             "gate/no_detection_passes", "gate/nested_passes_and_is_counted",
                             "gate/not_resolved_when_no_stand_in_or_unreadable",
                             "gate/refused_rewrite_latches_the_piece", "stat/gate_names")

    # ---- art_ready ---------------------------------------------------------------

    def test_art_ready_not_armed_no_icon_or_ground_install_tried(self):
        self.assertScenarios("art_ready/not_armed_no_icon_or_ground_tried")

    def test_art_ready_a_call_that_is_not_waits_and_is_counted(self):
        self.assertScenarios("art_ready/a_call_not_art_ready_pushes_nothing_and_is_counted",
                             "art_ready/drops_switch_arms_the_art")

    # ---- the `mark1set drops:` line ----------------------------------------------

    def test_fresh_line_exact(self):
        self.assertScenarios("stat/fresh_line_exact", "stat/fresh_line_begins_mark1set_drops",
                             "stat/fresh_session_fills_the_fresh_line", "stat/no_double_space")

    def test_fresh_line_named_fields_in_order_and_filled(self):
        self.assertScenarios("stat/named_fields_in_order", "stat/filled_fields", "stat/stand_in_text")

    def test_reply_names_the_switch_the_install_and_the_gate(self):
        self.assertScenarios("stat/reply_names_the_switch_the_install_and_the_gate", "stat/reply_installed_and_refused")


if __name__ == "__main__":
    unittest.main()
