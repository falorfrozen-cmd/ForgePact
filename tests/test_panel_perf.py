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
each have a module of their own (`test_panel_oracle_replay.py`,
`test_panel_e2e*.py`), and all of them share `panel_browser.py`'s skip
conditions and npm helper. About six minutes.
"""
import re
import sys
import unittest
from pathlib import Path

PARALLEL_EXCLUSIVE = True

sys.path.insert(0, str(Path(__file__).resolve().parent))

from panel_browser import _missing, _npm  # noqa: E402


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
        # The verdict lines first: `out` is stdout then the sandboxes' stderr,
        # so a long stderr can push every FAIL line out of the tail.
        verdict = "\n".join(l for l in out.splitlines() if l.startswith(("FAIL ", "e2e-perf: ")))
        message = f"{verdict}\n--- tail ---\n{out[-4000:]}"
        self.assertEqual(code, 0, message)
        self.assertTrue(lines and re.fullmatch(r"e2e-perf: (\d+)/\1 checks passed", lines[-1]), message)


if __name__ == "__main__":
    unittest.main()
