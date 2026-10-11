#!/usr/bin/env python3
"""The panel half of "Always show Chaos Pillar mods" (`pillartips`, ForgePact #7).

The plugin half (which pillars count as on screen, the activated-pillar rule
and the show and hide writes) is pinned by the plugin's own tests. This file
pins what the panel owns:

- the switch is off by default, so a fresh install sends no `pillartips` line;
- with the switch on, startup sends `pillartips 1`, and nothing else for it;
- `/api/set` of the switch sends `pillartips 1|0` while the game runs, and
  nothing while it is closed, and the value is saved either way;
- the row sits in the Mods tab's Quality of Life card (`qolCard`), straight
  after Loot announcements and before Timed skill countdown;
- its label and text are exactly the player-facing ones the workorder fixed,
  ending `Off by default.` and naming no game object, counter or measurement;
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

LABEL = "Always show Chaos Pillar mods"
DESCRIPTION = ("Shows each Chaos Pillar's mods while the pillar is on screen, without hovering "
               "over it. A pillar you have activated stops showing them. Off by default.")


def pillartips_lines(cmds):
    return [c for c in cmds if c.startswith("pillartips")]


class ChaosPillarTipsPanelBaselineTests(unittest.TestCase):
    """The defaults: off, and nothing sent."""

    def test_the_switch_is_off_by_default(self):
        self.assertIs(forgepact.DEFAULTS["mod_pillar_tips"], False)

    def test_build_cmds_with_the_defaults_sends_no_pillartips_line(self):
        self.assertEqual(pillartips_lines(forgepact.build_cmds(defaults())), [])

    def test_off_sends_nothing_rather_than_pillartips_0(self):
        cfg = {**defaults(), "mod_pillar_tips": False}
        self.assertEqual(pillartips_lines(forgepact.build_cmds(cfg)), [])


class ChaosPillarTipsPanelBuildCmdsTests(unittest.TestCase):
    """Startup with the switch on."""

    def test_on_sends_pillartips_1_once(self):
        cfg = {**defaults(), "mod_pillar_tips": True}
        self.assertEqual(pillartips_lines(forgepact.build_cmds(cfg)), ["pillartips 1"])

    def test_the_switch_adds_exactly_one_line_to_startup(self):
        off = forgepact.build_cmds(defaults())
        on = forgepact.build_cmds({**defaults(), "mod_pillar_tips": True})
        self.assertEqual(len(on), len(off) + 1)


class ChaosPillarTipsPanelApiSetTests(unittest.TestCase):
    """`/api/set` while the game runs, and while it does not."""

    def test_the_switch_sends_pillartips_1_and_0_and_is_saved(self):
        live = LiveSandbox(self)
        code, body, sent = live.post("mod_pillar_tips", True)
        self.assertEqual(code, 200, body)
        self.assertEqual(sent, [["pillartips 1"]])
        self.assertIs(live.saved()["mod_pillar_tips"], True)
        code, body, sent = live.post("mod_pillar_tips", False)
        self.assertEqual(code, 200, body)
        self.assertEqual(sent, [["pillartips 0"]])
        self.assertIs(live.saved()["mod_pillar_tips"], False)

    def test_nothing_is_sent_while_the_game_is_closed(self):
        closed = LiveSandbox(self, running=False)
        for value in (True, False):
            with self.subTest(value=value):
                code, _, sent = closed.post("mod_pillar_tips", value)
                self.assertEqual((code, sent), (200, []))
                self.assertIs(closed.saved()["mod_pillar_tips"], value)


class ChaosPillarTipsPanelRowTests(unittest.TestCase):
    """The Mods-tab row and how it is wired."""

    def test_the_row_sits_in_the_quality_of_life_card_after_loot_announcements(self):
        self.assertIn('id="mod_pillar_tips"', qol_card())
        mods = panel_file("tabs/Mods.svelte")
        self.assertEqual(mods.count('id="mod_pillar_tips"'), 1)
        loot, pillar, timer = (mods.index(f'id="{i}"') for i in
                               ("mod_loot_announce", "mod_pillar_tips", "mod_skill_timer_style"))
        self.assertLess(loot, pillar)
        self.assertLess(pillar, timer)
        # Straight after Loot announcements: no other control sits between
        # either neighbour and this row.
        self.assertNotIn(' id="', mods[loot:pillar].split('id="mlaval"', 1)[1])
        self.assertEqual(mods[pillar:timer].count('<div class="row"'), 1)

    def test_the_row_is_a_switch_with_its_value(self):
        row = qol_row("mod_pillar_tips")
        self.assertIn('<label class="switch"><input type="checkbox" id="mod_pillar_tips" aria-label="Always show Chaos Pillar mods"><span class="sl"></span></label>', row)
        self.assertIn('<span class="val" id="mptval">off</span>', row)

    def test_the_label_and_text_are_the_player_facing_ones(self):
        label, text = row_text(qol_row("mod_pillar_tips"))
        self.assertEqual(label, LABEL)
        self.assertEqual(text, DESCRIPTION)
        self.assertLessEqual(len(text), 300, text)
        self.assertTrue(text.endswith("Off by default."), text)
        # No game object, counter or measurement.
        self.assertNotRegex(text, r"\d", text)
        self.assertNotRegex(text, r"_obj|_Parent|[a-z][A-Z]", text)
        for word in ("tooltip", "visible", "Tooltip", "pillartips", "#7", "frame", "measured"):
            self.assertNotIn(word, text, word)

    def test_the_switch_posts_a_boolean_and_redraws_its_value(self):
        panel = panel_file("panel.js")
        handler = panel.split("document.getElementById('mod_pillar_tips').onchange", 1)[1][:500]
        self.assertIn("{key:'mod_pillar_tips',value:e.target.checked}", handler)
        self.assertIn("document.getElementById('mptval')", handler)
        self.assertIn("mod_pillar_tips:'mod_pillar_tips'", panel)
        self.assertIn("mptval:'mod_pillar_tips'", panel)

    def test_the_switch_is_a_boolean_mod(self):
        mods = panel_file("enabled-mods.js")
        listed = mods.split("export const BOOLEAN_MODS = [", 1)[1].split("];", 1)[0]
        self.assertIn("'mod_pillar_tips'", listed)

    def test_enabled_mods_lists_it_only_while_on(self):
        base = defaults()
        cases = {"defaults": base, "on": {**base, "mod_pillar_tips": True}}
        driver = (f"const cases={json.dumps(cases)};"
                  "console.log(JSON.stringify(Object.fromEntries(Object.entries(cases)"
                  ".map(([k,c])=>[k,enabledControls(c)]))));")
        result = run_node(js_for_node(panel_file("enabled-mods.js")), driver)
        self.assertEqual(result, {"defaults": [], "on": ["mod_pillar_tips"]})


if __name__ == "__main__":
    unittest.main()
