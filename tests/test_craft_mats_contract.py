"""Contract tests for crafting from the stash's special tabs (ForgePact #14).

How the game counts and consumes a recipe's materials is not settled yet
(docs/crafting-materials-research.md: Phase 0 and Phase 1 done, Phase 1b
pending). This stage ships nothing a player can reach: a research-build
instrument (`craftprobe`), a game-independent decision core (CraftMatsMod.hpp,
whose behaviour test_craft_mats_behavior.py pins) wired only to a `craftmats`
switch that changes nothing yet, and the research document the live sessions
fill in. These tests pin the shape of all three, so the instrument cannot
quietly reach the player build, go blind, or write where it should refuse, and
so the switch cannot start doing work on the frame path before a mechanism is
chosen. Phase 1b widened the table to 202 rows and taught the readers to follow
instance references by name; Phase 1c added 21 rows, a selectable lookup shape,
the Socketable tab's per-cell walk, `node var` and the `store` reader. The tests
for each are marked below.
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

    # Phase 1c split `node` into a collector, a lookup-and-sum pass, a printer and
    # the member readers they share, so a claim about "the node reader" is made
    # on all of them together.
    NODE_FUNCTIONS = (
        "static void CpNodeStackMembers(", "static std::string CpNodeNumericMembers(",
        "static std::string CpNodeClass(", "static void CpNodeReadItem(", "static bool CpNodeCollect(",
        "static void CpNodeLookupAndSum(", "static void CpNodePrintSums(", "static void CpNodeRead(",
        "static void CpNodeTakeEntry(", "static void CpNodeSocket(", "static void CpNodeVar(",
        "static void CpNodeCommand(",
    )
    STORE_FUNCTIONS = (
        "static std::string CpInstanceObjectName(", "static void CpStoreList(", "static void CpStoreHolder(",
        "static void CpStoreGlobals(", "static void CpStore(",
    )

    def node_code(self):
        return "\n".join(self.body(fn) for fn in self.NODE_FUNCTIONS)

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
        # 97 Phase 0 rows + 105 from Phase 1b's widened search (research doc,
        # § Static search, "Phase 1b additions") + 21 from Phase 1c's search for
        # the closed-window store ("Phase 1c rows") + 29 from Phase 1e's search
        # for a stash-side take ("Phase 1e rows") = 252.
        self.assertGreaterEqual(len(self.rows), 252)
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
            # Phase 1b: grid input and drag - the pick-up/place handling nobody had hooked
            "ProcessInventoryGridInput", "___struct___305@ProcessInventoryGridInput@ProcessInventoryGridInputFunc",
            "___struct___308@ProcessInventoryGridInput@ProcessInventoryGridInputFunc", "InvStartDragging",
            "InvCopyItemDragData", "InvCopyItemToInvDragData", "s_InventoryDrag",
            "ResetDragState@anon@47432@s_InventoryDrag@DefineStructs", "InventorySwapItemsNew", "InvGridEquipV2",
            "InvGridEquipGamepad",
            # Phase 1b: adds and stacks outside the Phase 0 table
            "InventoryGridAddItem", "InventoryGridAddItemPos", "InventoryGridAddItemToTab", "InventoryGridHasSpace",
            "InventoryGridHasSpaceMulti", "GridAddToStack", "AddToInventory",
            "___struct___13@AddToInventory@AddToInventoryFunc", "IsStackable", "ItemsAreStackable", "UiASplitStack",
            "UiAInventoryMobileStackSplit", "GetItemFingerprint",
            # Phase 1b: the socket family (the Socketable tab's bag counterpart)
            "InventorySocketItem", "InventorySocketUpdateAndRemove", "InventorySocketUpdateAndSubtract",
            "___struct___169@InventorySocketUpdateAndSubtract@InventoryFuncs", "UiAInventorySocketTabClick",
            "UiDrawInventorySocketTab", "s_SocketData", "GetItemSocketDataStruct",
            "anon@1305@gml_Object_UI_Stash_Socket_New_obj_Create_0",
            # Phase 1b: stash tab buys and types
            "UiAStashTabSocketableBuy", "UiAStashTabUniqueBuy", "UiAStashTabBuy", "UiAStashUniqueItemType",
            # Phase 1b: node and tab plumbing
            "UiCreateNode", "___struct___409@UiCreateNode@UiFuncs", "UiRemoveNode", "UiMoveNode",
            "ValidateInventoryNode", "DetectInventoryDuplicateNode", "s_InvNode", "UiResizeInventoryNodes",
            "GetInventoryMaxTabs", "InventoryResetTabs", "InventorySortTab",
            "___struct___187@InventorySortTab@InventoryGrid", "UiAInventoryTabClick", "InventoryLogAddItem",
            "InventoryUpdateExtAddItemStats", "anon@403@gml_Object_UI_Button_Inventory_Tab_obj_Step_0",
            "___struct___448@gml_Object_Load_Inventory_obj_Other_62",
            # Phase 1c: where LoadStash may put the stash while its window is closed
            "GetItemMap", "GetInventoryMapPos", "SaveInventoryMap", "LoadInventoryOrderNew", "s_SaveStashConstants",
            # Phase 1e: every name that takes, removes, splits, validates or
            # converts inventory or stash items - the online stash Take family
            # and its add counterparts, the map's own removal, the validators,
            # the online conversions and the stack/split family
            "StashTakeItemOnline", "___struct___224@StashTakeItemOnline@InventoryStashFuncs",
            "___struct___225@StashTakeItemOnline@InventoryStashFuncs",
            "___struct___227@StashTakeItemOnline@InventoryStashFuncs", "StashUniqueTakeItemOnline",
            "___struct___229@StashUniqueTakeItemOnline@InventoryStashFuncs",
            "___struct___230@StashUniqueTakeItemOnline@InventoryStashFuncs", "StashGuildTakeItemOnline",
            "StashBloodPactTakeItemOnline", "StashAddItemOnline", "___struct___237@StashAddItemOnline@InventoryStashFuncs",
            "StashUniqueAddItemOnline", "RemoveItemFromMap", "OnlineRemoveItem", "CheckInventoryOperation",
            "ValidateInventory", "DetectInventoryDuplicates", "DetectInventoryModifications", "ConvertOnlineStash",
            "ConvertOnlineStashMap", "OnlineAddToStack", "___struct___16@OnlineAddToStack@AddToInventoryFunc",
            "InventorySplitOperation", "___struct___158@InventorySplitOperation@InventoryFuncs", "InventorySplitDrop",
            "___struct___155@InventorySplitDrop@InventoryFuncs", "GetOnlinePlayerItemOwner", "s_ItemOperation",
            "s_ItemGridInfo",
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
        # Phase 1b: the grid, inventory and stash-tab objects the hand move may run through
        "UI_Inventory_Grid_obj", "UI_Grid_obj", "UI_Node_Parent_obj", "UI_Inventory_obj", "UI_Inventory_Parent_obj",
        "UI_Stash_Socket_New_obj", "UI_Stash_Unique_Items_obj", "Load_Inventory_obj", "UI_Split_Stack_obj",
        # Phase 1c: LoadStash's self at character load, and the profile manager
        "Console_Save_obj", "Profile_Manager_obj",
    )

    def test_craftprobe_table_covers_every_sdk_closure_of_its_objects(self):
        table = {constant for _, _, constant in self.rows}
        expected = []
        for constant, value in re.findall(r'std::string_view (\w+) = "([^"]+)";', self.sdk):
            if any("@gml_Object_" + obj + "_Create_0" in value for obj in self.CLOSURE_OBJECTS):
                expected.append(constant)
        # A scan that finds nothing would pass the check below vacuously.
        # 29 cube/stash closures + 52 on the nine Phase 1b objects + 16 on the
        # two Phase 1c objects (6 Console_Save_obj, 10 Profile_Manager_obj).
        self.assertGreaterEqual(len(expected), 97, "SDK closure scan found too few constants - the regex is blind")
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
        # Phase 1e's forms too: the `id:<n>` self, `fp9:`, `map9`/`map9:<key>`
        # (only while mapkeep calls the kept map current) and `path:`.
        for step in ("CpFindRow(tok[1])", "runtime.find('@')", "CpIsProfileGetter(*t)", "MpResolve(",
                     "ApItemFromFingerprint(", "k->kept->call <= 0", "MpArg(a)",
                     '"instance_exists", { handle }', "HhResolveInstance(handle)",
                     "ApItemFromFingerprintAs(inst, RValue(a.substr(4)), RValue(9.0), v)", "MkCurrentMap(map, why)",
                     "MkMapEntry(map, key, v, form)", "CpCallPathArg(a.substr(5), v)"):
            self.assertLess(gate, call.index(step), step)
            self.assertLess(call.index(step), write, step)
        # Each refusal says nothing was called and returns.
        lines = call.split("\n")
        refusals = [i for i, l in enumerate(lines) if "refused" in l]
        self.assertGreaterEqual(len(refusals), 15)
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
                   "static void CpVar(", "static std::vector<std::string> CpSplitPath(", "static bool CpVarRoot(",
                   "static bool CpVarWalk(", "static std::string CpValueText(", "static CpRef CpClassifyRef(",
                   "static std::string CpDsText(", "static void CpFollowInstance(") \
                + self.NODE_FUNCTIONS + self.STORE_FUNCTIONS:
            body = self.body(fn)
            for forbidden in ("MmCreateHook", "HookOneScript", "ApCallScript", "script_execute", '"variable_instance_set"',
                              '"variable_struct_set"', '"variable_global_set"', '"array_set"'):
                self.assertNotIn(forbidden, body, fn + " " + forbidden)
        reader = self.body("static void CpReader(")
        # Objects are named through the SDK, never as literals.
        for obj in ("UI_Inventory_obj", "UI_Stash_obj", "Town_Stash_obj", "UI_Craft_obj"):
            self.assertIn("HeroSiege::Objects::GameObject::" + obj, reader)
            self.assertNotIn('"' + obj + '"', reader)

    # ---- Phase 1b: the marker, the ref-follower, the node reader, backing ----

    def test_craftprobe_reports_its_phase_marker(self):
        # A bare `craftprobe` answers the usage, whose FIRST line names this
        # build's phase and row count - so a live session tells this build from
        # Phase 1b's (202 rows) and aa0c72a's (97) in one command, before
        # anything else counts.
        command = self.body("static void CpCommand(")
        self.assertIn("if (tok.empty()) { CpUsage(); return; }", command)
        usage = self.body("static void CpUsage(")
        first = usage[usage.index("Out("):]
        first = first[:first.index(";")]
        self.assertIn("phase1e rows=", first)
        self.assertIn("kCpTargetCount", first)
        self.assertEqual(self.plugin.count("phase1e rows="), 1)
        self.assertEqual(self.plugin.count("phase1c rows="), 0)
        self.assertEqual(self.plugin.count("phase1b rows="), 0)

    def test_craftprobe_var_follows_instance_refs_by_name_only(self):
        classify = self.body("static CpRef CpClassifyRef(")
        # A ref is recognised from the runtime's own text for it, never from its bytes.
        self.assertIn("VALUE_REF", classify)
        for prefix in ('"ref instance "', '"ref ds_grid "', '"ref ds_list "', '"ref ds_map "'):
            self.assertIn(prefix, classify)
        follow = self.body("static void CpFollowInstance(")
        for name in ('"instance_exists"', '"object_get_name"', '"variable_instance_get_names"', '"variable_instance_get"'):
            self.assertIn(name, follow)
        # Nothing is read off an instance before the runtime says it exists.
        self.assertLess(follow.index('"instance_exists"'), follow.index('"variable_instance_get_names"'))
        self.assertLess(follow.index('"instance_exists"'), follow.index('"object_get_name"'))
        # A depth cap and a visited set: one instance hangs off several variables.
        self.assertRegex(self.block, r"static constexpr int kCpRefMaxDepth = \d+;")
        self.assertIn("kCpRefMaxDepth", follow)
        self.assertIn("visited", follow)
        self.assertRegex(follow, r"visited\.(insert|count|find)\(")
        # A data structure is read only after ds_exists answers true for its type.
        ds = self.body("static std::string CpDsText(")
        first_read = min(ds.index(p) for p in ('"ds_grid_', '"ds_list_', '"ds_map_') if p in ds)
        self.assertLess(ds.index('"ds_exists"'), first_read)
        for p in ('"ds_grid_', '"ds_list_', '"ds_map_'):
            self.assertIn(p, ds)
        self.assertRegex(self.block, r"static constexpr int kCpDsPreview = \d+;")
        self.assertIn("kCpDsPreview", ds)
        # A method value is described, never invoked; nothing is written.
        code = strip_comments(self.block)
        for forbidden in ("m_Pointer", "m_Object", '"method_call"', '"script_execute"', '"variable_instance_set"'):
            self.assertNotIn(forbidden, code)
        self.assertIn('"is_method"', follow + self.body("static std::string CpValueText("))
        var = self.body("static void CpVar(")
        # `id:<n>` and a dotted path are both roots the command takes.
        self.assertIn('"id:"', var)
        self.assertIn("CpSplitPath(", var)
        self.assertIn("'.'", self.body("static std::vector<std::string> CpSplitPath("))
        self.assertIn("CpFollowInstance(", var)
        # The walk `var` and `node var` share checks the instance at every step.
        walk = self.body("static bool CpVarWalk(")
        self.assertLess(walk.index('"instance_exists"'), walk.index('"variable_instance_get"'))

    def test_craftprobe_node_reader_is_hook_free_and_calls_only_the_fingerprint_lookup(self):
        node = self.node_code()
        command = self.body("static void CpNodeCommand(")
        for forbidden in ("MmCreateHook", "HookOneScript", "ApCallScript", "script_execute", "CallBuiltinEx",
                          '"variable_instance_set"', '"variable_struct_set"', '"array_set"', "m_Pointer"):
            self.assertNotIn(forbidden, node)
        # The one game script it calls is the fingerprint lookup, through a
        # by-name helper beside the one `call`'s fp: argument uses - capped per run.
        self.assertEqual(node.count("ApItemFromFingerprintAs("), 1)
        self.assertEqual(node.count("ApItemFromFingerprint("), 0)
        self.assertIn("ApItemFromFingerprint(", self.body("static void CpCall("))
        lookup = function_body(self.plugin, "static bool ApItemFromFingerprint(")
        self.assertIn("ApCallScript(kApFromFpName", lookup)
        self.assertRegex(self.block, r"static constexpr int kCpNodeMaxLookups = \d+;")
        self.assertIn("kCpNodeMaxLookups", node)
        # What it prints: size, fill, distinct fingerprints, and per item its
        # type, its definition's b, the class suffix, the stack-like members,
        # then a per-(class, b) sum.
        for token in ('"nodeGridWidth"', '"nodeGridHeight"', '"nodeFingerprint"', '"itemType"',
                      '"itemDefinitionStruct"', '"b"', '"stack"', '"amount"', '"count"', '"qty"', "sum"):
            self.assertIn(token, node, token)
        # No nodeGrid prints the variable names, never nothing.
        self.assertIn("no nodeGrid", node)
        self.assertIn('"variable_instance_get_names"', node)
        # stash / bag / id:<n> / <Obj> <nth>, objects named through the SDK.
        for sel in ('"stash"', '"bag"', '"id:"', '"socket"', '"var"'):
            self.assertIn(sel, command)
        self.assertIn("HeroSiege::Objects::GameObject::UI_Stash_obj", command)
        self.assertIn("HeroSiege::Objects::GameObject::UI_Inventory_Grid_obj", command)
        self.assertIn('if (sub == "node") { CpNodeCommand(tok); return; }', self.body("static void CpCommand("))

    def test_craftprobe_node_reader_counts_the_items_o_member(self):
        # The item struct carries the save's short keys (its definition's `b`
        # is the save's data.b), and the save's stack count is `o`
        # (docs/RUNTIME_DATA_MODELS.md § 2; the hub's stash_tab_counts.py sums
        # data.o). A filter of stack-like words alone can never match that
        # one-letter name, so a right container would read as no count at all.
        node = self.node_code()
        self.assertRegex(node, r'name\s*==\s*"o"')
        # Exact match only: `o` as a substring would take color, bonus, ...
        self.assertNotRegex(node, r'find\("o"\)')
        words = re.search(r"kStackWords\[\]\s*=\s*\{([^}]*)\}", node)
        self.assertIsNotNone(words)
        self.assertNotIn('"o"', words.group(1))
        # With no stack-named member on the definition, every numeric member of
        # it is printed, capped, so an unexpected name shows up instead of silence.
        self.assertRegex(self.block, r"static constexpr int kCpNodeDefMembersShown = \d+;")
        self.assertIn("kCpNodeDefMembersShown", node)
        self.assertIn("numeric members", node)
        # A lookup miss names what was supplied, so it reads "not resolved with
        # self=<obj>, a1=<a1>" and never "not resolvable".
        self.assertIn("returned no struct (self=", node)
        self.assertIn('", a1=" + o.a1Text', node)
        # The bag control proves the count half too: it passes only on a
        # per-(class, b) sum equal to a bag stack the owner can see.
        row = next(l for l in self.doc.splitlines() if l.startswith("| node-bag-control |"))
        self.assertIn("equal", row)
        self.assertIn("by eye", row)

    def test_craftprobe_backing_keeps_getter_returns_per_first_argument(self):
        per = self.body("static int CpBackingArgIndex(")
        self.assertIn("HeroSiege::Scripts::gml_Script_GetInventoryArray", per)
        self.assertIn("HeroSiege::Scripts::gml_Script_CountInventoryItem", per)
        self.assertIn("CpBackingArgIndex(t) >= 0", self.body("static bool CpKeepsPerArgument("))
        self.assertRegex(self.block, r"static constexpr int kCpBackingMaxArgs = 8;")
        capture = self.body("static void CpCapture(")
        self.assertIn("A[0]", capture)
        self.assertIn("kCpBackingMaxArgs", capture)
        self.assertIn("perArg", capture)
        # The detour hands the arguments to the capture; the getter is still
        # never invoked by the instrument (test_craftprobe_writes_are_confirm_gated).
        detour = self.plugin[self.plugin.index("#define CRAFTPROBE_DETOUR"):self.plugin.index("#define CRAFTPROBE_TARGETS")]
        self.assertIn("S, argc, A, r)", detour)
        dump = self.body("static void CpBackingDump(")
        self.assertIn("perArg", dump)
        self.assertIn('"_arg"', dump)
        on = self.body("static void CpBackingCommand(")
        self.assertIn("CpKeepsPerArgument(t)", on)

    # ---- Phase 1c: the lookup shape, backing per a1, node var, store ---------

    def test_craftprobe_node_lookup_takes_a_selectable_self_and_second_argument(self):
        # Bag cells resolve with GetItemFromFingerprint(fp, 0) on the grid read;
        # the three special-tab cells tried did not. Which of self and a1 the
        # game varies for a stash cell is not known, so both are selectable.
        helper = self.body("static bool ApItemFromFingerprintAs(")
        self.assertIn("static bool ApItemFromFingerprintAs(CInstance* self, const RValue& fp, const RValue& a1, RValue& item)",
                      self.plugin)
        self.assertIn("ApCallScript(kApFromFpName, self, { fp, a1 }, item)", helper)
        self.assertIn("ApIsPlainStruct(item)", helper)
        # Research build only; the player build's lookup keeps its fixed shape.
        self.assertNotIn("ApItemFromFingerprintAs", strip_research_blocks(self.plugin))
        self.assertIn("ApCallScript(kApFromFpName, gridInst, { fp, RValue(0.0) }, item)",
                      self.body("static bool ApItemFromFingerprint("))
        command = self.body("static void CpNodeCommand(")
        for option in ('"a1="', '"self=id:"', '"class="'):
            self.assertIn(option, command)
        # A named self is read only after the runtime says it exists.
        self.assertLess(command.index('"instance_exists"'), command.index("HhResolveInstance("))
        sweep = self.body("static void CpNodeLookupAndSum(")
        self.assertIn("ApItemFromFingerprintAs(self, e.value, o.a1, item)", sweep)
        self.assertIn("o.selfGiven ? o.selfInst : e.grid", sweep)
        self.assertIn("o.cls", sweep)
        # The instrument's own lookups are neither logged nor kept as the game's.
        self.assertLess(sweep.index("g_CpOwnLookup = true"), sweep.index("ApItemFromFingerprintAs("))
        self.assertLess(sweep.index("ApItemFromFingerprintAs("), sweep.index("g_CpOwnLookup = false"))
        self.assertIn("g_CpOwnLookup", self.body("static bool CpObserve("))
        self.assertIn("!g_CpOwnLookup", self.body("static void CpAfter("))
        # The cap covers the Socketable tab's 140 cells and a whole bag grid.
        cap = re.search(r"static constexpr int kCpNodeMaxLookups = (\d+);", self.block)
        self.assertGreaterEqual(int(cap.group(1)), 160)
        # `node socket`: the tab's window through the SDK, its `grid` walked one
        # level, each cell instance read only after instance_exists, once.
        socket = self.body("static void CpNodeSocket(")
        self.assertIn('"grid"', socket)
        self.assertIn("visited", socket)
        self.assertLess(socket.index('"instance_exists"'), socket.index("CpNodeCollect("))
        self.assertIn("HeroSiege::Objects::GameObject::UI_Stash_Socket_New_obj", command)
        self.assertIn("CpNodePrintSums(", socket)

    def test_craftprobe_backing_keeps_fingerprint_lookups_per_second_argument(self):
        per = self.body("static int CpBackingArgIndex(")
        self.assertRegex(per, r"gml_Script_GetItemFromFingerprint\)\s*return 1;")
        self.assertRegex(per, r"gml_Script_GetItemMap\)\s*return 0;")
        default = self.body("static bool CpDefaultBackingRow(")
        for row in ("gml_Script_GetItemFromFingerprint", "gml_Script_GetItemMap"):
            self.assertIn("HeroSiege::Scripts::" + row, default)
        on = self.body("static void CpBackingCommand(")
        self.assertIn("t.kept->argIndex = CpBackingArgIndex(t)", on)
        # The capture keys on the row's own argument, and per signature keeps a
        # call count, the latest a0 as text and up to six distinct self objects.
        capture = self.body("static void CpCapture(")
        self.assertIn("A[k]", capture)
        self.assertIn("kept.argIndex", capture)
        self.assertIn("++slot->calls", capture)
        self.assertIn("slot->a0 =", capture)
        self.assertIn("kCpBackingMaxSelves", capture)
        self.assertRegex(self.block, r"static constexpr int kCpBackingMaxSelves = 6;")
        dump = self.body("static void CpBackingDump(")
        for field in ('" calls="', '" a0="', '" selves="'):
            self.assertIn(field, dump)

    def test_craftprobe_store_reader_is_hook_free_and_names_its_objects_through_the_sdk(self):
        code = "\n".join(self.body(fn) for fn in self.STORE_FUNCTIONS)
        holders = self.block[self.block.index("kCpStoreHolders[] = {"):]
        holders = holders[:holders.index("};")]
        for obj in ("Console_Save_obj", "Profile_Manager_obj", "Town_Stash_obj", "Player_obj",
                    "New_Inventory_Data_obj", "Inventory_Loading_obj", "Load_Inventory_obj"):
            self.assertIn("HeroSiege::Objects::GameObject::" + obj, holders)
            self.assertNotIn('"' + obj + '"', code)
        store = self.body("static void CpStore(")
        self.assertIn("kCpStoreHolders", store)
        # A live instance is found before anything is read off it.
        self.assertLess(store.index('"instance_number"'), store.index('"instance_find"'))
        holder = self.body("static void CpStoreHolder(")
        self.assertLess(holder.index('"instance_exists"'), holder.index('"variable_instance_get_names"'))
        # The kept GetProfileInventoryData return is followed only when it is an
        # instance reference, never invoked (capture only).
        self.assertIn("HeroSiege::Scripts::gml_Script_GetProfileInventoryData", store)
        self.assertIn("CpRefKind::Instance", store)
        # A ref's shape: its object's name after instance_exists, a data
        # structure's size after ds_exists (through CpDsText).
        listing = self.body("static void CpStoreList(")
        self.assertIn("CpDsText(", listing)
        self.assertIn("CpInstanceObjectName(", listing)
        self.assertLess(listing.index('"instance_exists"'), listing.index("CpInstanceObjectName("))
        self.assertIn('"object_get_name"', self.body("static std::string CpInstanceObjectName("))
        # `store names` is never cut at the reader's 80 lines; the research
        # globals the instrument itself set are not a finding.
        self.assertRegex(self.block, r"static constexpr int kCpStoreMaxNames = 400;")
        self.assertIn("kCpStoreMaxNames", listing)
        self.assertIn("kCpResearchGlobalPrefix", listing)
        self.assertIn('"__cp_"', self.block)
        self.assertIn('"names"', store)
        self.assertIn("RValue(-5.0)", self.body("static void CpStoreGlobals("))
        for forbidden in ("MmCreateHook", "HookOneScript", "ApCallScript", "script_execute", '"variable_instance_set"',
                          '"variable_struct_set"', '"variable_global_set"', '"array_set"', "m_Pointer"):
            self.assertNotIn(forbidden, code)
        self.assertIn('if (sub == "store") { CpStore(tok); return; }', self.body("static void CpCommand("))

    def test_craftprobe_node_sums_an_array_or_map_container_the_same_way(self):
        var = self.body("static void CpNodeVar(")
        # The path is resolved the way `var` resolves it.
        self.assertIn("CpVarRoot(", var)
        self.assertIn("CpVarWalk(", var)
        # A live instance is read as a grid; an array, ds_list or ds_map (after
        # ds_exists with its own type) or a struct is read entry by entry.
        self.assertIn("CpNodeRead(", var)
        self.assertIn('"array_get"', var)
        for ds, read in (("kCpDsTypeList", '"ds_list_find_value"'), ("kCpDsTypeMap", '"ds_map_find_first"')):
            guard = var.index('"ds_exists", { cur, RValue(' + ds + ') }')
            self.assertLess(guard, var.index(read), read)
        self.assertIn('"ds_map_find_next"', var)
        self.assertIn("kCpNodeMaxEntries", var)
        # An entry is a fingerprint (one lookup, like a grid cell) or an item
        # struct, whose definition's b and o are read directly - no lookup.
        take = self.body("static void CpNodeTakeEntry(")
        self.assertIn('"nodeFingerprint"', take)
        self.assertIn('"itemDefinitionStruct"', take)
        self.assertIn("CpNodeReadItem(", take)
        self.assertNotIn("ApItemFromFingerprint", take)
        item = self.body("static void CpNodeReadItem(")
        self.assertIn('"b"', item)
        self.assertIn("CpNodeStackMembers(def", item)
        self.assertRegex(self.body("static void CpNodeStackMembers("), r'name\s*==\s*"o"')
        # Both routes end in the same lookup pass and the same sum lines.
        for fn in ("static void CpNodeRead(", "static void CpNodeVar("):
            body = self.body(fn)
            self.assertIn("CpNodePrintSums(", body, fn)
        self.assertIn("CpNodeLookupAndSum(", var)
        self.assertIn("CpNodeLookupAndSum(", self.body("static void CpNodeRead("))

    # ---- Phase 1e: mapkeep, the held rows, the whole map ---------------------

    MAPKEEP_START = "// ---- mapkeep:"
    MAPKEEP_END = "#endif // FORGEPACT_RELEASE (mapkeep)"

    def mapkeep_block(self):
        return self.plugin[self.plugin.index(self.MAPKEEP_START):self.plugin.index(self.MAPKEEP_END)]

    def test_mapkeep_is_research_build_only_and_dispatched_from_handle_craft_command(self):
        shipped = strip_research_blocks(self.plugin)
        self.assertIn("mapkeep", self.plugin)
        self.assertNotIn("mapkeep", shipped)
        for symbol in ("MkCommand", "MkHookGetItemMap", "MkHookLoadStash", "MkRoomTick", "MkCurrentMap", "g_MkCore"):
            self.assertIsNone(re.search(r"\b" + symbol, shipped), symbol + " reaches the player build")
        self.assertNotIn("mapkeep", self.player_commands())
        self.assertEqual(self.plugin.count('"mapkeep"'), 1)
        handler = function_body(self.plugin, "static bool HandleCraftCommand(")
        self.assertIn('if (lc == "mapkeep") { MkCommand(rest); return true; }', handler)
        self.assertNotIn("mapkeep", strip_research_blocks(handler))
        # It sits before craftprobe's block, so craftprobe's own pins (no
        # HookOneScript( in its block) stand and craftprobe can ask it.
        self.assertLess(self.plugin.index(self.MAPKEEP_START), self.plugin.index(BLOCK_START))
        self.assertLess(self.plugin.index(self.MAPKEEP_END), self.plugin.index(BLOCK_START))

    def test_mapkeep_installs_through_the_player_build_installer_and_never_calls_the_game(self):
        block = self.mapkeep_block()
        # The player-build shape: HookOneScript, both hooks, each with its native
        # flag - and every name through the SDK, never a retyped literal.
        on = self.body("static void MkOn(")
        self.assertEqual(on.count("HookOneScript("), 2)
        self.assertIn("HookOneScript(kMkGetItemMapName, \"fp_mk_getitemmap\", (PVOID)MkHookGetItemMap, &g_MkOrigGetItemMap, &mapNative)", on)
        self.assertIn("HookOneScript(kMkLoadStashName, \"fp_mk_loadstash\", (PVOID)MkHookLoadStash, &g_MkOrigLoadStash, &loadNative)", on)
        self.assertIn("SdkShortScriptName(HeroSiege::Scripts::gml_Script_GetItemMap)", block)
        self.assertIn("SdkShortScriptName(HeroSiege::Scripts::gml_Script_LoadStash)", block)
        # The install line names the installer's answer either way.
        report = self.body("static void MkReportInstall(")
        for word in ("both-routes", "TABLE-ONLY", "NOT-INSTALLED"):
            self.assertIn(word, report)
        # It refuses when craftprobe already holds either row: a second inline
        # detour would fail and the installer would report a false TABLE-ONLY.
        self.assertLess(on.index("CpHoldsRow(HeroSiege::Scripts::gml_Script_GetItemMap)"), on.index("HookOneScript("))
        self.assertLess(on.index("CpHoldsRow(HeroSiege::Scripts::gml_Script_LoadStash)"), on.index("HookOneScript("))
        self.assertIn("already on", on)
        # Nothing here calls a game script or patches an address itself.
        for forbidden in ("script_execute", "ApCallScript", "MmCreateHook", "CallGameScriptEx", '"variable_instance_set"',
                          '"variable_struct_set"', '"array_set"', "m_Pointer"):
            self.assertNotIn(forbidden, block)
        # The hook bodies forward through the trampoline; GetItemMap compares
        # its first argument numerically (int64:9 and real:9.0 alike).
        getmap = self.body("static RValue& MkHookGetItemMap(")
        self.assertIn("g_MkOrigGetItemMap(S, O, R, argc, A)", getmap)
        self.assertIn("PpIsNumber(*A[0])", getmap)
        self.assertIn("d == 9.0", getmap)
        self.assertNotIn("m_Kind == VALUE_INT64", getmap)
        load = self.body("static RValue& MkHookLoadStash(")
        self.assertIn("g_MkOrigLoadStash(S, O, R, argc, A)", load)
        self.assertLess(load.index("Invalidate(ForgePact::CraftMatsKeptMapReason::CharacterLoaded)"),
                        load.index("g_MkOrigLoadStash(S, O, R, argc, A)"))
        # The kept value is rooted in a research global the collector sees.
        keep = self.body("static void MkKeep(")
        self.assertLess(keep.index('"variable_global_set"'), keep.index("g_MkCore.Refreshed(index)"))
        self.assertIn('"__cp_mapkeep_9"', block)
        self.assertIn("kMkKeepLines", keep)

    def test_mapkeep_answers_currency_at_the_point_of_use(self):
        block = self.mapkeep_block()
        self.assertIn("ForgePact::CraftMatsKeptMap g_MkCore", block)
        # The shared point-of-use answer: the room read again now, the core's
        # rule, then ds_exists as a map - ds_exists never makes it current.
        current = self.body("static bool MkCurrentMap(")
        self.assertLess(current.index("MkRoomPoll()"), current.index("g_MkCore.IsCurrent()"))
        self.assertLess(current.index("g_MkCore.IsCurrent()"), current.index('"ds_exists"'))
        self.assertIn('"ds-gone"', current)
        for fn in ("static void MkStat(", "static void MkFind("):
            self.assertIn("MkCurrentMap(map, reason)", self.body(fn), fn)
        self.assertIn("MkCurrentMap(map, why)", self.body("static void CpCall("))
        # The frame path is housekeeping only: it notices a room change.
        tick = self.body("static void MkRoomTick(")
        self.assertIn("kMkRoomPollFrames", tick)
        self.assertIn("MkRoomPoll()", tick)
        frame = function_body(self.plugin, "void FrameCallback(FWFrame& FrameContext)")
        self.assertIn("MkRoomTick();", frame)
        self.assertNotIn("MkRoomTick", strip_research_blocks(frame))
        poll = self.body("static void MkRoomPoll(")
        # An unreadable room is skipped, never compared.
        self.assertLess(poll.index("key == INT64_MIN"), poll.index("Invalidate(ForgePact::CraftMatsKeptMapReason::RoomChanged)"))
        # What `stat` prints.
        stat = self.body("static void MkStat(")
        for token in ('" a0=9 calls="', '"a0=0 calls="', '" LoadStash calls="', '" current="', '" reason="',
                      '" refreshed="', '" | first9: "', '" | kept="'):
            self.assertIn(token.strip('"'), stat.replace('"', ""), token)
        # `find` walks by name, only after the currency check, capped.
        find = self.body("static void MkFind(")
        self.assertLess(find.index("MkCurrentMap("), find.index('"ds_map_find_first"'))
        self.assertIn("kMkFindMaxEntries", find)
        self.assertRegex(self.plugin, r"static constexpr int kMkFindMaxEntries = 4000;")
        self.assertIn("entries walked=", find)

    def test_craftprobe_hook_reports_rows_mapkeep_holds_as_held_not_failed(self):
        install = self.body("static void CpInstall(")
        self.assertIn("held by mapkeep", install)
        held = install.index("MkHolds(t.runtimeName)")
        self.assertLess(held, install.index("CpResolve(t, why)"))
        self.assertIn("++held", install)
        holds = self.body("static bool MkHolds(")
        self.assertIn("HeroSiege::Scripts::gml_Script_GetItemMap", holds)
        self.assertIn("HeroSiege::Scripts::gml_Script_LoadStash", holds)
        self.assertIn("t.installed.load()", self.body("static bool CpHoldsRow("))

    def test_craftprobe_node_var_reads_the_whole_stash_map(self):
        # Live 1d's reads stopped at 1000 of the stash map's 1626 entries.
        self.assertRegex(self.block, r"static constexpr int kCpNodeMaxEntries = 2000;")

    def test_craftprobe_call_takes_the_phase1e_argument_forms(self):
        call = self.body("static void CpCall(")
        for form in ('"id:"', '"fp9:"', '"map9"', '"map9:"', '"path:"'):
            self.assertIn(form, call, form)
        # `path:` resolves the way `var` does; `map9:` reads the kept map by name.
        path = self.body("static bool CpCallPathArg(")
        self.assertIn("CpVarRoot(", path)
        self.assertIn("CpVarWalk(", path)
        entry = self.body("static bool MkMapEntry(")
        self.assertLess(entry.index('"ds_map_exists"'), entry.index('"ds_map_find_value"'))

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
        head = "\n".join(self.doc.split("\n")[:8])
        self.assertIn("phase0-status: complete", head)
        self.assertRegex(head, r"phase1-status: (pending|complete)")
        self.assertRegex(head, r"phase1b-status: (pending|complete)")
        self.assertRegex(head, r"phase1c-status: (pending|complete)")
        self.assertRegex(head, r"phase1d-status: (pending|complete)")
        self.assertRegex(head, r"(?m)^phase1e-status: (pending|complete)$")
        for heading in ("## Interpretation", "## Static search", "### Negative results, sourced",
                        "## Baseline (vanilla) to measure", "## Hypotheses", "## Instrument", "## Live procedure",
                        "## Results", "### Constraints from Phase 1", "### Live procedure 1b",
                        "### Phase 1b results", "### Phase 1c rows", "### Phase 1c readers",
                        "### Live procedure 1c", "### Phase 1c results", "### Live procedure 1d",
                        "### Phase 1d results", "## Decision gate"):
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
