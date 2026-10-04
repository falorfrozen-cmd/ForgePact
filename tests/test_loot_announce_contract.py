"""Loot announcements (`lootann`): the plugin wiring and the rules the core must keep.

tests/test_loot_announce_behavior.py runs the real core against controlled
drops. This file pins what that harness cannot see: the announced rarity set
in the header is exactly {9, 7, 10}; the verb is a player command with its own
early return; LootGroundInit is named in exactly one HookOneScript call in the
player build, inside HiddenLootInstall, and the announcement installs through
it and is handed the call by the shared detour after the game's original; the
LootGroundDrop detour holds the bag-drop window around its original; the
frame tick costs nothing while off and runs before hidden loot's; the rarity is
read by name from itemInfoStruct["27"]; the research instrument `lootannprobe`
and its count-only hook table never reach the player build; the shipped sink
is the one the research doc's `announce-route:` names (`server`, picked by
Live procedure 1); and the modstate object and the line formats the live
operator reads.
"""
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PLUGIN = ROOT / "plugin" / "ModuleMain.cpp"
HEADER = ROOT / "plugin" / "include" / "ForgePact" / "LootAnnounceMod.hpp"
RESEARCH = ROOT / "docs" / "loot-announcement-research.md"

ADAPTER_START = "// ---- Loot announcements (LootAnnounceMod.hpp): the adapter"
ADAPTER_END = "// ---- end of the loot announcement adapter"

# The route Live procedure 1 picked (`announce-route:` in the research doc's
# "Route"): `server`, 2026-10-04. A later route changes this together with the
# header's kShippedSink.
EXPECTED_ROUTE = "server"

# The research instrument's names, none of which the player build may carry.
RESEARCH_ONLY_NAMES = ("lootannprobe", "LootAnnProbeCommand", "g_LaProbeRows", "LaProbeDetour", "LaProbeNoteInit",
                       "LaProbeNoteDrop", "LaProbeAttach", "LaProbePlace", "LaProbeTry", "LaProbeSay")


