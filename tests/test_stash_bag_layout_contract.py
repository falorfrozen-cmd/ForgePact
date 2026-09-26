"""Contract tests for `menulayout`'s stash and bag rows, and for the
`craftprobe` additions the stash and bag research launch needs.

The hub's five stash and bag tools (`hs_give_item`, `hs_stash_open`,
`hs_stash_close`, `hs_stash_tab`, `hs_bag_tab`) set up game state for other
tests, so each acts through the runtime by name - the game's own routine,
called with the shape the research launch logged - and keeps a click on a
listed row only as the fallback a § Decision line names. (The by-name move
between the bag and the stash, `hs_move_item`, was split out to
`hs-drive-stash-move-research`.)
`menulayout` is the reader that proves each of them: a tab, a grid or a window
is identified by the variables it prints. So the listing grows: the stash,
bag and inventory objects join the candidate table
(tests/test_menu_layout_contract.py pins the set), a row prints more of the
variables an instance carries, and an array-valued variable prints as
`[a,b,...]`. It still ships in the player build, so these tests also pin that
it stays a reader - and that it calls no game script, directly or through a
builtin that would run one.

The mechanisms the hub tools rely on are measured in one research launch and
recorded in docs/stash-bag-layout-research.md; its headings, its § Results
columns and its decision keys are pinned here so the hub is never written
against a key the document lacks. The research-build instrument those
launches replay the game's calls with (`craftprobe`'s `other:<id>`,
`methods`, `callm`'s `inst` holder and three rows for the first; the `id:<n>`
and `obj:<Name>` arguments and the `UiACloseButton` row for the second) is
pinned here too, and stays out of the player build.
"""
import os
import re
import shutil
import subprocess
import sys
import unittest
from pathlib import Path

TESTS_DIR = Path(__file__).resolve().parent
FORGEPACT_DIR = TESTS_DIR.parent
PLUGIN_SRC = FORGEPACT_DIR / "plugin" / "ModuleMain.cpp"
NOTES = FORGEPACT_DIR / "release-notes-v1.4.7.md"
README = FORGEPACT_DIR / "README.md"
DOC = FORGEPACT_DIR / "docs" / "stash-bag-layout-research.md"
HARNESS = TESTS_DIR / "menu_layout_value_text.cpp"

if str(TESTS_DIR) not in sys.path:
    sys.path.insert(0, str(TESTS_DIR))

from test_menu_layout_contract import FORBIDDEN, doc_section, helper_signatures  # noqa: E402
from test_release_hook_contract import function_body, strip_comments, strip_research_blocks  # noqa: E402

# The optional variables a row prints, in order, each only when the instance
# carries it; `text` stays last and is not one of them. The six first names
# are the character-select set; the rest are what the stash and bag research
# expects to identify tabs and grids by (docs/stash-bag-layout-research.md,
# § Instrument). `activationFunc` is deliberately absent: a method value has
# no stable text. `tabSelected` joined after live 2: it is the bag sub-tab
# state (§ Decision, bagTabState), which `stashTabSelected` is not.
OPTIONAL_FIELDS = (
    "label", "name", "slot", "index", "page", "selected",
    "uiNodeCallstack", "activationArgs", "enabled", "tabNumber", "tabType",
    "stashTabSelected", "tabSelected", "nodeGridWidth", "nodeGridHeight", "gridScale", "gridName",
)

# The five player verbs (Step 5): verb -> (its Handle* helper, its command function).
VERBS = {
    "playerwarp": ("HandlePlayerWarpCommand", "static void PlayerWarpCommand("),
    "stashtab": ("HandleStashTabCommand", "static void StashTabCommand("),
    "bagtab": ("HandleBagTabCommand", "static void BagTabCommand("),
    "stashclose": ("HandleStashCloseCommand", "static void StashCloseCommand("),
    "giveitem": ("HandleGiveItemCommand", "static void GiveItemCommand("),
}
VERBS_BLOCK = ("// ---- playerwarp, stashtab, bagtab, stashclose, giveitem: the stash and bag player verbs (toolkit #147)",
               "// ---- end stash and bag player verbs")
# The game routines the tab and close verbs call, each by its SDK constant.
VERB_SCRIPTS = {"UiACloseButton", "UiAStashTabClick", "UiAStashMaterialTabClick",
                "UiAInventoryMaterialTabClick", "UiAInventorySocketTabClick"}
# giveitem's order: craftmats' own constants, CmMakeUnit's order.
GIVE_ORDER = ("kCmSaveStructName", "kCmTimestampName", "kCmFromJsonName", "kCmAddToMapName",
              "kCmPreferredName", "kCmPlaceName")
# The builtins a cell row may read through.
CELL_READS = {"variable_instance_exists", "variable_instance_get", "array_length", "array_get",
              "variable_struct_exists", "variable_struct_get"}


def results_rows(results):
    """§ Results' table rows by their check name - the one parse both the
    document and the negative control go through."""
    return {row.split("|")[1].strip(): row for row in results.split("\n") if row.startswith("|")}

# A game script run from the player build is out of scope, whichever way it
# would be reached: by name through script_execute, or with a supplied self
# through CallBuiltinEx. Plus every word the character-select contract already
# forbids (hooks, events, create/destroy, writes).
STASH_FORBIDDEN = tuple(FORBIDDEN) + ("script_execute", "CallBuiltinEx")

DOC_HEADINGS = ("## Static search", "## Static readings", "## Instrument",
                "## Live procedure", "## Results", "## Decision")
