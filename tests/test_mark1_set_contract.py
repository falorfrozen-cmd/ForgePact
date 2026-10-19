"""Mark 1 set (ForgePact #166, phase 2): the plugin wiring the harness cannot see.

tests/test_mark1_set_behavior.py runs the set's decision core
(plugin/include/ForgePact/Mark1Set.hpp). This file reads ModuleMain.cpp as
text, comments stripped, and pins the adapter around it:

- `mark1set` is a player command with a standalone early return of its own in
  RunCommand (never one more `else if`: MSVC C1061); its forms, its replies and
  its stat line are the core's;
- every game name goes through hs-game-sdk (HeroSiege::Scripts) and runtime
  lookups by name; no address, `Rva` constant or module-base arithmetic;
- the four pieces are Custom Forge built-ins, recognition only (`builtin =
  true`), tried after every loaded entry and after the flight charms, so a
  sidecar entry for the same item still wins; ForgeCouldMatch lets their seeds
  through; dressing writes the name, Heroic rarity, the gold rows and
  `fp_mechanic` = `mark1`, removes the native stats, writes no tier and no lore,
  and installs nothing inside CreateItemNew;
- the bonus arms like jumpscenery since 2.2.1 (the guide's Known Limitations
  item 8): the tick returns at once while the switch is off, and nothing is
  hooked until the local player resolves, so nothing installs at character
  select;
- the full-set read happens at the point of use, inside the CheckTalentUse
  detour, through GetOnlinePlayerItemOwner and GetItemFromFingerprint, as
  MinerHelmetMod.hpp's ReadWorn; the detour is HookOneScript's and is refused
  unless its inline detour went in, and refused while a research probe holds
  the script, naming the holder; the research probes name `mark1set` for a
  row Mark 1 holds;
- `mark1probe spawn` sits inside the research block;
- art_fallback: the pieces' own art takes the flight charms' route, and with no
  icon embedded, or one that failed to load, nothing is written, loaded or
  overridden, so each piece keeps its base item's own bag and ground sprites;
- the art rule (Mark1Set.hpp's, applied here): the art is armed by the
  session's first switch-on; until then nothing is loaded or written; a
  dressing writes "11" only where ArtWritesIcon allows it (bag icon and ground
  copy both loaded, the shared detour's route `not-installed` or `both`); the
  ground consumer goes in once, armed, from the tick whatever the switch now
  says, never at character select; the stat line's art, ground and
  ground-route fields come from the adapter's own state.
"""
import re
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from test_release_hook_contract import function_body, strip_comments, strip_research_blocks  # noqa: E402

ROOT = Path(__file__).resolve().parents[1]
PLUGIN = ROOT / "plugin" / "ModuleMain.cpp"
SDK_SCRIPTS = ROOT.parent / "hs-game-sdk" / "cpp" / "include" / "hs_game_sdk" / "scripts.hpp"

FORGE_READS_START = "// ---- Mark 1 set (Mark1Set.hpp): what the Custom Forge reads"
FORGE_READS_END = "// ---- end of what the Custom Forge reads for the Mark 1 set"
ADAPTER_START = "// ---- Mark 1 set (Mark1Set.hpp): the adapter"
ADAPTER_END = "// ---- end of the Mark 1 set adapter"
PROBE_START = "// ---- mark1probe: the Mark 1 set's phase 1 instrument (ForgePact #166)"
PROBE_END = "#endif // FORGEPACT_RELEASE (mark1probe)"

