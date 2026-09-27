#!/usr/bin/env python3
"""The panel's behaviour oracle, replayed against the current build.

`npm run oracle:replay` replays `panel/tests/behaviour-oracle.json` (every
control's POST bodies and plugin commands, recorded from the old page)
against the current build and fails on any difference, and with it
`panel/tests/behaviour-oracle-gems.json` (the Gems of Incarnation controls,
recorded from origin/main's last pre-port page, named by its `sourceRev`)
and `panel/tests/behaviour-oracle-primeevil.json` (the Prime Evil Parts
slider, recorded from origin/main's 1.4.7 page at 841c2db). Because the
backend now sends the Prime Evil reset line in every full key reset, the
legacy recording and the Gems supplement are replayed through
`insertAddedKeys` (panel/tests/lib/oracle-relocate.mjs), which inserts
exactly that line; the files themselves are never edited, and
`test_panel_oracle.py` checks, with no browser, that the inserted line is what
the merged backend sends and where each recording came from.

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

    def test_behaviour_oracle_replays_with_no_mismatch(self):
        code, out = _npm("oracle:replay")
        lines = [l for l in out.splitlines() if l.startswith("oracle: ")]
        self.assertEqual(code, 0, out[-4000:])
        self.assertTrue(lines and re.fullmatch(r"oracle: \d+ steps, 0 mismatches", lines[-1]), out[-4000:])


if __name__ == "__main__":
    unittest.main()