DECISION_KEYS = (
    "giveItemRoute", "warpRoute", "stashOpenRoute", "stashCloseRoute",
    "stashTabRoute", "stashTabRule", "stashTabState", "bagTabRoute", "bagTabRule",
    "bagTabState", "cellRule", "itemRule", "moveWholeRoute", "moveOneRoute",
    "countReader",
)
# § Results carries, per by-name call, the shape the game's own call logged
# beside the shape the replay supplied, so a mismatch is never read as the
# game refusing the route.
RESULTS_HEADER = "| Check | Observation | Logged shape | Supplied shape | Control | Date |"
# The two outcome labels that are never a route negative (the recording rule).
RECORDING_LABELS = ("shape not reproduced", "not-run (instrument")
# § Results' rows: live 1's checks, then live 2's (its dll-hash, marker and
# control carry "(live 2)" so the live-1 rows stay distinct).
LIVE1_CHECKS = ["dll-hash", "marker", "control"] + [f"P0-{n}" for n in range(1, 11)]
LIVE2_CHECKS = ["dll-hash (live 2)", "marker (live 2)", "control (live 2)"] + [f"P2-{n}" for n in range(1, 9)]
# Live 3 (verification, player build, 2026-09-26): no `marker` row (the
# player build has no `craftprobe`).
LIVE3_CHECKS = ["dll-hash (live 3)", "control (live 3)", "V0 (live 3)", "V0b (live 3)"] + \
    [f"V{n} (live 3)" for n in range(1, 7)]

# craftprobe's three rows for the stash and bag launch: after the Phase 1k
# rows, before the control, which stays last.
CRAFTPROBE_BLOCK = ("// ---- craftprobe: the crafting-materials Phase 0 instrument (issue #14)",
                    "#endif // FORGEPACT_RELEASE (craftprobe)")
STASH_ROWS = ("gml_Script_CreateItemNew", "gml_Script_UiCreate", "gml_Script_NetworkSendInventoryUpdate")
# Live procedure 2's row: after those three, directly before the control.
CLOSE_ROW = "gml_Script_UiACloseButton"
CP_ROWS = 286


