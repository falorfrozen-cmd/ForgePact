"""Every Drop Rates row has an icon in the panel.

The rows come from `forgepact.KEYS`; their icons from `CONTROL_ICONS['keys']`
in panel/src/icons.js. Prime Evil Parts was added in 1.4.7 without one, so its
row was the only Drop Rates row with no icon in the 2.0 panel.
"""
import re
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "src"))
import forgepact

ICONS_JS = ROOT / "panel" / "src" / "icons.js"


def key_icons(source):
    """{setting key: icon name} from the `'keys': zip([...], [...])` literal, parsed as text."""
    m = re.search(r"'keys':\s*zip\(\[(.*?)\],\s*\[(.*?)\]\)", source, re.S)
    if m is None:
        raise AssertionError("CONTROL_ICONS 'keys' zip not found in icons.js")
    keys, icons = (re.findall(r"'([^']+)'", part) for part in m.groups())
    if len(keys) != len(icons):
        raise AssertionError(f"'keys' zip lists {len(keys)} keys but {len(icons)} icons")
    return dict(zip(keys, icons))


def icon_names(source):
    """The names defined in the ICONS object: lines like `  'name': ...`."""
    m = re.search(r"export const ICONS = \{(.*?)\n\};", source, re.S)
    if m is None:
        raise AssertionError("ICONS object not found in icons.js")
    return set(re.findall(r"^  '([a-z0-9-]+)':", m.group(1), re.M))


class KeyIconTests(unittest.TestCase):
    def setUp(self):
        self.source = ICONS_JS.read_text(encoding="utf-8")

    def test_every_drop_rates_row_has_an_icon(self):
        mapping = key_icons(self.source)
        missing = [key for key, *_ in forgepact.KEYS if key not in mapping]
        self.assertEqual(missing, [], "Drop Rates rows without an icon")

    def test_every_mapped_icon_is_drawn(self):
        drawn = icon_names(self.source)
        undrawn = sorted({icon for icon in key_icons(self.source).values() if icon not in drawn})
        self.assertEqual(undrawn, [], "icons mapped for Drop Rates rows but not defined in ICONS")

    def test_prime_evil_parts_has_its_own_icon(self):
        self.assertEqual(key_icons(self.source)["primeevil"], "prime-evil-part")


if __name__ == "__main__":
    unittest.main()
