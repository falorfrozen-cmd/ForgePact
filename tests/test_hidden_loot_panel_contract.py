#!/usr/bin/env python3
"""The panel half of "Sleep loot your filter hides" (`hiddenloot`, #95 part 2b).

The plugin half is pinned by test_hidden_loot_mod_contract.py and exercised by
test_hidden_loot_behavior.py. This file pins what the panel owns:

- the switch is off by default and the show key defaults to Left Alt (VK 164,
  the owner's Q-key answer), so a fresh install sends no `hiddenloot` line;
- with the switch on, startup sends the key first (`hiddenloot key <vk>`, 0
  for none) and then `hiddenloot 1`;
- `/api/set` of the switch sends `hiddenloot 1|0`, and turning it on restates
  the key first, as turning Map Reveal or Auto-prospect on restates their
  child: the plugin only hears the key while the game runs, so a key picked
  with the game closed would otherwise never reach it;
- `/api/set` of the key sends `hiddenloot key <vk>` whether the switch is on
  or off (the plugin stores it and costs nothing while off), and answers 400
  for a code the list does not offer;
- the Python key list and the panel's (`panel/src/hidden-loot-keys.js`) are
  the same list, code for code and name for name, and every code is 0 or
  3-254 (1 and 2 are the game's own mouse buttons);
- the row's text is short, player-facing and ends `Off by default.`, and the
  key row is a child row of the switch's row, wired as Auto-prospect's
  material move is.

Runs against `PanelSandbox` (isolated settings, `send_cmds` captured, never
the game's IPC) and the panel's source read as text.
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
from test_panel_performance import run_node  # noqa: E402
from test_satanic_panel import PanelSandbox  # noqa: E402

LEFT_ALT = 164
# Refused on purpose (context "How ForgePact reads keys today"): the left and
# right mouse buttons are the game's own; generic Alt, Right Alt and F10 can
# put a Win32 window into its menu mode.
NEVER_OFFERED = (1, 2, 18, 121, 165)


def defaults():
    return {k: v for k, v in forgepact.DEFAULTS.items() if k != "game_exe"}


def hidden_lines(cmds):
    return [c for c in cmds if c.startswith("hiddenloot")]


class LiveSandbox:
    """PanelSandbox with the game reported running and each POST's sends read back."""

    def __init__(self, test, running=True):
        self.sandbox = PanelSandbox()
        self.sandbox.__enter__()
        test.addCleanup(self.sandbox.__exit__)
        self.sandbox.mocks[1].return_value = running   # game_running
        self.sent = self.sandbox.mocks[3]              # send_cmds

    def post(self, key, value):
        self.sent.reset_mock()
        code, body = self.sandbox.request(dict(section=None, key=key, value=value))
        return code, body, [c.args[0] for c in self.sent.call_args_list]

    def saved(self):
        return json.loads(self.sandbox.config.read_text(encoding="utf-8"))


def qol_row(control_id):
    """The Mods.svelte row (from its opening `<div class="row"`) holding `control_id`."""
    mods = panel_file("tabs/Mods.svelte")
    at = mods.index(f'id="{control_id}"')
    start = mods.rindex('<div class="row"', 0, at)
    return mods[start:mods.index("</div>", at)]


def row_text(row):
    """(title, description) of a Mods row, the description with its tags taken out."""
    label = re.search(r'<span class="lbl"[^>]*>([^<]*)<br><span [^>]*>(.*?)</span></span>', row, re.S)
    if label is None:
        raise AssertionError(row)
    return label.group(1), re.sub(r"<[^>]+>", "", label.group(2))


