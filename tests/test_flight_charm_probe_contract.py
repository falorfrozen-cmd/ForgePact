"""Contract tests for the flight-charms research instrument (ForgePact #158, phase 1).

Which itemInfoStruct keys hold a charm's footprint and sprite, what a save does
when a forged footprint appears or goes away, and which unique rarities
DropItemHeroic places are measured in one research launch
(docs/flight-charms-research.md) before phase 2 builds the Flight charms. This
phase ships one research-build instrument for it, `charmprobe`, and these tests
pin it on comment-stripped source:

- it is research build only: the word `charmprobe`, its two headers, its
  handler and every `Fcp*` symbol vanish from what the player build compiles,
  it is not a player command, and it is dispatched as a standalone early return
  of its own straight after gambaprobe's (RunCommand's else-if chain is at
  MSVC's C1061 limit);
- its seven forms are `charms`, `repo`, `spawn`, `dress`, `heroic`, `sprite`
  and the bare marker, and nothing of it runs on the frame path;
- every game name is an hs-game-sdk constant, every call is by name, and no
  `Rva` constant or hex address reaches a call;
- the equipped charms are read as variables off Controller_obj.inventoryData,
  never through GetPlayerCharmData or GetProfileInventoryData;
- `dress` writes the two size keys at CreateItemNew's outermost return and
  touches nothing else, and the Custom Forge's player-build pass is unchanged;
- `sprite_add` and the probe's icon bytes appear only inside the research
  block, so the player build's only `sprite_add` stays the pack markers', and
  the icon is our own PNG, reproduced by its generator.

FlightCharmProbe.hpp's decisions are exercised by
test_flight_charm_probe_behavior.py.
"""
import importlib.util
import re
import struct
import sys
import unittest
from pathlib import Path

TESTS_DIR = Path(__file__).resolve().parent
ROOT = TESTS_DIR.parent
PLUGIN = ROOT / "plugin" / "ModuleMain.cpp"
INCLUDE = ROOT / "plugin" / "include" / "ForgePact"
HEADER = INCLUDE / "FlightCharmProbe.hpp"
ICON_HEADER = INCLUDE / "FlightCharmProbeIcon.hpp"
ASSETS = ROOT / "assets" / "flight-charms"
SDK_INCLUDE = ROOT.parent / "hs-game-sdk" / "cpp" / "include" / "hs_game_sdk"

if str(TESTS_DIR) not in sys.path:
    sys.path.insert(0, str(TESTS_DIR))

from test_release_hook_contract import function_body, strip_comments, strip_research_blocks  # noqa: E402

BLOCK_START = "// ---- charmprobe: the flight-charms phase 1 instrument (ForgePact #158)"
BLOCK_END = "#endif // FORGEPACT_RELEASE (charmprobe)"
FCP_SYMBOL = r"\b(?:g_|k)?Fcp[A-Z0-9_]\w*"

# The names the adapter must spell through hs-game-sdk constants.
SDK_SCRIPTS = ("gml_Script_DropItemHeroic", "gml_Script_GetItemSize", "gml_Script_GetUniqueRepoStruct",
               "gml_Script_InitItemFromJson", "gml_Script_LootGroundCreateFromItem")
SDK_OBJECTS = ("Controller_obj", "Loot_Ground_obj")

# The standard headers the core may include, and nothing else.
STD_HEADERS = {"algorithm", "cmath", "cstdint", "cstdio", "map", "set", "string", "string_view", "utility", "vector"}


