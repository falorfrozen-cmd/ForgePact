#!/usr/bin/env python3
"""Panel contract for "Pet collects relics" (`mod_pet_relic_pickup`, `petrelic`, #124).

The switch mirrors every site Pet collects quest items (`mod_pet_quest_pickup`)
has, because a switch missing one of them fails quietly: without the default it
is not off on a fresh install, without the `/api/set` key list it never saves,
without the live dispatch it saves and the running game never hears it, without
`build_cmds` it is off again after a game restart, and without the panel.js
sites it shows the wrong state after a reload or a Turn off. The plugin side
(the command, the tick, the counters) is pinned by
test_pet_relic_collector_contract.py.

Baseline: with the defaults, nothing is sent and the switch reads off. Target:
on, the launch commands carry `petrelic 1`, and the Mods tab shows the row on
the Quality of Life card directly after Pet collects quest items.
"""

import re
import sys
import unittest
from pathlib import Path

TESTS_DIR = Path(__file__).resolve().parent
FORGEPACT_DIR = TESTS_DIR.parent
REPO_ROOT = FORGEPACT_DIR.parent
SRC_DIR = FORGEPACT_DIR / "src"
SDK_PY_PATH = REPO_ROOT / "hs-game-sdk" / "python"

for path in (TESTS_DIR, SRC_DIR, SDK_PY_PATH):
    if str(path) not in sys.path:
        sys.path.insert(0, str(path))

import forgepact  # noqa: E402
from panel_source import panel_file  # noqa: E402

KEY = "mod_pet_relic_pickup"
VALUE_SPAN = "mprpval"
VERB = "petrelic"
TITLE = "Pet collects relics"


def _qol_card(mods):
    start = mods.index('id="qolCard"')
    end = mods.find('<div class="card', start)
    return mods[start:] if end < 0 else mods[start:end]


class PetRelicPanelBaselineTests(unittest.TestCase):
    """Mod off: the default is off and nothing is sent at launch."""

    def test_default_is_off(self):
        self.assertIn(KEY, forgepact.DEFAULTS)
        self.assertIs(forgepact.DEFAULTS[KEY], False)

    def test_build_cmds_sends_nothing_while_off(self):
        cmds = forgepact.build_cmds(dict(forgepact.DEFAULTS))
        self.assertFalse([c for c in cmds if c.startswith(VERB)], cmds)

    def test_build_cmds_sends_nothing_when_explicitly_off(self):
        cfg = dict(forgepact.DEFAULTS)
        cfg[KEY] = False
        cfg["mod_pet_quest_pickup"] = True
        cmds = forgepact.build_cmds(cfg)
        # Negative control: the neighbouring switch still sends its own command.
        self.assertIn("petquest 1", cmds)
        self.assertFalse([c for c in cmds if c.startswith(VERB)], cmds)


class PetRelicPanelBackendTests(unittest.TestCase):
    """Mod on: the launch command, the saved key and the live command."""

    @classmethod
    def setUpClass(cls):
        cls.backend = (SRC_DIR / "forgepact.py").read_text(encoding="utf-8-sig")

    def test_build_cmds_sends_the_command_when_on(self):
        cfg = dict(forgepact.DEFAULTS)
        cfg[KEY] = True
        self.assertIn(f"{VERB} 1", forgepact.build_cmds(cfg))

    def test_build_cmds_is_independent_of_pet_collects_quest_items(self):
        cfg = dict(forgepact.DEFAULTS)
        cfg[KEY] = True
        cmds = forgepact.build_cmds(cfg)
        self.assertNotIn("petquest 1", cmds)
        self.assertEqual([c for c in cmds if c.startswith(VERB)], [f"{VERB} 1"])

    def test_api_set_boolean_key_list_carries_the_key(self):
        bool_keys = re.search(r'elif key in \((?P<keys>[^)]*"mod_pet_quest_pickup"[^)]*)\):', self.backend)
        self.assertIsNotNone(bool_keys)
        self.assertIn(f'"{KEY}"', bool_keys.group("keys"))

    def test_live_dispatch_sends_the_command(self):
        self.assertEqual(self.backend.count(f'elif key == "{KEY}":'), 1)
        branch = self.backend.split(f'elif key == "{KEY}":', 1)[1][:200]
        self.assertIn(f"send_cmds([f\"{VERB} {{1 if cfg['{KEY}'] else 0}}\"], cfg)", branch)


