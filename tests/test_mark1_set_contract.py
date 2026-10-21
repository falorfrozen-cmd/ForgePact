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
  through; dressing writes the name, Satanic rarity, SS tier (the core's
  `kTier`) and `fp_mechanic` = `mark1`, removes the native stats, writes no
  affix rows and no lore, and installs nothing inside CreateItemNew;
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
- the automation (Mark1Automation.hpp, applied here): the one game write is
  the press, made only inside the CheckTalentUse detour, for the local
  player, into the `inputState` of the profile slot the gate's third argument
  names, read back as written and put back after the original in every path
  (this replaces Round 0's "the detour writes nothing" pin); the bar and the
  toggle state are read by name at the point of use;
- failure_counts: every column counter reaches its ColumnCounts field, the
  four failures by their own names; `gate-calls`, `gate-other` and `threw`
  come from the detour's own counts and CallsThrew();
- border_line: Hook_DrawHudBuffs holds one `Mark1BorderDraw();` line directly
  after `ToggleIndicatorDraw();`, outside every research block, and the
  border draws only while the bonus is active;
- `mark1probe spawn` sits inside the research block;
- art_fallback: the pieces' own art takes the flight charms' route, and with no
  icon embedded, or one that failed to load, nothing is written, loaded or
  overridden, so each piece keeps its base item's own bag and ground sprites;
- the art rule (Mark1Set.hpp's, applied here): the art is armed by the
  session's first piece dressed outside Item Truth's builds (the owner's
  playtest, 2026-10-21: pieces placed before `mark1set bonus 1` kept their base
  art), or the drops' switch-on; until then nothing is loaded or written; a
  dressing writes "11" only where ArtWritesIcon allows it (bag icon and ground
  copy both loaded, the shared detour's route `not-installed` or `both`); the
  ground consumer goes in once, armed, from the tick whatever the switch now
  says, never at character select; the stat line's art, ground and
  ground-route fields come from the adapter's own state;
- art_default: a player who holds no piece and never turns the drops on gets
  nothing loaded, written or installed for the art. The armed flag's writers
  are the core's NotePieceDressed (from the dressing, after its Item Truth
  guard and before its armed check, and from the research spawn, before its
  ground install) and, for the drops' switch, NoteDropsSwitch through
  Mark1Drops.hpp's SetSwitch; never SetEnabled. The dressing, the ground
  install and the tick each ask the core's rule before anything else, and no
  dressing inside CreateItemNew's pass installs a hook;
- tooltip: a piece, recognised by Mark1PieceOf where both tooltip passes arm
  their rows (the player's and Item Truth's drawn one), gets the core's set
  block from one worn read (Mark1ReadWorn + Evaluate, never Bonus::ReadNow) per
  armed pass, drawn at the forged rows' anchor in its colours; every other
  forged item keeps its gold rows; the pass's draw_text draws the piece's type
  line as the core's `Satanic Set <type>`, the hook going in with the ground
  consumer's callers, once the art is armed and a character exists, never
  behind the icons' own gate; `tip=` and `tip-unread=` count the blocks drawn
  in the player's own (on-screen) passes, `tip-armed=` those passes armed for a
  piece, `typeline=` / `typeline-miss=` whether each drew its type line, and
  Item Truth's off-screen passes count in none of them;
- the jump (Mark1Jump.hpp, applied here): one skillsLeap detour and one
  install of the five collision builtins in the whole plugin, shared with
  jumpscenery (and the flight charms for the builtins); Mark 1's take-off
  decision runs before the original and its scaling after it, and while the
  bonus takes a jump jumpscenery's entry is skipped, unless jumpscenery's own
  window is already open; jump_objects: the three objects resolved by name by
  whichever install runs first, classified by Mark 1's own classifier;
  learner_feed: M is ChooseMax over jumpscenery's public getters and a clean
  jump reaches jumpscenery's public Learn; the landing reads are Mark 1's own,
  through its own player, and a failed one is a throw or Cell::Unreadable,
  never an answer; grid_read: global.pathGrid and mp_grid_get_cell by name,
  outside every research block; jump_counts: every JumpCounts field from the
  core's counter of the same name, M from the getters.
- the half walk (Mark1Movement.hpp, applied here; `speed-route: stat-model`):
  Mark 1's part of the plugin's one shared ReturnSpecificStat detour
  (ReturnSpecificStatHook.hpp, which skill sliders runs in too), never a second
  HookOneScript of the script; part of Mark 1's all-or-nothing install,
  refused unless the detour is native, and refused while a research probe
  holds the script; the probes name `mark1set` for it. The lever runs only for
  the local player's id-64 calls, asks the bonus at the point of use, and the
  stat line's `speed=` is its count; no cast-penalty lever ships
  (`penalty-route: none`, `penalty=unavailable`).
- the turns (Mark1Automation.hpp's turn rule, applied here): landing: a Mark 1
  detour on TalentUseSetSpeed, HookOneScript's with its own id, part of the
  all-or-nothing install, refused unless its inline detour went in and while
  a research probe holds the script (tgprobe, projprobe, mark1probe), which
  each refuse that row naming `mark1set`; it counts the local player's calls
  with the switch on (`cast-calls=`) and marks a landing only while the gate's
  original runs for a call that pressed a column, fed to the core's Landing
  after the original returns or throws; turn_fields: `landed=`, `skip-turn=`
  and `yielded=` from the core's counters of the same names.
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
HEADER = ROOT / "plugin" / "include" / "ForgePact" / "Mark1Set.hpp"

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
    "tooltip_rows": "static std::vector<TipRow> Mark1TooltipRows(M1sNs::Piece piece)",
    "detour": "static RValue& Mark1HookCheckTalentUse(CInstance* S, CInstance* O, RValue& R, int argc, RValue** A)",
    "landing": "static RValue& Mark1HookTalentUseSetSpeed(CInstance* S, CInstance* O, RValue& R, int argc, RValue** A)",
    "cast_window": "struct Mark1CastWindow {",
    "refresh": "static void Mark1RefreshPlayer()",
    "hud_row": "static bool Mark1HudRow(RValue& row0)",
    "hud_column": "static Mark1HudColumn Mark1ReadHudColumn(const RValue& row0, int column)",
    "read_toggle": "static ForgePact::Mark1ToggleState Mark1ReadToggle(const Mark1HudColumn& c)",
    "input_state": "static bool Mark1InputState(int argc, RValue** A, RValue& out)",
    "input_get": "static bool Mark1InputGet(const RValue& input, int entry, RValue& out)",
    "input_set": "static bool Mark1InputSet(const RValue& input, int entry, const RValue& value)",
    "border": "static void Mark1BorderDraw()",
    "hud": "static RValue& Hook_DrawHudBuffs(CInstance* S, CInstance* O, RValue& R, int argc, RValue** A)",
    "toggle_draw": "static void ToggleIndicatorDraw()",
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
    "landing_research_holder": "static std::string Mark1LandingResearchHolder()",
    "pp_attach": "static void ProjProbeAttach(int idx)",
    "m1p_table_held": "static bool M1pTableHeld(std::string_view runtimeName)",
    "sp_holder": "static std::string SpHolder(std::string_view name, volatile long*& calls)",
    "tg_attach": "static void TgProbeAttach(TgProbeTarget& t)",
    "spawn": "static void M1pSpawn(const M1pNs::Command& c)",
    "m1p_holder": "static std::string M1pScriptHolder(std::string_view runtimeName)",
    "m1p_builtin_holder": "static std::string M1pBuiltinHolder(std::string_view name)",
    "m1p_builtin_held": "static bool M1pBuiltinHeldBy(std::string_view name, std::string& holder)",
    "jump_research_holder": "static std::string Mark1JumpResearchHolder()",
    # the jump (Mark1Jump.hpp), in the adapter
    "jump_position": "static bool Mark1JumpPosition(double& x, double& y)",
    "jump_cursor": "static bool Mark1JumpCursor(double& x, double& y)",
    "jump_family": "static bool Mark1JumpFamilyAt(double x, double y)",
    "jump_gate": "static bool Mark1JumpGateOnLine(double x0, double y0, double x1, double y1)",
    "jump_kind": "static M1jNs::ValueKind Mark1JumpKindOf(const RValue& v)",
    "jump_grid": "static M1jNs::Cell Mark1JumpGridAt(double x, double y)",
    "jump_max": "static M1jNs::Max Mark1JumpMax()",
    "jump_learn": "static void Mark1JumpLearn(const M1jNs::Learned& l)",
    "jump_entry": "static bool Mark1JumpOnLeapEntry(CInstance* S, int64_t frame)",
    "jump_exit": "static void Mark1JumpOnLeapExit(CInstance* S, int64_t frame)",
    "jump_builtin": "static void Mark1JumpOnBuiltin(int row, RValue& Result, CInstance* S, CInstance* O, int argc, RValue* Args)",
    "jump_install": "static std::string Mark1JumpInstall()",
    "jump_tick": "static void Mark1JumpTick()",
    # the half walk (Mark1Movement.hpp), in the adapter
    "walk": "static void Mark1MovementOnStat(CInstance* S, int argc, RValue** A, RValue& r)",
    "walk_install": "static std::string Mark1MovementInstall()",
    "walk_research_holder": "static std::string Mark1MovementResearchHolder()",
    "pp_attach_stat": "static bool ProjProbeAttachStat()",
    # the shared detours and installs, in jumpscenery's adapter
    "leap": "static RValue& JsHookLeap(CInstance* S, CInstance* O, RValue& R, int argc, RValue** A)",
    "leap_install": "static std::string SkillsLeapInstall()",
    "resolve_objects": "static std::string JsResolveObjects()",
    "shared_detour": "static void CollisionBuiltinDetour(int row, RValue& Result, CInstance* S, CInstance* O, int argc, RValue* Args)",
    "js_install": "static std::string JumpSceneryInstall()",
    "js_held": "static std::string JumpSceneryHeldHooks()",
    "flight_install": "static std::string FlightInstall()",
    # the drops (Mark1Drops.hpp), in their own adapter block
    "drops_install": "static std::string Mark1DropInstall()",
    "drops_switch": "static void Mark1DropsSwitch(bool on)",
}

SHIPPED = ("decode", "of", "entry", "final", "icon_index", "icon_path", "icons_load", "ground", "ground_install",
           "ground_route", "art_install_if_ready", "read_worn", "tooltip_rows", "detour", "landing", "cast_window", "install", "install_if_ready", "tick", "stat_fields", "command",
           "refresh", "hud_row", "hud_column", "read_toggle", "input_state", "input_get", "input_set", "border",
           "jump_position", "jump_cursor", "jump_family", "jump_gate", "jump_kind", "jump_grid", "jump_max", "jump_learn",
           "jump_entry", "jump_exit", "jump_builtin", "jump_install", "jump_tick", "leap", "leap_install", "resolve_objects",
           "walk", "walk_install")
JUMP_HEADER = ROOT / "plugin" / "include" / "ForgePact" / "Mark1Jump.hpp"
AUTOMATION_HEADER = ROOT / "plugin" / "include" / "ForgePact" / "Mark1Automation.hpp"
MOVEMENT_HEADER = ROOT / "plugin" / "include" / "ForgePact" / "Mark1Movement.hpp"
SHARED_STAT_HEADER = ROOT / "plugin" / "include" / "ForgePact" / "ReturnSpecificStatHook.hpp"


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
        for header in ("#include <ForgePact/Mark1Set.hpp>", "#include <ForgePact/Mark1Icons.hpp>",
                       "#include <ForgePact/Mark1Automation.hpp>", "#include <ForgePact/Mark1Movement.hpp>"):
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
        # No affix rows: the tooltip's set block says what the set does.
        self.assertNotIn("kAffixRows", self.plugin)
        # No affix, stats or lore written (the tier is the next test's).
        for absent in ("entry.affix", "entry.lore", "entry.stats", "entry.sprite"):
            self.assertNotIn(absent, entry)
        # The selector is the item's own, as the flight charms'.
        for key in ('{"t", t}', '{"a", a}', '{"b", b}', '{"c", c}', '{"j", j}'):
            self.assertIn(key, entry)
        decode = self.body("decode")
        self.assertIn("M1sNs::IsPiece(t, a, b, c, j, piece)", decode)
        self.assertLess(decode.index('"itemType"'), decode.index('"a"'))

    def test_the_forge_entry_sets_the_pieces_tier_from_the_cores_constant(self):
        # Every piece is SS (the owner, 2026-10-20): the entry's tier is the
        # core's constant, never a literal, and the Custom Forge's final pass
        # writes it to itemInfoStruct["32"] beside the rarity's "27" for any
        # entry whose tier is set - the drops' rewrite, `mark1probe spawn` and a
        # piece loaded from a save alike.
        entry = self.body("entry")
        self.assertEqual(entry.count("entry.tier"), 1)
        self.assertIn("entry.tier = M1sNs::kTier;", entry)
        self.assertIn("entry.rarity = M1sNs::kRarity;", entry)
        forge = self.body("forge")
        self.assertIn("if (entry.tier >= 0)", forge)
        self.assertIn('{ info, RValue("32"), RValue((double)entry.tier) }', forge)
        self.assertIn('{ info, RValue("27"), RValue((double)entry.rarity) }', forge)
        # Still no lore, stats or sprite of its own.
        for absent in ("entry.lore", "entry.stats", "entry.sprite"):
            self.assertNotIn(absent, entry)

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
        self.assertEqual(lines[0], "const bool artPending = M1sNs::ArtInstallPending(g_Mark1.ArtArmed(), "
                                   "(int)ForgePact::Mark1Icons::kIconCount, g_M1sGroundTried, g_TipTypeLineTried);")
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
        # The two install sites in the adapter (the gate and the landing
        # detour, both in Mark1SetInstall), reached only through the gate.
        self.assertEqual(len(re.findall(r"\bHookOneScript\(", self.code)), 2)
        self.assertEqual(len(re.findall(r"\bHookOneScript\(", self.body("install"))), 2)
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
        lines = self.lines("detour")
        # Switch off, the adapter's own calls, no self: the original alone,
        # nothing read or counted. Any other self: the original alone, counted.
        self.assertEqual(lines[0], "if (!g_Mark1.Enabled() || g_M1sBusy || !S) "
                                   "return g_M1sOrigCheckTalentUse(S, O, R, argc, A);")
        self.assertEqual(lines[1], "if (S != g_M1sPlayer) { ++g_M1sGateOther; "
                                   "return g_M1sOrigCheckTalentUse(S, O, R, argc, A); }")
        self.assertIn("g_Mark1.Active(", detour)
        self.assertIn("Mark1ReadWorn", detour)
        self.assertIn("g_RuntimeFrame", detour)
        self.assertLess(detour.index("g_Mark1.Active("), detour.rindex("g_M1sOrigCheckTalentUse(S, O, R, argc, A)"))
        # The local player is HhResolveLocalPlayer's (which accepts the
        # VALUE_REF this runner returns), resolved to its instance by the tick.
        refresh = self.body("refresh")
        self.assertIn("HhResolveLocalPlayer(p)", refresh)
        self.assertIn("g_M1sPlayer = inst;", refresh)
        self.assertNotIn("VALUE_OBJECT", refresh)

    # ---- the automation: the press inside the gate ------------------------------------

    def automation_header(self):
        return strip_comments(AUTOMATION_HEADER.read_text(encoding="utf-8").replace("\r\n", "\n"))

    def test_automation_the_press_is_written_only_inside_the_gate_and_restored_in_every_path(self):
        # Replaces Round 0's "the detour writes nothing" pin: the one write is
        # the press (and its restore), reached only from the detour's core call.
        detour = self.body("detour")
        lines = self.lines("detour")
        # Inactive: the original alone, before anything is read for the automation.
        inactive = lines.index("if (!active) return g_M1sOrigCheckTalentUse(S, O, R, argc, A);")
        for later in ("automation.Plan(", "Mark1InputState(", "automation.PressAround<RValue>("):
            with self.subTest(later=later):
                self.assertLess(detour.index(lines[inactive]), detour.index(later))
        # The original runs inside the core's PressAround, which presses the
        # planned columns before it and puts each pressed entry back after it;
        # the landing window is open around the original alone.
        self.assertIn("RValue& result = automation.PressAround<RValue>(plan,", detour)
        self.assertIn("[&]() -> RValue& { Mark1CastWindow window(pressing); "
                      "return g_M1sOrigCheckTalentUse(S, O, R, argc, A); });", detour)
        self.assertIn("haveInput && Mark1InputGet(input, entry, out)", detour)
        self.assertIn("haveInput && Mark1InputSet(input, entry, value)", detour)
        # The detour itself writes nothing to the game.
        for write in ("variable_instance_set", "variable_struct_set", "array_set", "variable_global_set"):
            with self.subTest(write=write):
                self.assertNotIn(write, detour)
        # The write: the profile slot's inputState entry, read back as written.
        put = self.body("input_set")
        self.assertIn('g_Yytk->CallBuiltin("array_set", { input, RValue((double)entry), value });', put)
        self.assertIn("return N1Numeric(back) && back.ToDouble() == value.ToDouble();", put)
        self.assertLess(put.index('"array_set"'), put.index("return N1Numeric(back)"))
        # The one array_set and the one write path in the adapter.
        self.assertEqual(self.code.count('"array_set"'), 1)
        self.assertNotIn("variable_global_set", self.code)
        # The adapter's two variable_instance_set calls: the ground consumer's
        # sprite, and the jump's scaled position, written only after a
        # skillsLeap call's original returned (jump-adapter).
        self.assertEqual(self.code.count('"variable_instance_set"'), 2)
        self.assertIn('"variable_instance_set"', self.body("ground"))
        self.assertIn('"variable_instance_set"', self.body("jump_exit"))
        self.assertEqual(len(re.findall(r"\bMark1InputSet\(", self.code)), 2)   # definition and the detour's write
        self.assertEqual(len(re.findall(r"\bMark1InputGet\(", self.code)), 2)
        # The original value is read before the press and an unreadable one
        # is never pressed; the restore runs after a return and after a throw.
        around = function_body(self.automation_header(), "decltype(auto) PressAround(")
        self.assertIn("if (!readOk) { ++m_Counters[i].originalUnreadable; continue; }", around)
        self.assertLess(around.index("read(entry, originals[i])"), around.index("write(entry, Value(kMark1PressValue))"))
        # The put-back is a guard's destructor, declared before the call, so it
        # runs after a void call, after a value and after a throw alike; a
        # throw is counted there and reaches the adapter untouched (the
        # adapter writes the rethrow line), with no catch that rethrows here.
        guard = around[around.index("struct PutBack {"):]
        self.assertIn("if (std::uncaught_exceptions() > uncaught) ++self.m_CallsThrew;", guard)
        self.assertLess(guard.index("++self.m_CallsThrew;"), guard.index("run();"))
        self.assertLess(around.index("} putBack{ *this, restore };"), around.index("call();"))
        self.assertNotIn("throw;", around)

    def test_automation_the_profile_slot_is_the_one_the_gates_third_argument_names(self):
        state = self.body("input_state")
        self.assertIn("if (argc < 3 || !A || !A[2] || !N1Numeric(*A[2])) return false;", state)
        self.assertIn("const double slot = A[2]->ToDouble();", state)
        self.assertIn("HeroSiege::Objects::GameObject::Input_Device_Manager_obj", state)
        self.assertIn('RValue("playerProfileSlot")', state)
        self.assertIn('RValue("inputState")', state)
        self.assertIn("if (!(slot >= 0 && slot < length && slot == std::floor(slot))) return false;", state)
        # An instance is any handle kind, never a VALUE_OBJECT check alone.
        self.assertIn("HeroSiege::Player::IsInstanceHandle(manager)", state)
        self.assertIn("HeroSiege::Player::IsInstanceHandle(entry)", state)
        # Resolved only when a column is to be pressed, and only for the local player.
        detour = self.body("detour")
        self.assertIn("haveInput = plan.PressCount() > 0 && Mark1InputState(argc, A, input);", detour)
        self.assertLess(detour.index("if (S != g_M1sPlayer)"), detour.index("Mark1InputState("))
        self.assertEqual(len(re.findall(r"\bMark1InputState\(", self.code)), 2)

    def test_automation_the_columns_and_the_toggle_state_are_read_by_name_at_the_point_of_use(self):
        detour = self.body("detour")
        self.assertIn("plan = automation.Plan(true, (long long)g_RuntimeFrame,", detour)
        self.assertIn("if (haveRow) c = Mark1ReadHudColumn(row0, column);", detour)
        self.assertIn("return Mark1ReadToggle(columns[column - ForgePact::kMark1FirstColumn]);", detour)
        row = self.body("hud_row")
        self.assertIn("HeroSiege::Objects::GameObject::UI_Hud_Talent_obj", row)
        self.assertIn('RValue("row0")', row)
        column = self.body("hud_column")
        for name in ('RValue("talentId")', 'RValue("auraSkill")', "ToggleTableRowForTalentId(c.talentId)"):
            self.assertIn(name, column)
        # Until the toggle table's first walk, a set talent is unreadable: a
        # toggle never passes for a skill to press on every call.
        self.assertIn("if (g_ToggleResolveWalks == 0) return c;", column)
        self.assertLess(column.index("if (g_ToggleResolveWalks == 0) return c;"), column.rindex("c.read.readable = true;"))
        self.assertLess(column.index('RValue("auraSkill")'), column.index("if (g_ToggleResolveWalks == 0) return c;"))
        toggle = self.body("read_toggle")
        self.assertIn("ToggleIndicatorReadRow(row, c.talentId, &detail, false);", toggle)
        self.assertIn("ForgePact::ToggleIndicatorModel::Decide(detail, ForgePact::ToggleRowRequiresMark(row))", toggle)
        self.assertIn("return ForgePact::Mark1ToggleState::Unreadable;", toggle)

    def test_automation_failure_counts(self):
        fields = self.body("stat_fields")
        # Every ColumnCounts field the stat line prints is filled, the four
        # failures by their own names, never folded into a skip.
        struct = function_body(self.header(), "struct ColumnCounts {")
        names = re.findall(r"uint64_t (\w+) = 0;", struct)
        self.assertEqual(len(names), 13, names)   # ten since the automation, three since the turns
        for name in names:
            with self.subTest(field=name):
                self.assertEqual(len(re.findall(r"\bc\." + name + r" = \(uint64_t\)k\.\w+;", fields)), 1)
        for line in ("c.unreadable = (uint64_t)k.unreadable;", "c.originalUnreadable = (uint64_t)k.originalUnreadable;",
                     "c.pressWriteFailed = (uint64_t)k.pressWriteFailed;", "c.restoreFailed = (uint64_t)k.restoreFailed;",
                     "c.presses = (uint64_t)k.presses;", "c.skipEmpty = (uint64_t)k.empty;", "c.skipAura = (uint64_t)k.aura;",
                     "c.skipToggleOn = (uint64_t)k.toggleOn;", "c.skipSettle = (uint64_t)k.toggleSettling;",
                     "c.skipNoState = (uint64_t)k.toggleUnreadable;"):
            with self.subTest(line=line):
                self.assertIn(line, fields)
        self.assertIn("automation.Counters(ForgePact::kMark1FirstColumn + i)", fields)
        self.assertIn("for (int i = 0; i < M1sNs::kColumnCount; ++i)", fields)
        self.assertIn("static_assert(M1sNs::kColumnCount == ForgePact::kMark1ColumnCount", self.code)
        self.assertIn("f.gateCalls = g_M1sGateCalls;", fields)
        self.assertIn("f.gateOther = g_M1sGateOther;", fields)
        self.assertIn("f.threw = (uint64_t)automation.CallsThrew();", fields)
        # gate-calls: the local player's entries while active, counted once,
        # after the inactive return; gate-other: the switch on, any other self.
        detour = self.body("detour")
        self.assertEqual(self.code.count("++g_M1sGateCalls;"), 1)
        self.assertLess(detour.index("if (!active) return"), detour.index("++g_M1sGateCalls;"))
        self.assertEqual(self.code.count("++g_M1sGateOther;"), 1)
        self.assertLess(detour.index("if (!g_Mark1.Enabled() || g_M1sBusy || !S)"), detour.index("++g_M1sGateOther;"))
        self.assertLess(detour.index("++g_M1sGateOther;"), detour.index("g_Mark1.Active("))
        # The counters' only writers are those two increments.
        self.assertEqual(re.findall(r"\bg_M1sGate(?:Calls|Other)\s*(?:\+=|=(?!=))", self.code),
                         ["g_M1sGateCalls =", "g_M1sGateOther ="])

    def test_automation_border_line(self):
        raw = function_body(self.plugin, SIGNATURES["hud"])
        lines = raw.split("\n")
        self.assertEqual(lines.count("    Mark1BorderDraw();"), 1, raw)
        at = lines.index("    Mark1BorderDraw();")
        self.assertEqual(lines[at - 1], "    ToggleIndicatorDraw();")
        # Outside every research block: it survives the strip.
        self.assertIn("    ToggleIndicatorDraw();\n    Mark1BorderDraw();\n",
                      function_body(strip_research_blocks(self.plugin), SIGNATURES["hud"]))
        # Not inside the toggle border's own draw, which returns while its switch is off.
        self.assertNotIn("Mark1BorderDraw", function_body(self.plugin, SIGNATURES["toggle_draw"]))
        # Declaration, the one call and the definition.
        self.assertEqual(len(re.findall(r"\bMark1BorderDraw\(\)", strip_comments(self.plugin))), 3)
        # It draws only while the bonus is active: its first statement.
        border = self.body("border")
        self.assertEqual(self.lines("border")[0], "if (!g_Mark1.ActiveCached()) return;")
        self.assertIn("ForgePact::Mark1Automation::Instance().BorderMask(true,", border)
        self.assertIn("ToggleIndicatorMarkerBox(bx, by, bw, bh, x, y, w, h);", border)
        self.assertIn("ToggleIndicatorDrawMarker(x, y, w, h);", border)
        for field in ('"navBboxX"', '"navBboxY"', '"navBboxWidth"', '"navBboxHeight"'):
            self.assertIn(field, border)
        # The colour and alpha are put back after the markers.
        self.assertLess(border.index("ToggleIndicatorDrawMarker("), border.index('"draw_set_alpha", { prevAlpha }'))
        self.assertLess(border.index("ToggleIndicatorDrawMarker("), border.index('"draw_set_colour", { prevColour }'))
        # The core's mask: neither empty nor an aura, nothing while inactive.
        header = self.automation_header()
        self.assertIn("return r.readable && !IsEmpty(r) && !r.aura;", function_body(header, "static bool Bordered("))
        self.assertIn("if (!bonusActive) return 0u;", function_body(header, "unsigned BorderMask("))

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

    def test_the_worn_read_scans_every_gear_slot_not_one_slot_per_piece(self):
        # Mod live 2's `equip-detect`: the worn gloves were not at slot 3, so
        # the read hands the core gear slots 0..9 and the core counts each
        # piece wherever it is.
        read = self.body("read_worn")
        self.assertIn("for (int i = 0; i < M1sNs::kGearSlotCount; ++i)", read)
        self.assertIn("ForgePact::MinerHelmet::Index(worn, i, fingerprint)", read)
        self.assertNotIn(".slot", read)
        self.assertNotIn("kPieceCount", read)
        # player.hpp's names for the read's literals.
        self.assertIn("HeroSiege::Player::kMaxLocalPlayerIndex", read)
        self.assertIn("HeroSiege::Player::kItemInstanceTypeField", read)
        self.assertIn("HeroSiege::Player::kItemInstanceDefinitionField", read)
        self.assertNotIn("index > 4", read)
        self.assertNotIn('"itemType"', read)
        self.assertNotIn('"itemDefinitionStruct"', read)
        # Gear without a numeric `a` or `j` is read, not unreadable.
        self.assertIn('if (!FlightNumber(definition, "a", a)) a = std::nan("");', read)
        self.assertIn('if (!FlightNumber(definition, "j", j)) j = std::nan("");', read)
        header = self.header()
        self.assertIn("inline constexpr int kGearSlotCount = 10;", header)
        self.assertIn("using WornSlots = std::array<WornSlot, kGearSlotCount>;", header)
        self.assertIn('s += " worn=" + WornText(f.set);', header)

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
        # Its callers: Mark 1's own install, the tick's art install, the
        # drops' install (Mark1Drops.hpp: no roll builds a piece before it was
        # tried) and the research spawn; never a dressing
        # (test_dressing_installs_nothing...).
        self.assertEqual(len(re.findall(r"\bMark1GroundIconInstall\(\);", strip_comments(self.plugin))), 4)

    def test_art_fallback_the_ticks_ground_install_waits_for_a_character(self):
        ready = self.body("art_install_if_ready")
        gate = ("const bool player = M1sNs::ArtInstallPending(g_Mark1.ArtArmed(), "
                "(int)ForgePact::Mark1Icons::kIconCount, g_M1sGroundTried, g_TipTypeLineTried) && g_Setup "
                "&& Mark1PlayerResolves();")
        self.assertIn(gate, ready)
        want = ("if (!M1sNs::ArtWantsInstall(g_Mark1.ArtArmed(), (int)ForgePact::Mark1Icons::kIconCount, "
                "g_M1sGroundTried, g_TipTypeLineTried, g_Setup, player)) return;")
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

    # ---- the art rule for a default player: art_default ----------------------------------
    #
    # Round 0's finding: our runtime sprite under "11" was written whatever the
    # switch said, while the only thing that sets a dropped piece's ground
    # sprite rides a detour installed with the switch on, so a default
    # player's dropped piece would draw Error_spr. These pin that the bag icon
    # and the ground consumer now both follow the switch, through the core.

    def header(self):
        return strip_comments(HEADER.read_text(encoding="utf-8").replace("\r\n", "\n"))

    def test_art_default_dressing_writes_11_only_as_the_cores_rule_allows(self):
        lines = self.lines("final")
        # Not armed (the switch never on this session): back before any read,
        # load or write, so the piece keeps its base item's own bag sprite.
        self.assertEqual(lines[:4], ["if (g_TruthDepth != 0) return;",
                                     "g_Mark1.NotePieceDressed();",
                                     "if (ForgePact::Mark1Icons::kIconCount == 0) return;",
                                     "if (!g_Mark1.ArtArmed()) return;"])
        final = self.body("final")
        armed = final.index("if (!g_Mark1.ArtArmed()) return;")
        for later in ("FlightField(", "Mark1IconsLoad();", "g_Yytk->", 'RValue("11")'):
            with self.subTest(later=later):
                self.assertLess(armed, final.index(later))
        # The one "11" write is the statement the core's rule governs, with
        # the armed flag, both copies' load state and the shared detour's route.
        governed = re.search(
            re.escape("if (M1sNs::ArtWritesIcon(g_Mark1.ArtArmed(), g_M1sIconOk[k], groundLoaded, Mark1GroundRoute()))")
            + r"\s*" + re.escape('g_Yytk->CallBuiltin("variable_struct_set", { info, RValue("11"), g_M1sIconSprite[k] });'),
            final)
        self.assertIsNotNone(governed, "the \"11\" write is not governed by ArtWritesIcon")
        self.assertEqual(len(re.findall(r'RValue\("11"\)', self.code)), 1)
        # The rule itself, in the core: armed, both copies loaded, the route allowing it.
        rule = function_body(self.header(), "inline bool ArtWritesIcon(")
        self.assertIn("return armed && bagLoaded && groundLoaded && GroundRouteAllowsArt(groundRoute);", rule)

    def test_art_default_the_armed_flag_has_no_writer_but_a_piece_and_the_drops(self):
        whole = strip_comments(self.plugin)
        # The adapter never touches the flag but through the core's setters.
        self.assertNotIn("artArmed", whole)
        self.assertEqual(re.findall(r"\bg_Mark1\.SetEnabled\([^;]*\);", whole), ["g_Mark1.SetEnabled(c.on);"])
        self.assertIn("g_Mark1.SetEnabled(c.on);", self.body("command"))
        # A piece arms its own art: the dressing, after its Item Truth guard
        # and before its armed check, and the research spawn, before its
        # ground install. Nowhere else; the player build has the dressing's.
        self.assertEqual(len(re.findall(r"\bg_Mark1\.NotePieceDressed\(\);", whole)), 2)
        self.assertEqual(len(re.findall(r"\bg_Mark1\.NotePieceDressed\(\);", self.shipped)), 1)
        final = self.body("final")
        self.assertLess(final.index("if (g_TruthDepth != 0) return;"), final.index("g_Mark1.NotePieceDressed();"))
        self.assertLess(final.index("g_Mark1.NotePieceDressed();"), final.index("if (!g_Mark1.ArtArmed()) return;"))
        spawn = self.body("spawn")
        self.assertLess(spawn.index("g_Mark1.NotePieceDressed();"), spawn.index("Mark1GroundIconInstall();"))
        # The research spawn never turns the switch.
        probe = strip_comments(self.probe)
        self.assertNotIn("SetEnabled(", probe)
        self.assertNotIn("artArmed", probe)
        # In the core, the writes that set it are NotePieceDressed's and the
        # drops' NoteDropsSwitch on a switch-on (drops arm the art, the drops
        # workorder's default); SetEnabled writes no art flag (the owner's
        # playtest, 2026-10-21). The adapter reaches NoteDropsSwitch only
        # through Mark1Drops.hpp's SetSwitch, from the drops' switch.
        header = self.header()
        self.assertEqual(re.findall(r"\bartArmed_\s*=\s*true\b", header), ["artArmed_ = true", "artArmed_ = true"])
        self.assertNotIn("artArmed_", function_body(header, "void SetEnabled(bool on)"))
        self.assertIn("artArmed_ = true;", function_body(header, "void NotePieceDressed()"))
        self.assertIn("if (on) artArmed_ = true;", function_body(header, "void NoteDropsSwitch(bool on)"))
        self.assertEqual(len(re.findall(r"\bartArmed_\s*=(?!=)", header)), 3)   # those two writes and the member's `= false`
        self.assertNotIn("NoteDropsSwitch", whole)
        self.assertEqual(re.findall(r"\bM1dNs::SetSwitch\([^;]*\);", whole), ["M1dNs::SetSwitch(g_M1Drop.core, g_Mark1, on);"])
        self.assertIn("M1dNs::SetSwitch(g_M1Drop.core, g_Mark1, on);", self.body("drops_switch"))

    def test_art_default_ground_install_returns_unless_armed_before_the_flight_install(self):
        self.assertEqual(self.lines("ground_install"), [
            "if (M1sNs::TypeLineInstallPending(g_Mark1.ArtArmed(), g_TipTypeLineTried)) InstallTipTypeLineHook();",
            "if (ForgePact::Mark1Icons::kIconCount == 0) return;",
            "if (!M1sNs::ArtGroundInstallPending(g_Mark1.ArtArmed(), (int)ForgePact::Mark1Icons::kIconCount, "
            "g_M1sGroundTried)) return;",
            "g_M1sGroundTried = true;",
            "FlightGroundIconInstall();",
        ])
        pending = function_body(self.header(), "inline bool ArtGroundInstallPending(")
        self.assertIn("return armed && iconsEmbedded > 0 && !tried;", pending)
        # So the research spawn's call installs nothing for an unarmed session.
        self.assertIn("Mark1GroundIconInstall();", self.body("spawn"))

    def test_art_default_the_tick_installs_through_the_cores_decision_whatever_the_switch(self):
        lines = self.lines("tick")
        # Not armed and the switch off: back at once, and nothing before that
        # return reaches the game.
        self.assertEqual(lines[1], "if (!g_Mark1.Enabled() && !artPending) return;")
        for line in lines[:2]:
            for call in ("g_Yytk", "CallBuiltin", "CallGameScript", "Resolve", "Mark1PlayerResolves",
                         "InstallIfReady(", "IconInstall(", "SetInstall(", "Hook"):
                with self.subTest(line=line, call=call):
                    self.assertNotIn(call, line)
        self.assertIn("ArtInstallPending(g_Mark1.ArtArmed(),", lines[0])
        # The art's install comes before the switch's own return, so it runs
        # whatever the switch now says.
        tick = self.body("tick")
        self.assertLess(tick.index("Mark1ArtInstallIfReady();"), tick.index("if (!g_Mark1.Enabled()) return;"))
        self.assertEqual(self.lines("art_install_if_ready"), [
            "const bool player = M1sNs::ArtInstallPending(g_Mark1.ArtArmed(), (int)ForgePact::Mark1Icons::kIconCount, "
            "g_M1sGroundTried, g_TipTypeLineTried) && g_Setup && Mark1PlayerResolves();",
            "if (!M1sNs::ArtWantsInstall(g_Mark1.ArtArmed(), (int)ForgePact::Mark1Icons::kIconCount, "
            "g_M1sGroundTried, g_TipTypeLineTried, g_Setup, player)) return;",
            "Mark1GroundIconInstall();",
        ])
        # The core's decision takes no switch: armed, embedded, untried (the
        # ground consumer and the type line each), setup, player.
        self.assertIn("inline bool ArtWantsInstall(bool armed, int iconsEmbedded, bool groundTried, bool typeLineTried, "
                      "bool setupDone, bool playerResolved)", self.header())

    def test_art_default_nothing_installs_inside_create_item_new_pass(self):
        whole = strip_comments(self.plugin)
        # The pass reaches the art through the one Mark1DressFinal call, in
        # the Custom Forge's final pass.
        self.assertEqual(len(re.findall(r"\bMark1DressFinal\(", whole)), 3)   # declaration, call, definition
        self.assertIn("if (finalPass && mark1Matched) Mark1DressFinal(*candidate, mark1Piece);", self.body("forge"))
        installs = ("Mark1GroundIconInstall(", "Mark1ArtInstallIfReady(", "Mark1InstallIfReady(", "Mark1SetInstall(",
                    "FlightGroundIconInstall(", "HiddenLootInstall(", "HookOneScript", "HookBuiltin", "MmCreateHook")
        for function in ("final", "icons_load", "icon_path", "icon_index", "ground_route", "of", "decode", "entry"):
            for install in installs:
                with self.subTest(function=function, install=install):
                    self.assertNotIn(install, self.body(function))
        # The ground install's four callers, none of them a dressing.
        callers = {which: self.body(which).count("Mark1GroundIconInstall();")
                   for which in ("install", "art_install_if_ready", "spawn", "drops_install")}
        self.assertEqual(callers, {"install": 1, "art_install_if_ready": 1, "spawn": 1, "drops_install": 1})
        self.assertEqual(len(re.findall(r"\bMark1GroundIconInstall\(\);", whole)), 4)

    def test_art_default_stat_icons_fields_come_from_the_flag_the_ground_count_and_the_route(self):
        fields = self.body("stat_fields")
        # `art=` from the core's flag, through FillStat; never set by the adapter.
        self.assertIn("g_Mark1.FillStat(f);", fields)
        self.assertNotIn("f.artArmed", self.code)
        self.assertIn("f.artArmed = artArmed_;", function_body(self.header(), "void FillStat(StatFields& f) const"))
        # `ground=` is the consumer's own count, bumped only after it gave a
        # ground item our sprite.
        self.assertIn("f.ground = g_M1sGround;", fields)
        self.assertEqual(re.findall(r"\+\+g_M1sGround\b|\bg_M1sGround\s*(?:\+=|=(?!=))", self.code),
                         ["g_M1sGround =", "++g_M1sGround"])   # the declaration's `= 0` and the consumer's count
        ground = self.body("ground")
        self.assertLess(ground.index('RValue("sprite_index"), g_M1sIconSprite[k]'), ground.index("++g_M1sGround;"))
        # `ground-route=` is hidden loot's RouteName(), with its install state.
        self.assertIn("f.groundRoute = Mark1GroundRoute();", fields)
        self.assertIn("ForgePact::HiddenLootMod::Instance().RouteName()", self.body("ground_route"))
        self.assertLess(fields.index("g_Mark1.FillStat(f);"), fields.index("f.ground = g_M1sGround;"))

    # ---- the set block in a piece's tooltip (the owner's playtest, 2026-10-21) ----------

    def test_tooltip_both_arming_sites_build_the_block_for_a_piece_by_mark1_piece_of(self):
        # The player's tooltip and Item Truth's drawn one: a piece, by its own
        # fields (class, seed, `j` 0; never fp_mechanic), gets the core's
        # block, and anything else its gold rows from ForgedTooltipRows, before
        # the rows are armed.
        for name, item in (("static RValue& Hook_DrawInventoryItemV2(", "*A[3]"), ("static bool TipDrawOne(", "item")):
            with self.subTest(site=name):
                body = strip_comments(function_body(self.shipped, name))
                piece = f"if (Mark1PieceOf({item}, tipPiece)) g_TipRows = Mark1TooltipRows(tipPiece);"
                gold = f"else g_TipRows = TipGoldRows(ForgedTooltipRows({item}));"
                self.assertIn(piece, body)
                self.assertIn(gold, body)
                self.assertLess(body.index(piece), body.index(gold))
                self.assertLess(body.index(gold),
                                body.index("g_TipRowsPending = ForgePact::ForgedTooltipRows::Pending(g_TipRows.size());"))
                # The pass's block state starts clear and is cleared when it ends.
                self.assertGreaterEqual(body.count("TipSetBlockClear();"), 2)
                self.assertNotIn('"fp_mechanic"', body)

    def test_tooltip_the_worn_read_is_once_per_armed_pass(self):
        rows = self.body("tooltip_rows")
        self.assertEqual(rows.count("Mark1ReadWorn("), 1)
        self.assertEqual(rows.count("M1sNs::Evaluate("), 1)
        self.assertNotIn("ReadNow(", rows)   # `reads=` and `set=` keep their meaning
        self.assertNotIn("Active(", rows)
        self.assertIn("M1sNs::SetBlockRows(read, readOk, g_Mark1.Enabled())", rows)
        self.assertIn("const M1sNs::SetRead read = readOk ? M1sNs::Evaluate(slots) : M1sNs::SetRead{};", rows)
        self.assertIn("g_TipSetBlockUnread = M1sNs::BlockUnread(read, readOk);", rows)
        # Mark 1's own game calls run under its busy flag, put back after.
        self.assertLess(rows.index("g_M1sBusy = true;"), rows.index("Mark1ReadWorn("))
        self.assertLess(rows.index("Mark1ReadWorn("), rows.index("g_M1sBusy = busy;"))
        # Its two callers are the two arming sites, and nothing else reads the
        # set for a tooltip.
        self.assertEqual(len(re.findall(r"\bMark1TooltipRows\(tipPiece\)", self.shipped)), 2)
        for install in ("HookOneScript", "HookBuiltin", "MmCreateHook", "Install"):
            self.assertNotIn(install, rows)

    def test_tooltip_the_block_draws_at_the_anchor_in_its_colours_and_other_rows_stay_gold(self):
        stat = function_body(self.shipped, "static RValue& Hook_DrawInventoryStatsNew(")
        self.assertIn("for (const TipRow& row : g_TipRows) extra += TipDrawRow(x, y + extra, row);", stat)
        draw = function_body(self.shipped, "static double TipDrawRow(double x, double y, const TipRow& row)")
        # A forged item's gold row is HhDrawTooltipLine's, unchanged.
        self.assertIn("if (!row.wrapped && row.colour == kTipGold) {", draw)
        self.assertIn("HhDrawTooltipLine(x, y, row.text);", draw)
        self.assertIn("static constexpr double kTipGold = 6472946.0;", self.shipped)
        gold = function_body(self.shipped, "static std::vector<TipRow> TipGoldRows(")
        self.assertIn("TipRow{ text, kTipGold, false }", gold)
        # The description wraps with the game's builtins and adds its measured height.
        self.assertIn('"draw_text_ext"', draw)
        self.assertIn('"string_height_ext"', draw)
        self.assertIn("RValue(kTipWrapSeparation), RValue(kTipWrapWidth)", draw)
        self.assertIn("static const double kTipWrapWidth = 622.0;", self.shipped)
        self.assertIn("static const double kTipWrapSeparation = -1.0;", self.shipped)
        self.assertIn('{ RValue(row.colour) }', draw)

    def test_tooltip_the_type_line_is_the_cores_through_the_pass_draw_text(self):
        line = function_body(self.shipped, "static bool TipTypeLineDraw(")
        self.assertIn("M1sNs::TypeLineFor(true, piece, drawn)", line)
        self.assertIn("if (line == drawn) return false;", line)
        self.assertLess(line.index("if (line == drawn) return false;"), line.index("args[2] = RValue(line);"))
        # The pass ends the piece's type line where it ends its rows.
        for name in ("static RValue& Hook_DrawInventoryItemV2(", "static bool TipDrawOne("):
            body = function_body(self.shipped, name)
            self.assertGreater(body.rindex("TipSetBlockClear();"), body.index("g_Orig_DrawInventoryItemV2("), name)
        self.assertIn("g_TipTypeLinePiece = -1;", function_body(self.shipped, "static void TipSetBlockClear()"))
        hook = self.shipped[self.shipped.index("static void Hook_TipBuiltinText("):]
        hook = hook[:hook.index("\n}\n") + 2]
        self.assertIn("if constexpr (N == 0) {", hook)
        self.assertIn("if (g_TipTypeLinePiece >= 0 && TipTypeLineDraw(Result, S, O, argc, Args)) return;", hook)
        # The pass's piece is armed only by Mark1TooltipRows.
        self.assertEqual(re.findall(r"\bg_TipTypeLinePiece\s*=\s*static_cast<int>\(piece\);", self.shipped),
                         ["g_TipTypeLinePiece = static_cast<int>(piece);"])
        self.assertIn("g_TipTypeLinePiece = static_cast<int>(piece);", self.body("tooltip_rows"))
        # The hook goes in with the pieces' ground consumer, never from a draw or a dressing.
        install = function_body(self.shipped, "static void InstallTipTypeLineHook()")
        self.assertIn("InstallTipBuiltinTextHook<0>();", install)
        self.assertIn("InstallForgedTooltipHooks();", install)
        self.assertEqual(len(re.findall(r"\bInstallTipTypeLineHook\(\);", self.shipped)), 1)
        self.assertIn("InstallTipTypeLineHook();", self.body("ground_install"))

    def test_tooltip_stat_fields_tip_and_tip_unread_count_blocks_drawn(self):
        fields = self.body("stat_fields")
        self.assertIn("f.tip = g_TipSetBlocks;", fields)
        self.assertIn("f.tipUnread = g_TipSetBlocksUnread;", fields)
        stat = function_body(self.shipped, "static RValue& Hook_DrawInventoryStatsNew(")
        count = "if (g_TipSetBlock && g_TipSetOnScreen) {"
        self.assertIn(count, stat)
        self.assertLess(stat.index("extra += TipDrawRow(x, y + extra, row);"), stat.index(count))
        self.assertIn("++g_TipSetBlocks;", stat)
        self.assertIn("if (g_TipSetBlockUnread) ++g_TipSetBlocksUnread;", stat)
        self.assertEqual(re.findall(r"\+\+g_TipSetBlocks\b", self.shipped), ["++g_TipSetBlocks"])

    # ---- round 0's instrument-blindness findings: what the tooltip counts ------------------

    def test_type_line_hook_not_gated_on_icons(self):
        # The pieces' type line (the tooltip's draw_text) goes in once the art
        # is armed and a character exists, with or without embedded art: the
        # ground install's first statement, before the icons' own return.
        install = self.body("ground_install")
        first = "if (M1sNs::TypeLineInstallPending(g_Mark1.ArtArmed(), g_TipTypeLineTried)) InstallTipTypeLineHook();"
        self.assertEqual(self.lines("ground_install")[0], first)
        self.assertLess(install.index(first), install.index("if (ForgePact::Mark1Icons::kIconCount == 0) return;"))
        # The core's rule takes no icon count; the tick's pending and wanted
        # checks cover it, so a build with no art embedded still installs it.
        header = self.header()
        self.assertIn("return armed && !tried;", function_body(header, "inline bool TypeLineInstallPending(bool armed, bool tried)"))
        pending = function_body(header, "inline bool ArtInstallPending(")
        self.assertIn("ArtGroundInstallPending(armed, iconsEmbedded, groundTried)", pending)
        self.assertIn("TypeLineInstallPending(armed, typeLineTried)", pending)
        self.assertIn("|| TypeLineInstallPending(armed, typeLineTried)", pending)
        self.assertIn("g_TipTypeLineTried", self.lines("tick")[0])
        self.assertIn("g_TipTypeLineTried", self.body("art_install_if_ready"))
        # Tried once a session; the hook's only caller is that statement.
        hook = function_body(self.shipped, "static void InstallTipTypeLineHook()")
        self.assertLess(hook.index("if (g_TipTypeLineTried) return;"), hook.index("g_TipTypeLineTried = true;"))
        self.assertLess(hook.index("g_TipTypeLineTried = true;"), hook.index("InstallTipBuiltinTextHook<0>();"))
        self.assertEqual(len(re.findall(r"\bInstallTipTypeLineHook\(\);", self.shipped)), 1)

    def test_item_truth_passes_not_counted_in_tip(self):
        # `tip=`, `tip-unread=`, `tip-armed=`, `typeline=` and `typeline-miss=`
        # count the player's own (on-screen) passes only. Item Truth's
        # off-screen TipDrawOne passes arm the block and draw the type line
        # too, but never mark the pass on screen and bump none of the counts.
        one = strip_comments(function_body(self.shipped, "static bool TipDrawOne("))
        for name in ("g_TipSetOnScreen", "g_TipSetBlocks", "g_TipSetBlocksUnread", "g_TipSetPassesArmed",
                     "g_TipTypeLines", "g_TipTypeLineMisses", "g_TipTypeLineSwaps"):
            with self.subTest(name=name):
                self.assertNotIn(name, one)
        # The on-screen flag is set in one place: the player's pass, once armed for a piece.
        self.assertEqual(re.findall(r"\bg_TipSetOnScreen\s*=\s*true\b", self.shipped), ["g_TipSetOnScreen = true"])
        item = strip_comments(function_body(self.shipped, "static RValue& Hook_DrawInventoryItemV2("))
        arm = "if (pieceOnScreen) {"
        self.assertIn("const bool pieceOnScreen = g_TipSetBlock;", item)
        self.assertIn("g_TipSetOnScreen = true;", item)
        self.assertIn("++g_TipSetPassesArmed;", item)
        # Armed after the piece's rows were built, and before the game's pass.
        self.assertLess(item.index("Mark1TooltipRows(tipPiece)"), item.index("const bool pieceOnScreen = g_TipSetBlock;"))
        self.assertLess(item.index("++g_TipSetPassesArmed;"), item.index("g_Orig_DrawInventoryItemV2("))
        # Off-screen batches run before the arming, so never inside an armed pass.
        self.assertLess(item.index("TipDrawBatch(S, O, argc, A);"), item.index("const bool pieceOnScreen = g_TipSetBlock;"))
        # The pass end: of the armed passes, swapped or not, then cleared.
        end = "if (g_TipTypeLineSwaps != swapsBefore) ++g_TipTypeLines;"
        self.assertIn(end, item)
        self.assertIn("else ++g_TipTypeLineMisses;", item)
        self.assertLess(item.index("g_Orig_DrawInventoryItemV2("), item.index(end))
        self.assertLess(item.index(end), item.rindex("TipSetBlockClear();"))
        self.assertEqual(item.count(arm), 2)
        # The swap is counted where the type-line detour draws it, and nowhere else.
        line = strip_comments(function_body(self.shipped, "static bool TipTypeLineDraw("))
        self.assertLess(line.index("if (line == drawn) return false;"), line.index("++g_TipTypeLineSwaps;"))
        self.assertEqual(re.findall(r"\+\+g_TipTypeLineSwaps\b", self.shipped), ["++g_TipTypeLineSwaps"])
        for counter in ("g_TipSetPassesArmed", "g_TipTypeLines", "g_TipTypeLineMisses"):
            with self.subTest(counter=counter):
                self.assertEqual(re.findall(r"\+\+" + counter + r"\b", self.shipped), ["++" + counter])
        # The block's own count needs the on-screen pass too.
        stat = function_body(self.shipped, "static RValue& Hook_DrawInventoryStatsNew(")
        self.assertIn("if (g_TipSetBlock && g_TipSetOnScreen) {", stat)
        # And the line's three new fields read those counters.
        fields = self.body("stat_fields")
        self.assertIn("f.tipArmed = g_TipSetPassesArmed;", fields)
        self.assertIn("f.typeLine = g_TipTypeLines;", fields)
        self.assertIn("f.typeLineMiss = g_TipTypeLineMisses;", fields)
        # Each pass starts and ends with the on-screen flag clear.
        clear = function_body(self.shipped, "static void TipSetBlockClear()")
        self.assertIn("g_TipSetOnScreen = false;", clear)

    # ---- the jump (Mark1Jump.hpp): the shared detours ------------------------------------

    def jump_header(self):
        return strip_comments(JUMP_HEADER.read_text(encoding="utf-8").replace("\r\n", "\n"))

    def test_jump_one_skills_leap_detour_and_one_builtin_install_in_the_whole_plugin(self):
        whole = strip_comments(self.plugin)
        # One HookOneScript of skillsLeap for the player build's mods, in the
        # shared install, which jumpscenery and Mark 1 both call.
        self.assertEqual(whole.count('"fp_jumpscenery_leap"'), 1)
        leap_install = self.body("leap_install")
        self.assertIn("HookOneScript(SdkShortScriptName(HeroSiege::Scripts::gml_Script_skillsLeap), \"fp_jumpscenery_leap\","
                      " (PVOID)JsHookLeap, &g_JsOrigLeap, &native)", leap_install)
        self.assertIn("if (!g_JsOrigLeap) {", leap_install)   # whoever is first installs; the other finds it
        self.assertIn("if (!g_JsLeapNative)", leap_install)
        self.assertEqual(len(re.findall(r"\bSkillsLeapInstall\(\)", self.shipped)), 3)   # definition, jumpscenery, Mark 1
        self.assertIn("SkillsLeapInstall()", self.body("js_install"))
        self.assertIn("SkillsLeapInstall()", self.body("jump_install"))
        # One install of the five builtins: jumpscenery, the flight charms and Mark 1 call it.
        self.assertEqual(len(re.findall(r"HookBuiltin\([^;]*kJsBuiltinDetours", whole)), 1)
        self.assertEqual(len(re.findall(r"\bCollisionBuiltinsInstall\(\)", self.shipped)), 4)
        for which in ("js_install", "flight_install", "jump_install"):
            with self.subTest(install=which):
                self.assertIn("CollisionBuiltinsInstall()", self.body(which))
        # Mark 1's adapter hooks neither itself.
        self.assertNotIn("HookBuiltin(", self.code)
        self.assertNotIn("skillsLeap\"", self.code)

    def test_jump_the_leap_detour_decides_before_the_original_and_scales_after_it(self):
        leap = self.body("leap")
        lines = self.lines("leap")
        # Both switches off, or the adapters' own calls: the original alone.
        self.assertEqual(lines[0], "if ((!g_JumpScenery.Enabled() && !g_Mark1.Enabled()) || g_JsBusy) "
                                   "return g_JsOrigLeap(S, O, R, argc, A);")
        entry = leap.index("mark1 = Mark1JumpOnLeapEntry(S, frame);")
        first_orig = leap.index("g_JsOrigLeap(S, O, R, argc, A)", leap.index(lines[0]) + len(lines[0]))
        self.assertLess(entry, first_orig)
        self.assertLess(leap.index("RValue& r = g_JsOrigLeap(S, O, R, argc, A);"), leap.index("Mark1JumpOnLeapExit(S, frame);"))
        # With Mark 1 not taking the entry, jumpscenery's path is the one it
        # was: its entry before the original, for the local player as before.
        self.assertIn("if (!mark1 && g_JumpScenery.Enabled()) {", leap)
        self.assertIn("g_JumpScenery.OnLeapEntry(frame, S && S == g_JsPlayer);", leap)
        self.assertLess(entry, leap.index("g_JumpScenery.OnLeapEntry("))
        self.assertIn("if (!mark1) return g_JsOrigLeap(S, O, R, argc, A);", leap)
        self.assertLess(leap.index("g_JumpScenery.OnLeapEntry("), leap.index("if (!mark1) return g_JsOrigLeap(S, O, R, argc, A);"))
        # The adapters' reads inside it reach no collision detour.
        self.assertLess(leap.index("g_JsBusy = true;"), entry)

    def test_jump_the_builtin_detours_third_consumer_is_mark1(self):
        detour = self.body("shared_detour")
        self.assertEqual(len(re.findall(r"\borig\(", detour)), 1)
        self.assertLess(detour.index("FlightOnBuiltin(row, Result, S, O, argc, Args);"),
                        detour.index("Mark1JumpOnBuiltin(row, Result, S, O, argc, Args);"))
        self.assertLess(detour.index("if (g_JsBusy || g_FlightBusy) return;"), detour.index("Mark1JumpOnBuiltin("))
        part = self.body("jump_builtin")
        lines = self.lines("jump_builtin")
        self.assertEqual(lines[0], "if (!g_M1jWired || !g_Mark1.Enabled() || !g_M1jCore.Active()) return;")
        self.assertEqual(lines[1], "if (!S || S != g_M1sPlayer) return;")
        # Every really-blocked query of the local player reaches the core with
        # its object's classes, in a window or not; a free answer stands.
        self.assertIn("if (!JsBlocked(b.answer, Result, false)) return;", part)
        self.assertNotIn("WindowOpen", part)
        self.assertIn("JsObjectIndexOf(Args[b.objectArg])", part)
        self.assertIn("g_M1jClasses.Classify(object)", part)
        self.assertIn("g_M1jCore.OnQuery(", part)
        self.assertIn("JsExcludedBlocks(orig, b, S, O, argc, Args)", part)
        self.assertIn("if (answer != JsNs::Answer::Real) Result = JsAnswerValue(answer);", part)

    # ---- the jump: learner_feed -------------------------------------------------------------

    def test_jump_learner_feed_m_reads_jumpscenerys_getters_through_choose_max(self):
        max_ = self.body("jump_max")
        self.assertIn("M1jNs::ChooseMax(g_JumpScenery.HasCap(), g_JumpScenery.Cap(), g_JumpScenery.HasReach(), "
                      "g_JumpScenery.Reach())", max_)
        # Whatever jumpscenery's switch says.
        self.assertNotIn("Enabled()", max_)
        self.assertIn("g_M1jCore.SetMax(&Mark1JumpMax);", self.body("jump_install"))

    def test_jump_learner_feed_a_clean_jump_reaches_jumpsceneries_public_learn(self):
        learn = self.body("jump_learn")
        self.assertIn("g_JumpScenery.Learn(l.takeoffX, l.takeoffY, l.haveCursor, l.cursorX, l.cursorY, l.naturalLength);", learn)
        self.assertNotIn("Enabled()", learn)
        self.assertIn("g_M1jCore.SetLearner(&Mark1JumpLearn);", self.body("jump_install"))
        # The core closes a jump's window from the tick, so learning goes on
        # while jumps are scaled.
        self.assertIn("g_M1jCore.Tick(", self.body("jump_tick"))
        tick = self.body("tick")
        self.assertIn("Mark1JumpTick();", tick)
        self.assertLess(tick.index("Mark1RefreshPlayer();"), tick.index("Mark1JumpTick();"))
        # Learn is jumpscenery's public rule, in its header.
        header = strip_comments((ROOT / "plugin" / "include" / "ForgePact" / "JumpScenery.hpp").read_text(encoding="utf-8"))
        self.assertIn("void Learn(double takeoffX, double takeoffY, bool haveCursor, double cursorX, double cursorY, double length)",
                      header)

    def test_jump_learner_feed_jumpscenerys_entry_is_skipped_while_mark1_takes_the_jump(self):
        entry = self.body("jump_entry")
        # jumpscenery's open window keeps its jump: Mark 1 takes the next one.
        self.assertIn("if (g_JumpScenery.WindowOpen(frame)) return false;", entry)
        self.assertLess(entry.index("g_JumpScenery.WindowOpen(frame)"), entry.index("g_M1jCore.OnLeapEntry("))
        # The bonus read at the point of use, for Mark 1's own player.
        self.assertIn("if (!S || S != g_M1sPlayer) return false;", entry)
        self.assertIn("g_Mark1.Active(", entry)
        self.assertIn("Mark1ReadWorn", entry)
        self.assertIn("g_M1jCore.SetActive(active);", entry)
        self.assertLess(entry.index("g_M1jCore.SetActive(active);"), entry.index("g_M1jCore.OnLeapEntry("))
        self.assertIn("return g_M1jCore.OnLeapEntry(frame, true);", entry)
        # Switch off: a jump in progress is dropped, never handed to jumpscenery's window.
        self.assertIn("if (!g_Mark1.Enabled()) { g_M1jCore.SetActive(false); return false; }", entry)

    # ---- the jump: the landing reads ------------------------------------------------------

    def test_jump_landing_reads_are_mark1s_own_and_a_failed_read_is_never_an_answer(self):
        jump = "\n".join(self.body(w) for w in ("jump_position", "jump_cursor", "jump_family", "jump_gate", "jump_grid",
                                                "jump_entry", "jump_exit", "jump_builtin", "jump_install"))
        # Not jumpscenery's player-bound reads: JsFamilyAt turns a failed read
        # into "blocked", and both read g_JsPlayer, which only its tick refreshes.
        for forbidden in ("JsFamilyAt(", "JsPlayerPosition(", "g_JsPlayer"):
            with self.subTest(forbidden=forbidden):
                self.assertNotIn(forbidden, jump)
        position = self.body("jump_position")
        self.assertIn("g_M1sPlayer->ToRValue()", position)
        self.assertIn('RValue("x")', position)
        self.assertIn("JsCursor(x, y, route)", self.body("jump_cursor"))
        family = self.body("jump_family")
        self.assertIn("g_JsOrig[(int)JsNs::Builtin::PlaceMeeting]", family)
        self.assertIn("orig(r, g_M1sPlayer, g_M1sPlayer, 3, args);", family)
        self.assertIn("RValue((double)g_JsFamilyIdx)", family)
        self.assertIn("throw Mark1JumpUnread{};", family)
        self.assertLess(family.index("throw Mark1JumpUnread{};"), family.index("orig(r,"))
        gate = self.body("jump_gate")
        self.assertIn("g_JsOrig[(int)JsNs::Builtin::CollisionLine]", gate)
        self.assertIn("for (int e : g_JsExcludedIdx)", gate)
        self.assertIn("orig(r, g_M1sPlayer, g_M1sPlayer, 7, args);", gate)
        self.assertIn("throw Mark1JumpUnread{};", gate)
        install = self.body("jump_install")
        for wiring in ("g_M1jCore.SetPlayerPosition(&Mark1JumpPosition);", "g_M1jCore.SetCursor(&Mark1JumpCursor);",
                       "g_M1jCore.SetRoomSize(&JsRoomSize);", "g_M1jCore.SetPlaceMeeting(&Mark1JumpFamilyAt);",
                       "g_M1jCore.SetGrid(&Mark1JumpGridAt);", "g_M1jCore.SetGateOnLine(&Mark1JumpGateOnLine);"):
            with self.subTest(wiring=wiring):
                self.assertIn(wiring, install)
        # The scaled position is written only after the original returned, to
        # Mark 1's own player.
        exit_ = self.body("jump_exit")
        self.assertIn("g_M1jCore.OnLeapExit(frame, S && S == g_M1sPlayer)", exit_)
        self.assertLess(exit_.index("if (!m.write || !g_M1sPlayer) return;"), exit_.index('"variable_instance_set"'))

    # ---- the jump: grid_read -----------------------------------------------------------------

    def test_jump_grid_read_by_name_outside_every_research_block(self):
        self.assertIn(SIGNATURES["jump_grid"], strip_research_blocks(self.plugin))
        grid = self.body("jump_grid")
        self.assertIn('g_Yytk->CallBuiltin("variable_global_get", { RValue("pathGrid") })', grid)
        self.assertIn('g_Yytk->CallBuiltin("mp_grid_get_cell", { grid, RValue(cx), RValue(cy) })', grid)
        self.assertIn("M1jNs::GridCellAt(", grid)
        self.assertIn("catch (...) { return M1jNs::Cell::Unreadable; }", grid)
        # Nothing of the research build's grid read.
        for research in ("M1pGridAt", "GridCellOf", "M1pNs", "Mark1Probe"):
            with self.subTest(research=research):
                self.assertNotIn(research, grid)
        kind = self.body("jump_kind")
        for k in ("M1jNs::ValueKind::Number", "M1jNs::ValueKind::Reference", "M1jNs::ValueKind::Other"):
            self.assertIn(k, kind)
        # The mapping is the header's: not a grid handle is NoGrid, below 0 Blocked.
        header = self.jump_header()
        self.assertIn("if (!IsGridHandle(gridKind, grid)) return Cell::NoGrid;", header)
        self.assertIn("return value < 0.0 ? Cell::Blocked : Cell::Free;", header)
        self.assertIn("#include <ForgePact/Mark1Jump.hpp>", self.shipped)

    # ---- the jump: jump_objects --------------------------------------------------------------

    def test_jump_objects_resolved_by_name_by_whichever_install_runs_first(self):
        resolve = self.body("resolve_objects")
        self.assertIn('"asset_get_index"', resolve)
        self.assertIn("HeroSiege::Objects::GetObjectName(kJsFamily)", resolve)
        self.assertIn("for (const HeroSiege::Objects::GameObject obj : kJsExcluded)", resolve)
        self.assertIn("g_JsFamilyIdx = family;", resolve)
        self.assertIn("g_JsExcludedIdx = excluded;", resolve)
        # Both installs call it; Mark 1's before any hook, refusing with the names.
        self.assertIn("JsResolveObjects()", self.body("js_install"))
        install = self.body("jump_install")
        self.assertIn("const std::string missing = JsResolveObjects();", install)
        self.assertIn('if (!missing.empty()) return missing + " did not resolve by name', install)
        self.assertLess(install.index("JsResolveObjects()"), install.index("SkillsLeapInstall()"))
        self.assertEqual(len(re.findall(r"\bJsResolveObjects\(\)", self.shipped)), 3)

    def test_jump_objects_classified_by_mark1s_own_classifier(self):
        install = self.body("jump_install")
        self.assertIn('"object_is_ancestor"', install)
        self.assertIn("g_M1jClasses.SetIsAncestor(", install)
        self.assertIn("g_M1jClasses.SetObjects(g_JsFamilyIdx, g_JsExcludedIdx[0], g_JsExcludedIdx[1]);", install)
        self.assertIn("static ForgePact::Mark1Jump::Classifier g_M1jClasses;", self.shipped)
        # The gate and lock re-run and the gate line use the resolved list.
        self.assertIn("for (int e : g_JsExcludedIdx)", self.body("jump_gate"))
        # The header's classifier, not jumpscenery's private one.
        self.assertIn("class Classifier {", self.jump_header())

    # ---- the jump: the install and the holders ---------------------------------------------------

    def test_jump_install_is_part_of_mark1s_all_or_nothing_install(self):
        install = self.body("install")
        self.assertIn("const std::string jump = Mark1JumpInstall();", install)
        self.assertIn("if (!jump.empty()) return jump;", install)
        self.assertLess(install.index("if (!g_M1sCtuNative)"), install.index("Mark1JumpInstall()"))
        self.assertLess(install.index("Mark1JumpInstall()"), install.index("Mark1GroundIconInstall();"))
        self.assertEqual(len(re.findall(r"\bMark1JumpInstall\(\)", self.shipped)), 2)   # definition and Mark1SetInstall
        jump = self.body("jump_install")
        self.assertIn("g_M1jHooksTried = true;", jump)
        self.assertLess(jump.index("g_M1jHooksTried = true;"), jump.index("SkillsLeapInstall()"))
        self.assertIn("g_M1jWired = true;", jump)
        self.assertLess(jump.index("CollisionBuiltinsInstall()"), jump.index("g_M1jWired = true;"))

    def test_jump_refused_while_a_research_probe_or_citrace_holds_one(self):
        raw = function_body(self.plugin, SIGNATURES["install"])
        research = raw[raw.index("#ifndef FORGEPACT_RELEASE"):raw.index("#endif")]
        self.assertIn("Mark1JumpResearchHolder()", research)
        self.assertLess(raw.index("Mark1JumpResearchHolder()"), raw.index("HookOneScript("))
        holder = self.body("jump_research_holder")
        self.assertIn("JumpSceneryResearchHolders(false)", holder)   # citrace and jumpprobe
        self.assertIn("M1pScriptHolder(HeroSiege::Scripts::gml_Script_skillsLeap)", holder)
        self.assertIn("M1pBuiltinHolder(b.name)", holder)
        own = self.body("m1p_builtin_holder")
        self.assertIn("t.orig", own)
        self.assertIn('"mark1probe hook"', own)
        self.assertNotIn("Mark1JumpResearchHolder", self.shipped)

    def test_jump_the_research_probes_name_mark1set_for_what_it_holds(self):
        held = self.body("js_held")
        self.assertIn("g_M1jHooksTried", held)
        self.assertIn("mark1set", held)
        self.assertIn("mark1set", self.body("m1p_builtin_held"))

    # ---- the jump: jump_counts -----------------------------------------------------------------

    def test_jump_counts_every_field_from_the_cores_counter_of_the_same_name(self):
        fields = self.body("stat_fields")
        struct = function_body(self.header(), "struct JumpCounts {")
        names = re.findall(r"uint64_t\s+(\w+) = 0;", struct)
        self.assertEqual(len(names), 24, names)
        counters = function_body(self.jump_header(), "struct Counters {")
        for name in names:
            with self.subTest(field=name):
                self.assertIn("uint64_t " + name + " = 0;", counters)
                self.assertEqual(len(re.findall(r"\bjc\." + name + r" = j\." + name + r";", fields)), 1)
        self.assertIn("const M1jNs::Counters& j = g_M1jCore.Stats();", fields)
        self.assertIn("M1sNs::JumpCounts& jc = f.jump;", fields)
        # M with its source, as the jump will read it.
        self.assertIn("const M1jNs::Max m = Mark1JumpMax();", fields)
        self.assertIn("jc.max = m.value;", fields)
        self.assertIn("jc.maxSource = std::string(M1jNs::MaxSourceName(m.source));", fields)

    def test_jump_grid_reads_counted_by_answer_so_a_nogrid_read_is_never_silent(self):
        # A grid read that never attaches returns NoGrid, which counts free; the
        # stat line counts each grid cell a decision read by its answer, so it
        # shows as nogrid rather than as a free cell (instrument-blindness).
        for name in ("gridFree", "gridBlocked", "gridNoGrid"):
            with self.subTest(field=name):
                self.assertIn("jc." + name + " = j." + name + ";", self.body("stat_fields"))
        header = self.jump_header()
        self.assertIn("if (tally) CountCell(*tally, c);", header)
        self.assertIn("else if (c == Cell::NoGrid) ++k.gridNoGrid;", header)
        self.assertIn("Check(jump_.takeoffX, jump_.takeoffY, ux, uy, dist, w, h, &failed, &counters_);", header)
        self.assertIn(
            '" grid-reads=" + JoinCounts({ j.gridFree, j.gridBlocked, j.gridNoGrid })', self.header())

    # ---- the half walk (Mark1Movement.hpp) ---------------------------------------------------

    def movement_header(self):
        return strip_comments(MOVEMENT_HEADER.read_text(encoding="utf-8").replace("\r\n", "\n"))

    def test_walk_rides_the_one_shared_return_specific_stat_detour(self):
        # A second HookOneScript of ReturnSpecificStat would come up table-only
        # and blind, so the player build has exactly one: the shared header's.
        hook_call = re.compile(r"\bHookOneScript(?:Table)?\s*\(")

        def stat_hook_calls(source):
            calls = []
            for match in hook_call.finditer(source):
                depth, end = 0, len(source) - 1
                for index in range(match.end() - 1, len(source)):
                    if source[index] == "(":
                        depth += 1
                    elif source[index] == ")":
                        depth -= 1
                        if depth == 0:
                            end = index
                            break
                call = source[match.start():end + 1]
                if "ReturnSpecificStat" in call:
                    calls.append(call)
            return calls

        # The scanner's own control: it finds skill sliders' old spelling, and
        # ignores another script.
        self.assertEqual(len(stat_hook_calls(
            'HookOneScript(SdkShortScriptName(HeroSiege::Scripts::gml_Script_ReturnSpecificStat), "fp_ss_stat", '
            '(void*)HookReturnSpecificStat, &hook.orig, &native);')), 1)
        self.assertEqual(stat_hook_calls("HookOneScript(script.name, script.hookId, script.detour, &o, &n);"), [])
        found = {"ModuleMain.cpp": len(stat_hook_calls(self.shipped))}
        for header in sorted((ROOT / "plugin" / "include" / "ForgePact").glob("*.hpp")):
            text = strip_comments(strip_research_blocks(header.read_text(encoding="utf-8").replace("\r\n", "\n")))
            n = len(stat_hook_calls(text))
            if n:
                found[header.name] = n
        self.assertEqual(found, {"ModuleMain.cpp": 0, "ReturnSpecificStatHook.hpp": 1})
        install = self.body("walk_install")
        self.assertIn("if (!Rss::Install())", install)
        self.assertIn("TABLE-ONLY", install)
        # Mark 1's part is registered only once the detour is native.
        self.assertLess(install.index("if (!Rss::Install())"), install.index("Rss::mark1 = Mark1MovementOnStat;"))
        self.assertLess(install.index("Rss::mark1 = Mark1MovementOnStat;"), install.index("g_M1mWired = true;"))
        shared = strip_comments(SHARED_STAT_HEADER.read_text(encoding="utf-8").replace("\r\n", "\n"))
        hook = function_body(shared, "inline RValue& Hook(")
        self.assertLess(hook.index("original(S, O, R, argc, A)"), hook.index("if (mark1) mark1(S, argc, A, r);"))

    def test_walk_install_is_part_of_mark1s_all_or_nothing_install(self):
        install = self.body("install")
        self.assertIn("const std::string walk = Mark1MovementInstall();", install)
        self.assertIn("if (!walk.empty()) return walk;", install)
        self.assertLess(install.index("Mark1JumpInstall()"), install.index("Mark1MovementInstall()"))
        self.assertLess(install.index("Mark1MovementInstall()"), install.index("Mark1GroundIconInstall();"))
        self.assertEqual(len(re.findall(r"\bMark1MovementInstall\(\)", self.shipped)), 2)   # definition and Mark1SetInstall

    def test_walk_the_lever_runs_for_the_local_player_at_the_point_of_use(self):
        lines = self.lines("walk")
        # Switch off, the adapter's own calls, no self or any other self:
        # nothing, before any game read.
        self.assertEqual(lines[0], "if (!g_Mark1.Enabled() || g_M1sBusy || !S || S != g_M1sPlayer) return;")
        walk = self.body("walk")
        # The scope (id 64, a1) is asked before the bonus; the bonus at the
        # point of use, through the same read the gate's detour uses, guarded.
        self.assertLess(walk.index("M1mNs::Speed::Wants(true, haveId, id)"), walk.index("g_Mark1.Active("))
        self.assertIn("g_Mark1.Active((unsigned long long)g_RuntimeFrame, Mark1ReadWorn)", walk)
        self.assertLess(walk.index("g_M1sBusy = true;"), walk.index("g_Mark1.Active("))
        self.assertLess(walk.index("g_Mark1.Active("), walk.rindex("g_M1sBusy = false;"))
        self.assertIn("A[1]", walk)
        # The answer is written only when the core changed it.
        self.assertIn("if (g_M1mSpeed.Answer(active, number, answer)) r = RValue(answer);", walk)
        self.assertEqual(walk.count("r = RValue("), 1)
        # The local player is the tick's (HhResolveLocalPlayer); no kind check
        # on the self decides anything.
        self.assertNotIn("VALUE_OBJECT", walk)
        self.assertNotIn("VALUE_REF", walk)
        header = self.movement_header()
        self.assertIn("return localPlayer && haveStatId && statId == kMovementSpeedStatId;", header)
        self.assertIn("inline constexpr double kMovementSpeedStatId = 64.0;", header)

    def test_walk_refused_while_a_research_probe_holds_return_specific_stat(self):
        raw = function_body(self.plugin, SIGNATURES["install"])
        research = raw[raw.index("#ifndef FORGEPACT_RELEASE"):raw.index("#endif")]
        self.assertIn("Mark1MovementResearchHolder()", research)
        self.assertLess(raw.index("Mark1MovementResearchHolder()"), raw.index("HookOneScript("))
        holder = self.body("walk_research_holder")
        for check in ("JpHolder(", "M1pScriptHolder(name)", "HeroSiege::Scripts::gml_Script_ReturnSpecificStat"):
            self.assertIn(check, holder)
        # projprobe's names stay in its region (test_skill_sliders_probe_contract):
        # its hold shows as the shared install's TABLE-ONLY refusal instead.
        self.assertNotIn("g_Pp", holder)
        # Its own hold is not a refusal.
        self.assertIn('holder != "mark1set"', holder)
        self.assertNotIn("Mark1MovementResearchHolder", self.shipped)

    def test_walk_the_research_probes_name_mark1set_for_return_specific_stat(self):
        holder = self.body("holder")
        self.assertIn("g_M1mWired", holder)
        self.assertIn("HeroSiege::Scripts::gml_Script_ReturnSpecificStat", holder)
        # projprobe asks before it installs, and refuses naming the holder.
        attach = self.body("pp_attach_stat")
        self.assertIn("Mark1SetHolder(HeroSiege::Scripts::gml_Script_ReturnSpecificStat)", attach)
        self.assertLess(attach.index("Mark1SetHolder("), attach.index("HookOneScript("))

    def test_walk_stat_fields_speed_counts_and_penalty_unavailable(self):
        fields = self.body("stat_fields")
        self.assertIn("f.speed.available = M1mNs::kSpeedLever;", fields)
        self.assertIn("f.speed.count = g_M1mSpeed.Stats().halved;", fields)
        self.assertIn("f.penalty.available = M1mNs::kPenaltyLever;", fields)
        self.assertNotIn("f.penalty.count", fields)
        header = self.movement_header()
        self.assertIn("inline constexpr bool kSpeedLever = true;", header)
        self.assertIn("inline constexpr bool kPenaltyLever = false;", header)
        self.assertIn('inline constexpr std::string_view kPenaltyRoute = "none";', header)

    def test_walk_no_cast_penalty_lever_ships(self):
        # penalty-route: none. Mark 1 hooks no TalentUse and holds no effect
        # element: the adapter names neither.
        for token in ("gml_Script_TalentUse)", "playerEffect", "Mark1Penalty"):
            self.assertNotIn(token, self.code)

    # ---- the turns: the landing detour on TalentUseSetSpeed ---------------------------------

    def test_landing_detour_goes_in_through_hook_one_script_with_its_own_id_in_the_all_or_nothing_install(self):
        install = self.body("install")
        call = ('HookOneScript(SdkShortScriptName(HeroSiege::Scripts::gml_Script_TalentUseSetSpeed), "fp_mark1set_tuss",')
        self.assertIn(call, install)
        self.assertIn("(PVOID)Mark1HookTalentUseSetSpeed, &g_M1sOrigTalentUseSetSpeed, &g_M1sTussNative)", install)
        # Its own hook id, never the gate's.
        self.assertIn('"fp_mark1set_ctu"', install)
        self.assertEqual(self.code.count('"fp_mark1set_tuss"'), 1)
        # Installed once: a hook that went in stays in.
        self.assertIn("if (!g_M1sOrigTalentUseSetSpeed", install)
        # After the gate went in native, before the jump, the walk and the art:
        # part of the one all-or-nothing install, reached only through the gate.
        self.assertLess(install.index("if (!g_M1sCtuNative)"), install.index(call))
        for later in ("Mark1JumpInstall()", "Mark1MovementInstall()", "Mark1GroundIconInstall();"):
            with self.subTest(later=later):
                self.assertLess(install.index(call), install.index(later))
        self.assertIn("TalentUseSetSpeed was not found by name", install)
        # Both builds: the detour is the player's landing instrument.
        self.assertIn(call, strip_comments(function_body(strip_research_blocks(self.plugin), SIGNATURES["install"])))
        # Exactly one TalentUseSetSpeed detour in the adapter.
        self.assertEqual(len(re.findall(r"\bMark1HookTalentUseSetSpeed\b", self.code)), 2)   # definition and the install

    def test_landing_refused_unless_its_inline_detour_went_in(self):
        install = self.body("install")
        self.assertIn("if (!g_M1sTussNative)", install)
        refusal = install[install.index("if (!g_M1sTussNative)"):]
        refusal = refusal[:refusal.index(";") + 1]
        self.assertIn("TalentUseSetSpeed went in TABLE-ONLY", refusal)
        self.assertIn("return", refusal)
        self.assertLess(install.index("fp_mark1set_tuss"), install.index("if (!g_M1sTussNative)"))
        self.assertLess(install.index("if (!g_M1sTussNative)"), install.index("Mark1JumpInstall()"))

    def test_landing_refused_while_a_research_probe_holds_the_script(self):
        raw = function_body(self.plugin, SIGNATURES["install"])
        research = raw[raw.index("#ifndef FORGEPACT_RELEASE"):raw.index("#endif")]
        self.assertIn("Mark1LandingResearchHolder()", research)
        self.assertLess(raw.index("Mark1LandingResearchHolder()"), raw.index("HookOneScript("))
        holder = self.body("landing_research_holder")
        self.assertIn("HeroSiege::Scripts::gml_Script_TalentUseSetSpeed", holder)
        # tgprobe (and every holder skillprobe knows, JpHolder) and the set's
        # research probe by name; projprobe, whose names stay in its region
        # (test_skill_sliders_probe_contract), by the thunk its row leaves in
        # the script table, asked before the set's own HookOneScript would
        # chain onto it table-only.
        for check in ("JpHolder(name, calls)", "M1pScriptHolder(name)", "M1pTableHeld(name)", "projprobe hook"):
            self.assertIn(check, holder)
        self.assertNotRegex(holder, r"\bg_Pp|\bkPp|\bProjProbe")
        table = self.body("m1p_table_held")
        self.assertIn("GetNamedRoutinePointer(", table)
        self.assertIn("return !AddrIsExecutableInModule(GetModuleHandleA(nullptr), "
                      "(const void*)sc->m_Functions->m_ScriptFunction);", table)
        # Its own hold is not a refusal, by name or by its own table thunk.
        self.assertIn('holder != "mark1set"', holder)
        self.assertIn('if (g_M1sOrigTalentUseSetSpeed) return "";', holder)
        self.assertLess(holder.index('if (g_M1sOrigTalentUseSetSpeed) return "";'), holder.index("M1pTableHeld(name)"))
        self.assertNotIn("Mark1LandingResearchHolder", self.shipped)
        # The gate's check, asked again on a retry after a partial install,
        # does not take the set's own hold for a probe's either.
        self.assertIn('holder != "mark1set"', self.body("research_holder"))

    def test_landing_the_research_probes_name_mark1set_while_it_holds_the_script(self):
        holder = self.body("holder")
        self.assertIn("g_M1sOrigTalentUseSetSpeed", holder)
        self.assertIn("HeroSiege::Scripts::gml_Script_TalentUseSetSpeed", holder)
        # Asked before the gate's own check returns "" for any other script.
        self.assertLess(holder.index("g_M1sOrigTalentUseSetSpeed"), holder.index("if (!g_M1sOrigCheckTalentUse)"))
        # TalentUse stays co-op's (and the live session's independent count).
        self.assertNotIn("gml_Script_TalentUse)", holder)
        self.assertNotIn("gml_Script_TalentUse,", holder)
        # tgprobe asks before it blocks; projprobe asks before it installs and
        # refuses naming the holder; mark1probe asks through JpHolder (SpHolder).
        self.assertIn("Mark1SetHolder(runtimeName)", self.body("tg_attach"))
        attach = self.body("pp_attach")
        self.assertIn("Mark1SetHolder(", attach)
        self.assertLess(attach.index("Mark1SetHolder("), attach.index("HookOneScript("))
        self.assertIn('"blocked: " + held + " holds it (a script takes one inline detour)"', attach)
        self.assertIn("Mark1SetHolder(name)", self.body("sp_holder"))
        m1p = strip_comments(function_body(self.plugin, "static bool M1pHeldBy(const M1pRow& t, std::string& holder)"))
        self.assertIn("JpHolder(t.runtimeName, calls)", m1p)

    def test_landing_counts_only_the_local_players_calls_while_the_switch_is_on(self):
        lines = self.lines("landing")
        self.assertEqual(lines[0], "if (g_Mark1.Enabled() && S && S == g_M1sPlayer) {")
        landing = self.body("landing")
        self.assertIn("++g_M1sCastCalls;", landing)
        self.assertIn("if (g_M1sCastWindow) g_M1sCastLanded = true;", landing)
        # The original always runs, after the counts, and its result is the game's.
        self.assertEqual(lines[-1], "return g_M1sOrigTalentUseSetSpeed(S, O, R, argc, A);")
        self.assertEqual(landing.count("g_M1sOrigTalentUseSetSpeed("), 1)
        # Nothing is read from or written to the game; no kind check decides it.
        for token in ("CallBuiltin", "variable_", "array_set", "VALUE_OBJECT", "VALUE_REF"):
            with self.subTest(token=token):
                self.assertNotIn(token, landing)
        # The count's and the flag's only writers.
        self.assertEqual(self.code.count("++g_M1sCastCalls;"), 1)
        self.assertEqual(re.findall(r"\bg_M1sCastCalls\s*(?:\+=|=(?!=))", self.code), ["g_M1sCastCalls ="])
        self.assertEqual(self.code.count("g_M1sCastLanded = true;"), 1)

    def test_landing_attributed_only_inside_a_gate_call_that_pressed_a_column(self):
        detour = self.body("detour")
        # The window opens only for a call that is to press a column, and only
        # around the original: TalentUseSetSpeed's calls before the gate or
        # after it (the restore) are not this call's landing.
        self.assertIn("const bool pressing = haveInput && plan.PressCount() > 0;", detour)
        self.assertLess(detour.index("haveInput = plan.PressCount() > 0 && Mark1InputState("),
                        detour.index("const bool pressing ="))
        window = self.body("cast_window")
        self.assertIn("g_M1sCastWindow = open;", window)
        self.assertIn("~Mark1CastWindow() { g_M1sCastWindow = previous; }", window)
        # Its writers: the definition, and the window's open and close.
        self.assertEqual(len(re.findall(r"\bg_M1sCastWindow\s*=(?!=)", self.code)), 3)
        # The flag starts clear for this call and is fed to the core after the
        # original returns, and after it throws (then rethrown).
        self.assertIn("g_M1sCastLanded = false;", detour)
        self.assertLess(detour.index("g_M1sCastLanded = false;"), detour.index("automation.PressAround<RValue>("))
        feed = "automation.Landing(plan, landed);"
        self.assertEqual(detour.count(feed), 1)
        self.assertEqual(detour.count("feedLanding();"), 2)
        after = detour[detour.index("automation.PressAround<RValue>("):]
        self.assertLess(after.index("feedLanding();"), after.index("return result;"))
        catch = after[after.index("catch (...)"):]
        self.assertLess(catch.index("feedLanding();"), catch.index("throw;"))
        # The core credits a landing only to the holder PressAround pressed in
        # that same Plan call, once.
        landing = function_body(self.automation_header(), "void Landing(const Mark1FramePlan& plan, bool landed)")
        self.assertIn("plan.call != m_PressedCall", landing)
        self.assertIn("m_PressedCall = 0;", landing)
        around = function_body(self.automation_header(), "decltype(auto) PressAround(")
        self.assertIn("if (column == plan.holder) m_PressedCall = plan.call;", around)
        self.assertLess(around.index("++m_Counters[i].presses;"), around.index("m_PressedCall = plan.call;"))
        # Inside the adapter, the core's one Landing call.
        self.assertEqual(self.code.count(".Landing("), 1)

    def test_turn_fields_each_come_from_the_cores_counter_of_the_same_name(self):
        fields = self.body("stat_fields")
        for line in ("c.landed = (uint64_t)k.landed;", "c.skipTurn = (uint64_t)k.skipTurn;",
                     "c.yielded = (uint64_t)k.yielded;"):
            with self.subTest(line=line):
                self.assertIn(line, fields)
        # The core's counters of those names.
        counters = function_body(self.automation_header(), "struct Mark1ColumnCounters {")
        for name in ("landed", "skipTurn", "yielded"):
            with self.subTest(counter=name):
                self.assertRegex(counters, r"long\s+" + name + r"\s+= 0;")
        # cast-calls: the landing detour's own count.
        self.assertIn("f.castCalls = g_M1sCastCalls;", fields)
        self.assertIn("uint64_t     castCalls = 0;", function_body(self.header(), "struct StatFields {"))


if __name__ == "__main__":
    unittest.main()
