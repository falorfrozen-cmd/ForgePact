"""Source and text contract for Headhunter and Tyrant's Crown from the game's own Angelic roll (#74).

#63 put both items into ForgePact's own Angelic/Unholy pool, so they dropped on the
slider's die whatever their World switches said. #74 (owner-directed, 2026-10-02) took
them out of that pool again and, after a first "beside" design that spawned them next to
the game's own item, moved them into the game's own roll by list injection ("list
injection first"): for the length of each `DropItemAngelicChance` call the game's
Angelic list (a variable of the first `Controller_obj` instance) carries one stand-in
entry per switched-on item - a real Angelic unique of the item's own type, Liquor Holster
for Headhunter - the game's picker and die decide, a hit typed (replan 1) from the
`GetUniqueRepoStruct` read the roll made for it as the stand-in's whole entry (type, sub,
b) is ours at one entry's share, and on ours the record the item is built from is rewritten
at `CreateItemNew`'s entry (Session 5: `CreateDefaultParams`' struct has no `a`, and
`LootGroundCreate` stores its own `a` after it) so the game itself builds and places the
item, one per hit, never beside; a refused rewrite latches its item off for the session
(`rewrite refused: <why>`, `refused=`). A hit no read
types stays the game's (`untyped=`), and a push a fresh read of the list does not show is
taken off again before the roll can carry it (the held read-back). The owner's decision of
2026-10-02 ("Panel switch only") makes the gate the panel switch (`force`), not the
mechanic's enabled state, which a forged item's auto-arm also sets.

`test_angelic_hit_behavior.py` runs the hook natively but skips without a C++ toolchain;
this file pins the same shape on the source text, so it is checked everywhere the suite
runs: the item table, the list resolved by name, the push and removal held by a scope
guard inside the roll's hook, the rewrite read back, no spawn on the roll path, the
detection installed only from the switches' `force` paths and the research levers, the
gate, the status tokens the live procedure reads, the research levers kept out of the
player build, and the player-facing texts.
"""
import importlib.util
import os
import pathlib
import re
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
SOURCE = pathlib.Path(os.environ.get("FORGEPACT_TEST_PLUGIN_SOURCE", ROOT / "plugin" / "ModuleMain.cpp")).read_text(encoding="utf-8").replace("\r\n", "\n")
PANEL_SRC = pathlib.Path(os.environ.get("FORGEPACT_TEST_PANEL_SRC", str(ROOT / "panel" / "src")))

# One definition of "what the player build compiles", shared with the release contract.
_spec = importlib.util.spec_from_file_location("_release_hook_contract", ROOT / "tests" / "test_release_hook_contract.py")
_release = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(_release)
strip_research_blocks = _release.strip_research_blocks
strip_comments = _release.strip_comments


