"""The Prime Evil Parts slider: Key of Terror parts from bosses.

The six parts (Gurag's Soul, Death's Sigil, Damien's Eye, Anubis' Ankh, Karp
King's Bellybutton, Satan's Horn) and their six infernal versions share
LoadDrops type 41 with Relics. So the slider opens no gate. It only scales the
parts' own roll where the game already rolls it, which is on bosses. While the
Relic gate rolls, the plugin keeps skipping the part scripts, so raising Relics
never drops parts.

Measured on 2026-09-26 on Karp King, killed through the game's own death path:
about 0.7 bellybuttons per kill at x1, 3 at x5 and 9 at x35
(docs/prime-evil-parts-research.md).
"""
import re
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "src"))
import forgepact

PLUGIN = ROOT / "plugin" / "ModuleMain.cpp"

# The parts' category-13 names, and neighbours in the same category that must stay out.
PARTS = [
    "collectible_gurags_soul", "collectible_deaths_sigil", "collectible_damiens_eye",
    "collectible_anubis_ankh", "collectible_karp_kings_bellybutton", "collectible_satans_horn",
    "collectible_gurags_infernal_soul", "collectible_deaths_infernal_sigil",
    "collectible_damiens_infernal_eye", "collectible_anubis_infernal_ankh",
    "collectible_karp_kings_infernal_bellybutton", "collectible_satans_infernal_horn",
]
NOT_PARTS = [
    "collectible_battle_fragment", "collectible_dimensional_shard", "collectible_scroll_of_ra",
    "collectible_colosseum_fragment", "collectible_soul_of_anguish", "collectible_soul_of_despair",
    "collectible_soul_of_corruption", "collectible_soul_of_infernal_anguish",
]


def plugin_group(name):
    """(category, name fragments, drop type) of one kDropGruplar row."""
    source = PLUGIN.read_text(encoding="utf-8", errors="replace")
    m = re.search(r'\{\s*"%s",\s*(\d+),\s*"([^"]*)",\s*(-?\d+)\s*\}' % re.escape(name), source)
    if m is None:
        raise AssertionError(f"no kDropGruplar row named {name!r}")
    return int(m.group(1)), m.group(2).split(","), int(m.group(3))


def group_matches(fragments, key):
    """DropGrupUygula's rule: a trailing '$' matches the end of the name, anything else a substring."""
    key = key.lower()
    for fragment in (f.lower() for f in fragments if f):
        if fragment.endswith("$"):
            if key.endswith(fragment[:-1]):
                return True
        elif fragment in key:
            return True
    return False


class PrimeEvilPartsSliderTests(unittest.TestCase):
    def test_the_panel_offers_the_family_without_a_drop_type(self):
        entry = [k for k in forgepact.KEYS if k[0] == "primeevil"]
        self.assertEqual(len(entry), 1)
        self.assertIsNone(entry[0][2], "a drop type would open type 41, which drops Relics too")
        self.assertIn("Prime Evil", entry[0][1])

    def test_the_slider_scales_the_parts_own_roll_and_opens_no_gate(self):
        commands = forgepact.build_key_cmds({"primeevil": 10})
        self.assertIn("droprate group primeevil 10", commands)
        self.assertFalse([c for c in commands if c.startswith("dungeonkey")])

    def test_raising_relics_still_opens_type_41_for_relics_only(self):
        commands = forgepact.build_key_cmds({"relic": 5, "primeevil": 10})
        self.assertIn("dungeonkey add 41 5", commands)
        self.assertIn("droprate group primeevil 10", commands)

    def test_the_plugin_group_covers_all_twelve_parts_and_nothing_else(self):
        category, fragments, drop_type = plugin_group("primeevil")
        self.assertEqual((category, drop_type), (13, 41))
        self.assertEqual([k for k in PARTS if not group_matches(fragments, k)], [],
                         "every part must be scaled, infernal ones included")
        self.assertEqual([k for k in NOT_PARTS if group_matches(fragments, k)], [])

    def test_the_relic_gate_keeps_skipping_the_part_scripts(self):
        source = PLUGIN.read_text(encoding="utf-8", errors="replace")
        for script in ("DropBossParts", "DropUberParts"):
            self.assertIn(f'HookOneScript("{script}"', source)
            body = re.search(r"static RValue& Hook_%s\(.*?\n\}" % script, source, re.DOTALL)
            self.assertIsNotNone(body, script)
            self.assertIn("g_DkPartsGuard", body.group(0))

    def test_the_row_note_says_bosses_only(self):
        note = re.search(r"if\(key==='primeevil'\) return `([^`]*)`", forgepact.HTML)
        self.assertIsNotNone(note)
        self.assertIn("bosses", note.group(1))


if __name__ == "__main__":
    unittest.main()
