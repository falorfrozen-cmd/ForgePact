"""Contract tests for the Mark 1 set's research instrument (ForgePact #166, phase 1).

The Mark 1 set's full bonus casts the bar's Q, E, R and Y skills whenever the
game would allow it, removes those casts' movement penalty, halves the final
movement speed and triples the jump. Five mechanisms behind that are not
established on this build, and docs/mark1-set-research.md asks them of one
research-build instrument, `mark1probe`. Its decisions and lines live in
plugin/include/ForgePact/Mark1Probe.hpp (test_mark1_probe_behavior.py runs
that core); these tests pin ModuleMain.cpp's adapter on comment-stripped
source. Every test name carries `research_only`:

- the word `mark1probe`, the core's include and every `M1p*` symbol vanish
  from what the player build compiles; the command is not a player command
  and is a standalone early return of its own straight after charmprobe's
  (RunCommand's else-if chain is at MSVC's C1061 limit); its one per-frame
  piece is M1pFrameTick;
- each of the nine rows is installed through HookOneScript by its
  HeroSiege::Scripts constant, and counts as hooked only with the inline
  detour; a row another hook holds is reported held, named where an existing
  holder check knows the holder, and never installed on top;
- the local player comes from HhResolveLocalPlayer / HhResolveInstance, the
  cursor from JsCursor, objects from HeroSiege::Objects or asset_get_index;
- no `Rva`, hex address or module-base arithmetic reaches the adapter;
- game state is written only inside the set, scale, move and stat levers:
  M1pWriteTarget (members, for set, scale and move) and the result write
  (stat), both reached only through the core's Around.

Phase 2's walk research (the Mark 1 set's Live procedure 1) adds, research
build only like the rest: eleven builtin rows, each by the core's runtime name
through one HookBuiltin site once its routine is game code, a held one left to
its holder and an unknown name `not-found` rather than failed; their detour
passes every call through and touches only the local player; `hold`'s one
restore, written through the core like every lever write; statpin's answer
through the same SetResult as stat; `pos` sampling (with a clock) whether or
not a row is hooked; and `base`, whose one game-script call is InitItemFromJson
by its SDK constant, never placing what it builds.

The Mark 1 set's adapter (phase 2) adds `spawn`, research build only like the
rest: the pieces built from Mark1Set.hpp's table through the same loader and
placed by LootGroundCreateFromItem with the local player as self, both by
their SDK constants, the pieces' ground icon installed first the way the
set's own install puts it in, and nothing written by the probe itself; and
`M1pScriptHolder`, the probe's own hold on a script, which the set's install
asks before it hooks CheckTalentUse.

The drops (phase 3) add `heroic`, `pool` and `copies`, research build only like
the rest: `heroic` calls DropItemHeroic by its SDK constant with the local
player as self (charmprobe heroic's call shape) and recognises a piece through
the shipped Mark1PieceOf; `pool` reads through the drop adapter's own reads and
writes nothing; `copies` is the only writer of the copies lever, a constant in
the player build. The kill route adds `unique` (the same calls on
DropUniqueItems) and `pool kill` (the kill route's own reads of all six lists,
refused while a roll's entries are on one).

Mod live procedure 6 adds `killgate [0|1]` (`test_killgate_research_only`): a lever
that opens LoadDrops' type-45 gate once per kill inside the game's own call, research
build only, installed inline by its SDK name and refused on a hook that is not the
inline detour; it writes only chances[45], re-enters only through the trampoline, never
names DropUniqueItems and rolls no die.

Mod live procedure 7 adds `draws`, `draws push` and `pool draw`
(`test_draws_research_only`): research build only, their call-outs inside research
blocks of the detours that already exist, DropItem's slot spliced from DropManager's
saved original and never hooked a second time, refused while angelicprobe's drop-item
row or gambaprobe's DropItem row holds it, and the lever writing only through the drop
adapter's add and delete. `test_satanic_tier_one_detour`: LoadSatanicDropTier has one
HookOneScript in the source, raredrop's HookSatanicTier, installed by one function that
reads the route back and that raredrop satanic, the watch and the kill draw's install
(#166) all call.

The Mark 1 set's replan after Live 1 adds `statask`, `manaregen` and `procgaps`
(`test_statask_manaregen_procgaps_are_research_only`): research build only;
`statask` asks ReturnSpecificStat by its SDK constant outside the adapter's busy
flag; `manaregen`'s id-42 answer sits in the set's consumer ahead of its switch
check and its cost; `procgaps` is fed from the gate detour for a proc the core
counted as returned.
"""
import re
import sys
import unittest
from pathlib import Path

TESTS_DIR = Path(__file__).resolve().parent
ROOT = TESTS_DIR.parent
PLUGIN = ROOT / "plugin" / "ModuleMain.cpp"
HEADER = ROOT / "plugin" / "include" / "ForgePact" / "Mark1Probe.hpp"
SDK_INCLUDE = ROOT.parent / "hs-game-sdk" / "cpp" / "include" / "hs_game_sdk"

if str(TESTS_DIR) not in sys.path:
    sys.path.insert(0, str(TESTS_DIR))

from test_release_hook_contract import function_body, strip_comments, strip_research_blocks  # noqa: E402

BLOCK_START = "// ---- mark1probe: the Mark 1 set's phase 1 instrument (ForgePact #166)"
BLOCK_END = "#endif // FORGEPACT_RELEASE (mark1probe)"
M1P_SYMBOL = r"\b(?:g_|k)?M1p[A-Z0-9_]\w*"

# The nine rows, in the core's Row order: (Row enumerator, SDK constant).
ROWS = (
    ("CheckTalentUse", "gml_Script_CheckTalentUse"),
    ("TalentUse", "gml_Script_TalentUse"),
    ("TalentUseSetSpeed", "gml_Script_TalentUseSetSpeed"),
    ("PlayerMovement", "gml_Script_PlayerMovement"),
    ("ReturnSpecificStat", "gml_Script_ReturnSpecificStat"),
    ("StatMovementSpeed", "gml_Script_StatMovementSpeed"),
    ("SkillsLeap", "gml_Script_skillsLeap"),
    ("PlayerJumpGravity", "gml_Script_playerJumpGravity"),
    ("StatJumpPower", "gml_Script_StatJumpPower"),
)
SDK_OBJECTS = ("Profile_Manager_obj", "Input_Device_Manager_obj", "Collision_Parent_obj", "Wall_Parent_obj")

# The builtin rows, in the core's Row order after the nine scripts:
# (Row enumerator, the runtime name the core's kRowNames gives it).
BUILTIN_ROWS = (
    ("PlaceMeeting", "place_meeting"),
    ("MpLinearStep", "mp_linear_step"),
    ("MpLinearStepObject", "mp_linear_step_object"),
    ("MpPotentialStep", "mp_potential_step"),
    ("MpPotentialStepObject", "mp_potential_step_object"),
    ("MoveTowardsPoint", "move_towards_point"),
    ("MotionSet", "motion_set"),
    ("MotionAdd", "motion_add"),
    ("MoveContactSolid", "move_contact_solid"),
    ("MoveOutsideSolid", "move_outside_solid"),
    ("MoveAndCollide", "move_and_collide"),
)

# Builtins and calls that change game state. The adapter may use the first
# three only inside M1pWriteTarget; the rest never. Game scripts are called
# by `base` (InitItemFromJson) and `spawn` (InitItemFromJson, then
# LootGroundCreateFromItem) only, each pinned by its own test below.
MEMBER_WRITES = ("variable_instance_set", "variable_struct_set", "array_set")
NEVER = ("variable_global_set", "instance_create", "instance_destroy", "instance_deactivate", "instance_activate",
         "ds_list_add", "ds_list_set", "ds_list_delete", "ds_map_set", "ds_map_add", "ds_map_delete",
         "mp_grid_add", "mp_grid_clear", "tilemap_set", "layer_set", "keyboard_key_press",
         "io_clear", "game_set_speed", "alarm_set", "LootGroundCreate(", "CallGameScript(")


