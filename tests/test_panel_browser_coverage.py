#!/usr/bin/env python3
"""Every panel browser suite has a Python module that runs it.

CI runs the panel's browser suites (`panel/tests/*.e2e.mjs`) only through the
Python modules that call `panel_browser._npm("<script>")`: the release
workflow's `panel-browser-tests` job and `panel-browser-pr.yml` run those
modules, never `npm` directly. A suite with no module therefore runs nowhere
but on a developer's machine. `e2e:review` and `e2e:form` were in that state:
`e2e:review` passed 28/28 on main and 15/28 on a pull request that changed
the default theme (ForgePact#105), and nothing reported it.

So a suite is covered when a `package.json` script runs its file and a
`tests/test_*.py` module calls that script through `_npm`. A suite that
genuinely cannot run in CI goes in `EXEMPT` with the reason, which keeps the
exception visible in review instead of silent.
"""
import json
import re
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

# file name -> why no CI module runs it. Empty today: every suite has one.
EXEMPT = {}

NPM_CALL = re.compile(r"""_npm\(\s*["']([^"']+)["']""")


def uncovered(root):
    """The `panel/tests/*.e2e.mjs` files no test module runs, sorted.

    A file counts as run when some `panel/package.json` script names it and a
    `tests/test_*.py` module calls that script through `_npm(...)`.
    """
    panel = root / "panel"
    scripts = json.loads((panel / "package.json").read_text(encoding="utf-8")).get("scripts", {})
    called = set()
    for module in (root / "tests").glob("test_*.py"):
        called.update(NPM_CALL.findall(module.read_text(encoding="utf-8")))
    missing = []
    for suite in sorted((panel / "tests").glob("*.e2e.mjs")):
        runners = {name for name, command in scripts.items()
                   if re.search(rf"(^|[\s/]){re.escape(suite.name)}(\s|$)", command)}
        if not runners & called:
            missing.append(suite.name)
    return missing


class PanelBrowserCoverageTests(unittest.TestCase):
    def test_every_browser_suite_has_a_module_or_a_stated_exemption(self):
        missing = [name for name in uncovered(ROOT) if name not in EXEMPT]
        self.assertEqual(missing, [], "add a tests/test_panel_e2e_<name>.py like test_panel_e2e_polish.py "
                                      "(or an EXEMPT entry with the reason CI cannot run it)")

    def test_every_exemption_names_a_suite_that_exists_and_is_still_uncovered(self):
        suites = {p.name for p in (ROOT / "panel" / "tests").glob("*.e2e.mjs")}
        stale = set(uncovered(ROOT))
        for name, reason in EXEMPT.items():
            with self.subTest(suite=name):
                self.assertIn(name, suites)
                self.assertIn(name, stale, "it has a module now; drop the exemption")
                self.assertTrue(reason.strip())

    def test_the_check_finds_an_unwrapped_suite(self):
        # Negative control: a suite with a script but no module, and one with
        # a module, in a throwaway tree; only the first may be reported.
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / "panel" / "tests").mkdir(parents=True)
            (root / "tests").mkdir()
            for name in ("a.e2e.mjs", "b.e2e.mjs"):
                (root / "panel" / "tests" / name).write_text("", encoding="utf-8")
            (root / "panel" / "package.json").write_text(json.dumps({"scripts": {
                "e2e:a": "node tests/a.e2e.mjs", "e2e:b": "node tests/b.e2e.mjs"}}), encoding="utf-8")
            (root / "tests" / "test_b.py").write_text('_npm("e2e:b")\n', encoding="utf-8")
            self.assertEqual(uncovered(root), ["a.e2e.mjs"])


if __name__ == "__main__":
    unittest.main()
