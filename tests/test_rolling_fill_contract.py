"""Fill the map as you approach (`fillroll`, issue #183): the plugin's wiring.

The rule itself is driven in tests/rolling_fill_harness.cpp
(test_rolling_fill_behavior.py): with `fillroll` off the fill answers exactly
as before, and with it on only creators within the reach of the local player
are answered 0, the rest keep their native distance, and turning it off
re-arms the zone. This file pins what the harness cannot see, in
ModuleMain.cpp and the headers it includes:

- the verb is a player command and a standalone early return;
- it starts off;
- its usage line and its reply's fields;
- Hook_distance_to_object's reveal path reaches the rolling decision through
  MayPopulate, which decides the reach after readiness and before the
  admission request, and the Beacon branch is unchanged;
- the local player's position is read at most once a frame;
- densityroll's reach is infinite under the fill only while `fillroll` is off;
- the modstate JSON reports `fillRolling` and `fillRollReach`.
"""
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PLUGIN = ROOT / "plugin" / "ModuleMain.cpp"
HEADERS = ROOT / "plugin" / "include" / "ForgePact"
ROLLING = HEADERS / "RollingFill.hpp"
REVEAL = HEADERS / "MapRevealManager.hpp"


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


def _read(path: Path) -> str:
    return path.read_text(encoding="utf-8").replace("\r\n", "\n")


class RollingFillVerbTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.plugin = _read(PLUGIN)
        cls.code = _code(cls.plugin)
        cls.rolling = _code(_read(ROLLING))
        cls.command = _code(_body(cls.plugin, "static void FillRollCommand(const std::string& rest)"))

    def test_the_player_build_accepts_the_verb(self):
        start = self.plugin.index("kPlayerCommands = {")
        self.assertIn('"fillroll"', self.plugin[start:self.plugin.index("};", start)])

    def test_it_is_a_standalone_early_return(self):
        run = _body(self.plugin, "static void RunCommand(const std::string& line)")
        self.assertIn('if (lc == "fillroll") { FillRollCommand(rest); return; }', run)
        # Beside densityroll's, not inside the research-only `else if` chain.
        self.assertLess(run.index('if (lc == "fillroll")'), run.index('} else if (lc == "reveal") {'))

    def test_it_starts_off_at_the_default_reach(self):
        self.assertIn("bool m_On{ false };", self.rolling)
        self.assertIn("double m_Reach{ kDefaultReach };", self.rolling)
        self.assertIn("static constexpr double kDefaultReach = 3000.0;", self.rolling)
        self.assertIn("static constexpr double kMinReach = 1500.0;", self.rolling)
        self.assertIn("static constexpr double kMaxReach = 20000.0;", self.rolling)

    def test_the_usage_text_and_a_bad_argument_changes_nothing(self):
        self.assertIn('kUsage = "fillroll: usage fillroll 1 | 0 | <reach px, 1500-20000> | stat";', self.rolling)
        self.assertIn("ForgePact::RollingFill::Parse(arg, reach)", self.command)
        usage = self.command.index("Command::Usage")
        self.assertIn("Out(ForgePact::RollingFill::kUsage); return;", self.command[usage:usage + 120])
        # Nothing is set before the usage return.
        self.assertLess(usage, self.command.index("SetFillRolling("))

    def test_on_and_off_go_through_the_manager_and_refresh_densityroll(self):
        self.assertIn("reveal.SetFillRolling(true, reach);", self.command)
        self.assertIn("reveal.SetFillRolling(false);", self.command)
        self.assertIn("DensityRollRefresh();", self.command)

    def test_the_reply_carries_every_field(self):
        self.assertIn('std::string("fillroll: ") + (reveal.FillRolling() ? "on" : "off")', self.command)
        for field in ('" | reach "', '" | fill "', '" | answered "', '" | held back "',
                      '" | monsters "', '" | player "', '" | room "'):
            self.assertIn(field, self.command)
        self.assertIn("rolling.Answered()", self.command)
        self.assertIn("rolling.HeldBack()", self.command)
        self.assertIn('RValue("Enemy_Parent_obj")', self.command)
        self.assertIn('"instance_number"', self.command)
        self.assertIn('GetBuiltin("room_width"', self.command)
        self.assertIn('GetBuiltin("room_height"', self.command)
        self.assertIn("HhResolveLocalPlayer(player)", self.command)
        # "fill on" is the fill itself: Map Reveal on with its pack pass on.
        self.assertIn("reveal.IsEnabled() && reveal.PacksEnabled()", self.command)

    def test_the_counters_reset_when_the_zone_changes(self):
        reveal = _code(_read(REVEAL))
        self.assertEqual(reveal.count("RollingFill::Instance().ResetCounters();"), 2)


class RollingFillDetourTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.plugin = _read(PLUGIN)
        cls.hook = _code(_body(cls.plugin, "static void Hook_distance_to_object("))
        cls.reveal = _code(_read(REVEAL))
        cls.rolling = _code(_read(ROLLING))

    def test_the_reveal_path_consults_the_rolling_decision(self):
        self.assertIn("ForgePact::MapRevealManager::Instance().MayPopulate(inst)", self.hook)
        self.assertLess(self.hook.index("IsCreatorObject("), self.hook.index("MayPopulate(inst)"))
        self.assertLess(self.hook.index("MayPopulate(inst)"), self.hook.index("Result = RValue(0.0);"))
        may = _body(self.reveal, "bool MayPopulate(const RValue& creator)")
        self.assertIn("if (rollingOn && !rolling.Admits(creator)) return false;", may)
        # The reach after readiness, and before the admission request: a
        # creator held back for distance never takes an admission slot.
        self.assertLess(may.index("CreatorIsReady(creator)"), may.index("rolling.Admits(creator)"))
        self.assertLess(may.index("PopulationCapacityAvailable()"), may.index("rolling.Admits(creator)"))
        self.assertLess(may.index("rolling.Admits(creator)"), may.index("m_Admission.Request("))
        self.assertIn("if (granted && rollingOn) rolling.NoteAnswered();", may)

    def test_the_detour_decides_nothing_of_its_own_for_the_reach(self):
        # One decision, in one place: the detour reads no reach and no player
        # for the reveal path; MayPopulate holds it.
        self.assertNotIn("RollingFill", self.hook)
        self.assertNotIn("Admits(", self.hook)

    def test_the_beacon_branch_is_unchanged(self):
        self.assertIn("if (!beaconWants) return;", self.hook)
        self.assertIn("if (!ForgePact::MapRevealManager::CreatorIsReady(inst)) return;", self.hook)
        self.assertIn("if (dx * dx + dy * dy > g_BeWakeRadius * g_BeWakeRadius) return;", self.hook)
        self.assertIn("if (revealOk) ++g_RevealSpawnLies; else ++g_BeSpawnLies;", self.hook)

    def test_the_player_is_read_at_most_once_a_frame(self):
        position = _body(self.rolling, "bool PlayerPosition(double& x, double& y)")
        self.assertIn("if (!m_PlayerRead) {", position)
        self.assertIn("m_PlayerRead = true;", position)
        self.assertIn("HhResolveLocalPlayer(player)", position)
        self.assertIn("void BeginFrame() { m_PlayerRead = false; }", self.rolling)
        on_frame = _body(self.reveal, "void OnFrame(uint64_t frameCount)")
        self.assertIn("RollingFill::Instance().BeginFrame();", on_frame)
        # Before the manager's own early return, so it runs every frame.
        self.assertLess(on_frame.index("BeginFrame();"), on_frame.index("if (!m_Enabled) return;"))

    def test_no_local_player_means_the_native_answer(self):
        admits = _body(self.rolling, "bool Admits(const RValue& creator)")
        self.assertIn("if (!PlayerPosition(px, py)) return false;", admits)

    def test_turning_it_off_rearms_the_zone(self):
        set_rolling = _body(self.reveal, "void SetFillRolling(bool on, double reach = RollingFill::kDefaultReach)")
        self.assertIn("if (was && !on && m_Enabled && m_Packs) { m_PacksPending = true; m_PendingTicks = 0; }",
                      set_rolling)

    def test_the_pass_stays_armed_for_the_zone_visit(self):
        on_frame = _body(self.reveal, "void OnFrame(uint64_t frameCount)")
        self.assertIn("if (w == 1 && RollingFill::Instance().IsOn())", on_frame)


class RollingFillDensityAndStateTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.plugin = _read(PLUGIN)

    def test_densityroll_is_infinite_under_the_fill_only_while_fillroll_is_off(self):
        refresh = _code(_body(self.plugin, "static void DensityRollRefresh()"))
        self.assertIn("if (reveal.IsEnabled() && reveal.PacksEnabled() && !reveal.FillRolling()) "
                      "reach = std::numeric_limits<double>::infinity();", refresh)
        self.assertNotIn("if (reveal.IsEnabled() && reveal.PacksEnabled()) reach", refresh)

    def test_mod_state_reports_it(self):
        self.assertIn('\\"fillRolling\\":', self.plugin)
        self.assertIn('\\"fillRollReach\\":', self.plugin)
        self.assertIn("reveal.FillRolling() ? \"true\" : \"false\"", self.plugin)


if __name__ == "__main__":
    unittest.main()
