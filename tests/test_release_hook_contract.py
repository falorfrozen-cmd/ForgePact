import copy
import importlib.util
import re
import unittest
from collections import Counter
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
PLUGIN_HEADER_DIR = PROJECT_ROOT / "plugin" / "include" / "ForgePact"
SDK_SCRIPTS_HEADER_PATH = PROJECT_ROOT.parent / "hs-game-sdk" / "cpp" / "include" / "hs_game_sdk" / "scripts.hpp"

# Every closure name (`anon@N@...`) the plugin can compile, spelled either as a
# raw GML string literal or through an `HeroSiege::Scripts::` constant. A
# literal matches the SDK once the `gml_Script_` prefix (present on every SDK
# constant's value, absent from some literals) is added back. An SDK reference
# resolves through the parsed identifier table instead of a regex on its value.
SDK_CONSTANT_RE = re.compile(r'inline constexpr std::string_view (\w+) = "([^"]+)";')
RAW_CLOSURE_RE = re.compile(r'"((?:gml_Script_)?[A-Za-z0-9_]*@?anon@\d+@[A-Za-z0-9_@]+)"')
SDK_CLOSURE_REF_RE = re.compile(r"HeroSiege::Scripts::(\w*anon_\d+\w*)")


def load_sdk_script_constants():
    """identifier -> full SDK string value, parsed from hs-game-sdk's own header.

    Skips (rather than fails) when the sibling checkout is absent, same as
    `build.bat` itself needs it as `/I ..\\..\\hs-game-sdk\\cpp\\include`.
    """
    if not SDK_SCRIPTS_HEADER_PATH.exists():
        raise unittest.SkipTest(f"hs-game-sdk header not found at {SDK_SCRIPTS_HEADER_PATH}")
    text = SDK_SCRIPTS_HEADER_PATH.read_text(encoding="utf-8")
    constants = dict(SDK_CONSTANT_RE.findall(text))
    assert len(constants) > 6000, f"parsed only {len(constants)} SDK script constants; parser is broken"
    return constants


def raw_closure_literals(source, sdk_values):
    """[(literal_text, full_sdk_name_or_None), ...] for every quoted `anon@N@...`
    string in `source`. `full_sdk_name` is the SDK's own value once the literal
    is matched (adding the `gml_Script_` prefix first, when the literal lacks
    one), or None when nothing in the SDK matches it.
    """
    entries = []
    for match in RAW_CLOSURE_RE.finditer(source):
        raw = match.group(1)
        full = raw if raw.startswith("gml_Script_") else "gml_Script_" + raw
        entries.append((raw, full if full in sdk_values else None))
    return entries


def sdk_closure_refs(source, sdk_constants):
    """[(identifier, full_sdk_name_or_None), ...] for every `HeroSiege::Scripts::`
    closure identifier in `source`. None when the identifier isn't in the SDK
    at all (a `_Index` suffix, if present, is stripped before the lookup).
    """
    entries = []
    for match in SDK_CLOSURE_REF_RE.finditer(source):
        ident = match.group(1)
        if ident.endswith("_Index"):
            ident = ident[: -len("_Index")]
        entries.append((ident, sdk_constants.get(ident)))
    return entries


def closure_names(source, sdk_constants):
    """[(spelling, full_sdk_name_or_None), ...] for every closure name `source`
    compiles, raw literal or SDK reference alike."""
    return (raw_closure_literals(source, set(sdk_constants.values()))
            + sdk_closure_refs(source, sdk_constants))


# `using namespace HeroSiege::Scripts;` or a namespace alias for it would let a
# future call site spell a closure name unqualified - the whole point of the
# contract above is that every closure name traces back to an explicit
# `HeroSiege::Scripts::` reference this scanner can find.
SDK_NAMESPACE_MISUSE_RE = re.compile(
    r"\busing\s+namespace\s+HeroSiege::Scripts\s*;"
    r"|\bnamespace\s+\w+\s*=\s*HeroSiege::Scripts\s*;"
)

# HookOneScript/HookOneScriptTable prepend "gml_Script_" themselves (see
# SdkShortScriptName above them in the plugin). A raw `HeroSiege::Scripts::X.data()`
# argument - skipping that helper - hands them the value that ALREADY has the
# prefix, so the installed hook name doubles it and silently never resolves.
HOOK_CALL_RAW_SDK_CONSTANT_RE = re.compile(
    r"HookOneScript(?:Table)?\(\s*HeroSiege::Scripts::\w+\.data\(\)"
)

