#!/usr/bin/env python3
"""The panel half of "Boss soul pieces" (`soulpiece`, ForgePact #197).

The plugin half (the drop roll, the act table and the transfer) is pinned by
the plugin's own tests. This file pins what the panel owns:

- the switch is off by default, so a fresh install sends no `soulpiece` line;
- with the switch on, startup sends `soulpiece 1`, and nothing else for it;
- `/api/set` of the switch sends `soulpiece 1|0` while the game runs, and
  nothing while it is closed;
- the row sits on the Loot tab, in its own card `#soulPiecesCard` right after
  the Gems of Incarnation card, in the Gems rows' feature-row shape;
- its text is short, player-facing and ends `Off by default.`;
- it is a boolean mod for the "Enabled mods" list, and the Loot search hides
  its card as it hides the Gems card.

Runs against `PanelSandbox` (isolated settings, `send_cmds` captured, never
the game's IPC) and the panel's source read as text (tests/panel_source.py),
never a built file.
"""

import json
import re
import sys
import unittest
from pathlib import Path

TESTS = Path(__file__).resolve().parent
ROOT = TESTS.parent
sys.path.insert(0, str(ROOT / "src"))
sys.path.insert(0, str(TESTS))

import forgepact  # noqa: E402
from panel_source import js_for_node, panel_file  # noqa: E402
from test_jump_scenery_panel_contract import LiveSandbox, defaults  # noqa: E402
from test_panel_performance import run_node  # noqa: E402


KEY = "mod_boss_soul_pieces"


def soulpiece_lines(cmds):
    return [c for c in cmds if c.startswith("soulpiece")]


def soul_card():
    """The Loot.svelte Boss soul pieces card, up to the next card."""
    loot = panel_file("tabs/Loot.svelte")
    start = loot.index('id="soulPiecesCard"')
    end = loot.find('<div class="card', start)
    return loot[start:end if end >= 0 else len(loot)]


def feature_row_text(card):
    """(label, description) of the card's feature row, tags taken out."""
    label = re.search(r'<span class="lbl"[^>]*>([^<]*)</span>', card)
    text = re.search(r'<span class="feature-description">(.*?)</span>', card, re.S)
    if label is None or text is None:
        raise AssertionError(card)
    return label.group(1), re.sub(r"<[^>]+>", "", text.group(1))


class SoulPiecePanelBaselineTests(unittest.TestCase):
    """The defaults: off, and nothing sent."""

    def test_the_switch_is_off_by_default(self):
        self.assertIs(forgepact.DEFAULTS[KEY], False)

    def test_build_cmds_with_the_defaults_sends_no_soulpiece_line(self):
        self.assertEqual(soulpiece_lines(forgepact.build_cmds(defaults())), [])

    def test_off_sends_nothing_rather_than_soulpiece_0(self):
        cfg = {**defaults(), KEY: False}
        self.assertEqual(soulpiece_lines(forgepact.build_cmds(cfg)), [])


class SoulPiecePanelBuildCmdsTests(unittest.TestCase):
    """Startup with the switch on."""

    def test_on_sends_soulpiece_1_once(self):
        cfg = {**defaults(), KEY: True}
        self.assertEqual(soulpiece_lines(forgepact.build_cmds(cfg)), ["soulpiece 1"])

    def test_the_switch_adds_exactly_one_line_to_startup(self):
        off = forgepact.build_cmds(defaults())
        on = forgepact.build_cmds({**defaults(), KEY: True})
        self.assertEqual(len(on), len(off) + 1)


class SoulPiecePanelApiSetTests(unittest.TestCase):
    """`/api/set` while the game runs, and while it does not: the verb at both ends."""

    def test_the_switch_sends_soulpiece_1_and_0(self):
        live = LiveSandbox(self)
        code, body, sent = live.post(KEY, True)
        self.assertEqual(code, 200, body)
        self.assertEqual(sent, [["soulpiece 1"]])
        self.assertIs(live.saved()[KEY], True)
        code, body, sent = live.post(KEY, False)
        self.assertEqual(code, 200, body)
        self.assertEqual(sent, [["soulpiece 0"]])
        self.assertIs(live.saved()[KEY], False)

    def test_nothing_is_sent_while_the_game_is_closed(self):
        closed = LiveSandbox(self, running=False)
        for value in (True, False):
            with self.subTest(value=value):
                code, _, sent = closed.post(KEY, value)
                self.assertEqual((code, sent), (200, []))
                self.assertIs(closed.saved()[KEY], value)


