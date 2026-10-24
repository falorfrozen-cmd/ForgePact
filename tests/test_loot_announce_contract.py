"""Loot announcements (`lootann`): the plugin wiring and the rules the core must keep.

tests/test_loot_announce_behavior.py runs the real core against controlled
drops. This file pins what that harness cannot see: the announced rarity set
in the header is exactly {9, 7, 10}; the verb is a player command with
its own early return; the hooks install only when the core's ShouldInstall
says so, with the local player resolved through HhResolveLocalPlayer (the
guide's Known Limitations item 8); LootGroundInit is named in exactly one HookOneScript call in the
player build, inside HiddenLootInstall, and the announcement installs through
it and is handed the call by the shared detour after the game's original; the
creation guard notes every CreateItemNew return through the shared hook, its
route is reported as create-hook=, and the mod no longer hooks LootGroundDrop;
the frame tick costs nothing while off, runs before hidden loot's and ages the
creation window after its batch; the rarity is read by name from
itemInfoStruct["27"]; the research instrument `lootannprobe`
and its count-only hook table never reach the player build; the shipped sink
is the one the research doc's `announce-route:` names (`chatcolour` since
ForgePact #228; Live procedure 1 picked `server`); and the modstate object and
the line formats the live operator reads.

The line's colours (ForgePact #228): the core holds the five colours (the
online announcement's orange sender colour, the grey chat text and the three
rarity colours), the rarity-to-colour rule and both the line's parts, in the
online announcement's layout (the owner, 2026-10-23: the character's name as
the sender part, "found <item name>" as the text part), and the red line's
text; the `chatcolour` body calls ChatAddMessage by SDK name with the player
as self and the fifteen arguments in shape 8's kinds, the colours the core's,
and the adapter holds no colour value but netsend's own pre-#228 constant,
exempted by name; a refused `chatcolour` call logs, then runs the `server`
body, which takes its text from the core; the `lootann line:` line and
`lootannprobe place`'s tier write are research-build only; and, in the hub
checkout, the header's Heroic, Angelic and Unholy colours and its sender
colour equal the hub's curated record
(hs-game-sdk/curated/loot_announcement_measurements.json, announcement_colours
codes 9, 7 and 10, and announcement_sender_colour; skipped in a standalone
clone).

The Chaos Tower's loot burst (ForgePact #227): the tick reads
Controller_obj.lootExplosionList by name (FindControllerInstance,
variable_instance_get, ds_exists through SigListHandle, ds_list_size, and
ds_list_find_value for the new entries only) after the batch's decisions and
before AgeCreationWindow; the guard in LaProcess consults the burst memory; the
reader adds no HookOneScript, no address and no write; `lootannprobe burst`
and the `lootann decide:` line are research-build only; `lootannprobe place`
sets the loot filter's tier and prints the stamp; and the stat line carries
burst-listed=, burst-fresh=, burst-landed= and burst-overflow=, then the
adapter's burst-list= (the last lookup's answer) and burst-nokey= (list
entries that gave no key), both in the player build.
"""
import copy
import json
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PLUGIN = ROOT / "plugin" / "ModuleMain.cpp"
HEADER = ROOT / "plugin" / "include" / "ForgePact" / "LootAnnounceMod.hpp"
RESEARCH = ROOT / "docs" / "loot-announcement-research.md"
# The hub's curated record of the announcement's colours. It exists only when
# ForgePact is checked out as the hub's submodule, not in a standalone clone.
CURATED = ROOT.parent / "hs-game-sdk" / "curated" / "loot_announcement_measurements.json"

ADAPTER_START = "// ---- Loot announcements (LootAnnounceMod.hpp): the adapter"
ADAPTER_END = "// ---- end of the loot announcement adapter"

# The shipped route (`announce-route:` in the research doc's "Route"): Live
# procedure 1 picked `server` on 2026-10-04; ForgePact #228 ships
# `chatcolour` (2026-10-23), with `server` as its red fallback. A later route
# changes this together with the header's kShippedSink.
EXPECTED_ROUTE = "chatcolour"
SINK_NAMES = {"method", "netsend", "chatadd", "server", "chatcolour"}

# The chat line's colours the core holds (ForgePact #228), GameMaker's
# blue-green-red integers: the grey chat text, Heroic, Angelic, Unholy, and
# the online announcement's sender colour (orange, the owner's layout of
# 2026-10-23).
LINE_COLOURS = ("11974842", "9764608", "10681599", "7224024", "18687")

# netsend's own copy of the online sender colour (its second argument, as the
# closure supplies it). It predates #228 and is that body's, so the adapter's
# no-colour scan exempts exactly this declaration by name.
NETSEND_SENDER_CONSTANT = "static constexpr double kLaNetSendA1 = 18687.0;"

# The research instrument's names, none of which the player build may carry.
RESEARCH_ONLY_NAMES = ("lootannprobe", "LootAnnProbeCommand", "g_LaProbeRows", "LaProbeDetour", "LaProbeNoteInit",
                       "LaProbeAttach", "LaProbePlace", "LaProbeTry", "LaProbeSay",
                       "LaProbeAnonControl", "LaProbeUnresolvedRows", "LaProbeBurst", "g_LaBurstKeep")

# The burst reader (ForgePact #227): the function the tick calls to read
# Controller_obj.lootExplosionList, and the lookup it shares with
# `lootannprobe burst`.
BURST_READ = "static void LaReadBurstList()"
BURST_LIST = "static bool LaBurstList(RValue& ctrl, RValue& list, double& id, std::string& why)"

# The lootannprobe research block's own bounds in ModuleMain.cpp.
PROBE_START = "#ifndef FORGEPACT_RELEASE\n// ===== lootannprobe:"
PROBE_END = "#endif // FORGEPACT_RELEASE (lootannprobe)"


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


def _header_announcement_colours(code: str) -> dict:
    """Rarity code -> text colour, as the header's AnnouncementColour maps
    them, each name resolved through the header's own constants."""
    constants = dict((name, int(value)) for name, value in
                     re.findall(r"static constexpr int (k\w+) = (-?\d+);", code))
    colour = _body(code, "static constexpr int AnnouncementColour(int rarityCode)")
    return {constants[rarity]: constants[value]
            for rarity, value in re.findall(r"case (k\w+): return (k\w+);", colour)}


