#!/usr/bin/env python3
"""The panel's slider switches, its "Enabled mods" list and its theme choice.

What the page must carry, read from `panel/src` through `panel_source.py`:
every slider row but Monster Density's has a switch (density keeps `#den_on`),
the list sits under the control dock with the fixed copy, its Turn off buttons
reach a control only through that control's own handler (so the list's files
never name an `/api` route and `/api/set` gains exactly two call sites, the
switch handler and the theme handler), and the theme is a `<select>` in the
Setup tab's Appearance card (it was in the status bar until the owner's
polish pass, 2026-09-25) painted as `data-theme` on the root.

Two parity checks tie the JavaScript to `src/forgepact.py`: the node tests'
defaults fixture is the real `DEFAULTS`, and the page's list of switched
sliders, run through node, is the backend's `SLIDER_SWITCH_IDS`.

The behaviour itself is proven in the browser: `npm run oracle:replay` (the
derived oracle) and `npm run e2e` (the `enabled_mods` group).
"""

import json
import re
import sys
import unittest
from pathlib import Path

TESTS = Path(__file__).resolve().parent
ROOT = TESTS.parent
sys.path.insert(0, str(TESTS))
sys.path.insert(0, str(ROOT / "src"))

from panel_source import js_for_node, panel_file, panel_source  # noqa: E402
from test_panel_performance import run_node  # noqa: E402

FIXED_COPY = ("Enabled mods", "Nothing is on", "Turn off", "Theme", "Ledger", "Graphite", "Sigil")
LIST_FILES = ("enabled-mods.js", "lib/enabled-mods-list.js")


def _opening_tag(text, marker):
    """The whole opening tag that contains `marker`."""
    at = text.index(marker)
    return text[text.rindex("<", 0, at):text.index(">", at) + 1]


