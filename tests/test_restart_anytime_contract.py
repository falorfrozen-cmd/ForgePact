"""Contract tests for "Restart zone at any time" (ForgePact issue #8).

Phase 0 is research: `restartprobe`, the research-build instrument that
measures which value the pause menu's Restart is refused on
(docs/restart-always-available-research.md). These tests pin that it stays a
research instrument - absent from the player build, dispatched from its own
helper, nothing of it on the per-frame path - and that it can actually answer
the question: every candidate the static search found is a row, every row is
an SDK name, every detour is validated before the hooking library sees it,
the one write refuses before writing and reads back, and the positive control
is printed where a reader cannot miss it.

Round 2 (after round 1 showed the gate sits upstream of the activation) adds
the UI node API rows, a dump of the Restart button's own instance taken inside
its draw call, a hold that writes only inside a hooked call before the game's
own body runs, a one-argument override, and a path scope resolved by the
existing deep reader. The tests below pin where each of those reads and
writes happens, because "where" is what round 1's overwritten write got wrong.
"""
import re
import sys
import unittest
from pathlib import Path

TESTS_DIR = Path(__file__).resolve().parent
FORGEPACT_DIR = TESTS_DIR.parent
REPO_ROOT = FORGEPACT_DIR.parent
PLUGIN_SRC = FORGEPACT_DIR / "plugin" / "ModuleMain.cpp"
SDK_SCRIPTS_HPP = REPO_ROOT / "hs-game-sdk" / "cpp" / "include" / "hs_game_sdk" / "scripts.hpp"
SDK_OBJECTS_HPP = REPO_ROOT / "hs-game-sdk" / "cpp" / "include" / "hs_game_sdk" / "objects.hpp"
DOC = FORGEPACT_DIR / "docs" / "restart-always-available-research.md"

if str(TESTS_DIR) not in sys.path:
    sys.path.insert(0, str(TESTS_DIR))

from test_release_hook_contract import function_body, strip_research_blocks  # noqa: E402

BLOCK_START = "// ---- restartprobe: pause-menu Restart gate research"
BLOCK_END = "#endif // FORGEPACT_RELEASE (restartprobe)"

# The research doc's § Static search script candidates, as SDK constants.
ROUND0_SCRIPTS = {
    "gml_Script_UiAIngameRestart",
    "gml_Script_UiDrawIngameRestart",
    "gml_Script_ZoneGenRestart",
    "gml_Script_anon_1402_gml_Object_UI_Pause_obj_Create_0",
    "gml_Script_anon_1714_gml_Object_UI_Pause_obj_Create_0",
    "gml_Script_anon_1867_gml_Object_UI_Pause_obj_Create_0",
    "gml_Script_anon_2018_gml_Object_UI_Pause_obj_Create_0",
    "gml_Script_anon_2582_gml_Object_UI_Pause_obj_Create_0",
    "gml_Script_anon_6013_gml_Object_UI_Pause_obj_Create_0",
}

# Round 2: the UI node API (§ Static search › Round 2), all count-only rows.
ROUND2_SCRIPTS = {
    "gml_Script_UiSetRowEnabled",
    "gml_Script_SetGlobalUiEnable",
    "gml_Script_EnableNav",
    "gml_Script_UiSetActivationFunc",
    "gml_Script_UiSetUpdateFunc",
    "gml_Script_UiCreate",
    "gml_Script_UiCreateNode",
    "gml_Script_UiRemoveNode",
    "gml_Script_UiSetFocus",
    "gml_Script_UiSetRef",
    "gml_Script_UiDrawPauseButtonInfo",
    "gml_Script_UiDrawPauseButtonJournalInfo",
    "gml_Script_UiACloseButton",
}

PLANNED_SCRIPTS = ROUND0_SCRIPTS | ROUND2_SCRIPTS

# The one path the round-2 static search added to the sampled set.
PLANNED_PATHS = ("global.tupm[1].in_combat",)

# The four write builtins; each appears once in the block, inside RpWrite.
WRITE_BUILTINS = ('"variable_instance_set"', '"variable_global_set"', '"variable_struct_set"', '"array_set"')

