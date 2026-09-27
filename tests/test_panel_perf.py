#!/usr/bin/env python3
"""The panel's perf suite, run from the Python suite, with nothing beside it.

`npm run e2e:perf` measures every interaction the owner named (the Enabled
mods list and its tray, toggles, sliders, tabs, searches, hovers, idle polls)
with rendering on, and holds each to the owner's hard budgets (longest frame
50 ms, no frame over 50 ms, input-to-next-paint and result paint p95 100 ms,
no idle-poll mutation), medians over five runs.

It measures frame timings, so it cannot share the CPU with other test
modules: under `tools/run_tests_parallel.py` a native harness's compile or
another browser suite beside it put one frame over the budget, which measured
the machine, not the panel. `PARALLEL_EXCLUSIVE` makes that runner start this
module last and alone; the serial run is unaffected. The other browser suites
stay in `test_panel_oracle.py`, whose skip conditions and npm helper this
module shares. About six minutes.
"""
import re
import sys
import unittest
from pathlib import Path

PARALLEL_EXCLUSIVE = True

sys.path.insert(0, str(Path(__file__).resolve().parent))

from test_panel_oracle import _missing, _npm  # noqa: E402


class PanelPerfSuiteTests(unittest.TestCase):
    def setUp(self):
        missing = _missing()
        if missing:
            self.skipTest(missing)

    def test_perf_e2e_suite_passes(self):
        # The owner's hard budgets, on medians over five runs; if CI is noisy,
        # raise --runs, never the budgets (forgepact-ui-responsive, D10).
        code, out = _npm("e2e:perf")
        lines = [l for l in out.splitlines() if l.startswith("e2e-perf: ")]
        self.assertEqual(code, 0, out[-4000:])
        self.assertTrue(lines and re.fullmatch(r"e2e-perf: (\d+)/\1 checks passed", lines[-1]), out[-4000:])


if __name__ == "__main__":
    unittest.main()
