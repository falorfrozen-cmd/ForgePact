#!/usr/bin/env python3
"""The panel half of "Fill the map as you approach" (`fillroll`, ForgePact #183).

The plugin half (the reach, the held-back creators and the re-arm on turning
it off) is pinned by test_rolling_fill_contract.py and exercised by
test_rolling_fill_behavior.py. This file pins what the panel owns:

- the switch (`fill_rolling`) is off by default, so a fresh install sends no
  `fillroll` line;
- with the switch on, startup sends `fillroll 1`, and nothing else for it;
- `/api/set` of the switch sends `fillroll 1|0` while the game runs, and
  nothing while it is closed;
- the row is a top-level row of the Mods tab's Quality of Life card
  (`qolCard`), directly after Extra packs as you approach;
- its text is short, player-facing, names Map Reveal's "Really spawn every
  pack on arrival" (the option it changes) and ends `Off by default.`, and
  names no game object, counter, reach or measurement;
- it is a boolean mod for the "Enabled mods" list.

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

KEY = "fill_rolling"


def fillroll_lines(cmds):
    return [c for c in cmds if c.startswith("fillroll")]


class RollingFillPanelBaselineTests(unittest.TestCase):
    """The defaults: off, and nothing sent."""

    def test_the_switch_is_off_by_default(self):
        self.assertIs(forgepact.DEFAULTS[KEY], False)

    def test_build_cmds_with_the_defaults_sends_no_fillroll_line(self):
        self.assertEqual(fillroll_lines(forgepact.build_cmds(defaults())), [])

    def test_off_sends_nothing_rather_than_fillroll_0(self):
        cfg = {**defaults(), KEY: False}
        self.assertEqual(fillroll_lines(forgepact.build_cmds(cfg)), [])

    def test_off_sends_nothing_even_with_the_fill_on(self):
        # The fill alone is the baseline: switching Map Reveal's spawn pass on
        # never brings a fillroll line with it.
        cfg = {**defaults(), "map_reveal": True, "map_reveal_packs": True, "map_reveal_spawn": True, KEY: False}
        self.assertEqual(fillroll_lines(forgepact.build_cmds(cfg)), [])


class RollingFillPanelBuildCmdsTests(unittest.TestCase):
    """Startup with the switch on."""

    def test_on_sends_fillroll_1_once(self):
        cfg = {**defaults(), KEY: True}
        self.assertEqual(fillroll_lines(forgepact.build_cmds(cfg)), ["fillroll 1"])

    def test_the_switch_adds_exactly_one_line_to_startup(self):
        off = forgepact.build_cmds(defaults())
        on = forgepact.build_cmds({**defaults(), KEY: True})
        self.assertEqual(len(on), len(off) + 1)

    def test_on_sends_the_same_line_with_the_fill_on(self):
        # The panel sends the switch on its own; the plugin decides what it
        # does with the fill on or off.
        cfg = {**defaults(), "map_reveal": True, "map_reveal_packs": True, "map_reveal_spawn": True, KEY: True}
        self.assertEqual(fillroll_lines(forgepact.build_cmds(cfg)), ["fillroll 1"])


class RollingFillPanelApiSetTests(unittest.TestCase):
    """`/api/set` while the game runs, and while it does not."""

    def test_the_switch_sends_fillroll_1_and_0(self):
        live = LiveSandbox(self)
        code, body, sent = live.post(KEY, True)
        self.assertEqual(code, 200, body)
        self.assertEqual(sent, [["fillroll 1"]])
        self.assertIs(live.saved()[KEY], True)
        code, body, sent = live.post(KEY, False)
        self.assertEqual(code, 200, body)
        self.assertEqual(sent, [["fillroll 0"]])
        self.assertIs(live.saved()[KEY], False)

    def test_nothing_is_sent_while_the_game_is_closed(self):
        closed = LiveSandbox(self, running=False)
        for value in (True, False):
            with self.subTest(value=value):
                code, _, sent = closed.post(KEY, value)
                self.assertEqual((code, sent), (200, []))
                self.assertIs(closed.saved()[KEY], value)


class RollingFillPanelRowTests(unittest.TestCase):
    """The Mods-tab row and how it is wired."""

    def test_the_row_sits_in_the_quality_of_life_card(self):
        self.assertIn(f'id="{KEY}"', qol_card())
        mods = panel_file("tabs/Mods.svelte")
        self.assertEqual(mods.count(f'id="{KEY}"'), 1)

    def test_the_row_comes_directly_after_extra_packs_as_you_approach(self):
        mods = panel_file("tabs/Mods.svelte")
        extra, fill, hidden = (mods.index(f'id="{i}"') for i in ("density_rolling", KEY, "mod_hidden_loot"))
        self.assertLess(extra, fill)
        self.assertLess(fill, hidden)
        # Exactly one row opens between Extra packs' switch and this one: its own.
        self.assertEqual(mods[extra:fill].count('<div class="row"'), 1)
        self.assertEqual(row_text(qol_row("density_rolling"))[0], "Extra packs as you approach")

    def test_the_row_is_a_top_level_row(self):
        # A child row carries an id of its own (`<parent>_row`) and is grouped
        # under its parent; this one is neither.
        row = qol_row(KEY)
        self.assertTrue(row.startswith('<div class="row" style="border:none">'), row[:80])
        self.assertNotIn(f'id="{KEY}_row"', panel_file("tabs/Mods.svelte"))

    def test_the_row_is_a_switch_with_its_value(self):
        row = qol_row(KEY)
        self.assertIn(f'<label class="switch"><input type="checkbox" id="{KEY}"><span class="sl"></span></label>', row)
        self.assertIn('<span class="val" id="frlval">off</span>', row)
        self.assertEqual(row_text(row)[0], "Fill the map as you approach")

    def test_the_description_is_short_and_says_what_it_does_for_the_player(self):
        _, text = row_text(qol_row(KEY))
        self.assertLessEqual(len(text), 300, text)
        self.assertTrue(text.endswith("Off by default."), text)
        # It changes Map Reveal's fill, and says so by the option's own name.
        self.assertIn("Really spawn every pack on arrival", text)
        # No game object, counter, reach or measurement.
        self.assertNotRegex(text, r"\d", text)
        self.assertNotRegex(text, r"_obj|_Parent|[a-z][A-Z]", text)
        for word in ("measured", "fillroll", "#183", "px", "pixel", "reach", "creator", "spawner",
                     "distance_to_object", "instance", "frame thread", "frameprof"):
            self.assertNotIn(word, text, word)

    def test_the_switch_posts_a_boolean_and_redraws_its_value(self):
        panel = panel_file("panel.js")
        handler = panel.split(f"document.getElementById('{KEY}').onchange", 1)[1][:500]
        self.assertIn(f"{{key:'{KEY}',value:e.target.checked}}", handler)
        self.assertIn("document.getElementById('frlval')", handler)
        self.assertIn(f"{KEY}:'{KEY}'", panel)
        self.assertIn(f"frlval:'{KEY}'", panel)

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