def _body(source: str, signature: str) -> str:
    start = source.rfind(signature)
    if start < 0:
        raise AssertionError(f"{signature} not found")
    brace = source.find("{", start)
    depth = 0
    for index in range(brace, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[brace + 1:index]
    raise AssertionError(f"unterminated body for {signature}")


def _code(source: str) -> str:
    source = re.sub(r"/\*.*?\*/", "", source, flags=re.S)
    return re.sub(r"//[^\n]*", "", source)


def _player_build(source: str) -> str:
    """What the player build compiles (FORGEPACT_RELEASE defined), comments
    stripped. Evaluates both spellings of research code: `#ifndef
    FORGEPACT_RELEASE ... #endif` and the `#else` half of `#ifdef
    FORGEPACT_RELEASE` (tests/test_release_hook_contract.py's rule)."""
    kept, stack = [], []
    for line in source.split("\n"):
        stripped = line.strip()
        if stripped.startswith("#ifdef FORGEPACT_RELEASE"):
            stack.append([True, True])
        elif stripped.startswith("#ifndef FORGEPACT_RELEASE"):
            stack.append([True, False])
        elif stripped.startswith("#if"):
            stack.append([False, True])
        elif stripped.startswith("#else") and stack:
            if stack[-1][0]:
                stack[-1][1] = not stack[-1][1]
        elif stripped.startswith("#endif") and stack:
            stack.pop()
        elif all(active for _, active in stack):
            kept.append(line)
    return _code("\n".join(kept))


def _research_names_in(text: str) -> list:
    return [name for name in RESEARCH_ONLY_NAMES if re.search(rf"\b{name}\b", text)]


def _loot_ground_init_hook_calls(text: str) -> list:
    return re.findall(r"HookOneScript\w*\([^;]*LootGroundInit\b", text)


class LootAnnounceHeaderTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.code = _code(HEADER.read_text(encoding="utf-8").replace("\r\n", "\n"))

    def test_the_announced_rarity_set_is_heroic_angelic_unholy(self):
        constants = dict((name, int(value)) for name, value in
                         re.findall(r"static constexpr int (k\w+) = (-?\d+);", self.code))
        members = re.search(r"kAnnouncedRarities\s*=\s*\{([^}]*)\}", self.code)
        self.assertIsNotNone(members, "kAnnouncedRarities is not an enumerable list in the header")
        names = [m.strip() for m in members.group(1).split(",") if m.strip()]
        values = [constants[n] if n in constants else int(n) for n in names]
        self.assertEqual(len(values), 3, values)
        self.assertEqual(set(values), {9, 7, 10})

    def test_it_starts_off_and_counts_what_it_holds_back(self):
        self.assertIn("bool m_Enabled = false;", self.code)
        for counter in ("seen", "announced", "heldRarity", "heldNoRarity", "heldDuplicate", "heldBagDrop", "sinkRefused"):
            self.assertRegex(self.code, rf"long long {counter} = 0;", counter)

    def test_it_is_game_independent(self):
        for forbidden in ("RValue", "CInstance", "g_Yytk", "YYTK", "#include <Windows", "Aurie"):
            self.assertNotIn(forbidden, self.code, forbidden)

    def test_the_shipped_sink_is_the_route_the_research_doc_names(self):
        shipped = re.search(r"static constexpr Sink kShippedSink = Sink::(\w+);", self.code)
        self.assertIsNotNone(shipped)
        names = dict(re.findall(r"case Sink::(\w+): return \"(\w+)\";", self.code))
        self.assertEqual(set(names.values()), {"method", "netsend", "chatadd", "server"})
        self.assertEqual(names[shipped.group(1)], EXPECTED_ROUTE)
        # Once Live procedure 1 picked one, the research doc says which, and it
        # must be this one.
        if RESEARCH.is_file():
            routes = re.findall(r"announce-route:\s*`?(\w[\w-]*)", RESEARCH.read_text(encoding="utf-8"))
            picked = [r for r in routes if r in {"method", "netsend", "chatadd", "server", "not-observed"}]
            if picked:
                self.assertEqual(picked[-1], EXPECTED_ROUTE, picked)

    def test_the_line_formats(self):
        self.assertIn('"lootann: on"', self.code)
        self.assertIn('"lootann: off"', self.code)
        stat = _body(self.code, "std::string StatLine() const")
        fields = re.findall(r'" ([a-z-]+)="', stat)
        self.assertEqual(fields, ["route", "seen", "announced", "held-rarity", "held-no-rarity", "held-duplicate",
                                  "held-bag-drop", "sink-refused", "remembered"])


class LootAnnouncePluginWiringTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.plugin = PLUGIN.read_text(encoding="utf-8").replace("\r\n", "\n")
        cls.player = _player_build(cls.plugin)
        start = cls.plugin.find(ADAPTER_START)
        end = cls.plugin.find(ADAPTER_END)
        if start < 0 or end < start:
            raise AssertionError("the loot announcement adapter's start and end markers are missing from ModuleMain.cpp")
        cls.adapter = _code(cls.plugin[start:end])

    def test_the_player_build_accepts_the_verb(self):
        start = self.plugin.index("kPlayerCommands = {")
        block = self.plugin[start:self.plugin.index("};", start)]
        self.assertIn('"lootann"', block)

    def test_the_verb_is_a_standalone_early_return(self):
        run = _body(self.player, "static void RunCommand(const std::string& line)")
        self.assertIn('if (lc == "lootann") { LootAnnounceCommand(rest); return; }', run)
        self.assertNotIn('else if (lc == "lootann")', run)
        self.assertIn("static void LootAnnounceCommand(", self.player)

    def test_loot_ground_init_is_hooked_once_through_hidden_loot(self):
        calls = _loot_ground_init_hook_calls(self.player)
        self.assertEqual(len(calls), 1, calls)
        install = _code(_body(self.plugin, "static void HiddenLootInstall()"))
        self.assertEqual(len(_loot_ground_init_hook_calls(install)), 1)
        # The research build adds none either: its row counts through the
        # shared detour.
        self.assertEqual(len(_loot_ground_init_hook_calls(_code(self.plugin))), 1)
        # The announcement installs through HiddenLootInstall.
        lai = _code(_body(self.plugin, "static void LootAnnounceInstall()"))
        self.assertIn("if (!g_HiddenLootInstallTried) HiddenLootInstall();", lai)

    def test_the_shared_detour_hands_the_call_on_after_the_original(self):
        hook = _code(_body(self.plugin, "static RValue& HookHiddenLootInit(CInstance* S, CInstance* O, RValue& R, int argc, RValue** A)"))
        self.assertEqual(hook.count("LootAnnounceOnInit(S, argc, A);"), 1)
        self.assertLess(hook.index("g_Orig_LootGroundInit(S, O, R, argc, A)"), hook.index("LootAnnounceOnInit(S, argc, A);"))
        self.assertLess(hook.index(".OnInit("), hook.index("LootAnnounceOnInit(S, argc, A);"))
        on_init = _code(_body(self.plugin, "static void LootAnnounceOnInit(CInstance* S, int argc, RValue** A)"))
        statements = [s.strip() for s in on_init.split("\n") if s.strip() and not s.strip().startswith("#")]
        self.assertEqual(statements[1] if statements[0].startswith("LaProbeNoteInit") else statements[0],
                         "if (!g_LootAnnounce.Enabled()) return;")
        # Inside the call: the handles are reduced and the bag-drop window
        # noted; no item is read, nothing is decided or said there.
        self.assertIn("g_LootAnnounce.BagDropActive()", on_init)
        for forbidden in ("Decide(", "LootAnnounceSink(", "itemInfoStruct", "ChatAdd"):
            self.assertNotIn(forbidden, on_init, forbidden)

    def test_the_bag_drop_detour_holds_the_window_around_its_original(self):
        lai = _code(_body(self.plugin, "static void LaInstallDropHook()"))
        self.assertIn("HookOneScript(SdkShortScriptName(HeroSiege::Scripts::gml_Script_LootGroundDrop)", lai)
        self.assertIn("&g_Orig_LootGroundDropLa, &native)", lai)
        drop = _code(_body(self.plugin, "static RValue& LaHookLootGroundDrop(CInstance* S, CInstance* O, RValue& R, int argc, RValue** A)"))
        self.assertIn("ForgePact::LootAnnounceMod::BagDropScope window(g_LootAnnounce);", drop)
        self.assertLess(drop.index("BagDropScope window"), drop.index("g_Orig_LootGroundDropLa(S, O, R, argc, A)"))

    def test_the_tick_costs_nothing_while_off_and_runs_before_hidden_loot(self):
        tick = [l.strip() for l in _code(_body(self.plugin, "static void LootAnnounceTick()")).splitlines() if l.strip()]
        self.assertEqual(tick[0], "if (!g_LootAnnounce.Enabled()) return;")
        frame = _code(_body(self.plugin, "void FrameCallback(FWFrame& FrameContext)"))
        self.assertEqual(frame.count("LootAnnounceTick();"), 1)
        self.assertIn("if (g_Setup) LootAnnounceTick();", frame)
        self.assertLess(frame.index("if (g_Setup) LootAnnounceTick();"), frame.index("if (g_Setup) HiddenLootTick();"))

    def test_the_rarity_is_read_by_name_from_key_27(self):
        rarity = _code(_body(self.plugin, "static int LaRarity(const RValue& item)"))
        self.assertIn('LaField(item, "itemInfoStruct", info)', rarity)
        self.assertIn('LaField(info, "27", code)', rarity)
        self.assertIn("kRarityUnread", rarity)
        for forbidden in ('"c"', "level", "relicLevel", '"b"'):
            self.assertNotIn(forbidden, rarity, forbidden)
        process = _code(_body(self.plugin, "static void LaProcess(const LaPending& p)"))
        self.assertIn("HeroSiege::Player::kGroundItemInstanceField", process)
        self.assertIn("HeroSiege::Objects::GameObject::Loot_Ground_obj", process)
        self.assertIn("g_LootAnnounce.Decide(LaRarity(item), p.bagDrop, (int64_t)id, LaTimeStamp(item))", process)
        # The kind never decides whether the read happens: no instance-kind gate.
        self.assertNotIn("IsInstanceHandle", process)

    def test_the_four_sinks_are_by_sdk_name_and_the_mod_runs_the_shipped_one(self):
        sink = _code(_body(self.plugin, "static bool LootAnnounceSink(const RValue& item, CInstance* lootInst)"))
        self.assertIn("LaRunSink(ForgePact::LootAnnounceMod::kShippedSink, kLaNetSendA0IsPlayer, item, lootInst, nullptr)", sink)
        run = _code(_body(self.plugin, "static LaSinkRun LaRunSink("))
        for script in ("gml_Script_NetworkSendChatMessageIngame", "gml_Script_GetItemDropMessage",
                       "gml_Script_ChatAddMessage", "gml_Script_ChatAddServerMessage"):
            self.assertIn(f"SdkShortScriptName(HeroSiege::Scripts::{script})", run, script)
        self.assertIn("InvokeMethodValue(lootInst, lootInst, method, {}, run.ret)", run)
        self.assertIn("kLaClosureShort", self.adapter)
        self.assertIn("SdkShortScriptName(HeroSiege::Scripts::gml_Script_anon_1138_gml_Object_Loot_Ground_obj_Create_0)",
                      self.adapter)
        # A refusal is logged naming the field, and counted.
        self.assertIn('"lootann: no line (sink "', sink)
        process = _code(_body(self.plugin, "static void LaProcess(const LaPending& p)"))
        self.assertIn("g_LootAnnounce.NoteSinkRefused();", process)

    def test_no_address_no_destroy_in_the_adapter(self):
        self.assertIsNone(re.search(r"\bk\w*Rva\w*\b", self.adapter))
        self.assertIsNone(re.search(r"\(\s*char\s*\*\s*\)\s*\w+\s*\+\s*(?:0x[0-9A-Fa-f]+)", self.adapter))
        for forbidden in ("instance_destroy", "GetModuleHandle", "MmCreateHook", "reinterpret_cast", "instance_deactivate"):
            self.assertNotIn(forbidden, self.adapter, forbidden)

    def test_the_research_instrument_never_reaches_the_player_build(self):
        self.assertEqual(_research_names_in(self.player), [])
        # ...and is really there in the research build.
        code = _code(self.plugin)
        for name in ("LootAnnProbeCommand", "g_LaProbeRows", "LaProbeDetour"):
            self.assertIn(name, code, name)
        self.assertIn('if (lc == "lootannprobe") { LootAnnProbeCommand(rest); return; }', code)
        # Negative control: the same check finds a name left outside a block.
        leaked = "static void LootAnnProbeCommand(const std::string& rest) {}\n" \
                 "#ifndef FORGEPACT_RELEASE\nstatic LaProbeRow g_LaProbeRows[1];\n#endif\n"
        self.assertEqual(_research_names_in(_player_build(leaked)), ["LootAnnProbeCommand"])

    def test_the_probe_rows_are_the_static_search_table_by_sdk_name(self):
        code = _code(self.plugin)
        start = code.index("static LaProbeRow g_LaProbeRows[] = {")
        table = code[start:code.index("};", start)]
        scripts = re.findall(r"SdkShortScriptName\(HeroSiege::Scripts::(\w+)\)", table)
        self.assertEqual(scripts, [
            "gml_Script_anon_1138_gml_Object_Loot_Ground_obj_Create_0", "gml_Script_GetRareDropAnnouncement",
            "gml_Script_NetworkSendChatMessageIngame", "gml_Script_GetItemDropMessage", "gml_Script_ChatAddMessage",
            "gml_Script_ChatAddServerMessage", "gml_Script_ChatAddIngameMessageFiltered", "gml_Script_CA_chatIngame",
            "gml_Script_PacketSend", "gml_Script_ChatSendServerMessage", "gml_Script_ReportClient",
            "gml_Script_LootGroundInit", "gml_Script_LootGroundDrop", "gml_Script_LootGroundCreateFromItem",
            "gml_Script_anon_6032_gml_Object_Loot_Ground_obj_Create_0",
            "gml_Script_anon_11081_gml_Object_Loot_Ground_obj_Create_0",
        ])
        # LootGroundInit and LootGroundDrop count through the adapter's hooks.
        self.assertIn("LaShared::Init", table)
        self.assertIn("LaShared::Drop", table)
        # A row held by another research hook is read, never hooked twice.
        attach = _code(_body(self.plugin, "static void LaProbeAttach(int idx)"))
        self.assertIn("g_ApRollRows", attach)
        self.assertIn("g_DpChat", attach)
        self.assertIn("g_Orig_LootGroundCreateFromItem", attach)
        self.assertLess(attach.index("g_ApRollRows"), attach.index("HookOneScript("))
        self.assertLess(attach.index("g_DpChat"), attach.index("HookOneScript("))

    def test_mod_state_reports_it(self):
        start = self.plugin.index('\\"lootAnnounce\\":{\\"on\\":')
        block = self.plugin[start:self.plugin.index('+ "}";', start)]
        fields = re.findall(r'\\"(\w+)\\":', block)
        self.assertEqual(fields, ["lootAnnounce", "on", "route", "seen", "announced", "heldRarity", "heldNoRarity",
                                  "heldDuplicate", "heldBagDrop", "sinkRefused"])

    def test_the_lines_the_live_operator_reads(self):
        command = _code(_body(self.plugin, "static void LootAnnounceCommand(const std::string& rest)"))
        self.assertIn("g_LootAnnounce.StatusLine()", command)
        self.assertIn('"lootann: usage lootann 1 | 0 | stat"', command)
        self.assertIn('if (arg.empty() || arg == "stat") { Out(LootAnnounceStatLine()); return; }', command)
        stat = _code(_body(self.plugin, "static std::string LootAnnounceStatLine()"))
        fields = re.findall(r'" ([a-z-]+)="', stat)
        self.assertEqual(fields, ["init-hook", "drop-hook", "bag-drop-calls", "unidentified", "no-item", "queue-full"])


if __name__ == "__main__":
    unittest.main()
