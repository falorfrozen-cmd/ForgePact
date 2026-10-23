#!/usr/bin/env python3
"""The panel half of "Sell loot your filter hides" (`autosell`, ForgePact #196).

The plugin half (which drops are sold, the price, the gold call and the
interplay with hidden loot's sleep) is pinned by the plugin's own tests. This
file pins what the panel owns:

- the switch is off by default, so a fresh install sends no `autosell` line;
- with the switch on, startup sends `autosell 1`, and nothing else for it;
- `/api/set` of the switch sends `autosell 1|0` while the game runs, and
  nothing while it is closed, and the value is saved either way;
- the row sits in the Mods tab's Quality of Life card (`qolCard`), straight
  after Sleep loot your filter hides and its show key, before Jump through
  scenery;
- its label and text are the player-facing ones: at most 300 characters,
  naming the merchant's price, no buyback and Sleep loot your filter hides,
  ending `Off by default.` and naming no game object, counter or measurement;
- it is a boolean mod for the "Enabled mods" list, and a native boolean the
  behaviour oracle derives steps for.

Runs against `PanelSandbox` (isolated settings, `send_cmds` captured, never
the game's IPC) and the panel's source read as text (tests/panel_source.py),
never a built file.
"""

import json
import sys
import unittest
from pathlib import Path

TESTS = Path(__file__).resolve().parent
ROOT = TESTS.parent
sys.path.insert(0, str(ROOT / "src"))
sys.path.insert(0, str(TESTS))

import forgepact  # noqa: E402
from panel_source import js_for_node, panel_file  # noqa: E402
from test_jump_scenery_panel_contract import LiveSandbox, defaults, qol_card, qol_row, row_text  # noqa: E402
from test_panel_performance import run_node  # noqa: E402

LABEL = "Sell loot your filter hides"
DESCRIPTION = ("Items your loot filter hides are sold the moment they drop, for the gold a merchant "
               "would pay, and none of them goes on the merchant's buyback list. With Sleep loot your "
               "filter hides also on, selling comes first: only what can't be sold is put to sleep. "
               "Off by default.")


def autosell_lines(cmds):
    return [c for c in cmds if c.startswith("autosell")]


class AutoSellPanelBaselineTests(unittest.TestCase):
    """The defaults: off, and nothing sent."""

    def test_the_switch_is_off_by_default(self):
        self.assertIs(forgepact.DEFAULTS["mod_auto_sell"], False)

    def test_build_cmds_with_the_defaults_sends_no_autosell_line(self):
        self.assertEqual(autosell_lines(forgepact.build_cmds(defaults())), [])

    def test_off_sends_nothing_rather_than_autosell_0(self):
        cfg = {**defaults(), "mod_auto_sell": False}
        self.assertEqual(autosell_lines(forgepact.build_cmds(cfg)), [])


class AutoSellPanelBuildCmdsTests(unittest.TestCase):
    """Startup with the switch on."""

    def test_on_sends_autosell_1_once(self):
        cfg = {**defaults(), "mod_auto_sell": True}
        self.assertEqual(autosell_lines(forgepact.build_cmds(cfg)), ["autosell 1"])

    def test_the_switch_adds_exactly_one_line_to_startup(self):
        off = forgepact.build_cmds(defaults())
        on = forgepact.build_cmds({**defaults(), "mod_auto_sell": True})
        self.assertEqual(len(on), len(off) + 1)

    def test_with_hidden_loot_also_on_both_switches_are_sent(self):
        # Sell supersedes hide in the plugin; the panel sends both switches
        # as they are and leaves the order of work to the plugin.
        cmds = forgepact.build_cmds({**defaults(), "mod_auto_sell": True, "mod_hidden_loot": True})
        self.assertEqual(autosell_lines(cmds), ["autosell 1"])
        self.assertIn("hiddenloot 1", cmds)


class AutoSellPanelApiSetTests(unittest.TestCase):
    """`/api/set` while the game runs, and while it does not."""

    def test_the_switch_sends_autosell_1_and_0_and_is_saved(self):
        live = LiveSandbox(self)
        code, body, sent = live.post("mod_auto_sell", True)
        self.assertEqual(code, 200, body)
        self.assertEqual(sent, [["autosell 1"]])
        self.assertIs(live.saved()["mod_auto_sell"], True)
        code, body, sent = live.post("mod_auto_sell", False)
        self.assertEqual(code, 200, body)
        self.assertEqual(sent, [["autosell 0"]])
        self.assertIs(live.saved()["mod_auto_sell"], False)

    def test_nothing_is_sent_while_the_game_is_closed(self):
        closed = LiveSandbox(self, running=False)
        for value in (True, False):
            with self.subTest(value=value):
                code, _, sent = closed.post("mod_auto_sell", value)
                self.assertEqual((code, sent), (200, []))
                self.assertIs(closed.saved()["mod_auto_sell"], value)


