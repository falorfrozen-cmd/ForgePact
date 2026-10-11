#!/usr/bin/env python3
"""Contract tests for `soulprobe`, the boss soul pieces research instrument
(ForgePact #197, docs/boss-soul-pieces-research.md § The instrument).

`soulprobe` is Live procedure 3's instrument: it reads the act table, the key
repository, the Key tab and the bag, places and picks up pieces of a chosen
base, watches the backpack's use candidates and the gem roll, reads, opens and
adjusts portals to an act's boss room, travels there through the game's own
script, lists the bosses in the room and kills one through its health record.
It is research build only, and it reaches the game only by name. These tests
read the plugin as text, comment-stripped, in the shape of
test_hidden_loot_contract.py: research only; placement through the loader;
the gem watch as a consumer of the shared DropGems detour; one both-route
detour per use candidate, none a script the player build already detours; the
portal made only from Portal_obj, its target written only through the
protected store's setter and only under a key inside the store; `goto` only
through NetworkRoomGoto; the boss kill only through `enemy_hp`; no stack hook
and no boss creation.
"""

import re
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from test_release_hook_contract import function_body, strip_comments, strip_research_blocks  # noqa: E402

FORGEPACT = Path(__file__).resolve().parents[1]
PLUGIN_SRC = FORGEPACT / "plugin" / "ModuleMain.cpp"
CORE_SRC = FORGEPACT / "plugin" / "include" / "ForgePact" / "SoulPieceMod.hpp"
SDK_ROOMS = FORGEPACT.parent / "hs-game-sdk" / "cpp" / "include" / "hs_game_sdk" / "rooms.hpp"

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

# Every reply line Live procedure 3 greps.
REPLY_PREFIXES = ("status", "keys", "act", "base", "dress", "gem", "place", "pickup", "keytab", "bag", "usewatch",
                  "usecall", "use", "gemwatch", "gemroll", "portals", "portal", "portalwatch", "portalset",
                  "portalroute", "portaloffset", "goto", "bosses", "kill")

# Subcommands Replan 1 removed, with the state behind them.
REMOVED = ("stackhook", "spawn", "spawnroute", "spawnoffset", "take", "sprite")

# The act bosses a pentagram brings out (ctx "### Act bosses"), as their SDK
# names: nothing in the probe may create one.
BOSS_OBJECTS = ("Gurag_obj", "Reaper_obj", "Anubis_obj", "Damien_obj", "Karp_King_obj", "Satan_obj", "Mevius_obj",
                "Odin_Phase_1_obj", "Cthulhu_obj", "Enemy_Child_Boss_obj")


def hook_one_script_calls(code):
    """Every `HookOneScript(...)` call statement in `code`, up to its `;`."""
    return [code[m.start():code.index(";", m.start())] for m in re.finditer(r"\bHookOneScript\(", code)]


def player_build_detours(player_code, short_name):
    """The player build's HookOneScript calls that name `short_name`, either as
    a literal or through its SDK constant."""
    pattern = re.compile(r'"%s"|gml_Script_%s\b|Scripts::%s\b' % (short_name, short_name, short_name))
    return [call for call in hook_one_script_calls(player_code)
            if not call.startswith("HookOneScript(const char*") and pattern.search(call)]


def calls_of(code, callee):
    """Every call `callee(...)` in `code`, as the text from the callee's name
    to the end of its line."""
    return re.findall(r"[^\n]*\b%s\([^\n]*" % re.escape(callee), code)