# CallGameScriptEx/GetNamedRoutinePointer need the FULL name (the constant's own
# value); SdkShortScriptName strips the prefix HookOneScript needs instead, so
# handing its result to one of these full-name APIs silently looks up the wrong name.
FULL_NAME_API_GIVEN_SHORT_NAME_RE = re.compile(
    r"CallGameScriptEx\([^,]*,\s*SdkShortScriptName\("
    r"|GetNamedRoutinePointer\(\s*SdkShortScriptName\("
)


def scanner_misuse_violations(source):
    """[description, ...] for every misuse of the HeroSiege::Scripts contract in
    `source`: an alias/using-directive that would let a name go unqualified, a
    HookOneScript*/HookOneScriptTable call given an already-prefixed full name
    (doubles "gml_Script_"), or a full-name API given the short form instead.
    Empty when `source` uses the contract correctly.
    """
    violations = []
    for match in SDK_NAMESPACE_MISUSE_RE.finditer(source):
        violations.append(f"HeroSiege::Scripts alias/using-directive: {match.group(0)!r}")
    for match in HOOK_CALL_RAW_SDK_CONSTANT_RE.finditer(source):
        violations.append("HookOneScript*/HookOneScriptTable given a raw SDK constant "
                           f"instead of SdkShortScriptName(...): {match.group(0)!r}")
    for match in FULL_NAME_API_GIVEN_SHORT_NAME_RE.finditer(source):
        violations.append("full-name API given a short name via SdkShortScriptName(...) "
                           f"instead of the constant's own gml_Script_ value: {match.group(0)!r}")
    return violations


def player_build_text(text):
    return strip_comments(strip_research_blocks(text))


def player_build_source(path):
    return player_build_text(path.read_text(encoding="utf-8"))


class ReleaseHookContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.plugin = PLUGIN_PATH.read_text(encoding="utf-8")
        cls.map_reveal_header = MAP_REVEAL_HEADER_PATH.read_text(encoding="utf-8")
        cls.stats_header = STATS_HEADER_PATH.read_text(encoding="utf-8")
        cls.density_header = DENSITY_HEADER_PATH.read_text(encoding="utf-8")

    def test_every_toggle_command_treats_zero_as_off(self):
        # `census 0` used to turn census ON: its handler only tested "off", so a
        # zero fell through to the enable branch and answered ACIK. Found the
        # expensive way on 2026-09-15 - census costs ~72 ms per run and silently
        # contaminated a perf measurement until the reply was read closely.
        #
        # Asserted across every toggle rather than just census, because the bug
        # was an inconsistency with the house idiom, and one handler drifting
        # from it is exactly how it happened.
        for verb, marker in (
            ("census", 'v == "off" || v == "0"'),
            ("objidxprobe", 'oiArg == "off" || oiArg == "0"'),
            ("beaconspawn", 'v == "off" || v == "0"'),
        ):
            self.assertIn(marker, self.plugin,
                          f"{verb} must accept 0 as off, like every other toggle")

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
        self.assertIn("EstForceTick(fc);", frame)
        # Both halves early-return while nothing is forced, so the all-off
        # baseline costs one comparison per frame and no runtime call at all.
        tick = function_body(self.plugin, "static void EstForceTick(")
        self.assertIn("if (g_EstForce.empty() || !g_Yytk) return;", tick)
        est = function_body(self.plugin, "static void EstForceApply()")
        self.assertIn("if (g_EstForce.empty() || !g_Yytk) return;", est)
        self.assertRegex(
            frame,
            r"#ifndef FORGEPACT_RELEASE\s*"
            r"static bool f5p[\s\S]*?f5p = f5;\s*#endif",
        )
        self.assertNotIn("if ((fc % 60) == 0) KonsoluGizle();", frame)

    def test_stall_watchdog_never_reaches_the_player_build(self):
        # A diagnostic, not a feature: a thread waking twice a second that
        # suspends the game's frame thread during a stall, plus a per-frame
        # heartbeat. The freeze it was built to name was concluded not to be
        # ForgePact, and a player gains nothing from paying for it.
        release = strip_comments(strip_research_blocks(self.plugin))
        for name in ("StallWatchdogLoop", "StartStallWatchdog", "g_LastFrameTickMs",
                     "g_FrameThread", "SuspendThread", "GetThreadContext"):
            self.assertNotIn(name, release)
        # Positive control: the same helper keeps the research build's copy.
        research = strip_comments(self.plugin)
        self.assertIn("StartStallWatchdog();", research)
        self.assertIn("g_LastFrameTickMs.store(GetTickCount64());", research)

    def test_the_object_index_struct_read_never_reaches_the_player_build(self):
        # Finding 8 plants an instrument, not a feature. GetMembers() is not a
        # field read - it calls GetBuiltin("id") per invocation and picks one
        # of three union layouts by comparing m_ID, and on a build where none
        # match it returns a layout that is not this one. A garbage object
        # index makes IsCreatorObject() false, so map reveal and the Beacon
        # would report armed and silently stop lying. AGENTS.md wants a
        # positive control on this runtime first; the probe is that control.
        player = strip_research_blocks(self.plugin)
        self.assertNotIn("GetMembers(", player)
        self.assertNotIn("ObjIdxProbe", player)
        self.assertNotIn("QueryPerformanceCounter", player)

    def test_the_player_build_still_reads_object_index_through_the_builtin(self):
        hook = function_body(strip_research_blocks(self.plugin),
                             "static void Hook_distance_to_object(")
        self.assertIn('CallBuiltin("variable_instance_get", { inst, RValue("object_index") })', hook)
        self.assertIn("IsCreatorObject((int)oi.ToDouble())", hook)

    def test_the_probe_reports_both_counters_and_both_timings(self):
        # A probe that cannot produce timings closes finding 8 as "measured,
        # not worth it", so the numbers it must print are pinned here.
        report = function_body(self.plugin, "static void ObjIdxProbeReport()")
        for field in ("agree=", "disagree=", "getmembers-failed=",
                      "variable_instance_get median=", "GetMembers median=", "ratio="):
            self.assertIn(field, report)
        # 0/0 has measured nothing; it must not be readable as "no
        # disagreements found".
        self.assertIn("measured NOTHING", report)
        probe = function_body(self.plugin, "static void ObjIdxProbe(")
        for counter in ("g_ObjIdxAgree", "g_ObjIdxDisagree", "g_ObjIdxFailed"):
            self.assertIn(counter, probe)

    def test_the_probe_is_off_until_it_is_asked_for(self):
        # GetMembers() picking an arm this build does not have is documented to
        # return a wrong layout, and if the real CInstance is smaller than that
        # arm the field read is past the allocation - which /EHsc means the
        # probe's own catch (...) will not catch. Backing out of that has to
        # cost a command, not a rebuild, so the probe does not run until it is
        # switched on.
        self.assertIn("static std::atomic<bool> g_ObjIdxProbeOn{ false };", self.plugin)
        self.assertIn("g_ObjIdxProbeOn.load()", function_body(self.plugin, "static void ObjIdxProbe("))
        self.assertIn('oiArg == "on"', self.plugin)
        # ...and `reset` must not be readable as "stop" - it only clears.
        reset = function_body(self.plugin, "static void ObjIdxProbeReset()")
        self.assertNotIn("g_ObjIdxProbeOn", reset)

    def test_the_probe_says_what_its_counts_are_scoped_to(self):
        # It runs after the beacon/reveal early-out and before IsCreatorObject,
        # so the number is instances through the hook while a lie was wanted -
        # not creators, which is how the guide would otherwise read it.
        report = function_body(self.plugin, "static void ObjIdxProbeReport()")
        self.assertIn("while a lie is wanted", report)
        self.assertIn("not creators", report)

    def test_the_probe_command_is_not_a_player_command(self):
        allowlist = re.search(r"kPlayerCommands\s*=\s*\{(?P<body>.*?)\};",
                              self.plugin, re.DOTALL)
        self.assertIsNotNone(allowlist)
        self.assertNotIn("objidxprobe", allowlist.group("body"))
        self.assertNotIn("objidxprobe", strip_research_blocks(self.plugin))

    def test_the_head_label_hook_install_is_untouched(self):
        # Not changed here; the live session measures it, and "leave it alone"
        # is a legitimate outcome. Pinned so this change cannot drift into it.
        body = function_body(self.plugin, "static void InstallHeadLabelHook()")
        self.assertIn("g_HhLabelHookAttempted", body)
        self.assertIn('HookOneScript("DrawHudBuffs"', body)
        init = function_body(self.plugin, "EXPORTED AurieStatus ModuleInitialize")
        self.assertIn("InstallHeadLabelHook();", init)

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

    def test_routine_fallback_validates_the_pointer_before_calling_it(self):
        # RefreshItemHash's last-resort route reads a function pointer off a
        # game struct (CScript::m_Functions) and calls it directly. /EHsc does
        # not turn an access violation into a C++ exception, so the try/catch
        # around it is not a guard - the address has to be validated as code
        # inside Hero_Siege.exe BEFORE the call, the same way HookOneScript
        # validates a table entry before patching it (the test above). Without
        # this the check could be reordered past the call by a later edit with
        # every other test still green.
        body = function_body(self.plugin, "static bool RefreshItemHash(")
        self.assertIn("AddrIsExecutableInModule(GetModuleHandleA(nullptr)", body)
        self.assertLess(body.index("AddrIsExecutableInModule"), body.index("fnp("))

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
        self.assertIn("HeroSiege::Scripts::gml_Script_anon_119_gml_Object_Spawn_Shadow_Realm_obj_Create_0", install)
        self.assertIn("HeroSiege::Scripts::gml_Script_anon_97_gml_Object_Spawn_Chaos_Tower_obj_Create_0", install)

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


