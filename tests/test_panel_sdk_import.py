"""The panel imports only the two Satanic Zone pools from hs_game_sdk.

hs_game_sdk builds each of its five generated tables (objects, scripts, rooms,
sprites, sounds) the first time one of their names is imported (hub PR #286).
A table is an IntEnum, and building one takes time quadratic in its member
count on CPython 3.10 and 3.13 (3.14's enum.py has the same scan). So a table
name the panel imports is paid for at every panel start, every sandbox a
browser test starts and every launch of the packaged exe, used or not.
src/forgepact.py used to import GameObject, GameScript and seven more names it
never used, and so built the objects and scripts tables for nothing (hub
guide, ForgePact Known Limitations item 31).

The table check imports the panel in a new interpreter, because a table this
process already imported would hide a regression, and it reads what that
import loaded rather than the import statement, so a table name imported by
any module the panel loads is caught too. FORGEPACT_TEST_PANEL_DIR runs the
tests against another tree's forgepact.py, as in test_mods_categories.py.
"""
import ast
import json
import os
import subprocess
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PANEL_DIR = Path(os.environ.get("FORGEPACT_TEST_PANEL_DIR", str(ROOT / "src")))
TABLES = {"objects", "scripts", "rooms", "sprites", "sounds"}
# What src/forgepact.py imports from hs_game_sdk. The hub pins the same list
# (tests/test_sdk_lazy_import.py, FORGEPACT_NAMES): change both together.
PANEL_SDK_NAMES = {"SATANIC_BUFFS", "SATANIC_DEBUFFS"}

PROBE = """
import json, sys
sys.path.insert(0, {panel_dir!r})
result = None
{statement}
sdk = sys.modules.get("hs_game_sdk")
print(json.dumps({{
    "sdk": getattr(sdk, "__file__", None),
    "loaded": sorted(m.split(".", 1)[1] for m in sys.modules if m.startswith("hs_game_sdk.")),
    "result": result,
}}))
"""


def probe(statement):
    """Run `statement` in a new interpreter with the panel's directory on sys.path."""
    code = PROBE.format(panel_dir=str(PANEL_DIR), statement=statement)
    run = subprocess.run([sys.executable, "-c", code], capture_output=True, text=True,
                         encoding="utf-8", errors="replace", timeout=300)
    if run.returncode:
        raise AssertionError(f"probe exited {run.returncode}:\n{run.stderr}")
    return json.loads(run.stdout.strip().splitlines()[-1])


def sdk_imports_run_at_import(tree):
    """The `from hs_game_sdk import` statements outside function and class bodies."""
    found = []

    def visit(node):
        for child in ast.iter_child_nodes(node):
            if isinstance(child, (ast.FunctionDef, ast.AsyncFunctionDef, ast.ClassDef, ast.Lambda)):
                continue
            if isinstance(child, ast.ImportFrom) and child.module == "hs_game_sdk":
                found.append(child)
            visit(child)

    visit(tree)
    return found


class PanelSdkImportTests(unittest.TestCase):
    def test_both_imports_name_only_the_satanic_pools(self):
        tree = ast.parse((PANEL_DIR / "forgepact.py").read_text(encoding="utf-8-sig"))
        imports = sdk_imports_run_at_import(tree)
        self.assertEqual(len(imports), 2, "the plain import and the one after the sys.path fallback")
        for node in imports:
            with self.subTest(line=node.lineno):
                self.assertEqual({alias.name for alias in node.names}, PANEL_SDK_NAMES)

    def test_without_the_sdk_the_panel_imports_with_empty_pools(self):
        # None in sys.modules makes every import of hs_game_sdk raise
        # ImportError, so the sys.path fallback runs and fails too.
        got = probe('sys.modules["hs_game_sdk"] = None\n'
                    "import forgepact\n"
                    "result = [forgepact.SATANIC_BUFFS, forgepact.SATANIC_DEBUFFS,\n"
                    "          forgepact.SATANIC_BUFF_LIST, forgepact.SATANIC_DEBUFF_LIST]")
        self.assertEqual(got["result"], [[], [], [], []])


class PanelBuildsNoTableTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.panel = probe("import forgepact")
        if cls.panel["sdk"] is None:
            raise unittest.SkipTest("hs_game_sdk is not importable here, so the panel's pools are empty")
        # The same package on its own: an SDK from before hub PR #286 builds
        # every table when imported, whatever the panel names.
        sdk_python = str(Path(cls.panel["sdk"]).parents[1])
        package = probe(f"sys.path.insert(0, {sdk_python!r})\nimport hs_game_sdk")
        if set(package["loaded"]) & TABLES:
            raise unittest.SkipTest("this hs-game-sdk builds every table when it is imported (before hub PR #286)")

    def test_importing_the_panel_builds_no_table(self):
        self.assertEqual(set(self.panel["loaded"]) & TABLES, set())

    def test_the_probe_sees_a_table_being_built(self):
        # The positive control: the same probe, with one table name imported
        # after the panel, reports that table.
        got = probe("import forgepact\nfrom hs_game_sdk import GameObject")
        self.assertIn("objects", got["loaded"])


if __name__ == "__main__":
    unittest.main()
