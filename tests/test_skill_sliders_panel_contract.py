"""Skill sliders in the panel and in the commands it sends (#160).

test_skill_sliders_behavior.py runs the plugin's levers; this covers the other
end: the three Modifiers rows, what they save, and the `skillslider` lines they
send at startup and live, against the ceilings SkillSlidersMod.hpp clamps to.
"""
import copy
import re
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "src"))
sys.path.insert(0, str(Path(__file__).resolve().parent))
import forgepact
from panel_source import panel_file
from test_slider_switches import LiveSandbox

SLIDERS_HEADER = ROOT / "plugin/include/ForgePact/SkillSlidersMod.hpp"

SKILL_KEYS = ("projspeed", "projamount", "aoesize")
# key -> the header constant its ceiling must equal
HEADER_CLAMPS = {"projspeed": "kProjSpeedMax", "projamount": "kProjAmountMax", "aoesize": "kAoeSizeMax"}


def percent_row(key):
    return next(row for row in forgepact.PERCENT_STATS if row[0] == key)


class SkillRowTests(unittest.TestCase):
    def test_baseline_existing_rows_keep_their_modes(self):
        self.assertEqual(percent_row("damage"), ("damage", "Total Damage", 1000, 5, "multiply"))
        self.assertEqual(percent_row("allskills"), ("allskills", "All Skills", 100, 1, "add"))

    def test_the_three_rows_their_mode_ceiling_and_step(self):
        self.assertEqual(percent_row("projspeed"), ("projspeed", "Projectile Speed", 100, 5, "skill"))
        self.assertEqual(percent_row("projamount"), ("projamount", "Projectile Amount", 5, 1, "skill"))
        self.assertEqual(percent_row("aoesize"), ("aoesize", "Area of Effect", 100, 5, "skill"))
        self.assertEqual([k for k, *_, mode in forgepact.PERCENT_STATS if mode == "skill"], list(SKILL_KEYS))

    def test_projectile_amount_is_whole(self):
        self.assertEqual(forgepact.WHOLE_PERCENT_STATS, frozenset({"allskills", "projamount"}))

    def test_all_three_default_to_off_with_a_switch(self):
        for key in SKILL_KEYS:
            with self.subTest(key=key):
                self.assertEqual(forgepact.DEFAULTS["percent_stats"][key], 0)
                self.assertIn(f"percent_stats.{key}", forgepact.SLIDER_SWITCH_IDS)

    def test_each_ceiling_is_the_plugins_clamp(self):
        header = SLIDERS_HEADER.read_text(encoding="utf-8")
        for key, name in HEADER_CLAMPS.items():
            with self.subTest(key=key):
                cap = re.search(rf"static constexpr double {name} = ([0-9.]+);", header)
                self.assertIsNotNone(cap, name)
                self.assertEqual(float(cap.group(1)), percent_row(key)[2])


class StartupCommandTests(unittest.TestCase):
    def cmds(self, **values):
        cfg = copy.deepcopy(forgepact.DEFAULTS)
        cfg["percent_stats"].update(values)
        return [c for c in forgepact.build_cmds(cfg) if c.startswith("skillslider ")]

    def test_baseline_defaults_send_nothing(self):
        self.assertEqual(self.cmds(), [])
        self.assertFalse(any(c.startswith("skillslider ") for c in forgepact.build_cmds(copy.deepcopy(forgepact.DEFAULTS))))

    def test_target_each_slider_sends_its_skillslider_line(self):
        self.assertEqual(self.cmds(projspeed=50, projamount=2, aoesize=25),
                         ["skillslider projspeed 50", "skillslider projamount 2", "skillslider aoesize 25"])

    def test_values_snap_to_the_step_and_the_ceiling(self):
        self.assertEqual(self.cmds(projspeed=52, projamount=2.6, aoesize=9000),
                         ["skillslider projspeed 50", "skillslider projamount 3", "skillslider aoesize 100"])
        self.assertEqual(self.cmds(projamount=40), ["skillslider projamount 5"])

    def test_a_switched_off_slider_sends_nothing(self):
        cfg = copy.deepcopy(forgepact.DEFAULTS)
        cfg["percent_stats"]["projspeed"] = 50
        cfg["switches"] = {"percent_stats.projspeed": False}
        self.assertFalse(any(c.startswith("skillslider ") for c in forgepact.build_cmds(cfg)))


