"""Loot announcements (`lootann`): the plugin wiring and the rules the core must keep.

tests/test_loot_announce_behavior.py runs the real core against controlled
drops. This file pins what that harness cannot see: the announced rarity set
in the header is exactly {9, 7, 10, 6, 5}; the verb is a player command with
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
                       "LaProbeAttach", "LaProbePlace", "LaProbeTry", "LaProbeSay",
                       "LaProbeAnonControl", "LaProbeUnresolvedRows")

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


class LootAnnounceHeaderTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.code = _code(HEADER.read_text(encoding="utf-8").replace("\r\n", "\n"))

    def test_the_announced_rarity_set_is_heroic_angelic_unholy_satanic_mythic(self):
        # Satanic (6) and Mythic (5) joined on the owner's word, 2026-10-08.
        constants = dict((name, int(value)) for name, value in
                         re.findall(r"static constexpr int (k\w+) = (-?\d+);", self.code))
        members = re.search(r"kAnnouncedRarities\s*=\s*\{([^}]*)\}", self.code)
        self.assertIsNotNone(members, "kAnnouncedRarities is not an enumerable list in the header")
        names = [m.strip() for m in members.group(1).split(",") if m.strip()]
        values = [constants[n] if n in constants else int(n) for n in names]
        self.assertEqual(len(values), 5, values)
        self.assertEqual(set(values), {9, 7, 10, 6, 5})
        self.assertRegex(self.code, r"std::array<int, 5> kAnnouncedRarities")

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
                                  "held-bag-drop", "sink-refused", "remembered", "created", "create-overflow"])


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
        self.assertIn('LaField(item, "itemInfoStruct", info)', rarity)
        self.assertIn('LaField(info, "27", code)', rarity)
        self.assertIn("kRarityUnread", rarity)
        for forbidden in ('"c"', "level", "relicLevel", '"b"'):
            self.assertNotIn(forbidden, rarity, forbidden)
        process = _code(_body(self.plugin, "static void LaProcess(const LaPending& p)"))
        self.assertIn("HeroSiege::Player::kGroundItemInstanceField", process)
        self.assertIn("HeroSiege::Objects::GameObject::Loot_Ground_obj", process)
        self.assertIn("const bool hasKey = LaItemKey(item, key);", process)
        self.assertIn("if (!hasKey) ++g_LaNoKey;", process)
        self.assertIn("const bool recent = hasKey && g_LootAnnounce.RecentlyCreated(key);", process)
        self.assertIn("g_LootAnnounce.Decide(LaRarity(item), recent, (int64_t)id, type, stamp)", process)
        # An identity that could not be read is counted and left before the
        # core's memory sees it: "unread" must never compare equal to "unread".
        self.assertNotIn("double id = -1.0;", process)
        self.assertIn("idRead = id >= 0.0;", process)
        refuse = "if (!ForgePact::LootAnnounceMod::Identifiable(idRead, type, stamp)) { ++g_LaNoIdentity; return; }"
        self.assertIn(refuse, process)
        self.assertLess(process.index(refuse), process.index("g_LootAnnounce.Decide("))
        item_type = _code(_body(self.plugin, "static std::string LaItemType(const RValue& item)"))
        self.assertIn('LaField(item, "itemType", t)', item_type)
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
                                  "queue-full"])
        install = '" install=" + g_LootAnnounce.InstallState(g_LaInstallTried)'
        self.assertIn(install, stat)
        # `lootann 1` names both hooks' routes, then whether they are in.
        self.assertIn('" init-hook=" + LaInitRoute() + " create-hook=" + g_LaCreateRoute', command)
        reply = re.search(r'" init-hook=" \+ LaInitRoute\(\) \+ " create-hook=" \+ g_LaCreateRoute\s*\+ (.*?)\);', command,
                          flags=re.S)
        self.assertIsNotNone(reply, "the lootann 1 reply does not continue after create-hook=")
        self.assertEqual(reply.group(1).strip(), install)

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
