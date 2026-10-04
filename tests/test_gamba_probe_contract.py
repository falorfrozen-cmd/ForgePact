"""Contract tests for the gamba machine research (ForgePact #134, phase 1).

How the gamba machine (Slot_Machine_01_obj) takes a spin, when it explodes
and where it rolls and builds its prize (Goburin's Head) is measured in one
research launch (docs/gamba-machine-research.md) before the pity mod is
written against it. This phase ships one research-build instrument for that
launch, `gambaprobe`, and these tests pin it on comment-stripped source:

- it is research build only: the word `gambaprobe`, its header, its handler
  and every `Gp*` symbol vanish from what the player build compiles, it is not
  a player command, and it is dispatched from its own handler as a standalone
  early return straight after `HandleJumpProbeCommand` (RunCommand's else-if
  chain is at MSVC's C1061 limit), the handler and its line inside the guard;
- the only piece on the frame path is one tick, which returns at once while
  nothing is armed or on;
- every script row is an hs-game-sdk constant, the expected set derived from
  scripts.hpp and objects.hpp and covering every row the plan names; the
  machine and its events are named through the SDK's object name; the rows the
  player build already detours are shared through those hooks, never detoured
  again; every detour sits behind AddrIsExecutableInModule; no hex or RVA
  literal reaches a call; and `hook` refuses while citrace, jumpprobe or
  jumpscenery holds one of its builtins;
- the lever writes a result only when it answers, and then runs no original;
- the local player and the machine resolve by the instance-handle rule, never
  a kind check;
- the research document carries its eight headings in order and the six
  Decision keys, each `pending` or one of its listed labels, and `pending` is
  refused once `### Live 1 results` exists.

GambaProbe.hpp's decision itself is exercised by test_gamba_probe_behavior.py.
"""
import re
import sys
import unittest
from pathlib import Path

TESTS_DIR = Path(__file__).resolve().parent
ROOT = TESTS_DIR.parent
PLUGIN = ROOT / "plugin" / "ModuleMain.cpp"
HEADER = ROOT / "plugin" / "include" / "ForgePact" / "GambaProbe.hpp"
DOC = ROOT / "docs" / "gamba-machine-research.md"
BEHAVIOR = TESTS_DIR / "test_gamba_probe_behavior.py"
SDK_SCRIPTS = ROOT.parent / "hs-game-sdk" / "cpp" / "include" / "hs_game_sdk" / "scripts.hpp"
SDK_OBJECTS = ROOT.parent / "hs-game-sdk" / "cpp" / "include" / "hs_game_sdk" / "objects.hpp"

if str(TESTS_DIR) not in sys.path:
    sys.path.insert(0, str(TESTS_DIR))

from test_release_hook_contract import function_body, strip_comments, strip_research_blocks  # noqa: E402

BLOCK_START = "// ---- gambaprobe: the gamba machine phase 1 instrument (ForgePact #134)"
BLOCK_END = "#endif // FORGEPACT_RELEASE (gambaprobe)"
GP_SYMBOL = r"\b(?:g_|k)?Gp[A-Z0-9_]\w*"
MACHINE = "Slot_Machine_01_obj"

# docs/gamba-machine-research.md: the eight headings, in this order.
DOC_HEADINGS = ("Status", "Static search", "Static reading", "Instrument", "Live procedure 1", "Results",
                "Decision", "Not established")

# The rows the plan's instrument names (context "### The instrument"); the
# table may carry more, never fewer. The closure is named by its SDK index
# stem here only - the plugin spells it through the constant.
PLAN_SCRIPT_ROWS = ("anon@1474", "GPV", "SPV", "InitPV", "FPV", "GetGoldAmount", "GoldOperationPending",
                    "GetGoldCounterHash", "PickUpGoldCheck", "LootBlocksUseKey", "CheckUseKey", "InputPressed",
                    "NetworkSendClientEffect", "PlaySound3D", "LootGroundCreate", "LootGroundCreateFromItem",
                    "CreateLootInFreePos", "CreateItemNew", "DropItem", "DropUniqueItems", "GetUniqueRepoStruct",
                    "CreateDefaultParams", "cpr_irandom", "cpr_rand32")
