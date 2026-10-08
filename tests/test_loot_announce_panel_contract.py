#!/usr/bin/env python3
"""The panel half of "Loot announcements" (`lootann`, ForgePact #17).

The plugin half (the rarity gate, the once-per-item rule and the chat sink) is
pinned by the plugin's own tests. This file pins what the panel owns:

- the switch is off by default, so a fresh install sends no `lootann` line;
- with the switch on, startup sends `lootann 1`, and nothing else for it;
- `/api/set` of the switch sends `lootann 1|0` while the game runs, and
  nothing while it is closed;
- the row sits in the Mods tab's Quality of Life card (`qolCard`), after
  Jump through scenery and before Timed skill countdown;
- its text is short, player-facing and ends `Off by default.`, names the
  five rarities it announces, and names no game object, counter or
  measurement;
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


def lootann_lines(cmds):
    return [c for c in cmds if c.startswith("lootann")]


class LootAnnouncePanelBaselineTests(unittest.TestCase):
    """The defaults: off, and nothing sent."""

    def test_the_switch_is_off_by_default(self):
        self.assertIs(forgepact.DEFAULTS["mod_loot_announce"], False)

    def test_build_cmds_with_the_defaults_sends_no_lootann_line(self):
        self.assertEqual(lootann_lines(forgepact.build_cmds(defaults())), [])

    def test_off_sends_nothing_rather_than_lootann_0(self):
        cfg = {**defaults(), "mod_loot_announce": False}
        self.assertEqual(lootann_lines(forgepact.build_cmds(cfg)), [])


class LootAnnouncePanelBuildCmdsTests(unittest.TestCase):
    """Startup with the switch on."""

    def test_on_sends_lootann_1_once(self):
        cfg = {**defaults(), "mod_loot_announce": True}
        self.assertEqual(lootann_lines(forgepact.build_cmds(cfg)), ["lootann 1"])

    def test_the_switch_adds_exactly_one_line_to_startup(self):
        off = forgepact.build_cmds(defaults())
        on = forgepact.build_cmds({**defaults(), "mod_loot_announce": True})
        self.assertEqual(len(on), len(off) + 1)


class LootAnnouncePanelApiSetTests(unittest.TestCase):
    """`/api/set` while the game runs, and while it does not."""

    def test_the_switch_sends_lootann_1_and_0(self):
        live = LiveSandbox(self)
        code, body, sent = live.post("mod_loot_announce", True)
        self.assertEqual(code, 200, body)
        self.assertEqual(sent, [["lootann 1"]])
        self.assertIs(live.saved()["mod_loot_announce"], True)
        code, body, sent = live.post("mod_loot_announce", False)
        self.assertEqual(code, 200, body)
        self.assertEqual(sent, [["lootann 0"]])
        self.assertIs(live.saved()["mod_loot_announce"], False)

    def test_nothing_is_sent_while_the_game_is_closed(self):
        closed = LiveSandbox(self, running=False)
        for value in (True, False):
            with self.subTest(value=value):
                code, _, sent = closed.post("mod_loot_announce", value)
                self.assertEqual((code, sent), (200, []))
                self.assertIs(closed.saved()["mod_loot_announce"], value)


class LootAnnouncePanelRowTests(unittest.TestCase):
    """The Mods-tab row and how it is wired."""

    def test_the_row_sits_in_the_quality_of_life_card(self):
        self.assertIn('id="mod_loot_announce"', qol_card())
        mods = panel_file("tabs/Mods.svelte")
        self.assertEqual(mods.count('id="mod_loot_announce"'), 1)
        self.assertLess(mods.index('id="mod_jump_scenery"'), mods.index('id="mod_loot_announce"'))
        self.assertLess(mods.index('id="mod_loot_announce"'), mods.index('id="mod_skill_timer_style"'))

    def test_the_row_is_a_switch_with_its_value(self):
        row = qol_row("mod_loot_announce")
        self.assertIn('<label class="switch"><input type="checkbox" id="mod_loot_announce"><span class="sl"></span></label>', row)
        self.assertIn('<span class="val" id="mlaval">off</span>', row)
        self.assertEqual(row_text(row)[0], "Loot announcements")

    def test_the_description_is_short_and_says_what_it_does_for_the_player(self):
        _, text = row_text(qol_row("mod_loot_announce"))
        self.assertLessEqual(len(text), 300, text)
        self.assertTrue(text.endswith("Off by default."), text)
        self.assertIn("chat", text)
        # Satanic and Mythic joined on the owner's word, 2026-10-08.
        for rarity in ("Heroic", "Angelic", "Unholy", "Satanic", "Mythic"):
            self.assertIn(rarity, text)
        # Items the player drops are not announced: the one caveat they act on.
        self.assertIn("drop yourself", text)
        # No game object, counter or measurement.
        self.assertNotRegex(text, r"\d", text)
        self.assertNotRegex(text, r"_obj|_Parent|[a-z][A-Z]", text)
        for word in ("measured", "itemInfoStruct", "ChatAddServerMessage", "LootGround", "Chat_obj",
                     "rarity", "lootann", "#17", "packet"):
            self.assertNotIn(word, text, word)

    def test_the_switch_posts_a_boolean_and_redraws_its_value(self):
        panel = panel_file("panel.js")
        handler = panel.split("document.getElementById('mod_loot_announce').onchange", 1)[1][:500]
        self.assertIn("{key:'mod_loot_announce',value:e.target.checked}", handler)
        self.assertIn("document.getElementById('mlaval')", handler)
        self.assertIn("mod_loot_announce:'mod_loot_announce'", panel)
        self.assertIn("mlaval:'mod_loot_announce'", panel)

    def test_the_switch_is_a_boolean_mod(self):
        mods = panel_file("enabled-mods.js")
        listed = mods.split("export const BOOLEAN_MODS = [", 1)[1].split("];", 1)[0]
        self.assertIn("'mod_loot_announce'", listed)

    def test_enabled_mods_lists_it_only_while_on(self):
        base = defaults()
        cases = {"defaults": base, "on": {**base, "mod_loot_announce": True}}
        driver = (f"const cases={json.dumps(cases)};"
                  "console.log(JSON.stringify(Object.fromEntries(Object.entries(cases)"
                  ".map(([k,c])=>[k,enabledControls(c)]))));")
        result = run_node(js_for_node(panel_file("enabled-mods.js")), driver)
        self.assertEqual(result, {"defaults": [], "on": ["mod_loot_announce"]})


if __name__ == "__main__":
    unittest.main()
