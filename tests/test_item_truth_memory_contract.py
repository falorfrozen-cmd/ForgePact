"""Source contracts for the Item Truth memory research (`truthmem`, research build).

2026-09-26: a session that evaluated 197,704 items ended in a WER report and the
evaluations were thought to keep ~95 KB each. docs/item-truth-memory-research.md
measured it: they keep nothing, and a held item costs about 6 KB. What made the
measurement trustworthy is pinned here:

- `truthmem` (the collector's state, the positive control that holds evaluated
  items, gc_collect) exists only in the research build: the player build
  compiles none of it, and its evaluation path is byte for byte what it was;
- the positive control holds items only in one global struct, which
  `truthmem release` drops, so a measured "hold" is the game's own collector
  keeping them, and nothing of it outlives the command;
- LogDrop (research build) skips Item Truth's own builds: it used to copy every
  evaluated item's full text into a session-long set and into itemdrops.jsonl.
"""

import re
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from test_release_hook_contract import (  # noqa: E402
    function_body,
    strip_comments,
    strip_research_blocks,
)

PROJECT_ROOT = Path(__file__).resolve().parents[1]
PLUGIN_PATH = PROJECT_ROOT / "plugin" / "ModuleMain.cpp"


class ItemTruthMemoryResearchContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.plugin = PLUGIN_PATH.read_text(encoding="utf-8").replace("\r\n", "\n")
        cls.research = strip_comments(cls.plugin)
        cls.release = strip_comments(strip_research_blocks(cls.plugin))

    def test_the_player_build_compiles_none_of_it(self):
        for name in ("truthmem", "TruthMem", "fp_truthmem_hold", "psapi.h", "g_ItemTruthBuilding++",
                     "++g_ItemTruthBuilding"):
            self.assertNotIn(name, self.release, name)
        self.assertIn("static void TruthMemCommand(", self.research)

    def test_it_is_a_research_command_with_its_own_early_return(self):
        players = self.release[self.release.index("kPlayerCommands = {"):]
        players = players[:players.index("};")]
        self.assertNotIn('"truthmem"', players)
        self.assertIn('if (lc == "truthmem") { TruthMemCommand(rest); return; }', self.research,
                      "an early return, not one more `else if` in the chain at MSVC's C1061 limit")
        self.assertNotIn('else if (lc == "truthmem")', self.research)

    def test_the_player_builds_evaluation_path_is_unchanged(self):
        body = function_body(self.release, "static bool TruthEvalOne(").strip()
        self.assertEqual("return TruthBuildItem(entry).m_Kind == VALUE_OBJECT;", body)
        research = function_body(self.research, "static bool TruthEvalOne(")
        hold = research.index("if (g_TruthMemHold) {")
        self.assertLess(research.index("TruthMemHoldItem(item);", hold), research.index(
            "return TruthBuildItem(entry).m_Kind == VALUE_OBJECT;"), "only a held run takes the other path")

    def test_the_positive_control_holds_items_in_one_global_that_release_drops(self):
        hold = function_body(self.research, "static void TruthMemHoldItem(")
        self.assertIn('variable_global_get", { RValue("fp_truthmem_hold") }', hold)
        self.assertIn('variable_global_set", { RValue("fp_truthmem_hold"), reg }', hold)
        self.assertIn('variable_struct_set", { reg, RValue(std::to_string(g_TruthMemHeld)), item }', hold)
        self.assertNotRegex(hold, r"std::vector<RValue>|push_back\(item\)",
                            "a C++ copy is invisible to the collector: it would neither hold nor free the item")
        command = function_body(self.research, "static void TruthMemCommand(")
        release = command[command.index('verb == "release"'):]
        self.assertIn('variable_global_set", { RValue("fp_truthmem_hold"), RValue() }', release[:400])
        self.assertIn("g_TruthMemHeld = 0;", release[:400])

    def test_gc_collect_is_read_again_on_later_frames(self):
        command = function_body(self.research, "static void TruthMemCommand(")
        gc = command[command.index('verb == "gc"'):]
        self.assertLess(gc.index('CallBuiltin("gc_collect", {})'), gc.index('TruthMemStat("right after gc_collect")'))
        self.assertIn("{ 1ull, 60ull, 600ull }", gc, "gc_collect runs at the end of the frame, measured")
        frame = function_body(self.plugin, "void FrameCallback(")
        tick = frame.index("TruthMemTick(g_RuntimeFrame);")
        self.assertGreater(tick, frame.rindex("#ifndef FORGEPACT_RELEASE", 0, tick))
        self.assertLess(tick, frame.index("#endif", tick))

    def test_the_stat_reads_the_collector_and_private_bytes(self):
        stat = function_body(self.research, "static void TruthMemStat(")
        for read in ("GetProcessMemoryInfo(", '"gc_get_stats"', '"gc_is_enabled"', '"gc_get_target_frame_time"',
                     '"instance_number", { RValue(-3.0) }'):
            self.assertIn(read, stat)
        self.assertNotIn("sprintf", stat, "a label is formatted by concatenation, never into a fixed buffer")

    def test_logdrop_skips_item_truths_own_builds(self):
        body = function_body(self.research, "static void LogDrop(")
        self.assertTrue(body.lstrip().startswith("if (g_ItemTruthBuilding) return;"))
        build = function_body(self.plugin, "static RValue TruthBuildItem(")
        guard = build.index("struct Building { Building() { ++g_ItemTruthBuilding; } ~Building() { --g_ItemTruthBuilding; } } building;")
        self.assertLess(guard, build.index('"json_parse"'), "the counter covers the whole build")
        self.assertGreater(guard, build.index("#ifndef FORGEPACT_RELEASE"))
        self.assertNotIn("g_ItemTruthBuilding", function_body(self.release, "static RValue TruthBuildItem("))
        self.assertRegex(self.release, r"#define BP_LOGDROP\(a,b,c,d\) \(\(void\)0\)",
                         "the player build never calls LogDrop at all")


if __name__ == "__main__":
    unittest.main()