def body(source, signature):
    """Brace-matched definition; `rfind` so a forward declaration is skipped."""
    start = source.rfind(signature)
    if start < 0:
        raise AssertionError(f"not found: {signature}")
    brace = source.index("{", start)
    depth = 0
    for index in range(brace, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[start:index + 1]
    raise AssertionError(f"unterminated: {signature}")


def read(path):
    return pathlib.Path(path).read_text(encoding="utf-8").replace("\r\n", "\n")


# Every function the roll path runs through in the player build.
ROLL_PATH = (
    "static RValue& HookAngelicChance(",
    "static RValue& Hook_CreateDefaultParams(",
    "static RValue& Hook_GetUniqueRepoStruct(",
    "static bool SignatureTailHolds(",
    "static bool SignatureHeldReadBack(",
    "static void SignatureInjectPush(",
    "static void SignatureInjectRemove(",
    "static void SignatureAttributeHit(",
    "static bool SignatureRewriteParams(",
    "static void SignatureRefuse(",
    "static void SignatureBeforeCreate(",
    "static void SignatureAfterHit(",
    "static void SignatureNoteBuilt(",
)


class AngelicHitSourceContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.code = strip_comments(SOURCE)
        cls.shipped = strip_research_blocks(SOURCE)
        cls.shipped_code = strip_comments(cls.shipped)

    # ---- the pool no longer carries the two items, and the beside design is gone -------

    def test_the_pool_append_is_gone(self):
        self.assertNotIn("AppendSignatureCandidates", self.code)

    def test_a_pool_candidate_has_no_signature_member(self):
        struct = re.search(r"struct AngelicCandidate \{([^}]*)\}", self.code)
        self.assertIsNotNone(struct, "struct AngelicCandidate")
        self.assertNotIn("signature", struct.group(1))

    def test_the_slider_never_spawns_a_signature_item(self):
        kill = body(self.code, "static void AngelicDropOnKill(CInstance* S)")
        self.assertNotIn("SpawnSignatureItem(", kill)
        self.assertIn("SpawnAngelicItem(", kill)

    def test_the_beside_design_is_gone(self):
        for name in ("SignatureShare", "SignatureDropOnAngelicHit", "g_SigShareRolls", "g_SigFromGame", "g_AngHitSharePct"):
            with self.subTest(name=name):
                self.assertNotIn(name, self.code)
        for token in ("shareRolls=", "sigFromGame="):
            self.assertNotIn(token, SOURCE)

    # ---- the mod items: one table, a stand-in each --------------------------------------

    def test_the_table_holds_the_two_switched_items(self):
        start = self.code.index("kSignatureItems[]")
        table = self.code[start:self.code.index("\n};", start)]
        rows = re.findall(r'\{\s*"([^"]+)",\s*(\d+),\s*(\d+),\s*([\d.]+),\s*([\d.]+),\s*(nullptr|"[^"]*")\s*\}', table)
        self.assertEqual(len(rows), 2, table)
        by_name = {row[0]: row[1:] for row in rows}
        # Headhunter: switch 1, a Heavy Belt (type 8) with its own seed and base, Liquor Holster
        # the stand-in (the owner's words in #74).
        self.assertEqual(by_name["Headhunter"], ("1", "8", "777002.0", "2.0", '"Liquor Holster"'))
        # Tyrant's Crown: switch 0, a Great Helm (type 0); its stand-in is the owner's default,
        # the validated type-0 unique with the lowest droprate.base (reversible: one cell).
        self.assertEqual(by_name["Tyrant's Crown"], ("0", "0", "777001.0", "7.0", "nullptr"))
        self.assertNotIn("Miner", table, "Miner's Helmet has no panel switch and is not in the table")
        resolve = body(self.code, "static void SignatureResolveStandIns(")
        self.assertIn("BuildAngelicPool(false)", resolve)
        self.assertIn("SigUniqueDropBase(", resolve)
        self.assertIn("base < s.base", resolve, "the default stand-in is the lowest droprate.base")
        # Replan 1: the pool-based ambiguity refusal is gone - it covered kAngelicBases only and
        # left the hit untyped; a hit is typed on the whole triple instead.
        self.assertNotIn("ambiguous", resolve)
        self.assertNotIn("shares its sub/b", SOURCE)
        self.assertIn('"droprate"', body(self.code, "static bool SigUniqueDropBase("))

    def test_the_switch_on_names_the_list_and_the_stand_ins(self):
        install = body(self.code, "static void InstallSignatureAngelicHooks(")
        self.assertIn("SignatureResolveStandIns();", install)
        self.assertIn("SignatureListResolve(", install)
        self.assertIn('Out("signature drops: " + SignatureListLine());', install)
        line = body(self.code, "static std::string SignatureListLine(")
        self.assertIn("list missing (", line)
        self.assertIn("SignatureStandInsText(", line)
        self.assertIn("(not validated)", body(self.code, "static std::string SignatureStandInsText("))

    # ---- the list: Controller_obj's variable, by name ------------------------------------

    def test_the_list_is_resolved_by_name_on_controller_obj(self):
        # The name Live 2's reach check passed on (2026-10-02); the player build reads only it.
        self.assertIn('static const char* kAngelicListVar = "lootListUnique";', self.shipped_code)
        self.assertIn("static std::string g_SigListName = kAngelicListVar;", self.shipped_code)
        self.assertEqual(len(re.findall(r"\bg_SigListName\s*=[^=]", self.shipped_code)), 1,
                         "only the research build's inject lever may name another variable")
        controller = body(self.code, "static bool SignatureController(")
        self.assertIn("GameObject::Controller_obj", controller)
        for builtin in ("asset_get_index", "instance_number", "instance_find"):
            self.assertIn('"%s"' % builtin, controller)
        resolve = body(self.code, "static bool SignatureListResolve(")
        self.assertIn('"variable_instance_exists", { instance, RValue(g_SigListName) }', resolve)
        self.assertIn('"variable_instance_get", { instance, RValue(g_SigListName) }', resolve)

    def test_the_list_is_the_ds_list_at_the_index(self):
        # Replan 2: the variable is an array of six elements and the roll draws from element 5, a ds_list
        # (static reading); the outer array is never the list. The player build reads the
        # constant index, the research build the `at` lever's variable.
        self.assertIn("static const int kAngelicListIndex = 5;", self.shipped_code)
        self.assertIn("static constexpr int g_SigListIndex = kAngelicListIndex;", self.shipped_code)
        self.assertEqual(len(re.findall(r"\bg_SigListIndex\s*=(?!=)", self.shipped_code)), 1,
                         "only the research build's `at` lever moves the index")
        self.assertIn("static const int kSigListMinSize = 10;", self.shipped_code)
        self.assertNotIn("kSigListMinLength", SOURCE)
        resolve = body(self.code, "static bool SignatureListResolve(")
        self.assertIn('"array_get", { list, RValue((double)index) }', resolve)
        self.assertIn("const int index = g_SigListIndex;", resolve)
        self.assertLess(resolve.index("SigListHandle(sub, id, step)"), resolve.index('"ds_list_size"'),
                        "SigListHandle's ds_exists before any ds_list_size")
        self.assertIn("SignatureListShape(list, index, sub, id, size, counts, why, kSigListMinSize)", resolve)
        self.assertNotIn('"array_length", { list }).ToDouble() : -1', resolve, "no flat length read")
        shape = body(self.code, "static bool SignatureListShape(")
        self.assertIn('"array_get", { outer, RValue((double)index) }', shape)
        self.assertLess(shape.index("SigListHandle(sub, id, step)"), shape.index('"ds_list_size"'),
                        "SigListHandle's ds_exists before any ds_list_size")
        self.assertIn("SigEntry(sub, i, e)", shape)
        # Each refusal names its step (delta 1's reasons).
        for reason in ('"is not an array"', '" elements, none at "', '" is not a ds_list ("', '" entries, fewer than "',
                       '" is not three numbers"'):
            with self.subTest(reason=reason):
                self.assertIn(reason, shape)
        entry = body(self.code, "static bool SigEntry(")
        self.assertIn('"ds_list_find_value", { sub, RValue((double)i) }', entry)
        self.assertIn("!= 3", entry, "an entry is exactly three numbers")
        # The live-list gate: ds_exists (2 is ds_type_list) asked of the value as read, after
        # ToDouble refused an unreadable handle; a refusal names the kind it got and the step.
        handle = body(self.code, "static bool SigListHandle(")
        self.assertIn('"ds_exists", { v, RValue(2.0) }', handle, "2 is ds_type_list")
        self.assertLess(handle.index("v.ToDouble()"), handle.index('"ds_exists"'),
                        "only a value that can be a handle is handed to ds_exists")
        self.assertIn('why = "kind=" + kind + ", " + step;', handle, "the ds_list refusal names the kind it got")
        # The gate is kind-free: a ds_list handle may arrive as a number or a reference (Live 2
        # read lootListUnique[5] as a ref), so the kind is only named in the refusal.
        # Outside `refuse`'s diagnostic the function reads no m_Kind at all.
        diag = handle.index("const auto refuse")
        diag_end = handle.index("\n    };", diag)
        gate = strip_comments(handle[:diag] + handle[diag_end:])
        self.assertNotIn("m_Kind", gate, "no kind check decides whether ds_exists is asked")
        # report#2 (#74): converting an array or a string raises a runner error that the catch
        # after ToDouble does not take back, so the kinds that can never be a handle are refused
        # first, through a named predicate. It is a deny-list: it names the five kinds and no kind
        # a handle arrives as, so it cannot refuse a live list held as a number or a reference.
        self.assertIn('if (SigNeverAHandle(v)) return refuse("never a handle");', gate)
        self.assertLess(gate.index("SigNeverAHandle(v)"), gate.index("v.ToDouble()"),
                        "the never-a-handle kinds are refused before any conversion")
        never = strip_comments(body(self.code, "static bool SigNeverAHandle("))
        for kind in ("VALUE_ARRAY", "VALUE_STRING", "VALUE_OBJECT", "VALUE_UNDEFINED", "VALUE_NULL"):
            with self.subTest(refused=kind):
                self.assertIn(kind, never)
        for kind in ("VALUE_REAL", "VALUE_REF", "VALUE_INT32", "VALUE_INT64"):
            with self.subTest(handle_kind=kind):
                self.assertNotIn(kind, never, "a kind a handle arrives as is never refused by its kind")

    # ---- the injection: per roll, under a scope guard inside the roll's hook ------------

    def test_the_push_and_removal_are_a_scope_guard_inside_the_roll_hook(self):
        hook = strip_comments(body(self.shipped, "static RValue& HookAngelicChance("))
        guard = re.search(r"\bSignatureInjectGuard\s+\w+\s*;", hook)
        self.assertIsNotNone(guard, "HookAngelicChance holds the injection with SignatureInjectGuard")
        self.assertLess(hook.index("SignatureRollScope"), guard.start())
        self.assertLess(guard.start(), hook.index("g_OrigAngChance(S"),
                        "the guard is raised before the first original call and, living to the "
                        "end of the function, covers the extra-roll loop")
        struct = body(self.shipped_code, "struct SignatureInjectGuard {")
        self.assertRegex(struct, r"SignatureInjectGuard\(\)\s*\{[^}]*SignatureInjectPush\(\);")
        self.assertRegex(struct, r"~SignatureInjectGuard\(\)\s*\{[^}]*SignatureInjectRemove\(\);")
        # Nothing but the guard pushes or removes.
        self.assertEqual(self.code.count("SignatureInjectPush("), 2)
        self.assertEqual(self.code.count("SignatureInjectRemove("), 2)

    def test_both_switches_off_make_no_call(self):
        push = body(self.code, "static void SignatureInjectPush(")
        gate = push.index("if (!g_SigDetectNative || (!g_TyForced.load() && !g_HhForced.load())) return;")
        for later in ("g_Yytk", "SignatureListResolve(", "SignatureResolveStandIns("):
            self.assertLess(gate, push.index(later), later)
        # The entry (array_create and its three array_set), then onto the ds_list, never the outer array.
        add = push.index('"ds_list_add", { sub, entry }')
        self.assertEqual(push.count('"array_set", { entry,'), 3)
        self.assertLess(push.rindex('"array_set", { entry,'), add)
        self.assertNotIn("array_push", push)
        self.assertIn("SignatureListResolve(list, false, false, &sub)", push)
        self.assertIn("g_SigRollListId = sub;", push)
        self.assertIn("g_SigRollIndex = g_SigListIndex;", push)
        self.assertIn("SignatureSwitchOn(w)", push)
        self.assertIn("InterlockedIncrement(&g_SigInjected)", push)
        self.assertRegex(push, r"for \(int c = 0; c < g_SigCopies\b", "each enabled item pushes g_SigCopies entries")

    def test_the_player_build_pushes_one_copy(self):
        # The copies variable is the constant 1 in the player build; only the research lever
        # `angelicprobe inject copies <k>` (1..400) assigns it.
        self.assertIn("static constexpr int g_SigCopies = 1;", self.shipped_code)
        self.assertEqual(re.findall(r"\bg_SigCopies\s*=(?!=)\s*\w+", self.shipped_code), ["g_SigCopies = 1"],
                         "nothing in the player build assigns the copies")
        command = body(self.code, "static void SigInjectCommand(")
        self.assertIn("g_SigCopies = k;", command)
        self.assertIn("k < 1 || k > 400", command)

    def test_the_held_read_back_comes_before_any_attribution(self):
        push = body(self.code, "static void SignatureInjectPush(")
        held = push.index("SignatureHeldReadBack(g_SigRollListId, g_SigRollIndex, g_SigRollBefore, g_SigRollPushed, seen)")
        self.assertLess(push.rindex('"ds_list_add", { sub, entry }'), held)
        self.assertLess(held, push.index("InterlockedIncrement(&g_SigInjected)"),
                        "injected= counts only pushes the fresh read showed")
        refusal = push[held:]
        self.assertIn("inject: push not visible through Controller_obj.", refusal)
        self.assertIn("InterlockedIncrement(&g_SigAnomalies)", refusal)
        self.assertIn("g_SigRollPushed = 0;", refusal, "a roll whose push was not visible carries nothing")
        # The entries come off the ds_list they went onto, under the tail rule, never by resize.
        cut = refusal.index("SignatureTailHolds(g_SigRollListId, g_SigRollBefore, g_SigRollPushed, len)")
        self.assertLess(cut, refusal.index('"ds_list_delete", { g_SigRollListId,'))
        self.assertNotIn("array_resize", push)
        self.assertIn("g_SigHeldMissLogged", refusal, "logged once per change, not once per roll")
        # A fresh read by name off the instance, never the handle just pushed onto: the element
        # at the same index must be the same ds_list id, and that list hold the tail. Light: never
        # the full shape walk.
        read_back = body(self.code, "static bool SignatureHeldReadBack(")
        self.assertIn("SignatureController(instance, why)", read_back)
        self.assertIn('"variable_instance_get", { instance, RValue(g_SigListName) }', read_back)
        self.assertIn('"array_get", { held, RValue((double)index) }', read_back)
        self.assertIn("got != want", read_back, "the same ds_list id, or a held miss")
        self.assertLess(read_back.index("got != want"), read_back.index("SignatureTailHolds(sub, before, pushed, len)"))
        self.assertNotIn("SignatureListShape", read_back)
        self.assertNotIn("g_SigRollList", read_back)
        # Attribution needs the roll to carry entries, which a refused read-back clears.
        self.assertIn("g_SigRollPushed > 0", body(self.code, "static RValue& Hook_CreateDefaultParams("))

    def test_the_removal_cuts_only_its_own_tail(self):
        remove = body(self.code, "static void SignatureInjectRemove(")
        self.assertIn("const RValue sub = g_SigRollListId;", remove, "off the ds_list the entries went onto")
        self.assertLess(remove.index("SignatureTailHolds(sub, before, pushed, len)"), remove.index('"ds_list_delete"'))
        self.assertIn('"ds_list_delete", { sub, RValue((double)(len - 1 - i)) }', remove, "the last entry, once per pushed entry")
        self.assertIn("for (int i = 0; i < pushed; ++i)", remove)
        self.assertLess(remove.index('"ds_list_delete"'), remove.rindex('"ds_list_size", { sub }'), "the size is read back after")
        self.assertIn("if (len == before) return;", remove)
        self.assertNotIn("array_resize", remove)
        self.assertIn("inject: list changed during the roll, left as found", remove)
        self.assertIn("InterlockedIncrement(&g_SigAnomalies)", remove)
        # The tail check covers every copy of every enabled item, in push order, on the ds_list.
        tail = body(self.code, "static bool SignatureTailHolds(")
        self.assertLess(tail.index('"ds_exists", { sub, RValue(2.0) }'), tail.index('"ds_list_size", { sub }'))
        self.assertIn("len != before + pushed", tail)
        self.assertIn("c < g_SigRollCopies[w]", tail)
        self.assertIn("SigEntry(sub, at++, e)", tail)
        self.assertIn("e[0] != (double)kSignatureItems[w].t", tail, "the tail is matched on the whole triple")

    # ---- typing the hit, attribution and the rewrite -----------------------------------

    def test_the_typing_hook_is_installed_by_sdk_name_inside_the_installer(self):
        install = body(self.code, "static void InstallSignatureAngelicHooks(")
        self.assertRegex(install, r"HookOneScript\(SdkShortScriptName\(HeroSiege::Scripts::gml_Script_GetUniqueRepoStruct\),\s*"
                                  r"\"fp_sig_urepo\",\s*\(PVOID\)Hook_GetUniqueRepoStruct,\s*&g_Orig_GetUniqueRepoStruct,\s*&native\)")
        self.assertEqual(self.code.count("(PVOID)Hook_GetUniqueRepoStruct"), 1, "the typing hook is installed in one place")
        self.assertGreaterEqual(install.count("HookOneScript("), 3)
        self.assertIn("savedRoute(g_Orig_GetUniqueRepoStruct)", install)
        self.assertIn('static void InstallSignatureAngelicHooks(', self.shipped_code)
        self.assertIn("gml_Script_GetUniqueRepoStruct", body(self.shipped_code, "static void InstallSignatureAngelicHooks("),
                      "the typing hook ships: the gate needs it")

    def test_the_typing_hook_records_only_inside_the_roll(self):
        hook = body(self.code, "static RValue& Hook_GetUniqueRepoStruct(")
        guard = hook.index("if (g_SigRollDepth > 0)")
        self.assertLess(guard, hook.index("g_SigRepoSeen ="))
        self.assertIn("return g_Orig_GetUniqueRepoStruct ? g_Orig_GetUniqueRepoStruct(S, O, R, argc, A) : R;", hook,
                      "it always calls through")
        for k in ("SigNumber(*A[0], t)", "SigNumber(*A[1], s)", "SigNumber(*A[2], b)"):
            self.assertIn(k, hook)
        reset = body(self.code, "static void SignatureHitReset(")
        for name in ("g_SigRepoSeen = false", "g_SigHitTyped = false"):
            self.assertIn(name, reset, "the record is reset before each original call")

    def test_a_hit_is_typed_or_untyped(self):
        hook = body(self.code, "static RValue& Hook_CreateDefaultParams(")
        self.assertIn("g_SigHitTyped = sub && b && g_SigRepoSeen && g_SigRepoSub == g_SigLastSub && g_SigRepoB == g_SigLastB;", hook)
        self.assertIn("g_SigHitType = g_SigHitTyped ? g_SigRepoType : -1.0;", hook)
        self.assertIn("if (!g_SigHitTyped) InterlockedIncrement(&g_SigUntyped);", hook)
        self.assertIn("if (g_SigRollDepth > 0 && g_SigHitTyped && g_SigRollPushed > 0)", hook,
                      "an untyped hit is never attributed nor rewritten")
        self.assertIn('"untyped "', body(self.code, "static void SignatureAfterHit("),
                      "the hit line names the typed triple or says untyped")

    def test_a_hit_is_attributed_after_the_game_built_the_parameters(self):
        hook = body(self.code, "static RValue& Hook_CreateDefaultParams(")
        self.assertIn("RValue& r = g_Orig_CreateDefaultParams ? g_Orig_CreateDefaultParams(S, O, R, argc, A) : R;", hook)
        self.assertLess(hook.index("g_Orig_CreateDefaultParams(S, O, R, argc, A)"), hook.index("SignatureAttributeHit()"))
        self.assertIn("g_SigRollPushed > 0", hook)
        attribute = body(self.code, "static void SignatureAttributeHit(")
        self.assertIn("g_SigHitType != (double)kSignatureItems[w].t", attribute, "attribution compares the type")
        self.assertIn("(double)g_SigStandIn[w].sub != g_SigLastSub || (double)g_SigStandIn[w].b != g_SigLastB", attribute)
        self.assertIn("share += g_SigRollCopies[w];", attribute)
        self.assertIn("std::uniform_int_distribution<int>(0, n + share - 1)", attribute, "m·k in n + m·k")
        self.assertIn("InterlockedIncrement(&g_SigOurHits)", attribute)
        # Session 5: CreateDefaultParams' struct has no `a` (Live 2), and LootGroundCreate stores
        # its own `a` after it, so attribution only hands the hit to the rewrite point.
        self.assertIn("g_SigHitItem = which;", attribute)
        self.assertNotIn("SignatureRewriteParams(", attribute, "the rewrite no longer runs at CreateDefaultParams' return")
        self.assertIn("SignatureAttributeHit();", hook)

    def test_the_rewrite_runs_at_create_item_new_entry(self):
        # The rewrite point (static reading, Session 5): CreateItemNew's entry, in the item hook's
        # pre-call slot beside GemsBeforeCreate, on the instance's itemDefinitionStruct.
        macro = self.code[self.code.index("#define ITEM_CREATE_HOOK(NAME)"):]
        macro = macro[:macro.index("return _res; \\\n    }")]
        pre = macro.index("SignatureBeforeCreate(argc, A);")
        self.assertLess(macro.index("GemsBeforeCreate(argc, A);"), pre)
        self.assertLess(pre, macro.index("g_Orig_##NAME(S, O, R, argc, A)"), "the record is rewritten before the original builds from it")
        self.assertIn("if (_final && g_TruthDepth == 0) {", macro, "only on CreateItemNew's outermost call")
        before = body(self.code, "static void SignatureBeforeCreate(")
        self.assertIn("g_SigRollDepth <= 0 || !g_SigHitSeen || g_SigHitAtPoint", before, "once per hit, inside the roll")
        self.assertIn('RValue("itemDefinitionStruct")', before)
        self.assertIn("SignatureRewriteParams(record, kSignatureItems[which], g_SigHitWhy)", before)
        self.assertIn("if (which < 0) return;", before, "the game's own hit is never touched")
        self.assertLess(before.index("if (which < 0) return;"), before.index("SignatureRewriteParams("))
        self.assertIn("SignatureRefuse(which,", before)
        self.assertIn("++g_SigPending[which]", before)
        self.assertNotIn("SignatureBeforeCreate", body(self.code, "static RValue& Hook_CreateDefaultParams("))
        # The point is reached only through a detour (LootGroundCreate calls CreateItemNew
        # directly), so the installer hooks it by its SDK name and counts its route into the gate.
        install = strip_comments(body(self.code, "static void InstallSignatureAngelicHooks("))
        self.assertRegex(install, r"HookOneScript\(SdkShortScriptName\(HeroSiege::Scripts::gml_Script_CreateItemNew\),\s*"
                                  r"\"fp_sig_citemnew\",\s*\(PVOID\)Hook_CreateItemNew,\s*&g_Orig_CreateItemNew,\s*&native\)")
        self.assertIn("itemRoute = savedRoute(g_Orig_CreateItemNew);", install)
        self.assertIn("gml_Script_CreateItemNew", body(self.shipped_code, "static void InstallSignatureAngelicHooks("),
                      "the rewrite point ships: the gate needs it")
        self.assertIn("SignatureBeforeCreate(argc, A);", self.shipped_code)

    def test_the_rewrite_reads_back_what_it_wrote(self):
        rewrite = body(self.code, "static bool SignatureRewriteParams(")
        self.assertIn('{ "a", "b", "c", "j" }', rewrite)
        self.assertIn("{ item.a, item.b, 0.0, 0.0 }", rewrite)
        exists = rewrite.index('"variable_struct_exists"')
        write = rewrite.index('"variable_struct_set", { params, RValue(kFields[k]), RValue(want[k]) }')
        read_back = rewrite.rindex('"variable_struct_get"')
        self.assertLess(exists, write)
        self.assertLess(write, read_back, "the fields are read back after the write")
        self.assertIn("did not read back", rewrite)
        self.assertIn("restore()", rewrite, "a refusal puts every field back")
        # A field the record lacks is created, not refused; a refusal removes it again.
        self.assertNotIn("no field ", rewrite, "a missing field is created, not a refusal")
        self.assertIn('"variable_struct_remove", { params, RValue(kFields[k]) }', rewrite)
        self.assertIn("if (had[k])", rewrite)
        self.assertIn("SigJson(record)", body(self.code, "static void SignatureBeforeCreate("),
                      "a refusal logs the record's JSON")

    def test_a_refused_rewrite_latches_its_item_off(self):
        # Both builds: the first refusal turns that item off for the session (no more copies
        # pushed), the status says `rewrite refused: <why>`, and `sigdrop status` counts refused=.
        refuse = body(self.code, "static void SignatureRefuse(")
        self.assertIn("InterlockedIncrement(&g_SigRefusals)", refuse)
        self.assertIn("g_SigRefused[which] = true;", refuse)
        self.assertIn('" refused ("', refuse)
        gate = body(self.shipped_code, "static bool SignatureSwitchOn(")
        self.assertIn("g_SigRefused[which]", gate)
        reason = body(self.shipped_code, "static std::string SignatureOffReason(")
        self.assertIn('"rewrite refused: " + g_SigRefusedWhy[which]', reason)
        status = body(self.shipped_code, "static void SigDropStatus()")
        self.assertLess(status.index('" ourHits="'), status.index('" refused=" + std::to_string(g_SigRefusals)'))
        self.assertLess(status.index('" refused="'), status.index('" untyped="'))
        # A hit of ours that never reached the rewrite point is a refusal too.
        self.assertIn("SignatureRefuse(which,", body(self.shipped_code, "static void SignatureAfterHit("))
        # `headhunter status` / the tyrant line stop reading on.
        self.assertIn('SignatureRefused(1) ? "refused"', body(self.shipped_code, "static void HeadhunterStatus("))
        self.assertIn('SignatureRefused(0) ? "refused"', body(self.shipped_code, "static void TyrantStatus()"))

    def test_built_is_what_the_forge_hook_saw_the_game_build(self):
        forge = body(self.code, "static bool TryApplyCustomForge(")
        self.assertLess(forge.index("CustomForgeMatches(entry, *candidate, definition)"),
                        forge.index("if (finalPass) SignatureNoteBuilt(entry.selector);"))
        note = body(self.code, "static void SignatureNoteBuilt(")
        self.assertIn("g_SigPending[w] <= 0", note)
        self.assertIn("InterlockedIncrement(&g_SigBuilt)", note)

    def test_nothing_on_the_roll_path_spawns(self):
        # One hit, one item, placed by the game: in the player build SpawnSignatureItem is
        # reached from `sigdrop`'s kill hook alone.
        for signature in ROLL_PATH:
            with self.subTest(function=signature):
                self.assertNotIn("SpawnSignatureItem(", strip_comments(body(self.shipped, signature)))
        callers = [m.start() for m in re.finditer(r"SpawnSignatureItem\(", self.shipped_code)]
        self.assertEqual(len(callers), 2, "the definition and SignatureDropOnKill")
        self.assertIn("SpawnSignatureItem(", body(self.shipped_code, "static void SignatureDropOnKill("))

    # ---- detection: CreateDefaultParams, by name, from the switches only ---------------

    def test_create_default_params_is_hooked_once_inside_the_installer(self):
        self.assertEqual(self.code.count('HookOneScript("CreateDefaultParams"'), 1)
        install = body(self.code, "static void InstallSignatureAngelicHooks(")
        self.assertIn('HookOneScript("CreateDefaultParams"', install)
        self.assertIn("&g_Orig_CreateDefaultParams", install)
        self.assertIn("(PVOID)HookAngelicChance, &g_OrigAngChance", install)
        self.assertRegex(install, r"if \(!g_OrigAngChance\)")

    def test_the_probe_row_and_the_typing_hook_never_both_detour(self):
        # Design step 3's coexistence rule, as chosen: whichever comes second refuses and says
        # so. The typing hook second: route `held-by-angelicprobe`, the gate stays off. The
        # probe's unique-repo row second: `blocked`, before any detour is attempted.
        install = body(SOURCE, "static void InstallSignatureAngelicHooks(")
        research = install[install.index("#ifndef FORGEPACT_RELEASE"):install.index("#endif", install.index("#ifndef FORGEPACT_RELEASE"))]
        self.assertIn("probeHolds = !g_Orig_GetUniqueRepoStruct && ApRollHoldsUniqueRepo();", research)
        self.assertIn('repoRoute = "held-by-angelicprobe";', install)
        self.assertLess(install.index("if (probeHolds)"), install.index("(PVOID)Hook_GetUniqueRepoStruct"))
        self.assertNotIn("ApRollHoldsUniqueRepo", self.shipped)
        holds = body(self.code, "static bool ApRollHoldsUniqueRepo()")
        self.assertIn('"unique-repo"', holds)
        self.assertIn("r.tramp != nullptr", holds)
        attach = body(self.code, "static void ApRollAttach(")
        refusal = attach.index('std::string_view(r.id) == "unique-repo" && g_Orig_GetUniqueRepoStruct')
        self.assertLess(refusal, attach.index("MmCreateHook("))
        self.assertIn("kApRollBlocked", attach[refusal:attach.index("return;", refusal)])

    def test_the_installer_is_in_the_player_build(self):
        self.assertIn("static void InstallSignatureAngelicHooks(", self.shipped_code)
        self.assertIn('HookOneScript("CreateDefaultParams"', self.shipped_code)

    def test_installed_by_the_switches_never_at_startup(self):
        for signature in ("static void InstallHook()", "EXPORTED AurieStatus ModuleInitialize"):
            with self.subTest(function=signature):
                self.assertNotIn("InstallSignatureAngelicHooks", body(self.code, signature))
        frame = body(self.code, "void FrameCallback(FWFrame& FrameContext)")
        for name in ("InstallSignatureAngelicHooks", "SignatureInject", "SignatureListResolve", "g_SigGame"):
            self.assertNotIn(name, frame, "nothing new runs every frame")
        # Owner, 2026-10-02 ("Panel switch only"): a forged item's auto-arm turns its mechanic
        # on at every launch but never the drop, so neither it nor the enable path it shares
        # with `headhunter on` installs the detection.
        for signature in ("static void EnableHeadhunter()", "static void HeadhunterAutoArm()", "static void TyrantAutoArm()"):
            with self.subTest(function=signature):
                self.assertNotIn("InstallSignatureAngelicHooks", body(self.code, signature))
        # Only the panel switch's own command, `headhunter force` / `tyrant force`, installs it:
        # every line of the command that does names the `force` value and no other.
        command = body(self.code, "static bool HandleHeadhunterCommand(")
        for verb in ("headhunter", "tyrant"):
            with self.subTest(command=verb):
                start = command.index('lc == "%s"' % verb)
                branch = command[start:command.index("} else if (lc ==", start)]
                lines = [line for line in branch.splitlines() if "InstallSignatureAngelicHooks" in line]
                self.assertTrue(lines, "the %s command's force path installs the detection" % verb)
                for line in lines:
                    self.assertIn('== "force"', line)
                    for other in ('"on"', '"1"', '"off"', '"0"'):
                        self.assertNotIn(other, line, "only `force` installs the detection, not on/1 or off")
        # The research levers install it too, so gameHits= counts with both switches off.
        self.assertIn("InstallSignatureAngelicHooks();", body(self.code, "static void AngelicHitCommand("))

    def test_the_gate_reads_the_panel_switch_and_the_list(self):
        # The flags `tyrant force` / `headhunter force` set and `off` clears, which the panel
        # sends; the auto-arm from a forged item sets only g_TyEnabled / g_HhEnabled.
        gate = body(self.code, "static bool SignatureSwitchOn(")
        for name in ("g_TyForced", "g_HhForced", "g_SigDetectNative", "g_SigListOk", "g_SigStandIn[which].ok"):
            self.assertIn(name, gate)
        self.assertNotIn("Enabled", gate)
        # g_SigDetectNative is all four routes: a hit the plugin cannot see, type or rewrite never arms.
        install = body(self.code, "static void InstallSignatureAngelicHooks(")
        self.assertIn("g_SigDetectNative = detoured == cdpRoute && detoured == rollRoute && detoured == repoRoute && detoured == itemRoute;", install)
        self.assertIn("g_SigRefused[which]", gate, "a refused item is latched off")
        # The auto-arm log lines say what the gate does, not what the mechanic does.
        self.assertIn("SignatureSwitchOn(0)", body(self.code, "static void TyrantAutoArm()"))
        self.assertIn("SignatureSwitchOn(1)", body(self.code, "static void HeadhunterAutoArm()"))

    # ---- the roll-in-progress state is a scope guard ------------------------------------

    def test_the_roll_state_is_a_scope_guard_in_the_player_build(self):
        hook = strip_comments(body(self.shipped, "static RValue& HookAngelicChance("))
        self.assertNotIn("ApRoll", hook)
        for token in ("++g_SigRollDepth", "--g_SigRollDepth", "g_SigRollDepth++", "g_SigRollDepth--"):
            self.assertNotIn(token, hook, "a bare ++/-- leaves the state raised when the game's roll throws")
        guard = re.search(r"\b([A-Za-z_]\w*Scope)\s+\w+\s*;", hook)
        self.assertIsNotNone(guard, "HookAngelicChance declares the roll-in-progress guard")
        self.assertFalse(guard.group(1).startswith("ApRoll"))
        self.assertLess(guard.start(), hook.index("g_OrigAngChance(S"),
                        "the guard is raised before the first original call and, living to the "
                        "end of the function, covers the extra-roll loop")
        struct = re.search(r"struct %s \{(.*?)\n\};" % guard.group(1), self.shipped_code, re.S)
        self.assertIsNotNone(struct)
        self.assertRegex(struct.group(1), r"%s\(\)\s*\{\s*\+\+g_SigRollDepth;" % guard.group(1))
        self.assertRegex(struct.group(1), r"~%s\(\)\s*\{\s*--g_SigRollDepth;" % guard.group(1))

    def test_the_research_depth_guard_is_untouched(self):
        full = strip_comments(body(SOURCE, "static RValue& HookAngelicChance("))
        self.assertRegex(full, r"\bApRollChanceDepthScope\s+\w+\s*;")

    # ---- counters and status -----------------------------------------------------------

    def test_sigdrop_status_carries_the_live_procedure_tokens(self):
        status = body(self.code, "static void SigDropStatus()")
        # Design step 7's banner, in this order (the live procedure's `control` reads it whole).
        ordered = ('" | game roll: gameRolls="', '" gameHits="', '" injected="', '" ourHits="', '" refused="', '" untyped="',
                   '" built="', '" crown="', '" belt="', '" anomalies="', '" list=" + SignatureListText()',
                   '" gate=tyrant:"', '",headhunter:"', '" cdpCalls="', '" detect="')
        at = [status.index(token) for token in ordered]
        self.assertEqual(at, sorted(at), "the banner's tokens are out of order")
        for token in ("force ", " | rolls=", " drops=", " fails="):
            self.assertIn(token, status)
        self.assertNotIn("#ifndef", status, "every banner token ships in both builds")
        text = body(self.code, "static std::string SignatureListText(")
        for value in ('"none"', '"missing"'):
            self.assertIn(value, text)
        # `<name>[<index>]:<size>` (replan 2): the index rides inside list=, on both status lines.
        self.assertIn('g_SigListName + "[" + std::to_string(g_SigListIndex) + "]:" + std::to_string(g_SigListLen)', text)
        self.assertIn('" list=" + (g_SigListOk ? SignatureListText() : std::string("none"))', body(self.code, "static void SigInjectStatus("))
        self.assertIn("SignatureListText()", body(self.code, "static std::string SignatureListLine("))

    def test_the_research_tokens_stay_out_of_the_player_build(self):
        # Design steps 7-8: the reach and typing controls, the copies and the built type are
        # Live 1's instruments, research build only.
        for token in ("standinPicks=", "heldMiss=", "typeAgree=", "typeDisagree=", "builtType=", " copies="):
            with self.subTest(token=token):
                self.assertIn(token, SOURCE)
                self.assertNotIn(token, self.shipped)
        status = body(self.code, "static void SigInjectStatus(")
        ordered = ('"angelicprobe inject: mode="', '" copies="', '" list="', '" standins="', '" injected="', '" ourHits="',
                   '" untyped="', '" standinPicks="', '" heldMiss="', '" typeAgree="', '" typeDisagree="',
                   '" built="', '" removed="', '" anomalies="')
        at = [status.index(token) for token in ordered]
        self.assertEqual(at, sorted(at), "`angelicprobe inject status`'s tokens are out of order")
        # standinPicks= counts the pair alone, before and whatever the typing said.
        hook = body(self.code, "static RValue& Hook_CreateDefaultParams(")
        self.assertIn("InterlockedIncrement(&g_SigStandinPicks)", hook)
        self.assertLess(hook.index("g_SigStandinPicks"), hook.index("g_Orig_CreateDefaultParams(S, O, R, argc, A)"))
        # typeAgree= / typeDisagree= come from the Custom Forge hook's final pass, the roll in progress.
        forge = body(self.code, "static bool TryApplyCustomForge(")
        self.assertIn("SignatureNoteBuiltType(*candidate);", forge)
        note = body(self.code, "static void SignatureNoteBuiltType(")
        self.assertIn("g_SigRollDepth <= 0", note)
        self.assertIn('TryStructNumber(item, "itemType", t)', note)
        self.assertIn("!g_SigHitTyped || g_SigHitRewritten", note, "only a typed hit that was not rewritten is compared")

    def test_both_status_lines_carry_the_detection_route(self):
        # The live procedure reads detect= and cdpCalls= before it trusts gameHits=0: an
        # unreachable hook must not read as a roll that never hit.
        for signature in ("static void SigDropStatus()", "static void AngelicHitStatus()"):
            with self.subTest(status=signature):
                status = body(self.code, signature)
                self.assertRegex(status, r'" cdpCalls=" \+ std::to_string\(g_SigCdpCalls\) \+ " detect=" \+ g_SigDetectRoute\);\s*\}$')
        hook = body(self.code, "static RValue& Hook_CreateDefaultParams(")
        self.assertLess(hook.index("InterlockedIncrement(&g_SigCdpCalls)"), hook.index("g_SigRollDepth"),
                        "cdpCalls counts every call that reaches the hook, before the roll check")
        self.assertIn('static std::string g_SigDetectRoute = "off";', self.shipped_code)
        # `detoured` only when all four are; otherwise the first that is not, by name.
        install = strip_comments(body(self.code, "static void InstallSignatureAngelicHooks("))
        self.assertRegex(install, r"g_SigDetectRoute = detoured != cdpRoute \? std::string\(cdpRoute\)\s*"
                                  r": detoured != rollRoute \? std::string\(\"DropItemAngelicChance:\"\) \+ rollRoute\s*"
                                  r": detoured != repoRoute \? std::string\(\"GetUniqueRepoStruct:\"\) \+ repoRoute\s*"
                                  r": detoured != itemRoute \? std::string\(\"CreateItemNew:\"\) \+ itemRoute\s*"
                                  r": detoured;")

    def test_one_angelic_hit_line_per_hit(self):
        hook = body(self.code, "static RValue& HookAngelicChance(")
        self.assertIn("SignatureAfterHit(S, x, y);", hook)
        self.assertEqual(hook.count("SignatureHitReset();"), 2, "the hit state is cleared before each original call")
        hit = body(self.code, "static void SignatureAfterHit(")
        self.assertIn('"angelic hit: picked "', hit)
        self.assertIn('" built by the game"', hit)
        self.assertEqual(hit.count("Out("), 1)

    # ---- the research levers never reach a player -----------------------------------

    def test_the_hit_levers_are_research_build_only(self):
        self.assertIn('"angelicprobe hit', SOURCE)
        self.assertNotIn("angelicprobe hit", self.shipped)
        command = body(self.code, "static void AngelicHitCommand(")
        for lever in ("chance", "rate", "show", "share", "off", "status"):
            self.assertIn('"%s"' % lever, command)
        self.assertNotIn("AngelicHitCommand", self.shipped)
        # Live 3's `record` dump (Session 5) replaces the parameter lines and ships in neither form.
        for token in ("angelic hit record: vanilla ", "angelic hit record: before ", "angelic hit record: after ",
                      "angelic hit built: itemType=", "g_SigShowLeft", "g_SigHitShow"):
            with self.subTest(token=token):
                self.assertIn(token, SOURCE)
                self.assertNotIn(token, self.shipped)
        self.assertNotIn("angelic hit params", SOURCE)
        self.assertIn('" | show=" + std::to_string(g_SigShowLeft)', body(self.code, "static void AngelicHitStatus()"))

    def test_hit_share_is_a_no_op(self):
        # The beside design's share lever has nothing left to set: the game's picker gives each
        # item one entry's share. It stays a subcommand that says so.
        command = body(self.code, "static void AngelicHitCommand(")
        start = command.index('if (lever == "share")')
        branch = command[start:command.index("} else if", start)]
        self.assertIn("no-op", branch)
        self.assertNotIn("=", branch.replace("==", ""), "the share branch changes nothing")

    def test_the_inject_levers_are_research_build_only(self):
        for token in ('"angelicprobe inject', "lootDelta=", "g_SigReplaceMode", "SigReplaceStandIn", "SigInjectCommand",
                      "SigListScan", "g_SigRemoved", "SigListDump", "angelicprobe list dump"):
            with self.subTest(token=token):
                self.assertIn(token, SOURCE)
                self.assertNotIn(token, self.shipped)
        command = body(self.code, "static void SigInjectCommand(")
        for lever in ('"name"', '"auto"', '"at"', '"copies"', '"mode"', '"inject"', '"replace"', '"status"'):
            self.assertIn(lever, command)
        # One spelling of the scan: `angelicprobe inject auto`. The usage line is exact (the
        # research doc's procedure quotes it), and `name auto` is refused, naming the right
        # spelling, before anything is assigned - a slip can never set the literal name "auto".
        usage = "angelicprobe inject: name <var> | auto | at <k> | copies <k> | mode inject|replace | status"
        self.assertIn('"%s"' % usage, command)
        self.assertEqual(SOURCE.count(usage), 1)
        self.assertEqual(SOURCE.count("angelicprobe inject: name <var>"), 1)
        # `at <k>` (replan 2) sets the sub-list index, 0..31 only, and resolves again.
        start = command.index('lever == "at"')
        branch = command[start:command.index("} else if", start)]
        self.assertIn("k < 0 || k > 31", branch)
        self.assertLess(branch.index("k < 0 || k > 31"), branch.index("g_SigListIndex = k;"))
        self.assertIn("nothing changed", branch)
        self.assertIn("SignatureListResolve(list, true, true)", branch)
        refusal = command.index('if (lever == "name" && Lower(value) == "auto")')
        self.assertLess(refusal, command.index("g_SigListName = name;"))
        branch = command[refusal:command.index("} else if", refusal)]
        self.assertIn("angelicprobe inject auto", branch)
        self.assertIn("nothing changed", branch)
        self.assertNotIn("g_SigListName", branch)
        self.assertNotIn("inject name auto", SOURCE)
        probe = body(self.code, "static void ApRollCommand(")
        self.assertIn('sub == "inject"', probe)
        self.assertIn("SigInjectCommand(TrimCopy(rest).substr(6))", probe, "a variable name keeps its case")
        # The replace mode is the only path from the roll to SpawnSignatureItem, and it is research-only.
        self.assertIn("SpawnSignatureItem(which, x, y, S)", body(self.code, "static std::string SigReplaceStandIn("))
        self.assertIn('"instance_destroy"', body(self.code, "static std::string SigReplaceStandIn("))

    def test_the_scan_and_the_dump_read_the_layout(self):
        # Replan 2, delta 4: a candidate is an array whose element at the index is a live
        # ds_list of triples (the gate's own shape check), lootListUnique preferred.
        scan = body(self.code, "static std::string SigListScan(")
        self.assertIn("SignatureListShape(value, g_SigListIndex, sub, id, size, counts, shape, kSigListMinSize)", scan)
        for token in ('" at="', '" ds_list_size="', '" first="', '" standins="', '"angelicprobe list: rejected "', '" why="'):
            self.assertIn(token, scan)
        self.assertIn('name == "lootListUnique"', scan, "best is lootListUnique when it is a candidate")
        self.assertIn('best + "[" + std::to_string(g_SigListIndex) + "]:" + std::to_string(bestN)', scan)
        # Delta 5: `angelicprobe list dump [<var>]`, dispatched from the `list` subcommand, the
        # name keeping its case; read-only, two levels down.
        listing = body(self.code, "static void ApRollList(")
        self.assertIn('== "dump"', listing)
        self.assertIn("SigListDump(", listing)
        dump = body(self.code, "static void SigListDump(")
        self.assertIn('std::string("lootListUnique")', dump, "lootListUnique when no name is given")
        for token in ('" kind=array array_length="', '", not an array"', '" ds_list=no"', '" ds_list=yes:"', '" triples="',
                      '" first="', '" standins="', '" elements="', '" ds_lists="', '" triple-lists="', '"EXCEPTION at ["'):
            self.assertIn(token, dump)
        self.assertIn("i < len && i < 32", dump)
        self.assertIn("size < 2000 ? size : 2000", dump)
        self.assertLess(dump.index("SigListHandle(e, id, step)"), dump.index('"ds_list_size", { e }'),
                        "SigListHandle's ds_exists before any ds_list_size")
        for write in ("ds_list_add", "ds_list_delete", "array_set", "array_push", "array_resize", "variable_instance_set"):
            self.assertNotIn(write, dump, "the dump writes nothing")

    def test_no_new_player_command(self):
        allowlist = re.search(r"kPlayerCommands\s*=\s*\{(?P<body>.*?)\};", SOURCE, re.S)
        self.assertIsNotNone(allowlist)
        for verb in ("angelichit", "angelicprobe", "inject"):
            self.assertNotIn(verb, allowlist.group("body"))
        run = body(self.code, "static void RunCommand(const std::string& line)")
        self.assertNotIn('lc == "inject"', run)


class AngelicHitPlayerTextTests(unittest.TestCase):
    """What the `player-text` item wrote, as assertions (the plan's checks)."""

    def test_panel_text(self):
        loot = read(PANEL_SRC / "tabs" / "Loot.svelte")
        card = re.search(r'id="angelicCard".*?id="angelicnote"', loot, re.S)
        self.assertIsNotNone(card)
        self.assertNotIn("Headhunter", card.group(0), "the slider does not drop Headhunter")
        self.assertNotIn("Tyrant", card.group(0), "the slider does not drop Tyrant's Crown")
        mods = read(PANEL_SRC / "tabs" / "Mods.svelte")
        for anchor in ('id="headhunter"', 'id="tyrant"'):
            with self.subTest(switch=anchor):
                at = mods.index(anchor)
                self.assertIn("Angelic", mods[at - 1500:at])
                self.assertNotIn("beside", mods[at - 1500:at], "one item per hit, never beside")

    def test_readme(self):
        text = read(ROOT / "README.md")
        start = text.index("\n### Signature drops\n")
        end = text.find("\n### ", start + 1)
        section = text[start:] if end < 0 else text[start:end]
        for token in ("Angelic roll", "Headhunter", "Tyrant", "sigdrop", "Liquor Holster"):
            self.assertIn(token, section)
        self.assertIn("switch", section.lower())
        self.assertNotIn("beside", section)
        row = [line for line in text.splitlines() if line.startswith("| **Angelic / Unholy Drops")][0]
        self.assertNotIn("Headhunter", row)
        self.assertNotIn("Tyrant", row)

    def test_release_notes(self):
        # Published notes leave main through forgepact-notes-cleanup.yml.
        if not (ROOT / "release-notes-v2.2.0.md").is_file():
            self.skipTest("release-notes-v2.2.0.md is published and gone from main")
        notes = read(ROOT / "release-notes-v2.2.0.md")
        self.assertTrue(notes.startswith("# ForgePact 2.2.0"))
        for token in ("Release date:", "Headhunter", "Tyrant's Crown", "Angelic", "Liquor Holster", "## How to update"):
            self.assertIn(token, notes)
        self.assertNotIn("beside", notes)


if __name__ == "__main__":
    unittest.main()
