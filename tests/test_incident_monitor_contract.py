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
        for part in (" | baseline ", " | worst judged ", " | slow judged frames ", " | window ", " | in-hook ",
                     "incident: hooks tagged ", "report write errors "):
            self.assertIn(part, body, part)
        # The thread's own start line, which the crash check reads, stays.
        self.assertIn('inline constexpr char kMonitorRunningLine[] = "incident: monitor running";', self.header)
        run = strip_comments(function_body(self.plugin, "static void IncidentMonitorRun() noexcept"))
        self.assertIn("OutRaw(std::string(inc::kMonitorRunningLine)", run)
        for fed in ("in.inHookId = inc::g_Accounting.InHookId();", "stat.window =", "stat.hooksTagged =",
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


if __name__ == "__main__":
    unittest.main()
