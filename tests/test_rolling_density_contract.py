"""Rolling density copies (`densityroll`): the wiring and the rules.

The behaviour lives in two harnesses: tests/adaptive_population.cpp runs the
real DeferredDensityCopies with a reach (jobs out of reach wait, are neither
taken nor dropped, and become due as the player comes near), and
tests/density_population_harness.cpp runs the production DensityCopiesTick
with a reach against a controlled runner (test_adaptive_population.py); the
pack markers' side is tests/pack_markers_harness.cpp (`copy/...`). This file
pins what those cannot see: the switch is a player command that starts off,
the reach follows the map-fill and hunt settings, the budget sees only the
due copies, and the panel sends `densityroll 1|0`.
"""
import re
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PLUGIN = ROOT / "plugin" / "ModuleMain.cpp"
QUEUE = ROOT / "plugin" / "include" / "ForgePact" / "DeferredDensityCopies.hpp"
MARKERS = ROOT / "plugin" / "include" / "ForgePact" / "PackMarkers.hpp"
PANEL = ROOT / "src" / "forgepact.py"
README = ROOT / "README.md"
# forgepact-notes-cleanup.yml deletes release-notes-v*.md from main once that
# version is published (the release page keeps the text); the notes check
# reads them while they exist.
NOTES = ROOT / "release-notes-v2.1.0.md"


