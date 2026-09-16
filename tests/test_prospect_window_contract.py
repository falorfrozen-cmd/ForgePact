"""Contract tests for the prospect window research stage (ForgePact issue #9).

What sizes the prospecting cube's input grid is not yet measured
(docs/prospect-window-research.md, Phase 0 pending). This stage ships nothing a
player can reach: a research-build instrument (`prospectprobe`), a
game-independent sizing core (ProspectWindowMod.hpp, whose behaviour is pinned
by test_prospect_window_behavior.py) and the research document the live
session fills in. These tests pin the shape of all three, so the instrument
cannot quietly reach the player build, go blind, or write where it should
refuse - and so the document's procedure keeps its hook-free controls first.
"""
import importlib.util
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PLUGIN = ROOT / "plugin" / "ModuleMain.cpp"
HEADER = ROOT / "plugin" / "include" / "ForgePact" / "ProspectWindowMod.hpp"
DOC = ROOT / "docs" / "prospect-window-research.md"
PANEL = ROOT / "src" / "forgepact.py"
SDK_SCRIPTS = ROOT.parent / "hs-game-sdk" / "cpp" / "include" / "hs_game_sdk" / "scripts.hpp"

# One definition of "what the player build compiles", shared with the release
# contract rather than copied, so the two can never disagree about it.
_spec = importlib.util.spec_from_file_location(
    "_release_hook_contract", ROOT / "tests" / "test_release_hook_contract.py")
_release = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(_release)
strip_research_blocks = _release.strip_research_blocks
function_body = _release.function_body
strip_comments = _release.strip_comments


def collapse(text):
    return " ".join(text.split())


def section(doc, heading):
    """From a line that is exactly `heading` to the next `## ` heading."""
    doc = doc.replace("\r\n", "\n")
    start = doc.index("\n" + heading + "\n") + 1
    following = doc.find("\n## ", start + len(heading))
    return doc[start:] if following < 0 else doc[start:following]


class ProspectWindowContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.plugin = PLUGIN.read_text(encoding="utf-8")
        cls.header = HEADER.read_text(encoding="utf-8")
        cls.doc = DOC.read_text(encoding="utf-8")
        cls.sdk = SDK_SCRIPTS.read_text(encoding="utf-8")

        table = cls.plugin[cls.plugin.index("#define PROSPECTPROBE_TARGETS(X)"):]
        table = table[:table.index("#define PP_DEFINE_DETOUR")]
        cls.rows = re.findall(r'X\((\w+),\s*"([^"]+)",\s*(\w+)\)', table)

    def runtime_name(self, constant):
        match = re.search(
            r'inline constexpr std::string_view ' + re.escape(constant) + r' = "([^"]+)";', self.sdk)
        self.assertIsNotNone(match, f"{constant} is not an hs-game-sdk Scripts constant")
        return match.group(1)

    # ---- the instrument never reaches a player -------------------------------

    def test_prospectprobe_is_research_build_only(self):
        shipped = strip_research_blocks(self.plugin)
        self.assertIn("prospectprobe", self.plugin)
        self.assertNotIn("prospectprobe", shipped)
        self.assertNotIn("PpCommand(", shipped)
        self.assertNotIn("PpDetour_", shipped)
        self.assertNotIn("g_PpTargets", shipped)

    def test_prospectprobe_absent_from_player_commands(self):
        start = self.plugin.index("kPlayerCommands = {")
        block = self.plugin[start:self.plugin.index("};", start)]
        self.assertNotIn("prospect", block)

    def test_no_prospectsize_command_yet(self):
        # Stage A ships nothing player-visible: no toggle command, no panel row.
        self.assertNotIn("prospectsize", self.plugin)
        self.assertNotIn("prospectsize", PANEL.read_text(encoding="utf-8"))
        self.assertNotIn("mod_prospect_window", PANEL.read_text(encoding="utf-8"))
        self.assertNotIn("ProspectWindowMod::Instance()", self.plugin)

    def test_prospectprobe_dispatched_from_handle_prospect_command(self):
        # RunCommand's else-if chain is at MSVC's nesting limit (C1061), so the
        # verb lives in its own handler, called straight after the Headhunter's.
        run = function_body(self.plugin, "static void RunCommand(const std::string& line)")
        self.assertIn(
            "if (HandleHeadhunterCommand(lc, rest)) return;\n    if (HandleProspectCommand(lc, rest)) return;",
            run.replace("\r\n", "\n"))
        handler = function_body(self.plugin, "static bool HandleProspectCommand(")
        self.assertIn('lc == "prospectprobe"', handler)
        self.assertEqual(self.plugin.count('"prospectprobe"'), 1)
        self.assertNotIn('"prospectprobe"', run)

    # ---- the target table ----------------------------------------------------

    def test_target_table_names_every_candidate(self):
        self.assertGreaterEqual(len(self.rows), 45)
        labels = [label for _, label, _ in self.rows]
        self.assertEqual(len(labels), len(set(labels)), "duplicate probe label")
        names = [self.runtime_name(constant) for _, _, constant in self.rows]
        for anchor in (
            "gml_Script_UiSetGrid",
            "gml_Script_UiSetGridArray",
            "gml_Script_s_ItemGridInfo",
            "gml_Script_InventoryInitGrids",
            "gml_Script_UiAProspectButton",
            "gml_Script_anon@1038@gml_Object_UI_Prospect_obj_Create_0",
            "gml_Script_anon@2729@gml_Object_UI_Prospect_obj_Create_0",
            "gml_Script_anon@3551@gml_Object_UI_Prospect_obj_Create_0",
            "gml_Script_CheckPlayerInteraction",
            "gml_Script_PlayerMouseAction",
        ):
            self.assertIn(anchor, names)
        # Every row's runtime name is written down where the live session reads it.
        for label, name in zip(labels, names):
            self.assertIn(name, self.doc, f"row {label}: {name} missing from the research doc")
        # The table spells names through the SDK constant, never as a literal.
        entry = self.plugin[self.plugin.index("#define PP_ENTRY"):self.plugin.index("#undef PP_ENTRY")]
        self.assertIn("HeroSiege::Scripts::CONSTANT.data()", entry)

    def test_resolver_refuses_address_outside_module_before_hook(self):
        resolver = function_body(self.plugin, "static PVOID PpResolve(")
        self.assertIn("GetNamedRoutinePointer(t.runtimeName", resolver)
        self.assertIn("AddrIsExecutableInModule(GetModuleHandleA(nullptr), fn)", resolver)
        self.assertLess(resolver.index("AddrIsExecutableInModule"), resolver.index("return fn;"))
        install = function_body(self.plugin, "static void PpInstall(")
        self.assertLess(install.index("PpResolve(t, why)"), install.index("MmCreateHook("))
        self.assertLess(install.index("if (!src)"), install.index("MmCreateHook("))
        self.assertNotIn("HookOneScriptTable", install)

    # ---- the two writes ------------------------------------------------------

    def test_set_refuses_missing_and_non_numeric(self):
        body = strip_comments(function_body(self.plugin, "static void PpSet("))
        write = body.index('"variable_instance_set"')
        self.assertLess(body.index('"asset_get_index"'), write)
        self.assertLess(body.index('"instance_number"'), write)
        self.assertLess(body.index('"variable_instance_exists"'), write)
        self.assertLess(body.index("PpIsNumber(was)"), write)
        for refusal in ("unknown object", "no such instance", "no such variable", "not a number"):
            self.assertIn(refusal, body)
        # The instance kind never decides whether the write happens (VALUE_REF
        # on this runner; Known Limitations item 7).
        self.assertNotIn("VALUE_REF", body)
        self.assertNotIn("VALUE_OBJECT", body)
        self.assertIn("readback", body)
        number = function_body(self.plugin, "static bool PpIsNumber(")
        for kind in ("VALUE_REAL", "VALUE_INT32", "VALUE_INT64"):
            self.assertIn(kind, number)

    def test_override_requires_a_detoured_row(self):
        body = function_body(self.plugin, "static void PpOverride(")
        self.assertIn("!row->installed.load()", body)
        self.assertIn("hook it first", body)
        self.assertLess(body.index("!row->installed.load()"), body.index("InterlockedExchange(&g_PpOverrideLeft"))
        observe = function_body(self.plugin, "static void PpObserve(")
        self.assertLess(observe.index("PpIsNumber(*A[i])"), observe.index("*A[i] = RValue("))

    # ---- a spent budget and a misplaced override are visible, not silent -----
    # Round-0 review B1/B2: a fixed 6-line budget let rows that fire before the
    # window opens hide the sizing call's arguments, and an override could land
    # on another caller of the same row with nothing saying so. Either would
    # have turned an unseen candidate into evidence for H3.

    def test_arm_accepts_a_budget_and_label_filters(self):
        arm = strip_comments(function_body(self.plugin, "static void PpArm("))
        self.assertIn('"budget="', arm)
        self.assertIn("kPpMaxLogBudget", arm)
        self.assertIn("InterlockedExchange(t.logOn", arm)
        self.assertIn("InterlockedExchange(&g_PpLogBudget, budget)", arm)
        # A refused budget arms nothing.
        self.assertLess(arm.index("not armed"), arm.index("g_PpArmed.store(true)"))
        command = function_body(self.plugin, "static void PpCommand(")
        self.assertIn("PpArm(std::vector<std::string>(tok.begin() + 1, tok.end()))", command)
        observe = strip_comments(function_body(self.plugin, "static void PpObserve("))
        self.assertIn("g_PpLogBudget", observe)
        self.assertIn("*logOn", observe)
        self.assertNotRegex(self.plugin, r"\bkPpLogBudget\b")

    def test_show_reports_unlogged_calls(self):
        show = function_body(self.plugin, "static void PpShow(")
        self.assertIn("UNLOGGED=", show)
        self.assertIn("not observed", show)
        self.assertIn("calls - logged", show)
        self.assertIn("notApplied=", show)
        # R1-N2: after a filtered re-arm, a row left out of the filter that
        # still fired had calls nobody saw, and says so rather than looking clean.
        self.assertIn("(not selected - not observed)", show)

    def test_override_applied_line_describes_self_other_and_args(self):
        observe = strip_comments(function_body(self.plugin, "static void PpObserve("))
        write = observe.index("*A[i] = RValue(g_PpOverrideValue)")
        applied = observe[write:observe.index("Out(line)", write)]
        self.assertIn("PpDescribeSelf(S)", applied)
        self.assertIn("PpDescribeSelf(O)", applied)
        self.assertIn("AggroArgs(argc, A)", applied)

    def test_override_selector_gates_the_write(self):
        observe = strip_comments(function_body(self.plugin, "static void PpObserve("))
        self.assertLess(observe.index("PpSelectorMismatch(S, O, *A[i])"),
                        observe.index("*A[i] = RValue(g_PpOverrideValue)"))
        self.assertIn("numeric && mismatch.empty()", observe)
        selector = function_body(self.plugin, "static std::string PpSelectorMismatch(")
        for field in ("g_PpOverrideWhen", "g_PpOverrideSelf", "g_PpOverrideOther", "PpObjectName(S)", "PpObjectName(O)"):
            self.assertIn(field, selector)
        # A struct self has no numeric object_index and must never reach object_get_name.
        name = function_body(self.plugin, "static std::string PpObjectName(")
        self.assertLess(name.index("numeric"), name.index('"object_get_name"'))
        parser = function_body(self.plugin, "static void PpOverrideCommand(")
        for key in ('"self="', '"other="', '"when="'):
            self.assertIn(key, parser)

    def test_research_doc_h3_requires_non_empty_fully_logged_r2_r4(self):
        deciding = collapse(section(self.doc, "## Deciding the hypothesis"))
        self.assertIn("**H3** needs **R2 and R4 both non-empty**", deciding)
        self.assertIn("**fully logged** (no `UNLOGGED` left)", deciding)
        self.assertIn("never H3", deciding)
        self.assertIn("`not observed (budget spent)`", deciding)
        self.assertIn("`not observed (override landed elsewhere; grid <changed|unchanged>)`", deciding)
        self.assertIn("`@id` ignored", deciding)
        # R1-N4: a row the maximum budget cannot cover stays unseen, by design.
        self.assertIn("`not observed (budget spent)` by design", deciding)

    def test_research_doc_live_procedure_budgets_and_matches_the_override(self):
        live = collapse(section(self.doc, "## Live procedure"))
        l10 = live[live.index("**L10.**"):live.index("**L11")]
        l11 = live[live.index("**L11"):live.index("**L12.**")]
        self.assertIn("prospectprobe arm budget=", l10)
        self.assertIn("UNLOGGED", l10)
        self.assertIn("when=<vanilla>", l11)
        self.assertIn("self=<Obj>", l11)
        # R1-N5: the selector takes the object name alone.
        self.assertIn("`self=UI_Prospect_obj`, not `self=UI_Prospect_obj#5220@100456`", l11)
        self.assertIn("not observed (override landed elsewhere", l11)
        # Round-1 review R1-B: a reopen re-runs Create, so the right call always
        # carries a new `@id`. A match rule that compared it would record the real
        # mechanism as "landed elsewhere" with the enlarged grid on screen.
        self.assertIn("**ignore `@id`**", l11)
        self.assertIn("same `#object_index`", l11)
        self.assertIn("instance ids or handles", l11)
        self.assertIn("`(not an instance: …)` matches `(not an instance: …)`", l11)
        # The grid observation is always recorded, and before nothing is dropped.
        self.assertIn("Record both, always", l11)
        self.assertIn("grid <changed|unchanged>", l11)
        self.assertNotIn("Only then", l11)
        # The instrument section documents what the procedure uses.
        instrument = collapse(section(self.doc, "## Instrument"))
        self.assertIn("`prospectprobe arm [budget=N] [substr ...]`", instrument)
        self.assertIn("[self=<Obj>] [other=<Obj>] [when=<number>]", instrument)

    # ---- nothing on the frame path -------------------------------------------

    def test_frame_callback_unchanged(self):
        frame = function_body(self.plugin, "void FrameCallback(")
        # Stage A adds nothing to the per-frame path (the frame-loop budget
        # tests in test_release_hook_contract.py pin the rest of it).
        for name in ("rospect", "PpCommand", "PpInstall", "PpShow", "PpArm", "PpSet", "g_Pp"):
            self.assertNotIn(name, frame)

    # ---- the core header -----------------------------------------------------

    def test_core_header_is_game_independent(self):
        for forbidden in ("g_Yytk", "CallBuiltin", "Out(", "RValue"):
            self.assertNotIn(forbidden, self.header)
        includes = [l.strip() for l in self.header.split("\n") if l.strip().startswith("#include")]
        self.assertEqual(includes, ['#include "Common.hpp"'])
        self.assertIn("#include <ForgePact/ProspectWindowMod.hpp>", self.plugin)

    # ---- the research document -----------------------------------------------

    def test_research_doc_has_its_sections_and_is_pending(self):
        self.assertIn("phase0-status: pending", self.doc)
        for heading in ("## Static search", "## Hypotheses", "## Instrument", "## Live procedure",
                        "## Deciding the hypothesis", "## Results"):
            self.assertIn("\n" + heading + "\n", self.doc.replace("\r\n", "\n"))
        results = section(self.doc, "## Results")
        for field in ("R1", "R2", "R3", "R4", "R5a", "R5b", "R6", "R7", "R8", "| C |", "| H |"):
            self.assertIn(field, results)

    def test_research_doc_hook_free_steps_come_first(self):
        # A detour can be blind; an enumeration and a write control cannot be
        # blind in the same way. Both run before anything is hooked.
        for text in (self.doc, section(self.doc, "## Live procedure")):
            hook = text.index("prospectprobe hook")
            self.assertLess(text.index("citrace dumpobj"), hook)
            self.assertLess(text.index("prospectprobe set"), hook)

    def test_research_doc_pins_the_two_sentences(self):
        doc = collapse(self.doc)
        self.assertIn(
            "A row whose arguments carry the vanilla numbers is a candidate, not a result; "
            "only an override on that row that changes the drawn grid counts for H1.", doc)
        self.assertIn(
            "A variable write that enlarges the drawn frame but not the cells the game "
            "accepts items into does not count for H2.", doc)


if __name__ == "__main__":
    unittest.main()