def store_writes(code):
    """Every call of the protected store's setter, by its constant."""
    return re.findall(r"CallGameScript(?:Ex)?\([^;]*kSpSetVarFull[^;]*;", code)


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
        for name in ("soulprobe", "SoulProbe", "SpUseCall", "kSpUseTargets", "Hook_SoulProbe", "SpOpenPortal",
                     "SpNs::"):
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

    def test_every_subcommand_is_dispatched(self):
        command = self.body("static void SoulProbeCommand(")
        for sub in ("status", "keys", "act", "base", "dress", "gem", "place", "pickup", "keytab", "bag", "portals",
                    "portal", "portalset", "goto", "bosses", "portalroute", "portaloffset", "usewatch", "gemwatch",
                    "kill"):
            self.assertIn('sub == "%s"' % sub, command, sub)

    def test_replan_1_removed_the_stack_hook_the_spawn_the_take_and_the_sprite(self):
        command = self.body("static void SoulProbeCommand(")
        for sub in REMOVED:
            self.assertNotIn('sub == "%s"' % sub, command, sub)
            self.assertNotIn('"soulprobe %s:' % sub, self.section, sub)
        for gone in ("SpStackMode", "takeClear", "useArmed", "spawnScp", "SpSpawn", "kSpClearName", "kSpGemSpriteName",
                     "g_Sp.sprite", "VetoStack", "Hook_SoulProbeStack"):
            self.assertNotIn(gone, self.section, gone)
        status = self.body("static void SpStatus()")
        for field in ("base=", "category=", "dress=", "usewatch=", "usescript=", "gemwatch=", "portalroute=",
                      "portaloffset=", "usecalls=", "usecallsby=", "uses=", "portals=", "gemrolls=", "gemroute="):
            self.assertIn(field, status, field)
        for field in ("stackhook=", "sprite=", "take=", "spawnroute=", "spawned=", "armed=", "suppressed="):
            self.assertNotIn(field, status, field)

    # ---- placement ---------------------------------------------------------------------
    def test_pieces_are_placed_through_the_loader_not_loot_ground_create(self):
        self.assertIn("kSpFromJsonFull = HeroSiege::Scripts::gml_Script_InitItemFromJson.data();", self.section)
        self.assertIn("kSpLootFromItemFull = HeroSiege::Scripts::gml_Script_LootGroundCreateFromItem.data();", self.section)
        place = self.body("static bool SpPlaceOne(")
        loader = place.index("CallGameScriptEx(item, kSpFromJsonFull, g, g,")
        ground = place.index("CallGameScriptEx(res, kSpLootFromItemFull, self, self,")
        self.assertLess(loader, ground)
        self.assertIn('\\"o\\":1}', place, "each placed item is one item, count 1")
        self.assertIn("g_Sp.baseCategory", place, "the save key names the base's class")
        command = self.body("static void SpPlaceCommand(")
        self.assertIn("GpPlayerXY(px, py, &self)", command, "the player is the placement's self")
        self.assertIn("SpNs::SeedOfAct(act)", command, "a piece's seed is the core's")
        self.assertIn("seed >= 777000 && seed <= 777999", command, "a plain item's seed is never a reserved one")
        self.assertIsNone(re.search(r"LootGroundCreate(?!FromItem)", self.section),
                          "LootGroundCreate writes a fresh seed; the probe never calls it")

    def test_the_base_is_found_by_repository_name_in_categories_11_then_12(self):
        self.assertIn("kSpConsumableCategory = (int)HeroSiege::Items::ItemType::Consumable;", self.section)
        self.assertIn("kSpKeyCategory = (int)HeroSiege::Items::ItemType::Key;", self.section)
        base = self.body("static void SpBaseCommand(")
        self.assertIn("for (const int category : { kSpConsumableCategory, kSpKeyCategory })", base)
        self.assertIn("Lower(name) != want", base)
        self.assertIn("SpNs::kBaseRepoName", base, "`auto` is the core's base name")
        self.assertIsNone(re.search(r"baseIndex\s*=\s*\d", base), "never an index constant")
        piece = self.body("static int SpPieceAct(")
        self.assertIn("SpNs::PieceActOfItem(", piece, "a consumable piece is the core's to recognise")

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
        room = self.body("static SpRoom SpReadRoom()")
        self.assertIn("SpNs::ActOfRoomActIndex(isNumber, n)", room, "the act is the core's conversion")

    # ---- the use watch -----------------------------------------------------------------------
    def test_each_use_candidate_is_one_both_route_detour_by_sdk_constant(self):
        table = self.section[self.section.index("kSpUseTargets[] = {"):]
        table = table[:table.index("};")]
        for constant in USE_CANDIDATES:
            self.assertEqual(1, len(re.findall(r"HeroSiege::Scripts::%s\b" % constant, table)), constant)
        # Outside the table no candidate is named at all.
        outside = self.section.replace(table, "")
        for constant in USE_CANDIDATES:
            self.assertIsNone(re.search(r"HeroSiege::Scripts::%s\b" % constant, outside), constant)
        ids = re.findall(r'"(fp_soulprobe_[a-z_]+)"', self.section)
        self.assertEqual(len(ids), len(set(ids)), "one hook id per detour")
        self.assertEqual(len(USE_CANDIDATES), len(ids))
        hooks = self.section[self.section.index("kSpUseHooks[] = {"):]
        hooks = hooks[:hooks.index("};")]
        self.assertEqual(len(USE_CANDIDATES), len(re.findall(r"Hook_SoulProbeUse<\d>", hooks)))
        install = self.body("static void SpInstallUseWatch()")
        self.assertIn("HookOneScript(kSpUseTargets[i].name, kSpUseTargets[i].id, (PVOID)kSpUseHooks[i], &g_SpUseOrig[i], &native)",
                      install)
        self.assertIn("if (g_Sp.useTried)", install, "installed once")
        # One install site in all, HookOneScript; never the table-only installer.
        self.assertEqual(1, len(hook_one_script_calls(self.section)))
        self.assertNotIn("HookOneScriptTable", self.section)
        self.assertNotIn("MmCreateHook", self.section)

    def test_no_candidate_is_a_script_the_player_build_detours(self):
        for name in USE_CANDIDATES.values():
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

    def test_a_piece_in_the_chosen_script_never_reaches_the_game(self):
        use = self.body("static RValue& SpUseCall(")
        branch = use.index("if (g_Sp.use == SpUseMode::Use && act && n == g_Sp.useScript)")
        other = use.index("res = orig ? &orig(S, O, R, argc, A) : &R;", branch)
        intercepted = use[branch:other]
        self.assertNotIn("orig(", intercepted, "the original is never called for a piece in `use`")
        self.assertIn("SpNs::DecideUse(true, room.name, room.act, act)", intercepted)
        self.assertLess(intercepted.index("SpNs::UseVerdict::Portal"), intercepted.index("SpOpenPortal(act, g_Sp.portalScp)"),
                        "a portal opens only on the core's `portal`")
        self.assertIn("R = opened ? (g_SpHaveControl[n] ? g_SpControlRet[n] : RValue(true)) : RValue(false);", intercepted,
                      "the control's answer on success, false on a refusal or a failed portal")
        self.assertIn("res = orig ? &orig(S, O, R, argc, A) : &R;", use[other:], "everything else passes to the game")
        self.assertIn("SpNs::RoomEligible(room.name, room.act, refusal)", self.body("static void SpActCommand()"))
        self.assertIn("namespace SpNs = ForgePact::SoulPieces;", self.section)
        self.assertIn("#include <ForgePact/SoulPieceMod.hpp>", self.code)

    # ---- the portal --------------------------------------------------------------------------
    def test_the_portal_is_made_only_from_portal_obj_by_sdk_constant(self):
        portal = self.body("static SpPortalResult SpOpenPortal(")
        self.assertIn("HeroSiege::Objects::GameObject::Portal_obj", portal)
        self.assertIn("ApCallScript(kSpScpName, self, { RValue(r.x), RValue(r.y), RValue(index) }, id)", portal)
        self.assertIn('"instance_create_depth", { RValue(r.x), RValue(r.y), RValue(depth), RValue(index) }', portal)
        self.assertIn("kSpScpName = SdkShortScriptName(HeroSiege::Scripts::gml_Script_sCP);", self.section)
        # The index both routes create is the one resolved from Portal_obj, and
        # nothing else in the probe creates an instance.
        self.assertEqual(1, len(re.findall(r"double index = -1;", portal)))
        self.assertEqual(1, len(re.findall(r'"instance_create_depth"', self.section)))
        self.assertIsNone(re.search(r'"instance_create(?:_layer)?"|"instance_copy"', self.section))
        self.assertEqual(1, len(re.findall(r"ApCallScript\(kSpScpName,", self.section)))
        # The target is the core's boss room, resolved by name.
        room = self.body("static bool SpBossRoom(")
        self.assertIn("SpNs::PortalTarget(act)", room)
        self.assertIn('"asset_get_index", { RValue(std::string(name)) }', room)
        self.assertIn('"room_exists"', room)

    def test_the_target_is_written_only_through_the_stores_setter_under_a_key_in_range(self):
        self.assertIn("kSpSetVarFull = HeroSiege::Scripts::gml_Script_PC_SetVariableGMLWrapper.data();", self.section)
        self.assertIn("kSpGetVarFull = HeroSiege::Scripts::gml_Script_PC_GetVariableGMLWrapper.data();", self.section)
        read = self.body("static bool SpProtectedRead(")
        self.assertLess(read.index("BossProbeKeyInRange(key)"), read.index("CallGameScript(kSpGetVarFull, { key })"),
                        "a store read only for a whole-number key inside the store")
        # Every write sits in a function that first proved its key through
        # SpProtectedRead (or, for the kill, BossProbeKeyInRange), and writes
        # under that key.
        writers = {
            "static SpPortalResult SpOpenPortal(": "SpProtectedRead(id, \"portalZone\", key, was, why)",
            "static void SpPortalSetCommand(": "SpProtectedRead(h, var, key, was, why)",
            "static void SpKillCommand(": "BossProbeKeyInRange(key)",
        }
        total = 0
        for signature, proof in writers.items():
            body = self.body(signature)
            writes = store_writes(body)
            self.assertTrue(writes, signature)
            for w in writes:
                self.assertRegex(w, r"\{ key, ", signature)
                self.assertLess(body.index(proof), body.index(w), signature)
            total += len(writes)
        self.assertEqual(total, len(store_writes(self.section)), "no other store write in the probe")
        # Every store read in the probe goes through SpProtectedRead, or reads
        # back a key it has just written under.
        reads = re.findall(r"CallGameScript\(kSpGetVarFull, \{ key \}\)", self.section)
        self.assertEqual(len(reads), len(re.findall(r"kSpGetVarFull", self.section)) - 1)
        # The portal's room is checked on the way back.
        portal = self.body("static SpPortalResult SpOpenPortal(")
        self.assertIn("got != r.room", portal)
        self.assertIn('"portalZone", "portalActive", "isEligible", "portalUsable", "pSpwd"', self.section)
        portalset = self.body("static void SpPortalSetCommand(")
        self.assertIn("HeroSiege::Objects::IsDescendantOf(oi, parent)", portalset, "only on a portal")

    def test_the_negative_control_of_the_store_scan(self):
        # A write with no key proof before it, or under another value, is caught.
        unproven = "g_Yytk->CallGameScript(kSpSetVarFull, { RValue(-1.0), RValue(0.0) });"
        self.assertEqual(1, len(store_writes(unproven)))
        self.assertIsNone(re.search(r"\{ key, ", store_writes(unproven)[0]))

    def test_goto_travels_only_through_network_room_goto(self):
        self.assertIn("kSpRoomGotoFull = HeroSiege::Scripts::gml_Script_NetworkRoomGoto.data();", self.section)
        goto = self.body("static void SpGotoCommand(")
        self.assertIn("CallGameScriptEx(res, kSpRoomGotoFull, self, self, { RValue(room) })", goto)
        self.assertIn("SpBossRoom(act, target, room, why)", goto)
        self.assertEqual(1, len(re.findall(r"kSpRoomGotoFull", self.section)) - 1, "called in one place only")
        self.assertIsNone(re.search(r'"room_goto\w*"', self.section), "never the raw room_goto builtin")
        self.assertNotIn("gml_Script_RoomGoto", self.section)

    def test_every_boss_room_is_an_sdk_room(self):
        core = CORE_SRC.read_text(encoding="utf-8")
        rooms = re.findall(r'"(Act_0\d_Boss)", true \}', core)
        self.assertEqual(9, len(rooms))
        if SDK_ROOMS.exists():
            sdk = SDK_ROOMS.read_text(encoding="utf-8")
            for room in rooms:
                self.assertRegex(sdk, r"\b%s = \d+," % room, room)

    # ---- names -------------------------------------------------------------------------------
    def test_scripts_and_objects_resolve_by_sdk_name(self):
        self.assertNotIn('"gml_Script_', self.section, "every script name is an hs-game-sdk constant")
        self.assertIsNone(re.search(r'"[A-Za-z0-9_]+_obj"', self.section), "every object is an hs-game-sdk constant")
        self.assertIsNone(re.search(r'"Act_\d', self.section), "every room name is the core's")
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
        instances = self.body("static std::vector<RValue> SpInstancesOf(")
        self.assertIn("SpResolveObject(std::string(HeroSiege::Objects::GetObjectName(obj))", instances)
        self.assertIsNone(re.search(r"\bk\w*Rva\w*\b|0x[0-9A-Fa-f]{6,}", self.section), "no address")
        self.assertNotIn("Hero_Siege.exe+", self.section_raw, "no module offset, even in a comment")

    def test_gem_rate_and_key_table_are_found_by_repository_name(self):
        rate = self.body("static bool SpGemRate(")
        self.assertIn("RepoAd(st) == SpNs::kGemRepoName", rate)
        self.assertIn("RepoIndexValid(SpNs::kGemCategory, i)", rate)
        keys = self.body("static std::vector<SpKeyRow> SpReadKeys(")
        self.assertIn("RepoIndexValid(kSpKeyCategory, i)", keys)
        self.assertIn("kSpKeyDungeonFull", keys)
        self.assertIn("kSpDungeonKeysFull", keys)

    # ---- the boss ------------------------------------------------------------------------------
    def test_the_boss_kill_is_only_through_its_enemy_hp_key(self):
        kill = self.body("static void SpKillCommand(")
        self.assertIn('RValue("enemy_hp")', kill)
        self.assertIn("BossProbeKeyInRange(key)", kill, "only a key inside the store is handed to the setter")
        self.assertIn("g_Yytk->CallGameScript(kSpSetVarFull, { key, RValue(0.0) })", kill)
        self.assertNotIn("instance_destroy", kill)
        # The only destroy in the probe is the picked-up ground item's.
        self.assertEqual(1, self.section.count('"instance_destroy"'))
        self.assertIn('"instance_destroy"', self.body("static void SpPickupCommand()"))
        bosses = self.body("static void SpBossesCommand()")
        self.assertIn("HeroSiege::Objects::GameObject::Enemy_Child_Boss_obj", bosses)
        self.assertIn("SpHp(h)", bosses)

    def test_no_stack_hook_and_no_boss_is_created(self):
        self.assertNotIn("ItemsAreStackable", self.section)
        for name in ("InventoryGridAddToStack", "GridAddToStack", "AddToInventory"):
            self.assertNotIn("gml_Script_%s" % name, self.section, name)
        for boss in BOSS_OBJECTS:
            for call in calls_of(self.section, "SpOpenPortal") + re.findall(r"[^\n]*instance_create_depth[^\n]*", self.section):
                self.assertNotIn(boss, call, boss)
        # The one object both creation routes resolve is Portal_obj.
        portal = self.body("static SpPortalResult SpOpenPortal(")
        resolved = re.findall(r"SpResolveObject\(std::string\(HeroSiege::Objects::GetObjectName\(HeroSiege::Objects::GameObject::(\w+)\)\)",
                              portal)
        self.assertEqual(["Portal_obj"], resolved)
        self.assertEqual(1, len(re.findall(r"\bSpResolveObject\(", portal)))
        self.assertEqual(3, len(calls_of(self.section, "SpOpenPortal")),
                         "its definition, `portal` and a use; made nowhere else")


if __name__ == "__main__":
    unittest.main()
