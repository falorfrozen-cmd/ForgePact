#!/usr/bin/env python3
"""Contract tests for boss soul pieces (`soulpiece`, ForgePact #197), the
shipped mod's adapter in ModuleMain.cpp.

While the mod is on, an ordinary monster dying in a zone of an act can drop a
Piece of <boss>'s Soul (the game's own Edible PAS Logo marked by a reserved
seed), rolled inside the game's gem roll; right-clicking a piece in the
backpack and confirming moves the player straight into the act's boss room
through the game's own NetworkRoomGoto, after the boss room's saved zone state
is dropped with the game's own ZoneStateResetSingle so the boss comes out
again (Live procedure 3's goto-travel; the owner's direct transfer and fresh
boss, 2026-10-11). The decisions are the core's (SoulPieceMod.hpp, exercised by
soul_piece_harness.cpp through test_soul_piece_behavior.py); these tests read
the adapter as text, comment-stripped, in the shape of
test_soul_probe_contract.py: off by default and a player command; the drop a
consumer of the shared DropGems detour, placed through the loader; one
both-route detour on PotionUse that never hands a piece to the base's own use;
the transfer and the zone-state reset by SDK name, the reset only on the boss
room and before the travel; no portal, no stack hook, no boss spawn; no hook
before the mod is turned on or a piece is seen; the research probe stays
research-only.
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
PANEL_SRC = FORGEPACT / "src" / "forgepact.py"
SDK_ROOMS = FORGEPACT.parent / "hs-game-sdk" / "cpp" / "include" / "hs_game_sdk" / "rooms.hpp"

SECTION_START = "// ===== Boss soul pieces (`soulpiece`, ForgePact #197)"
SECTION_END = "// ===== Boss soul pieces research probe (`soulprobe`, ForgePact #197)"

# The act bosses a pentagram brings out, as their SDK names: the mod creates none.
BOSS_OBJECTS = ("Gurag_obj", "Reaper_obj", "Anubis_obj", "Damien_obj", "Karp_King_obj", "Satan_obj", "Mevius_obj",
                "Odin_Phase_1_obj", "Cthulhu_obj", "Enemy_Child_Boss_obj")


def hook_one_script_calls(code):
    """Every `HookOneScript(...)` call statement in `code`, up to its `;`,
    without the function's own definition or declaration."""
    calls = [code[m.start():code.index(";", m.start())] for m in re.finditer(r"\bHookOneScript\(", code)]
    return [c for c in calls if not c.startswith("HookOneScript(const char*")]


def calls_naming(code, pattern):
    """The HookOneScript calls in `code` whose text matches `pattern`."""
    return [c for c in hook_one_script_calls(code) if re.search(pattern, c)]


def gate_precedes(body, gate, action):
    """True when `gate` appears in `body` and before every `action`."""
    if gate not in body or action not in body:
        return False
    return body.index(gate) < body.index(action)


