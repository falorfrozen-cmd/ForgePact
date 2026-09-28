"""Hidden loot sleep (`hiddenloot`): the plugin wiring and the rules the class must keep.

tests/test_hidden_loot_behavior.py runs the real class against a controlled
runner. This file pins what that harness cannot see: the verb is a player
command with its own early return; the frame callback runs the tick only after
setup and the tick costs nothing while the switch is off; the one hook goes on
`LootGroundInit` by its SDK name through HookOneScript with `nativeOut`, calls
the trampoline first, and a table-only or failed install is said out loud and
falls back to the pass; no game address, no struct layout and no
instance_destroy anywhere in the mod; the builtins it calls are the documented
ones; the object's kind never decides whether it is looked at; the key is read
only while the game's own window is in front; the class starts off with Left
Alt (164) stored; and the modstate field and the line formats the live
operator reads byte for byte.
"""
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PLUGIN = ROOT / "plugin" / "ModuleMain.cpp"
HEADER = ROOT / "plugin" / "include" / "ForgePact" / "HiddenLootMod.hpp"

ADAPTER_START = "// ---- Hidden loot sleep (HiddenLootMod.hpp): the adapter"
ADAPTER_END = "// ---- end of the hidden loot sleep adapter"

ALLOWED_BUILTINS = {
    "instance_activate_object", "instance_deactivate_object", "variable_instance_exists",
    "variable_instance_get", "variable_instance_set", "instance_number", "instance_find",
    "instance_exists", "asset_get_index", "room_get_name", "room_persistent",
}


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


class HiddenLootPluginWiringTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.plugin = PLUGIN.read_text(encoding="utf-8").replace("\r\n", "\n")
        start = cls.plugin.find(ADAPTER_START)
        end = cls.plugin.find(ADAPTER_END)
        if start < 0 or end < start:
            raise AssertionError("the hidden loot adapter's start and end markers are missing from ModuleMain.cpp")
        cls.adapter_raw = cls.plugin[start:end]
        cls.adapter = _code(cls.adapter_raw)

    def test_the_player_build_accepts_the_verb(self):
        start = self.plugin.index("kPlayerCommands = {")
        block = self.plugin[start:self.plugin.index("};", start)]
        self.assertIn('"hiddenloot"', block)

    def test_the_verb_is_a_standalone_early_return(self):
        run = _body(self.plugin, "static void RunCommand(const std::string& line)")
        self.assertIn('if (lc == "hiddenloot") { HiddenLootCommand(rest); return; }', run)
        research = "\n".join(_research_blocks(self.plugin))
        self.assertNotIn('lc == "hiddenloot"', research)
        self.assertNotIn("static void HiddenLootCommand(", research)

    def test_the_tick_runs_after_setup_once_a_frame_after_far_sleep(self):
        frame = _code(_body(self.plugin, "void FrameCallback(FWFrame& FrameContext)"))
        self.assertEqual(frame.count("HiddenLootTick();"), 1)
        self.assertIn("if (g_Setup) HiddenLootTick();", frame)
        self.assertLess(frame.index("if (g_Setup) FarSleepTick();"), frame.index("if (g_Setup) HiddenLootTick();"))

    def test_the_tick_costs_nothing_while_off(self):
        tick = [l.strip() for l in _code(_body(self.plugin, "static void HiddenLootTick()")).splitlines() if l.strip()]
        self.assertEqual(tick[0], "auto& hl = ForgePact::HiddenLootMod::Instance();")
        self.assertEqual(tick[1], "if (!hl.Enabled()) return;")

    def test_the_hook_goes_on_loot_ground_init_by_sdk_name_with_both_routes(self):
        install = _code(_body(self.plugin, "static void HiddenLootInstall()"))
        self.assertIn("HookOneScript(SdkShortScriptName(HeroSiege::Scripts::gml_Script_LootGroundInit)", install)
        self.assertIn("&g_Orig_LootGroundInit, &native)", install)
        self.assertIn("g_HiddenLootInstallTried = true;", install)
        self.assertIn("ForgePact::HiddenLootMod::Route::Both", install)
        self.assertIn("ForgePact::HiddenLootMod::Route::TableOnly", install)
        self.assertIn("ForgePact::HiddenLootMod::Route::None", install)
        # A table-only or failed install is logged, never silent.
        self.assertIn('"TABLE-ONLY"', install)
        self.assertIn("Out(", install)
        self.assertIn("every 18 frames", install)
        # Installed once, and only after setup.
        command = _code(_body(self.plugin, "static void HiddenLootCommand(const std::string& rest)"))
        self.assertIn("if (g_Setup && !g_HiddenLootInstallTried) HiddenLootInstall();", command)
        tick = _code(_body(self.plugin, "static void HiddenLootTick()"))
        self.assertIn("if (!g_HiddenLootInstallTried) HiddenLootInstall();", tick)

    def test_nothing_else_hooks_loot_ground_init(self):
        calls = re.findall(r"HookOneScript\w*\([^;]*LootGroundInit\b", _code(self.plugin))
        self.assertEqual(len(calls), 1, calls)

    def test_the_hook_body_calls_the_trampoline_first_and_only_records(self):
        hook = _code(_body(self.plugin, "static RValue& HookHiddenLootInit(CInstance* S, CInstance* O, RValue& R, int argc, RValue** A)"))
        self.assertIn("g_Orig_LootGroundInit(S, O, R, argc, A)", hook)
        self.assertLess(hook.index("g_Orig_LootGroundInit(S, O, R, argc, A)"), hook.index(".OnInit("))
        self.assertIn(".Enabled()", hook)
        self.assertLess(hook.index(".Enabled()"), hook.index(".OnInit("))
        # Inside the game's own call the hook makes no runner call.
        self.assertNotIn("CallBuiltin", hook)
        self.assertNotIn("instance_deactivate_object", hook)

    def test_no_address_no_base_plus_literal_no_destroy(self):
        for text, where in ((self.adapter, "adapter"), (_code(HEADER.read_text(encoding="utf-8")), "header")):
            self.assertIsNone(re.search(r"\bk\w*Rva\w*\b", text), where)
            self.assertIsNone(re.search(r"\(\s*char\s*\*\s*\)\s*\w+\s*\+\s*(?:0x[0-9A-Fa-f]+)", text), where)
            self.assertNotIn("instance_destroy", text, where)
            self.assertNotIn("GetModuleHandle", text, where)
            self.assertNotIn("IsInstanceHandle", text, where)

    def test_the_adapter_calls_only_the_documented_builtins(self):
        names = set(re.findall(r'CallBuiltin\("(\w+)"', self.adapter))
        self.assertLessEqual(names, ALLOWED_BUILTINS, names - ALLOWED_BUILTINS)
        # The room is read as far sleep reads it: name and persistence.
        self.assertIn("FarSleepRoomInfo()", self.adapter)
        probe = _code(_body(self.plugin, "static ForgePact::FarSleep::RoomInfo FarSleepRoomInfo()"))
        self.assertIn('"room_get_name"', probe)
        self.assertIn('"room_persistent"', probe)

    def test_the_key_is_read_only_while_the_game_is_in_front(self):
        key = _code(_body(self.plugin, "static bool HiddenLootKeyDown(int vk)"))
        self.assertIn("GetAsyncKeyState(vk) & 0x8000", key)
        front = _code(_body(self.plugin, "static bool HiddenLootGameInFront()"))
        for call in ("GetForegroundWindow()", "GetWindowThreadProcessId(", "GetCurrentProcessId()"):
            self.assertIn(call, front)
        command = _code(_body(self.plugin, "static void HiddenLootCommand(const std::string& rest)"))
        self.assertIn("hl.SetInput(&HiddenLootKeyDown, &HiddenLootGameInFront);", command)

    def test_mod_state_reports_it(self):
        self.assertIn('\\"hiddenLoot\\":{\\"enabled\\":', self.plugin)
        start = self.plugin.index('\\"hiddenLoot\\":{\\"enabled\\":')
        block = self.plugin[start:self.plugin.index("}", self.plugin.index('\\"errors\\":', start) + 1) + 1]
        fields = re.findall(r'\\"(\w+)\\":', block)
        self.assertEqual(fields, ["hiddenLoot", "enabled", "route", "key", "asleep", "shown", "held", "errors"])

    def test_the_lines_the_live_operator_reads(self):
        a = self.adapter_raw
        self.assertIn('"hiddenloot -> ON route="', a)
        on = _code(_body(self.plugin, "static void HiddenLootCommand(const std::string& rest)"))
        for token in ('"hiddenloot -> ON route="', '" key="', '" walk-slept="', '" walk-visible="', '" walk-no-filter-var="'):
            self.assertIn(token, on)
        self.assertLess(on.index('" walk-slept="'), on.index('" walk-visible="'))
        self.assertLess(on.index('" walk-visible="'), on.index('" walk-no-filter-var="'))
        self.assertIn('"hiddenloot -> OFF woken="', a)
        self.assertIn('" exist-after="', on)
        self.assertIn('"hiddenloot stat: "', a)
        self.assertIn('"hiddenloot: key="', a)
        self.assertIn('"hiddenloot: key refused - "', a)
        self.assertIn('"; key stays "', a)
        self.assertIn('"hiddenloot: usage hiddenloot 1 | 0 | stat | key <vk>"', a)
        # The stat fields, in the documented order, shared by `stat` and `0`.
        stat = _code(_body(self.plugin, "static std::string HiddenLootStatFields()"))
        fields = re.findall(r'"\s?([a-z-]+)="', stat)
        self.assertEqual(fields, ["on", "route", "key", "held", "inits", "slept", "asleep-now", "shown-now", "visible",
                                  "no-filter-var", "unidentified", "gone", "passes", "skipped-persistent", "errors"])
        self.assertEqual(on.count("HiddenLootStatFields()"), 2)
        # A key is written as its code, or `none` for 0.
        self.assertIn('"none"', a)


class HiddenLootClassRulesTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.header = HEADER.read_text(encoding="utf-8").replace("\r\n", "\n")
        cls.code = _code(cls.header)

    def test_it_starts_off_with_left_alt_stored(self):
        self.assertIn("bool m_Enabled = false;", self.code)
        self.assertIn("static constexpr int kDefaultKey = 164;", self.code)
        self.assertIn("int m_Key = kDefaultKey;", self.code)

    def test_it_reaches_the_game_only_through_call_builtin(self):
        calls = re.findall(r"g_Yytk->(\w+)", self.code)
        self.assertTrue(calls)
        self.assertEqual(set(calls), {"CallBuiltin"})
        for forbidden in ("GetInstanceObject", "GetMembers", "reinterpret_cast", "m_Functions", "GetBuiltin",
                          "Hero_Siege.exe", "0x14", "GetAsyncKeyState", "GetForegroundWindow"):
            self.assertNotIn(forbidden, self.code, forbidden)

    def test_the_calls_it_makes_are_the_documented_ones(self):
        names = set(re.findall(r'Call\("(\w+)"', self.code))
        self.assertTrue(names)
        self.assertLessEqual(names, ALLOWED_BUILTINS, names - ALLOWED_BUILTINS)

    def test_the_item_is_identified_by_its_object_by_sdk_name(self):
        self.assertIn("HeroSiege::Objects::GetObjectName(HeroSiege::Objects::GameObject::Loot_Ground_obj)", self.code)
        identify = _body(self.code, "bool Identify(const RValue& candidate, RValue& handle, int64_t& id)")
        self.assertIn('Call("instance_exists"', identify)
        self.assertIn('RValue("object_index")', identify)
        self.assertIn("m_LootIndex", identify)
        # The kind never decides: no instance-kind gate before the object test.
        for kind in ("VALUE_OBJECT", "VALUE_REF", "VALUE_REAL", "IsInstanceHandle"):
            self.assertNotIn(kind, identify, kind)
        # Of argument 0, argument 1 and `self`, each is offered to that test.
        pending = _body(self.code, "void ProcessPending(RoomProbe& roomInfo)")
        self.assertIn("for (const RValue& candidate : call.candidates)", pending)

    def test_the_init_path_only_records(self):
        init = _body(self.code, "void OnInit(const RValue& arg0, const RValue& arg1, const RValue& self)")
        self.assertNotIn("Call(", init)
        self.assertNotIn("CallBuiltin", init)
        self.assertIn("if (!m_Enabled) return;", init)

    def test_the_verdict_is_read_only_after_it_is_known_to_exist(self):
        verdict = _body(self.code, "int Verdict(const RValue& handle)")
        self.assertLess(verdict.index('Call("variable_instance_exists"'), verdict.index('Call("variable_instance_get"'))
        self.assertIn('"lootFilterVisible"', self.code)

    def test_the_key_waits_for_the_game_window(self):
        frame = _body(self.code, "void OnFrame(uint64_t frame, int64_t roomKey, RoomProbe roomInfo)")
        self.assertLess(frame.index("if (!m_Enabled) return;"), frame.index("m_InFront()"))
        self.assertLess(frame.index("m_Key != 0"), frame.index("m_InFront()"))
        self.assertLess(frame.index("m_InFront()"), frame.index("m_KeyDown(m_Key)"))

    def test_the_constants(self):
        self.assertIn("static constexpr uint64_t kPassFrames = 18;", self.code)
        self.assertIn("static constexpr long kWalkCap = 8192;", self.code)
        allowed = _body(self.code, "static bool KeyAllowed(int vk)")
        self.assertIn("vk == 0 || (vk >= 3 && vk <= 254)", allowed)

    def test_the_fallback_pass_never_runs_with_both_routes(self):
        frame = _body(self.code, "void OnFrame(uint64_t frame, int64_t roomKey, RoomProbe roomInfo)")
        self.assertIn("if (m_Route != Route::Both && frame >= m_LastPass + kPassFrames)", frame)


if __name__ == "__main__":
    unittest.main()
