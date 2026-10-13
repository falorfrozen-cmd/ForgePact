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
SDK_OBJECTS = ("Profile_Manager_obj", "Collision_Parent_obj", "Wall_Parent_obj")

# Builtins and calls that change game state. The adapter may use the first
# three only inside M1pWriteTarget; the rest never.
MEMBER_WRITES = ("variable_instance_set", "variable_struct_set", "array_set")
NEVER = ("variable_global_set", "instance_create", "instance_destroy", "instance_deactivate", "instance_activate",
         "ds_list_add", "ds_list_set", "ds_list_delete", "ds_map_set", "ds_map_add", "ds_map_delete",
         "mp_grid_add", "mp_grid_clear", "tilemap_set", "layer_set", "CallGameScript", "keyboard_key_press",
         "io_clear", "game_set_speed")


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

    # ---- research build only ----------------------------------------------------

    def test_research_only_every_m1p_symbol_and_the_verb_vanish_from_the_player_build(self):
        symbols = set(re.findall(M1P_SYMBOL, self.code))
        # A scan that finds nothing would pass vacuously.
        for expected in ("M1pNs", "M1pCommand", "M1pInstall", "M1pHeldBy", "M1pOnScript", "M1pAccess", "M1pFrameTick",
                         "M1pRefreshPlayer", "M1pResolveScope", "M1pReadTarget", "M1pWriteTarget", "M1pSnapshot",
                         "M1pWalkable", "M1pCount", "M1pPos", "g_M1pRows", "g_M1pProbe", "g_M1pPlayer"):
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
        self.assertEqual(self.code.count("HookOneScript("), 1, "one install site, in M1pInstall")
        self.assertIn("HookOneScript(t.shortName, t.hookId, t.detour, &t.orig, &native)", install)
        # Counted only with the inline detour; a table-only install is failed.
        self.assertIn("native ? M1pNs::HookState::Detoured : M1pNs::HookState::Failed", install)
        for other in ("MmCreateHook", "HookOneScriptTable", "InstallScriptHook"):
            self.assertNotIn(other, self.code)

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
        # The one module handle is the held check's: is the table entry game code.
        self.assertEqual(self.code.count("GetModuleHandle"), 1)
        self.assertIn("AddrIsExecutableInModule(GetModuleHandleA(nullptr)", self.code)

    # ---- writes ---------------------------------------------------------------------

    def test_research_only_game_state_is_written_only_inside_the_levers(self):
        write = self.body("static bool M1pWriteTarget(CInstance* player, const M1pNs::Target& t, const M1pNs::Value& v)")
        for name in MEMBER_WRITES:
            self.assertIn('"' + name + '"', write)                               # positive control
            self.assertEqual(self.code.count('"' + name + '"'), write.count('"' + name + '"'),
                             name + " is called outside M1pWriteTarget")
        for name in NEVER:
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

    def test_research_only_the_profile_scope_says_which_it_read(self):
        resolve = self.body("static bool M1pResolveScope(CInstance* player, M1pNs::Scope scope, M1pScopeRef& out, std::string& why)")
        self.assertIn('"playerProfileSlot"', resolve)
        self.assertIn('"pSt"', resolve)
        self.assertIn("HeroSiege::Objects::GameObject::Profile_Manager_obj", resolve)
        self.assertIn('"instance_find"', resolve)
        # A struct is told apart from an instance before an instance is assumed.
        as_scope = self.body("static bool M1pAsScope(const RValue& v, const std::string& label, M1pScopeRef& out, std::string& why)")
        self.assertLess(as_scope.index('"is_struct"'), as_scope.index('"instance_exists"'))
        self.assertIn("HeroSiege::Player::IsInstanceHandle(v)", as_scope)


if __name__ == "__main__":
    unittest.main()
