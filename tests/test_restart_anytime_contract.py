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
PLANNED_SCRIPTS = {
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
        writes = [body.index('"variable_global_set"'), body.index('"variable_instance_set"')]
        for at in refusals:
            self.assertLess(at, min(writes))
        # Every refusal says nothing was written.
        self.assertGreaterEqual(body.count("nothing written"), 6)
        # One write, then a read-back, then wrote= from the read-back.
        self.assertEqual(self.block.count('"variable_instance_set"'), 1)
        self.assertEqual(self.block.count('"variable_global_set"'), 1)
        readback = body.index("RValue after;")
        self.assertLess(max(writes), readback)
        self.assertLess(readback, body.index('" wrote="'))
        self.assertIn("after.ToDouble() == value", body)
        # changed= compares the read-back with the value before the write, so
        # a control that writes the value already held cannot pass as a write.
        self.assertIn("after.ToDouble() != before.ToDouble()", body)
        self.assertLess(body.index('" wrote="'), body.index('" changed="'))
        # set is the only mutating verb.
        others = self.block.replace(body, "")
        for word in ('"variable_instance_set"', '"variable_global_set"', '"instance_create', '"instance_destroy"',
                     '"room_restart"', '"game_restart"', "CallGameScriptEx"):
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
        for name in ("RestartProbe", "g_RpRows", "RpScope", "RpRead", "RpDetourBody"):
            self.assertNotIn(name, frame, name)
        for line_no, line in enumerate(self.plugin.splitlines(), 1):
            if "restartprobe" in line:
                self.assertNotIn(line, frame, line_no)
        # The hook rows sample the candidates inside the hooked call, before the original runs.
        body = function_body(self.plugin, "static RValue& RpDetourBody(")
        self.assertLess(body.index("RpScopeLine(s)"), body.index("t.tramp ? t.tramp("))


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
            # Phase 0: nothing is measured, so every line is pending.
            self.assertEqual(lines[0].strip(), "pending", key)


if __name__ == "__main__":
    unittest.main()
