"""Move all into the stash (`stashmoveall`, `stashmove`, F4): the wiring.

tests/test_stash_move_all_behavior.py runs the real decision core
(plugin/include/ForgePact/StashMoveAllMod.hpp) against its baseline and target
scenarios. This file pins what that harness cannot see, on comment-stripped
source the way the stash and bag verbs' contract does (StashBagPlayerVerbs in
test_stash_bag_layout_contract.py): the two verbs are player commands, each
dispatched from its own helper; every game routine is an hs-game-sdk constant
resolved by name, never an address; nothing writes a container, a map entry or
an item; the frame path does nothing while the switch is off and the key only
starts a run with the game in front and the stash open; each item is re-read
and decided before the next; a refusal skips and a loss turns the mod off; and
the panel, README, release notes and research doc say what the mod does
(docs/stash-move-research.md, § Ship design).

ForgePact #131: the button's origin comes from the core's ButtonOrigin and the
extents measured on the node itself, with at most two UiCreateNode calls per
creation step; the cell route's StashAddToStack passes a stackable's whole
count and a true answer there is confirmed as a merge by the sum; and the stack
route's room is a stack with room for the whole count, not any stack.
Owner scope of 2026-09-30: the button takes the Sort Tab button's look, its
sprite and scale read off the Sort node by name and copied onto the mod's own
node, never a sprite constant, and its size is judged against Sort's on the
settled box, kept and said once when off.
"""
import re
import sys
import unittest
from pathlib import Path

TESTS = Path(__file__).resolve().parent
ROOT = TESTS.parent
sys.path.insert(0, str(TESTS))
sys.path.insert(0, str(ROOT / "src"))

from panel_source import panel_file  # noqa: E402
from test_release_hook_contract import function_body, strip_comments, strip_research_blocks  # noqa: E402

PLUGIN = ROOT / "plugin" / "ModuleMain.cpp"
HEADER = ROOT / "plugin" / "include" / "ForgePact" / "StashMoveAllMod.hpp"
README = ROOT / "README.md"
DOC = ROOT / "docs" / "stash-move-research.md"
NOTES_VERSION = "2.1.0"
NOTES = ROOT / f"release-notes-v{NOTES_VERSION}.md"

BLOCK = ("// ---- stashmoveall, stashmove: Move all into the stash (ForgePact #68)",
         "// ---- end stashmoveall, stashmove")
BUTTON_BLOCK = ("// ---- stashmoveall button: the in-game Move all button (ForgePact #68)",
                "// ---- end stashmoveall button")
VERBS = {
    "stashmoveall": ("HandleStashMoveAllCommand", "StashMoveAllCommand"),
    "stashmove": ("HandleStashMoveCommand", "StashMoveCommand"),
}
# The game routines the move calls, each only through its SDK constant
# (§ Decision: gridMoveRoute, stackMoveRoute, sourceCellClear, mapOwnerRule,
# newMaterialRoute, wholeStackMerge; GridRemoveItem is the undo).
ROUTINES = {"ValidateItem", "StashAddToStack", "GridAddItem", "InvGridClearItemNode",
            "ChangeItemOwner", "GridRemoveItem"}
# What would write a container, a map entry or an item, or make or destroy an
# instance: none of it may appear in the adapter.
WRITES = ("variable_instance_set", "variable_struct_set", "variable_global_set", "array_set", "array_push",
          "array_delete", "array_insert", "ds_map_add", "ds_map_replace", "ds_map_delete", "ds_map_set",
          "ds_list_", "ds_grid_", "instance_create", "instance_destroy", "[@", "[?")
# What would reach the game by address, or hook it.
ADDRESSES = ("Rva", "GetModuleHandle", "MmCreateHook", "HookOneScript", "HookBuiltin", "reinterpret_cast",
             "CallGameScriptEx", "event_perform")
# Words that belong in the README, notes and research doc, never in the
# panel's one-line description (guide § Representative Change Workflow step 7).
DEV_WORDS = ("ValidateItem", "StashAddToStack", "GridAddItem", "ChangeItemOwner", "map 0", "map 9", "byname",
             "by name", "measured", "Live 1", "Live 2", "#68", "stashmoveall", "fingerprint")
SPILL = re.compile(r"(never|not) [^.]*(another|other|next) [^.]*(tab|page)", re.I)


def _block(source):
    start, end = source.index(BLOCK[0]), source.index(BLOCK[1])
    return source[start:end]


class StashMoveAllContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.plugin = PLUGIN.read_text(encoding="utf-8").replace("\r\n", "\n")
        cls.shipped = strip_research_blocks(cls.plugin)
        cls.block = _block(cls.plugin)
        cls.code = strip_comments(cls.block)
        cls.header = strip_comments(HEADER.read_text(encoding="utf-8").replace("\r\n", "\n"))

    def body(self, signature):
        return strip_comments(function_body(self.plugin, signature))

    # ---- the verbs ------------------------------------------------------------

    def test_verbs_are_player_commands_dispatched_from_their_own_handlers(self):
        run = function_body(self.plugin, "static void RunCommand(const std::string& line)")
        commands = run[run.index("kPlayerCommands = {"):]
        commands = commands[:commands.index("};")]
        self.assertIn('"menulayout"', commands)   # the set is the one read, not an empty slice
        # The whole block ships: none of it is research-only.
        self.assertIn(self.block, self.shipped)
        self.assertNotIn("FORGEPACT_RELEASE", self.block)
        for verb, (handler, command) in VERBS.items():
            self.assertIn(f'"{verb}"', commands, verb)
            self.assertIn(f"    if ({handler}(lc, rest)) return;\n", run, verb)
            self.assertIn(f'if (lc == "{verb}") {{ {command}(rest); return true; }}',
                          self.body(f"static bool {handler}("), verb)
            self.assertEqual(self.plugin.count(f'lc == "{verb}"'), 1, verb)
            self.assertIn(f"static void {command}(", self.shipped, verb)
        # The switch, its run and the bare state line.
        cmd = self.body("static void StashMoveAllCommand(")
        self.assertIn('arg == "1"', cmd)
        self.assertIn('arg == "0"', cmd)
        self.assertIn('arg == "run"', cmd)
        self.assertIn("StashMoveAllRun()", cmd)
        self.assertIn("mod.StateLine()", cmd)
        self.assertIn('"stashmoveall: state="', self.header)
        self.assertIn('" key=F4"', self.header)

    def test_every_routine_is_an_sdk_constant_resolved_by_name(self):
        consts = re.findall(r"static constexpr TalentAllocScript (kSma\w+)\{ HeroSiege::Scripts::gml_Script_(\w+),\s*"
                            r"SdkShortScriptName\(HeroSiege::Scripts::gml_Script_(\w+)\) \};", self.code)
        self.assertEqual({c[1] for c in consts}, ROUTINES)
        for name, routine, short in consts:
            self.assertEqual(routine, short, name)
        # Nothing names a game script by hand.
        self.assertNotIn('"gml_Script_', self.code)
        for routine in ROUTINES:
            self.assertNotIn(f'"{routine}"', self.code, routine)
        # One dispatcher: GetNamedRoutinePointer on the SDK constant, then
        # asset_get_index on its short name, then script_execute with self and
        # other passed apart.
        call = self.body("static TalentAllocCall SmaCall(")
        order = [call.index(t) for t in ("GetNamedRoutinePointer(s.routine.data()", 'CallBuiltin("asset_get_index"',
                                         'CallBuiltinEx(res, "script_execute", self, other, callArgs)')]
        self.assertEqual(order, sorted(order))
        self.assertEqual(self.code.count("CallBuiltinEx("), 1)
        self.assertEqual(self.code.count('"script_execute"'), 1)
        # Every routine call goes through it with a kSma constant.
        for m in re.finditer(r"SmaCall\((\w+),", self.code):
            self.assertIn(m.group(1), {c[0] for c in consts} | {"const"}, m.group(0))
        # Every instance by name: the SDK's object enum, never a hand-typed name.
        self.assertNotIn('"UI_', self.code)
        self.assertNotIn('"Controller_obj"', self.code)
        self.assertIn("HeroSiege::Objects::GameObject::UI_Stash_obj", self.code)
        self.assertIn("HeroSiege::Objects::GameObject::UI_Inventory_Grid_obj", self.code)

    def test_no_container_or_map_is_written_directly(self):
        for word in WRITES:
            self.assertNotIn(word, self.code, word)
        # The core names no runtime surface at all (criterion 5's guard).
        for word in ("RValue", "CInstance", "YYTK", "script_execute", "GetNamedRoutinePointer", "Aurie"):
            self.assertNotIn(word, self.header, word)
        # Map reads only: the stash map is looked up, never edited.
        self.assertIn("CmMapItem(s.map9", self.code)
        self.assertNotIn("CmCall(kCmAddToMapName", self.code)
        self.assertNotIn("CmCall(kCmRemoveFromMapName", self.code)

    def test_nothing_is_reached_by_address(self):
        for word in ADDRESSES:
            self.assertNotIn(word, self.code, word)
        # The only hex literal is the key state's high bit (the hotkey's and
        # the modifiers').
        self.assertEqual(set(re.findall(r"0x[0-9A-Fa-f]+", self.code)), {"0x8000"})
        self.assertIn("static constexpr int kSmaHotkey = VK_F4;", self.code)

    # ---- the frame path -------------------------------------------------------

    def test_frame_path_is_gated_on_the_switch_and_the_hotkey_needs_foreground_and_a_listed_stash(self):
        frame = strip_comments(function_body(self.plugin, "void FrameCallback(FWFrame& FrameContext)"))
        self.assertEqual(frame.count("StashMoveAllTick()"), 1)
        self.assertIn("if (g_Setup) StashMoveAllTick();", frame)
        self.assertNotIn("StashMoveAllRun", frame)
        tick = self.body("static void StashMoveAllTick(")
        # Off: nothing is read, not even the key; a button node still held is
        # removed, and knowing whether one is held reads nothing of the game.
        # A node UiRemoveNode left listed is retried at the ensure step's
        # pace, not every frame.
        gate = tick.index("if (!mod.IsEnabled()) { s_WasOn = false; if ((s_Frame++ % kSmaButtonEveryFrames) == 0) "
                          "SmaButtonRemove(); return; }")
        self.assertLess(gate, tick.index("GetAsyncKeyState(kSmaHotkey)"))
        self.assertEqual(tick.count("GetAsyncKeyState"), 1)
        self.assertTrue(self.body("static void SmaButtonRemove(").lstrip("{ \n").startswith("if (!g_SmaButtonHeld) return;"))
        # The foreground and the stash window are asked only while the key is
        # down or the button recorded a press.
        down = tick.index("if (down || pressed) {")
        self.assertLess(down, tick.index("SmaGameInForeground()"))
        self.assertLess(down, tick.index("HeroSiege::Objects::GameObject::UI_Stash_obj"))
        self.assertIn("listed = fg && CmInstance(", tick)
        self.assertIn("const bool key = mod.KeyEdge(down, fg, listed, modifier);", tick)
        self.assertIn("if (key || button) StashMoveAllRun();", tick)
        self.assertEqual(tick.count("StashMoveAllRun()"), 1)
        # The first frame on only notes the key.
        self.assertIn("if (!s_WasOn) { s_WasOn = true; mod.KeyEdge(down, false, false, false); return; }", tick)
        self.assertIn("return press && foreground && stashListed && !modifier;", self.header)
        fg = self.body("static bool SmaGameInForeground(")
        self.assertIn("GetForegroundWindow()", fg)
        self.assertIn("pid == GetCurrentProcessId()", fg)

    # ---- one item at a time ---------------------------------------------------

    def test_each_item_is_confirmed_by_reread_before_the_next(self):
        run = self.body("static void StashMoveAllRun(")
        loop = run[run.index("for (const ForgePact::StashMoveItem& it : plan.items)"):]
        self.assertLess(loop.index("SmaMoveOne(s, plan, it, note)"), loop.index("mod.Record(t, res)"))
        self.assertIn("if (!go) break;", loop)
        self.assertLess(run.index("mod.Plan(s.view)"), run.index("SmaMoveOne("))
        one = self.body("static ForgePact::StashMoveResult SmaMoveOne(")
        # At the point of use, before any call: the bag cell, the item on map
        # 0, the shown tab still the planned one and its room, the core's gate.
        first_call = one.index("SmaCall(")
        for read in ("SmaBagCellHolds(s, it.cell.x, it.cell.y, key, &cell) != 1", "ApItemFromFingerprint(s.bag",
                     'SmaTab(s.window, "stashTabSelected") == plan.stashTab', "Mod::Room(SmaGrid(arr)",
                     "Mod::MayCall(use, room, skip)"):
            self.assertLess(one.index(read), first_call, read)
        # After the calls: the tab on show, its own array, the bag cell - then
        # the core decides.
        decide = one.index("Mod::Decide(use, r)")
        last_owner = one.rindex("SmaCall(kSmaOwner", 0, decide)
        tab_now = one.index('const int tabNow = SmaTab(s.window, "stashTabSelected");')
        self.assertLess(last_owner, tab_now)
        self.assertLess(tab_now, decide)
        self.assertIn("r.shownTabChanged = tabNow == Mod::kUnreadTab ? -1 : (tabNow != plan.stashTab ? 1 : 0);", one)
        self.assertLess(one.index("r.sourceHasKey = SmaBagCellHolds("), decide)
        self.assertLess(one.index("r.stackAfter = nowRead ? SmaStackSum("), decide)
        # The shown tab's array is the only stash container read.
        arrays = self.body("static bool SmaShownArray(")
        self.assertIn('CmArrayVar(s.stashNode, "nodeGrid", cells)', arrays)
        self.assertIn("CmArrayVar(handle, kCmMaterialTabVar, cells)", arrays)
        self.assertNotIn("kCmSocketTabVar", self.code)
        # The source clear before the owner step, the owner step only once the
        # bag cell reads empty, and a merge's clear only after the sum rose by
        # the whole count.
        self.assertLess(one.index("SmaCall(kSmaClear, s.bag, s.bag, { cell, RValue() }, res);"),
                        one.index("SmaCall(kSmaOwner, s.sg, s.bag, { RValue(kSmaCharacterOwner), RValue(kSmaStashOwner)"))
        owner = one[one.index("if (!personal) {"):]
        self.assertLess(owner.index("if (SmaBagCellHolds(s, it.cell.x, it.cell.y, key) == 0) {"),
                        owner.index("SmaCall(kSmaOwner"))
        self.assertIn("r.accepted && before >= 0 && after - before == count", one)
        self.assertIn("RValue((double)count)", one)

    def test_a_refusal_skips_and_a_loss_turns_the_mod_off(self):
        record = function_body(HEADER.read_text(encoding="utf-8").replace("\r\n", "\n"),
                               "bool Record(StashMoveTally& t, const StashMoveResult& r)")
        self.assertIn("if (r.outcome == StashMoveOutcome::Skipped) { ++t.skipped; return true; }", record)
        self.assertIn("TurnOffForSession(reason);", record)
        self.assertIn("if (on && m_OffThisSession.load()) return false;", self.header)
        one_verb = self.body("static void StashMoveCommand(")
        self.assertIn('if (res.outcome == ForgePact::StashMoveOutcome::Unconfirmed) mod.TurnOffForSession("item " + res.key + ": " + res.answer);', one_verb)
        # A refusal is printed with the core's refusal line, which says nothing
        # was called; the switch turned on again after a loss is refused.
        self.assertIn('return verb + ": refused - " + reason + "; nothing was called";', self.header)
        cmd = self.body("static void StashMoveAllCommand(")
        self.assertIn("if (!mod.SetEnabled(true)) { Out(mod.OffForSessionLine()); Out(mod.StateLine()); return; }", cmd)
        # The undo runs only for a placed item whose bag cell did not clear.
        one = self.body("static ForgePact::StashMoveResult SmaMoveOne(")
        undo = one[one.index("out.outcome == ForgePact::StashMoveOutcome::Unconfirmed && placing"):]
        self.assertIn("r.sourceHasKey == 1 && CmCellsHold(now, key) == 1", undo)
        self.assertIn("SmaCall(kSmaRemove, s.sg, s.sg, { now, RValue(key) }, res);", undo)

    # ---- the round-2 review's fixes --------------------------------------------

    def test_cell_route_rereads_the_stack_sum_before_stash_add_to_stack(self):
        # The route is decided at the point of use: before the item's first
        # call, a stackable's stacks are re-read on the shown array and the
        # core's RouteAtUse decides, whatever the plan said; the branch taken
        # is the one it answers, and the outcome is decided on that item.
        # Since #131 the core is handed each stack, not their sum.
        one = self.body("static ForgePact::StashMoveResult SmaMoveOne(")
        reread = one.index("else if (arrRead) stacksNow = SmaStacks(s, arr, cls, base);")
        at_use = one.index("use = Mod::RouteAtUse(use, plan.stashTab, stacksNow);")
        self.assertLess(reread, one.index("before = Mod::StackSum(stacksNow);"))
        self.assertLess(reread, at_use)
        self.assertIn("if (it.cell.stackable) {", one[:reread])
        self.assertLess(at_use, one.index("SmaCall(kSmaAddToStack"))
        self.assertLess(at_use, one.index("SmaCall("))
        self.assertIn("const bool cellRoute = use.route == ForgePact::StashMoveRoute::Cell;", one)
        self.assertLess(at_use, one.index("const bool cellRoute = use.route"))
        self.assertNotIn("it.route == ForgePact::StashMoveRoute::Cell", one)
        self.assertIn("Mod::Decide(use, r)", one)
        self.assertNotIn("Mod::Decide(it, r)", one)
        # The core's helper decides with the same rules as the plan.
        at = function_body(HEADER.read_text(encoding="utf-8").replace("\r\n", "\n"),
                           "static StashMoveItem RouteAtUse(")
        self.assertIn("RouteFor(TabOf(stashTab), planned.cell, stacksNow, routes, item);", at)
        self.assertIn("RouteFor(tab, c, c.destinationStacks, routes, item);", self.header)
        # The plan reads each identity's stacks the same way.
        scene = self.body("static bool SmaReadScene(")
        self.assertIn("stacks[id] = shownRead ? SmaStacks(s, shown, cls, base) : ForgePact::StashMoveStacks();", scene)
        self.assertIn("c.destinationStacks = stacks[id];", scene)

    def test_key_edge_ignores_a_held_modifier(self):
        # Alt+F4 closes the game: any held Alt, Ctrl or Shift is no edge.
        held = self.body("static bool SmaModifierHeld(")
        for key in ("VK_MENU", "VK_CONTROL", "VK_SHIFT"):
            self.assertIn(key, held, key)
        self.assertIn("GetAsyncKeyState", held)
        tick = self.body("static void StashMoveAllTick(")
        down = tick.index("if (down || pressed) {")
        self.assertLess(down, tick.index("modifier = SmaModifierHeld();"))
        self.assertIn("const bool key = mod.KeyEdge(down, fg, listed, modifier);", tick)
        self.assertIn("const bool button = mod.TakeButtonPress(fg, listed, modifier);", tick)
        self.assertIn("bool KeyEdge(bool down, bool foreground, bool stashListed, bool modifier)", self.header)
        self.assertIn("return press && foreground && stashListed && !modifier;", self.header)

    def test_off_after_loss_is_reported_distinctly(self):
        # After a loss the state line says so, with the reason; turning on
        # again answers with the reason and stays off; every switch and a loss
        # print the state line, whose last copy the panel reads.
        # The button's fields stay on it, the free-text reason last.
        self.assertIn('"stashmoveall: state=off-for-this-session" + ButtonFields() + " reason=" + m_OffReason',
                      self.header)
        self.assertIn('return LossLine("stashmoveall", m_OffReason);', self.header)
        cmd = self.body("static void StashMoveAllCommand(")
        self.assertIn("if (!mod.SetEnabled(true)) { Out(mod.OffForSessionLine()); Out(mod.StateLine()); return; }", cmd)
        self.assertEqual(cmd.count("Out(mod.StateLine());"), 4)
        run = self.body("static void StashMoveAllRun(")
        self.assertIn("if (t.stopped) Out(mod.StateLine());", run)
        one = self.body("static void StashMoveCommand(")
        self.assertIn("if (res.outcome == ForgePact::StashMoveOutcome::Unconfirmed) Out(mod.StateLine());", one)
        # The panel reads the last state line and shows the loss.
        import forgepact  # noqa: E402
        source = (ROOT / "src" / "forgepact.py").read_text(encoding="utf-8")
        self.assertIn('"stash_move_all_session": stash_move_all_session(cfg)', source)
        import tempfile
        from unittest import mock
        with tempfile.TemporaryDirectory() as d, mock.patch.object(forgepact, "ipc_dir", return_value=Path(d)):
            out = Path(d) / "out.txt"
            read = forgepact.stash_move_all_session
            self.assertEqual(read({}), "")   # no log: the plugin has said nothing
            out.write_bytes(b"BloodPact plugin loaded\r\nstashmoveall: state=on key=F4\r\nping\r\n")
            self.assertEqual(read({}), "on")
            with out.open("ab") as fh:
                fh.write(b"stashmoveall: off for this session - item k: x; turn it on again after restarting the game\r\n"
                         b"stashmoveall: state=off-for-this-session reason=item k: x\r\n")
            self.assertEqual(read({}), "off-after-loss")
            with out.open("ab") as fh:
                fh.write(b"stashmoveall: state=off key=F4\r\n")
            self.assertEqual(read({}), "off")
            # Only the tail is read: a state line older than 64 KB is not this
            # read's to report.
            out.write_bytes(b"stashmoveall: state=off-for-this-session reason=old\r\n" + b"." * (70 * 1024))
            self.assertEqual(read({}), "")
        js = panel_file("panel.js")
        self.assertIn("off (this session)", js)
        self.assertIn("stash_move_all_session", js)

    def test_owner_step_answer_enters_the_report(self):
        # "Ran and did nothing" is told apart from success: the second
        # ValidateItem's answer and the owner step's dispatch, answer and the
        # key's map 0 lookup after it go into the report the core decides on.
        one = self.body("static ForgePact::StashMoveResult SmaMoveOne(")
        self.assertIn("r.validateAnswer = SmaAnswerText(kSmaValidate, vc, res);", one)
        self.assertIn("r.ownerStep = 1;", one)
        self.assertIn("r.ownerDispatched = ownerRan ? 1 : 0;", one)
        self.assertIn("r.ownerAnswer = SmaAnswerText(kSmaOwner, oc, res);", one)
        self.assertIn("r.keyOnMap0 = SmaKeyOnMap0(s, key);", one)
        self.assertLess(one.index("r.keyOnMap0 = SmaKeyOnMap0(s, key);"), one.index("Mod::Decide(use, r)"))
        lookup = self.body("static int SmaKeyOnMap0(")
        self.assertIn("ApCallScript(kApFromFpName, s.bag, { RValue(key), RValue(0.0) }, item)", lookup)
        self.assertIn("return item.m_Kind == VALUE_UNDEFINED ? 0 : -1;", lookup)
        for field in ("validateAnswer", "ownerStep", "ownerDispatched", "ownerAnswer", "keyOnMap0"):
            self.assertIn(field, self.header, field)
        decide = function_body(HEADER.read_text(encoding="utf-8").replace("\r\n", "\n"),
                               "static StashMoveResult Decide(")
        self.assertIn("if (r.ownerStep == 1) {", decide)
        self.assertIn("if (r.ownerDispatched != 1)", decide)
        self.assertIn("if (r.keyOnMap0 == 1)", decide)
        self.assertIn("if (r.keyOnMap0 != 0)", decide)

    # ---- #131: the per-stack rule at the point of use ---------------------------

    def test_cell_route_passes_the_whole_count_and_confirms_a_merge_by_the_sum(self):
        # The cell route's first StashAddToStack carries a stackable's whole
        # count, the value the measured merge passes, so a merge the game
        # makes there takes the whole item, never one unit of it. A true
        # answer is decided as a merge: the sum re-read, the bag cell cleared
        # only after it rose by exactly the count, and the core's AsMerge puts
        # the item on the stack route so Decide confirms it by the sum.
        one = self.body("static ForgePact::StashMoveResult SmaMoveOne(")
        cell = one[one.index("else if (cellRoute) {"):one.index("if (merge) {")]
        self.assertIn("RValue(use.cell.stackable ? (double)count : 1.0), RValue(kSmaStackA5) }, res);", cell)
        self.assertNotIn("RValue(1.0), RValue(0.0) }", one)
        self.assertIn("merge = use.cell.stackable;", cell)
        self.assertNotIn("merged, not placed", self.plugin)
        followup = one[one.index("if (merge) {"):one.index("const bool placing = cellRoute && !merge;")]
        self.assertIn("r.stackBefore = before;", followup)
        self.assertIn("const int64_t after = SmaReread(s, plan.stashTab, node, now) ? SmaStackSum(s, now, cls, base) : -1;",
                      followup)
        self.assertLess(followup.index("after - before == count"), followup.index("SmaCall(kSmaClear"))
        self.assertIn("if (cellRoute) use = Mod::AsMerge(use);", followup)
        # The outcome is read as a merge: the stack re-read, no placement undo.
        self.assertLess(one.index("const bool placing = cellRoute && !merge;"), one.index("Mod::Decide(use, r)"))
        self.assertIn("if (placing) {", one)
        self.assertIn("out.outcome == ForgePact::StashMoveOutcome::Unconfirmed && placing && nowRead", one)
        # The core's side: AsMerge is the stack route, nothing else changed.
        merge = function_body(HEADER.read_text(encoding="utf-8").replace("\r\n", "\n"), "static StashMoveItem AsMerge(")
        self.assertIn("merged.route = StashMoveRoute::Stack;", merge)
        # The sixth argument is the measured one, and the core's cap is read
        # for the same value.
        self.assertIn("static_assert((int)kSmaStackA5 == ForgePact::StashMoveAllMod::kPageStackFlags", self.code)
        self.assertIn("static_assert((int)kSmaSocketStackA5 == ForgePact::StashMoveAllMod::kSocketStackFlags", self.code)
        self.assertIn("return (sixthArgument & kStackFlag8) ? kStackCapFlag8 : kStackCap;", self.header)
        self.assertIn("static constexpr int64_t kStackCap = 999;", self.header)
        self.assertIn("static constexpr int64_t kStackCapFlag8 = 999999;", self.header)

    def test_stack_route_room_is_a_stack_with_room_not_any_stack(self):
        # A full stack answers false and moves nothing, so the stack route's
        # room is the core's "a stack of the identity has room for the whole
        # count" at the cap the route's sixth argument sets - never "a stack
        # exists" (the sum-only rule the owner's report of 2026-09-30 found).
        one = self.body("static ForgePact::StashMoveResult SmaMoveOne(")
        self.assertIn("room = Mod::StackRoom(stacksNow, use.cell.count, Mod::CapFor(plan.stashTab));", one)
        self.assertNotIn("before > 0 ? 1", one)
        self.assertLess(one.index("Mod::StackRoom("), one.index("Mod::MayCall(use, room, skip)"))
        # Unread stacks are unread as a whole, never "none there".
        stacks = self.body("static ForgePact::StashMoveStacks SmaStacks(")
        self.assertIn("out.read = true;", stacks)
        self.assertEqual(stacks.count("return ForgePact::StashMoveStacks();"), 5)
        room = function_body(HEADER.read_text(encoding="utf-8").replace("\r\n", "\n"), "static int StackRoom(")
        self.assertIn("return fit >= 0 ? 1 : (fit == kNoStackFits ? 0 : -1);", room)
        fits = function_body(HEADER.read_text(encoding="utf-8").replace("\r\n", "\n"), "static int StackThatFits(")
        self.assertIn("if (!stacks.read || count < 1) return kStacksUnknown;", fits)
        self.assertIn("if (stacks.counts[i] + count <= cap) return (int)i;", fits)

    # ---- the in-game button (buttonRoute: poll, Live 1g) ------------------------

    def test_button_origin_comes_from_the_core_and_the_measured_extents(self):
        # #131: UiCreateNode's x, y are the node's origin, UI_Button_Small_obj's
        # being its bbox centre and Sort's its top-left (Live 1f and 1g). The
        # origin is the core's ButtonOrigin from Sort's bbox and the extents
        # read from the node itself after it is made and labelled (kept for
        # the session), never a formula built on Sort's top-left or a
        # constant; an off-target node is taken away by UiRemoveNode and made
        # again once, so a Create step makes at most two; one still off is
        # kept and said once, and the mod stays on.
        header = HEADER.read_text(encoding="utf-8").replace("\r\n", "\n")
        create = self.body("static void SmaButtonCreate(")
        self.assertNotIn("sx - sw - kSmaButtonGap", create)
        self.assertIn("const ForgePact::StashMoveBox sortBox = SmaBox(sort);", create)
        self.assertIn('const ForgePact::StashMoveExtents extents = mod.ButtonExtents(sortBox, MenuLayoutRead(sort, "x"), '
                      'MenuLayoutRead(sort, "y"));', create)
        self.assertIn("ForgePact::StashMoveAllMod::ButtonOrigin(sortBox, extents, kSmaButtonGap, x, y)", create)
        self.assertLess(create.index("SmaButtonMake(stash, window, x, y, why)"), create.index("mod.NoteButtonMade(false);"))
        # The extents: measured on a settled node this session; before that
        # Sort's own about its x, y (the node wears Sort's look, owner scope
        # 2026-09-30); the centred box of Sort's size when those did not read.
        extents = function_body(header, "StashMoveExtents ButtonExtents(")
        order = [extents.index(t) for t in ("if (m_ButtonExtentsRead) return m_ButtonExtents;",
                                            "if (ExtentsOf(sortX, sortY, sort, own)) return own;",
                                            "return ProvisionalExtents(sort);")]
        self.assertEqual(order, sorted(order))
        # Review of round 0 (instrument blindness): the place is never judged
        # in the frame the node is made - Create reads no box of the node -
        # but by SmaButtonCheck on later ensure steps, from what the node
        # reads then (visible, x, y, bbox), and only once its box has read
        # the same on two steps with the node visible.
        for word in ("SmaBox(g_SmaButton)", "ButtonOnTarget", "ButtonCheck", "ExtentsOf"):
            self.assertNotIn(word, create, word)
        check = self.body("static void SmaButtonCheck(")
        self.assertTrue(check.lstrip("{ \n").startswith("if (!g_SmaButtonHeld) return;"))
        self.assertIn('"variable_instance_get", { g_SmaButton, RValue("visible") }', check)
        self.assertIn('mod.ButtonCheck(visible, SmaBox(sort),\n        MenuLayoutRead(g_SmaButton, "x"), '
                      'MenuLayoutRead(g_SmaButton, "y"), SmaBox(g_SmaButton), kSmaButtonGap, x, y, line);', check)
        self.assertIn("default: SmaButtonCheck(stash, window, sort); break;", self.body("static void SmaButtonEnsure("))
        judge = function_body(header, "StashMoveButtonCheck ButtonCheck(")
        self.assertIn("if (m_ButtonMakes == 0 || m_ButtonChecked) return StashMoveButtonCheck::Keep;", judge)
        self.assertIn("const bool reads = visible && BoxReads(box) && BoxReads(sort);", judge)
        self.assertIn("const bool settled = reads && m_ButtonHaveLast && SameBox(box, m_ButtonLast) "
                      "&& SameBox(sort, m_ButtonLastSort);", judge)
        self.assertLess(judge.index("if (!settled) {"), judge.index("ExtentsOf(nodeX, nodeY, box, e)"))
        self.assertLess(judge.index("ExtentsOf(nodeX, nodeY, box, e)"), judge.index("ButtonOnTarget(sort, box, gap)"))
        self.assertIn("static constexpr int kButtonSettleSteps = 6;", self.header)
        # One UiCreateNode site, reached at most twice per Create step: the
        # first make, and one more only after the core asked for it once and
        # UiRemoveNode took the first away.
        self.assertEqual(self.button_block().count("SmaCall(kSmaUiCreateNode"), 1)
        self.assertEqual(self.button_block().count("SmaButtonMake(stash, window, x, y, why)"), 2)
        self.assertIn("if (!m_ButtonRemakeAsked && ButtonOrigin(sort, e, gap, x, y)) {", judge)
        self.assertLess(judge.index("if (!m_ButtonRemakeAsked"), judge.index("m_ButtonRemakeAsked = true;"))
        made = function_body(header, "void NoteButtonMade(bool remake)")
        self.assertIn("if (!remake) m_ButtonRemakeAsked = false;", made)
        remake = check[check.index("if (step != ForgePact::StashMoveButtonCheck::Remake) return;"):]
        order = [remake.index(t) for t in ("SmaButtonRemove();", "if (g_SmaButtonHeld) return;",
                                           "SmaButtonMake(stash, window, x, y, why)", "mod.NoteButtonMade(true);")]
        self.assertEqual(order, sorted(order))
        # Still off: said once by the core, the mod left on; placed on target
        # is said once too, and the state line carries what the check read.
        self.assertIn("line = ButtonOffTarget(sort, box, gap);", judge)
        self.assertIn('" off beside Sort; F4 still works"', self.header)
        self.assertIn('"stashmoveall: button - placed beside Sort, box "', self.header)
        for field in ('" button_place="', '" button_box="', '" button_extents="', '" button_makes="', '" button_step="'):
            self.assertIn(field, self.header, field)
        for body in (function_body(header, "std::string ButtonOffTarget("), judge):
            self.assertNotIn("TurnOffForSession", body)
            self.assertNotIn("SetEnabled", body)
        # No x or y is written to the node: only its text and Sort's look.
        self.assertEqual(self.button_block().count("variable_instance_set"), 2)
        for var in ('"x"', '"y"'):
            self.assertNotIn(var, self.body("static ForgePact::StashMoveButtonLook SmaButtonLook("), var)
        origin = function_body(HEADER.read_text(encoding="utf-8").replace("\r\n", "\n"), "static bool ButtonOrigin(")
        self.assertIn("x = sort.left - gap - node.right;", origin)
        self.assertIn("y = (sort.top + sort.bottom) / 2 - (node.down - node.up) / 2;", origin)
        self.assertIn("static constexpr double kSmaButtonGap = 8.0;", self.button_block())
        self.assertIn("static constexpr double kButtonTolerance = 1.0;", self.header)

    def test_button_takes_sorts_look_read_from_sort_by_name(self):
        # Owner scope, 2026-09-30: the button has the Sort Tab button's look
        # and size. The variables that carry a node's sprite and size are read
        # off the Sort node by name each time and written onto the mod's own
        # node as read - never a sprite named, looked up or sized here, never
        # a routine called for it - then read back off both, and the core is
        # told whether the look took. A look that did not take keeps the node.
        block = self.button_block()
        self.assertIn('static constexpr const char* kSmaLookVars[] = { "sprite_index", "image_xscale", "image_yscale" };',
                      block)
        look = self.body("static ForgePact::StashMoveButtonLook SmaButtonLook(")
        self.assertIn("for (const char* var : kSmaLookVars) {", look)
        self.assertIn('const RValue v = g_Yytk->CallBuiltin("variable_instance_get", { sort, RValue(var) });', look)
        self.assertIn('if (copy) g_Yytk->CallBuiltin("variable_instance_set", { g_SmaButton, RValue(var), v });', look)
        self.assertIn("got = MenuLayoutRead(g_SmaButton, var);", look)
        order = [look.index(t) for t in ('"variable_instance_get", { sort,', "if (!std::isfinite(want))",
                                         "if (copy)", "got = MenuLayoutRead(g_SmaButton, var);")]
        self.assertEqual(order, sorted(order))
        for word in ("ForgePact::StashMoveButtonLook::Unread", "ForgePact::StashMoveButtonLook::Differs",
                     "ForgePact::StashMoveButtonLook::Sort"):
            self.assertIn(word, look, word)
        for word in ("SmaCall(", "SmaButtonRemove", "asset_get_index", "_spr", "sprite_get_", "UiSetNodeScale",
                     "UI_Layout_Apply_Sprite", "TurnOffForSession", "SetEnabled"):
            self.assertNotIn(word, look, word)
        # No sprite constant anywhere in the block; the one asset looked up is
        # the node's object.
        self.assertNotIn("_spr", block)
        self.assertNotIn("sprite_get_", block)
        self.assertEqual(block.count('"asset_get_index"'), 1)
        # Copied once per node made, after the label and after the core was
        # told of the make; re-read, not re-written, each step the node is
        # checked, before the check.
        create = self.body("static void SmaButtonCreate(")
        order = [create.index(t) for t in ("SmaButtonMake(stash, window, x, y, why)", "mod.NoteButtonMade(false);",
                                           "mod.NoteButtonLook(SmaButtonLook(sort, true));")]
        self.assertEqual(order, sorted(order))
        check = self.body("static void SmaButtonCheck(")
        self.assertLess(check.index("if (mod.ButtonLookWanted()) mod.NoteButtonLook(SmaButtonLook(sort, false));"),
                        check.index("mod.ButtonCheck("))
        self.assertLess(check.index("mod.NoteButtonMade(true);"), check.index("mod.NoteButtonLook(SmaButtonLook(sort, true));"))
        self.assertEqual(block.count("SmaButtonLook(sort, true)"), 2)
        make = self.body("static bool SmaButtonMake(")
        self.assertNotIn("SmaButtonLook", make)
        # The core keeps the look judged: a read after the node was decided
        # changes nothing.
        noted = function_body(HEADER.read_text(encoding="utf-8").replace("\r\n", "\n"),
                              "void NoteButtonLook(StashMoveButtonLook look)")
        self.assertIn("if (m_ButtonMakes == 0 || m_ButtonChecked) return;", noted)

    def test_button_size_is_judged_against_sorts_on_the_settled_box(self):
        # The size rule: width and height each within kButtonTolerance of
        # Sort's, an unread box never. Judged with the look on the settled
        # read the place was judged on, for a node kept (on target, or still
        # off) and never for one about to be remade; either off is kept,
        # never a remake for it, said once a session each on its own line,
        # and the mod stays on.
        header = HEADER.read_text(encoding="utf-8").replace("\r\n", "\n")
        sized = strip_comments(function_body(header, "static bool ButtonSortSized("))
        self.assertIn("if (!BoxReads(sort) || !BoxReads(node)) return false;", sized)
        self.assertEqual(sized.count("<= kButtonTolerance"), 2)
        judge = strip_comments(function_body(header, "StashMoveButtonCheck ButtonCheck("))
        self.assertEqual(judge.count("JudgeLook(sort, box, line);"), 2)
        self.assertLess(judge.index("if (!settled) {"), judge.index("JudgeLook(sort, box, line);"))
        self.assertLess(judge.index('NotePlace("on", box, &e);'), judge.index("JudgeLook(sort, box, line);"))
        remake = judge[judge.index("if (!m_ButtonRemakeAsked"):judge.index("return StashMoveButtonCheck::Remake;")]
        self.assertNotIn("JudgeLook", remake)
        self.assertLess(judge.index("line = ButtonOffTarget(sort, box, gap);"), judge.rindex("JudgeLook(sort, box, line);"))
        look = strip_comments(function_body(header, "void JudgeLook("))
        self.assertIn("if (!ButtonSortSized(sort, box))", look)
        for flag in ("m_ButtonSizeSaid", "m_ButtonLookSaid", "m_ButtonLookUnreadSaid"):
            self.assertIn(f"SayButtonOff({flag},", look, flag)
        self.assertIn('line += (line.empty() ? "" : "\\n") + said;', look)
        for word in ("TurnOffForSession", "SetEnabled", "Remake", "m_ButtonRemakeAsked"):
            self.assertNotIn(word, look, word)
        for field in ('" button_look="', '" button_size="'):
            self.assertIn(field, self.header, field)

    def button_block(self):
        start, end = self.plugin.index(BUTTON_BLOCK[0]), self.plugin.index(BUTTON_BLOCK[1])
        return strip_comments(self.plugin[start:end])

    def test_button_node_is_created_and_removed_by_name_and_only_while_on(self):
        block = self.button_block()
        # It ships, whole, in the player build.
        start, end = self.plugin.index(BUTTON_BLOCK[0]), self.plugin.index(BUTTON_BLOCK[1])
        self.assertIn(self.plugin[start:end], self.shipped)
        self.assertNotIn("FORGEPACT_RELEASE", self.plugin[start:end])
        # The two node routines, each an SDK constant, and the only routines here.
        consts = re.findall(r"static constexpr TalentAllocScript (kSma\w+)\{ HeroSiege::Scripts::gml_Script_(\w+),\s*"
                            r"SdkShortScriptName\(HeroSiege::Scripts::gml_Script_(\w+)\) \};", block)
        self.assertEqual({c[1] for c in consts}, {"UiCreateNode", "UiRemoveNode"})
        for name, routine, short in consts:
            self.assertEqual(routine, short, name)
        self.assertEqual(set(re.findall(r"SmaCall\((\w+),", block)), {"kSmaUiCreateNode", "kSmaUiRemoveNode"})
        self.assertNotIn('"gml_Script_', block)
        self.assertNotIn('"UI_', block)
        # Made under the stash window, beside Sort found by its call-stack
        # name (sortActivation: its text reads Sort Tab, never searched).
        ensure = self.body("static void SmaButtonEnsure(")
        self.assertIn("CmInstance(HeroSiege::Objects::GameObject::UI_Stash_obj, window, stash)", ensure)
        self.assertIn('StashVerbByString(HeroSiege::Objects::GameObject::UI_Button_Small_obj, "uiNodeCallstack", kSmaSortCallstack, sort, sortInst)', ensure)
        self.assertIn('static constexpr const char* kSmaSortCallstack = "InventorySort";', block)
        self.assertNotIn('"Sort', block)
        self.assertIn("mod.ButtonStep(stashListed, sortListed, sortVisible, g_SmaButtonHeld)", ensure)
        # A Sort that never shows is said once, not silence.
        self.assertIn('"stashmoveall: button - not shown: no visible Sort button (uiNodeCallstack "', ensure)
        self.assertIn("if (s_NoSort >= 3 && !s_NoSortSaid && mod.IsEnabled()) {", ensure)
        create = self.body("static bool SmaButtonMake(")
        self.assertIn("SmaCall(kSmaUiCreateNode, stash, stash,", create)
        self.assertIn("object, RValue(), RValue(std::string(kSmaButtonCallstack)) }, node);", create)
        self.assertIn('static constexpr const char* kSmaButtonCallstack = "ForgePactMoveAll";', block)
        self.assertIn('static constexpr const char* kSmaButtonText = "Move all";', block)
        # A refusal is the core's once-only line; the mod stays on.
        self.assertIn("mod.ButtonRefused(why)", self.body("static void SmaButtonCreate("))
        self.assertIn("mod.ButtonRefused(why)", self.body("static void SmaButtonCheck("))
        self.assertNotIn("TurnOffForSession", block)
        self.assertNotIn("SetEnabled", block)
        # The writes: the label of the node the mod made, read back, and
        # Sort's look on that same node (test_button_takes_sorts_look_read_
        # from_sort_by_name).
        self.assertEqual(block.count("variable_instance_set"), 2)
        self.assertIn('"variable_instance_set", { node, RValue("text"), RValue(std::string(kSmaButtonText)) }', create)
        self.assertIn('"variable_instance_set", { g_SmaButton, RValue(var), v }',
                      self.body("static ForgePact::StashMoveButtonLook SmaButtonLook("))
        # Removal: UiRemoveNode with the owner window as self while it is
        # listed, instance_destroy on the mod's own node only when it is not.
        remove = self.body("static void SmaButtonRemove(")
        self.assertIn("SmaCall(kSmaUiRemoveNode, owner, owner, { g_SmaButton }, res);", remove)
        self.assertEqual(block.count('"instance_destroy"'), 1)
        self.assertLess(remove.index("if (owner) {"), remove.index('"instance_destroy"'))
        self.assertLess(remove.index("if (!SmaButtonIsOurs(g_SmaButton))"), remove.index("SmaCall(kSmaUiRemoveNode"))
        ours = self.body("static bool SmaButtonIsOurs(")
        self.assertIn("v.ToString() == kSmaButtonCallstack", ours)
        # Only while on: the tick's ensure step at most every tenth frame, and
        # the switch turned off, a loss, and the off tick each remove it.
        tick = self.body("static void StashMoveAllTick(")
        self.assertIn("if ((s_Frame++ % kSmaButtonEveryFrames) == 0) SmaButtonEnsure();", tick)
        self.assertIn("static constexpr unsigned kSmaButtonEveryFrames = 10;", self.code)
        self.assertLess(tick.index("if (!mod.IsEnabled())"), tick.index("SmaButtonEnsure()"))
        cmd = self.body("static void StashMoveAllCommand(")
        off = cmd[cmd.index('if (arg == "0" || arg == "off") {'):]
        self.assertLess(off.index("mod.SetEnabled(false);"), off.index("SmaButtonRemove();"))
        self.assertIn("if (t.stopped) SmaButtonRemove();", self.body("static void StashMoveAllRun("))
        self.assertIn("if (res.outcome == ForgePact::StashMoveOutcome::Unconfirmed) SmaButtonRemove();",
                      self.body("static void StashMoveCommand("))
        # The core: the node exists exactly while on, the stash and Sort listed.
        step = function_body(HEADER.read_text(encoding="utf-8").replace("\r\n", "\n"), "StashMoveButtonStep ButtonStep(")
        self.assertIn("const bool wanted = IsEnabled() && stashListed && sortListed && sortVisible;", step)

    def test_button_binds_no_activation_and_installs_no_script_hook(self):
        # Live 1f: a click on a node bound to a game script ran that script
        # with the node as self and ended the game. The player build binds
        # nothing and hooks nothing for the button, whichever route stands.
        block = self.button_block()
        for word in ("UiSetActivationFunc", "HookOneScript", "HookBuiltin", "MmCreateHook", "activationFunc",
                     "event_perform", "Rva", "GetModuleHandle", "reinterpret_cast"):
            self.assertNotIn(word, block, word)
        release = strip_comments(self.shipped)
        self.assertNotIn("gml_Script_UiSetActivationFunc", release)
        self.assertNotIn("kSmaProbeBind", release)
        # UiCreateNode's fourth argument, the activation, is undefined.
        create = self.body("static bool SmaButtonMake(")
        self.assertRegex(create, r"SmaCall\(kSmaUiCreateNode, stash, stash,\s*\{ RValue\(x\), "
                                 r"RValue\(y\), object, RValue\(\), RValue\(std::string\(kSmaButtonCallstack\)\) \}, node\);")
        # Positive control: the research build's probe still carries the bind.
        self.assertIn("SmaCall(kSmaProbeBind, stash, stash, { node, script }, res);", strip_comments(self.plugin))

    def test_button_press_is_the_frame_poll_inside_the_node_bbox(self):
        poll = self.body("static bool SmaButtonPoll(")
        self.assertTrue(poll.lstrip("{ \n").startswith("if (!g_SmaButtonHeld) return false;"))
        order = [poll.index(t) for t in ('CallBuiltin("mouse_check_button_pressed", { RValue(kSmaMbLeft) })',
                                         'CallBuiltin("device_mouse_x_to_gui", { RValue(0.0) })',
                                         'CallBuiltin("device_mouse_y_to_gui", { RValue(0.0) })',
                                         'MenuLayoutRead(g_SmaButton, "bbox_left")',
                                         "ForgePact::StashMoveAllMod::PressInNode(mx, my, l, t, r, b)",
                                         "NoteButtonPress()")]
        self.assertEqual(order, sorted(order))
        for side in ("bbox_left", "bbox_top", "bbox_right", "bbox_bottom"):
            self.assertIn(f'MenuLayoutRead(g_SmaButton, "{side}")', poll)
        # The press is handed to the core, never acted on in the poll.
        for word in ("StashMoveAllRun", "SmaMoveOne", "SmaCall", "KeyEdge", "TakeButtonPress"):
            self.assertNotIn(word, poll, word)
        tick = self.body("static void StashMoveAllTick(")
        self.assertLess(tick.index("const bool pressed = SmaButtonPoll();"), tick.index("if (down || pressed) {"))
        self.assertLess(tick.index("if (down || pressed) {"), tick.index("mod.TakeButtonPress(fg, listed, modifier)"))
        self.assertIn("static bool PressInNode(double x, double y, double left, double top, double right, double bottom)",
                      self.header)
        # The guard is the key's: off, the game not in front, the stash not
        # listed, or a modifier held drops the press, counted with its reason.
        self.assertIn('const char* drop = !IsEnabled() ? "off" : !foreground ? "fg" : !stashListed ? "stash" : '
                      'modifier ? "modifier" : nullptr;', self.header)
        # None of the research probe's strings reach the player build (the
        # built DLL is checked for them too, criterion ship-strings).
        release = strip_comments(self.shipped)
        for word in ('"poll_presses', '"stashmoveall probe', '"stashmoveall probe copy: "'):
            self.assertNotIn(word, release, word)

    def test_button_press_path_counts_where_each_press_went(self):
        # Review of Phase C (instrument blindness): the player build carries
        # no probe, so a click on the button that moved nothing must say why.
        # Every left press the poll reads while it holds a node is counted
        # once, inside or as a miss, a poll that threw apart, and a recorded
        # press the guard drops with its reason; the state line prints them.
        poll = self.body("static bool SmaButtonPoll(")
        miss = poll.index("if (!ForgePact::StashMoveAllMod::PressInNode(mx, my, l, t, r, b)) {")
        self.assertLess(miss, poll.index("mod.NoteButtonMiss(ForgePact::StashMoveAllMod::PressReads(mx, my, l, t, r, b));"))
        self.assertLess(poll.index("NoteButtonMiss("), poll.index("mod.NoteButtonPress();"))
        self.assertIn("catch (...) { mod.NoteButtonPollError(); return false; }", poll)
        # No press is counted before mouse_check_button_pressed says one was.
        self.assertLess(poll.index('CallBuiltin("mouse_check_button_pressed"'), poll.index("NoteButtonMiss("))
        # The node held or not, as the adapter holds it.
        self.assertIn("mod.NoteButtonHeld(true);", self.body("static bool SmaButtonMake("))
        self.assertIn("NoteButtonHeld(false);", self.body("static void SmaButtonForget("))
        # The core's side: each counter, and the state line carrying them
        # after the key, the state word still first for the panel.
        for field in ('" button="', '" presses="', '" in_node="', '" outside="', '" unread="', '" errors="',
                      '" taken="', '" dropped="', '" last_drop="'):
            self.assertIn(field, self.header, field)
        self.assertIn('return std::string("stashmoveall: state=") + (IsEnabled() ? "on" : "off") + " key=F4" '
                      '+ ButtonFields();', self.header)
        take = function_body(self.header, "bool TakeButtonPress(bool foreground, bool stashListed, bool modifier)")
        self.assertIn("++m_PressesDropped;", take)
        self.assertIn("m_LastDrop.store(drop);", take)
        self.assertIn("++m_PressesTaken;", take)
        # The bare verb prints the state line, so the operator reads it after
        # a click; the panel still reads the state word from it.
        cmd = self.body("static void StashMoveAllCommand(")
        self.assertRegex(cmd, r'if \(arg == "run"\) \{ StashMoveAllRun\(\); return; \}\s*Out\(mod\.StateLine\(\)\);')
        import tempfile
        from unittest import mock
        import forgepact  # noqa: E402
        with tempfile.TemporaryDirectory() as d, mock.patch.object(forgepact, "ipc_dir", return_value=Path(d)):
            (Path(d) / "out.txt").write_bytes(
                b"stashmoveall: state=on key=F4 button=held presses=2 in_node=1 outside=1 unread=0 errors=0"
                b" taken=0 dropped=1 last_drop=fg\r\n")
            self.assertEqual(forgepact.stash_move_all_session({}), "on")
        # Live procedure 2's button-press step reads it after the click.
        doc = DOC.read_text(encoding="utf-8")
        s = doc.index("\n## Ship design\n")
        e = doc.find("\n## ", s + 1)
        self.assertIn("last_drop=", doc[s:e if e >= 0 else len(doc)])

    # ---- the Socketable tab (socketMergeRoute, Live 1f and 1g) ---------------

    def test_socket_tab_reads_its_grid_nodes_not_the_slot_array(self):
        # Live 1e finding 2: Controller_obj.stashSocketItemSlot is not the
        # container. The tab is read as the set of StashSocketGrid nodes,
        # each cell's key on map 9, and the merge is handed the node's own
        # one-cell nodeGrid with 9, 2, the item, its count and 8.
        self.assertIn("bool socketMerge = true;", self.header)
        self.assertIn("bool socketNew = false;", self.header)
        # Live 1f measured a one-unit merge only; #131 turns the whole-count
        # merge on (Live procedure 3's socket-whole confirms it), and the flag
        # still decides it, so turning it off makes a stack a planned skip.
        self.assertIn("bool socketWholeStackMerge = true;", self.header)
        self.assertIn("else if (many && socket && !routes.socketWholeStackMerge)", self.header)
        # One stack per kind: a full one is a skip, never a second stack.
        self.assertIn('if (!stacks.counts.empty()) item.refusal = "its stack on the shown tab is full";', self.header)
        self.assertIn('static constexpr const char* kSmaSocketGrid = "StashSocketGrid";', self.code)
        self.assertNotIn("stashSocketItemSlot", self.code)
        self.assertNotIn("kCmSocketTabVar", self.code)
        node = self.body("static int SmaSocketNode(")
        self.assertIn("TalentAllocInstances(HeroSiege::Objects::GameObject::UI_Inventory_Grid_obj)", node)
        self.assertIn("v.ToString() != kSmaSocketGrid", node)
        self.assertIn('CmArrayVar(h, "nodeGrid", grid)', node)
        self.assertIn("SmaStackSum(s, grid, cls, base)", node)
        self.assertIn("CmMapItem(s.map9", self.body("static bool SmaResolve("))
        one = self.body("static ForgePact::StashMoveResult SmaMoveOne(")
        self.assertIn("const int held = tabStill && socket ? SmaSocketNode(s, cls, base, node, arr) : -1;", one)
        self.assertIn("RValue(socket ? kSmaSocketStackA5 : kSmaStackA5)", one)
        self.assertIn("static constexpr double kSmaStackA5 = 0.0, kSmaSocketStackA5 = 8.0;", self.code)
        # No ValidateItem first, as the measured merge ran none; no placement
        # room read there (socketRoute new: not-observed).
        self.assertIn("if (!materials && !socket) c = SmaCall(kSmaValidate, s.bag, s.bag, { item }, res);", one)
        self.assertIn("if (arrRead && cellRoute && !socket) room = Mod::Room(", one)
        # The re-reads, the merge's own and the core's, are of the node handed.
        self.assertIn("const int64_t after = SmaReread(s, plan.stashTab, node, now) ? SmaStackSum(s, now, cls, base) : -1;", one)
        self.assertIn("const bool nowRead = r.shownTabChanged == 0 && SmaReread(s, plan.stashTab, node, now);", one)
        reread = self.body("static bool SmaReread(")
        self.assertIn('if (node.m_Kind != VALUE_UNDEFINED) return CmArrayVar(node, "nodeGrid", cells);', reread)
        scene = self.body("static bool SmaReadScene(")
        self.assertIn("const int held = SmaSocketNode(s, cls, base, node, cells);", scene)
        # The source is the bag's Socket view only (the core's rule).
        plan = function_body(HEADER.read_text(encoding="utf-8").replace("\r\n", "\n"), "static StashMovePlan PlanWith(")
        self.assertIn("const bool fits = page ? tab != StashMoveTab::Socketable", plan)

    # ---- the panel ------------------------------------------------------------

    def test_panel_toggle_defaults_off_and_emits_stashmoveall(self):
        import forgepact  # noqa: E402  (only here: the other tests need no hs_game_sdk)
        self.assertIs(forgepact.DEFAULTS["mod_stash_move_all"], False)
        cfg = {k: v for k, v in forgepact.DEFAULTS.items() if k != "game_exe"}
        self.assertNotIn("stashmoveall 1", forgepact.build_cmds(dict(cfg)))
        on = dict(cfg, mod_stash_move_all=True)
        self.assertEqual(forgepact.build_cmds(on).count("stashmoveall 1"), 1)
        source = (ROOT / "src" / "forgepact.py").read_text(encoding="utf-8")
        self.assertIn('send_cmds([f"stashmoveall {1 if cfg[\'mod_stash_move_all\'] else 0}"], cfg)', source)
        # One of /api/set's live booleans (its place in that tuple is free).
        self.assertRegex(source, r'elif key in \("density_on", "auto_apply", [^)]*"mod_stash_move_all", ')
        mods = panel_file("tabs/Mods.svelte")
        row = mods[mods.index('id="mod_stash_move_all"') - 700:mods.index('id="msmaval"')]
        self.assertIn("Move all into the stash<br>", row)
        self.assertLess(mods.index('id="mod_craft_mats"'), mods.index('id="mod_stash_move_all"'))
        js = panel_file("panel.js")
        self.assertIn("document.getElementById('mod_stash_move_all').onchange=async(e)=>{", js)
        self.assertIn("{key:'mod_stash_move_all',value:e.target.checked}", js)
        self.assertIn("'mod_stash_move_all'", panel_file("enabled-mods.js"))

    def test_panel_text_is_player_facing_and_short(self):
        mods = panel_file("tabs/Mods.svelte")
        m = re.search(r'Move all into the stash<br><span class="feature-description">([^<]*)</span>', mods)
        self.assertIsNotNone(m)
        text = m.group(1)
        self.assertLess(len(text), 300, text)
        self.assertIn("F4", text)
        self.assertIn("stay in your bag", text)
        self.assertIn("Off by default.", text)
        for word in DEV_WORDS:
            self.assertNotIn(word, text, word)

    # ---- the documents --------------------------------------------------------

    def test_readme_and_release_notes_say_what_happens_when_the_tab_fills(self):
        readme = README.read_text(encoding="utf-8")
        self.assertIn("**Move all into the stash**", readme)
        section = readme[readme.index("\n## Move all into the stash\n"):]
        nxt = section.find("\n## ", 1)
        section = section if nxt < 0 else section[:nxt]
        self.assertIn("F4", section)
        self.assertRegex(section, r"fill|full|room")
        self.assertRegex(section, SPILL)
        self.assertIn("Socketable", section)
        notes = NOTES.read_text(encoding="utf-8")
        self.assertIn("\n## New\n", notes)
        new = notes[notes.index("\n## New\n"):]
        self.assertIn("Move all", new)
        self.assertIn("F4", new)
        self.assertRegex(new, r"fill|full|room")
        self.assertRegex(new, SPILL)
        self.assertIn("Socketable", new)
        # #131: a stash stack holds up to 999, a stackable starts a new stack
        # when none of its kind has room, and a socketable stack now joins its
        # kind's one stack: both say so, and neither keeps the old skip.
        new_section = new[:new.find("\n## ", 1)]
        for text in (section, new_section):
            self.assertIn("999", text)
            self.assertRegex(text, r"new stack")
            self.assertNotIn("A stack of more than one socketable stays", text)
            self.assertNotIn("a socketable merge of more than one unit is not measured", text)
        self.assertIn("\n## How to update\n", notes)

    def test_readme_notes_and_panel_name_the_button(self):
        # The button, where it is, that F4 does the same, and what the
        # Socketable tab now does, in player words in all three places.
        readme = README.read_text(encoding="utf-8")
        row = next(l for l in readme.splitlines() if l.startswith("| **Move all into the stash** |"))
        for word in ("**Move all** button", "Sort", "F4", "Socketable"):
            self.assertIn(word, row, word)
        section = readme[readme.index("\n## Move all into the stash\n"):]
        nxt = section.find("\n## ", 1)
        section = section if nxt < 0 else section[:nxt]
        for word in ("**Move all** button", "**Sort**", "F4", "a new kind stays in the bag", "stashmoveall: button - "):
            self.assertIn(word, section, word)
        self.assertRegex(section, r"switch off takes it\s+away")
        notes = NOTES.read_text(encoding="utf-8")
        new = notes[notes.index("\n## New\n"):]
        new = new[:new.find("\n## ", 1)]
        self.assertRegex(new, r"Move all\*\* button")
        for word in ("**Sort**", "F4", "Socketable", "stays in your backpack"):
            self.assertIn(word, new, word)
        mods = panel_file("tabs/Mods.svelte")
        m = re.search(r'Move all into the stash<br><span class="feature-description">([^<]*)</span>', mods)
        self.assertIsNotNone(m)
        for word in ("Move all button", "Sort", "F4"):
            self.assertIn(word, m.group(1), word)

    def test_research_doc_has_ship_design_and_live_results(self):
        doc = DOC.read_text(encoding="utf-8")
        ship = doc.index("\n## Ship design\n")
        self.assertLess(doc.index("\n## Decision\n"), ship)
        section = doc[ship:]
        nxt = section.find("\n## ", 1)
        section = section if nxt < 0 else section[:nxt]
        for routine in ROUTINES:
            self.assertIn(routine, section, routine)
        for line in ("stashmoveall: moved", "stashmoveall: refused - ", "stashmoveall: off for this session - ",
                     "not observed"):
            self.assertIn(line, section, line)
        # Phase C: the button's routine and the Socketable tab's merge.
        for word in ("UiCreateNode", "UiRemoveNode", "ForgePactMoveAll", "InventorySort", "mouse_check_button_pressed",
                     "stashmoveall: button - ", "StashSocketGrid", "socketMergeRoute", "a new kind stays in the bag"):
            self.assertIn(word, section, word)
        self.assertIn("\n### Live 2 results\n", doc)
        self.assertLess(doc.index("\n### Live 1e results\n"), doc.index("\n### Live 2 results\n"))

    # ---- the Live 1f probe's own controls (research build) -------------------

    def test_probe_poll_and_detour_carry_their_own_controls(self):
        # The round-0 review of Live 1f's instrument: a poll that counts only
        # presses inside the node cannot tell a blind read or another
        # coordinate space from a miss, and a count of the bound script with
        # the node as self cannot tell a press from the game's own call.
        tick = self.body("static void SmaProbeTick(")
        self.assertIn("if (!g_SmaProbePollArmed) return;", tick)
        self.assertNotIn("if (g_SmaProbeNodeId.load() < 0) return;", tick)
        any_press = tick.index("InterlockedIncrement(&g_SmaProbePollAnyPresses);")
        self.assertLess(any_press, tick.index("InterlockedIncrement(&g_SmaProbePollSortPresses);"))
        self.assertLess(any_press, tick.index("InterlockedIncrement(&g_SmaProbePollPresses);"))
        self.assertIn("g_SmaProbeLastPress = ", tick)
        self.assertIn("g_SmaProbePollArmed = true;", self.body("static void SmaProbeSortCommand("))

        saw = self.body("static void SmaProbeSawCall(")
        self.assertIn("InterlockedIncrement(&g_SmaProbeRowCallsSelfNode);", saw)
        self.assertIn("g_SmaProbeWatch == label", saw)
        create = self.body("static void SmaProbeCreateCommand(")
        self.assertIn('"watch:"', create)
        self.assertIn("g_SmaProbeWatch = watchRow;", create)
        self.assertIn("SmaProbeLoopCheck(objIdx)", create)
        loop = self.body("static std::string SmaProbeLoopCheck(")
        for name in ("GameObject::UI_Hud_Talent_obj", "gml_Script_ControllerCheckInput", "object_is_ancestor",
                     "object_get_parent", "CONFOUND"):
            self.assertIn(name, loop)
        show = self.body("static void SmaProbeShowCommand(")
        for field in ("row_calls_self_node=", "poll_any_presses=", "poll_sort_presses=", "last_press=",
                      "sort_bbox=", "node_bbox="):
            self.assertIn(field, show)
        # Research build only: none of it reaches the player build.
        for name in ("SmaProbeTick", "SmaProbeSawCall", "SmaProbeLoopCheck", "g_SmaProbeRowCallsSelfNode"):
            self.assertNotIn(name, self.shipped)
        # Positive control: the research copy keeps them.
        self.assertIn("SmaProbeTick();", self.plugin)

    # ---- #131's label instrument: dump, diff, lookcopy (research build) ------

    def probe_block(self):
        start = self.plugin.index("// ---- stashmoveall probe: the in-game button's instrument")
        end = self.plugin.index("// ---- end stashmoveall probe")
        return self.plugin[start:end]

    def test_stash_probe_dump_diff_and_lookcopy_are_research_only(self):
        # Live procedure 5 (docs/stash-move-research.md): the static reading
        # could not name the member that places a node's label, so the
        # research build dumps two nodes by name, diffs them, and tries the
        # copy on the mod's own node. None of it is a player command.
        block = self.probe_block()
        start = self.plugin.index("// ---- stashmoveall probe: the in-game button's instrument")
        self.assertIn("#ifndef FORGEPACT_RELEASE", self.plugin[start - 40:start])
        dispatch = self.body("static bool SmaProbeCommand(")
        for sub, handler in (('"dump"', "SmaProbeDumpCommand(tok)"), ('"diff"', "SmaProbeDiffCommand(tok)"),
                             ('"lookcopy"', "SmaProbeLookCopyCommand(tok)")):
            self.assertIn(f"sub == {sub}", dispatch, sub)
            self.assertIn(handler, dispatch, handler)
        # `probe help`, and any word it does not know, fall to the usage line.
        self.assertRegex(dispatch, r'else if \(sub == "lookcopy"\) SmaProbeLookCopyCommand\(tok\);\s*'
                                   r'else usage\(\);\s*return true;\s*$')
        # The usage line (`probe help`, and any word it does not know) names
        # every subcommand, so a build that has them says so (the marker).
        usage = function_body(self.plugin, "static bool SmaProbeCommand(")
        for word in ("sort [id:<n>]", "create [", "remove", "show", "copy <template key on map 9> <count>",
                     "dump <label> id:<n>", "diff <a> <b>", "lookcopy id:<src> missing|changed", "help"):
            self.assertIn(word, usage, word)
        # Read by name, the instance checked with instance_exists before any read.
        cap = self.body("static bool SmaProbeDumpCapture(")
        self.assertLess(cap.index("SmaProbeExists(inst)"), cap.index('"variable_instance_get"'))
        self.assertLess(cap.index("SmaProbeExists(inst)"), cap.index('"variable_instance_get_names"'))
        for builtin in ("id", "object_index", "visible", "sprite_index", "image_index", "image_speed",
                        "image_blend", "image_alpha", "image_xscale", "image_yscale", "image_angle", "depth",
                        "x", "y", "bbox_left", "bbox_top", "bbox_right", "bbox_bottom"):
            self.assertIn(f'"{builtin}"', block[block.index("kSmaProbeDumpBuiltins[] = {"):], builtin)
        self.assertIn('"sprite_get_name"', cap)
        self.assertIn("kSmaProbeDumpKeep", self.body("static SmaProbeDump& SmaProbeStoreDump("))
        diff = self.body("static void SmaProbeDiffCommand(")
        for mark in ('"~ "', '"+ "', '"- "', '"changed="', '" added="', '" removed="'):
            self.assertIn(mark, diff, mark)
        # Reads only: dump and diff write nothing and call no routine.
        for name in ("static bool SmaProbeDumpCapture(", "static void SmaProbeDumpCommand(",
                     "static void SmaProbeDiffCommand("):
            body = self.body(name)
            for word in WRITES + ("SmaCall(", "CallBuiltinEx("):
                self.assertNotIn(word, body, (name, word))
        # restartprobe's own dump is untouched: its capture and diff are not
        # called from here, and its lines still name restartprobe.
        for word in ("RpDumpCapture(", "RpDumpDiff(", "RpStoreDump("):
            self.assertNotIn(word, strip_comments(block), word)
        self.assertIn('Out("restartprobe dump diff " + da.kind', function_body(self.plugin, "static void RpDumpDiff("))
        # Nothing reaches the player build, the player command set or the frame path.
        release = strip_comments(self.shipped)
        for word in ("SmaProbeDumpCommand", "SmaProbeDiffCommand", "SmaProbeLookCopyCommand", "SmaProbeDumpCapture",
                     "kSmaProbeLookCopyNever", '"lookcopy"', "probe dump", "probe diff", "lookcopy"):
            self.assertNotIn(word, release, word)
        run = function_body(self.plugin, "static void RunCommand(const std::string& line)")
        commands = run[run.index("kPlayerCommands = {"):]
        commands = commands[:commands.index("};")]
        for word in ('"probe', '"dump', '"diff', '"lookcopy'):
            self.assertNotIn(word, commands, word)
        frame = function_body(self.plugin, "void FrameCallback(FWFrame& FrameContext)")
        for word in ("SmaProbeDump", "SmaProbeLookCopy", "SmaProbeDiff", "lookcopy"):
            self.assertNotIn(word, frame, word)
        # Positive control: the research copy carries them.
        self.assertIn("static void SmaProbeLookCopyCommand(", self.plugin)

    def test_stash_probe_lookcopy_writes_only_the_mods_own_node(self):
        look = self.body("static void SmaProbeLookCopyCommand(")
        # Refused, with nothing written, when the mod holds no node: the one
        # write goes to the node SmaButtonIsOurs accepts, checked first.
        self.assertEqual(look.count('"variable_instance_set"'), 1)
        self.assertIn('CallBuiltin("variable_instance_set", { g_SmaButton, RValue(name), v });', look)
        self.assertLess(look.index("SmaButtonIsOurs(g_SmaButton)"), look.index('"variable_instance_set"'))
        self.assertIn("refused - the mod holds no Move all node", look)
        # The source is checked with instance_exists before any read of it.
        self.assertLess(look.index("SmaProbeExists(src)"), look.index("SmaProbeDumpCapture(src"))
        # Never the members that say what the node is, where it is and what it does.
        never = self.probe_block()
        never = never[never.index("kSmaProbeLookCopyNever[] = {"):]
        never = never[:never.index("};")]
        for name in ("id", "object_index", "x", "y", "xstart", "ystart", "xprevious", "yprevious",
                     "bbox_left", "bbox_top", "bbox_right", "bbox_bottom", "uiNodeCallstack",
                     "activationFunc", "activationArgs", "text", "visible", "enabled"):
            self.assertIn(f'"{name}"', never, name)
        self.assertLess(look.index("SmaProbeLookCopyExcluded(name)"), look.index('"variable_instance_set"'))
        # Only a number, bool, string or asset is written: the kind is read off
        # the value itself, a handle is an asset only when the runtime names
        # an asset type, and a reference, struct, array, method or undefined
        # never is.
        self.assertLess(look.index("SmaProbeWritable(kind)"), look.index('"variable_instance_set"'))
        writable = self.body("static bool SmaProbeWritable(")
        self.assertEqual(set(re.findall(r"SmaProbeKind::(\w+)", writable)), {"Number", "Bool", "String", "Asset"})
        kind = self.body("static SmaProbeKind SmaProbeKindOf(")
        for word in ('"is_method"', '"is_struct"', "VALUE_ARRAY", "VALUE_UNDEFINED", "VALUE_REF",
                     "SmaProbeKind::Reference", "SmaProbeKind::Method", "SmaProbeKind::Struct",
                     "SmaProbeKind::Array", "SmaProbeKind::Undefined", "kSmaProbeAssetTypes"):
            self.assertIn(word, kind, word)
        # Each write is read back off the node and printed beside what was written.
        self.assertLess(look.index('"variable_instance_set"'),
                        look.index('CallBuiltin("variable_instance_get", { g_SmaButton, RValue(name) })'))
        self.assertIn('"  wrote " + name + "=" + want + " read back " + got', look)
        # No routine, no hook, no other instance written.
        for word in ("SmaCall(", "CallBuiltinEx(", "HookOneScript", "instance_create", "instance_destroy",
                     "variable_struct_set", "variable_global_set"):
            self.assertNotIn(word, look, word)
        self.assertIn('"missing"', look)
        self.assertIn('"changed"', look)


if __name__ == "__main__":
    unittest.main()
