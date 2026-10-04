#!/usr/bin/env python3
"""The panel half of "Jump through scenery" (`jumpscenery`, #16 phase 2).

The plugin half is pinned by test_jump_scenery_mod_contract.py and exercised by
test_jump_scenery_mod_behavior.py. This file pins what the panel owns:

- the switch is off by default, so a fresh install sends no `jumpscenery` line;
- with the switch on, startup sends `jumpscenery 1`, and nothing else for it;
- `/api/set` of the switch sends `jumpscenery 1|0` while the game runs, and
  nothing while it is closed;
- the row sits in the Mods tab's Quality of Life card (`qolCard`), after
  Sleep loot your filter hides and before Timed skill countdown;
- its text is short, player-facing and ends `Off by default.`, and names no
  game object, counter or measurement;
- it is a boolean mod for the "Enabled mods" list.

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


def defaults():
    return {k: v for k, v in forgepact.DEFAULTS.items() if k != "game_exe"}


def jump_lines(cmds):
    return [c for c in cmds if c.startswith("jumpscenery")]


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


def qol_card():
    """The Mods.svelte Quality of Life card, up to the next card."""
    mods = panel_file("tabs/Mods.svelte")
    start = mods.index('id="qolCard"')
    end = mods.find('<div class="card', start)
    return mods[start:end if end >= 0 else len(mods)]


class JumpSceneryPanelBaselineTests(unittest.TestCase):
    """The defaults: off, and nothing sent."""

    def test_the_switch_is_off_by_default(self):
        self.assertIs(forgepact.DEFAULTS["mod_jump_scenery"], False)

    def test_build_cmds_with_the_defaults_sends_no_jumpscenery_line(self):
        self.assertEqual(jump_lines(forgepact.build_cmds(defaults())), [])

    def test_off_sends_nothing_rather_than_jumpscenery_0(self):
        cfg = {**defaults(), "mod_jump_scenery": False}
        self.assertEqual(jump_lines(forgepact.build_cmds(cfg)), [])


class JumpSceneryPanelBuildCmdsTests(unittest.TestCase):
    """Startup with the switch on."""

    def test_on_sends_jumpscenery_1_once(self):
        cfg = {**defaults(), "mod_jump_scenery": True}
        self.assertEqual(jump_lines(forgepact.build_cmds(cfg)), ["jumpscenery 1"])

    def test_the_switch_adds_exactly_one_line_to_startup(self):
        off = forgepact.build_cmds(defaults())
        on = forgepact.build_cmds({**defaults(), "mod_jump_scenery": True})
        self.assertEqual(len(on), len(off) + 1)


class JumpSceneryPanelApiSetTests(unittest.TestCase):
    """`/api/set` while the game runs, and while it does not."""

    def test_the_switch_sends_jumpscenery_1_and_0(self):
        live = LiveSandbox(self)
        code, body, sent = live.post("mod_jump_scenery", True)
        self.assertEqual(code, 200, body)
        self.assertEqual(sent, [["jumpscenery 1"]])
        self.assertIs(live.saved()["mod_jump_scenery"], True)
        code, body, sent = live.post("mod_jump_scenery", False)
        self.assertEqual(code, 200, body)
        self.assertEqual(sent, [["jumpscenery 0"]])
        self.assertIs(live.saved()["mod_jump_scenery"], False)

    def test_nothing_is_sent_while_the_game_is_closed(self):
        closed = LiveSandbox(self, running=False)
        for value in (True, False):
            with self.subTest(value=value):
                code, _, sent = closed.post("mod_jump_scenery", value)
                self.assertEqual((code, sent), (200, []))
                self.assertIs(closed.saved()["mod_jump_scenery"], value)


class JumpSceneryPanelRowTests(unittest.TestCase):
    """The Mods-tab row and how it is wired."""

    def test_the_row_sits_in_the_quality_of_life_card(self):
        self.assertIn('id="mod_jump_scenery"', qol_card())
        mods = panel_file("tabs/Mods.svelte")
        self.assertEqual(mods.count('id="mod_jump_scenery"'), 1)
        self.assertLess(mods.index('id="mod_hidden_loot_key_row"'), mods.index('id="mod_jump_scenery"'))
        self.assertLess(mods.index('id="mod_jump_scenery"'), mods.index('id="mod_skill_timer_style"'))

    def test_the_row_is_a_switch_with_its_value(self):
        row = qol_row("mod_jump_scenery")
        self.assertIn('<label class="switch"><input type="checkbox" id="mod_jump_scenery"><span class="sl"></span></label>', row)
        self.assertIn('<span class="val" id="mjsval">off</span>', row)
        self.assertEqual(row_text(row)[0], "Jump through scenery")

    def test_the_description_is_short_and_says_what_it_does_for_the_player(self):
        _, text = row_text(qol_row("mod_jump_scenery"))
        self.assertLessEqual(len(text), 300, text)
        self.assertTrue(text.endswith("Off by default."), text)
        self.assertIn("jump", text.lower())
        # No game object, counter or measurement.
        self.assertNotRegex(text, r"\d", text)
        self.assertNotRegex(text, r"_obj|_Parent|[a-z][A-Z]", text)
        for word in ("measured", "skillsLeap", "Collision", "collision_", "instance_", "place_meeting",
                     "granted", "refused", "answered", "before-open", "excluded", "jumpscenery", "#16"):
            self.assertNotIn(word, text, word)

    def test_the_switch_posts_a_boolean_and_redraws_its_value(self):
        panel = panel_file("panel.js")
        handler = panel.split("document.getElementById('mod_jump_scenery').onchange", 1)[1][:500]
        self.assertIn("{key:'mod_jump_scenery',value:e.target.checked}", handler)
        self.assertIn("document.getElementById('mjsval')", handler)
        self.assertIn("mod_jump_scenery:'mod_jump_scenery'", panel)
        self.assertIn("mjsval:'mod_jump_scenery'", panel)

    def test_the_switch_is_a_boolean_mod(self):
        mods = panel_file("enabled-mods.js")
        listed = mods.split("export const BOOLEAN_MODS = [", 1)[1].split("];", 1)[0]
        self.assertIn("'mod_jump_scenery'", listed)

    def test_enabled_mods_lists_it_only_while_on(self):
        base = defaults()
        cases = {"defaults": base, "on": {**base, "mod_jump_scenery": True}}
        driver = (f"const cases={json.dumps(cases)};"
                  "console.log(JSON.stringify(Object.fromEntries(Object.entries(cases)"
                  ".map(([k,c])=>[k,enabledControls(c)]))));")
        result = run_node(js_for_node(panel_file("enabled-mods.js")), driver)
        self.assertEqual(result, {"defaults": [], "on": ["mod_jump_scenery"]})


if __name__ == "__main__":
    unittest.main()
