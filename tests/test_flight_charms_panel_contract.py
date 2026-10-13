#!/usr/bin/env python3
"""The panel half of the three flight charms (`flightcharm`, #158 phase 2).

The plugin half is pinned by test_flight_charms_contract.py and exercised by
test_flight_charms_behavior.py. This file pins what the panel owns, for each
of Pocket Elf (`mod_flight_elf`, `elf`), Gabriel Wings (`mod_flight_wings`,
`wings`) and Witch's Broomstick (`mod_flight_broom`, `broom`):

- the switch is off by default, so a fresh install sends no `flightcharm`
  line;
- with a switch on, startup sends `flightcharm <word> 1` for that charm, and
  nothing for the others;
- `/api/set` of a switch sends `flightcharm <word> 1|0` while the game runs,
  and nothing while it is closed;
- the three rows sit in the Mods tab's Items card (`itemsCard`), after
  Beacon, each its own top-level row;
- each description is short, player-facing, ends `Off by default.` and names
  the item `a ForgePact charm` (the Items card's classification rule);
- each is a boolean mod for the "Enabled mods" list, and a native boolean of
  the derived behaviour oracle, entered on Mods > Items.

Runs against `PanelSandbox` (isolated settings, `send_cmds` captured, never
the game's IPC) and the panel's source read as text. Modelled on
test_jump_scenery_panel_contract.py.
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

# (config key, command word, value span id, row title), in the order the rows
# render.
CHARMS = [
    ("mod_flight_elf", "elf", "mfeval", "Pocket Elf drops"),
    ("mod_flight_wings", "wings", "mfwval", "Gabriel Wings drops"),
    ("mod_flight_broom", "broom", "mfbval", "Witch's Broomstick drops"),
]
KEYS = [key for key, _, _, _ in CHARMS]
CHARM_TEXT = "a ForgePact charm"


def defaults():
    return {k: v for k, v in forgepact.DEFAULTS.items() if k != "game_exe"}


def flight_lines(cmds):
    return [c for c in cmds if c.startswith("flightcharm")]


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


def items_card():
    """The Mods.svelte Items card, up to the next card."""
    mods = panel_file("tabs/Mods.svelte")
    start = mods.index('id="itemsCard"')
    end = mods.find('<div class="card', start)
    return mods[start:end if end >= 0 else len(mods)]


def items_row(control_id):
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


class FlightCharmsPanelBaselineTests(unittest.TestCase):
    """The defaults: off, and nothing sent."""

    def test_every_switch_is_off_by_default(self):
        for key in KEYS:
            with self.subTest(key=key):
                self.assertIs(forgepact.DEFAULTS[key], False)

    def test_build_cmds_with_the_defaults_sends_no_flightcharm_line(self):
        self.assertEqual(flight_lines(forgepact.build_cmds(defaults())), [])

    def test_a_config_without_the_keys_sends_nothing(self):
        cfg = {k: v for k, v in defaults().items() if k not in KEYS}
        self.assertEqual(flight_lines(forgepact.build_cmds(cfg)), [])

    def test_off_sends_nothing_rather_than_flightcharm_0(self):
        cfg = {**defaults(), **{key: False for key in KEYS}}
        self.assertEqual(flight_lines(forgepact.build_cmds(cfg)), [])


class FlightCharmsPanelBuildCmdsTests(unittest.TestCase):
    """Startup with a switch on."""

    def test_each_switch_on_sends_its_own_line_once(self):
        for key, word, _, _ in CHARMS:
            with self.subTest(key=key):
                cfg = {**defaults(), key: True}
                self.assertEqual(flight_lines(forgepact.build_cmds(cfg)), [f"flightcharm {word} 1"])

    def test_all_three_on_send_three_lines_in_order(self):
        cfg = {**defaults(), **{key: True for key in KEYS}}
        self.assertEqual(flight_lines(forgepact.build_cmds(cfg)),
                         ["flightcharm elf 1", "flightcharm wings 1", "flightcharm broom 1"])

    def test_each_switch_adds_exactly_one_line_to_startup(self):
        off = forgepact.build_cmds(defaults())
        for key in KEYS:
            with self.subTest(key=key):
                on = forgepact.build_cmds({**defaults(), key: True})
                self.assertEqual(len(on), len(off) + 1)


class FlightCharmsPanelApiSetTests(unittest.TestCase):
    """`/api/set` while the game runs, and while it does not."""

    def test_each_switch_sends_flightcharm_word_1_and_0(self):
        live = LiveSandbox(self)
        for key, word, _, _ in CHARMS:
            with self.subTest(key=key):
                code, body, sent = live.post(key, True)
                self.assertEqual(code, 200, body)
                self.assertEqual(sent, [[f"flightcharm {word} 1"]])
                self.assertIs(live.saved()[key], True)
                code, body, sent = live.post(key, False)
                self.assertEqual(code, 200, body)
                self.assertEqual(sent, [[f"flightcharm {word} 0"]])
                self.assertIs(live.saved()[key], False)

    def test_nothing_is_sent_while_the_game_is_closed(self):
        closed = LiveSandbox(self, running=False)
        for key in KEYS:
            for value in (True, False):
                with self.subTest(key=key, value=value):
                    code, _, sent = closed.post(key, value)
                    self.assertEqual((code, sent), (200, []))
                    self.assertIs(closed.saved()[key], value)


class FlightCharmsPanelRowTests(unittest.TestCase):
    """The Mods-tab rows and how they are wired."""

    def test_the_rows_sit_in_the_items_card_after_beacon_in_order(self):
        card = items_card()
        mods = panel_file("tabs/Mods.svelte")
        positions = []
        for key in KEYS:
            with self.subTest(key=key):
                self.assertIn(f'id="{key}"', card)
                self.assertEqual(mods.count(f'id="{key}"'), 1)
            positions.append(card.index(f'id="{key}"'))
        self.assertLess(card.index('id="beacon"'), positions[0])
        self.assertEqual(positions, sorted(positions))

    def test_each_row_is_its_own_top_level_switch_row(self):
        for key, _, val_id, title in CHARMS:
            with self.subTest(key=key):
                row = items_row(key)
                # Its own row: no other control shares it.
                self.assertEqual(len(re.findall(r'<input\b', row)), 1, row)
                self.assertTrue(row.startswith('<div class="row" style="border:none">'), row)
                self.assertIn(f'<label class="switch"><input type="checkbox" id="{key}"', row)
                self.assertIn(f'<span class="val" id="{val_id}">off</span>', row)
                self.assertEqual(row_text(row)[0], title)

    def test_each_description_is_short_player_facing_and_names_a_forgepact_charm(self):
        for key, word, _, _ in CHARMS:
            with self.subTest(key=key):
                _, text = row_text(items_row(key))
                self.assertLessEqual(len(text), 300, text)
                self.assertTrue(text.endswith("Off by default."), text)
                self.assertEqual(text.count(CHARM_TEXT), 1, text)
                # The Items rule's other phrase belongs to forged Mechanic rows.
                self.assertNotIn("forged with Mechanic:", text)
                # No game object, counter, command or measurement ("ForgePact"
                # is the one camel-cased word it may say).
                self.assertNotRegex(text, r"\d", text)
                self.assertNotRegex(text.replace("ForgePact", ""), r"_obj|_Parent|[a-z][A-Z]", text)
                for jargon in ("flightcharm", "lootListUnique", "DropUniqueItems", "stand-in", "#158",
                               "measured", "hook", "seed"):
                    self.assertNotIn(jargon, text, jargon)

    def test_each_switch_posts_a_boolean_and_redraws_its_value(self):
        panel = panel_file("panel.js")
        for key, _, val_id, _ in CHARMS:
            with self.subTest(key=key):
                handler = panel.split(f"document.getElementById('{key}').onchange", 1)[1][:500]
                self.assertIn(f"{{key:'{key}',value:e.target.checked}}", handler)
                self.assertIn(f"document.getElementById('{val_id}')", handler)
                self.assertIn(f"{key}:'{key}'", panel)
                self.assertIn(f"{val_id}:'{key}'", panel)

    def test_each_switch_is_a_boolean_mod(self):
        mods = panel_file("enabled-mods.js")
        listed = mods.split("export const BOOLEAN_MODS = [", 1)[1].split("];", 1)[0]
        for key in KEYS:
            self.assertIn(f"'{key}'", listed)

    def test_enabled_mods_lists_each_only_while_on(self):
        base = defaults()
        cases = {"defaults": base, **{key: {**base, key: True} for key in KEYS}}
        driver = (f"const cases={json.dumps(cases)};"
                  "console.log(JSON.stringify(Object.fromEntries(Object.entries(cases)"
                  ".map(([k,c])=>[k,enabledControls(c)]))));")
        result = run_node(js_for_node(panel_file("enabled-mods.js")), driver)
        self.assertEqual(result, {"defaults": [], **{key: [key] for key in KEYS}})


class FlightCharmsOracleTests(unittest.TestCase):
    """The three NATIVE_BOOLEANS entries of the derived behaviour oracle."""

    def test_the_native_booleans_carry_the_three_switches_on_mods_items(self):
        source = (ROOT / "panel" / "tests" / "oracle-derive.mjs").read_text(encoding="utf-8")
        block = source.split("export const NATIVE_BOOLEANS = [", 1)[1].split("];", 1)[0]
        for key, word, _, _ in CHARMS:
            with self.subTest(key=key):
                self.assertIn(
                    f"{{ key: '{key}', tab: 'tab:mods', sub: 'subtab:items', verb: 'flightcharm {word}' }}",
                    block)
        # Before Jump through scenery, so the loop still ends on Mods > Quality
        # of Life, where the show key's select follows without navigating.
        self.assertLess(block.index("'mod_flight_broom'"), block.index("'mod_jump_scenery'"))
        last = re.findall(r"\{ key: '[^']+', tab: '([^']+)'(?:, sub: '([^']+)')?", block)[-1]
        self.assertEqual(last, ("tab:mods", "subtab:qol"))


if __name__ == "__main__":
    unittest.main()