PLAN_EVENTS = ("Create_0", "Alarm_0", "Alarm_9", "Step_0", "CleanUp_0")
PLAN_BUILTINS = ("irandom", "irandom_range", "random", "random_range", "choose", "instance_destroy",
                 "instance_create_depth", "instance_create_layer")

# The rows the player build already detours (context "### Hooks already
# held"), each with the saved original its hook calls through.
SHARED_SCRIPTS = {
    "GPV": "&g_Orig_GPV_Trace",
    "LootGroundCreate": "&ForgePact::MiningOre::originalLoot",
    "CreateItemNew": "&g_Orig_CreateItemNew",
    "GetUniqueRepoStruct": "&g_Orig_GetUniqueRepoStruct",
    "CreateDefaultParams": "&g_Orig_CreateDefaultParams",
}

# The standard headers the decision core may include, and nothing else.
STD_HEADERS = {"cmath", "cstdint", "cstdio", "functional", "string_view", "unordered_map", "algorithm", "array",
               "string", "vector", "optional", "limits", "utility"}


def sdk_scripts():
    if not SDK_SCRIPTS.exists():
        raise unittest.SkipTest(f"hs-game-sdk header not found at {SDK_SCRIPTS}")
    constants = dict(re.findall(r'std::string_view (\w+) = "([^"]+)";', SDK_SCRIPTS.read_text(encoding="utf-8")))
    assert len(constants) > 6000, f"parsed only {len(constants)} SDK script constants; the parser is broken"
    return constants


def sdk_objects():
    if not SDK_OBJECTS.exists():
        raise unittest.SkipTest(f"hs-game-sdk header not found at {SDK_OBJECTS}")
    objects = dict(re.findall(r"^\s*(\w+) = (\d+),\s*$", SDK_OBJECTS.read_text(encoding="utf-8"), flags=re.M))
    assert len(objects) > 3000, f"parsed only {len(objects)} SDK objects; the parser is broken"
    return objects


def short_name(value):
    return value[len("gml_Script_"):] if value.startswith("gml_Script_") else value


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


def without_strings(code):
    return re.sub(r'"(?:\\.|[^"\\])*"', '""', code)


def doc_section(text, heading):
    start = text.index("\n## " + heading + "\n")
    end = text.find("\n## ", start + 1)
    return text[start:end if end >= 0 else len(text)]


def header_decision_keys(header):
    keys = []
    for key, check, labels, count, open_name in re.findall(
            r'\{\s*"([\w-]+)",\s*"([\w-]*)",\s*\{([^}]*)\},\s*(\d+),\s*(true|false)\s*\}', header):
        keys.append((key, check, tuple(re.findall(r'"([\w-]*)"', labels)[:int(count)]), open_name == "true"))
    return keys