def _body(source: str, signature: str) -> str:
    start = source.rfind(signature)
    if start < 0:
        raise AssertionError(f"{signature} not found")
    brace = source.find("{", start)
    depth = 0
    for index in range(brace, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[brace + 1:index]
    raise AssertionError(f"unterminated body for {signature}")


def _code(source: str) -> str:
    source = re.sub(r"/\*.*?\*/", "", source, flags=re.S)
    return re.sub(r"//[^\n]*", "", source)


class RollingDensityPluginTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.plugin = PLUGIN.read_text(encoding="utf-8").replace("\r\n", "\n")
        cls.code = _code(cls.plugin)

    def test_the_player_build_accepts_the_verb(self):
        start = self.plugin.index("kPlayerCommands = {")
        self.assertIn('"densityroll"', self.plugin[start:self.plugin.index("};", start)])
        run = _body(self.plugin, "static void RunCommand(const std::string& line)")
        self.assertIn('if (lc == "densityroll") { DensityRollCommand(rest); return; }', run)

    def test_it_starts_off(self):
        self.assertIn("static double g_DensityRollReach=0.0;", self.code)
        self.assertIn("static double g_DensityReachNow=std::numeric_limits<double>::infinity();", self.code)
        self.assertIn("static bool DensityRolling(){return std::isfinite(g_DensityReachNow);}", self.code)

    def test_the_budget_sees_only_the_due_copies(self):
        self.assertIn("static size_t DeferredDensityPending(){return DensityRolling()?g_DensityDue:g_DensityCopies.Pending();}",
                      self.code)
        tick = _code(_body(self.plugin, "static void DensityCopiesTick()"))
        self.assertIn("g_DensityCopies.TakeNearest(x,y,g_RuntimeFrame,g_DensityReachNow)", tick)
        self.assertIn("g_DensityDue=DensityRolling()?g_DensityCopies.DueWithin(x,y,g_DensityReachNow):g_DensityCopies.Pending();",
                      tick)
        self.assertIn("if(!g_DensityCopies.Pending()){g_DensityDue=0;return;}", tick)

    def test_the_markers_are_told_only_while_rolling(self):
        tick = _code(_body(self.plugin, "static void DensityCopiesTick()"))
        self.assertIn("if(DensityRolling() && g_DensityCopyMade)g_DensityCopyMade(", tick)
        self.assertIn("ForgePact::PackMarkers::Instance().NoteCopy(objectIndex);", self.code)

    def test_the_reach_follows_map_fill_and_the_hunt(self):
        refresh = _code(_body(self.plugin, "static void DensityRollRefresh()"))
        self.assertIn("if (reveal.IsEnabled() && reveal.PacksEnabled()) reach = std::numeric_limits<double>::infinity();",
                      refresh)
        # Any hunt (Beacon or Tyrant's Crown) keeps monsters within the wake
        # radius hunting, so the reach covers it; a whole-map hunt, or
        # `beaconspawn` with the radius off, needs every copy.
        self.assertIn("else if (HuntPolicy() != 0) {", refresh)
        self.assertIn("if (g_BeWakeRadius < 0.0 || (g_BeWakeRadius == 0.0 && g_BeSpawnNear && BeaconActive()))", refresh)
        self.assertIn("else if (g_BeWakeRadius > 0.0) reach = (std::max)(reach, g_BeWakeRadius + 500.0);", refresh)
        self.assertNotIn("PacksEnabled() ||", refresh)
        self.assertNotIn("MarksEnabled", refresh)   # pack markers do not need the copies
        frame = _code(_body(self.plugin, "void FrameCallback(FWFrame& FrameContext)"))
        self.assertIn("if (g_Setup && (g_RuntimeFrame % 60) == 0) DensityRollRefresh();", frame)

    def test_the_command_takes_a_sane_reach(self):
        command = _code(_body(self.plugin, "static void DensityRollCommand(const std::string& rest)"))
        self.assertIn("g_DensityRollReach = kDensityRollDefaultPx;", command)
        self.assertIn("if (!(v >= 1500.0 && v <= 20000.0))", command)
        self.assertIn("static constexpr double kDensityRollDefaultPx=3000.0;", self.code)

    def test_mod_state_reports_it(self):
        self.assertIn('\\"deferredDensityCopies\\":', self.plugin)
        self.assertIn('\\"densityRollReach\\":', self.plugin)


class RollingDensityQueueTests(unittest.TestCase):
    def test_the_queue_waits_rather_than_drops(self):
        queue = _code(QUEUE.read_text(encoding="utf-8"))
        self.assertIn("double reach=std::numeric_limits<double>::infinity()", queue)
        self.assertIn("if(m_Jobs.front().distance>reach*reach)return std::nullopt;", queue)
        self.assertIn("size_t DueWithin(double x,double y,double reach)const", queue)

    def test_the_markers_count_a_copy_into_its_family(self):
        markers = _code(MARKERS.read_text(encoding="utf-8"))
        note = _body(markers, "void NoteCopy(int objectIndex)")
        self.assertIn("++m_FamilyPeak[k]", note)


class RollingDensityPanelTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if str(PANEL.parent) not in sys.path:
            sys.path.insert(0, str(PANEL.parent))
        import forgepact  # noqa: E402 - the panel, imported only here
        cls.forgepact = forgepact
        cls.source = PANEL.read_text(encoding="utf-8").replace("\r\n", "\n")

    def test_off_by_default_and_sent_only_when_on(self):
        self.assertIs(self.forgepact.DEFAULTS["density_rolling"], False)
        cfg = dict(self.forgepact.DEFAULTS)
        self.assertNotIn("densityroll 1", self.forgepact.build_cmds(cfg))
        cfg["density_rolling"] = True
        self.assertIn("densityroll 1", self.forgepact.build_cmds(cfg))

    def test_the_live_switch_sends_one_or_zero(self):
        self.assertIn("""send_cmds([f"densityroll {1 if cfg['density_rolling'] else 0}"], cfg)""", self.source)
        # One of /api/set's live booleans (its place in that tuple is free).
        self.assertRegex(self.source, r'elif key in \("density_on", "auto_apply", [^)]*"density_rolling", ')


class RollingDensityPlayerTextTests(unittest.TestCase):
    """The player-facing text names the switch the panel shows and says it
    starts off, the Beacon/Tyrant and fill-the-map exceptions included."""

    def test_the_readme_has_the_row_and_the_section(self):
        readme = README.read_text(encoding="utf-8").replace("\r\n", "\n")
        self.assertIn("| **Extra Packs As You Approach** | Mods → Quality of Life, off by default;", readme)
        self.assertIn("## Extra packs as you approach (lighter frames at high density)", readme)
        self.assertIn("(#extra-packs-as-you-approach-lighter-frames-at-high-density)", readme)
        section = readme[readme.index("## Extra packs as you approach"):]
        section = section[:section.index("\n## ", 1)]
        for phrase in ("`densityroll 1|0`", "Off by default", "Really spawn every pack on",
                       "Beacon or Tyrant's Crown"):
            self.assertIn(phrase, section)

    def test_the_release_notes_while_they_exist(self):
        if not NOTES.is_file():
            self.skipTest("release notes already published and removed from main")
        notes = NOTES.read_text(encoding="utf-8").replace("\r\n", "\n")
        self.assertTrue(notes.startswith("# ForgePact 2.1.0\n"))
        self.assertIn("- **Extra packs as you approach.** A new switch in Mods → Quality of Life, off\n  by default;",
                      notes)
        self.assertIn("press **Install Mod Plugin** once after updating", notes)


if __name__ == "__main__":
    unittest.main()
