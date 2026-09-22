"""Contract tests for crafting from the stash's material tab (ForgePact #14).

How the game counts and consumes a recipe's materials is not measured yet
(docs/crafting-materials-research.md, Phase 0 done, Phase 1 pending). This
stage ships nothing a player can reach: a research-build instrument
(`craftprobe`), a game-independent decision core (CraftMatsMod.hpp, whose
behaviour test_craft_mats_behavior.py pins) wired only to a `craftmats` switch
that changes nothing yet, and the research document the live session fills in.
These tests pin the shape of all three, so the instrument cannot quietly reach
the player build, go blind, or write where it should refuse, and so the switch
cannot start doing work on the frame path before a mechanism is chosen.
"""
import importlib.util
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PLUGIN = ROOT / "plugin" / "ModuleMain.cpp"
HEADER = ROOT / "plugin" / "include" / "ForgePact" / "CraftMatsMod.hpp"
DOC = ROOT / "docs" / "crafting-materials-research.md"
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

BLOCK_START = "// ---- craftprobe: the crafting-materials Phase 0 instrument (issue #14)"
BLOCK_END = "#endif // FORGEPACT_RELEASE (craftprobe)"


def collapse(text):
    return " ".join(text.split())


class CraftMatsContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.plugin = PLUGIN.read_text(encoding="utf-8").replace("\r\n", "\n")
        cls.header = HEADER.read_text(encoding="utf-8").replace("\r\n", "\n")
        cls.doc = DOC.read_text(encoding="utf-8").replace("\r\n", "\n")
        cls.sdk = SDK_SCRIPTS.read_text(encoding="utf-8")
        cls.block = cls.plugin[cls.plugin.index(BLOCK_START):cls.plugin.index(BLOCK_END)]

        table = cls.plugin[cls.plugin.index("#define CRAFTPROBE_TARGETS(X)"):]
        table = table[:table.index("#define CP_DEFINE_DETOUR")]
        cls.rows = re.findall(r'X\((\w+),\s*"([^"]+)",\s*(\w+)\)', table)

    def body(self, signature):
        return strip_comments(function_body(self.plugin, signature))

    def runtime_name(self, constant):
        match = re.search(
            r'inline constexpr std::string_view ' + re.escape(constant) + r' = "([^"]+)";', self.sdk)
        self.assertIsNotNone(match, f"{constant} is not an hs-game-sdk Scripts constant")
        return match.group(1)

    def player_commands(self):
        start = self.plugin.index("kPlayerCommands = {")
        return re.findall(r'"([^"]+)"', self.plugin[start:self.plugin.index("};", start)])

    # ---- the instrument never reaches a player -------------------------------

    def test_craftprobe_is_research_build_only(self):
        shipped = strip_research_blocks(self.plugin)
        self.assertIn("craftprobe", self.plugin)
        self.assertNotIn("craftprobe", shipped)
        # Every craftprobe symbol is gone from the player build, not just the verb.
        for symbol in ("CpCommand", "CpDetour_", "g_CpTargets", "CpCall", "CpInstall", "CpResolve", "CpReader",
                       "CpCapture", "CRAFTPROBE_TARGETS"):
            self.assertIsNone(re.search(r"\b" + symbol, shipped), symbol + " reaches the player build")

    def test_craftprobe_absent_from_player_commands(self):
        entries = self.player_commands()
        self.assertNotIn("craftprobe", entries)
        self.assertEqual([e for e in entries if "craft" in e], [],
                         "no crafting verb is a player command before a mechanism is chosen and shipped")

    def test_craftprobe_dispatched_from_handle_craft_command(self):
        # RunCommand's else-if chain is at MSVC's nesting limit (C1061), so the
        # verbs live in their own handler, called after the menu-layout one.
        run = function_body(self.plugin, "static void RunCommand(const std::string& line)")
        self.assertIn("if (HandleMenuLayoutCommand(lc, rest)) return;\n    if (HandleCraftCommand(lc, rest)) return;", run)
        self.assertNotIn('"craftprobe"', run)
        self.assertNotIn('"craftmats"', run)
        handler = function_body(self.plugin, "static bool HandleCraftCommand(")
        self.assertIn('lc == "craftprobe"', handler)
        self.assertEqual(self.plugin.count('"craftprobe"'), 1)
        self.assertEqual(self.plugin.count('"craftmats"'), 1)
        shipped_handler = strip_research_blocks(handler)
        self.assertIn('lc == "craftmats"', shipped_handler)
        self.assertNotIn("craftprobe", shipped_handler)

    # ---- the target table ----------------------------------------------------

    def test_craftprobe_table_names_every_candidate(self):
        self.assertGreaterEqual(len(self.rows), 97)
        labels = [label for _, label, _ in self.rows]
        self.assertEqual(len(labels), len(set(labels)), "duplicate probe label")
        safes = [safe for safe, _, _ in self.rows]
        self.assertEqual(len(safes), len(set(safes)), "duplicate probe identifier")
        names = [self.runtime_name(constant) for _, _, constant in self.rows]
        # The enumerated static search (research doc, § Static search), by
        # group; the closures are pinned by the coverage test below.
        for anchor in (
            # crafting
            "GetCraftItemsAvailable", "CraftFindRecipeItems", "___struct___68@CraftFindRecipeItems@DefineCraftingFuncs",
            "DoCraftResult", "___struct___86@DoCraftResult@DefineCraftingFuncs", "CraftEditGrid",
            "___struct___87@CraftEditGrid@DefineCraftingFuncs", "CraftEditPlayerInventory",
            "___struct___88@CraftEditPlayerInventory@DefineCraftingFuncs", "UiACraftButton",
            "UiACraftMultiAmountConfirm", "GetCraftRecipeName", "s_CraftData", "s_CraftItem",
            # count / find / consume
            "CountInventoryItem", "FindInventoryItem", "FindInventoryItemData", "FindInventoryItemOperation",
            "___struct___152@FindInventoryItemOperation@InventoryFuncs", "InventoryStackHandler",
            "InventoryStackUpdateAndRemove", "___struct___161@InventoryStackUpdateAndRemove@InventoryFuncs",
            "InventoryStackUpdateAndEdit", "___struct___172@InventoryStackUpdateAndEdit@InventoryFuncs",
            "s_PendingStackOperation", "GetStackOpLocationFromGridType", "GetItemOwnerFromStackOpLocation",
            "GetItemOwnerStr", "ChangeItemOwner", "GetMaxStack", "IsItemTypeStackable",
            "UiAInventoryConsumeItemConfirm", "GetItemFromFingerprint", "ReturnItemTypeFromFingerPrint",
            # stash
            "StashAddToStack", "StashGridAddItem", "s_StashTabData", "GetStashMaxTabs", "LoadStash", "SaveStash",
            "___struct___357@SaveStash@SaveStashFunc", "___struct___359@SaveStash@SaveStashFunc",
            "___struct___361@SaveStash@SaveStashFunc", "___struct___363@SaveStash@SaveStashFunc",
            "___struct___364@SaveStash@SaveStashFunc", "GetStashMapPos", "UiAStashTabClick",
            "UiAStashMaterialTabClick", "UiAStashTabMaterialBuy", "UiAStashListBtnActivate",
            "anon@1820@UiAStashListBtnActivate@UiActivateFuncs", "UiDrawStashTabBuy",
            # the bag's materials tab and stack moves
            "UiAInventoryMaterialTabClick", "UiDrawInventoryMaterialTab", "InventoryGridAddToStack",
            "InventoryGridCanAddToStack", "InvGridClearItemNode", "GetItemPreferredGrid", "GridAddItem",
            "GetInventoryGridNode", "InventoryGridRemoveItem", "GridRemoveItem",
            # profile getters (capture only)
            "GetProfileInventoryData", "GetPlayerItemOwner", "GetInventoryArray", "GetPlayerProfileObj",
            # the positive control and the stash tab button's Step closure
            "CheckPlayerInteraction", "anon@503@gml_Object_UI_Button_Stash_Tab_obj_Step_0",
        ):
            self.assertIn("gml_Script_" + anchor, names)
        # Every row's runtime name is written down where the live session reads it.
        for label, name in zip(labels, names):
            self.assertIn(name, self.doc, f"row {label}: {name} missing from the research doc")
        # The table spells names through the SDK constant, never as a literal.
        entry = self.plugin[self.plugin.index("#define CP_ENTRY"):self.plugin.index("#undef CP_ENTRY")]
        self.assertIn("HeroSiege::Scripts::CONSTANT.data()", entry)
        table = self.plugin[self.plugin.index("#define CRAFTPROBE_TARGETS(X)"):self.plugin.index("#define CP_DEFINE_DETOUR")]
        self.assertNotIn('"gml_Script_', table)

    # The Phase 0a prospect session was blind because its table named closures
    # a game patch had renumbered. This table is derived from the SDK: every
    # closure the SDK names on these objects' Create events must be a row, so
    # the next regeneration fails here, by name, instead of in a live session.
    CLOSURE_OBJECTS = (
        "UI_Craft_obj", "UI_Journal_Crafting_obj", "UI_Craft_Recipe_List_Item_obj", "Craft_Cube_obj",
        "UI_Button_Journal_Craft_obj", "UI_Stash_obj", "UI_Stash_Tab_Bar_Container_obj", "Town_Stash_obj",
    )

    def test_craftprobe_table_covers_every_sdk_closure_of_its_objects(self):
        table = {constant for _, _, constant in self.rows}
        expected = []
        for constant, value in re.findall(r'std::string_view (\w+) = "([^"]+)";', self.sdk):
            if any("@gml_Object_" + obj + "_Create_0" in value for obj in self.CLOSURE_OBJECTS):
                expected.append(constant)
        # A scan that finds nothing would pass the check below vacuously.
        self.assertGreaterEqual(len(expected), 29, "SDK closure scan found too few constants - the regex is blind")
        missing = [c for c in expected if c not in table]
        self.assertEqual(missing, [], "SDK closures missing from CRAFTPROBE_TARGETS: " + ", ".join(missing))
        # Negative control: an object with no closure row must not be covered by accident.
        self.assertFalse(any("UI_Prospect_obj" in c for c in table))

    def test_craftprobe_resolver_refuses_address_outside_module_before_hook(self):
        resolver = self.body("static PVOID CpResolve(")
        self.assertIn("GetNamedRoutinePointer(t.runtimeName", resolver)
        self.assertIn("AddrIsExecutableInModule(GetModuleHandleA(nullptr), fn)", resolver)
        self.assertLess(resolver.index("AddrIsExecutableInModule"), resolver.index("return fn;"))
        install = self.body("static void CpInstall(")
        self.assertLess(install.index("CpResolve(t, why)"), install.index("MmCreateHook("))
        self.assertLess(install.index("if (!src)"), install.index("MmCreateHook("))
        self.assertNotIn("HookOneScriptTable", install)
        # The detour calls the trampoline, never re-enters the table.
        detour = self.plugin[self.plugin.index("#define CRAFTPROBE_DETOUR"):self.plugin.index("#define CRAFTPROBE_TARGETS")]
        self.assertIn("g_CpOrig_##SAFE(S, O, R, argc, A)", detour)
        self.assertIn("*t.origSlot = reinterpret_cast<PFUNC_YYGMLScript>(tramp);", install)
        # No address anywhere in the instrument but the one it resolved by name.
        code = strip_comments(self.block)
        self.assertNotIn("Rva", code)
        self.assertIsNone(re.search(r"GetModuleHandleA\(nullptr\)\s*\+", code))

    # ---- the one write -------------------------------------------------------

    def test_craftprobe_writes_are_confirm_gated(self):
        code = strip_comments(self.block)
        # The only calls into the game's own scripts are the one in `call` and
        # the fingerprint lookup that resolves its argument; nothing else in the
        # instrument writes an instance, a struct or an array.
        for forbidden in ('"variable_instance_set"', '"variable_struct_set"', '"array_set"', "CallGameScriptEx",
                          '"event_perform', '"script_execute"', "HookOneScript("):
            self.assertNotIn(forbidden, code)
        self.assertEqual(code.count("ApCallScript("), 1)
        call = self.body("static void CpCall(")
        gate = call.index('Lower(tok.back()) != "confirm"')
        # Every precondition, and every argument's resolution, comes after the
        # confirm gate and before the one call.
        write = call.index("ApCallScript(")
        for step in ("CpFindRow(tok[1])", "runtime.find('@')", "CpIsProfileGetter(*t)", "MpResolve(",
                     "ApItemFromFingerprint(", "k->kept->call <= 0", "MpArg(a)"):
            self.assertLess(gate, call.index(step), step)
            self.assertLess(call.index(step), write, step)
        # Each refusal says nothing was called and returns.
        lines = call.split("\n")
        refusals = [i for i, l in enumerate(lines) if "refused" in l]
        self.assertGreaterEqual(len(refusals), 7)
        for i in refusals:
            self.assertIn("nothing was called", lines[i])
            self.assertIn("return", lines[i] + lines[i + 1] + lines[i + 2], lines[i])
        # It prints what was supplied, the instance either side and the answer.
        self.assertLess(call.index('"craftprobe call: " + name + " self=other="'), write)
        self.assertLess(call.index('"  before: "'), write)
        self.assertGreater(call.index('"  after:  "'), write)
        self.assertIn("NOT dispatched", call)
        self.assertIn("dispatched -> ret=", call)
        # `backing` keeps the game's own returns and never invokes a getter.
        for fn in ("static void CpCapture(", "static void CpBackingCommand(", "static void CpBackingDump("):
            body = self.body(fn)
            self.assertNotIn("ApCallScript", body)
            self.assertNotIn("script_execute", body)

    def test_craftprobe_readers_are_hook_free_and_never_write(self):
        for fn in ("static void CpReader(", "static void CpListObjectVars(", "static void CpListGlobals(",
                   "static void CpVar(", "static std::string CpValueText("):
            body = self.body(fn)
            for forbidden in ("MmCreateHook", "HookOneScript", "ApCallScript", "script_execute", '"variable_instance_set"',
                              '"variable_struct_set"', '"variable_global_set"', '"array_set"'):
                self.assertNotIn(forbidden, body, fn + " " + forbidden)
        reader = self.body("static void CpReader(")
        # Objects are named through the SDK, never as literals.
        for obj in ("UI_Inventory_obj", "UI_Stash_obj", "Town_Stash_obj", "UI_Craft_obj"):
            self.assertIn("HeroSiege::Objects::GameObject::" + obj, reader)
            self.assertNotIn('"' + obj + '"', reader)

    # ---- the switch ----------------------------------------------------------

    def test_craftmats_off_by_default_and_frame_path_gated(self):
        self.assertIn("std::atomic<bool> m_Enabled{ false };", self.header)
        # The frame callback does nothing for this mod unless the switch is on.
        # Phase 0 wires nothing at all; a shipped adapter must sit behind the gate.
        frame = strip_comments(function_body(self.plugin, "void FrameCallback(FWFrame& FrameContext)"))
        mentions = [m.start() for m in re.finditer(r"CraftMats|HandleCraftCommand|Cp[A-Z]\w*\(", frame)]
        if mentions:
            gate = frame.find("ForgePact::CraftMatsMod::Instance().IsEnabled()")
            self.assertGreaterEqual(gate, 0, "craftmats work on the frame path without the IsEnabled() gate")
            self.assertLessEqual(gate, mentions[0])
        # The command only flips the core's switch: no hook, no game call.
        command = self.body("static void CraftMatsCommand(")
        self.assertIn("mod.SetEnabled(true)", command)
        self.assertIn("mod.SetEnabled(false)", command)
        for forbidden in ("HookOneScript", "MmCreateHook", "CallBuiltin", "ApCallScript", "script_execute", "CallGameScript"):
            self.assertNotIn(forbidden, command)
        # `craftmats stat` is research-build only (the user's rule that player
        # builds carry no debug tooling); the reply never claims work was done.
        shipped = strip_research_blocks(function_body(self.plugin, "static void CraftMatsCommand("))
        self.assertNotIn('"stat"', shipped)
        self.assertIn("crafting is unchanged", command)

    # ---- the core ------------------------------------------------------------

    def test_core_header_is_game_independent(self):
        for forbidden in ("g_Yytk", "CallBuiltin", "Out(", "RValue", "CInstance", "YYTK", "Aurie"):
            self.assertNotIn(forbidden, self.header)
        includes = [l.strip() for l in self.header.split("\n") if l.strip().startswith("#include")]
        self.assertEqual(includes, ["#include <atomic>", "#include <cstdint>", "#include <string>", "#include <vector>"])
        self.assertIn("#include <ForgePact/CraftMatsMod.hpp>", self.plugin)
        # One source, by design: nothing an adapter passes can name another container.
        self.assertIn("enum class CraftMatsSource : int { StashMaterialTab = 1 };", self.header)

    # ---- the research document -------------------------------------------------

    def test_research_doc_has_its_sections_and_status(self):
        head = "\n".join(self.doc.split("\n")[:5])
        self.assertIn("phase0-status: complete", head)
        self.assertRegex(head, r"phase1-status: (pending|complete)")
        for heading in ("## Interpretation", "## Static search", "### Negative results, sourced",
                        "## Baseline (vanilla) to measure", "## Hypotheses", "## Instrument", "## Live procedure",
                        "## Results", "## Decision gate"):
            self.assertIn("\n" + heading + "\n", self.doc, heading)
        results = self.doc[self.doc.index("\n## Results\n"):self.doc.index("\n## Decision gate\n")]
        for row in ("| B0-vanilla |", "| C-control |", "| H-A |", "| H-B |", "| H-C |"):
            self.assertIn(row, results)
        # The decision is the owner's; the document never pre-decides it.
        self.assertRegex(self.doc, r"(?m)^`decision: pending`|^decision: (H-[ABC]|none)$")
        # A negative is "not observed", never "does not happen".
        self.assertNotIn("does not happen", self.doc)

    def test_research_doc_procedure_runs_the_vanilla_check_and_readers_before_the_hook(self):
        procedure = self.doc[self.doc.index("\n## Live procedure\n"):self.doc.index("\n## Results\n")]
        self.assertLess(procedure.index("**B0"), procedure.index("`craftprobe hook`"))
        self.assertLess(procedure.index("`craftprobe bag`"), procedure.index("`craftprobe hook`"))
        self.assertLess(procedure.index("`craftprobe hook`"), procedure.index("`C-control`"))
        self.assertLess(procedure.index("`C-control`"), procedure.index("**Hand craft"))
        self.assertIn("Back up the saves first", procedure)


if __name__ == "__main__":
    unittest.main()
