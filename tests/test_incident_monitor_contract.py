"""The incident monitor's wiring (issue #76), read from the source.

The behaviour is tests/test_incident_monitor_behavior.py's; this pins what a
harness cannot see. The header stays game-independent. FrameCallback takes the
incident frame boundary right after the frame profiler's. The `incident` verb
is a player command and a standalone early return. The monitor thread's code
never reaches into the game's runtime or YYToolkit. The player build reads the
high-resolution clock only in the incident monitor. The clean-shutdown
marker is written by a by-name ExitProcess hook first and the static
destructor second, with Win32 file calls only. Nothing is left for the C
runtime to destroy at ExitProcess. The thresholds are the plan's (D5). Every
player-build hook installer tags its hook with the in-hook id (D8), and
`incident stat` reports what the monitor judged and tagged (D16), whose first
line the live checks look for. out.txt has one writer at a time. The keys the
plugin reads from the panel's files are the shared fixture's and the panel's.
Every call into the game's original from a timed body sits inside the guard,
and the plugin shows the player no notice of any report.
"""
import json
import re
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from test_release_hook_contract import function_body, strip_comments, strip_research_blocks  # noqa: E402

ROOT = Path(__file__).resolve().parents[1]
PLUGIN = ROOT / "plugin" / "ModuleMain.cpp"
HEADER = ROOT / "plugin" / "include" / "ForgePact" / "IncidentMonitor.hpp"
DROPS = ROOT / "plugin" / "include" / "ForgePact" / "DropManager.hpp"
PANEL = ROOT / "src" / "forgepact.py"
FIXTURES = ROOT / "tests" / "fixtures" / "incident"
BUILD_BAT = ROOT / "plugin_build" / "build.bat"

REGION_START = "// ===== Incident monitor (issue #76) ====="
REGION_END = "// ===== end of the incident monitor (issue #76) ====="
THREAD_START = "// ---- incident monitor thread: Win32 and files only ----"
THREAD_END = "// ---- end of the incident monitor thread ----"


def read(path):
    return path.read_text(encoding="utf-8").replace("\r\n", "\n")


def code_lines(body):
    return [line.strip() for line in strip_comments(body).splitlines() if line.strip()]


# A call into the game's original from a hook body: the trampoline a hook
# installer handed back (g_Orig_<Name>, DropManager's m_Orig_<Name> and the
# macro's m_Orig_##NAME) or a routine passed in as `orig`.
ORIGINAL_CALL = re.compile(r"\b(?:g_Orig_\w+|m_Orig_\w*(?:##\w+)?|orig)\s*\(")
# The guard that pauses ForgePact's clock around one such call.
GUARD_CALL = re.compile(r"\b(?:FP_GAME_ORIGINAL|FP_DROP_GAME_ORIGINAL)\s*\(")


def matching_paren(code, open_index):
    depth = 0
    for index in range(open_index, len(code)):
        if code[index] == "(":
            depth += 1
        elif code[index] == ")":
            depth -= 1
            if depth == 0:
                return index
    raise AssertionError("unbalanced parenthesis at " + code[open_index:open_index + 80])


def scoped_bodies(code, scope, header):
    """{name: body} for every function body in `code` that opens `scope`.

    The body is the brace block after the nearest line before the scope that
    matches `header` (a function's first line), so a scope opened inside a
    nested block (FrameCallback's) yields the whole function."""
    bodies = {}
    lines = list(re.finditer(r"[^\n]*\n?", code))
    for found in re.finditer(scope, code):
        start = None
        for line in lines:
            if line.start() > found.start():
                break
            if re.match(header, line.group(0)):
                start = line.start()
        if start is None:
            raise AssertionError("no function encloses " + code[found.start():found.start() + 80])
        brace = code.index("{", start)
        depth = 0
        for index in range(brace, len(code)):
            if code[index] == "{":
                depth += 1
            elif code[index] == "}":
                depth -= 1
                if depth == 0:
                    break
        if not brace < found.start() < index:
            raise AssertionError("the scope is outside the body found for it: " + code[start:start + 80])
        name = re.search(r"([A-Za-z_]\w*(?:##\w+)?)\s*\(", code[start:brace]).group(1)
        bodies[name] = code[brace + 1:index]
    return bodies


def brace_block(code, start_text):
    """The text inside the braces that open after the one `start_text`."""
    start = code.index(start_text)
    brace = code.index("{", start)
    depth = 0
    for index in range(brace, len(code)):
        if code[index] == "{":
            depth += 1
        elif code[index] == "}":
            depth -= 1
            if depth == 0:
                return code[brace + 1:index]
    raise AssertionError("unbalanced braces after " + start_text)


