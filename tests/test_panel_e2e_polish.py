#!/usr/bin/env python3
"""The polish e2e suite: `npm run e2e:polish`.

It checks the owner's polish pass: the Mining Ore Multiplier label, one card
per Mods mod, the theme on Setup, the plugin warning icons and their
tooltips, the Modifiers separators, the helmet's accent, and an idle slider's
note as a tooltip that moves no row.

A module of its own so `tools/run_tests_parallel.py` can run it beside the
other panel browser suites (`PARALLEL_GROUP = "panel-browser"`); the skip
conditions and the npm helper are `panel_browser.py`'s, imported, not copied.
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

    def test_polish_e2e_suite_passes(self):
        code, out = _npm("e2e:polish")
        lines = [l for l in out.splitlines() if l.startswith("e2e-polish: ")]
        self.assertEqual(code, 0, out[-4000:])
        self.assertTrue(lines and re.fullmatch(r"e2e-polish: (\d+)/\1 checks passed", lines[-1]), out[-4000:])


if __name__ == "__main__":
    unittest.main()
