#!/usr/bin/env python3
"""The Gems of Incarnation e2e suite: `npm run e2e:gems`.

It checks the Gems of Incarnation controls' place, defaults, Enabled mods
entries and mod filter list.

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

    def test_gems_e2e_suite_passes(self):
        code, out = _npm("e2e:gems")
        lines = [l for l in out.splitlines() if l.startswith("e2e-gems: ")]
        self.assertEqual(code, 0, out[-4000:])
        self.assertTrue(lines and re.fullmatch(r"e2e-gems: (\d+)/\1 checks passed", lines[-1]), out[-4000:])


if __name__ == "__main__":
    unittest.main()