class HiddenLootPanelBaselineTests(unittest.TestCase):
    """The defaults: off, Left Alt, and nothing sent."""

    def test_defaults_are_off_and_left_alt(self):
        self.assertIs(forgepact.DEFAULTS["mod_hidden_loot"], False)
        self.assertEqual(forgepact.DEFAULTS["mod_hidden_loot_key"], LEFT_ALT)
        self.assertIs(type(forgepact.DEFAULTS["mod_hidden_loot_key"]), int)

    def test_build_cmds_with_the_defaults_sends_no_hiddenloot_line(self):
        self.assertEqual(hidden_lines(forgepact.build_cmds(defaults())), [])

    def test_a_key_alone_sends_nothing_while_the_switch_is_off(self):
        for key in (0, None, 17, LEFT_ALT):
            with self.subTest(key=key):
                cfg = {**defaults(), "mod_hidden_loot_key": key}
                self.assertEqual(hidden_lines(forgepact.build_cmds(cfg)), [])


class HiddenLootPanelBuildCmdsTests(unittest.TestCase):
    """Startup with the switch on: the key, then the switch."""

    def test_on_sends_the_default_key_then_the_switch(self):
        cfg = {**defaults(), "mod_hidden_loot": True}
        self.assertEqual(hidden_lines(forgepact.build_cmds(cfg)), ["hiddenloot key 164", "hiddenloot 1"])

    def test_the_key_line_comes_straight_before_the_switch(self):
        cmds = forgepact.build_cmds({**defaults(), "mod_hidden_loot": True, "mod_hidden_loot_key": 17})
        at = cmds.index("hiddenloot 1")
        self.assertEqual(cmds[at - 1], "hiddenloot key 17")

    def test_none_and_zero_send_key_zero(self):
        for key in (None, 0):
            with self.subTest(key=key):
                cfg = {**defaults(), "mod_hidden_loot": True, "mod_hidden_loot_key": key}
                self.assertEqual(hidden_lines(forgepact.build_cmds(cfg)), ["hiddenloot key 0", "hiddenloot 1"])

    def test_a_hand_edited_key_outside_the_list_falls_back_to_left_alt(self):
        # Never a refused code on the wire: the plugin would refuse 1 and 2,
        # and a code the panel cannot show would be a key nobody can see.
        for key in (1, 2, 121, 999, "Alt", True):
            with self.subTest(key=key):
                cfg = {**defaults(), "mod_hidden_loot": True, "mod_hidden_loot_key": key}
                self.assertEqual(hidden_lines(forgepact.build_cmds(cfg)), ["hiddenloot key 164", "hiddenloot 1"])


class HiddenLootPanelApiSetTests(unittest.TestCase):
    """`/api/set` while the game runs, and while it does not."""

    def test_the_switch_sends_hiddenloot_1_and_0(self):
        live = LiveSandbox(self)
        code, body, sent = live.post("mod_hidden_loot", True)
        self.assertEqual(code, 200, body)
        self.assertEqual(sent, [["hiddenloot key 164", "hiddenloot 1"]])
        self.assertIs(live.saved()["mod_hidden_loot"], True)
        code, body, sent = live.post("mod_hidden_loot", False)
        self.assertEqual(code, 200, body)
        self.assertEqual(sent, [["hiddenloot 0"]])
        self.assertIs(live.saved()["mod_hidden_loot"], False)

    def test_turning_the_switch_on_restates_a_key_picked_while_the_game_was_closed(self):
        closed = LiveSandbox(self, running=False)
        code, _, sent = closed.post("mod_hidden_loot_key", 17)
        self.assertEqual((code, sent), (200, []))
        closed.sandbox.mocks[1].return_value = True
        _, _, sent = closed.post("mod_hidden_loot", True)
        self.assertEqual(sent, [["hiddenloot key 17", "hiddenloot 1"]])

    def test_the_key_sends_hiddenloot_key_whether_the_switch_is_on_or_off(self):
        live = LiveSandbox(self)
        for switch in (False, True):
            live.post("mod_hidden_loot", switch)
            for value, wire in ((17, "hiddenloot key 17"), (0, "hiddenloot key 0"), (LEFT_ALT, "hiddenloot key 164")):
                with self.subTest(switch=switch, value=value):
                    code, body, sent = live.post("mod_hidden_loot_key", value)
                    self.assertEqual(code, 200, body)
                    self.assertEqual(sent, [[wire]])
                    self.assertEqual(live.saved()["mod_hidden_loot_key"], value)

    def test_none_is_saved_and_sent_as_zero(self):
        live = LiveSandbox(self)
        code, _, sent = live.post("mod_hidden_loot_key", None)
        self.assertEqual((code, sent), (200, [["hiddenloot key 0"]]))
        self.assertEqual(live.saved()["mod_hidden_loot_key"], 0)

    def test_a_code_outside_the_list_is_refused_and_nothing_is_sent(self):
        live = LiveSandbox(self)
        live.post("mod_hidden_loot_key", 17)
        for bad in (*NEVER_OFFERED, 3, 255, 999, -1, "17", "Alt", 17.0, True, False, [17]):
            with self.subTest(value=bad):
                code, body, sent = live.post("mod_hidden_loot_key", bad)
                self.assertEqual(code, 400)
                self.assertEqual(body, {"err": "invalid hidden loot key"})
                self.assertEqual(sent, [])
                self.assertEqual(live.saved()["mod_hidden_loot_key"], 17)

    def test_nothing_is_sent_while_the_game_is_closed(self):
        closed = LiveSandbox(self, running=False)
        for key, value in (("mod_hidden_loot", True), ("mod_hidden_loot_key", 65), ("mod_hidden_loot", False)):
            with self.subTest(key=key):
                code, _, sent = closed.post(key, value)
                self.assertEqual((code, sent), (200, []))
                self.assertEqual(closed.saved()[key], value)


