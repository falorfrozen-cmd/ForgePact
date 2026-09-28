"""Keep the review regressions in the release gate, in Ember and Ledger."""
import re
import sys
import unittest
from pathlib import Path

PARALLEL_GROUP = "panel-browser"
sys.path.insert(0, str(Path(__file__).resolve().parent))
from panel_browser import _missing, _npm  # noqa: E402


class ReviewBrowserTests(unittest.TestCase):
    def test_ember_shortcuts_and_search(self):
        missing = _missing()
        if missing:
            self.skipTest(missing)
        code, out = _npm("e2e:ember")
        self.assertEqual(code, 0, out[-6000:])
        self.assertRegex(out, r'"passed":\s*9')

    def test_review_regressions(self):
        missing = _missing()
        if missing:
            self.skipTest(missing)
        code, out = _npm("e2e:review")
        self.assertEqual(code, 0, out[-6000:])
        lines = [line for line in out.splitlines() if line.startswith("e2e-review: ")]
        self.assertTrue(lines and re.fullmatch(r"e2e-review: (\d+)/\1 checks passed", lines[-1]), out[-6000:])


if __name__ == "__main__":
    unittest.main()
