#!/usr/bin/env python3
"""The review-fixes e2e suite: `npm run e2e:review`.

It checks the fixes from the 2.0 panel's reviews: the Enabled mods tray
popover, Turn off and its Undo toast (name, timing, same POSTs as the
control, where the toast sits), focus after a Turn off, the list held in its
form while a key or pointer is down, the slider switches, Setup and the
Satanic pool.

It had no module of its own until ForgePact's browser suites started running
on pull requests, so nothing ran it: it passed on main and failed on a pull
request with nobody told. `test_panel_browser_coverage.py` now fails when a
suite has no module. A module of its own so `tools/run_tests_parallel.py`
can run it beside the other panel browser suites
(`PARALLEL_GROUP = "panel-browser"`); the skip conditions and the npm helper
are `panel_browser.py`'s, imported, not copied.
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

    def test_review_fixes_e2e_suite_passes(self):
        code, out = _npm("e2e:review")
        lines = [l for l in out.splitlines() if l.startswith("e2e-review: ")]
        self.assertEqual(code, 0, out[-4000:])
        self.assertTrue(lines and re.fullmatch(r"e2e-review: (\d+)/\1 checks passed", lines[-1]), out[-4000:])


if __name__ == "__main__":
    unittest.main()
