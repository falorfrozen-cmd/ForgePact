#!/usr/bin/env python3
"""World > Monster Rarity names its two rows by the tier the game shows (#159).

The hook raises a normal monster to `enemyRarity` 3 or 4. The game shows rank 3
as an Ancient (yellow name) and rank 4 as a Legion, so the row that writes
rank 3 (`rarity_rare`) reads "Ancient" and the row that writes rank 4
(`rarity_ancient`) reads "Legion". Until #159 the rows said "Rare" and
"Ancient", one tier too low.

Only words changed. The config keys, element ids, the `rarity <rank3> <rank4>`
command and the plugin's tier mapping are the baseline half of this file and
stay as they were, so saved settings and the behaviour oracles carry over.
"""

import json
import re
import sys
import unittest
from pathlib import Path

TESTS = Path(__file__).resolve().parent
ROOT = TESTS.parent
sys.path.insert(0, str(TESTS))
sys.path.insert(0, str(ROOT / "src"))

import forgepact  # noqa: E402
from panel_source import panel_file  # noqa: E402

EXPORT = ROOT / "panel" / "design" / "figma-export.json"
NOTES = ROOT / "release-notes-v2.2.0.md"
PLUGIN = ROOT / "plugin" / "ModuleMain.cpp"

SUMMARY = "Choose the share of normal monsters upgraded to Ancient or Legion."
OLD_SUMMARY = "upgraded to Rare or Ancient"


def _rarity_card(world):
    """The `#rarityCard` block of World.svelte, opening tag to closing div."""
    start = world.index('id="rarityCard"')
    start = world.rindex("<div", 0, start)
    end = world.index("\n</div>", start)
    return world[start:end + len("\n</div>")]


def _row(card, slider_id):
    """The `.row` div that holds the slider `slider_id`."""
    at = card.index(f'id="{slider_id}"')
    start = card.rindex('<div class="row"', 0, at)
    end = card.index("</div>", at)
    return card[start:end + len("</div>")]


def _strings(node):
    """Every string anywhere in a parsed JSON value."""
    if isinstance(node, str):
        yield node
    elif isinstance(node, dict):
        for value in node.values():
            yield from _strings(value)
    elif isinstance(node, list):
        for value in node:
            yield from _strings(value)


class MonsterRarityLabelTests(unittest.TestCase):
    """Target: the words a player sees name the game's tiers."""

    def setUp(self):
        self.world = panel_file("tabs/World.svelte")
        self.card = _rarity_card(self.world)
        self.panel = panel_file("panel.js")

    def test_rank3_row_reads_ancient(self):
        row = _row(self.card, "rarity_rare")
        self.assertIn('<span class="lbl">Ancient</span>', row)
        self.assertIn('data-switch="rarity_rare" aria-label="Enable Ancient"', row)

    def test_rank4_row_reads_legion(self):
        row = _row(self.card, "rarity_ancient")
        self.assertIn('<span class="lbl">Legion</span>', row)
        self.assertIn('data-switch="rarity_ancient" aria-label="Enable Legion"', row)

    def test_ancient_row_comes_first(self):
        self.assertLess(self.card.index('id="rarity_rare"'), self.card.index('id="rarity_ancient"'))

    def test_card_never_says_rare(self):
        self.assertIsNone(re.search(r"\brares?\b", self.card, re.I),
                          "the Monster Rarity card still says rare")

    def test_summary_and_note(self):
        self.assertIn(f"rarityCard:'{SUMMARY}'", self.panel)
        self.assertIn(
            "`of the normal monsters: ${a}% Legion, ${r}% Ancient, "
            "${Math.max(0,100-r-a)}% stay normal`",
            self.panel)

    def test_design_export_moves_with_the_page(self):
        export = json.loads(EXPORT.read_text(encoding="utf-8"))
        self.assertEqual(export["amendments"]["enabledMods"]["entryTitle"]["example"],
                         "Monster Rarity › Ancient")
        holding = [s for s in export["screens"] if SUMMARY in s.get("texts", [])]
        self.assertTrue(holding, "no screen in the export carries the new summary")
        for screen in holding:
            with self.subTest(screen=screen.get("name", screen.get("id"))):
                self.assertIn("Ancient", screen["texts"])
                self.assertIn("Legion", screen["texts"])
        for text in _strings(export):
            self.assertNotEqual(text, "Rare")
            self.assertNotIn(OLD_SUMMARY, text)

    def test_release_notes_record_the_fix(self):
        # Published notes leave main through forgepact-notes-cleanup.yml.
        if not NOTES.is_file():
            self.skipTest("release-notes-v2.2.0.md is published and gone from main")
        text = NOTES.read_text(encoding="utf-8")
        self.assertIn("\n## Fixed", text)
        fixed = text[text.index("\n## Fixed"):]
        nxt = fixed.find("\n## ", 1)
        fixed = fixed if nxt < 0 else fixed[:nxt]
        for word in ("Monster Rarity", "Ancient", "Legion"):
            with self.subTest(word=word):
                self.assertIn(word, fixed)


class MonsterRarityBehaviourBaselineTests(unittest.TestCase):
    """Baseline: keys, ids, the command and the plugin are unchanged."""

    def test_config_keys(self):
        self.assertEqual(forgepact.DEFAULTS["rarity_rare"], 0)
        self.assertEqual(forgepact.DEFAULTS["rarity_ancient"], 0)
        self.assertIn("rarity_rare", forgepact.SLIDER_SWITCH_IDS)
        self.assertIn("rarity_ancient", forgepact.SLIDER_SWITCH_IDS)

    def test_command(self):
        self.assertEqual(forgepact.rarity_cmd({"rarity_rare": 25, "rarity_ancient": 15}),
                         "rarity 25 15")

    def test_element_ids(self):
        world = panel_file("tabs/World.svelte")
        for element_id in ("rarity_rare", "sw_rarity_rare", "rarity_ancient",
                           "sw_rarity_ancient", "rarityrareval", "rarityancval"):
            with self.subTest(element_id=element_id):
                self.assertIn(f'id="{element_id}"', world)

    def test_plugin_tier_mapping(self):
        plugin = PLUGIN.read_text(encoding="utf-8", errors="replace")
        self.assertIn("if (roll < g_RarAncientPct) tier = 4;", plugin)
        self.assertIn("else if (roll < g_RarAncientPct + g_RarRarePct) tier = 3;", plugin)


if __name__ == "__main__":
    unittest.main()
