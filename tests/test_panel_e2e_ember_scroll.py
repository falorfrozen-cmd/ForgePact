"""Exercise actual Ember scrolling in release CI, with visible native scrollbars."""
import re
import sys
import unittest
from pathlib import Path

PARALLEL_GROUP = "panel-browser"
sys.path.insert(0, str(Path(__file__).resolve().parent))
from panel_browser import _missing, _npm  # noqa: E402


class EmberScrollBrowserTests(unittest.TestCase):
    def test_content_and_sidebar_scroll(self):
        missing = _missing()
        if missing:
            self.skipTest(missing)
        code, out = _npm("e2e:ember-scroll")
        self.assertEqual(code, 0, out[-4000:])
        lines = [line for line in out.splitlines() if line.startswith("e2e-ember-scroll: ")]
        self.assertTrue(lines and re.fullmatch(r"e2e-ember-scroll: (\d+)/\1 checks passed", lines[-1]), out[-4000:])


if __name__ == "__main__":
    unittest.main()