class LiveSetTests(unittest.TestCase):
    def test_each_slider_sends_its_line_live(self):
        live = LiveSandbox(self)
        for key, value, line in (("projspeed", 40, "skillslider projspeed 40"),
                                 ("projamount", 3, "skillslider projamount 3"),
                                 ("aoesize", 100, "skillslider aoesize 100")):
            with self.subTest(key=key):
                code, _, sent = live.post("percent_stats", key, value)
                self.assertEqual(code, 200)
                self.assertEqual(sent, [[line]])

    def test_a_typed_projectile_amount_is_saved_and_sent_whole(self):
        live = LiveSandbox(self)
        code, _, sent = live.post("percent_stats", "projamount", 2.6)
        self.assertEqual(code, 200)
        self.assertEqual(sent, [["skillslider projamount 3"]])
        self.assertEqual(live.saved()["percent_stats"]["projamount"], 3)

    def test_a_typed_value_is_clamped_to_the_ceiling(self):
        live = LiveSandbox(self)
        _, _, sent = live.post("percent_stats", "projamount", 40)
        self.assertEqual(sent, [["skillslider projamount 5"]])

    def test_setting_zero_sends_zero(self):
        live = LiveSandbox(self)
        live.post("percent_stats", "aoesize", 50)
        _, _, sent = live.post("percent_stats", "aoesize", 0)
        self.assertEqual(sent, [["skillslider aoesize 0"]])

    def test_switching_a_running_slider_off_sends_zero_and_on_restores_it(self):
        live = LiveSandbox(self)
        live.post("percent_stats", "projspeed", 60)
        code, _, sent = live.post("switches", "percent_stats.projspeed", False)
        self.assertEqual(code, 200)
        self.assertEqual(sent, [["skillslider projspeed 0"]])
        _, _, sent = live.post("switches", "percent_stats.projspeed", True)
        self.assertEqual(sent, [["skillslider projspeed 60"]])


class PanelSourceTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.panel = panel_file("panel.js")
        cls.icons = panel_file("icons.js")
        cls.modifiers = panel_file("tabs/Modifiers.svelte")

    def test_baseline_offense_list_is_unchanged(self):
        self.assertIn("percentRows(['damage','attackspeed','castrate','skillhaste','allskills'])", self.panel)

    def test_the_rows_are_drawn_in_a_skills_group_on_modifiers(self):
        self.assertIn("document.getElementById('skillstats').innerHTML=percentRows(['projspeed','projamount','aoesize']);",
                      self.panel)
        group = re.search(r'<div class="modifier-group">\s*<div class="group-title[^"]*" data-icon="projectile-amount">Skills</div>\s*'
                          r'<div id="skillstats"></div>\s*</div>', self.modifiers)
        self.assertIsNotNone(group, "no Skills group holding #skillstats in Modifiers.svelte")
        # Each group title names its own icon, so adding or moving a group cannot
        # shift icons onto the wrong heading (#172 review).
        titles = re.findall(r'<div class="group-title[^"]*"([^>]*)>', self.modifiers)
        self.assertEqual(len(titles), 5)
        self.assertTrue(all('data-icon="' in a for a in titles), titles)
        self.assertIn("decorateIconLabel(label,label.dataset.icon)", self.panel)
        css = panel_file("app.css")
        self.assertIn(".modifier-group:last-child:nth-child(odd){grid-column:1/-1}", css)
        self.assertIn('data-tab="modifiers"', self.modifiers)

    def test_speed_reads_as_a_percentage_the_others_as_a_count(self):
        levels = re.search(r"const LEVEL_PERCENT_STATS=new Set\(\[(.*?)\]\);", self.panel)
        self.assertIsNotNone(levels)
        keys = set(re.findall(r"'([^']+)'", levels.group(1)))
        self.assertLessEqual({"allskills", "projamount", "aoesize"}, keys)
        self.assertNotIn("projspeed", keys)

    def notes(self):
        body = self.panel.split("function percentStatNote(key,v){", 1)[1].split("\n}\n", 1)[0]
        found = {}
        for key in SKILL_KEYS:
            m = re.search(rf"if\(key==='{key}'\) return `([^`]*)`;", body)
            self.assertIsNotNone(m, f"no note for {key}")
            found[key] = m.group(1)
        return found

    def test_each_note_is_short_and_says_what_it_does_without_measurement(self):
        for key, note in self.notes().items():
            with self.subTest(key=key):
                self.assertLess(len(note), 300)
                self.assertIn("your own", note)
                for word in ("measured", "Live", "live", "tested", "research", "stat 75", "554",
                             "Return", "LoadAllModifiers", "hook", "double cast"):
                    self.assertNotIn(word, note)

    def test_each_row_has_a_drawn_icon(self):
        m = re.search(r"'percent_stats':\s*zip\(\[(.*?)\],\s*\[(.*?)\]\)", self.icons, re.S)
        keys, icons = (re.findall(r"'([^']+)'", part) for part in m.groups())
        self.assertEqual(len(keys), len(icons))
        mapping = dict(zip(keys, icons))
        drawn = set(re.findall(r"^  '([a-z0-9-]+)':",
                               re.search(r"export const ICONS = \{(.*?)\n\};", self.icons, re.S).group(1), re.M))
        for key in SKILL_KEYS:
            with self.subTest(key=key):
                self.assertIn(key, mapping)
                self.assertIn(mapping[key], drawn)


class DefaultsFixtureTests(unittest.TestCase):
    def test_the_defaults_fixture_carries_the_three_keys_at_zero(self):
        import json
        fixture = json.loads((ROOT / "panel/tests/fixtures/defaults-cfg.json").read_text(encoding="utf-8"))
        for key in SKILL_KEYS:
            self.assertEqual(fixture["percent_stats"].get(key), 0, key)


if __name__ == "__main__":
    unittest.main()
