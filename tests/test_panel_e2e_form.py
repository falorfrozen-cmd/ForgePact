#!/usr/bin/env python3
"""The Enabled mods form e2e suite: `npm run e2e:form`.

It checks the Enabled mods list's two forms (the inline row while its
entries fit on one line, the tray once they overfill it, and the hysteresis
band between), the entries' names, and the remembered value on a
switched-off slider.

Like `test_panel_e2e_review.py`, it had no module of its own until the
browser suites started running on pull requests; see
`test_panel_browser_coverage.py`. A module of its own so
`tools/run_tests_parallel.py` can run it beside the other panel browser
suites (`PARALLEL_GROUP = "panel-browser"`); the skip conditions and the npm
helper are `panel_browser.py`'s, imported, not copied.
"""
import re
import sys
import unittest
from pathlib import Path

PARALLEL_GROUP = "panel-browser"

sys.path.insert(0, str(Path(__file__).resolve().parent))

from panel_browser import _missing, _npm  # noqa: E402


class PanelBrowserSuiteTests(unittest.TestCase):
    def setUp(self):
        missing = _missing()
        if missing:
            self.skipTest(missing)

    def test_enabled_mods_form_e2e_suite_passes(self):
        code, out = _npm("e2e:form")
        lines = [l for l in out.splitlines() if l.startswith("e2e-form: ")]
        self.assertEqual(code, 0, out[-4000:])
        self.assertTrue(lines and re.fullmatch(r"e2e-form: (\d+)/\1 checks passed", lines[-1]), out[-4000:])


if __name__ == "__main__":
    unittest.main()
