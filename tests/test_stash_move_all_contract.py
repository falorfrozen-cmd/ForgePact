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
NOTES_VERSION = "2.0.1"
NOTES = ROOT / f"release-notes-v{NOTES_VERSION}.md"

BLOCK = ("// ---- stashmoveall, stashmove: Move all into the stash (ForgePact #68)",
         "// ---- end stashmoveall, stashmove")
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
        # Off: nothing is read, not even the key.
        gate = tick.index("if (!mod.IsEnabled()) { s_WasOn = false; return; }")
        self.assertLess(gate, tick.index("GetAsyncKeyState(kSmaHotkey)"))
        self.assertEqual(tick.count("GetAsyncKeyState"), 1)
        # The foreground and the stash window are asked only while the key is down.
        down = tick.index("if (down) {")
        self.assertLess(down, tick.index("SmaGameInForeground()"))
        self.assertLess(down, tick.index("HeroSiege::Objects::GameObject::UI_Stash_obj"))
        self.assertIn("listed = fg && CmInstance(", tick)
        self.assertIn("if (mod.KeyEdge(down, fg, listed, modifier)) StashMoveAllRun();", tick)
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
        undo = one[one.index("out.outcome == ForgePact::StashMoveOutcome::Unconfirmed && cellRoute"):]
        self.assertIn("r.sourceHasKey == 1 && CmCellsHold(now, key) == 1", undo)
        self.assertIn("SmaCall(kSmaRemove, s.sg, s.sg, { now, RValue(key) }, res);", undo)

    # ---- the round-2 review's fixes --------------------------------------------

    def test_cell_route_rereads_the_stack_sum_before_stash_add_to_stack(self):
        # The route is decided at the point of use: before the item's first
        # call, a stackable's sum is re-read on the shown array and the core's
        # RouteAtUse decides, whatever the plan said; the branch taken is the
        # one it answers, and the outcome is decided on that item.
        one = self.body("static ForgePact::StashMoveResult SmaMoveOne(")
        reread = one.index("before = arrRead ? SmaStackSum(s, arr, cls, base) : -1;")
        at_use = one.index("use = Mod::RouteAtUse(use, plan.stashTab, before);")
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
        self.assertIn("RouteFor(TabOf(stashTab), planned.cell, read, stackSumNow > 0, routes, item);", at)
        self.assertIn("RouteFor(tab, c, c.destinationStackRead, c.destinationHasStack, routes, item);", self.header)

    def test_key_edge_ignores_a_held_modifier(self):
        # Alt+F4 closes the game: any held Alt, Ctrl or Shift is no edge.
        held = self.body("static bool SmaModifierHeld(")
        for key in ("VK_MENU", "VK_CONTROL", "VK_SHIFT"):
            self.assertIn(key, held, key)
        self.assertIn("GetAsyncKeyState", held)
        tick = self.body("static void StashMoveAllTick(")
        down = tick.index("if (down) {")
        self.assertLess(down, tick.index("modifier = SmaModifierHeld();"))
        self.assertIn("if (mod.KeyEdge(down, fg, listed, modifier)) StashMoveAllRun();", tick)
        self.assertIn("bool KeyEdge(bool down, bool foreground, bool stashListed, bool modifier)", self.header)
        self.assertIn("return press && foreground && stashListed && !modifier;", self.header)

    def test_off_after_loss_is_reported_distinctly(self):
        # After a loss the state line says so, with the reason; turning on
        # again answers with the reason and stays off; every switch and a loss
        # print the state line, whose last copy the panel reads.
        self.assertIn('"stashmoveall: state=off-for-this-session reason=" + m_OffReason', self.header)
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
        self.assertIn('"mod_far_sleep", "mod_stash_move_all", "mod_craft_mats"', source)
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
        self.assertIn("\n## How to update\n", notes)

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
        self.assertIn("\n### Live 2 results\n", doc)
        self.assertLess(doc.index("\n### Live 1e results\n"), doc.index("\n### Live 2 results\n"))


if __name__ == "__main__":
    unittest.main()
