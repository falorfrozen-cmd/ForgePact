#!/usr/bin/env python3
"""The motion e2e suite: `npm run e2e:motion`.

It checks the export's motion (hover, press, the tray, the theme picker, the
tooltips, the toasts, the removed entry) and reduced motion removing movement
while the fades stay.

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

    def test_motion_e2e_suite_passes(self):
        # The export's motion.notes, reduced motion removing movement and scale
        # while opacity and colour fades stay (amendments.ship), and the
        # owner's F2/F4/E4 (forgepact-ui-ship).
        code, out = _npm("e2e:motion")
        lines = [l for l in out.splitlines() if l.startswith("e2e-motion: ")]
        self.assertEqual(code, 0, out[-4000:])
        self.assertTrue(lines and re.fullmatch(r"e2e-motion: (\d+)/\1 checks passed", lines[-1]), out[-4000:])


if __name__ == "__main__":
    unittest.main()