class StashBagLayoutContract(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.plugin = PLUGIN_SRC.read_text(encoding="utf-8").replace("\r\n", "\n")
        signatures = helper_signatures(cls.plugin)
        cls.bodies = {name: function_body(cls.plugin, sig) for name, sig in signatures.items()}
        cls.row = cls.bodies["MenuLayoutRow"]

    # --- the fields a row prints ---------------------------------------------

    def optional_literal(self):
        match = re.search(r"kOptional\[\] = \{([^}]*)\}", self.row)
        self.assertIsNotNone(match, "MenuLayoutRow's optional-field literal")
        return tuple(re.findall(r'"(\w+)"', match.group(1)))

    def test_optional_fields_are_the_ordered_list(self):
        self.assertEqual(self.optional_literal(), OPTIONAL_FIELDS)

    def test_a_method_value_and_text_are_not_optional_fields(self):
        fields = self.optional_literal()
        self.assertNotIn("activationFunc", fields)
        self.assertNotIn("text", fields)
        # text= is still the last thing on every row.
        self.assertIn('row += " text=" + (hasText ? text : std::string());\n    return row;', self.row)

    def test_optional_fields_print_only_when_carried(self):
        self.assertIn("MenuLayoutOptional(inst, var, present)", self.row)
        self.assertIn('if (present) row += std::string(" ") + var + "=" + v;', self.row)
        self.assertIn('"variable_instance_exists"', self.bodies["MenuLayoutOptional"])

    # --- array-valued fields print as [a,b,...] -------------------------------

    def test_an_array_value_is_routed_to_the_array_formatter(self):
        self.assertIn("case VALUE_ARRAY: return MenuLayoutArrayText(v);",
                      self.bodies["MenuLayoutValueText"])

    def test_only_a_string_goes_through_tostring(self):
        # A handle, struct or pointer (what activationArgs is likely to hold)
        # is named by kind: the runner's string conversion of one is unmeasured.
        body = self.bodies["MenuLayoutValueText"]
        self.assertEqual(body.count("ToString()"), 1)
        self.assertIn("case VALUE_STRING: return MenuLayoutOneLine(v.ToString());", body)
        for case in ('case VALUE_REF:    return "<ref>";', 'case VALUE_OBJECT: return "<object>";',
                     'case VALUE_PTR:    return "<ptr>";'):
            self.assertIn(case, body)

    def test_array_elements_are_capped(self):
        self.assertIn("static constexpr int kMenuLayoutMaxArrayItems = 32;", self.plugin)
        self.assertIn('",...+"', self.bodies["MenuLayoutArrayText"])

    def test_array_format_in_order(self):
        body = self.bodies["MenuLayoutArrayText"]
        pieces = ['"array_length"', '"["', '","', '"array_get"', '"<array>"',
                  "MenuLayoutValueText(", '"]"']
        at = [body.index(p) for p in pieces]
        self.assertEqual(at, sorted(at))
        # A nested array is named, never recursed into: one row stays one
        # bounded line.
        self.assertNotIn("MenuLayoutArrayText(", body)

    def test_array_format_compiled(self):
        """The production formatter, cut out of ModuleMain.cpp and compiled
        against a stand-in runtime that can represent arrays, nested arrays
        and VALUE_REF handles (tests/menu_layout_value_text.cpp)."""
        start = self.plugin.index("static constexpr int kMenuLayoutMaxRows")
        end = self.plugin.index("// A variable printed only when")
        source = HARNESS.read_text(encoding="utf-8").replace(
            "// PRODUCTION_VALUE_TEXT", self.plugin[start:end])
        output = FORGEPACT_DIR / "build" / "menu-layout-tests"
        output.mkdir(parents=True, exist_ok=True)
        cpp = output / "menu_layout_value_text.cpp"
        cpp.write_text(source, encoding="utf-8")
        binary = output / ("value_text.exe" if os.name == "nt" else "value_text")
        if os.name == "nt":
            finder = Path(os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)")) \
                / "Microsoft Visual Studio/Installer/vswhere.exe"
            if not finder.exists():
                self.skipTest("MSVC not installed")
            install = subprocess.check_output(
                [str(finder), "-latest", "-products", "*", "-requires",
                 "Microsoft.VisualStudio.Component.VC.Tools.x86.x64", "-property", "installationPath"],
                text=True).strip()
            if not install:
                self.skipTest("MSVC not installed")
            script = output / "compile.cmd"
            script.write_text(
                f'@echo off\ncall "{install}\\VC\\Auxiliary\\Build\\vcvars64.bat" >nul\n'
                "if errorlevel 1 exit /b 1\n"
                f'cl /nologo /std:c++20 /EHsc /utf-8 "{cpp}" /Fe:"{binary}" /Fo:"{output / "value_text.obj"}"\n'
                "exit /b %errorlevel%\n")
            command = ["cmd", "/d", "/c", str(script)]
        else:
            compiler = shutil.which("c++")
            if not compiler:
                self.skipTest("C++20 compiler not installed")
            command = [compiler, "-std=c++20", str(cpp), "-o", str(binary)]
        result = subprocess.run(command, cwd=output, capture_output=True, text=True)
        self.assertEqual(0, result.returncode, result.stdout + result.stderr)
        result = subprocess.run([str(binary)], capture_output=True, text=True)
        (output / "value-text-run.log").write_text(result.stdout + result.stderr, encoding="utf-8")
        self.assertEqual(0, result.returncode, result.stdout + result.stderr)
        self.assertIn("RESULT OK", result.stdout)

    # --- still a reader ---------------------------------------------------------

    def test_forbidden_list_covers_game_script_calls(self):
        for word in ("script_execute", "CallBuiltinEx", "variable_instance_set"):
            self.assertIn(word, STASH_FORBIDDEN)
        # The character-select contract checks the same two.
        self.assertIn("script_execute", FORBIDDEN)
        self.assertIn("CallBuiltinEx", FORBIDDEN)

    def test_no_helper_calls_a_script_hooks_or_writes(self):
        self.assertIn("MenuLayoutArrayText", self.bodies)
        for name, body in self.bodies.items():
            for word in STASH_FORBIDDEN:
                self.assertNotIn(word, body, f"{name} mentions {word}")

    def test_row_cap_unchanged(self):
        self.assertIn("static constexpr int kMenuLayoutMaxRows = 200;", self.plugin)


class StashBagResearchDoc(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.doc = DOC.read_text(encoding="utf-8").replace("\r\n", "\n")

    def test_headings_in_order(self):
        at = [self.doc.index("\n" + h + "\n") for h in DOC_HEADINGS]
        self.assertEqual(at, sorted(at))

    def test_one_line_per_decision_key(self):
        decision = doc_section(self.doc, "## Decision")
        for key in DECISION_KEYS:
            lines = re.findall(r"(?m)^" + key + r": (.+)$", decision)
            self.assertEqual(len(lines), 1, key)
            self.assertTrue(lines[0].strip(), key)

    def test_no_other_decision_key(self):
        decision = doc_section(self.doc, "## Decision")
        keys = re.findall(r"(?m)^(\w+): ", decision)
        self.assertEqual(sorted(keys), sorted(DECISION_KEYS))

    def test_candidate_table_is_documented(self):
        from test_menu_layout_contract import CHARACTER_SELECT_OBJECTS, STASH_BAG_OBJECTS
        static = doc_section(self.doc, "## Static search")
        for name in CHARACTER_SELECT_OBJECTS | STASH_BAG_OBJECTS:
            self.assertIn(f"`{name}`", static, name)

    def test_optional_fields_are_documented(self):
        instrument = doc_section(self.doc, "## Instrument")
        for name in OPTIONAL_FIELDS[6:]:
            self.assertIn(f"`{name}`", instrument, name)
        self.assertIn("[a,b,...]", instrument)

    def test_results_table_carries_logged_and_supplied_shapes(self):
        results = doc_section(self.doc, "## Results")
        rows = [line for line in results.split("\n") if line.startswith("|")]
        self.assertEqual(rows[0], RESULTS_HEADER)
        # One row per check the live procedures name, each with six cells.
        checks = [row.split("|")[1].strip() for row in rows[2:]]
        self.assertEqual(checks, LIVE1_CHECKS + LIVE2_CHECKS + LIVE3_CHECKS)
        for row in rows[2:]:
            self.assertEqual(row.count("|"), 7, row)

    def test_all_results_rows_are_recorded(self):
        # Live 1 and live 2 have both run (2026-09-25, rescope D11): every
        # row is filled and dated, none left pending.
        results = doc_section(self.doc, "## Results")
        rows = results_rows(results)
        for check in LIVE1_CHECKS + LIVE2_CHECKS:
            self.assertNotIn("pending", rows[check], check)
            self.assertTrue(rows[check].rstrip().endswith("| 2026-09-25 |"), check)
        # Live 3 (verification, player build) ran 2026-09-26.
        for check in LIVE3_CHECKS:
            self.assertNotIn("pending", rows[check], check)
            self.assertTrue(rows[check].rstrip().endswith("| 2026-09-26 |"), check)
        # Negative control: the same parse, handed a section with one pending
        # row among filled ones, finds that row by its check and sees it
        # pending - so an empty or unparsed section cannot pass the loop above.
        fabricated = results.replace(rows["P2-8"], "| P2-8 | pending | - | - | - | - |")
        control = results_rows(fabricated)
        self.assertEqual(set(control), set(rows))
        self.assertIn("pending", control["P2-8"])
        self.assertNotIn("pending", control["P2-7"])

    def test_every_decision_line_settled(self):
        # Live 1 settled six lines; live procedure 2 settled the other nine
        # (rescope D11 moved moveWholeRoute/moveOneRoute to
        # hs-drive-stash-move-research rather than leaving them pending).
        decision = doc_section(self.doc, "## Decision")
        for key in DECISION_KEYS:
            line = re.search(r"(?m)^" + key + r": (.+)$", decision).group(1)
            self.assertNotIn("pending", line, key)

    def test_move_decision_lines_record_where_they_went(self):
        decision = doc_section(self.doc, "## Decision")
        for key in ("moveWholeRoute", "moveOneRoute"):
            line = re.search(r"(?m)^" + key + r": (.+)$", decision).group(1)
            self.assertTrue(
                line.startswith("moved to hs-drive-stash-move-research"), key)

    def test_live_procedure_names_the_outcomes_that_are_not_route_negatives(self):
        procedure = doc_section(self.doc, "## Live procedure")
        for label in RECORDING_LABELS:
            self.assertIn(label, procedure, label)
        self.assertIn("reproduced", procedure)
        # Live 1's marker was Step 0c's row count; live 2's is Step 0d's.
        self.assertIn("rows=285", procedure)
        self.assertIn("rows=286", procedure)
        for heading in ("\n### Live procedure 1\n", "\n### Live procedure 2\n"):
            self.assertIn(heading, procedure)
        # Live 2's capture ends with a block tools/live_checks.py can read.
        live2 = procedure[procedure.index("\n### Live procedure 2\n"):]
        self.assertIn("## Checks", live2)
        for check in LIVE2_CHECKS[3:]:
            self.assertIn(check, live2, check)

    def test_instrument_names_the_phase0_craftprobe_additions(self):
        instrument = doc_section(self.doc, "## Instrument")
        for token in ("other:<id>", "methods", "inst", "CreateItemNew", "UiCreate", "NetworkSendInventoryUpdate",
                      "held by", "id:<n>", "obj:<Name>", "`UiACloseButton`", "rows=286"):
            self.assertIn(token, instrument, token)


class CraftprobePhase0Additions(unittest.TestCase):
    """Step 0c's `craftprobe` additions (research build only): the replay of a
    logged call needs its `other`, a closure on an instance needs a holder,
    and P0-1 and P0-3 reach three routines no row logged before."""

    @classmethod
    def setUpClass(cls):
        cls.plugin = PLUGIN_SRC.read_text(encoding="utf-8").replace("\r\n", "\n")
        cls.code = strip_comments(cls.plugin)
        start, end = CRAFTPROBE_BLOCK
        cls.block = strip_comments(cls.plugin[cls.plugin.index(start):cls.plugin.index(end)])
        table = cls.plugin[cls.plugin.index("#define CRAFTPROBE_TARGETS(X)"):]
        table = table[:table.index("#define CP_DEFINE_DETOUR")]
        cls.rows = re.findall(r'X\((\w+),\s*"([^"]+)",\s*(\w+)\)', table)

    def body(self, signature):
        return strip_comments(function_body(self.plugin, signature))

    def test_three_rows_by_sdk_constant_before_the_control(self):
        constants = [constant for _, _, constant in self.rows]
        self.assertEqual(len(self.rows), CP_ROWS)
        for constant in STASH_ROWS:
            self.assertEqual(constants.count(constant), 1, constant)
        at = [constants.index(c) for c in STASH_ROWS]
        self.assertEqual(at, list(range(at[0], at[0] + 3)), "the three rows sit together, in this order")
        self.assertEqual(at[0], constants.index("gml_Script_ReportClient") + 1, "after the Phase 1k rows")
        self.assertEqual(constants[-1], "gml_Script_CheckPlayerInteraction", "the control stays last")
        # Step 0d's close row sits between them and the control.
        self.assertEqual(at[-1] + 1, constants.index(CLOSE_ROW))
        self.assertEqual(constants.index(CLOSE_ROW), len(constants) - 2)
        # Every row's runtime name is the SDK constant's own value.
        self.assertIn("HeroSiege::Scripts::CONSTANT.data()", self.plugin)
        shipped = strip_research_blocks(self.plugin)
        self.assertIn("kPlayerCommands", shipped)   # negative control: the strip keeps player code
        for safe, label, constant in self.rows:
            if constant in STASH_ROWS:
                self.assertNotIn(f'X({safe}, "{label}", {constant})', shipped, label)

    def test_marker_counts_the_rows(self):
        usage = self.body("static void CpUsage(")
        first = usage[usage.index("Out("):]
        self.assertIn("rows=\" + std::to_string(kCpTargetCount)", first[:first.index(";")])

    def test_one_other_parser_serves_call_and_callm(self):
        self.assertEqual(self.code.count("static bool CpResolveOther("), 1)
        other = self.body("static bool CpResolveOther(")
        # Resolved like an id:<n> self, refused with nothing called.
        at = [other.index(step) for step in ('"instance_exists"', "HhResolveInstance(")]
        self.assertEqual(at, sorted(at))
        self.assertIn("nothing was called", other)
        self.assertNotIn("script_execute", other)
        for fn in ("static void CpCall(", "static void CpCallMethod("):
            body = self.body(fn)
            self.assertIn("CpResolveOther(", body, fn)
            self.assertIn('"other:"', body, fn)
            # The other is resolved before the one dispatch.
            dispatch = "CpDispatchScript(" if fn == "static void CpCall(" else "CpDispatchMethod("
            self.assertLess(body.index("CpResolveOther("), body.index(dispatch), fn)
            # The reply prints self and other as the armed lines do.
            self.assertIn('" other=" + PpDescribeSelf(other)', body, fn)

    def test_both_dispatchers_pass_a_separate_other(self):
        for fn in ("static CpCallOutcome CpDispatchScript(", "static CpCallOutcome CpDispatchMethod("):
            body = self.body(fn)
            self.assertIn('CallBuiltinEx(res, "script_execute", self, other, callArgs)', body, fn)
            self.assertNotIn("self, self", body, fn)
            signature = self.code[self.code.index(fn):]
            self.assertIn("CInstance* self, CInstance* other,", signature[:signature.index("{")], fn)

    def test_methods_reader_calls_nothing_and_writes_nothing(self):
        self.assertIn('if (sub == "methods") { CpMethods(tok); return; }', self.body("static void CpCommand("))
        body = self.body("static void CpMethods(")
        for word in ("script_execute", "CallBuiltinEx", "MmCreateHook", "variable_instance_set", "variable_struct_set",
                     "CpDispatch"):
            self.assertNotIn(word, body, word)
        # It names the script a method wraps and the row naming it, read-only.
        self.assertIn("CpIsMethod(", body)
        self.assertIn("CpRowForMethod(", body)

    def test_callm_inst_holder_is_guarded_by_a_row(self):
        callm = self.body("static void CpCallMethod(")
        self.assertIn('== "inst"', callm)
        self.assertIn('"variable_instance_get", { handle, RValue(member) }', callm)
        guard = callm.index("CpRowForMethod(")
        self.assertLess(guard, callm.index("CpDispatchMethod("))
        # The refusal for a method no row names comes before the dispatch.
        refusal = callm.index("names no craftprobe row")
        self.assertLess(guard, refusal)
        self.assertLess(refusal, callm.index("CpDispatchMethod("))

    def test_call_closure_refusal_names_the_method_route(self):
        call = self.body("static void CpCall(")
        refusal = call[call.index("runtime.find('@')"):]
        refusal = refusal[:refusal.index("return;")]
        self.assertIn("callm", refusal)
        self.assertIn("methods", refusal)

    def test_additions_stay_out_of_the_player_build(self):
        shipped = strip_comments(strip_research_blocks(self.plugin))
        self.assertIn("kPlayerCommands", shipped)   # negative control: the strip keeps player code
        for symbol in ("CpResolveOther", "CpMethods", "CpCallMethod", "CpDispatchScript", "CpItemHookName"):
            self.assertNotIn(symbol, shipped, symbol)

    # --- the CreateItemNew row under this build's own item hooks -------------
    # The research build's item-inspect hook swaps CreateItemNew's table entry
    # at setup, table-only. The row detours the saved original instead (the
    # shape TgProbeAttach ships), and reports the function held when another
    # install already detoured it inline - decided by the address, never by a
    # flag - so P0-1's check is `not-run (instrument: held ...)`, not failed.

    def test_resolver_falls_back_to_the_saved_original_only_for_create_item_new(self):
        resolver = self.body("static PVOID CpResolve(")
        self.assertEqual(resolver.count("g_Orig_CreateItemNew"), 2, "read in the one fallback only")
        branch = resolver[:resolver.index("fn = (PVOID)g_Orig_CreateItemNew;")]
        branch = branch[branch.rindex("if ("):]
        # Only under the SDK constant, and only when the table entry is not the
        # game's code.
        self.assertIn("HeroSiege::Scripts::gml_Script_CreateItemNew", branch)
        self.assertIn("!AddrIsExecutableInModule(GetModuleHandleA(nullptr), fn)", branch)
        # The fallback still passes the same executable-code gate before return.
        gate = resolver.rindex("if (!AddrIsExecutableInModule(GetModuleHandleA(nullptr), fn))")
        self.assertLess(resolver.index("fn = (PVOID)g_Orig_CreateItemNew;"), gate)
        self.assertLess(gate, resolver.index("return fn;"))
        self.assertEqual(resolver.count("return fn;"), 1)
        # Negative control: the fallback names no other row's original.
        self.assertIsNone(re.search(r"g_Orig_(?!CreateItemNew)\w+", resolver))

    def test_an_inline_detoured_create_item_new_is_held_before_resolving(self):
        install = self.body("static void CpInstall(")
        held = install.index("held by \" + CpItemHookName(true)")
        condition = install[:held]
        condition = condition[condition.rindex("if ("):]
        self.assertIn("HeroSiege::Scripts::gml_Script_CreateItemNew", condition)
        # Held is decided by the saved original's address, not by a flag.
        self.assertIn("!AddrIsExecutableInModule(mainMod, (const void*)g_Orig_CreateItemNew)", condition)
        for flag in ("g_CustomForgeHooksActive", "g_TruthOn"):
            self.assertNotIn(flag, condition, flag)
        self.assertLess(held, install.index("CpResolve(t, why)"))
        self.assertLess(held, install.index("MmCreateHook("))
        self.assertIn("++held", install[held:install.index("CpResolve(t, why)")])
        # The flags only name the install in the message.
        name = self.body("static const char* CpItemHookName(")
        self.assertIn("g_CustomForgeHooksActive", name)
        self.assertIn("g_TruthOn", name)

    def test_the_detoured_line_marks_the_table_only_path(self):
        resolver = self.body("static PVOID CpResolve(")
        self.assertIn('why = std::string("under table-only ") + CpItemHookName(false);', resolver)
        install = self.body("static void CpInstall(")
        detoured = install[install.index('"craftprobe hook: detoured %s at exe+0x%llX"'):]
        detoured = detoured[:detoured.index("++ok;")]
        self.assertIn('why.empty() ? std::string() : " (" + why + ")"', detoured)
        self.assertEqual(self.body("static const char* CpItemHookName(").count('"bp_citemn"'), 1)

    def test_the_item_inspect_hook_still_swaps_create_item_new_table_only(self):
        inspect = self.body("static void InstallItemInspectHooks(")
        self.assertIn('HookOneScriptTable("CreateItemNew",', inspect)
        self.assertNotIn('HookOneScript("CreateItemNew",', inspect)


class CraftprobeLive2Additions(unittest.TestCase):
    """Step 0d's `craftprobe` additions (research build only). Live 1's by-name
    open came back `shape not reproduced` twice: `UiCreate`'s first argument is
    an object reference and the stash closure's is a live instance reference,
    and no argument form supplied either. `id:<n>` and `obj:<Name>` join the one
    parser `call`, `callm` and `skillprobe call` share, and the close gets its
    own row."""

    @classmethod
    def setUpClass(cls):
        cls.plugin = PLUGIN_SRC.read_text(encoding="utf-8").replace("\r\n", "\n")
        cls.code = strip_comments(cls.plugin)
        table = cls.plugin[cls.plugin.index("#define CRAFTPROBE_TARGETS(X)"):]
        table = table[:table.index("#define CP_DEFINE_DETOUR")]
        cls.rows = re.findall(r'X\((\w+),\s*"([^"]+)",\s*(\w+)\)', table)
        cls.arg = strip_comments(function_body(cls.plugin, "static bool CpResolveArg("))

    def body(self, signature):
        return strip_comments(function_body(self.plugin, signature))

    def branch(self, prefix):
        # From the prefix test to the next `} else if` / `} else {`.
        start = self.arg.index(f'la.rfind("{prefix}", 0) == 0')
        end = self.arg.index("} else", start)
        return self.arg[start:end]

    def test_id_argument_is_the_instances_own_id(self):
        branch = self.branch("id:")
        at = [branch.index(step) for step in ('"instance_exists", { handle }', '"variable_instance_get", { handle, RValue("id") }')]
        self.assertEqual(at, sorted(at), "alive first, then its own id")
        self.assertIn("return false;", branch)

    def test_obj_argument_is_asset_get_index_as_returned(self):
        branch = self.branch("obj:")
        self.assertIn('"asset_get_index"', branch)
        # Refused unless the runtime names that index an object of that name.
        self.assertIn('"object_exists"', branch)
        self.assertIn('"object_get_name"', branch)
        self.assertIn("v = index;", branch, "passed exactly as asset_get_index returned it")
        self.assertLess(branch.index('"object_get_name"'), branch.index("v = index;"))

    def test_each_refusal_says_nothing_was_called_and_returns(self):
        # id: refuses a malformed id, a dead instance and an unreadable id;
        # obj: refuses once, whatever made the name not an object's.
        for prefix, count in (("id:", 3), ("obj:", 1)):
            lines = self.branch(prefix).split("\n")
            refusals = [i for i, line in enumerate(lines) if "refused" in line]
            self.assertEqual(len(refusals), count, prefix)
            for i in refusals:
                self.assertIn("nothing was called", lines[i], prefix)
                self.assertIn("return false;", lines[i], prefix)
        # The parser dispatches nothing: every refusal precedes the one call.
        for word in ("script_execute", "CpDispatch"):
            self.assertNotIn(word, self.arg, word)

    def test_skillprobe_delegates_to_the_one_parser(self):
        sp = self.body("static bool SpResolveArg(")
        self.assertIn('CpResolveArg("skillprobe call", a, inst, v)', sp)
        # The id: logic lives in CpResolveArg only, never a second copy.
        self.assertNotIn('"variable_instance_get"', sp)
        self.assertNotIn('"instance_exists"', sp)
        self.assertNotIn("id:", sp)

    def test_usage_strings_name_both_argument_kinds(self):
        for fn in ("static void CpCall(", "static void CpCallMethod(", "static void CpUsage("):
            body = function_body(self.plugin, fn)
            for token in ("id:<n>", "obj:<Name>"):
                self.assertIn(token, body, f"{fn} {token}")
        # `call`'s own argument list names them, not only its self selector.
        call = function_body(self.plugin, "static void CpCall(")
        args = call[call.index("(arg: "):]
        args = args[:args.index(")\";")]
        for token in ("id:<n>", "obj:<Name>"):
            self.assertIn(token, args, token)

    def test_close_row_by_sdk_constant_before_the_control(self):
        constants = [constant for _, _, constant in self.rows]
        self.assertEqual(len(self.rows), CP_ROWS)
        self.assertEqual(constants.count(CLOSE_ROW), 1)
        self.assertEqual(constants.index(CLOSE_ROW), len(constants) - 2)
        self.assertEqual(constants[-1], "gml_Script_CheckPlayerInteraction")
        shipped = strip_research_blocks(self.plugin)
        self.assertIn("kPlayerCommands", shipped)   # negative control: the strip keeps player code
        for safe, label, constant in self.rows:
            if constant == CLOSE_ROW:
                self.assertNotIn(f'X({safe}, "{label}", {constant})', shipped)

    def test_marker_prints_the_row_count(self):
        usage = self.body("static void CpUsage(")
        first = usage[usage.index("Out("):]
        self.assertIn('"craftprobe: phase1k rows=" + std::to_string(kCpTargetCount)', first[:first.index(";")])
        self.assertEqual(len(self.rows), 286)


class MenuLayoutCellRows(unittest.TestCase):
    """Step 5's cell rows: after each `UI_Inventory_Grid_obj` row, one
    `  cell=<x>,<y> grid=<id> fp=<fingerprint|none> o=none` row per occupied
    node (§ Decision, cellRule: a node carries its fingerprint and no count)."""

    @classmethod
    def setUpClass(cls):
        cls.plugin = PLUGIN_SRC.read_text(encoding="utf-8").replace("\r\n", "\n")

    def body(self, signature):
        return strip_comments(function_body(self.plugin, signature))

    def test_cell_rows_follow_each_grid_row(self):
        command = self.body("static void MenuLayoutCommand(")
        self.assertIn("HeroSiege::Objects::GameObject::UI_Inventory_Grid_obj", command)
        row = command.index("Out(MenuLayoutRow(inst, sc));")
        cells = command.index('if (gridIdx >= 0 && MenuLayoutRead(inst, "object_index") == gridIdx) MenuLayoutCells(inst, true);')
        self.assertLess(row, cells)
        # Cell rows are not instances: only an instance row counts toward the cap.
        self.assertEqual(command.count("++listed;"), 1)
        self.assertNotIn("listed", self.body("static int MenuLayoutCells("))

    def test_cell_row_format(self):
        cells = self.body("static int MenuLayoutCells(")
        at = [cells.index(p) for p in ('"  cell="', '" grid="', '" fp="', '" o=none"')]
        self.assertEqual(at, sorted(at))
        self.assertIn('"nodeFingerprint"', cells)
        # No count is read: o= is the literal none, never a value.
        self.assertNotIn('"o"', cells)
        self.assertIn('std::string fp = "none";', cells)

    def test_cell_rows_read_only(self):
        cells = self.body("static int MenuLayoutCells(")
        used = set(re.findall(r'CallBuiltin\("(\w+)"', cells))
        # Negative control: the scan sees the reads that are there.
        self.assertIn("array_get", used)
        self.assertIn("variable_struct_get", used)
        self.assertLessEqual(used, CELL_READS)
        for word in STASH_FORBIDDEN + ("variable_struct_set", "array_set", "Cm", "Ap"):
            self.assertNotIn(word, cells, word)

    def test_a_grid_past_the_cap_says_so_on_its_row(self):
        self.assertIn("static constexpr int kMenuLayoutMaxCells = 200;", self.plugin)
        cells = self.body("static int MenuLayoutCells(")
        self.assertIn("if (!print || occupied > kMenuLayoutMaxCells) continue;", cells)
        row = self.body("static std::string MenuLayoutRow(")
        cap = row.index('row += " cellcap=1";')
        self.assertLess(row.index("MenuLayoutCells(inst, false) > kMenuLayoutMaxCells"), cap)
        self.assertLess(cap, row.index('row += " text="'))
        # Rows walked before columns: the first rows printed are the first row-major ones.
        self.assertLess(cells.index("for (int y = 0; y < height; ++y)"), cells.index("for (int x = 0; x < width; ++x)"))


class StashBagPlayerVerbs(unittest.TestCase):
    """Step 5's five player verbs: tool-facing, player build, each from its own
    helper, everything by name, a before/after line and a refusal line."""

    @classmethod
    def setUpClass(cls):
        cls.plugin = PLUGIN_SRC.read_text(encoding="utf-8").replace("\r\n", "\n")
        start, end = VERBS_BLOCK
        cls.block = cls.plugin[cls.plugin.index(start):cls.plugin.index(end)]
        cls.code = strip_comments(cls.block)
        cls.shipped = strip_comments(strip_research_blocks(cls.plugin))

    def body(self, signature):
        return strip_comments(function_body(self.plugin, signature))

    def player_commands(self):
        run = function_body(self.plugin, "static void RunCommand(const std::string& line)")
        commands = run[run.index("kPlayerCommands = {"):]
        return commands[:commands.index("};")]

    def test_each_verb_ships_in_the_player_build(self):
        commands = self.player_commands()
        self.assertIn('"menulayout"', commands)   # the set is the one read, not an empty slice
        for verb, (_, command) in VERBS.items():
            self.assertIn(f'"{verb}"', commands, verb)
            self.assertIn(command, self.shipped, verb)
        self.assertNotIn("FORGEPACT_RELEASE", self.block)
        # Neither a by-name open nor a move is a verb here.
        for verb in ("stashopen", "stashmove"):
            self.assertNotIn(f'"{verb}"', commands, verb)
            self.assertNotIn(f'lc == "{verb}"', self.plugin, verb)

    def test_each_is_dispatched_from_its_own_handler_as_a_standalone_early_return(self):
        run = function_body(self.plugin, "static void RunCommand(const std::string& line)")
        for verb, (handler, command) in VERBS.items():
            self.assertIn(f"    if ({handler}(lc, rest)) return;\n", run, verb)
            name = command[len("static void "):-1]
            self.assertIn(f'if (lc == "{verb}") {{ {name}(rest); return true; }}',
                          self.body(f"static bool {handler}("), verb)
            self.assertEqual(self.plugin.count(f'lc == "{verb}"'), 1, verb)

    def test_frame_callback_never_mentions_them(self):
        frame = function_body(self.plugin, "void FrameCallback(")
        for verb, (handler, command) in VERBS.items():
            name = command[len("static void "):-1]
            for word in (verb, handler, name):
                self.assertNotIn(word, frame, word)

    def test_nothing_is_hooked_or_reached_by_address(self):
        for word in ("Rva", "GetModuleHandle", "MmCreateHook", "HookOneScript", "HookOneScriptTable", "HookBuiltin",
                     "HookRawNamedRoutine", "InstallScriptHook", "instance_create", "instance_destroy",
                     "event_perform", "CallGameScriptEx", "ds_map_replace", "array_set"):
            self.assertNotIn(word, self.code, word)

    def test_instances_are_resolved_by_name(self):
        # No object is ever typed: every one through the SDK's GameObject enum.
        self.assertIsNone(re.search(r'"\w+_obj"', self.code))
        self.assertIn("HeroSiege::Objects::GameObject::UI_Stash_obj", self.code)
        for signature in ("static std::vector<RValue> TalentAllocInstances(", "static bool CmInstance("):
            body = self.body(signature)
            for step in ('"asset_get_index"', '"instance_number"', '"instance_find"'):
                self.assertIn(step, body, signature)
        self.assertIn("TalentAllocInstances(obj)", self.body("static bool StashVerbByString("))
        self.assertIn("HhResolveLocalPlayer(player, nullptr)", self.body("static void PlayerWarpCommand("))

    def test_routines_are_called_by_sdk_constant(self):
        scripts = set(re.findall(r"HeroSiege::Scripts::gml_Script_(\w+)", self.code))
        self.assertEqual(scripts, VERB_SCRIPTS)
        pairs = re.findall(r"TalentAllocScript (\w+)\{ HeroSiege::Scripts::(\w+),\s*"
                           r"SdkShortScriptName\(HeroSiege::Scripts::(\w+)\) \}", self.code)
        self.assertEqual(len(pairs), len(VERB_SCRIPTS))
        for name, routine, short in pairs:
            self.assertEqual(routine, short, name)
        # The dispatcher resolves the constant through GetNamedRoutinePointer
        # before anything is called.
        dispatch = self.body("static TalentAllocCall TalentAllocDispatch(")
        self.assertLess(dispatch.index("GetNamedRoutinePointer(s.routine.data(), &p)"), dispatch.index('"asset_get_index"'))
        self.assertLess(dispatch.index('"asset_get_index"'), dispatch.index('"script_execute"'))
        # Every by-name dispatch here names one of the verb constants.
        for call in re.findall(r"TalentAllocDispatch\(([^,]+),", self.code):
            self.assertIn(call.strip(), ("kStashVerbClose", "*named", "*script"), call)
        # The one other dispatch is Socketable's closure, the method value the
        # button itself holds, read from its activationFunc first.
        self.assertEqual(self.code.count("CallBuiltinEx("), 1)
        tab = self.body("static void StashTabCommand(")
        self.assertLess(tab.index('RValue("activationFunc")'), tab.index('CallBuiltinEx(res, "script_execute", self, self, callArgs)'))
        self.assertIn("std::vector<RValue> callArgs{ method };", tab)
        # giveitem's calls are craftmats' own constants, through CmCall.
        give = self.body("static void GiveItemCommand(")
        for call in re.findall(r"CmCall\((\w+),", give):
            self.assertIn(call, GIVE_ORDER + ("kCmRemoveFromMapName",), call)

    def test_each_prints_before_after_and_a_refusal_with_its_name(self):
        for verb, (_, command) in VERBS.items():
            body = self.body(command)
            self.assertIn(f'const std::string tag = "{verb}: ";', body, verb)
            self.assertIsNone(re.search(r"Out\((?!tag \+)", body), verb)
            self.assertIn('"refused - ', body, verb)
            self.assertIn('before="', body, verb)
            self.assertIn('" after="', body, verb)

    def test_what_is_not_measured_is_refused_before_anything_is_called(self):
        give = self.body("static void GiveItemCommand(")
        stash = give.index('Lower(tok[0]) == "stash"')
        self.assertLess(stash, give.index("CmCall("))
        refusal = give[stash:give.index("return;", stash)]
        self.assertIn("route_not_measured", refusal)
        self.assertIn("hs-drive-stash-move-research", refusal)
        bag = self.body("static void BagTabCommand(")
        self.assertLess(bag.index("route_not_measured"), bag.index("TalentAllocDispatch("))
        for name in ('"materials"', '"socket"', '"InventoryTabMaterial"', '"InventoryTabSocket"'):
            self.assertIn(name, bag)
        tab = self.body("static void StashTabCommand(")
        self.assertLess(tab.index("is not a measured shape"), tab.index("TalentAllocDispatch("))

    def test_each_shape_is_live_2s_supplied_one(self):
        close = self.body("static void StashCloseCommand(")
        self.assertIn("TalentAllocDispatch(kStashVerbClose, close, stash, {})", close)
        self.assertIn('"InventoryClose"', close)
        bag = self.body("static void BagTabCommand(")
        self.assertIn("TalentAllocDispatch(*script, self, stash, {})", bag)
        self.assertIn('"tabSelected"', bag)
        self.assertIn('"activeNode"', bag)
        tab = self.body("static void StashTabCommand(")
        self.assertIn("TalentAllocDispatch(*named, self, bar, args)", tab)
        self.assertIn("const std::vector<RValue> args{ RValue(n), button };", tab)
        self.assertIn('"stashTabSelected"', tab)
        self.assertIn('RValue("tabNumber")', tab)
        # The state did not change: a refusal, never a pass.
        for body, var in ((tab, "stashTabSelected"), (bag, "tabSelected")):
            self.assertIn(f'"refused - the state did not change ({var} stayed "', body)

    def test_playerwarp_writes_only_x_and_y(self):
        warp = self.body("static void PlayerWarpCommand(")
        writes = re.findall(r'"variable_instance_set", \{ RValue\(id\), RValue\("(\w+)"\)', warp)
        self.assertEqual(writes, ["x", "y"])
        self.assertEqual(self.code.count('"variable_instance_set"'), 2)
        # A non-finite argument is refused before the player is resolved.
        self.assertLess(warp.index("StashVerbNumber(tok[0], x)"), warp.index("HhResolveLocalPlayer("))
        self.assertIn("std::isfinite(out)", self.body("static bool StashVerbNumber("))

    def test_giveitem_copies_by_the_loader_into_the_bag_and_moves_nothing(self):
        give = self.body("static void GiveItemCommand(")
        for word in ("ChangeItemOwner", "StashGridAddItem", "RemoveItemFromMap", "GridRemoveItem",
                     "kCmStashOwner", "kCmGridRemoveName", "CmTake(", "CmMakeUnit("):
            self.assertNotIn(word, give, word)
        self.assertIn("CmItemMap(save, kCmCharacterOwner, map0)", give)
        # CmMakeUnit's order.
        at = [give.index(c) for c in GIVE_ORDER]
        self.assertEqual(at, sorted(at))
        # The only write is the save struct's o, before the loader runs:
        # nothing edits the item after it is made.
        self.assertEqual(give.count('"variable_struct_set"'), 1)
        self.assertLess(give.index('"variable_struct_set", { saved, RValue("o")'), give.index("kCmFromJsonName"))
        # A non-stackable takes count 1; a count above the template's own o is refused.
        self.assertLess(give.index("!stackable && count > 1"), give.index("kCmTimestampName"))
        self.assertLess(give.index("count > have"), give.index("kCmTimestampName"))
        # The undo is the only way out of map 0, and only after the add.
        self.assertEqual(give.count("kCmRemoveFromMapName"), 1)
        self.assertLess(give.index("kCmAddToMapName"), give.index("kCmRemoveFromMapName"))
        # The proof is its own re-read of map 0 and the destination cells.
        for part in ('"key="', '" before="', '" after="', '" o="', '"confirmed - "', '" in map 0 and in the destination cells"',
                     '"not confirmed - map 0 answers "', "after == before + 1", "CmCellsHold(cellsNow, made)"):
            self.assertIn(part, give, part)


class StashBagDocumentation(unittest.TestCase):
    def test_release_notes_mention_the_stash_and_the_bag(self):
        if not NOTES.is_file():   # published notes leave main (forgepact-notes-cleanup.yml)
            self.skipTest(f"{NOTES.name} is published and no longer on main")
        notes = NOTES.read_text(encoding="utf-8")
        start = notes.index("`menulayout`")
        bullet = notes[start:notes.find("\n- ", start)]
        self.assertIn("stash", bullet)
        self.assertIn("bag", bullet)
        self.assertIn("grid cells", bullet)

    def test_readme_documents_the_new_fields(self):
        readme = README.read_text(encoding="utf-8").replace("\r\n", "\n")
        start = readme.index("## Menu layout")
        section = readme[start:readme.index("\n## ", start + 1)]
        for name in OPTIONAL_FIELDS[6:]:
            self.assertIn(name, section, name)
        self.assertIn("[a,b,...]", section)
        self.assertIn("stash", section)
        for token in ("<ref>", "<object>", "<ptr>", "<kind N>", "<read-failed>"):
            self.assertIn(token, section, token)

    def test_notes_file_is_the_unpublished_version(self):
        self.assertEqual(NOTES.name, "release-notes-v1.4.7.md")

    def test_readme_and_notes_agree_on_the_drag_object(self):
        # Whether UI_Inventory_Drag_obj holds the item on the cursor is not
        # measured (prospect-window-research.md attributes the held item to
        # s_InventoryDrag), so both name it as what it is and claim no more.
        readme = README.read_text(encoding="utf-8").replace("\r\n", "\n")
        start = readme.index("## Menu layout")
        texts = {"README": readme[start:readme.index("\n## ", start + 1)]}
        if NOTES.is_file():
            notes = NOTES.read_text(encoding="utf-8").replace("\r\n", "\n")
            at = notes.index("`menulayout`")
            texts["notes"] = notes[at:notes.find("\n- ", at)]
        for where, text in texts.items():
            flat = " ".join(text.split())
            self.assertIn("the inventory's drag object (`UI_Inventory_Drag_obj`)", flat, where)
            self.assertNotIn("cursor", flat, where)


if __name__ == "__main__":
    unittest.main()