class CharmProbeContract(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.plugin = PLUGIN.read_text(encoding="utf-8").replace("\r\n", "\n")
        cls.block = cls.plugin[cls.plugin.index(BLOCK_START):cls.plugin.index(BLOCK_END)]
        cls.code = strip_comments(cls.block)
        cls.shipped = strip_comments(strip_research_blocks(cls.plugin))
        cls.header = strip_comments(HEADER.read_text(encoding="utf-8").replace("\r\n", "\n"))

    def body(self, signature):
        return strip_comments(function_body(self.plugin, signature))

    # ---- research build only ----------------------------------------------------

    def test_every_fcp_symbol_and_the_verb_vanish_from_the_player_build(self):
        symbols = set(re.findall(FCP_SYMBOL, self.code))
        # A scan that finds nothing would pass vacuously.
        for expected in ("FcpCommand", "FcpCharms", "FcpRepo", "FcpSpawn", "FcpDress", "FcpDressBuilt", "FcpHeroic",
                         "FcpSprite", "FcpSpriteLoad", "FcpUsage", "FcpNs", "g_FcpSize", "g_FcpDress", "g_FcpSpriteKey",
                         "kFcpMaxCharms"):
            self.assertIn(expected, symbols)
        # Negative control: the strip keeps player code, so an absence below
        # is the strip working, not an empty string.
        self.assertIn("kPlayerCommands", self.shipped)
        self.assertIn("static void CustomForgePostProcess(", self.shipped)
        for symbol in sorted(symbols):
            self.assertIsNone(re.search(r"\b" + re.escape(symbol) + r"\b", self.shipped), symbol + " reaches the player build")
        self.assertIsNone(re.search(FCP_SYMBOL, self.shipped), "an Fcp* symbol reaches the player build")
        self.assertNotIn("charmprobe", self.shipped)
        self.assertNotIn("HandleCharmProbeCommand", self.shipped)
        player_text = strip_research_blocks(self.plugin)   # comments kept: not even a mention ships
        self.assertNotIn("FlightCharmProbe", player_text)
        self.assertNotIn("charmprobe", player_text)
        # Phase 2's shipping core, FlightCharms.hpp, shares the namespace
        # ForgePact::FlightCharms (ForgePact #158), so the namespace itself
        # ships. What must not is the probe's own: its alias, its icon
        # namespace, and every name its header declares that the core does not.
        self.assertIsNone(re.search(r"\bFcpNs\b", self.shipped))
        self.assertIsNone(re.search(r"\bForgePact::FlightCharmProbeIcon\b", self.shipped))
        core = strip_comments((INCLUDE / "FlightCharms.hpp").read_text(encoding="utf-8").replace("\r\n", "\n"))
        declared = r"\b(?:struct|class|enum(?:\s+class)?|using)\s+([A-Za-z_]\w*)"
        probe_only = set(re.findall(declared, self.header)) - set(re.findall(declared, core))
        self.assertTrue(probe_only)   # a scan that finds nothing would pass vacuously
        for name in sorted(probe_only):
            self.assertIsNone(re.search(r"\b(?:ForgePact::FlightCharms|FcNs)::" + re.escape(name) + r"\b", self.shipped),
                              name + " reaches the player build")

    def test_not_a_player_command(self):
        run = function_body(self.plugin, "static void RunCommand(const std::string& line)")
        commands = run[run.index("kPlayerCommands = {"):]
        commands = commands[:commands.index("};")]
        self.assertIn('"menulayout"', commands)   # the set is the one read, not an empty slice
        self.assertNotIn('"charmprobe"', commands)

    def test_dispatched_as_a_standalone_early_return(self):
        run = function_body(self.plugin, "static void RunCommand(const std::string& line)")
        self.assertIn("    if (HandleGambaProbeCommand(lc, rest)) return;\n"
                      "#endif\n"
                      "#ifndef FORGEPACT_RELEASE\n"
                      "    if (HandleCharmProbeCommand(lc, rest)) return;\n"
                      "#endif\n", run)
        self.assertEqual(run.count("HandleCharmProbeCommand"), 1)
        self.assertNotIn('"charmprobe"', run)
        self.assertNotIn("else if (HandleCharmProbeCommand", run)
        self.assertEqual(self.plugin.count('lc == "charmprobe"'), 1)
        handler = self.body("static bool HandleCharmProbeCommand(")
        self.assertIn('if (lc == "charmprobe") { FcpCommand(rest); return true; }', handler)
        self.assertIn("return false;", handler)
        # The handler is inside the research block, so its name ships nowhere.
        self.assertIn("static bool HandleCharmProbeCommand(", self.code)

    def test_seven_forms_and_nothing_on_the_frame_path(self):
        command = self.body("static void FcpCommand(const std::string& rest)")
        for form, handler in (("charms", "FcpCharms();"), ("repo", "FcpRepo(tail);"), ("spawn", "FcpSpawn(tail);"),
                              ("dress", "FcpDress(tail);"), ("heroic", "FcpHeroic(tail);"),
                              ("sprite", "FcpSprite(tail);")):
            self.assertIn('if (sub == "' + form + '") { ' + handler + " return; }", command)
        self.assertEqual(len(re.findall(r'if \(sub == "', command)), 6)
        self.assertIn("if (tok.empty()) { FcpUsage(); return; }", command)   # the bare marker
        usage = self.body("static void FcpUsage()")
        self.assertIn("Out(FcpNs::MarkerLine(g_FcpSize, g_FcpDress));", usage)
        self.assertIn('"charmprobe: research build marker="', self.header)
        frame = strip_comments(function_body(self.plugin, "void FrameCallback(FWFrame& FrameContext)"))
        self.assertIsNone(re.search(FCP_SYMBOL, frame), "charmprobe has no per-frame piece")

    # ---- names, calls and addresses ----------------------------------------------

    def test_game_names_are_sdk_constants(self):
        scripts = (SDK_INCLUDE / "scripts.hpp").read_text(encoding="utf-8")
        objects = (SDK_INCLUDE / "objects.hpp").read_text(encoding="utf-8")
        for name in SDK_SCRIPTS:
            self.assertIn(f'std::string_view {name} = "{name}";', scripts)   # the constant exists
            self.assertIn("HeroSiege::Scripts::" + name, self.code)
            short = name[len("gml_Script_"):]
            self.assertNotIn('"' + name + '"', self.code)
            self.assertNotIn('"' + short + '"', self.code)
        for name in SDK_OBJECTS:
            self.assertIsNotNone(re.search(r"\b" + name + r" = \d+,", objects))
            self.assertIn("HeroSiege::Objects::GameObject::" + name, self.code)
            self.assertNotIn('"' + name + '"', self.code)
        self.assertNotIn('"gml_Script_', self.code)
        # Every by-name script call names an SDK constant.
        calls = re.findall(r"CallGameScriptEx\(\s*\w+\s*,\s*([^,]+),", self.code)
        self.assertGreaterEqual(len(calls), 5)
        for target in calls:
            self.assertTrue(target.strip().startswith("HeroSiege::Scripts::gml_Script_"), target)
        self.assertNotIn("CallGameScript(", self.code)

    def test_no_rva_or_hex_address_reaches_a_call(self):
        for source in (self.code, self.header):
            self.assertIsNone(re.search(r"\b\w*Rva\w*\b", source))
            self.assertIsNone(re.search(r"\b0x[0-9A-Fa-f]{5,}\b", source))
            self.assertNotIn("GetModuleHandle", source)
            self.assertIsNone(re.search(r"\(\s*char\s*\*\s*\)\s*\w+\s*\+", source))
            self.assertNotIn("reinterpret_cast<PFUNC", source)

    def test_the_charms_are_read_as_variables(self):
        read = self.body("static bool FcpCharmArray(RValue& array, std::string& why)")
        self.assertIn("HeroSiege::Player::kRelicTabHolderField", read)          # inventoryData
        self.assertIn('"localCharmItemArray"', read)
        self.assertIn("HeroSiege::Player::kOnlineFlagGlobal", read)             # the relic tab's key rule
        self.assertIn('"mplr"', read)
        self.assertIn("key - 1", read)
        self.assertIn("HeroSiege::Player::IsInstanceHandle(controller)", read)  # never a kind check
        for getter in ("GetPlayerCharmData", "GetProfileInventoryData", "UpdateCharmData"):
            self.assertNotIn(getter, self.code)

    def test_charms_prints_each_items_stat_struct_and_writes_nothing(self):
        charms = self.body("static void FcpCharms()")
        # S13's read-back: the worn charm's own itemStatStruct, read like info=.
        self.assertIn('FcpMember(item, "itemStatStruct", statStruct)', charms)
        self.assertIn('" itemstats="', charms)
        self.assertIn('"missing(no itemStatStruct)"', charms)
        self.assertLess(charms.index('" itemstats="'), charms.index('" info="'))
        # A read only: the worn charms are never written. Negative control: the
        # scratch read-back FcpCharms ends with does write, in its own body.
        self.assertNotIn("variable_struct_set", charms)
        self.assertIn("variable_struct_set", self.body("static bool FcpSizeReadBack("))

    def test_the_heroic_roll_is_called_by_name_with_the_player(self):
        heroic = self.body("static void FcpHeroic(const std::vector<std::string>& tail)")
        self.assertIn("CallGameScriptEx(res, HeroSiege::Scripts::gml_Script_DropItemHeroic, self, self, { RValue(x), RValue(y) })",
                      heroic)
        self.assertIn("FcpNs::ParseHeroic(tail, n, why)", heroic)
        self.assertIn("HeroSiege::Player::kGroundItemInstanceField", heroic)
        self.assertIn("HeroSiege::Items::ItemType::Charm", heroic)

    def test_the_spawn_reuses_the_signature_items_route(self):
        spawn = self.body("static void FcpSpawn(const std::vector<std::string>& tail)")
        order = [spawn.index(s) for s in ('"json_parse"', "gml_Script_InitItemFromJson", "gml_Script_LootGroundCreateFromItem")]
        self.assertEqual(order, sorted(order))
        self.assertIn("FcpNs::SpawnSeed(g_FcpSpawns++)", spawn)
        for refusal in ("size keys unknown", "sprite not loaded", "sprite key unknown"):
            self.assertIn(refusal, spawn)
        # Each refusal comes before anything is built.
        self.assertLess(spawn.index("sprite key unknown"), spawn.index("gml_Script_InitItemFromJson"))

    def test_every_refusal_names_its_reason(self):
        self.assertIsNone(re.search(r'refused"\s*\)', self.code))
        self.assertIsNone(re.search(r'refused ?"\s*\);', self.code))
        self.assertGreaterEqual(self.code.count("refused "), 15)

    # ---- dress ---------------------------------------------------------------------

    def test_dress_touches_nothing_outside_its_research_rule(self):
        dress = self.body("static void FcpDressBuilt(RValue& item)")
        first = dress.strip().split("\n")[0].strip()
        self.assertEqual(first, "if (g_FcpDress.Empty() || g_TruthDepth != 0 || !g_FcpSize.Known()) return;")
        writes = re.findall(r'CallBuiltin\("(\w+)", \{ (\w+), RValue\((\w+\.\w+\(\))\)', dress)
        self.assertEqual(writes, [("variable_struct_set", "info", "g_FcpSize.W()"),
                                  ("variable_struct_set", "info", "g_FcpSize.H()")])
        self.assertEqual(dress.count("CallBuiltin("), 2)
        for other in ("CallGameScript", "variable_struct_remove", "variable_instance_set", "g_CustomForgeEntries"):
            self.assertNotIn(other, dress)
        self.assertIn("g_FcpDress.Find(seed)", dress)
        # Called from one place: the Custom Forge's pass, inside the research guard.
        self.assertEqual(self.plugin.count("FcpDressBuilt("), 3)   # declaration, call, definition
        post = function_body(self.plugin, "static void CustomForgePostProcess(")
        self.assertIn("#ifndef FORGEPACT_RELEASE\n    if (finalPass) FcpDressBuilt(result);\n#endif\n", post)
        self.assertLess(post.index("FcpDressBuilt"), post.index("TryApplyCustomForge"))
        shipped_post = strip_comments(function_body(strip_research_blocks(self.plugin), "static void CustomForgePostProcess("))
        self.assertEqual(re.sub(r"\s+", " ", shipped_post).strip(),
                         "PERF_SCOPE(g_PerfForge); if (TryApplyCustomForge(&result, finalPass)) return; "
                         "for (int i = 0; i < argc && i < 8; ++i) if (args && TryApplyCustomForge(args[i], finalPass)) return;")

    def test_dress_needs_the_size_keys_and_hooks_create_item_new_by_name(self):
        dress = self.body("static void FcpDress(const std::vector<std::string>& tail)")
        self.assertLess(dress.index("size keys unknown"), dress.index("g_FcpDress.Add(r.rule, why)"))
        self.assertIn("HookOneScript(SdkShortScriptName(HeroSiege::Scripts::gml_Script_CreateItemNew)", dress)
        self.assertIn("(PVOID)Hook_CreateItemNew, &g_Orig_CreateItemNew", dress)
        self.assertIn("CreateItemNew could not be hooked", dress)

    # ---- sprite --------------------------------------------------------------------

    def test_sprite_add_and_the_icon_bytes_stay_in_the_research_block(self):
        self.assertIn("sprite_add", self.code)                                # positive control
        # Phase 2's forge loads the six charm icons in both builds (ForgePact
        # #158, test_flight_charms_contract.py), from FlightIconsLoad, and the
        # Mark 1 set loads its pieces' icons the same way, from Mark1IconsLoad
        # (ForgePact #166, test_mark1_set_contract.py); no other player-build
        # code calls sprite_add.
        forge_load = function_body(self.plugin, "static void FlightIconsLoad()")
        self.assertIn("sprite_add", forge_load)                               # the flight charms' shipped call
        mark1_load = function_body(self.plugin, "static void Mark1IconsLoad()")
        self.assertIn("sprite_add", mark1_load)                               # the Mark 1 pieces' shipped call
        self.assertNotIn("sprite_add", strip_comments(strip_research_blocks(self.plugin)
                                                      .replace(forge_load, "").replace(mark1_load, "")))
        for probe_only in ("charmprobe", "FlightCharmProbeIcon", "kPng", "Fcp"):
            self.assertNotIn(probe_only, forge_load)
        self.assertNotIn("FlightCharmProbeIcon", self.shipped)
        self.assertNotIn("kPng", self.shipped)
        self.assertEqual(self.plugin.count("#include <ForgePact/FlightCharmProbeIcon.hpp>"), 1)
        self.assertIn("#include <ForgePact/FlightCharmProbeIcon.hpp>", self.block)
        # The pack markers' sprite_add is the only call any plugin header makes
        # (FrameProfilerBuiltins.hpp names builtins to time, and calls none).
        holders = sorted(h.name for h in INCLUDE.glob("*.hpp")
                         if 'CallBuiltin("sprite_add"' in strip_comments(h.read_text(encoding="utf-8")))
        self.assertEqual(holders, ["PackMarkers.hpp"])
        includers = sorted(h.name for h in INCLUDE.glob("*.hpp") if "FlightCharmProbeIcon" in h.read_text(encoding="utf-8"))
        self.assertEqual(includers, ["FlightCharmProbeIcon.hpp"])

    def test_the_sprite_route_is_the_pack_markers(self):
        load = self.body("static void FcpSpriteLoad()")
        self.assertIn('IPC_DIR + "\\\\charmprobe"', load)
        self.assertIn("if (!std::filesystem::exists(file, ec))", load)          # written only when absent
        self.assertIn("{ &relative, &file }", load)                             # relative first, absolute second
        self.assertIn("ApNumber(r, idx)", load)                                 # a reference or a real
        self.assertIn('"sprite_set_offset"', load)
        # The shipped route is not touched.
        self.assertNotIn("charmprobe", (INCLUDE / "PackMarkers.hpp").read_text(encoding="utf-8"))
        icon_path = function_body(self.plugin, "static bool PackMarkerIconPath(")
        self.assertNotIn("charmprobe", icon_path)

    def test_the_icon_is_our_own_png_reproduced_by_its_generator(self):
        png = (ASSETS / "charmprobe.png").read_bytes()
        self.assertEqual(png[:8], b"\x89PNG\r\n\x1a\n")
        width, height, depth, colour = struct.unpack(">IIBB", png[16:26])
        self.assertEqual((width, height, depth, colour), (32, 32, 8, 6))      # 32x32, 8-bit RGBA
        header = ICON_HEADER.read_text(encoding="utf-8")
        self.assertIn("Generated by assets/flight-charms/make_charmprobe_icon.py", header)
        self.assertIn("nothing taken from the game", header)
        body = header[header.index("kPng[] = {"):header.index("};")]
        embedded = bytes(int(b, 16) for b in re.findall(r"0x([0-9a-f]{2})", body))
        self.assertEqual(embedded, png)
        self.assertIn("inline constexpr int kSize = 32;", header)
        spec = importlib.util.spec_from_file_location("make_charmprobe_icon", ASSETS / "make_charmprobe_icon.py")
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        self.assertEqual(module.png_bytes(), png)

    # ---- the core ------------------------------------------------------------------

    def test_the_core_is_game_independent(self):
        includes = set(re.findall(r"#include <([^>]+)>", self.header))
        self.assertTrue(includes <= STD_HEADERS, includes - STD_HEADERS)
        for word in ("RValue", "CInstance", "YYTK", "g_Yytk", "CallBuiltin", "HeroSiege"):
            self.assertNotIn(word, self.header)
        self.assertIn("namespace ForgePact::FlightCharms", self.header)
        self.assertIn("#include <ForgePact/FlightCharmProbe.hpp>", self.block)
        self.assertNotIn("FlightCharmProbe.hpp", strip_research_blocks(self.plugin))


if __name__ == "__main__":
    unittest.main()
