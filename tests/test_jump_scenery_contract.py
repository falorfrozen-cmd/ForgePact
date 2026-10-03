"""Contract tests for the jump-through-scenery research (ForgePact #16).

How the game decides that the player's universal jump is blocked by scenery,
and whether answering "no collision" to the player's own collision queries
while airborne lets the jump cross a prop, is measured in one research launch
(docs/jump-scenery-research.md) before any player command is written against
it. This phase ships one research-build instrument for that launch,
`jumpprobe`, and these tests pin it on comment-stripped source:

- it is research build only: the word `jumpprobe`, its header and every `Jp*`
  symbol vanish from what the player build compiles, it is not a player
  command, and it is dispatched from its own handler as a standalone early
  return straight after `HandleSkillProbeCommand` (RunCommand's else-if chain
  is at MSVC's C1061 limit);
- the only piece on the frame path is one tick, which returns at once while
  nothing is armed, traced or on;
- every script row is an hs-game-sdk constant (the jump scripts derived from
  scripts.hpp, not from a hand list), every detour sits behind
  AddrIsExecutableInModule, and `hook` refuses while citrace holds a spatial
  builtin, since a builtin detours once;
- the lever writes a result only when it answers, and runs no original then;
- the research document carries its eight headings and the two Decision keys,
  which read `pending` until Live 1 has run.

JumpSceneryProbe.hpp's decision itself is exercised by
test_jump_scenery_behavior.py.
"""
import re
import sys
import unittest
from pathlib import Path

TESTS_DIR = Path(__file__).resolve().parent
ROOT = TESTS_DIR.parent
PLUGIN = ROOT / "plugin" / "ModuleMain.cpp"
HEADER = ROOT / "plugin" / "include" / "ForgePact" / "JumpSceneryProbe.hpp"
DOC = ROOT / "docs" / "jump-scenery-research.md"
SDK_SCRIPTS = ROOT.parent / "hs-game-sdk" / "cpp" / "include" / "hs_game_sdk" / "scripts.hpp"

if str(TESTS_DIR) not in sys.path:
    sys.path.insert(0, str(TESTS_DIR))

from test_release_hook_contract import function_body, strip_comments, strip_research_blocks  # noqa: E402

BLOCK_START = "// ---- jumpprobe: the jump-through-scenery phase 1 instrument (ForgePact #16)"
BLOCK_END = "#endif // FORGEPACT_RELEASE (jumpprobe)"
JP_SYMBOL = r"\b(?:g_|k)?Jp[A-Z0-9_]\w*"

# docs/jump-scenery-research.md: the eight headings, in this order.
DOC_HEADINGS = ("Status", "Static search", "Static reading", "Instrument", "Live procedure 1", "Results",
                "Decision", "Not established")

# The standard headers the decision core may include, and nothing else.
STD_HEADERS = {"cstdint", "functional", "string_view", "unordered_map", "algorithm", "array", "string", "vector",
               "optional", "limits", "utility"}


def sdk_constants():
    if not SDK_SCRIPTS.exists():
        raise unittest.SkipTest(f"hs-game-sdk header not found at {SDK_SCRIPTS}")
    constants = dict(re.findall(r'std::string_view (\w+) = "([^"]+)";', SDK_SCRIPTS.read_text(encoding="utf-8")))
    assert len(constants) > 6000, f"parsed only {len(constants)} SDK script constants; the parser is broken"
    return constants


def braced_block(code, opener):
    """The text of the braced block whose `{` ends `opener` (exclusive)."""
    start = code.index(opener) + len(opener) - 1
    assert code[start] == "{", opener
    depth = 0
    for index in range(start, len(code)):
        if code[index] == "{":
            depth += 1
        elif code[index] == "}":
            depth -= 1
            if depth == 0:
                return code[start + 1:index]
    raise AssertionError("unterminated block after " + opener)


def doc_section(text, heading):
    start = text.index("\n## " + heading + "\n")
    end = text.find("\n## ", start + 1)
    return text[start:end if end >= 0 else len(text)]


