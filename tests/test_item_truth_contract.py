"""Source contracts for Item Truth (`plugin/ModuleMain.cpp` + ItemTruth.hpp).

Item Truth journals every item the game finishes building so the Item Editor
can show the game's own numbers instead of replaying them. What must never
change is pinned here: it records only the outermost CreateItemNew return,
after the Custom Forge dressing; the game thread never touches the disk for
it; nothing is hooked unless the Item Editor asked; and the old itemstats.json
snapshot is taken only once CreateItemNew is done (it used to be taken inside
CreateItemInit/GenerateItemRandomStats and missed the socket count).
Source contracts rather than a harness for the hook itself: what the game
hands the hook is game behaviour; tests/item_truth_harness.cpp covers the
plain-C++ half.
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
HEADER_PATH = PROJECT_ROOT / "plugin" / "include" / "ForgePact" / "ItemTruth.hpp"
DISK_IO = re.compile(r"ofstream|ifstream|fopen|CreateFile|WriteFile|std::filesystem::(?!path)")


class ItemTruthContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.plugin = PLUGIN_PATH.read_text(encoding="utf-8")
        cls.release = strip_comments(strip_research_blocks(cls.plugin))
        cls.header = HEADER_PATH.read_text(encoding="utf-8")

    def create_hook_macro(self):
        start = self.release.index("#define ITEM_CREATE_HOOK(NAME)")
        end = self.release.index("ITEM_CREATE_HOOK(CreateItemNew)", start)
        return self.release[start:end]

    def test_only_the_outermost_create_item_new_is_recorded_after_the_dressing(self):
        macro = self.create_hook_macro()
        self.assertIn('constexpr bool _final = std::string_view(#NAME) == std::string_view("CreateItemNew");', macro)
        self.assertIn("TruthDepthGuard _depth(_final);", macro)
        self.assertIn("const bool _outermost = _final && g_TruthDepth == 0;", macro)
        guard = macro.index("TruthDepthGuard _depth(_final);")
        original = macro.index("_resp = &g_Orig_##NAME(S, O, R, argc, A);")
        self.assertLess(guard, original, "the depth counter must cover the original call")
        native = macro.index("ItemTruthNativeSnapshot(_res)")
        dressing = macro.index("CustomForgePostProcess(_res, argc, A, _final);")
        capture = macro.index("if (_outermost) ItemTruthCapture(_res, _native);")
        self.assertLess(native, dressing, "the native stats are taken before the Custom Forge dressing")
        self.assertLess(dressing, capture, "the record is what the game shows: after the dressing")

    def test_the_depth_guard_counts_only_create_item_new(self):
        guard = self.release[self.release.index("struct TruthDepthGuard"):]
        guard = guard[:guard.index("};") + 2]
        self.assertIn("if (on) ++g_TruthDepth;", guard)
        self.assertIn("if (on) --g_TruthDepth;", guard)
        self.assertIn("static thread_local int g_TruthDepth = 0;", self.release)

    def test_capture_reads_the_item_and_queues_one_line_without_disk_io(self):
        body = function_body(self.release, "static void ItemTruthCapture(")
        self.assertTrue(body.lstrip().startswith("if (!g_TruthOn || !g_TruthJournal || item.m_Kind != VALUE_OBJECT) return;"))
        self.assertIn('field("itemDefinitionStruct")', body)
        self.assertIn('field("itemStatStruct")', body)
        self.assertIn("if (r.timestamp.empty()) return;", body)
        self.assertIn("g_TruthSeen.Insert(", body)
        self.assertIn("g_TruthJournal->Enqueue(std::move(line))", body)
        self.assertIsNone(DISK_IO.search(body), "the game thread must not write the journal itself")
        for helper in ("static std::string TruthStringify(", "static std::string ItemTruthNativeSnapshot("):
            self.assertIsNone(DISK_IO.search(function_body(self.release, helper)), helper)

    def test_nothing_is_hooked_unless_the_item_editor_asked(self):
        body = function_body(self.release, "static void InstallItemTruth(")
        requested = body.index("ForgePact::ItemTruth::CaptureRequested(root)")
        hook = body.index('HookOneScript("CreateItemNew", "fp_itemtruth_new", (PVOID)Hook_CreateItemNew, &g_Orig_CreateItemNew);')
        self.assertLess(requested, hook)
        self.assertIn("if (!requested) {", body[requested:hook])
        self.assertRegex(body, r'if \(!g_Orig_CreateItemNew\)\s*HookOneScript\("CreateItemNew", "fp_itemtruth_new"',
                         "an existing CreateItemNew hook (the Custom Forge's) is reused, never stacked")
        self.assertIn("g_TruthOn = false;", body, "withdrawing the request pauses the capture")

    def test_setup_and_the_frame_tick_reach_it_in_the_player_build(self):
        install = function_body(self.release, "static void InstallHook(")
        self.assertIn("InstallCustomForgeItemHooks();\n    InstallItemTruth();", install.replace("\r", ""))
        frame = function_body(self.release, "void FrameCallback(")
        self.assertIn("if (g_Setup) { ItemTruthTick(fc); ItemTruthEvalTick(fc); }", frame)
        tick = function_body(self.release, "static void ItemTruthTick(")
        self.assertIn("if (frame % 600 == 0) InstallItemTruth();", tick)

    def test_the_journal_lives_on_the_heap_for_the_whole_process(self):
        self.assertIn("static ForgePact::ItemTruth::Journal* g_TruthJournal = nullptr;", self.release)
        self.assertNotRegex(self.release, r"static\s+ForgePact::ItemTruth::Journal\s+\w+\s*;")
        self.assertIn("new ForgePact::ItemTruth::Journal()", function_body(self.release, "static void InstallItemTruth("))

    def test_item_stats_json_waits_for_create_item_new_to_finish(self):
        body = function_body(self.release, "static bool TryApplyCustomForge(")
        block = self.braced_block(body, body.index("if (finalPass) {") + len("if (finalPass) "))
        self.assertIn("RecordItemStats(*candidate, stats);", block)
        self.assertEqual(1, body.count("RecordItemStats("))

    @staticmethod
    def braced_block(source, brace):
        depth = 0
        for index in range(brace, len(source)):
            if source[index] == "{":
                depth += 1
            elif source[index] == "}":
                depth -= 1
                if depth == 0:
                    return source[brace:index + 1]
        raise AssertionError("unterminated block")

    def test_evaluation_builds_through_the_games_save_loader_and_never_drops_the_item(self):
        body = function_body(self.release, "static bool TruthEvalOne(")
        self.assertIn('CallBuiltinEx(parsed, "json_parse", g, g, { RValue(entry.json) })', body)
        self.assertIn('"gml_Script_InitItemFromJson", g, g, { parsed, RValue(entry.key) }', body)
        for forbidden in ("LootGroundCreate", "InventoryGridAdd", "GridAddItem", "SaveGame", "ds_list_add"):
            self.assertNotIn(forbidden, body)

    def test_evaluation_runs_only_while_capturing_and_within_a_frame_budget(self):
        tick = function_body(self.release, "static void ItemTruthEvalTick(")
        self.assertTrue(tick.lstrip().startswith("if (!g_TruthOn || !g_TruthJournal) return;"))
        self.assertIn("kTruthEvalBudgetSeconds", tick)
        self.assertIn("kTruthEvalMaxPerFrame", tick)
        self.assertIn("static constexpr double kTruthEvalBudgetSeconds = 0.004;", self.release)
        claim = tick.index("std::filesystem::rename(next, working, ec);")
        self.assertLess(claim, tick.index("ParseRequest("), "a request is claimed before it is read")
        tag = tick.index("g_TruthEvalRequest = g_TruthEval.id;")
        call = tick.index("ok = TruthEvalOne(entry);")
        clear = tick.index("g_TruthEvalRequest.clear();", call)
        self.assertLess(tag, call)
        self.assertLess(call, clear, "only the requested item carries the tag")
        self.assertIn("std::filesystem::remove(g_TruthEval.file, ec);", tick)
        self.assertIn('stopped.replace_extension(L".stopped");', tick, "a failing request is set aside, not retried")
        frame = function_body(self.release, "void FrameCallback(")
        self.assertIn("if (g_Setup) { ItemTruthTick(fc); ItemTruthEvalTick(fc); }", frame)

    def test_an_unfinished_check_is_never_resumed_on_its_own(self):
        install = function_body(self.release, "static void InstallItemTruth(")
        self.assertIn('ForgePact::ItemTruth::AbandonWorking(root / L"requests")', install)
        self.assertNotIn("ResumeWorking", self.release)
        self.assertNotIn("ResumeWorking", self.header)

    def test_evaluated_items_are_tagged_and_always_written(self):
        body = function_body(self.release, "static void ItemTruthCapture(")
        self.assertIn("const bool evaluating = !g_TruthEvalRequest.empty();", body)
        self.assertIn("&& !evaluating) return;", body)
        self.assertIn('if (evaluating) { r.source = "eval"; r.request = g_TruthEvalRequest; }', body)

    def test_the_header_keeps_all_disk_io_on_the_writer_thread(self):
        enqueue = function_body(self.header, "bool Enqueue(std::string line)")
        self.assertIsNone(DISK_IO.search(enqueue))
        self.assertIn("queue_.size() >= kMaxQueuedLines", enqueue)
        self.assertIn('L"capture.request"', self.header)
        self.assertIn('L"Hero_Siege" / L"itemtruth"', self.header)


if __name__ == "__main__":
    unittest.main()
