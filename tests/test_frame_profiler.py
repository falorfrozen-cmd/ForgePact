"""The frame profiler (`frameprof`): where the game's frame thread spends its time.

Two halves.

**Behaviour.** tests/frame_profiler_harness.cpp compiles the real
ForgePact::FrameProfiler and points it at a thread of its own that runs a
known call chain, named through a fake compiled-code table laid out like the
game's YYGMLFuncs. Each capture's JSON report must name that chain the way it
would name the game's scripts and object events; a sleeping thread must read
as idle; slow frames must be pinned on the code that ran during them; and a
frame thread that hammers the heap must not deadlock the sampler, which is
the property the whole design (nothing allocates while the frame thread is
suspended) exists to guarantee.

**Contract.** The plugin wiring: the verb is its own standalone early return
in the player whitelist, FrameCallback records the frame boundary before any
other ForgePact work, the header stays game-independent, and the code that
runs while the frame thread is suspended stays free of anything that can
take a lock.
"""
import json
import os
import re
import shutil
import subprocess
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PLUGIN = ROOT / "plugin" / "ModuleMain.cpp"
HEADER = ROOT / "plugin" / "include" / "ForgePact" / "FrameProfiler.hpp"


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


@unittest.skipUnless(os.name == "nt", "the profiler samples with Win32 thread suspension")
class FrameProfilerBehaviorTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        out = ROOT / "build" / "frame-profiler-behavior"
        out.mkdir(parents=True, exist_ok=True)
        cls.binary = out / "frameprof.exe"
        vswhere = Path(os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)")) / "Microsoft Visual Studio/Installer/vswhere.exe"
        if not vswhere.is_file():
            raise unittest.SkipTest("Visual Studio C++ compiler is required for native behavior tests")
        install = subprocess.check_output(
            [str(vswhere), "-latest", "-products", "*", "-requires",
             "Microsoft.VisualStudio.Component.VC.Tools.x86.x64", "-property", "installationPath"],
            text=True).strip()
        if not install:
            raise unittest.SkipTest("Visual Studio C++ toolchain not installed")
        vcvars = Path(install) / "VC/Auxiliary/Build/vcvars64.bat"
        batch = out / "compile.cmd"
        # /INCREMENTAL:NO: a function's address must be the function, not an
        # incremental-link thunk, or the fake table names the wrong code.
        batch.write_text(
            f'@echo off\ncall "{vcvars}" >nul\nif errorlevel 1 exit /b 1\n'
            f'cl /nologo /std:c++20 /EHsc /O2 /W4 /I "{ROOT / "plugin/include"}" '
            f'"{ROOT / "tests/frame_profiler_harness.cpp"}" /Fe:"{cls.binary}" /Fo:"{out / "frameprof.obj"}" '
            f'/link /INCREMENTAL:NO\n'
            f'exit /b %errorlevel%\n', encoding="utf-8")
        result = subprocess.run(["cmd", "/d", "/c", str(batch)], cwd=out, capture_output=True, text=True,
                                encoding="utf-8", errors="replace")
        (out / "compile.log").write_text(result.stdout + result.stderr, encoding="utf-8")
        if result.returncode:
            raise AssertionError(result.stdout + result.stderr)
        reports = out / "reports"
        if reports.exists():
            shutil.rmtree(reports, ignore_errors=True)
        run = subprocess.run([str(cls.binary), str(reports)], capture_output=True, text=True,
                             encoding="utf-8", errors="replace", timeout=240)
        cls.output = run.stdout
        cls.returncode = run.returncode
        (out / "run.log").write_text(run.stdout + run.stderr, encoding="utf-8")
        cls.reports = {}
        for line in cls.output.splitlines():
            if line.startswith("REPORT "):
                _, label, path = line.split(" ", 2)
                cls.reports[label] = json.loads(Path(path).read_text(encoding="utf-8"))

    def line(self, label):
        for line in self.output.splitlines():
            if line.split(" ")[1:2] == [label]:
                return line
        raise AssertionError(f"check {label!r} not in harness output:\n{self.output}")

    def rows(self, report, key):
        return {row["name"]: row["percent"] for row in report[key]}

    def bucket(self, report, key):
        return next(b["percent"] for b in report["buckets"] if b["key"] == key)

    def test_every_check_passes(self):
        self.assertEqual(self.returncode, 0, self.output)
        self.assertIn("RESULT OK", self.output)
        self.assertNotIn("FAIL ", self.output)

    def test_the_known_chain_is_named_like_game_code(self):
        r = self.reports["chain"]
        self.assertEqual(r["schema"], "forgepact-frameprof/1")
        self.assertGreater(r["capture"]["samples"], 50)
        self.assertGreaterEqual(self.rows(r, "events").get("Fake_Enemy_obj Step", 0), 60.0)
        self.assertGreaterEqual(self.rows(r, "gmlTotal").get("FakeMiddle", 0), 60.0)
        # The first game function from the leaf is the script, not the event.
        self.assertEqual(r["gmlSelf"][0]["name"], "FakeMiddle")
        self.assertGreaterEqual(self.rows(r, "builtins").get("fake_spin()", 0), 60.0)
        self.assertGreaterEqual(self.bucket(r, "game"), 60.0)
        self.assertEqual(r["game"]["gmlEntries"], 4)

    def test_collapsed_stacks_run_outermost_first(self):
        r = self.reports["chain"]
        stacks = (ROOT / "build" / "frame-profiler-behavior" / "reports" / r["files"]["stacks"]).read_text(encoding="utf-8")
        lines = [l for l in stacks.splitlines() if l.strip()]
        self.assertTrue(all(re.search(r" \d+$", l) for l in lines), lines[:3])
        self.assertTrue(any("Fake_Enemy_obj Step;FakeMiddle;fake_spin()" in l for l in lines), lines[:5])

    def test_the_context_callback_lands_in_the_timeline(self):
        timeline = self.reports["chain"]["timeline"]
        with_context = [s for s in timeline if "room" in s]
        self.assertTrue(with_context, timeline)
        self.assertEqual(with_context[0]["room"], "Harness_rm")
        self.assertEqual(with_context[0]["instances"], 321)
        self.assertEqual(with_context[0]["monsters"], 12)

    def test_a_spinning_frame_limiter_counts_as_waiting(self):
        # Like the GameMaker runner: a little work, then the rest of each frame
        # spun away on the clock in a function no table names.
        r = self.reports["spinning"]
        self.assertIsNotNone(r["spinWait"])
        self.assertGreaterEqual(self.bucket(r, "spin"), 60.0)
        self.assertLessEqual(r["time"]["workingPercent"], 40.0)
        # The harness recaptures until it has 100 samples (ForgePact #190):
        # fewer, and "no game code seen" says the sampler was starved.
        self.assertGreaterEqual(r["capture"]["samples"], 100, self.line("spinning/enough_samples"))
        self.assertGreater(self.bucket(r, "game"), 0.0)
        # A clock read under game code is not a frame limiter.
        self.assertIsNone(self.reports["chain"]["spinWait"])
        self.assertIsNone(self.reports["hitches"]["spinWait"])

    def test_a_sleeping_frame_thread_reads_as_idle(self):
        r = self.reports["sleeping"]
        self.assertGreaterEqual(self.bucket(r, "idle"), 60.0)
        self.assertLessEqual(r["time"]["workingPercent"], 40.0)

    def test_heap_churn_finishes(self):
        # The harness's own check fails, and its watchdog kills the process,
        # if the sampler ever deadlocks against the frame thread.
        self.assertTrue(self.line("heap/finished_without_deadlock").startswith("PASS "))
        self.assertGreater(self.reports["heap"]["capture"]["samples"], 100)

    def test_slow_frames_are_pinned_on_what_ran_during_them(self):
        r = self.reports["hitches"]
        self.assertGreaterEqual(r["frames"]["over100ms"], 1)
        worst = r["hitches"][0]
        self.assertGreaterEqual(worst["ms"], 100.0)
        self.assertEqual(worst["gmlTotal"][0]["name"], "FakeSlowWork")
        self.assertEqual(worst["room"], "Harness_rm")

    def test_stop_and_a_vanished_thread_end_a_capture_early(self):
        self.assertLess(self.reports["stopped"]["capture"]["seconds"], 5.0)
        ended = self.reports["ended"]["capture"]["endedEarly"]
        self.assertTrue("could not be paused" in ended or "registers" in ended, ended)

    def test_the_report_measures_its_own_cost(self):
        c = self.reports["chain"]["capture"]
        self.assertGreater(c["pauseUsAvg"], 0.0)
        self.assertIn("pausePercentOfTime", c)
        self.assertEqual(c["timer"], "high-resolution")

    def test_the_sampler_slows_down_when_its_pauses_add_up(self):
        # The heap capture asks for the maximum rate. Either its pauses stayed
        # within the budget, or the sampler cut its rate to get back under it.
        c = self.reports["heap"]["capture"]
        self.assertEqual(c["hz"], 2000)
        if c["pausePercentOfTime"] > c["pauseBudgetPercent"] + 0.5:
            self.assertGreaterEqual(c["rateCuts"], 1, c)
            self.assertLess(c["hzLowest"], 2000, c)


class FrameProfilerContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.plugin = PLUGIN.read_text(encoding="utf-8").replace("\r\n", "\n")
        cls.header = HEADER.read_text(encoding="utf-8").replace("\r\n", "\n")

    def test_the_player_build_accepts_the_verb(self):
        start = self.plugin.index("kPlayerCommands = {")
        block = self.plugin[start:self.plugin.index("};", start)]
        self.assertIn('"frameprof"', block)

    def test_the_verb_is_a_standalone_early_return(self):
        run = _body(self.plugin, "static void RunCommand(const std::string& line)")
        self.assertIn('if (lc == "frameprof") { FrameProfCommand(rest); return; }', run)
        # Its own verb: the research build's `perf` (the plugin's own hook
        # timings, answered inside HandleHeadhunterCommand) keeps working.
        self.assertIn('} else if (lc == "perf") {', self.plugin)
        self.assertEqual(self.plugin.count('"frameprof"'), 2)

    def test_the_frame_boundary_is_taken_first(self):
        frame = _body(self.plugin, "void FrameCallback(FWFrame& FrameContext)")
        code = [l.strip() for l in _code(frame).splitlines() if l.strip()]
        self.assertEqual(code[:2], ["UNREFERENCED_PARAMETER(FrameContext);", "FrameProfilerTick();"])
        self.assertEqual(frame.count("FrameProfilerTick();"), 1)

    def test_the_tick_is_cheap_while_idle(self):
        tick = _code(_body(self.plugin, "static void FrameProfilerTick()"))
        self.assertIn("profiler.OnFrame();", tick)
        self.assertIn("if (profiler.TakeSummary(lines))", tick)
        on_frame = _code(_body(self.header, "void OnFrame() noexcept"))
        first = [l.strip() for l in on_frame.splitlines() if l.strip()][0]
        self.assertEqual(first, "if (!m_Recording.load(std::memory_order_acquire)) return;")
        take = _code(_body(self.header, "bool TakeSummary(std::vector<std::string>& lines)"))
        first = [l.strip() for l in take.splitlines() if l.strip()][0]
        self.assertEqual(first, "if (!m_SummaryReady.load(std::memory_order_acquire)) return false;")

    def test_nothing_that_can_lock_runs_while_the_frame_thread_is_suspended(self):
        capture = _code(_body(self.header, "inline CaptureResult CaptureOnce("))
        suspended = capture[capture.index("SuspendThread("):capture.rindex("ResumeThread(")]
        for forbidden in ("new ", "delete", "malloc", "push_back", "emplace", "std::string", "std::vector",
                          "ofstream", "printf", "lock", "Out(", "RtlLookupFunctionEntry", "RtlVirtualUnwind",
                          "throw"):
            self.assertNotIn(forbidden, suspended, forbidden)
        # The walk happens after ResumeThread, on the copy.
        run = _code(_body(self.header, "void Run() noexcept"))
        self.assertLess(run.index("CaptureOnce("), run.index("WalkCopy("))
        # No loader-lock-taking lookup anywhere: the walk uses its own copy
        # of each module's unwind table.
        self.assertNotIn("RtlLookupFunctionEntry(", _code(self.header))

    def test_the_header_is_game_independent(self):
        code = _code(self.header)
        for game in ("RValue", "g_Yytk", "YYTK", "CInstance", "Aurie"):
            self.assertNotIn(game, code, game)
        # <windows.h> without NOMINMAX in ModuleMain.cpp breaks these.
        self.assertIsNone(re.search(r"std::(min|max)\s*\(", code))
        self.assertNotIn("numeric_limits", code)

    def test_process_exit_is_safe(self):
        # A heap-held singleton and an ExitSafeThread: nothing for the CRT to
        # destroy at ExitProcess while the sampler thread was terminated.
        self.assertIn("static Profiler* profiler = new Profiler();", self.header)
        self.assertIn("ExitSafeThread m_Thread;", self.header)

    def test_it_measures_and_changes_nothing(self):
        # No hook, no write into the game: the adapter reads a table entry,
        # named built-ins and three read-only values once a second.
        # The adapter's region ends at the next `// ===== ` section banner:
        # later sections (the incident monitor, the dungeon chest probe) sit
        # between it and RunCommand and are pinned by their own tests.
        start = self.plugin.index("// ===== Frame profiler (`frameprof`) =====")
        adapter = _code(self.plugin[start:self.plugin.index("\n// ===== ", start + 1)])
        for forbidden in ("MmCreateHook", "HookOneScript", "SetBuiltin(", "PC_SetVariable"):
            self.assertNotIn(forbidden, adapter, forbidden)
        # A capture writes no shared ForgePact state either: the monster
        # count resolves its own Enemy_Parent_obj index.
        self.assertIsNone(re.search(r"g_EnemyParentIdx\s*=[^=]", adapter))
        # The built-in list holds names to look up, never calls: every
        # CallBuiltin in the adapter is one of three read-only ones.
        called = set(re.findall(r'CallBuiltin\("(\w+)"', adapter))
        self.assertEqual(called, {"room_get_name", "asset_get_index", "instance_number"})
        self.assertEqual(set(re.findall(r'GetBuiltin\("(\w+)"', adapter)), {"room", "instance_count"})

    def test_out_survives_a_percent_sign_and_a_long_line(self):
        # YYToolkit's PrintInfo runs its result through vsprintf_s a second
        # time as a format (CmWriteInfo), into 4096-byte buffers: an undoubled
        # '%' ("working 54%, waiting") ended the game with an invalid-parameter
        # fast fail (0xC0000409 in ucrtbase, data 5), and so can a line longer
        # than the buffer.
        out = _code(_body(self.plugin, "static void Out(const std::string& s)"))
        self.assertIn("if (c == '%') printable += '%';", out)
        self.assertIn('g_Yytk->PrintInfo("[BP] %s", printable.c_str());', out)
        self.assertNotIn("s.c_str()", out)
        self.assertIn("if (printable.size() >= kOutPrintLimit)", out)
        limit = int(re.search(r"kOutPrintLimit = (\d+);", self.plugin).group(1))
        # Doubling can grow the text past the limit by one '%'; "[BP] " and
        # "..." go on top. All of it must stay well inside 4096.
        self.assertLess(limit + 1 + len("[BP] ") + len("...") + 1, 4096)
        # The line is in out.txt, stream closed, before PrintInfo runs.
        write = out.index('f << s << "\\n";')
        closed = out.index("}", write)
        self.assertLess(closed, out.index("PrintInfo"))

    def test_the_builtin_names_live_beside_the_profiler(self):
        # A lookup table of names, not calls: kept out of ModuleMain.cpp,
        # whose contract tests count built-in literals to pin call sites.
        names_header = (HEADER.parent / "FrameProfilerBuiltins.hpp").read_text(encoding="utf-8")
        names = re.findall(r'"([a-z0-9_]+)"', names_header)
        self.assertEqual(len(names), len(set(names)))
        self.assertGreaterEqual(len(names), 400)
        self.assertIn("for (const char* name : ForgePact::FrameProfiler::kBuiltinNames) {",
                      _body(self.plugin, "static void FrameProfCommand(const std::string& rest)"))
        self.assertNotIn("kFrameProfBuiltins", self.plugin)

    def test_defaults(self):
        self.assertIn("inline constexpr unsigned kDefaultHz = 250;", self.header)
        self.assertIn("inline constexpr double kDefaultSeconds = 30.0;", self.header)
        self.assertIn("inline constexpr double kMaxSeconds = 600.0;", self.header)


if __name__ == "__main__":
    unittest.main()