SIGNATURES = {
    "decode": "static bool Mark1PieceDecode(const RValue& item, const RValue& definition, M1sNs::Piece& piece)",
    "of": "static bool Mark1PieceOf(const RValue& item, M1sNs::Piece& piece)",
    "entry": "static bool Mark1ForgeEntry(const RValue& item, const RValue& definition, CustomForgeEntry& entry, M1sNs::Piece& piece)",
    "final": "static void Mark1DressFinal(RValue& item, M1sNs::Piece piece)",
    "icon_index": "static size_t Mark1IconIndex(M1sNs::Piece piece, bool ground)",
    "icon_path": "static bool Mark1IconPath(size_t k, std::string& gmlPath, std::string& absolutePath)",
    "icons_load": "static void Mark1IconsLoad()",
    "ground": "static void Mark1OnGroundInit(int argc, RValue** A)",
    "ground_install": "static void Mark1GroundIconInstall()",
    "ground_route": "static std::string Mark1GroundRoute()",
    "art_install_if_ready": "static void Mark1ArtInstallIfReady()",
    "read_worn": "static bool Mark1ReadWorn(M1sNs::WornSlots& slots)",
    "detour": "static RValue& Mark1HookCheckTalentUse(CInstance* S, CInstance* O, RValue& R, int argc, RValue** A)",
    "install": "static std::string Mark1SetInstall()",
    "install_if_ready": "static void Mark1InstallIfReady(bool fromTick)",
    "tick": "static void Mark1SetTick()",
    "stat_fields": "static M1sNs::StatFields Mark1SetStatFields()",
    "command": "static void Mark1SetCommand(const std::string& rest)",
    "forge": "static bool TryApplyCustomForge(",
    "could_match": "static bool ForgeCouldMatch(",
    "hidden_init": "static RValue& HookHiddenLootInit(CInstance* S, CInstance* O, RValue& R, int argc, RValue** A)",
    "run": "static void RunCommand(const std::string& line)",
    "frame": "void FrameCallback(FWFrame& FrameContext)",
    # research build only
    "holder": "static std::string Mark1SetHolder(std::string_view runtimeName)",
    "research_holder": "static std::string Mark1SetResearchHolder()",
    "sp_holder": "static std::string SpHolder(std::string_view name, volatile long*& calls)",
    "tg_attach": "static void TgProbeAttach(TgProbeTarget& t)",
    "spawn": "static void M1pSpawn(const M1pNs::Command& c)",
    "m1p_holder": "static std::string M1pScriptHolder(std::string_view runtimeName)",
}

SHIPPED = ("decode", "of", "entry", "final", "icon_index", "icon_path", "icons_load", "ground", "ground_install",
           "ground_route", "art_install_if_ready", "read_worn", "detour", "install", "install_if_ready", "tick", "stat_fields", "command")


