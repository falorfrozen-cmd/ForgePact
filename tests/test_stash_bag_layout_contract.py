"""Contract tests for `menulayout`'s stash and bag rows, and for the
`craftprobe` additions the stash and bag research launch needs.

The hub's six stash and bag tools (`hs_give_item`, `hs_stash_open`,
`hs_stash_close`, `hs_stash_tab`, `hs_bag_tab`, `hs_move_item`) set up game
state for other tests, so each acts through the runtime by name - the game's
own routine, called with the shape the research launch logged - and keeps a
click on a listed row only as the fallback a § Decision line names.
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
against a key the document lacks. The research-build instrument that launch
replays the game's calls with (`craftprobe`'s `other:<id>`, `methods`,
`callm`'s `inst` holder and three rows) is pinned here too, and stays out of
the player build.
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
NOTES = FORGEPACT_DIR / "release-notes-v1.4.6.md"
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
# no stable text.
OPTIONAL_FIELDS = (
    "label", "name", "slot", "index", "page", "selected",
    "uiNodeCallstack", "activationArgs", "enabled", "tabNumber", "tabType",
    "stashTabSelected", "nodeGridWidth", "nodeGridHeight", "gridScale", "gridName",
)

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

# craftprobe's three rows for the stash and bag launch: after the Phase 1k
# rows, before the control, which stays last.
CRAFTPROBE_BLOCK = ("// ---- craftprobe: the crafting-materials Phase 0 instrument (issue #14)",
                    "#endif // FORGEPACT_RELEASE (craftprobe)")
STASH_ROWS = ("gml_Script_CreateItemNew", "gml_Script_UiCreate", "gml_Script_NetworkSendInventoryUpdate")


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
            # `pending` is allowed until the phase-0 launch has measured it.
            self.assertTrue(lines[0].strip(), key)

    def test_no_other_decision_key(self):
        decision = doc_section(self.doc, "## Decision")
        keys = re.findall(r"(?m)^(\w+): ", decision)
        self.assertEqual(sorted(keys), sorted(DECISION_KEYS))

    def test_candidate_table_is_documented(self):
        from test_menu_layout_contract import PLANNED_OBJECTS
        static = doc_section(self.doc, "## Static search")
        for name in PLANNED_OBJECTS:
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
        # One row per check the live procedure names, each with six cells.
        checks = [row.split("|")[1].strip() for row in rows[2:]]
        self.assertEqual(checks, ["dll-hash", "marker", "control"] + [f"P0-{n}" for n in range(1, 11)])
        for row in rows[2:]:
            self.assertEqual(row.count("|"), 7, row)

    def test_live_procedure_names_the_outcomes_that_are_not_route_negatives(self):
        procedure = doc_section(self.doc, "## Live procedure")
        for label in RECORDING_LABELS:
            self.assertIn(label, procedure, label)
        self.assertIn("reproduced", procedure)
        # The marker the launch checks first is Step 0c's row count.
        self.assertIn("rows=285", procedure)

    def test_instrument_names_the_phase0_craftprobe_additions(self):
        instrument = doc_section(self.doc, "## Instrument")
        for token in ("other:<id>", "methods", "inst", "CreateItemNew", "UiCreate", "NetworkSendInventoryUpdate",
                      "held by"):
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
        self.assertEqual(len(self.rows), 285)
        for constant in STASH_ROWS:
            self.assertEqual(constants.count(constant), 1, constant)
        at = [constants.index(c) for c in STASH_ROWS]
        self.assertEqual(at, list(range(at[0], at[0] + 3)), "the three rows sit together, in this order")
        self.assertEqual(at[0], constants.index("gml_Script_ReportClient") + 1, "after the Phase 1k rows")
        self.assertEqual(constants[-1], "gml_Script_CheckPlayerInteraction", "the control stays last")
        self.assertEqual(at[-1] + 1, len(constants) - 1)
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
        self.assertEqual(NOTES.name, "release-notes-v1.4.6.md")

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
