#!/usr/bin/env python3
"""The finish-review e2e suite: `npm run e2e:finish`.

It checks the finish review's eight fixes, and holds the ThemePicker's posts
to the derived oracle's theme steps.

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

    def test_finish_e2e_suite_passes(self):
        # The Impeccable finish review's eight owner-approved fixes, the
        # ThemePicker's posts held to the derived oracle's theme steps
        # (forgepact-ui-ship, round 2).
        code, out = _npm("e2e:finish")
        lines = [l for l in out.splitlines() if l.startswith("e2e-finish: ")]
        self.assertEqual(code, 0, out[-4000:])
        self.assertTrue(lines and re.fullmatch(r"e2e-finish: (\d+)/\1 checks passed", lines[-1]), out[-4000:])


if __name__ == "__main__":
    unittest.main()