class HiddenLootKeyListTests(unittest.TestCase):
    """One key list, in Python and in the panel."""

    EXPECTED = (
        [(0, "None"), (164, "Left Alt"), (17, "Ctrl"), (16, "Shift"), (9, "Tab"), (20, "Caps Lock"),
         (32, "Space"), (192, "Backquote"), (4, "Middle mouse"), (5, "Mouse 4"), (6, "Mouse 5")]
        + [(111 + n, f"F{n}") for n in range(1, 10)] + [(122, "F11"), (123, "F12")]
        + [(ord(c), c) for c in "ABCDEFGHIJKLMNOPQRSTUVWXYZ"]
        + [(ord(c), c) for c in "0123456789"]
    )

    def panel_keys(self):
        driver = "console.log(JSON.stringify({keys:HIDDEN_LOOT_KEYS,fallback:HIDDEN_LOOT_KEY_DEFAULT}));"
        return run_node(js_for_node(panel_file("hidden-loot-keys.js")), driver)

    def test_the_python_list_is_the_agreed_set(self):
        self.assertEqual([tuple(k) for k in forgepact.HIDDEN_LOOT_KEYS], self.EXPECTED)

    def test_the_panel_list_equals_the_python_list_code_for_code_and_name_for_name(self):
        panel = self.panel_keys()
        self.assertEqual([tuple(k) for k in panel["keys"]], [tuple(k) for k in forgepact.HIDDEN_LOOT_KEYS])
        self.assertEqual(panel["fallback"], forgepact.DEFAULTS["mod_hidden_loot_key"])

    def test_every_code_is_zero_or_a_key_the_plugin_accepts(self):
        codes = [code for code, _ in forgepact.HIDDEN_LOOT_KEYS]
        self.assertEqual(len(codes), len(set(codes)))
        self.assertEqual(len({name for _, name in forgepact.HIDDEN_LOOT_KEYS}), len(codes))
        for code in codes:
            self.assertTrue(code == 0 or 3 <= code <= 254, code)
        for code in NEVER_OFFERED:
            self.assertNotIn(code, codes)
        self.assertIn(LEFT_ALT, codes)


