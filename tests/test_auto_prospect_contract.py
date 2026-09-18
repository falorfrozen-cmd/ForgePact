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
           "static void AutoProspectTick(", "static void AutoProspectInstall(", "static void AutoProspectCommand(")


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
        self.assertIn('"autoprospect: first prospect - "', self.header)
        # A failed dispatch is said once, too.
        self.assertIn("if (!dispatched && !g_AutoProspectDispatchLogged) {", tick)
        # All of it ships: none of these lines sits behind the research guard.
        shipped_tick = self.body("static void AutoProspectTick(", strip_research_blocks(self.plugin))
        for line in ("Out(mod.RefusalLine(r));", "Out(mod.FirstProspectLine());", "did not dispatch"):
            self.assertIn(line, shipped_tick)
        self.assertIn('"holding back - "', self.header)

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
        self.assertIn('send_cmds([f"autoprospect {1 if cfg[\'mod_auto_prospect\'] else 0}"], cfg)', source)
        self.assertIn('id="mod_auto_prospect"', source)
        # The switch's copy: materials stay, take them out, and what is left
        # in the grid when the game saves is lost.
        row = source[source.index('id="mod_auto_prospect"') - 1200:source.index('id="mod_auto_prospect"')]
        self.assertIn("Materials stay in the grid", row)
        self.assertIn("take them out", row)
        self.assertIn("lost", row)

    def test_release_notes_and_docs_record_the_feature(self):
        notes = NOTES.read_text(encoding="utf-8")
        headings = [l for l in notes.split("\n") if l.startswith("## ")]
        self.assertEqual(headings, ["## New", "## How to update"])
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


if __name__ == "__main__":
    unittest.main()
