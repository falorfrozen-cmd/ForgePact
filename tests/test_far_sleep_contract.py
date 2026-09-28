"""Far sleep (`farsleep`): the plugin wiring and the rules the class must keep.

tests/test_far_sleep_behavior.py runs the real class against a controlled
runner. This file pins what that harness cannot see: the verb is a player
command with its own early return, the frame callback runs the tick only
after setup and the tick costs nothing while the switch is off, the class
starts off and reaches the game only through CallBuiltin (no game addresses,
no struct layouts), which families it may touch and which it never does,
which rooms it leaves alone, and that the research-only parts stay out of the
player build.
"""
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PLUGIN = ROOT / "plugin" / "ModuleMain.cpp"
HEADER = ROOT / "plugin" / "include" / "ForgePact" / "FarSleep.hpp"


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


def _research_blocks(source: str) -> list:
    """The spans between `#ifndef FORGEPACT_RELEASE` and its `#endif`."""
    spans = []
    for match in re.finditer(r"^#ifndef FORGEPACT_RELEASE$", source, re.M):
        depth, pos = 1, match.end()
        for directive in re.finditer(r"^#(if|ifdef|ifndef|endif)\b", source[pos:], re.M):
            depth += -1 if directive.group(1) == "endif" else 1
            if depth == 0:
                spans.append(source[match.end():pos + directive.start()])
                break
    return spans


class FarSleepPluginWiringTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.plugin = PLUGIN.read_text(encoding="utf-8").replace("\r\n", "\n")

    def test_the_player_build_accepts_the_verb(self):
        start = self.plugin.index("kPlayerCommands = {")
        block = self.plugin[start:self.plugin.index("};", start)]
        self.assertIn('"farsleep"', block)

    def test_the_verb_is_a_standalone_early_return(self):
        run = _body(self.plugin, "static void RunCommand(const std::string& line)")
        self.assertIn('if (lc == "farsleep") { FarSleepCommand(rest); return; }', run)

    def test_the_tick_runs_after_setup_once_a_frame(self):
        frame = _code(_body(self.plugin, "void FrameCallback(FWFrame& FrameContext)"))
        self.assertEqual(frame.count("FarSleepTick();"), 1)
        self.assertIn("if (g_Setup) FarSleepTick();", frame)

    def test_the_tick_costs_nothing_while_off(self):
        tick = [l.strip() for l in _code(_body(self.plugin, "static void FarSleepTick()")).splitlines() if l.strip()]
        self.assertEqual(tick[0], "auto& fs = ForgePact::FarSleep::Instance();")
        self.assertEqual(tick[1], "if (!fs.Enabled() && !fs.Draining()) return;")

    def test_the_switch_and_its_report(self):
        command = _code(_body(self.plugin, "static void FarSleepCommand(const std::string& rest)"))
        self.assertIn('if (arg == "1" || arg == "on")', command)
        self.assertIn("fs.SetFrameEventOwner(&FarSleepOwnsFrameEvent);", command)
        self.assertIn('else if (arg == "0" || arg == "off")', command)
        self.assertIn("FarSleepStatus();", command)

    def test_the_research_parts_stay_out_of_the_player_build(self):
        research = "\n".join(_research_blocks(self.plugin))
        self.assertIn('arg == "ids plain"', research)
        self.assertIn('if (lc == "zonecensus") { ZoneCensusCommand(rest); return; }', research)
        self.assertIn('if (lc == "evcount") { EvCountCommand(rest); return; }', research)
        self.assertIn("static void ZoneCensusCommand(const std::string& rest)", research)
        self.assertIn("static void EvCountCommand(const std::string& rest)", research)

    def test_objects_with_per_frame_code_are_read_from_the_code_table(self):
        owners = _code(_body(self.plugin, "static void FarSleepReadFrameOwners()"))
        for suffix in ("_Step_0", "_Step_1", "_Step_2", "_Draw_64", "_Draw_74", "_Draw_75"):
            self.assertIn(f'"{suffix}"', owners)
        self.assertIn("FrameProfGmlAnchor()", owners)

    def test_the_room_probe_reads_name_and_persistence(self):
        probe = _code(_body(self.plugin, "static ForgePact::FarSleep::RoomInfo FarSleepRoomInfo()"))
        self.assertIn('"room_persistent"', probe)
        self.assertIn('"room_get_name"', probe)

    def test_mod_state_reports_it(self):
        self.assertIn('\\"farSleep\\":{\\"enabled\\":', self.plugin)


class FarSleepClassRulesTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.header = HEADER.read_text(encoding="utf-8").replace("\r\n", "\n")
        cls.code = _code(cls.header)

    def test_it_starts_off(self):
        self.assertIn("bool m_Enabled = false;", self.code)

    def test_it_reaches_the_game_only_through_call_builtin(self):
        calls = re.findall(r"g_Yytk->(\w+)", self.code)
        self.assertTrue(calls)
        self.assertEqual(set(calls), {"CallBuiltin"})
        for forbidden in ("GetInstanceObject", "GetMembers", "reinterpret_cast", "m_Functions", "GetBuiltin",
                          "Hero_Siege.exe", "0x14"):
            self.assertNotIn(forbidden, self.code, forbidden)

    def test_only_the_scenery_families(self):
        families = re.search(r"kFamilies\[\] = \{(.*?)\};", self.code, re.S).group(1)
        self.assertEqual(re.findall(r'"(\w+)"', families),
                         ["Visual_Parent_obj", "Destructible_NoCollision_Parent_obj", "Collision_Prop_obj"])

    def test_the_parents_it_never_touches(self):
        denied = re.search(r"kDenied\[\] = \{(.*?)\};", self.code, re.S).group(1)
        self.assertEqual(set(re.findall(r'"(\w+)"', denied)), {
            "Shrine_Parent_obj", "Special_Dungeon_Parent_obj", "Pile_Parent_obj", "Chest_Parent_obj",
            "Quest_Object_Parent_obj", "Enemy_Parent_obj", "Wall_Parent_obj", "Block_obj"})
        self.assertIn('name.rfind("Trap_", 0) == 0', self.code)
        self.assertIn("m_FrameEventOwner(name)", self.code)

    def test_the_rooms_it_leaves_alone(self):
        allowed = _body(self.code, "static bool ZoneAllowed(const RoomInfo& room)")
        self.assertIn("room.persistent", allowed)
        self.assertIn('room.name.rfind("Town", 0) == 0', allowed)
        self.assertIn('room.name.find("Menu")', allowed)
        self.assertIn('room.name.rfind("Dev_", 0) == 0', allowed)

    def test_the_calls_it_makes_are_the_documented_ones(self):
        names = set(re.findall(r'Call\("(\w+)"', self.code))
        self.assertEqual(names, {
            "asset_get_index", "object_exists", "object_get_parent", "object_get_name",
            "instance_number", "instance_find", "variable_instance_get",
            "instance_activate_object", "instance_deactivate_object", "instance_exists",
            "view_get_camera", "camera_get_view_width", "camera_get_view_height"})

    def test_a_player_jump_gets_the_bigger_budget_and_off_wakes_everything(self):
        normal = int(re.search(r"kCallsPerFrame = (\d+);", self.code).group(1))
        urgent = int(re.search(r"kUrgentCallsPerFrame = (\d+);", self.code).group(1))
        self.assertLess(normal, urgent)
        self.assertIn("m_Urgent = jumped;", self.code)
        drain = _body(self.code, "void Drain()")
        self.assertIn('Call("instance_activate_object"', drain)


if __name__ == "__main__":
    unittest.main()