class SoulPiecePanelRowTests(unittest.TestCase):
    """The Loot-tab card and how it is wired."""

    def test_the_card_sits_on_the_loot_tab_after_the_gems_card(self):
        loot = panel_file("tabs/Loot.svelte")
        self.assertEqual(loot.count(f'id="{KEY}"'), 1)
        self.assertLess(loot.index('id="gemsCard"'), loot.index('id="soulPiecesCard"'))
        # Right after it: no other card between the two.
        between = loot[loot.index('id="gemsCard"'):loot.index('id="soulPiecesCard"')]
        self.assertEqual(between.count('<div class="card'), 1, between)
        self.assertIn('<div class="card tab-card" data-tab="loot" id="soulPiecesCard">', loot)
        self.assertIn("<h2>Boss soul pieces</h2>", soul_card())
        # Nowhere else on the page.
        self.assertNotIn(f'id="{KEY}"', panel_file("tabs/Mods.svelte"))

    def test_the_row_is_a_switch_with_its_value(self):
        card = soul_card()
        self.assertIn('<div class="row feature-row"', card)
        self.assertIn(f'<label class="switch"><input type="checkbox" id="{KEY}"><span class="sl"></span></label>', card)
        self.assertIn('<span class="val" id="mbspval">off</span>', card)
        self.assertEqual(feature_row_text(card)[0], "Boss soul pieces")

    def test_the_description_is_short_and_says_what_it_does_for_the_player(self):
        _, text = feature_row_text(soul_card())
        self.assertLessEqual(len(text), 300, text)
        self.assertTrue(text.endswith("Off by default."), text)
        # What it does, and the one action the player takes.
        self.assertIn("Right-click a piece in your bag", text)
        self.assertIn("boss's lair", text)
        self.assertIn("fresh fight", text)
        # Pieces are bag consumables that move the player straight into the
        # boss room (Amendment 2): the old text's Key tab, summon near the
        # player and portal (Replan 1) are gone.
        self.assertNotIn("portal", text.lower(), text)
        for word in ("Key tab", "stack", "summon", "outside town"):
            self.assertNotIn(word, text, word)
        # No game object, counter or measurement.
        self.assertNotRegex(text, r"\d", text)
        self.assertNotRegex(text, r"_obj|_Parent|[a-z][A-Z]", text)
        for word in ("soulpiece", "#197", "seed", "spawner", "density", "measured"):
            self.assertNotIn(word, text, word)

    def test_the_switch_posts_a_boolean_and_redraws_its_value(self):
        panel = panel_file("panel.js")
        self.assertIn(f"['{KEY}','mbspval','{KEY}','Boss soul pieces']", panel)
        # The handler follows the loop's list, wherever the entry sits in it:
        # from the entry to the list's closing `]){`, not the entry's own `]`.
        after = panel.split(f"['{KEY}','mbspval','{KEY}','Boss soul pieces']", 1)[1]
        handler = after[after.index("]){") + 3:][:500]
        self.assertIn("{key:key,value:e.target.checked}", handler)
        self.assertIn("document.getElementById(val)", handler)
        self.assertIn(f"['{KEY}','mbspval','{KEY}']", panel)
        self.assertIn(f"{KEY}:'{KEY}'", panel)
        self.assertIn(f"mbspval:'{KEY}'", panel)

    def test_the_loot_search_hides_the_card(self):
        panel = panel_file("panel.js")
        search = panel.split("export function filterControlRows(){", 1)[1]
        search = search[:search.index("\n}\n")]
        self.assertIn("document.getElementById('soulPiecesCard')", search)
        self.assertIn(f"document.getElementById('{KEY}').checked", search)

    def test_the_switch_is_a_boolean_mod(self):
        mods = panel_file("enabled-mods.js")
        listed = mods.split("export const BOOLEAN_MODS = [", 1)[1].split("];", 1)[0]
        self.assertIn(f"'{KEY}'", listed)

    def test_enabled_mods_lists_it_only_while_on(self):
        base = defaults()
        cases = {"defaults": base, "on": {**base, KEY: True}}
        driver = (f"const cases={json.dumps(cases)};"
                  "console.log(JSON.stringify(Object.fromEntries(Object.entries(cases)"
                  ".map(([k,c])=>[k,enabledControls(c)]))));")
        result = run_node(js_for_node(panel_file("enabled-mods.js")), driver)
        self.assertEqual(result, {"defaults": [], "on": [KEY]})


if __name__ == "__main__":
    unittest.main()
