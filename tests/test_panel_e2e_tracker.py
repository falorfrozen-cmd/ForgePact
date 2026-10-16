#!/usr/bin/env python3
"""The Tracker tab's e2e suite: `npm run e2e:tracker`.

It runs every `panel/tests/tracker/<name>.check.mjs` (#185): the tab's shell
(its nav entry in both layouts, the `mod_tracker` switch, when the page polls
`/api/tracker`), and each section's own checks, each module against its own
sandbox started with the tracker fixture.

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

    def test_tracker_e2e_suite_passes(self):
        code, out = _npm("e2e:tracker")
        lines = [l for l in out.splitlines() if l.startswith("e2e-tracker: ")]
        self.assertEqual(code, 0, out[-4000:])
        self.assertTrue(lines and re.fullmatch(r"e2e-tracker: (\d+)/\1 checks passed", lines[-1]), out[-4000:])


if __name__ == "__main__":
    unittest.main()
