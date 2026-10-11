"""Always show Chaos Pillar mods (`pillartips`): the plugin wiring.

tests/test_chaos_pillar_tips_behavior.py runs the real decision core,
plugin/include/ForgePact/ChaosPillarTips.hpp, against controlled frames. This
file pins what that harness cannot see, on comment-stripped ModuleMain.cpp
(and, for the player build, with the research blocks evaluated away):

- `pillartips` is a player command with its own standalone early return in
  RunCommand (MSVC's C1061 nesting limit), and the research instrument
  `pillarprobe` and its symbols never reach the player build;
- the tick is one call line in Hook_DrawHudBuffs, directly after
  `DungeonChestDraw();`, outside every research block, and the adapter
  installs no hook of its own (no HookOneScript, no MmCreateHook);
- both objects are named through the SDK's GetObjectName, never a literal
  name or index;
- every builtin the adapter (and the probe) calls is in a listed set, so a new
  one is a reviewed change, and the writes are `visible` on the tooltip alone,
  each behind instance_exists;
- the stat line's fields come in the order the workorder fixed, and the
  modstate object carries `enabled` and `shown`;
- off with nothing shown, the tick returns before any builtin call (the all-off
  baseline).
"""
import re
import sys
import unittest
from pathlib import Path

TESTS_DIR = Path(__file__).resolve().parent
ROOT = TESTS_DIR.parent
PLUGIN = ROOT / "plugin" / "ModuleMain.cpp"
HEADER = ROOT / "plugin" / "include" / "ForgePact" / "ChaosPillarTips.hpp"

if str(TESTS_DIR) not in sys.path:
    sys.path.insert(0, str(TESTS_DIR))

from test_release_hook_contract import function_body, strip_comments, strip_research_blocks  # noqa: E402

ADAPTER_START = "// ---- Always show Chaos Pillar mods (ChaosPillarTips.hpp): the adapter"
ADAPTER_END = "// ---- end of the Chaos Pillar mods adapter"
PROBE_START = "// ---- pillarprobe: the Chaos Pillar tooltip research instrument (ForgePact #7)"
PROBE_END = "#endif // FORGEPACT_RELEASE (pillarprobe)"

TICK_LINE = "    PillarTipsTick();"
HUD_SIGNATURE = "static RValue& Hook_DrawHudBuffs("

# The builtins the shipped adapter may call, by name: the object lookups, the
# census, the view, and the point-of-use check and the write. The room is read
# through CurrentRoomKey's built-in variable, which is no builtin call.
ADAPTER_BUILTINS = {
    "asset_get_index", "instance_number", "instance_find", "instance_exists",
    "variable_instance_get", "variable_instance_set",
    "view_get_camera", "camera_get_view_x", "camera_get_view_y", "camera_get_view_width", "camera_get_view_height",
}
# The research probe adds the window size and the tooltip's layer name.
PROBE_BUILTINS = ADAPTER_BUILTINS | {"window_get_width", "window_get_height", "layer_get_name"}

# `pillartips: <on|off>` then these fields, in this order (ctx "### Names,
# commands and strings").
STAT_FIELDS = ["pillars", "onscreen", "shown", "activated", "unreadable", "notooltip", "showWrites", "hideWrites",
               "ticks", "hudCalls"]
MODSTATE_FIELDS = ["enabled", "shown"]

# Names the research build alone may carry.
RESEARCH_ONLY_NAMES = ("pillarprobe", "PillarProbeCommand", "PillarProbeList", "PillarProbeDevice", "PtProbeFlag",
                       "PtProbeNum", "PtProbeGet")


def builtins_called(code):
    return set(re.findall(r'CallBuiltin\(\s*"(\w+)"', code))


class ChaosPillarTipsWiringTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.plugin = PLUGIN.read_text(encoding="utf-8").replace("\r\n", "\n")
        cls.code = strip_comments(cls.plugin)
        cls.player = strip_comments(strip_research_blocks(cls.plugin))
        start = cls.plugin.find(ADAPTER_START)
        end = cls.plugin.find(ADAPTER_END)
        if start < 0 or end < start:
            raise AssertionError("the Chaos Pillar adapter's start and end markers are missing from ModuleMain.cpp")
        cls.adapter = strip_comments(cls.plugin[start:end])
        pstart = cls.plugin.find(PROBE_START)
        pend = cls.plugin.find(PROBE_END)
        if pstart < 0 or pend < pstart:
            raise AssertionError("the pillarprobe block's start and end markers are missing from ModuleMain.cpp")
        cls.probe = strip_comments(cls.plugin[pstart:pend])
        cls.header = strip_comments(HEADER.read_text(encoding="utf-8").replace("\r\n", "\n"))

    # ---- the commands ------------------------------------------------------

    def player_commands(self):
        run = function_body(self.plugin, "static void RunCommand(const std::string& line)")
        block = run[run.index("kPlayerCommands = {"):]
        return block[:block.index("};")]

    def test_pillartips_is_a_player_command_with_its_own_early_return(self):
        commands = self.player_commands()
        self.assertIn('"lootann"', commands)   # the set read is the real one, not an empty slice
        self.assertIn('"pillartips"', commands)
        run = function_body(self.player, "static void RunCommand(const std::string& line)")
        self.assertIn('if (lc == "pillartips") { PillarTipsCommand(rest); return; }', run)
        self.assertNotIn('else if (lc == "pillartips")', run)
        self.assertIn("static void PillarTipsCommand(", self.player)

    def test_pillarprobe_is_research_build_only(self):
        self.assertNotIn('"pillarprobe"', self.player_commands())
        # Positive control: the research build dispatches it, standalone.
        run = function_body(self.code, "static void RunCommand(const std::string& line)")
        self.assertIn('if (lc == "pillarprobe") { PillarProbeCommand(rest); return; }', run)
        for name in RESEARCH_ONLY_NAMES:
            self.assertRegex(self.code, rf"\b{name}\b", name)
            self.assertNotRegex(self.player, rf"\b{name}\b", name + " reaches the player build")
        # Negative control: the strip keeps player code.
        self.assertIn("static void PillarTipsTick()", self.player)
        self.assertTrue(self.plugin[:self.plugin.index(PROBE_START)].rstrip().endswith("#ifndef FORGEPACT_RELEASE"))

    def test_every_form_prints_the_stat_line_and_zero_is_off(self):
        body = function_body(self.code, "static void PillarTipsCommand(const std::string& rest)")
        self.assertIn('arg == "1" || arg == "on"', body)
        self.assertIn('arg == "0" || arg == "off"', body)
        self.assertIn("PtApply(g_PillarTips.SetEnabled(true));", body)
        self.assertIn("PtApply(g_PillarTips.SetEnabled(false));", body)
        self.assertIn("Out(g_PillarTips.StatLine(g_HhHudCalls));", body)

    # ---- the hook site -----------------------------------------------------

    def test_the_tick_runs_right_after_the_dungeon_chest_draw_outside_every_research_block(self):
        body = function_body(self.plugin, HUD_SIGNATURE)
        lines = body.split("\n")
        self.assertEqual(lines.count(TICK_LINE), 1, body)
        at = lines.index(TICK_LINE)
        self.assertEqual(lines[at - 1], "    DungeonChestDraw();", body)
        # Outside every conditional block: no #if is open at the call line.
        depth = 0
        for line in lines[:at]:
            stripped = line.strip()
            if stripped.startswith("#if"):
                depth += 1
            elif stripped.startswith("#endif"):
                depth -= 1
        self.assertEqual(depth, 0, "PillarTipsTick() sits inside a conditional block")
        # And the player build keeps it.
        self.assertIn("PillarTipsTick();", function_body(self.player, HUD_SIGNATURE))
        # The hook keeps running under the hudlabels incident scope.
        self.assertIn("IncidentScope incidentScope(IncidentMod::hudlabels);", body)

    def test_the_adapter_installs_no_hook(self):
        for code in (self.adapter, self.probe):
            for installer in ("HookOneScript", "MmCreateHook", "HookBuiltin", "InstallScriptHook", "HookRawNamedRoutine"):
                self.assertNotIn(installer, code, installer)
        # Negative control: the same scan sees an installer where there is one.
        self.assertIn("HookOneScript", strip_comments(function_body(self.plugin, "static void InstallHeadLabelHook()")))
        # No IncidentMonitor entry of its own: it runs under `hudlabels`.
        self.assertNotIn("IncidentScope", self.adapter)

    # ---- names, builtins and writes ---------------------------------------

    def test_both_objects_are_named_through_the_sdk(self):
        for code in (self.adapter, self.probe):
            self.assertIn("GetObjectName(HeroSiege::Objects::GameObject::Chaos_Pillar_obj)", code)
        self.assertIn("GetObjectName(HeroSiege::Objects::GameObject::Chaos_Pillar_Tooltip_obj)", self.adapter)
        self.assertIn("GetObjectName(HeroSiege::Objects::GameObject::Chaos_Pillar_Tooltip_obj)", self.probe)
        for code in (self.adapter, self.probe):
            self.assertNotRegex(code, r'"Chaos_Pillar(?:_Tooltip)?_obj"')
            self.assertNotRegex(code, r"\b86[12]\b")
        # Each object index comes from asset_get_index, the lootcensus way.
        self.assertIn('CallBuiltin("asset_get_index"', self.adapter)

    def test_every_builtin_the_adapter_calls_is_listed(self):
        adapter = builtins_called(self.adapter)
        probe = builtins_called(self.probe)
        # Positive control: the scan finds the calls that must be there.
        self.assertTrue({"asset_get_index", "instance_exists", "variable_instance_set", "view_get_camera"} <= adapter)
        self.assertEqual(adapter - ADAPTER_BUILTINS, set())
        self.assertEqual(probe - PROBE_BUILTINS, set())
        # Negative control: an unlisted builtin would be caught.
        self.assertEqual(builtins_called('g_Yytk->CallBuiltin("instance_destroy", {})') - ADAPTER_BUILTINS,
                         {"instance_destroy"})

    def test_the_only_write_is_visible_behind_instance_exists(self):
        sets = re.findall(r'CallBuiltin\(\s*"variable_instance_set"\s*,\s*\{\s*\w+\s*,\s*RValue\("(\w+)"\)', self.adapter)
        self.assertEqual(sets, ["visible"])
        apply = function_body(self.adapter, "static void PtApply(")
        self.assertLess(apply.index('"instance_exists"'), apply.index('"variable_instance_set"'))
        self.assertIn("g_PillarTips.NoteWriteRefused();", apply)
        # The probe's one write is the pillar's `device`, research build only.
        probe_sets = re.findall(r'CallBuiltin\(\s*"variable_instance_set"\s*,\s*\{\s*\w+\s*,\s*RValue\("(\w+)"\)', self.probe)
        self.assertEqual(probe_sets, ["device"])

    def test_instances_are_kept_as_ids_and_accepted_by_the_handle_rule(self):
        census = function_body(self.adapter, "static bool PtTakeCensus(")
        self.assertIn("HeroSiege::Player::IsInstanceHandle(inst)", census)
        self.assertNotIn("m_Kind == VALUE_OBJECT", self.adapter)
        self.assertNotIn("CInstance*", self.adapter)
        self.assertIn("InstanceIdOf(inst)", census)
        # The room is read through the built-in, never as a global.
        self.assertIn("CurrentRoomKey()", function_body(self.adapter, "static void PillarTipsTick()"))
        self.assertNotIn("variable_global_get", self.adapter)

    # ---- the lines ----------------------------------------------------------

    def test_the_stat_line_fields_come_in_order(self):
        stat = function_body(self.header, "std::string StatLine(long long hudCalls) const")
        self.assertIn('"pillartips: on"', stat)
        self.assertIn('"pillartips: off"', stat)
        self.assertEqual(re.findall(r'" (\w+)="', stat), STAT_FIELDS)

    def test_the_modstate_object(self):
        flush = strip_comments(function_body(self.plugin, "static void FlushModState("))
        at = flush.index('\\"pillarTips\\":{')
        tail = flush[at:at + 400]
        self.assertEqual(re.findall(r'\\"(\w+)\\":', tail)[:3], ["pillarTips"] + MODSTATE_FIELDS)
        self.assertIn("g_PillarTips.Enabled()", tail)
        self.assertIn("g_PillarTips.Shown()", tail)

    # ---- the all-off baseline ----------------------------------------------

    def test_off_with_nothing_shown_the_tick_returns_before_any_builtin(self):
        tick = function_body(self.adapter, "static void PillarTipsTick()")
        first = tick.strip().split("\n")[0].strip()
        self.assertEqual(first, "if (!g_PillarTips.Active()) return;")
        self.assertNotIn("CallBuiltin", tick[:tick.index(first) + len(first)])
        active = function_body(self.header, "bool Active() const")
        self.assertIn("return m_Enabled || !m_Shown.empty();", active)
        # And the core starts off.
        self.assertIn("bool m_Enabled = false;", self.header)
        # Negative control: a tick that read the room first would fail the check.
        bad = "    const int64_t room = CurrentRoomKey();\n    if (!g_PillarTips.Active()) return;"
        self.assertNotEqual(bad.strip().split("\n")[0].strip(), "if (!g_PillarTips.Active()) return;")


if __name__ == "__main__":
    unittest.main()