class HiddenLootPanelRowTests(unittest.TestCase):
    """The Mods-tab rows and how they are wired."""

    def test_the_switch_row_follows_far_scenery_sleep(self):
        mods = panel_file("tabs/Mods.svelte")
        self.assertLess(mods.index('id="mod_far_sleep"'), mods.index('id="mod_hidden_loot"'))
        self.assertLess(mods.index('id="mod_hidden_loot_key_row"'), mods.index('id="mod_skill_timer_style"'))
        row = qol_row("mod_hidden_loot")
        self.assertIn('<label class="switch"><input type="checkbox" id="mod_hidden_loot"><span class="sl"></span></label>', row)
        self.assertIn('<span class="val" id="mhlval">off</span>', row)
        self.assertEqual(row_text(row)[0], "Sleep loot your filter hides")

    def test_the_description_is_short_and_says_what_it_does_for_the_player(self):
        _, text = row_text(qol_row("mod_hidden_loot"))
        self.assertLessEqual(len(text), 300, text)
        self.assertTrue(text.endswith("Off by default."), text)
        self.assertIn("Left Alt", text)
        self.assertNotRegex(text, r"\d", text)
        for word in ("µs", "measured", "LootGroundInit", "lootFilterVisible", "instance_deactivate", "#95"):
            self.assertNotIn(word, text, word)

    def test_the_key_row_is_a_child_row_holding_the_select(self):
        mods = panel_file("tabs/Mods.svelte")
        row = qol_row("mod_hidden_loot_key")
        self.assertTrue(row.startswith('<div class="row" id="mod_hidden_loot_key_row">'), row)
        self.assertEqual(row_text(row)[0], "Show hidden loot while held")
        self.assertIn('<select class="style-select" id="mod_hidden_loot_key">', row)
        # Its options come from the one key module.
        self.assertIn("import { HIDDEN_LOOT_KEYS, HIDDEN_LOOT_KEY_DEFAULT } from '../hidden-loot-keys.js';", mods)
        self.assertIn("selected={code === HIDDEN_LOOT_KEY_DEFAULT}", row)
        self.assertIn("{#each HIDDEN_LOOT_KEYS as [code, name] (code)}", row)
        self.assertLess(mods.index('id="mod_hidden_loot"'), mods.index('id="mod_hidden_loot_key_row"'))

    def test_the_key_row_is_grouped_with_its_parent_and_indented(self):
        panel = panel_file("panel.js")
        self.assertIn("const hlParent=document.getElementById('mod_hidden_loot').closest('.row'),"
                      "hlChild=document.getElementById('mod_hidden_loot_key_row');", panel)
        self.assertIn("hlGroup.append(hlParent,hlChild);", panel)
        self.assertIn(".feature-with-child > #mod_hidden_loot_key_row", panel_file("ember/relief.css"))
        finish = (ROOT / "panel" / "tests" / "finish.e2e.mjs").read_text(encoding="utf-8")
        self.assertIn("'mod_hidden_loot_key_row'", finish.split("const indent =", 1)[1][:300])

    def test_the_key_is_disabled_while_the_switch_is_off(self):
        sync = panel_file("mods-sync.js")
        self.assertIn("export function syncHiddenLootKey(parentOn){", sync)
        body = sync.split("export function syncHiddenLootKey(parentOn){", 1)[1].split("\n}", 1)[0]
        self.assertIn("box.disabled=!parentOn;", body)
        panel = panel_file("panel.js")
        self.assertGreaterEqual(panel.count("syncHiddenLootKey("), 3)

    def test_the_select_posts_an_integer(self):
        panel = panel_file("panel.js")
        handler = panel.split("document.getElementById('mod_hidden_loot_key').onchange", 1)[1][:400]
        self.assertIn("{key:'mod_hidden_loot_key',value:Number(e.target.value)}", handler)
        switch = panel.split("document.getElementById('mod_hidden_loot').onchange", 1)[1][:400]
        self.assertIn("{key:'mod_hidden_loot',value:e.target.checked}", switch)

    def test_the_switch_is_a_boolean_mod_and_the_key_is_not(self):
        mods = panel_file("enabled-mods.js")
        listed = mods.split("export const BOOLEAN_MODS = [", 1)[1].split("];", 1)[0]
        self.assertIn("'mod_hidden_loot'", listed)
        self.assertNotIn("'mod_hidden_loot_key'", listed)


if __name__ == "__main__":
    unittest.main()