class Mark1ProbeContract(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.plugin = PLUGIN.read_text(encoding="utf-8").replace("\r\n", "\n")
        start = cls.plugin.find(BLOCK_START)
        end = cls.plugin.find(BLOCK_END)
        if start < 0 or end < start:
            raise AssertionError("mark1probe's research block was not found in ModuleMain.cpp")
        cls.block = cls.plugin[start:end + len(BLOCK_END)]
        cls.code = strip_comments(cls.block)
        cls.shipped = strip_comments(strip_research_blocks(cls.plugin))
        cls.header = strip_comments(HEADER.read_text(encoding="utf-8").replace("\r\n", "\n"))

    def body(self, signature):
        return strip_comments(function_body(self.plugin, signature))

    def killgate_ops(self):
        ops = self.code[self.code.index("struct M1pKillgateOps {"):]
        return ops[:ops.index("\n};")]

    # ---- research build only ----------------------------------------------------

    def test_research_only_every_m1p_symbol_and_the_verb_vanish_from_the_player_build(self):
        symbols = set(re.findall(M1P_SYMBOL, self.code))
        # A scan that finds nothing would pass vacuously.
        for expected in ("M1pNs", "M1pCommand", "M1pInstall", "M1pHeldBy", "M1pOnScript", "M1pAccess", "M1pFrameTick",
                         "M1pRefreshPlayer", "M1pResolveScope", "M1pReadTarget", "M1pWriteTarget", "M1pSnapshot",
                         "M1pWalkable", "M1pCount", "M1pPos", "g_M1pRows", "g_M1pProbe", "g_M1pPlayer",
                         "M1pOnBuiltin", "M1pInstallBuiltins", "M1pBuiltinHeldBy", "M1pRowInstalled", "g_M1pBuiltinRows",
                         "M1pStatlog", "M1pBase", "M1pRestoreHold", "M1pSnapshotPlayerArrays", "M1pAlarmGet",
                         "M1pSpawn", "M1pScriptHolder"):
            self.assertIn(expected, symbols)
        # Negative control: the strip keeps player code, so an absence below
        # is the strip working, not an empty string.
        self.assertIn("kPlayerCommands", self.shipped)
        self.assertIn("static bool HookOneScript(", self.shipped)
        for symbol in sorted(symbols):
            self.assertIsNone(re.search(r"\b" + re.escape(symbol) + r"\b", self.shipped), symbol + " reaches the player build")
        self.assertIsNone(re.search(M1P_SYMBOL, self.shipped), "an M1p* symbol reaches the player build")
        player_text = strip_research_blocks(self.plugin)   # comments kept: not even a mention ships
        for word in ("mark1probe", "Mark1Probe", "HandleMark1ProbeCommand", "M1p"):
            self.assertNotIn(word, player_text)

    def test_research_only_the_core_is_included_inside_the_block(self):
        self.assertEqual(self.plugin.count("#include <ForgePact/Mark1Probe.hpp>"), 1)
        self.assertIn("#include <ForgePact/Mark1Probe.hpp>", self.block)
        self.assertIn("namespace M1pNs = ForgePact::Mark1Probe;", self.code)
        # The block is a research guard of its own: it opens one and closes it.
        self.assertIn("#ifndef FORGEPACT_RELEASE\n" + BLOCK_START, self.plugin)
        self.assertNotIn("Mark1Probe.hpp", strip_research_blocks(self.plugin))

    def test_research_only_not_a_player_command(self):
        run = function_body(self.plugin, "static void RunCommand(const std::string& line)")
        commands = run[run.index("kPlayerCommands = {"):]
        commands = commands[:commands.index("};")]
        self.assertIn('"menulayout"', commands)   # the set is the one read, not an empty slice
        self.assertNotIn('"mark1probe"', commands)

    def test_research_only_dispatched_as_a_standalone_early_return(self):
        run = function_body(self.plugin, "static void RunCommand(const std::string& line)")
        self.assertIn("    if (HandleCharmProbeCommand(lc, rest)) return;\n"
                      "#endif\n"
                      "#ifndef FORGEPACT_RELEASE\n"
                      "    if (HandleMark1ProbeCommand(lc, rest)) return;\n"
                      "#endif\n", run)
        self.assertEqual(run.count("HandleMark1ProbeCommand"), 1)
        self.assertNotIn('"mark1probe"', run)
        self.assertNotIn("else if (HandleMark1ProbeCommand", run)
        self.assertEqual(self.plugin.count('lc == "mark1probe"'), 1)
        handler = self.body("static bool HandleMark1ProbeCommand(")
        self.assertIn('if (lc == "mark1probe") { M1pCommand(rest); return true; }', handler)
        self.assertIn("return false;", handler)
        self.assertIn("static bool HandleMark1ProbeCommand(", self.code)

    def test_research_only_frame_callback_names_only_the_tick(self):
        frame = strip_comments(function_body(self.plugin, "void FrameCallback(FWFrame& FrameContext)"))
        self.assertEqual(re.findall(M1P_SYMBOL, frame), ["M1pFrameTick"],
                         "a probe that runs more than one tick every frame is a mod, not a probe")
        shipped_frame = strip_comments(function_body(strip_research_blocks(self.plugin),
                                                     "void FrameCallback(FWFrame& FrameContext)"))
        self.assertNotIn("M1pFrameTick", shipped_frame)
        tick = self.body("static void M1pFrameTick()")
        self.assertEqual(tick.strip().split("\n")[0].strip(), "if (!g_M1pTicking) return;")

    # ---- the rows: by name, both routes, the detour counted ------------------------

    def test_research_only_nine_rows_through_hook_one_script_by_their_sdk_constants(self):
        scripts = (SDK_INCLUDE / "scripts.hpp").read_text(encoding="utf-8")
        table = self.code[self.code.index("#define M1P_ROWS(X)"):]
        table = table[:table.index("\n\n")]
        listed = re.findall(r"X\((\w+), (\w+)\)", table)
        self.assertEqual(tuple(listed), ROWS, "the rows are the core's nine, in its order")
        for row, constant in ROWS:
            self.assertIn(f'std::string_view {constant} = "{constant}";', scripts)   # the constant exists
            self.assertIn(row + ",", self.header)                               # a Row enumerator of the core
            short = constant[len("gml_Script_"):]
            self.assertNotIn('"' + constant + '"', self.code)
            self.assertNotIn('"' + short + '"', self.code)
        entry = self.code[self.code.index("#define M1P_ENTRY("):]
        entry = entry[:entry.index("#undef M1P_ENTRY")]
        self.assertIn("SdkShortScriptName(HeroSiege::Scripts::CONSTANT)", entry)
        self.assertIn("HeroSiege::Scripts::CONSTANT.data()", entry)
        # The row order is checked against the core's names at compile time.
        self.assertIn("M1pNs::kRowNames[static_cast<int>(M1pNs::Row::ROW)] == "
                      "std::string_view(SdkShortScriptName(HeroSiege::Scripts::CONSTANT))", self.code)
        self.assertNotIn('"gml_Script_', self.code)

        install = self.body("static void M1pInstall()")
        # Four install sites: M1pInstall's rows, killgate's LoadDrops, the draws watch's
        # count-only ReturnRandomSatanic and vitals' DrawHud (each pinned by its own test below).
        self.assertEqual(self.code.count("HookOneScript("), 4,
                         "four install sites, M1pInstall, M1pKillgateInstall, M1pDrawsInstall and M1pVitalsInstall")
        self.assertEqual(self.body("static void M1pKillgateInstall()").count("HookOneScript("), 1)
        self.assertEqual(self.body("static void M1pDrawsInstall()").count("HookOneScript("), 1)
        self.assertEqual(self.body("static bool M1pVitalsInstall(std::string& why)").count("HookOneScript("), 1)
        self.assertIn("HookOneScript(t.shortName, t.hookId, t.detour, &t.orig, &native)", install)
        # Counted only with the inline detour; a table-only install is failed.
        self.assertIn("native ? M1pNs::HookState::Detoured : M1pNs::HookState::Failed", install)
        for other in ("MmCreateHook", "HookOneScriptTable", "InstallScriptHook"):
            self.assertNotIn(other, self.code)

    def test_research_only_builtin_rows_through_hook_builtin_by_the_cores_names(self):
        table = self.code[self.code.index("#define M1P_BUILTINS(X)"):]
        table = table[:table.index("\n\n")]
        listed = tuple(re.findall(r"X\((\w+)\)", table))
        self.assertEqual(listed, tuple(row for row, _ in BUILTIN_ROWS), "the builtin rows are the core's, in its order")
        # The core names each one, after the nine scripts, by its runtime name.
        names = self.header[self.header.index("kRowNames[kRowCount] = {"):]
        names = names[:names.index("};")]
        quoted = re.findall(r'"(\w+)"', names)
        self.assertEqual(tuple(quoted[len(ROWS):]), tuple(name for _, name in BUILTIN_ROWS))
        # The adapter spells none of them; place_meeting only where `walkable`
        # calls it as a query of its own.
        point = self.body("static M1pNs::WalkPoint M1pWalkPoint(double px, double py, double x, double y)")
        for row, name in BUILTIN_ROWS:
            self.assertIn(row + ",", self.header)
            self.assertEqual(self.code.count('"' + name + '"'), point.count('"' + name + '"'),
                             name + " is spelled in the adapter, not taken from the core")
        install = self.body("static void M1pInstallBuiltins()")
        self.assertEqual(self.code.count("HookBuiltin("), 1, "one builtin install site, in M1pInstallBuiltins")
        self.assertIn("HookBuiltin(name.c_str(), t.hookId, t.detour, &t.orig)", install)
        self.assertIn("const std::string name(M1pNs::RowName(t.row));", install)
        # Held first, then the name, then game code, then the install.
        held = install.index("M1pBuiltinHeldBy(name, holder)")
        lookup = install.index("GetNamedRoutinePointer(name.c_str(), &p)")
        game = install.index("AddrIsExecutableInModule(")
        hook = install.index("HookBuiltin(")
        self.assertLess(held, lookup)
        self.assertLess(lookup, game)
        self.assertLess(game, hook)
        # An unknown name is not-found, never failed; a held one is left alone.
        not_found = install[lookup:game]
        self.assertIn("M1pNs::HookState::NotFound", not_found)
        self.assertNotIn("M1pNs::HookState::Failed", not_found)
        self.assertIn("g_M1pProbe.NoteHook(t.row, M1pNs::HookState::Held, holder);", install[held:lookup])
        self.assertIn("continue;", install[held:lookup])
        holder = self.body("static bool M1pBuiltinHeldBy(std::string_view name, std::string& holder)")
        for check in ("g_JsOrig[i]", "g_JpBuiltinRows[i].installed"):
            self.assertIn(check, holder)
        # M1pInstall runs it, and `hook` lists what it did not find.
        whole = self.body("static void M1pInstall()")
        self.assertIn("M1pInstallBuiltins();", whole)
        self.assertIn("g_M1pProbe.NotFoundLines()", whole)
        self.assertIn('"an earlier hook (table-only)"', self.header)

    def test_research_only_the_builtin_detour_passes_through_and_touches_only_the_local_player(self):
        detour = self.body("static void M1pOnBuiltin(int row, RValue& Result, CInstance* S, CInstance* O, int argc, RValue* Args)")
        self.assertIn("if (g_M1pBusy) { orig(Result, S, O, argc, Args); return; }", detour)
        self.assertIn("const bool local = S && S == g_M1pPlayer;", detour)
        self.assertIn("g_M1pProbe.Around(static_cast<M1pNs::Row>(row), (int64_t)g_RuntimeFrame, local, access, "
                      "[&]() { orig(Result, S, O, argc, Args); });", detour)
        self.assertEqual(detour.count("orig(Result, S, O, argc, Args)"), 2)
        # A lever is refused on a row of either kind that is not installed.
        lever = self.body("static void M1pLever(const M1pNs::Command& c)")
        self.assertIn("if (!M1pRowInstalled(l.row)) {", lever)
        self.assertNotIn("g_M1pRows[static_cast<int>(l.row)]", lever)

    def test_research_only_base_builds_through_the_loader_by_its_sdk_constant_and_places_nothing(self):
        base = self.body("static void M1pBase(const M1pNs::Command& c)")
        spawn = self.body("static void M1pSpawn(const M1pNs::Command& c)")
        # base's one game-script call; spawn's two, the drops' forms' one (heroic and unique share
        # it) and statask's dispatcher call are the only others.
        heroic = self.body("static void M1pDropCalls(const char* form, std::string_view script, const char* scriptName, int count)")
        statask = self.body("static M1pNs::StatAskEntry M1pStatAskOne(CInstance* player, long long id)")
        self.assertEqual(base.count("CallGameScriptEx("), 1)
        self.assertEqual(spawn.count("CallGameScriptEx("), 2)
        self.assertEqual(heroic.count("CallGameScriptEx("), 1)
        self.assertEqual(statask.count("CallGameScriptEx("), 1)
        self.assertEqual(self.code.count("CallGameScriptEx("), 5)
        self.assertIn("g_Yytk->CallGameScriptEx(item, HeroSiege::Scripts::gml_Script_InitItemFromJson, g, g,", base)
        self.assertIn('g_Yytk->CallBuiltinEx(parsed, "json_parse", g, g, { RValue(M1pNs::BaseJson(c.base)) });', base)
        self.assertIn("M1pNs::BaseKey(ms, c.itemClass)", base)
        self.assertLess(base.index('"json_parse"'), base.index("CallGameScriptEx("))
        # It reads the build and writes nothing back.
        for name in MEMBER_WRITES:
            self.assertNotIn(name, base)
        self.assertNotIn("M1pWriteTarget", base)
        self.assertIn('"sprite_exists"', base)
        # The scratch seed is outside every range a ForgePact item dresses.
        self.assertIn("inline constexpr long long kBaseScratchSeed = 779000;", self.header)
        command = self.body("static void M1pCommand(const std::string& rest)")
        self.assertIn("case M1pNs::Form::Base: M1pBase(c); return;", command)

    def test_research_only_spawn_builds_the_set_pieces_and_places_them_at_the_player(self):
        spawn = self.body("static void M1pSpawn(const M1pNs::Command& c)")
        # The loader, by its SDK constant, with the global instance as self,
        # from the piece's own row of Mark1Set.hpp.
        # Since the pieces' rolls (the 2026-10-22 playtest) the seed is the roll
        # word's in the piece's band (M1pNs::SpawnSeed), a `random` roll drawn
        # from TyRng() as the drops draw theirs; was the row's one fixed seed.
        self.assertIn('g_Yytk->CallBuiltinEx(parsed, "json_parse", g, g, { RValue(M1pNs::PieceJson(seed, row.baseB)) });', spawn)
        self.assertIn("const long long seed = M1pNs::SpawnSeed(pieces[i], c.roll, u);", spawn)
        self.assertIn("const double u = std::uniform_real_distribution<double>(0.0, 1.0)(TyRng());", spawn)
        self.assertIn("r.roll = c.roll;", spawn)
        self.assertIn("g_Yytk->CallGameScriptEx(item, HeroSiege::Scripts::gml_Script_InitItemFromJson, g, g,", spawn)
        self.assertIn("M1pNs::BaseKey(stamp, row.itemType)", spawn)
        self.assertIn("M1pNs::SpawnPieces(c.piece)", spawn)
        # Placed at the player, with the player as self (the soul pieces' route).
        self.assertIn("g_Yytk->CallGameScriptEx(res, HeroSiege::Scripts::gml_Script_LootGroundCreateFromItem, g_M1pPlayer,", spawn)
        self.assertLess(spawn.index("gml_Script_InitItemFromJson"), spawn.index("gml_Script_LootGroundCreateFromItem"))
        self.assertEqual(self.code.count("LootGroundCreate"), spawn.count("LootGroundCreate"),
                         "only spawn places anything on the ground")
        # The ground icon goes in first, the way the set's own install does it.
        self.assertLess(spawn.index("Mark1GroundIconInstall();"), spawn.index("gml_Script_LootGroundCreateFromItem"))
        # It writes no member of anything itself: dressing is the game's own
        # CreateItemNew's, through the Custom Forge.
        for name in MEMBER_WRITES:
            self.assertNotIn(name, spawn)
        self.assertNotIn("M1pWriteTarget", spawn)
        self.assertIn("M1pNs::SpawnLine(r)", spawn)
        command = self.body("static void M1pCommand(const std::string& rest)")
        self.assertIn("case M1pNs::Form::Spawn: M1pSpawn(c); return;", command)

    def test_research_only_the_drops_forms_call_the_roll_by_name_and_read_through_the_drop_adapter(self):
        # `heroic`: DropItemHeroic by its SDK constant with the local player as self and its x, y
        # (charmprobe heroic's call shape), what it placed read off the newest ground item, a
        # piece recognised by the shipped Mark1PieceOf, and the core's lines.
        # `unique` (the kill route's) is the same calls on DropUniqueItems, under its own name.
        calls = self.body("static void M1pDropCalls(const char* form, std::string_view script, const char* scriptName, int count)")
        self.assertIn("FcpPlayer(player, self, x, y, why)", calls)
        self.assertIn("g_Yytk->CallGameScriptEx(res, script, self, self, { RValue(x), RValue(y) });", calls)
        self.assertIn("FcpNewestGround(ground, &newest)", calls)
        self.assertIn("r.isPiece = Mark1PieceOf(item, r.piece);", calls)
        self.assertIn("M1pNs::CallLine(form, i, r)", calls)
        self.assertIn("M1pNs::HeroicTally tally(form);", calls)
        self.assertIn("Out(tally.Line());", calls)
        self.assertNotIn('"gml_Script_', calls)
        self.assertIn('M1pDropCalls("heroic", HeroSiege::Scripts::gml_Script_DropItemHeroic, "DropItemHeroic", c.count);',
                      self.body("static void M1pHeroic(const M1pNs::Command& c)"))
        self.assertIn('M1pDropCalls("unique", HeroSiege::Scripts::gml_Script_DropUniqueItems, "DropUniqueItems", c.count);',
                      self.body("static void M1pUnique(const M1pNs::Command& c)"))
        # `pool kill`: the kill route's own reads of all six lists and the core's kill stand-in
        # rule; refused while a roll's entries are on a list; nothing written.
        kill = self.body("static void M1pPoolKill(const M1pNs::Command&)")
        self.assertIn("Mark1KillReadLists(subs, keys, why)", kill)
        self.assertIn("FlightDropOutsideRoll outside;", kill)
        self.assertIn("M1pNs::PoolKill(lists, Mark1DropVerdict)", kill)
        self.assertIn("M1pNs::PoolKillLines(s)", kill)
        self.assertLess(kill.index("g_M1Drop.killPushed > 0"), kill.index("Mark1KillReadLists("))
        for name in MEMBER_WRITES + ("CallGameScript", "ds_list_add", "ds_list_delete"):
            self.assertNotIn(name, kill)
        # `pool`: the drop adapter's own reads (the list by name, every entry, each distinct
        # triple's repository verdict outside a roll), the core's summary; nothing written.
        pool = self.body("static void M1pPool(const M1pNs::Command& c)")
        self.assertIn("Mark1DropReadList(c.count, sub, id, size, why)", pool)
        self.assertIn("FlightDropOutsideRoll outside;", pool)
        self.assertIn("M1pNs::Pool(c.count, Mark1DropEntries(sub, size), Mark1DropVerdict)", pool)
        self.assertIn("M1pNs::PoolLines(s)", pool)
        for name in MEMBER_WRITES + ("CallGameScript",):
            self.assertNotIn(name, pool)
        # `copies`: the research build's copies lever, a plain int there and a constexpr 1 in the
        # player build (so the player build has no writer of it).
        copies = self.body("static void M1pCopies(const M1pNs::Command& c)")
        self.assertIn("g_Mark1DropCopies = c.count;", copies)
        self.assertEqual(self.code.count("g_Mark1DropCopies ="), 1)
        self.assertIn("#ifdef FORGEPACT_RELEASE\nstatic constexpr int g_Mark1DropCopies = M1dNs::kPlayerCopies;\n#else\n"
                      "static int g_Mark1DropCopies = M1dNs::kPlayerCopies;\n#endif", self.plugin)
        command = self.body("static void M1pCommand(const std::string& rest)")
        for case in ("case M1pNs::Form::Heroic: M1pHeroic(c); return;", "case M1pNs::Form::Pool: M1pPool(c); return;",
                     "case M1pNs::Form::Copies: M1pCopies(c); return;", "case M1pNs::Form::Unique: M1pUnique(c); return;",
                     "case M1pNs::Form::PoolKill: M1pPoolKill(c); return;"):
            self.assertIn(case, command)

    def test_research_only_the_probes_own_hold_is_named_for_the_set(self):
        holder = self.body("static std::string M1pScriptHolder(std::string_view runtimeName)")
        self.assertIn("for (const M1pRow& t : g_M1pRows)", holder)
        self.assertIn('return "mark1probe hook";', holder)
        # The set's own detour is a holder this probe's check knows, through JpHolder.
        self.assertIn("Mark1SetHolder(name)", self.body("static std::string SpHolder(std::string_view name, volatile long*& calls)"))

    def test_research_only_the_talent_use_set_speed_rows_refuse_naming_mark1set_while_it_holds_the_script(self):
        # The set's landing detour (the turns) holds TalentUseSetSpeed; the set's
        # holder check names it for that script, and every probe with a row on
        # it asks before it installs: this probe through JpHolder (SpHolder),
        # tgprobe before it blocks, projprobe before its HookOneScript.
        holder = self.body("static std::string Mark1SetHolder(std::string_view runtimeName)")
        self.assertIn("g_M1sOrigTalentUseSetSpeed", holder)
        self.assertIn("HeroSiege::Scripts::gml_Script_TalentUseSetSpeed", holder)
        self.assertIn('return "mark1set";', holder)
        # This probe's TalentUse row (the live session's `casts=`) is not the set's.
        self.assertNotRegex(holder, r"gml_Script_TalentUse\b")
        held = self.body("static bool M1pHeldBy(const M1pRow& t, std::string& holder)")
        self.assertIn("holder = JpHolder(t.runtimeName, calls);", held)
        install = self.body("static void M1pInstall()")
        self.assertLess(install.index("M1pHeldBy(t, holder)"), install.index("HookOneScript("))
        self.assertIn("g_M1pProbe.NoteHook(t.row, M1pNs::HookState::Held, holder);", install)
        self.assertIn("Mark1SetHolder(runtimeName)", self.body("static void TgProbeAttach(TgProbeTarget& t)"))
        attach = self.body("static void ProjProbeAttach(int idx)")
        self.assertLess(attach.index("Mark1SetHolder("), attach.index("HookOneScript("))
        # The set's install, the other way: this probe's hold on the script is a refusal.
        self.assertIn("M1pScriptHolder(name)", self.body("static std::string Mark1LandingResearchHolder()"))

    def test_research_only_pos_samples_with_a_clock_whether_or_not_a_row_is_hooked(self):
        tick = self.body("static void M1pFrameTick()")
        self.assertIn("g_M1pProbe.ObservePosFrame(frame, read, vx.number, vy.number, ms);", tick)
        self.assertIn("std::chrono::steady_clock::now()", tick)
        pos = self.body("static void M1pPos(int64_t frames)")
        # A probe with no row hooked still samples: no early return on it.
        self.assertIn("g_M1pProbe.StartPos(frames, (int64_t)g_RuntimeFrame);", pos)
        self.assertIn("g_M1pTicking = true;", pos)
        self.assertNotIn("return", pos)

    def test_research_only_a_held_row_is_named_and_never_installed_on_top(self):
        install = self.body("static void M1pInstall()")
        self.assertIn("if (t.orig) continue;", install)              # an earlier `hook`'s row stays as it is
        held = install.index("M1pHeldBy(t, holder)")
        self.assertLess(held, install.index("HookOneScript("))
        self.assertIn("g_M1pProbe.NoteHook(t.row, M1pNs::HookState::Held, holder);", install)
        after_held = install[held:install.index("HookOneScript(")]
        self.assertIn("continue;", after_held)
        holder = self.body("static bool M1pHeldBy(const M1pRow& t, std::string& holder)")
        # The existing holder checks name the holder they know.
        for check in ("SpKnownOriginal(", "JumpSceneryHeldHooks()", "JpHolder(", "g_JpRows"):
            self.assertIn(check, holder)
        # A table entry that is not the game's own code is held, whoever took it.
        self.assertIn("AddrIsExecutableInModule(GetModuleHandleA(nullptr)", holder)
        # The core names an unknown holder `an earlier hook (table-only)`.
        self.assertIn('"an earlier hook (table-only)"', self.header)

    def test_research_only_the_detour_passes_through_and_touches_only_the_local_player(self):
        detour = self.body("static RValue& M1pOnScript(int row, CInstance* S, CInstance* O, RValue& R, int argc, RValue** A)")
        self.assertIn("if (g_M1pBusy) return orig(S, O, R, argc, A);", detour)
        self.assertIn("const bool local = S && S == g_M1pPlayer;", detour)
        self.assertIn("g_M1pProbe.Around(static_cast<M1pNs::Row>(row), frame, local, access, [&]() { out = &orig(S, O, R, argc, A); });",
                      detour)
        self.assertEqual(detour.count("orig(S, O, R, argc, A)"), 2)   # the busy pass-through and the core's one call
        refresh = self.body("static void M1pRefreshPlayer()")
        self.assertIn("HhResolveLocalPlayer(p)", refresh)
        self.assertIn("HhResolveInstance(p)", refresh)
        self.assertIn("g_M1pPlayer = inst;", refresh)

    def test_research_only_the_cursor_and_objects_resolve_by_name(self):
        pos = self.body("static void M1pPos(int64_t frames)")
        self.assertIn("JsCursor(x, y, route)", pos)
        objects = (SDK_INCLUDE / "objects.hpp").read_text(encoding="utf-8")
        for name in SDK_OBJECTS:
            self.assertIsNotNone(re.search(r"\b" + name + r" = \d+,", objects))
            self.assertIn("HeroSiege::Objects::GameObject::" + name, self.code)
            self.assertNotIn('"' + name + '"', self.code)
        index = self.body("static int M1pObjectIndex(HeroSiege::Objects::GameObject object)")
        self.assertIn('"asset_get_index"', index)
        self.assertIn("HeroSiege::Objects::GetObjectName(object)", index)
        count = self.body("static void M1pCount(const std::string& object, int64_t frames)")
        self.assertIn('"asset_get_index"', count)
        self.assertIn('"object_exists"', count)
        self.assertLess(count.index("refused"), count.index("StartCount"))   # an unresolved name is refused first

    def test_research_only_no_rva_hex_address_or_module_base_arithmetic(self):
        for source in (self.code, self.header):
            self.assertIsNone(re.search(r"\b\w*Rva\w*\b", source))
            self.assertIsNone(re.search(r"\b0x[0-9A-Fa-f]{5,}\b", source))
            self.assertIsNone(re.search(r"\(\s*char\s*\*\s*\)\s*\w+\s*[+-]", source))
            self.assertNotIn("reinterpret_cast<PFUNC", source)
            self.assertNotIn("exe+", source)
        # The three module handles are the game-code checks: is a script's table
        # entry game code (M1pHeldBy, and M1pTableHeld for the Mark 1 set's
        # landing install), and is a builtin's routine (M1pInstallBuiltins).
        self.assertEqual(self.code.count("GetModuleHandle"), 3)
        self.assertEqual(self.code.count("AddrIsExecutableInModule(GetModuleHandleA(nullptr)"), 3)
        self.assertIn("AddrIsExecutableInModule(GetModuleHandleA(nullptr)", self.body("static bool M1pHeldBy(const M1pRow& t, std::string& holder)"))
        self.assertIn("AddrIsExecutableInModule(GetModuleHandleA(nullptr)", self.body("static bool M1pTableHeld(std::string_view runtimeName)"))
        self.assertIn("AddrIsExecutableInModule(GetModuleHandleA(nullptr), p)", self.body("static void M1pInstallBuiltins()"))

    def test_research_only_walkable_reads_a_meeting_and_a_line_through_their_own_verdicts(self):
        # Live 2 read family=blocked at the player's own position: one shared
        # hit test read place_meeting's answer as an instance id, where a
        # numeric 0 is "no meeting". Each builtin now has its own verdict in
        # the core (test_mark1_probe_behavior.py runs them), and its raw answer
        # is kept beside it for the line.
        self.assertNotIn("M1pHit", self.code)
        point = self.body("static M1pNs::WalkPoint M1pWalkPoint(double px, double py, double x, double y)")
        meeting = point.index('M1pCallRaw(player, "place_meeting"')
        self.assertLess(meeting, point.index("w.family = M1pNs::FamilyFrom(w.familyRaw);"))
        line = point.index('M1pCallRaw(player, "collision_line"')
        self.assertLess(line, point.index("w.wallLine = M1pNs::WallLineFrom(w.wallLineRaw);"))
        self.assertIn("w.grid = M1pNs::GridFrom(w.gridRaw);", point)
        self.assertNotIn("FamilyFrom(w.wallLineRaw)", point)
        self.assertNotIn("WallLineFrom(w.familyRaw)", point)
        # What a blocked family met is looked up only then, on the same family.
        self.assertIn("if (w.family == M1pNs::Family::Blocked) w.met = M1pMet(player, x, y, family);", point)
        met = self.body("static std::string M1pMet(CInstance* player, double x, double y, int family)")
        self.assertIn('"instance_place"', met)
        self.assertIn("M1pNs::MetText(inst, name)", met)
        # The verdicts themselves live in the core, game-independent.
        for verdict in ("inline Family FamilyFrom(const RawResult& r)", "inline WallLine WallLineFrom(const RawResult& r)",
                        "inline Grid GridFrom(const RawResult& r)", "inline std::string RawText(const RawResult& r)"):
            self.assertIn(verdict, self.header)

    # ---- writes ---------------------------------------------------------------------

    def test_research_only_game_state_is_written_only_inside_the_levers(self):
        write = self.body("static bool M1pWriteTarget(CInstance* player, const M1pNs::Target& t, const M1pNs::Value& v)")
        # The one other writer is killgate's chances[45] write (its own test below), reached
        # only through the core's Killgate::Around.
        killgate = self.killgate_ops()
        for name in MEMBER_WRITES:
            self.assertIn('"' + name + '"', write)                               # positive control
            self.assertEqual(self.code.count('"' + name + '"'),
                             write.count('"' + name + '"') + killgate.count('"' + name + '"'),
                             name + " is called outside M1pWriteTarget and killgate's WriteChance")
        # The one game-script call outside `base` and `spawn` is `pool draw`'s repository read,
        # GetUniqueRepoStruct by its SDK constant in M1pDrawVerdict (test_draws_research_only).
        verdict = self.body("static bool M1pDrawVerdict(const M1dNs::Triple& key, M1pNs::DrawVerdict& out)")
        self.assertIn("g_Yytk->CallGameScript(HeroSiege::Scripts::gml_Script_GetUniqueRepoStruct,", verdict)
        for name in NEVER:
            if name == "CallGameScript(":
                self.assertEqual(self.code.count(name), verdict.count(name), name + " is called outside M1pDrawVerdict")
                continue
            self.assertNotIn(name, self.code)
        # M1pWriteTarget is reached from the access the core's Around drives:
        # Write (set and scale, and their restore) and SetPosition (move).
        callers = re.findall(r"M1pWriteTarget\(", self.code)
        self.assertEqual(len(callers), 4)   # the definition, Write, and SetPosition's x and y
        access = self.code[self.code.index("struct M1pAccess {"):]
        access = access[:access.index("\n};")]
        self.assertEqual(access.count("M1pWriteTarget("), 3)
        # The result is replaced in one place, SetResult (stat).
        self.assertEqual(len(re.findall(r"\*\*out\s*=", self.code)), 1)
        self.assertIn("void SetResult(double n)", access)
        self.assertIsNotNone(re.search(r"void SetResult\(double n\)[^}]*\*\*out\s*=\s*RValue\(n\)", access))
        # Nothing outside the access calls its write members: only the core does.
        outside = self.code.replace(access, "")
        for member in (".Write(", ".SetPosition(", ".SetResult("):
            self.assertNotIn(member, outside)
        # The core calls them only for its levers: Write under set/scale,
        # SetPosition under move, SetResult under stat (its behaviour test,
        # test_mark1_probe_behavior.py, runs each lever and the baseline).
        around = self.header[self.header.index("void Around(Row row"):]
        around = around[:around.index("const RowCalls& Calls(")]
        self.assertIn("const bool writes = live && outer && (lever_.kind == LeverKind::Set || lever_.kind == LeverKind::Scale);",
                      around)
        self.assertIn("const bool moves = live && outer && lever_.kind == LeverKind::Move;", around)
        self.assertLess(around.index("if (writes) {"), around.index("access.Write("))
        self.assertLess(around.index("if (moves) {"), around.index("access.SetPosition("))
        self.assertLess(around.index("lever_.kind == LeverKind::Stat"), around.index("access.SetResult("))
        # hold writes before each call under its own lever, and its one
        # restore goes through the core's RestoreHold, which the adapter
        # reaches only through M1pRestoreHold with an M1pAccess.
        self.assertIn("const bool holds = live && outer && lever_.kind == LeverKind::Hold;", around)
        self.assertLess(around.index("if (holds) {"), around.index("access.Write(lever_.target, holdValue_)"))
        restore = self.header[self.header.index("std::string RestoreHold(Access& access, int64_t frame, bool force)"):]
        restore = restore[:restore.index("\n    }\n")]
        self.assertIn("access.Write(holdTarget_, holdStart_)", restore)
        self.assertIn("if (!holdDirty_) return {};", restore)
        adapter_restore = self.body("static void M1pRestoreHold(int64_t frame, bool force)")
        self.assertIn("M1pAccess access{ g_M1pPlayer, 0, nullptr, nullptr };", adapter_restore)
        self.assertIn("g_M1pProbe.RestoreHold(access, frame, force)", adapter_restore)
        self.assertEqual(len(re.findall(r"\.RestoreHold\(", self.code)), 1)
        # statpin answers through the same SetResult as stat, only under one of the two.
        self.assertIn("const bool statOn = LeverLive(frame) && (lever_.kind == LeverKind::Stat || lever_.kind == LeverKind::Pin);",
                      around)
        self.assertIn("access.SetResult(lever_.kind == LeverKind::Stat ? n * lever_.mult : lever_.value.number);", around)
        # An alarm is read through alarm_get and never written.
        write = self.body("static bool M1pWriteTarget(CInstance* player, const M1pNs::Target& t, const M1pNs::Value& v)")
        self.assertIn("if (M1pIsAlarm(t)) return false;", write)

    # ---- killgate (Mod live procedure 6) -------------------------------------------

    def test_killgate_research_only(self):
        # No lever name - command, function or global - is left once the research blocks are
        # stripped (comments kept: not even a mention ships), while the player build keeps
        # LoadDrops' hook and dungeonkey's install (the negative control that the strip
        # leaves player code).
        player_text = strip_research_blocks(self.plugin)
        for word in ("killgate", "Killgate", "M1pKillgate", "g_M1pLoadDropsNative", "kKillgate"):
            self.assertNotIn(word, player_text)
        self.assertIn("static RValue& Hook_LoadDrops(", player_text)
        shipped_hook = strip_comments(function_body(player_text, "static RValue& Hook_LoadDrops("))
        self.assertIn("RValue& res = g_OrigLoadDrops ? g_OrigLoadDrops(S, O, R, argc, A) : R;", shipped_hook)
        shipped_dk = strip_comments(function_body(player_text, "static void DungeonKeyCmd(const std::string& rest)"))
        self.assertIn('HookOneScript("LoadDrops", "fp_loaddrops", (PVOID)Hook_LoadDrops, &g_OrigLoadDrops);', shipped_dk)
        # Not a player command; dispatched through mark1probe's own handler.
        run = function_body(self.plugin, "static void RunCommand(const std::string& line)")
        commands = run[run.index("kPlayerCommands = {"):]
        commands = commands[:commands.index("};")]
        self.assertIn('"menulayout"', commands)
        self.assertNotIn("killgate", commands)
        self.assertNotIn("mark1probe", commands)
        command = self.body("static void M1pCommand(const std::string& rest)")
        self.assertIn("case M1pNs::Form::Killgate:\n    case M1pNs::Form::KillgateSet: M1pKillgate(c); return;", command)
        # The research build's Hook_LoadDrops: only its first call goes through the lever.
        hook = function_body(self.plugin, "static RValue& Hook_LoadDrops(")
        self.assertIn("#ifndef FORGEPACT_RELEASE", hook)
        self.assertEqual(hook.count("M1pKillgateLoadDrops("), 1)
        self.assertLess(hook.index("RValue& res = M1pKillgateLoadDrops(S, O, R, argc, A);\n#else\n"
                                   "    RValue& res = g_OrigLoadDrops ? g_OrigLoadDrops(S, O, R, argc, A) : R;\n#endif"),
                        hook.index("if (!g_DkOn || tip != 11) return res;"))
        # Its install is the inline HookOneScript by the SDK name, never on top of another
        # hook; an existing one is read (dungeonkey's records whether it went in inline, the
        # table-only forms leave that false), and the lever refuses anything but the detour.
        install = self.body("static void M1pKillgateInstall()")
        self.assertIn('HookOneScript(SdkShortScriptName(HeroSiege::Scripts::gml_Script_LoadDrops), "fp_loaddrops",', install)
        self.assertIn("(PVOID)Hook_LoadDrops, &g_OrigLoadDrops, &native);", install)
        self.assertLess(install.index("if (g_OrigLoadDrops || g_M1pKillgateInstallFailed) return;"), install.index("HookOneScript("))
        self.assertIn("g_M1pLoadDropsNative = ok && native;", install)
        self.assertNotIn('"LoadDrops"', install)
        dk = function_body(self.plugin, "static void DungeonKeyCmd(const std::string& rest)")
        self.assertIn('#ifndef FORGEPACT_RELEASE\n        HookOneScript("LoadDrops", "fp_loaddrops", (PVOID)Hook_LoadDrops, '
                      '&g_OrigLoadDrops, &g_M1pLoadDropsNative);\n#else', dk)
        # Its writers: the declaration, the lever's install, and dungeonkey's install alone by address.
        self.assertEqual(self.plugin.count("g_M1pLoadDropsNative ="), 2)
        self.assertEqual(self.plugin.count("&g_M1pLoadDropsNative"), 1)
        self.assertIn("M1pNs::KillgateHookOf(g_M1pKillgateInstallFailed, g_OrigLoadDrops != nullptr, g_M1pLoadDropsNative)",
                      self.body("static M1pNs::KillgateHook M1pKillgateHook()"))
        lever = self.body("static void M1pKillgate(const M1pNs::Command& c)")
        self.assertLess(lever.index("M1pKillgateInstall();"), lever.index("g_M1pKillgate.Set(c.count == 1, M1pKillgateHook())"))
        self.assertIn("if (hook != KillgateHook::Detoured)", self.header)
        # Every decision is the core's: the adapter's one per-call entry is Killgate::Around.
        entry = self.body("static RValue& M1pKillgateLoadDrops(CInstance* S, CInstance* O, RValue& R, int argc, RValue** A)")
        self.assertIn("g_M1pKillgate.Around(type, hasArray, id, (int64_t)g_RuntimeFrame, ops);", entry)
        self.assertEqual(len(re.findall(r"\.Around\(type", self.code)), 1)
        # It never names DropUniqueItems as a call target and calls no die of its own: the
        # re-entry is the trampoline at type 45, and the only count it reads is the shared
        # detour's kill-calls.
        ops = self.killgate_ops()
        self.assertIn("A2[2] = &type;", ops)
        self.assertIn("g_OrigLoadDrops(S, O, r2, argc, A2.data());", ops)
        self.assertIn("uint64_t UniqueCalls() { return g_M1Drop.core.KillCalls(); }", ops)
        self.assertEqual(ops.count('"array_set"'), 1)
        start = self.code.index("static M1pNs::Killgate g_M1pKillgate;")
        end = self.code.index("static void M1pKillgate(const M1pNs::Command& c)")
        adapter = self.code[start:end] + lever
        core = self.header[self.header.index("inline constexpr int kKillgateType"):]
        core = core[:core.index("inline constexpr int kGridCell")]
        for source in (adapter, core):
            for word in ("DropUniqueItems", "CallGameScript", "irandom", "random", "rand(", "KendiZar", "MmCreateHook",
                         "HookOneScriptTable"):
                self.assertNotIn(word, source)
        self.assertIn("kKillgateOpenChance", core)   # the slice is the core, not an empty string
        self.assertIn("ops.WriteChance(vanilla);", core)

    # ---- draws (Mod live procedure 7) ----------------------------------------------

    def test_draws_research_only(self):
        # Nothing of the watch, its lever or `pool draw` is left once the research blocks are
        # stripped (comments kept: not even a mention ships), while the player build keeps the
        # functions the call-outs sit in (the negative control that the strip leaves player code).
        player_text = strip_research_blocks(self.plugin)
        for word in ("M1pDraws", "M1pPoolDraw", "M1pDrawVerdict", "DrawPushBook", "draws push", "pool draw"):
            self.assertNotIn(word, player_text)
        callouts = (
            ("static RValue& Hook_GetUniqueRepoStruct(", "M1pDrawsNoteRead(argc, A);"),
            ("static RValue& Hook_CreateDefaultParams(", "M1pDrawsNotePick(argc, A);"),
            ("static RValue& HookSatanicTier(", "M1pDrawsNoteTier(r);"),
            ("static RValue& HookAngelicChance(", "M1pDrawsRollGuard m1pDrawsAngelic(M1pDrawsEnterAngelic());"),
            ("static void CustomForgePostProcess(", "if (finalPass) M1pDrawsNoteBuilt(result, argc, args);"),
            ("static void ApRollAttach(ApRollRow& r)", 'std::string_view(r.id) == "drop-item" && M1pDrawsHoldsDropItem()'),
            ("static void GpInstallScripts(HMODULE mainMod)", "&t == &g_GpScriptRows[kGpScript_DropItem] && M1pDrawsHoldsDropItem()"),
        )
        for signature, call in callouts:
            with self.subTest(function=signature):
                full = strip_comments(function_body(self.plugin, signature))
                self.assertEqual(full.count(call), 1, call)
                if signature not in player_text:
                    continue   # the whole function is research build only (angelicprobe's, gambaprobe's)
                at = full.index(call)
                self.assertGreater(full.rfind("#ifndef FORGEPACT_RELEASE", 0, at), full.rfind("#endif", 0, at),
                                   "the call-out sits inside a research block")
        for signature in ("static RValue& Hook_GetUniqueRepoStruct(", "static RValue& Hook_CreateDefaultParams(",
                          "static RValue& HookSatanicTier(", "static void CustomForgePostProcess("):
            self.assertNotIn("M1p", function_body(player_text, signature))
        # The tier's call-out reads what LoadSatanicDropTier answered, after the lift's call.
        tier = strip_comments(function_body(self.plugin, "static RValue& HookSatanicTier("))
        self.assertLess(tier.index("RValue& r = SatanicTierLift(S, O, R, argc, A);"), tier.index("M1pDrawsNoteTier(r);"))
        # The CreateDefaultParams call-out sees the pick before the game's call; the read is the
        # first thing GetUniqueRepoStruct's detour does.
        cdp = strip_comments(function_body(self.plugin, "static RValue& Hook_CreateDefaultParams("))
        self.assertLess(cdp.index("M1pDrawsNotePick(argc, A);"), cdp.index("g_Orig_CreateDefaultParams(S, O, R, argc, A)"))
        # DropManager.hpp is untouched by the watch: its one DropItem detour is spliced from here.
        drop_manager = (ROOT / "plugin" / "include" / "ForgePact" / "DropManager.hpp").read_text(encoding="utf-8")
        self.assertNotIn("M1p", drop_manager)
        self.assertNotIn("mark1probe", drop_manager)
        # Never a second hook on DropItem: the watch splices DropManager's saved original
        # (GpDropItemHolder, gambaprobe's splice), a trampoline only, and refuses while
        # angelicprobe's drop-item row or gambaprobe's DropItem row holds the slot.
        install = self.body("static void M1pDrawsInstall()")
        self.assertIn("!g_ApRollRows[kApRowDropItem].tramp && !g_GpScriptRows[kGpScript_DropItem].orig", install)
        self.assertIn("PFUNC_YYGMLScript* slot = GpDropItemHolder();", install)
        self.assertIn("M1pDrawsSplice(slot, g_M1pDrawsDropItemOrig, M1pDrawsDropItem);", install)
        self.assertLess(install.index("M1pDrawsSplice(slot,"), install.index("if (!g_M1pDrawsDropItemOrig) return;"))
        self.assertLess(install.index("if (!g_M1pDrawsDropItemOrig) return;"), install.index("FlightDropInstallUnique();"))
        self.assertIn("M1pDrawsSplice(&g_Orig_FlightDropUnique, g_M1pDrawsUniqueOrig, M1pDrawsUnique);", install)
        self.assertIn("InstallSatanicTierHook();", install)
        self.assertIn('HookOneScript(SdkShortScriptName(HeroSiege::Scripts::gml_Script_ReturnRandomSatanic), "fp_m1p_retsat",', install)
        splice = self.body("static bool M1pDrawsSplice(PFUNC_YYGMLScript* slot, PFUNC_YYGMLScript& saved, PFUNC_YYGMLScript detour)")
        self.assertIn("if (!slot || !GpHolderIsNative((const void*)*slot, why)) return false;", splice)
        self.assertLess(splice.index("saved = *slot;"), splice.index("*slot = detour;"))
        for name in ("gml_Script_DropItem", "gml_Script_DropUniqueItems", "gml_Script_DropItemHeroic",
                     "gml_Script_LoadSatanicDropTier", "gml_Script_GetUniqueRepoStruct", "gml_Script_CreateDefaultParams"):
            self.assertIsNone(re.search(r"HookOneScript\([^;]*" + name, self.code), name + " is hooked a second time")
        route = self.body("static std::string M1pDrawsDropItemRoute()")
        for held in ('"held-by-angelicprobe"', '"held-by-gambaprobe"', '"TABLE-ONLY"', '"not-installed"'):
            self.assertIn(held, route)
        self.assertIn("if (on && r.dropitem != kDrawRouteDetoured)", self.header)
        # With the watch off the spliced DropItem is the trampoline's call and nothing else.
        dropitem = self.body("static RValue& M1pDrawsDropItem(CInstance* S, CInstance* O, RValue& R, int argc, RValue** A)")
        self.assertIn("if (!g_M1pDraws.On()) return g_M1pDrawsDropItemOrig(S, O, R, argc, A);", dropitem)
        self.assertEqual(dropitem.count("g_M1pDrawsDropItemOrig(S, O, R, argc, A)"), 2)
        # The lever writes only through the drop adapter's own add and delete, from the core's
        # book, and refuses while Mark 1 drops or Pocket Elf is switched on.
        ops = self.code[self.code.index("struct M1pDrawsListOps {"):]
        ops = ops[:ops.index("\n};")]
        self.assertIn("Mark1DropAddEntry(subs[k], t);", ops)
        self.assertIn("Mark1DropDeleteLast(subs[k], len, n);", ops)
        self.assertIn("Mark1ListTailHolds(subs[k], before, entries, on, len);", ops)
        self.assertEqual(self.code.count("Mark1DropAddEntry("), 1)
        self.assertEqual(self.code.count("Mark1DropDeleteLast("), 1)
        self.assertIn("g_M1pDrawsBook.Push(M1pNs::DrawPushPlan(*s, g_M1pDraws.Push()), g_M1pDrawsOps, lines)",
                      self.body("static bool M1pDrawsPush()"))
        # The lever's stand-ins are `pool draw`'s candidates (an entry the draw keeps as read),
        # never `pool kill`'s DropUniqueItems choice, and its reply is the core's line.
        standins = self.body("static const M1pNs::DrawLeverStandIns* M1pDrawsStandIns(RValue subs[], const M1dNs::KillListKeys& keys, "
                             "M1pNs::DrawSide side)")
        self.assertIn("M1pNs::DrawLeverPick(M1pNs::PoolDraw(lists, M1pDrawVerdict), side)", standins)
        self.assertIn("FlightDropOutsideRoll outside;", standins)
        self.assertNotIn("ResolveKillStandIns", standins)
        self.assertIn("static M1pNs::DrawLeverCache g_M1pDrawsStandIns;", self.code)
        self.assertIn("Out(M1pNs::DrawLeverLine(*s, g_M1pDraws.On(), g_M1pDraws.Side()));",
                      self.body("static void M1pDrawsPushCommand(const M1pNs::Command& c)"))
        call = self.code[self.code.index("struct M1pDrawsCall {"):]
        call = call[:call.index("\n};")]
        self.assertIn("g_M1pDraws.ShouldPush(g_M1Drop.core.Enabled(), M1pDrawsElfOn())", call)
        self.assertLess(call.index("if (pushed) { try { M1pDrawsRemove(); } catch (...) {} }"), call.index("if (counted) g_M1pDraws.EndCall();"))
        self.assertIn("g_M1pDraws.SetPush(c.count, g_M1Drop.core.Enabled(), M1pDrawsElfOn(), c.side)",
                      self.body("static void M1pDrawsPushCommand(const M1pNs::Command& c)"))
        self.assertIn("if (copies > 0 && (mark1On || elfOn))", self.header)
        # `pool draw` reads GetUniqueRepoStruct by its SDK constant outside a roll and is
        # refused, as `pool kill` is, while a roll's entries are on a list.
        pool = self.body("static void M1pPoolDraw(const M1pNs::Command&)")
        self.assertIn("FlightDropOutsideRoll outside;", pool)
        self.assertIn("M1pDrawsPushing()", pool)
        self.assertIn("M1pDrawsPushing()", self.body("static void M1pPoolKill(const M1pNs::Command&)"))
        # Dispatched through mark1probe's own handler, and not a player command.
        command = self.body("static void M1pCommand(const std::string& rest)")
        self.assertIn("case M1pNs::Form::Draws:\n    case M1pNs::Form::DrawsSet: M1pDraws(c); return;", command)
        self.assertIn("case M1pNs::Form::DrawsPush: M1pDrawsPushCommand(c); return;", command)
        self.assertIn("case M1pNs::Form::PoolDraw: M1pPoolDraw(c); return;", command)
        run = function_body(self.plugin, "static void RunCommand(const std::string& line)")
        commands = run[run.index("kPlayerCommands = {"):]
        commands = commands[:commands.index("};")]
        self.assertNotIn('"draws"', commands)

    def test_draws_push_skip_counted_on_every_silent_path(self):
        # Live 1b's push-held=/push-skipped= rest on the adapter: M1pDrawsPush returns true only
        # after a Held push the core noted (NotePushed, which counts push-held), and false on
        # every other way out - the lists unread, no stand-ins, nothing planned (None), and a
        # push taken off again (NotHeld, Threw) - and M1pDrawsCall notes one skip whenever no
        # Held push followed ShouldPush, a throw it swallows included.
        push = self.body("static bool M1pDrawsPush()")
        self.assertIn("if (!Mark1KillReadLists(subs, keys, why)) return false;", push)
        self.assertIn("if (!s) return false;", push)
        self.assertIn("if (result == M1pNs::DrawPushResult::None) return false;", push)
        self.assertEqual(push.count("return true;"), 1)
        self.assertEqual(push.count("return false;"), 4, "the list read, the stand-ins, None, and NotHeld/Threw")
        self.assertNotRegex(push, r"return\s*;", "no way out of the push that says neither held nor skipped")
        held = push[push.index("if (result == M1pNs::DrawPushResult::Held) {"):]
        held = held[:held.index("}")]
        self.assertLess(held.index("g_M1pDraws.NotePushed(g_M1pDrawsBook.Entries());"), held.index("return true;"))
        call = self.code[self.code.index("struct M1pDrawsCall {"):]
        call = call[:call.index("\n};")]
        self.assertIn("bool held = false;", call)
        self.assertIn("try { held = M1pDrawsPush(); } catch (...) { held = false; }", call)
        self.assertIn("if (!held) g_M1pDraws.NotePushSkipped();", call)
        self.assertLess(call.index("g_M1pDraws.ShouldPush(g_M1Drop.core.Enabled(), M1pDrawsElfOn())"),
                        call.index("try { held = M1pDrawsPush(); }"))
        self.assertLess(call.index("try { held = M1pDrawsPush(); }"), call.index("if (!held) g_M1pDraws.NotePushSkipped();"))
        self.assertEqual(self.code.count("NotePushSkipped("), 1, "one place notes a skip, so a call is never counted twice")
        self.assertEqual(self.code.count("M1pDrawsPush()"), 2, "the definition and M1pDrawsCall's one call")
        # The core takes the notes: push-held in NotePushed, push-skipped in NotePushSkipped,
        # pushed-reads in NoteRead, all printed after pick-builds=; the side reaches the pick.
        self.assertIn("++counts_.pushHeld;", self.header)
        self.assertIn("++counts_.pushSkipped;", self.header)
        self.assertIn("++counts_.pushedReads;", self.header)
        self.assertIn('" pick-builds=" + n(c.pickBuilds) + " push-held=" + n(c.pushHeld) + " push-skipped=" + n(c.pushSkipped)', self.header)
        self.assertIn("M1pDrawsStandIns(subs, keys, g_M1pDraws.Side())", push)
        # All of it is research build only: nothing of the counters or the side ships.
        player_text = strip_research_blocks(self.plugin)
        # (NotePushed itself is not listed: the player build's own drop cores carry the name.)
        for word in ("NotePushSkipped", "M1pDrawsCall", "DrawSide", "push-skipped", "pushed-reads", "push-held"):
            self.assertNotIn(word, player_text)
        for name in ("struct M1pDrawsCall {", "static bool M1pDrawsPush()", "static void M1pDrawsPushCommand(const M1pNs::Command& c)"):
            self.assertIn(name, self.block)

    def test_satanic_tier_one_detour(self):
        # LoadSatanicDropTier has one detour in the whole source, raredrop's HookSatanicTier,
        # installed by one function that reads the route back (the five-argument HookOneScript)
        # and that raredrop satanic and the draws watch both call.
        whole = strip_comments(self.plugin)
        installs = re.findall(r"HookOneScript(?:Table)?\([^;]*LoadSatanicDropTier[^;]*;", whole)
        self.assertEqual(len(installs), 1, installs)
        self.assertIn('HookOneScript(SdkShortScriptName(HeroSiege::Scripts::gml_Script_LoadSatanicDropTier), "fp_sattier",', installs[0])
        self.assertIn("(PVOID)HookSatanicTier, &g_OrigSatTier, &native);", installs[0])
        self.assertNotIn('"LoadSatanicDropTier"', re.sub(r"static const char\* kNames\[\] = \{.*?\};", "", whole, flags=re.S))
        self.assertEqual(whole.count("&g_OrigSatTier"), 1)
        for other in ("MmCreateHook", "InstallScriptHook"):
            self.assertIsNone(re.search(other + r"\([^;]*SatanicTier", whole))
        install = strip_comments(function_body(self.plugin, "static bool InstallSatanicTierHook()"))
        self.assertIn('g_SatTierRoute = !ok ? "not-found" : native ? "detoured" : "TABLE-ONLY";', install)
        self.assertLess(install.index("if (!g_SatTierTried && !g_OrigSatTier) {"), install.index("HookOneScript("))
        # Reached from both: raredrop satanic (player code) and the watch (research only).
        raredrop = strip_comments(function_body(self.plugin, "static void RareDropCmd(const std::string& rest)"))
        self.assertIn("if (mult > 1.0 && !InstallSatanicTierHook()) {", raredrop)
        self.assertIn("InstallSatanicTierHook();", self.body("static void M1pDrawsInstall()"))
        # The kill draw's install (#166, the Mark 1 drops' and Pocket Elf's) is the third caller, in
        # both builds: the draw's tier is this detour's call-out.
        self.assertIn("InstallSatanicTierHook();", strip_comments(function_body(strip_research_blocks(self.plugin),
                                                                                  "static std::string KillDrawInstall()")))
        self.assertEqual(len(re.findall(r"\bInstallSatanicTierHook\(\)", whole)), 4)   # the definition and the three callers
        self.assertIn("InstallSatanicTierHook()", strip_comments(function_body(strip_research_blocks(self.plugin),
                                                                                 "static void RareDropCmd(const std::string& rest)")))
        # raredrop's lift is as it was, inside the one detour; the watch's route is read from it.
        lift = strip_comments(function_body(self.plugin, "static RValue& SatanicTierLift("))
        for step in ("double lifted = level * g_SatanicMult;", "if (lifted > kSatanicTopBracket) lifted = kSatanicTopBracket;",
                     "args[0] = &liftedRV;", "BP_DIAG_INCREMENT(g_SatTierHits);",
                     "return g_OrigSatTier ? g_OrigSatTier(S, O, R, argc, A) : R;"):
            self.assertIn(step, lift)
        self.assertIn("r.sattier = g_SatTierRoute;", self.body("static M1pNs::DrawRoutes M1pDrawsRoutes()"))

    def test_statask_manaregen_procgaps_are_research_only(self):
        # The replan's three forms (Live procedure 2). Each is the probe's: its
        # handler sits in the research block, and nothing of it ships.
        for name in ("static void M1pStatAsk(", "static M1pNs::StatAskEntry M1pStatAskOne(", "static void M1pManaRegen(",
                     "static void M1pProcGaps(", "static void M1pNoteProc(", "static M1pNs::ProcGaps g_M1pProcGaps;"):
            with self.subTest(name=name):
                self.assertIn(name, self.code)
                self.assertNotIn(name, self.shipped)
        command = self.body("static void M1pCommand(const std::string& rest)")
        for case in ("case M1pNs::Form::StatAsk: M1pStatAsk(c); return;", "case M1pNs::Form::ManaRegen: M1pManaRegen(c); return;",
                     "case M1pNs::Form::ProcGapsReset: M1pProcGaps(c); return;",
                     "case M1pNs::Form::BonusOthersReset: M1pBonusOthers(c); return;"):
            self.assertIn(case, command)
        # statask names the dispatcher by its SDK constant, self and other the
        # local player, in Mark1ReadStatByName's argument shape, and NOT under
        # the adapter's busy flag (the set's consumer answers it as a game call).
        ask = self.body("static M1pNs::StatAskEntry M1pStatAskOne(CInstance* player, long long id)")
        self.assertIn("CallGameScriptEx(result, HeroSiege::Scripts::gml_Script_ReturnSpecificStat.data(),", ask)
        self.assertIn("player, player, { RValue(1.0), RValue((double)id), RValue(0.0), RValue(), RValue() }", ask)
        self.assertIn('"array_get", { result, RValue(0.0) }', ask)
        self.assertNotIn('"ReturnSpecificStat"', ask)
        statask = self.body("static void M1pStatAsk(const M1pNs::Command& c)")
        for body in (ask, statask):
            self.assertNotIn("g_M1sBusy", body)
            self.assertNotIn("M1pBusyScope", body)
        self.assertLess(statask.index("if (!player)"), statask.index("M1pStatAskOne("))
        self.assertIn("refused - no local player", statask)
        # The id-42 override: research build only, inside the set's consumer,
        # before the switch check and before the cost.
        consumer = function_body(self.plugin, "static void Mark1SetBonusOnStat(CInstance* S, int argc, RValue** A, RValue& r)")
        shipped_consumer = function_body(strip_research_blocks(self.plugin),
                                         "static void Mark1SetBonusOnStat(CInstance* S, int argc, RValue** A, RValue& r)")
        # The local player's calls only (replan 4: the talent screen is answered too, not forced).
        override = "if (local && g_M1bRegenOn && id == ForgePact::kMark1ReplenishStatId) {"
        self.assertIn(override, consumer)
        self.assertIn("r = RValue(g_M1bRegenValue);", consumer)
        self.assertIn("++g_M1bRegenForced;", consumer)
        self.assertNotIn("g_M1bRegen", shipped_consumer)
        self.assertNotIn("g_M1bRegen", strip_research_blocks(self.plugin))
        self.assertIn("if (!g_Mark1.Enabled() || (!bonusWants && !costWants)) return;", shipped_consumer)   # control
        self.assertLess(consumer.index(override), consumer.index("if (!g_Mark1.Enabled()"))
        self.assertLess(consumer.index(override), consumer.index("g_M1aCost.Answer("))
        # Off on load.
        self.assertIn("static bool g_M1bRegenOn = false;", self.plugin)
        # procgaps: fed from the gate detour around Mark1FireProc, only for a
        # proc the core counted as returned; the recording is research only.
        detour = function_body(self.plugin, "static RValue& Mark1HookCheckTalentUse(CInstance* S, CInstance* O, RValue& R, int argc, RValue** A)")
        self.assertIn("M1pNoteProc(plan.procColumn, plan.clock.nowMs,", detour)
        self.assertLess(detour.index("Mark1FireProc(plan)"), detour.index("M1pNoteProc("))
        self.assertNotIn("M1pNoteProc", function_body(strip_research_blocks(self.plugin),
                         "static RValue& Mark1HookCheckTalentUse(CInstance* S, CInstance* O, RValue& R, int argc, RValue** A)"))
        # The line is the core's pure function (tests/mark1_probe_harness.cpp runs it).
        self.assertIn("Out(g_M1pProcGaps.Line());", self.body("static void M1pProcGaps(const M1pNs::Command& c)"))
        self.assertIn("class ProcGaps {", self.header)

    def test_vitals_is_research_only(self):
        # Replan 4's `vitals`: the DrawHud hook, the scope flag and the
        # recorder are the probe's, and nothing of them reaches the player build.
        for name in ("static M1pNs::VitalsRecorder g_M1pVitals;", "static M1pNs::VitalsWindow   g_M1pVitalsWindow;",
                     "static RValue& M1pVitalsHud(", "struct M1pVitalsHudScope {", "static bool M1pVitalsInstall(",
                     "static M1pNs::VitalsReading M1pVitalsRead()", "static void M1pVitals(", "static void M1pVitalsOffer("):
            with self.subTest(name=name):
                self.assertIn(name, self.code)
                self.assertNotIn(name, self.shipped)
        player_text = strip_research_blocks(self.plugin)
        for word in ("g_M1pVitalsInHud", "M1pVitalsOffer", "VitalsRecorder", "fp_m1p_vitals_hud"):
            self.assertIn(word, self.plugin)
            self.assertNotIn(word, player_text)
        # The scope flag is declared, and read by the text detours, only under the guard.
        self.assertIn("#ifndef FORGEPACT_RELEASE\n// `mark1probe vitals`", self.plugin)
        self.assertIn("static int g_M1pVitalsInHud = 0;", self.plugin)
        detour = self.plugin[self.plugin.index("static void Hook_TipBuiltinText("):]
        detour = detour[:detour.index("\n}\n") + 2]
        self.assertIn("#ifndef FORGEPACT_RELEASE\n    if (g_M1pVitalsInHud > 0) M1pVitalsOffer(argc, Args);\n#endif\n", detour)
        shipped_detour = player_text[player_text.index("static void Hook_TipBuiltinText("):]
        shipped_detour = shipped_detour[:shipped_detour.index("\n}\n") + 2]
        self.assertIn("TipNoteText(kTipBuiltinText[N], argc, Args);", shipped_detour)   # control: the detour itself ships
        self.assertNotIn("Vitals", shipped_detour)
        # Only the DrawHud hook's scope sets the flag, and it ends on a throw too.
        self.assertEqual(len(re.findall(r"\+\+g_M1pVitalsInHud", self.plugin)), 1)
        self.assertEqual(len(re.findall(r"--g_M1pVitalsInHud", self.plugin)), 1)
        scope = self.code[self.code.index("struct M1pVitalsHudScope {"):]
        scope = scope[:scope.index("\n};")]
        self.assertIn("~M1pVitalsHudScope() { g_M1pVitals.End(); --g_M1pVitalsInHud; }", scope)
        hud = self.body("static RValue& M1pVitalsHud(CInstance* S, CInstance* O, RValue& R, int argc, RValue** A)")
        self.assertLess(hud.index("M1pVitalsHudScope scope;"), hud.index("g_M1pVitalsOrigHud(S, O, R, argc, A)"))
        # DrawHud by its SDK constant, both routes, never by a literal; a held
        # DrawHud is left to its holder; the text detours through their installer.
        install = self.body("static bool M1pVitalsInstall(std::string& why)")
        self.assertIn("HookOneScript(SdkShortScriptName(HeroSiege::Scripts::gml_Script_DrawHud), \"fp_m1p_vitals_hud\","
                      " (PVOID)M1pVitalsHud,", install)
        self.assertNotIn('"DrawHud"', install)
        self.assertNotIn('"gml_Script_DrawHud"', self.plugin)
        self.assertIn("InstallTipBuiltinTextHooks(std::make_index_sequence<kTipBuiltinTextCount>{});", install)
        self.assertLess(install.index("TgProbeSpriteHudRowAttached()"), install.index("HookOneScript("))
        self.assertLess(install.index("M1pTableHeld(HeroSiege::Scripts::gml_Script_DrawHud)"), install.index("HookOneScript("))
        # 36 and 37 are asked as statask asks them, and matched in the core.
        read = self.body("static M1pNs::VitalsReading M1pVitalsRead()")
        self.assertIn("M1pStatAskOne(player, M1pNs::kVitalsLifeStatId)", read)
        self.assertIn("M1pStatAskOne(player, M1pNs::kVitalsManaStatId)", read)
        self.assertIn("M1pNs::ReadVitals(g_M1pVitals.Last(),", read)
        self.assertIn("inline constexpr long long kVitalsLifeStatId = 36;", self.header)
        self.assertIn("inline constexpr long long kVitalsManaStatId = 37;", self.header)
        # The command, and the window's end line from the frame tick.
        self.assertIn("case M1pNs::Form::Vitals: M1pVitals(c); return;", self.body("static void M1pCommand(const std::string& rest)"))
        tick = self.body("static void M1pFrameTick()")
        self.assertIn("if (g_M1pVitalsWindow.Due(frame, g_M1pVitals.Last())) Out(g_M1pVitalsWindow.Finish(M1pVitalsRead()));", tick)
        self.assertIn("g_M1pTicking = true;", self.body("static void M1pVitals(const M1pNs::Command& c)"))

    def test_research_only_the_profile_scope_says_which_it_read(self):
        resolve = self.body("static bool M1pResolveScope(CInstance* player, M1pNs::Scope scope, M1pScopeRef& out, std::string& why)")
        self.assertIn('"playerProfileSlot"', resolve)
        self.assertIn('"pSt"', resolve)
        self.assertIn("HeroSiege::Objects::GameObject::Profile_Manager_obj", resolve)
        self.assertIn('"instance_find"', resolve)
        # profile and input read the gate's own source: the entry of
        # Input_Device_Manager_obj's playerProfileSlot at CheckTalentUse's
        # third argument, and no stand-in when that route is unresolved.
        self.assertIn("HeroSiege::Objects::GameObject::Input_Device_Manager_obj", resolve)
        self.assertIn("g_M1pSlotArg", resolve)
        self.assertNotIn("Profile_Manager_obj#0", resolve)
        note = self.body("static void M1pNoteLocalCall(int row, int64_t frame, int argc, RValue** A)")
        self.assertIn("g_M1pSlotArg = A[2]->ToDouble();", note)
        # queue: abilityQueue and mobileTalentPressed are globals, never
        # written wholesale; any other member name is refused.
        member = self.body("static bool M1pResolveMember(CInstance* player, M1pNs::Scope scope, const std::string& name, M1pScopeRef& out, std::string& why)")
        self.assertIn('"abilityQueue"', member)
        self.assertIn('"mobileTalentPressed"', member)
        self.assertIn('"stackSkillList"', member)
        write = self.body("static bool M1pWriteTarget(CInstance* player, const M1pNs::Target& t, const M1pNs::Value& v)")
        self.assertIsNotNone(re.search(r"else if \(s\.isGlobal\)\s*\{\s*return false;", write))
        # A write counts as applied only when it reads back as written.
        self.assertIn("return M1pReadTarget(player, t, back) && back.number == v.number;", write)
        # A struct is told apart from an instance before an instance is assumed.
        as_scope = self.body("static bool M1pAsScope(const RValue& v, const std::string& label, M1pScopeRef& out, std::string& why)")
        self.assertLess(as_scope.index('"is_struct"'), as_scope.index('"instance_exists"'))
        self.assertIn("HeroSiege::Player::IsInstanceHandle(v)", as_scope)


if __name__ == "__main__":
    unittest.main()