class AutoSellPanelRowTests(unittest.TestCase):
    """The Mods-tab row and how it is wired."""

    def test_the_row_sits_in_the_quality_of_life_card_after_hidden_loot(self):
        self.assertIn('id="mod_auto_sell"', qol_card())
        mods = panel_file("tabs/Mods.svelte")
        self.assertEqual(mods.count('id="mod_auto_sell"'), 1)
        key_row, sell, jump = (mods.index(f'id="{i}"') for i in
                               ("mod_hidden_loot_key_row", "mod_auto_sell", "mod_jump_scenery"))
        self.assertLess(key_row, sell)
        self.assertLess(sell, jump)
        # Straight after the show key's row, and straight before Jump through
        # scenery: one row of its own between them.
        self.assertEqual(mods[key_row:sell].count('<div class="row"'), 1)
        self.assertEqual(mods[sell:jump].count('<div class="row"'), 1)

    def test_the_row_is_a_switch_with_its_value(self):
        row = qol_row("mod_auto_sell")
        self.assertIn('<label class="switch"><input type="checkbox" id="mod_auto_sell" aria-label="Sell loot your filter hides"><span class="sl"></span></label>', row)
        self.assertIn('<span class="val" id="masval">off</span>', row)

    def test_the_label_and_text_are_the_player_facing_ones(self):
        label, text = row_text(qol_row("mod_auto_sell"))
        self.assertEqual(label, LABEL)
        self.assertEqual(text, DESCRIPTION)
        self.assertLessEqual(len(text), 300, text)
        self.assertTrue(text.endswith("Off by default."), text)
        # What the owner decided (2026-10-23): a merchant's price, no buyback,
        # and selling comes before Sleep loot your filter hides.
        self.assertIn("merchant", text)
        self.assertIn("buyback", text)
        self.assertIn("Sleep loot your filter hides", text)
        # No game object, counter or measurement.
        self.assertNotRegex(text, r"\d", text)
        self.assertNotRegex(text, r"_obj|_Parent|[a-z][A-Z]", text)
        for word in ("autosell", "hiddenloot", "#196", "fingerprint", "sell pool", "frame", "measured"):
            self.assertNotIn(word, text, word)

    def test_the_switch_posts_a_boolean_and_redraws_its_value(self):
        panel = panel_file("panel.js")
        handler = panel.split("document.getElementById('mod_auto_sell').onchange", 1)[1][:500]
        self.assertIn("{key:'mod_auto_sell',value:e.target.checked}", handler)
        self.assertIn("document.getElementById('masval')", handler)
        self.assertIn("mod_auto_sell:'mod_auto_sell'", panel)
        self.assertIn("masval:'mod_auto_sell'", panel)

    def test_the_switch_is_a_boolean_mod(self):
        mods = panel_file("enabled-mods.js")
        listed = mods.split("export const BOOLEAN_MODS = [", 1)[1].split("];", 1)[0]
        self.assertIn("'mod_auto_sell'", listed)

    def test_enabled_mods_lists_it_only_while_on(self):
        base = defaults()
        cases = {"defaults": base, "on": {**base, "mod_auto_sell": True}}
        driver = (f"const cases={json.dumps(cases)};"
                  "console.log(JSON.stringify(Object.fromEntries(Object.entries(cases)"
                  ".map(([k,c])=>[k,enabledControls(c)]))));")
        result = run_node(js_for_node(panel_file("enabled-mods.js")), driver)
        self.assertEqual(result, {"defaults": [], "on": ["mod_auto_sell"]})

    def test_the_behaviour_oracle_derives_it_as_a_native_boolean(self):
        derive = (ROOT / "panel" / "tests" / "oracle-derive.mjs").read_text(encoding="utf-8")
        self.assertIn("{ key: 'mod_auto_sell', tab: 'tab:mods', sub: 'subtab:qol', verb: 'autosell' }", derive)
        derived = (ROOT / "panel" / "tests" / "behaviour-oracle-derived.json").read_text(encoding="utf-8")
        self.assertIn('"#mod_auto_sell"', derived)
        self.assertIn("autosell 1", derived)
        self.assertIn("autosell 0", derived)


if __name__ == "__main__":
    unittest.main()
