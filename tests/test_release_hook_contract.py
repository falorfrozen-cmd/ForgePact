import copy
import importlib.util
import re
import unittest
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[1]
PLUGIN_PATH = PROJECT_ROOT / "plugin" / "ModuleMain.cpp"
PANEL_PATH = PROJECT_ROOT / "src" / "forgepact.py"


def function_body(source: str, signature: str) -> str:
    # Some runtime functions have an early forward declaration; the final
    # occurrence is the implementation whose body defines the contract.
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


class ReleaseHookContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.plugin = PLUGIN_PATH.read_text(encoding="utf-8")

    def test_release_initialization_has_no_eager_gameplay_hook_group(self):
        body = function_body(self.plugin, "static void InstallHook()")
        release = body.split("#ifdef FORGEPACT_RELEASE", 1)[1].split("#else", 1)[0]
        for eager in (
            "InstallCreateHooks();",
            "InstallDropMultHooks();",
            "InstallNecroBalanceHooks();",
        ):
            self.assertNotIn(eager, release)

    def test_functional_hooks_install_only_for_non_vanilla_commands(self):
        drop = function_body(self.plugin, "static void SetDropMult(")
        self.assertIn("if (n > 1) InstallDropMultHooks();", drop)

        special = function_body(self.plugin, "static void SpecialRate(")
        self.assertIn("if (n > 1)", special)
        self.assertIn("InstallCreateHooks();", special)
        self.assertNotIn("InstallSpecialLifecycleHook();", special)

        command = function_body(self.plugin, "static void RunCommand(")
        density = command.split('else if (lc == "density")', 1)[1].split(
            'else if (lc == "dropstats")', 1
        )[0]
        self.assertIn("if (d > 1.0)", density)
        self.assertIn("InstallCreateHooks();", density)
        self.assertIn("InstallDensityLifecycleHooks();", density)

    def test_ship_hot_path_telemetry_compiles_to_no_op(self):
        macro = re.search(
            r"#ifdef FORGEPACT_RELEASE\s*"
            r"#define BP_DIAG_INCREMENT\(counter\) \(\(void\)0\)",
            self.plugin,
        )
        self.assertIsNotNone(macro)
        self.assertIn("BP_DIAG_INCREMENT(g_cnt_##NAME);", self.plugin)
        self.assertIn("BP_DIAG_INCREMENT(g_StatSayac_##NAME);", self.plugin)
        self.assertIn("BP_DIAG_INCREMENT(g_StatAddSayac_##NAME);", self.plugin)

        create = function_body(self.plugin, "static void DoMultiCreate(")
        research_prefix = create.split("#endif", 1)[0]
        self.assertIn("#ifndef FORGEPACT_RELEASE", research_prefix)
        self.assertIn("g_CreateCounts[objIdx]++", research_prefix)
        self.assertIn("PullNearApply(objIdx, Args, argc);", research_prefix)
        self.assertIn("LogCreatePos(objIdx, Args, argc);", research_prefix)
        self.assertIn("BP_DIAG_INCREMENT(g_DensityRevisitSkips);", create)
        self.assertIn("BP_DIAG_INCREMENT(g_ExtraCreators);", create)
        self.assertRegex(
            create,
            r"#ifndef FORGEPACT_RELEASE\s*"
            r"PostCreateCheck\(objIdx, Result, Args, argc\);\s*#endif",
        )

        for signature in (
            "static void HookICD(",
            "static void HookICL(",
        ):
            hook = function_body(self.plugin, signature)
            self.assertRegex(
                hook,
                r"#ifndef FORGEPACT_RELEASE\s*[\s\S]*?_ReturnAddress\(\);\s*#endif",
            )
            self.assertRegex(
                hook,
                r"#ifndef FORGEPACT_RELEASE\s*WatchLog\(ret, objIdx\);\s*#endif",
            )

    def test_repeated_frame_work_is_bounded(self):
        reveal = function_body(self.plugin, "static void AutoRevealTick()")
        self.assertIn("instanceKey == g_AutoRevealLastInstance", reveal)
        self.assertIn("gridKey == g_AutoRevealLastGrid", reveal)
        self.assertIn("roomKey == g_AutoRevealLastRoom", reveal)

        special = function_body(self.plugin, "static void SpecialRate(")
        self.assertIn("if (n > 1) SetObjectMultiplier(oi, n);", special)
        self.assertIn("SetObjectMultiplier(oi, 1);", special)
        self.assertIn("KuyruktanNesneyiSil(oi);", special)

        queue = function_body(self.plugin, "static void KuyrukIsle()")
        self.assertIn("(g_KuyrukKare % 10) == 0", queue)

        create = function_body(self.plugin, "static void DoMultiCreate(")
        self.assertIn("ObjectMultiplier(objIdx)", create)
        self.assertIn("SpecialCreateScope specialScope(ozelIcerik);", create)

        frame = function_body(self.plugin, "void FrameCallback(")
        self.assertIn("((fc++) % 30) == 0", frame)

    def test_player_frame_loop_keeps_only_required_polling(self):
        frame = function_body(self.plugin, "void FrameCallback(")
        self.assertIn("EstForceApply();", frame)
        est = function_body(self.plugin, "static void EstForceApply()")
        self.assertIn("if (g_EstForce.empty() || !g_Yytk) return;", est)
        self.assertRegex(
            frame,
            r"#ifndef FORGEPACT_RELEASE\s*"
            r"static bool f5p[\s\S]*?f5p = f5;\s*#endif",
        )
        self.assertNotIn("if ((fc % 60) == 0) KonsoluGizle();", frame)

    def test_disabled_single_instance_hooks_are_not_in_player_binary(self):
        block = self.plugin.split("// ===== Single-instance bypass:", 1)[1].split(
            "EXPORTED AurieStatus ModuleInitialize", 1
        )[0]
        self.assertRegex(
            block,
            r"Installed as early as possible\. =====\s*"
            r"#ifndef FORGEPACT_RELEASE[\s\S]*#endif\s*$",
        )

    def test_socket_research_hooks_are_excluded_from_player_binary(self):
        marker = "// ---- socketprobe: capture the two socket rolls"
        marker_at = self.plugin.index(marker)
        guard_at = self.plugin.rfind("#ifndef FORGEPACT_RELEASE", 0, marker_at)
        end_at = self.plugin.index("\n#endif", marker_at)
        rare_at = self.plugin.index("static void RareDropCmd", marker_at)
        self.assertGreaterEqual(guard_at, marker_at - 40)
        self.assertLess(end_at, rare_at)

        command = function_body(self.plugin, "static void RunCommand(")
        socket_at = command.index('lc == "socketprobe"')
        command_guard = command.rfind("#ifndef FORGEPACT_RELEASE", 0, socket_at)
        command_end = command.index("#endif", socket_at)
        self.assertGreater(command_guard, command.rfind('lc == "raredrop"', 0, socket_at))
        self.assertLess(command_end, command.index('lc == "droprate"', socket_at))

    def test_all_off_runtime_is_native_pass_through(self):
        self.assertIn("static double g_CreatorMult = 1.0;", self.plugin)
        self.assertIn("static bool g_AutoReveal = false;", self.plugin)

        for signature, original in (
            ("static void HookICD(", "g_OrigICD(Result, S, O, argc, Args);"),
            ("static void HookICL(", "g_OrigICL(Result, S, O, argc, Args);"),
        ):
            hook = function_body(self.plugin, signature)
            self.assertIn("g_CreatorMult <= 1.0", hook)
            self.assertIn("g_ObjMult.empty()", hook)
            self.assertNotIn("g_NecroBalanceEnabled", hook)
            self.assertIn(original, hook)

        stat = function_body(self.plugin, "static void StatCmd(")
        native = stat.index("c == 1.0 && !*hedef->orij")
        install = stat.index("HookOneScript(hedef->ad", native)
        self.assertLess(native, install)

    def test_special_queue_is_not_cleared_during_zone_generation(self):
        self.assertNotIn("HookZoneStateResetSingleSpecial", self.plugin)
        self.assertNotIn("fp_special_queue_reset_single", self.plugin)
        special = function_body(self.plugin, "static void SpecialRate(")
        self.assertIn("KuyruktanNesneyiSil(oi);", special)

    def test_once_only_mechanics_reset_their_flags_before_activation(self):
        # Shadow Realm and Chaos Tower activate once per run.  The plugin resets
        # the persistent flags immediately before the game's own activate code
        # runs, and only while the marker multiplier is above vanilla.
        self.assertIn('{ "chaostower",   "Spawn_Chaos_Tower_obj",    6,', self.plugin)
        self.assertIn('{ "shadowrealm",  "Spawn_Shadow_Realm_obj",   9,', self.plugin)
        special = function_body(self.plugin, "static void SpecialRate(")
        self.assertIn("if (sc->gateHook) InstallMechGateHooks();", special)
        self.assertLess(special.index("if (n > 1)"), special.index("InstallMechGateHooks();"))

        install = function_body(self.plugin, "static void InstallMechGateHooks()")
        self.assertIn('"anon@119@gml_Object_Spawn_Shadow_Realm_obj_Create_0"', install)
        self.assertIn('"anon@97@gml_Object_Spawn_Chaos_Tower_obj_Create_0"', install)

        sr = function_body(self.plugin, "static RValue& Hook_ShadowRealmGate(")
        orig = "g_Orig_ShadowRealmGate(S, O, R, argc, A)"
        self.assertIn("SpecialMultiplierOn(", sr)
        self.assertLess(sr.index('"shadowRealmSpawned"'), sr.index(orig))
        # The difficulty gate is only forced for the duration of the call.
        self.assertLess(sr.index("diff.Arm(2.0)"), sr.index(orig))
        self.assertLess(sr.index(orig), sr.index("diff.Restore()"))

        ct = function_body(self.plugin, "static RValue& Hook_ChaosTowerGate(")
        orig = "g_Orig_ChaosTowerGate(S, O, R, argc, A)"
        self.assertIn("SpecialMultiplierOn(", ct)
        for flag in ('"chaosTowerSpawnZone"', '"chaosTowerStarted"', '"gml_Script_SPV"'):
            self.assertLess(ct.index(flag), ct.index(orig), flag)
        self.assertLess(ct.index("diff.Arm(1.0)"), ct.index(orig))
        self.assertLess(ct.index(orig), ct.index("diff.Restore()"))

        force = function_body(self.plugin, "struct DifficultyGateForce")
        self.assertIn("if (old < minValue)", force)
        self.assertIn("RValue(old)", force)


class PanelAllOffContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        spec = importlib.util.spec_from_file_location("forgepact_contract_module", PANEL_PATH)
        cls.panel = importlib.util.module_from_spec(spec)
        assert spec.loader is not None
        spec.loader.exec_module(cls.panel)

    def test_default_config_emits_no_gameplay_commands(self):
        cfg = copy.deepcopy(self.panel.DEFAULTS)
        self.assertEqual([], self.panel.build_cmds(cfg))

    def test_once_only_mechanics_are_offered_and_emitted(self):
        # Chaos Tower and Shadow Realm became available once their Season 10
        # activation gates were decoded (2026-09-03).  Nothing stays hidden.
        keys = {key for key, *_ in self.panel.SPAWNERS}
        self.assertIn("chaostower", keys)
        self.assertIn("shadowrealm", keys)
        self.assertEqual(set(), set(self.panel.DISABLED_SPAWNER_KEYS))
        cfg = copy.deepcopy(self.panel.DEFAULTS)
        cfg["spawners"]["chaostower"] = 100
        cfg["spawners"]["shadowrealm"] = 3
        commands = self.panel.build_cmds(cfg)
        self.assertIn("specialrate chaostower 100", commands)
        self.assertIn("specialrate shadowrealm 3", commands)
        # x1 is vanilla and must stay silent.
        cfg["spawners"]["chaostower"] = 1
        cfg["spawners"]["shadowrealm"] = 1
        self.assertFalse(any("chaostower" in c or "shadowrealm" in c
                             for c in self.panel.build_cmds(cfg)))

    def test_panel_started_after_game_still_auto_applies(self):
        source = PANEL_PATH.read_text(encoding="utf-8")
        watcher = source.split("def watcher():", 1)[1].split("\n\nclass H", 1)[0]
        self.assertIn("was_running = False", watcher)
        self.assertIn("wait_for_plugin_ready(cfg)", watcher)
        ready = source.split("def wait_for_plugin_ready(", 1)[1].split("\n\ndef watcher", 1)[0]
        self.assertIn('send_cmds(["ping"], cfg)', ready)
        self.assertIn("not command_file.exists()", ready)


if __name__ == "__main__":
    unittest.main()
