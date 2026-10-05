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
  machine and its events are named through the SDK's object name; the rows
  another hook already holds go through that hook, never hooked a second time
  - spliced when the holder's saved original is a trampoline, detoured under
  it when the holder is table-only and its saved original is still the game's
  function - and a table-only row is named; every detour sits behind
  AddrIsExecutableInModule; no hex or RVA literal reaches a call; and `hook`
  refuses while citrace, jumpprobe or jumpscenery holds one of its builtins;
- the trace budget is per window: `trace`, `hook` again and `spawn` start it
  over, a builtin's lines are keyed by their argument text and an event's by
  its instance, and `status` names a row that has spent its budget;
- the lever writes a result only when it answers, and then runs no original;
- the local player and the machine resolve by the instance-handle rule, never
  a kind check;
- the research document carries its nine headings in order (Live procedure 2
  between Live procedure 1 and Results) and the six Decision keys, each
  `pending` or one of its listed labels, and `pending` is refused once `###
  Live 2 results` exists (Live 1 measured nothing: INSTRUMENT-BLIND).

Replan 1, after Live 1 found every spawned machine cleaned up in the step after
its Create_0:

- `spawn [depth|game|layer|self]` carries four routes - `spawn game` calls the
  game's instance_create script by its hs-game-sdk constant through the
  by-name route DungeonChestChat uses, `spawn layer` uses the player's own
  layer value - and no route spells a layer name or an effect id; every
  refusal names the step that failed;
- the caller walk (`cleanup-caller` in Live procedure 2) runs at CleanUp_0,
  Alarm_9 and the window's first Create_0 before the original, prints its
  frames through the header's FrameText and names game frames only by rows
  whose function AddrIsExecutableInModule places in the game;
- the by-name route (`byname-resolve`, `byname-visible`) is read for every
  script row before any is installed, printed at the end of every script
  row's hook line and on `status`, and its detours sit behind
  AddrIsExecutableInModule and never answer;
- instance_change, layer_destroy_instances, instance_deactivate_object and
  room_goto are named builtin rows, a call whose first argument names a
  machine counts as `machine-arg=`, and `selftest` calls irandom outside the
  busy guard.

Phase 4, the explosion watch (the owner ruled on 2026-10-05 that the pity
fires on the machine's explosion, which nothing had observed):

- the tick reads each live machine's sprite by name (variable_instance_get,
  sprite_get_name) with no kind check and no sprite spelled or numbered, and
  feeds the core only after a refresh that ran to its end;
- every build row's call goes into the ring before the machine-self filter,
  whatever its self, and inside a window the build rows and the instance
  create/destroy builtins print a window line for any self, apart from the
  trace budget;
- round 1: build lines and instance lines keep apart rings (so a burst of
  creates cannot evict a build) and apart caps (instance lines per created
  object, never the build cap), both reported when dropped and topped up by
  an extension; the automatic span is at least 600 frames; and the by-name
  route feeds the watch for the build rows too;
- round 2: `status` names an open window's end, the current frame and what
  remains; the first refusal of an object's share prints a `capped` line;
  the rings drop their oldest with `pop_front`; the by-argument rule reuses
  the watch's read of instance_destroy's argument; and a call fed through a
  by-name slot runs its original under a marker that keeps the script row
  from feeding it again;
- the built-item line is read after CreateItemNew returns;
- `window [frames]` opens a window by hand, `status` prints the watch's
  counters, `off` closes an open window, and every new symbol stays inside
  the research-build guard.

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

# docs/gamba-machine-research.md: the ten headings, in this order.
DOC_HEADINGS = ("Status", "Static search", "Static reading", "Instrument", "Live procedure 1", "Live procedure 2",
                "Live procedure 3", "Results", "Decision", "Not established")

# What Live procedure 2 runs and the checks it records (replan 1).
LIVE2_COMMANDS = ("spawn game", "spawn layer", "spawn self", "selftest")
LIVE2_CHECKS = ("selftest-rng", "byname-resolve", "spawn-depth", "cleanup-caller", "spawn-game", "spawn-layer",
                "spawn-self", "byname-visible", "rng-rows-live")
SPAWN_ROUTES = ("depth", "game", "layer", "self", "scp", "stamp")

# What Live procedure 3 runs and the checks it records (1c).
LIVE3_COMMANDS = ("spawn scp", "spawn stamp", "fnwalk", "Slot Machine Spawned")
LIVE3_CHECKS = ("spawn-scp", "spawn-stamp", "stamp-readback", "natural-machine", "state-rows-live", "byname-visible",
                "fnwalk", "state-trace", "hook-timing", "ext-rows-hooked")

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
                 "instance_create_depth", "instance_create_layer",
                 # replan 1: the runner paths that could end or swap a machine
                 "instance_change", "layer_destroy_instances", "instance_deactivate_object", "room_goto",
                 # 1c: the extension functions Alarm_9 and sCP call by name
                 "GetVariable", "SetVariable", "SetVariableToUndefined")
# The rows whose first argument is checked for a machine (machine-arg=).
ARGUMENT_BUILTINS = ("InstanceDestroy", "InstanceChange", "InstanceDeactivateObject")

# The rows another ForgePact hook already holds (context "### Hooks already
# held", plus the two the research build holds at startup: DropManager's
# native DropItem hook and the item-inspect table-only hook on
# LootGroundCreateFromItem), each with the saved original its hook calls
# through.
SHARED_SCRIPTS = {
    "GPV": "&g_Orig_GPV_Trace",
    "LootGroundCreate": "&ForgePact::MiningOre::originalLoot",
    "LootGroundCreateFromItem": "&g_Orig_LootGroundCreateFromItem",
    "CreateItemNew": "&g_Orig_CreateItemNew",
    "DropItem": "GpDropItemHolder()",
    "GetUniqueRepoStruct": "&g_Orig_GetUniqueRepoStruct",
    "CreateDefaultParams": "&g_Orig_CreateDefaultParams",
}

