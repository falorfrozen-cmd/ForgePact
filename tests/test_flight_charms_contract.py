"""Flight charms (ForgePact #158, phase 2): the plugin wiring the harness cannot see.

tests/test_flight_charms_behavior.py runs the decision core
(plugin/include/ForgePact/FlightCharms.hpp) against a controlled world. This
file reads ModuleMain.cpp as text, comments stripped, and pins the adapter
around it.

The forge part (recognition, dressing, icons, the ground fix and the size
guard):
- the three charms are recognised as Custom Forge built-ins by their seed
  ranges, after every sidecar and signature entry, so a sidecar entry for the
  same item still wins; ForgeCouldMatch lets the ranges through;
- dressing writes the name, rarity, tier, lore, the gold `Flight` row,
  `fp_mechanic` = `flight` and the rolled stats with the native ones removed,
  and arms flight without installing anything inside CreateItemNew;
- on CreateItemNew's outermost return it also writes the footprint ("2" and
  "3") and our bag icon ("11"), before the item's hash is refreshed;
- the six icons load the pack markers' way, lazily and once a session, and a
  failed icon writes nothing;
- the ground icon rides hidden loot's one LootGroundInit detour, as its third
  consumer, and is identified by what the item is (its seed and our bag icon
  under "11"), never by a field it merely carries;
- the size guard's two hooks go in with the Custom Forge's own, by their SDK
  names through HookOneScript; a table-only result turns the guard off and
  says so, and the research build's instruments refuse those two scripts;
- `flightcharm` is a player command with its own early return;
- no address is computed anywhere in it.
"""
import re
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from test_release_hook_contract import function_body, strip_comments, strip_research_blocks  # noqa: E402

ROOT = Path(__file__).resolve().parents[1]
PLUGIN = ROOT / "plugin" / "ModuleMain.cpp"
ICONS = ROOT / "plugin" / "include" / "ForgePact" / "FlightCharmIcons.hpp"

FORGE_READS_START = "// ---- Flight charms (FlightCharms.hpp): what the Custom Forge reads"
FORGE_READS_END = "// ---- end of what the Custom Forge reads for the flight charms"
FORGE_START = "// ---- Flight charms (FlightCharms.hpp): the forge adapter"
FORGE_END = "// ---- end of the flight charms forge adapter"

SIGNATURES = {
    "decode": "static bool FlightCharmDecode(const RValue& item, const RValue& definition, FcNs::Rolled& rolled)",
    "of": "static bool FlightCharmOf(const RValue& item, FcNs::Rolled& rolled)",
    "entry": "static bool FlightCharmForgeEntry(const RValue& item, const RValue& definition, CustomForgeEntry& entry, FcNs::Rolled& rolled)",
    "final": "static void FlightCharmDressFinal(RValue& item, const FcNs::Rolled& rolled)",
    "icon_path": "static bool FlightIconPath(size_t k, std::string& gmlPath, std::string& absolutePath)",
    "icons_load": "static void FlightIconsLoad()",
    "parse_hook": "static RValue& Hook_FlightParseItemToGrid(CInstance* S, CInstance* O, RValue& R, int argc, RValue** A)",
    "add_hook": "static RValue& Hook_FlightGridAddItemPos(CInstance* S, CInstance* O, RValue& R, int argc, RValue** A)",
    "close": "static void FlightGuardClose()",
    "guard_install": "static void FlightGuardInstall()",
    "holds": "static bool FlightGuardHolds(std::string_view runtimeName)",
    "ground": "static void FlightCharmOnGroundInit(int argc, RValue** A)",
    "stat_fields": "static FcNs::StatFields FlightCharmStatFields()",
    "command": "static void FlightCharmCommand(const std::string& rest)",
    "forge": "static bool TryApplyCustomForge(",
    "could_match": "static bool ForgeCouldMatch(",
    "forge_install": "static void InstallCustomForgeItemHooks()",
    "hidden_init": "static RValue& HookHiddenLootInit(CInstance* S, CInstance* O, RValue& R, int argc, RValue** A)",
    "run": "static void RunCommand(const std::string& line)",
}


