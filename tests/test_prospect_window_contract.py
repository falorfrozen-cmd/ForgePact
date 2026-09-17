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
        self.assertGreaterEqual(len(self.rows), 85)
        labels = [label for _, label, _ in self.rows]
        self.assertEqual(len(labels), len(set(labels)), "duplicate probe label")
        names = [self.runtime_name(constant) for _, _, constant in self.rows]
        for anchor in (
            "gml_Script_UiSetGrid",
            "gml_Script_UiSetGridArray",
            "gml_Script_s_ItemGridInfo",
            "gml_Script_InventoryInitGrids",
            "gml_Script_UiAProspectButton",
            # Phase 0a read these off the live window's method values
            # (m_SetInventoryLocalPlayer, m_Resize, m_UpdateInventoryGrid) and
            # the ProspectGrid node's m_RefreshNode.
            "gml_Script_anon@1065@gml_Object_UI_Prospect_obj_Create_0",
            "gml_Script_anon@2806@gml_Object_UI_Prospect_obj_Create_0",
            "gml_Script_anon@3657@gml_Object_UI_Prospect_obj_Create_0",
            "gml_Script_anon@36159@gml_Object_UI_Inventory_Grid_obj_Create_0",
            "gml_Script____struct___411@UiContainerChange@UiFuncs",
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

    # The Phase 0a session was blind because the table named closures a game
    # patch had renumbered. The table is now derived from the SDK: every
    # closure the SDK names on these objects' Create events must be a row, so
    # the next regeneration fails here, by name, instead of in a live session.
    UI_CLOSURE_OBJECTS = (
        "UI_Prospect_obj", "UI_Inventory_Grid_obj", "UI_Inventory_Parent_obj", "UI_Node_Parent_obj",
        "UI_Grid_obj", "UI_Container_obj", "UI_Parent_obj", "Prospect_Cube_obj",
        "UI_Button_Journal_Prospect_obj", "UI_Journal_Prospecting_obj",
    )

    def test_target_table_covers_every_sdk_closure_of_the_ui_objects(self):
        table = {constant for _, _, constant in self.rows}
        expected = []
        for constant, value in re.findall(r'std::string_view (\w+) = "([^"]+)";', self.sdk):
            if any("@gml_Object_" + obj + "_Create_0" in value for obj in self.UI_CLOSURE_OBJECTS):
                expected.append(constant)
        # A scan that finds nothing would pass the check below vacuously.
        self.assertGreaterEqual(len(expected), 45, "SDK closure scan found too few constants - the regex is blind")
        missing = [c for c in expected if c not in table]
        self.assertEqual(missing, [], "SDK closures missing from PROSPECTPROBE_TARGETS: " + ", ".join(missing))

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
        observe = function_body(self.plugin, "static bool PpObserve(")
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
        observe = strip_comments(function_body(self.plugin, "static bool PpObserve("))
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
        observe = strip_comments(function_body(self.plugin, "static bool PpObserve("))
        write = observe.index("*A[i] = RValue(g_PpOverrideValue)")
        applied = observe[write:observe.index("Out(line)", write)]
        self.assertIn("PpDescribeSelf(S)", applied)
        self.assertIn("PpDescribeSelf(O)", applied)
        self.assertIn("AggroArgs(argc, A)", applied)

    def test_override_selector_gates_the_write(self):
        observe = strip_comments(function_body(self.plugin, "static bool PpObserve("))
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

    # ---- Phase 0b: the builder can be found without a size argument ----------
    # Phase 0a showed the ProspectGrid node created with no size in any logged
    # argument, and a bare nodeGridWidth write crashing the node's Draw (R5b).
    # So the instrument reads the node's shape around each logged call, and the
    # two writes it adds either land before the game's builder runs or are
    # followed by the game's own builder in the same handler.

    def test_grid_snapshot_is_hook_free_and_budgeted(self):
        snap = strip_comments(function_body(self.plugin, "static bool PpGridSnapshot("))
        for text in ('"instance_number"', '"instance_find"', '"variable_instance_exists"', '"array_length"',
                     "g_PpInSnapshot", '\\"ProspectGrid\\"', '"uiNodeCallstack"'):
            self.assertIn(text, snap)
        self.assertNotIn("MmCreateHook", snap)
        self.assertNotIn("_Create_0", snap)   # the node is found by what it is, not by a hook
        # Objects by SDK name, never a literal index.
        self.assertIn("GameObject::UI_Inventory_Grid_obj", self.plugin[self.plugin.index("static int PpGridObjectIndex("):])
        # The pre-snapshot is taken only for a call that is being logged.
        observe = strip_comments(function_body(self.plugin, "static bool PpObserve("))
        self.assertLess(observe.index("InterlockedIncrement(logged)"), observe.index("grid-pre="))
        self.assertLess(observe.index("g_PpWatch"), observe.index("grid-pre="))
        frame = function_body(self.plugin, "void FrameCallback(")
        self.assertNotIn("PpGrid", frame)
        self.assertNotIn("g_PpWatch", frame)

    def test_watch_post_line_exists_and_is_gated_on_logged(self):
        after = strip_comments(function_body(self.plugin, "static void PpAfter("))
        self.assertIn("grid-post=", after)
        self.assertIn("PpSnapCompare(gridPre, post)", after)
        # Round-1 N4: two failed reads must not print as `same`.
        compare = strip_comments(function_body(self.plugin, "static const char* PpSnapCompare("))
        for verdict in ('" UNREADABLE"', '" same"', '" CHANGED"'):
            self.assertIn(verdict, compare)
        self.assertLess(compare.index('" UNREADABLE"'), compare.index('" CHANGED"'))
        self.assertIn('"none"', compare)
        self.assertIn('"@"', compare)
        # Round-2 N6': `none` on both sides is not evidence that nothing changed
        # (a node without uiNodeCallstack yet also reads `none`).
        self.assertIn('" same (no node)"', compare)
        self.assertLess(compare.index('" same (no node)"'), compare.index('" CHANGED"'))
        self.assertIn("`same (no node)`", collapse(section(self.doc, "## Instrument")))
        l5 = collapse(section(self.doc, "## Live procedure"))
        self.assertIn("`same (no node)`", l5[l5.index("**L5"):l5.index("**L6")])
        self.assertIn("PpSnapCompare(before, after)", strip_comments(function_body(self.plugin, "static void PpCall(")))
        self.assertIn("if (!logged", after)
        self.assertLess(after.index("if (!logged"), after.index("PpGridSnapshot("))
        detour = self.plugin[self.plugin.index("#define PROSPECTPROBE_DETOUR"):self.plugin.index("#define PROSPECTPROBE_TARGETS")]
        self.assertIn("PpAfter(", detour)
        self.assertLess(detour.index("PpObserve("), detour.index("g_PpOrig_##SAFE(S, O, R, argc, A)"))
        self.assertLess(detour.index("g_PpOrig_##SAFE(S, O, R, argc, A)"), detour.index("PpAfter("))
        reset = function_body(self.plugin, "static void PpReset(")
        self.assertIn("g_PpWatch.store(false)", reset)

    def test_call_invokes_method_values_by_name_only(self):
        for signature in ("static void PpCall(", "static void PpResize("):
            body = strip_comments(function_body(self.plugin, signature))
            self.assertIn('"script_execute"', body, signature)
            self.assertIn("CallBuiltinEx(", body, signature)
            for forbidden in ("MethodValueFunction", "CScriptRef", "m_CallYYC", "Rva", "InvokeMethodValue"):
                self.assertNotIn(forbidden, body, signature)
        # Refusals come before the call, and the resolution is printed.
        target = strip_comments(function_body(self.plugin, "static bool PpMethodTarget("))
        self.assertIn('"variable_instance_exists"', target)
        self.assertIn("VALUE_OBJECT", target)
        self.assertIn("CiTryResolveMethod(", target)
        call = strip_comments(function_body(self.plugin, "static void PpCall("))
        self.assertLess(call.index("PpMethodTarget("), call.index("CallBuiltinEx("))

    def test_resize_requires_via_and_reverts_when_the_builder_does_not_follow(self):
        resize = strip_comments(function_body(self.plugin, "static void PpResize("))
        self.assertIn("R5b", resize)
        self.assertLess(resize.index('"script_execute"'), resize.index("PpMeasureStore(node)"))
        self.assertIn("reverted", resize)
        self.assertIn("kept", resize)
        self.assertGreaterEqual(resize.count('"variable_instance_set"'), 4)
        # The method is resolved (and refused) before anything is written.
        self.assertLess(resize.index("PpMethodTarget("), resize.index('"variable_instance_set"'))
        command = strip_comments(function_body(self.plugin, "static void PpCommand("))
        self.assertIn('"via"', command)
        # Round-1 N3: the store is measured over every row, and `kept` needs
        # every row to follow, not only row 0.
        measure = strip_comments(function_body(self.plugin, "static PpStoreShape PpMeasureStore("))
        self.assertIn('"array_length"', measure)
        self.assertIn("for (int i = 0; i < s.rows; ++i)", measure)
        self.assertIn("s.colsMin == cols && s.colsMax == cols", resize)

    def test_resize_restore_never_exceeds_the_store(self):
        # Round-1 N3: a builder that shrank or partly resized nodeGrid makes the
        # vanilla values the R5b crash on the next Draw. The restore is capped
        # per axis at what the store covers, and says so.
        resize = strip_comments(function_body(self.plugin, "static void PpResize("))
        revert = resize[resize.index("double w = wasW.ToDouble()"):]
        self.assertIn("s.colsMin < w", revert)
        self.assertIn("s.rows < h", revert)
        self.assertIn("restore unsafe", revert)
        self.assertLess(revert.index("s.colsMin < w"), revert.index('"variable_instance_set"'))
        self.assertNotIn("{ node, wName, wasW }", resize)
        # Round-1 N5: a method that replaced the node still gives an outcome.
        self.assertIn("rebuilt (", resize)
        self.assertIn("PpMeasureStore(newNode)", resize)
        self.assertIn("destroyed (", resize)
        # Round-2 N4': a line may not say nothing was written after writing.
        self.assertNotIn("nothing safe to write", resize)
        # Round-2 N5': kept/rebuilt check the size variables against the store.
        self.assertGreaterEqual(resize.count("PpSizeText("), 2)
        size = strip_comments(function_body(self.plugin, "static std::string PpSizeText("))
        self.assertIn("size exceeds store", size)
        self.assertIn("s.colsMin", size)
        self.assertIn("s.rows", size)
        # Round-2 P0b-B2: the outcome line says whether the probe was a shrink,
        # because a shrink's `reverted` never counts toward H3.
        self.assertIn("probe=shrink", resize)
        self.assertIn("a shrink's reverted never counts toward H3", resize)

    def test_call_and_resize_refuse_while_a_rewrite_is_pending(self):
        # Round-2 N2': a pending override or setat would fire inside our own
        # invoke and rewrite what the method received while `args=` printed what
        # was supplied. Both refuse, with nothing written or called.
        pending = strip_comments(function_body(self.plugin, "static std::string PpPendingRewrite("))
        self.assertIn("g_PpOverrideLeft", pending)
        self.assertIn("g_PpSetAtPending", pending)
        call = strip_comments(function_body(self.plugin, "static void PpCall("))
        self.assertLess(call.index("PpPendingRewrite()"), call.index("CallBuiltinEx("))
        resize = strip_comments(function_body(self.plugin, "static void PpResize("))
        self.assertLess(resize.index("PpPendingRewrite()"), resize.index('"variable_instance_set"'))

    def test_call_and_resize_prove_the_method_body_ran(self):
        # Round-1 P0b-B1: script_execute succeeding proves the dispatch, not
        # that the builder's body ran, so a `reverted` said nothing. The body's
        # own detoured row counting during the invoke is the proof, and every
        # outcome line names what was supplied.
        row = strip_comments(function_body(self.plugin, "static PpTarget* PpInvokedRow("))
        self.assertIn('"->method:"', row)
        self.assertIn("t.runtimeName", row)
        self.assertIn('"gml_Script_"', row)
        text = strip_comments(function_body(self.plugin, "static std::string PpInvokedText("))
        for verdict in ('"invoked=unproven (no detoured row for"', '"invoked=yes ("', '"invoked=NO ("',
                        "row->installed.load()", "*row->calls - callsBefore"):
            self.assertIn(verdict, text)
        for signature in ("static void PpCall(", "static void PpResize("):
            body = strip_comments(function_body(self.plugin, signature))
            call = body.index("CallBuiltinEx(")
            self.assertLess(body.index("PpInvokedRow(resolution)"), call, signature)
            self.assertLess(body.index("callsBefore = row ? (long)*row->calls : 0"), call, signature)
            self.assertLess(call, body.index("PpInvokedText(row, callsBefore, resolution)"), signature)
            self.assertIn("PpArgsText(args)", body, signature)
            self.assertIn('" self=" + PpDescribeSelf(self)', body, signature)
            self.assertIn("callArgs.push_back(RValue(a))", body, signature)
            self.assertIn("script_execute\", self, self, callArgs)", body, signature)
        resize = strip_comments(function_body(self.plugin, "static void PpResize("))
        self.assertIn("const std::vector<double>& args", self.plugin[self.plugin.index("static void PpResize("):][:120])
        # `supplied` rides on the kept, reverted, rebuilt and destroyed lines.
        self.assertGreaterEqual(resize.count("+ supplied"), 4)
        command = strip_comments(function_body(self.plugin, "static void PpCommand("))
        self.assertIn("PpResize(cols, rows, hasVia ? tok[4] : std::string(), args)", command)
        self.assertIn("arguments after the method must be numbers", command)

    def test_setat_requires_a_detoured_row_and_an_existing_variable(self):
        arm = strip_comments(function_body(self.plugin, "static void PpSetAt("))
        self.assertIn("!row->installed.load()", arm)
        self.assertIn("hook it first", arm)
        apply = strip_comments(function_body(self.plugin, "static void PpSetAtTry("))
        self.assertLess(apply.index('"variable_instance_exists"'), apply.index('"variable_instance_set"'))
        self.assertLess(apply.index("PpIsNumber(was)"), apply.index('"variable_instance_set"'))
        # Every refusal goes through one budgeted, counted "not applied" line.
        self.assertLess(apply.index("PpSetAtRefused("), apply.index('"variable_instance_set"'))
        refused = strip_comments(function_body(self.plugin, "static void PpSetAtRefused("))
        self.assertIn("not applied", refused)
        self.assertIn("kPpRefusalLogBudget", refused)
        self.assertIn("g_PpSetAtNotApplied", refused)
        parser = strip_comments(function_body(self.plugin, "static void PpSetAtCommand("))
        self.assertIn('"arg"', parser)
        self.assertIn('"when="', parser)   # refused, not silently accepted
        observe = strip_comments(function_body(self.plugin, "static bool PpObserve("))
        self.assertIn("PpSetAtTry(", observe)
        after = strip_comments(function_body(self.plugin, "static void PpAfter("))
        self.assertIn("PpSetAtTry(", after)
        self.assertIn("g_PpSetAtPending = false", function_body(self.plugin, "static void PpReset("))

    def test_research_doc_h3_requires_non_empty_fully_logged_r2_r4(self):
        deciding = collapse(section(self.doc, "## Deciding the hypothesis"))
        self.assertIn("**H3** needs **R2-window, R4' and R9 all measured**", deciding)
        self.assertIn("**fully logged** (no `UNLOGGED` left)", deciding)
        self.assertIn("never H3", deciding)
        self.assertIn("`not observed (budget spent)`", deciding)
        self.assertIn("`not observed (override landed elsewhere; grid <changed|unchanged>)`", deciding)
        self.assertIn("`@id` ignored", deciding)
        # R1-N4: a row the maximum budget cannot cover stays unseen, by design.
        self.assertIn("`not observed (budget spent)` by design", deciding)

    def test_research_doc_live_procedure_budgets_and_matches_the_override(self):
        live = collapse(section(self.doc, "## Live procedure"))
        l5 = live[live.index("**L5"):live.index("**L6")]
        l11 = live[live.index("**L6"):live.index("**L7")]
        self.assertIn("prospectprobe watch on", l5)
        self.assertIn("prospectprobe arm budget=", l5)
        self.assertIn("UNLOGGED", l5)
        self.assertIn("CHANGED", l5)
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
        # The write-then-rebuild experiment can end the session (D7): it runs last.
        l9 = live[live.index("**L9"):live.index("**L10")]
        self.assertIn("via m_RefreshNode", l9)
        self.assertIn("last", l9)
        self.assertIn("Never a bare", l9)
        # The instrument section documents what the procedure uses.
        instrument = collapse(section(self.doc, "## Instrument"))
        self.assertIn("`prospectprobe arm [budget=N] [substr ...]`", instrument)
        self.assertIn("[self=<Obj>] [other=<Obj>] [when=<number>]", instrument)
        for command in ("`prospectprobe grid`", "`prospectprobe watch on|off`", "`prospectprobe call ",
                        "`prospectprobe resize <cols> <rows> via <m_Method> [number ...]`", "`prospectprobe setat "):
            self.assertIn(command, instrument)

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
            self.assertLess(text.index("prospectprobe grid"), hook)

    def test_research_doc_pins_the_three_sentences(self):
        doc = collapse(self.doc)
        self.assertIn(
            "A row whose arguments carry the vanilla numbers is a candidate, not a result; "
            "only an override on that row that changes the drawn grid counts for H1.", doc)
        self.assertIn(
            "A variable write that enlarges the drawn frame but not the cells the game "
            "accepts items into does not count for H2.", doc)
        self.assertIn(
            "A snapshot that changes inside a call's extent names the builder's extent, not the "
            "builder; only a write the builder then follows counts for H2'.", doc)

    def test_research_doc_records_phase0a_as_instrument_failure(self):
        self.assertIn("phase0-status: pending", self.doc)
        results = section(self.doc, "## Results")
        for text in ("Phase 0a", "Phase 0b", "instrument blind", "stale", "Draw_64", "not observed"):
            self.assertIn(text, results)
        self.assertNotIn("does not happen", self.doc.lower())
        # Phase 0a's column is filled; Phase 0b's is still open.
        for field in ("R2-window", "R4'", "R5c", "R9", "R10", "R11", "C-grid", "C-write2"):
            self.assertIn("| " + field + " |", results)
        rows = [line for line in results.replace("\r\n", "\n").split("\n") if line.startswith("| R1 |")]
        self.assertEqual(len(rows), 1)
        cells = [c.strip() for c in rows[0].strip("|").split("|")]
        self.assertNotEqual(cells[-2], "unknown")
        self.assertEqual(cells[-1], "unknown")

    def test_research_doc_closure_rows_have_a_positive_control(self):
        live = collapse(section(self.doc, "## Live procedure"))
        l4 = live[live.index("**L4"):live.index("**L5")]
        self.assertIn("must print `detoured`", l4)
        self.assertIn("session 7", l4)
        # Round-1 N2: the stop condition covers the four names read live, not
        # the nested struct closures nothing has measured resolving.
        for name in ("anon@1065", "anon@2806", "anon@3657", "anon@36159"):
            self.assertIn(name, l4)
        self.assertNotIn("**Every `UI_Prospect_obj anon@…` and `UI_Inventory_Grid_obj …` row", l4)
        # Round-1 N1: `detoured` proves resolution; a count needs invoked=yes.
        instrument = collapse(section(self.doc, "## Instrument"))
        self.assertIn("proves only that the name resolved", instrument)
        self.assertIn("`invoked=yes`", instrument)

    def test_research_doc_pins_the_resize_revert_rule(self):
        # Round-1 P0b-B1: a `reverted` whose body never ran, or ran with a call
        # shape the game does not use, is not evidence against the builder.
        sentence = ("A `reverted` from `resize via` counts toward H3 only when it printed `invoked=yes` "
                    "and supplied the `self` and argument shape L5 logged when the game itself called that "
                    "row; if L5 never logged the game calling it, it is `not observed (call shape unknown)`.")
        deciding = collapse(section(self.doc, "## Deciding the hypothesis"))
        self.assertIn(sentence, deciding)
        hypotheses = collapse(section(self.doc, "## Hypotheses"))
        h3 = [line for line in hypotheses.split("| **H3** |")[1:]][0].split("| **not observed** |")[0]
        self.assertIn("`invoked=yes`", h3)
        self.assertNotIn("each with its control passing", h3)
        results = section(self.doc, "## Results")
        r10 = [line for line in results.replace("\r\n", "\n").split("\n") if line.startswith("| R10 |")][0]
        self.assertIn("invoked=", r10)
        self.assertIn("args", r10)
        live = collapse(section(self.doc, "## Live procedure"))
        l9 = live[live.index("**L9"):live.index("**L10")]
        # Round-1 N3: each method is probed with a shrink first.
        self.assertIn("prospectprobe resize 8 5 via m_RefreshNode", l9)
        self.assertLess(l9.index("resize 8 5"), l9.index("resize 18 6"))
        self.assertIn("restore unsafe", l9)
        self.assertIn("invoked=", l9)
        self.assertIn("rebuilt", l9)
        # Round-2 P0b-B2: GML's element assignment grows an array and never
        # truncates it, so an assignment builder answers a shrink with an
        # unchanged store. Forbidding the grow after that shrink skipped the one
        # probe that would have kept, and H3 then counted the shrink.
        grow_after_revert = ("A method whose shrink printed `reverted` with `invoked=yes` and an unchanged store "
                             "is still grown: an assignment-built store grows and never truncates, so a shrink "
                             "alone cannot show that the builder reads the size.")
        shrink_never_counts = ("Only a grow's `reverted` from `resize via` counts toward H3; a shrink's `reverted` "
                               "is `not observed (shrink only — an assignment-built store never truncates)`.")
        self.assertIn(grow_after_revert, l9)
        self.assertIn(shrink_never_counts, deciding)
        self.assertIn("**Shrink every method first**", l9)
        self.assertLess(l9.index("**Shrink every method first**"), l9.index("resize 9 6"))
        self.assertNotIn("A method whose shrink printed `reverted`, `rebuilt` or `destroyed` is not grown", l9)
        self.assertNotIn("Only a method whose shrink printed `kept` is then grown", l9)
        self.assertIn("grown (`resize 18 6`)", h3)
        self.assertIn("a shrink's `reverted` never counts", h3)
        h3_rule = deciding[deciding.index("**H3** needs"):]
        self.assertIn("a shrink's `reverted` never counts", h3_rule)
        self.assertIn("only a grow's `reverted` counts toward H3", r10)
        # Round-2 N3': the detour logs the self/args the method actually received.
        self.assertIn("prospectprobe arm budget=10", l9)
        self.assertLess(l9.index("prospectprobe arm budget=10"), l9.index("resize 8 5"))
        # Round-2 N7': a non-numeric argument cannot be supplied.
        self.assertIn("non-numeric", l9)
        self.assertIn("`not observed (call shape unknown)`", l9)
        # Round-2 N8': the name matcher failing voids every resize of the session.
        self.assertIn("`invoked=unproven (no detoured row for …)`", l9)
        self.assertIn("nothing from `resize` counts this session", l9)
        l5 = live[live.index("**L5"):live.index("**L6")]
        # Round-1 N4: "no CHANGED" is a result only if the snapshot resolved.
        self.assertIn("UNREADABLE", l5)
        self.assertIn("grid-post=@", l5)

    # ---- Phase 0c: what backs nodeGrid, read from the game's own calls -------
    # Phase 0b found nodeGrid built inside m_SetInventoryLocalPlayer from the
    # player's profile inventory data, and a blind `callnum
    # GetProfileInventoryData` (no correct self) crashed the game. So Phase 0c
    # never calls a getter: it keeps what the game's own call returned, and
    # tests identity against that, with a positive control first.

    GETTERS = ("GetProfileInventoryData", "GetPlayerItemOwner", "GetInventoryArray", "GetPlayerProfileObj")

    def backing_functions(self):
        # name -> body, by the full `static ... PpBackingX(` signature, so a call
        # site later in the file is never mistaken for the definition.
        found = re.findall(r"^(static [^\n(]*?\b(PpBacking\w+)\()", self.plugin, re.M)
        bodies = {name: strip_comments(function_body(self.plugin, signature)) for signature, name in found}
        for name in ("PpBackingCapture", "PpBackingCommand", "PpBackingDump", "PpBackingIdCheck",
                     "PpBackingIsEmptyCell", "PpBackingRelease"):
            self.assertIn(name, bodies)
        return bodies

    def test_backing_captures_getter_returns_without_invoking(self):
        labels = [label for _, label, _ in self.rows]
        for getter in self.GETTERS:
            self.assertIn(getter, labels)
        bodies = self.backing_functions()
        joined = "\n".join(bodies.values())
        for used in ('"json_stringify"', '"array_length"', '"variable_instance_get"', "g_PpBackingProfile",
                     "g_PpBackingOwner"):
            self.assertIn(used, joined)
        # Nothing in the backing instrument calls a game script, by any route.
        for name, body in bodies.items():
            for forbidden in ("CallGameScript", "script_execute", "InvokeMethodValue", "MethodValueFunction",
                              "callnum", "PpCall(", "PpResize("):
                self.assertNotIn(forbidden, body, name)
        # The capture runs after the game's own function returned, inside PpAfter,
        # and it is handed the value the trampoline produced.
        detour = self.plugin[self.plugin.index("#define PROSPECTPROBE_DETOUR"):self.plugin.index("#define PROSPECTPROBE_TARGETS")]
        self.assertIn("PpAfter(LABEL, n, logged, gridPre, S, O, argc, A, r)", detour)
        after = strip_comments(function_body(self.plugin, "static void PpAfter("))
        self.assertIn("PpBackingCapture(", after)
        self.assertLess(after.index("PpBackingCapture("), after.index("if (!logged"))
        capture = bodies["PpBackingCapture"]
        self.assertLess(capture.index("g_PpBacking.load()"), capture.index("PpBackingShape("))
        self.assertIn('"json_stringify"', bodies["PpBackingJsonText"])
        self.assertIn("kPpBackingLogBudget", capture)
        self.assertIn("g_PpInBacking", capture)
        # Round-0 N2: nothing is serialised inside the game's own getter call (a
        # struct cycle would overflow json_stringify there, at character load).
        # `backing dump` writes the files, and only after a depth-capped walk
        # of the value finished without hitting the cap.
        self.assertNotIn("PpBackingJsonText(", capture)
        self.assertNotIn("PpBackingJsonFile(", capture)
        self.assertNotIn("json_stringify", capture)
        json_file = bodies["PpBackingJsonFile"]
        self.assertLess(json_file.index("PpBackingWalk("), json_file.index("PpBackingJsonText("))
        self.assertLess(json_file.index("scan.depthCapped > 0"), json_file.index("PpBackingJsonText("))
        self.assertIn("PpBackingJsonFile(", bodies["PpBackingDump"])
        # Round-0 N1: an RValue copy roots an array, not a struct, so every kept
        # value is also assigned to a research global, cleared on release.
        self.assertIn('"variable_global_set", { RValue(root), result }', capture)
        self.assertIn('"variable_global_set"', bodies["PpBackingClearSlot"])
        self.assertIn("PpBackingClearSlot(", bodies["PpBackingRelease"])
        # Off by default, and reset turns it off and releases what it kept.
        self.assertIn("static std::atomic<bool> g_PpBacking{ false };", self.plugin)
        reset = strip_comments(function_body(self.plugin, "static void PpReset("))
        self.assertIn("g_PpBacking.store(false)", reset)
        self.assertIn("PpBackingRelease()", reset)
        frame = function_body(self.plugin, "void FrameCallback(")
        self.assertNotIn("PpBacking", frame)
        self.assertNotIn("g_PpBacking", frame)
        command = strip_comments(function_body(self.plugin, "static void PpCommand("))
        self.assertIn('sub == "backing"', command)
        usage = function_body(self.plugin, "static void PpUsage(")
        for text in ("backing on|off", "backing dump", "backing idcheck"):
            self.assertIn(text, usage)
        self.assertNotIn("PpBacking", strip_research_blocks(self.plugin))

    def test_idcheck_runs_a_positive_control_before_its_verdict(self):
        body = self.backing_functions()["PpBackingIdCheck"]
        control = body.index('"array_create"')
        message = body.index("stash does not track live arrays")
        self.assertLess(control, message)
        for later in ('"nodeGrid"', "reference-identical", "copy"):
            self.assertLess(message, body.index(later), later)
        # The control and the scanner are proven both ways: the scan finds the
        # sentinel through the stash, and does not find it in a separate array.
        self.assertIn("scanner finds it in a separate array", body)
        self.assertLess(body.index("scanner finds it in a separate array"), body.index('"nodeGrid"'))
        # A failed control returns before anything touches the game's store.
        failed = body[message:]
        self.assertLess(failed.index("return;"), failed.index('"nodeGrid"'))
        # `copy` needs a complete scan; an incomplete one is not observed.
        self.assertIn("scan incomplete", body)
        self.assertLess(body.index("scan incomplete"), body.rindex("copy"))
        # Round-0 P0c-B1: a value the walk cannot look inside is not a leaf. Only
        # a number, bool, string, undefined, null or unset is; a reference
        # (VALUE_REF, how this runner hands out instances), a method value or a
        # pointer counts as unwalked, and an unwalked value makes the walk
        # incomplete - so storage behind a handle can never read as `copy`.
        bodies = self.backing_functions()
        leaf = bodies["PpBackingIsPlainLeaf"]
        for kind in ("VALUE_REAL", "VALUE_INT32", "VALUE_INT64", "VALUE_BOOL", "VALUE_STRING", "VALUE_UNDEFINED"):
            self.assertIn(kind, leaf)
        for kind in ("VALUE_REF", "VALUE_PTR", "VALUE_OBJECT", "VALUE_ARRAY"):
            self.assertNotIn(kind, leaf)
        self.assertIn("return false;", leaf[leaf.index("default:"):])
        walk = bodies["PpBackingWalk"]
        self.assertLess(walk.index("PpBackingIsPlainLeaf(v)"), walk.index("++scan.references"))
        self.assertIn("++scan.methods", walk)
        self.assertIn("bool Complete() const { return !truncated && depthCapped == 0 && Unwalked() == 0; }", self.plugin)
        self.assertIn("long Unwalked() const { return methods + references; }", self.plugin)
        # Any kept return of any getter holding the sentinel decides
        # `reference-identical (via <getter> ...)`; `copy` only when every walk
        # completed and none hit. No getter is skipped when reading the hits.
        self.assertNotIn("g_PpBackingProfile) continue", body)
        self.assertIn("reference-identical (via ", body)
        reading = body[body.index("for (const PpBackingHit& h : results) {"):body.index("std::string verdict;")]
        self.assertNotIn("continue", reading)
        self.assertNotIn("return", reading)
        self.assertIn("if (!h.hits.empty() && via.empty()) via = h.what", reading)
        verdict = body[body.index("std::string verdict;"):]
        self.assertLess(verdict.index("!via.empty()"), verdict.index("incomplete > 0"))
        self.assertLess(verdict.index("incomplete > 0"), verdict.index('"copy: every walk'))
        walked = body[body.index('"array_set", { row, RValue((double)c), RValue(kPpBackingSentinel) }'):
                      body.index('"array_set", { row, RValue((double)c), original }')]
        self.assertIn("PpBackingForEachKept(", walked)

    def test_idcheck_refuses_unless_a_kept_return_came_from_the_open_window(self):
        # Round-0 P0c-B2: UI_Prospect_obj holds two grids and a later open calls
        # the getters again, so the last return is not necessarily what this
        # ProspectGrid was built from. Every window-self return is kept (bounded)
        # with its @id, and idcheck writes nothing unless one of them came from
        # the window open now.
        bodies = self.backing_functions()
        capture = bodies["PpBackingCapture"]
        self.assertIn("st->windowKept % kPpBackingKeepMax", capture)
        self.assertIn("windowSelf ? st->window[slotIndex] : st->other", capture)
        self.assertIn("PpInstanceId(S->ToRValue(), slot.selfId)", capture)
        self.assertIn("static constexpr int kPpBackingKeepMax = 8;", self.plugin)
        # An unreadable id is -1 and a failed read returns false, so two
        # unreadable ids never match each other.
        instance_id = strip_comments(function_body(self.plugin, "static bool PpInstanceId("))
        self.assertIn("id = -1;", instance_id)
        self.assertIn("v.ToDouble() <= 0) return false;", instance_id)
        body = bodies["PpBackingIdCheck"]
        write = body.index('"array_set", { row, RValue((double)c), RValue(kPpBackingSentinel) }')
        refusal = body.index("no kept return came from the open window")
        self.assertLess(refusal, write)
        self.assertIn("return;", body[refusal:write])
        self.assertIn("k.selfId != windowId", body[:refusal])
        self.assertLess(body.index("PpInstanceId(window, windowId)"), refusal)
        self.assertIn("no open UI_Prospect_obj window with a readable id", body)
        self.assertLess(body.index("no open UI_Prospect_obj window with a readable id"), write)
        # The live procedure runs idcheck before any item is moved: C3 comes
        # before C2b, and before the first item is placed.
        live = collapse(section(self.doc, "## Phase 0c live procedure"))
        self.assertLess(live.index("**C3"), live.index("**C2b"))
        self.assertLess(live.index("`prospectprobe backing idcheck`"), live.index("places one item"))
        self.assertIn("with no item moved", live[:live.index("**C2b")])

    def test_structural_agreement_counts_only_non_empty_cells_and_is_a_lead(self):
        # Round-0 P0c-B3: an empty 6x9 agrees with any 6x9 of empties, and a copy
        # agrees by construction. Agreement counts only nodeGrid's non-empty
        # cells, compares objects by identity rather than by how they print, and
        # the dump says it never picks a gate branch.
        bodies = self.backing_functions()
        agree = bodies["PpBackingCellsAgree"]
        self.assertLess(agree.index("PpBackingIsEmptyCell(g)) continue;"), agree.index("++same"))
        self.assertIn("++occupied", agree)
        same = bodies["PpBackingSameValue"]
        self.assertIn("a.m_Pointer == b.m_Pointer", same)
        self.assertNotIn("Describe(", same)
        dump = bodies["PpBackingDump"]
        self.assertIn("occupied == 0", dump)
        self.assertIn("nodeGrid holds no item - nothing to compare", dump)
        self.assertIn("never picks a gate branch", dump)

    def test_idcheck_restores_the_cell_and_only_writes_an_empty_one(self):
        body = self.backing_functions()["PpBackingIdCheck"]
        # The control writes the sentinel into an array it built itself; the one
        # write into the game's store is the one on `row`.
        write = body.index('"array_set", { row, RValue((double)c), RValue(kPpBackingSentinel) }')
        self.assertEqual(body.count('"array_set", { row,'), 2, "one sentinel write and one restore on the live row")
        empty = self.backing_functions()["PpBackingIsEmptyCell"]
        self.assertIn("VALUE_UNDEFINED", empty)
        self.assertIn("== 0.0", empty)
        self.assertLess(body.index("PpBackingIsEmptyCell("), write)
        refusal = body.index("no empty cell")
        self.assertLess(refusal, write)
        self.assertIn("return;", body[refusal:write])
        restore = body.index('"array_set", { row, RValue((double)c), original }')
        self.assertLess(write, restore)
        self.assertLess(restore, body.rindex("reference-identical"))
        self.assertIn("restored", body[restore:])
        # Refusals before any write: no capture, no node.
        for refusal in ("never captured", "no ProspectGrid node"):
            self.assertLess(body.index(refusal), write, refusal)
        # The write is checked against a fresh read of the live store; a write
        # that did not land cannot be read as a copy.
        self.assertIn("did not land", body)

    def test_research_doc_records_phase0b_and_the_ghidra_reading(self):
        self.assertIn("phase0-status: pending", self.doc)
        results = section(self.doc, "## Results")
        for text in ("Phase 0b", "profile inventory", "Ghidra read (paraphrase)", "instrument misuse",
                     "Craft_Grid_Large_spr"):
            self.assertIn(text, results)
        self.assertNotIn("does not happen", self.doc.lower())
        header = [line for line in results.replace("\r\n", "\n").split("\n") if line.startswith("| Field |")][0]
        self.assertIn("Phase 0b", header)
        self.assertIn("Phase 0c", header)
        row = [line for line in results.replace("\r\n", "\n").split("\n") if line.startswith("| R1 |")][0]
        cells = [c.strip() for c in row.strip("|").split("|")]
        self.assertEqual(len(cells), 5)
        self.assertNotEqual(cells[3], "unknown")   # Phase 0b, filled
        # Phase 0c's cells stay `unknown` until the live session fills them.
        for field in ("R7", "R12", "backing structural", "backing idcheck", "load-time capture", "gate branch", "H"):
            self.assertTrue(any(line.startswith("| " + field + " |") for line in results.replace("\r\n", "\n").split("\n")),
                            field)
        self.assertIn("## Phase 0c live procedure", self.doc)
        live = collapse(section(self.doc, "## Phase 0c live procedure"))
        for label in ("**C1.**", "**C2", "**C2b", "**C3", "**C4", "**C5", "**C6.**", "**C7", "**C8.**"):
            self.assertIn(label, live)
        instrument = collapse(section(self.doc, "## Instrument"))
        for command in ("`prospectprobe backing on|off`", "`prospectprobe backing dump`", "`prospectprobe backing idcheck`"):
            self.assertIn(command, instrument)
        self.assertLess(instrument.index("`prospectprobe grid`"), instrument.index("`prospectprobe backing on|off`"))

    def test_research_doc_states_the_decision_gate(self):
        deciding = collapse(section(self.doc, "## Deciding the hypothesis"))
        for text in ("save-data", "save-backed", "reference-identical", "stranded", "ValidateInventory",
                     "prospect all", "auto-prospect", "UiAProspectButton", "H2''"):
            self.assertIn(text, deciding)
        gate = deciding[deciding.index("auto-prospect"):]
        self.assertIn("claims to verify", gate)
        # Round-0 P0c-B3: the gate's rules have an order, and structural
        # agreement is not one of them.
        rules = deciding[deciding.index("### Decision gate"):]
        self.assertIn("in this order, and the first that holds decides", rules)
        saved = rules.index("- **Save-backed** —")
        not_saved = rules.index("- **Not save-backed** — only when save-backed does not hold")
        inconclusive = rules.index("- **Inconclusive** — everything else")
        self.assertLess(saved, not_saved)
        self.assertLess(not_saved, inconclusive)
        self.assertNotIn("mirrors a profile sub-array", rules[saved:not_saved])
        self.assertIn("**Structural agreement is a lead, never a branch.**", rules)
        self.assertIn("it never picks save-backed, not save-backed or inconclusive", rules)
        # Round-0 P0c-B1, in the doc: what counts as unwalked, and `copy` only
        # when every walk completed.
        instrument = collapse(section(self.doc, "## Instrument"))
        self.assertIn("an instance reference (`VALUE_REF`, how this runner hands out instances), a method "
                      "value (its bound `self` may hold the storage), a pointer — counts as unwalked.", instrument)
        self.assertIn("`copy` **only** when every walk completed and none holds it", instrument)
        self.assertIn("**no kept window return whose `@id` is the open window's**", instrument)

    # Tokens a decompiler prints and this repository never commits. Spelled in
    # pieces so this file does not match its own guard.
    DECOMPILER_TOKENS = ("FUN" + "_14", "undefined" + "8", "__fast" + "call", "gml_push" + "glb",
                         "@@This" + "@@", "uStack" + "_", "param" + "_1")

    def test_research_doc_carries_no_decompiler_tokens(self):
        for token in self.DECOMPILER_TOKENS:
            self.assertNotIn(token, self.doc)

    def test_contract_tests_carry_no_decompiler_tokens(self):
        this = Path(__file__).read_text(encoding="utf-8")
        for token in self.DECOMPILER_TOKENS:
            self.assertNotIn(token, this)


if __name__ == "__main__":
    unittest.main()
