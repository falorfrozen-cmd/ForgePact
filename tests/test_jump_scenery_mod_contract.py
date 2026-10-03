"""Jump through scenery (`jumpscenery`): the plugin wiring the harness cannot see.

tests/test_jump_scenery_mod_behavior.py runs the decision core
(plugin/include/ForgePact/JumpScenery.hpp) against a controlled world. This
file reads ModuleMain.cpp as text, comments stripped, and pins the adapter
around it: the verb is a player command with its own early return, its six
hooks go in only on `jumpscenery 1` and each is resolved by name (skillsLeap
through its SDK constant and HookOneScript, the five builtins through
HookBuiltin), the gates and locks are named through the SDK, every detour and
the frame tick return the game's own answer at once while the mod is off, the
research build's citrace and jumpprobe and this mod refuse each other's
builtins, the mod state carries the switch, `stat` names every counter, and no
address is computed anywhere in it.
"""
import re
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from test_release_hook_contract import function_body, strip_comments, strip_research_blocks  # noqa: E402

ROOT = Path(__file__).resolve().parents[1]
PLUGIN = ROOT / "plugin" / "ModuleMain.cpp"
HEADER = ROOT / "plugin" / "include" / "ForgePact" / "JumpScenery.hpp"
SDK_OBJECTS = ROOT.parent / "hs-game-sdk" / "cpp" / "include" / "hs_game_sdk" / "objects.hpp"

BLOCK_START = "// ---- Jump through scenery (JumpScenery.hpp): the adapter"
BLOCK_END = "// ---- end of the jump through scenery adapter"

# The five builtins whose answers crossed a jump in Live 1's J3.
BUILTINS = {"position_meeting", "place_meeting", "instance_position", "collision_line", "collision_circle"}

# ctx "### Player surface": the stat line's fields, in order.
STAT_FIELDS = ("reach=", "jumps=", "granted=", "answered=", "refused-landing=", "refused-room=", "refused-no-reach=",
               "no-direction=", "landed-inside=", "before-open=", "excluded=", "room=")

SIGNATURES = {
    "command": "static void JumpSceneryCommand(const std::string& rest)",
    "install": "static std::string JumpSceneryInstall()",
    "tick": "static void JumpSceneryTick()",
    "builtin": "static void JsOnBuiltin(",
    "leap": "static RValue& JsHookLeap(",
    "excluded": "static bool JsExcludedBlocks(",
    "family_at": "static bool JsFamilyAt(",
    "room": "static bool JsRoomSize(",
    "player": "static void JsRefreshPlayer()",
    "answer": "static RValue JsAnswerValue(",
    "holders": "static std::string JumpSceneryResearchHolders()",
    "held": "static std::string JumpSceneryHeldHooks()",
}


def sdk_object_index(name):
    if not SDK_OBJECTS.exists():
        raise unittest.SkipTest(f"hs-game-sdk header not found at {SDK_OBJECTS}")
    match = re.search(r"\b" + name + r" = (\d+),", SDK_OBJECTS.read_text(encoding="utf-8"))
    assert match, name
    return match.group(1)


