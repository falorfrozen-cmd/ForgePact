"""Mark 1 drops (ForgePact #166, phase 3): the plugin wiring the harnesses cannot see.

tests/test_mark1_drops_behavior.py runs the drops' decision core
(plugin/include/ForgePact/Mark1Drops.hpp), and tests/test_angelic_hit_behavior.py
runs the drop adapter's own functions against a model of the Heroic roll
(`mark1_heroic_*`). This file reads ModuleMain.cpp as text, comments stripped,
and pins what neither can see:

- heroic_hook: the hook on DropItemHeroic goes in by its SDK name, once,
  through HookOneScript with its own id, and the install refuses unless the
  inline detour went in (the game's compiled calls pass a table-only hook by);
  the hook's every decision is the core's BeginCall, made before anything is
  read, and a call it does not resolve is the game's own, untouched; for one
  it does, the push (outside the roll's scope), #74's roll-in-progress and hit
  reset come before the original, and the entries come off on every way out;
- counters_put_back: #74's counters are put back after the call
  (FlightUniqueCounterScope), so `sigdrop status` reads as before;
- installs_with_a_character: `mark1set drops 1` arms; the install happens only
  once setup holds and the local player resolves, from the switch with a
  character or from the tick beside the set's, never at character select
  (the guide's Known Limitations item 8); the pieces' ground icon goes in
  first, then #74's detection, then the hook;
- research_holder: in the research build the install refuses while
  angelicprobe's drop-heroic row holds the script, and that row refuses while
  the drops hold it, both research-only;
- the call-outs: the only reach into #74's and the Custom Forge's machinery is
  one line each beside the flight charms' (the attribution, the rewrite point,
  the hit reset, the built count), and the rewrite is #74's
  SignatureRewriteParams with the core's RewriteFor;
- `mark1set drops 1|0` and the second stat line are the core's, and the copies
  are the core's constant in the player build.
"""
import re
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from test_release_hook_contract import function_body, strip_comments, strip_research_blocks  # noqa: E402

ROOT = Path(__file__).resolve().parents[1]
PLUGIN = ROOT / "plugin" / "ModuleMain.cpp"
SDK_SCRIPTS = ROOT.parent / "hs-game-sdk" / "cpp" / "include" / "hs_game_sdk" / "scripts.hpp"

BLOCK_START = "// ---- Mark 1 set (Mark1Drops.hpp): the drop adapter"
BLOCK_END = "// ---- end of the Mark 1 drop adapter"

SIGNATURES = {
    "art_ready": "static bool Mark1DropArtReady()",
    "arg_of": "static M1dNs::ArgKind Mark1DropArgOf(int argc, RValue** A, double& value)",
    "read_list": "static bool Mark1DropReadList(int index, RValue& sub, double& id, int& size, std::string& why)",
    "verdict": "static bool Mark1DropVerdict(const M1dNs::Triple& key, int& rarity, bool& hidden)",
    "resolve": "static void Mark1DropResolve(int index, const RValue& sub, double id, int size)",
    "held": "static bool Mark1DropHeldReadBack(std::string& seen)",
    "remove": "static void Mark1DropRemove()",
    "push": "static void Mark1DropPush(int index)",
    "hit_reset": "static void Mark1DropHitReset()",
    "attribute": "static void Mark1DropAttributeHit()",
    "refuse": "static void Mark1DropRefuse(int i, const std::string& record)",
    "before_create": "static bool Mark1DropBeforeCreate(RValue& record, const std::string& missing)",
    "note_built": "static void Mark1DropNoteBuilt(M1sNs::Piece piece)",
    "after_hit": "static void Mark1DropAfterHit()",
    "guard": "struct Mark1DropInjectGuard {",
    "hook": "static RValue& Hook_Mark1DropHeroic(CInstance* S, CInstance* O, RValue& R, int argc, RValue** A)",
    "install": "static std::string Mark1DropInstall()",
    "install_if_ready": "static void Mark1DropInstallIfReady(bool fromTick)",
    "tick": "static void Mark1DropsTick()",
    "switch": "static void Mark1DropsSwitch(bool on)",
    "stat_line": "static std::string Mark1DropStatLine()",
    # research build only
    "research_holder": "static std::string Mark1DropResearchHolder()",
    "holds_heroic": "static bool Mark1DropHoldsHeroic()",
    # outside the block: the call-outs and the holders' other half
    "cdp": "static RValue& Hook_CreateDefaultParams(CInstance* S, CInstance* O, RValue& R, int argc, RValue** A)",
    "sig_before_create": "static void SignatureBeforeCreate(int argc, RValue** A)",
    "sig_hit_reset": "static void SignatureHitReset()",
    "forge": "static bool TryApplyCustomForge(",
    "ap_attach": "static void ApRollAttach(ApRollRow& r)",
    "counter_scope": "struct FlightUniqueCounterScope {",
    "set_command": "static void Mark1SetCommand(const std::string& rest)",
    "frame": "void FrameCallback(FWFrame& FrameContext)",
}
SHIPPED = ("art_ready", "arg_of", "read_list", "verdict", "resolve", "held", "remove", "push", "hit_reset", "attribute",
           "refuse", "before_create", "note_built", "after_hit", "guard", "hook", "install", "install_if_ready", "tick",
           "switch", "stat_line")


