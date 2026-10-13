"""HS Offline Tracker's sensor (`tracker`, ForgePact #185): the plugin wiring.

tests/test_tracker_mod_behavior.py runs the real core and its writer, and
tests/test_tracker_mod_exit_behavior.py the writer at ExitProcess. This file
pins what neither can see, in ModuleMain.cpp and MiningOreMod.hpp:

- the verb is a player command with its own early return;
- GoldLogAdd and EnemyAddStatistics are hooked by their SDK names through
  HookOneScript with `nativeOut`, each detour reading before the game's call
  and counting after it; no address constant anywhere in the tracker;
- ExperienceUpdate is hooked once, by MiningOreMod.hpp's InstallExperience,
  and both consumers (Extra rolls' pass-through and the tracker's count) sit
  in that one detour, the tracker's count silenced inside an extra run;
- the tracker is the fourth consumer of the shared LootGroundInit detour and
  notes creations from the shared Hook_CreateItemNew;
- it starts no thread, hooks no RoomGoto and runs nothing at process exit;
- it hooks only after `tracker 1` and a resolved local player;
- the mod state's `tracker` key and the stat line's keys.
"""
import re
import unittest
from pathlib import Path

from test_release_hook_contract import function_body, strip_comments, strip_research_blocks

ROOT = Path(__file__).resolve().parents[1]
PLUGIN = ROOT / "plugin" / "ModuleMain.cpp"
HEADER = ROOT / "plugin" / "include" / "ForgePact" / "TrackerMod.hpp"
MINING = ROOT / "plugin" / "include" / "ForgePact" / "MiningOreMod.hpp"
MINING_HARNESS = ROOT / "tests" / "mining_ore_harness.cpp"

ADAPTER_START = "// ---- HS Offline Tracker's sensor (TrackerMod.hpp): the adapter"
ADAPTER_END = "// ---- end of the tracker adapter"

#: The keys Live procedure 1 reads off `tracker stat`.
STAT_KEYS = ("install=", "gold=", "xp=", "kills=", "drops=", "gold-calls=", "xp-calls=", "kill-calls=",
             "gold-sum=", "xp-sum=", "kills-sum=", "drops-written=", "held-bag-drop=", "drops-untracked=",
             "lines=", "write-errors=", "file=")

#: A file-scope `static` object declaration in the adapter (not a function, not
#: a constexpr): its type and name.
STATIC_DECLARATION = re.compile(
    r"^static (?!constexpr|void|bool \w+\(|std::string \w+\(|const char\* \w+\(|RValue& \w+\(|"
    r"unsigned long long \w+\(|std::int64_t \w+\(|ForgePact::\w+::\w+ \w+\()([^;=(]+?)\s+(\w+)\s*(?:=|;)", re.M)
#: The kinds such an object may have: none has a destructor that does work.
ALLOWED_STATIC_KINDS = {"unsigned long long", "PFUNC_YYGMLScript", "const char*", "long long", "TrackerAdapterState&"}


def read(path):
    return path.read_text(encoding="utf-8").replace("\r\n", "\n")


class TrackerModContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        plugin = read(PLUGIN)
        cls.plugin = plugin
        cls.code = strip_comments(plugin)
        cls.player = strip_comments(strip_research_blocks(plugin))
        start = plugin.find(ADAPTER_START)
        end = plugin.find(ADAPTER_END)
        if start < 0 or end < start:
            raise AssertionError("the tracker adapter's start and end markers are missing from ModuleMain.cpp")
        cls.adapter = strip_comments(plugin[start:end])
        cls.header = strip_comments(read(HEADER))
        cls.mining = strip_comments(read(MINING))

    def body(self, signature, source=None):
        return function_body(source if source is not None else self.code, signature)

    def assertBefore(self, first, second, code, where):
        for text in (first, second):
            self.assertTrue(text in code, f"{where} lacks {text!r}")
        self.assertLess(code.index(first), code.index(second), f"{where}: {first!r} must come before {second!r}")

    # ---- the command ---------------------------------------------------------

    def test_the_command_is_a_player_command(self):
        start = self.player.index("kPlayerCommands = {")
        self.assertIn('"tracker"', self.player[start:self.player.index("};", start)])

    def test_the_verb_is_a_standalone_early_return_in_both_builds(self):
        run = function_body(self.player, "static void RunCommand(const std::string& line)")
        self.assertIn('if (lc == "tracker") { TrackerCommand(rest); return; }', run)
        self.assertNotIn('else if (lc == "tracker")', run)

    def test_every_reply_begins_with_on_or_off(self):
        command = self.body("static void TrackerCommand(const std::string& rest)")
        outs = re.findall(r"Out\(([^;]*)\);", command)
        self.assertEqual(4, len(outs), outs)
        for out in outs:
            self.assertTrue(out.startswith(("TrackerStatLine()", "g_Tracker.StatusLine()")), out)
        self.assertIn('return m_Enabled ? "tracker: on" : "tracker: off";', self.header)

    def test_the_stat_line_carries_the_keys_live_procedure_1_reads(self):
        stat = self.body("std::string StatLine(bool installTried, const std::string& fileName) const", self.header)
        summary = self.body("std::string RouteSummary() const", self.header)
        for key in STAT_KEYS:
            self.assertTrue(f" {key}" in stat or f'"{key}' in summary or f' {key}' in summary, key)
        self.assertIn("g_Tracker.StatLine(g_TrInstallTried,", self.body("static std::string TrackerStatLine()"))

    # ---- the routes ------------------------------------------------------------

    def test_gold_and_kills_by_sdk_name_with_both_routes(self):
        install = self.body("static void TrackerInstall()")
        self.assertIn('HookOneScript(SdkShortScriptName(HeroSiege::Scripts::gml_Script_GoldLogAdd), "fp_tracker_gold",',
                      install)
        self.assertIn("(PVOID)HookTrackerGold, &g_Orig_GoldLogAdd, &native);", install)
        self.assertIn('HookOneScript(SdkShortScriptName(HeroSiege::Scripts::gml_Script_EnemyAddStatistics), "fp_tracker_kills",',
                      install)
        self.assertIn("(PVOID)HookTrackerKill, &g_Orig_EnemyAddStatistics, &native);", install)
        # By SDK name only: no hand-typed script name anywhere in the tracker.
        for raw in ('"GoldLogAdd"', '"EnemyAddStatistics"', '"ExperienceUpdate"', '"gml_Script_'):
            self.assertNotIn(raw, self.adapter)

    def test_each_counter_detour_reads_before_and_counts_after_the_game(self):
        gold = self.body("static RValue& HookTrackerGold(CInstance* S, CInstance* O, RValue& R, int argc, RValue** A)")
        self.assertBefore("ForgePact::TrackerMod::CaptureGold(argc, args)", "RValue& result = g_Orig_GoldLogAdd(", gold,
                          "HookTrackerGold")
        self.assertBefore("RValue& result = g_Orig_GoldLogAdd(S, O, R, argc, A);", "g_Tracker.CountGold(amount);", gold,
                          "HookTrackerGold")
        kill = self.body("static RValue& HookTrackerKill(CInstance* S, CInstance* O, RValue& R, int argc, RValue** A)")
        self.assertBefore("ForgePact::TrackerMod::CaptureKill(argc, args)",
                          "RValue& result = g_Orig_EnemyAddStatistics(S, O, R, argc, A);", kill, "HookTrackerKill")
        self.assertBefore("RValue& result = g_Orig_EnemyAddStatistics(S, O, R, argc, A);", "g_Tracker.CountKill(id);", kill,
                          "HookTrackerKill")
        # Off, each passes straight through.
        self.assertIn("if (!g_Tracker.Enabled()) return g_Orig_GoldLogAdd(S, O, R, argc, A);", gold)
        self.assertIn("if (!g_Tracker.Enabled()) return g_Orig_EnemyAddStatistics(S, O, R, argc, A);", kill)

    def test_no_address_constant_in_the_tracker(self):
        for name, code in (("the adapter", self.adapter), ("TrackerMod.hpp", self.header)):
            self.assertIsNone(re.search(r"\bk\w*Rva\w*\b|GetModuleHandle|\b0x[0-9A-Fa-f]{5,}\b", code), name)

    # ---- ExperienceUpdate: one detour, two consumers ------------------------------

    def test_experience_update_is_hooked_once(self):
        # Neither ModuleMain.cpp nor the rolls' own loop hooks it: only
        # InstallExperience, once a session.
        self.assertIsNone(re.search(r"HookOneScript\w*\([^;]*ExperienceUpdate", self.code))
        install = self.body("inline bool InstallExperience()", self.mining)
        self.assertEqual(1, len(re.findall(r"\bHookOneScript\(", install)))
        self.assertIn("(PVOID)kSilencedHooks[kExperienceScript]", install)
        self.assertBefore("if (!experienceInstallTried) {", "HookOneScript(", install, "InstallExperience")
        self.assertIn("inline constexpr size_t kExperienceScript = 1;", self.mining)
        self.assertIn("gml_Script_ExperienceUpdate), \"fp_mining_rolls_xp\"", self.mining)
        passthrough = self.body("inline std::string HookPassThrough()", self.mining)
        self.assertBefore("if (i == kExperienceScript) {", "if (!InstallExperience())", passthrough, "HookPassThrough")
        self.assertBefore("continue;", "const bool ok = HookOneScript(", passthrough, "HookPassThrough")
        self.assertIn("HookSilenced<0>, HookExperience, HookSilenced<2>, HookSilenced<3> };", self.mining)
        # The tracker installs it through the same function.
        self.assertIn("ForgePact::MiningOre::InstallExperience();", self.body("static void TrackerInstall()"))

    def test_both_consumers_sit_in_that_one_detour(self):
        hook = self.body("inline RValue& HookExperience(CInstance* S, CInstance* O, RValue& R, int argc, RValue** A)",
                         self.mining)
        # Silenced inside an extra run: no game call and no count.
        self.assertBefore("if (inExtraRun) { ++script.silenced; ++silencedCalls; return R; }", "capture(argc, A)", hook,
                          "HookExperience")
        self.assertBefore("const ExperienceCapture captured = capture(argc, A);",
                          "RValue& result = script.original(S, O, R, argc, A);", hook, "HookExperience")
        self.assertBefore("RValue& result = script.original(S, O, R, argc, A);", "count(captured, result);", hook,
                          "HookExperience")
        install = self.body("static void TrackerInstall()")
        self.assertIn("ForgePact::MiningOre::experienceCapture = &TrXpCapture;", install)
        self.assertIn("ForgePact::MiningOre::experienceCount = &TrXpCount;", install)
        self.assertBefore("ForgePact::MiningOre::experienceCount = &TrXpCount;",
                          "ForgePact::MiningOre::InstallExperience();", install, "TrackerInstall")
        count = self.body("static void TrXpCount(const ForgePact::MiningOre::ExperienceCapture& c, const RValue& result)")
        self.assertIn("result.m_Kind == VALUE_BOOL && !result.ToBoolean()", count)

    def test_miningrolls_stays_available_with_the_tracker_installed_first(self):
        harness = read(MINING_HARNESS)
        self.assertIn('"target/tracker_first_then_rolls_share_one_native_xp_detour"', harness)
        self.assertIn('"target/a_table_only_shared_xp_detour_keeps_rolls_unavailable"', harness)
        behavior = read(ROOT / "tests" / "test_mining_ore_behavior.py")
        self.assertIn("PASS target/tracker_first_then_rolls_share_one_native_xp_detour", behavior)

    # ---- drops ---------------------------------------------------------------------

    def test_the_fourth_consumer_of_the_shared_loot_ground_init_detour(self):
        hook = self.body("static RValue& HookHiddenLootInit(CInstance* S, CInstance* O, RValue& R, int argc, RValue** A)")
        self.assertEqual(1, hook.count("TrackerOnInit(S, argc, A);"))
        self.assertBefore("g_Orig_LootGroundInit(S, O, R, argc, A)", "TrackerOnInit(S, argc, A);", hook, "HookHiddenLootInit")
        self.assertBefore("FlightCharmOnGroundInit(argc, A);", "TrackerOnInit(S, argc, A);", hook, "HookHiddenLootInit")
        self.assertEqual(1, len(re.findall(r"HookOneScript\w*\([^;]*LootGroundInit\b", self.code)))
        self.assertIn("if (!g_HiddenLootInstallTried) HiddenLootInstall();", self.body("static void TrackerInstall()"))
        # Inside the call: the handles made durable, nothing of the item.
        on_init = self.body("static void TrackerOnInit(CInstance* S, int argc, RValue** A)")
        self.assertIn("if (!g_Tracker.Enabled()) return;", on_init)
        self.assertIn("LaDurable(", on_init)
        for forbidden in ("DecideDrop", "itemInfoStruct", "LaRarity", "LaField", "CallBuiltin"):
            self.assertNotIn(forbidden, on_init, forbidden)

    def test_creations_are_noted_from_the_shared_create_item_new_hook(self):
        start = self.plugin.index("#define ITEM_CREATE_HOOK(NAME)")
        macro = self.plugin[start:self.plugin.index("ITEM_CREATE_HOOK(CreateItemNew)", start)]
        note = "if (_final && g_Tracker.Enabled()) TrackerNoteCreated(argc, A, _res);"
        self.assertEqual(1, macro.count(note))
        self.assertBefore("_resp = &g_Orig_##NAME(S, O, R, argc, A);", note, macro, "ITEM_CREATE_HOOK")
        self.assertBefore(note, "if (g_GemTableBuilding) return _res;", macro, "ITEM_CREATE_HOOK")
        self.assertLess(self.plugin.index("static void TrackerNoteCreated(int argc, RValue** A, const RValue& result);"), start)
        create = self.body("static void TrInstallCreateHook()")
        self.assertBefore("if (g_Orig_CreateItemNew) {", "HookOneScript(", create, "TrInstallCreateHook")
        self.assertIn('HookOneScript(SdkShortScriptName(HeroSiege::Scripts::gml_Script_CreateItemNew), "fp_tracker_new",', create)
        self.assertIn("(PVOID)Hook_CreateItemNew, &g_Orig_CreateItemNew, &native);", create)

    def test_the_guard_is_never_compiled_out_of_the_plugin(self):
        self.assertNotIn("FORGEPACT_TRACKER_CREATION_GUARD", self.code)
        self.assertIn("#define FORGEPACT_TRACKER_CREATION_GUARD 1", read(HEADER))

    # ---- no thread, no RoomGoto, nothing at exit -------------------------------------

    def test_no_thread_and_no_room_goto_detour(self):
        for name, code in (("the adapter", self.adapter), ("TrackerMod.hpp", self.header)):
            for forbidden in ("std::thread", "std::jthread", "CreateThread", "_beginthread", "ExitSafeThread", "RoomGoto"):
                self.assertNotIn(forbidden, code, f"{name}: {forbidden}")
        poll = self.body("static void TrPollRoom()")
        self.assertIn('GetBuiltin("room", nullptr, NULL_INDEX, roomV)', poll)
        self.assertIn('CallBuiltin("room_get_name", { roomV })', poll)

    def test_nothing_of_the_tracker_runs_at_process_exit(self):
        self.assertIn("static ForgePact::TrackerMod& g_Tracker = *new ForgePact::TrackerMod();", self.code)
        self.assertIn("static TrackerAdapterState& g_TrState = *new TrackerAdapterState();", self.code)
        self.assertIn("static ForgePact::TrackerJournalFile g_TrFile;", self.code)
        self.assertIn("static_assert(std::is_trivially_destructible_v<TrackerJournalFile>,", self.header)
        self.assertNotRegex(self.header, r"~\w+\s*\(")
        self.assertNotRegex(self.header, r"CloseHandle")
        # Every other file-scope object of the adapter is a scalar, a pointer or
        # a reference: nothing with a destructor that does work.
        declarations = STATIC_DECLARATION.findall(self.adapter)
        for kind, name in declarations:
            self.assertIn(kind.strip(), ALLOWED_STATIC_KINDS, f"{name} is a static {kind.strip()}")
        self.assertTrue(declarations, "the scan found no declaration at all")
        for exit_hook in ("atexit", "DLL_PROCESS_DETACH"):
            self.assertNotIn(exit_hook, self.adapter)

    def test_the_static_scan_sees_an_object_with_a_destructor(self):
        # Positive control for the scan above: the shapes it must refuse.
        found = STATIC_DECLARATION.findall(
            'static std::vector<LaPending> g_TrPending;\nstatic std::string g_TrRoom = "";\n'
            "static bool TrHelper(int x)\n{\n}\nstatic long long g_TrCount = 0;\n")
        self.assertEqual([("std::vector<LaPending>", "g_TrPending"), ("std::string", "g_TrRoom"),
                          ("long long", "g_TrCount")], found)

    def test_the_writer_appends_whole_lines_with_shared_read_and_delete(self):
        self.assertIn('"session-%lu-%llu.ndjson"', self.header)
        self.assertIn('ipcDir + "\\\\tracker"', self.header)
        self.assertIn("FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_DELETE", self.header)
        self.assertIn("g_TrFile.Open(IPC_DIR, GetCurrentProcessId(), TrUnixMs())", self.adapter)
        self.assertIn('g_Tracker.SetIdentity(GetCurrentProcessId(), std::string("forgepact-") + FORGEPACT_VERSION);',
                      self.body("static void TrackerInstall()"))

    # ---- arming -----------------------------------------------------------------------

    def test_hooks_go_in_only_after_tracker_1_and_a_resolved_player(self):
        ready = self.body("static void TrInstallIfReady()")
        self.assertIn("HhResolveLocalPlayer(player)", ready)
        self.assertIn("if (g_Tracker.ShouldInstall(g_Setup, havePlayer, g_TrInstallTried)) TrackerInstall();", ready)
        # TrInstallIfReady is the only caller of the install.
        self.assertEqual(1, len(re.findall(r"\bTrackerInstall\(\);", self.code)))
        self.assertIn("g_TrInstallTried = true;", self.body("static void TrackerInstall()"))
        tick = [l.strip() for l in self.body("static void TrackerTick()").splitlines() if l.strip()]
        self.assertEqual("if (!g_Tracker.Enabled()) return;", tick[0])
        self.assertIn("if (!g_TrInstallTried && ForgePact::TrackerMod::LooksForPlayer(g_TrArmedFrames++)) TrInstallIfReady();",
                      tick[1])
        frame = self.body("void FrameCallback(FWFrame& FrameContext)")
        self.assertEqual(1, frame.count("TrackerTick();"))
        self.assertBefore("if (g_Setup) LootAnnounceTick();", "if (g_Setup) TrackerTick();", frame, "FrameCallback")
        self.assertBefore("if (g_Setup) TrackerTick();", "if (g_Setup) HiddenLootTick();", frame, "FrameCallback")
        # Never at setup: nothing in InstallHook mentions the tracker.
        self.assertNotIn("Tracker", self.body("static void InstallHook()"))

    # ---- the mod state ------------------------------------------------------------------

    def test_the_mod_state_has_a_tracker_entry(self):
        flush = self.plugin[self.plugin.index("static void FlushModState(uint32_t frame)"):]
        flush = flush[:flush.index("\n}\n")]
        self.assertIn('body += ",\\"tracker\\":{\\"on\\":"; body += g_Tracker.Enabled() ? "true" : "false";', flush)
        for field in ("install", "gold", "xp", "kills", "drops", "file", "writeErrors"):
            self.assertIn(f'\\"{field}\\":', flush)


if __name__ == "__main__":
    unittest.main()
