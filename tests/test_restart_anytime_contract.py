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

Round 3 identified the gate (the Restart button's own `manualDisable`, written
false inside `UiSetFocus`), and phase 2 ships it as `restartanytime`:
RestartAnytimeContractTests pins what a source read can about the shipped
mod, beside test_restart_anytime_behavior.py, which runs the hook end to end.
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
HEADER = FORGEPACT_DIR / "plugin" / "include" / "ForgePact" / "RestartAnytimeMod.hpp"
SRC_DIR = FORGEPACT_DIR / "src"
NOTES = FORGEPACT_DIR / "release-notes-v1.4.5.md"
README = FORGEPACT_DIR / "README.md"
GUIDE = REPO_ROOT / "docs" / "submodules" / "ForgePact" / "instructions.md"

if str(TESTS_DIR) not in sys.path:
    sys.path.insert(0, str(TESTS_DIR))
if str(SRC_DIR) not in sys.path:
    sys.path.insert(0, str(SRC_DIR))

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

# Round 3's six Decision lines, pinned literally (they replaced round 2's,
# which replaced round 1's; the doc carries only the latest set).
ROUND3_DECISION = (
    "owner: UI_Button_obj (the pause menu's Restart node, told apart from every other node by its own uiNodeCallstack \"PauseRestart\"; round 3)",
    "variable: manualDisable (round 3 T3 - manualDisable alone unlocks the press; enabled alone did not)",
    "readyValue: manualDisable=false (bool; round 3 - written in the kind read at entry)",
    "gate: button-member (manualDisable)",
    "override: works (UiSetFocus site, arg0, enabled=true and manualDisable=false, bool; round 3 T2 - UiAIngameRestart calls=1 at frame 27419 with wasInCombat=bool:true; manualDisable alone the same at frame 49425)",
    "shipRoute: setfocus-write",
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


def result_row(results, step):
    """One plain `| <step> |` result row, Printed and Reading columns together."""
    return re.search(r"(?m)^\| " + re.escape(step) + r" \|(.*)$", results).group(1)


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
        # Round 3: the apply takes the call's arguments too (scope `arg0`).
        self.assertLess(detour.index("RpHoldApply(idx, S, argc, A)"), tramp)
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
            # Round 3: a site other than the draw needs the draw attached,
            # because the draw row's calls are what disarm that hold.
            cmd.index("needs the Restart draw attached"),
            cmd.index('confirm != "confirm"'),
            cmd.index("std::stod(numberText"),
            cmd.index("two holds armed"),
        ]
        self.assertEqual(refusals, sorted(refusals))
        armed = cmd.index('"restartprobe hold armed: "')
        self.assertLess(max(refusals), armed)
        self.assertGreaterEqual(cmd.count("nothing armed"), 9)
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
        self.assertLess(self.show.index('" control="'), self.show.index("RpHoldStatLine("))
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

    def test_restartprobe_hold_arg0_targets_the_call_argument_at_point_of_use(self):
        # Round 3: scope `arg0` is the site call's own argument 0 - the Restart
        # button, when the site is UiSetFocus - resolved on every call. The
        # call must carry the argument, the argument must read as an instance
        # (never a kind check: this runner hands out VALUE_REF), then the
        # member is read by name, then written in the kind read at entry.
        self.assertEqual(self.plugin.count("static void RpHoldApply(int idx, CInstance* S, int argc, RValue** A)"), 1)
        self.assertIn("RpHoldApply(idx, S, argc, A)", self.detour)
        apply = self.hold_apply
        arg0 = apply[apply.index('h.scope == "arg0"'):]
        usable = arg0.index("HhUsableInstance(*A[0])")
        self.assertLess(arg0.index("argc > 0"), usable)
        self.assertLess(arg0.index("A[0]"), usable)
        read = arg0.index("RpReadTarget(target, entry")
        write = arg0.index("RpWrite(")
        self.assertLess(usable, read)
        self.assertLess(read, write)
        self.assertLess(arg0.index("RpNumberInKind(entry, "), write)
        self.assertNotIn("m_Kind", apply)
        # The command accepts the scope, the apply resolves it; like `button`,
        # its member cannot be checked at arming time, so the first call that
        # finds it absent or not a number disarms.
        self.assertIn('"arg0"', self.hold_cmd)
        self.assertIn('"arg0"', apply)
        self.assertRegex(apply, r'h\.scope == "button" \|\| h\.scope == "arg0"')
        # Every instance target is labelled from the instance it resolved to,
        # `<object name>#<id>.<member>` - at a non-draw site the self is not
        # the button, so a `button.` literal would misattribute the write.
        self.assertNotIn('"button." + ', self.block)
        label = function_body(self.plugin, "static std::string RpInstanceLabel(")
        for piece in ('"object_index"', '"object_get_name"', '"id"', '"#"'):
            self.assertIn(piece, label, piece)
        self.assertEqual(apply.count("RpInstanceLabel("), 2)   # button and arg0

    def test_restartprobe_hold_disarm_is_keyed_to_the_menu_not_a_hover_gap(self):
        # Every row records the frame of its last call, in the detour, before
        # the hold runs.
        row = self.plugin[self.plugin.index("struct RestartProbeRow {"):]
        row = row[:row.index("};")]
        self.assertIn("lastCallFrame", row)
        detour = self.detour
        stamp = detour.index("t.lastCallFrame = g_RuntimeFrame;")
        self.assertLess(stamp, detour.index("RpHoldApply(idx, S, argc, A)"))
        apply = self.hold_apply
        write = apply.index("RpWrite(")
        # The site's own 3-frame gap applies only in a branch on the draw row
        # (C3: the draw stops with the menu closed) ...
        branch = apply.index("h.site == kRp_UiDrawIngameRestart")
        own_gap = apply.index("g_RuntimeFrame - h.lastWriteFrame > kRpHoldGapFrames")
        self.assertLess(branch, own_gap)
        self.assertEqual(apply.count("g_RuntimeFrame - h.lastWriteFrame"), 1)
        # ... and any other site compares the draw row's lastCallFrame, so a
        # hover gap on UiSetFocus disarms nothing while the menu is drawn.
        other = apply[own_gap:write]
        self.assertIn("g_RpRows[kRp_UiDrawIngameRestart]", other)
        self.assertIn("draw.lastCallFrame > kRpHoldGapFrames", other)
        self.assertIn("draw.gapTo > h.lastWriteFrame", other)
        self.assertIn("hold: disarmed (menu not drawing since frame ", other)
        self.assertIn("h.writes > 0", other)
        # The draw row's own gap (closed, then reopened) is recorded in the
        # detour, so a reopen between two site calls is seen too.
        self.assertIn("t.gapTo = g_RuntimeFrame;", detour)
        self.assertLess(detour.index("t.gapTo = g_RuntimeFrame;"), stamp)
        # Arming a site other than the draw refuses while the draw row is not
        # attached: that hold would have no menu oracle.
        cmd = self.hold_cmd
        refuse = cmd.index("needs the Restart draw attached")
        self.assertIn("g_RpRows[kRp_UiDrawIngameRestart].mode != kRpNative", cmd)
        self.assertLess(cmd.index("is not an attached native row"), refuse)
        self.assertLess(refuse, cmd.index('confirm != "confirm"'))

    def test_restartprobe_hold_has_two_slots_and_a_run_length_entry_ring(self):
        self.assertRegex(self.block, r"kRpHoldSlots = 2;")
        self.assertRegex(self.block, r"kRpHoldRing = 1024;")
        self.assertRegex(self.block, r"kRpHoldRingLines = 32;")
        self.assertIn("g_RpHold[kRpHoldSlots]", self.block)
        cmd = self.hold_cmd
        # A third hold is refused - never a silent replacement.
        self.assertIn("two holds armed; hold off first", cmd)
        self.assertNotIn("replaced the previous hold", self.block)
        self.assertLess(cmd.index("two holds armed"), cmd.index('"restartprobe hold armed: "'))
        # Arming starts from a fresh state, which clears that slot's ring.
        self.assertIn("RpHoldState()", cmd)
        # `hold off` disarms every slot.
        off = cmd[cmd.index('first == "off"'):cmd.index('first == "stat"')]
        self.assertIn("kRpHoldSlots", off)
        self.assertIn(".armed = false", off)
        # `hold stat` prints each slot's stat line, then that slot's ring.
        stat = cmd[cmd.index('first == "stat"'):]
        stat = stat[:stat.index("return;")]
        self.assertIn("kRpHoldSlots", stat)
        self.assertLess(stat.index("RpHoldStatLine("), stat.index("RpHoldRingLines("))
        self.assertIn('"hold["', function_body(self.plugin, "static std::string RpHoldStatLine("))
        ring = function_body(self.plugin, "static std::vector<std::string> RpHoldRingLines(")
        self.assertIn('"] ring: frames "', ring)
        self.assertIn("kRpHoldRingLines", ring)
        for field in ('" entry="', '" held="', '" wrote="', '" readback="', '" x"'):
            self.assertIn(field, ring, field)
        # The ring is filled inside the apply, once per write, after the read-back.
        apply = self.hold_apply
        self.assertIn("for (int slot = 0; slot < kRpHoldSlots; ++slot)", apply)
        self.assertEqual(apply.count("RpHoldRingPush("), 1)
        self.assertLess(apply.index("RpWrite("), apply.index("RpHoldRingPush("))
        push = function_body(self.plugin, "static void RpHoldRingPush(")
        self.assertIn("kRpHoldRing", push)
        # `show` prints the per-slot stat lines only, not the rings.
        self.assertIn("kRpHoldSlots", self.show)
        self.assertNotIn("RpHoldRingLines(", self.show)


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
        # Round 3 identified the gate: its six lines stand, literally, and no
        # other `key:` line may creep in beside them.
        for line in ROUND3_DECISION:
            self.assertIn("\n" + line + "\n", decision + "\n", line)
        keyed = re.findall(r"(?m)^(\w+): ", decision)
        self.assertEqual(keyed, list(DECISION_KEYS))
        self.assertNotIn("pending", decision)

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
        # The round-2 procedure, S1-S7, under its own heading, its step names
        # in bold now that the plain rows are round 2's results.
        live = doc_section(self.doc, "## Live procedure")
        self.assertIn("\n### Round 2\n", live)
        round2 = live[live.index("\n### Round 2\n"):]
        self.assertEqual(sorted(re.findall(r"(?m)^\| \*\*(S[1-7])\*\* \|", round2)),
                         ["S1", "S2", "S3", "S4", "S5", "S6", "S7"])
        self.assertEqual(re.findall(r"(?m)^\| S[1-7] \|", round2), [])
        for step in ("C3", "C4"):
            self.assertRegex(round2, r"(?m)^\| \*\*" + step + r"\*\* \|", step)
        # The instrument section documents every round-2 verb.
        instrument = doc_section(self.doc, "## Instrument")
        for verb in ("restartprobe dump", "restartprobe hold", "restartprobe argset", "`path`", "selfIds="):
            self.assertIn(verb, instrument, verb)
        # Status names round 1's result and round 2's.
        status = self.doc[self.doc.index("**Status.**"):self.doc.index("\n## The question")]
        self.assertIn("round 1", status.lower())
        self.assertIn("round 2", status.lower())
        self.assertNotIn(EXIT_ACTIVATION, self.doc)

    def test_research_doc_round2_results_and_round3_procedure(self):
        results = doc_section(self.doc, "## Results")
        rows = re.findall(r"(?m)^\| (S[1-7]|C[34]) \|(.*)$", results)
        self.assertEqual(sorted(r[0] for r in rows),
                         sorted(["C3", "C4", "S1", "S2", "S3", "S4", "S5", "S6", "S7"]))
        # Plain rows are results; the procedure tables bold their step names.
        self.assertEqual(len(re.findall(r"(?m)^\| (?:S[1-7]|C[34]) \|", self.doc)), 9)
        self.assertEqual(len(re.findall(r"(?m)^\| \*\*(?:S[1-7]|C[34])\*\* \|", self.doc)), 9)
        by_step = {step: text for step, text in rows}
        # Printed fragments from the round-2 session, quoted.
        self.assertIn("control=0", by_step["C3"])
        self.assertIn("selfIds=262247", by_step["S1"])
        self.assertIn("enabled=bool:true", by_step["S1"])
        self.assertIn("manualDisable=bool:false", by_step["S1"])
        self.assertIn("enabled: true -> false", by_step["S2"])
        self.assertIn("UiACloseButton calls=1", by_step["S3"])
        self.assertIn("UiAIngameRestart calls=0", by_step["S3"])
        self.assertIn("readbackOk=1260", by_step["C4"])
        self.assertIn("entryHeld=7179 entryOther=51", by_step["S4"])
        self.assertIn("entryHeld=0 entryOther=5010", by_step["S5"])
        self.assertIn("entryHeld=0 entryOther=10380", by_step["S5"])
        # Never a value the session did not print.
        self.assertIn("not recorded", by_step["S6"])
        self.assertIn("not recorded", by_step["S7"])
        # Round 1's rows are untouched in count.
        self.assertEqual(len(re.findall(r"(?m)^\| (?:R[1-8]|C[12]) \|", self.doc)), 10)
        # The round-2 review's wording fixes: a zero on a row with no positive
        # control is "not observed", and the step order is an assumption.
        self.assertNotIn("does not go through", self.doc)
        self.assertIn("no positive control on that row", result_row(results, "R4"))
        self.assertEqual(self.doc.count("the assumption is that instances step in creation order"), 1)
        # C5 is named before the procedure.
        controls = doc_section(self.doc, "## Candidates and controls")
        self.assertEqual(len(re.findall(r"(?m)^- \*\*C5", self.doc)), 1)
        self.assertEqual(len(re.findall(r"(?m)^- \*\*C5", controls)), 1)
        # The round-3 procedure, after round 2's, with bolded step names.
        live = doc_section(self.doc, "## Live procedure")
        self.assertLess(live.index("\n### Round 2\n"), live.index("\n### Round 3\n"))
        round3 = live[live.index("\n### Round 3\n"):]
        self.assertEqual(sorted(re.findall(r"(?m)^\| \*\*(C5|T[1-6])\*\* \|", round3)),
                         ["C5", "T1", "T2", "T3", "T4", "T5", "T6"])
        self.assertIn("setfocus-write", round3)
        # The instrument documents the arg0 scope, the two slots, the ring and
        # the draw-keyed disarm.
        instrument = doc_section(self.doc, "## Instrument")
        self.assertGreaterEqual(instrument.count("arg0"), 3)
        self.assertIn("menu not drawing", instrument)
        self.assertGreaterEqual(instrument.count("ring"), 2)
        self.assertIn("two holds armed", instrument)
        # Status names round 3.
        status = self.doc[self.doc.index("**Status.**"):self.doc.index("\n## The question")]
        self.assertIn("round 3", status.lower())
        self.assertNotIn("round 2's live session is pending", self.doc)

    def test_research_doc_round3_results_and_decision(self):
        results = doc_section(self.doc, "## Results")
        rows = re.findall(r"(?m)^\| (C5|T[1-6]) \|(.*)$", results)
        self.assertEqual(sorted(r[0] for r in rows), ["C5", "T1", "T2", "T3", "T4", "T5", "T6"])
        self.assertEqual(len(re.findall(r"(?m)^\| (?:C5|T[1-6]) \|", self.doc)), 7)
        by_step = {step: text for step, text in rows}
        # Printed fragments from the round-3 session, quoted.
        self.assertIn("UiSetFocus calls=450", by_step["T1"])
        self.assertIn("selfIds=262264", by_step["T1"])
        self.assertIn("writes=450 readbackOk=450", by_step["C5"])
        self.assertIn("UI_Button_obj#264101", by_step["C5"])
        # T2 quotes both slots' stat lines and the ring.
        self.assertIn("UiAIngameRestart calls=1", by_step["T2"])
        self.assertIn("wasInCombat=bool:true", by_step["T2"])
        for slot in ("`hold[0]`", "`hold[1]`"):
            self.assertIn(slot, by_step["T2"], slot)
        self.assertIn("writes=4560 readbackOk=4560 entryHeld=0 entryOther=4560", by_step["T2"])
        self.assertIn("ring: frames 26396..27419 x1024", by_step["T2"])
        self.assertIn("UiAIngameRestart calls=0", by_step["T3"])
        self.assertIn("frame 49425", by_step["T3"])
        self.assertIn("not run", by_step["T4"])
        self.assertIn("menu not drawing since frame 93865", by_step["T5"])
        self.assertIn("20260922T203033Z_pre-restartprobe-r3", by_step["T6"])
        # The round-3 rule: `override: works` stands only on C1 and C5 passing
        # in the same session, with the press reaching the activation.
        decision = doc_section(self.doc, "## Decision")
        override = re.search(r"(?m)^override: (.+)$", decision).group(1)
        if override.startswith("works"):
            round3 = results[results.index("Round 3's session"):]
            self.assertIn("C1 passed", round3[:round3.index("| Step |")])
            self.assertRegex(round3[:round3.index("| Step |")], r"control=[1-9]\d*")
            self.assertIn("Pass", by_step["C5"].split("|")[-2])
            self.assertIn("override: works", by_step["T2"])
        else:
            self.assertRegex(override, r"^(not observed|unmeasured) \(")
        self.assertIn(re.search(r"(?m)^shipRoute: (.+)$", decision).group(1), ("setfocus-write", "none"))
        # Exit is never researched, here either.
        self.assertNotIn(EXIT_ACTIVATION, self.doc)