# The one-time setup block in FrameCallback (test_relic_filter_contract.py
# pins the text).
SETUP_BLOCK = "if (!g_Setup && fc > 300)"
SETUP_SCOPE = "IncidentScope incidentSetup(IncidentMod::setup);"
# D17: the menu rooms, by the SDK's enum names (ctx "The menu-room list").
MENU_ROOMS = ("Init_rm", "Game_Start_rm", "Login_rm", "Login_Valhalla_rm", "Main_Menu_rm",
              "Main_Menu_Valhalla_rm", "Char_Select_rm", "Chose_rm")
SDK_ROOMS = ROOT.parent / "hs-game-sdk" / "cpp" / "include" / "hs_game_sdk" / "rooms.hpp"


def unguarded_original_calls(body):
    """Each game-original call in `body` that no guard's parentheses enclose."""
    code = strip_comments(body)
    guarded = []
    for guard in GUARD_CALL.finditer(code):
        open_index = code.index("(", guard.start())
        guarded.append((open_index, matching_paren(code, open_index)))
    return [m.group(0) for m in ORIGINAL_CALL.finditer(code)
            if not any(a < m.start() < b for a, b in guarded)]


class IncidentMonitorContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.plugin = read(PLUGIN)
        cls.header = read(HEADER)
        cls.drops = read(DROPS)

    def region(self, start, end):
        a = self.plugin.index(start)
        b = self.plugin.index(end, a)
        return self.plugin[a:b]

    def test_the_header_is_game_independent(self):
        code = strip_comments(self.header)
        for game in ("RValue", "g_Yytk", "YYTK", "CInstance", "Aurie", "CallBuiltin", "GetBuiltin",
                     "SuspendThread", "GetThreadContext", "std::thread"):
            self.assertNotIn(game, code, game)
        # <windows.h> without NOMINMAX in ModuleMain.cpp breaks these.
        self.assertIsNone(re.search(r"std::(min|max)\s*\(", code))
        self.assertNotIn("numeric_limits", code)

    def test_the_frame_tick_is_the_second_statement(self):
        frame = function_body(self.plugin, "void FrameCallback(FWFrame& FrameContext)")
        code = code_lines(frame)
        # FrameProfilerTick stays first (test_frame_profiler.py); the incident
        # boundary comes right after it, before any other ForgePact work.
        self.assertEqual(code[1:3], ["FrameProfilerTick();", "IncidentFrameTick();"])
        self.assertEqual(frame.count("IncidentFrameTick();"), 1)
        self.assertEqual(code[3], "IncidentScope incidentFrame(IncidentMod::frame);")
        tick = strip_comments(function_body(self.plugin, "static void IncidentFrameTick()"))
        # The boundary every frame; the once-a-second reads only after setup.
        self.assertLess(tick.index("inc::g_Accounting.OnFrame(now);"), tick.index("if (!g_Setup) return;"))
        self.assertLess(tick.index("if (!g_Setup) return;"), tick.index("CurrentRoomKey()"))
        # The context comes from the frame profiler's adapter: no new built-in
        # literal here (test_frame_profiler.py pins that region's set).
        self.assertIn("FrameProfContext(context)", tick)
        region = strip_comments(self.region(REGION_START, REGION_END))
        self.assertNotIn('CallBuiltin("', region)
        self.assertNotIn('GetBuiltin("', region)

    def test_the_verb_is_a_player_command_and_an_early_return(self):
        start = self.plugin.index("kPlayerCommands = {")
        block = self.plugin[start:self.plugin.index("};", start)]
        self.assertIn('"incident"', block)
        run = function_body(self.plugin, "static void RunCommand(const std::string& line)")
        self.assertIn('if (lc == "incident") { IncidentCommand(rest); return; }', run)
        self.assertEqual(self.plugin.count('"incident"'), 2)
        command = strip_comments(function_body(self.plugin, "static void IncidentCommand(const std::string& rest)"))
        # `stat` reads the monitor's last published counters and changes nothing.
        self.assertIn("Monitor::Instance().Snapshot()", command)
        self.assertNotIn("Arm(", command)

    def test_the_monitor_thread_never_touches_the_runtime(self):
        thread = strip_comments(self.region(THREAD_START, THREAD_END))
        for forbidden in ("Out(", "g_Yytk", "CallBuiltin", "GetBuiltin", "CInstance", "RValue", "PrintInfo",
                          "SuspendThread", "GetThreadContext", "CurrentRoomKey", "FrameProfContext"):
            self.assertNotIn(forbidden, thread, forbidden)
        run = strip_comments(function_body(self.plugin, "static void IncidentMonitorRun() noexcept"))
        self.assertIn(run, thread)
        self.assertIn("OutRaw(", run)
        self.assertIn("Sleep(inc::kWakeMs);", run)
        # The whole region: the frame tick and `incident stat` are the named
        # exceptions that may print through Out(), on the frame thread.
        region = strip_comments(self.region(REGION_START, REGION_END))
        for forbidden in ("g_Yytk", "CallBuiltin", "CInstance", "RValue"):
            self.assertNotIn(forbidden, region, forbidden)
        outside = region.replace(thread, "")
        for name in ("static void IncidentCommand(const std::string& rest)", "static void IncidentMonitorStart()"):
            outside = outside.replace(function_body(region, name), "")
        self.assertNotIn("Out(", outside)
        # OutRaw is in both builds now: it sits outside every research block.
        self.assertIn("static void OutRaw(const std::string& s)", strip_research_blocks(self.plugin))

    def test_qpc_lives_only_in_the_incident_region(self):
        # What the player build compiles, comments gone: the region runs from
        # its first function to RunCommand, which follows it.
        player = strip_comments(strip_research_blocks(self.plugin))
        first = player.index("static void IncidentFrameTick()")
        last = player.index("static void RunCommand(const std::string& line)", first)
        for match in re.finditer(r"QueryPerformanceCounter", player):
            self.assertTrue(first <= match.start() < last, player[match.start() - 200:match.start() + 50])
        # Positive control: the header is where the clock is read.
        self.assertIn("QueryPerformanceCounter(&v);", self.header)

    def test_the_shutdown_destructor_uses_win32_only(self):
        # D14: one Write, called by the ExitProcess hook and by the destructor;
        # both run while the process is ending, so Win32 file calls only.
        write = strip_comments(function_body(self.header, "bool Write(ShutdownRoute route) noexcept"))
        destructor = strip_comments(function_body(self.header, "~ShutdownMarker()"))
        for body in (write, destructor):
            for forbidden in ("ofstream", "Out(", "fopen", "std::string", "new ", "malloc", "printf", "LoadLibrary",
                              "std::filesystem"):
                self.assertNotIn(forbidden, body, forbidden)
        for needed in ("CreateFileA(", "WriteFile(", "CloseHandle(", "kCleanShutdownLine", "kCleanShutdownDetachLine"):
            self.assertIn(needed, write, needed)
        # Once only, whichever route comes first.
        self.assertIn("m_Written.exchange(true", write)
        self.assertEqual(code_lines(destructor), ["Write(ShutdownRoute::detach);"])
        self.assertIn('inline constexpr char kCleanShutdownLine[] = "==== clean shutdown ====";', self.header)
        self.assertIn('inline constexpr char kCleanShutdownDetachLine[] = "==== clean shutdown (detach) ====";', self.header)
        self.assertIn('inline constexpr char kCleanShutdownPrefix[] = "==== clean shutdown";', self.header)
        clean = strip_comments(function_body(self.header, "inline bool SessionEndedCleanly(const std::string& session)"))
        self.assertIn("kCleanShutdownPrefix", clean)
        self.assertIn("inline ShutdownMarker g_ShutdownMarker;", self.header)
        start = strip_comments(function_body(self.plugin, "static void IncidentMonitorStart()"))
        self.assertIn("g_ShutdownMarker.Arm(OutPath().c_str());", start)

    def test_process_exit_is_safe(self):
        # A heap-held singleton and an ExitSafeThread: nothing for the CRT to
        # destroy at ExitProcess while the monitor thread was terminated.
        self.assertIn("static Monitor* monitor = new Monitor();", self.header)
        self.assertIn("ExitSafeThread m_Thread;", self.header)
        self.assertIn("inline constinit Accounting g_Accounting;", self.header)
        self.assertIn("static_assert(std::is_trivially_destructible_v<Accounting>", self.header)
        # Started from ModuleInitialize once the frame callback is registered.
        init = function_body(self.plugin, "EXPORTED AurieStatus ModuleInitialize(")
        self.assertLess(init.index("CreateCallback(Module, EVENT_FRAME"), init.index("IncidentMonitorStart();"))
        self.assertEqual(self.plugin.count("IncidentMonitorStart();"), 1)
        self.assertIn("Start(&IncidentMonitorRun)", self.plugin)

    def test_thresholds(self):
        expected = {
            "kHitchMs": "250.0", "kSustainedMult": "2.5", "kSustainedMs": "2000.0", "kFreezeMs": "3000.0",
            "kRoomGraceMs": "5000.0", "kRingFrames": "600", "kEpisodeGapMs": "30000.0", "kMaxEpisodes": "50",
            "kBundleGapMs": "300000.0", "kMaxBundles": "10", "kKeepReports": "10",
        }
        for name, value in expected.items():
            self.assertRegex(self.header, rf"inline constexpr \w+ {name} = {re.escape(value)};", name)

    def test_every_named_body_carries_its_scope(self):
        # D8: one scope line at the top of each body, nothing reordered.
        for signature, scope in (
            ("static void DensityCopiesTick()", "IncidentScope incidentScope(IncidentMod::density);"),
            ("static void DoMultiCreate(", "IncidentSampledScope incidentScope(IncidentMod::density);"),
            ("static void GemsTick(uint32_t frame)", "IncidentScope incidentScope(IncidentMod::gems);"),
            ("static RValue& Hook_DrawHudBuffs(", "IncidentScope incidentScope(IncidentMod::hudlabels);"),
            ("static RValue& Hook_DropRelic(", "IncidentScope incidentScope(IncidentMod::drops);"),
            ("static void AutoProspectTick()", "IncidentScope incidentScope(IncidentMod::autoprospect);"),
            ("static void StashMoveAllTick()", "IncidentScope incidentScope(IncidentMod::stashmoveall);"),
        ):
            self.assertEqual(code_lines(function_body(self.plugin, signature))[0], scope, signature)
        # Far sleep's off test stays first (test_far_sleep_contract.py), so
        # its scope follows it: switched off, the tick still costs nothing.
        self.assertEqual(code_lines(function_body(self.plugin, "static void FarSleepTick()"))[1:3],
                         ["if (!fs.Enabled() && !fs.Draining()) return;",
                          "IncidentScope incidentScope(IncidentMod::farsleep);"])
        frame = strip_comments(function_body(self.plugin, "void FrameCallback(FWFrame& FrameContext)"))
        self.assertLess(frame.index("IncidentScope incidentScope(IncidentMod::mapreveal);"),
                        frame.index("ForgePact::MapRevealManager::Instance().OnFrame(g_RuntimeFrame);"))
        self.assertIn("IncidentScope incidentScope(IncidentMod::miner); ForgePact::MinerHelmet::Tick();", frame)
        self.assertLess(frame.index("IncidentScope incidentScope(IncidentMod::ipc);"),
                        frame.index("ForgePact::IpcServer::Instance().PollCommands();"))
        # D18: the one-time setup is its own row, opened first in its block,
        # so its seconds are never the `frame` row's.
        setup = brace_block(frame, SETUP_BLOCK)
        self.assertEqual(code_lines(setup)[0], SETUP_SCOPE)
        self.assertLess(setup.index(SETUP_SCOPE), setup.index("LoadConfig();"))
        # The dropmult bodies in DropManager.hpp: the macro's and the three
        # written out, each starting with the scope.
        self.assertIn("static RValue& Hook_##NAME(CInstance* S, CInstance* O, RValue& R, int argc, RValue** A) { \\\n"
                      "        FP_DROP_INCIDENT_SCOPE(); \\\n", self.drops)
        for name in ("Hook_DropGold", "Hook_DropMonsterGold", "Hook_DropKeys"):
            body = function_body(self.drops, f"static RValue& {name}(")
            self.assertEqual(code_lines(body)[0], "FP_DROP_INCIDENT_SCOPE();", name)
        self.assertIn("#define FP_DROP_INCIDENT_SCOPE() ::ForgePact::Incident::IncidentScope "
                      "incidentScope(::ForgePact::Incident::Mod::drops)", self.drops)
        self.assertIn('#include "IncidentMonitor.hpp"', self.drops)
        self.assertIn("#define FORGEPACT_INCIDENT_MONITOR_HPP 1", self.header)
        # The plugin includes the header before DropManager.hpp.
        self.assertLess(self.plugin.index("#include <ForgePact/IncidentMonitor.hpp>"),
                        self.plugin.index("#include <ForgePact/DropManager.hpp>"))

    def test_the_hash_links_by_pragma_and_build_bat_is_unchanged(self):
        region = self.region(REGION_START, REGION_END)
        self.assertIn('#pragma comment(lib, "bcrypt.lib")', region)
        self.assertIn("BCryptHash(", region)
        bat = BUILD_BAT.read_text(encoding="utf-8")
        cl = [line for line in bat.splitlines() if line.startswith("cl ")]
        self.assertEqual(len(cl), 1)
        self.assertTrue(cl[0].endswith("/link /DLL user32.lib"), cl[0])

    # ---- replan 3: D8's installer tag, D14's exit hook, D16's honest counters

    def test_every_installer_tags_its_hook(self):
        for signature, thunks in (("static bool HookOneScript(", "TaggedThunks<PFUNC_YYGMLScript>::Tagged("),
                                  ("static bool HookBuiltin(", "TaggedThunks<TRoutine>::Tagged(")):
            body = strip_comments(function_body(self.plugin, signature))
            found = re.search(r"\b(\w+)\s*=\s*" + re.escape(thunks), body)
            self.assertIsNotNone(found, signature)
            tagged = found.group(1)
            # The caller's dest reaches the thunk table and nothing else.
            self.assertEqual(len(re.findall(r"\bdest\b", body)), 1, signature)
            self.assertRegex(body, re.escape(thunks) + r"[^;]*\bdest\b", signature)
            hooks = re.findall(r"MmCreateHook\(([^;]*)\);", body)
            self.assertEqual(len(hooks), 1, signature)
            detour = [a.strip() for a in hooks[0].split(",")]
            self.assertIn(tagged, detour[3], signature)
            if "HookOneScript" in signature:
                table = re.findall(r"m_ScriptFunction\s*=\s*([^;]+);", body)
                self.assertEqual(len(table), 1)
                self.assertIn(tagged, table[0])
                # auto-prospect's contract: the detour, then the report of it.
                self.assertLess(body.index("MmCreateHook"), body.index("if (nativeOut) *nativeOut = true;"))
                # The body still gets the trampoline (Prove the Instrument).
                self.assertIn("*origOut = reinterpret_cast<PFUNC_YYGMLScript>(tramp);", body)
        slot = strip_comments(function_body(self.plugin, "static void InstallSlotHook()"))
        found = re.search(r"\b(\w+)\s*=\s*TaggedThunks<PFUNC_YYGMLScript>::Tagged\(\"bp_getslot\", HookGetSlotBloodPact\)",
                          slot)
        self.assertIsNotNone(found)
        detour = [a.strip() for a in re.search(r"MmCreateHook\(([^;]*)\);", slot).group(1).split(",")]
        self.assertIn(found.group(1), detour[3])
        self.assertNotIn("HookGetSlotBloodPact", detour[3])
        # A native-signature hook the thunks do not cover tags itself...
        self.assertEqual(code_lines(function_body(self.plugin, "static double __cdecl HookProtGet(double key)"))[0],
                         'IncidentHookTag incidentHookTag("fp_acgetvar");')
        # ...and the ten pool detours are counted as untagged, per success.
        pool = strip_comments(function_body(self.plugin, "static bool PreparePopulationCapacity()"))
        self.assertIn("g_Accounting.CountUntagged()", pool)
        self.assertLess(pool.index("MmCreateHook("), pool.index("g_Accounting.CountUntagged()"))
        self.assertIn("kHookSlots = 128;", self.header)

    def test_the_stat_line_prefix_is_the_live_marker(self):
        body = strip_comments(function_body(self.header, "inline std::vector<std::string> StatLines(const StatFacts& s)"))
        idle = body.index("if (!s.running) {")
        after = body.index("return lines;", idle)
        first = re.search(r'lines\.push_back\("([^"]*)"', body[after:])
        self.assertIsNotNone(first)
        self.assertTrue(first.group(1).startswith("incident: frames "), first.group(1))
        for part in (" | baseline ", " | worst judged ", " | slow judged frames ", " | window ", " | menu ",
                     " | in-hook ", "incident: hooks tagged ", "report write errors "):
            self.assertIn(part, body, part)
        # D17: `menu yes|no` right after `window yes|no` (Live 3's menu-flag).
        self.assertLess(body.index(" | window "), body.index(" | menu "))
        self.assertLess(body.index(" | menu "), body.index(" | in-hook "))
        # The thread's own start line, which the crash check reads, stays.
        self.assertIn('inline constexpr char kMonitorRunningLine[] = "incident: monitor running";', self.header)
        run = strip_comments(function_body(self.plugin, "static void IncidentMonitorRun() noexcept"))
        self.assertIn("OutRaw(std::string(inc::kMonitorRunningLine)", run)
        for fed in ("in.inHookId = inc::g_Accounting.InHookId();", "stat.window =", "stat.menu =", "stat.hooksTagged =",
                    "stat.hooksUntagged =", "stat.writeErrors ="):
            self.assertIn(fed, run, fed)

    def test_out_and_outraw_share_one_lock(self):
        names = []
        for signature in ("static void Out(const std::string& s)", "static void OutRaw(const std::string& s)"):
            body = strip_comments(function_body(self.plugin, signature))
            found = re.search(r"std::lock_guard<std::mutex>\s+\w+\((\w+)\);", body)
            self.assertIsNotNone(found, signature)
            self.assertLess(found.start(), body.index('f << s << "\\n";'), signature)
            names.append(found.group(1))
        self.assertEqual(names[0], names[1])
        self.assertIn(f"static std::mutex {names[0]};", self.plugin)
        # The shutdown marker never waits on it: a thread ExitProcess ended
        # may have held it.
        self.assertNotIn(names[0], self.header)

    def test_exit_and_panel_json_keys_agree(self):
        keys = set(re.findall(r'Json(?:String|Number)Field\(\s*\w+\s*,\s*"(\w+)"', strip_comments(self.plugin)))
        # Positive control: the four the crash check and the panel check read.
        self.assertEqual(keys, {"exit_code", "faulting_module", "version", "pid"})
        fixture = set()
        for name in ("exit.json", "panel.json"):
            fixture |= set(json.loads((FIXTURES / name).read_text(encoding="utf-8")))
        panel = read(PANEL)
        for key in sorted(keys):
            self.assertIn(key, fixture, key)
            self.assertRegex(panel, rf"[\"']{key}[\"']", key)

    def test_the_exit_hook_writes_the_marker_by_name(self):
        install = strip_comments(function_body(self.plugin, "static void IncidentInstallExitHook()"))
        for needed in ('GetModuleHandleW(L"kernelbase.dll")', 'GetModuleHandleW(L"kernel32.dll")',
                       'GetProcAddress(', '"ExitProcess"', 'MmCreateHook(g_ArSelfModule, "fp_exit_marker"'):
            self.assertIn(needed, install, needed)
        self.assertLess(install.index('L"kernelbase.dll"'), install.index('L"kernel32.dll"'))
        self.assertIsNone(re.search(r"\b0x[0-9A-Fa-f]+", install))
        hook = strip_comments(function_body(self.plugin, "static void WINAPI IncidentHookExitProcess(UINT code)"))
        self.assertLess(hook.index("g_ShutdownMarker.Write(ForgePact::Incident::ShutdownRoute::exitProcess);"),
                        hook.index("g_IncidentOrigExitProcess(code);"))
        for forbidden in ("Out(", "ofstream", "lock_guard"):
            self.assertNotIn(forbidden, hook, forbidden)
        start = strip_comments(function_body(self.plugin, "static void IncidentMonitorStart()"))
        self.assertLess(start.index("g_ShutdownMarker.Arm(OutPath().c_str());"), start.index("IncidentInstallExitHook();"))
        self.assertEqual(self.plugin.count("IncidentInstallExitHook();"), 1)
        # In the player build, resolved by name only.
        shipped = strip_comments(strip_research_blocks(self.plugin))
        self.assertIn("fp_exit_marker", shipped)
        self.assertIn("static void IncidentInstallExitHook()", shipped)
        # Outside the incident region: it and the frame profiler's install no
        # hook (test_frame_profiler.py::test_it_measures_and_changes_nothing).
        self.assertNotIn("MmCreateHook", strip_comments(self.region(REGION_START, REGION_END)))
        self.assertLess(self.plugin.index("static void IncidentInstallExitHook()"),
                        self.plugin.index("// ===== Frame profiler (`frameprof`) ====="))

    # ---- amendment 5 (the owner, 2026-10-02): only our own work, no notice

    def test_every_scoped_original_call_is_guarded(self):
        # A mod is charged only for its own code: every call into the game's
        # original from a timed body sits inside the guard that pauses the clock.
        bodies = scoped_bodies(strip_comments(self.plugin), r"\bIncident(?:Sampled)?Scope\s+\w+\s*\(",
                               r"[A-Za-z_][^;\n]*\(")
        drops = scoped_bodies(strip_comments(self.drops), r"\bFP_DROP_INCIDENT_SCOPE\(\);",
                              r"\s*static RValue& Hook_")
        # Positive control: the bodies that wrap an original are all found.
        for name in ("Hook_DrawHudBuffs", "DoMultiCreate", "Hook_DropRelic", "DensityCopiesTick", "FrameCallback"):
            self.assertIn(name, bodies, name)
        for name in ("Hook_##NAME", "Hook_DropGold", "Hook_DropMonsterGold", "Hook_DropKeys"):
            self.assertIn(name, drops, name)
        calls = 0
        for name, body in list(bodies.items()) + list(drops.items()):
            self.assertEqual(unguarded_original_calls(body), [], name)
            calls += len(ORIGINAL_CALL.findall(strip_comments(body)))
        # Hook_DrawHudBuffs 1, DoMultiCreate 2, Hook_DropRelic 3,
        # DensityCopiesTick 1, the macro 2, the three gold and key bodies 6.
        self.assertGreaterEqual(calls, 15)
        # The guard: a header macro, and DropManager's own that is the bare
        # call where the header is absent (drop_gold_harness.cpp).
        self.assertIn("#define FP_GAME_ORIGINAL(call) ::ForgePact::Incident::GameOriginal(", self.header)
        self.assertIn("#define FP_DROP_GAME_ORIGINAL(call) FP_GAME_ORIGINAL(call)", self.drops)
        self.assertIn("#define FP_DROP_GAME_ORIGINAL(call) (call)", self.drops)
        # Negative control: the same checker on a body with one unguarded call.
        snippet = (
            "static RValue& Hook_Snippet(CInstance* S, CInstance* O, RValue& R, int argc, RValue** A)\n"
            "{\n"
            "    IncidentScope incidentScope(IncidentMod::drops);\n"
            "    RValue t; FP_GAME_ORIGINAL(g_Orig_Snippet(S, O, t, argc, A));\n"
            "    return g_Orig_Snippet(S, O, R, argc, A);\n"
            "}\n")
        found = scoped_bodies(snippet, r"\bIncident(?:Sampled)?Scope\s+\w+\s*\(", r"[A-Za-z_][^;\n]*\(")
        self.assertEqual(list(found), ["Hook_Snippet"])
        self.assertEqual(unguarded_original_calls(found["Hook_Snippet"]), ["g_Orig_Snippet("])

    def test_the_plugin_shows_no_notice(self):
        # The owner, 2026-10-02: every report is written and listed, and the
        # player is told about none of them.
        for notice in ("MessageBoxW", "IncidentNotify"):
            self.assertEqual(self.plugin.count(notice), 0, notice)
        # Positive control: the freeze and the crash still write their bundle,
        # and the bundle still asks the panel for its version.
        self.assertEqual(len(re.findall(r"= IncidentWriteBundle\(std::move\(facts\), failed\);", self.plugin)), 2)
        self.assertIn("IncidentPanelLive(&facts.panelVersion);", self.plugin)

    # ---- replan 5 (Live 2): the menu-room load, D17, and the setup row, D18

    def test_the_menu_rooms_come_from_the_sdk(self):
        # D17: the adapter's table, each room spelled from the SDK's enum
        # through the one macro, so a name the SDK lacks fails the compile.
        region = strip_comments(self.region(REGION_START, REGION_END))
        self.assertEqual(len(re.findall(r"#define FP_INCIDENT_MENU_ROOM\(\w+\)", region)), 1)
        start = region.index("kIncidentMenuRooms[] = {")
        table = region[region.index("{", start) + 1:region.index("};", start)]
        names = []
        for entry in (e.strip() for e in table.split(",")):
            if not entry:
                continue
            found = re.fullmatch(r"FP_INCIDENT_MENU_ROOM\(HeroSiege::Rooms::GameRoom::(\w+)\)", entry)
            self.assertIsNotNone(found, entry)
            names.append(found.group(1))
        self.assertEqual(sorted(names), sorted(MENU_ROOMS))
        self.assertEqual(len(names), len(MENU_ROOMS))
        # Positive control: the SDK's enum has each name the table spells.
        if SDK_ROOMS.is_file():
            rooms = SDK_ROOMS.read_text(encoding="utf-8")
            for name in MENU_ROOMS:
                self.assertRegex(rooms, rf"\b{name} = \d+,", name)
        # The predicate reads the table; the tick passes its answer with the
        # context, and the monitor thread hands the stored flag to the detector.
        is_menu = strip_comments(function_body(self.plugin, "static bool IncidentIsMenuRoom(const std::string& room)"))
        self.assertIn("kIncidentMenuRooms", is_menu)
        tick = strip_comments(function_body(self.plugin, "static void IncidentFrameTick()"))
        self.assertRegex(tick, r"StoreContext\([^;]*IncidentIsMenuRoom\(context\.room\)")
        self.assertEqual(self.plugin.count("StoreContext("), 1)
        run = strip_comments(function_body(self.plugin, "static void IncidentMonitorRun() noexcept"))
        self.assertIn("in.inMenu = monitor.InMenu();", run)
        self.assertIn("TakeFreezeEnded(endedMs, endedLoad, endedMenu)", run)
        self.assertIn("FreezeEndedLine(endedMs, endedLoad, endedMenu)", run)
        # The header takes the flag and never the names.
        self.assertNotIn("hs_game_sdk", self.header)
        self.assertNotIn("HeroSiege::", strip_comments(self.header))
        self.assertIn("bool inMenu = false;", self.header)

    def test_the_setup_block_carries_its_scope_and_prints_its_time(self):
        # D18: the one-time setup is its own row, `setup`, after `ipc`.
        self.assertIn('"stashmoveall", "ipc", "setup",', self.header)
        self.assertRegex(self.header, r"ipc,[^\n]*\n\s*setup,[^\n]*\n\s*Count")
        self.assertEqual(self.plugin.count(SETUP_BLOCK), 1)
        frame = strip_comments(function_body(self.plugin, "void FrameCallback(FWFrame& FrameContext)"))
        setup = brace_block(frame, SETUP_BLOCK)
        self.assertEqual(self.plugin.count(SETUP_SCOPE), 1)
        self.assertIn(SETUP_SCOPE, setup)
        # One line, printed once from the block through Out, after the setup.
        self.assertEqual(self.plugin.count('"incident: setup '), 1)
        printed = re.search(r'\bOut\("incident: setup "', setup)
        self.assertIsNotNone(printed)
        self.assertLess(setup.index("InstallHook();"), printed.start())
        for part in ('" ms at frame "', '": config "', '" ms, hooks "', "SetupSlowest("):
            self.assertIn(part, setup, part)
        # Its clock is the monitor's (test_qpc_lives_only_in_the_incident_region).
        self.assertGreaterEqual(setup.count("ForgePact::Incident::Qpc()"), 3)
        self.assertNotIn("QueryPerformanceCounter", setup)
        self.assertIn("SetupLapStart();", setup)
        # InstallHook times each installer of its normal path, both builds'.
        install = strip_comments(function_body(self.plugin, "static void InstallHook()"))
        player = strip_comments(function_body(strip_research_blocks(self.plugin), "static void InstallHook()"))
        for call, lap in (("CaptureAngelicScriptCode();", "CaptureAngelicScriptCode"),
                          ("LoadCustomForgeEntries();", "LoadCustomForgeEntries"),
                          # test_item_truth_contract.py pins these two as adjacent lines.
                          ("InstallItemTruth();", "InstallCustomForgeItemHooks+InstallItemTruth"),
                          ("HeadhunterAutoArm();", "HeadhunterAutoArm"),
                          ("TyrantAutoArm();", "TyrantAutoArm"),
                          ("BeaconAutoArm();", "BeaconAutoArm"),
                          ("InstallHeadhunterHook();", "InstallHeadhunterHook")):
            self.assertIn(f'{call} SetupLap("{lap}");', player, call)
        for call in ("InstallCreateHooks", "FindAngelicGate", "InstallDropMultHooks", "InstallNecroBalanceHooks",
                     "InstallSlotHook", "InstallLoginHook", "InstallIsMyPlayerHook", "InstallBuffHooks",
                     "InstallEnemyHooks", "InstallChaosTowerHooks", "ForgePact::MiningOre::Install",
                     "InstallItemInspectHooks"):
            self.assertIn(f'{call}(); SetupLap("{call}");', install, call)
        self.assertIn('SetupLap("GetBloodPactInfo detour");', install)
        # A lap outside the setup records nothing (a later `InstallHook` call).
        lap = strip_comments(function_body(self.plugin, "static void SetupLap(const char* name)"))
        self.assertIn("if (!g_SetupLapQpc) return;", lap)


if __name__ == "__main__":
    unittest.main()
