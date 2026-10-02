"""The incident monitor's wiring (issue #76), read from the source.

The behaviour is tests/test_incident_monitor_behavior.py's; this pins what a
harness cannot see. The header stays game-independent. FrameCallback takes the
incident frame boundary right after the frame profiler's. The `incident` verb
is a player command and a standalone early return. The monitor thread's code
never reaches into the game's runtime or YYToolkit. The player build reads the
high-resolution clock only in the incident monitor. The clean-shutdown
marker's destructor uses Win32 file calls only. Nothing is left for the C
runtime to destroy at ExitProcess. The thresholds are the plan's (D5).
"""
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
        body = strip_comments(function_body(self.header, "~ShutdownMarker()"))
        for forbidden in ("ofstream", "Out(", "fopen", "std::string", "new ", "malloc", "printf", "LoadLibrary",
                          "std::filesystem"):
            self.assertNotIn(forbidden, body, forbidden)
        for needed in ("CreateFileA(", "WriteFile(", "CloseHandle(", "kCleanShutdownLine"):
            self.assertIn(needed, body, needed)
        self.assertIn('inline constexpr char kCleanShutdownLine[] = "==== clean shutdown ====";', self.header)
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


if __name__ == "__main__":
    unittest.main()
