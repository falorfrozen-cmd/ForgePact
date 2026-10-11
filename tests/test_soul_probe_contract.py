#!/usr/bin/env python3
"""Contract tests for `soulprobe`, the boss soul pieces research instrument
(ForgePact #197, docs/boss-soul-pieces-research.md § The instrument).

`soulprobe` is Live procedure 1's instrument: it reads the act table, the key
repository and the Key tab, places and picks up pieces, watches the stack
check, the backpack's use candidates and the gem roll, and spawns and kills act
bosses. It is research build only, and it reaches the game only by name. These
tests read the plugin as text, comment-stripped, in the shape of
test_hidden_loot_contract.py: research only; placement through the loader;
the gem watch as a consumer of the shared DropGems detour; one both-route
detour per use candidate, none a script the player build already detours;
scripts and objects by SDK constant; the boss kill only through `enemy_hp`.
"""

import re
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from test_release_hook_contract import function_body, strip_comments, strip_research_blocks  # noqa: E402

FORGEPACT = Path(__file__).resolve().parents[1]
PLUGIN_SRC = FORGEPACT / "plugin" / "ModuleMain.cpp"
SDK_SPRITES = FORGEPACT.parent / "hs-game-sdk" / "python" / "hs_game_sdk" / "sprites.py"

SECTION_START = "// ===== Boss soul pieces research probe (`soulprobe`, ForgePact #197)"
SECTION_END = "#endif // FORGEPACT_RELEASE (soulprobe)"

# The use candidates (ctx "### Using a piece"), by their hs-game-sdk constant.
USE_CANDIDATES = {
    "gml_Script_UiAInventoryConsumeItemConfirm": "UiAInventoryConsumeItemConfirm",
    "gml_Script_CanUsePotion": "CanUsePotion",
    "gml_Script_PotionUse": "PotionUse",
    "gml_Script_PotionUseFlask": "PotionUseFlask",
    "gml_Script_InvGridClearItemNode": "InvGridClearItemNode",
    "gml_Script_InvGridEquipV2": "InvGridEquipV2",
    "ConsumeToken": "ConsumeToken",
    "gml_Script_UiASteamInventoryItemUse": "UiASteamInventoryItemUse",
    "gml_Script_UiAGuildStashConsumeTabToken": "UiAGuildStashConsumeTabToken",
}
STACK_SCRIPT = "ItemsAreStackable"

# Every reply line Live procedure 1 greps.
REPLY_PREFIXES = ("status", "keys", "act", "base", "gem", "place", "pickup", "keytab", "stack", "usewatch",
                  "usecall", "use", "take", "gemwatch", "gemroll", "spawn", "spawnwatch", "kill")


def hook_one_script_calls(code):
    """Every `HookOneScript(...)` call statement in `code`, up to its `;`."""
    return [code[m.start():code.index(";", m.start())] for m in re.finditer(r"\bHookOneScript\(", code)]


def player_build_detours(player_code, short_name):
    """The player build's HookOneScript calls that name `short_name`, either as
    a literal or through its SDK constant."""
    pattern = re.compile(r'"%s"|gml_Script_%s\b|Scripts::%s\b' % (short_name, short_name, short_name))
    return [call for call in hook_one_script_calls(player_code)
            if not call.startswith("HookOneScript(const char*") and pattern.search(call)]