class ClosureNameContractTests(unittest.TestCase):
    """Every closure name (`anon@N@...`) the player build compiles must be a
    name hs-game-sdk still has, so a game update that moves a closure breaks
    the build when the SDK is regenerated instead of silently failing a name
    lookup at runtime. See `AGENTS.md`, "HS Game SDK Usage".

    Also rejects three ways of reaching an SDK constant that would defeat the
    contract even though the name itself is fine: an unqualified `using
    namespace`/alias, a HookOneScript*/HookOneScriptTable call given the
    already-prefixed full name instead of the short one, and a full-name API
    (CallGameScriptEx/GetNamedRoutinePointer) given the short name instead.
    """

    @classmethod
    def setUpClass(cls):
        cls.sdk_constants = load_sdk_script_constants()
        cls.plugin_source = PLUGIN_PATH.read_text(encoding="utf-8")

    def test_player_build_closure_names_all_match_the_sdk(self):
        sources = [player_build_source(PLUGIN_PATH)]
        sources += [player_build_source(header) for header in sorted(PLUGIN_HEADER_DIR.glob("*.hpp"))]
        unmatched = []
        violations = []
        for source in sources:
            unmatched.extend(spelling for spelling, full in closure_names(source, self.sdk_constants)
                              if full is None)
            violations.extend(scanner_misuse_violations(source))
        self.assertEqual([], unmatched, f"closure names the player build compiles but the SDK does not have: {unmatched}")
        self.assertEqual([], violations, f"HeroSiege::Scripts contract misuse in the player build: {violations}")

    def test_player_build_closure_scan_finds_the_known_sites(self):
        entries = closure_names(player_build_source(PLUGIN_PATH), self.sdk_constants)
        self.assertGreaterEqual(len(entries), 6, entries)
        resolved = {full for _, full in entries if full}
        for expected in (
            "gml_Script_GenerateItemHash@anon@4645@s_ItemInstanceStruct@InventoryV2Funcs",
            "gml_Script_anon@119@gml_Object_Spawn_Shadow_Realm_obj_Create_0",
            "gml_Script_anon@97@gml_Object_Spawn_Chaos_Tower_obj_Create_0",
            "gml_Script_anon@119@gml_Object_Spawn_Abyss_obj_Create_0",
        ):
            self.assertIn(expected, resolved)

    def test_closure_scan_matches_every_research_literal(self):
        # Deliberately NOT strip_research_blocks: this is the positive control
        # proving the scanner can see the research-only literals (CITRACE_HOOK_NAMED,
        # HookOneScriptTable, CINAT_ENTRY, the creator-trace anon@849) through the
        # same regex the player-build test relies on to report an unmatched name at all.
        # "Research literal" here means exactly that: an occurrence present in the
        # full source but not in what the player build compiles, found by diffing the
        # two scans rather than by guessing which macros are research-only, so this
        # stays correct if the research/player split moves and independent of the
        # RefreshItemHash fallback fix this same file makes elsewhere.
        sdk_values = set(self.sdk_constants.values())
        full_source = strip_comments(self.plugin_source)
        full_entries = raw_closure_literals(full_source, sdk_values)
        player_entries = raw_closure_literals(player_build_text(self.plugin_source), sdk_values)
        research_counts = Counter(s for s, _ in full_entries) - Counter(s for s, _ in player_entries)
        full_lookup = dict(full_entries)
        research_entries = [(spelling, full_lookup[spelling])
                             for spelling, count in research_counts.items() for _ in range(count)]
        self.assertGreaterEqual(len(research_entries), 40, research_entries)
        unmatched = [spelling for spelling, full in research_entries if full is None]
        self.assertEqual([], unmatched)
        self.assertTrue(any(spelling.startswith("gml_Script_") for spelling, _ in research_entries),
                         "expected at least one research literal with the gml_Script_ prefix")
        self.assertTrue(any(not spelling.startswith("gml_Script_") for spelling, _ in research_entries),
                         "expected at least one research literal without the gml_Script_ prefix")

    def test_closure_check_rejects_a_stale_name(self):
        synthetic = "\n".join([
            'const char* stale = "gml_Script_GenerateItemHash@anon@4638@s_ItemInstanceStruct@InventoryV2Funcs";',
            'const char* wrong_offset = "anon@118@gml_Object_Spawn_Shadow_Realm_obj_Create_0";',
            "auto bogus_sdk = HeroSiege::Scripts::gml_Script_anon_1_gml_Object_Bogus_obj_Create_0;",
            'const char* correct = "anon@119@gml_Object_Spawn_Shadow_Realm_obj_Create_0";',
            "#ifndef FORGEPACT_RELEASE",
            'const char* research_bogus = "anon@2@gml_Object_Totally_Bogus_obj_Create_0";',
            "#endif",
            # Round 2: each new scanner rule gets a case here too, using a real,
            # valid SDK identifier so the violation is isolated to the call
            # SHAPE, not name validity (that part is already covered above).
            "using namespace HeroSiege::Scripts;",
            "namespace HSS = HeroSiege::Scripts;",
            'HookOneScript(HeroSiege::Scripts::gml_Script_anon_119_gml_Object_Spawn_Shadow_Realm_obj_Create_0.data(), "id", nullptr, nullptr);',
            "g_Yytk->CallGameScriptEx(res, SdkShortScriptName(HeroSiege::Scripts::gml_Script_anon_119_gml_Object_Spawn_Shadow_Realm_obj_Create_0), self, self, {});",
            "g_Yytk->GetNamedRoutinePointer(SdkShortScriptName(HeroSiege::Scripts::gml_Script_anon_97_gml_Object_Spawn_Chaos_Tower_obj_Create_0), &p);",
            # Correct usage of the same helper must NOT be flagged by any new rule.
            'HookOneScript(SdkShortScriptName(HeroSiege::Scripts::gml_Script_anon_119_gml_Object_Spawn_Abyss_obj_Create_0), "id2", nullptr, nullptr);',
        ])
        player_source = player_build_text(synthetic)
        entries = closure_names(player_source, self.sdk_constants)
        unmatched = [spelling for spelling, full in entries if full is None]
        matched = [spelling for spelling, full in entries if full is not None]
        self.assertEqual(3, len(unmatched), unmatched)
        self.assertFalse(any("Totally_Bogus" in spelling for spelling in unmatched),
                          "the research-only bogus literal must not even be seen by the player scan")
        # The correct raw literal, plus the (valid) SDK identifiers the round-2
        # misuse cases below reference - those calls are still shaped wrong,
        # but the identifiers themselves are real, so closure_names() alone
        # must not flag them; that is scanner_misuse_violations()'s job below.
        self.assertIn("anon@119@gml_Object_Spawn_Shadow_Realm_obj_Create_0", matched)

        violations = scanner_misuse_violations(player_source)
        self.assertEqual(5, len(violations), violations)
        self.assertEqual(2, sum("alias/using-directive" in v for v in violations), violations)
        self.assertEqual(1, sum("raw SDK constant instead of SdkShortScriptName" in v for v in violations), violations)
        self.assertEqual(2, sum("short name via SdkShortScriptName" in v for v in violations), violations)
        # The one correctly-shaped HookOneScript(SdkShortScriptName(...)) call
        # above must not itself be counted in any of the three buckets.
        self.assertFalse(any("Abyss_obj_Create_0.data()" in v for v in violations), violations)


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
