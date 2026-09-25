"""Contract tests for `menulayout`'s stash and bag rows.

The hub's stash and bag tools (`hs_stash_open`, `hs_stash_close`,
`hs_stash_tab`, `hs_bag_tab`, `hs_move_item`) click only what `menulayout`
lists, and identify a tab, a grid or a window by the variables it prints. So
the listing grows: the stash, bag and inventory objects join the candidate
table (tests/test_menu_layout_contract.py pins the set), a row prints more of
the variables an instance carries, and an array-valued variable prints as
`[a,b,...]`. It still ships in the player build, so these tests also pin that
it stays a reader - and that it calls no game script, directly or through a
builtin that would run one.

The mechanisms the hub tools rely on are measured in one research launch and
recorded in docs/stash-bag-layout-research.md; its headings and decision keys
are pinned here so the hub is never written against a key the document lacks.
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
NOTES = FORGEPACT_DIR / "release-notes-v1.4.5.md"
README = FORGEPACT_DIR / "README.md"
DOC = FORGEPACT_DIR / "docs" / "stash-bag-layout-research.md"
HARNESS = TESTS_DIR / "menu_layout_value_text.cpp"

if str(TESTS_DIR) not in sys.path:
    sys.path.insert(0, str(TESTS_DIR))

from test_menu_layout_contract import FORBIDDEN, doc_section, helper_signatures  # noqa: E402
from test_release_hook_contract import function_body  # noqa: E402

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

DOC_HEADINGS = ("## Static search", "## Instrument", "## Live procedure",
                "## Results", "## Decision")
DECISION_KEYS = (
    "stashOpenRoute", "interactKey", "moveKeys", "warpRoute", "stashCloseRoute",
    "stashTabRule", "stashTabState", "bagTabRule", "bagTabState", "cellRule",
    "itemRule", "moveWholeRoute", "moveOneRoute", "countReader",
)


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


class StashBagDocumentation(unittest.TestCase):
    def test_release_notes_mention_the_stash_and_the_bag(self):
        if not NOTES.is_file():   # published notes leave main (forgepact-notes-cleanup.yml)
            self.skipTest(f"{NOTES.name} is published and no longer on main")
        notes = NOTES.read_text(encoding="utf-8")
        start = notes.index("`menulayout`")
        bullet = notes[start:notes.find("\n- ", start)]
        self.assertIn("stash", bullet)
        self.assertIn("bag", bullet)

    def test_readme_documents_the_new_fields(self):
        readme = README.read_text(encoding="utf-8").replace("\r\n", "\n")
        start = readme.index("## Menu layout")
        section = readme[start:readme.index("\n## ", start + 1)]
        for name in OPTIONAL_FIELDS[6:]:
            self.assertIn(name, section, name)
        self.assertIn("[a,b,...]", section)
        self.assertIn("stash", section)


if __name__ == "__main__":
    unittest.main()
