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


if __name__ == "__main__":
    unittest.main()