class Mark1DropsContract(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.plugin = PLUGIN.read_text(encoding="utf-8").replace("\r\n", "\n")
        start = cls.plugin.find(BLOCK_START)
        end = cls.plugin.find(BLOCK_END)
        if start < 0 or end < start:
            raise AssertionError("the Mark 1 drop adapter's block was not found in ModuleMain.cpp")
        cls.block = cls.plugin[start:end]
        cls.code = strip_comments(cls.block)
        cls.shipped = strip_comments(strip_research_blocks(cls.plugin))
        cls.whole = strip_comments(cls.plugin)

    def body(self, which, source=None):
        signature = SIGNATURES[which]
        text = source if source is not None else self.plugin
        if signature.startswith("struct "):
            start = text.rfind(signature)
            self.assertGreaterEqual(start, 0, signature)
            return strip_comments(text[start:text.index("\n};", start)])
        return strip_comments(function_body(text, signature))

    def lines(self, which):
        return [l.strip() for l in self.body(which).split("\n") if l.strip()]

    # ---- both builds --------------------------------------------------------------------

    def test_the_drop_adapter_ships_in_both_builds(self):
        player = strip_research_blocks(self.plugin)
        for name in SHIPPED:
            with self.subTest(function=name):
                self.assertIn(SIGNATURES[name], player)
        self.assertIn("#include <ForgePact/Mark1Drops.hpp>", self.shipped)
        self.assertIn("namespace M1dNs = ForgePact::Mark1Drops;", self.shipped)
        self.assertIn("static Mark1DropState g_M1Drop;", self.shipped)
        # The block is one bounded region, and the research probe's words stay out of the
        # player build even in its comments (test_mark1_probe_contract.py's rule).
        self.assertEqual(self.plugin.count(BLOCK_START), 1)
        self.assertEqual(self.plugin.count(BLOCK_END), 1)
        shipped_block = strip_research_blocks(self.block)
        self.assertNotIn("mark1probe", shipped_block)
        self.assertNotIn("M1p", shipped_block)

    def test_the_copies_are_the_cores_constant_in_the_player_build(self):
        self.assertIn("#ifdef FORGEPACT_RELEASE\nstatic constexpr int g_Mark1DropCopies = M1dNs::kPlayerCopies;\n#else\n"
                      "static int g_Mark1DropCopies = M1dNs::kPlayerCopies;\n#endif", self.block)
        # The player build never writes it: its one writer is the research probe's copies form.
        self.assertIsNone(re.search(r"(?<!int )\bg_Mark1DropCopies\s*=(?!=)", strip_comments(strip_research_blocks(self.plugin))))
        self.assertIn("d.core.PlanFor(*r, g_Mark1DropCopies)", self.body("push"))

    # ---- the hook -----------------------------------------------------------------------

    def test_the_heroic_hook_goes_in_by_its_sdk_name_once_and_only_detoured(self):
        scripts = SDK_SCRIPTS.read_text(encoding="utf-8")
        self.assertIn('std::string_view gml_Script_DropItemHeroic = "gml_Script_DropItemHeroic";', scripts)
        install = self.body("install")
        self.assertIn('HookOneScript(SdkShortScriptName(HeroSiege::Scripts::gml_Script_DropItemHeroic), "fp_mark1_dheroic",\n'
                      '                                      (PVOID)Hook_Mark1DropHeroic, &g_Orig_Mark1DropHeroic, &native);', install)
        self.assertEqual(self.whole.count("(PVOID)Hook_Mark1DropHeroic"), 1)
        self.assertEqual(self.whole.count('"fp_mark1_dheroic"'), 1)
        self.assertEqual(self.code.count("HookOneScript("), 1, "one install site, in Mark1DropInstall")
        for other in ("HookOneScriptTable", "MmCreateHook", "InstallScriptHook", "HookBuiltin"):
            self.assertNotIn(other, self.code)
        # Tried once; refused unless the inline detour went in, the route named either way.
        self.assertIn("if (!d.hookTried) {", install)
        self.assertIn("d.hookNative = ok && native;", install)
        self.assertIn('d.hookRoute = !ok ? "not-found" : native ? "detoured" : "TABLE-ONLY";', install)
        self.assertIn("if (!d.hookNative)", install)
        self.assertLess(install.index("HookOneScript("), install.index("if (!d.hookNative)"))
        self.assertTrue(install.rstrip().endswith('return "";'))

    def test_the_heroic_hook_decides_through_the_cores_begin_call_before_anything_is_read(self):
        lines = self.lines("hook")
        self.assertEqual(lines[0], "if (!g_Orig_Mark1DropHeroic) return R;")
        self.assertEqual(lines[1], "double value = 0.0;")
        self.assertEqual(lines[2], "const M1dNs::ArgKind kind = Mark1DropArgOf(argc, A, value);")
        self.assertEqual(lines[3], "int index = M1dNs::kListRefused;")
        self.assertEqual(lines[4], "const M1dNs::CallAction action = g_M1Drop.core.BeginCall(g_SigDetectNative, g_SigRollDepth, "
                                   "Mark1DropArtReady(), kind, value, index);")
        self.assertEqual(lines[5], "if (action != M1dNs::CallAction::Resolve) return g_Orig_Mark1DropHeroic(S, O, R, argc, A);")
        # Nothing before the decision reaches the game: the argument's kind is read off the
        # RValue itself, and the art test is the core's on the adapter's own state.
        for line in lines[:5]:
            for call in ("g_Yytk", "CallBuiltin", "CallGameScript", "Push(", "Resolve(", "Install"):
                with self.subTest(line=line, call=call):
                    self.assertNotIn(call, line)
        for call in ("g_Yytk", "CallBuiltin"):
            self.assertNotIn(call, self.body("arg_of"))
            self.assertNotIn(call, self.body("art_ready"))
        self.assertIn("M1dNs::ArtReady(g_Mark1.ArtArmed(), (int)ForgePact::Mark1Icons::kIconCount, g_M1sGroundTried)", self.body("art_ready"))
        self.assertIn("return M1dNs::ArgKind::Absent;", self.body("arg_of"))
        self.assertIn("return M1dNs::ArgKind::Undefined;", self.body("arg_of"))
        # Resolved: the push outside the roll's scope, then #74's roll-in-progress, the counter
        # scope and the hit reset, then the original; the guard takes the entries off on every way out.
        hook = self.body("hook")
        order = ["Mark1DropInjectGuard inject(index);", "SignatureRollScope roll;", "FlightUniqueCounterScope counters;",
                 "SignatureHitReset();", "RValue& r = g_Orig_Mark1DropHeroic(S, O, R, argc, A);", "Mark1DropAfterHit();"]
        for a, b in zip(order, order[1:]):
            self.assertLess(hook.index(a), hook.index(b), a + " before " + b)
        self.assertEqual(hook.count("g_Orig_Mark1DropHeroic(S, O, R, argc, A)"), 2)
        guard = self.body("guard")
        self.assertRegex(guard, r"Mark1DropInjectGuard\(int index\)\s*\{\s*g_M1Drop\.rolling = true;\s*try \{ Mark1DropPush\(index\); \} catch \(\.\.\.\) \{\}")
        self.assertRegex(guard, r"~Mark1DropInjectGuard\(\)\s*\{\s*try \{ Mark1DropRemove\(\); \} catch \(\.\.\.\) \{\}\s*g_M1Drop\.rolling = false;")

    def test_the_heroic_hook_counters_put_back_as_the_unique_rolls(self):
        self.assertIn("FlightUniqueCounterScope counters;", self.body("hook"))
        scope = self.body("counter_scope")
        for field in ("g_SigUntyped = untyped;", "g_SigStandinPicks = picks;", "g_SigTypeAgree = agree;", "g_SigTypeDisagree = disagree;"):
            self.assertIn(field, scope)
        # The drops never write #74's or the flight charms' counters themselves.
        for counter in ("g_SigUntyped", "g_SigInjected", "g_SigOurHits", "g_SigBuilt", "g_SigRefusals", "g_SigAnomalies",
                        "g_FlightDrop."):
            self.assertNotIn(counter, self.code)

    def test_the_push_reads_outside_the_roll_resolves_by_the_core_and_holds_read_back(self):
        resolve = self.body("resolve")
        self.assertIn("FlightDropOutsideRoll outside;", resolve)
        self.assertIn("M1dNs::ResolveStandIns(index, Mark1DropEntries(sub, size), Mark1DropVerdict)", resolve)
        self.assertIn("g_M1Drop.core.Cache().Store((uint64_t)id, r);", resolve)
        push = self.body("push")
        self.assertIn("if (d.core.Cache().Stale(index, (uint64_t)id, size)) Mark1DropResolve(index, sub, id, size);", push)
        self.assertIn('g_Yytk->CallBuiltin("ds_list_add", { d.sub, entry });', push)
        self.assertLess(push.index('"ds_list_add"'), push.index("Mark1DropHeldReadBack(seen)"))
        self.assertLess(push.index("Mark1DropHeldReadBack(seen)"), push.index("d.core.NotePushed(d.plan);"))
        self.assertIn("d.core.NoteListUnreadable(index);", push)
        # The verdict is the repository's by name, the reads FlightDropUniquePasses makes.
        verdict = self.body("verdict")
        self.assertIn("g_Yytk->CallGameScript(HeroSiege::Scripts::gml_Script_GetUniqueRepoStruct,", verdict)
        self.assertIn('TryStructNumber(info, "27", r)', verdict)
        self.assertIn('RValue("40")', verdict)
        # The list by name off the first Controller_obj, a live ds_list or nothing.
        read = self.body("read_list")
        self.assertIn("SignatureController(instance, why)", read)
        self.assertIn("SigListHandle(sub, id, step)", read)
        # Removal is tail-checked and counts an anomaly otherwise.
        remove = self.body("remove")
        self.assertIn("tail = Mark1DropTailHolds(len);", remove)
        self.assertLess(remove.index("Mark1DropTailHolds(len)"), remove.index('"ds_list_delete"'))
        self.assertIn("d.core.NoteAnomaly();", remove)
        # The only game writes in the block are the push and its removal.
        for name in ("variable_instance_set", "variable_struct_set", "variable_global_set", "ds_list_set", "instance_create",
                     "instance_destroy"):
            self.assertNotIn(name, self.code)
        self.assertEqual(self.code.count('"ds_list_add"'), 1)
        self.assertEqual(self.code.count('"ds_list_delete"'), 1)

    # ---- the hit, the rewrite and the built count ------------------------------------------

    def test_the_call_outs_sit_beside_the_flight_charms(self):
        cdp = self.body("cdp")
        self.assertIn("if (g_SigRollDepth > 0 && g_SigHitTyped) {\n        try { Mark1DropAttributeHit(); } catch (...) {}", cdp)
        self.assertLess(cdp.index("FlightDropAttributeHit();"), cdp.index("Mark1DropAttributeHit();"))
        before = self.body("sig_before_create")
        self.assertIn("if (which < 0 && Mark1DropBeforeCreate(record, missing)) return;", before)
        self.assertLess(before.index("FlightDropBeforeCreate(record, missing)"), before.index("Mark1DropBeforeCreate(record, missing)"))
        self.assertLess(before.index("Mark1DropBeforeCreate(record, missing)"), before.index("if (which < 0) return;"))
        reset = self.lines("sig_hit_reset")
        self.assertEqual(reset[-2:], ["FlightDropHitReset();", "Mark1DropHitReset();"])
        forge = self.body("forge")
        self.assertIn("if (finalPass && mark1Matched) Mark1DropNoteBuilt(mark1Piece);", forge)
        self.assertLess(forge.index("FlightDropNoteBuilt(flightRolled.charm);"), forge.index("Mark1DropNoteBuilt(mark1Piece);"))
        # Each call-out appears once (forward declarations aside), outside the block.
        for call in ("Mark1DropAttributeHit();", "Mark1DropBeforeCreate(record, missing)", "Mark1DropHitReset();",
                     "Mark1DropNoteBuilt(mark1Piece);"):
            self.assertEqual(len(re.findall(r"(?<!void )(?<!bool )\b" + re.escape(call), self.whole)), 1, call)
            self.assertNotIn(call, self.code)

    def test_a_hit_is_the_cores_attribution_and_the_rewrite_74s(self):
        attribute = self.body("attribute")
        self.assertIn("if (!d.rolling || d.pushed <= 0 || g_SigRollDepth <= 0 || !g_SigHitTyped || g_SigHitItem >= 0 || d.hitStandIn >= 0) return;",
                      attribute)
        self.assertIn("M1dNs::AttributeHit(d.plan, g_SigHitType, g_SigLastSub, g_SigLastB, u, piece)", attribute)
        self.assertIn("d.core.NoteOurs(piece);", attribute)
        before = self.body("before_create")
        self.assertIn("M1dNs::RewriteFor(static_cast<M1sNs::Piece>(i))", before)
        self.assertIn("SignatureRewriteParams(record, item, why)", before)
        self.assertIn("Mark1DropRefuse(i,", before)
        self.assertIn("d.core.NoteRewriteRefused(p);", self.body("refuse"))
        built = self.body("note_built")
        self.assertIn("g_M1Drop.pending[i] <= 0) return;", built)
        self.assertIn("g_M1Drop.core.NoteBuilt(piece);", built)
        # A hit that never reached the rewrite point is a refusal too.
        after = self.body("after_hit")
        self.assertIn("if (!g_SigHitAtPoint) {", after)
        self.assertLess(after.index("if (!g_SigHitAtPoint) {"), after.index("Mark1DropRefuse(i,"))

    # ---- the install --------------------------------------------------------------------

    def test_installs_with_a_character_never_at_character_select(self):
        ready = self.body("install_if_ready")
        self.assertIn("const bool havePlayer = d.core.ShouldInstall(g_Setup, true, fromTick) && Mark1PlayerResolves();", ready)
        self.assertIn("if (!d.core.ShouldInstall(g_Setup, havePlayer, fromTick)) return;", ready)
        self.assertLess(ready.index("if (!d.core.ShouldInstall(g_Setup, havePlayer, fromTick)) return;"), ready.index("Mark1DropInstall()"))
        self.assertIn("d.core.NoteInstall(why.empty(), fromTick);", ready)
        # The tick returns at once unless armed and not installed, then polls.
        tick = self.lines("tick")
        self.assertEqual(tick[1], "if (!d.core.TickNeeded()) return;")
        self.assertEqual(tick[2], "if (M1dNs::Drops::LooksForPlayer(d.armedFrames++)) Mark1DropInstallIfReady(true);")
        frame = strip_comments(function_body(self.plugin, SIGNATURES["frame"]))
        self.assertIn("if (g_Setup) Mark1DropsTick();", frame)
        self.assertEqual(frame.count("Mark1DropsTick()"), 1)
        # The switch: the core's SetSwitch (which arms the art), then the install only on a switch-on.
        switch = self.lines("switch")
        self.assertEqual(switch, [
            "M1dNs::SetSwitch(g_M1Drop.core, g_Mark1, on);",
            "if (on && !g_M1Drop.core.Installed()) Mark1DropInstallIfReady(false);",
            "Out(M1dNs::DropsReply(g_M1Drop.core, g_SigDetectNative));",
        ])
        # The one install path: the pieces' ground icon first, then #74's detection, then the hook.
        install = self.body("install")
        self.assertLess(install.index("Mark1GroundIconInstall();"), install.index("InstallSignatureAngelicHooks();"))
        self.assertLess(install.index("InstallSignatureAngelicHooks();"), install.index("HookOneScript("))
        callers = re.findall(r"\bMark1DropInstall\(\)", self.whole)
        self.assertEqual(len(callers), 2)   # the definition and Mark1DropInstallIfReady's call
        self.assertEqual(len(re.findall(r"\bMark1DropInstallIfReady\(", self.whole)), 3)   # definition, the tick, the switch

    def test_research_holder_refuses_both_ways_in_the_research_build_only(self):
        holder = self.body("research_holder")
        self.assertIn('std::string_view(r.id) == "drop-heroic" && r.tramp != nullptr', holder)
        install = self.body("install")
        self.assertIn("const std::string holder = Mark1DropResearchHolder();", install)
        self.assertLess(install.index("Mark1DropResearchHolder()"), install.index("HookOneScript("))
        self.assertIn("return g_Orig_Mark1DropHeroic != nullptr;", self.body("holds_heroic"))
        attach = self.body("ap_attach")
        refusal = attach.index('std::string_view(r.id) == "drop-heroic" && Mark1DropHoldsHeroic()')
        self.assertLess(refusal, attach.index("GetNamedRoutinePointer"))
        self.assertIn("kApRollBlocked", attach[refusal:attach.index("GetNamedRoutinePointer")])
        # Both halves are research-only.
        for name in ("Mark1DropResearchHolder", "Mark1DropHoldsHeroic", "g_ApRollRows"):
            self.assertNotIn(name, self.shipped)

    # ---- the command and the stat line ---------------------------------------------------

    def test_the_drops_form_and_the_second_stat_line_are_the_cores(self):
        command = self.body("set_command")
        self.assertIn("if (c.form == M1sNs::CommandForm::Drops) { Mark1DropsSwitch(c.on); return; }", command)
        self.assertLess(command.index("M1sNs::ParseCommand(args, c, why)"), command.index("M1sNs::CommandForm::Drops"))
        self.assertLess(command.index("M1sNs::CommandForm::Drops"), command.index("M1sNs::CommandForm::Bonus"))
        self.assertLess(command.index("Out(M1sNs::StatLine(Mark1SetStatFields()));"), command.index("Out(Mark1DropStatLine());"))
        stat = self.lines("stat_line")
        self.assertEqual(stat, [
            "M1dNs::DropStatFields f;",
            "g_M1Drop.core.FillStat(f, g_SigDetectNative);",
            "f.hook = g_M1Drop.hookRoute;",
            "return M1dNs::DropStatLine(f);",
        ])
        self.assertIn("Out(M1dNs::DropsReply(g_M1Drop.core, g_SigDetectNative));", self.body("switch"))

    # ---- game names -----------------------------------------------------------------------

    def test_game_names_go_through_the_sdk_and_no_address_reaches_the_block(self):
        self.assertNotIn('"gml_Script_', self.code)
        self.assertNotIn('"DropItemHeroic"', self.code)
        self.assertNotIn('"GetUniqueRepoStruct"', self.code)
        self.assertIsNone(re.search(r"\b\w*Rva\w*\b", self.code))
        self.assertIsNone(re.search(r"\b0x[0-9A-Fa-f]{5,}\b", self.code))
        self.assertIsNone(re.search(r"\(\s*char\s*\*\s*\)\s*\w+\s*[+-]", self.code))
        self.assertNotIn("GetModuleHandle", self.code)
        self.assertNotIn("reinterpret_cast<PFUNC", self.code)


if __name__ == "__main__":
    unittest.main()