class GambaProbeContract(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.plugin = PLUGIN.read_text(encoding="utf-8").replace("\r\n", "\n")
        cls.block = cls.plugin[cls.plugin.index(BLOCK_START):cls.plugin.index(BLOCK_END)]
        cls.code = strip_comments(cls.block)
        cls.shipped = strip_comments(strip_research_blocks(cls.plugin))
        cls.header = strip_comments(HEADER.read_text(encoding="utf-8").replace("\r\n", "\n"))
        table = cls.code[cls.code.index("#define GAMBAPROBE_SCRIPTS(X)"):]
        table = table[:table.index("#define GP_ROW_INDEX")]
        cls.rows = re.findall(r"X\((\w+),\s*(\w+),\s*([^)\n]+?)\)", table)

    def body(self, signature):
        return strip_comments(function_body(self.plugin, signature))

    # ---- research build only ----------------------------------------------------

    def test_every_gp_symbol_and_the_verb_vanish_from_the_player_build(self):
        symbols = set(re.findall(GP_SYMBOL, self.code)) | set(re.findall(r"\bGAMBAPROBE_\w+|\bGP_[A-Z_]+\b", self.code))
        # A scan that finds nothing would pass vacuously.
        for expected in ("GpCommand", "GpInstall", "GpOnScript", "GpOnBuiltin", "GpOnEvent", "GpFrameTick", "g_GpCore",
                         "g_GpScriptRows", "kGpScriptCount", "GpRefreshMachines", "GAMBAPROBE_SCRIPTS", "GP_SCRIPT_ENTRY"):
            self.assertIn(expected, symbols)
        # Negative control: the strip keeps player code, so an absence below
        # is the strip working, not an empty string.
        self.assertIn("kPlayerCommands", self.shipped)
        self.assertIn("static bool HandleJumpProbeCommand(", self.shipped)
        for symbol in sorted(symbols):
            self.assertIsNone(re.search(r"\b" + re.escape(symbol) + r"\b", self.shipped), symbol + " reaches the player build")
        self.assertIsNone(re.search(GP_SYMBOL, self.shipped), "a Gp* symbol reaches the player build")
        for word in ("gambaprobe", "GambaProbe", "GpNs", MACHINE):
            self.assertNotIn(word, self.shipped, word + " reaches the player build")
        self.assertNotIn("GambaProbe.hpp", strip_research_blocks(self.plugin))

    def test_not_a_player_command(self):
        run = function_body(self.plugin, "static void RunCommand(const std::string& line)")
        commands = run[run.index("kPlayerCommands = {"):]
        commands = commands[:commands.index("};")]
        self.assertIn('"menulayout"', commands)   # the set is the one read, not an empty slice
        self.assertNotIn('"gambaprobe"', commands)

    def test_dispatched_straight_after_jumpprobe_as_a_standalone_early_return(self):
        run = function_body(self.plugin, "static void RunCommand(const std::string& line)")
        self.assertIn("    if (HandleJumpProbeCommand(lc, rest)) return;\n"
                      "#ifndef FORGEPACT_RELEASE\n"
                      "    if (HandleGambaProbeCommand(lc, rest)) return;\n"
                      "#endif\n", run)
        self.assertEqual(run.count("HandleGambaProbeCommand"), 1)
        self.assertNotIn('"gambaprobe"', run)
        self.assertEqual(self.plugin.count('lc == "gambaprobe"'), 1)
        handler = self.body("static bool HandleGambaProbeCommand(")
        self.assertIn('if (lc == "gambaprobe") { GpCommand(rest); return true; }', handler)
        self.assertIn("return false;", handler)
        # The handler is inside the research block, so its name ships nowhere.
        self.assertIn("static bool HandleGambaProbeCommand(", self.code)
        self.assertNotIn("HandleGambaProbeCommand", self.shipped)
        shipped_run = function_body(strip_research_blocks(self.plugin), "static void RunCommand(const std::string& line)")
        self.assertIn("    if (HandleJumpProbeCommand(lc, rest)) return;\n"
                      "    if (HandleLiveOneResearchCommand(lc, rest)) return;\n", shipped_run)

    # ---- the frame path ---------------------------------------------------------

    def test_frame_callback_names_only_the_tick(self):
        frame = strip_comments(function_body(self.plugin, "void FrameCallback(FWFrame& FrameContext)"))
        self.assertEqual(re.findall(GP_SYMBOL, frame), ["GpFrameTick"],
                         "a probe that runs more than one tick every frame is a mod, not a probe")
        self.assertIn("GpFrameTick();", frame)
        shipped_frame = strip_comments(function_body(strip_research_blocks(self.plugin),
                                                     "void FrameCallback(FWFrame& FrameContext)"))
        self.assertNotIn("GpFrameTick", shipped_frame)

    def test_the_tick_and_every_detour_return_at_once_while_idle(self):
        tick = self.body("static void GpFrameTick()")
        lines = [line.strip() for line in tick.split("\n") if line.strip()]
        self.assertEqual(lines[0], "if (!g_GpCore.Active()) return;")
        active = braced_block(self.header, "bool Active() const {")
        self.assertEqual(active.strip(), "return armed_ || rngOn_;")
        # Observe counts the call, then returns before reading the self.
        observe = braced_block(self.header, "Seen Observe(int row, SelfFn&& selfObject, bool inMachineEvent)\n    {")
        order = [observe.index(s) for s in ("++c.calls;", "if (!Active()) return Seen::Idle;", "selfObject()")]
        self.assertEqual(order, sorted(order))
        script = self.body("static RValue& GpOnScript(")
        self.assertIn("if (seen == GpNs::Seen::Idle || seen == GpNs::Seen::Other) return t.orig ? t.orig(S, O, R, argc, A) : R;",
                      script)
        builtin = self.body("static void GpOnBuiltin(")
        idle = braced_block(builtin, "if (d.seen == GpNs::Seen::Idle || d.seen == GpNs::Seen::Other || !GpCanLog(row)) {")
        self.assertEqual([line.strip() for line in idle.split("\n") if line.strip()],
                         ["if (t.orig) t.orig(Result, S, O, argc, Args);", "return;"])
        event = self.body("static void GpOnEvent(")
        self.assertIn("if (seen != GpNs::Seen::Machine) { if (t.orig) t.orig(S, O); return; }", event)

    # ---- the rows ---------------------------------------------------------------

    def test_every_script_row_is_an_sdk_constant(self):
        sdk = sdk_scripts()
        self.assertEqual(len(self.rows), len(PLAN_SCRIPT_ROWS))
        for safe, constant, holder in self.rows:
            self.assertIn(constant, sdk, constant + " is not an hs-game-sdk script constant")
            self.assertTrue(sdk[constant].startswith("gml_Script_"), constant + ": SdkShortScriptName needs the prefix")
        # The table's names reach the runtime only through the constants.
        self.assertIn('{ SdkShortScriptName(HeroSiege::Scripts::CONSTANT), "fp_gp_" #SAFE, (PVOID)GpDetour_##SAFE, HOLDER },',
                      self.code)
        for _, constant, _ in self.rows:
            for spelled in (short_name(sdk[constant]), sdk[constant]):
                self.assertNotIn('"' + spelled + '"', self.code, constant + " is spelled outside its row")
        self.assertNotIn("anon@", self.code, "the closure's name moves with every patch: the SDK constant only")

    def test_the_row_set_is_derived_from_the_sdk_and_covers_the_plan(self):
        sdk = sdk_scripts()
        objects = sdk_objects()
        self.assertIn(MACHINE, objects)
        row_constants = {constant for _, constant, _ in self.rows}
        # Every SDK script that belongs to the machine (its closures) is a row.
        machine_scripts = {constant for constant, value in sdk.items() if MACHINE in value}
        self.assertTrue(machine_scripts, "the SDK names no script of the machine; the derivation is broken")
        for constant in sorted(machine_scripts):
            self.assertIn(constant, row_constants, constant + " (an SDK script of " + MACHINE + ") is not a row")
        bases = {short_name(sdk[c]) for c in row_constants}
        for name in PLAN_SCRIPT_ROWS:
            found = name in bases or any(b.startswith(name + "@") for b in bases)
            self.assertTrue(found, name + " (a row the plan names) is missing")
        events = re.findall(r'\{\s*"(\w+)",\s*"(\w+)"\s*\}', self.header)
        self.assertEqual(tuple(e for e, _ in events), PLAN_EVENTS)
        builtins = re.findall(r'\{\s*"(\w+)",\s*AnswerKind::\w+\s*\}', self.header)
        self.assertEqual(tuple(builtins), PLAN_BUILTINS)
        # The event rows are the machine's own, by the SDK's object name.
        events_install = self.body("static void GpInstallEvents(")
        self.assertIn('t.name = "gml_Object_" + machine + "_" + std::string(GpNs::kEvents[i].event);', events_install)
        self.assertIn("const std::string machine = GpMachineName();", events_install)

    def test_every_object_is_an_sdk_name(self):
        objects = sdk_objects()
        used = set(re.findall(r"GameObject::(\w+)", self.code))
        self.assertEqual(used, {MACHINE})
        for name in used:
            self.assertIn(name, objects)
        self.assertIsNone(re.search(r'"\w*_obj"', self.code), "an object name spelled as a string")
        resolve = self.body("static int GpResolveMachineObject()")
        self.assertIn('"asset_get_index"', resolve)
        self.assertIn("HeroSiege::Objects::GetObjectName(", resolve)
        self.assertIsNone(re.search(r"\b" + objects[MACHINE] + r"\b", self.code), "a hand-written object index")

    def test_shared_rows_go_through_their_existing_hooks(self):
        holders = {short_name(sdk_scripts()[constant]): holder.strip() for _, constant, holder in self.rows}
        for name, holder in holders.items():
            self.assertEqual(holder, SHARED_SCRIPTS.get(name, "nullptr"), name + "'s holder")
        install = self.body("static std::string GpInstallScriptHolder(")
        self.assertIn("InstallSignatureAngelicHooks();", install)
        self.assertIn("ForgePact::MiningOre::Install();", install)
        self.assertIn('HookOneScript(SdkShortScriptName(HeroSiege::Scripts::gml_Script_GPV), "bp_gpvtrace", (PVOID)Hook_GPV_Trace,',
                      install)
        scripts = self.body("static void GpInstallScripts()")
        shared = braced_block(scripts, "if (t.holder) {")
        self.assertNotIn("HookOneScript(", shared, "a shared row is never detoured a second time")
        self.assertLess(shared.index("GpHolderIsNative("), shared.index("*t.holder = "))
        self.assertTrue(shared.rstrip().endswith("continue;"))
        builtins = self.body("static void GpInstallBuiltins(")
        self.assertIn("if (!g_OrigICD || !g_OrigICL) InstallCreateHooks();", builtins)
        for slot in ("&g_OrigICD", "&g_OrigICL", "&g_OrigInstDestroy", "&g_OrigDestroy"):
            self.assertIn("holder = " + slot + ";", builtins)
        spliced = braced_block(builtins, "if (holder) {")
        self.assertNotIn("HookBuiltin(", spliced)
        self.assertLess(spliced.index("GpHolderIsNative("), spliced.index("*holder = "))
        # Only a trampoline is spliced: game code or this plugin's code there
        # means the holder is table-only.
        native = self.body("static bool GpHolderIsNative(")
        self.assertIn("AddrIsExecutableInModule(GetModuleHandleA(nullptr), saved)", native)
        self.assertIn("AddrIsExecutableInModule(GpSelfModule(), saved)", native)
        # DebugLogAddExt is `debuglog`'s and no row of this probe.
        self.assertNotIn("DebugLogAddExt", self.code)

    # ---- the installs -----------------------------------------------------------

    def test_every_detour_sits_behind_addr_is_executable_in_module(self):
        events = self.body("static void GpInstallEvents(")
        self.assertEqual(events.count("MmCreateHook("), 1)
        self.assertLess(events.index("if (!AddrIsExecutableInModule(mainMod, fn))"), events.index("MmCreateHook("))
        self.assertIn("FsFindGmlRow(t.name)", events)
        self.assertNotIn("GetNamedRoutinePointer(", events, "object events do not resolve by name on this build")
        self.assertIn("TaggedThunks<GpEventFn>::Tagged(", events)
        builtins = self.body("static void GpInstallBuiltins(")
        self.assertLess(builtins.index("if (!AddrIsExecutableInModule(mainMod, p))"), builtins.index("HookBuiltin("))
        self.assertEqual(builtins.count("HookBuiltin("), 1)
        # Script rows only through HookOneScript, which validates its table
        # entry before its own inline detour.
        self.assertEqual(self.code.count("MmCreateHook("), 1)
        self.assertNotIn("HookOneScriptTable", self.code)
        self.assertNotIn("HookRawNamedRoutine", self.code)
        hook = strip_comments(function_body(self.plugin, "static bool HookOneScript(const char* shortName, const char* id, PVOID dest, PFUNC_YYGMLScript* origOut,\n                          bool* nativeOut)"))
        self.assertLess(hook.index("AddrIsExecutableInModule("), hook.index("MmCreateHook("))

    def test_no_hex_or_rva_literal_reaches_a_call(self):
        bare = without_strings(self.code)
        self.assertIsNone(re.search(r"\b0[xX][0-9A-Fa-f]+", bare), "a hex literal in gambaprobe's code")
        self.assertIsNone(re.search(r"Rva\w*", bare), "an RVA in gambaprobe's code")
        self.assertIsNone(re.search(r"GetModuleHandle\w*\([^)]*\)\s*\+", bare), "a call target computed from a module base")

    def test_hook_refuses_while_citrace_jumpprobe_or_jumpscenery_holds_a_builtin(self):
        install = self.body("static void GpInstall()")
        first = install.index("GpInstallEvents(mainMod);")
        for holder in ("JpCitraceHolders()", "g_OrigCi_InstanceDestroy", "GpJumpProbeHolders()", "JumpSceneryHeldHooks()"):
            self.assertLess(install.index(holder), first, holder + " is checked after a row is installed")
        for who in ("citrace", "jumpprobe", "jumpscenery"):
            self.assertIn('"gambaprobe hook: refused - ' + who + ' holds "', install)
        self.assertIn("installed", self.body("static std::string GpJumpProbeHolders()"))

    # ---- the lever --------------------------------------------------------------

    def test_the_lever_writes_only_when_it_answers_and_then_runs_no_original(self):
        fn = self.body("static void GpOnBuiltin(")
        answered = braced_block(fn, "if (d.answer) {")
        self.assertEqual(fn.count("Result = "), 1, "GpOnBuiltin writes its result in more than one place")
        self.assertIn("Result = GpAnswerValue(builtin, d, argc, Args);", answered)
        self.assertNotIn("t.orig(", answered, "the lever runs the original when it answers")
        self.assertTrue(answered.rstrip().endswith("return;"))
        # The answer comes only from the decision core.
        self.assertIn("const GpNs::RngDecision d = g_GpCore.DecideRng(", fn)
        # The core answers a machine self only, with answers left.
        decide = braced_block(self.header, "RngDecision DecideRng(Builtin builtin, SelfFn&& selfObject, bool inMachineEvent, int argc)\n    {")
        self.assertIn("if (d.seen != Seen::Machine || kind == AnswerKind::NotRng || !rngOn_ || rngRemaining_ <= 0) return d;",
                      decide)
        self.assertLess(decide.index("return d;"), decide.index("d.answer = true;"))
        # Scripts and events are never answered.
        for signature in ("static RValue& GpOnScript(", "static void GpOnEvent("):
            body = self.body(signature)
            self.assertIsNone(re.search(r"\bR\s*=[^=]", body), signature + " writes a result")
            self.assertNotIn("DecideRng", body)

    def test_an_inert_lever_is_named_and_every_dead_arming_warns(self):
        inert = braced_block(self.header, "bool Inert() const {")
        self.assertEqual(inert.strip(), "return rngOn_ && rngAnswered_ == 0;")
        line = braced_block(self.header, "std::string RngLine() const\n    {")
        self.assertIn("if (Inert())", line)
        self.assertIn("INERT", line)
        self.assertIn("Out(g_GpCore.RngLine());", self.body("static void GpStatus()"))
        lever = self.body("static void GpRng(")
        for guard in ("if (rngRows == 0)", "if (g_GpCore.MachineObject() < 0)", "else if (g_GpMachines.empty())"):
            self.assertIn(guard, lever)
        self.assertEqual(lever.count("WARNING"), 3)

    # ---- who a self is ----------------------------------------------------------

    def test_the_local_player_and_the_machine_resolve_by_the_instance_handle_rule(self):
        """The runner hands instances over as VALUE_REF: no kind check may gate the work."""
        player = self.body("static bool GpPlayerXY(")
        self.assertIn("if (!HhResolveLocalPlayer(p)) return false;", player)
        self.assertIn("HhResolveInstance(p)", player)
        machines = self.body("static void GpRefreshMachines()")
        self.assertIn("CInstance* inst = HhResolveInstance(handle);", machines)
        self.assertIn("g_GpCore.IsMachine(GpObjectIndexOf(handle))", machines)
        self.assertIn("N1ObjectIndex(", self.body("static int GpObjectIndexOf("))
        self.assertIn("HhResolveInstance(id)", self.body("static void GpSpawn()"))
        for word in ("m_Kind", "VALUE_OBJECT", "VALUE_REF"):
            self.assertNotIn(word, self.code, word + ": a raw kind check in gambaprobe would silently disable it")
        machine = braced_block(self.header, "bool IsMachine(int object) const {")
        self.assertEqual(machine.strip(), "return machineObject_ >= 0 && object == machineObject_;")

    def test_the_decision_core_is_game_independent(self):
        includes = re.findall(r"#include\s*[<\"]([^>\"]+)[>\"]", self.header)
        self.assertTrue(includes)
        for name in includes:
            self.assertIn(name, STD_HEADERS, name + " is not a standard header")
        for word in ("RValue", "CInstance", "g_Yytk", "YYTK", "Aurie", "CallBuiltin"):
            self.assertNotIn(word, self.header)
        self.assertIsNone(re.search(r"\bkTraceLinesPerRow\s*=\s*\d+;", self.code), "the trace budget is the header's")
        self.assertRegex(self.header, r"inline constexpr int kTraceLinesPerRow = \d+;")

    def test_the_behavior_test_declares_no_parallel_group(self):
        self.assertNotIn("PARALLEL_GROUP", BEHAVIOR.read_text(encoding="utf-8"))

    # ---- the research document --------------------------------------------------

    def test_the_research_doc_has_its_eight_headings_in_order(self):
        text = DOC.read_text(encoding="utf-8").replace("\r\n", "\n")
        positions = [text.find("\n## " + heading + "\n") for heading in DOC_HEADINGS]
        self.assertTrue(all(p >= 0 for p in positions), list(zip(DOC_HEADINGS, positions)))
        self.assertEqual(positions, sorted(positions))
        self.assertIn("gambaprobe", text)

    def test_the_decision_keys_are_pending_or_a_listed_label(self):
        """Six keys, each exactly once in ## Decision.

        Before Live 1 a key reads `pending`; once ## Results carries `### Live
        1 results`, `pending` is no longer an answer and every key names one of
        its labels (drop-route: the script or builtin that placed the prize).
        """
        text = DOC.read_text(encoding="utf-8").replace("\r\n", "\n")
        decision = doc_section(text, "Decision")
        live1 = "\n### Live 1 results" in doc_section(text, "Results")
        keys = header_decision_keys(self.header)
        self.assertEqual([k for k, _, _, _ in keys],
                         ["roll-route", "explosion-rule", "drop-route", "counter-route", "fallback-drop", "pity-design"])
        for key, check, labels, open_name in keys:
            found = re.findall(r"(?m)^[-*\s]*`?" + re.escape(key) + r"`?:\s*`?([\w@-]+)`?", decision)
            self.assertEqual(len(found), 1, key + ": must appear exactly once in ## Decision")
            answer = found[0]
            if live1:
                self.assertNotEqual(answer, "pending", "Live 1 has run; `pending` is no longer an answer for " + key)
            if answer != "pending":
                if open_name:
                    self.assertRegex(answer, r"^[A-Za-z0-9_@]+$|^not-observed$", key)
                else:
                    self.assertIn(answer, labels, key + ": " + answer + " is not one of its labels")
            for label in labels:
                self.assertIn("`" + label + "`", decision, key + " no longer lists " + label)
            if check:
                self.assertIn("`" + check + "`", decision, key + " no longer names its check " + check)


if __name__ == "__main__":
    unittest.main()