class FlightCharmsForgeContract(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.plugin = PLUGIN.read_text(encoding="utf-8").replace("\r\n", "\n")
        cls.reads = cls.plugin[cls.plugin.index(FORGE_READS_START):cls.plugin.index(FORGE_READS_END)]
        cls.block = cls.plugin[cls.plugin.index(FORGE_START):cls.plugin.index(FORGE_END)]
        cls.code = strip_comments(cls.reads + cls.block)
        cls.shipped = strip_comments(strip_research_blocks(cls.plugin))

    def body(self, which, source=None):
        return strip_comments(function_body(source if source is not None else self.plugin, SIGNATURES[which]))

    # ---- both builds ------------------------------------------------------------

    def test_the_adapter_ships_in_both_builds(self):
        for name in ("decode", "of", "entry", "final", "icon_path", "icons_load", "parse_hook", "add_hook",
                     "close", "guard_install", "holds", "ground", "stat_fields", "command"):
            with self.subTest(function=name):
                self.assertIn(SIGNATURES[name], strip_research_blocks(self.plugin))
        for header in ("#include <ForgePact/FlightCharms.hpp>", "#include <ForgePact/FlightCharmIcons.hpp>"):
            self.assertIn(header, self.shipped)
        self.assertIn("static ForgePact::FlightCharms::Flight g_Flight;", self.shipped)

    # ---- recognition --------------------------------------------------------------

    def test_recognition_is_the_cores_rule_on_the_items_own_fields(self):
        decode = self.body("decode")
        self.assertIn('FlightNumber(item, "itemType", t)', decode)
        for key in ("a", "b", "c", "j"):
            self.assertIn(f'FlightNumber(definition, "{key}", {key})', decode)
        # A struct or the reference the runner may hand out instead, each
        # through its own builtins: the kind picks the reader, never whether
        # the item is read.
        field = strip_comments(function_body(self.plugin, "static bool FlightField(const RValue& owner, const char* name, RValue& out)"))
        self.assertIn('"variable_struct_get"', field)
        self.assertIn('"variable_instance_get"', field)
        self.assertIn("owner.m_Kind == VALUE_REF", field)
        self.assertIn("FcNs::IsFlightCharm(t, a, b, c, j, rolled)", decode)
        # The type is read first and a non-charm stops there: one read for
        # every other item the game builds.
        self.assertLess(decode.index('"itemType"'), decode.index('"a"'))
        self.assertIn("FcNs::kCharmTypeT", decode)

    def test_sidecar_and_signature_entries_are_matched_first(self):
        forge = self.body("forge")
        # The entries' own loop runs before the ranges are tried, so a sidecar
        # entry for the same item wins, as with the built-ins today.
        self.assertIn("CustomForgeMatches(entry, *candidate, definition)", forge)
        self.assertIn("FlightCharmForgeEntry(*candidate, definition, flightEntry, flightRolled)", forge)
        self.assertLess(forge.index("g_CustomForgeEntries.size()"),
                        forge.index("FlightCharmForgeEntry(*candidate, definition, flightEntry, flightRolled)"))
        self.assertEqual(forge.count("FlightCharmForgeEntry("), 1)
        # The ranges are tried only once every entry missed: the extra pass
        # after the last entry, and the entries' own test skipped for it.
        self.assertIn("for (size_t i = 0; i <= entryCount; ++i)", forge)
        self.assertIn("const bool flightMatched = i == entryCount;", forge)
        self.assertIn("if (flightMatched && !FlightCharmForgeEntry(", forge)
        self.assertIn("if (!flightMatched && !CustomForgeMatches(entry, *candidate, definition)) continue;", forge)

    def test_the_selector_index_lets_the_ranges_through(self):
        could = self.body("could_match")
        self.assertIn("FcNs::DecodeSeed(", could)
        self.assertIn("FcNs::kCharmTypeT", could)
        self.assertIn("FcNs::kCharmBaseB", could)
        # Before the exact index's own early refusal, which a range cannot pass.
        self.assertLess(could.index("FcNs::DecodeSeed("), could.index("g_ForgeSelectorKeys.count("))

    # ---- what dressing writes ----------------------------------------------------

    def test_the_entry_carries_the_charms_row(self):
        entry = self.body("entry")
        self.assertIn("FlightCharmDecode(item, definition, rolled)", entry)
        self.assertIn("FcNs::RowOf(rolled.charm)", entry)
        for field in ("entry.name = std::string(row.name);", "entry.lore = std::string(row.lore);",
                      "entry.rarity = row.rarity;", "entry.tier = row.tier;",
                      "entry.affix = std::string(FcNs::kAffixRow);", "entry.mechanic = std::string(FcNs::kMechanic);",
                      "entry.keepNative = false;", "entry.builtin = true;"):
            with self.subTest(field=field):
                self.assertIn(field, entry)
        self.assertIn("FcNs::StatsFor(rolled)", entry)
        # The selector is the item's own, so the lore key is per seed.
        for key in ('{"t", t}', '{"a", a}', '{"b", b}', '{"c", c}', '{"j", j}'):
            self.assertIn(key, entry)

    def test_dressing_arms_flight_and_installs_nothing_inside_create_item_new(self):
        forge = self.body("forge")
        self.assertIn('if (entry.mechanic == "flight") g_Flight.Arm();', forge)
        for function in ("forge", "entry", "final", "icons_load", "icon_path"):
            text = self.body(function)
            for install in ("HookOneScript", "HookBuiltin", "HiddenLootInstall(", "MmCreateHook"):
                with self.subTest(function=function, install=install):
                    self.assertNotIn(install, text)

    def test_the_final_pass_writes_footprint_and_icon_before_the_hash(self):
        forge = self.body("forge")
        call = "if (finalPass && flightMatched) FlightCharmDressFinal(*candidate, flightRolled);"
        self.assertIn(call, forge)
        self.assertLess(forge.index(call), forge.index("RefreshItemHash(*candidate, &how)"))
        final = self.body("final")
        # Only CreateItemNew's outermost return: an item built inside another
        # item's construction is part of it.
        self.assertIn("if (g_TruthDepth != 0) return;", final)
        self.assertIn('RValue("2"), RValue((double)row.w)', final)
        self.assertIn('RValue("3"), RValue((double)row.h)', final)
        self.assertIn("if (g_FlightDressFootprint)", final)
        self.assertLess(final.index("if (g_FlightDressFootprint)"), final.index('RValue("2")'))
        # The icon only once it loaded; otherwise the charm keeps the Small
        # Charm's own.
        self.assertIn("FlightIconsLoad();", final)
        self.assertIn('RValue("11"), g_FlightIconSprite[k]', final)
        self.assertIn("if (g_FlightIconOk[k])", final)
        self.assertLess(final.index("if (g_FlightIconOk[k])"), final.index('RValue("11")'))
        self.assertIn("static bool g_FlightDressFootprint = true;", self.code)

    # ---- the icons ----------------------------------------------------------------

    def test_the_icons_take_the_pack_markers_route(self):
        path = self.body("icon_path")
        self.assertIn('IPC_DIR + "\\\\flightcharms"', path)
        self.assertIn("if (!std::filesystem::exists(file, ec))", path)
        self.assertLess(path.index("std::filesystem::exists(file, ec)"), path.index("std::ofstream f(file"))
        self.assertIn('std::string("bp_ipc\\\\flightcharms\\\\") + icon.file', path)
        self.assertIn("ForgePact::FlightCharmIcons::kIcons[k]", path)
        load = self.body("icons_load")
        self.assertIn("if (g_FlightIconsTried) return;", load)
        self.assertLess(load.index("if (g_FlightIconsTried) return;"), load.index("g_FlightIconsTried = true;"))
        self.assertIn('CallBuiltin("sprite_add"', load)
        self.assertIn('CallBuiltin("sprite_set_offset"', load)
        self.assertIn("icon.width", load)
        self.assertIn("icon.height", load)
        # The relative path first, the absolute second, as the pack markers do.
        self.assertIn("{ &relative, &absolute }", load)

    def test_the_icon_state_reads_as_the_stat_line_says(self):
        fields = self.body("stat_fields")
        for state in ('"not-loaded"', '"loaded"', '"failed"'):
            self.assertIn(state, fields)
        self.assertIn("s.ground = g_FlightGround;", fields)
        self.assertIn("g_Flight.FillStat(s);", fields)
        self.assertIn("g_FlightGuard.FillStat(s);", fields)

    def test_the_embedded_icons_are_the_six_the_table_names(self):
        header = ICONS.read_text(encoding="utf-8")
        for name in ("pocket_elf", "gabriel_wings", "witch_broomstick"):
            self.assertIn(f'"{name}.png"', header)
            self.assertIn(f'"{name}_ground.png"', header)

    # ---- the ground icon ----------------------------------------------------------

    def test_the_ground_icon_is_the_shared_detours_third_consumer(self):
        hook = self.body("hidden_init")
        self.assertEqual(hook.count("FlightCharmOnGroundInit(argc, A);"), 1)
        self.assertLess(hook.index("g_Orig_LootGroundInit(S, O, R, argc, A)"), hook.index("FlightCharmOnGroundInit(argc, A);"))
        self.assertLess(hook.index(".OnInit("), hook.index("FlightCharmOnGroundInit(argc, A);"))
        self.assertLess(hook.index("LootAnnounceOnInit(S, argc, A);"), hook.index("FlightCharmOnGroundInit(argc, A);"))
        # No detour of its own: LootGroundInit is hooked once, by hidden loot.
        calls = re.findall(r"HookOneScript\w*\([^;]*LootGroundInit\b", strip_comments(self.plugin))
        self.assertEqual(len(calls), 1, calls)
        self.assertNotIn("LootGroundInit", self.body("guard_install"))

    def test_the_ground_consumer_costs_nothing_without_a_ground_icon(self):
        lines = [l.strip() for l in self.body("ground").split("\n") if l.strip()]
        self.assertEqual(lines[0], "if (!g_FlightGroundReady) return;")

    def test_the_ground_item_is_identified_by_what_it_is(self):
        ground = self.body("ground")
        self.assertIn("HeroSiege::Player::kGroundItemInstanceField", ground)
        self.assertIn("FlightCharmOf(item, rolled)", ground)
        # Our own bag icon under "11" is the positive signal that we dressed
        # it; the seed alone would also match an item a sidecar entry claimed.
        self.assertIn('"11"', ground)
        self.assertIn("FlightIconIndex(rolled.charm, false)", ground)
        self.assertIn('RValue("sprite_index"), g_FlightIconSprite[k]', ground)
        self.assertIn("++g_FlightGround;", ground)
        self.assertLess(ground.index("FlightCharmOf(item, rolled)"), ground.index('RValue("sprite_index")'))
        # The kind of argument 0 never decides whether the work happens (the
        # runner hands a reference).
        self.assertNotIn("m_Kind == VALUE_OBJECT", ground)
        self.assertNotIn("m_Kind != VALUE_OBJECT", ground)

    # ---- the size guard ------------------------------------------------------------

    def test_the_guard_installs_with_the_custom_forges_hooks_by_sdk_name(self):
        install = self.body("forge_install")
        self.assertIn("FlightGuardInstall();", install)
        guard = self.body("guard_install")
        for script, orig in (("ParseItemToGrid", "g_Orig_FlightParseItemToGrid"),
                             ("InventoryGridAddItemPos", "g_Orig_FlightGridAddItemPos")):
            with self.subTest(script=script):
                self.assertRegex(guard, r"HookOneScript\(SdkShortScriptName\(HeroSiege::Scripts::gml_Script_"
                                 + script + r"\),[^;]*&" + orig + r", &\w+\)")
        self.assertNotIn("HookOneScriptTable", guard)
        # A table-only (or failed) install turns the guard off and says so.
        self.assertIn("g_FlightGuardOn = ", guard)
        self.assertIn("Out(", guard)
        self.assertIn("TABLE-ONLY", guard)

    def test_the_load_scope_opens_before_and_closes_after_the_original(self):
        parse = self.body("parse_hook")
        self.assertIn("if (!g_FlightGuardOn) return g_Orig_FlightParseItemToGrid(S, O, R, argc, A);", parse)
        self.assertLess(parse.index("g_FlightGuard.Open();"), parse.rindex("g_Orig_FlightParseItemToGrid(S, O, R, argc, A)"))
        # Closed on the way out, and on a throw too.
        self.assertEqual(parse.count("FlightGuardClose();"), 2)
        self.assertIn("catch (...)", parse)
        self.assertIn("throw;", parse)

    def test_a_large_charm_inside_a_load_is_deferred_and_nothing_else(self):
        add = self.body("add_hook")
        self.assertIn("g_FlightGuardOn && g_FlightGuard.Depth() > 0", add)
        self.assertIn("FlightCharmOf(*A[1], rolled)", add)
        self.assertIn("g_FlightGuard.Defer(p, true, w, h)", add)
        # Every other call goes to the game's own function.
        self.assertTrue(add.rstrip().endswith("return g_Orig_FlightGridAddItemPos(S, O, R, argc, A);"))
        self.assertLess(add.index("g_FlightGuard.Depth() > 0"), add.index("FlightCharmOf("))

    def test_the_flush_places_full_then_one_cell_with_the_hash_and_tag(self):
        close = self.body("close")
        self.assertIn("g_FlightGuard.Close(", close)
        self.assertIn('RValue("2"), RValue(1.0)', close)
        self.assertIn('RValue("3"), RValue(1.0)', close)
        self.assertIn("RefreshItemHash(p.item)", close)
        self.assertIn("RValue(std::string(FcNs::kGuardTag)), RValue(true)", close)
        self.assertLess(close.index('RValue("2"), RValue(1.0)'), close.index("RefreshItemHash(p.item)"))
        self.assertIn("FcNs::GuardLine(", close)
        self.assertIn("g_Orig_FlightGridAddItemPos(p.self, p.other, res, (int)ptrs.size(), ptrs.data())", close)

    def test_the_research_instruments_refuse_the_guards_scripts(self):
        holds = self.body("holds")
        self.assertIn("HeroSiege::Scripts::gml_Script_ParseItemToGrid", holds)
        self.assertIn("HeroSiege::Scripts::gml_Script_InventoryGridAddItemPos", holds)
        for signature, resolve in (("static void PpInstall(", "PpResolve(t, why)"),
                                   ("static void CpInstall(", "CpResolve(t, why)")):
            with self.subTest(instrument=signature):
                install = strip_comments(function_body(self.plugin, signature))
                self.assertIn("FlightGuardHolds(t.runtimeName)", install)
                self.assertLess(install.index("FlightGuardHolds(t.runtimeName)"), install.index(resolve))
                self.assertLess(install.index("FlightGuardHolds(t.runtimeName)"), install.index("MmCreateHook("))

    # ---- the command -----------------------------------------------------------------

    def test_the_player_build_accepts_the_verb(self):
        allowlist = re.search(r"kPlayerCommands\s*=\s*\{(?P<body>.*?)\};", self.plugin, re.S).group("body")
        self.assertIn('"flightcharm"', allowlist)
        self.assertIn('"jumpscenery"', allowlist)   # the set read is the real one
        self.assertNotIn('"flightprobe"', allowlist)

    def test_the_verb_is_a_standalone_early_return_in_both_builds(self):
        dispatch = 'if (lc == "flightcharm") { FlightCharmCommand(rest); return; }'
        for source in (self.plugin, strip_research_blocks(self.plugin)):
            run = function_body(source, SIGNATURES["run"])
            self.assertIn(dispatch, run)
            self.assertNotIn('else if (lc == "flightcharm")', run)
        self.assertEqual(self.plugin.count('lc == "flightcharm"'), 1)

    def test_stat_prints_the_cores_line(self):
        command = self.body("command")
        self.assertIn("FcNs::ParseCommand(args, c, why)", command)
        self.assertIn("Out(FcNs::StatLine(FlightCharmStatFields()));", command)
        # The state is refreshed when the line is printed, with the counter
        # every frame advances, so a charm equipped in place reads `state=on`.
        fields = self.body("stat_fields")
        self.assertIn("g_Flight.RefreshForStat((int64_t)g_RuntimeFrame, HhResolveLocalPlayer(player));", fields)
        self.assertLess(fields.index("g_Flight.RefreshForStat("), fields.index("g_Flight.FillStat(s);"))

    # ---- no address -------------------------------------------------------------------

    def test_no_hand_resolved_address(self):
        for text in (self.code,):
            self.assertIsNone(re.search(r"\bk\w*Rva\w*\b", text))
            self.assertIsNone(re.search(r"\(\s*char\s*\*\s*\)\s*\w+\s*\+\s*(?:0x[0-9A-Fa-f]+)", text))
            self.assertNotIn("GetModuleHandle", text)
            self.assertNotIn("MmCreateHook", text)
            self.assertNotIn("HookOneScriptTable", text)

    def test_the_research_icon_stays_in_research_blocks(self):
        for symbol in ("FlightCharmProbeIcon", "bp_ipc\\\\charmprobe"):
            self.assertNotIn(symbol, self.shipped, symbol)


# ---- the lever: flight's answers and drawing in front ---------------------------------

FLIGHT_START = "// ---- Flight charms (FlightCharms.hpp): the flight adapter"
FLIGHT_END = "// ---- end of the flight charms flight adapter"
JS_START = "// ---- Jump through scenery (JumpScenery.hpp): the adapter"
JS_END = "// ---- end of the jump through scenery adapter"
SDK_OBJECTS = ROOT.parent / "hs-game-sdk" / "cpp" / "include" / "hs_game_sdk" / "objects.hpp"

LEVER = {
    "array": "static bool FlightCharmArray(RValue& array)",
    "equipped": "static bool FlightEquipped(unsigned& mask)",
    "overlap": "static bool FlightOverlap(bool& overlapping)",
    "layer_ends": "static bool FlightLayerEnds(int64_t room, FcNs::LayerEnd& first, FcNs::LayerEnd& last)",
    "excluded": "static bool FlightExcludedBlocks(TRoutine orig, const FcNs::HookedRow& h, CInstance* S, CInstance* O, int argc, RValue* Args)",
    "on_builtin": "static void FlightOnBuiltin(int row, RValue& Result, CInstance* S, CInstance* O, int argc, RValue* Args)",
    "tallest": "static RValue& Hook_FlightInstancePlaceTallest(CInstance* S, CInstance* O, RValue& R, int argc, RValue** A)",
    "depth": "static RValue& Hook_FlightUpdateDepth(CInstance* S, CInstance* O, RValue& R, int argc, RValue** A)",
    "install": "static std::string FlightInstall()",
    "install_if_ready": "static void FlightInstallIfReady()",
    "tick": "static void FlightCharmTick()",
    "player": "static void FlightRefreshPlayer()",
    "shared_detour": "static void CollisionBuiltinDetour(int row, RValue& Result, CInstance* S, CInstance* O, int argc, RValue* Args)",
    "shared_install": "static std::string CollisionBuiltinsInstall()",
    "js_builtin": "static void JsOnBuiltin(",
    "js_install": "static std::string JumpSceneryInstall()",
    "holders": "static std::string JumpSceneryResearchHolders(bool forFlight)",
    "held": "static std::string JumpSceneryHeldHooks()",
    "frame": "void FrameCallback(FWFrame& FrameContext)",
}


def sdk_object_index(name):
    if not SDK_OBJECTS.exists():
        raise unittest.SkipTest(f"hs-game-sdk header not found at {SDK_OBJECTS}")
    match = re.search(r"\b" + name + r" = (\d+),", SDK_OBJECTS.read_text(encoding="utf-8"))
    assert match, name
    return match.group(1)


class FlightCharmsLeverContract(unittest.TestCase):
    """The flight adapter: which calls flight answers, and how it gets in.

    Pinned here: the five builtin detours are one shared install, run by
    jumpscenery and by flight, and each detour runs the original once, then
    jumpscenery, then flight; flight answers only the local player's blocked
    query, through the core, with the family and its exclusions named through
    the SDK; InstancePlaceTallest and UpdateDepth are hooked by their SDK names
    and a table-only result refuses; the equipped read is the offline charm
    array, read at the point of use; nothing installs before a charm arms
    flight, and the install runs from the tick once a character resolves; the
    research build's instruments and flight refuse each other's hooks.
    """

    @classmethod
    def setUpClass(cls):
        cls.plugin = PLUGIN.read_text(encoding="utf-8").replace("\r\n", "\n")
        cls.block = cls.plugin[cls.plugin.index(FLIGHT_START):cls.plugin.index(FLIGHT_END)]
        cls.code = strip_comments(cls.block)
        cls.js = strip_comments(cls.plugin[cls.plugin.index(JS_START):cls.plugin.index(JS_END)])
        cls.shipped = strip_comments(strip_research_blocks(cls.plugin))
        cls.shipped_block = strip_comments(strip_research_blocks(cls.block))

    def body(self, which, source=None):
        return strip_comments(function_body(source if source is not None else self.plugin, LEVER[which]))

    def lines(self, which):
        return [line.strip() for line in self.body(which).split("\n") if line.strip()]

    # ---- both builds ---------------------------------------------------------------

    def test_the_flight_adapter_ships_in_both_builds(self):
        player = strip_research_blocks(self.plugin)
        for name in ("array", "equipped", "overlap", "layer_ends", "excluded", "on_builtin", "tallest", "depth",
                     "install", "install_if_ready", "tick", "player", "shared_detour", "shared_install"):
            with self.subTest(function=name):
                self.assertIn(LEVER[name], player)
        # The flight adapter comes after jumpscenery's, whose helpers it reuses.
        self.assertLess(self.plugin.index(JS_END), self.plugin.index(FLIGHT_START))

    # ---- the shared builtin detours ----------------------------------------------------

    def test_the_five_builtins_are_one_shared_install(self):
        shared = self.body("shared_install")
        self.assertIn("HookBuiltin(name, kJsBuiltinHookIds[i], kJsBuiltinDetours[i], &g_JsOrig[i])", shared)
        self.assertLess(shared.index("if (!AddrIsExecutableInModule(GetModuleHandleA(nullptr), p))"),
                        shared.index("HookBuiltin("))
        self.assertIn("if (g_JsOrig[i]) continue;", shared)
        # The one HookBuiltin of these builtins in the whole plugin.
        self.assertEqual(len(re.findall(r"HookBuiltin\([^;]*kJsBuiltinDetours", strip_comments(self.plugin))), 1)
        # Both mods install through it, and flight hooks no builtin itself.
        self.assertIn("CollisionBuiltinsInstall()", self.body("js_install"))
        self.assertIn("CollisionBuiltinsInstall()", self.body("install"))
        self.assertNotIn("HookBuiltin(", self.code)

    def test_each_detour_runs_the_original_once_then_jumpscenery_then_flight(self):
        detour = self.body("shared_detour")
        self.assertEqual(len(re.findall(r"\borig\(", detour)), 1)
        call = detour.index("orig(Result, S, O, argc, Args);")
        self.assertLess(call, detour.index("JsOnBuiltin(row, Result, S, O, argc, Args);"))
        self.assertLess(detour.index("JsOnBuiltin(row, Result, S, O, argc, Args);"),
                        detour.index("FlightOnBuiltin(row, Result, S, O, argc, Args);"))
        # The adapters' own calls inside a detour reach neither mod.
        self.assertLess(call, detour.index("if (g_JsBusy || g_FlightBusy) return;"))
        self.assertLess(detour.index("if (g_JsBusy || g_FlightBusy) return;"), detour.index("JsOnBuiltin("))
        # The builtin detours the macro defines are the shared one.
        self.assertIn("CollisionBuiltinDetour(I, Result, S, O, argc, Args);", self.js)
        # Neither mod refuses because of the other: no Enabled() or Installed()
        # of the other in either install.
        self.assertNotIn("g_JumpScenery", self.body("install"))
        self.assertNotIn("g_Flight", self.body("js_install"))

    # ---- what flight answers ------------------------------------------------------------

    def test_flight_answers_only_the_local_players_blocked_query(self):
        lines = self.lines("on_builtin")
        self.assertEqual(lines[0], "if (!S || S != g_FlightPlayer || g_FlightBusy || !g_Flight.Installed()) return;")
        body = self.body("on_builtin")
        # A free answer stands, before anything is read.
        self.assertIn("if (!FlightBlocked(h.answer, Result)) return;", body)
        self.assertLess(body.index("FlightBlocked(h.answer, Result)"), body.index("g_Flight.Refresh("))
        # The state is refreshed at the point of use, and nothing more is read while off.
        self.assertIn("g_Flight.Refresh(frame);", body)
        self.assertIn("if (g_Flight.CurrentState() != FcNs::State::Off)", body)
        self.assertIn("JsObjectIndexOf(Args[h.objectArg])", body)
        self.assertIn("g_Flight.OnQuery(q, [&]() { return FlightExcludedBlocks(orig, h, S, O, argc, Args); })", body)
        self.assertIn("if (answer != FcNs::Answer::Real) Result = FlightAnswerValue(answer);", body)
        # The rows are jumpscenery's five, in its order.
        self.assertIn("static_assert(FlightRowsMatchJumpScenery()", self.code)

    def test_the_points_and_the_room_are_read_for_the_core(self):
        body = self.body("on_builtin")
        self.assertIn("FlightPoints(row, argc, Args, q);", body)
        points = strip_comments(function_body(self.plugin, "static void FlightPoints("))
        for field in ("q.x0 = ", "q.y0 = ", "q.x1 = ", "q.y1 = "):
            self.assertIn(field, points)
        self.assertIn("FcNs::Hooked::CollisionLine", points)
        install = self.body("install")
        self.assertIn("g_Flight.SetRoomSize(&JsRoomSize);", install)

    def test_the_family_and_its_exclusions_through_the_sdk(self):
        self.assertIn("static constexpr HeroSiege::Objects::GameObject kFlightFamily ="
                      " HeroSiege::Objects::GameObject::Collision_Parent_obj;", self.code)
        excluded = re.search(r"kFlightExcluded\[\] = \{(.*?)\};", self.code, re.S).group(1)
        self.assertEqual(re.findall(r"HeroSiege::Objects::GameObject::(\w+)", excluded), ["Wall_Parent_obj", "Lock_obj"])
        install = self.body("install")
        self.assertIn('"asset_get_index"', install)
        self.assertIn("HeroSiege::Objects::GetObjectName(", install)
        self.assertIn('"object_is_ancestor"', install)
        self.assertIn("g_Flight.SetFamily(", install)
        for name in ("Collision_Parent_obj", "Wall_Parent_obj", "Lock_obj", "Gate_Parent_obj", "Controller_obj"):
            index = sdk_object_index(name)
            self.assertIsNone(re.search(r"\b" + index + r"\b", self.code), name + "'s index is written as a number")

    def test_the_wall_check_reruns_the_same_original_against_each_excluded_object(self):
        excluded = self.body("excluded")
        self.assertIn("for (int e : g_FlightExcludedIdx)", excluded)
        self.assertIn("copy[h.objectArg] = RValue((double)e);", excluded)
        self.assertIn("orig(r, S, O, argc, copy.data());", excluded)

    # ---- InstancePlaceTallest and UpdateDepth ----------------------------------------------

    def test_the_two_scripts_by_sdk_name_and_both_routes(self):
        install = self.body("install")
        for constant, hook, orig in (("gml_Script_InstancePlaceTallest", "Hook_FlightInstancePlaceTallest", "g_FlightOrigIpt"),
                                     ("gml_Script_UpdateDepth", "Hook_FlightUpdateDepth", "g_FlightOrigDepth")):
            with self.subTest(script=constant):
                self.assertRegex(install, r"HookOneScript\(SdkShortScriptName\(HeroSiege::Scripts::" + constant
                                 + r"\),[^;]*\(PVOID\)" + hook + r", &" + orig + r", &\w+\)")
        self.assertNotIn("HookOneScriptTable", self.code)
        self.assertNotIn("MmCreateHook(", self.code)
        self.assertNotIn('"InstancePlaceTallest"', self.code)
        self.assertNotIn('"UpdateDepth"', self.code)
        # A table-only hook would be passed by the game's compiled calls: refuse.
        self.assertIn("if (!g_FlightIptNative)", install)
        self.assertIn("if (!g_FlightDepthNative)", install)

    def test_instance_place_tallest_tests_the_instance_it_answered(self):
        hook = self.body("tallest")
        self.assertIn("RValue& r = g_FlightOrigIpt(S, O, R, argc, A);", hook)
        self.assertLess(hook.index("g_FlightOrigIpt(S, O, R, argc, A)"), hook.index("g_Flight.OnQuery("))
        self.assertIn("if (!S || S != g_FlightPlayer || g_FlightBusy || !g_Flight.Installed()) return r;", hook)
        self.assertIn("FcNs::Hooked::InstancePlaceTallest", hook)
        self.assertIn("FlightInstanceObject(r)", hook)
        # Never re-run: the core's single-argument OnQuery.
        self.assertIn("g_Flight.OnQuery(q)", hook)
        self.assertIn("r = FlightAnswerValue(answer);", hook)

    def test_update_depth_writes_the_front_layer_for_the_player_only(self):
        hook = self.body("depth")
        self.assertIn("RValue& r = g_FlightOrigDepth(S, O, R, argc, A);", hook)
        self.assertLess(hook.index("g_FlightOrigDepth(S, O, R, argc, A)"), hook.index("g_Flight.OnUpdateDepth("))
        lines = self.lines("depth")
        self.assertEqual(lines[1], "if (!S || S != g_FlightPlayer || g_FlightBusy || !g_Flight.Installed()) return r;")
        self.assertIn('RValue("layer")', hook)
        self.assertIn("if (w.write)", hook)
        # yDepthSet stays as the game set it.
        self.assertNotIn("yDepthSet", self.code)
        ends = self.body("layer_ends")
        self.assertIn('"gameLayer"', ends)
        self.assertIn('"layer_get_depth"', ends)
        self.assertIn("g_Flight.SetLayerEnds(&FlightLayerEnds);", self.body("install"))

    # ---- residual, and no warp -----------------------------------------------------------

    def test_residual_reads_the_saved_original_place_meeting_on_the_player(self):
        overlap = self.body("overlap")
        self.assertIn("g_JsOrig[(int)JsNs::Builtin::PlaceMeeting]", overlap)
        self.assertIn("RValue((double)g_FlightFamilyIdx)", overlap)
        self.assertIn("orig(r, g_FlightPlayer, g_FlightPlayer, 3, args);", overlap)
        self.assertIn("g_Flight.SetOverlap(&FlightOverlap);", self.body("install"))

    def test_the_player_is_never_moved(self):
        for name in ('"x"', '"y"'):
            self.assertIsNone(re.search(r'variable_instance_set"\s*,\s*\{[^}]*RValue\(' + name, self.code))
        self.assertNotIn("playerwarp", self.code)

    # ---- the equipped read ------------------------------------------------------------------

    def test_the_equipped_read_is_the_offline_charm_array(self):
        array = self.body("array")
        self.assertIn('GetGlobalVariable(g_Yytk, "mplr")', array)
        self.assertIn("HeroSiege::Player::kOnlineFlagGlobal", array)
        self.assertIn("HeroSiege::Objects::GameObject::Controller_obj", array)
        self.assertIn("HeroSiege::Player::kRelicTabHolderField", array)
        self.assertIn('"localCharmItemArray"', array)
        self.assertIn("HeroSiege::Player::IsInstanceHandle(controller)", array)
        # Online refuses before anything else is read.
        self.assertLess(array.index("kOnlineFlagGlobal"), array.index('"instance_find"'))
        equipped = self.body("equipped")
        self.assertIn("FlightCharmArray(array)", equipped)
        self.assertIn("FlightCharmOf(item, rolled)", equipped)
        self.assertIn("FcNs::kMechanic", equipped)
        self.assertIn("mask |= FcNs::CharmBit(rolled.charm);", equipped)
        self.assertIn("g_Flight.SetEquipped(&FlightEquipped);", self.body("install"))

    def test_the_state_is_refreshed_at_the_point_of_use_never_at_the_frame(self):
        # The tick refreshes the player pointer, never flight's state.
        for refresh in ("g_Flight.Refresh(", "g_Flight.RefreshForStat(", "g_Flight.OnQuery(", "g_Flight.OnUpdateDepth("):
            self.assertNotIn(refresh, self.body("tick"))
            self.assertNotIn(refresh, self.body("player"))
        frame = strip_comments(function_body(self.plugin, LEVER["frame"]))
        self.assertNotIn("g_Flight", frame)

    # ---- arming and the install ----------------------------------------------------------------

    def test_nothing_installs_before_a_charm_arms_flight(self):
        tick = self.lines("tick")
        self.assertEqual(tick[0], "if (!g_Flight.Armed()) return;")
        body = self.body("tick")
        self.assertIn("if (FcNs::Flight::LooksForPlayer(g_FlightArmedFrames++)) FlightInstallIfReady();", body)
        self.assertLess(body.index("if (!g_Flight.Installed())"), body.index("FlightRefreshPlayer();"))
        for source in (self.plugin, strip_research_blocks(self.plugin)):
            frame = strip_comments(function_body(source, LEVER["frame"]))
            self.assertEqual(frame.count("FlightCharmTick();"), 1)
            self.assertIn("if (g_Setup) FlightCharmTick();", frame)
        # The install path: from the tick alone, and every hook inside it.
        self.assertEqual(self.shipped.count("FlightInstall()"), 2, "the definition and one call")
        self.assertEqual(self.shipped.count("FlightInstallIfReady("), 2, "the definition and the tick")
        install = self.body("install")
        for call in ("HookOneScript(", "CollisionBuiltinsInstall("):
            self.assertEqual(self.code.count(call), install.count(call), call + " outside FlightInstall")

    def test_the_install_waits_for_a_character_and_refuses_once(self):
        ready = self.body("install_if_ready")
        self.assertIn("g_Flight.ShouldInstall(g_Setup, ", ready)
        self.assertIn("FlightPlayerResolves()", ready)
        self.assertLess(ready.index("FlightPlayerResolves()"), ready.index("FlightInstall();"))
        self.assertLess(ready.index("FlightInstall();"), ready.index("g_Flight.NoteInstall(why.empty());"))
        self.assertIn("Out(FcNs::RefusedLine(why));", ready)
        self.assertEqual(self.shipped.count("FcNs::RefusedLine(why)"), 1)
        # The ground icon rides hidden loot's one LootGroundInit detour.
        self.assertIn("if (!g_HiddenLootInstallTried) HiddenLootInstall();", ready)
        resolves = strip_comments(function_body(self.plugin, "static bool FlightPlayerResolves()"))
        self.assertIn("HhResolveLocalPlayer(p)", resolves)
        self.assertIn("HhResolveInstance(p)", resolves)
        player = self.body("player")
        self.assertIn("HhResolveLocalPlayer(p)", player)

    # ---- the research build, both directions -------------------------------------------------

    def test_flight_refuses_while_a_research_hook_holds_one_of_its_six(self):
        install = self.body("install")
        self.assertIn("JumpSceneryResearchHolders(true)", install)
        self.assertLess(install.index("JumpSceneryResearchHolders(true)"), install.index("HookOneScript("))
        self.assertNotIn("JumpSceneryResearchHolders", self.shipped)
        holders = self.body("holders")
        self.assertIn("g_JpRows[kJpRow_InstancePlaceTallest].installed", holders)
        self.assertIn("g_JpRows[kJpRow_SkillsLeap].installed", holders)
        self.assertIn("JumpSceneryResearchHolders(false)", self.body("js_install"))

    def test_the_research_instruments_refuse_while_flight_holds_them(self):
        held = self.body("held")
        self.assertIn("FlightHeldScripts()", held)
        self.assertIn("g_JsOrig[i]", held)
        scripts = strip_comments(function_body(self.plugin, "static std::string FlightHeldScripts()"))
        self.assertIn("g_FlightOrigIpt", scripts)
        self.assertIn("g_FlightOrigDepth", scripts)
        self.assertNotIn("JumpSceneryHeldHooks", self.shipped)
        self.assertNotIn("FlightHeldScripts", self.shipped)

    # ---- no address ---------------------------------------------------------------------------

    def test_no_hand_resolved_address(self):
        self.assertIsNone(re.search(r"\bk\w*Rva\w*\b", self.code))
        self.assertIsNone(re.search(r"\(\s*char\s*\*\s*\)\s*\w+\s*\+", self.code))
        self.assertIsNone(re.search(r"0x[0-9A-Fa-f]{5,}", self.code))
        self.assertNotIn("GetModuleHandle", self.code)
        # Not even a mention of the research probe ships in it.
        self.assertNotIn("charmprobe", strip_research_blocks(self.block))


if __name__ == "__main__":
    unittest.main()
