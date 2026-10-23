#!/usr/bin/env python3
"""The panel half of Mark 1 drops (`mark1set drops`, #166).

The plugin half (the Heroic roll's stand-ins, the coin, the rewrite to a
piece and the art) is pinned by the plugin's own contract and behaviour
tests. This file pins what the panel owns, for the one switch
`mod_mark1_drops`:

- the switch is off by default, so a fresh install sends no `mark1set drops`
  line;
- with it on, startup sends `mark1set drops 1`, once, and the Mark 1 set's
  own switch still sends only its own line;
- `/api/set` sends `mark1set drops 1|0` while the game runs, and nothing while
  it is closed;
- the row sits in the Mods tab's Items card (`itemsCard`), directly after the
  Mark 1 set's row, as its own top-level row titled "Mark 1 drops";
- its description is short, player-facing, says the four pieces can drop from
  the monsters you kill, among the game's own items, and names a vault only as
  the roll the pieces join; it says neither "anywhere" (Mod live 8 measured
  kills in Hell only) nor "from vaults" (a vault's own use of the roll was not
  observed); it ends `Off by default.` and names the set `a ForgePact item
  set` exactly once (the Items card's classification rule);
- it is a boolean mod for the "Enabled mods" list, and a native boolean of the
  derived behaviour oracle, entered on Mods > Items after the Mark 1 set's.

Runs against `PanelSandbox` (isolated settings, `send_cmds` captured, never
the game's IPC) and the panel's source read as text. Modelled on
test_mark1_set_panel_contract.py.
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

KEY = "mod_mark1_drops"
VERB = "mark1set drops"
VAL_ID = "mk1dval"
TITLE = "Mark 1 drops"
SET_KEY = "mod_mark1_set"
SET_VERB = "mark1set bonus"
SET_TEXT = "a ForgePact item set"
CHARM_TEXT = "a ForgePact charm"


def defaults():
    return {k: v for k, v in forgepact.DEFAULTS.items() if k != "game_exe"}


def mark1_lines(cmds):
    return [c for c in cmds if c.startswith("mark1set")]


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


class Mark1DropsPanelBaselineTests(unittest.TestCase):
    """The defaults: off, and nothing sent."""

    def test_the_switch_is_off_by_default(self):
        self.assertIs(forgepact.DEFAULTS[KEY], False)

    def test_build_cmds_with_the_defaults_sends_no_drops_line(self):
        self.assertEqual(mark1_lines(forgepact.build_cmds(defaults())), [])

    def test_a_config_without_the_key_sends_nothing(self):
        cfg = {k: v for k, v in defaults().items() if k != KEY}
        self.assertEqual(mark1_lines(forgepact.build_cmds(cfg)), [])

    def test_off_sends_nothing_rather_than_drops_0(self):
        self.assertEqual(mark1_lines(forgepact.build_cmds({**defaults(), KEY: False})), [])

    def test_the_set_switch_alone_still_sends_only_its_own_line(self):
        self.assertEqual(mark1_lines(forgepact.build_cmds({**defaults(), SET_KEY: True})), [f"{SET_VERB} 1"])


class Mark1DropsPanelBuildCmdsTests(unittest.TestCase):
    """Startup with the switch on."""

    def test_on_sends_mark1set_drops_1_once(self):
        self.assertEqual(mark1_lines(forgepact.build_cmds({**defaults(), KEY: True})), [f"{VERB} 1"])

    def test_on_adds_exactly_one_line_to_startup(self):
        off = forgepact.build_cmds(defaults())
        on = forgepact.build_cmds({**defaults(), KEY: True})
        self.assertEqual(len(on), len(off) + 1)

    def test_both_switches_send_one_line_each_the_set_first(self):
        cmds = forgepact.build_cmds({**defaults(), SET_KEY: True, KEY: True})
        self.assertEqual(mark1_lines(cmds), [f"{SET_VERB} 1", f"{VERB} 1"])


class Mark1DropsPanelApiSetTests(unittest.TestCase):
    """`/api/set` while the game runs, and while it does not."""

    def test_the_switch_sends_mark1set_drops_1_and_0(self):
        live = LiveSandbox(self)
        code, body, sent = live.post(KEY, True)
        self.assertEqual(code, 200, body)
        self.assertEqual(sent, [[f"{VERB} 1"]])
        self.assertIs(live.saved()[KEY], True)
        code, body, sent = live.post(KEY, False)
        self.assertEqual(code, 200, body)
        self.assertEqual(sent, [[f"{VERB} 0"]])
        self.assertIs(live.saved()[KEY], False)

    def test_a_truthy_value_is_stored_as_a_boolean(self):
        live = LiveSandbox(self)
        code, body, sent = live.post(KEY, 1)
        self.assertEqual(code, 200, body)
        self.assertIs(live.saved()[KEY], True)
        self.assertEqual(sent, [[f"{VERB} 1"]])

    def test_nothing_is_sent_while_the_game_is_closed(self):
        closed = LiveSandbox(self, running=False)
        for value in (True, False):
            with self.subTest(value=value):
                code, _, sent = closed.post(KEY, value)
                self.assertEqual((code, sent), (200, []))
                self.assertIs(closed.saved()[KEY], value)


class Mark1DropsPanelRowTests(unittest.TestCase):
    """The Mods-tab row and how it is wired."""

    def test_the_row_sits_in_the_items_card_directly_after_the_mark1_set(self):
        card = items_card()
        mods = panel_file("tabs/Mods.svelte")
        self.assertIn(f'id="{KEY}"', card)
        self.assertEqual(mods.count(f'id="{KEY}"'), 1)
        self.assertLess(card.index(f'id="{SET_KEY}"'), card.index(f'id="{KEY}"'))
        # Directly after: no other control between the two rows.
        # From the set's switch to the start of this row: the set row's tail.
        set_at = card.index(f'id="{SET_KEY}"')
        row_at = card.rindex('<div class="row"', 0, card.index(f'id="{KEY}"'))
        self.assertLess(set_at, row_at)
        between = card[set_at + len(f'id="{SET_KEY}"'):row_at]
        self.assertEqual(len(re.findall(r'<input\b|<select\b', between)), 0, between)
        self.assertNotIn('<div class="row"', between)

    def test_the_row_is_its_own_top_level_switch_row(self):
        row = items_row(KEY)
        self.assertEqual(len(re.findall(r'<input\b', row)), 1, row)
        self.assertTrue(row.startswith('<div class="row" style="border:none">'), row)
        self.assertIn(f'<label class="switch"><input type="checkbox" id="{KEY}" aria-label="{TITLE}">', row)
        self.assertIn(f'<span class="val" id="{VAL_ID}">off</span>', row)
        self.assertEqual(row_text(row)[0], TITLE)

    def test_the_description_says_the_pieces_drop_from_kills_short_and_player_facing(self):
        _, text = row_text(items_row(KEY))
        self.assertLessEqual(len(text), 300, text)
        self.assertTrue(text.endswith("Off by default."), text)
        self.assertEqual(text.count(SET_TEXT), 1, text)
        # The Items rule's other two phrases belong to other rows.
        self.assertNotIn(CHARM_TEXT, text)
        self.assertNotIn("forged with Mechanic:", text)
        # What it does for the player: the four pieces can drop from the
        # monsters you kill, among the game's own items (the kill route, owner
        # 2026-10-20), and join a vault's roll. It says what was measured and
        # no more: Mod live 8 measured kills dropping the pieces in Hell only,
        # so "anywhere" would overclaim; and a vault's own use of the roll was
        # not observed (Mod live 4 built pieces only from by-name calls of the
        # roll), so the vault is named as the roll the pieces join, never as
        # a place they drop "from".
        for said in ("helmet", "body armor", "boots", "gloves", "drop", "monsters you kill",
                     "among the game's own items", "vault"):
            self.assertIn(said, text, said)
        for overclaim in ("anywhere", "from vaults"):
            self.assertNotIn(overclaim, text, overclaim)
        # No game object, counter, command or measurement: the only digit is
        # the set's own name, and "ForgePact" the one camel-cased word.
        self.assertNotRegex(text.replace("Mark 1", ""), r"\d", text)
        self.assertNotRegex(text.replace("ForgePact", ""), r"_obj|_Parent|[a-z][A-Z]", text)
        for jargon in ("mark1set", "Heroic", "#166", "measured", "hook", "stand-in", "stat", "lever",
                       "share", "detour", "probe", "%"):
            self.assertNotIn(jargon, text, jargon)

    def test_the_switch_posts_a_boolean_and_redraws_its_value(self):
        panel = panel_file("panel.js")
        # Bound in the shared loop the Mark 1 set's switch uses, so it adds no
        # `/api/set` call site of its own.
        entry = f"['{KEY}','{VAL_ID}','{KEY}','{TITLE}']"
        self.assertEqual(panel.count(entry), 1)
        after = panel.split(entry, 1)[1]
        handler = after[after.index("]){") + 3:][:500]
        self.assertIn("{key:key,value:e.target.checked}", handler)
        self.assertIn("document.getElementById(val)", handler)
        self.assertIn(f"['{KEY}','{VAL_ID}','{KEY}']", panel)
        self.assertNotIn(f"document.getElementById('{KEY}').onchange", panel)
        self.assertIn(f"{KEY}:'{KEY}'", panel)
        self.assertIn(f"{VAL_ID}:'{KEY}'", panel)

    def test_the_switch_is_a_boolean_mod_after_the_mark1_set(self):
        mods = panel_file("enabled-mods.js")
        listed = mods.split("export const BOOLEAN_MODS = [", 1)[1].split("];", 1)[0]
        self.assertIn(f"'{KEY}'", listed)
        self.assertLess(listed.index(f"'{SET_KEY}'"), listed.index(f"'{KEY}'"))

    def test_enabled_mods_lists_it_only_while_on(self):
        base = defaults()
        cases = {"defaults": base, "on": {**base, KEY: True}, "both": {**base, SET_KEY: True, KEY: True}}
        driver = (f"const cases={json.dumps(cases)};"
                  "console.log(JSON.stringify(Object.fromEntries(Object.entries(cases)"
                  ".map(([k,c])=>[k,enabledControls(c)]))));")
        result = run_node(js_for_node(panel_file("enabled-mods.js")), driver)
        self.assertEqual(result, {"defaults": [], "on": [KEY], "both": [SET_KEY, KEY]})

    def test_the_defaults_fixture_carries_it_off(self):
        fixture = json.loads((ROOT / "panel" / "tests" / "fixtures" / "defaults-cfg.json").read_text(encoding="utf-8"))
        self.assertIs(fixture[KEY], False)


class Mark1DropsOracleTests(unittest.TestCase):
    """The NATIVE_BOOLEANS entry of the derived behaviour oracle."""

    def test_the_native_booleans_carry_the_switch_on_mods_items_after_the_mark1_set(self):
        source = (ROOT / "panel" / "tests" / "oracle-derive.mjs").read_text(encoding="utf-8")
        block = source.split("export const NATIVE_BOOLEANS = [", 1)[1].split("];", 1)[0]
        self.assertIn(f"{{ key: '{KEY}', tab: 'tab:mods', sub: 'subtab:items', verb: '{VERB}' }}", block)
        self.assertLess(block.index(f"'{SET_KEY}'"), block.index(f"'{KEY}'"))
        # Before Jump through scenery, so the loop still ends on Mods > Quality
        # of Life, where the show key's select follows without navigating.
        self.assertLess(block.index(f"'{KEY}'"), block.index("'mod_jump_scenery'"))
        last = re.findall(r"\{ key: '[^']+', tab: '([^']+)'(?:, sub: '([^']+)')?", block)[-1]
        self.assertEqual(last, ("tab:mods", "subtab:qol"))

    def test_the_derived_oracle_carries_its_steps(self):
        derived = (ROOT / "panel" / "tests" / "behaviour-oracle-derived.json").read_text(encoding="utf-8")
        self.assertIn(f'"#{KEY}"', derived)
        self.assertIn(f'"{VERB} 1"', derived)
        self.assertIn(f'"{VERB} 0"', derived)


if __name__ == "__main__":
    unittest.main()