class PetRelicPanelPageTests(unittest.TestCase):
    """The Mods-tab row and the panel.js, enabled-mods.js and icons.js sites."""

    @classmethod
    def setUpClass(cls):
        cls.mods = panel_file("tabs/Mods.svelte")
        cls.panel_js = panel_file("panel.js")
        cls.enabled = panel_file("enabled-mods.js")
        cls.icons = panel_file("icons.js")

    def row(self):
        self.assertEqual(self.mods.count(f'id="{KEY}"'), 1)
        head = self.mods[:self.mods.index(f'id="{KEY}"')]
        start = head.rindex('<div class="row"')
        end = self.mods.index("</div>", self.mods.index(f'id="{KEY}"'))
        return self.mods[start:end]

    def test_row_sits_after_pet_quest_items_and_before_pet_moves_on(self):
        card = _qol_card(self.mods)
        quest = card.index('id="mod_pet_quest_pickup"')
        relic = card.index(f'id="{KEY}"')
        unstick = card.index('id="mod_pet_loot_unstick"')
        self.assertLess(quest, relic)
        self.assertLess(relic, unstick)

    def test_row_is_a_switch_with_its_value_span(self):
        row = self.row()
        self.assertIn(f'<label class="switch"><input type="checkbox" id="{KEY}"><span class="sl"></span></label>', row)
        self.assertIn(f'<span class="val" id="{VALUE_SPAN}">off</span>', row)
        self.assertEqual(self.mods.count(f'id="{VALUE_SPAN}"'), 1)

    def test_row_text_is_short_and_names_no_coverage_figure(self):
        label = re.search(r'<span class="lbl"[^>]*>([^<]*)<br><span class="feature-description">(.*?)</span></span>',
                          self.row())
        self.assertIsNotNone(label, self.row())
        self.assertEqual(label.group(1), TITLE)
        text = label.group(2)
        self.assertLessEqual(len(text), 300, text)
        self.assertIn("10/10", text)
        self.assertIn("Off by default.", text)
        self.assertNotRegex(text, r"\d+\s*(%|px|frames?)", text)
        for word in ("measured", "static reading", "PickupLoot", "PickupRelic", "relicLevel", "#124"):
            self.assertNotIn(word, text, word)

    def test_panel_js_restores_the_switch_from_config(self):
        self.assertIn(f"const mprp=!!c.{KEY};", self.panel_js)
        self.assertIn(f"document.getElementById('{KEY}').checked=mprp;", self.panel_js)
        self.assertIn(f"document.getElementById('{VALUE_SPAN}').textContent=mprp?'on':'off';", self.panel_js)

    def test_panel_js_posts_the_key_and_toasts_on_change(self):
        start = self.panel_js.index(f"document.getElementById('{KEY}').onchange=")
        handler = self.panel_js[start:self.panel_js.index("};", start)]
        self.assertIn(f"j('/api/set',{{method:'POST',body:JSON.stringify({{key:'{KEY}',value:e.target.checked}})}})", handler)
        self.assertIn(f"document.getElementById('{VALUE_SPAN}')", handler)
        self.assertIn(f"toast('{TITLE} '+(e.target.checked?'ON':'OFF')", handler)

    def test_panel_js_lists_it_in_the_booleans_and_value_span_maps(self):
        start = self.panel_js.index("const booleans={")
        booleans = self.panel_js[start:self.panel_js.index("};", start)]
        self.assertIn(f"{KEY}:'{KEY}'", booleans)
        spans = self.panel_js[self.panel_js.index("mpqpval:'mod_pet_quest_pickup'") - 200:]
        spans = spans[:spans.index("})")]
        self.assertIn(f"{VALUE_SPAN}:'{KEY}'", spans)

    def test_enabled_mods_lists_it(self):
        start = self.enabled.index("export const BOOLEAN_MODS = [")
        body = self.enabled[start:self.enabled.index("];", start)]
        self.assertIn(f"'{KEY}'", body)
        self.assertLess(body.index("'mod_pet_quest_pickup'"), body.index(f"'{KEY}'"))

    def test_icons_give_it_the_pet_icon(self):
        start = self.icons.index("export const STATIC_ICONS = {")
        body = self.icons[start:self.icons.index("};", start)]
        self.assertIn(f"'{KEY}': 'pet'", body)


if __name__ == "__main__":
    unittest.main()