class JumpProbeContract(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.plugin = PLUGIN.read_text(encoding="utf-8").replace("\r\n", "\n")
        cls.block = cls.plugin[cls.plugin.index(BLOCK_START):cls.plugin.index(BLOCK_END)]
        cls.code = strip_comments(cls.block)
        cls.shipped = strip_comments(strip_research_blocks(cls.plugin))
        cls.header = strip_comments(HEADER.read_text(encoding="utf-8").replace("\r\n", "\n"))
        table = cls.code[cls.code.index("#define JUMPPROBE_TARGETS(X)"):]
        table = table[:table.index("#define JP_ROW_INDEX")]
        cls.rows = re.findall(r'X\((\w+),\s*"([^"]+)",\s*(\w+),\s*(\w+)\)', table)

    def body(self, signature):
        return strip_comments(function_body(self.plugin, signature))

    # ---- research build only ----------------------------------------------------

    def test_every_jp_symbol_and_the_verb_vanish_from_the_player_build(self):
        symbols = set(re.findall(JP_SYMBOL, self.code)) | set(re.findall(r"\bJUMPPROBE_\w+|\bJP_[A-Z_]+\b", self.code))
        # A scan that finds nothing would pass vacuously.
        for expected in ("JpCommand", "JpInstall", "JpOnScript", "JpOnBuiltin", "JpFrameTick", "g_JpRows",
                         "g_JpCore", "kJpRowCount", "JUMPPROBE_TARGETS", "JP_ENTRY"):
            self.assertIn(expected, symbols)
        # Negative control: the strip keeps player code, so an absence below
        # is the strip working, not an empty string.
        self.assertIn("kPlayerCommands", self.shipped)
        self.assertIn("static bool HandleJumpProbeCommand(", self.shipped)
        for symbol in sorted(symbols):
            self.assertIsNone(re.search(r"\b" + re.escape(symbol) + r"\b", self.shipped), symbol + " reaches the player build")
        self.assertIsNone(re.search(JP_SYMBOL, self.shipped), "a Jp* symbol reaches the player build")
        self.assertNotIn("jumpprobe", self.shipped)
        self.assertNotIn("JumpSceneryProbe.hpp", strip_research_blocks(self.plugin))
        self.assertNotIn("JumpScenery", self.shipped)

    def test_not_a_player_command(self):
        run = function_body(self.plugin, "static void RunCommand(const std::string& line)")
        commands = run[run.index("kPlayerCommands = {"):]
        commands = commands[:commands.index("};")]
        self.assertIn('"menulayout"', commands)   # the set is the one read, not an empty slice
        self.assertNotIn('"jumpprobe"', commands)

    def test_dispatched_straight_after_skillprobe_as_a_standalone_early_return(self):
        run = function_body(self.plugin, "static void RunCommand(const std::string& line)")
        self.assertIn("    if (HandleSkillProbeCommand(lc, rest)) return;\n"
                      "    if (HandleJumpProbeCommand(lc, rest)) return;\n", run)
        self.assertEqual(run.count("HandleJumpProbeCommand"), 1)
        self.assertNotIn('"jumpprobe"', run)
        self.assertEqual(self.plugin.count('lc == "jumpprobe"'), 1)
        handler = function_body(self.plugin, "static bool HandleJumpProbeCommand(")
        self.assertLess(handler.index("#ifndef FORGEPACT_RELEASE"),
                        handler.index('if (lc == "jumpprobe") { JpCommand(rest); return true; }'))
        self.assertIn("return false;", handler)
        # In the player build the handler answers false and names nothing.
        shipped_handler = function_body(strip_research_blocks(self.plugin), "static bool HandleJumpProbeCommand(")
        self.assertNotIn("JpCommand", shipped_handler)
        self.assertIn("return false;", shipped_handler)

    # ---- the frame path ---------------------------------------------------------

    def test_frame_callback_names_only_the_tick(self):
        frame = strip_comments(function_body(self.plugin, "void FrameCallback(FWFrame& FrameContext)"))
        self.assertEqual(re.findall(JP_SYMBOL, frame), ["JpFrameTick"],
                         "a probe that runs more than one tick every frame is a mod, not a probe")
        self.assertIn("JpFrameTick();", frame)
        shipped_frame = strip_comments(function_body(strip_research_blocks(self.plugin),
                                                     "void FrameCallback(FWFrame& FrameContext)"))
        self.assertNotIn("JpFrameTick", shipped_frame)

    def test_the_tick_returns_at_once_while_unarmed(self):
        tick = self.body("static void JpFrameTick()")
        lines = [line.strip() for line in tick.split("\n") if line.strip()]
        self.assertEqual(lines[0], "if (!g_JpCore.Active()) return;")
        # Active() is armed, tracing or the lever on - nothing else.
        active = braced_block(self.header, "bool Active() const {")
        self.assertEqual(active.strip(), "return armed_ || trace_ || lever_;")
        # Every detour takes the same early way out to the original.
        script = self.body("static RValue& JpOnScript(")
        builtin = self.body("static void JpOnBuiltin(")
        self.assertIn("if (g_JpBusy || !g_JpCore.Active()) return t.orig ? t.orig(S, O, R, argc, A) : R;", script)
        self.assertIn("if (g_JpBusy || !g_JpCore.Active()) { if (t.orig) t.orig(Result, S, O, argc, Args); return; }", builtin)

    # ---- the rows ---------------------------------------------------------------

    def test_every_script_row_is_an_sdk_constant(self):
        sdk = sdk_constants()
        self.assertEqual(len(self.rows), 20)
        for safe, label, constant, role in self.rows:
            self.assertIn(constant, sdk, constant + " is not an hs-game-sdk script constant")
            value = sdk[constant]
            self.assertEqual(label, value[len("gml_Script_"):] if value.startswith("gml_Script_") else value,
                             label + " is not the constant's own name")
        # The table's names reach the runtime only through the constants.
        self.assertIn('{ LABEL, #SAFE, HeroSiege::Scripts::CONSTANT.data(), "fp_jp_" #SAFE, (PVOID)JpDetour_##SAFE, ROLE },',
                      self.code)
        for _, label, _, _ in self.rows:
            self.assertEqual(self.code.count('"' + label + '"'), 1, label + " is spelled outside its row")

    def test_every_sdk_jump_script_is_a_row(self):
        """The expected set comes from scripts.hpp, not from a hand list."""
        sdk = sdk_constants()
        bases = {}
        for constant, value in sdk.items():
            base = value[len("gml_Script_"):] if value.startswith("gml_Script_") else value
            if "jump" in base.lower() and "@" not in base:
                bases.setdefault(base, set()).add(constant)
        self.assertGreaterEqual(len(bases), 5, bases)   # CA_playerJump, PlayerForceJump, ... - the parse found them
        row_constants = {constant for _, _, constant, _ in self.rows}
        for base, constants in sorted(bases.items()):
            self.assertTrue(constants & row_constants, base + " (an SDK jump script) is not a jumpprobe row")

    def test_the_roles_controls_and_lever_rows(self):
        roles = {label: role for _, label, _, role in self.rows}
        self.assertEqual({label for label, role in roles.items() if role == "kJpRoleOpensWindow"},
                         {"CA_playerJump", "PlayerForceJump"})
        self.assertEqual(roles["CanMove"], "kJpRoleCanMove")
        self.assertEqual(roles["InstancePlaceTallest"], "kJpRoleInstancePlaceTallest")
        self.assertEqual(roles["TilePlaceMeeting"], "kJpRoleTilePlaceMeeting")
        lever = {label for label, role in roles.items() if role not in ("kJpRolePlain", "kJpRoleOpensWindow")}
        self.assertEqual(lever, {"CanMove", "InstancePlaceTallest", "TilePlaceMeeting"})
        # The controls: the enemies' jump (never answered) and CheckTalentUse.
        self.assertEqual(roles["CA_enemyJump"], "kJpRolePlain")
        self.assertEqual(roles["CheckTalentUse"], "kJpRolePlain")
        self.assertIn("HeroSiege::Scripts::gml_Script_CheckTalentUse", self.body("static bool JpIsOwnControl("))
        self.assertIn("kJpBuiltinControl = (int)JpNs::Builtin::PositionMeeting", self.code)

    def test_builtins_are_named_once_in_the_header_and_hooked_by_name(self):
        names = re.findall(r'\{\s*"(\w+)",\s*-?\d+,\s*Answer::\w+\s*\}', self.header)
        self.assertEqual(len(names), 10, names)
        doc = DOC.read_text(encoding="utf-8")
        search = doc_section(doc, "Static search")
        for name in names:
            self.assertIn(name, search, name + " is a builtin row the research doc's static search does not name")
        install = self.body("static void JpInstall()")
        self.assertIn("HookBuiltin(name.c_str(), kJpBuiltinHookIds[i], kJpBuiltinDetours[i], &t.orig)", install)
        self.assertIn("const std::string name = JpBuiltinName(i);", install)
        for name in names:
            self.assertNotIn('HookBuiltin("' + name + '"', self.code)

    def test_objects_resolve_by_sdk_name_and_family_through_object_is_ancestor(self):
        families = self.body("static void JpResolveFamilies()")
        self.assertIn("HeroSiege::Objects::GameObject::Collision_Prop_obj", families)
        self.assertIn("HeroSiege::Objects::GameObject::Collision_Parent_obj", families)
        self.assertIn('"asset_get_index"', families)
        self.assertIn('"object_is_ancestor"', families)
        # No object index written as a number anywhere in the block.
        for index in ("957", "959", "2269", "3553"):
            self.assertIsNone(re.search(r"\b" + index + r"\b", self.code), index + " is a hand-written object index")

    # ---- the installs -----------------------------------------------------------

    def test_every_detour_sits_behind_addr_is_executable_in_module(self):
        install = self.body("static void JpInstall()")
        self.assertEqual(install.count("MmCreateHook("), 1)
        self.assertLess(install.index("if (!AddrIsExecutableInModule(mainMod, src))"), install.index("MmCreateHook("))
        self.assertLess(install.index("if (!AddrIsExecutableInModule(mainMod, p))"), install.index("HookBuiltin("))
        self.assertNotIn("HookOneScriptTable", self.code)
        self.assertNotIn("HookOneScript(", self.code)
        # A held row is never detoured a second time.
        self.assertLess(install.index("JpHolder(t.runtimeName, holderCalls)"), install.index("MmCreateHook("))

    def test_hook_refuses_while_citrace_holds_a_spatial_builtin(self):
        install = self.body("static void JpInstall()")
        self.assertLess(install.index("JpCitraceHolders()"), install.index("for (JpRow& t : g_JpRows)"))
        self.assertIn('"jumpprobe hook: refused - citrace holds "', install)
        holders = self.body("static std::string JpCitraceHolders()")
        # Every citrace spatial builtin hook, derived from its own macro list.
        spatial = re.findall(r'CITRACE_BUILTIN_PM\((\w+), "(\w+)"\)', self.plugin)
        self.assertGreaterEqual(len(spatial), 8, spatial)
        for safe, name in spatial:
            self.assertIn("&g_OrigCi_" + safe, holders, "g_OrigCi_" + safe + " is not checked")
            self.assertIn('"' + name + '"', holders)

    # ---- the lever --------------------------------------------------------------

    def test_the_lever_writes_only_when_it_answers_and_then_runs_no_original(self):
        for signature, write, orig in (("static void JpOnBuiltin(", "Result = ", "t.orig("),
                                       ("static RValue& JpOnScript(", "R = ", "t.orig(")):
            fn = self.body(signature)
            answered = braced_block(fn, "if (answer != JpNs::Answer::RunOriginal) {")
            self.assertEqual(fn.count(write), 1, signature + " writes its result in more than one place")
            self.assertIn(write + "JpAnswerValue(answer);", answered)
            self.assertNotIn(orig, answered, signature + " runs the original when it answers")
            self.assertTrue(answered.rstrip().endswith(("return;", "return R;")), signature)
            # The answer comes only from the decision core: every assignment
            # is either "run the original" or the core's own decision.
            for rhs in re.findall(r"\banswer = ([^;(]+)", fn):
                self.assertIn(rhs.strip(), ("JpNs::Answer::RunOriginal", "g_JpCore.DecideBuiltin", "g_JpCore.DecideScript"),
                              signature + " sets its answer from " + rhs)
        builtin = self.body("static void JpOnBuiltin(")
        self.assertIn("g_JpCore.DecideBuiltin(", builtin)
        script = self.body("static RValue& JpOnScript(")
        self.assertIn("if (t.role >= 0) answer = g_JpCore.DecideScript(", script)

    def test_outside_the_window_the_core_answers_run_original(self):
        decide = braced_block(self.header, "Answer DecideBuiltin(Builtin row, int64_t frame, bool playerSelf, ObjectFn&& objectOf)\n    {")
        order = [decide.index(s) for s in ("if (!playerSelf)", "if (!lever_)", "if (!WindowOpen(frame))", "++c.passed;")]
        self.assertEqual(order, sorted(order))
        self.assertIn("if (!WindowOpen(frame)) { ++c.outsideWindow; return Answer::RunOriginal; }", decide)
        script = braced_block(self.header, "Answer DecideScript(ScriptLever row, int64_t frame, bool playerSelf)\n    {")
        self.assertLess(script.index("if (!WindowOpen(frame))"), script.index("++c.passed;"))

    def test_only_the_local_players_own_jump_opens_the_window(self):
        script = self.body("static RValue& JpOnScript(")
        self.assertIn("const bool player = S && S == g_JpPlayer;", script)
        self.assertIn("t.role == kJpRoleOpensWindow && g_JpCore.OnJumpEntry(frame, player)", script)
        entry = braced_block(self.header, "bool OnJumpEntry(int64_t frame, bool playerSelf)\n    {")
        self.assertIn("if (!lever_ || !playerSelf) return false;", entry)

    def test_armed_lines_are_the_local_players_only(self):
        for signature in ("static RValue& JpOnScript(", "static void JpOnBuiltin("):
            fn = self.body(signature)
            slots = [m.start() for m in re.finditer(r"JpTakeLogSlot\(", fn)]
            self.assertEqual(len(slots), 2, signature)
            for at in slots:
                self.assertTrue(fn[:at].rstrip().endswith("if (player &&"), signature + " logs a call that is not the player's")
        self.assertIn('" self=" + PpDescribeSelf(S)', self.code)
        self.assertIn('" frame=" + std::to_string(frame)', self.code)

    def test_the_decision_core_is_game_independent(self):
        includes = re.findall(r"#include\s*[<\"]([^>\"]+)[>\"]", self.header)
        self.assertTrue(includes)
        for name in includes:
            self.assertIn(name, STD_HEADERS, name + " is not a standard header")
        for word in ("RValue", "CInstance", "g_Yytk", "YYTK", "Aurie", "CallBuiltin"):
            self.assertNotIn(word, self.header)

    # ---- the research document --------------------------------------------------

    def test_the_research_doc_has_its_eight_headings_in_order(self):
        text = DOC.read_text(encoding="utf-8").replace("\r\n", "\n")
        positions = [text.find("\n## " + heading + "\n") for heading in DOC_HEADINGS]
        self.assertTrue(all(p >= 0 for p in positions), list(zip(DOC_HEADINGS, positions)))
        self.assertEqual(positions, sorted(positions))
        self.assertIn("jumpprobe", text)

    def test_the_decision_keys_read_pending_until_live_1(self):
        """Until the live gate: both keys exist and read `pending`.

        Replace these assertions once Live 1 has run (the way
        test_menu_probe_contract.py did): the keys must then name a measured
        value, never `pending`.
        """
        decision = doc_section(DOC.read_text(encoding="utf-8").replace("\r\n", "\n"), "Decision")
        for key in ("finding", "valid-landing"):
            match = re.search(r"(?m)^`?" + re.escape(key) + r":`?\s*`?([\w-]+)", decision)
            self.assertIsNotNone(match, key + ": is missing from ## Decision")
            self.assertEqual(match.group(1), "pending", key + " no longer reads pending: replace this test")


if __name__ == "__main__":
    unittest.main()