def _header_sender_colour(code: str) -> int:
    """The sender colour Line() returns (its third member), resolved through
    the header's own constants."""
    constants = dict((name, int(value)) for name, value in
                     re.findall(r"static constexpr int (k\w+) = (-?\d+);", code))
    line = _body(code, "static ChatLine Line(const std::string& character, const std::string& itemName, int rarityCode)")
    match = re.search(r"return \{[^,]+,[^,]+,\s*(k\w+),", line)
    if not match:
        raise AssertionError(f"Line() returns no named sender colour: {line.strip()}")
    return constants[match.group(1)]


def _curated_colour_mismatches(header: dict, record: dict) -> list:
    """Each announced rarity (Heroic 9, Angelic 7, Unholy 10) whose header
    colour differs from the curated record's announcement_colours entry."""
    curated = {entry["code"]: entry["bgr"] for entry in record.get("announcement_colours", [])}
    mismatches = []
    for code in (9, 7, 10):
        if code not in curated:
            mismatches.append(f"rarity {code}: no announcement_colours entry in the curated record")
        elif header.get(code) != curated[code]:
            mismatches.append(f"rarity {code}: header {header.get(code)} != curated {curated[code]}")
    return mismatches


class LootAnnounceHeaderTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.code = _code(HEADER.read_text(encoding="utf-8").replace("\r\n", "\n"))

    def test_the_announced_rarity_set_is_heroic_angelic_unholy_satanic_and_mythic_held(self):
        # Satanic (6) and Mythic (5) were added and reverted on the owner's
        # word the same day (2026-10-08): neither is in the set.
        constants = dict((name, int(value)) for name, value in
                         re.findall(r"static constexpr int (k\w+) = (-?\d+);", self.code))
        members = re.search(r"kAnnouncedRarities\s*=\s*\{([^}]*)\}", self.code)
        self.assertIsNotNone(members, "kAnnouncedRarities is not an enumerable list in the header")
        names = [m.strip() for m in members.group(1).split(",") if m.strip()]
        values = [constants[n] if n in constants else int(n) for n in names]
        self.assertEqual(len(values), 3, values)
        self.assertEqual(set(values), {9, 7, 10})
        self.assertRegex(self.code, r"std::array<int, 3> kAnnouncedRarities")

    def test_the_install_decision_is_the_core_s(self):
        # Known Limitations item 8: on, setup done, a player resolved, not
        # tried yet; the tick looks every 60 frames; the state names are the
        # ` install=` values the live operator reads.
        should = _body(self.code, "bool ShouldInstall(bool setupDone, bool playerResolved, bool installTried) const")
        self.assertIn("return m_Enabled && setupDone && playerResolved && !installTried;", should)
        self.assertIn("static constexpr unsigned long long kInstallPollFrames = 60;", self.code)
        looks = _body(self.code, "static constexpr bool LooksForPlayer(unsigned long long armedFrames)")
        self.assertIn("armedFrames % kInstallPollFrames == 0", looks)
        state = _body(self.code, "const char* InstallState(bool installTried) const")
        self.assertEqual(re.findall(r'"([a-z-]+)"', state), ["installed", "waiting-for-character", "not-armed"])

    def test_it_starts_off_and_counts_what_it_holds_back(self):
        self.assertIn("bool m_Enabled = false;", self.code)
        for counter in ("seen", "announced", "heldRarity", "heldNoRarity", "heldDuplicate", "heldBagDrop", "sinkRefused",
                        "created", "createOverflow"):
            self.assertRegex(self.code, rf"long long {counter} = 0;", counter)

    def test_the_creation_window_replaced_the_bag_drop_window(self):
        # Live procedure 2's bag drop was announced while the LootGroundDrop
        # window counted 0; the core now decides on "built this frame or the
        # last", with a capped window that switching off clears.
        for gone in ("BagDropScope", "BeginBagDrop", "EndBagDrop", "BagDropActive", "m_BagDropDepth"):
            self.assertNotIn(gone, self.code, gone)
        cap = re.search(r"static constexpr std::size_t kCreationCap = (\d+);", self.code)
        self.assertIsNotNone(cap)
        self.assertGreaterEqual(int(cap.group(1)), 1024)
        decide = _body(self.code, "Verdict Decide(")
        self.assertLess(decide.index("++m_Stats.seen;"), decide.index("if (!recentlyCreated)"))
        self.assertLess(decide.index("if (!recentlyCreated)"), decide.index("kRarityUnread"))
        enabled = _body(self.code, "void SetEnabled(bool on)")
        self.assertIn("m_CreatedNow.clear();", enabled)
        self.assertIn("m_CreatedBefore.clear();", enabled)

    def test_it_is_game_independent(self):
        for forbidden in ("RValue", "CInstance", "g_Yytk", "YYTK", "#include <Windows", "Aurie"):
            self.assertNotIn(forbidden, self.code, forbidden)

    def test_the_shipped_sink_is_the_route_the_research_doc_names(self):
        shipped = re.search(r"static constexpr Sink kShippedSink = Sink::(\w+);", self.code)
        self.assertIsNotNone(shipped)
        names = dict(re.findall(r"case Sink::(\w+): return \"(\w+)\";", self.code))
        self.assertEqual(set(names.values()), SINK_NAMES)
        self.assertEqual(names[shipped.group(1)], EXPECTED_ROUTE)
        # `lootannprobe try 1..5` numbers the first four bodies by the enum's
        # order; the fifth is appended, never inserted.
        self.assertIn("enum class Sink : int { Method, NetSend, ChatAdd, Server, ChatColour };", self.code)
        # Once Live procedure 1 picked one, the research doc says which, and it
        # must be this one (the last `announce-route:` line is the current one).
        if RESEARCH.is_file():
            routes = re.findall(r"announce-route:\s*`?(\w[\w-]*)", RESEARCH.read_text(encoding="utf-8"))
            picked = [r for r in routes if r in SINK_NAMES | {"not-observed"}]
            if picked:
                self.assertEqual(picked[-1], EXPECTED_ROUTE, picked)

    def test_the_line_s_colours_and_parts_are_the_core_s(self):
        # ForgePact #228: the core holds every colour of the line and builds
        # both its parts and the red line's text; the adapter only passes them.
        for value in LINE_COLOURS:
            self.assertRegex(self.code, rf"static constexpr int k\w+Colour = {value};", value)
        colour = _body(self.code, "static constexpr int AnnouncementColour(int rarityCode)")
        for case in ("case kHeroic: return kHeroicColour;", "case kAngelic: return kAngelicColour;",
                     "case kUnholy: return kUnholyColour;", "default: return kNeutralColour;"):
            self.assertIn(case, colour)
        line = _body(self.code, "static ChatLine Line(const std::string& character, const std::string& itemName, int rarityCode)")
        # The online layout (the owner, 2026-10-23): the character's name as the
        # sender part in the online sender colour, "found <item name>" as the
        # text part in the rarity's colour.
        self.assertIn('return { character, "found " + itemName, kOnlineSenderColour, AnnouncementColour(rarityCode) };',
                      line)
        self.assertRegex(self.code, r"static constexpr int kOnlineSenderColour = 18687;")
        server = _body(self.code, "static std::string ServerText(const std::string& character, const std::string& itemName)")
        self.assertIn('return character + " found " + itemName;', server)
        # Never 0 or 6776816 (the sender colour that greys the text) in code.
        self.assertNotIn("6776816", self.code)

    def test_header_colours_match_the_curated_record(self):
        # The hub's curated record (hs-game-sdk/curated) and the header state
        # the same three announcement colours; a standalone clone has no
        # record to compare against, so the test skips there.
        if not CURATED.is_file():
            self.skipTest(f"{CURATED.name} not found at {CURATED}: a standalone ForgePact clone has no hub record")
        record = json.loads(CURATED.read_text(encoding="utf-8"))
        header = _header_announcement_colours(self.code)
        self.assertEqual(_curated_colour_mismatches(header, record), [])
        # Negative control: a record whose Heroic colour is one off, and one
        # missing Unholy, are both caught.
        off_by_one = copy.deepcopy(record)
        for entry in off_by_one["announcement_colours"]:
            if entry["code"] == 9:
                entry["bgr"] += 1
        self.assertEqual(len(_curated_colour_mismatches(header, off_by_one)), 1)
        no_unholy = copy.deepcopy(record)
        no_unholy["announcement_colours"] = [e for e in record["announcement_colours"] if e["code"] != 10]
        self.assertEqual(len(_curated_colour_mismatches(header, no_unholy)), 1)
        # The line's sender colour (the online layout) is the record's
        # announcement_sender_colour; a record one off is caught.
        sender = _header_sender_colour(self.code)
        self.assertEqual(sender, record["announcement_sender_colour"]["bgr"])
        self.assertNotEqual(sender, record["announcement_sender_colour"]["bgr"] + 1)

    def test_the_line_formats(self):
        self.assertIn('"lootann: on"', self.code)
        self.assertIn('"lootann: off"', self.code)
        stat = _body(self.code, "std::string StatLine() const")
        fields = re.findall(r'" ([a-z-]+)="', stat)
        self.assertEqual(fields, ["route", "seen", "announced", "held-rarity", "held-no-rarity", "held-duplicate",
                                  "held-bag-drop", "sink-refused", "remembered", "created", "create-overflow",
                                  "burst-listed", "burst-fresh", "burst-landed", "burst-overflow"])


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

    def test_the_announcement_comes_before_the_flight_charms_ground_icon(self):
        # ForgePact #158: the flight charms' ground icon is the detour's third
        # consumer, after the announcement, so the announcement sees the call
        # exactly as before; ForgePact #185's tracker is the fourth.
        hook = _code(_body(self.plugin, "static RValue& HookHiddenLootInit(CInstance* S, CInstance* O, RValue& R, int argc, RValue** A)"))
        self.assertEqual(hook.count("FlightCharmOnGroundInit(argc, A);"), 1)
        self.assertLess(hook.index("LootAnnounceOnInit(S, argc, A);"), hook.index("FlightCharmOnGroundInit(argc, A);"))
        self.assertLess(hook.index("LootAnnounceOnInit(S, argc, A);"), hook.index("TrackerOnInit(S, argc, A);"))

    def test_the_shared_detour_hands_the_call_on_after_the_original(self):
        hook = _code(_body(self.plugin, "static RValue& HookHiddenLootInit(CInstance* S, CInstance* O, RValue& R, int argc, RValue** A)"))
        self.assertEqual(hook.count("LootAnnounceOnInit(S, argc, A);"), 1)
        self.assertLess(hook.index("g_Orig_LootGroundInit(S, O, R, argc, A)"), hook.index("LootAnnounceOnInit(S, argc, A);"))
        self.assertLess(hook.index(".OnInit("), hook.index("LootAnnounceOnInit(S, argc, A);"))
        on_init = _code(_body(self.plugin, "static void LootAnnounceOnInit(CInstance* S, int argc, RValue** A)"))
        statements = [s.strip() for s in on_init.split("\n") if s.strip() and not s.strip().startswith("#")]
        self.assertEqual(statements[1] if statements[0].startswith("LaProbeNoteInit") else statements[0],
                         "if (!g_LootAnnounce.Enabled()) return;")
        # Inside the call: the handles are reduced; no item is read, nothing is
        # decided or said there.
        for forbidden in ("Decide(", "LootAnnounceSink(", "itemInfoStruct", "ChatAdd", "BagDrop"):
            self.assertNotIn(forbidden, on_init, forbidden)

    def test_the_creation_guard_notes_every_create_item_new_return(self):
        # The note sits in the shared hook body, for CreateItemNew only, after
        # the original returns and before the gem tables' early return, behind
        # one enabled check; inner and outermost calls alike.
        start = self.plugin.index("#define ITEM_CREATE_HOOK(NAME)")
        macro = self.plugin[start:self.plugin.index("ITEM_CREATE_HOOK(CreateItemNew)", start)]
        note = "if (_final && g_LootAnnounce.Enabled()) LootAnnounceNoteCreated(argc, A, _res);"
        self.assertEqual(macro.count(note), 1)
        self.assertLess(macro.index("_resp = &g_Orig_##NAME(S, O, R, argc, A);"), macro.index(note))
        self.assertLess(macro.index(note), macro.index("if (g_GemTableBuilding) return _res;"))
        self.assertNotIn("g_TruthDepth == 0) LootAnnounceNoteCreated", macro)
        # Declared before the macro, defined in the adapter.
        self.assertLess(self.plugin.index("static void LootAnnounceNoteCreated(int argc, RValue** A, const RValue& result);"),
                        start)
        noted = _code(_body(self.plugin, "static void LootAnnounceNoteCreated(int argc, RValue** A, const RValue& result)"))
        self.assertIn("LaItemKey(*A[0], key)) g_LootAnnounce.NoteCreated(key);", noted)
        self.assertIn("if (LaItemKey(result, key)) g_LootAnnounce.NoteCreated(key);", noted)
        # The key: a struct by its object pointer, a reference by the value it
        # holds, anything else none; compared, never followed.
        key = _code(_body(self.plugin, "static bool LaItemKey(const RValue& v, ForgePact::LootAnnounceMod::ItemKey& out)"))
        self.assertIn("v.m_Kind == VALUE_OBJECT && v.m_Object", key)
        self.assertIn("ForgePact::LootAnnounceMod::kKeyStruct, (std::uint64_t)(uintptr_t)v.m_Object", key)
        self.assertIn("v.m_Kind == VALUE_REF", key)
        self.assertIn("ForgePact::LootAnnounceMod::kKeyReference, (std::uint64_t)v.m_i64", key)
        self.assertIn("return false;", key)
        for forbidden in ("->", "CallBuiltin", "*v.m_Object"):
            self.assertNotIn(forbidden, key, forbidden)
        # The mod no longer hooks LootGroundDrop; its bag-drop window is gone.
        for gone in ("g_Orig_LootGroundDropLa", "fp_lootann_drop", "LaHookLootGroundDrop", "LaInstallDropHook",
                     "g_LaDropRoute", "g_LaBagDropCalls", "BagDropScope", "BagDropActive"):
            self.assertNotIn(gone, self.plugin, gone)
        self.assertNotIn("gml_Script_LootGroundDrop", self.adapter)

    def test_create_item_new_is_installed_and_its_route_said(self):
        lai = _code(_body(self.plugin, "static void LootAnnounceInstall()"))
        self.assertIn("LaInstallCreateHook();", lai)
        self.assertLess(lai.index("HiddenLootInstall();"), lai.index("LaInstallCreateHook();"))
        self.assertIn('std::string_view(g_LaCreateRoute) != "both"', lai)
        self.assertIn('"lootann: CreateItemNew hook "', lai)
        create = _code(_body(self.plugin, "static void LaInstallCreateHook()"))
        self.assertIn('HookOneScript(SdkShortScriptName(HeroSiege::Scripts::gml_Script_CreateItemNew), "fp_lootann_new",', create)
        self.assertIn("(PVOID)Hook_CreateItemNew, &g_Orig_CreateItemNew, &native)", create)
        self.assertIn('g_LaCreateRoute = ok ? (native ? "both" : "table-only") : "none";', create)
        # Held by another feature first: read from its saved original, the way
        # InstallSignatureAngelicHooks' savedRoute does.
        self.assertLess(create.index("if (g_Orig_CreateItemNew)"), create.index("HookOneScript("))
        self.assertIn('SavedOriginalIsTableOnly(g_Orig_CreateItemNew) ? "table-only" : "both"', create)
        saved = _code(_body(self.plugin, "static void InstallSignatureAngelicHooks()"))
        helper = _code(_body(self.plugin, "static bool SavedOriginalIsTableOnly(PFUNC_YYGMLScript orig)"))
        test = "AddrIsExecutableInModule(GetModuleHandleA(nullptr), (const void*)orig)"
        self.assertIn(test, saved)
        self.assertIn(test, helper)
        # The research build's item inspection installs it with both routes,
        # so no later installer inherits a table swap.
        inspect = _code(_body(self.plugin, "static void InstallItemInspectHooks()"))
        self.assertIn('HookOneScript("CreateItemNew",', inspect)
        self.assertNotIn('HookOneScriptTable("CreateItemNew"', inspect)

    def test_the_tick_costs_nothing_while_off_and_runs_before_hidden_loot(self):
        tick = [l.strip() for l in _code(_body(self.plugin, "static void LootAnnounceTick()")).splitlines() if l.strip()]
        self.assertEqual(tick[0], "if (!g_LootAnnounce.Enabled()) return;")
        # The creation window ages once per tick, after its batch, whether or
        # not the frame noted a ground item.
        self.assertEqual(tick[-1], "g_LootAnnounce.AgeCreationWindow();")
        self.assertEqual(sum(l.count("AgeCreationWindow") for l in tick), 1)
        frame = _code(_body(self.plugin, "void FrameCallback(FWFrame& FrameContext)"))
        self.assertEqual(frame.count("LootAnnounceTick();"), 1)
        self.assertIn("if (g_Setup) LootAnnounceTick();", frame)
        self.assertLess(frame.index("if (g_Setup) LootAnnounceTick();"), frame.index("if (g_Setup) HiddenLootTick();"))

    def test_the_rarity_is_read_by_name_from_key_27(self):
        rarity = _code(_body(self.plugin, "static int LaRarity(const RValue& item)"))
        self.assertIn('StructOrRefField(item, "itemInfoStruct", info)', rarity)
        self.assertIn('StructOrRefField(info, "27", code)', rarity)
        self.assertIn("kRarityUnread", rarity)
        for forbidden in ('"c"', "level", "relicLevel", '"b"'):
            self.assertNotIn(forbidden, rarity, forbidden)
        process = _code(_body(self.plugin, "static void LaProcess(const LaPending& p)"))
        self.assertIn("HeroSiege::Player::kGroundItemInstanceField", process)
        self.assertIn("HeroSiege::Objects::GameObject::Loot_Ground_obj", process)
        self.assertIn("const bool hasKey = LaItemKey(item, key);", process)
        self.assertIn("if (!hasKey) ++g_LaNoKey;", process)
        self.assertIn("const bool recent = hasKey && g_LootAnnounce.RecentlyCreated(key);", process)
        self.assertIn("const int rarity = LaRarity(item);", process)
        self.assertIn("g_LootAnnounce.Decide(rarity, recent, (int64_t)id, type, stamp, burst)", process)
        # An identity that could not be read is counted and left before the
        # core's memory sees it: "unread" must never compare equal to "unread".
        self.assertNotIn("double id = -1.0;", process)
        self.assertIn("idRead = id >= 0.0;", process)
        refuse = "if (!ForgePact::LootAnnounceMod::Identifiable(idRead, type, stamp)) { ++g_LaNoIdentity; return; }"
        self.assertIn(refuse, process)
        self.assertLess(process.index(refuse), process.index("g_LootAnnounce.Decide("))
        item_type = _code(_body(self.plugin, "static std::string LaItemType(const RValue& item)"))
        self.assertIn('StructOrRefField(item, "itemType", t)', item_type)
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

    def _chatcolour_body(self) -> str:
        run = _code(_body(self.plugin, "static LaSinkRun LaRunSink("))
        start = run.index("if (sink == Sink::ChatColour) {")
        return run[start:run.index("return run;", start)]

    def test_chatcolour_calls_chat_add_message_with_the_core_s_line(self):
        # ForgePact #228: ChatAddMessage by SDK name through LaCall, the player
        # as self, the fifteen arguments in shape 8's kinds: sender, text, the
        # core's two colours as reals, int64 0 twice, two reals, the clock
        # stamp, then six undefined.
        body = self._chatcolour_body()
        self.assertIn("ForgePact::LootAnnounceMod::Line(character, name, LaRarity(item))", body)
        args = re.search(r"std::vector<RValue> args = \{(.*?)\};", body, flags=re.S)
        self.assertIsNotNone(args, "the chatcolour body builds no argument list")
        supplied = [a.strip() for a in args.group(1).split(",")]
        self.assertEqual(supplied, ["RValue(line.sender)", "RValue(line.text)", "RValue((double)line.senderColour)",
                                    "RValue((double)line.textColour)", "RValue((int64_t)0)", "RValue((int64_t)0)",
                                    "RValue(0.0)", "RValue(0.0)", "RValue(LaClockStamp())"])
        self.assertIn("for (int i = 0; i < 6; ++i) args.push_back(RValue());", body)
        self.assertIn('LaCall(run, SdkShortScriptName(HeroSiege::Scripts::gml_Script_ChatAddMessage), player, "Player_obj", '
                      "args, before);", body)
        # The item name that is not text refuses before any call, as before.
        run = _code(_body(self.plugin, "static LaSinkRun LaRunSink("))
        self.assertLess(run.index('run.failed = "text (itemInfoStruct[\\"28\\"] is not text)";'),
                        run.index("if (sink == Sink::ChatColour) {"))

    def test_the_adapter_holds_no_colour_value(self):
        # The colours are the core's; the adapter passes line.senderColour and
        # line.textColour. netsend's own constants predate #228 and are that
        # body's, unchanged; kLaNetSendA1 holds the online sender colour, so
        # that one declaration, and only it, is exempted by name.
        self.assertEqual(self.adapter.count(NETSEND_SENDER_CONSTANT), 1, NETSEND_SENDER_CONSTANT)
        scanned = self.adapter.replace(NETSEND_SENDER_CONSTANT, "")
        for value in LINE_COLOURS + ("6776816",):
            self.assertNotIn(value, scanned, value)
        # Negative control: the exemption hides only netsend's declaration;
        # 18687 anywhere else in the adapter is still found.
        planted = (self.adapter + "\nRValue(18687.0);\n").replace(NETSEND_SENDER_CONSTANT, "")
        self.assertIn("18687", planted)
        body = self._chatcolour_body()
        self.assertIsNone(re.search(r"\b\d{4,}\b|0x[0-9A-Fa-f]+", body), body)
        # Negative control: the same scan finds a colour literal in a body.
        self.assertIsNotNone(re.search(r"\b\d{4,}\b|0x[0-9A-Fa-f]+", "RValue(9764608.0)"))

    def test_server_takes_its_text_from_the_core(self):
        run = _code(_body(self.plugin, "static LaSinkRun LaRunSink("))
        self.assertIn('LaCall(run, SdkShortScriptName(HeroSiege::Scripts::gml_Script_ChatAddServerMessage), player, "Player_obj",\n'
                      "           { RValue(ForgePact::LootAnnounceMod::ServerText(character, name)) }, before);", run)
        self.assertNotIn('" found "', run)

    def test_a_refused_chatcolour_call_logs_then_shows_the_red_line(self):
        sink = _code(_body(self.plugin, "static bool LootAnnounceSink(const RValue& item, CInstance* lootInst)"))
        self.assertIn("const bool fallBack = Mod::kShippedSink == Mod::Sink::ChatColour && run.attempted;", sink)
        logged = sink.index('"lootann: no line (sink "')
        self.assertIn('"; showing the red line (sink server) instead"', sink)
        red = sink.index("LaRunSink(ForgePact::LootAnnounceMod::Sink::Server, kLaNetSendA0IsPlayer, item, lootInst, nullptr)")
        self.assertLess(logged, red)
        self.assertLess(sink.index("if (fallBack) {"), red)
        # The announcement still counts as refused, whether the red line showed.
        self.assertTrue(sink.rstrip().endswith("return false;"), sink)
        self.assertEqual(sink.count("return true;"), 1)
        # `attempted` is set by LaCall alone, before it resolves the script, so
        # a refusal before the call (no player, no item name) has no fallback.
        call = _code(_body(self.plugin, "static void LaCall(LaSinkRun& run, const char* script, CInstance* self,"))
        self.assertLess(call.index("run.attempted = true;"), call.index('"asset_get_index"'))
        self.assertEqual(len(re.findall(r"\battempted = true\b", self.adapter)), 1)

    def test_the_supplied_line_and_the_tier_write_are_research_only(self):
        # The `lootann line:` line Live 1 reads, and `lootannprobe place`'s
        # tier write, are in the research build and not the player build.
        line = 'Out(std::string("lootann line: sink=")'
        self.assertIn(line, self.plugin)
        self.assertNotIn("lootann line:", self.player)
        self.assertNotIn("g_LaLineLogs", self.player)
        # After the call, inside the chatcolour body's own research block.
        raw = self.plugin[self.plugin.index("if (sink == Sink::ChatColour) {"):]
        raw = raw[:raw.index("return run;")]
        block = raw[raw.index("#ifndef FORGEPACT_RELEASE"):raw.index("#endif")]
        self.assertIn("if (++g_LaLineLogs <= 100)", block)
        for field in ('" sender=\\""', '"\\" body=\\""', '"\\" sender-colour="', '" body-colour="', '" dispatched="'):
            self.assertIn(field, block, field)
        self.assertLess(raw.index("LaCall(run,"), raw.index("#ifndef FORGEPACT_RELEASE"))
        tier = 'g_Yytk->CallBuiltin("variable_struct_set", { info, RValue("32"), RValue(5.0) })'
        self.assertIn(tier, _code(self.plugin))
        self.assertNotIn(tier, self.player)
        # Negative control: the same check finds a line left outside a block.
        self.assertIn("lootann line:", _player_build('Out("lootann line: x");\n#ifndef FORGEPACT_RELEASE\n#endif\n'))

    def test_the_closure_is_resolved_by_number_and_listed_with_its_index(self):
        # Live procedure 1 listed three method variables as `<undefined>`:
        # script_get_name was handed method_get_index's raw value. The proven
        # resolver (CiTryResolveMethod) converts it to a number first, which a
        # VALUE_REF index needs too, and every row prints `#<index>` so a name
        # that does not resolve still shows what it was asked about.
        find = _code(_body(self.plugin, "static bool LaFindClosure("))
        self.assertIn('CallBuiltin("method_get_index", { v })', find)
        # IsNumericInstanceRead accepts a REAL, an INT32/INT64 or a VALUE_REF.
        self.assertIn("if (!IsNumericInstanceRead(idx))", find)
        self.assertIn("(int)idx.ToDouble()", find)
        self.assertIn('CallBuiltin("script_get_name", { RValue((double)scriptIdx) })', find)
        self.assertNotIn('CallBuiltin("script_get_name", { idx })', find)
        self.assertIn('"#" + std::to_string(scriptIdx)', find)
        # The SDK closure is matched on the resolved name, never the index.
        self.assertIn("const bool match = script == shortName || script == fullName;", find)

    def test_the_methods_listing_carries_the_anon_control(self):
        # Live procedure 1's listing resolved the named s_lootDrawData while
        # every anon@ method came back `<undefined>`, so a named row cannot
        # tell a working listing from a blind one. The control counts the two
        # Create-bound anon@ variables and how many resolved; 0 resolved
        # makes the listing INSTRUMENT-BLIND (docs/loot-announcement-research.md).
        start = self.plugin.find(PROBE_START)
        end = self.plugin.find(PROBE_END)
        self.assertGreaterEqual(start, 0, "the lootannprobe research block's start is missing")
        self.assertGreater(end, start, "the lootannprobe research block's end is missing")
        probe = self.plugin[start:end]
        self.assertIn('"lootannprobe methods: anon rows resolved: "', probe)
        control = _code(_body(probe, "static void LaProbeAnonControl("))
        self.assertIn('"m_LootFilter"', control)
        self.assertIn('"m_LootGroundDeActiveStep"', control)
        self.assertIn('"anon@"', control)
        # The Create event is spelled through the SDK closure's name.
        self.assertIn("kLaClosureShort", control)
        self.assertNotIn("s_lootDrawData", control)
        methods = _code(_body(probe, "static void LaProbeMethods()"))
        listed = methods.index('"lootannprobe methods on "')
        counted = methods.index("LaProbeAnonControl(listing, anonResolved, anonListed);")
        line = methods.index('"lootannprobe methods: anon rows resolved: "')
        self.assertLess(methods.index("LaFindClosure(loot, method, variable, listing)"), counted)
        self.assertLess(listed, line)
        self.assertLess(counted, line)
        # A passing control does not make every row readable: `fail` needs
        # every listed method row resolved, so the probe counts the rows that
        # did not (`<undefined>#<n>`, `?#?`, `#(<kind>)`, an unreadable read)
        # on a line of their own, after the anon control.
        self.assertIn('"lootannprobe methods: unresolved rows: "', probe)
        unresolved = _code(_body(probe, "static void LaProbeUnresolvedRows("))
        for marker in ('"<undefined>"', '"?#"', '"#("', '"(unreadable)"'):
            self.assertIn(marker, unresolved)
        tallied = methods.index("LaProbeUnresolvedRows(listing, unresolvedRows, methodRows, unresolvedText);")
        unresolvedLine = methods.index('"lootannprobe methods: unresolved rows: "')
        self.assertLess(methods.index("LaFindClosure(loot, method, variable, listing)"), tallied)
        self.assertLess(tallied, unresolvedLine)
        self.assertLess(line, unresolvedLine)
        # Research build only; the shared resolver the mod calls is untouched.
        self.assertNotIn("anon rows resolved", self.player)
        self.assertNotIn("unresolved rows", self.player)
        self.assertNotIn("m_LootFilter", _code(_body(self.plugin, "static bool LaFindClosure(")))

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
        # LootGroundInit counts through the shared detour; LootGroundDrop is an
        # ordinary count-only row of its own, since the mod no longer hooks it.
        self.assertIn("LaShared::Init", table)
        self.assertNotIn("LaShared::Drop", code)
        self.assertNotIn("kLaRowDrop", code)
        self.assertIn('gml_Script_LootGroundDrop),                "fp_lap_lgdrop",    LaShared::None,', table)
        rule1 = self.plugin[self.plugin.index("//   1. `via fp_hiddenloot_init`"):self.plugin.index("//   2. `via angelicprobe")]
        self.assertNotIn("fp_lootann_drop", rule1)
        self.assertNotIn("LaProbeNoteDrop", rule1)
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
        self.assertEqual(fields, ["init-hook", "create-hook", "install", "unidentified", "no-item", "no-key", "no-identity",
                                  "queue-full", "burst-list", "burst-nokey"])
        install = '" install=" + g_LootAnnounce.InstallState(g_LaInstallTried)'
        self.assertIn(install, stat)
        # `lootann 1` names both hooks' routes, then whether they are in.
        self.assertIn('" init-hook=" + LaInitRoute() + " create-hook=" + g_LaCreateRoute', command)
        reply = re.search(r'" init-hook=" \+ LaInitRoute\(\) \+ " create-hook=" \+ g_LaCreateRoute\s*\+ (.*?)\);', command,
                          flags=re.S)
        self.assertIsNotNone(reply, "the lootann 1 reply does not continue after create-hook=")
        self.assertEqual(reply.group(1).strip(), install)
        # The core's line comes first, so the four burst fields the live
        # procedure reads (burst-listed=, burst-fresh=, burst-landed=,
        # burst-overflow=, right after create-overflow=) are on it.
        self.assertTrue(stat.strip().startswith("return g_LootAnnounce.StatLine()"), stat)
        core_stat = _body(_code(HEADER.read_text(encoding="utf-8")), "std::string StatLine() const")
        core_fields = re.findall(r'" ([a-z-]+)="', core_stat)
        self.assertEqual(core_fields[-5:], ["create-overflow", "burst-listed", "burst-fresh", "burst-landed",
                                            "burst-overflow"])

    def test_the_burst_list_is_read_after_the_decisions_and_before_the_window_ages(self):
        # ForgePact #227: the burst frame's landings need the memory filled in
        # earlier frames, and an entry collected this frame needs this frame's
        # CreateItemNew notes, so the list is read between the two, after the
        # tick's off check.
        tick = _code(_body(self.plugin, "static void LootAnnounceTick()"))
        self.assertEqual(tick.count("LaReadBurstList();"), 1)
        self.assertLess(tick.index("LaProcess(p);"), tick.index("LaReadBurstList();"))
        self.assertLess(tick.index("LaReadBurstList();"), tick.index("g_LootAnnounce.AgeCreationWindow();"))
        self.assertLess(tick.index("if (!g_LootAnnounce.Enabled()) return;"), tick.index("LaReadBurstList();"))
        # The reader is the fix: it is in the adapter and in the player build.
        self.assertIn(BURST_READ, self.adapter)
        self.assertIn(BURST_READ, self.player)
        self.assertIn(BURST_LIST, self.adapter)
        # Switching off restarts the count: the first read after the switch
        # comes on starts at the list's length then.
        command = _code(_body(self.plugin, "static void LootAnnounceCommand(const std::string& rest)"))
        off = command[command.index('if (arg == "0" || arg == "off")'):]
        self.assertIn("g_LaBurstHave = false;", off[:off.index("return;")])

    def test_the_burst_list_is_read_by_name(self):
        lookup = _code(_body(self.plugin, BURST_LIST))
        self.assertIn("FindControllerInstance(ctrl)", lookup)
        self.assertIn('g_Yytk->CallBuiltin("variable_instance_get", { ctrl, RValue("lootExplosionList") })', lookup)
        # SigListHandle asks ds_exists(list, ds_type_list) of the value as it
        # was read: a number or a reference alike; the kind decides how the
        # handle is read, never whether.
        self.assertIn("SigListHandle(list, id, why)", lookup)
        handle = _code(_body(self.plugin, "static bool SigListHandle(const RValue& v, double& id, std::string& why)"))
        self.assertIn('CallBuiltin("ds_exists", { v, RValue(2.0) })', handle)
        read = _code(_body(self.plugin, BURST_READ))
        self.assertIn("LaBurstList(ctrl, list, id, why)", read)
        self.assertIn('g_Yytk->CallBuiltin("ds_list_size", { list })', read)
        self.assertIn('g_Yytk->CallBuiltin("ds_list_find_value", { list, RValue((double)i) })', read)
        # Only the entries past those already read.
        self.assertIn("for (int i = g_LaBurstCount; i < size; ++i)", read)
        self.assertIn('StructOrRefField(entry, "itemInstance", item)', read)
        self.assertIn("LaItemKey(item, key)", read)
        self.assertIn("g_LootAnnounce.NoteListed(key, keep)", read)
        # A shorter list, a different list value, or none: the core forgets
        # and the count restarts at the list's length.
        self.assertIn("size < g_LaBurstCount", read)
        self.assertIn("id != g_LaBurstListId", read)
        self.assertGreaterEqual(read.count("g_LootAnnounce.ForgetBurst();"), 2)
        # A throw skips the frame's read and says so once.
        self.assertIn('"lootann: burst list unreadable - "', read)
        # The player build keeps every entry; only the research build's
        # `lootannprobe burst off` turns keeping off.
        self.assertIn("bool keep = true;", read)

    def test_the_stat_line_tells_a_missing_list_and_a_keyless_entry_apart(self):
        # ForgePact #227, after review: `burst-listed=0` alone reads the same
        # for a renamed lootExplosionList, a renamed itemInstance and a session
        # that never cleared a floor. `burst-list=` carries the last lookup's
        # answer (ok, or none(<why>) from LaBurstList) and `burst-nokey=`
        # counts the list entries that gave no key. Both ship in the player
        # build, after every field the line printed before.
        stat = _body(self.player, "static std::string LootAnnounceStatLine()")
        fields = re.findall(r'" ([a-z-]+)="', stat)
        self.assertEqual(fields[-3:], ["queue-full", "burst-list", "burst-nokey"])
        self.assertIn('" burst-list=" + g_LaBurstListState', stat)
        self.assertIn('" burst-nokey=" + std::to_string(g_LaBurstNoKey)', stat)
        read = _body(self.player, BURST_READ)
        # The state is set from LaBurstList's own result and reason.
        lookup = "const bool found = LaBurstList(ctrl, list, id, why);"
        state = 'g_LaBurstListState = found ? std::string("ok") : "none(" + why + ")";'
        self.assertIn(lookup, read)
        self.assertIn(state, read)
        self.assertLess(read.index(lookup), read.index(state))
        self.assertLess(read.index(state), read.index("if (!found)"))
        # A keyless entry is counted, on the branch beside NoteListed.
        noted = 'if (StructOrRefField(entry, "itemInstance", item) && LaItemKey(item, key)) g_LootAnnounce.NoteListed(key, keep);'
        self.assertIn(noted, read)
        self.assertTrue(read[read.index(noted) + len(noted):].lstrip().startswith("else ++g_LaBurstNoKey;"), read)
        # Negative control: the same check finds the field missing from a
        # line that stops at queue-full.
        short = re.findall(r'" ([a-z-]+)="', '+ " queue-full=" + std::to_string(n);')
        self.assertNotEqual(short[-3:], ["queue-full", "burst-list", "burst-nokey"])

    def test_the_guard_consults_the_burst_memory(self):
        process = _code(_body(self.plugin, "static void LaProcess(const LaPending& p)"))
        self.assertIn("const bool recent = hasKey && g_LootAnnounce.RecentlyCreated(key);", process)
        self.assertIn("const bool burst = hasKey && g_LootAnnounce.InBurstMemory(key);", process)
        self.assertIn("g_LootAnnounce.Decide(rarity, recent, (int64_t)id, type, stamp, burst)", process)

    def test_the_burst_reader_adds_no_hook_no_address_and_no_write(self):
        for name in (BURST_READ, BURST_LIST):
            body = _code(_body(self.plugin, name))
            for forbidden in ("HookOneScript", "MmCreateHook", "GetModuleHandle", "reinterpret_cast",
                              "variable_instance_set", "variable_struct_set", "ds_list_set", "ds_list_add",
                              "ds_list_delete", "ds_list_clear", "ds_list_destroy", "instance_destroy"):
                self.assertNotIn(forbidden, body, f"{name}: {forbidden}")
            self.assertIsNone(re.search(r"\bk\w*Rva\w*\b", body), name)
            self.assertIsNone(re.search(r"0x[0-9A-Fa-f]{6,}", body), name)
        # Negative control: the same scan finds a hook call.
        self.assertIn("HookOneScript", _code('static void LaReadBurstList() { HookOneScript("x"); }'))

    def test_the_burst_probe_and_the_decide_line_are_research_only(self):
        code = _code(self.plugin)
        for text in ("lootannprobe burst", "lootann decide:", "g_LaBurstKeep", "LaProbeBurst"):
            self.assertIn(text, code, text)
            self.assertNotIn(text, self.player, text)
        # The decide line is LaProcess's, inside a research block of its own.
        start = self.plugin.index("static void LaProcess(const LaPending& p)")
        process = self.plugin[start:self.plugin.index("\n}\n", start)]
        opened = process.index("#ifndef FORGEPACT_RELEASE")
        block = process[opened:process.index("#endif", opened)]
        self.assertIn("lootann decide:", block)
        self.assertIn("IsAnnouncedRarity(rarity)", block)
        for field in ('" rarity="', '" stamp="', '" verdict="', '" via="'):
            self.assertIn(field, block, field)
        # The probe's verb, its usage and its exact line.
        command = _code(_body(self.plugin, "static void LootAnnProbeCommand(const std::string& rest)"))
        self.assertIn('if (sub == "burst") { LaProbeBurst(Lower(TrimCopy(tail))); return; }', command)
        self.assertIn("burst [on|off]", command)
        burst = _code(_body(self.plugin, "static void LaProbeBurst(const std::string& arg)"))
        for field in ('"lootannprobe burst: memory="', '" tower="', '" list="', '" size="', '" given="'):
            self.assertIn(field, burst, field)
        self.assertIn("InChaosTowerCached()", burst)
        self.assertIn('RValue("lootExplosionGiven")', burst)
        self.assertIn("HeroSiege::Scripts::gml_Script_GPV", burst)
        self.assertIn("LaBurstList(ctrl, list, id, why)", burst)
        # Negative control: the same check finds a decide line left outside a
        # research block.
        self.assertIn("lootann decide:", _player_build('Out("lootann decide: x");\n#ifndef FORGEPACT_RELEASE\n#endif\n'))

    def test_place_sets_the_loot_filter_tier_and_prints_the_stamp(self):
        place = _code(_body(self.plugin, "static void LaProbePlace(const std::string& which)"))
        tier = 'g_Yytk->CallBuiltin("variable_struct_set", { info, RValue("32"), RValue(5.0) })'
        self.assertIn(tier, place)
        self.assertLess(place.index(tier), place.index("gml_Script_LootGroundCreateFromItem"))
        self.assertIn('" \\"32\\"="', place)
        self.assertIn('" stamp=" + LaTimeStamp(item)', place)

    def test_the_hooks_go_in_only_when_the_core_says_a_character_exists(self):
        # Known Limitations item 8: a hook installed at character select stalls
        # the runner. The one call to LootAnnounceInstall sits behind the
        # core's ShouldInstall, fed g_Setup, a player HhResolveLocalPlayer
        # found, and g_LaInstallTried.
        definition = "static void LootAnnounceInstall()"
        code = _code(self.plugin)
        calls = [m.start() for m in re.finditer(r"\bLootAnnounceInstall\(\)", code)
                 if not code[max(0, m.start() - len("static void ")):m.start()].endswith("static void ")]
        self.assertEqual(len(calls), 1, "LootAnnounceInstall() is called somewhere other than LaInstallIfReady")
        ready = _code(_body(self.plugin, "static void LaInstallIfReady()"))
        self.assertIn("if (g_LootAnnounce.ShouldInstall(g_Setup, havePlayer, g_LaInstallTried)) LootAnnounceInstall();",
                      ready)
        self.assertIn("const bool havePlayer = g_Setup && !g_LaInstallTried && HhResolveLocalPlayer(player);", ready)
        self.assertLess(ready.index("HhResolveLocalPlayer(player)"), ready.index("ShouldInstall("))
        self.assertIn(definition, self.plugin)
        # The command tries at once (in game: installs; at character select:
        # arms only); the tick looks every kInstallPollFrames frames while
        # armed, after its off check.
        command = _code(_body(self.plugin, "static void LootAnnounceCommand(const std::string& rest)"))
        self.assertIn("if (!g_LaInstallTried) LaInstallIfReady();", command)
        self.assertNotIn("LootAnnounceInstall(", command)
        tick = [l.strip() for l in _code(_body(self.plugin, "static void LootAnnounceTick()")).splitlines() if l.strip()]
        self.assertEqual(tick[1], "if (!g_LaInstallTried && ForgePact::LootAnnounceMod::LooksForPlayer(g_LaArmedFrames++)) "
                                  "LaInstallIfReady();")
        self.assertNotIn("LootAnnounceInstall(", "\n".join(tick))


if __name__ == "__main__":
    unittest.main()