class Mark1SetContract(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.plugin = PLUGIN.read_text(encoding="utf-8").replace("\r\n", "\n")
        cls.reads = cls.plugin[cls.plugin.index(FORGE_READS_START):cls.plugin.index(FORGE_READS_END)]
        cls.adapter = cls.plugin[cls.plugin.index(ADAPTER_START):cls.plugin.index(ADAPTER_END)]
        cls.code = strip_comments(cls.reads + cls.adapter)
        cls.shipped = strip_comments(strip_research_blocks(cls.plugin))
        cls.probe = cls.plugin[cls.plugin.index(PROBE_START):cls.plugin.index(PROBE_END)]

    def body(self, which, source=None):
        return strip_comments(function_body(source if source is not None else self.plugin, SIGNATURES[which]))

    def lines(self, which):
        return [l.strip() for l in self.body(which).split("\n") if l.strip()]

    # ---- both builds ----------------------------------------------------------------

    def test_the_adapter_ships_in_both_builds(self):
        player = strip_research_blocks(self.plugin)
        for name in SHIPPED:
            with self.subTest(function=name):
                self.assertIn(SIGNATURES[name], player)
        for header in ("#include <ForgePact/Mark1Set.hpp>", "#include <ForgePact/Mark1Icons.hpp>"):
            self.assertIn(header, self.shipped)
        self.assertIn("static ForgePact::Mark1Set::Bonus g_Mark1;", self.shipped)
        # The research probe's own words stay out of the player build even in
        # the adapter's comments (test_mark1_probe_contract.py's rule).
        self.assertNotIn("mark1probe", player)
        self.assertNotIn("M1p", player)

    # ---- the command --------------------------------------------------------------

    def test_mark1set_is_a_player_command_with_a_standalone_early_return(self):
        run = function_body(self.plugin, SIGNATURES["run"])
        commands = run[run.index("kPlayerCommands = {"):]
        commands = commands[:commands.index("};")]
        self.assertIn('"flightcharm"', commands)   # the set is the one read, not an empty slice
        self.assertIn('"mark1set"', commands)
        self.assertIn('    if (lc == "mark1set") { Mark1SetCommand(rest); return; }\n', run)
        self.assertEqual(self.plugin.count('lc == "mark1set"'), 1)
        self.assertNotIn('else if (lc == "mark1set"', self.plugin)
        # It ships: the early return is not inside a research guard.
        self.assertIn('if (lc == "mark1set") { Mark1SetCommand(rest); return; }', strip_research_blocks(run))

    def test_the_forms_replies_and_stat_line_are_the_cores(self):
        command = self.body("command")
        self.assertIn("M1sNs::ParseCommand(args, c, why)", command)
        self.assertIn("M1sNs::BonusReply(g_Mark1)", command)
        self.assertIn("M1sNs::StatLine(Mark1SetStatFields())", command)
        self.assertIn("g_Mark1.SetEnabled(c.on);", command)
        # `stat` reads the set afresh, but only with a character to read.
        self.assertIn("g_Mark1.ReadNow(", command)
        self.assertLess(command.index("Mark1PlayerResolves()"), command.index("g_Mark1.ReadNow("))
        fields = self.body("stat_fields")
        self.assertIn("g_Mark1.FillStat(f);", fields)
        self.assertIn("f.iconsEmbedded = (int)ForgePact::Mark1Icons::kIconCount;", fields)
        self.assertIn("f.iconsLoaded = ", fields)
        # The art group: `art=` from FillStat (Bonus::ArtArmed), the ground
        # count and the shared detour's route from the adapter's own state.
        self.assertIn("f.ground = g_M1sGround;", fields)
        self.assertIn("f.groundRoute = Mark1GroundRoute();", fields)
        self.assertLess(fields.index("g_Mark1.FillStat(f);"), fields.index("f.groundRoute = "))
        # The command itself never installs: the one install site is Mark1SetInstall.
        for install in ("HookOneScript", "HookBuiltin", "MmCreateHook"):
            self.assertNotIn(install, command)

    # ---- game names ----------------------------------------------------------------

    def test_game_names_go_through_the_sdk(self):
        scripts = SDK_SCRIPTS.read_text(encoding="utf-8")
        for constant in ("gml_Script_CheckTalentUse", "gml_Script_GetOnlinePlayerItemOwner",
                         "gml_Script_GetItemFromFingerprint"):
            with self.subTest(constant=constant):
                self.assertIn(f'std::string_view {constant} = "{constant}";', scripts)
                self.assertIn("HeroSiege::Scripts::" + constant, self.code)
                self.assertNotIn('"' + constant + '"', self.code)
                self.assertNotIn('"' + constant[len("gml_Script_"):] + '"', self.code)
        self.assertNotIn('"gml_Script_', self.code)
        install = self.body("install")
        self.assertIn("HookOneScript(SdkShortScriptName(HeroSiege::Scripts::gml_Script_CheckTalentUse)", install)
        self.assertNotIn("HookOneScriptTable", self.code)

    def test_no_rva_hex_address_or_module_base_arithmetic(self):
        self.assertIsNone(re.search(r"\b\w*Rva\w*\b", self.code))
        self.assertIsNone(re.search(r"\b0x[0-9A-Fa-f]{5,}\b", self.code))
        self.assertIsNone(re.search(r"\(\s*char\s*\*\s*\)\s*\w+\s*[+-]", self.code))
        self.assertNotIn("GetModuleHandle", self.code)
        self.assertNotIn("reinterpret_cast<PFUNC", self.code)

    # ---- the forge entries -----------------------------------------------------------

    def test_the_forge_entries_are_recognition_only_built_ins(self):
        entry = self.body("entry")
        self.assertIn("Mark1PieceDecode(item, definition, piece)", entry)
        self.assertIn("M1sNs::RowOf(piece)", entry)
        for field in ("entry.builtin = true;", "entry.keepNative = false;", "entry.rarity = M1sNs::kRarity;",
                      "entry.name = std::string(row.name);", "entry.mechanic = std::string(M1sNs::kMechanic);"):
            with self.subTest(field=field):
                self.assertIn(field, entry)
        # The gold rows: the core's, joined by newlines (at most three rows).
        self.assertIn("M1sNs::kAffixRows", entry)
        self.assertIn('"\\n"', entry)
        # No stats, no tier and no lore written.
        for absent in ("entry.tier", "entry.lore", "entry.stats", "entry.sprite"):
            self.assertNotIn(absent, entry)
        # The selector is the item's own, as the flight charms'.
        for key in ('{"t", t}', '{"a", a}', '{"b", b}', '{"c", c}', '{"j", j}'):
            self.assertIn(key, entry)
        decode = self.body("decode")
        self.assertIn("M1sNs::IsPiece(t, a, b, c, j, piece)", decode)
        self.assertLess(decode.index('"itemType"'), decode.index('"a"'))

    def test_the_pieces_are_tried_after_every_loaded_entry_and_the_flight_charms(self):
        forge = self.body("forge")
        self.assertEqual(forge.count("Mark1ForgeEntry("), 1)
        self.assertIn("const bool mark1Matched = i == entryCount + 1;", forge)
        self.assertIn("if (mark1Matched && !Mark1ForgeEntry(*candidate, definition, mark1Entry, mark1Piece)) break;", forge)
        self.assertLess(forge.index("FlightCharmForgeEntry("), forge.index("Mark1ForgeEntry("))
        self.assertIn("if (!flightMatched && !mark1Matched && !CustomForgeMatches(entry, *candidate, definition)) continue;", forge)
        # Its own final pass (the bag icon), before the hash refresh.
        call = "if (finalPass && mark1Matched) Mark1DressFinal(*candidate, mark1Piece);"
        self.assertIn(call, forge)
        self.assertLess(forge.index(call), forge.index("RefreshItemHash(*candidate, &how)"))
        # A piece is dressed whatever the switch says: no switch read here.
        self.assertNotIn("g_Mark1", forge)
        could = self.body("could_match")
        self.assertIn("M1sNs::IsPiece(", could)
        self.assertLess(could.index("M1sNs::IsPiece("), could.index("g_ForgeSelectorKeys.count("))

    def test_dressing_installs_nothing_inside_create_item_new(self):
        for function in ("forge", "entry", "decode", "final", "icons_load", "icon_path", "icon_index"):
            text = self.body(function)
            for install in ("HookOneScript", "HookBuiltin", "HiddenLootInstall(", "FlightGroundIconInstall(", "MmCreateHook"):
                with self.subTest(function=function, install=install):
                    self.assertNotIn(install, text)

    # ---- arming, install, holders -------------------------------------------------------

    def test_the_tick_returns_at_once_while_not_armed(self):
        # Off and no art install pending: back at once, before any game call.
        # The pending check is the core's, on the adapter's own state only.
        lines = self.lines("tick")
        self.assertEqual(lines[0], "const bool artPending = M1sNs::ArtGroundInstallPending(g_Mark1.ArtArmed(), "
                                   "(int)ForgePact::Mark1Icons::kIconCount, g_M1sGroundTried);")
        self.assertEqual(lines[1], "if (!g_Mark1.Enabled() && !artPending) return;")
        # The art's one ground install runs whatever the switch now says, on
        # the polled frames; the set's own part still needs the switch.
        self.assertEqual(lines[2], "if (artPending && M1sNs::Bonus::LooksForPlayer(g_M1sArtFrames++)) Mark1ArtInstallIfReady();")
        self.assertEqual(lines[3], "if (!g_Mark1.Enabled()) return;")
        frame = strip_comments(function_body(self.plugin, SIGNATURES["frame"]))
        self.assertIn("if (g_Setup) Mark1SetTick();", frame)
        self.assertEqual(frame.count("Mark1SetTick()"), 1)
        tick = self.body("tick")
        self.assertIn("M1sNs::Bonus::LooksForPlayer(g_M1sArmedFrames++)", tick)
        self.assertIn("Mark1InstallIfReady(true)", tick)

    def test_no_install_at_character_select(self):
        ready = self.body("install_if_ready")
        # The core's gate: setup done and the local player resolved, before
        # the one install call.
        self.assertIn("g_Mark1.ShouldInstall(g_Setup, true, fromTick) && Mark1PlayerResolves()", ready)
        self.assertIn("if (!g_Mark1.ShouldInstall(g_Setup, havePlayer, fromTick)) return;", ready)
        self.assertLess(ready.index("if (!g_Mark1.ShouldInstall(g_Setup, havePlayer, fromTick)) return;"),
                        ready.index("Mark1SetInstall()"))
        self.assertIn("g_Mark1.NoteInstall(why.empty(), fromTick);", ready)
        resolves = strip_comments(function_body(self.plugin, "static bool Mark1PlayerResolves()"))
        self.assertIn("HhResolveLocalPlayer(p)", resolves)
        self.assertIn("HhResolveInstance(p)", resolves)
        # The one install site in the adapter, reached only through the gate.
        self.assertEqual(len(re.findall(r"\bHookOneScript\(", self.code)), 1)
        self.assertEqual(len(re.findall(r"\bMark1SetInstall\(\)", self.code)), 2)   # definition and the gate's call
        command = self.body("command")
        self.assertIn("Mark1InstallIfReady(false);", command)

    def test_check_talent_use_is_refused_unless_its_inline_detour_went_in(self):
        install = self.body("install")
        self.assertIn("&g_M1sOrigCheckTalentUse, &g_M1sCtuNative)", install)
        self.assertIn("if (!g_M1sCtuNative)", install)
        self.assertIn("TABLE-ONLY", install)
        self.assertLess(install.index("HookOneScript("), install.index("if (!g_M1sCtuNative)"))
        # The ground icon goes in with it, through the flight charms' install.
        self.assertIn("Mark1GroundIconInstall();", install)

    def test_check_talent_use_is_refused_while_a_research_probe_holds_it(self):
        raw = function_body(self.plugin, SIGNATURES["install"])
        research = raw[raw.index("#ifndef FORGEPACT_RELEASE"):raw.index("#endif")]
        self.assertIn("Mark1SetResearchHolder()", research)
        self.assertLess(raw.index("Mark1SetResearchHolder()"), raw.index("HookOneScript("))
        holder = self.body("research_holder")
        # skillprobe and every holder it knows (tgprobe among them), jumpprobe,
        # and the research probe of this set.
        for check in ("JpHolder(", "g_JpRows", "M1pScriptHolder("):
            self.assertIn(check, holder)
        self.assertNotIn("Mark1SetResearchHolder", self.shipped)
        self.assertNotIn("Mark1SetHolder", self.shipped)

    def test_the_research_probes_name_mark1set_for_a_row_it_holds(self):
        holder = self.body("holder")
        self.assertIn("g_M1sOrigCheckTalentUse", holder)
        self.assertIn('"mark1set"', holder)
        self.assertIn("HeroSiege::Scripts::gml_Script_CheckTalentUse", holder)
        # skillprobe, jumpprobe (JpHolder) and the set's own probe (M1pHeldBy,
        # through JpHolder) all ask SpHolder; tgprobe asks before it blocks.
        self.assertIn("Mark1SetHolder(name)", self.body("sp_holder"))
        attach = self.body("tg_attach")
        self.assertIn("Mark1SetHolder(runtimeName)", attach)
        self.assertLess(attach.index("Mark1SetHolder(runtimeName)"),
                        attach.index('"blocked: table entry is not code inside Hero_Siege.exe"'))
        m1p = strip_comments(function_body(self.plugin, "static bool M1pHeldBy(const M1pRow& t, std::string& holder)"))
        self.assertIn("JpHolder(", m1p)
        own = self.body("m1p_holder")
        self.assertIn("t.orig", own)
        self.assertIn('"mark1probe hook"', own)

    # ---- the full-set read at the point of use -------------------------------------------

    def test_the_detour_reads_the_set_at_the_point_of_use_for_the_local_player(self):
        detour = self.body("detour")
        first = self.lines("detour")[0]
        self.assertEqual(first, "if (!g_Mark1.Enabled() || g_M1sBusy || !S || S != g_M1sPlayer) "
                                "return g_M1sOrigCheckTalentUse(S, O, R, argc, A);")
        self.assertIn("g_Mark1.Active(", detour)
        self.assertIn("Mark1ReadWorn", detour)
        self.assertIn("g_RuntimeFrame", detour)
        self.assertLess(detour.index("g_Mark1.Active("), detour.rindex("g_M1sOrigCheckTalentUse(S, O, R, argc, A)"))
        # Nothing is written to the game here yet (the automation item fills it).
        for write in ("variable_instance_set", "variable_struct_set", "array_set", "variable_global_set"):
            self.assertNotIn(write, detour)

    def test_the_worn_read_resolves_fingerprints_like_the_miners_helmet(self):
        read = self.body("read_worn")
        self.assertIn('"equippedItems"', read)
        self.assertIn('"mplr"', read)
        self.assertIn("HeroSiege::Scripts::gml_Script_GetOnlinePlayerItemOwner", read)
        self.assertIn("HeroSiege::Scripts::gml_Script_GetItemFromFingerprint", read)
        self.assertLess(read.index("gml_Script_GetOnlinePlayerItemOwner"), read.index("gml_Script_GetItemFromFingerprint"))
        self.assertIn("HhResolveLocalPlayer(", read)
        # A fingerprint that is not a string never reaches the game's resolver.
        self.assertIn("fingerprint.m_Kind != VALUE_STRING", read)
        self.assertLess(read.index("fingerprint.m_Kind != VALUE_STRING"), read.index("gml_Script_GetItemFromFingerprint"))
        for state in ("M1sNs::SlotRead::Empty", "M1sNs::SlotRead::Item", "M1sNs::SlotRead::Unreadable"):
            self.assertIn(state, read)
        # Nothing written.
        for write in ("variable_instance_set", "variable_struct_set", "array_set", "variable_global_set"):
            self.assertNotIn(write, read)

    # ---- the research probe's spawn ----------------------------------------------------

    def test_spawn_is_research_only_inside_the_probe_block(self):
        self.assertIn(SIGNATURES["spawn"], self.probe)
        self.assertNotIn("M1pSpawn", self.shipped)
        spawn = self.body("spawn")
        self.assertIn("HeroSiege::Scripts::gml_Script_InitItemFromJson", spawn)
        self.assertIn("HeroSiege::Scripts::gml_Script_LootGroundCreateFromItem", spawn)
        self.assertLess(spawn.index('"json_parse"'), spawn.index("gml_Script_InitItemFromJson"))
        self.assertLess(spawn.index("gml_Script_InitItemFromJson"), spawn.index("gml_Script_LootGroundCreateFromItem"))
        self.assertIn("M1pNs::SpawnLine(r)", spawn)
        self.assertIn("M1pNs::PieceJson(", spawn)
        # Its ground icon goes in the way Mark 1's install puts it in.
        self.assertIn("Mark1GroundIconInstall();", spawn)
        self.assertLess(spawn.index("Mark1GroundIconInstall();"), spawn.index("gml_Script_LootGroundCreateFromItem"))
        command = strip_comments(function_body(self.plugin, "static void M1pCommand(const std::string& rest)"))
        self.assertIn("case M1pNs::Form::Spawn: M1pSpawn(c); return;", command)

    # ---- the pieces' own art: art_fallback ------------------------------------------------

    def test_art_fallback_bag_icon_written_only_when_that_pieces_icon_loaded(self):
        final = self.body("final")
        self.assertIn("if (g_TruthDepth != 0) return;", final)
        self.assertIn("Mark1IconsLoad();", final)
        self.assertIn('RValue("11"), g_M1sIconSprite[k]', final)
        # The core's rule decides the write: armed, this piece's bag icon and
        # its ground copy both loaded, and the shared detour's route allowing it.
        rule = "if (M1sNs::ArtWritesIcon(g_Mark1.ArtArmed(), g_M1sIconOk[k], groundLoaded, Mark1GroundRoute()))"
        self.assertIn(rule, final)
        self.assertLess(final.index(rule), final.index('RValue("11")'))
        self.assertIn("const size_t g = Mark1IconIndex(piece, true);", final)
        self.assertIn("const bool groundLoaded = g < ForgePact::Mark1Icons::kIconCount && g_M1sIconOk[g];", final)
        self.assertIn("if (k >= ForgePact::Mark1Icons::kIconCount) return;", final)
        self.assertLess(final.index("if (k >= ForgePact::Mark1Icons::kIconCount) return;"), final.index('RValue("11")'))
        # "11" is written nowhere else in the adapter.
        self.assertEqual(self.code.count('RValue("11"), g_M1sIconSprite'), 1)

    def test_art_fallback_ground_sprite_only_for_our_bag_sprite_and_a_loaded_ground_copy(self):
        ground = self.body("ground")
        self.assertEqual(self.lines("ground")[0], "if (!g_M1sGroundReady) return;")
        self.assertIn("HeroSiege::Player::kGroundItemInstanceField", ground)
        self.assertIn("Mark1PieceOf(item, piece)", ground)
        self.assertIn("if (!g_M1sIconOk[bag] || !g_M1sIconOk[k]) return;", ground)
        self.assertIn("if (shown.ToDouble() != g_M1sIconSprite[bag].ToDouble()) return;", ground)
        write = ground.index('RValue("sprite_index"), g_M1sIconSprite[k]')
        for gate in ("Mark1PieceOf(item, piece)", "if (!g_M1sIconOk[bag] || !g_M1sIconOk[k]) return;",
                     "if (shown.ToDouble() != g_M1sIconSprite[bag].ToDouble()) return;"):
            self.assertLess(ground.index(gate), write)
        self.assertEqual(self.code.count('"sprite_index"'), 1)

    def test_art_fallback_png_written_only_when_no_file_of_that_name_is_there(self):
        path = self.body("icon_path")
        self.assertIn('IPC_DIR + "\\\\mark1"', path)
        self.assertIn("if (!std::filesystem::exists(file, ec))", path)
        self.assertLess(path.index("if (!std::filesystem::exists(file, ec))"), path.index("std::ofstream"))
        self.assertIn('std::string("bp_ipc\\\\mark1\\\\") + icon.file', path)
        self.assertIn("if (k >= ForgePact::Mark1Icons::kIconCount) return false;", path)

    def test_art_fallback_sprite_add_once_a_session(self):
        load = self.lines("icons_load")
        self.assertEqual(load[0], "if (g_M1sIconsTried) return;")
        self.assertEqual(load[1], "g_M1sIconsTried = true;")
        body = self.body("icons_load")
        self.assertIn('"sprite_add"', body)
        self.assertEqual(self.code.count('"sprite_add"'), 1)
        self.assertIn("for (size_t k = 0; k < ForgePact::Mark1Icons::kIconCount; ++k)", body)
        # A failed load prints one line naming the files, as the flight charms do.
        self.assertIn("Out(", body)
        self.assertIn("g_M1sGroundReady = true;", body)
        # Per-icon state is sized by the most there can be: kIconCount may be 0.
        self.assertIn("g_M1sIconSprite[ForgePact::Mark1Icons::kMaxIcons]", self.code)
        self.assertIn("g_M1sIconOk[ForgePact::Mark1Icons::kMaxIcons]", self.code)

    def test_art_fallback_ground_consumer_rides_the_one_shared_detour_after_the_flight_charms(self):
        hook = strip_comments(function_body(self.plugin, SIGNATURES["hidden_init"]))
        self.assertEqual(hook.count("Mark1OnGroundInit(argc, A);"), 1)
        self.assertLess(hook.index("g_Orig_LootGroundInit(S, O, R, argc, A)"), hook.index("Mark1OnGroundInit(argc, A);"))
        self.assertLess(hook.index("FlightCharmOnGroundInit(argc, A);"), hook.index("Mark1OnGroundInit(argc, A);"))
        self.assertLess(hook.index("Mark1OnGroundInit(argc, A);"), hook.index("TrackerOnInit(S, argc, A);"))
        # Never a second LootGroundInit hook: it goes in through the flight
        # charms' install, and only when an icon is embedded.
        calls = re.findall(r"HookOneScript\w*\([^;]*LootGroundInit\b", strip_comments(self.plugin))
        self.assertEqual(len(calls), 1, calls)
        install = self.body("ground_install")
        self.assertIn("FlightGroundIconInstall();", install)
        self.assertIn("if (ForgePact::Mark1Icons::kIconCount == 0) return;", install)
        self.assertLess(install.index("if (ForgePact::Mark1Icons::kIconCount == 0) return;"),
                        install.index("FlightGroundIconInstall();"))
        self.assertNotIn("LootGroundInit", self.code)

    def test_art_fallback_nothing_embedded_means_nothing_loaded(self):
        final = self.body("final")
        self.assertIn("if (ForgePact::Mark1Icons::kIconCount == 0) return;", final)
        self.assertLess(final.index("if (ForgePact::Mark1Icons::kIconCount == 0) return;"), final.index("Mark1IconsLoad();"))
        index = self.body("icon_index")
        self.assertIn("return ForgePact::Mark1Icons::kIconCount;", index)

    # ---- the art rule (Mark1Set.hpp), applied --------------------------------------------

    def test_art_fallback_not_armed_means_nothing_loaded_or_written(self):
        # sprite_add happens at the first armed dressing, never before.
        final = self.body("final")
        self.assertIn("if (!g_Mark1.ArtArmed()) return;", final)
        self.assertLess(final.index("if (!g_Mark1.ArtArmed()) return;"), final.index("Mark1IconsLoad();"))
        # Mark1IconsLoad is reached from the armed dressing alone.
        self.assertEqual(len(re.findall(r"\bMark1IconsLoad\(\);", self.code)), 1)

    def test_art_fallback_ground_install_once_and_only_while_armed(self):
        install = self.body("ground_install")
        pending = ("if (!M1sNs::ArtGroundInstallPending(g_Mark1.ArtArmed(), (int)ForgePact::Mark1Icons::kIconCount, "
                   "g_M1sGroundTried)) return;")
        self.assertIn(pending, install)
        self.assertIn("g_M1sGroundTried = true;", install)
        self.assertLess(install.index(pending), install.index("g_M1sGroundTried = true;"))
        self.assertLess(install.index("g_M1sGroundTried = true;"), install.index("FlightGroundIconInstall();"))
        # Its callers: Mark 1's own install, the tick's art install and the
        # research spawn; never a dressing (test_dressing_installs_nothing...).
        self.assertEqual(len(re.findall(r"\bMark1GroundIconInstall\(\);", strip_comments(self.plugin))), 3)

    def test_art_fallback_the_ticks_ground_install_waits_for_a_character(self):
        ready = self.body("art_install_if_ready")
        gate = ("const bool player = M1sNs::ArtGroundInstallPending(g_Mark1.ArtArmed(), "
                "(int)ForgePact::Mark1Icons::kIconCount, g_M1sGroundTried) && g_Setup && Mark1PlayerResolves();")
        self.assertIn(gate, ready)
        want = ("if (!M1sNs::ArtWantsGroundInstall(g_Mark1.ArtArmed(), (int)ForgePact::Mark1Icons::kIconCount, "
                "g_M1sGroundTried, g_Setup, player)) return;")
        self.assertIn(want, ready)
        self.assertLess(ready.index(want), ready.index("Mark1GroundIconInstall();"))
        # The switch is not asked: once armed, the consumer is wanted whatever it says.
        self.assertNotIn("Enabled()", ready)

    def test_art_fallback_ground_route_is_the_shared_detours_with_its_install_state(self):
        route = self.body("ground_route")
        self.assertIn("M1sNs::GroundRouteName(g_HiddenLootInstallTried, ForgePact::HiddenLootMod::Instance().RouteName())",
                      route)
        # RouteName() is never used bare in the adapter: it reads `none` both
        # before the install and after a refused one.
        self.assertEqual(self.code.count("RouteName()"), 1)


if __name__ == "__main__":
    unittest.main()