class EnabledModsPanelTests(unittest.TestCase):
    def setUp(self):
        self.page = panel_source()
        self.app = panel_file("App.svelte")
        self.panel = panel_file("panel.js")
        self.lister = panel_file("lib/enabled-mods-list.js")
        self.setup = panel_file("tabs/Setup.svelte")

    def test_list_strings_are_the_fixed_copy(self):
        for text in FIXED_COPY:
            with self.subTest(text=text):
                self.assertIn(text, self.page)
        self.assertIn('<section id="enabledMods" aria-label="Enabled mods"><h2>Enabled mods</h2>', self.app)
        self.assertIn('<p class="enabled-mods-empty">Nothing is on</p>', self.app)
        # The count, the empty line, the button and both aria-labels, as the
        # renderer builds them.
        self.assertIn("`${controls.length} on`", self.lister)
        self.assertIn("empty.textContent = 'Nothing is on';", self.lister)
        self.assertIn("button.textContent = 'Turn off';", self.lister)
        self.assertIn("`Turn off ${name}`", self.lister)
        self.assertIn('aria-label="Enable ${label}"', self.panel)
        theme = panel_file("theme.js")
        self.assertIn("{ value: 'ledger', label: 'Ledger' }", theme)
        self.assertIn("{ value: 'graphite', label: 'Graphite' }", theme)
        self.assertIn("{ value: 'sigil', label: 'Sigil' }", theme)

    def test_quick_disable_and_entries_carry_data_for(self):
        self.assertIn("item.className = 'enabled-mod';", self.lister)
        self.assertIn("item.dataset.for = control.id;", self.lister)
        self.assertIn("button.className = 'quick-disable';", self.lister)
        self.assertIn("button.dataset.for = control.id;", self.lister)
        self.assertIn("button.type = 'button';", self.lister)
        # No id, so the oracle's control walk (`button[id]`) never lists it.
        self.assertNotRegex(self.lister, r"button\.id\s*=")
        for cls in ("enabled-mod-name", "enabled-mod-value"):
            self.assertIn(f"'{cls}'", self.lister)
        # Turn off is the control's own change handler, not a write of its own.
        body = self.lister[self.lister.index("export function quickDisable"):]
        body = body[:body.index("\n}\n")]
        self.assertIn("control.checked = false", body)
        self.assertIn("control.value = 'off'", body)
        self.assertIn("control.dispatchEvent(new Event('change', { bubbles: true }))", body)

    def test_enabled_mods_files_never_name_an_api_route(self):
        for rel in LIST_FILES + ("theme.js",):
            with self.subTest(file=rel):
                text = panel_file(rel)
                self.assertNotIn("/api", text)
                self.assertNotIn("fetch(", text)
                self.assertNotIn("localStorage", text)

    def test_api_set_call_sites_are_the_old_ones_plus_switch_and_theme(self):
        # The old ones: the legacy page's 25, plus the 2 main's legacy page
        # added for Gems of Incarnation (its switches' handler and the filter's
        # save) before it was ported here, plus 4 no legacy page had: the Pet
        # moves on switch (forgepact-pet-loot-stuck), Far scenery sleep's
        # switch, and Sleep loot your filter hides' switch and show key
        # (forgepact-issue-95-mod; all four in the derived oracle).
        self.assertEqual(self.panel.count("j('/api/set'"), 27 + 2 + 4)
        self.assertEqual(self.page.count("j('/api/set'"), 27 + 2 + 4)
        self.assertIn("section:'switches',key:box.dataset.switch,value:box.checked", self.panel)
        self.assertIn("{key:'theme',value:e.target.value}", self.panel)
        # One handler for every switch, bound by the data attribute.
        self.assertEqual(self.panel.count("section:'switches'"), 1)
        self.assertIn("document.querySelectorAll('input[data-switch]').forEach(box=>{\n    box.onchange=", self.panel)

    def test_every_slider_row_has_a_switch_and_density_keeps_den_on(self):
        row = re.search(r"function row\(sec,key,label,val,tagHtml,max,note,step\)\{.*?\n\}\n", self.panel, re.S)
        self.assertIsNotNone(row)
        body = row.group(0)
        # Inside the row, before the range.
        self.assertLess(body.index("switchMarkup(sec+'.'+key,label)"), body.index('<input type="range"'))
        markup = self.panel[self.panel.index("function switchMarkup(id,label){"):]
        markup = markup[:markup.index("\n}\n")]
        self.assertIn('<label class="switch slider-switch"><input type="checkbox" id="${switchControlId(id)}" '
                      'data-switch="${id}" aria-label="Enable ${label}"><span class="sl"></span></label>', markup)
        world, loot = panel_file("tabs/World.svelte"), panel_file("tabs/Loot.svelte")
        for text, switch_id, range_id, label in (
                (world, "enemy_speed", "enemyspeed", "Speed bonus"),
                (world, "rarity_rare", "rarity_rare", "Rare"),
                (world, "rarity_ancient", "rarity_ancient", "Ancient"),
                (loot, "angelic_items", "angelic_items", "Angelic / Unholy items")):
            with self.subTest(switch=switch_id):
                tag = (f'<label class="switch slider-switch"><input type="checkbox" id="sw_{switch_id}" '
                       f'data-switch="{switch_id}" aria-label="Enable {label}"><span class="sl"></span></label>')
                self.assertEqual(text.count(tag), 1)
                self.assertLess(text.index(tag), text.index(f'id="{range_id}"'))
                # Same row: no row boundary between the switch and its range.
                self.assertNotIn('<div class="row"', text[text.index(tag):text.index(f'id="{range_id}"')])
        # Density keeps its own switch and gets no second one.
        self.assertEqual(world.count('id="den_on"'), 1)
        self.assertNotIn("sw_den", self.page)
        self.assertNotIn('data-switch="density"', self.page)
        # Nothing new is a tab card.
        self.assertEqual(self.page.count('data-tab="mods"'), 3)
        for tag in (_opening_tag(self.app, 'id="enabledMods"'), _opening_tag(self.setup, 'id="theme"')):
            self.assertNotIn("data-tab", tag)

    def test_list_sits_under_the_control_dock(self):
        dock = self.app.index('<div class="control-dock">')
        section = self.app.index('<section id="enabledMods"')
        warning = self.app.index('<div id="pluginWarning"')
        self.assertLess(dock, section)
        self.assertLess(section, warning)
        # After the dock has closed, inside main#wrap.
        self.assertIn("</div></div>", self.app[dock:section])
        main = _opening_tag(self.app, 'id="wrap"')
        self.assertTrue(main.startswith("<main "))
        self.assertIn('tabindex="0"', main)
        self.assertIn('aria-labelledby="pageTitle"', main)
        self.assertLess(self.app.index(main), section)
        self.assertLess(section, self.app.index("</main>"))
        # Re-rendered from boot() and from refreshSavedControls().
        boot = self.panel[self.panel.index("async function boot(){"):self.panel.index("function paintVersion(){")]
        refresh = self.panel[self.panel.index("export function refreshSavedControls(){"):self.panel.index("export function filterControlRows(){")]
        self.assertIn("renderEnabledMods(", boot)
        self.assertIn("renderEnabledMods(c);", refresh)
        self.assertIn("paintSwitches(c);", boot)
        self.assertIn("paintSwitches(c);", refresh)
        self.assertLess(refresh.index("paintSwitches(c);"), refresh.index("range.oninput"))

    def test_theme_select_and_root_attribute(self):
        # The theme is the Setup tab's Appearance card (owner, 2026-09-25),
        # not the status bar: a Setup tab card after #setupCard, and nowhere
        # in App.svelte.
        card = self.setup[self.setup.index('<h2>Appearance</h2>'):]
        card = card[:card.index("</div>\n</div>")]
        # The ThemePicker (finish review F2) labels its trigger with the row's
        # "Theme"; the native select stays inside the picker as the control of
        # record, hidden from Tab and assistive technology.
        self.assertIn('<span class="lbl" id="themeLabel">Theme</span><div class="theme-picker">', card)
        self.assertIn('<select id="theme" class="theme-picker-native" aria-hidden="true" tabindex="-1">', card)
        self.assertLess(self.setup.index('id="setupCard"'), self.setup.index('id="theme"'))
        before = self.setup[:self.setup.index('<h2>Appearance</h2>')]
        opening = before[before.rindex('<div class="card'):]
        self.assertIn('data-tab="setup"', opening)
        self.assertIn("tab-card", opening)
        self.assertIn("{#each THEMES as theme", card)
        self.assertNotIn('id="theme"', self.app)
        theme = panel_file("theme.js")
        self.assertIn("document.documentElement.dataset.theme = painted", theme)
        self.assertIn("typeof document !== 'undefined'", theme)
        css = panel_file("tokens.css")
        for name in ("graphite", "sigil"):
            block = re.search(r':root\[data-theme="' + name + r'"\]\s*\{([^}]*)\}', css)
            self.assertIsNotNone(block)
            self.assertGreaterEqual(len(re.findall(r"--[\w-]+:", block.group(1))), 2)
        boot = self.panel[self.panel.index("async function boot(){"):self.panel.index("function paintVersion(){")]
        self.assertIn("applyTheme(c.theme)", boot)
        handler = self.panel[self.panel.index("document.getElementById('theme').onchange"):]
        handler = handler[:handler.index("\n  };\n")]
        self.assertIn("applyTheme((res.cfg||ST.cfg).theme)", handler)
        for rel in ("theme.js", "panel.js") + LIST_FILES:
            self.assertNotIn("localStorage", panel_file(rel))

    def test_defaults_fixture_is_forgepacts_defaults(self):
        import forgepact  # noqa: E402  (only here: the other tests need no hs_game_sdk)
        fixture = json.loads((ROOT / "panel" / "tests" / "fixtures" / "defaults-cfg.json").read_text(encoding="utf-8"))
        expected = {k: v for k, v in forgepact.DEFAULTS.items() if k != "game_exe"}
        self.assertEqual(fixture, json.loads(json.dumps(expected)))

    def test_switch_ids_match_the_backend(self):
        import forgepact  # noqa: E402
        cfg = json.dumps({k: v for k, v in forgepact.DEFAULTS.items() if k != "game_exe"})
        driver = f"const cfg={cfg};console.log(JSON.stringify({{ids:sliderSwitchIds(cfg),controls:enabledControls(cfg)}}));"
        result = run_node(js_for_node(panel_file("enabled-mods.js")), driver)
        self.assertEqual(result["ids"], list(forgepact.SLIDER_SWITCH_IDS))
        self.assertEqual(result["controls"], [])
        self.assertEqual(forgepact.build_cmds(json.loads(cfg)), [])

    def test_gem_switches_are_entries_only_while_on(self):
        # Both Gems of Incarnation switches are boolean mods, off by default,
        # so a fresh install lists nothing; turned on, each is its own entry.
        # The mod filter is an option of the Mythic entry, never one itself.
        import forgepact  # noqa: E402
        base = {k: v for k, v in forgepact.DEFAULTS.items() if k != "game_exe"}
        cases = {
            "defaults": base,
            "both_on": {**base, "mod_gem_mythic": True, "mod_gem_maxroll": True},
            "filter_only": {**base, "gem_filter": [68, 284]},
        }
        driver = (f"const cases={json.dumps(cases)};"
                  "console.log(JSON.stringify(Object.fromEntries(Object.entries(cases)"
                  ".map(([k,c])=>[k,enabledControls(c)]))));")
        result = run_node(js_for_node(panel_file("enabled-mods.js")), driver)
        self.assertEqual(result["defaults"], [])
        self.assertEqual(result["both_on"], ["mod_gem_mythic", "mod_gem_maxroll"])
        self.assertEqual(result["filter_only"], [])
        # The backend agrees: the defaults send no gem command, both on send two.
        self.assertEqual(forgepact.build_cmds(cases["defaults"]), [])
        self.assertEqual([c for c in forgepact.build_cmds(cases["both_on"]) if c.startswith("gem")],
                         ["gemmythic 1", "gemmaxroll 1"])

    def test_hidden_loot_is_an_entry_only_while_on_and_its_key_never_is(self):
        # Sleep loot your filter hides is a boolean mod, off by default; its
        # show key rides on its entry, as the gem filter rides on Mythic's.
        # The list and the backend agree: an entry exactly when a command goes.
        import forgepact  # noqa: E402
        base = {k: v for k, v in forgepact.DEFAULTS.items() if k != "game_exe"}
        cases = {
            "defaults": base,
            "key_only": {**base, "mod_hidden_loot_key": 17},
            "on": {**base, "mod_hidden_loot": True},
        }
        driver = (f"const cases={json.dumps(cases)};"
                  "console.log(JSON.stringify(Object.fromEntries(Object.entries(cases)"
                  ".map(([k,c])=>[k,enabledControls(c)]))));")
        result = run_node(js_for_node(panel_file("enabled-mods.js")), driver)
        self.assertEqual(result, {"defaults": [], "key_only": [], "on": ["mod_hidden_loot"]})
        sent = {name: [c for c in forgepact.build_cmds(cfg) if c.startswith("hiddenloot")] for name, cfg in cases.items()}
        self.assertEqual(sent, {"defaults": [], "key_only": [], "on": ["hiddenloot key 164", "hiddenloot 1"]})


if __name__ == "__main__":
    unittest.main()
