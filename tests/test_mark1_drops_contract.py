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
  are the core's constant in the player build;
- kill_hook (the kill route, Amendment 3): the pieces' share of the ordinary
  unique roll goes through one detour on DropUniqueItems by its SDK name, the
  one Pocket Elf holds, and the drops' install refuses whole unless it went in
  inline; its every call is counted and the core's BeginKillCall decides
  Mark 1's part before anything is read; the push (outside the roll's scope,
  after Pocket Elf's) and the removal (before Pocket Elf's) wrap the original;
- shares_the_elf: Pocket Elf and the Mark 1 drops reach the same detour and the
  same install function (FlightDropInstallUnique), which hooks the script once
  a session; neither installs a second, and the research rows that hold
  DropUniqueItems refuse the drops and are refused by them, as for Pocket Elf.
- draw_hook (the kill draw, DropItem's own unique draw, #166): DropItem's one
  detour is DropManager's, installed alone (InstallDropItemHook) with the
  draw's call-out pair spliced into its saved original over the trampoline
  only; LoadSatanicDropTier's is raredrop's one shared install; the install
  refuses whole unless both are inline, from the drops' install and
  `flightcharm elf 1`; each original call is begun and ended on every way
  out, counted before anything is read; #74's Angelic roll, the shared
  DropUniqueItems detour and the Heroic hook each set the draw's entries aside
  first; the tier, read, pick, rewrite and built call-outs sit in the existing
  hooks; the push is the cores' plan on all six lists, held read-back and
  tail-checked; the research instruments that hold DropItem's slot refuse the
  draw and are refused by it, research-only.
"""
import re
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from test_release_hook_contract import function_body, strip_comments, strip_research_blocks  # noqa: E402

ROOT = Path(__file__).resolve().parents[1]
PLUGIN = ROOT / "plugin" / "ModuleMain.cpp"
DROP_MANAGER = ROOT / "plugin" / "include" / "ForgePact" / "DropManager.hpp"
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
    "tail": "static bool Mark1ListTailHolds(const RValue& sub, int before, const std::vector<M1dNs::PushEntry>& entries,",
    "add_entry": "static void Mark1DropAddEntry(const RValue& sub, const M1dNs::Triple& k)",
    "delete_last": "static int Mark1DropDeleteLast(const RValue& sub, int len, int count)",
    "remove": "static void Mark1DropRemove()",
    "push": "static void Mark1DropPush(int index)",
    # the kill route
    "kill_read": "static bool Mark1KillReadLists(RValue subs[], M1dNs::KillListKeys& keys, std::string& why)",
    "kill_resolve": "static void Mark1KillResolve(const RValue subs[], const M1dNs::KillListKeys& keys)",
    "kill_held": "static bool Mark1KillHeldReadBack(std::string& seen)",
    "kill_remove": "static void Mark1KillRemove()",
    "kill_push": "static void Mark1KillPush()",
    "kill_begin": "static bool Mark1KillBeginCall(int uniqueDepth)",
    "kill_enter": "static void Mark1KillEnter()",
    "kill_leave": "static void Mark1KillLeave()",
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
    "kill_research_holder": "static std::string Mark1KillResearchHolder()",
    # the shared DropUniqueItems detour, in the flight charms' drop adapter
    "shared_hook": "static RValue& Hook_FlightDropUniqueItems(CInstance* S, CInstance* O, RValue& R, int argc, RValue** A)",
    "shared_install": "static void FlightDropInstallUnique()",
    "shared_route": "static std::string FlightDropUniqueRoute()",
    "kill_guard": "struct Mark1KillInjectGuard {",
    "elf_guard": "struct FlightUniqueInjectGuard {",
    "flight_switch": "static void FlightDropSwitch(FcNs::Charm c, bool on)",
    "gp_install": "static void GpInstallScripts(HMODULE mainMod)",
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
SHIPPED = ("art_ready", "arg_of", "read_list", "verdict", "resolve", "held", "tail", "add_entry", "delete_last", "remove",
           "push", "kill_read", "kill_resolve", "kill_held", "kill_remove", "kill_push", "kill_begin", "kill_enter",
           "kill_leave", "hit_reset", "attribute", "refuse", "before_create", "note_built", "after_hit", "guard", "hook",
           "install", "install_if_ready", "tick", "switch", "stat_line", "shared_hook", "shared_install", "shared_route",
           "kill_guard")


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
        # The same lever on the kill route.
        self.assertIn("d.core.PlanKillFor(*r, g_Mark1DropCopies)", self.body("kill_push"))

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
        # #166's kill draw: inside a DropItem call the draw's entries come off for this roll, on every
        # way out (test_draw_hook_nested_rolls_see_the_lists_as_before).
        self.assertEqual(lines[1], "KillDrawRollAside drawAside;")
        self.assertEqual(lines[2], "double value = 0.0;")
        self.assertEqual(lines[3], "const M1dNs::ArgKind kind = Mark1DropArgOf(argc, A, value);")
        self.assertEqual(lines[4], "int index = M1dNs::kListRefused;")
        self.assertEqual(lines[5], "const M1dNs::CallAction action = g_M1Drop.core.BeginCall(g_SigDetectNative, g_SigRollDepth, "
                                   "Mark1DropArtReady(), kind, value, index);")
        self.assertEqual(lines[6], "if (action != M1dNs::CallAction::Resolve) return g_Orig_Mark1DropHeroic(S, O, R, argc, A);")
        # Nothing before the decision reaches the game: the argument's kind is read off the
        # RValue itself, and the art test is the core's on the adapter's own state.
        for line in lines[2:6]:
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
        self.assertIn("Mark1DropAddEntry(d.sub, p.triple);", push)
        self.assertIn('g_Yytk->CallBuiltin("ds_list_add", { sub, entry });', self.body("add_entry"))
        self.assertLess(push.index("Mark1DropAddEntry("), push.index("Mark1DropHeldReadBack(seen)"))
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
        self.assertLess(remove.index("Mark1DropTailHolds(len)"), remove.index("Mark1DropDeleteLast("))
        self.assertIn("d.core.NoteAnomaly();", remove)
        self.assertIn('g_Yytk->CallBuiltin("ds_list_delete", { sub, RValue((double)(len - 1 - n)) });', self.body("delete_last"))
        # The only game writes in the block are the push and its removal, one helper each, for
        # both rolls.
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
        self.assertIn("if (g_SigRollDepth <= 0 || !g_SigHitTyped || g_SigHitItem >= 0 || d.hitStandIn >= 0) return;", attribute)
        # The call in progress with entries of ours: the Heroic roll's, else the kill route's.
        self.assertIn("const bool heroic = d.rolling && d.pushed > 0;", attribute)
        self.assertIn("const bool kill = !heroic && d.killRolling && d.killPushed > 0;", attribute)
        self.assertIn("if (!heroic && !kill) return;", attribute)
        self.assertIn("M1dNs::AttributeHit(d.plan, g_SigHitType, g_SigLastSub, g_SigLastB, u, piece)", attribute)
        self.assertIn("M1dNs::AttributeHit(d.killPlan, g_SigHitType, g_SigLastSub, g_SigLastB, u, piece)", attribute)
        self.assertIn("d.hitRoll = heroic ? M1dNs::Roll::Heroic : M1dNs::Roll::Kill;", attribute)
        self.assertIn("d.core.NoteOurs(piece, d.hitRoll);", attribute)
        before = self.body("before_create")
        self.assertIn("M1dNs::RewriteFor(static_cast<M1sNs::Piece>(i))", before)
        self.assertIn("SignatureRewriteParams(record, item, why)", before)
        self.assertIn("Mark1DropRefuse(i,", before)
        self.assertIn("d.core.NoteRewriteRefused(p);", self.body("refuse"))
        built = self.body("note_built")
        self.assertIn("g_M1Drop.pending[i] <= 0) return;", built)
        self.assertIn("g_M1Drop.core.NoteBuilt(piece, g_M1Drop.hitRoll);", built)
        self.assertIn("d.hitRoll = M1dNs::Roll::Heroic;", self.body("hit_reset"))
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
        for name in ("Mark1DropResearchHolder", "Mark1DropHoldsHeroic", "g_ApRollRows", "Mark1KillResearchHolder"):
            self.assertNotIn(name, self.shipped)

    # ---- the kill route: the shared DropUniqueItems detour ----------------------------------

    def test_the_kill_hook_is_one_detour_on_drop_unique_items_by_its_sdk_name_refused_unless_inline(self):
        scripts = SDK_SCRIPTS.read_text(encoding="utf-8")
        self.assertIn('std::string_view gml_Script_DropUniqueItems = "gml_Script_DropUniqueItems";', scripts)
        shared = self.body("shared_install")
        self.assertRegex(shared, r"HookOneScript\(SdkShortScriptName\(HeroSiege::Scripts::gml_Script_DropUniqueItems\),\s*"
                                 r"\"fp_flight_duniq\",\s*\(PVOID\)Hook_FlightDropUniqueItems,\s*&g_Orig_FlightDropUnique,\s*&native\)")
        self.assertIn("d.uniqueNative = ok && native;", shared)
        self.assertIn('d.uniqueRoute = !ok ? "not-found" : native ? "detoured" : "TABLE-ONLY";', shared)
        self.assertIn("return g_FlightDrop.uniqueRoute;", self.body("shared_route"))
        # The drops' install adds it after the Heroic hook and refuses whole unless it is detoured,
        # with its own line; the route is the stat line's kill-hook=.
        install = self.body("install")
        self.assertLess(install.index("HookOneScript("), install.index("FlightDropInstallUnique();"))
        self.assertIn("d.killRoute = FlightDropUniqueRoute();", install)
        self.assertLess(install.index("FlightDropInstallUnique();"), install.index("d.killRoute = FlightDropUniqueRoute();"))
        self.assertIn('if (d.killRoute != "detoured")', install)
        self.assertIn('"DropUniqueItems went in " + d.killRoute + " (no inline detour)', install)
        self.assertLess(install.index('if (d.killRoute != "detoured")'), install.rindex('return "";'))
        self.assertTrue(install.rstrip().endswith('return "";'))
        # The detour: every call counted for the kill route first (the core's BeginKillCall, nothing
        # read before it), each feature's part only with its own switch, the game's own call with
        # neither; then the pushes (Pocket Elf's, then Mark 1's) outside the roll's scope, #74's roll
        # in progress, the counter scope and the hit reset, the original, each feature's after-hit.
        lines = [l.strip() for l in self.body("shared_hook").split("\n") if l.strip()]
        self.assertEqual(lines[0], "if (!g_Orig_FlightDropUnique) return R;")
        # #166's kill draw: inside a DropItem call the draw's entries come off for this roll, whether
        # or not either feature takes it (test_draw_hook_nested_rolls_see_the_lists_as_before).
        self.assertEqual(lines[1], "KillDrawRollAside drawAside;")
        self.assertEqual(lines[2], "const bool mark1 = Mark1KillBeginCall(g_FlightDrop.uniqueDepth);")
        self.assertEqual(lines[3], "const bool elf = g_SigRollDepth <= 0 && g_FlightDrop.uniqueDepth <= 0 && "
                                   "g_FlightDrop.switches[(int)FcNs::Charm::Elf];")
        self.assertEqual(lines[4], "if (!elf && !mark1) return g_Orig_FlightDropUnique(S, O, R, argc, A);")
        hook = self.body("shared_hook")
        order = ["FlightUniqueDepthScope depth;", "FlightUniqueInjectGuard inject(elf);", "Mark1KillInjectGuard mark1Inject(mark1);",
                 "SignatureRollScope roll;", "FlightUniqueCounterScope counters;", "SignatureHitReset();",
                 "RValue& r = g_Orig_FlightDropUnique(S, O, R, argc, A);", "FlightDropAfterHit();", "Mark1DropAfterHit();"]
        for a, b in zip(order, order[1:]):
            self.assertLess(hook.index(a), hook.index(b), a + " before " + b)
        begin = self.body("kill_begin")
        self.assertIn("g_M1Drop.core.BeginKillCall(g_SigDetectNative, g_SigRollDepth, uniqueDepth, Mark1DropArtReady())", begin)
        self.assertIn("== M1dNs::CallAction::Resolve", begin)
        for call in ("g_Yytk", "CallBuiltin", "CallGameScript"):
            self.assertNotIn(call, begin)
        # Mark 1's guard is its push and its removal on every way out, only with its part of the call.
        guard = self.body("kill_guard")
        self.assertRegex(guard, r"Mark1KillInjectGuard\(bool on\) : on\(on\) \{ if \(on\) Mark1KillEnter\(\); \}")
        self.assertRegex(guard, r"~Mark1KillInjectGuard\(\) \{ if \(on\) Mark1KillLeave\(\); \}")
        self.assertIn("try { Mark1KillPush(); } catch (...) {}", self.body("kill_enter"))
        self.assertIn("try { Mark1KillRemove(); } catch (...) {}", self.body("kill_leave"))
        self.assertIn("g_M1Drop.killRolling = false;", self.body("kill_leave"))

    def test_the_kill_push_is_the_cores_plan_on_all_five_lists_held_read_back_and_tail_checked(self):
        push = self.body("kill_push")
        self.assertIn("if (!Mark1KillReadLists(subs, keys, why)) {", push)
        self.assertIn("d.core.NoteKillListsUnreadable();", push)
        self.assertIn("if (d.core.KillCache().Stale(keys)) Mark1KillResolve(subs, keys);", push)
        self.assertIn("const int index = M1dNs::kKillLists[k];", push)
        self.assertIn("Mark1DropAddEntry(d.killSub[k], plan.entries[e].triple);", push)
        self.assertLess(push.index("Mark1DropAddEntry("), push.index("Mark1KillHeldReadBack(seen)"))
        self.assertLess(push.index("Mark1KillHeldReadBack(seen)"), push.index("d.core.NoteKillPushed(d.killPlan);"))
        resolve = self.body("kill_resolve")
        self.assertIn("FlightDropOutsideRoll outside;", resolve)
        self.assertIn("M1dNs::ResolveKillStandIns(lists, Mark1DropVerdict)", resolve)
        self.assertIn("g_M1Drop.core.KillCache().Store(keys,", resolve)
        read = self.body("kill_read")
        self.assertIn("SignatureController(instance, why)", read)
        self.assertIn("SigListHandle(subs[k], id, step)", read)
        held = self.body("kill_held")
        self.assertIn("Mark1KillReadLists(subs, keys, why)", held)
        self.assertIn("Mark1KillTailHolds(k, len)", held)
        remove = self.body("kill_remove")
        self.assertLess(remove.index("Mark1KillTailHolds(k, len)"), remove.index("Mark1DropDeleteLast("))
        self.assertIn("d.core.NoteAnomaly();", remove)

    def test_pocket_elf_and_the_drops_shares_the_elfs_detour_and_install(self):
        # One HookOneScript names DropUniqueItems in the whole plugin, in the one install function;
        # the drops' block names neither the hook function's address nor a second install.
        self.assertEqual(self.whole.count("HookOneScript(SdkShortScriptName(HeroSiege::Scripts::gml_Script_DropUniqueItems)"), 1)
        self.assertEqual(self.whole.count("(PVOID)Hook_FlightDropUniqueItems"), 1)
        self.assertEqual(self.whole.count('"fp_flight_duniq"'), 1)
        self.assertNotIn("gml_Script_DropUniqueItems", self.code)
        self.assertNotIn("Hook_FlightDropUniqueItems", self.code)
        # Its callers: the definition, Pocket Elf's switch-on, the drops' install, and the research
        # build's `mark1probe draws` install (#166 uniques), which ships in no player build.
        self.assertEqual(len(re.findall(r"\bFlightDropInstallUnique\(\)", self.whole)), 4)
        self.assertIn("FlightDropInstallUnique();", self.body("flight_switch"))
        self.assertIn("FlightDropInstallUnique();", self.body("install"))
        self.assertIn("FlightDropInstallUnique();", strip_comments(function_body(self.plugin, "static void M1pDrawsInstall()")))
        self.assertNotIn("static void M1pDrawsInstall()", strip_research_blocks(self.whole))
        self.assertEqual(len(re.findall(r"\bFlightDropInstallUnique\(\)", strip_comments(strip_research_blocks(self.whole)))), 3)
        # Once a session, whichever comes first; Pocket Elf's refusal is printed only with its switch on.
        shared = self.body("shared_install")
        self.assertIn("const bool elf = d.switches[(int)FcNs::Charm::Elf];", shared)
        self.assertIn("if (!d.uniqueTried) {", shared)
        self.assertIn("if (elf && !d.uniqueNative)", shared)
        # Pocket Elf's guard pushes only with its part of the call.
        self.assertRegex(self.body("elf_guard"), r"FlightUniqueInjectGuard\(bool on\) : on\(on\) \{ if \(!on\) return;")
        # The research rows that hold DropUniqueItems refuse the drops (before anything is hooked),
        # and are refused once the shared detour is in, both research-only.
        holder = self.body("kill_research_holder")
        self.assertIn("ApRollHoldsDropUnique()", holder)
        self.assertIn("GpHoldsDropUnique()", holder)
        install = self.body("install")
        self.assertIn("const std::string holder = Mark1KillResearchHolder();", install)
        self.assertLess(install.index("Mark1KillResearchHolder()"), install.index("Mark1GroundIconInstall();"))
        attach = self.body("ap_attach")
        refusal = attach.index('std::string_view(r.id) == "drop-unique" && g_Orig_FlightDropUnique')
        self.assertLess(refusal, attach.index("GetNamedRoutinePointer"))
        self.assertIn("kApRollBlocked", attach[refusal:attach.index("GetNamedRoutinePointer")])
        self.assertIn("&t == &g_GpScriptRows[kGpScript_DropUniqueItems] && g_Orig_FlightDropUnique", self.body("gp_install"))
        self.assertNotIn("Mark1KillResearchHolder", self.shipped)

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
            "f.killHook = g_M1Drop.killRoute;",
            "f.drawHook = KillDrawRoute();",
            "f.tierHook = g_SatTierRoute;",
            "return M1dNs::DropStatLine(f);",
        ])
        self.assertIn("Out(M1dNs::DropsReply(g_M1Drop.core, g_SigDetectNative));", self.body("switch"))

    # ---- game names -----------------------------------------------------------------------

    def test_game_names_go_through_the_sdk_and_no_address_reaches_the_block(self):
        self.assertNotIn('"gml_Script_', self.code)
        self.assertNotIn('"DropItemHeroic"', self.code)
        self.assertNotIn('"DropUniqueItems"', self.code)
        self.assertNotIn('"GetUniqueRepoStruct"', self.code)
        self.assertIsNone(re.search(r"\b\w*Rva\w*\b", self.code))
        self.assertIsNone(re.search(r"\b0x[0-9A-Fa-f]{5,}\b", self.code))
        self.assertIsNone(re.search(r"\(\s*char\s*\*\s*\)\s*\w+\s*[+-]", self.code))
        self.assertNotIn("GetModuleHandle", self.code)
        self.assertNotIn("reinterpret_cast<PFUNC", self.code)

    # ---- the kill draw (#166): DropItem's own unique draw --------------------------------

    def draw(self, signature):
        return strip_comments(function_body(self.plugin, signature))

    def test_draw_hook_is_dropmanagers_one_detour_and_raredrops_one_tier_detour(self):
        # DropItem's one detour is DropManager's Hook_DropItem (by name, `bp_ditem`), installed alone by
        # InstallDropItemHook, which InstallHooks() goes through too; the plugin never hooks DropItem
        # itself. LoadSatanicDropTier's one detour is raredrop's, through the one shared install.
        manager = DROP_MANAGER.read_text(encoding="utf-8").replace("\r\n", "\n")
        self.assertEqual(manager.count('HookOneScript("DropItem",'), 1)
        alone = strip_comments(function_body(manager, "std::string InstallDropItemHook()"))
        self.assertIn("if (!m_DropItemTried) {", alone)
        self.assertIn('HookOneScript("DropItem", "bp_ditem", (void*)Hook_DropItem, &m_Orig_DropItem, &native);', alone)
        self.assertIn('m_DropItemRoute = !ok ? "not-found" : native ? "detoured" : "TABLE-ONLY";', alone)
        self.assertIn("InstallDropItemHook();", strip_comments(function_body(manager, "void InstallHooks()")))
        self.assertIn("FP_DROP_HOOK(DropItem)", manager)
        self.assertNotIn("HookOneScript(SdkShortScriptName(HeroSiege::Scripts::gml_Script_DropItem)", self.whole)
        self.assertNotIn('HookOneScript("DropItem"', self.whole)
        self.assertEqual(self.whole.count("HookOneScript(SdkShortScriptName(HeroSiege::Scripts::gml_Script_LoadSatanicDropTier)"), 1)
        install = self.draw("static std::string KillDrawInstall()")
        self.assertIn("d.route = ForgePact::DropManager::Instance().InstallDropItemHook();", install)
        self.assertIn("PFUNC_YYGMLScript* slot = ForgePact::DropManager::Instance().DropItemOriginal();", install)
        # The call-out pair goes into the saved original only over the trampoline (the route read
        # back detoured), once (KillDrawInstall runs its install only while nothing is spliced).
        self.assertIn('if (d.route == "detoured" && slot && *slot) {', install)
        self.assertIn("d.orig = *slot;", install)
        self.assertIn("*slot = KillDrawDropItem;", install)
        self.assertLess(install.index("if (!d.orig) {"), install.index("InstallDropItemHook()"))
        self.assertIn("InstallSatanicTierHook();", install)
        # Refused whole unless both are inline, each with its reason; "" only at the end.
        for why in ('"DropItem was not found by name', '"DropItem went in " + d.route + " (no inline detour)',
                    '"LoadSatanicDropTier was not found by name', '"LoadSatanicDropTier went in " + g_SatTierRoute + " (no inline detour)'):
            self.assertIn(why, install)
        self.assertTrue(install.rstrip().endswith('return "";'))
        # Its two callers: the drops' install (refusing whole with its reason) and `flightcharm elf 1`.
        self.assertEqual(len(re.findall(r"\bKillDrawInstall\(\)", self.whole)), 4)   # declaration, definition, two callers
        drops = self.body("install")
        self.assertIn("const std::string drawWhy = KillDrawInstall();", drops)
        self.assertLess(drops.index("FlightDropInstallUnique();"), drops.index("KillDrawInstall()"))
        self.assertIn("if (!drawWhy.empty()) return drawWhy;", drops)
        self.assertIn("const std::string drawWhy = KillDrawInstall();", self.body("flight_switch"))

    def test_draw_hook_call_out_pair_wraps_each_original_call(self):
        # The splice: the call-out pair around the trampoline's call, the caller's own arguments, the
        # game's call timed as the game's; with the pair begun and ended on every way out.
        splice = self.draw("static RValue& KillDrawDropItem(CInstance* S, CInstance* O, RValue& R, int argc, RValue** A)")
        lines = [l.strip() for l in splice.split("\n") if l.strip()]
        self.assertEqual(lines, ["if (!g_KillDraw.orig) return R;", "IncidentScope incidentScope(IncidentMod::drops);",
                                 "KillDrawCall call;", "return FP_GAME_ORIGINAL(g_KillDraw.orig(S, O, R, argc, A));"])
        call = strip_comments(self.plugin[self.plugin.index("struct KillDrawCall {"):])
        call = call[:call.index("\n};")]
        self.assertRegex(call, r"KillDrawCall\(\)\s*\{\s*try \{ KillDrawBeginCall\(\); \} catch \(\.\.\.\) \{\}")
        self.assertRegex(call, r"~KillDrawCall\(\)\s*\{\s*try \{ KillDrawEndCall\(\); \} catch \(\.\.\.\) \{\}")
        # Every call is counted by both cores before anything is read; the push only for a call
        # either feature resolves (the switch, the gate, the art, no roll or DropItem call in progress).
        begin = self.draw("static void KillDrawBeginCall()")
        self.assertIn("g_M1Drop.core.BeginDrawCall(g_SigDetectNative, g_SigRollDepth + d.aside, g_FlightDrop.uniqueDepth, "
                      "Mark1DropArtReady())", begin)
        self.assertIn("g_ElfDraw.BeginCall(KillDrawElfOn(), g_SigRollDepth + g_FlightDrop.uniqueDepth)", begin)
        self.assertIn("if (!mark1 && !elf) return;", begin)
        self.assertLess(begin.index("if (!mark1 && !elf) return;"), begin.index("KillDrawPush(mark1, elf)"))
        for call_name in ("g_Yytk", "CallBuiltin", "CallGameScript"):
            self.assertNotIn(call_name, begin)
        end = self.draw("static void KillDrawEndCall()")
        order = ["KillDrawCloseHit();", "KillDrawRemove();", "g_M1Drop.core.EndDrawCall();", "g_ElfDraw.EndCall();"]
        for a, b in zip(order, order[1:]):
            self.assertLess(end.index(a), end.index(b), a + " before " + b)
        self.assertIn("if (d.pushDepth > 0 && d.pushDepth == d.depth) {", end)

    def test_draw_hook_nested_rolls_see_the_lists_as_before(self):
        # Each nested roll's hook declares the set-aside first, so its own entries come off before
        # the draw's go back on; the guard is the two halves, the outermost roll taking the entries off
        # whole and putting them back held read-back.
        aside = strip_comments(self.plugin[self.plugin.index("struct KillDrawRollAside {"):])
        aside = aside[:aside.index("\n};")]
        self.assertRegex(aside, r"KillDrawRollAside\(\)\s*\{\s*KillDrawEnterRoll\(\);\s*\}")
        self.assertRegex(aside, r"~KillDrawRollAside\(\)\s*\{\s*KillDrawLeaveRoll\(\);\s*\}")
        angelic = strip_comments(function_body(strip_research_blocks(self.plugin), "static RValue& HookAngelicChance("))
        for hook, first in ((angelic, "SignatureRollScope sigRoll;"), (self.body("shared_hook"), "FlightUniqueDepthScope depth;"),
                            (self.body("hook"), "Mark1DropInjectGuard inject(index);")):
            with self.subTest(first=first):
                self.assertEqual(hook.count("KillDrawRollAside drawAside;"), 1)
                self.assertLess(hook.index("KillDrawRollAside drawAside;"), hook.index(first))
        self.assertLess(angelic.index("KillDrawRollAside drawAside;"), angelic.index("SignatureInjectGuard sigInject;"))
        self.assertEqual(len(re.findall(r"\bKillDrawRollAside \w+;", self.whole)), 3)
        enter = self.draw("static void KillDrawEnterRoll()")
        self.assertIn("g_M1Drop.core.SetDrawAside();", enter)
        self.assertIn("g_ElfDraw.EnterRoll();", enter)
        self.assertIn("if (d.aside++ == 0 && d.pushDepth > 0 && d.onLists) d.offForRoll = KillDrawRemove();", enter)
        leave = self.draw("static void KillDrawLeaveRoll()")
        self.assertIn("g_M1Drop.core.RestoreDraw();", leave)
        self.assertIn("g_ElfDraw.LeaveRoll();", leave)
        self.assertLess(leave.index("if (d.aside != 0 || !d.offForRoll) return;"), leave.index("KillDrawPut(seen, threw)"))
        # The draw's scope is outside every roll: #74's typing never runs there, so its counters
        # cannot move.
        scope = self.draw("static bool KillDrawScope()")
        self.assertIn("d.aside == 0", scope)
        self.assertIn("g_SigRollDepth <= 0", scope)
        self.assertIn("!d.busy", scope)

    def test_draw_hook_typing_tier_rewrite_and_built_call_outs(self):
        # The tier after raredrop's lift, for the innermost DropItem call; the draw's latest read and
        # typed pick in #74's two typing detours (the pick settled after the game built the params);
        # the rewrite at CreateItemNew's entry before #74's own; the built count before the Heroic
        # roll's and the DropUniqueItems route's.
        tier = strip_comments(function_body(strip_research_blocks(self.plugin), "static RValue& HookSatanicTier("))
        self.assertLess(tier.index("RValue& r = SatanicTierLift(S, O, R, argc, A);"), tier.index("KillDrawNoteTier(r);"))
        repo = strip_comments(function_body(self.plugin, "static RValue& Hook_GetUniqueRepoStruct("))
        self.assertIn("KillDrawNoteRead(argc, A);", repo)
        self.assertLess(repo.index("KillDrawNoteRead(argc, A);"), repo.index("g_Orig_GetUniqueRepoStruct(S, O, R, argc, A)"))
        cdp = self.body("cdp")
        self.assertLess(cdp.index("const bool drawTyped = KillDrawTypePick(argc, A);"), cdp.index("g_Orig_CreateDefaultParams(S, O, R, argc, A)"))
        self.assertLess(cdp.index("g_Orig_CreateDefaultParams(S, O, R, argc, A)"), cdp.index("KillDrawSettleHit();"))
        before = self.body("sig_before_create")
        self.assertEqual([l.strip() for l in before.split("\n") if l.strip()][0],
                         "if (g_SigRollDepth <= 0 && KillDrawBeforeCreate(argc, A)) return;")
        self.assertEqual([l.strip() for l in self.body("note_built").split("\n") if l.strip()][0],
                         "if (KillDrawNoteBuiltPiece(piece)) return;")
        settle = self.draw("static void KillDrawSettleHit()")
        self.assertLess(settle.index("KillDrawCloseHit();"), settle.index("g_M1Drop.core.SettleDrawHit("))
        self.assertIn("const int k = g_M1Drop.core.DrawTier();", settle)
        rewrite = self.draw("static bool KillDrawBeforeCreate(int argc, RValue** A)")
        self.assertIn("g_M1Drop.core.TakeDrawRewrite(piece)", rewrite)
        self.assertIn("M1dNs::RewriteFor(piece)", rewrite)
        self.assertIn("SignatureRewriteParams(record, item, why)", rewrite)
        self.assertIn("Mark1DropRefuse(static_cast<int>(piece), shown);", rewrite)
        built = self.draw("static bool KillDrawNoteBuiltPiece(M1sNs::Piece piece)")
        self.assertIn("g_M1Drop.core.NoteBuilt(piece, M1dNs::Roll::Draw);", built)
        self.assertIn("g_SigRollDepth > 0 || d.aside > 0", built)
        close = self.draw("static void KillDrawCloseHit()")
        self.assertIn('"mark1set drops hit: "', close)
        self.assertIn('"kill draw -> "', close)
        self.assertEqual(close.count("Out("), 1)

    def test_draw_hook_push_is_the_cores_plan_on_all_six_lists_held_and_tail_checked(self):
        push = self.draw("static void KillDrawPush(bool mark1, bool elf)")
        self.assertIn("if (!Mark1KillReadLists(subs, keys, why)) {", push)
        self.assertIn("g_M1Drop.core.NoteDrawListsUnreadable();", push)
        self.assertIn("KillDrawBusyScope busy;", push)
        self.assertIn("FlightDropOutsideRoll outside;", push)
        self.assertIn("if (g_M1Drop.core.DrawCache().Stale(keys)) {", push)
        self.assertIn("M1dNs::ResolveDrawStandIns(lists, KillDrawPieceVerdict)", push)
        self.assertIn("plan = g_M1Drop.core.PlanDrawFor(*r, g_Mark1DropCopies);", push)
        self.assertLess(push.index("KillDrawPut(seen, threw)"), push.index("g_M1Drop.core.NoteDrawPushed(plan);"))
        put = self.draw("static bool KillDrawPut(std::string& seen, bool& threw)")
        self.assertIn("for (int k = 0; k < M1dNs::kListCount && !threw; ++k) {", put)
        self.assertLess(put.index("Mark1DropAddEntry(d.subs[k], d.rows[e].triple);"), put.index("KillDrawHeldReadBack(seen)"))
        self.assertIn("KillDrawRemove();", put)
        held = self.draw("static bool KillDrawHeldReadBack(std::string& seen)")
        self.assertIn("Mark1KillReadLists(subs, keys, why)", held)
        self.assertIn("Mark1ListTailHolds(d.subs[k], d.before[k], d.rows, d.on[(size_t)k], len)", held)
        remove = self.draw("static bool KillDrawRemove()")
        self.assertLess(remove.index("Mark1ListTailHolds("), remove.index("Mark1DropDeleteLast("))
        self.assertIn("KillDrawAnomaly();", remove)
        verdict = self.draw("static bool KillDrawDefinition(long long t, long long sub, long long b, int& rarity, int& tier, bool& f40, bool& f41)")
        self.assertIn("g_Yytk->CallGameScript(HeroSiege::Scripts::gml_Script_GetUniqueRepoStruct,", verdict)
        for field in ('"27"', '"32"', '"40"', '"41"'):
            self.assertIn(field, verdict)

    def test_draw_hook_research_holders_refuse_both_ways_in_the_research_build_only(self):
        install = self.draw("static std::string KillDrawInstall()")
        for holder in ("g_ApRollRows[kApRowDropItem].tramp", "g_GpScriptRows[kGpScript_DropItem].orig", "M1pDrawsHoldsDropItem()"):
            self.assertIn(holder, install)
            self.assertLess(install.index(holder), install.index("InstallDropItemHook()"))
        shipped = strip_comments(function_body(strip_research_blocks(self.plugin), "static std::string KillDrawInstall()"))
        for holder in ("g_ApRollRows", "g_GpScriptRows", "M1pDraws", "probe"):
            self.assertNotIn(holder, shipped)
        attach = self.body("ap_attach")
        refusal = attach.index('std::string_view(r.id) == "drop-item" && KillDrawHoldsDropItem()')
        self.assertLess(refusal, attach.index("GetNamedRoutinePointer"))
        self.assertIn("kApRollBlocked", attach[refusal:attach.index("GetNamedRoutinePointer")])
        self.assertIn("&t == &g_GpScriptRows[kGpScript_DropItem] && KillDrawHoldsDropItem()", self.body("gp_install"))
        watch = strip_comments(function_body(self.plugin, "static void M1pDrawsInstall()"))
        self.assertIn("&& !KillDrawHoldsDropItem()", watch)
        self.assertIn('if (KillDrawHoldsDropItem()) return "held-by-kill-draw";',
                      strip_comments(function_body(self.plugin, "static std::string M1pDrawsDropItemRoute()")))
        self.assertIn("return g_KillDraw.orig != nullptr;", self.draw("static bool KillDrawHoldsDropItem()"))

    def test_draw_hook_player_build_names_and_the_copies_lever(self):
        start = self.plugin.index("// ---- the kill draw (#166): DropItem's own unique draw")
        end = self.plugin.index("// ---- end of the kill draw")
        block = self.plugin[start:end]
        shipped = strip_research_blocks(block)
        for word in ("mark1probe", "flightprobe", "angelicprobe", "gambaprobe", "M1p"):
            self.assertNotIn(word, shipped)
        code = strip_comments(shipped)
        self.assertNotIn('"gml_Script_', code)
        self.assertIsNone(re.search(r"\b\w*Rva\w*\b", code))
        self.assertIsNone(re.search(r"\b0x[0-9A-Fa-f]{5,}\b", code))
        self.assertNotIn("GetModuleHandle", code)
        self.assertNotIn("HookOneScript(", code)
        # The copies are each feature's own lever, a constexpr 1 in the player build.
        self.assertIn("g_M1Drop.core.PlanDrawFor(*r, g_Mark1DropCopies)", code)
        self.assertIn("FcNs::ElfDrawCopies(elfStandIn, g_FlightDropCopies)", code)
        self.assertIn("static constexpr int g_FlightDropCopies = 1;", self.plugin)
        # The `mark1set drops 1` reply carries the draw's gate (the core's DropsReply).
        header = (ROOT / "plugin" / "include" / "ForgePact" / "Mark1Drops.hpp").read_text(encoding="utf-8")
        self.assertIn('" draw-gate=" + std::string(GateName(d.DrawGateNow(detectionNative)))', header)


if __name__ == "__main__":
    unittest.main()
