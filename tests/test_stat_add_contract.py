"""Skill Haste and All Skills in the panel and in the commands it sends (#114).

test_stat_add_behavior.py runs the plugin's hooks; this covers the other end:
the two panel rows, what they save, and the `statadd` lines they send.
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

STATS_HEADER = ROOT / "plugin/include/ForgePact/StatsManager.hpp"
PLUGIN = ROOT / "plugin/ModuleMain.cpp"


def percent_row(key):
    return next(row for row in forgepact.PERCENT_STATS if row[0] == key)


class PercentStatRowTests(unittest.TestCase):
    def test_baseline_faster_cast_rate_row_is_unchanged(self):
        self.assertEqual(percent_row("castrate"), ("castrate", "Faster Cast Rate", 500, 5, "add"))

    def test_skill_haste_and_all_skills_are_additive_rows(self):
        # 200, not Stat Forge's 500: the game counts at most 200 Skill Haste in
        # total (measured, #114 Live 1), so a larger bonus changes nothing.
        self.assertEqual(percent_row("skillhaste"), ("skillhaste", "Skill Haste", 200, 5, "add"))
        self.assertEqual(percent_row("allskills"), ("allskills", "All Skills", 100, 1, "add"))
        self.assertEqual(forgepact.WHOLE_PERCENT_STATS, frozenset({"allskills"}))

    def test_both_default_to_off(self):
        self.assertEqual(forgepact.DEFAULTS["percent_stats"]["skillhaste"], 0)
        self.assertEqual(forgepact.DEFAULTS["percent_stats"]["allskills"], 0)
        self.assertIn("percent_stats.skillhaste", forgepact.SLIDER_SWITCH_IDS)
        self.assertIn("percent_stats.allskills", forgepact.SLIDER_SWITCH_IDS)

    def test_the_all_skills_ceiling_is_the_plugins_cap(self):
        header = STATS_HEADER.read_text(encoding="utf-8")
        cap = re.search(r"static constexpr double kAllSkillsMax = ([0-9.]+);", header)
        self.assertIsNotNone(cap)
        self.assertEqual(float(cap.group(1)), percent_row("allskills")[2])


class StartupCommandTests(unittest.TestCase):
    def cmds(self, **values):
        cfg = copy.deepcopy(forgepact.DEFAULTS)
        cfg["percent_stats"].update(values)
        return [c for c in forgepact.build_cmds(cfg) if c.startswith("statadd ")]

    def test_baseline_off_sends_nothing(self):
        self.assertEqual(self.cmds(), [])

    def test_target_each_boost_sends_its_statadd_line(self):
        self.assertEqual(self.cmds(skillhaste=100, allskills=19),
                         ["statadd skillhaste 100", "statadd allskills 19"])

    def test_values_snap_to_the_step_and_the_ceiling(self):
        self.assertEqual(self.cmds(skillhaste=102, allskills=19.6),
                         ["statadd skillhaste 100", "statadd allskills 20"])
        self.assertEqual(self.cmds(skillhaste=9000, allskills=500),
                         ["statadd skillhaste 200", "statadd allskills 100"])


class LiveSetTests(unittest.TestCase):
    def test_a_typed_all_skills_value_is_saved_and_sent_whole(self):
        live = LiveSandbox(self)
        code, _, sent = live.post("percent_stats", "allskills", 19.6)
        self.assertEqual(code, 200)
        self.assertEqual(sent, [["statadd allskills 20"]])
        self.assertEqual(live.saved()["percent_stats"]["allskills"], 20)

    def test_a_typed_skill_haste_value_keeps_two_decimals(self):
        live = LiveSandbox(self)
        code, _, sent = live.post("percent_stats", "skillhaste", 12.5)
        self.assertEqual(code, 200)
        self.assertEqual(sent, [["statadd skillhaste 12.5"]])
        self.assertEqual(live.saved()["percent_stats"]["skillhaste"], 12.5)

    def test_turning_either_off_sends_zero(self):
        live = LiveSandbox(self)
        live.post("percent_stats", "skillhaste", 100)
        _, _, sent = live.post("percent_stats", "skillhaste", 0)
        self.assertEqual(sent, [["statadd skillhaste 0"]])


class PluginSourceTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.header = STATS_HEADER.read_text(encoding="utf-8").replace("\r\n", "\n")
        cls.plugin = PLUGIN.read_text(encoding="utf-8").replace("\r\n", "\n")

    def test_the_three_entries_share_one_table(self):
        for alias, script, hook_id in (("castrate", "StatFasterCastRate", "fp_sta_fcr"),
                                       ("skillhaste", "StatSpellHaste", "fp_sta_sh"),
                                       ("allskills", "StatAllSkills", "fp_sta_as")):
            self.assertRegex(self.header, rf'\{{ "{alias}",\s+"{script}",\s+"{hook_id}",')
            self.assertIn(f"FP_STAT_ADD_HOOK({script})", self.header)

    def test_the_install_asks_for_the_native_route_and_refuses_table_only(self):
        self.assertIn("HookOneScript(e->script, e->hookId, e->hook, e->orig, &native);", self.header)
        self.assertIn("if (*e->route != kRouteNative) {", self.header)

    def test_the_first_call_line_ships_in_player_builds(self):
        # BP_DIAG_INCREMENT compiles away under FORGEPACT_RELEASE; this line
        # is how a player build shows the hook fired.
        body = self.header.split("static void AddAndReport(", 1)[1].split("\n    }\n", 1)[0]
        self.assertIn('"statadd %s: first boosted call %g -> %g"', body)
        self.assertNotIn("FORGEPACT_RELEASE", body)

    def test_statadd_is_a_player_command_and_tgprobe_cast_is_research_only(self):
        player = re.search(r"kPlayerCommands = \{(.*?)\};", self.plugin, re.S).group(1)
        self.assertIn('"statadd"', player)
        self.assertNotIn('"tgprobe"', player)
        cast = self.plugin.index("static void TgProbeCast(")
        self.assertLess(self.plugin.rfind("#ifndef FORGEPACT_RELEASE", 0, cast),
                        cast)
        self.assertGreater(self.plugin.rfind("#ifndef FORGEPACT_RELEASE", 0, cast),
                           self.plugin.rfind("#endif", 0, cast))

    def test_naddrall_lists_the_new_hook_targets(self):
        body = self.plugin.split("static void NAddrAll()", 1)[1].split("std::ofstream", 1)[0]
        self.assertIn('"StatSpellHaste", "StatAllSkills",', body)


class PanelSourceTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.panel = panel_file("panel.js")
        cls.icons = panel_file("icons.js")

    def test_both_rows_are_drawn_with_the_offensive_stats(self):
        self.assertIn("percentRows(['damage','attackspeed','castrate','skillhaste','allskills'])", self.panel)

    def test_all_skills_reads_as_levels_not_a_percentage(self):
        self.assertIn("const LEVEL_PERCENT_STATS=new Set(['allskills']);", self.panel)
        self.assertIn("LEVEL_PERCENT_STATS.has(key)?'':'%'", self.panel)
        # every place that paints a percent value passes the row's key
        self.assertEqual(re.findall(r"sliderText\((?:sec,val|r\.dataset\.sec,v)\)", self.panel), [])

    def test_each_row_explains_itself(self):
        self.assertIn("if(key==='skillhaste') return", self.panel)
        self.assertIn("if(key==='allskills') return", self.panel)

    def test_each_row_has_a_drawn_icon(self):
        m = re.search(r"'percent_stats':\s*zip\(\[(.*?)\],\s*\[(.*?)\]\)", self.icons, re.S)
        keys, icons = (re.findall(r"'([^']+)'", part) for part in m.groups())
        mapping = dict(zip(keys, icons))
        self.assertEqual(len(keys), len(icons))
        self.assertEqual(mapping["skillhaste"], "clock")
        self.assertEqual(mapping["allskills"], "skills")
        drawn = set(re.findall(r"^  '([a-z0-9-]+)':", re.search(r"export const ICONS = \{(.*?)\n\};", self.icons, re.S).group(1), re.M))
        self.assertLessEqual({"clock", "skills"}, drawn)


class DocsTests(unittest.TestCase):
    def test_readme_explains_both_boosts_and_the_cap(self):
        readme = (ROOT / "README.md").read_text(encoding="utf-8")
        section = readme.split("\n## Skill Haste and All Skills\n", 1)[1].split("\n## ", 1)[0]
        for fact in ("statadd skillhaste", "statadd allskills", "statadd list", "at most 200",
                     "first boosted call", "Stat Forge"):
            self.assertIn(fact, section)
        self.assertIn("[Skill Haste and All Skills](#skill-haste-and-all-skills)", readme)

    def test_the_release_notes_name_both_boosts(self):
        notes = (ROOT / "release-notes-v2.1.0.md").read_text(encoding="utf-8")
        new = notes.split("\n## New\n", 1)[1].split("\n## ", 1)[0]
        self.assertIn("**Skill Haste and All Skills**", new)
        self.assertIn("at most 200", new)


if __name__ == "__main__":
    unittest.main()
