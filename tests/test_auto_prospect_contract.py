"""Contract tests for auto-prospect on insert (ForgePact issue #9, Stage B).

The human chose auto-prospect on insert over a bigger prospect grid on
2026-09-18 (docs/prospect-window-research.md, § Decision gate). Its decision
core, plugin/include/ForgePact/AutoProspectMod.hpp, is pinned behaviourally by
test_auto_prospect_behavior.py. This file pins everything around it, on
comment-stripped source: the core names no runtime interface; the adapter in
ModuleMain.cpp hooks m_MoveItemToGrid through the SDK constant and both of
HookOneScript's routes, lazily and once, and turns itself off when only the
table route went in; the hook body never invokes; the grid is identified by
what it is; the invoke happens in FrameCallback on objects re-found there, with
the one shape Phase 1 recorded (§ Stage B results, P-shapes), by name only;
the mod is off by default and nothing of it runs while it is off; `autoprospect`
is a player command and `autoprospect stat` is research-only; the player build
logs each refusal once and the first prospect once; and the panel, the release
notes and the docs carry it.

Stage C (the previous batch to the materials tab) adds: a material is
identified in the adapter by its itemType against the SDK's
ItemType::Material, and the core knows no item type; the move is the recorded
M7 route by name only (four SDK script names through script_execute, the grid
node as self and other); the pass runs in AutoProspectTick between the core's
first decision and the invoke, with a second read and decision in the same
frame; a cell is cleared only after the add reported success on the same
fingerprint; `bag` is a sub-option of `autoprospect`, on by default and nested
in the panel; each reason a material stayed, and the pass turning itself off,
is logged once; and the notes, README and research doc record it.
"""
import importlib.util
import re
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT / "src") not in sys.path:
    sys.path.insert(0, str(ROOT / "src"))   # the panel imports its sibling modules
HEADER = ROOT / "plugin" / "include" / "ForgePact" / "AutoProspectMod.hpp"
PLUGIN = ROOT / "plugin" / "ModuleMain.cpp"
PANEL = ROOT / "src" / "forgepact.py"
NOTES = ROOT / "release-notes-v1.4.4.md"
README = ROOT / "README.md"
DOC = ROOT / "docs" / "prospect-window-research.md"

_spec = importlib.util.spec_from_file_location(
    "_release_hook_contract_ap", ROOT / "tests" / "test_release_hook_contract.py")
_release = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(_release)
strip_research_blocks = _release.strip_research_blocks
function_body = _release.function_body
strip_comments = _release.strip_comments

ADAPTER = ("static bool ApIsProspectGrid(", "static RValue& Hook_AutoProspectInsert(", "static bool ApFindWindow(",
           "static bool ApFindGrid(", "static bool ApReadCells(", "static bool ApFindButton(",
           "static void AutoProspectTick(", "static void AutoProspectInstall(", "static void AutoProspectCommand(",
           # Stage C: the move of the previous batch to the materials tab.
           "static bool ApIsPlainStruct(", "static bool ApIsEmptyCell(", "static bool ApReadCell(",
           "static bool ApCellFingerprint(", "static bool ApCallScript(", "static bool ApIsMaterial(",
           "static bool ApItemFromFingerprint(", "static bool ApReadMaterials(", "static bool ApAddSucceeded(",
           "static int ApCellHolds(", "static ForgePact::AutoProspectMoveReport ApMoveCell(",
           "static void ApMovePass(",
           # Stage D: the new-type route's preferred grid.
           "static bool ApPreferredGrid(")


def collapse(text):
    return " ".join(text.split())


class AutoProspectContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.header = HEADER.read_text(encoding="utf-8")
        cls.plugin = PLUGIN.read_text(encoding="utf-8")
        cls.code = strip_comments(cls.plugin)
        cls.shipped = strip_comments(strip_research_blocks(cls.plugin))

    def body(self, signature, source=None):
        return strip_comments(function_body(source if source is not None else self.plugin, signature))

    def test_core_header_is_game_independent(self):
        for forbidden in ("g_Yytk", "CallBuiltin", "Out(", "RValue"):
            self.assertNotIn(forbidden, self.header)
        includes = [l.strip() for l in self.header.split("\n") if l.strip().startswith("#include")]
        self.assertEqual(includes, ['#include "Common.hpp"'])
        # Off by default, and the hook-side entry point never decides to invoke.
        self.assertIn("std::atomic<bool> m_Enabled{ false };", self.header)
        self.assertIn("void OnInsert(int64_t nodeId, bool isProspectGrid, bool invoking)", self.header)

    # ---- the hook ------------------------------------------------------------

    def test_hook_resolves_through_the_sdk_constant_with_both_routes(self):
        install = self.body("static void AutoProspectInstall(")
        self.assertIn(
            "HookOneScript(SdkShortScriptName(HeroSiege::Scripts::"
            "gml_Script_anon_15345_gml_Object_UI_Inventory_Grid_obj_Create_0)", collapse(install))
        self.assertNotIn("HookOneScriptTable", install)
        self.assertNotIn("anon@15345", install)   # no raw literal: the SDK constant fails the build if it moves
        # The installer reports whether its own inline detour went in.
        hook_one = self.body("static bool HookOneScript(")
        self.assertIn("if (nativeOut) *nativeOut = false;", hook_one)
        self.assertIn("if (nativeOut) *nativeOut = true;", hook_one)
        self.assertLess(hook_one.index("MmCreateHook"), hook_one.index("if (nativeOut) *nativeOut = true;"))

    def test_hook_installs_lazily_once_and_reports_table_only(self):
        install = self.body("static void AutoProspectInstall(")
        self.assertIn("g_AutoProspectInstallTried = true;", install)
        self.assertIn("&native);", install)
        self.assertIn('if (ok && native) { Out("autoprospect: hook installed -> ON"); return; }', install)
        # Anything short of both routes turns the mod off, and says so.
        self.assertIn("g_AutoProspectBlind = true;", install)
        self.assertIn("SetEnabled(false);", install)
        self.assertIn('"TABLE-ONLY"', install)
        # Only FrameCallback installs, once, after setup and only while on.
        frame = self.body("void FrameCallback(")
        self.assertIn("if (ForgePact::AutoProspectMod::Instance().IsEnabled() && g_Setup) {", frame)
        self.assertIn("if (!g_AutoProspectInstallTried) AutoProspectInstall();", frame)
        self.assertEqual(self.code.count("AutoProspectInstall();"), 1)
        # The command never installs anything itself.
        command = self.body("static void AutoProspectCommand(")
        self.assertNotIn("HookOneScript", command)
        self.assertNotIn("AutoProspectInstall", command)
        self.assertIn("if (g_AutoProspectBlind) {", command)

    def test_hook_never_invokes(self):
        hook = self.body("static RValue& Hook_AutoProspectInsert(")
        # The game's own function first, through the trampoline.
        self.assertIn("g_Orig_AutoProspectInsert ? g_Orig_AutoProspectInsert(S, O, R, argc, A) : R;", hook)
        self.assertLess(hook.index("g_Orig_AutoProspectInsert("), hook.index("OnInsert("))
        # Off: counted, nothing read.
        self.assertIn("if (!mod.IsEnabled()) { mod.OnInsert(-1, false, false); return res; }", hook)
        self.assertLess(hook.index("if (!mod.IsEnabled())"), hook.index("ApIsProspectGrid("))
        for forbidden in ("CallBuiltinEx", "script_execute", "CallGameScript", "Decide(", "ApReadCells", "AutoProspectTick"):
            self.assertNotIn(forbidden, hook)
        self.assertIn("mod.OnInsert(nodeId, isGrid, g_AutoProspectInvoking);", hook)
        # The invoking flag brackets the one call, so an insert the game makes
        # inside it is ignored and counted.
        tick = self.body("static void AutoProspectTick(")
        call = tick.index("CallBuiltinEx(")
        self.assertLess(tick.index("g_AutoProspectInvoking = true;"), call)
        self.assertLess(call, tick.index("g_AutoProspectInvoking = false;"))

    def test_prospect_grid_identified_by_what_it_is(self):
        grid = self.body("static bool ApIsProspectGrid(")
        # Its object, by index through the SDK name, accepting VALUE_REF ...
        self.assertIn("GameObject::UI_Inventory_Grid_obj", grid)
        self.assertIn("N1ObjectIndex(", grid)
        self.assertIn('RValue("object_index")', grid)
        # ... AND its name: the HUD's inventory grids are the same object.
        self.assertIn("ApNamesProspectGrid(node)", grid)
        names = self.body("static bool ApNamesProspectGrid(")
        self.assertIn('RValue("uiNodeCallstack")', names)
        self.assertIn("ForgePact::kAutoProspectGridName", names)
        self.assertIn("VALUE_STRING", names)
        self.assertIn("VALUE_ARRAY", names)
        self.assertIn('kAutoProspectGridName = "ProspectGrid";', self.header)
        self.assertIn("ApNamesProspectGrid(inst)", self.body("static bool ApFindGrid("))
        # The button: linked to the window AND carrying the handler, compared
        # by method_get_index against asset_get_index - never "the only linked one".
        button = self.body("static bool ApFindButton(")
        self.assertIn("GameObject::UI_Button_Small_obj", button)
        self.assertIn('{ "masterUi", "parent" }', button)
        self.assertIn("ForgePact::kAutoProspectHandlerVar", button)
        self.assertIn('"is_method"', button)
        self.assertIn('"method_get_index"', button)
        self.assertIn("mi != scriptIdx", button)
        self.assertIn("if (qualified != 1) { button = RValue(); return false; }", button)
        self.assertIn('kAutoProspectHandlerVar = "activationFunc";', self.header)
        # No kind check decides whether the work happens: ids and indices are
        # accepted as VALUE_REF too.
        number = self.body("static bool ApNumber(")
        self.assertIn("VALUE_REF", number)

    def test_invoke_is_at_point_of_use_in_frame_callback(self):
        tick = self.body("static void AutoProspectTick(")
        decide = tick.index("mod.Decide(v)")
        # Everything the decision reads is re-found in this call, before it.
        for read in ("ApFindWindow(window, windowId)", "ApFindGrid(node, nodeId)", "ApReadCells(node, v,",
                     "ApFindButton(windowId, handlerIndex, button, args, argsOk)",
                     "HhResolveInstance(window)", "HhResolveInstance(button)"):
            self.assertIn(read, tick)
            self.assertLess(tick.index(read), decide)
        self.assertLess(decide, tick.index("CallBuiltinEx("))
        self.assertIn("if (d.action == ForgePact::AutoProspectAction::Invoke) {", tick)
        # The grid is re-read after the call and handed to the core with it.
        self.assertIn("mod.OnInvoked(dispatched, after);", tick)
        self.assertLess(tick.index("CallBuiltinEx("), tick.index("ApReadCells(n, after, true)"))
        # Exactly one invoke site, and it is in the tick FrameCallback runs.
        self.assertEqual(self.shipped.count('"script_execute", buttonInst, windowInst'), 1)
        frame = self.body("void FrameCallback(")
        self.assertIn("if (g_Orig_AutoProspectInsert && ForgePact::AutoProspectMod::Instance().IsEnabled()) AutoProspectTick();", frame)

    def test_invoke_uses_the_recorded_shape_by_name_only(self):
        tick = self.body("static void AutoProspectTick(")
        # exec-index: the handler's own asset index, by its SDK name.
        self.assertIn('CallBuiltin("asset_get_index", { RValue(std::string(SdkShortScriptName(HeroSiege::Scripts::gml_Script_UiAProspectButton))) })', collapse(tick))
        # through script_execute, self = the found button, other = the window,
        # the one argument = the button's own activationArgs array.
        self.assertIn("std::vector<RValue> callArgs{ handlerIndex, args };", tick)
        self.assertIn('CallBuiltinEx(res, "script_execute", buttonInst, windowInst, callArgs)', tick)
        button = self.body("static bool ApFindButton(")
        self.assertIn("ForgePact::kAutoProspectArgsVar", button)
        self.assertIn("argsOk = args.m_Kind == VALUE_ARRAY;", button)
        self.assertIn('kAutoProspectArgsVar = "activationArgs";', self.header)
        # By name only: no address, no struct layout, no captured argument.
        for signature in ADAPTER:
            body = self.body(signature)
            for forbidden in ("Rva", "MethodValueFunction", "CScriptRef", "m_CallYYC", "InvokeMethodValue",
                              "GetModuleHandle", "__pp_press_arg", "g_PpPress", "CallGameScriptEx"):
                self.assertNotIn(forbidden, body, f"{signature} names {forbidden}")
        # And none of it is a research helper the player build does not have.
        for signature in ADAPTER:
            self.assertIn(signature, self.shipped)
            self.assertNotRegex(self.body(signature), r"\bPp[A-Z]\w*\(")

    def test_off_by_default_and_frame_path_gated(self):
        self.assertIn("std::atomic<bool> m_Enabled{ false };", self.header)
        frame = self.body("void FrameCallback(")
        gate = frame.index("if (ForgePact::AutoProspectMod::Instance().IsEnabled() && g_Setup) {")
        # Nothing of the mod appears in FrameCallback outside that block.
        block_end = frame.index("}", frame.index("AutoProspectTick();", gate))
        outside = frame[:gate] + frame[block_end:]
        self.assertNotIn("AutoProspect", outside)
        self.assertIsNone(re.search(r"\bAp[A-Z]\w*\(", outside))
        # The panel's default is off, and the default config sends nothing.
        panel = PANEL.read_text(encoding="utf-8")
        self.assertIn('"mod_auto_prospect": False,', panel)

    def test_autoprospect_is_a_player_command_and_stat_is_research_only(self):
        allowlist = re.search(r"kPlayerCommands\s*=\s*\{(?P<body>.*?)\};", self.plugin, re.S).group("body")
        entries = re.findall(r'"([^"]+)"', allowlist)
        self.assertIn("autoprospect", entries)
        self.assertNotIn("prospectprobe", entries)
        handler = self.body("static bool HandleProspectCommand(")
        self.assertIn('if (lc == "autoprospect") { AutoProspectCommand(rest); return true; }', handler)
        self.assertLess(handler.index('"autoprospect"'), handler.index("#ifndef FORGEPACT_RELEASE"))
        self.assertIn('"autoprospect"', strip_comments(strip_research_blocks(function_body(self.plugin, "static bool HandleProspectCommand("))))
        command = function_body(self.plugin, "static void AutoProspectCommand(")
        self.assertRegex(command, r'#ifndef FORGEPACT_RELEASE\s*if \(v == "stat"\) \{[\s\S]*?StatLine\(\)[\s\S]*?#endif')
        shipped_command = self.body("static void AutoProspectCommand(", strip_research_blocks(self.plugin))
        self.assertNotIn('"stat"', shipped_command)
        self.assertNotIn("StatLine()", shipped_command)
        # 0 is off, like every other toggle.
        self.assertIn('const bool off = v == "0" || v == "off" || v == "false";', shipped_command)

    def test_first_refusal_is_logged_once_per_reason(self):
        tick = self.body("static void AutoProspectTick(")
        self.assertIn("for (ForgePact::AutoProspectRefusal r = mod.TakeFirstRefusal(); "
                      "r != ForgePact::AutoProspectRefusal::None; r = mod.TakeFirstRefusal())", collapse(tick))
        self.assertIn("Out(mod.RefusalLine(r));", tick)
        # The player build's line naming work done (S-player-dll), once.
        self.assertIn("if (mod.TakeFirstProspect()) Out(mod.FirstProspectLine());", tick)
        self.assertIn('HeadedStatLine("first prospect")', self.header)
        # An invoke that ran and changed nothing, or could not be checked, is
        # said once each (Phase 3 S7: ran-no-effect=1 reached only `stat`).
        self.assertIn("if (mod.TakeFirstRanNoEffect()) Out(mod.RanNoEffectLine());", tick)
        self.assertIn("if (mod.TakeFirstUnverified()) Out(mod.UnverifiedLine());", tick)
        self.assertIn('HeadedStatLine("the Prospect ran but the grid did not change")', self.header)
        self.assertIn('HeadedStatLine("the Prospect ran but the grid could not be read afterwards")', self.header)
        # A failed dispatch is said once, too.
        self.assertIn("if (!dispatched && !g_AutoProspectDispatchLogged) {", tick)
        # All of it ships: none of these lines sits behind the research guard.
        shipped_tick = self.body("static void AutoProspectTick(", strip_research_blocks(self.plugin))
        for line in ("Out(mod.RefusalLine(r));", "Out(mod.FirstProspectLine());", "Out(mod.RanNoEffectLine());",
                     "Out(mod.UnverifiedLine());", "did not dispatch"):
            self.assertIn(line, shipped_tick)
        self.assertIn('"holding back - "', self.header)
        # The grid-full line names the grid, not materials: Phase 3 S5's grid
        # was full of items when it first printed.
        self.assertIn('"; empty some of the grid"', self.header)
        self.assertNotIn("take the materials out", self.header)

    # ---- the panel, the notes, the docs ----------------------------------------

    def test_panel_toggle_defaults_off_and_emits_autoprospect(self):
        spec = importlib.util.spec_from_file_location("forgepact_ap_contract", PANEL)
        panel = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(panel)
        import copy
        cfg = copy.deepcopy(panel.DEFAULTS)
        self.assertIs(cfg["mod_auto_prospect"], False)
        self.assertNotIn("autoprospect 1", panel.build_cmds(cfg))
        self.assertFalse(any(c.startswith("autoprospect") for c in panel.build_cmds(cfg)))
        cfg["mod_auto_prospect"] = True
        self.assertIn("autoprospect 1", panel.build_cmds(cfg))
        source = PANEL.read_text(encoding="utf-8")
        self.assertGreaterEqual(len([l for l in source.split("\n") if "mod_auto_prospect" in l]), 7)
        # The live send; turning it on also restates the Stage C sub-option.
        self.assertIn('cmds = [f"autoprospect {1 if cfg[\'mod_auto_prospect\'] else 0}"]', source)
        self.assertIn('id="mod_auto_prospect"', source)
        # The switch's copy: what is left in the grid when the game saves is
        # lost. It no longer tells the player to take the materials out -
        # Stage C's sub-switch moves them to the materials tab.
        row = source[source.index('id="mod_auto_prospect"') - 1200:source.index('id="mod_auto_prospect"')]
        self.assertIn("lost", row)
        self.assertNotIn("Materials stay in the grid", row)
        self.assertNotIn("take them out", row)
        self.assertNotIn("take the materials out", source)

    def test_release_notes_and_docs_record_the_feature(self):
        notes = NOTES.read_text(encoding="utf-8")
        headings = [l for l in notes.split("\n") if l.startswith("## ")]
        # 1.4.4 also carries the mod-loader rebuild from main as its own
        # `## Fixed`; auto-prospect itself is `## New` only.
        self.assertEqual(headings, ["## New", "## Fixed", "## How to update"])
        self.assertIn("off by default", notes.lower())
        self.assertIn("lost", notes.lower())
        self.assertIn("9×6", notes)
        self.assertNotIn("prospectprobe", notes)   # player language, no research commands
        readme = README.read_text(encoding="utf-8")
        self.assertIn("Auto-prospect", readme)
        self.assertIn("lost", readme[readme.index("Auto-prospect"):])
        doc = DOC.read_text(encoding="utf-8")
        self.assertIn("\nphase1-status: complete\n", doc.replace("\r\n", "\n"))
        self.assertNotIn("phase1-status: pending", doc)
        self.assertIn("exec-index button:activationArgs self=found", doc)
        self.assertNotIn("cannot re-trigger", doc)
        self.assertNotIn("Materials never merge", doc)

    # ---- Stage C: the previous batch to the materials tab -----------------------

    def test_material_identified_by_its_item_type_through_the_sdk(self):
        material = self.body("static bool ApIsMaterial(")
        self.assertIn('RValue("itemType")', material)
        self.assertIn("HeroSiege::Items::ItemType::Material", material)
        self.assertIn("ApIsPlainStruct(item)", material)
        # The flag the core gets comes from the item, looked up by the game.
        materials = self.body("static bool ApReadMaterials(")
        self.assertIn("c.material = ApItemFromFingerprint(gridInst, fp, item) && ApIsMaterial(item);", materials)
        # No local constant, no fingerprint suffix, and the core knows no item type.
        for source in (self.header, self.plugin):
            self.assertNotIn("kMaterialItemType", source)
        self.assertNotIn('-14"', self.shipped)
        for forbidden in ("ItemType", "itemType", "14"):
            self.assertNotIn(forbidden, strip_comments(self.header))
        self.assertIn("bool        material = false;", self.header)
        # Identity is read only on frames the core may ask for a pass.
        tick = self.body("static void AutoProspectTick(")
        gate = tick.index("if (v.button && v.args && mod.NeedsMaterials()) {")
        self.assertLess(gate, tick.index("ApReadMaterials(node, gridInst, v)"))
        self.assertEqual(self.code.count("ApReadMaterials("), 2)   # its definition and this one call

    def test_move_uses_the_recorded_shape_by_name_only(self):
        # The four scripts by their SDK names, through script_execute with the
        # grid node as self and other - the M7 route.
        for name in ("GetItemFromFingerprint", "InventoryGridCanAddToStack", "InventoryGridAddToStack",
                     "InvGridClearItemNode"):
            self.assertIn(f"SdkShortScriptName(HeroSiege::Scripts::gml_Script_{name})", self.shipped)
        call = self.body("static bool ApCallScript(")
        self.assertIn('CallBuiltin("asset_get_index", { RValue(std::string(name)) })', call)
        self.assertIn('CallBuiltinEx(res, "script_execute", gridInst, gridInst, callArgs)', call)
        self.assertIn("return AurieSuccess(st);", call)
        # The recorded arguments, exactly.
        self.assertIn("ApCallScript(kApFromFpName, gridInst, { fp, RValue(0.0) }, item)",
                      self.body("static bool ApItemFromFingerprint("))
        move = self.body("static ForgePact::AutoProspectMoveReport ApMoveCell(")
        self.assertIn("ApCallScript(kApCanAddName, gridInst, { RValue(1.0), RValue(), item }, canRes)", move)
        self.assertIn("ApCallScript(kApAddName, gridInst, { RValue(1.0), item }, addRes)", move)
        self.assertIn("ApCallScript(kApClearName, gridInst, { cellNow, RValue() }, clearRes)", move)
        # The research command's names are not reused: the player build has none of them.
        for forbidden in ("kPpFromFpName", "kPpCanAddName", "kPpAddName", "kPpClearName", "kPpPreferredName",
                          "kPpGridAddName", "PpStackMoveCommand", "PpPlaceSucceeded"):
            for signature in ADAPTER:
                self.assertNotIn(forbidden, self.body(signature))
        # Exactly one invoke of the Prospect button, still: the moves use the grid.
        self.assertEqual(self.shipped.count('"script_execute", buttonInst, windowInst'), 1)
        self.assertEqual(self.shipped.count('"script_execute", gridInst, gridInst'), 1)

    def test_move_pass_runs_at_the_point_of_use_before_the_invoke(self):
        tick = self.body("static void AutoProspectTick(")
        first = tick.index("ForgePact::AutoProspectDecision d = mod.Decide(v);")
        self.assertLess(first, tick.index("if (d.action == ForgePact::AutoProspectAction::MoveMaterials) {"))
        order = [tick.index(s) for s in ("ApMovePass(d, node);", "read(moved);", "d = mod.Decide(moved);",
                                         "if (d.action == ForgePact::AutoProspectAction::Invoke) {", "CallBuiltinEx(")]
        self.assertLess(first, order[0])
        self.assertEqual(order, sorted(order))
        # The second read re-finds everything the invoke uses, through the same reader.
        self.assertIn("auto read = [&](ForgePact::AutoProspectView& v) {", tick)
        self.assertLess(tick.index("auto read = [&]"), first)
        # The pass holds the invoking flag across every move, and stops as
        # soon as the core turns it off.
        move_pass = self.body("static void ApMovePass(")
        loop = move_pass.index("for (const ForgePact::AutoProspectCell& c : d.moves) {")
        self.assertLess(move_pass.index("g_AutoProspectInvoking = true;"), loop)
        self.assertLess(loop, move_pass.index("g_AutoProspectInvoking = false;"))
        self.assertLess(loop, move_pass.index("if (!mod.MovePassOn()) break;"))
        self.assertIn("mod.OnMoveReport(r);", move_pass)
        self.assertEqual(self.code.count("ApMovePass(d, node);"), 1)
        # The core asks only from Decide, once per landed insert, just before the free-cell check.
        decide = self.header[self.header.index("AutoProspectDecision Decide("):]
        self.assertLess(decide.index("if (!in.args)"), decide.index("if (MovePassOn() && !m_PassDone) {"))
        self.assertLess(decide.index("if (MovePassOn() && !m_PassDone) {"), decide.index("if (in.empty < kAutoProspectMinFreeCells)"))
        # Nothing new reaches FrameCallback.
        frame = self.body("void FrameCallback(")
        self.assertNotIn("ApMovePass", frame)
        self.assertNotIn("MoveMaterials", frame)

    def test_clear_only_after_success_on_the_same_fingerprint(self):
        move = self.body("static ForgePact::AutoProspectMoveReport ApMoveCell(")
        # Stage D: a has-a-stack "no" is the new-type route now, not a return;
        # both routes end at the one clear site.
        steps = [move.index(s) for s in (
            "r.heldBefore = true;",
            "r.lookup = ApItemFromFingerprint(gridInst, fp, item) && ApIsMaterial(item);",
            "kApCanAddName",
            "if (!r.canAddRan) { r.heldAfter = ApCellHolds(node, c); return r; }",
            "if (r.canAdd) {",
            "kApAddName",
            "r.success = r.addRan && ApAddSucceeded(addRes);",
            "} else {",
            "kApPreferredName",
            "kApPlaceName",
            "r.success = r.placeRan && ApAddSucceeded(placeRes);",
            "if (r.success && ApCellHolds(node, c) == 1) {",
            "kApClearName",
            "r.heldAfter = ApCellHolds(node, c);\n    } catch")]
        self.assertEqual(steps, sorted(steps))
        self.assertEqual(move.count("kApClearName"), 1)
        # Before the first call, the cell must still hold what the view saw.
        self.assertIn("text != c.fingerprint", move[:move.index("r.heldBefore = true;")])
        success = self.body("static bool ApAddSucceeded(")
        self.assertIn('RValue("success")', success)
        self.assertIn("VALUE_BOOL", success)
        # An unreadable cell is never "gone".
        holds = self.body("static int ApCellHolds(")
        self.assertIn("if (!ApReadCell(node, c.row, c.col, cell)) return -1;", holds)
        # The core: moved only with success and the cell no longer holding it.
        classify = strip_comments(self.header[self.header.index("static AutoProspectMoveOutcome ClassifyMove("):])
        classify = classify[:classify.index("\n    }\n")]
        self.assertIn("if (r.success) {", classify)
        self.assertIn("if (r.heldAfter == 0) return AutoProspectMoveOutcome::Moved;", classify)
        self.assertLess(classify.index("if (r.success) {"), classify.index("AutoProspectMoveOutcome::Moved"))
        self.assertIn("if (r.heldAfter == 0) return AutoProspectMoveOutcome::Vanished;", classify)

    # ---- Stage D: a material whose type has no stack yet -----------------------

    def test_new_type_route_runs_only_after_the_stack_check_said_no(self):
        # The route the game's own click-move took (research doc, § Stage D
        # results): the preferred grid for the item, then the place into its
        # `grid` member, by SDK name through the same script_execute site, the
        # grid node as self and other - and only on the has-a-stack check's "no".
        for name in ("GetItemPreferredGrid", "GridAddItem"):
            self.assertIn(f"SdkShortScriptName(HeroSiege::Scripts::gml_Script_{name})", self.shipped)
        self.assertIn("kApPreferredName = SdkShortScriptName(HeroSiege::Scripts::gml_Script_GetItemPreferredGrid)", self.shipped)
        self.assertIn("kApPlaceName = SdkShortScriptName(HeroSiege::Scripts::gml_Script_GridAddItem)", self.shipped)
        self.assertIn('kApPreferredGridMember = "grid"', self.shipped)
        move = self.body("static ForgePact::AutoProspectMoveReport ApMoveCell(")
        stack = move.index("if (r.canAdd) {")
        new_type = move.index("} else {", stack)
        preferred = move.index("ApCallScript(kApPreferredName, gridInst, { RValue(1.0), item }, prefRes)")
        place = move.index("ApCallScript(kApPlaceName, gridInst, { placeGrid, item, RValue(0.0), RValue() }, placeRes)")
        self.assertLess(move.index("kApCanAddName"), stack)
        self.assertLess(new_type, preferred)
        self.assertLess(preferred, move.index("r.preferredOk = r.preferredRan && ApPreferredGrid(prefRes, placeGrid);"))
        self.assertLess(move.index("if (!r.preferredOk) { r.heldAfter = ApCellHolds(node, c); return r; }"), place)
        # Each name has exactly one call, inside the new-type branch.
        self.assertEqual(move.count("kApPreferredName"), 1)
        self.assertEqual(move.count("kApPlaceName"), 1)
        self.assertLess(move.index("kApAddName"), new_type)
        # The grid handed on is the struct's `grid` member, an array, read by name.
        grid = self.body("static bool ApPreferredGrid(")
        self.assertIn("ApIsPlainStruct(res)", grid)
        self.assertIn('"variable_struct_get", { res, RValue(kApPreferredGridMember) }', grid)
        self.assertIn("return grid.m_Kind == VALUE_ARRAY;", grid)
        # Still one script_execute site for the grid, and still no address.
        self.assertEqual(self.shipped.count('"script_execute", gridInst, gridInst'), 1)
        # The core: the route is decided before the success rules, and a
        # refused new type is its own outcome, never move-failed or moved.
        classify = strip_comments(self.header[self.header.index("static AutoProspectMoveOutcome ClassifyMove("):])
        classify = classify[:classify.index("\n    }\n")]
        order = [classify.index(s) for s in (
            "if (!r.heldBefore || !r.lookup || !r.canAddRan) return AutoProspectMoveOutcome::MoveFailed;",
            "if (!r.canAdd) {",
            "if (!r.preferredRan) return AutoProspectMoveOutcome::MoveFailed;",
            "if (!r.preferredOk) return AutoProspectMoveOutcome::NoPreferredGrid;",
            "if (r.placeRan && !r.success && r.heldAfter == 1) return AutoProspectMoveOutcome::NotPlaced;",
            "if (r.success) {")]
        self.assertEqual(order, sorted(order))
        report = self.header[self.header.index("AutoProspectMoveOutcome OnMoveReport("):]
        report = report[:report.index("\n    }\n")]
        self.assertIn("if (r.route == AutoProspectMoveRoute::Place) m_MovedNew.fetch_add(1);", report)

    def test_a_preferred_lookup_that_did_not_run_is_move_failed(self):
        # Closing round: `no-preferred-grid` says "the game named no grid", so
        # it is reached only when the lookup ran. A lookup that never ran (the
        # name did not resolve, script_execute failed) is a call that did not
        # run - `move-failed`, as on every other route.
        classify = strip_comments(self.header[self.header.index("static AutoProspectMoveOutcome ClassifyMove("):])
        classify = classify[:classify.index("\n    }\n")]
        not_run = classify.index("if (!r.preferredRan) return AutoProspectMoveOutcome::MoveFailed;")
        self.assertLess(classify.index("if (!r.canAdd) {"), not_run)
        self.assertLess(not_run, classify.index("return AutoProspectMoveOutcome::NoPreferredGrid;"))
        self.assertEqual(classify.count("return AutoProspectMoveOutcome::NoPreferredGrid;"), 1)
        # The adapter reports whether the lookup ran separately from its shape.
        move = self.body("static ForgePact::AutoProspectMoveReport ApMoveCell(")
        self.assertIn("r.preferredRan = ApCallScript(kApPreferredName,", move)
        self.assertIn("r.preferredOk = r.preferredRan && ApPreferredGrid(prefRes, placeGrid);", move)

    def test_move_failed_line_names_the_failed_step_once_per_reason(self):
        # PR prep: seven different failures classify as `move-failed`; the
        # report carries which step failed so the one line names it, and the
        # line is still written once per reason, not once per step.
        header = strip_comments(self.header)
        self.assertIn("enum class AutoProspectMoveStep : int {", header)
        steps = header[header.index("enum class AutoProspectMoveStep : int {"):]
        steps = steps[:steps.index("};")]
        for name in ("CellBefore", "ItemLookup", "HasStackCheck", "PreferredGrid", "Add", "Place", "FinalRead"):
            self.assertIn(name, steps)
        failed = header[header.index("static AutoProspectMoveStep FailedStep("):]
        failed = failed[:failed.index("\n    }\n")]
        self.assertIn("if (ClassifyMove(r) != AutoProspectMoveOutcome::MoveFailed) return AutoProspectMoveStep::None;", failed)
        report = header[header.index("AutoProspectMoveOutcome OnMoveReport("):]
        report = report[:report.index("\n    }\n")]
        # The step is recorded only inside the once-per-reason branch.
        once = report.index("if (!(m_MoveReportedMask & bit)) {")
        self.assertLess(once, report.index("m_MoveFailedStep = FailedStep(r);"))
        self.assertEqual(report.count("m_MoveUnreported.push_back(o);"), 1)
        line = header[header.index("std::string MoveProblemLine("):]
        line = line[:line.index("\n    }\n")]
        self.assertIn("MoveStepText(m_MoveFailedStep)", line)
        # The step text is plain words, no script names.
        text = header[header.index("static const char* MoveStepText("):]
        text = text[:text.index("\n    }\n")]
        for script in ("CanAdd", "GridAdd", "GetItemPreferredGrid", "Clear", "kAp"):
            self.assertNotIn(script, text)

    def test_new_type_clear_only_on_the_recorded_success_signal(self):
        # N-gridadd-return: the place returned `{tabNumber, x, y, tabType,
        # success}`, the add's shape, so the same `success == true` check
        # decides it; the one clear site follows either route's success.
        move = self.body("static ForgePact::AutoProspectMoveReport ApMoveCell(")
        self.assertIn("r.success = r.placeRan && ApAddSucceeded(placeRes);", move)
        self.assertIn("if (r.placeRan) r.route = ForgePact::AutoProspectMoveRoute::Place;", move)
        self.assertEqual(move.count("ApCallScript(kApClearName"), 1)
        clear = move.index("ApCallScript(kApClearName")
        self.assertLess(move.index("r.success = r.placeRan && ApAddSucceeded(placeRes);"), clear)
        self.assertLess(move.index("if (r.success && ApCellHolds(node, c) == 1) {"), clear)
        # No other success reading: not a truthy result, not the call dispatching.
        self.assertNotIn("placeRes.ToBoolean()", move)
        self.assertNotIn("r.success = r.placeRan;", move)
        success = self.body("static bool ApAddSucceeded(")
        self.assertIn('RValue("success")', success)
        self.assertIn("if (!ApIsPlainStruct(res)) return false;", success)

    def test_bag_is_a_sub_option_of_autoprospect(self):
        # On by default in the core, left alone by the parent's toggle.
        self.assertIn("std::atomic<bool> m_BagEnabled{ true };", self.header)
        set_enabled = self.header[self.header.index("void SetEnabled(bool enabled) {"):]
        set_enabled = set_enabled[:set_enabled.index("\n    }\n")]
        self.assertNotIn("m_BagEnabled", set_enabled)
        self.assertNotIn("m_BagOffThisSession", set_enabled)
        # `autoprospect bag 1|0`, a player command under the same verb.
        command = self.body("static void AutoProspectCommand(", strip_research_blocks(self.plugin))
        self.assertIn('if (v.rfind("bag", 0) == 0) {', command)
        self.assertIn("mod.SetBagEnabled(false);", command)
        self.assertIn("if (!mod.SetBagEnabled(true)) {", command)
        self.assertIn("bag unavailable this session", command)
        self.assertLess(command.index('if (v.rfind("bag", 0) == 0) {'),
                        command.index('const bool on = v == "1" || v == "on" || v == "true";'))
        # The panel: nested under the parent like map_reveal_packs, on by
        # default, sent only to turn it off, restated when the parent turns on.
        spec = importlib.util.spec_from_file_location("forgepact_ap_bag_contract", PANEL)
        panel = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(panel)
        import copy
        cfg = copy.deepcopy(panel.DEFAULTS)
        self.assertIs(cfg["mod_auto_prospect_bag"], True)
        self.assertFalse(any(c.startswith("autoprospect") for c in panel.build_cmds(cfg)))
        cfg["mod_auto_prospect"] = True
        self.assertNotIn("autoprospect bag 0", panel.build_cmds(cfg))
        cfg["mod_auto_prospect_bag"] = False
        cmds = panel.build_cmds(cfg)
        self.assertGreater(cmds.index("autoprospect bag 0"), cmds.index("autoprospect 1"))
        cfg["mod_auto_prospect"] = False
        self.assertFalse(any(c.startswith("autoprospect") for c in panel.build_cmds(cfg)))
        source = PANEL.read_text(encoding="utf-8")
        self.assertIn("cmds.append(f\"autoprospect bag {1 if cfg.get('mod_auto_prospect_bag', True) else 0}\")", source)
        self.assertIn('send_cmds([f"autoprospect bag {1 if cfg[\'mod_auto_prospect_bag\'] else 0}"], cfg)', source)
        self.assertIn('id="mod_auto_prospect_bag_row"', source)
        self.assertIn("function syncProspectBag(parentOn,bagOn){", source)
        self.assertIn("mod_auto_prospect_bag:'mod_auto_prospect_bag'", source)
        self.assertIn("apGroup.className='feature-with-child'", source)
        row = source[source.index('id="mod_auto_prospect_bag_row"'):source.index('id="mod_auto_prospect_bag"')]
        self.assertIn("materials tab", row)
        self.assertIn("newest batch stays in the grid", row)

    def test_move_refusals_and_vanished_are_logged_once(self):
        tick = self.body("static void AutoProspectTick(")
        self.assertIn("for (ForgePact::AutoProspectMoveOutcome o = mod.TakeFirstMoveProblem(); "
                      "o != ForgePact::AutoProspectMoveOutcome::None; o = mod.TakeFirstMoveProblem())", collapse(tick))
        self.assertIn("Out(mod.MoveProblemLine(o));", tick)
        self.assertIn("if (mod.TakeFirstMove()) Out(mod.FirstMoveLine());", tick)
        shipped_tick = self.body("static void AutoProspectTick(", strip_research_blocks(self.plugin))
        for line in ("Out(mod.MoveProblemLine(o));", "Out(mod.FirstMoveLine());"):
            self.assertIn(line, shipped_tick)
        for name in ('"no-preferred-grid"', '"not-placed"', '"not-added"', '"move-failed"', '"vanished"', '"cell-kept"'):
            self.assertIn(name, self.header)
        # Stage D retired not-stackable: a has-a-stack "no" is a route now, and
        # an outcome nothing can reach would print 0 forever.
        self.assertNotIn("not-stackable", strip_comments(self.header))
        self.assertNotIn("NotStackable", strip_comments(self.header))
        self.assertIn('HeadedStatLine("first move to bag")', self.header)
        # vanished and cell-kept turn the pass off for the session, and say so.
        report = self.header[self.header.index("AutoProspectMoveOutcome OnMoveReport("):]
        report = report[:report.index("\n    }\n")]
        self.assertIn("if (o == AutoProspectMoveOutcome::Vanished || o == AutoProspectMoveOutcome::CellKept)", report)
        self.assertIn("m_BagOffThisSession.store(true);", report)
        self.assertEqual(self.header.count("off for this session"), 2)
        # The stat line names what the pass did.
        for field in ('" moved="', '" moved-new="', '" no-preferred-grid="', '" not-placed="', '" vanished="',
                      '" cell-kept="', '" bag="', '"off-this-session"'):
            self.assertIn(field, self.header)

    def test_release_notes_and_docs_record_the_bag_move(self):
        notes = NOTES.read_text(encoding="utf-8")
        self.assertIn("materials tab", notes.lower())
        self.assertIn("are gone when you load again", notes)
        self.assertNotIn("were gone when we loaded", notes)
        self.assertIn("could not check", notes)
        self.assertNotIn("take them out", notes.lower())
        self.assertNotIn("stackmove", notes)
        readme = README.read_text(encoding="utf-8")
        section = readme[readme.index("## Auto-prospect"):readme.index("## 🔧")]
        self.assertIn("materials tab", section.lower())
        self.assertIn("autoprospect bag 1|0", section)
        self.assertNotIn("take them out", readme.lower())
        doc = DOC.read_text(encoding="utf-8").replace("\r\n", "\n")
        self.assertIn("\nstage-c-status: complete\n", doc)
        self.assertIn("move-shape: stackmove route (plus success check)", doc)
        self.assertIn("\n## Stage C ship design\n", doc)
        self.assertIn("\n## Stage C Phase 3 results\n", doc)
        self.assertRegex(doc, r"\nphase3c-status: (pending|complete)\n")
        self.assertNotIn("Not built, on purpose:** returning materials", doc)
        # Stage D: the first-of-its-kind route and the ore finding, in player
        # words, with no retired outcome name and no claim of a fix for the ore.
        self.assertIn("first of its kind", notes)
        self.assertIn("in your bag rather than the materials tab", collapse(notes))
        self.assertIn("is the game, not the mod", collapse(notes))
        self.assertNotIn("not-stackable", notes)
        self.assertNotIn("we have not seen that happen yet", notes)
        # No fix is claimed for the ore: the only `## Fixed` entry is the
        # mod loader's, and it says nothing about prospecting.
        fixed = notes[notes.index("## Fixed"):notes.index("## How to update")].lower()
        for word in ("prospect", "ore", "material"):
            self.assertNotRegex(fixed, r"\b%s" % word)
        for name in ("no-preferred-grid", "not-placed", "not-added", "move-failed"):
            self.assertIn(name, section)
        self.assertNotIn("not-stackable", section)
        self.assertIn("\nstage-d-status: complete\n", doc)
        self.assertIn("\n## Stage D ship design\n", doc)
        self.assertRegex(doc, r"\nphase3d-status: (pending|complete)\n")

    def test_move_set_is_the_recorded_batch_not_every_material(self):
        # Round 1: live on e63eed5 an inserted ore - itself a material - was
        # moved back to the tab and never prospected, because the pass named
        # every material cell. The batch is what the core's own invoke
        # produced; the harness pins the behaviour, and this stops a later edit
        # from quietly going back to "every material".
        header = strip_comments(self.header)
        decide = header[header.index("AutoProspectDecision Decide("):header.index("void OnInvoked(")]
        push = decide.index("d.moves.push_back(c);")
        condition = decide[decide.rindex("if (", 0, push):push]
        self.assertIn("c.material", condition)
        self.assertIn("Contains(m_Batch, c.fingerprint)", condition)
        self.assertEqual(decide.count("d.moves.push_back("), 1)
        self.assertNotRegex(decide, r"if \(c\.material\)\s*d\.moves\.push_back")
        # The pass consumes the batch.
        self.assertLess(push, decide.index("m_Batch.clear();", push))
        # Recorded in OnInvoked from `after`, against the invoking view's list.
        invoked = header[header.index("void OnInvoked("):]
        invoked = invoked[:invoked.index("\n    }\n")]
        self.assertIn("for (const std::string& fp : after.printList)", invoked)
        self.assertIn("if (!Contains(m_InvokePrints, fp)) m_Batch.push_back(fp);", invoked)
        self.assertIn("if (dispatched && readable)", invoked)
        self.assertIn("m_InvokePrints = in.printList;", decide)
        # Forgotten on first sight, a removal and a not-landed expiry.
        first_sight = decide[decide.index("if (in.nodeId != m_NodeId || m_Settled < 0) {"):decide.index("if (!m_Pending) {")]
        self.assertIn("m_Batch.clear();", first_sight)
        self.assertIn("if (in.filled < m_Settled) { m_Settled = in.filled; m_Batch.clear(); }", decide)
        expiry = decide[decide.index("if (++m_PendingAge > kAutoProspectLandFrames) {"):]
        expiry = expiry[:expiry.index("return d;")]
        self.assertIn("m_NotLanded.fetch_add(1);", expiry)
        self.assertIn("m_Batch.clear();", expiry)
        # No batch, no item lookups.
        needs = header[header.index("bool NeedsMaterials() const {"):]
        needs = needs[:needs.index("}")]
        self.assertIn("!m_Batch.empty()", needs)
        self.assertIn('" batch="', self.header)
        # The adapter hands the core the distinct fingerprints as a list,
        # filled by the same reader both the invoke frame and `after` use.
        cells = self.body("static bool ApReadCells(")
        self.assertIn("v.printList.clear();", cells)
        self.assertIn("v.printList = seen;", cells)
        self.assertLess(cells.index("v.printList.clear();"), cells.index("v.printList = seen;"))


if __name__ == "__main__":
    unittest.main()