class SoulPieceContractTests(unittest.TestCase):

    @classmethod
    def setUpClass(cls):
        cls.plugin_code = PLUGIN_SRC.read_text(encoding="utf-8").replace("\r\n", "\n")
        cls.code = strip_comments(cls.plugin_code)
        cls.player = strip_comments(strip_research_blocks(cls.plugin_code))
        cls.core = strip_comments(CORE_SRC.read_text(encoding="utf-8").replace("\r\n", "\n"))
        cls.allowlist = re.search(r"kPlayerCommands\s*=\s*\{(?P<body>.*?)\};", cls.plugin_code, re.S).group("body")
        start = cls.plugin_code.index(SECTION_START)
        end = cls.plugin_code.index(SECTION_END, start)
        cls.section_raw = cls.plugin_code[start:end]
        cls.section = strip_comments(cls.section_raw)
        cls.shipped = strip_comments(strip_research_blocks(cls.section_raw))

    def body(self, signature, code=None):
        return strip_comments(function_body(code if code is not None else self.plugin_code, signature))

    # ---- the switch ------------------------------------------------------------------------
    def test_mod_is_off_by_default(self):
        state = self.core[self.core.index("struct State {"):]
        state = state[:state.index("};")]
        self.assertIn("bool on = false;", state)
        self.assertIn("bool forced = false;", state)
        runtime = self.section[self.section.index("struct SoulPieceRuntime {"):]
        runtime = runtime[:runtime.index("};")]
        self.assertIn("SoulCore::State core;", runtime, "the adapter's state is the core's, off")
        self.assertIn("bool wanted = false;", runtime)
        self.assertNotIn("core.on = true", self.section, "only the verb turns it on")
        if PANEL_SRC.exists():
            self.assertIn('"mod_boss_soul_pieces": False,', PANEL_SRC.read_text(encoding="utf-8"))

    def test_soulpiece_is_a_player_command(self):
        self.assertIn('"soulpiece"', self.allowlist)
        self.assertIn('if (lc == "soulpiece") { SoulPieceCommand(rest); return; }', self.player)
        command = self.body("static void SoulPieceCommand(")
        self.assertIn("SoulCore::ParseCommand(TrimCopy(rest))", command)
        self.assertIn("SoulCore::Apply(g_SoulPiece.core, c)", command)
        self.assertIn("Out(SoulPieceStatus());", command, "every form answers its status line")
        self.assertIn("soulpiece: usage -> soulpiece 1|0|on|off|status|force on|force off", command)

    def test_zero_turns_it_off(self):
        parse = self.body("inline Command ParseCommand(", self.core)
        self.assertIn('if (words[0] == "0" || words[0] == "off") return Command::Off;', parse)
        self.assertIn('if (words[0] == "1" || words[0] == "on") return Command::On;', parse)
        apply = self.body("inline bool Apply(", self.core)
        self.assertIn("case Command::Off: s.on = false; return true;", apply)
        # Off leaves the hooks in place but every consumer reads the switch.
        self.assertIn("if (!g_SoulPiece.core.on) return;", self.body("static void SoulPieceGemRoll("))
        self.assertIn("SoulCore::DecideUse(g_SoulPiece.core.on,", self.body("static RValue& SoulPieceUseCall("))

    # ---- the drop ----------------------------------------------------------------------------
    def test_pieces_are_placed_through_the_loader_not_loot_ground_create(self):
        self.assertIn("kSoulPieceFromJsonFull = HeroSiege::Scripts::gml_Script_InitItemFromJson.data();", self.section)
        self.assertIn("kSoulPieceLootFromItemFull = HeroSiege::Scripts::gml_Script_LootGroundCreateFromItem.data();", self.section)
        place = self.body("static bool SoulPiecePlace(")
        loader = place.index("CallGameScriptEx(item, kSoulPieceFromJsonFull, g, g,")
        ground = place.index("CallGameScriptEx(res, kSoulPieceLootFromItemFull, source, source,")
        self.assertLess(loader, ground)
        self.assertIn("SoulCore::SeedOfAct(act)", place, "a piece's seed is the core's")
        self.assertIn("g_SoulPiece.baseIndex", place, "the base resolved by name")
        self.assertIn('\\"o\\":1}', place, "each piece is one item, count 1")
        self.assertIsNone(re.search(r"LootGroundCreate(?!FromItem)", self.section),
                          "LootGroundCreate writes a fresh seed; the mod never calls it")
        roll = self.body("static void SoulPieceGemRoll(")
        self.assertLess(roll.index("SoulCore::RecordDrop("), roll.index("SoulPiecePlace(room.act, S, why)"))
        self.assertIn("if (v != SoulCore::DropVerdict::Hit) return;", roll, "a piece only on the core's `hit`")
        self.assertIn("SoulCore::RecordDropFailed(g_SoulPiece.core);", roll, "a hit that placed nothing says so")

    def test_drop_consumer_rides_the_shared_gem_drop_detour(self):
        hook = function_body(self.player, "static RValue& Hook_DropGems(")
        original = hook.index("g_Orig_DropGems(S, O, R, argc, A)")
        consumer = hook.index("SoulPieceGemRoll(S);")
        self.assertLess(original, consumer, "the consumer runs after the game's own DropGems returned")
        # One DropGems detour in the whole plugin, installed by the gem mod's own installer.
        self.assertEqual(1, self.code.count("SdkShortScriptName(HeroSiege::Scripts::gml_Script_DropGems)"))
        self.assertEqual([], calls_naming(self.section, r"DropGems"))
        tick = self.body("static void SoulPieceTick(")
        self.assertIn("GemsInstallDropHook()", tick)
        self.assertTrue(gate_precedes(tick, "HhResolveLocalPlayer(player)", "GemsInstallDropHook()"),
                        "the drop hook waits for a player (§ 15)")
        self.assertIn("if (g_SoulPiece.wanted && !g_GemDropHookTried) GemsInstallDropHook();", tick,
                      "and only once the mod was turned on")
        roll = self.body("static void SoulPieceGemRoll(")
        self.assertIn("SoulCore::DecideDrop(g_SoulPiece.core.on, g_SoulPiece.core.forced, room.name, room.act,", roll)
        self.assertIn("HeroSiege::Objects::GameObject::Enemy_Parent_obj", roll, "a monster by the SDK's parent table")
        self.assertIn("RarInstanceIsBoss(self)", roll)

    # ---- the use ------------------------------------------------------------------------------
    def test_use_consumer_is_one_potion_use_detour(self):
        self.assertIn("kSoulPieceUseName = SdkShortScriptName(HeroSiege::Scripts::gml_Script_PotionUse);", self.section)
        install = self.body("static void SoulPieceInstallUse()")
        self.assertIn('HookOneScript(kSoulPieceUseName, "fp_soulpiece_use", (PVOID)Hook_SoulPieceUse, &g_SoulPieceOrigUse, &native)',
                      install)
        self.assertIn("if (g_SoulPiece.useTried) return;", install, "installed once")
        self.assertEqual(1, len(hook_one_script_calls(self.section)), "the mod's only detour")
        self.assertNotIn("HookOneScriptTable", self.section)
        self.assertNotIn("MmCreateHook", self.section)
        hook = self.body("static RValue& Hook_SoulPieceUse(", self.player)
        self.assertIn("return SoulPieceUseCall(S, O, R, argc, A);", hook)

    def test_piece_use_never_reaches_the_base_items_own_use(self):
        use = self.body("static RValue& SoulPieceUseCall(")
        passthrough = "if (!act) return orig ? orig(S, O, R, argc, A) : R;"
        self.assertIn(passthrough, use, "a call with no piece goes to the game untouched")
        after = use[use.index(passthrough) + len(passthrough):]
        self.assertNotIn("orig(", after, "the base's own PotionUse never runs for a piece")
        self.assertIn("R = RValue(answer);", after)
        self.assertLess(after.index("g_SoulPiece.pending = true;"), after.index("answer = true;"),
                        "true only once the transfer is recorded")
        self.assertEqual(1, after.count("answer = true;"))
        self.assertIn("v == SoulCore::UseVerdict::Transfer", after)
        self.assertIn("HhResolveLocalPlayer(player)", after, "validated with a player")
        arg = self.body("static int SoulPieceActOfArg(")
        self.assertIn("SoulCore::PieceActOfItem(", arg, "a piece is the core's to recognise")

    # ---- the transfer -----------------------------------------------------------------------------
    def test_transfer_is_network_room_goto_by_sdk_name(self):
        self.assertIn("kSoulPieceTravelFull = HeroSiege::Scripts::gml_Script_NetworkRoomGoto.data();", self.section)
        carry = self.body("static void SoulPieceCarryOut()")
        self.assertIn("CallGameScriptEx(res, kSoulPieceTravelFull, self, self, { RValue(index) })", carry)
        self.assertIn("if (HhResolveLocalPlayer(player)) self = HhResolveInstance(player);", carry, "the player is self")
        self.assertEqual(2, self.section.count("kSoulPieceTravelFull"), "its definition and one call")
        self.assertIsNone(re.search(r'"room_goto\w*"', self.section), "never the raw room_goto builtin")
        self.assertNotIn("gml_Script_RoomGoto", self.section)
        # Carried out on the frame tick, never inside the detour.
        self.assertNotIn("kSoulPieceTravelFull", self.body("static RValue& SoulPieceUseCall("))
        self.assertIn("if (g_SoulPiece.pending) SoulPieceCarryOut();", self.body("static void SoulPieceTick("))

    def test_boss_room_state_is_reset_by_sdk_name_before_the_transfer(self):
        self.assertIn("kSoulPieceZoneResetFull = HeroSiege::Scripts::gml_Script_ZoneStateResetSingle.data();", self.section)
        self.assertIn("kSoulPieceZoneExistsFull = HeroSiege::Scripts::gml_Script_ZoneStateExists.data();", self.section)
        carry = self.body("static void SoulPieceCarryOut()")
        reset = carry.index("CallGameScriptEx(res, kSoulPieceZoneResetFull, self, self, { asset })")
        travel = carry.index("kSoulPieceTravelFull")
        self.assertLess(reset, travel, "the reset comes before the travel")
        # The reset's argument is the boss room: the core's TransferTarget,
        # resolved by name, never the room the player is in.
        self.assertIn("const char* target = SoulCore::TransferTarget(act);", carry)
        self.assertIn("SoulPieceRoomByName(target, asset, index, why)", carry)
        self.assertEqual(1, len(re.findall(r"\basset\s*=", carry + self.body("static bool SoulPieceRoomByName("))),
                         "the argument is assigned in one place, from asset_get_index")
        # Asked before and after the reset, through the one helper.
        exists = [m.start() for m in re.finditer(r"SoulPieceZoneExists\(asset, self,", carry)]
        self.assertEqual(2, len(exists))
        self.assertLess(exists[0], reset)
        self.assertLess(reset, exists[1])
        self.assertLess(exists[1], travel)
        # Only on the transfer path: nowhere else in the plugin.
        self.assertEqual(2, self.section.count("kSoulPieceZoneResetFull"), "its definition and one call")
        self.assertNotIn("gml_Script_ZoneStateResetSingle", self.code.replace(self.section, ""))
        self.assertNotIn("ZoneStateResetAll", self.section)
        # A failed zone-state call is counted, and the travel still goes out.
        self.assertGreaterEqual(carry.count("SoulCore::RecordZoneCallFailed(g_SoulPiece.core);"), 3)
        self.assertIn("SoulCore::RecordTransfer(g_SoulPiece.core, act, travel == \"yes\", before, after);", carry)
        # A use inside a boss room is refused by the core, so the boss room is never the current one.
        self.assertIn("if (IsBossRoom(room)) return UseVerdict::BossRoom;", self.core)

    def test_no_portal_object_ships(self):
        # By whole name: `Spawn_Summon_Portal_obj` (a summon's spawn table) is another object.
        for name in ("Portal_obj", "portalZone", "PortalX", "Portal_Parent_obj", "kPortalOffsetPx", "PortalTarget"):
            pattern = r"\b%s\b" % name
            self.assertIsNone(re.search(pattern, self.player), name)
            self.assertIsNone(re.search(pattern, self.core), name)
        self.assertIsNone(re.search(r'"instance_create\w*"', self.shipped))
        # Negative control: the research build still carries the probe's portal,
        # so the strip above is not removing everything.
        self.assertRegex(self.code, r"\bPortal_obj\b")
        self.assertRegex(self.code, r"\bportalZone\b")

    def test_target_room_is_the_act_boss_room_by_sdk_name(self):
        by_name = self.body("static bool SoulPieceRoomByName(")
        self.assertIn('"asset_get_index", { RValue(name) }', by_name)
        self.assertIn('"room_exists"', by_name)
        self.assertIn("SoulCore::TransferTarget(act)", self.body("static RValue& SoulPieceUseCall("))
        self.assertIn("return boss ? boss->bossRoom : nullptr;", self.body("inline const char* TransferTarget(", self.core))
        self.assertIsNone(re.search(r'"Act_\d', self.section), "every room name is the core's")
        rooms = re.findall(r'"(Act_0\d_Boss)", true \}', self.core)
        self.assertEqual(["Act_0%d_Boss" % n for n in range(1, 10)], rooms)
        if SDK_ROOMS.exists():
            sdk = SDK_ROOMS.read_text(encoding="utf-8")
            for room in rooms:
                self.assertRegex(sdk, r"\b%s = \d+," % room, room)

    def test_gem_rate_is_read_by_repository_name(self):
        rate = self.body("static bool SoulPieceGemRate(")
        self.assertIn("RepoIndexValid(SoulCore::kGemCategory, i)", rate)
        self.assertIn("RepoAd(st) == SoulCore::kGemRepoName", rate)
        self.assertIn('CmMember(dr, "base", base)', rate)
        self.assertIsNone(re.search(r"gemIndex\s*=\s*\d", rate), "never an index constant")
        self.assertIn("g_SoulPieceGemStale = true;", self.body("static void DropRateCmd("),
                      "a droprate command makes it read again")

    def test_one_detour_per_script(self):
        # PotionUse: the mod's one detour, in either build (the probe's watch rides it).
        self.assertEqual(1, len(calls_naming(self.code, r"kSoulPieceUseName|gml_Script_PotionUse\b|\"PotionUse\"")))
        self.assertIn("if (i == kSpPotionUseSlot)", self.body("static void SpInstallUseWatch()"))
        # DropGems: the gem mod's, shared.
        self.assertEqual(1, len(calls_naming(self.code, r"gml_Script_DropGems\b|\"DropGems\"")))
        # The scripts the transfer calls are detoured by nothing of the player
        # build but density's ZoneStateResetSingle, which the call passes through.
        for name in ("ZoneStateExists", "NetworkRoomGoto", "RoomActIndex", "InitItemFromJson"):
            self.assertEqual([], calls_naming(self.player, r'\b%s\b|"%s"' % (name, name)), name)
        self.assertEqual(1, len(calls_naming(self.player, r'"ZoneStateResetSingle"')), "density's, and no second")
        # Negative control for the scan: a second install is found.
        doubled = ('HookOneScript(kSoulPieceUseName, "a", (PVOID)f, &o, &n);\n'
                   'HookOneScript("PotionUse", "b", (PVOID)f, &o);')
        self.assertEqual(2, len(calls_naming(doubled, r"kSoulPieceUseName|gml_Script_PotionUse\b|\"PotionUse\"")))

    def test_base_resolves_by_repository_name(self):
        dress = self.body("static void SoulPieceRegisterDress()")
        self.assertIn("RepoIndexValid(SoulCore::kBaseCategory, i)", dress)
        self.assertIn("RepoAd(st) == SoulCore::kBaseRepoName", dress)
        self.assertIsNone(re.search(r"baseIndex\s*=\s*\d", dress), "never an index constant")
        self.assertLess(dress.index('"variable_global_exists", { RValue("itemRepoNormal") }'), dress.index("RepoStruct("),
                        "the repository is walked only once it exists")
        self.assertIn('piece.selector = { {"t", t}, {"a", seed}, {"b", b}, {"c", 0.0}, {"j", 0.0} };', dress)
        self.assertIn("(double)SoulCore::SeedOfAct(act)", dress)
        self.assertIn("piece.keepNative = true;", dress, "the base's own stats are kept")
        self.assertIn("piece.builtin = true;", dress)
        self.assertIn("piece.mechanic = SoulCore::kMechanicTag;", dress)
        self.assertIn("RebuildForgeSelectorIndex();", dress)
        forge = self.body("static bool TryApplyCustomForge(")
        self.assertIn("if (entry.mechanic == ForgePact::SoulPieces::kMechanicTag) g_SoulPieceSeen = true;", forge)
        # Registered whatever the switch says, before the mod's own gate.
        tick = self.body("static void SoulPieceTick(")
        self.assertTrue(gate_precedes(tick, "SoulPieceRegisterDress();", "if (!g_SoulPiece.wanted && !g_SoulPieceSeen) return;"))

    # ---- the icon ----------------------------------------------------------------------------
    def test_piece_icon_is_the_gem_sprite_resolved_by_name(self):
        self.assertIn('inline constexpr char kPieceSpriteName[] = "Gem_Incarnation_spr";', self.core)
        self.assertIn('inline constexpr char kIconInfoKey[] = "11";', self.core)
        entry = self.code[self.code.index("struct CustomForgeEntry"):]
        entry = entry[:entry.index("};")]
        self.assertIn("std::string sprite;", entry, "an entry names its sprite, never an index")
        dress = self.body("static void SoulPieceRegisterDress()")
        self.assertIn("piece.sprite = SoulCore::kPieceSpriteName;", dress, "every piece entry names the gem's sprite")
        resolve = self.body("static bool ForgeDisplaySprite(")
        self.assertIn('"asset_get_index", { RValue(name) }', resolve)
        # Resolved to a sprite of exactly that name, or nothing.
        self.assertIn('"sprite_exists", { spr }', resolve)
        self.assertIn('"sprite_get_name", { spr }', resolve)
        self.assertIn("!= name) return false;", resolve)
        forge = self.body("static bool TryApplyCustomForge(")
        self.assertIn("ForgeDisplaySprite(entry.sprite, sprite)", forge)
        self.assertIn('{ info, RValue("11"), sprite }', forge, "the resolved value is what is written")
        # Never a numeric sprite index, in the dress, the forge pass or the resolver.
        number = r'RValue\("11"\),\s*RValue\(\(?\w*\)?\s*\d|sprite\s*=\s*-?\d|asset_get_index"\s*,\s*\{\s*RValue\(\d'
        for body in (dress, forge, resolve):
            self.assertIsNone(re.search(number, body))
        # Negative control for the scan: a numeric index is found.
        self.assertIsNotNone(re.search(number, 'g_Yytk->CallBuiltin("variable_struct_set", { info, RValue("11"), RValue(4512) });'))
        self.assertIsNotNone(re.search(number, "piece.sprite = 4512;"))

    def test_piece_icon_is_written_before_the_hash_refresh(self):
        forge = self.body("static bool TryApplyCustomForge(")
        write = '{ info, RValue("11"), sprite }'
        self.assertTrue(gate_precedes(forge, write, "RefreshItemHash(*candidate, &how)"),
                        "the hash covers the info struct: the icon is written before it is refreshed")
        # Only on an entry that names one, and only once it resolved.
        self.assertTrue(gate_precedes(forge, "if (!entry.sprite.empty()) {", write))
        self.assertTrue(gate_precedes(forge, "if (ForgeDisplaySprite(entry.sprite, sprite))", write))
        self.assertIn("!entry.sprite.empty())", forge[:forge.index("if (!entry.sprite.empty()) {")],
                      "the info struct is fetched for an entry with only a sprite")
        # A name that does not resolve writes nothing and says so, capped.
        miss = forge[forge.index("if (ForgeDisplaySprite(entry.sprite, sprite))"):]
        miss = miss[:miss.index("RefreshItemHash(")]
        self.assertIn("InterlockedIncrement(&g_ForgeSpriteMisses) == 1", miss)
        self.assertIn("Out(", miss)
        # Negative control for the order scan: a write after the refresh is caught.
        self.assertFalse(gate_precedes("RefreshItemHash(*candidate, &how);\n" + write, write,
                                       "RefreshItemHash(*candidate, &how)"))

    def test_no_new_hook_installs_while_off_without_a_piece(self):
        command = self.body("static void SoulPieceCommand(")
        for install in ("HookOneScript", "SoulPieceInstallUse", "GemsInstallDropHook"):
            self.assertNotIn(install, command, "the verb only arms the mod: " + install)
        tick = self.body("static void SoulPieceTick(")
        gate = "if (!g_SoulPiece.wanted && !g_SoulPieceSeen) return;"
        self.assertTrue(gate_precedes(tick, gate, "SoulPieceInstallUse();"))
        self.assertTrue(gate_precedes(tick, gate, "GemsInstallDropHook()"))
        self.assertTrue(gate_precedes(tick, "HhResolveLocalPlayer(player)", "SoulPieceInstallUse();"),
                        "never at character select")
        # SoulPieceInstallUse is called from the tick alone in the player build.
        self.assertEqual(2, len(re.findall(r"\bSoulPieceInstallUse\(\)", self.player)), "its definition and the tick")
        self.assertIn("if (g_Setup) SoulPieceTick(fc);", self.body("void FrameCallback(", self.player))
        # Negative control for the gate scan: an install ahead of the gate is caught.
        self.assertFalse(gate_precedes("SoulPieceInstallUse();\n" + gate, gate, "SoulPieceInstallUse();"))

    def test_no_stack_or_spawn_route_ships(self):
        for name in ("ItemsAreStackable", "InventoryGridAddToStack", "GridAddToStack", "AddToInventory", "gml_Script_sCP",
                     "instance_destroy", "instance_copy"):
            self.assertNotIn(name, self.section, name)
        for boss in BOSS_OBJECTS:
            self.assertNotIn(boss, self.section, boss)
        self.assertIsNone(re.search(r'"instance_create\w*"', self.section))
        self.assertIsNone(re.search(r"\bk\w*Rva\w*\b|0x[0-9A-Fa-f]{6,}", self.section), "no address")
        self.assertNotIn("Hero_Siege.exe+", self.section_raw, "no module offset, even in a comment")
        # Every game script the mod calls is an SDK constant.
        self.assertNotIn('"gml_Script_', self.section)
        for full in re.findall(r"static constexpr const char\* kSoulPiece\w+Full = ([^;]+);", self.section):
            self.assertRegex(full, r"^HeroSiege::Scripts::gml_Script_\w+\.data\(\)$")
        calls = re.findall(r"CallGameScript(?:Ex)?\(", self.section)
        named = (re.findall(r"CallGameScript\(kSoulPiece\w+Full,", self.section)
                 + re.findall(r"CallGameScriptEx\(\w+, kSoulPiece\w+Full,", self.section))
        self.assertTrue(calls)
        self.assertEqual(len(calls), len(named))

    def test_soulprobe_is_research_only(self):
        self.assertNotIn("soulprobe", self.allowlist)
        for name in ("soulprobe", "SoulProbe", "SpUseCall", "kSpUseTargets", "SpOpenPortal", "SpNs::"):
            self.assertNotIn(name, self.player, name)
        self.assertIsNone(re.search(r"\bg_Sp\b|\bg_Sp[A-Z]", self.player))
        # The shipped section reaches the probe only through its research block.
        self.assertNotIn("SoulProbe", self.shipped)
        self.assertIn("SoulProbePotionUse", self.section, "the research build's watch rides the shipped detour")


if __name__ == "__main__":
    unittest.main()