class JumpSceneryModContract(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.plugin = PLUGIN.read_text(encoding="utf-8").replace("\r\n", "\n")
        cls.block = cls.plugin[cls.plugin.index(BLOCK_START):cls.plugin.index(BLOCK_END)]
        cls.code = strip_comments(cls.block)
        cls.shipped = strip_comments(strip_research_blocks(cls.plugin))
        cls.shipped_block = strip_comments(strip_research_blocks(cls.block))
        cls.header = strip_comments(HEADER.read_text(encoding="utf-8").replace("\r\n", "\n"))

    def body(self, which, source=None):
        return strip_comments(function_body(source if source is not None else self.plugin, SIGNATURES[which]))

    def lines(self, which):
        return [line.strip() for line in self.body(which).split("\n") if line.strip()]

    # ---- the verb ---------------------------------------------------------------

    def test_the_player_build_accepts_the_verb(self):
        allowlist = re.search(r"kPlayerCommands\s*=\s*\{(?P<body>.*?)\};", self.plugin, re.S).group("body")
        self.assertIn('"jumpscenery"', allowlist)
        self.assertIn('"farsleep"', allowlist)   # the set read is the real one

    def test_the_verb_is_a_standalone_early_return_beside_farsleep_in_both_builds(self):
        dispatch = 'if (lc == "jumpscenery") { JumpSceneryCommand(rest); return; }'
        for source in (self.plugin, strip_research_blocks(self.plugin)):
            run = function_body(source, "static void RunCommand(const std::string& line)")
            self.assertIn(dispatch, run)
            self.assertLess(run.index('if (lc == "farsleep") { FarSleepCommand(rest); return; }'), run.index(dispatch))
            self.assertNotIn('else if (lc == "jumpscenery")', run)
        self.assertEqual(self.plugin.count('lc == "jumpscenery"'), 1)
        self.assertIn("static void JumpSceneryCommand(const std::string& rest)", self.shipped)

    def test_the_answers_the_live_procedure_reads(self):
        command = self.body("command")
        self.assertIn('if (arg == "1" || arg == "on")', command)
        self.assertIn('if (arg == "0" || arg == "off")', command)
        self.assertIn('if (arg.empty() || arg == "stat") { Out(g_JumpScenery.StatLine()); return; }', command)
        self.assertIn('Out("jumpscenery: refused - " + why);', command)
        self.assertIn('Out("jumpscenery: usage jumpscenery 1 | 0 | stat");', command)
        self.assertIn('return enabled_ ? "jumpscenery: on" : "jumpscenery: off";', self.header)
        stat = function_body(self.header, "std::string StatLine() const")
        positions = [stat.index('" ' + field) for field in STAT_FIELDS]
        self.assertEqual(positions, sorted(positions), "the stat fields are out of order")
        self.assertIn('" room=unknown"', stat)

    def test_mod_state_reports_it(self):
        self.assertIn('\\"jumpScenery\\":{\\"enabled\\":', self.shipped)
        self.assertIn("g_JumpScenery.Enabled()", self.shipped)

    # ---- the hooks ---------------------------------------------------------------

    def test_the_hooks_go_in_only_on_jumpscenery_1(self):
        self.assertEqual(self.shipped.count("JumpSceneryInstall()"), 2, "the definition and one call")
        command = self.body("command")
        on = command[command.index('if (arg == "1" || arg == "on")'):command.index('if (arg == "0" || arg == "off")')]
        self.assertIn("const std::string why = JumpSceneryInstall();", on)
        self.assertLess(on.index("JumpSceneryInstall();"), on.index("g_JumpScenery.SetEnabled(true);"))
        install = self.body("install")
        for call in ("HookBuiltin(", "HookOneScript("):
            self.assertEqual(self.code.count(call), install.count(call), call + " outside JumpSceneryInstall")
        self.assertNotIn("JumpSceneryInstall", self.body("tick"))
        self.assertNotIn("JumpSceneryInstall", strip_comments(function_body(self.plugin, "void FrameCallback(FWFrame& FrameContext)")))

    def test_skills_leap_through_its_sdk_constant_and_hook_one_script(self):
        install = self.body("install")
        self.assertIn("HookOneScript(SdkShortScriptName(HeroSiege::Scripts::gml_Script_skillsLeap), \"fp_jumpscenery_leap\","
                      " (PVOID)JsHookLeap, &g_JsOrigLeap, &native)", install)
        self.assertNotIn('"skillsLeap"', self.code)
        self.assertNotIn('"gml_Script_skillsLeap"', self.code)
        self.assertNotIn("HookOneScriptTable", self.code)
        self.assertNotIn("MmCreateHook(", self.code)
        # A table-only skillsLeap would never open the window: the mod refuses instead.
        self.assertIn("if (!g_JsLeapNative)", install)

    def test_the_five_builtins_by_name_and_no_other(self):
        names = set(re.findall(r'\{\s*"(\w+)",\s*\d+,\s*Answer::\w+\s*\}', self.header))
        self.assertEqual(names, BUILTINS)
        install = self.body("install")
        self.assertEqual(self.code.count("HookBuiltin("), 1)
        self.assertIn("HookBuiltin(name, kJsBuiltinHookIds[i], kJsBuiltinDetours[i], &g_JsOrig[i])", install)
        self.assertIn("const char* name = JsNs::kBuiltins[i].name.data();", install)
        self.assertIn("for (int i = 0; i < JsNs::kBuiltinCount; ++i)", install)
        self.assertLess(install.index("if (!AddrIsExecutableInModule(GetModuleHandleA(nullptr), p))"),
                        install.index("HookBuiltin("))
        for name in BUILTINS:
            self.assertNotIn('HookBuiltin("' + name + '"', self.code)
        self.assertEqual(len(re.findall(r'"fp_jumpscenery_\w+"', self.code)), 6, "one hook id per hook")

    def test_family_and_exclusions_through_the_sdk_with_no_raw_index(self):
        self.assertIn("static constexpr HeroSiege::Objects::GameObject kJsFamily ="
                      " HeroSiege::Objects::GameObject::Collision_Parent_obj;", self.code)
        excluded = re.search(r"kJsExcluded\[\] = \{(.*?)\};", self.code, re.S).group(1)
        self.assertEqual(re.findall(r"HeroSiege::Objects::GameObject::(\w+)", excluded), ["Gate_Parent_obj", "Lock_obj"])
        install = self.body("install")
        self.assertIn('"asset_get_index"', install)
        self.assertIn("HeroSiege::Objects::GetObjectName(", install)
        self.assertIn('"object_is_ancestor"', install)
        self.assertIn("g_JumpScenery.SetFamily(", install)
        for name in ("Collision_Parent_obj", "Gate_Parent_obj", "Lock_obj", "Wall_Parent_obj", "Player_obj"):
            index = sdk_object_index(name)
            self.assertIsNone(re.search(r"\b" + index + r"\b", self.code), name + "'s index is written as a number")
        # The header itself stays game-independent: no object named in its code.
        for name in ("Gate_Parent_obj", "Lock_obj", "HeroSiege::"):
            self.assertNotIn(name, self.header)

    def test_the_gate_check_reruns_the_same_original_against_each_excluded_object(self):
        excluded = self.body("excluded")
        self.assertIn("for (int e : g_JsExcludedIdx)", excluded)
        self.assertIn("copy[b.objectArg] = RValue((double)e);", excluded)
        self.assertIn("orig(r, S, O, argc, copy.data());", excluded)
        family_at = self.body("family_at")
        self.assertIn("g_JsOrig[(int)JsNs::Builtin::PlaceMeeting]", family_at)
        self.assertIn("orig(r, g_JsPlayer, g_JsPlayer, 3, args);", family_at)

    def test_the_room_is_read_as_a_builtin_variable(self):
        room = self.body("room")
        self.assertIn('g_Yytk->GetBuiltin("room_width", nullptr, NULL_INDEX, rw)', room)
        self.assertIn('g_Yytk->GetBuiltin("room_height", nullptr, NULL_INDEX, rh)', room)
        self.assertNotIn("variable_global_get", self.code)

    # ---- off costs nothing --------------------------------------------------------

    def test_each_detour_returns_the_original_first_while_off(self):
        self.assertEqual(self.lines("builtin")[0],
                         "if (!g_JumpScenery.Enabled() || g_JsBusy) { if (g_JsOrig[row]) g_JsOrig[row](Result, S, O, argc, Args); return; }")
        self.assertEqual(self.lines("leap")[0],
                         "if (!g_JumpScenery.Enabled() || g_JsBusy) return g_JsOrigLeap(S, O, R, argc, A);")

    def test_the_original_runs_before_the_core_decides_and_only_the_local_player_counts(self):
        builtin = self.body("builtin")
        self.assertLess(builtin.index("orig(Result, S, O, argc, Args);"), builtin.index("g_JumpScenery.OnQuery("))
        self.assertIn("if (!S || S != g_JsPlayer) return;", builtin)
        self.assertIn("if (answer != JsNs::Answer::Real) Result = JsAnswerValue(answer);", builtin)
        leap = self.body("leap")
        self.assertLess(leap.index("g_JumpScenery.OnLeapEntry("), leap.rindex("return g_JsOrigLeap(S, O, R, argc, A);"))
        self.assertIn("S && S == g_JsPlayer", leap)
        player = self.body("player")
        self.assertIn("HhResolveLocalPlayer(p)", player)
        self.assertIn("HhResolveInstance(p)", player)
        answer = self.body("answer")
        self.assertIn("RValue(JsNs::kNoone)", answer)
        self.assertIn("RValue(false)", answer)

    def test_frame_callback_runs_one_tick_that_returns_at_once_while_off(self):
        for source in (self.plugin, strip_research_blocks(self.plugin)):
            frame = strip_comments(function_body(source, "void FrameCallback(FWFrame& FrameContext)"))
            self.assertEqual(frame.count("JumpSceneryTick();"), 1)
            self.assertIn("if (g_Setup) JumpSceneryTick();", frame)
            self.assertNotIn("g_JumpScenery", frame)
        self.assertEqual(self.lines("tick")[0], "if (!g_JumpScenery.Enabled()) return;")

    # ---- the research build's holders ----------------------------------------------

    def test_jumpscenery_refuses_while_citrace_or_jumpprobe_holds_its_hooks(self):
        holders = self.body("holders")
        for held in ("g_OrigCi_InstancePosition", "g_OrigCi_PositionMeeting", "g_JpBuiltinRows[i].installed",
                     "g_JpRows[kJpRow_SkillsLeap].installed"):
            self.assertIn(held, holders)
        self.assertIn('"citrace holds "', holders)
        self.assertIn('"jumpprobe holds "', holders)
        install = self.body("install")
        self.assertLess(install.index("JumpSceneryResearchHolders()"), install.index("HookOneScript("))
        # Research build only: neither instrument exists in the player build.
        self.assertNotIn("JumpSceneryResearchHolders", self.shipped)
        self.assertNotIn("citrace", self.shipped_block)
        self.assertNotIn("jumpprobe", self.shipped_block)

    def test_jumpprobe_hook_refuses_while_jumpscenery_holds_them(self):
        jp_install = strip_comments(function_body(self.plugin, "static void JpInstall()"))
        self.assertLess(jp_install.index("JumpSceneryHeldHooks()"), jp_install.index("for (JpRow& t : g_JpRows)"))
        self.assertIn('"jumpprobe hook: refused - jumpscenery holds "', jp_install)
        held = self.body("held")
        self.assertIn("g_JsOrig[i]", held)
        self.assertIn("g_JsOrigLeap", held)
        self.assertNotIn("JumpSceneryHeldHooks", self.shipped)

    # ---- no address -----------------------------------------------------------------

    def test_no_address_constant_or_module_base_arithmetic(self):
        self.assertIsNone(re.search(r"\bk\w*Rva\w*\b", self.code))
        self.assertIsNone(re.search(r"\(\s*char\s*\*\s*\)\s*\w+\s*\+", self.code))
        self.assertIsNone(re.search(r"0x[0-9A-Fa-f]{5,}", self.code))
        for forbidden in ("reinterpret_cast<CScript", "m_Functions", "GetInstanceObject"):
            self.assertNotIn(forbidden, self.code)


if __name__ == "__main__":
    unittest.main()
