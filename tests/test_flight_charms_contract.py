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

The tooltip part (FlightCharmsTooltipContract: the gold row on an item with no
stats, anchored through ForgedTooltipRows.hpp), the lever part
(FlightCharmsLeverContract) and the drop part
(FlightCharmsDropContract, the three drop switches on #74's machinery; its
behaviour runs in tests/test_angelic_hit_behavior.py's flight scenarios) say
what they pin in their own docstrings.
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
        # (ForgePact #166's Mark 1 pieces take one more pass after it, which
        # tests/test_mark1_set_contract.py pins.)
        self.assertIn("for (size_t i = 0; i <= entryCount + 1; ++i)", forge)
        self.assertIn("const bool flightMatched = i == entryCount;", forge)
        self.assertIn("if (flightMatched && !FlightCharmForgeEntry(", forge)
        self.assertIn("if (!flightMatched && !mark1Matched && !CustomForgeMatches(entry, *candidate, definition)) continue;", forge)

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

    def test_the_bag_icon_goes_in_only_where_its_ground_route_can_follow(self):
        # #216 c5: a table-only or refused LootGroundInit route would leave our
        # sprite under "11" drawing Error_spr on the ground, so the dressing asks
        # Mark 1's rule first (called, never copied); the footprint is unchanged.
        final = self.body("final")
        check = "if (!M1sNs::GroundRouteAllowsArt(FlightGroundRoute())) return;"
        self.assertEqual(final.count(check), 1)
        self.assertLess(final.index('RValue("3"), RValue((double)row.h)'), final.index(check))
        self.assertLess(final.index(check), final.index("FlightIconsLoad();"))
        self.assertLess(final.index(check), final.index('RValue("11")'))
        self.assertLess(final.index(check), final.rindex("variable_struct_set"))
        # The route is hidden loot's own, with its install state, through
        # Mark 1's GroundRouteName: never a literal, never "installed" alone.
        route = strip_comments(function_body(self.plugin, "static std::string FlightGroundRoute()"))
        self.assertIn("M1sNs::GroundRouteName(g_HiddenLootInstallTried, ForgePact::HiddenLootMod::Instance().RouteName())",
                      route)
        # Negative control: no route text of its own, so `both` can only come
        # from HookOneScript's native flag via HiddenLootMod.
        for literal in ('"both"', '"table-only"', '"none"', '"not-installed"'):
            with self.subTest(literal=literal):
                self.assertNotIn(literal, route)
                self.assertNotIn(literal, final)
        # No second copy of the rule anywhere in the plugin: the one definition
        # stays in Mark1Set.hpp.
        self.assertNotIn("bool GroundRouteAllowsArt(", strip_comments(self.plugin))
        rule = (ROOT / "plugin" / "include" / "ForgePact" / "Mark1Set.hpp").read_text(encoding="utf-8")
        self.assertIn('return route == kGroundRouteNotInstalled || route == "both";', rule)

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
        # ForgePact #185's tracker comes after it, the fourth.
        self.assertLess(hook.index("FlightCharmOnGroundInit(argc, A);"), hook.index("TrackerOnInit(S, argc, A);"))
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

    def test_the_guard_line_says_how_far_a_load_got(self):
        # `flightcharm stat` prints the guard's own line after the stat line,
        # whose fresh-session text stays as it is.
        command = self.body("command")
        guard_line = "Out(FcNs::GuardStatLine(g_FlightGuardRoute, g_FlightGuard.Stats()));"
        self.assertIn(guard_line, command)
        self.assertLess(command.index("Out(FcNs::StatLine(FlightCharmStatFields()));"), command.index(guard_line))
        # Every placement inside a load is counted before it can be deferred,
        # so `seen=` and `charms=` say whether the argument layout held.
        add = self.body("add_hook")
        self.assertIn("g_FlightGuard.Saw(charm);", add)
        self.assertLess(add.index("g_FlightGuard.Saw("), add.index("g_FlightGuard.Defer("))
        # The route names the install's result: on, table-only or refused.
        install = self.body("guard_install")
        route = re.search(r"g_FlightGuardRoute = ([^;]*);", install)
        self.assertIsNotNone(route, install)
        for value in ('"on"', '"table-only"', '"refused"'):
            with self.subTest(route=value):
                self.assertIn(value, route.group(1))
        self.assertIn('static const char* g_FlightGuardRoute = "not-installed";', self.code)

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
        # The stat path never confirms the player (#216 c3): the confirm mark
        # is the hooks' alone, so ` player=` reports what they set. It refreshes
        # only while the pointer the overlap read uses as self is the local
        # player or none, never through a pointer the hooks have not confirmed.
        fields = self.body("stat_fields")
        self.assertIn("g_Flight.RefreshForStat((int64_t)g_RuntimeFrame, FlightStatPlayerReady());", fields)
        self.assertNotIn("FlightConfirmPlayer(", fields)
        self.assertNotIn("ConfirmsPlayer(", fields)
        self.assertNotIn("FlightRefreshPlayer(", fields)
        self.assertNotIn("g_FlightPlayer", fields)
        self.assertNotIn("HhResolveLocalPlayer(", fields)
        self.assertLess(fields.index("g_Flight.RefreshForStat("), fields.index("g_Flight.FillStat(s);"))
        ready = strip_comments(function_body(self.plugin, "static bool FlightStatPlayerReady()"))
        self.assertLess(ready.index("g_FlightBusy = true;"), ready.index("HhResolveLocalPlayer(p)"))
        self.assertLess(ready.index("HhResolveLocalPlayer(p)"), ready.index("g_FlightBusy = false;"))
        self.assertIn("now = HhResolveInstance(p);", ready)
        self.assertIn("return now != nullptr && (g_FlightPlayer == nullptr || g_FlightPlayer == now);", ready)
        self.assertIsNone(re.search(r"g_FlightPlayer\s*=(?!=)", ready), "the stat path never sets the player")
        for touch in ("ConfirmsPlayer(", "FlightConfirmPlayer(", "FlightRefreshPlayer("):
            with self.subTest(ready_never=touch):
                self.assertNotIn(touch, ready)
        # The trace line's ` player=` is checked before the stat line is built,
        # so nothing the stat path does can set what it reports.
        self.assertIn("const char* const player = FlightPlayerCheck();", command)
        self.assertLess(command.index("FlightPlayerCheck("), command.index("FlightCharmStatFields("))
        self.assertIn("Out(FcNs::TraceLine(player, g_Flight.Stats()));", command)
        self.assertEqual(command.count("FlightPlayerCheck("), 1)

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
    "layer_all": "static bool FlightLayerAll(std::vector<FcNs::LayerEnd>& all)",
    "layer_detail": "static bool FlightLayerDetail(int64_t id, FcNs::RoomLayer& layer)",
    "layer_effect": "static bool FlightLayerEffect(int64_t id, bool& effect)",
    "layer_count": "static bool FlightLayerCount(int64_t id, bool& visible, int64_t& elements)",
    "layer_place": "static bool FlightLayerPlace(double depth, int64_t& id)",
    "layer_move": "static bool FlightLayerMove(int64_t id, double depth)",
    "layer_valid": "static bool FlightLayerValid(int64_t id)",
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
        for name in ("array", "equipped", "overlap", "layer_ends", "layer_all", "layer_detail", "layer_effect",
                     "layer_count", "layer_place", "layer_move", "layer_valid", "excluded", "on_builtin", "tallest",
                     "depth", "install", "install_if_ready", "tick", "player", "shared_detour", "shared_install"):
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
        self.assertEqual(lines[0], "if (!S || g_FlightBusy || !g_Flight.Installed()) return;")
        # The player is confirmed at the frame's first hooked call (#216 c3),
        # right before the compare: one line, after the row bound.
        self.assertEqual(lines[1:4], ["if (row < 0 || row >= JsNs::kBuiltinCount) return;",
                                      "FlightConfirmPlayer();",
                                      "if (S != g_FlightPlayer) { g_Flight.NoteCall(false); return; }"])
        # Any self but the local player is counted (the trace line's not-player=)
        # and returns before anything is answered.
        body = self.body("on_builtin")
        self.assertLess(body.index("if (S != g_FlightPlayer)"), body.index("FlightBlocked(h.answer, Result)"))
        # A free answer stands, before anything is read, and is counted (free=).
        self.assertIn("if (!FlightBlocked(h.answer, Result)) { g_Flight.NoteCall(true); return; }", body)
        self.assertLess(body.index("FlightBlocked(h.answer, Result)"), body.index("g_Flight.StateAt("))
        # The state is refreshed at the point of use, in one call (#216 c4),
        # and nothing more is read while off.
        self.assertIn("const FcNs::State state = g_Flight.StateAt(frame);", body)
        self.assertNotIn("g_Flight.Refresh(", body)
        self.assertNotIn("g_Flight.CurrentState()", body)
        self.assertIn("if (state != FcNs::State::Off)", body)
        self.assertLess(body.index("g_Flight.StateAt(frame)"), body.index("if (state != FcNs::State::Off)"))
        self.assertIn("JsObjectIndexOf(Args[h.objectArg])", body)
        self.assertIn("g_Flight.OnQuery(q, [&]() { return FlightExcludedBlocks(orig, h, S, O, argc, Args); })", body)
        self.assertIn("if (answer != FcNs::Answer::Real) Result = FlightAnswerValue(answer);", body)
        # The rows are jumpscenery's five, in its order.
        self.assertIn("static_assert(FlightRowsMatchJumpScenery()", self.code)

    def test_every_early_return_reaches_a_counter(self):
        # Before the call reaches the core, every return either counts the
        # call (NoteCall, NoteDepthOther) or is one of the guards
        # FlightCounters names as uncounted: no self, flight's own re-entry,
        # flight not installed, and FlightOnBuiltin's row bound.
        uncounted = {
            "on_builtin": {"if (!S || g_FlightBusy || !g_Flight.Installed()) return;",
                           "if (row < 0 || row >= JsNs::kBuiltinCount) return;"},
            "tallest": {"if (!S || g_FlightBusy || !g_Flight.Installed()) return r;"},
            "depth": {"if (!S || g_FlightBusy || !g_Flight.Installed()) return r;"},
        }
        for which, allowed in uncounted.items():
            with self.subTest(function=which):
                lines = self.lines(which)
                core = next(i for i, line in enumerate(lines)
                            if "g_Flight.OnQuery(" in line or "g_Flight.OnUpdateDepth(" in line)
                early = [line for line in lines[:core] if re.search(r"\breturn\b", line)]
                counted = [line for line in early if line not in allowed]
                self.assertTrue(allowed <= set(early), early)
                self.assertTrue(counted, early)
                for line in counted:
                    self.assertRegex(line, r"g_Flight\.(?:NoteCall|NoteDepthOther)\(", line)
        shipped = strip_comments(self.plugin)
        self.assertGreaterEqual(len(re.findall(r"g_Flight\.NoteCall\(", shipped)), 1)
        self.assertGreaterEqual(len(re.findall(r"g_Flight\.NoteDepthOther\(", shipped)), 1)

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
        lines = self.lines("tallest")
        # The player is confirmed at the frame's first hooked call (#216 c3),
        # between the guard and the compare.
        self.assertEqual(lines[1:4], ["if (!S || g_FlightBusy || !g_Flight.Installed()) return r;",
                                      "FlightConfirmPlayer();",
                                      "if (S != g_FlightPlayer) { g_Flight.NoteCall(false); return r; }"])
        self.assertIn("if (!FlightBlocked(FcNs::Answer::Noone, r)) { g_Flight.NoteCall(true); return r; }", hook)
        self.assertLess(hook.index("if (S != g_FlightPlayer)"), hook.index("g_Flight.OnQuery("))
        # The state in one call (#216 c4), after the free answer stood.
        self.assertIn("const FcNs::State state = g_Flight.StateAt(frame);", hook)
        self.assertNotIn("g_Flight.Refresh(", hook)
        self.assertNotIn("g_Flight.CurrentState()", hook)
        self.assertIn("if (state != FcNs::State::Off)", hook)
        self.assertLess(hook.index("FlightBlocked(FcNs::Answer::Noone, r)"), hook.index("g_Flight.StateAt("))
        self.assertIn("FcNs::Hooked::InstancePlaceTallest", hook)
        self.assertIn("FlightInstanceObject(r)", hook)
        # Never re-run: the core's single-argument OnQuery.
        self.assertIn("g_Flight.OnQuery(q)", hook)
        self.assertIn("r = FlightAnswerValue(answer);", hook)

    def test_update_depth_writes_the_flight_layer_for_the_player_only(self):
        hook = self.body("depth")
        self.assertIn("RValue& r = g_FlightOrigDepth(S, O, R, argc, A);", hook)
        # The write comes after the game's own UpdateDepth.
        self.assertLess(hook.index("g_FlightOrigDepth(S, O, R, argc, A)"), hook.index("g_Flight.OnUpdateDepth("))
        lines = self.lines("depth")
        self.assertEqual(lines[1], "if (!S || g_FlightBusy || !g_Flight.Installed()) return r;")
        # The player is confirmed at the frame's first hooked call (#216 c3).
        self.assertEqual(lines[2], "FlightConfirmPlayer();")
        # Any other self is counted (depth-other=) and never written.
        self.assertEqual(lines[3], "if (S != g_FlightPlayer) { g_Flight.NoteDepthOther(); return r; }")
        self.assertIn('RValue("layer")', hook)
        self.assertIn("if (w.write)", hook)
        self.assertLess(hook.index("g_Flight.OnUpdateDepth("), hook.index('RValue("layer")'))
        # The layer written is the one the core chose, as the runtime answered it.
        self.assertIn("FlightLayerArg(w.layer)", hook)
        # yDepthSet stays as the game set it.
        self.assertNotIn("yDepthSet", self.code)
        ends = self.body("layer_ends")
        self.assertIn('"gameLayer"', ends)
        self.assertIn('"layer_get_depth"', ends)
        self.assertIn("g_Flight.SetLayerEnds(&FlightLayerEnds);", self.body("install"))

    # ---- the flight layer: named, validated, never per frame -------------------------------

    CORE = ROOT / "plugin" / "include" / "ForgePact" / "FlightCharms.hpp"
    LAYER_OPS = (("all", "layer_all"), ("detail", "layer_detail"), ("effect", "layer_effect"), ("count", "layer_count"),
                 ("place", "layer_place"), ("move", "layer_move"), ("valid", "layer_valid"))

    def core(self):
        return strip_comments(self.CORE.read_text(encoding="utf-8").replace("\r\n", "\n"))

    def test_the_flight_layer_is_named_found_and_created_only_by_name(self):
        place = self.body("layer_place")
        # Looked up by name first, created by name only when the room holds none.
        self.assertIn('"layer_get_id", { name }', place)
        self.assertIn('"layer_create", { RValue(depth), name }', place)
        self.assertLess(place.index('"layer_get_id"'), place.index('"layer_create"'))
        self.assertIn("RValue(std::string(FcNs::kFlightLayerName))", place)
        # The name is the core's one constant; the adapter never spells it.
        self.assertIn('inline constexpr std::string_view kFlightLayerName = "fp_flight";', self.core())
        self.assertNotIn('"fp_flight"', strip_comments(self.plugin))
        # The one layer_create, and the one layer_get_id, in the whole plugin.
        shipped = strip_comments(self.plugin)
        self.assertEqual(shipped.count('"layer_create"'), 1)
        self.assertEqual(shipped.count('"layer_get_id"'), 1)
        # No layer id is written as a number: each comes back from the runtime.
        self.assertIn("FlightLayerId(layer, found)", place)
        self.assertIn("FlightLayerArg(id)", self.body("layer_move"))

    def test_the_flight_layer_is_validated_where_it_is_written(self):
        valid = self.body("layer_valid")
        self.assertIn('"layer_exists"', valid)
        self.assertIn('"layer_get_name"', valid)
        self.assertIn("std::string(FcNs::kFlightLayerName)", valid)
        self.assertLess(valid.index('"layer_exists"'), valid.index('"layer_get_name"'))
        # The core checks it before each write: the write path of OnUpdateDepth
        # asks LayerValid before it counts a write, and a failed check runs a
        # new visit's census.
        update = function_body(self.core(), "DepthWrite OnUpdateDepth(int64_t frame, bool playerSelf, int64_t room)")
        self.assertIn("LayerValid(visit_.layer)", update)
        self.assertLess(update.index("LayerValid(visit_.layer)"), update.index("++counters_.depth;"))
        self.assertIn("Census(room, now);", update)
        self.assertIn("ops.valid = &FlightLayerValid;", self.body("install"))

    def test_no_census_and_no_layer_is_made_per_frame(self):
        install = self.body("install")
        shipped = strip_comments(self.plugin)
        for field, which in self.LAYER_OPS:
            function = "FlightLayer" + field.capitalize()
            with self.subTest(op=field):
                # Each op is handed to the core once, in the install, and the
                # adapter never calls one itself.
                self.assertIn("ops." + field + " = &" + function + ";", install)
                self.assertEqual(len(re.findall(r"\b" + function + r"\b", shipped)), 2, "the definition and the install")
        self.assertIn("g_Flight.SetLayerOps(std::move(ops));", install)
        for source in (self.plugin, strip_research_blocks(self.plugin)):
            for which in ("frame", "tick", "player"):
                body = strip_comments(function_body(source, LEVER[which]))
                for call in ('"layer_get_all"', '"layer_create"', '"layer_get_fx"', '"layer_depth"', "FlightLayer"):
                    self.assertNotIn(call, body, which)
        # In the core: layer_get_all and the placement run only in the census;
        # the recount and the move only in the re-choice, which is due once a
        # second; layer_get_fx only in ReadEffect, which only those two call.
        core = self.core()
        census = function_body(core, "void Census(int64_t room, double now)")
        rechoose = function_body(core, "void Rechoose(double now)")
        read_effect = function_body(core, "EffectRead ReadEffect(RoomLayer& l)")
        for op, where in (("all", census), ("effect", read_effect), ("place", census), ("detail", census),
                          ("count", rechoose), ("move", rechoose)):
            with self.subTest(core_op=op):
                self.assertEqual(core.count("ops_." + op + ","), 1, op)
                self.assertIn("ops_." + op + ",", where)
        self.assertEqual(core.count("ReadEffect(l)"), 2, "the census and the re-choice, and nothing else")
        self.assertIn("ReadEffect(l)", census)
        self.assertIn("ReadEffect(l)", rechoose)
        self.assertIn("if (now - choseAt_ < kRechoiceMs) return;", rechoose)
        self.assertIn("inline constexpr double kRechoiceMs = 1000.0;", core)

    def test_the_effect_is_read_through_layer_get_fx_by_name_for_a_visible_empty_layer(self):
        shipped = strip_comments(self.plugin)
        self.assertEqual(shipped.count('"layer_get_fx"'), 1)
        effect = self.body("layer_effect")
        self.assertIn('g_Yytk->CallBuiltin("layer_get_fx", { FlightLayerArg(id) })', effect)
        # A struct answer is an effect, anything else none; a throw fails the read.
        self.assertIn("effect = fx.m_Kind == VALUE_OBJECT;", effect)
        # The core reads it for a visible, empty layer only: in the census, and
        # (PR #214 review) in the re-choice for a remembered layer that became
        # visible and empty without its effect ever having been read.
        core = self.core()
        census = strip_comments(function_body(core, "void Census(int64_t room, double now)"))
        self.assertIn("if (l.visible && l.elements == 0) {", census)
        self.assertLess(census.index("if (l.visible && l.elements == 0) {"), census.index("ReadEffect(l)"))
        rechoose = strip_comments(function_body(core, "void Rechoose(double now)"))
        self.assertIn("if (l.visible && l.elements == 0 && !l.effectRead) ReadEffect(l);", rechoose)
        read_effect = strip_comments(function_body(core, "EffectRead ReadEffect(RoomLayer& l)"))
        self.assertIn("const bool read = Call(ops_.effect, l.id, effect);", read_effect)
        self.assertIn("l.effectRead = true;", read_effect)
        self.assertIn("l.effect = read && effect;", read_effect)
        # Every read lands in one of the session's three counts, which end the trace line.
        for count in ("++counters_.fxFailed;", "++counters_.fxStruct;", "++counters_.fxOther;"):
            self.assertIn(count, read_effect)
        self.assertEqual(core.count("Call(ops_.effect"), 1, "the effect is read in ReadEffect only")
        trace = function_body(self.core(), "inline std::string TraceLine(std::string_view player, const FlightCounters& c)")
        self.assertLess(trace.index('" no-front="'), trace.index('" fx-struct="'))
        self.assertLess(trace.index('" fx-struct="'), trace.index('" fx-other="'))
        self.assertLess(trace.index('" fx-other="'), trace.index('" fx-failed="'))

    # ---- residual, and no warp -----------------------------------------------------------

    def test_residual_reads_the_saved_original_place_meeting_on_the_player(self):
        overlap = self.body("overlap")
        self.assertIn("g_JsOrig[(int)JsNs::Builtin::PlaceMeeting]", overlap)
        self.assertIn("RValue((double)g_FlightFamilyIdx)", overlap)
        self.assertIn("orig(r, g_FlightPlayer, g_FlightPlayer, 3, args);", overlap)
        self.assertIn("g_Flight.SetOverlap(&FlightOverlap);", self.body("install"))

    # ---- monsters (the owner's scope, 2026-10-13: body collision only) ----------------------

    def test_the_monster_resolves_by_its_sdk_name_beside_the_family(self):
        self.assertIn("static constexpr HeroSiege::Objects::GameObject kFlightMonster ="
                      " HeroSiege::Objects::GameObject::Enemy_Parent_obj;", self.code)
        install = self.body("install")
        self.assertIn("HeroSiege::Objects::GetObjectName(kFlightMonster)", install)
        # Resolved beside the family, before the core is configured, and handed to it.
        self.assertLess(install.index("GetObjectName(kFlightFamily)"), install.index("GetObjectName(kFlightMonster)"))
        self.assertIn("g_FlightMonsterIdx = monster;", install)
        self.assertIn("g_Flight.SetMonster(monster);", install)
        self.assertLess(install.index("g_Flight.SetFamily("), install.index("g_Flight.SetMonster(monster);"))
        self.assertIsNone(re.search(r"\b" + sdk_object_index("Enemy_Parent_obj") + r"\b", self.code),
                          "Enemy_Parent_obj's index is written as a number")
        # The monster object is not one of the objects flight never passes.
        excluded = re.search(r"kFlightExcluded\[\] = \{(.*?)\};", self.code, re.S).group(1)
        self.assertNotIn("Enemy_Parent_obj", excluded)

    def test_an_unresolved_monster_refuses_flight_with_the_one_line(self):
        install = self.body("install")
        # The monster's name joins `missing`, so the existing one-line refusal names it,
        # and the refusal returns before anything is configured.
        self.assertRegex(install, r"if \(monster < 0\) missing \+= \(missing\.empty\(\) \? \"\" : \", \"\) \+ "
                                  r"std::string\(HeroSiege::Objects::GetObjectName\(kFlightMonster\)\);")
        self.assertEqual(install.count('" did not resolve by name; flight stays off"'), 1)
        refusal = install.index('" did not resolve by name; flight stays off"')
        self.assertLess(install.index("if (monster < 0)"), refusal)
        self.assertLess(refusal, install.index("g_FlightMonsterIdx = monster;"))
        self.assertLess(refusal, install.index("g_Flight.SetMonster(monster);"))

    def test_the_overlap_read_asks_the_monster_after_the_family_through_the_same_original(self):
        overlap = self.body("overlap")
        self.assertEqual(overlap.count("g_JsOrig[(int)JsNs::Builtin::PlaceMeeting]"), 1)
        self.assertIn("g_FlightMonsterIdx < 0", overlap)
        family = overlap.index("g_FlightFamilyIdx")
        monster = overlap.index("RValue((double)g_FlightMonsterIdx)")
        self.assertLess(overlap.index("RValue((double)g_FlightFamilyIdx)"), monster)
        self.assertLess(family, monster)
        # Both reads are the saved original on the player, as self and other, at its own x and y.
        self.assertEqual(overlap.count("orig(r, g_FlightPlayer, g_FlightPlayer, 3, args);"), 1)
        self.assertNotIn('"place_meeting"', overlap)
        self.assertNotIn("Enemy_Parent_obj", overlap)

    def test_the_monster_comment_no_longer_says_monsters_block(self):
        head = self.block[:self.block.index("kFlightExcluded[] = {")]
        head = head[head.rindex("kFlightFamily ="):]
        self.assertNotIn("keep blocking", head)
        self.assertIn("Enemy_Parent_obj", head)

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
        self.assertIn("if (!g_Flight.Installed())", body)
        # Installed, the tick no longer refreshes the player (#216 c3): the
        # hooks confirm it at the first hooked call of each frame instead.
        self.assertNotIn("FlightRefreshPlayer(", body)
        self.assertNotIn("FlightConfirmPlayer(", body)
        self.assertNotIn("g_FlightPlayer", body)
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
        resolves = strip_comments(function_body(self.plugin, "static bool FlightPlayerResolves()"))
        self.assertIn("HhResolveLocalPlayer(p)", resolves)
        self.assertIn("HhResolveInstance(p)", resolves)
        player = self.body("player")
        self.assertIn("HhResolveLocalPlayer(p)", player)

    def test_the_hooks_install_only_for_a_worn_charm_and_the_ground_detour_first(self):
        # #216 b2: armed by any dressed charm, but flight's own hooks go in
        # only at a look that finds a flight charm worn; the ground icon's
        # detour goes in at every look before the worn read, whatever is worn,
        # since a charm never worn can still be dropped.
        ready = self.body("install_if_ready")
        lines = self.lines("install_if_ready")
        self.assertEqual(lines[0], "const bool havePlayer = g_Flight.ShouldInstall(g_Setup, true, true) && FlightPlayerResolves();")
        self.assertEqual(lines[1], "if (!g_Flight.ShouldInstall(g_Setup, havePlayer, true)) { g_Flight.NoteLook(havePlayer, false); return; }")
        ground = ready.index("FlightGroundIconInstall();")
        worn = ready.index("FlightEquipped(worn)")
        note = ready.index("g_Flight.NoteLook(havePlayer, charmWorn);")
        gate = ready.index("if (!g_Flight.ShouldInstall(g_Setup, havePlayer, charmWorn)) return;")
        self.assertLess(ready.index("FlightPlayerResolves()"), ground)
        self.assertLess(ground, worn)
        self.assertLess(worn, note)
        self.assertLess(note, gate)
        self.assertLess(gate, ready.index("FlightInstall();"))
        self.assertIn("const bool charmWorn = FlightEquipped(worn) && worn != 0;", ready)
        self.assertEqual(ready.count("FlightGroundIconInstall();"), 1)
        # The ground detour is hidden loot's one LootGroundInit hook, tried once.
        ground_body = strip_comments(function_body(self.plugin, "static void FlightGroundIconInstall()"))
        self.assertIn("if (!g_HiddenLootInstallTried) HiddenLootInstall();", ground_body)
        # Negative control: no unconditional install path skips the worn gate.
        self.assertNotIn("g_Flight.ShouldInstall(g_Setup, havePlayer)", ready)
        self.assertEqual(self.code.count("g_Flight.ShouldInstall("), 3)
        # The core: the decision takes the worn charm, and the state names it.
        should = strip_comments(function_body(self.core(), "bool ShouldInstall(bool setupDone, bool playerResolved, bool charmWorn) const"))
        self.assertIn("&& charmWorn", should)
        name = strip_comments(function_body(self.core(), "std::string_view InstallStateName() const"))
        self.assertIn('"waiting-for-worn-charm"', name)
        for state in ('"installed"', '"refused"', '"not-armed"', '"waiting-for-character"'):
            self.assertIn(state, name)
        self.assertLess(name.index('"not-armed"'), name.index('"waiting-for-worn-charm"'))
        # The tick is unchanged: the look stays once every kInstallPollFrames.
        tick = self.body("tick")
        self.assertNotIn("FlightEquipped(", tick)
        self.assertNotIn("FlightGroundIconInstall(", tick)

    def test_the_player_is_confirmed_at_the_first_hooked_call_of_a_frame(self):
        # #216 c3: the core decides which call is the frame's first, keyed on
        # the counter the queries use; only that call resolves the player, as
        # FlightRefreshPlayer does (VALUE_REF-safe, under g_FlightBusy).
        confirm = strip_comments(function_body(self.plugin, "static bool FlightConfirmPlayer()"))
        self.assertIn("if (g_Flight.ConfirmsPlayer((int64_t)g_RuntimeFrame)) FlightRefreshPlayer();", confirm)
        self.assertIn("return g_FlightPlayer != nullptr;", confirm)
        self.assertNotIn("HhResolveLocalPlayer(", confirm)
        player = self.body("player")
        self.assertIn("g_FlightBusy = true;", player)
        self.assertLess(player.index("g_FlightBusy = true;"), player.index("HhResolveLocalPlayer(p)"))
        self.assertIn("HhResolveInstance(p)", player)
        # Each of the three hooks confirms once, before its `self` compare.
        # Nothing else does: not the tick, not the stat line.
        for which in ("on_builtin", "tallest", "depth"):
            with self.subTest(hook=which):
                body = self.body(which)
                self.assertEqual(body.count("FlightConfirmPlayer();"), 1)
                self.assertLess(body.index("FlightConfirmPlayer();"), body.index("if (S != g_FlightPlayer)"))
        self.assertEqual(len(re.findall(r"\bFlightConfirmPlayer\(\)", self.shipped)), 5,
                         "the declaration, the definition and the three hooks")
        self.assertEqual(len(re.findall(r"\bConfirmsPlayer\(", self.shipped)), 1, "FlightConfirmPlayer alone")

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


# ---- the drops: the three drop switches -------------------------------------------------

DROP_STATE_START = "// ---- Flight charms (FlightCharms.hpp): the drop adapter's state"
DROP_STATE_END = "// ---- end of the drop adapter's state"
DROP_START = "// ---- Flight charms (FlightCharms.hpp): the drop adapter ---"
DROP_END = "// ---- end of the flight charms drop adapter"

DROPS = {
    "gate": "static FcNs::Gate FlightDropGate(FcNs::Charm c)",
    "resolve_angelic": "static void FlightDropResolveAngelic()",
    "resolve_elf": "static void FlightDropResolveElf()",
    "passes": "static bool FlightDropUniquePasses(const FcNs::StandIn& s)",
    "tail": "static bool FlightDropTailHolds(int k, int& len)",
    "held": "static bool FlightDropHeldReadBack(std::string& seen)",
    "remove": "static void FlightDropRemove()",
    "push_onto": "static void FlightDropPushOnto(const RValue subs[], const int indices[], const int before[], int lists)",
    "angelic_push": "static void FlightDropAngelicPush()",
    "angelic_remove": "static void FlightDropAngelicRemove()",
    "unique_push": "static void FlightDropUniquePush()",
    "attribute": "static void FlightDropAttributeHit()",
    "refuse": "static void FlightDropRefuse(int i, const std::string& record)",
    "before_create": "static void FlightDropBeforeCreate(RValue& record, const std::string& missing)",
    "note_built": "static void FlightDropNoteBuilt(FcNs::Charm charm)",
    "after_hit": "static void FlightDropAfterHit()",
    "unique_hook": "static RValue& Hook_FlightDropUniqueItems(CInstance* S, CInstance* O, RValue& R, int argc, RValue** A)",
    "install_unique": "static void FlightDropInstallUnique()",
    "switch": "static void FlightDropSwitch(FcNs::Charm c, bool on)",
    "fill_stat": "static void FlightDropFillStat(FcNs::StatFields& s)",
    "trace": "static std::string FlightDropTraceLine()",
}


class FlightCharmsDropContract(unittest.TestCase):
    """The three drop switches, on #74's machinery.

    Pinned here: Pocket Elf's DropUniqueItems hook goes in by its SDK name
    through HookOneScript from `flightcharm elf 1` alone, and a table-only
    result keeps the elf off; a switch-on installs #74's detection and the
    shared ground-icon hook; the Angelic rows go on after #74's and come off
    before them, inside the roll's own guard; a hit is typed, coined and
    rewritten by #74's own hooks, whose roll-in-progress the unique roll's hook
    raises; `sigdrop status` stays #74's line alone; the player build pushes
    one copy; and the research rows that hook DropUniqueItems refuse, and are
    refused, both ways.
    """

    @classmethod
    def setUpClass(cls):
        cls.plugin = PLUGIN.read_text(encoding="utf-8").replace("\r\n", "\n")
        cls.state = cls.plugin[cls.plugin.index(DROP_STATE_START):cls.plugin.index(DROP_STATE_END)]
        cls.block = cls.plugin[cls.plugin.index(DROP_START):cls.plugin.index(DROP_END)]
        cls.code = strip_comments(cls.state + cls.block)
        cls.shipped = strip_comments(strip_research_blocks(cls.plugin))
        cls.all_code = strip_comments(cls.plugin)

    def body(self, which, source=None):
        signature = DROPS[which] if which in DROPS else which
        return strip_comments(function_body(source if source is not None else self.plugin, signature))

    # ---- both builds -----------------------------------------------------------------

    def test_the_drop_adapter_ships_in_both_builds(self):
        player = strip_research_blocks(self.plugin)
        for name, signature in DROPS.items():
            with self.subTest(function=name):
                self.assertIn(signature, player)
        self.assertIn("static FlightDropState g_FlightDrop;", self.shipped)
        # The state sits before #74's roll machinery, which calls into it; the
        # functions after Hook_GetUniqueRepoStruct, whose helpers they reuse.
        self.assertLess(self.plugin.index(DROP_STATE_START), self.plugin.rindex("static bool SignatureSwitchOn("))
        self.assertLess(self.plugin.index("static RValue& Hook_GetUniqueRepoStruct("), self.plugin.index(DROP_START))

    def test_the_player_build_pushes_one_copy(self):
        self.assertIn("static constexpr int g_FlightDropCopies = 1;", self.shipped)
        self.assertEqual(re.findall(r"\bg_FlightDropCopies\s*=(?!=)\s*\w+", self.shipped), ["g_FlightDropCopies = 1"])

    # ---- installed from the switch alone --------------------------------------------------

    def test_drop_unique_items_is_hooked_by_sdk_name_from_the_elf_switch_alone(self):
        install = self.body("install_unique")
        self.assertRegex(install, r"HookOneScript\(SdkShortScriptName\(HeroSiege::Scripts::gml_Script_DropUniqueItems\),\s*"
                                  r"\"fp_flight_duniq\",\s*\(PVOID\)Hook_FlightDropUniqueItems,\s*&g_Orig_FlightDropUnique,\s*&native\)")
        self.assertEqual(self.all_code.count("(PVOID)Hook_FlightDropUniqueItems"), 1)
        self.assertNotIn('"DropUniqueItems"', self.code)
        self.assertNotIn("HookOneScriptTable", self.code)
        # A table-only hook is passed by the game's compiled calls: the elf stays off.
        self.assertIn("d.uniqueNative = ok && native;", install)
        # (Said whenever the elf's switch comes on, since the Mark 1 drops may have tried it first.)
        self.assertIn("if (elf && !d.uniqueNative)", install)
        self.assertIn("FcNs::RefusedLine(", install)
        # One HookOneScript in the whole adapter, and three callers of the installer: the elf's
        # switch-on, the Mark 1 drops' install (#166), which share the one detour, and the
        # research build's `mark1probe draws` install (#166 uniques), in no player build.
        self.assertEqual(self.code.count("HookOneScript("), 1)
        self.assertEqual(self.all_code.count("FlightDropInstallUnique()"), 4,
                         "the definition, the switch-on, the Mark 1 drops' install and the draws watch's install")
        self.assertIn("FlightDropInstallUnique();", strip_comments(function_body(self.plugin, "static std::string Mark1DropInstall()")))
        self.assertIn("FlightDropInstallUnique();", strip_comments(function_body(self.plugin, "static void M1pDrawsInstall()")))
        self.assertNotIn("static void M1pDrawsInstall()", strip_research_blocks(self.plugin))
        self.assertEqual(self.shipped.count("FlightDropInstallUnique()"), 3,
                         "the player build keeps the definition, the switch-on and the Mark 1 drops' install")
        switch = self.body("switch")
        self.assertIn("if (on) {", switch)
        self.assertLess(switch.index("if (on) {"), switch.index("FlightDropInstallUnique();"))
        self.assertLess(switch.index("if (c == FcNs::Charm::Elf)"), switch.index("FlightDropInstallUnique();"))

    def test_a_switch_on_installs_the_detection_and_the_ground_icon(self):
        switch = self.body("switch")
        on = switch[switch.index("if (on) {"):]
        self.assertIn("InstallSignatureAngelicHooks();", on)
        self.assertIn("FlightGroundIconInstall();", on)
        ground = strip_comments(function_body(self.plugin, "static void FlightGroundIconInstall()"))
        self.assertIn("if (!g_HiddenLootInstallTried) HiddenLootInstall();", ground)
        # The switch is the command's only way in, in both builds.
        command = strip_comments(function_body(self.plugin, SIGNATURES["command"]))
        self.assertIn("FlightDropSwitch(c.charm, c.on);", command)
        self.assertEqual(self.all_code.count("FlightDropSwitch("), 2, "the definition and the command")
        self.assertNotIn("not available in this build", command)

    def test_the_ground_route_is_reported_on_the_trace_and_once_when_it_refuses(self):
        # #216 c5: the `flightcharm drops:` line names the route, and a route
        # that refuses the icon prints one line a session, from whichever of
        # flight's install points first learns it.
        trace = self.body("trace")
        self.assertIn('s += " ground-route=" + FlightGroundRoute();', trace)
        self.assertLess(trace.index('" elf="'), trace.index('" ground-route="'))
        self.assertLess(trace.index('" ground-route="'), trace.index('" | "'))
        note = strip_comments(function_body(self.plugin, "static void FlightGroundRouteNote()"))
        lines = [l.strip() for l in note.split("\n") if l.strip()]
        self.assertEqual(lines[0], "if (g_FlightGroundRouteNoted || !g_HiddenLootInstallTried) return;")
        self.assertIn("const std::string route = FlightGroundRoute();", note)
        self.assertIn("if (M1sNs::GroundRouteAllowsArt(route)) return;", note)
        self.assertIn("g_FlightGroundRouteNoted = true;", note)
        self.assertIn('Out("flightcharm: ground icon off - " + route', note)
        self.assertLess(note.index("GroundRouteAllowsArt(route)"), note.index("g_FlightGroundRouteNoted = true;"))
        self.assertLess(note.index("g_FlightGroundRouteNoted = true;"), note.index("Out("))
        self.assertIn("static bool g_FlightGroundRouteNoted = false;", self.all_code)
        # Negative control: the note decides from the route, never from a
        # `both` it wrote itself.
        self.assertNotIn('"both"', note)
        # Each of flight's install points notes right after the install.
        install_points = {
            "switch": self.body("switch"),
            "install_if_ready": strip_comments(function_body(self.plugin, "static void FlightInstallIfReady()")),
            "spawn": strip_comments(function_body(self.plugin, "static void FlpSpawn(const std::vector<std::string>& tail)")),
        }
        for name, body in install_points.items():
            with self.subTest(install_point=name):
                self.assertIn("FlightGroundIconInstall();\nFlightGroundRouteNote();",
                              "\n".join(l.strip() for l in body.split("\n") if l.strip()))
        # Mark 1's install is not flight's: it prints no flight line.
        mark1 = strip_comments(function_body(self.plugin, "static void Mark1GroundIconInstall()"))
        self.assertNotIn("FlightGroundRouteNote(", mark1)
        self.assertEqual(self.all_code.count("FlightGroundRouteNote();"), 4, "the declaration and three install points")

    def test_nothing_runs_at_startup_or_every_frame(self):
        frame = strip_comments(function_body(self.plugin, "void FrameCallback(FWFrame& FrameContext)"))
        for name in ("FlightDrop", "Hook_FlightDropUniqueItems"):
            self.assertNotIn(name, frame)
        for signature in ("static void InstallHook()", "EXPORTED AurieStatus ModuleInitialize", "static void InstallCustomForgeItemHooks()"):
            with self.subTest(function=signature):
                self.assertNotIn("FlightDrop", strip_comments(function_body(self.plugin, signature)))

    # ---- the Angelic rows ------------------------------------------------------------------

    def test_the_angelic_rows_ride_the_rolls_own_guard_after_74s(self):
        guard = strip_comments(function_body(self.shipped, "struct SignatureInjectGuard {"))
        ctor = guard[guard.index("SignatureInjectGuard()"):guard.index("~SignatureInjectGuard()")]
        self.assertLess(ctor.index("SignatureInjectPush();"), ctor.index("FlightDropAngelicPush();"))
        dtor = guard[guard.index("~SignatureInjectGuard()"):]
        self.assertLess(dtor.index("FlightDropAngelicRemove();"), dtor.index("SignatureInjectRemove();"),
                        "flight's entries come off first, so #74's removal finds its own tail")
        push = self.body("angelic_push")
        gate = push.index("if (!g_SigDetectNative || (!d.switches[(int)FcNs::Charm::Wings] && !d.switches[(int)FcNs::Charm::Broom])) return;")
        for later in ("SignatureListResolve(", "FlightDropResolveAngelic(", "FlightDropPushOnto("):
            self.assertLess(gate, push.index(later), later)
        # After #74's push the tail starts after its entries, on the same ds_list.
        self.assertIn("before = g_SigRollBefore + g_SigRollPushed;", push)
        self.assertIn("sub = g_SigRollListId;", push)
        self.assertIn("FlightDropOn(c)", push)
        self.assertIn("if (d.rollUnique) return;", push)

    def test_the_angelic_stand_ins_are_validated_against_the_pool(self):
        resolve = self.body("resolve_angelic")
        self.assertIn("BuildAngelicPool(false);", resolve)
        self.assertIn("FcNs::StandInFor(c)", resolve)
        self.assertIn("p.type == s->t && p.sub == s->sub && p.b == s->b", resolve)
        self.assertIn("FlightDropOutsideRoll outside;", resolve)

    # ---- the unique roll -------------------------------------------------------------------

    def test_the_unique_hook_raises_the_roll_and_pushes_outside_it(self):
        hook = self.body("unique_hook")
        lines = [line.strip() for line in hook.split("\n") if line.strip()]
        self.assertEqual(lines[0], "if (!g_Orig_FlightDropUnique) return R;")
        # Pocket Elf's part of the shared detour (#166): its switch on and no roll in progress, the
        # Angelic roll's or a unique roll either feature took; with neither feature's part, the
        # game's own call.
        self.assertIn("const bool elf = g_SigRollDepth <= 0 && g_FlightDrop.uniqueDepth <= 0 && "
                      "g_FlightDrop.switches[(int)FcNs::Charm::Elf];", hook)
        passthrough = "if (!elf && !mark1) return g_Orig_FlightDropUnique(S, O, R, argc, A);"
        self.assertIn(passthrough, hook)
        self.assertLess(hook.index("FlightUniqueDepthScope depth;"), hook.index("FlightUniqueInjectGuard inject(elf);"))
        inject = hook.index("FlightUniqueInjectGuard inject(elf);")
        roll = hook.index("SignatureRollScope roll;")
        counters = hook.index("FlightUniqueCounterScope counters;")
        reset = hook.index("SignatureHitReset();")
        original = hook.rindex("g_Orig_FlightDropUnique(S, O, R, argc, A)")
        self.assertLess(hook.index(passthrough), inject)
        # The push runs before the roll is raised: its repository reads are not the roll's.
        self.assertLess(inject, roll)
        self.assertLess(roll, counters)
        self.assertLess(counters, reset)
        self.assertLess(reset, original)
        self.assertLess(original, hook.index("FlightDropAfterHit();"))
        guard = strip_comments(function_body(self.plugin, "struct FlightUniqueInjectGuard {"))
        self.assertRegex(guard, r"FlightUniqueInjectGuard\(bool on\)[^{]*\{[^}]*FlightDropUniquePush\(\);")
        self.assertRegex(guard, r"~FlightUniqueInjectGuard\(\)\s*\{[^}]*FlightDropRemove\(\);")

    def test_the_unique_roll_leaves_74s_counters_as_they_were(self):
        scope = strip_comments(function_body(strip_research_blocks(self.plugin), "struct FlightUniqueCounterScope {"))
        self.assertIn("long untyped = g_SigUntyped;", scope)
        self.assertIn("g_SigUntyped = untyped;", scope)
        status = strip_comments(function_body(self.plugin, "static void SigDropStatus()"))
        for name in ("Flight", "flight"):
            self.assertNotIn(name, status, "`sigdrop status` stays #74's line")

    def test_the_elf_stand_in_is_the_cores_even_count_rule(self):
        resolve = self.body("resolve_elf")
        self.assertIn("FlightDropReadUniqueLists(subs, ids, sizes, why)", resolve)
        self.assertIn("if (size < kSigListMinSize)", resolve)
        # An entry that is not three numbers is skipped, never a refusal: the roll drops nothing for one.
        self.assertIn("if (!SigEntry(sub, e, t)) continue;", resolve)
        self.assertIn("for (int k = 0; ok && k < FcNs::kUniqueSubLists; ++k)", resolve)
        self.assertIn("FcNs::kElfCandidates[c]", resolve)
        self.assertIn("d.elfPick = FcNs::PickElfStandIn(counts);", resolve)
        self.assertIn("FcNs::RefusedLine(FcNs::kElfNoStandIn)", resolve)
        self.assertIn("FlightDropOutsideRoll outside;", resolve)
        passes = self.body("passes")
        self.assertIn("HeroSiege::Scripts::gml_Script_GetUniqueRepoStruct", passes)
        self.assertIn('RValue("itemBaseInfoStruct")', passes)
        self.assertIn('TryStructNumber(info, "27", rarity)', passes)
        self.assertIn('RValue("40")', passes)
        self.assertIn("FcNs::PassesUniqueFilter(", passes)
        # Counted again whenever a sub-list's length or id changed.
        push = self.body("unique_push")
        self.assertIn("ids[k] != d.elfId[k] || sizes[k] != d.elfLen[k]", push)
        self.assertLess(push.index("FlightDropResolveElf();"), push.index("FlightDropPushOnto("))
        self.assertIn("FcNs::kUniqueSubLists", push)

    def test_the_push_is_read_back_and_removed_under_the_tail_rule(self):
        onto = self.body("push_onto")
        self.assertEqual(onto.count('"array_set", { entry,'), 3)
        self.assertLess(onto.rindex('"ds_list_add", { d.rollSub[k], entry }'), onto.index("FlightDropHeldReadBack(seen)"))
        self.assertLess(onto.index("FlightDropHeldReadBack(seen)"), onto.index("d.injected += d.rollPushed;"))
        self.assertIn("++d.anomalies;", onto)
        self.assertIn("d.heldMissLogged", onto)
        self.assertIn("n < d.rollCopies[i]", onto)
        held = self.body("held")
        self.assertIn("SignatureController(instance, why)", held)
        self.assertIn('"variable_instance_get", { instance, RValue(g_SigListName) }', held)
        self.assertIn("got != want", held)
        self.assertNotIn("SignatureListShape", held)
        remove = self.body("remove")
        self.assertLess(remove.index("FlightDropTailHolds(k, len)"), remove.index('"ds_list_delete"'))
        self.assertIn('"ds_list_delete", { d.rollSub[k], RValue((double)(len - 1 - n)) }', remove)
        self.assertIn("changed during the roll, left as found", remove)
        self.assertNotIn("array_resize", self.code)
        self.assertNotIn("array_push", self.code)
        tail = self.body("tail")
        self.assertIn("e[0] != (double)s->t || e[1] != (double)s->sub || e[2] != (double)s->b", tail)
        self.assertIn("return at == len;", tail)

    # ---- the hit, the coin, the rewrite ------------------------------------------------------

    def test_the_hit_is_74s_typed_hit_and_the_coin_is_c_in_n_plus_c(self):
        hook = strip_comments(function_body(self.plugin, "static RValue& Hook_CreateDefaultParams("))
        call = "if (g_SigRollDepth > 0 && g_SigHitTyped && g_FlightDrop.rollPushed > 0)"
        self.assertIn(call, hook)
        self.assertLess(hook.index("g_Orig_CreateDefaultParams(S, O, R, argc, A)"), hook.index("FlightDropAttributeHit();"))
        self.assertLess(hook.index("SignatureAttributeHit();"), hook.index("FlightDropAttributeHit();"))
        attribute = self.body("attribute")
        self.assertIn("g_SigHitItem >= 0", attribute, "a hit #74 took is never also a charm's")
        self.assertIn("g_SigHitType != (double)s->t || g_SigLastSub != (double)s->sub || g_SigLastB != (double)s->b", attribute)
        self.assertIn("FcNs::CoinOurs(n, k, u)", attribute)
        self.assertIn("++d.tally[i].ours;", attribute)
        self.assertNotIn("SignatureRewriteParams(", attribute)

    def test_the_rewrite_runs_at_74s_point_to_the_charms_own_definition(self):
        before = strip_comments(function_body(self.plugin, "static void SignatureBeforeCreate("))
        call = "if (which < 0 && g_FlightDrop.hitCharm >= 0) { FlightDropBeforeCreate(record, missing); return; }"
        self.assertIn(call, before)
        self.assertLess(before.index(call), before.index("if (which < 0) return;"))
        rewrite = self.body("before_create")
        self.assertIn("FcNs::SeedFromUniform(c, u)", rewrite)
        self.assertIn("FcNs::kCharmTypeT", rewrite)
        self.assertIn("(double)FcNs::kCharmBaseB", rewrite)
        self.assertIn("SignatureRewriteParams(record, item, why)", rewrite)
        self.assertIn("++d.pending[i];", rewrite)
        self.assertIn("FlightDropRefuse(i,", rewrite)
        refuse = self.body("refuse")
        self.assertIn("d.refused[i] = true;", refuse)
        self.assertIn("++d.refusals;", refuse)
        self.assertIn("if (g_FlightDrop.refused[i]) return FcNs::Gate::Refused;", self.body("gate"))
        reset = strip_comments(function_body(self.plugin, "static void SignatureHitReset("))
        self.assertIn("FlightDropHitReset();", reset)

    def test_built_is_what_the_forge_hook_saw_the_game_build(self):
        forge = strip_comments(function_body(self.plugin, SIGNATURES["forge"]))
        self.assertIn("if (finalPass && flightMatched) FlightDropNoteBuilt(flightRolled.charm);", forge)
        note = self.body("note_built")
        self.assertIn("g_FlightDrop.pending[i] <= 0", note)
        self.assertIn("++g_FlightDrop.tally[i].built;", note)
        roll = strip_comments(function_body(self.plugin, "static RValue& HookAngelicChance("))
        self.assertLess(roll.index("SignatureAfterHit(S, x, y);"), roll.index("FlightDropAfterHit();"))
        after = self.body("after_hit")
        self.assertIn("if (!g_SigHitAtPoint)", after)
        self.assertIn('" built by the game"', after)

    def test_nothing_on_the_drop_path_spawns(self):
        for name in ("SpawnSignatureItem(", "LootGroundCreateFromItem", "InitItemFromJson", "instance_create"):
            self.assertNotIn(name, self.code)

    # ---- the stat line ------------------------------------------------------------------------

    def test_the_stat_line_carries_the_drop_fields(self):
        fields = strip_comments(function_body(self.plugin, SIGNATURES["stat_fields"]))
        self.assertIn("FlightDropFillStat(s);", fields)
        fill = self.body("fill_stat")
        for field in ("s.switches[i]", "s.drops[i]", "s.gate", "s.injected", "s.refused", "s.anomalies",
                      "s.standinElf = FcNs::ElfStandInText(d.elfResolved, d.elfPick, d.elfCounts);"):
            self.assertIn(field, fill)
        command = strip_comments(function_body(self.plugin, SIGNATURES["command"]))
        self.assertIn("Out(FlightDropTraceLine());", command)

    # ---- the research rows, both directions ----------------------------------------------------

    def test_the_research_rows_and_the_elf_hook_never_both_detour(self):
        install = function_body(self.plugin, DROPS["install_unique"])
        research = install[install.index("#ifndef FORGEPACT_RELEASE"):install.index("#endif", install.index("#ifndef FORGEPACT_RELEASE"))]
        self.assertIn("ApRollHoldsDropUnique() || GpHoldsDropUnique()", research)
        self.assertLess(install.index("ApRollHoldsDropUnique()"), install.index("HookOneScript("))
        for name in ("ApRollHoldsDropUnique", "GpHoldsDropUnique"):
            self.assertNotIn(name, self.shipped)
        self.assertIn('"drop-unique"', strip_comments(function_body(self.plugin, "static bool ApRollHoldsDropUnique()")))
        self.assertIn("g_GpScriptRows[kGpScript_DropUniqueItems].orig", strip_comments(function_body(self.plugin, "static bool GpHoldsDropUnique()")))
        attach = strip_comments(function_body(self.plugin, "static void ApRollAttach("))
        refusal = attach.index('std::string_view(r.id) == "drop-unique" && g_Orig_FlightDropUnique')
        self.assertLess(refusal, attach.index("MmCreateHook("))
        self.assertIn("kApRollBlocked", attach[refusal:attach.index("return;", refusal)])
        scripts = strip_comments(function_body(self.plugin, "static void GpInstallScripts("))
        refusal = scripts.index("&t == &g_GpScriptRows[kGpScript_DropUniqueItems] && g_Orig_FlightDropUnique")
        self.assertLess(refusal, scripts.index("HookOneScript("))

    # ---- no address ----------------------------------------------------------------------------

    def test_no_hand_resolved_address(self):
        self.assertIsNone(re.search(r"\bk\w*Rva\w*\b", self.code))
        self.assertIsNone(re.search(r"\(\s*char\s*\*\s*\)\s*\w+\s*\+", self.code))
        self.assertIsNone(re.search(r"0x[0-9A-Fa-f]{5,}", self.code))
        self.assertNotIn("GetModuleHandle", self.code)
        self.assertNotIn("MmCreateHook", self.code)

    # ---- Pocket Elf's kill route on DropItem's own draw (#166) ------------------------------
    # The adapter is the kill draw's, shared with the Mark 1 drops (test_mark1_drops_contract.py's
    # test_draw_hook_* pin its hooks); these pin Pocket Elf's part of it.

    def test_elf_kill_draw_switch_installs_the_shared_draw_and_refuses_whole(self):
        switch = self.body("switch")
        self.assertLess(switch.index("FlightDropInstallUnique();"), switch.index("const std::string drawWhy = KillDrawInstall();"))
        refusal = switch[switch.index("if (!drawWhy.empty()) {"):]
        refusal = refusal[:refusal.index("return;")]
        self.assertIn("g_FlightDrop.switches[i] = false;", refusal)
        self.assertIn('Out(FcNs::RefusedLine("elf: " + drawWhy + " - Pocket Elf stays off"));', refusal)
        self.assertLess(switch.index("KillDrawInstall()"), switch.index("FlightDropResolveElf();"))
        # Only Pocket Elf's switch installs the draw; Gabriel Wings and Witch's Broomstick do not.
        elf_branch = switch[switch.index("if (c == FcNs::Charm::Elf) {"):switch.index("} else {")]
        self.assertIn("KillDrawInstall()", elf_branch)
        self.assertEqual(switch.count("KillDrawInstall()"), 1)

    def test_elf_kill_draw_part_is_its_switch_gate_and_the_cores_decisions(self):
        elf_on = strip_comments(function_body(self.plugin, "static bool KillDrawElfOn()"))
        for term in ("g_FlightDrop.switches[elf]", "!g_FlightDrop.refused[elf]", "g_SigDetectNative", "g_KillDraw.orig != nullptr"):
            self.assertIn(term, elf_on)
        begin = strip_comments(function_body(self.plugin, "static void KillDrawBeginCall()"))
        self.assertIn("g_ElfDraw.BeginCall(KillDrawElfOn(), g_SigRollDepth + g_FlightDrop.uniqueDepth) == FcNs::DrawAction::Push", begin)
        push = strip_comments(function_body(self.plugin, "static void KillDrawPush(bool mark1, bool elf)"))
        self.assertIn("if (g_ElfDrawCache.Stale(elfKeys)) {", push)
        self.assertIn("FcNs::ResolveElfDrawStandIn(elfLists, KillDrawElfVerdict)", push)
        self.assertIn("elfCopies = FcNs::ElfDrawCopies(elfStandIn, g_FlightDropCopies);", push)
        self.assertIn("g_ElfDraw.NotePushed(elfStandIn, elfCopies);", push)
        # Pocket Elf's row goes on first, before the pieces'.
        self.assertLess(push.index("if (elfCopies > 0)"), push.index("for (const M1dNs::DrawPushEntry& e : plan.entries)"))
        tier = strip_comments(function_body(self.plugin, "static void KillDrawNoteTier(const RValue& result)"))
        self.assertIn("g_ElfDraw.NoteTier(", tier)
        settle = strip_comments(function_body(self.plugin, "static void KillDrawSettleHit()"))
        self.assertIn("g_ElfDraw.Hit(d.pickT, d.pickSub, d.pickB, u) == FcNs::DrawHit::Ours", settle)
        # The hit line's k is Pocket Elf's own recorded tier, the one its coin read.
        self.assertIn("const int ek = g_ElfDraw.Tier();", settle)
        self.assertIn("g_ElfDraw.EnterRoll();", strip_comments(function_body(self.plugin, "static void KillDrawEnterRoll()")))
        self.assertIn("g_ElfDraw.LeaveRoll();", strip_comments(function_body(self.plugin, "static void KillDrawLeaveRoll()")))

    def test_elf_kill_draw_rewrite_built_and_line_are_the_flight_charms_own(self):
        rewrite = strip_comments(function_body(self.plugin, "static bool KillDrawBeforeCreate(int argc, RValue** A)"))
        self.assertIn("if (!isPiece && !g_ElfDraw.HitPending()) return false;", rewrite)
        self.assertIn("FcNs::SeedFromUniform(FcNs::Charm::Elf, u)", rewrite)
        self.assertIn("(double)FcNs::kCharmBaseB", rewrite)
        self.assertIn("FlightDropRefuse(static_cast<int>(FcNs::Charm::Elf), shown);", rewrite)
        self.assertIn("g_ElfDraw.Settle(ok);", rewrite)
        note = self.body("note_built")
        self.assertEqual([l.strip() for l in note.split("\n") if l.strip()][0],
                         "if (charm == FcNs::Charm::Elf && KillDrawNoteBuiltElf()) return;")
        built = strip_comments(function_body(self.plugin, "static bool KillDrawNoteBuiltElf()"))
        self.assertIn("g_ElfDraw.NoteBuilt();", built)
        self.assertIn("g_SigRollDepth > 0 || d.aside > 0", built)
        self.assertNotIn("g_FlightDrop.tally", built)   # the DropUniqueItems route's tally is its own
        close = strip_comments(function_body(self.plugin, "static void KillDrawCloseHit()"))
        self.assertIn('"flightcharm hit: "', close)
        self.assertIn("g_ElfDraw.Settle(false);", close)

    def test_elf_kill_draw_stat_fields(self):
        fields = strip_comments(function_body(self.plugin, SIGNATURES["stat_fields"]))
        self.assertLess(fields.index("FlightDropFillStat(s);"), fields.index("KillDrawElfFillStat(s);"))
        fill = strip_comments(function_body(self.plugin, "static void KillDrawElfFillStat(FcNs::StatFields& s)"))
        self.assertIn("g_ElfDraw.FillStat(s, g_FlightDrop.switches[static_cast<int>(FcNs::Charm::Elf)]);", fill)
        self.assertIn("s.elfDrawHook = g_KillDraw.route;", fill)
        self.assertIn("s.elfDrawTyped = g_KillDraw.elfTyped;", fill)
        self.assertIn("static FcNs::ElfDraw g_ElfDraw;", self.shipped)
        self.assertIn("static FcNs::ElfDrawCache g_ElfDrawCache;", self.shipped)


# ---- the probe: the research build's `flightprobe` -------------------------------------------

PROBE_START = "// ---- flightprobe: the flight charms' phase 2 instrument"
PROBE_END = "// ---- end of flightprobe"
FLP_SYMBOL = r"\b(?:g_|k)?Flp[A-Z0-9_]\w*"

PROBE = {
    "command": "static void FlpCommand(const std::string& rest)",
    "usage": "static void FlpUsage()",
    "spawn": "static void FlpSpawn(const std::vector<std::string>& tail)",
    "ground": "static void FlpGround()",
    "free": "static void FlpFree(const std::vector<std::string>& tail)",
    "depth": "static void FlpDepth()",
    "dress": "static void FlpDress(const std::vector<std::string>& tail)",
    "copies": "static void FlpCopies(const std::vector<std::string>& tail)",
    "unique": "static void FlpUnique(const std::vector<std::string>& tail)",
    "pool": "static void FlpPool()",
    "stat": "static void FlpStat()",
}


class FlightCharmsProbeContract(unittest.TestCase):
    """`flightprobe`, the research build's instrument for the flight charms' Live 1.

    It pins that the verb and every Flp* name vanish from the player build, that
    RunCommand reaches it by a standalone early return inside the research guard,
    and how each form reads the game: `spawn` by charmprobe spawn's loader route
    with the shared ground-icon hook installed first, `ground` and `unique` by
    charmprobe heroic's newest-ground-id scan, `free` through the shared
    detours' saved originals only, `depth` through the flight adapter's own
    layer read with nothing written, `dress` by the footprint flag alone and
    `copies` by the one count both rolls' pushes read.
    """

    @classmethod
    def setUpClass(cls):
        cls.plugin = PLUGIN.read_text(encoding="utf-8").replace("\r\n", "\n")
        cls.block = cls.plugin[cls.plugin.index(PROBE_START):cls.plugin.index(PROBE_END)]
        cls.code = strip_comments(cls.block)
        cls.player = strip_research_blocks(cls.plugin)
        cls.shipped = strip_comments(cls.player)

    def body(self, which):
        return strip_comments(function_body(self.block, PROBE[which]))

    # ---- research build only -------------------------------------------------------------------

    def test_the_probe_is_research_only(self):
        for name, signature in PROBE.items():
            with self.subTest(function=name):
                self.assertIn(signature, self.block)
                self.assertNotIn(signature, self.player)
        self.assertNotIn(PROBE_START, self.player)
        self.assertNotIn("flightprobe", self.shipped)
        self.assertIsNone(re.search(FLP_SYMBOL, self.shipped), "an Flp* symbol reaches the player build")

    def test_dispatched_as_a_standalone_early_return_in_the_research_build_only(self):
        dispatch = 'if (lc == "flightprobe") { FlpCommand(rest); return; }'
        run = function_body(self.plugin, SIGNATURES["run"])
        self.assertIn(dispatch, run)
        self.assertNotIn('else if (lc == "flightprobe")', run)
        self.assertEqual(self.plugin.count('lc == "flightprobe"'), 1)
        self.assertNotIn(dispatch, function_body(self.player, SIGNATURES["run"]))
        allowlist = re.search(r"kPlayerCommands\s*=\s*\{(?P<body>.*?)\};", self.plugin, re.S).group("body")
        self.assertNotIn('"flightprobe"', allowlist)

    def test_every_form_and_the_bare_marker(self):
        command = self.body("command")
        self.assertIn("if (tok.empty()) { FlpUsage(); return; }", command)
        for form, handler in (("spawn", "FlpSpawn(tail);"), ("ground", "FlpGround();"), ("free", "FlpFree(tail);"),
                              ("depth", "FlpDepth();"), ("dress", "FlpDress(tail);"), ("copies", "FlpCopies(tail);"),
                              ("unique", "FlpUnique(tail);"), ("pool", "FlpPool();"), ("stat", "FlpStat();")):
            with self.subTest(form=form):
                self.assertIn('if (sub == "%s") { %s return; }' % (form, handler), command)
        self.assertIn('static constexpr const char* kFlpMarker = "flp3";', self.code)
        usage = self.body("usage")
        self.assertIn('Out(std::string("flightprobe: marker=") + kFlpMarker);', usage)
        self.assertLess(usage.index("marker="), usage.index("spawn elf|wings|broom"))
        # The bare usage line names both of replan 2's forms.
        for form in ("| depth |", "| pool |"):
            with self.subTest(usage=form):
                self.assertIn(form, usage)

    # ---- spawn and ground ----------------------------------------------------------------------

    def test_the_marker_is_flp3_for_the_monster_rule(self):
        # The monster scope moved the marker again, so Live 3's `marker` check proves the
        # installed build carries the monster rule.
        self.assertIn('static constexpr const char* kFlpMarker = "flp3";', self.code)
        for old in ('"flp1"', '"flp2"'):
            with self.subTest(old=old):
                self.assertNotIn(old, self.code)

    def test_spawn_uses_charmprobes_loader_route_with_the_charms_own_seed(self):
        spawn = self.body("spawn")
        self.assertIn("FcNs::CharmFromKey(", spawn)
        self.assertIn("FcNs::TopSeed(c)", spawn)
        self.assertIn("FcNs::SeedFor(c, v[0], v[1])", spawn)
        self.assertIn("FcpNs::SpawnJson(seed, FcNs::kCharmBaseB)", spawn)
        self.assertIn('"json_parse"', spawn)
        self.assertIn("HeroSiege::Scripts::gml_Script_InitItemFromJson", spawn)
        self.assertIn("HeroSiege::Scripts::gml_Script_LootGroundCreateFromItem, self, self", spawn)
        self.assertIn("FcpNewestGround(ground, &newest)", spawn)
        self.assertIn("ground=new id=", spawn)
        self.assertIn("ground=none", spawn)
        self.assertNotIn("SpawnSeed(", spawn)   # never a phase-1 research seed

    def test_spawn_installs_the_shared_ground_icon_hook_before_it_builds(self):
        spawn = self.body("spawn")
        self.assertIn("FlightGroundIconInstall();", spawn)
        self.assertLess(spawn.index("FlightGroundIconInstall();"), spawn.index('"json_parse"'))
        self.assertLess(spawn.index("FlightGroundIconInstall();"), spawn.index("gml_Script_InitItemFromJson"))
        install = strip_comments(function_body(self.plugin, "static void FlightGroundIconInstall()"))
        self.assertIn("HiddenLootInstall();", install)

    def test_ground_reads_the_newest_ground_item_and_names_ours_game_or_error(self):
        ground = self.body("ground")
        self.assertIn("FcpNewestGround(object, &newest)", ground)
        self.assertIn('"sprite_index"', ground)
        self.assertIn('"sprite_get_name"', ground)
        self.assertIn("g_FlightIconSprite[k]", ground)
        self.assertIn("kFlpErrorSprite", ground)
        for verdict in ('"ours"', '"game"', '"error"'):
            self.assertIn(verdict, ground)
        self.assertIn('static constexpr const char* kFlpErrorSprite = "Error_spr";', self.code)
        self.assertNotIn("variable_instance_set", ground)

    # ---- free and depth --------------------------------------------------------------------------

    def test_free_asks_only_the_shared_detours_saved_originals(self):
        free = self.body("free")
        self.assertIn("g_JsOrig[(int)JsNs::Builtin::PlaceMeeting]", free)
        self.assertIn("g_JsOrig[(int)JsNs::Builtin::InstancePosition]", free)
        self.assertLess(free.index("if (!meeting || !position)"), free.index("meeting(met,"))
        for builtin in ('"place_meeting"', '"instance_position"'):
            self.assertNotIn(builtin, free)
        self.assertIn("HeroSiege::Objects::GetObjectName(kFlightFamily)", free)
        self.assertIn('"object_get_name"', free)
        self.assertIn('"flightprobe free: blocked="', free)

    def test_depth_reads_the_flight_adapters_layer_ends_and_writes_nothing(self):
        depth = self.body("depth")
        self.assertIn("g_Flight.RefreshForStat((int64_t)g_RuntimeFrame,", depth)
        self.assertIn("FlightRoom(room)", depth)
        self.assertIn("FlightLayerEnds(room, first, last)", depth)
        self.assertIn("FcNs::GameFrontDepth(first, last, front)", depth)
        self.assertIn('"layer_get_depth"', depth)
        self.assertIn("FcNs::StateName(g_Flight.CurrentState())", depth)
        self.assertNotIn("variable_instance_set", depth)
        # The probe never runs a census or creates a layer itself.
        for call in ('"layer_create"', '"layer_depth"', '"layer_get_all"', '"layer_get_fx"', "FlightLayerPlace(",
                     "FlightLayerAll(", "FlightLayerDetail(", "FlightLayerEffect(", "FlightLayerCount(",
                     "FlightLayerMove(", "g_Flight.OnUpdateDepth("):
            self.assertNotIn(call, depth)

    def test_depth_replies_with_what_the_adapters_census_chose(self):
        depth = self.body("depth")
        # Replan 2's reply, every field in order.
        # fx= and unread= are the core's rendering of the visit's tally (FcNs::CensusTallyText).
        fields = ('"flightprobe depth: "', '"layer="', '" depth="', '" flight-layer="', '" flight-depth="', '" front="',
                  '" game-front="', '" filter="', "FcNs::CensusTallyText(", '" fx-kind="', '" layers="',
                  '" census-ms="', '" moves="', '" y="', '" state="', '" room="')
        at = -1
        for field in fields:
            with self.subTest(field=field):
                self.assertIn(field, depth)
                self.assertGreater(depth.index(field), at, "out of order")
                at = depth.index(field)
        # It reads the census's own record: the visit, its choice, and the core's own filter rule.
        self.assertIn("g_Flight.Visit()", depth)
        self.assertIn("v.by", depth)
        self.assertIn("v.moves", depth)
        self.assertIn("v.censusMs", depth)
        self.assertIn("v.listed", depth)
        self.assertIn("FcNs::RearmostFilter(v.gameFront, v.layers)", depth)
        self.assertIn('"game@"', depth)
        # The visit's flight layer counts only while it is this room's and still ours.
        self.assertIn("v.room == room", depth)
        self.assertIn("FlightLayerArg(v.layer)", depth)
        self.assertIn('"layer_exists"', depth)
        self.assertIn("std::string(FcNs::kFlightLayerName)", depth)
        self.assertLess(depth.index('"layer_exists"'), depth.index('"layer_get_name"'))
        # fx= and unread= are the census's own sort of every layer it was handed: the visit's tally,
        # kept by the core (test_depth_every_listed_layer_reaches_the_census_tally pins the rest).
        self.assertIn("FcNs::CensusTallyText(counted ? v.tally : FcNs::CensusTally{})", depth)

    def test_depth_every_listed_layer_reaches_the_census_tally(self):
        listing = strip_comments(function_body(self.plugin, "static bool FlightLayerAll(std::vector<FcNs::LayerEnd>& all)"))
        effect = strip_comments(function_body(self.plugin, "static bool FlightLayerEffect(int64_t id, bool& effect)"))
        depth = self.body("depth")
        # FlightLayerAll hands the core every element of layer_get_all's array: one whose id or depth
        # does not read is appended with a non-finite depth, never dropped by a `continue`.
        append = "all.push_back(FcNs::LayerEnd{ id, depth });"
        self.assertEqual(listing.count(append), 1)
        self.assertNotIn("continue", listing)
        self.assertIn("double depth = std::numeric_limits<double>::quiet_NaN();", listing)
        for read in ("ArrayAt(g_Yytk, layers, i, v)", "FlightLayerId(v, id)", '"layer_get_depth"', "depth = d.ToDouble();"):
            with self.subTest(read=read):
                self.assertLess(listing.index("double depth = std::numeric_limits<double>::quiet_NaN();"), listing.index(read))
                self.assertLess(listing.index(read), listing.index(append))
        # The append is outside the readable branch: it closes the loop body, after that branch's brace.
        self.assertRegex(listing, r"\}\s*\n\s*" + re.escape(append) + r"\s*\n\s*\}")
        # Only a readable layer keeps its argument for the later builtin calls.
        self.assertLess(listing.index("depth = d.ToDouble();"), listing.index("g_FlightLayerArgs[id] = v;"))
        # The first non-struct kind is reset where a census lists the layers, before the read.
        self.assertIn("g_FlightFxKind = -1;", listing)
        self.assertLess(listing.index("g_FlightFxKind = -1;"), listing.index("try {"))
        # FlightLayerEffect keeps its contract (a struct is the effect, nothing else is) and keeps the
        # first non-struct answer's kind; false only after the throw.
        self.assertIn("effect = fx.m_Kind == VALUE_OBJECT;", effect)
        self.assertNotIn("VALUE_REF", effect)
        self.assertIn("if (!effect && g_FlightFxKind < 0) g_FlightFxKind = (int)fx.m_Kind;", effect)
        self.assertEqual(effect.count("return false;"), 1)
        self.assertLess(effect.index("catch (...)"), effect.index("return false;"))
        self.assertLess(effect.index("g_FlightFxKind = (int)fx.m_Kind;"), effect.index("return true;"))
        # FlpDepth prints filter=, the core's rendering of the visit's tally, fx-kind= and layers=, in
        # that order, from g_Flight.Visit(); the adapter's own per-visit counts are retired.
        at = -1
        for field in ('" filter="', "FcNs::CensusTallyText(", '" fx-kind="', '" layers="'):
            with self.subTest(field=field):
                self.assertGreater(depth.index(field), at, "out of order")
                at = depth.index(field)
        self.assertIn("const FcNs::FlightLayerVisit& v = g_Flight.Visit();", depth)
        self.assertIn("counted ? v.tally : FcNs::CensusTally{}", depth)
        self.assertIn("counted && g_FlightFxKind >= 0 ? std::to_string(g_FlightFxKind)", depth)
        self.assertIn('std::string("none")', depth)
        self.assertIn("std::to_string(counted ? v.listed : 0)", depth)
        self.assertNotIn("g_FlightFxVisit", self.plugin)
        self.assertNotIn("FlightFxReads", self.plugin)
        # The core keeps the tally in the visit record and renders it once.
        core = strip_comments((ROOT / "plugin" / "include" / "ForgePact" / "FlightCharms.hpp").read_text(encoding="utf-8"))
        self.assertIn("CensusTally            tally;", core)
        self.assertIn("inline std::string CensusTallyText(const CensusTally& t)", core)
        self.assertIn('" unread=behind:"', core)

    def test_pool_reads_both_rolls_lists_and_writes_nothing(self):
        pool = self.body("pool")
        for field in ('"flightprobe pool: "', '"angelic list="', '" size="', '" droppable="', '" water-melon="',
                      '" soul-collector="', '" | unique sizes="', '" standin-elf="', '" counts="'):
            with self.subTest(field=field):
                self.assertIn(field, pool)
        self.assertIn('"refused "', pool)
        # The lists the rolls read: #74's Angelic element by its name and index, and lootListUnique[0..4].
        self.assertIn("SignatureController(instance, why)", pool)
        self.assertIn("g_SigListName", pool)
        self.assertIn("g_SigListIndex", pool)
        self.assertIn("SigListHandle(sub, id, step)", pool)
        self.assertIn("FlightDropReadUniqueLists(subs, ids, sizes, why)", pool)
        self.assertIn("SigEntry(sub, e, t)", pool)
        # The adapter's own stand-ins and its even-count rule.
        self.assertIn("FcNs::StandInFor(FcNs::Charm::Wings)", pool)
        self.assertIn("FcNs::StandInFor(FcNs::Charm::Broom)", pool)
        self.assertIn("FcNs::kElfCandidates[c]", pool)
        self.assertIn("FlightDropUniquePasses(FcNs::kElfCandidates[c])", pool)
        self.assertIn("FcNs::PickElfStandIn(counts)", pool)
        self.assertIn("FcNs::ElfStandInText(true, pick, counts)", pool)
        # Droppable: the Angelic roll's own two filters on the repository definition, read by name
        # outside any roll, so #74's typing hook records nothing.
        self.assertIn("FlpAngelicDroppable(t)", pool)
        self.assertIn("FlightDropOutsideRoll outside;", pool)
        droppable = strip_comments(function_body(self.block, "static bool FlpAngelicDroppable(const double t[3])"))
        self.assertIn("HeroSiege::Scripts::gml_Script_GetUniqueRepoStruct", droppable)
        self.assertIn('RValue("itemBaseInfoStruct")', droppable)
        self.assertIn('TryStructNumber(info, "27", rarity)', droppable)
        self.assertIn('RValue("40")', droppable)
        self.assertIn("rarity == 7.0 || rarity == 10.0", droppable)
        # Refused while a roll's entries are on a list, since its sizes would count them.
        self.assertIn("g_SigRollPushed > 0 || g_FlightDrop.rollPushed > 0", pool)
        self.assertLess(pool.index("g_SigRollPushed > 0"), pool.index("SignatureController("))
        # Read-only: nothing pushed, written, resolved or recorded.
        for call in ("ds_list_add", "ds_list_insert", "ds_list_delete", "ds_list_replace", "ds_list_set",
                     "variable_instance_set", "variable_struct_set", "array_set", "FlightDropPushOnto(",
                     "SignatureListResolve(", "FlightDropResolveElf(", "FlightDropResolveAngelic(",
                     "FlightDropCountAngelic(", "BuildAngelicPool(", "g_FlightDrop.elf", "g_FlightDrop.angelic",
                     "g_SigList"):
            with self.subTest(forbidden=call):
                if call == "g_SigList":
                    self.assertIsNone(re.search(r"\bg_SigList(?!Name\b|Index\b)\w*\s*=(?!=)", pool))
                    continue
                self.assertNotIn(call, pool)
                self.assertNotIn(call, droppable)

    # ---- dress, copies, unique, stat -------------------------------------------------------------

    def test_dress_suspends_the_footprint_write_alone(self):
        dress = self.body("dress")
        self.assertIn("g_FlightDressFootprint = on;", dress)
        self.assertNotIn("variable_struct_set", dress)
        final = strip_comments(function_body(self.plugin, SIGNATURES["final"]))
        guarded = function_body(final, "if (g_FlightDressFootprint)")
        self.assertIn('RValue("2")', guarded)
        self.assertIn('RValue("3")', guarded)
        self.assertNotIn('RValue("11")', guarded)
        self.assertIn('RValue("11")', final)

    def test_copies_sets_the_one_count_both_rolls_push(self):
        copies = self.body("copies")
        self.assertIn("FcpNs::ParseWhole(tail[0], 1, kFlpMaxCopies, k)", copies)
        self.assertIn("g_FlightDropCopies = (int)k;", copies)
        self.assertIn("static constexpr int kFlpMaxCopies = 400;", self.code)
        angelic = strip_comments(function_body(self.plugin, "static void FlightDropAngelicPush()"))
        unique = strip_comments(function_body(self.plugin, "static void FlightDropUniquePush()"))
        self.assertIn("= g_FlightDropCopies", angelic)
        self.assertIn("= g_FlightDropCopies", unique)
        self.assertIn("static constexpr int g_FlightDropCopies = 1;", self.shipped)

    def test_unique_calls_the_ordinary_roll_by_name_with_the_player(self):
        unique = self.body("unique")
        self.assertIn("FcpNs::ParseWhole(tail[0], 1, kFlpMaxUnique, n)", unique)
        self.assertIn("FcpPlayer(player, self, x, y, why)", unique)
        self.assertIn("CallGameScriptEx(res, HeroSiege::Scripts::gml_Script_DropUniqueItems, self, self, { RValue(x), RValue(y) })",
                      unique)
        self.assertLess(unique.index("FcpNewestGround(ground, nullptr)"), unique.index("gml_Script_DropUniqueItems"))
        self.assertIn("FcpNewestGround(ground, &newest)", unique)
        self.assertIn("rolled.charm == FcNs::Charm::Elf", unique)
        for reply in ('"placed "', '"nothing placed', '"threw"', '" self=player"'):
            self.assertIn(reply, unique)
        self.assertIn("static constexpr int kFlpMaxUnique = 20;", self.code)

    def test_stat_prints_the_flightcharm_lines_then_its_own(self):
        stat = self.body("stat")
        self.assertIn('FlightCharmCommand("stat");', stat)
        self.assertLess(stat.index('FlightCharmCommand("stat");'), stat.index('"flightprobe stat:'))

    # ---- no address ----------------------------------------------------------------------------

    def test_no_hand_resolved_address(self):
        self.assertIsNone(re.search(r"\bk\w*Rva\w*\b", self.code))
        self.assertIsNone(re.search(r"\(\s*char\s*\*\s*\)\s*\w+\s*\+", self.code))
        self.assertIsNone(re.search(r"0x[0-9A-Fa-f]{5,}", self.code))
        self.assertNotIn("GetModuleHandle", self.code)
        self.assertNotIn("MmCreateHook", self.code)
        self.assertNotIn("HookOneScript", self.code)


class FlightCharmsTooltipContract(unittest.TestCase):
    """The gold row on an item with no stats (Pocket Elf, Live 1's `bag-icon`).

    The forged rows anchor through ForgedTooltipRows.hpp's rule, which the
    harness's `tooltip_rows` scenarios run: the stats hook decides through
    the header, and the rows' arming reads "no stats" once a pass, in both
    places that arm them (the player's tooltip and the Item Editor's drawn
    one). Both return paths of the stats hook stay as
    test_item_truth_contract.py pins them.
    """

    @classmethod
    def setUpClass(cls):
        plugin = PLUGIN.read_text(encoding="utf-8").replace("\r\n", "\n")
        cls.release = strip_comments(strip_research_blocks(plugin))
        cls.header = (ROOT / "plugin" / "include" / "ForgePact" / "ForgedTooltipRows.hpp").read_text(encoding="utf-8")

    def test_the_header_is_booleans_and_numbers(self):
        self.assertIn("namespace ForgePact::ForgedTooltipRows {", self.header)
        for signature in ("constexpr bool Pending(std::size_t rowCount)",
                          "constexpr bool IsAnchorFormat(double format)",
                          "constexpr bool NeedsStatRead(bool pending, double format, bool itemHasNoStats)",
                          "constexpr bool AnchorsHere(bool pending, double format, bool statPresent, bool itemHasNoStats)"):
            self.assertIn(signature, self.header)
        self.assertNotRegex(strip_comments(self.header), r"RValue|CInstance|#include <(string|vector)>")
        self.assertIn("#include <ForgePact/ForgedTooltipRows.hpp>", self.release)

    def test_the_stats_hook_decides_through_the_headers_rule(self):
        stat = function_body(self.release, "static RValue& Hook_DrawInventoryStatsNew(")
        read = stat.index("ForgePact::ForgedTooltipRows::NeedsStatRead(g_TipRowsPending, fmt, g_TipItemNoStats)")
        present = stat.index("&& TipStatPresent(A, argc);", read)
        anchor = stat.index(
            "anchors = ForgePact::ForgedTooltipRows::AnchorsHere(g_TipRowsPending, fmt, present, g_TipItemNoStats);")
        self.assertLess(read, present, "the stat is read only when the rule asks")
        self.assertLess(present, anchor)
        self.assertLess(anchor, stat.index("if (anchors) {"))
        self.assertLess(stat.index("if (anchors) {"), stat.index("g_TipRowsPending = false;"), "an anchor ends the pending rows")
        # A Mark 1 piece alone asks its own core (Mark1Set.hpp's BlockAnchor,
        # after the piece's own stat lines, the 2026-10-22 playtest); every
        # other forged item takes the header's rule in the `else`.
        piece = stat.index("if (g_TipM1AnchorOn) {")
        self.assertLess(piece, stat.index("} else {"))
        self.assertLess(stat.index("} else {"), read)
        self.assertIn("anchors = g_TipM1Anchor.AnchorsHere(statId, has, fmt);", stat[piece:read])
        self.assertNotIn("fmt == 2.0 || fmt == 3.0) && TipStatPresent", stat, "no second copy of the rule")
        self.assertEqual(2, stat.count("TipNoteStat("))
        self.assertEqual(2, stat.count("g_Tip.statId = outerStat;"))

    def test_no_stats_is_read_once_a_pass_where_the_rows_are_armed(self):
        for name in ("static RValue& Hook_DrawInventoryItemV2(", "static bool TipDrawOne("):
            body = function_body(self.release, name)
            arm = body.index("g_TipRowsPending = ForgePact::ForgedTooltipRows::Pending(g_TipRows.size());")
            nostats = body.index("g_TipItemNoStats = g_TipRowsPending && TipItemHasNoStats(")
            self.assertLess(arm, nostats, name)
            self.assertIn("g_TipItemNoStats = false;", body[nostats:], name + " clears it when the pass ends")
        stat = function_body(self.release, "static RValue& Hook_DrawInventoryStatsNew(")
        self.assertNotIn("TipItemHasNoStats(", stat, "never read on every stat call")
        reader = function_body(self.release, "static bool TipItemHasNoStats(const RValue& item)")
        self.assertIn('RValue("itemStatStruct")', reader)
        self.assertIn('"variable_struct_names_count"', reader)
        self.assertIn("catch (...) { return false; }", reader, "a failed read keeps the old anchor")

    def test_a_charms_rows_stay_gold_beside_mark_1s_set_block(self):
        # Both arming sites hand an item that is not a Mark 1 piece its
        # ForgedTooltipRows, as gold rows; the anchor draws a gold row through
        # HhDrawTooltipLine, unchanged (ForgePact #166 drew Mark 1's block
        # beside them, in its own colours).
        for name, item in (("static RValue& Hook_DrawInventoryItemV2(", "*A[3]"), ("static bool TipDrawOne(", "item")):
            body = function_body(self.release, name)
            self.assertIn(f"else g_TipRows = TipGoldRows(ForgedTooltipRows({item}));", body, name)
        gold = function_body(self.release, "static std::vector<TipRow> TipGoldRows(")
        self.assertIn("TipRow{ text, kTipGold, false }", gold)
        self.assertIn("static constexpr double kTipGold = 6472946.0;", self.release)
        draw = function_body(self.release, "static double TipDrawRow(double x, double y, const TipRow& row)")
        # A gold row is never wrapped nor in the small font (Mark 1's
        # description alone is), so it draws through HhDrawTooltipLine.
        self.assertIn("if (!row.wrapped && !row.smallFont && row.colour == kTipGold) {", draw)
        self.assertIn("HhDrawTooltipLine(x, y, row.text);", draw)
        self.assertLess(draw.index("HhDrawTooltipLine(x, y, row.text);"), draw.index("const TipSmallFont font(row.smallFont);"))
        line = function_body(self.release, "static void HhDrawTooltipLine(double x, double y, const std::string& text)")
        self.assertIn('"make_colour_rgb", { RValue(242.0), RValue(196.0), RValue(98.0) }', line)
        # At most three rows of a forged item's own text, as before.
        rows = function_body(self.release, "static std::vector<std::string> ForgedTooltipRows(const RValue& item)")
        self.assertIn("rows.size() < 3", rows)


if __name__ == "__main__":
    unittest.main()