class RestartAnytimeContractTests(unittest.TestCase):
    """The shipped mod, `restartanytime` (phase 2, `shipRoute: setfocus-write`).

    Companion to test_restart_anytime_behavior.py, which runs the real hook
    body end to end. This pins what a source read can: a real player command
    that installs nothing itself; one `UiSetFocus` install, in FrameCallback,
    behind the toggleguard gate, through both routes, turning the mod off when
    the inline detour did not go in; the off fast path first; the button
    identified by its own member at the call; every counter printed; the
    panel; and the docs.
    """

    @classmethod
    def setUpClass(cls):
        cls.plugin = PLUGIN_SRC.read_text(encoding="utf-8").replace("\r\n", "\n")
        cls.player = strip_research_blocks(cls.plugin)
        cls.header = HEADER.read_text(encoding="utf-8").replace("\r\n", "\n")
        cls.hook = function_body(cls.plugin, "static RValue& HookRestartAnytimeSetFocus(")
        cls.install = function_body(cls.plugin, "static void RestartAnytimeInstall(")
        cls.frame = function_body(cls.plugin, "void FrameCallback(")
        start = cls.plugin.index('if (lc == "restartanytime")')
        cls.branch = cls.plugin[start:cls.plugin.index('if (lc == "toggleborder")', start)]

    def test_restartanytime_is_a_player_command(self):
        match = re.search(r"kPlayerCommands = \{(.*?)\};", self.plugin, re.S)
        self.assertIsNotNone(match)
        self.assertIn('"restartanytime"', match.group(1))
        # The allowlist entry and the dispatcher, both in the player build.
        self.assertGreaterEqual(self.player.count('"restartanytime"'), 2)
        self.assertIn('if (lc == "restartanytime")', function_body(self.player, "static void RunCommand("))
        # A standalone early return (C1061), not part of the research dispatcher.
        self.assertNotIn("restartanytime", function_body(self.plugin, "static bool HandleRestartProbeCommand("))
        self.assertIn("return;\n    }", self.branch)
        # The decision core is its own header, game-independent.
        includes = [line.strip() for line in self.header.splitlines() if line.strip().startswith("#include")]
        self.assertEqual(includes, ['#include "Common.hpp"'])
        for forbidden in ("g_Yytk", "CallBuiltin", "RValue", "CInstance"):
            self.assertNotIn(forbidden, self.header, forbidden)
        self.assertIn("#include <ForgePact/RestartAnytimeMod.hpp>", self.plugin)

    def test_hook_installs_lazily_from_framecallback_with_both_routes(self):
        needle = "RestartAnytimeInstall();"
        self.assertEqual(self.plugin.count(needle), 1)
        self.assertIn(needle, self.frame)
        call = self.frame.index(needle)
        gate = self.frame.rindex("if (ForgePact::RestartAnytimeMod::Instance().IsPending()", 0, call)
        enclosing = self.frame[gate:call]
        for name in ("g_Setup", "HhResolveLocalPlayer", "(fc % 60) == 0", "ClearPending()"):
            self.assertIn(name, enclosing, name)
        self.assertIn(needle, function_body(self.player, "void FrameCallback("))
        # One install on the site, by its SDK name, through HookOneScript
        # (table swap plus inline detour) with the detour's outcome read back.
        self.assertIn("inline constexpr std::string_view kRestartAnytimeSiteScript = "
                      "HeroSiege::Scripts::gml_Script_UiSetFocus;", self.header)
        self.assertEqual(self.plugin.count("HookOneScript(SdkShortScriptName(ForgePact::kRestartAnytimeSiteScript)"), 1)
        self.assertIn("HookOneScript(SdkShortScriptName(ForgePact::kRestartAnytimeSiteScript)", self.install)
        self.assertIn("&g_OrigUiSetFocus, &native)", self.install)
        self.assertNotIn("HookOneScriptTable(", self.install)
        # The command arms; it never installs.
        self.assertNotIn("HookOneScript", self.branch)
        self.assertNotIn("RestartAnytimeInstall", self.branch)

    def test_table_only_install_turns_the_mod_off_and_says_so(self):
        body = self.install
        on = body.index('if (ok && native) { Out("restartanytime: hook installed -> ON"); return; }')
        blind = body.index("MarkBlind();")
        self.assertLess(on, blind)
        self.assertIn('"TABLE-ONLY"', body[blind:])
        self.assertIn("-> OFF", body[blind:])
        # Blind stays off: MarkBlind clears the flag and SetEnabled refuses
        # to turn a blind session back on.
        mark = function_body(self.header, "void MarkBlind(")
        self.assertIn("m_Enabled.store(false);", mark)
        set_enabled = function_body(self.header, "void SetEnabled(bool enabled, bool alreadyHooked)")
        self.assertIn("if (enabled && m_Blind.load())", set_enabled)
        self.assertIn("IsBlind()", self.branch)
        state = function_body(self.player, "static std::string RestartAnytimeHookState(")
        for literal in ('"not installed"', '"installed"', '"TABLE-ONLY"'):
            self.assertIn(literal, state)
        self.assertIn("AddrIsExecutableInModule(GetModuleHandleA(nullptr), (const void*)g_OrigUiSetFocus)", state)

    def test_off_fast_path_reads_nothing(self):
        body = self.hook
        fast = body.index("if (!mod.IsEnabled()) return g_OrigUiSetFocus(S, O, R, argc, A);")
        # Nothing but fetching the mod comes before it.
        self.assertEqual(body[:fast].strip(),
                         "ForgePact::RestartAnytimeMod& mod = ForgePact::RestartAnytimeMod::Instance();")
        for read in ("g_Yytk", "HhUsableInstance(", "RestartAnytimeReadGate("):
            self.assertLess(fast, body.index(read), read)
        self.assertNotIn("#ifndef FORGEPACT_RELEASE", body)
        # The original is called on every path, once each.
        self.assertEqual(body.count("return g_OrigUiSetFocus(S, O, R, argc, A);"), 2)
        self.assertNotIn("return R;", body)

    def test_hook_body_identifies_the_button_by_its_own_member_never_by_position(self):
        body = self.hook
        arg = body.index("argc > 0 && A && A[0] && HhUsableInstance(*A[0])")
        key = body.index("ForgePact::kRestartButtonIdMember")
        gate = body.index("RestartAnytimeReadGate(*A[0], entry)")
        decide = body.index("ForgePact::RestartAnytimeModel::Decide(")
        write = body.index('"variable_instance_set"')
        self.assertLess(arg, key)
        self.assertLess(key, gate)
        self.assertLess(gate, decide)
        self.assertLess(decide, write)
        self.assertIn("kind == VALUE_STRING && key.ToString() == ForgePact::kRestartButtonIdValue", body)
        # The gate is read only once the button is known to be Restart.
        self.assertLess(body.index("if (isRestartButton) {"), gate)
        # Never by position, instance id, the call's self or the probe's ids.
        for forbidden in ("selfIds", "g_RpSelfIds", "A[1]", "S->", "RValue(S)", "ToRValue", "instance_find",
                          "\"id\"", "262264", "264101", "\"enabled\""):
            self.assertNotIn(forbidden, body, forbidden)
        # The write goes to the member the decision names, on a0, in the kind read.
        self.assertIn("{ *A[0], RValue(ForgePact::kRestartGateMember), ready }", body)
        self.assertIn("kind == VALUE_BOOL ? RValue(ForgePact::kRestartGateReadyValue)", body)
        for constant in ('kRestartButtonIdMember = "uiNodeCallstack";', 'kRestartButtonIdValue = "PauseRestart";',
                         'kRestartGateMember = "manualDisable";', "kRestartGateReadyValue = false;"):
            self.assertIn(constant, self.header, constant)
        self.assertIn("enum class RestartAnytimeDecision { Pass, Write };", self.header)

    def test_stat_and_zero_print_every_counter(self):
        line = function_body(self.player, "static std::string RestartAnytimeCountersLine(")
        for key in ("written=", "passed=", "otherNode=", "unreadable=", "hook="):
            self.assertIn(key, line, key)
        self.assertIn('v == "stat"', self.branch)
        self.assertIn('v == "off" || v == "0"', self.branch)
        stat = self.branch[self.branch.index('v == "stat"'):self.branch.index('v == "off" || v == "0"')]
        self.assertIn("RestartAnytimeCountersLine()", stat)
        self.assertNotIn("SetEnabled", stat)
        off = self.branch[self.branch.index('v == "off" || v == "0"'):]
        self.assertIn('"restartanytime -> off " + RestartAnytimeCountersLine()', off)
        self.assertIn('"ON (armed, applies once you are in-game)"', self.branch)
        self.assertIn('restartanytime: first write - ', self.hook)

    def test_panel_toggle_mirrors_every_mod_toggle_guard_site(self):
        import forgepact  # noqa: E402 - the panel, imported only here
        panel = (SRC_DIR / "forgepact.py").read_text(encoding="utf-8-sig")
        self.assertEqual(panel.count("mod_restart_anytime"), panel.count("mod_toggle_guard"))
        self.assertIs(forgepact.DEFAULTS["mod_restart_anytime"], False)
        self.assertFalse([c for c in forgepact.build_cmds(dict(forgepact.DEFAULTS)) if "restartanytime" in c])
        cfg = dict(forgepact.DEFAULTS)
        cfg["mod_restart_anytime"] = True
        self.assertIn("restartanytime 1", forgepact.build_cmds(cfg))
        self.assertIn("f\"restartanytime {1 if cfg['mod_restart_anytime'] else 0}\"", panel)
        self.assertIn('id="mod_restart_anytime"', forgepact.HTML)
        self.assertIn('id="mraval"', forgepact.HTML)
        self.assertIn("Restart zone at any time", forgepact.HTML)

    def test_release_notes_readme_and_guide_record_the_mod(self):
        if NOTES.is_file():   # published notes leave main (forgepact-notes-cleanup.yml)
            notes = NOTES.read_text(encoding="utf-8").replace("\r\n", "\n")
            new = notes[notes.index("\n## New\n"):notes.index("\n## ", notes.index("\n## New\n") + 1)]
            bullets = re.split(r"(?m)^- ", new)[1:]
            self.assertTrue([b for b in bullets if "Restart" in b and "off by default" in b])
        readme = README.read_text(encoding="utf-8").replace("\r\n", "\n")
        rows = [line for line in readme.splitlines() if line.startswith("| **")]
        self.assertTrue([r for r in rows if "Restart" in r and "off by default" in r])
        self.assertIn("docs/restart-always-available-research.md", readme)
        if not GUIDE.is_file():
            self.skipTest("the hub guide is not beside this checkout")
        guide = GUIDE.read_text(encoding="utf-8").replace("\r\n", "\n")
        self.assertGreaterEqual(guide.count("restartanytime"), 3)
        self.assertIn("test_restart_anytime_behavior.py", guide)
        item = re.search(r"(?ms)^\d+\. \*\*\"Restart zone at any time\".*?(?=^\d+\. \*\*|^---)", guide)
        self.assertIsNotNone(item)
        text = item.group(0)
        self.assertIn("risk accepted", text)
        self.assertIn("2026-09-22", text)
        self.assertIn("T4 was not run", text)

    def test_probe_and_mod_install_order_is_documented_and_the_row_stays_unbound(self):
        # The probe's UiSetFocus row carries no existing-hook pointer (D14).
        # Binding it to g_OrigUiSetFocus would not attach it: restartanytime
        # installs through both of HookOneScript's routes, so that pointer is
        # a trampoline and the table entry is our hook, neither inside
        # Hero_Siege.exe, and RestartProbeAttach ends at `blocked` either way.
        rows = {r["sdk"]: r for r in macro_rows(self.plugin, "#define RESTARTPROBE_SCRIPTS(X)")}
        self.assertEqual(rows["gml_Script_UiSetFocus"]["orig"], "nullptr")
        self.assertEqual(rows["gml_Script_UiSetFocus"]["held"], "nullptr")
        self.assertNotIn("g_OrigUiSetFocus", function_body(self.plugin, "static void RestartProbeAttach("))
        # The research doc says what each install order does instead, and
        # that it is a static reading.
        doc = DOC.read_text(encoding="utf-8").replace("\r\n", "\n")
        instrument = doc_section(doc, "## Instrument")
        para = re.search(r"(?ms)^Coexistence: .*?(?=\n\n|\Z)", instrument)
        self.assertIsNotNone(para)
        for phrase in ("restartanytime", "restartprobe hook", "blocked", "TABLE-ONLY", "not run live"):
            self.assertIn(phrase, para.group(0), phrase)
        self.assertNotIn("No such hook exists yet", doc)
        if not GUIDE.is_file():
            self.skipTest("the hub guide is not beside this checkout")
        guide = GUIDE.read_text(encoding="utf-8").replace("\r\n", "\n")
        item = re.search(r"(?ms)^\d+\. \*\*\"Restart zone at any time\".*?(?=^\d+\. \*\*|^---)", guide)
        self.assertIsNotNone(item)
        text = item.group(0)
        for phrase in ("Research probe and mod in one session", "restartprobe hook", "blocked", "TABLE-ONLY",
                       "not run live", "player build"):
            self.assertIn(phrase, text, phrase)


if __name__ == "__main__":
    unittest.main()