# The standard headers the decision core may include, and nothing else.
STD_HEADERS = {"cmath", "cstdint", "cstdio", "functional", "string_view", "unordered_map", "algorithm", "array",
               "string", "vector", "optional", "limits", "utility", "deque"}


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
        cls.rows = re.findall(r"(?m)^\s*X\((\w+),\s*(\w+),\s*(.+)\)\s*\\?\s*$", table)

    def body(self, signature):
        return strip_comments(function_body(self.plugin, signature))

    # ---- research build only ----------------------------------------------------

    def test_every_gp_symbol_and_the_verb_vanish_from_the_player_build(self):
        symbols = set(re.findall(GP_SYMBOL, self.code)) | set(re.findall(r"\bGAMBAPROBE_\w+|\bGP_[A-Z_]+\b", self.code))
        # A scan that finds nothing would pass vacuously.
        for expected in ("GpCommand", "GpInstall", "GpOnScript", "GpOnBuiltin", "GpOnEvent", "GpFrameTick", "g_GpCore",
                         "g_GpScriptRows", "kGpScriptCount", "GpRefreshMachines", "GAMBAPROBE_SCRIPTS", "GP_SCRIPT_ENTRY",
                         # phase 4: the explosion watch
                         "g_GpWatch", "GpWatchScript", "GpWatchBuilt", "GpWatchBuiltin", "GpWatchTick", "GpWindow",
                         "GpSpriteName", "GpIsBuildRow", "GpTraceScript", "g_GpMachinesRead"):
            self.assertIn(expected, symbols)
        # Negative control: the strip keeps player code, so an absence below
        # is the strip working, not an empty string.
        self.assertIn("kPlayerCommands", self.shipped)
        self.assertIn("static bool HandleJumpProbeCommand(", self.shipped)
        for symbol in sorted(symbols):
            self.assertIsNone(re.search(r"\b" + re.escape(symbol) + r"\b", self.shipped), symbol + " reaches the player build")
        self.assertIsNone(re.search(GP_SYMBOL, self.shipped), "a Gp* symbol reaches the player build")
        # Phase 2's gambapity (a player feature) names the machine by its SDK
        # object, so `Slot_Machine_01_obj` is no longer a research-only name.
        for word in ("gambaprobe", "GambaProbe", "GpNs"):
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
        script = self.body("static RValue& GpTraceScript(GpScriptRow& t, int row, GpNs::Seen seen,")
        self.assertIn("if (seen == GpNs::Seen::Idle || seen == GpNs::Seen::Other) return t.orig ? t.orig(S, O, R, argc, A) : R;",
                      script)
        # Phase 4: the explosion watch reads nothing for an idle call either.
        self.assertIn("if (seen != GpNs::Seen::Idle && GpIsBuildRow(script) && !fedByName && GpWatchScript(",
                      self.body("static RValue& GpOnScript("))
        builtin = self.body("static void GpOnBuiltin(")
        idle = braced_block(builtin, "if (d.seen == GpNs::Seen::Idle) {")
        self.assertEqual([line.strip() for line in idle.split("\n") if line.strip()],
                         ["if (t.orig) t.orig(Result, S, O, argc, Args);", "return;"])
        # The by-argument read happens only after the idle return.
        self.assertLess(builtin.index("if (d.seen == GpNs::Seen::Idle) {"), builtin.index("GpMachineArg("))
        by_name = self.body("static void GpOnByName(")
        self.assertIn("if (seen == GpNs::Seen::Idle || seen == GpNs::Seen::Other || !GpCanLog(row)) {", by_name)
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
        self.assertIn('{ SdkShortScriptName(HeroSiege::Scripts::CONSTANT), "fp_gp_" #SAFE, (PVOID)GpDetour_##SAFE, HOLDER, \\\n'
                      '      HeroSiege::Scripts::CONSTANT.data() },', self.code)
        # The by-name lookup's second name is the constant's own value, never
        # composed from the short one.
        self.assertNotIn('"gml_Script_" +', self.code)
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
        self.assertIn("ForgePact::DropManager::Instance().InstallHooks();", install)
        self.assertIn('HookOneScript(SdkShortScriptName(HeroSiege::Scripts::gml_Script_GPV), "bp_gpvtrace", (PVOID)Hook_GPV_Trace,',
                      install)
        # A second run of the item-inspect installer would re-swap a dozen
        # table entries: never from here.
        self.assertNotIn("InstallItemInspectHooks", install)
        # DropItem's holder is DropManager's own slot, by the SDK name.
        self.assertIn("ForgePact::DropManager::Instance().ResearchHeldOriginal(SdkShortScriptName(HeroSiege::Scripts::gml_Script_DropItem))",
                      self.body("static PFUNC_YYGMLScript* GpDropItemHolder()"))
        scripts = self.body("static void GpInstallScripts(HMODULE mainMod)")
        held = braced_block(scripts, "if (t.holder && *t.holder) {")
        self.assertNotIn("HookOneScript(", held, "a held row is never hooked a second time")
        self.assertTrue(held.rstrip().endswith("continue;"))
        # The holder's saved original decides: game code -> detour under it;
        # a trampoline -> splice; this plugin's code -> missing.
        game = braced_block(held, "if (AddrIsExecutableInModule(mainMod, saved)) {")
        self.assertIn("GpDetourUnder(t, mainMod, holder);", game)
        self.assertNotIn("*t.holder = ", game, "a table-only holder is detoured under, never spliced")
        self.assertTrue(game.rstrip().endswith("continue;"))
        order = [held.index(s) for s in ("if (AddrIsExecutableInModule(mainMod, saved)) {", "GpHolderIsNative(",
                                         "*t.holder = ")]
        self.assertEqual(order, sorted(order))
        under = self.body("static void GpDetourUnder(")
        self.assertLess(under.index("if (!AddrIsExecutableInModule(mainMod, fn)) {"), under.index("MmCreateHook("))
        self.assertIn("TaggedThunks<PFUNC_YYGMLScript>::Tagged(t.hookId,", under)
        self.assertIn("t.route = GpNs::Route::DetouredUnder;", under)
        self.assertNotIn("*t.holder =", under, "the holder is left as it is")
        # A table-only row is named, so a blind row cannot pass as a measured zero.
        gp_install = self.body("static void GpInstall()")
        self.assertIn("if (t.route == GpNs::Route::TableOnly) blind +=", gp_install)
        self.assertIn('" missing, "', gp_install)
        self.assertIn('" table-only ("', gp_install)
        self.assertIn("WARNING - table-only, blind to compiled GML's direct calls", gp_install)
        builtins = self.body("static void GpInstallBuiltins(")
        self.assertIn("if (!g_OrigICD || !g_OrigICL) InstallCreateHooks();", builtins)
        for slot in ("&g_OrigICD", "&g_OrigICL", "&g_OrigInstDestroy", "&g_OrigDestroy"):
            self.assertIn("holder = " + slot + ";", builtins)
        spliced = braced_block(builtins, "if (holder) {")
        self.assertNotIn("HookBuiltin(", spliced)
        self.assertLess(spliced.index("GpHolderIsNative("), spliced.index("*holder = "))
        # Only a trampoline is spliced: game code or this plugin's code there
        # means the holder is table-only (a script row detours under game
        # code before it gets here; a builtin row has no such route).
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
        # entry before its own inline detour, or under a table-only holder
        # (GpDetourUnder, pinned with the shared rows above).
        self.assertEqual(self.code.count("MmCreateHook("), 2)
        self.assertEqual(self.body("static void GpDetourUnder(").count("MmCreateHook("), 1)
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
        for holder in ("JpCitraceHolders()", "g_OrigCi_InstanceDestroy", "g_OrigCi_InstanceChange",
                       "g_OrigCi_InstanceDeactivateObject", "GpJumpProbeHolders()", "JumpSceneryHeldHooks()"):
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
        decide = braced_block(self.header, "RngDecision DecideRng(Builtin builtin, SelfFn&& selfObject, bool inMachineEvent, int argc, ArgsFn&& argsText)\n    {")
        self.assertIn("if (d.seen != Seen::Machine || kind == AnswerKind::NotRng || !rngOn_ || rngRemaining_ <= 0) return d;",
                      decide)
        self.assertLess(decide.index("return d;"), decide.index("d.answer = true;"))
        # ... and only the call it is aimed at: its target builtin, and the
        # argument text it names, both decided before any answer.
        for gate in ("if (builtin != rngTarget_) {", "if (text != rngArgs_) {"):
            self.assertLess(decide.index(gate), decide.index("d.answer = true;"), gate)
            self.assertIn("++c.passed;", braced_block(decide, gate))
            self.assertTrue(braced_block(decide, gate).rstrip().endswith("return d;"), gate)
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
        for guard in ("if (!targetHooked)", "if (g_GpCore.MachineObject() < 0)", "else if (g_GpMachines.empty())"):
            self.assertIn(guard, lever)
        self.assertEqual(lever.count("WARNING"), 3)
        # The line names what passed it by, so a lever that never reached its
        # target is told apart from one that had nothing to reach.
        self.assertIn("passed=", line)
        self.assertIn("other machine-self RNG call(s) passed through untouched", line)

    def test_the_lever_is_aimed_at_one_builtin_and_its_argument_text(self):
        """An unaimed lever hands its answer to the machine's first RNG call, not the prize roll."""
        lever = self.body("static void GpRng(")
        self.assertIn("GpNs::BuiltinByName(Lower(tail[0]), target)", lever)
        self.assertIn("g_GpCore.SetRng(target, value, count, args)", lever)
        set_rng = braced_block(self.header, "bool SetRng(Builtin target, double value, int64_t count, std::string_view args = {})\n    {")
        self.assertIn("kBuiltins[b].kind == AnswerKind::NotRng) return false;", set_rng)
        self.assertIn("rngArgs_ = ArgsKey(args);", set_rng)
        # The argument text the lever compares is the trace line's own text,
        # described only when the core asks for it.
        builtin = self.body("static void GpOnBuiltin(")
        self.assertIn("try { a = GpBuiltinArgs(argc, Args); } catch (...) { a.clear(); }", builtin)
        decide = braced_block(self.header, "RngDecision DecideRng(Builtin builtin, SelfFn&& selfObject, bool inMachineEvent, int argc, ArgsFn&& argsText)\n    {")
        self.assertLess(decide.index("if (!rngArgs_.empty()) {"), decide.index("argsText()"))
        self.assertIn("text = ArgsKey(argsText());", decide)
        # status counts what the armed lever let through.
        self.assertIn('" passed="', braced_block(self.header, "std::string RowText(int row) const\n    {"))
        self.assertIn('" passed="', braced_block(self.header, "std::string StatusLine() const\n    {"))
        # The procedure aims it: the builtin and the argument text step 4 found.
        procedure = doc_section(DOC.read_text(encoding="utf-8").replace("\r\n", "\n"), "Live procedure 1")
        self.assertIn("`gambaprobe rng <builtin> <value> 1 args <text>`", procedure)
        self.assertNotRegex(procedure, r"`gambaprobe rng (?:<the value|\d)", "an unaimed rng in the procedure")

    # ---- the trace window -------------------------------------------------------

    def test_the_trace_budget_starts_over_before_each_measured_spin(self):
        """A spent row only counts, so every moment a live check reads must get a fresh window."""
        command = self.body("static void GpCommand(")
        self.assertIn('if (sub == "trace") { GpTrace(); return; }', command)
        self.assertIn("g_GpCore.ResetTrace();", self.body("static void GpTrace()"))
        spawn = self.body("static void GpSpawn(")
        self.assertLess(spawn.index("g_GpCore.ResetTrace();"), spawn.index("GpSpawnCall("),
                        "the new machine's Create_0 must fall in the new window, whichever route made it")
        self.assertIn("g_GpCore.ResetTrace();", self.body("static void GpInstall()"))
        # A builtin line is keyed by its argument text, an event line by its
        # instance, so one repeated call shape cannot spend the row.
        builtin = self.body("static void GpOnBuiltin(")
        self.assertIn("GpLogCall(row, GpBuiltinName(builtin), selfText, argc, args, args, GpValueText(Result));", builtin)
        self.assertNotIn("std::string(), GpValueText(Result)", builtin)
        self.assertIn("g_GpCore.TakeTraceLine(row, (uint64_t)id, (uint64_t)frame)", self.body("static void GpOnEvent("))
        take = braced_block(self.header,
                            "bool TakeTraceLine(int row, uint64_t key, uint64_t text, std::string_view keyText = {})\n    {")
        self.assertIn("kTraceLinesPerKey", take)
        self.assertIn("++c.keyCapped;", take)
        # A repeat of a key's last line is not logged, but never silent: a
        # prize roll repeating a reel roll's line would otherwise vanish and
        # the reel roll read as the decider. The row counts it, the key holds
        # it for status, and the key's next line carries it.
        self.assertIn("++c.repeats;", take)
        self.assertIn("++it->second.repeats;", take)
        self.assertIn("takenRepeats_ = k.repeats;", take)
        row_text = braced_block(self.header, "std::string RowText(int row) const\n    {")
        self.assertIn('" repeats="', row_text)
        self.assertIn("PendingRepeatsText(row)", row_text)
        self.assertIn('" repeats="', braced_block(self.header, "std::string StatusLine() const\n    {"))
        self.assertIn("c.repeats = 0;", braced_block(self.header, "void ResetTrace()\n    {"))
        log_call = self.body("static void GpLogCall(")
        self.assertIn("GpHash(args + \" ret=\" + ret), key)", log_call)
        self.assertIn("g_GpCore.TakenRepeats()", log_call)
        # The row's cap covers kTraceKeysPerRow full keys, so the protected
        # store's moving keys cannot spend GPV's or SPV's row within a spin.
        self.assertIn("static_assert(kTraceLinesPerRow == kTraceLinesPerKey * kTraceKeysPerRow,", self.header)
        keys = re.search(r"inline constexpr int kTraceKeysPerRow = (\d+);", self.header)
        self.assertIsNotNone(keys)
        self.assertGreaterEqual(int(keys.group(1)), 28, "fewer keys per row than InitPV sets up")
        # A spent row says so, in the row and above the rows.
        self.assertIn("BUDGET SPENT", braced_block(self.header, "std::string RowText(int row) const\n    {"))
        status = self.body("static void GpStatus()")
        self.assertIn("if (g_GpCore.SpentRows() > 0)", status)
        self.assertIn("BUDGET SPENT", status)
        # The procedure re-arms before every spin a check reads.
        procedure = doc_section(DOC.read_text(encoding="utf-8").replace("\r\n", "\n"), "Live procedure 1")
        self.assertGreaterEqual(procedure.count("`gambaprobe trace`"), 4)

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
        self.assertIn("HhResolveInstance(id)", self.body("static void GpSpawn("))
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

    # ---- replan 1: the spawn routes -----------------------------------------------

    def test_spawn_carries_four_routes_and_spawn_game_calls_the_games_script_by_its_constant(self):
        """`spawn game` reproduces the game's own creation call; no route spells a layer name or an effect id."""
        routes = re.search(r"kGpSpawnRoutes\[\]\s*=\s*\{([^}]*)\}", self.code)
        self.assertIsNotNone(routes)
        self.assertEqual(tuple(re.findall(r'"(\w+)"', routes.group(1))), SPAWN_ROUTES)
        self.assertIn('if (sub == "spawn") { GpSpawn(tail); return; }', self.body("static void GpCommand("))
        call = self.body("static void GpSpawnCall(")
        # game: the SDK constant's short name, asset_get_index, then
        # script_execute through ApCallScript (DungeonChestChat's route), with
        # the local player as self and (x, y, machine).
        self.assertIn("SdkShortScriptName(HeroSiege::Scripts::gml_Script_instance_create)", call)
        self.assertIn("gml_Script_instance_create", sdk_scripts())
        self.assertIn('"asset_get_index"', call)
        self.assertIn("ApCallScript(script, self, { RValue(x), RValue(y), machine }, id)", call)
        self.assertNotIn('"instance_create"', self.code, "the game's script is named through its SDK constant")
        # layer: the player's own layer value, read by name at the call.
        self.assertIn('g_Yytk->CallBuiltin("variable_instance_get", { player, RValue("layer") })', call)
        self.assertIn('g_Yytk->CallBuiltin("instance_create_layer", { RValue(x), RValue(y), layer, machine })', call)
        # self: the player as self and other.
        self.assertIn('g_Yytk->CallBuiltinEx(id, "instance_create_depth", self, self,', call)
        # depth: Live 1's call, the control.
        self.assertIn('g_Yytk->CallBuiltin("instance_create_depth", { RValue(x), RValue(y), RValue(0.0), machine })', call)
        # Nothing stored, and no layer name or effect id anywhere.
        for word in ("layer_get_id", "layer_create", "ClientCreateEffect", "effect_create"):
            self.assertNotIn(word, self.code, word)
        self.assertIsNone(re.search(r"static\s+[^;(]*\blayer\w*\s*=", self.code), "a stored layer")
        # Every refusal names its step, and the line names the route.
        spawn = self.body("static void GpSpawn(")
        for step in ("no player", "name not resolved", "result not an instance"):
            self.assertIn(step, spawn + call, step)
        self.assertIn("dispatch failed", call)
        self.assertIn('"gambaprobe spawn: route=" + route + " id="', spawn)
        self.assertIn('" at " + std::to_string((int)x) + ","', spawn)

    def test_scp_and_stamp_routes_and_the_object_in_the_spawn_reply(self):
        """`scp` is the game's stamping spawner by name; `stamp` sets pSpwd; the reply reads the created object."""
        call = self.body("static void GpSpawnCall(")
        spawn = self.body("static void GpSpawn(")
        # scp: the SDK constant's short name, asset_get_index, then
        # script_execute through ApCallScript with the player as self, in both
        # argument orders; the name is never spelled as a literal.
        self.assertIn("SdkShortScriptName(HeroSiege::Scripts::gml_Script_sCP)", call)
        self.assertIn("gml_Script_sCP", sdk_scripts())
        self.assertIn('"asset_get_index"', call)
        self.assertIn("ApCallScript(script, self, args, id)", call)
        self.assertIn('scpOrder == "oxy"', call)
        self.assertIn("{ machine, RValue(x), RValue(y) }", call)
        self.assertIn("{ RValue(x), RValue(y), machine }", call)
        self.assertNotIn('"sCP"', self.code)
        # stamp: the game route's creation, then the pSpwd read by the
        # instance-handle rule and GetVariable/SetVariable by name with the
        # player as self and other; the reply carries before=/after=.
        self.assertIn('g_Yytk->CallBuiltin("variable_instance_get", { id, RValue("pSpwd") })', call)
        self.assertIn('GpExtCall(self, "GetVariable", { pSpwd }, before, failed)', call)
        self.assertIn('GpExtCall(self, "SetVariable", { pSpwd, RValue(true) }, after, failed)', call)
        self.assertIn('GpExtCall(self, "GetVariable", { pSpwd }, after, failed)', call)
        self.assertIn("g_Yytk->CallBuiltinEx(result, name, self, self, args)", self.body("static bool GpExtCall("))
        self.assertIn('" pSpwd key="', call)
        self.assertIn('" before="', call)
        self.assertIn('" after="', call)
        self.assertIn("no pSpwd on the instance", call)
        # Every route's reply names the created instance's object index, read
        # by the instance-handle rule, so a wrong argument order is visible.
        self.assertIn("GpObjectIndexOf(id)", spawn)
        self.assertIn('" object=" + std::to_string(obj)', spawn)

    # ---- replan 1: the caller walk -----------------------------------------------

    def test_the_caller_walk_runs_before_the_original_and_names_frames_by_game_rows(self):
        """`cleanup-caller` is read from these lines: they must print before CleanUp_0 runs, from what the instance is."""
        event = self.body("static void GpOnEvent(")
        walk = "if (g_GpCore.TakeCallerWalk(static_cast<GpNs::Event>(event))) GpCallerWalk(event, S, id);"
        self.assertIn(walk, event)
        self.assertLess(event.index(walk), event.index("if (t.orig) t.orig(S, O);\n    }"))
        self.assertLess(event.index("if (seen != GpNs::Seen::Machine)"), event.index(walk))
        caller = self.body("static void GpCallerWalk(")
        self.assertIn('"-caller id="', caller)
        for value in ("object_index", "x", "y", "layer", "depth"):
            self.assertIn('GpInstanceNumber(self, "' + value + '")', caller)
        self.assertIn('" alarm9="', caller)
        self.assertIn('" alarm11="', caller)
        self.assertIn("RtlCaptureStackBackTrace(1, GpNs::kCallerWalkFrames, frames, nullptr)", caller)
        self.assertIn("GpNs::FrameText(", caller)
        # Printed only: the walk keeps nothing but the busy flag it sets.
        self.assertEqual(set(re.findall(r"\b(g_Gp\w+)\s*(?:=[^=]|\.push_back|\.emplace)", caller)), {"g_GpBusy"})
        self.assertIn("variable_instance_get", self.body("static std::string GpInstanceNumber("))
        # The rows a frame is named by are game code only, read on every hook
        # before any row is installed.
        rows = self.body("static size_t GpLoadCodeRows(")
        self.assertLess(rows.index("AddrIsExecutableInModule(mainMod, e->function)"), rows.index("rows.push_back("))
        self.assertIn("FrameProfGmlAnchor()", rows)
        self.assertIn("GpNs::SortCodeRows(rows);", rows)
        install = self.body("static void GpInstall()")
        self.assertLess(install.index("GpLoadCodeRows(mainMod)"), install.index("GpInstallEvents(mainMod);"))
        # The budget and the frame text are the header's, tested whole there.
        for name in ("kCallerWalksPerRow = 4", "kCreateWalksPerWindow = 1", "kCallerWalkFrames = 24"):
            self.assertIn("inline constexpr int " + name + ";", self.header)
        frame = braced_block(self.header, "const CodeModule& game, const CodeModule& plugin, const CodeModule& other)\n{")
        for prefix in ('"gml:"', '"exe+"', '"forgepact+"', '"?"'):
            self.assertIn(prefix, frame)
        self.assertIn("c.walked = 0;", braced_block(self.header, "void ResetTrace()\n    {"))

    # ---- replan 1: the by-name route ---------------------------------------------

    def test_the_byname_route_is_read_and_printed_for_every_script_row(self):
        """`byname-resolve` reads every script row's hook line; it must end with the by-name word."""
        scripts = self.body("static void GpInstallScripts(HMODULE mainMod)")
        # Read before any row is installed.
        first = scripts.index("GpInstallByName(mainMod);")
        for later in ("GpInstallScriptHolder(", "HookOneScript(", "GpDetourUnder("):
            self.assertLess(first, scripts.index(later), later)
        # Every script row line goes through the one formatter, which ends
        # with idx=<short>/<gml_Script_> byname=<word>.
        self.assertGreater(scripts.count("Out("), 3)
        self.assertEqual(scripts.count("Out("), scripts.count("Out(GpScriptRowLine(t, "))
        line = self.body("static std::string GpScriptRowLine(")
        self.assertTrue(line.rstrip().rstrip(";").rstrip().endswith("GpNs::ByNameText(t.byname, t.shortName, t.fullName)"), line)
        text = braced_block(self.header, "const NameLookup& shortName, const NameLookup& fullName)\n{")
        self.assertIn('" byname=" + std::string(ByNameWord(b))', text)
        self.assertTrue(text.strip().startswith('return "idx=" + IndexText(shortName) + "/" + IndexText(fullName)'), text)
        for word in ("same", "detoured", "shared", "missing"):
            self.assertIn('return "' + word + '";', self.header)
        # Both names: the short one and the SDK constant's own value.
        by_name = self.body("static void GpInstallByName(")
        self.assertIn("t.shortName = GpLookUpName(t.label, mainMod);", by_name)
        self.assertIn("t.fullName = GpLookUpName(t.sdkName, mainMod);", by_name)
        self.assertIn("GetNamedRoutineIndex(name, &index)", self.body("static GpNs::NameLookup GpLookUpName("))
        self.assertIn("if (g_GpByNameRead) return;", by_name)
        # Each by-name detour sits behind AddrIsExecutableInModule and the
        # name still resolving to the routine, through HookBuiltin, in the
        # shared slot attacher GpInstallByName calls (and fnwalk re-runs).
        attach = self.body("static void GpAttachByNameSlots(HMODULE mainMod, int from)")
        self.assertLess(attach.index("AddrIsExecutableInModule(mainMod, (const void*)b.routine)"), attach.index("HookBuiltin("))
        self.assertLess(attach.index("GetNamedRoutinePointer(b.name.c_str(), &now)"), attach.index("HookBuiltin("))
        self.assertEqual(attach.count("HookBuiltin("), 1)
        self.assertIn("GpAttachByNameSlots(mainMod, 0);", by_name)
        self.assertIn("GpReclassifyByName();", by_name)
        self.assertIn("n.routineIsGameCode = p && AddrIsExecutableInModule(mainMod, p);",
                      self.body("static GpNs::NameLookup GpLookUpName("))
        # status prints it per script row, and a shared routine as its own row.
        status = self.body("static void GpStatus()")
        self.assertIn("GpByNameStatusTail(i)", status)
        self.assertIn("GpByNameLabel(s)", status)
        self.assertIn("GpNs::ByNameText(t.byname, t.shortName, t.fullName)", self.body("static std::string GpByNameStatusTail("))
        self.assertIn('"byname-shared "', self.body("static std::string GpByNameLabel("))
        # A by-name call is counted and described, never answered.
        detour = self.body("static void GpOnByName(")
        self.assertIsNone(re.search(r"\bResult\s*=[^=]", detour), "the by-name detour writes a result")
        self.assertNotIn("DecideRng", detour)
        self.assertIn("if (g_GpBusy ||", detour)

    def test_fnwalk_locates_the_array_by_validation_and_detours_through_the_byname_slots(self):
        """`fnwalk` resolves three seed names, checks eight entries behind AddrIsExecutableInModule, and never reads an RVA."""
        walk = self.body("static void GpFnWalk(")
        # The three seeds resolve by name through GetNamedRoutinePointer.
        for name in ("camera_create", "is_undefined", "instance_create_layer"):
            self.assertIn('GpRoutineOf("' + name + '", ok)', walk)
        self.assertIn("GetNamedRoutinePointer(name, &p)", self.body("static uintptr_t GpRoutineOf("))
        # The eight-entry validation sits behind AddrIsExecutableInModule.
        entry = self.body("static bool GpEntryValid(")
        self.assertIn("AddrIsExecutableInModule(mainMod,", entry)
        self.assertIn("for (int i = 0; i < 8; ++i)", walk)
        # A failed validation changes nothing and names the check.
        self.assertIn('"gambaprobe fnwalk: table not found (', walk)
        # Detours only through GpInstallByName's by-name slot path: no
        # HookBuiltin of its own, the shared slot attacher runs it.
        self.assertNotIn("HookBuiltin(", walk)
        self.assertIn("GpAttachByNameSlots(mainMod, startSlot)", walk)
        self.assertIn("GpReclassifyByName();", walk)
        # citrace dispatchdump's RVA is not a seed: never read here.
        self.assertNotIn("kCiDispatchTablePtrRvaDefault", walk)
        self.assertNotIn("kCiDispatchTablePtrRvaDefault", self.code)

    def test_the_hook_timing_line_is_printed_after_the_summary(self):
        """`hook: took` times each phase with the performance counter, after the rows summary."""
        install = self.body("static void GpInstall()")
        self.assertIn("QueryPerformanceCounter(&t0)", install)
        self.assertIn('"gambaprobe hook: took "', install)
        for word in ("(events ", ", byname ", ", scripts ", ", builtins ", ", table "):
            self.assertIn(word, install)
        self.assertLess(install.index('" rows, "'), install.index('"gambaprobe hook: took "'))

    # ---- replan 1: the new rows, the machine-arg rule and selftest ----------------

    def test_the_new_rows_are_named_builtins_and_count_a_machine_argument(self):
        rows = re.findall(r'\{\s*"(\w+)",\s*AnswerKind::(\w+)\s*\}', self.header)
        for name in PLAN_BUILTINS[8:]:
            self.assertIn((name, "NotRng"), rows, name + " is not a never-answered builtin row")
        self.assertEqual(len(re.findall(r'"fp_gp_b_\w+"', self.code)), len(PLAN_BUILTINS), "one hook id per builtin row")
        checks = braced_block(self.header, "inline constexpr bool BuiltinChecksArgument(Builtin b)\n{")
        self.assertEqual(sorted(re.findall(r"Builtin::(\w+)", checks)), sorted(ARGUMENT_BUILTINS))
        # The argument is read before the original runs (instance_destroy may
        # end it), and only off the idle path.
        builtin = self.body("static void GpOnBuiltin(")
        logged = builtin[builtin.index("const bool byArg"):]
        self.assertLess(logged.index("GpMachineArg("), logged.index("if (t.orig) t.orig(Result, S, O, argc, Args);"))
        arg = self.body("static GpNs::ArgTarget GpArgTarget(")
        self.assertIn('"object_is_ancestor"', arg)
        self.assertIn("GpObjectIndexOf(", arg)
        self.assertIn("NoteMachineArg(row, seen, g_GpCore.ArgNamesMachine(target))", self.body("static bool GpMachineArg("))
        self.assertIn('" machine-arg="', braced_block(self.header, "std::string RowText(int row) const\n    {"))

    def test_selftest_calls_irandom_outside_the_busy_guard(self):
        """`selftest-rng`: the irandom row must see the probe's own call, so it may not be the probe's busy call."""
        self.assertIn('if (sub == "selftest") { GpSelfTest(); return; }', self.body("static void GpCommand("))
        test = self.body("static void GpSelfTest()")
        self.assertNotIn("g_GpBusy", test)
        self.assertIn('g_Yytk->CallBuiltin("irandom", { RValue(100.0) })', test)
        self.assertIn('"gambaprobe selftest: irandom row calls=" + std::to_string(before) + " -> " + std::to_string(after)', test)
        self.assertLess(test.index("const uint64_t before"), test.index('CallBuiltin("irandom"'))
        self.assertLess(test.index('CallBuiltin("irandom"'), test.index("const uint64_t after"))

    # ---- phase 4: the explosion watch -------------------------------------------

    def test_the_machine_sprite_is_read_by_name_with_no_kind_check(self):
        """`sprite-control`: the tick names each machine's sprite through sprite_get_name, never a sprite index."""
        sprite = self.body("static std::string GpSpriteName(const RValue& handle)")
        self.assertIn('g_Yytk->CallBuiltin("variable_instance_get", { handle, RValue("sprite_index") })', sprite)
        self.assertIn('g_Yytk->CallBuiltin("sprite_get_name", { spr })', sprite)
        self.assertIn('return "?";', sprite)
        self.assertLess(sprite.index('"sprite_exists"'), sprite.index('"sprite_get_name"'))
        for word in ("m_Kind", "VALUE_"):
            self.assertNotIn(word, sprite)
        # No sprite is spelled or numbered: the watch prints whatever name the
        # machine shows, so the explosion's sprite need not be known.
        self.assertNotIn("_spr", without_strings(self.code))
        self.assertNotIn('_spr"', self.code)
        machines = self.body("static void GpRefreshMachines()")
        self.assertIn("found.push_back({ inst, GpIdOf(handle), GpSpriteName(handle) });", machines)
        self.assertIn("g_GpMachinesRead = read;", machines)
        # The tick feeds the watch only after a refresh that ran to its end,
        # so a refresh that threw cannot report every machine gone.
        tick = [line.strip() for line in self.body("static void GpFrameTick()").split("\n") if line.strip()]
        self.assertEqual(tick, ["if (!g_GpCore.Active()) return;", "GpRefreshMachines();", "GpWatchTick();"])
        watch_tick = self.body("static void GpWatchTick()")
        order = [watch_tick.index(s) for s in ("g_GpWatch.Tick(frame)", "if (!g_GpMachinesRead) return;",
                                               "g_GpWatch.Poll(frame, seen)")]
        self.assertEqual(order, sorted(order))

    def test_every_build_row_goes_into_the_ring_before_the_self_filter(self):
        """A build in the transition's own step runs before the end-of-frame poll: the ring keeps it, any self."""
        build = self.body("static bool GpIsBuildRow(int script)")
        cases = set(re.findall(r"case kGpScript_(\w+):", build))
        self.assertEqual(cases, {"CreateDefaultParams", "GetUniqueRepoStruct", "LootGroundCreate", "LootGroundCreateFromItem",
                                 "CreateLootInFreePos", "CreateItemNew", "DropItem", "DropUniqueItems"})
        for row in cases:
            self.assertIn(row, [safe for safe, _, _ in self.rows], row + " is not a script row")
        script = self.body("static RValue& GpOnScript(")
        self.assertLess(script.index("GpWatchScript("), script.index("GpTraceScript("),
                        "the ring push must come before the machine-self filter")
        self.assertNotIn("Seen::Other", script[:script.index("GpWatchScript(")])
        script_watch = self.body("static bool GpWatchScript(")
        self.assertIn("args = GpScriptArgs(argc, A, key);", script_watch)
        self.assertIn("return GpWatchCall(label, seen, S, argc, args, selfText);", script_watch)
        watch = self.body("static bool GpWatchCall(")
        self.assertIn("g_GpWatch.Push({ label, selfText, argc, args, frame });", watch)
        self.assertIn("selfText = GpSelfText(seen, S);", watch)
        for body in (watch, script_watch):
            self.assertNotIn("Seen::Machine", body)
            self.assertNotIn("Seen::Other", body)
        # Header: the build ring and the instance ring are apart, so a burst
        # of instance calls cannot evict a build.
        push = braced_block(self.header, "void Push(RingCall call)\n    {")
        self.assertIn("std::deque<RingCall>& ring = instance ? instanceRing_ : ring_;", push)
        # Round 2: a full ring drops its oldest in constant time.
        self.assertIn("if (ring.size() > size) ring.pop_front();", push)
        self.assertNotIn("erase(", push)
        self.assertRegex(self.header, r"inline constexpr int kWatchInstanceRingSize = (2[5-9]\d|[3-9]\d\d);")

    def test_the_byname_route_feeds_the_watch_for_build_rows(self):
        """A build row reached by name (a by-name slot) is a build-row call too, and CreateItemNew's built item is read."""
        by_name = self.body("static bool GpWatchByName(")
        self.assertIn("std::none_of(b.scripts.begin(), b.scripts.end(), GpIsBuildRow)", by_name)
        self.assertIn("GpWatchCall(GpByNameLabel(slot), seen, S, argc, args, selfText)", by_name)
        self.assertIn("kGpScript_CreateItemNew", by_name)
        on_by_name = self.body("static void GpOnByName(")
        feed = "const bool readBuilt = GpWatchByName(slot, seen, S, argc, Args, watchSelf, fed);"
        self.assertIn(feed, on_by_name)
        self.assertLess(on_by_name.index(feed),
                        on_by_name.index("if (seen == GpNs::Seen::Idle || seen == GpNs::Seen::Other || !GpCanLog(row)) {"),
                        "the by-name feed must come before the machine-self filter")
        self.assertEqual(on_by_name.count("if (readBuilt) GpWatchBuilt(Result, watchSelf);"), 2,
                         "the built item is read after the original on both return paths")
        for path in on_by_name.split("if (readBuilt) GpWatchBuilt(Result, watchSelf);")[:2]:
            self.assertIn("if (b.orig) b.orig(Result, S, O, argc, Args);", path[-200:])
            # Round 2: the original runs under the by-name marker, so the
            # script row it reaches is not fed a second time.
            self.assertIn("GpByNameFedScope scope(mark);", path[-200:])
        self.assertIn("const int mark = fed ? slot : g_GpByNameFed;", on_by_name)
        self.assertIn("static thread_local int g_GpByNameFed = -1;", self.code)
        fed_by_name = self.body("static bool GpFedByName(int script)")
        self.assertIn("g_GpByNameSlots[g_GpByNameFed].scripts", fed_by_name)
        script = self.body("static RValue& GpOnScript(")
        self.assertLess(script.index("!fedByName"), script.index("GpWatchScript("))
        # Phase 5 (N1): the mark is consumed by the first call the slot
        # routes, read before anything else, and this call's original runs
        # with no mark, so a later call inside the same by-name original is
        # fed as its own.
        self.assertIn("return GpNs::ConsumeFedMark(g_GpByNameFed, ", fed_by_name)
        consume = braced_block(self.header, "inline bool ConsumeFedMark(int& mark, bool slotRoutesThisScript)\n{")
        self.assertIn("mark = -1;", consume)
        order = [script.index(s) for s in ("const bool fedByName = GpFedByName(script);", "GpByNameFedScope unmarked(-1);",
                                           "g_GpCore.Observe(")]
        self.assertEqual(order, sorted(order))
        self.assertEqual(script.count("GpFedByName("), 1)

    def test_a_window_prints_its_lines_for_any_self_apart_from_the_trace_budget(self):
        watch = self.body("static bool GpWatchCall(")
        self.assertIn("if (window && g_GpWatch.TakeBuildLine(frame)) Out(GpNs::WindowCallLine(label, selfText, argc, args, frame));",
                      watch)
        self.assertLess(watch.index("const bool window = g_GpWatch.InWindow(frame);"), watch.index("g_GpWatch.Push("))
        # Instance calls: their own ring (pushed whether or not a window is
        # open, so a window replays the transition step's creates) and their
        # own cap, per created object; they never take a build line.
        builtin = self.body("static void GpWatchBuiltin(")
        self.assertIn("g_GpWatch.Push({ label, self, argc, args, frame, GpNs::CallKind::Instance, key });", builtin)
        self.assertIn("if (window && g_GpWatch.TakeInstanceLine(frame, key)) Out(GpNs::WindowCallLine(label, self, argc, args, frame));",
                      builtin)
        self.assertLess(builtin.index("g_GpWatch.Push("), builtin.index("TakeInstanceLine("))
        self.assertNotIn("TakeBuildLine", builtin)
        self.assertIn("const std::string key = GpInstanceKey(builtin, S, O, argc, Args, target, targetRead);", builtin)
        key = self.body("static std::string GpInstanceKey(")
        self.assertIn("GpValueText(Args[3])", key)
        self.assertIn("target = GpArgTarget(Args[0], S, O);", key)
        self.assertIn("targetRead = true;", key)
        # Round 2: the first refusal of an object's share is named, and the
        # by-argument rule reuses the watch's read of instance_destroy's
        # argument instead of reading it again.
        self.assertLess(builtin.index("TakeInstanceLine("), builtin.index("g_GpWatch.TakeCappedLines(frame)"))
        machine_arg = self.body("static bool GpMachineArg(")
        self.assertIn("if (known) target = *known;", machine_arg)
        self.assertLess(machine_arg.index("if (known) target = *known;"), machine_arg.index("GpArgTarget(Args[0], S, O)"))
        take_capped = braced_block(self.header, "if (perObject >= kWindowInstanceLinesPerObject) {")
        self.assertIn("pendingCapped_.emplace_back(key);", take_capped)
        self.assertIn('"gambaprobe window capped "', self.header)
        take = braced_block(self.header, "bool TakeLine(CallKind kind, std::string_view key, int64_t frame)\n    {")
        self.assertIn("kWindowBuildLineCap", take)
        self.assertIn("kWindowInstanceLineCap", take)
        self.assertIn("kWindowInstanceLinesPerObject", take)
        # Phase 5 (N2): each overall cap's first refusal in a window queues one
        # `full` line, which the adapter prints after a refused build line as
        # it does after a refused instance line.
        self.assertEqual(take.count("NoteFull(kind, frame);"), 2)
        self.assertIn('"gambaprobe window full "', self.header)
        self.assertIn("if (window) for (const std::string& line : g_GpWatch.TakeCappedLines(frame)) Out(line);",
                      self.body("static bool GpWatchCall("))
        built = self.body("static void GpWatchBuilt(")
        refused = braced_block(built, "if (!g_GpWatch.TakeBuildLine(frame)) {")
        self.assertIn("for (const std::string& line : g_GpWatch.TakeCappedLines(frame)) Out(line);", refused)
        # `window` (or a transition) on an open window tops both caps up.
        self.assertIn("TopUp();", braced_block(self.header, "if (open_) {"))
        closed = braced_block(self.header, "inline std::string WindowClosedLine(uint64_t buildLines, uint64_t buildDropped, "
                                           "uint64_t instanceLines, uint64_t instanceDropped,\n"
                                           "                                    uint64_t capped, int64_t frame)\n{")
        for field in ('" build-dropped="', '" instance-dropped="', '" capped="'):
            self.assertIn(field, closed)
        on_builtin = self.body("static void GpOnBuiltin(")
        call = "GpWatchBuiltin(builtin, d.seen, S, O, argc, Args, target, targetRead);"
        self.assertIn(call, on_builtin)
        # After the idle return, before the self filter and the original.
        self.assertLess(on_builtin.index("if (d.seen == GpNs::Seen::Idle) {"), on_builtin.index(call))
        self.assertLess(on_builtin.index(call), on_builtin.index("if ((d.seen == GpNs::Seen::Other && !byArg) || !GpCanLog(row)) {"))
        in_window = braced_block(self.header, "inline constexpr bool BuiltinInWindow(Builtin b)\n{")
        for name in ("InstanceCreateLayer", "InstanceCreateDepth", "InstanceDestroy"):
            self.assertIn("Builtin::" + name, in_window)
        # Window lines spend the window's own cap, never a row's or a key's.
        for signature in ("static bool GpWatchCall(", "static bool GpWatchScript(", "static bool GpWatchByName(",
                          "static void GpWatchBuiltin(", "static void GpWatchBuilt("):
            body = self.body(signature)
            for word in ("GpCanLog", "TakeTraceLine", "GpLogCall"):
                self.assertNotIn(word, body, signature + " spends the trace budget")
        for cap in ("kWindowBuildLineCap", "kWindowInstanceLineCap", "kWindowInstanceLinesPerObject", "kWindowSpanDefault"):
            self.assertRegex(self.header, r"inline constexpr int " + cap + r" = \d+;")
            self.assertIsNone(re.search(r"\b" + cap + r"\s*=\s*\d+;", self.code), cap + " is the header's")
        span = re.search(r"inline constexpr int kWindowSpanDefault = (\d+);", self.header)
        self.assertGreaterEqual(int(span.group(1)), 600, "an automatic window shorter than 600 frames")

    def test_the_built_item_line_is_read_after_create_item_new_returns(self):
        script = self.body("static RValue& GpOnScript(")
        self.assertIn("&& script == kGpScript_CreateItemNew) {", script)
        self.assertLess(script.index("RValue& built = GpTraceScript(t, row, seen, S, O, R, argc, A);"),
                        script.index("GpWatchBuilt(built, watchSelf);"))
        built = self.body("static void GpWatchBuilt(")
        self.assertLess(built.index("if (!g_GpWatch.TakeBuildLine(frame)) {"), built.index("g_GpBusy = true;"))
        refused = braced_block(built, "if (!g_GpWatch.TakeBuildLine(frame)) {")
        self.assertEqual([line.strip() for line in refused.split("\n") if line.strip()][-1], "return;")
        for read in ('TryStructNumber(item, "itemType", type)', 'RValue("itemDefinitionStruct")',
                     'TryStructNumber(def, "j", j)', 'TryStructNumber(def, "b", b)', 'TryStructNumber(def, "c", c)',
                     'RValue("itemInfoStruct")', 'TryStructNumber(info, "27", rarity)', 'StructKey(info, "28")',
                     "GpNs::WindowBuiltLine("):
            self.assertIn(read, built)
        self.assertIn('Out(GpNs::WindowBuiltLine(', built)

    def test_the_window_verb_and_the_status_counters(self):
        command = self.body("static void GpCommand(")
        self.assertIn('if (sub == "window") { GpWindow(tail); return; }', command)
        window = self.body("static void GpWindow(const std::vector<std::string>& tail)")
        self.assertIn("g_GpWatch.Open(GpNs::WindowReason::Command, -1, GpFrame(), GpNs::WindowSpan(frames))", window)
        self.assertLess(window.index("if (!g_GpHooked || !g_GpCore.Active()) {"), window.index("g_GpWatch.Open("))
        self.assertIn('"  window [frames]', self.body("static void GpUsage()"))
        status = self.body("static void GpStatus()")
        self.assertIn("Out(g_GpWatch.StatusLine(GpFrame()));", status)
        # Printed before the `hooked` early return, so `status` always shows it.
        self.assertLess(status.index("Out(g_GpWatch.StatusLine(GpFrame()));"), status.index("if (!g_GpHooked) return;"))
        # Round 2: an open window's end, the current frame and what remains,
        # so the operator can hold it.
        status_line = braced_block(self.header, "std::string StatusLine(int64_t now) const\n    {")
        for field in ('"open end="', '" frame="', '" remaining="', '" capped="'):
            self.assertIn(field, status_line)
        watch_status = braced_block(self.header, "class Watch {")
        for field in ('"gambaprobe watch: machines-seen="', '" transitions="', '" windows="', '" window="', '" ring="',
                      '" instance-ring="', '" build-dropped="', '" instance-dropped="', '" capped="'):
            self.assertIn(field, watch_status)
        off = self.body("static void GpOff()")
        self.assertIn("const std::string closed = g_GpWatch.Off(GpFrame());", off)
        # The gambapity coexistence refusals are untouched (criterion 6 pins
        # the text; this pins that `hook` still refuses).
        self.assertIn("Relaunch without", self.plugin)

    # ---- the research document --------------------------------------------------

    def test_the_research_doc_has_its_nine_headings_in_order(self):
        text = DOC.read_text(encoding="utf-8").replace("\r\n", "\n")
        positions = [text.find("\n## " + heading + "\n") for heading in DOC_HEADINGS]
        self.assertTrue(all(p >= 0 for p in positions), list(zip(DOC_HEADINGS, positions)))
        self.assertEqual(positions, sorted(positions))
        self.assertIn("gambaprobe", text)

    def test_live_procedure_2_names_its_routes_and_checks(self):
        """Live 2 runs `spawn game` and its siblings, `selftest`, and records cleanup-caller and the byname checks."""
        procedure = doc_section(DOC.read_text(encoding="utf-8").replace("\r\n", "\n"), "Live procedure 2")
        for command in LIVE2_COMMANDS:
            self.assertIn(command, procedure, command + " is not in Live procedure 2")
        for check in LIVE2_CHECKS:
            self.assertIn(check, procedure, check + " is not a Live procedure 2 check")

    def test_live_procedure_3_names_its_routes_and_checks(self):
        """Live 3 runs `spawn scp`/`spawn stamp` and `fnwalk` on a surviving machine, and records its checks."""
        procedure = doc_section(DOC.read_text(encoding="utf-8").replace("\r\n", "\n"), "Live procedure 3")
        for command in LIVE3_COMMANDS:
            self.assertIn(command, procedure, command + " is not in Live procedure 3")
        for check in LIVE3_CHECKS:
            self.assertIn(check, procedure, check + " is not a Live procedure 3 check")

    def test_the_decision_keys_are_pending_or_a_listed_label(self):
        """Six keys, each exactly once in ## Decision.

        Before Live 2 a key reads `pending` (Live 1 measured nothing: its
        instrument was blind); once ## Results carries `### Live 2 results`,
        `pending` is no longer an answer and every key names one of its labels
        (drop-route: the script or builtin that placed the prize).
        """
        text = DOC.read_text(encoding="utf-8").replace("\r\n", "\n")
        decision = doc_section(text, "Decision")
        live2 = "\n### Live 2 results" in doc_section(text, "Results")
        keys = header_decision_keys(self.header)
        self.assertEqual([k for k, _, _, _ in keys],
                         ["roll-route", "explosion-rule", "drop-route", "counter-route", "fallback-drop", "pity-design"])
        for key, check, labels, open_name in keys:
            found = re.findall(r"(?m)^[-*\s]*`?" + re.escape(key) + r"`?:\s*`?([\w@-]+)`?", decision)
            self.assertEqual(len(found), 1, key + ": must appear exactly once in ## Decision")
            answer = found[0]
            if live2:
                self.assertNotEqual(answer, "pending", "Live 2 has run; `pending` is no longer an answer for " + key)
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