# Round 1's six Decision lines, pinned literally until phase 1 of round 2 has run.
ROUND1_DECISION = (
    "owner: Player_obj (candidate after round 1 - tracks the gate, not shown to be it)",
    "variable: wasInCombat (candidate after round 1 - true while Restart is refused, false when allowed)",
    "readyValue: false (bool; round 1)",
    "gate: upstream (round 1: the refused press never calls UiAIngameRestart; button/node side, not identified)",
    "override: unmeasured (overwritten before use)",
    "shipRoute: pending (round 2)",
)

# The identifier names read from the executable's string table.
PLANNED_VARS = ("in_combat", "isCombat", "wasInCombat", "lastHit",
                "combatRefresh", "aggroTimer", "dpsMeterResetCombat")

PLANNED_SCOPE_OBJECTS = ("Controller_obj", "Player_obj", "UI_Pause_obj")

# The pause menu's Exit is always available and is not researched
# (Decisions, 2026-09-22). Spelled in pieces so this file never names it.
EXIT_ACTIVATION = "UiAIngame" + "Exit"

DOC_HEADINGS = ("## The question", "## Static search", "## Candidates and controls",
                "## Instrument", "## Live procedure", "## Results", "## Decision")
DECISION_KEYS = ("owner", "variable", "readyValue", "gate", "override", "shipRoute")
DOC_NAMES = ("UiAIngameRestart", "UiDrawIngameRestart", "ZoneGenRestart", "UI_Pause_obj",
             "in_combat", "isCombat", "wasInCombat", "lastHit", "combatRefresh", "aggroTimer",
             "dpsMeterResetCombat", "Hud_In_Combat_spr", "PauseRestart")


def macro_rows(source, header):
    """The X(...) rows of a multi-line `#define NAME(X) \\` table."""
    start = source.index(header)
    lines = []
    for line in source[start:].splitlines():
        lines.append(line)
        if not line.rstrip().endswith("\\"):
            break
    rows = []
    for line in lines[1:]:
        m = re.match(r"\s*X\((\w+), HeroSiege::Scripts::(\w+), \"([^\"]+)\", ([^,]+), ([^,]+), ([^)]+)\)", line)
        if m:
            rows.append({"safe": m.group(1), "sdk": m.group(2), "label": m.group(3),
                         "flags": m.group(4).strip(), "orig": m.group(5).strip(), "held": m.group(6).strip()})
    return rows


def doc_section(doc, heading):
    """From a line that is exactly `heading` to the next `## ` heading."""
    start = doc.index("\n" + heading + "\n") + 1
    following = doc.find("\n## ", start + len(heading))
    return doc[start:] if following < 0 else doc[start:following]


class RestartProbeContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.raw = PLUGIN_SRC.read_text(encoding="utf-8")
        cls.plugin = cls.raw.replace("\r\n", "\n")
        cls.player = strip_research_blocks(cls.plugin)
        start = cls.plugin.index(BLOCK_START)
        cls.block = cls.plugin[start:cls.plugin.index(BLOCK_END, start)]
        cls.rows = macro_rows(cls.plugin, "#define RESTARTPROBE_SCRIPTS(X)")
        cls.run_command = function_body(cls.plugin, "static void RunCommand(")
        cls.dispatcher = function_body(cls.plugin, "static bool HandleRestartProbeCommand(")
        cls.attach = function_body(cls.plugin, "static void RestartProbeAttach(")
        cls.set_body = function_body(cls.plugin, "static void RestartProbeSet(")
        cls.show = function_body(cls.plugin, "static void RestartProbeShow(")
        cls.detour = function_body(cls.plugin, "static RValue& RpDetourBody(")
        cls.write = function_body(cls.plugin, "static bool RpWrite(")
        cls.hold_cmd = function_body(cls.plugin, "static void RestartProbeHold(")
        cls.hold_apply = function_body(cls.plugin, "static void RpHoldApply(")
        cls.argset_cmd = function_body(cls.plugin, "static void RestartProbeArgset(")
        cls.argset_apply = function_body(cls.plugin, "static void RpArgsetApply(")
        cls.dump_capture = function_body(cls.plugin, "static bool RpDumpCapture(")
        cls.dump_cmd = function_body(cls.plugin, "static void RestartProbeDump(")
        cls.path_target = function_body(cls.plugin, "static bool RpPathTarget(")
        cls.read_target = function_body(cls.plugin, "static RpReadResult RpReadTarget(")

    def test_restartprobe_is_research_only(self):
        # One literal, in the dispatcher, inside a research block.
        self.assertEqual(self.plugin.count('"restartprobe"'), 1)
        self.assertIn('lc == "restartprobe"', self.dispatcher)
        self.assertNotIn("restartprobe", self.player)
        # The block itself, header comment included, is gone from the player build.
        self.assertNotIn(BLOCK_START, self.player)
        for fn in ("RestartProbeCommand(", "RestartProbeAttach(", "RestartProbeSet(", "RpDetourBody("):
            self.assertIsNone(re.search(r"\b" + re.escape(fn), self.player), fn)
        # Not a player command: the allowlist never names it.
        start = self.run_command.index("kPlayerCommands = {")
        allowlist = self.run_command[start:self.run_command.index("};", start)]
        self.assertNotIn("restartprobe", allowlist)
        # Its own helper, not one more branch of RunCommand's else-if chain (C1061).
        self.assertIn("if (HandleRestartProbeCommand(lc, rest)) return;", self.run_command)
        self.assertNotIn("restartprobe", self.run_command)
        # The helper compiles in both builds and answers false in the player one.
        self.assertIn("static bool HandleRestartProbeCommand(", self.player)
        self.assertIn("return false;", function_body(self.player, "static bool HandleRestartProbeCommand("))

    def test_restartprobe_rows_are_the_static_search_set(self):
        sdks = [r["sdk"] for r in self.rows]
        self.assertEqual(len(sdks), len(set(sdks)))
        self.assertEqual(set(sdks), PLANNED_SCRIPTS)
        # Every row is an SDK constant that exists, never a literal runtime name.
        header = SDK_SCRIPTS_HPP.read_text(encoding="utf-8")
        for name in sdks:
            self.assertRegex(header, r"inline constexpr std::string_view " + re.escape(name) + r" = ", name)
        self.assertNotRegex(self.block, r'"gml_Script_')
        self.assertNotIn(EXIT_ACTIVATION, self.block)
        # The candidate variables and the scopes they are read on.
        for var in PLANNED_VARS:
            self.assertIn(f'"{var}"', self.block, var)
        objects = SDK_OBJECTS_HPP.read_text(encoding="utf-8")
        for obj in PLANNED_SCOPE_OBJECTS:
            self.assertIn(f"HeroSiege::Objects::GameObject::{obj}", self.block, obj)
            self.assertRegex(objects, r"\n\s+" + re.escape(obj) + r" = \d+,", obj)
            self.assertNotIn(f'"{obj}"', self.block, obj)
        # Only the two activation/draw rows sample the candidates at the call;
        # the draw row is the positive control.
        sampling = {r["sdk"] for r in self.rows if "kRpSample" in r["flags"]}
        self.assertEqual(sampling, {"gml_Script_UiAIngameRestart", "gml_Script_UiDrawIngameRestart"})
        control = [r["sdk"] for r in self.rows if "kRpControl" in r["flags"]]
        self.assertEqual(control, ["gml_Script_UiDrawIngameRestart"])
        # ZoneGenRestart attaches under zonegenlog's table-only hook, nothing else is held.
        held = {r["sdk"]: r["orig"] for r in self.rows if r["orig"] != "nullptr"}
        self.assertEqual(held, {"gml_Script_ZoneGenRestart": "&g_OrigZg_ZoneGenRestart"})

    def test_restartprobe_rows_cover_every_sdk_closure_of_ui_pause_obj(self):
        header = SDK_SCRIPTS_HPP.read_text(encoding="utf-8")
        closures = set(re.findall(
            r"inline constexpr std::string_view (gml_Script_anon_\d+_gml_Object_UI_Pause_obj_Create_0) = ", header))
        self.assertEqual(len(closures), 6, sorted(closures))   # six on 2026-09-22
        missing = sorted(closures - {r["sdk"] for r in self.rows})
        self.assertEqual(missing, [], "UI_Pause_obj Create closures the probe table lacks: " + ", ".join(missing))

    def test_restartprobe_attach_validates_before_detouring(self):
        # One place calls the hooking library, and it is the resolver.
        self.assertEqual(self.block.count("MmCreateHook("), 1)
        self.assertIn("MmCreateHook(", self.attach)
        lookup = self.attach.index("GetNamedRoutinePointer")
        check = self.attach.index("AddrIsExecutableInModule(GetModuleHandleA(nullptr)")
        hook = self.attach.index("MmCreateHook(")
        self.assertLess(lookup, check)
        self.assertLess(check, hook)
        # The lambda refuses a non-code target before it reaches MmCreateHook.
        detour = self.attach[self.attach.index("auto detourAt"):]
        self.assertLess(detour.index("return false;"), detour.index("MmCreateHook("))
        for literal in ('"native"', '"native (under table-only "', '"blocked: "', '"not found ('):
            self.assertIn(literal, self.attach)
        # Never a table swap: those are blind to this build's direct calls.
        for call in ("HookOneScript(", "HookOneScriptTable(", "HookRawNamedRoutine(", "HookBuiltin("):
            self.assertNotIn(call, self.block)
        # No address anywhere but the one resolved by name and just validated.
        self.assertNotIn("Rva", self.block)
        self.assertNotIn("g_Base", self.block)
        self.assertIsNone(re.search(r"\b0x1[0-9a-fA-F]{7,}\b", self.block), "a literal code address")

    def test_restartprobe_set_is_confirm_gated_and_reads_back(self):
        body = self.set_body
        refusals = [
            body.index("unknown scope"),
            body.index("has no instance"),
            body.index('"absent"'),
            body.index("if (!RpIsNumeric(before))"),
            body.index('confirm != "confirm"'),
        ]
        self.assertEqual(refusals, sorted(refusals))
        self.assertEqual(body.count("RpWrite("), 1)
        write = body.index("RpWrite(")
        for at in refusals:
            self.assertLess(at, write)
        # Every refusal says nothing was written.
        self.assertGreaterEqual(body.count("nothing written"), 6)
        # One write, then a read-back, then wrote= from the read-back.
        readback = body.index("RValue after;")
        self.assertLess(write, readback)
        self.assertLess(readback, body.index('" wrote="'))
        self.assertIn("after.ToDouble() == value", body)
        # changed= compares the read-back with the value before the write, so
        # a control that writes the value already held cannot pass as a write.
        self.assertIn("after.ToDouble() != before.ToDouble()", body)
        self.assertLess(body.index('" wrote="'), body.index('" changed="'))
        # The four write builtins appear once each in the whole block, all of
        # them inside the one write helper.
        for builtin in WRITE_BUILTINS:
            self.assertEqual(self.block.count(builtin), 1, builtin)
            self.assertIn(builtin, self.write, builtin)
        # The helper has exactly two callers: set, and the hold's apply
        # function (the definition is the third occurrence).
        self.assertEqual(self.block.count("RpWrite("), 3)
        self.assertEqual(self.hold_apply.count("RpWrite("), 1)
        for fn in (self.hold_cmd, self.argset_cmd, self.argset_apply, self.dump_cmd, self.dump_capture, self.detour):
            self.assertNotIn("RpWrite(", fn)
        # Nothing else in the block creates, destroys or restarts anything.
        others = self.block.replace(body, "")
        for word in ('"instance_create', '"instance_destroy"', '"room_restart"', '"game_restart"', "CallGameScriptEx"):
            self.assertNotIn(word, others, word)

    def test_restartprobe_prints_its_positive_control(self):
        first = self.show[:self.show.index("for (")]
        self.assertIn('" control="', first)
        self.assertIn("g_RpRows[kRp_UiDrawIngameRestart]", first)
        # An unattached row prints n/a, never 0.
        calls = function_body(self.plugin, "static std::string RpCallsText(")
        self.assertIn('"n/a"', calls)
        self.assertIn("kRpNative", calls)
        hook = function_body(self.plugin, "static void RestartProbeHook(")
        self.assertIn("control:", hook)

    def test_nothing_of_the_probe_is_on_the_frame_path(self):
        frame = function_body(self.plugin, "void FrameCallback(")
        self.assertNotIn("restartprobe", frame.lower())
        for name in ("RestartProbe", "g_RpRows", "RpScope", "RpRead", "RpDetourBody",
                     "RpHold", "RpArgset", "RpDump", "RpWrite", "RpPath", "g_Rp", "kRp"):
            self.assertNotIn(name, frame, name)
        for line_no, line in enumerate(self.plugin.splitlines(), 1):
            if re.search(r"restartprobe|RpHold|RpArgset|RpDump", line):
                self.assertNotIn(line, frame, line_no)
        # The hook rows sample the candidates inside the hooked call, before the original runs.
        body = self.detour
        self.assertLess(body.index("RpScopeLine(s)"), body.index("t.tramp ? t.tramp("))

    def test_restartprobe_r2_rows_are_the_node_api_set(self):
        r2 = [r for r in self.rows if r["sdk"] in ROUND2_SCRIPTS]
        self.assertEqual({r["sdk"] for r in r2}, ROUND2_SCRIPTS)
        self.assertEqual(len(r2), 13)
        self.assertEqual(len(self.rows), 22)
        for r in r2:
            # Count-only: they log self, argc, three args and the return, and
            # neither sample nor stand in for the control.
            self.assertEqual(r["flags"], "kRpCount", r["sdk"])
            # Nothing of ours holds these scripts, so every row attaches on
            # the game's own code or says why not.
            self.assertEqual(r["orig"], "nullptr", r["sdk"])
            self.assertEqual(r["held"], "nullptr", r["sdk"])
            # The label is the script's own short name.
            self.assertEqual("gml_Script_" + r["label"], r["sdk"], r["sdk"])
        # The round-0 rows come first, in their round-0 order, so the doc's
        # R-rows and the attach listing read the same as in round 1.
        self.assertEqual({r["sdk"] for r in self.rows[:9]}, ROUND0_SCRIPTS)
        # Attached by the one resolver, not a second attach path.
        self.assertIn("for (RestartProbeRow& t : g_RpRows)", function_body(self.plugin, "static void RestartProbeHook("))

    def test_restartprobe_dump_reads_the_detour_self_at_point_of_use(self):
        cap = self.dump_capture
        # Validated by reading through it, never by the handle's kind.
        self.assertLess(cap.index("HhUsableInstance("), cap.index('"variable_instance_get_names"'))
        self.assertNotIn("m_Kind", cap)
        for builtin in ("id", "object_index", "visible", "sprite_index", "image_index",
                        "image_alpha", "depth", "x", "y"):
            self.assertIn(f'"{builtin}"', self.block, builtin)
        self.assertIn("kRpDumpMaxNames", cap)
        self.assertIn("TgProbeDescribeShort(", cap)
        self.assertRegex(self.block, r"kRpDumpMaxNames = 512;")
        self.assertRegex(self.block, r"kRpDumpMaxLabels = 8;")
        self.assertRegex(self.block, r"kRpDumpDiffLines = 300;")
        # `dump button` captures inside the draw row's detour, on the self the
        # game handed that call, before the game's own body runs.
        self.assertLess(self.detour.index("RpDumpFromDetour(idx, S)"), self.detour.index("t.tramp ? t.tramp("))
        from_detour = function_body(self.plugin, "static void RpDumpFromDetour(")
        self.assertIn("kRp_UiDrawIngameRestart", from_detour)
        self.assertIn("RpDumpCapture(S->ToRValue()", from_detour)
        # The command only arms it; `dump pause` reads at command time through
        # the scope finder; show/diff print what was stored.
        cmd = self.dump_cmd
        self.assertIn("armed; open the pause menu, then: restartprobe dump show ", cmd)
        self.assertIn("not captured yet", self.block)
        self.assertIn("RpScopeInstance(", cmd)
        self.assertIn("no instance", cmd)
        for verb in ('"button"', '"pause"', '"show"', '"diff"'):
            self.assertIn(verb, cmd, verb)
        self.assertNotIn("RpDumpCapture(S", cmd)
        diff = function_body(self.plugin, "static void RpDumpDiff(")
        for mark in ('"~ "', '"+ "', '"- "', "kRpDumpDiffLines"):
            self.assertIn(mark, diff, mark)
        # The arming path refuses when the draw row is not attached: an armed
        # dump that can never run would be "armed and doing nothing".
        self.assertIn("kRpNative", cmd)

    def test_restartprobe_hold_writes_only_inside_a_hooked_call(self):
        detour = self.detour
        tramp = detour.index("t.tramp ? t.tramp(")
        self.assertLess(detour.index("RpHoldApply(idx, S)"), tramp)
        self.assertLess(detour.index("RpArgsetApply(idx, argc, A)"), tramp)
        apply = self.hold_apply
        # Entry read, then the write in the kind read at entry, then the
        # read-back - all before the caller goes on to the trampoline.
        entry = apply.index("RpReadTarget(target, entry")
        write = apply.index("RpWrite(")
        readback = apply.index("RpReadTarget(target, back)")
        self.assertLess(entry, write)
        self.assertLess(write, readback)
        self.assertIn("RpNumberInKind(entry, ", apply)
        self.assertLess(apply.index("RpNumberInKind(entry, "), write)
        # entryHeld/entryOther are decided from the entry value, before writing.
        self.assertLess(apply.index("entryHeld"), write)
        self.assertLess(apply.index("entryOther"), write)
        # The button scope is the detour's own self, validated at the point of use.
        self.assertIn("HhUsableInstance(", apply)
        self.assertIn("S->ToRValue()", apply)
        # The command arms; it writes nothing and reads no per-call state.
        for word in WRITE_BUILTINS + ("RpWrite(", "kRpHoldGapFrames", "kRpHoldMaxWrites"):
            self.assertNotIn(word, self.hold_cmd, word)
        # Only the hold's site row reaches the apply function.
        self.assertIn("idx != h.site", apply)

    def test_restartprobe_hold_is_confirm_gated_auto_disarms_and_counts(self):
        cmd = self.hold_cmd
        refusals = [
            cmd.index("unknown scope"),
            cmd.index("has no instance"),
            cmd.index("is absent"),
            cmd.index("if (!RpIsNumeric(before))"),
            cmd.index("is not an attached native row"),
            cmd.index('confirm != "confirm"'),
            cmd.index("std::stod(numberText"),
        ]
        self.assertEqual(refusals, sorted(refusals))
        armed = cmd.index('"restartprobe hold armed: "')
        self.assertLess(max(refusals), armed)
        self.assertGreaterEqual(cmd.count("nothing armed"), 7)
        for word in ('"off"', '"stat"', '"at"'):
            self.assertIn(word, cmd, word)
        # The two disarm rules are constants compared inside the detour.
        self.assertRegex(self.block, r"kRpHoldGapFrames = 3;")
        self.assertRegex(self.block, r"kRpHoldMaxWrites = 20000;")
        apply = self.hold_apply
        write = apply.index("RpWrite(")
        gap = apply.index("> kRpHoldGapFrames")
        cap = apply.index(">= kRpHoldMaxWrites")
        self.assertLess(gap, write)
        self.assertLess(cap, write)
        self.assertIn("hold: disarmed (menu closed at frame ", apply)
        self.assertIn("hold: disarmed (cap)", apply)
        # Every counter is counted in the apply and printed by `hold stat` and `show`.
        stat = function_body(self.plugin, "static std::string RpHoldStatLine(")
        for counter in ("writes", "readbackOk", "entryHeld", "entryOther", "unreadable", "skipped"):
            self.assertIn(f"++h.{counter}", apply, counter)
            self.assertIn(f'" {counter}="', stat, counter)
        for field in ('"armed="', '" site="', '" scope="', '" name="', '" value="', '" lastWriteFrame="'):
            self.assertIn(field, stat, field)
        self.assertLess(self.show.index('" control="'), self.show.index("RpHoldStatLine()"))
        # The first calls after arming are logged into the site row's log.
        self.assertIn("kRpLogBudget", apply)
        self.assertIn('" entry="', apply)
        self.assertIn('" wrote="', apply)
        self.assertIn('" readback="', apply)

    def test_restartprobe_argset_is_budgeted_and_confirm_gated(self):
        cmd = self.argset_cmd
        refusals = [
            cmd.index("is not an attached native row"),
            cmd.index("is not a<i>"),
            cmd.index("std::stod(numberText"),
            cmd.index('confirm != "confirm"'),
        ]
        self.assertEqual(refusals, sorted(refusals))
        self.assertLess(max(refusals), cmd.index('"restartprobe argset armed: "'))
        self.assertGreaterEqual(cmd.count("nothing armed"), 4)
        self.assertIn('"clear"', cmd)
        self.assertIn('"calls="', cmd)
        apply = self.argset_apply
        # Only the named row, only when the argument exists and is a number,
        # and only in the argument's own kind; everything else is counted.
        self.assertIn("idx != a.row", apply)
        replace = apply.index("*A[a.index] = ")
        self.assertLess(apply.index("argc <= a.index"), replace)
        self.assertLess(apply.index("RpIsNumeric(*A[a.index])"), replace)
        self.assertLess(apply.index("++a.skippedArgc"), replace)
        self.assertLess(apply.index("++a.skipped"), replace)
        self.assertIn("RpNumberInKind(*A[a.index], ", apply)
        self.assertIn("++a.applied", apply)
        self.assertIn('" before="', apply)
        self.assertIn('" after="', apply)
        # A budget of N calls, and the hold's gap rule, both inside the detour.
        self.assertIn("a.remaining", apply)
        self.assertLess(apply.index("> kRpHoldGapFrames"), replace)
        # It writes an argument on the stack, never a variable.
        for word in WRITE_BUILTINS + ("RpWrite(",):
            self.assertNotIn(word, apply, word)
            self.assertNotIn(word, cmd, word)

    def test_restartprobe_path_scope_resolves_through_the_deep_reader(self):
        for path in PLANNED_PATHS:
            self.assertIn(f'"{path}"', self.block, path)
        self.assertIn("kRpVarPaths", self.block)
        # The deep reader is the only path resolver the block names.
        resolvers = set(re.findall(r"\bTgProbeDeep\w+\(", self.block))
        self.assertEqual(resolvers, {"TgProbeDeepGet("})
        for other in ("HhResolveInstance(", "N1GetTalentMap(", "N1GetTalentStruct("):
            self.assertNotIn(other, self.block, other)
        # A path target resolves its parent with the deep reader and picks the
        # setter from what the parent is; the read-back is the full path again.
        target = self.path_target
        self.assertIn("TgProbeDeepGet(", target)
        self.assertIn('"is_struct"', target)
        for kind in ("RpTarget::kStruct", "RpTarget::kArray", "RpTarget::kInstance", "RpTarget::kGlobal"):
            self.assertIn(kind, target, kind)
        self.assertIn("TgProbeDeepGet(t.path", self.read_target)
        # vars and both sampling detours print the path line.
        line = function_body(self.plugin, "static std::string RpPathLine(")
        self.assertIn("TgProbeDeepGet(", line)
        self.assertIn('"path:"', line)
        self.assertIn('"unresolved: "', line)
        self.assertIn("RpPathLine(", function_body(self.plugin, "static void RestartProbeVars("))
        self.assertLess(self.detour.index("RpPathLine("), self.detour.index("t.tramp ? t.tramp("))
        # set and hold both take scope `path`.
        self.assertIn('"path"', self.set_body)
        self.assertIn('"path"', self.hold_cmd)
        self.assertIn("RpPathTarget(", self.set_body)
        self.assertIn("RpPathTarget(", self.hold_apply)


class RestartResearchDocTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.raw = DOC.read_bytes()
        cls.doc = cls.raw.decode("utf-8").replace("\r\n", "\n")

    def test_research_doc_headings_and_decision_lines(self):
        # CRLF like every ForgePact doc.
        self.assertEqual(self.raw.count(b"\n"), self.raw.count(b"\r\n"))
        at = [self.doc.index("\n" + h + "\n") for h in DOC_HEADINGS]
        self.assertEqual(at, sorted(at))
        self.assertIn("no decompiled script text", self.doc)
        for name in DOC_NAMES:
            self.assertIn(name, self.doc, name)
        self.assertNotIn(EXIT_ACTIVATION, self.doc)
        # Both controls are named before the procedure.
        controls = doc_section(self.doc, "## Candidates and controls")
        self.assertIn("C1", controls)
        self.assertIn("C2", controls)
        # C2 must be able to fail: it needs a changed value, not just a read-back.
        self.assertIn("wrote=yes changed=yes", controls)
        # A failed R5 is read against the at-press sample, and a negative is
        # labelled with what was supplied (a command-time write).
        self.assertIn("at the press", controls)
        self.assertIn("command-time write", controls)
        decision = doc_section(self.doc, "## Decision")
        for key in DECISION_KEYS:
            lines = re.findall(r"(?m)^" + key + r": (.+)$", decision)
            self.assertEqual(len(lines), 1, key)
        # Round 1 ran and did not identify the gate: its six lines stand,
        # literally, until round 2's live session has run - and no other
        # `key:` line may creep in beside them.
        for line in ROUND1_DECISION:
            self.assertIn("\n" + line + "\n", decision + "\n", line)
        keyed = re.findall(r"(?m)^(\w+): ", decision)
        self.assertEqual(keyed, list(DECISION_KEYS))

    def test_research_doc_round1_results_and_round2_procedure(self):
        results = doc_section(self.doc, "## Results")
        rows = re.findall(r"(?m)^\| (R[1-8]|C[12]) \|(.*)$", results)
        self.assertEqual(sorted(r[0] for r in rows),
                         sorted(["R1", "R2", "R3", "R4", "R5", "R6", "R7", "R8", "C1", "C2"]))
        # The kept round-1 procedure table bolds its step names, so a plain
        # `| R1 |` row anywhere in the doc is a result.
        self.assertEqual(len(re.findall(r"(?m)^\| (?:R[1-8]|C[12]) \|", self.doc)), 10)
        by_step = {step: text for step, text in rows}
        # Printed fragments from the round-1 session, quoted.
        self.assertIn("control=1033", by_step["C1"])
        self.assertIn("UiAIngameRestart calls=0", by_step["R3"])
        self.assertIn("in_combat=real:432", by_step["R2"])
        self.assertIn("UiAIngameRestart calls=1", by_step["R4"])
        self.assertIn("wrote=yes changed=yes", by_step["C2"])
        self.assertIn("37710", by_step["R5"])
        self.assertIn("anon@6013", by_step["R7"])
        self.assertIn("wasInCombat=bool:false", by_step["R1"])
        # Never a value the session did not print.
        self.assertIn("not run", by_step["R6"])
        self.assertIn("not recorded", by_step["R8"])
        # Round 2's static search: the UI node API table, and the reason the
        # round-0 negative did not cover the enabled state.
        static = doc_section(self.doc, "## Static search")
        self.assertIn("\n### Round 2: the UI node API\n", static)
        r2 = static[static.index("### Round 2: the UI node API"):]
        for sdk in ROUND2_SCRIPTS:
            self.assertIn("`" + sdk[len("gml_Script_"):] + "`", r2, sdk)
        self.assertIn("enabled", r2)
        self.assertGreaterEqual(self.doc.count("UiSetRowEnabled"), 2)
        # C3 and C4 are named before the procedure, beside C1 and C2.
        controls = doc_section(self.doc, "## Candidates and controls")
        self.assertEqual(len(re.findall(r"(?m)^- \*\*C[34]", self.doc)), 2)
        self.assertEqual(len(re.findall(r"(?m)^- \*\*C[34]", controls)), 2)
        self.assertIn("readbackOk", controls)
        # The round-2 procedure, S1-S7, under its own heading.
        live = doc_section(self.doc, "## Live procedure")
        self.assertIn("\n### Round 2\n", live)
        round2 = live[live.index("\n### Round 2\n"):]
        self.assertEqual(sorted(re.findall(r"(?m)^\| (S[1-7]) \|", round2)),
                         ["S1", "S2", "S3", "S4", "S5", "S6", "S7"])
        self.assertEqual(len(re.findall(r"(?m)^\| S[1-7] \|", self.doc)), 7)
        for step in ("C3", "C4"):
            self.assertRegex(round2, r"(?m)^\| " + step + r"\b", step)
        # The instrument section documents every round-2 verb.
        instrument = doc_section(self.doc, "## Instrument")
        for verb in ("restartprobe dump", "restartprobe hold", "restartprobe argset", "`path`", "selfIds="):
            self.assertIn(verb, instrument, verb)
        # Status names round 1's result and says round 2 is pending.
        status = self.doc[self.doc.index("**Status.**"):self.doc.index("\n## The question")]
        self.assertIn("round 1", status.lower())
        self.assertIn("round 2", status.lower())
        self.assertNotIn(EXIT_ACTIVATION, self.doc)


if __name__ == "__main__":
    unittest.main()