class SoulProbeContractTests(unittest.TestCase):

    @classmethod
    def setUpClass(cls):
        cls.plugin_code = PLUGIN_SRC.read_text(encoding="utf-8").replace("\r\n", "\n")
        cls.code = strip_comments(cls.plugin_code)
        cls.player = strip_comments(strip_research_blocks(cls.plugin_code))
        cls.allowlist = re.search(r"kPlayerCommands\s*=\s*\{(?P<body>.*?)\};", cls.plugin_code, re.S).group("body")
        start = cls.plugin_code.index(SECTION_START)
        end = cls.plugin_code.index(SECTION_END, start)
        cls.section_raw = cls.plugin_code[start:end]
        cls.section = strip_comments(cls.section_raw)

    def body(self, signature):
        return strip_comments(function_body(self.plugin_code, signature))

    # ---- research only ---------------------------------------------------------------
    def test_soulprobe_is_research_only(self):
        self.assertNotIn("soulprobe", self.allowlist)
        guard = self.plugin_code.rfind("#ifndef FORGEPACT_RELEASE", 0, self.plugin_code.index(SECTION_START))
        self.assertLess(self.plugin_code.rfind("#endif", 0, self.plugin_code.index(SECTION_START)), guard,
                        "the probe section opens inside its own #ifndef FORGEPACT_RELEASE")
        for name in ("soulprobe", "SoulProbe", "SpUseCall", "kSpUseTargets", "Hook_SoulProbe", "SpNs::"):
            self.assertNotIn(name, self.player, name)
        self.assertIsNone(re.search(r"\bg_Sp\b|\bg_Sp[A-Z]", self.player), "the probe's state stays in the research build")
        self.assertIn('if (lc == "soulprobe") { SoulProbeCommand(rest); return; }', self.code)
        # The dress consumer compiles to nothing in the player build.
        self.assertRegex(self.plugin_code,
                         r"#else\n\s*#define SOUL_PROBE_AFTER_CREATE\(RES\) \(\(void\)0\)\n#endif")
        self.assertIn("SOUL_PROBE_AFTER_CREATE(RES) ((void)0)", self.player)
        # The tick runs only in the research build.
        self.assertIn("if (g_Setup) SoulProbeTick(fc);", self.code)
        self.assertNotIn("SoulProbeTick", self.player)

    def test_every_reply_starts_with_its_subcommand(self):
        for sub in REPLY_PREFIXES:
            self.assertIn('"soulprobe %s:' % sub, self.section, sub)

    # ---- placement ---------------------------------------------------------------------
    def test_pieces_are_placed_through_the_loader_not_loot_ground_create(self):
        self.assertIn("kSpFromJsonFull = HeroSiege::Scripts::gml_Script_InitItemFromJson.data();", self.section)
        self.assertIn("kSpLootFromItemFull = HeroSiege::Scripts::gml_Script_LootGroundCreateFromItem.data();", self.section)
        place = self.body("static void SpPlaceCommand(")
        loader = place.index("CallGameScriptEx(item, kSpFromJsonFull, g, g,")
        ground = place.index("CallGameScriptEx(res, kSpLootFromItemFull, self, self,")
        self.assertLess(loader, ground)
        self.assertIn("GpPlayerXY(px, py, &self)", place, "the player is the placement's self")
        self.assertIsNone(re.search(r"LootGroundCreate(?!FromItem)", self.section),
                          "LootGroundCreate writes a fresh seed; the probe never calls it")

    # ---- the gem watch -------------------------------------------------------------------
    def test_gem_watch_is_a_consumer_of_the_shared_drop_gems_detour(self):
        hook = function_body(self.plugin_code, "static RValue& Hook_DropGems(")
        original = hook.index("g_Orig_DropGems(S, O, R, argc, A)")
        consumer = hook.index("SoulProbeGemRoll(S);")
        self.assertLess(original, consumer, "the consumer runs after the game's own DropGems returned")
        self.assertRegex(hook, r"#ifndef FORGEPACT_RELEASE\s*SoulProbeGemRoll\(S\);\s*#endif")
        self.assertNotIn("SoulProbeGemRoll", function_body(self.player, "static RValue& Hook_DropGems("))
        # One DropGems detour in the whole plugin, and none of it the probe's.
        self.assertEqual(1, self.code.count("SdkShortScriptName(HeroSiege::Scripts::gml_Script_DropGems)"))
        self.assertNotIn("DropGems", "".join(hook_one_script_calls(self.section)))
        self.assertNotIn("Hook_DropGems", self.section)
        tick = self.body("static void SoulProbeTick(")
        self.assertIn("HhResolveLocalPlayer(p)", tick)
        self.assertIn("GemsInstallDropHook()", tick, "installed through the gem mod's own installer")
        roll = self.body("static void SoulProbeGemRoll(")
        self.assertIn("SpNs::DecideDrop(true, false,", roll, "the core's verdict, with the mod on and not forced")
        self.assertIn("SpNs::PieceDenominator(G)", roll)

    # ---- the use watch and the stack hook ---------------------------------------------------
    def test_each_use_candidate_is_one_both_route_detour_by_sdk_constant(self):
        table = self.section[self.section.index("kSpUseTargets[] = {"):]
        table = table[:table.index("};")]
        for constant in USE_CANDIDATES:
            self.assertEqual(1, len(re.findall(r"HeroSiege::Scripts::%s\b" % constant, table)), constant)
        # Outside the table a candidate is named only to be called by name
        # (take clear's InvGridClearItemNode), never to be hooked.
        outside = self.section.replace(table, "")
        for constant in USE_CANDIDATES:
            for line in re.findall(r"[^\n]*HeroSiege::Scripts::%s\b[^\n]*" % constant, outside):
                self.assertIn("kSpClearName = SdkShortScriptName(", line, constant)
        ids = re.findall(r'"(fp_soulprobe_[a-z_]+)"', self.section)
        self.assertEqual(len(ids), len(set(ids)), "one hook id per detour")
        self.assertEqual(len(USE_CANDIDATES) + 1, len(ids))
        hooks = self.section[self.section.index("kSpUseHooks[] = {"):]
        hooks = hooks[:hooks.index("};")]
        self.assertEqual(len(USE_CANDIDATES), len(re.findall(r"Hook_SoulProbeUse<\d>", hooks)))
        install = self.body("static void SpInstallUseWatch()")
        self.assertIn("HookOneScript(kSpUseTargets[i].name, kSpUseTargets[i].id, (PVOID)kSpUseHooks[i], &g_SpUseOrig[i], &native)",
                      install)
        self.assertIn("if (g_Sp.useTried)", install, "installed once")
        stack = self.body("static void SpInstallStackHook()")
        self.assertIn("HookOneScript(kSpStackName,", stack)
        self.assertIn("&native", stack)
        self.assertIn("kSpStackName = SdkShortScriptName(HeroSiege::Scripts::gml_Script_ItemsAreStackable);", self.section)
        # Two install sites in all, both HookOneScript; never the table-only installer.
        self.assertEqual(2, len(hook_one_script_calls(self.section)))
        self.assertNotIn("HookOneScriptTable", self.section)
        self.assertNotIn("MmCreateHook", self.section)

    def test_no_candidate_is_a_script_the_player_build_detours(self):
        for name in list(USE_CANDIDATES.values()) + [STACK_SCRIPT]:
            self.assertEqual([], player_build_detours(self.player, name), name)

    def test_the_player_build_detour_scan_sees_a_detour(self):
        # Negative control for the scan above: a player-build install of a
        # candidate, by literal or by SDK constant, is found.
        literal = 'HookOneScript("PotionUse", "x", (PVOID)Hook_X, &g_X);'
        constant = 'HookOneScript(SdkShortScriptName(HeroSiege::Scripts::gml_Script_InvGridEquipV2), "x", (PVOID)Hook_X, &g_X);'
        call_only = 'ApCallScript(kApClearName, gridInst, { cellNow, RValue() }, clearRes);'
        self.assertEqual(1, len(player_build_detours(literal, "PotionUse")))
        self.assertEqual(1, len(player_build_detours(constant, "InvGridEquipV2")))
        self.assertEqual([], player_build_detours(call_only, "InvGridClearItemNode"))
        self.assertEqual([], player_build_detours(literal, "PotionUseFlask"))

    def test_stack_veto_and_use_are_the_cores_decisions(self):
        stack = self.body("static RValue& Hook_SoulProbeStack(")
        self.assertLess(stack.index("g_SpStackOrig(S, O, R, argc, A)"), stack.index("SpNs::VetoStack(true,"),
                        "consulted only after the game answered")
        self.assertIn("game && g_Sp.stack == SpStackMode::Veto", stack)
        use = self.body("static RValue& SpUseCall(")
        self.assertIn("SpNs::DecideUse(true, room.name, act)", use)
        # The piece is taken only once the boss exists.
        self.assertLess(use.index("SpSpawn(boss->bossObject"), use.index("if (spawned && !g_Sp.takeClear)"))
        self.assertIn("if (spawned && g_Sp.takeClear)", use)
        self.assertIn("namespace SpNs = ForgePact::SoulPieces;", self.section)
        self.assertIn("#include <ForgePact/SoulPieceMod.hpp>", self.code)
        act = self.body("static void SpActCommand()")
        self.assertIn("SpNs::RoomEligible(room.name, room.act, refusal)", act)

    # ---- names -------------------------------------------------------------------------------
    def test_scripts_and_objects_resolve_by_sdk_name(self):
        self.assertNotIn('"gml_Script_', self.section, "every script name is an hs-game-sdk constant")
        self.assertIsNone(re.search(r'"[A-Za-z0-9_]+_obj"', self.section), "every object is an hs-game-sdk constant")
        for full in re.findall(r"static constexpr const char\* kSp\w+Full = ([^;]+);", self.section):
            self.assertRegex(full, r"^HeroSiege::Scripts::gml_Script_\w+\.data\(\)$")
        # Every game script the probe calls by name is one of those constants.
        calls = re.findall(r"CallGameScript\(", self.section) + re.findall(r"CallGameScriptEx\(", self.section)
        named = (re.findall(r"CallGameScript\(kSp\w+Full,", self.section)
                 + re.findall(r"CallGameScriptEx\(\w+, kSp\w+Full,", self.section))
        self.assertTrue(calls)
        self.assertEqual(len(calls), len(named))
        resolve = self.body("static bool SpResolveObject(")
        self.assertIn("HeroSiege::Objects::GetObjectName(", resolve, "an object must be in the SDK's table")
        self.assertIn('"asset_get_index"', resolve, "and is resolved by name at run time")
        self.assertIsNone(re.search(r"\bk\w*Rva\w*\b|0x[0-9A-Fa-f]{6,}", self.section), "no address")
        # The one non-SDK-binding name, the gem's sprite, is hs-game-sdk's.
        self.assertIn('kSpGemSpriteName = "Gem_Incarnation_spr";', self.section)
        if SDK_SPRITES.exists():
            self.assertIn("Gem_Incarnation_spr", SDK_SPRITES.read_text(encoding="utf-8"))

    def test_gem_rate_and_base_keys_are_found_by_repository_name(self):
        rate = self.body("static bool SpGemRate(")
        self.assertIn("RepoAd(st) == SpNs::kGemRepoName", rate)
        self.assertIn("RepoIndexValid(SpNs::kGemCategory, i)", rate)
        base = self.body("static void SpBaseCommand(")
        self.assertIn("Lower(r.name) == want", base)
        keys = self.body("static std::vector<SpKeyRow> SpReadKeys(")
        self.assertIn("RepoIndexValid(SpNs::kKeyCategory, i)", keys)
        self.assertIn("kSpKeyDungeonFull", keys)
        self.assertIn("kSpDungeonKeysFull", keys)

    # ---- the boss kill ---------------------------------------------------------------------------
    def test_the_boss_kill_is_only_through_its_enemy_hp_key(self):
        self.assertIn("kSpSetVarFull = HeroSiege::Scripts::gml_Script_PC_SetVariableGMLWrapper.data();", self.section)
        kill = self.body("static void SpKillCommand(")
        self.assertIn('RValue("enemy_hp")', kill)
        self.assertIn("BossProbeKeyInRange(key)", kill, "only a key inside the store is handed to the setter")
        self.assertIn("g_Yytk->CallGameScript(kSpSetVarFull, { key, RValue(0.0) })", kill)
        self.assertNotIn("instance_destroy", kill)
        # The only destroy in the probe is the picked-up ground item's.
        self.assertEqual(1, self.section.count('"instance_destroy"'))
        self.assertIn('"instance_destroy"', self.body("static void SpPickupCommand()"))


if __name__ == "__main__":
    unittest.main()
