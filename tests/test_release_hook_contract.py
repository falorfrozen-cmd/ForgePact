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


def strip_research_blocks(source: str) -> str:
    """What the player build compiles, i.e. with FORGEPACT_RELEASE defined.

    This evaluates the conditional rather than pattern-matching one spelling
    of it. Research code is written BOTH ways in this file - as
    `#ifndef FORGEPACT_RELEASE ... #endif` and as the `#else` half of
    `#ifdef FORGEPACT_RELEASE ... #else ... #endif` - and an earlier version
    of this helper only understood the first, so it reported a research-only
    call as shipping. Conditionals on anything other than FORGEPACT_RELEASE
    are passed through untouched; nesting is tracked so an inner `#if` cannot
    end an outer block early.
    """
    kept = []
    # One entry per open conditional: (is it about FORGEPACT_RELEASE,
    # is its current branch compiled). Unrelated conditionals keep both
    # halves, so `#else` must not flip them.
    stack = []
    for line in source.split("\n"):
        stripped = line.strip()
        if stripped.startswith("#ifdef FORGEPACT_RELEASE"):
            stack.append([True, True])
        elif stripped.startswith("#ifndef FORGEPACT_RELEASE"):
            stack.append([True, False])
        elif stripped.startswith("#if"):
            stack.append([False, True])
        elif stripped.startswith("#else") and stack:
            if stack[-1][0]:
                stack[-1][1] = not stack[-1][1]
        elif stripped.startswith("#endif") and stack:
            stack.pop()
        elif all(active for _, active in stack):
            kept.append(line)
    return "\n".join(kept)


def strip_comments(source: str) -> str:
    """Code only. Research notes name plenty of addresses; comments are fine."""
    source = re.sub(r"/\*.*?\*/", "", source, flags=re.S)
    return re.sub(r"//[^\n]*", "", source)


MAP_REVEAL_HEADER_PATH = PROJECT_ROOT / "plugin" / "include" / "ForgePact" / "MapRevealManager.hpp"
STATS_HEADER_PATH = PROJECT_ROOT / "plugin" / "include" / "ForgePact" / "StatsManager.hpp"
DENSITY_HEADER_PATH = PROJECT_ROOT / "plugin" / "include" / "ForgePact" / "DensityManager.hpp"


class ReleaseHookContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.plugin = PLUGIN_PATH.read_text(encoding="utf-8")
        cls.map_reveal_header = MAP_REVEAL_HEADER_PATH.read_text(encoding="utf-8")
        cls.stats_header = STATS_HEADER_PATH.read_text(encoding="utf-8")
        cls.density_header = DENSITY_HEADER_PATH.read_text(encoding="utf-8")

    def test_release_initialization_has_no_eager_gameplay_hook_group(self):
        body = function_body(self.plugin, "static void InstallHook()")
        release = body.split("#ifdef FORGEPACT_RELEASE", 1)[1].split("#else", 1)[0]
        for eager in (
            "InstallCreateHooks();",
            "InstallDropMultHooks();",
            "InstallNecroBalanceHooks();",
        ):
            self.assertNotIn(eager, release)

    def test_release_build_still_loads_custom_forge_and_auto_arms_items(self):
        # A prior edit moved the `#ifdef FORGEPACT_RELEASE ... return;` early
        # exit above these calls, so a shipped build skipped the Item Editor
        # sidecar and Headhunter/Tyrant's Crown/Beacon auto-arm entirely.
        # They must run unconditionally, before the release/dev split.
        body = function_body(self.plugin, "static void InstallHook()")
        guard_at = body.index("#ifdef FORGEPACT_RELEASE")
        unconditional = body[:guard_at]
        for call in (
            "LoadCustomForgeEntries();",
            "InstallCustomForgeItemHooks();",
            "HeadhunterAutoArm();",
            "TyrantAutoArm();",
            "BeaconAutoArm();",
        ):
            self.assertIn(call, unconditional)

    def test_module_initialize_registers_headhunter_hud_label_hook(self):
        # InstallHeadLabelHook() registers Hook_DrawHudBuffs, which renders the
        # stolen-affix labels; a rewrite of ModuleInitialize previously dropped
        # this call, so the labels stopped rendering even when the underlying
        # buff application kept working.
        init = function_body(self.plugin, "EXPORTED AurieStatus ModuleInitialize(")
        self.assertIn("InstallHeadLabelHook();", init)

    def test_safe_f_bounds_large_finite_magnitudes_not_just_nan_and_inf(self):
        # SafeF must reject not only inf/NaN but also large finite doubles
        # (e.g. an IPC-supplied "1e300"): %.0f of a merely-finite huge value
        # still overruns every fixed sprintf_s buffer it feeds.
        safe_f = function_body(self.plugin, "static inline double SafeF(double v)")
        self.assertIn("std::isfinite(v)", safe_f)
        self.assertRegex(safe_f, r"kMaxSafeF|1e1[0-9]|1e[2-9][0-9]?")
        self.assertIn("return kMaxSafeF;", safe_f)

    def test_functional_hooks_install_only_for_non_vanilla_commands(self):
        drop = function_body(self.plugin, "static void SetDropMult(")
        self.assertIn("if (n > 1) InstallDropMultHooks();", drop)

        special = function_body(self.plugin, "static void SpecialRate(")
        self.assertIn("if (n > 1)", special)
        self.assertIn("InstallCreateHooks();", special)
        self.assertNotIn("InstallSpecialLifecycleHook();", special)

        # The density command handler itself moved to
        # ForgePact::DensityManager::HandleCommand (2026-09 class split).
        command = function_body(self.plugin, "static void RunCommand(")
        density_call = command.split('else if (lc == "density")', 1)[1].split(
            'else if (lc == "dropstats")', 1
        )[0]
        self.assertIn("DensityManager::Instance().HandleCommand(rest);", density_call)
        density = function_body(self.density_header, "void HandleCommand(const std::string& rest)")
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
        # Stat hook telemetry moved to ForgePact::StatsManager (2026-09 class
        # split): same BP_DIAG_INCREMENT macro, now on class members.
        self.assertIn("BP_DIAG_INCREMENT(mgr.m_Calls_##NAME);", self.stats_header)
        self.assertIn("BP_DIAG_INCREMENT(mgr.m_CallsAdd_StatFasterCastRate);", self.stats_header)

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
        # Migrated into ForgePact::MapRevealManager::Tick() (2026-09 class split);
        # ModuleMain.cpp now only calls it via MapRevealManager::Instance().
        self.assertIn("ForgePact::MapRevealManager::Instance().OnFrame(g_RuntimeFrame);", self.plugin)
        reveal = function_body(self.map_reveal_header, "void Tick()")
        self.assertIn("instanceKey == m_LastInstance", reveal)
        self.assertIn("gridKey == m_LastGrid", reveal)
        self.assertIn("roomKey == m_LastRoom", reveal)

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
        # Migrated into ForgePact::DensityManager (2026-09 class split).
        self.assertIn("double Mult{ 1.0 };", self.density_header)
        # Migrated into ForgePact::MapRevealManager (2026-09 class split).
        self.assertIn("bool m_Enabled{ false };", self.map_reveal_header)

        for signature, original in (
            ("static void HookICD(", "g_OrigICD(Result, S, O, argc, Args);"),
            ("static void HookICL(", "g_OrigICL(Result, S, O, argc, Args);"),
        ):
            hook = function_body(self.plugin, signature)
            self.assertIn("ForgePact::DensityManager::Instance().Mult <= 1.0", hook)
            self.assertIn("g_ObjMult.empty()", hook)
            self.assertNotIn("g_NecroBalanceEnabled", hook)
            self.assertIn(original, hook)

        # StatCmd moved to ForgePact::StatsManager::HandleStatCommand (2026-09
        # class split); same "x1.0 with no hook installed stays native" guard.
        stat = function_body(self.stats_header, "void HandleStatCommand(const std::string& rest)")
        native = stat.index("c == 1.0 && !*hedef->orig")
        install = stat.index("HookOneScript(hedef->name", native)
        self.assertLess(native, install)

    def test_gameplay_hooks_intercept_direct_native_calls(self):
        # REPORTED 2026-09-12 (origin's review of PR #2, issue 1): the script
        # table swap alone is blind to compiled GML's direct `call rel32` -
        # the finding this branch itself documented after 34 hooked call sites
        # reported "0 calls" while the game was demonstrably running them.
        # Read-only inspection of the shipped exe found direct callers for
        # StatMovementSpeed, StatAttackSpeed, DropRelic, DropMonsterGold and
        # DropGold, so stat scaling, drop multipliers and the max-level relic
        # filter could report "HOOK INSTALLED" and change nothing.
        #
        # The interception belongs in the installer, not bolted onto whichever
        # names a review happened to verify.
        body = function_body(self.plugin, "static bool HookOneScript(")
        self.assertIn("MmCreateHook", body)
        self.assertIn("m_ScriptFunction", body)   # both routes, not one

    def test_native_detour_is_installed_once_and_only_on_the_real_target(self):
        # Three guards make repeat installation safe, which matters because
        # shared chokepoints (DropRelic) are installed from more than one call
        # site and re-install is how this file makes that idempotent.
        body = function_body(self.plugin, "static bool HookOneScript(")
        # 1. only the first install, when the table still holds the game's fn
        self.assertIn("const bool firstInstall = (origOut && !*origOut);", body)
        self.assertIn("if (firstInstall) {", body)
        self.assertLess(body.index("if (firstInstall) {"), body.index("MmCreateHook"))
        # 2. never patch a pointer that is not the game's code (another hook
        #    may already have swapped the entry to something in this module)
        self.assertIn("AddrIsExecutableInModule(GetModuleHandleA(nullptr)", body)
        self.assertLess(body.index("AddrIsExecutableInModule"), body.index("MmCreateHook"))
        # 3. a failed detour degrades to table-only and says so, rather than
        #    leaving *origOut null or pretending it worked
        self.assertIn("*origOut = tableEntry;", body)
        self.assertIn("TABLE-ONLY", body)

    def test_hook_bodies_call_through_the_trampoline(self):
        # MmCreateHook patches the bytes at the target, so a hook body that
        # called the original address directly would re-enter itself forever.
        # *origOut must become the trampoline.
        body = function_body(self.plugin, "static bool HookOneScript(")
        self.assertIn("*origOut = reinterpret_cast<PFUNC_YYGMLScript>(tramp);", body)
        # The Headhunter's hand-rolled supplemental detour must be gone - it
        # would now patch the trampoline HookOneScript just returned.
        install = function_body(self.plugin, "static void InstallHeadhunterHook()")
        self.assertNotIn("MmCreateHook", install)

    def test_research_hooks_stay_table_only(self):
        # `citrace nativetrace` runs a native detour beside a table hook on the
        # same target and prints both counters; that comparison is what proved
        # the blindness, and it only means something while one side really is
        # table-only. So the research hooks keep the old installer.
        table = function_body(self.plugin, "static bool HookOneScriptTable(")
        self.assertNotIn("MmCreateHook", table)
        # ...and nothing in the player build may use it.
        shipped = strip_comments(strip_research_blocks(self.plugin))
        calls = [m for m in re.findall(r"HookOneScriptTable\(", shipped)]
        self.assertEqual(
            len(calls), 1,  # the definition itself
            "HookOneScriptTable is reachable from the player build; gameplay hooks must use HookOneScript",
        )

    def test_player_binary_calls_no_hand_resolved_game_address(self):
        # The defect that killed `relicgate` and nearly shipped in the pet
        # quest collector: a constant RVA read off one build's decompiled body
        # points at unrelated bytes the moment the game is rebuilt, and a call
        # through it transfers control into whatever is there. Everything in
        # the player binary must resolve by name (GetNamedRoutinePointer,
        # asset_get_index, CallBuiltin) or off a runtime struct YYToolkit
        # defines - never off an address anybody typed in.
        #
        # See agents.md, "Never Call an Address You Resolved by Hand".
        shipped = strip_comments(strip_research_blocks(self.plugin))
        offender = re.search(r"\bk\w*Rva\w*\b", shipped)
        self.assertIsNone(
            offender,
            "fixed game-address constant reachable from the player build: "
            + (offender.group(0) if offender else ""),
        )
        call_target = re.search(
            r"\(\s*char\s*\*\s*\)\s*\w+\s*\+\s*(?:0x[0-9A-Fa-f]+|k\w*Rva\w*)", shipped)
        self.assertIsNone(
            call_target,
            "player build computes a call target from a module base plus a literal offset: "
            + (call_target.group(0) if call_target else ""),
        )

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
