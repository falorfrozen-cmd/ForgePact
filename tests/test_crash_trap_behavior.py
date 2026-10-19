"""The crash trap (issue #223): what a crashing game leaves behind, without a game.

tests/crash_trap_harness.cpp compiles plugin/include/ForgePact/CrashTrap.hpp
whole and installs the trap into a work directory, in its own process and in
child processes of itself.

In-process (the vectored route): a C++ exception thrown and caught is
remembered as the thread's type and writes no line (another thread still says
`unknown`); an access violation under `__try` writes exactly one
`route=first-chance` line naming the hook and the mod the harness set, and the
process carries on; a code that is not fatal (a custom one, a breakpoint)
writes nothing; the route answers EXCEPTION_CONTINUE_SEARCH to every code,
and its lines stop at kMaxFirstChanceLines.

In-process (the top-level filter, which another module can set over ours):
the next check finds it, writes one `filter replaced by <module>+0x<offset>`
line the fold does not take for a record, names it in the status line and puts
ours back on top; that filter calling ours again for the same exception walks
down the chain and ends the search instead of looping; once the chain is full
the other filter is left on top, said once.

In-process (the first-chance staging, Amendment 3): the session's first fatal
access violation outside a guarded read, handled by the harness's own
`__try`, leaves an `MDMP` dump, both log copies and the record line it was
taken for in the trap's folder, and the process carries on; two more inside
kStageMinIntervalMs each write their line, stage nothing and leave one limit
line between them; the same fault inside the guarded-read depth (a
GuardedReadScope, IsBadReadPtr under it, GuardedRead around a `__try`) writes
no line, stages nothing and is only counted.

Children (the unhandled route), with Windows' crash dialog off: one dies of an
access violation, one of an unhandled C++ exception, each holding two fake
logs open for writing. Each leaves a `route=unhandled` line (the C++ one with
its type), a crash.dmp that starts with the minidump signature `MDMP`, and the
copies of both logs; the access violation's dump is the one staged at first
chance, with no second one taken. A third child ends inside a frame handler's
filter, so no top-level filter runs (Live 1's shape): its first-chance line,
staged dump and log copies are all there, and no unhandled line. A clean
session started in the first child's directory afterwards rotates the crashed
session's folder to crashtrap.prev whole.

Every record is read back through IncidentMonitor.hpp's parser, the one the
next load folds into report.json.

Each scenario prints `<name> | <pass|fail> | <detail>`. Skips, with a message,
only where there is no MSVC. Writes only under build/crash-trap-behavior.
"""
import os
import subprocess
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / "build" / "crash-trap-behavior"

EXPECTED = (
    "install",
    "cpp-type-remembered",
    "first-chance-stages-dump",
    "stage-rate-limited",
    "guarded-read-stages-nothing",
    "first-chance-av-line",
    "non-fatal-quiet",
    "continue-search-only",
    "filter-replaced",
    "filter-chain-walk",
    "filter-left-on-top",
    "unhandled-av-child",
    "unhandled-cpp-child",
    "first-chance-child-terminated",
    "rotation",
)


def find_msvc():
    finder = Path(os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)")) / "Microsoft Visual Studio/Installer/vswhere.exe"
    if not finder.is_file():
        return None
    install = subprocess.check_output(
        [str(finder), "-latest", "-products", "*", "-requires",
         "Microsoft.VisualStudio.Component.VC.Tools.x86.x64", "-property", "installationPath"],
        text=True).strip()
    return install or None


class CrashTrapBehaviorTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if os.name != "nt":
            raise unittest.SkipTest("the trap under test is Win32's")
        install = find_msvc()
        if not install:
            raise unittest.SkipTest("Visual Studio C++ compiler is required for native behavior tests")
        OUTPUT.mkdir(parents=True, exist_ok=True)
        include = ROOT / "plugin" / "include"
        cls.harness = OUTPUT / "crash_trap_harness.exe"
        vcvars = Path(install) / "VC/Auxiliary/Build/vcvars64.bat"
        script = OUTPUT / "compile.cmd"
        # /MD, as the plugin is built.
        script.write_text(
            f'@echo off\ncall "{vcvars}" >nul\nif errorlevel 1 exit /b 1\n'
            f'cl /nologo /std:c++20 /EHsc /O2 /W4 /MD /I "{include}" '
            f'"{ROOT / "tests/crash_trap_harness.cpp"}" /Fe:"{cls.harness}" '
            f'/Fo:"{OUTPUT / "crash_trap_harness.obj"}" /link /INCREMENTAL:NO\n'
            'exit /b %errorlevel%\n', encoding="utf-8")
        result = subprocess.run(["cmd", "/d", "/c", str(script)], cwd=OUTPUT, capture_output=True, text=True,
                                encoding="utf-8", errors="replace")
        (OUTPUT / "compile.log").write_text(result.stdout + result.stderr, encoding="utf-8")
        if result.returncode:
            raise AssertionError("compile failed:\n" + result.stdout + result.stderr)
        run = subprocess.run([str(cls.harness), str(OUTPUT / "work")], capture_output=True, text=True,
                             encoding="utf-8", errors="replace", timeout=300)
        (OUTPUT / "run.log").write_text(f"exit {run.returncode}\n" + run.stdout + run.stderr, encoding="utf-8")
        cls.output = run.stdout
        cls.returncode = run.returncode
        cls.results = {}
        for line in run.stdout.splitlines():
            parts = [p.strip() for p in line.split(" | ", 2)]
            if len(parts) == 3:
                cls.results[parts[0]] = (parts[1], parts[2])

    def scenario(self, name):
        self.assertIn(name, self.results, f"{name} did not run:\n{self.output}")
        verdict, detail = self.results[name]
        self.assertEqual(verdict, "pass", f"{name}: {detail}")
        return detail

    def test_every_scenario_ran_and_passed(self):
        self.assertEqual(sorted(self.results), sorted(EXPECTED), self.output)
        for name, (verdict, detail) in self.results.items():
            self.assertEqual(verdict, "pass", f"{name}: {detail}")
        self.assertEqual(self.returncode, 0, self.output)
        self.assertIn("RESULT OK", self.output)

    def test_install_says_what_it_armed(self):
        detail = self.scenario("install")
        self.assertTrue(detail.startswith("incident: trap "), detail)
        self.assertIn("first-chance armed", detail)
        self.assertIn("unhandled armed", detail)
        self.assertIn("dump ready", detail)

    # Baseline: a C++ exception the program catches itself is no crash. The
    # trap remembers its type for this thread only, and writes nothing.
    def test_baseline_a_caught_cpp_exception_writes_nothing(self):
        detail = self.scenario("cpp-type-remembered")
        self.assertIn("before unknown | after .?AVruntime_error@std@@", detail)
        self.assertIn("another thread unknown", detail)

    # Target: a fatal code at first chance is one line, with the tags, and the
    # program's own handler still gets it.
    def test_target_a_first_chance_access_violation_is_one_line_with_the_tags(self):
        detail = self.scenario("first-chance-av-line")
        self.assertTrue(detail.startswith("carried on | exception 0xC0000005 at crash_trap_harness.exe+0x"), detail)
        self.assertIn(" thread=game tid=", detail)
        self.assertIn(" in-hook=harness_hook in-mod=drops game-original=no cpp=none route=first-chance", detail)

    def test_baseline_a_code_that_is_not_fatal_writes_nothing(self):
        # The count before is whatever the earlier scenarios wrote; nothing is added.
        detail = self.scenario("non-fatal-quiet")
        self.assertRegex(detail, r"lines (\d+) -> \1$")

    # Amendment 3, case a. Target: the first fatal fault outside a guarded read
    # is staged, dump, logs and its own line, and the program carries on.
    def test_target_a_first_chance_fatal_fault_stages_the_dump_and_the_logs(self):
        detail = self.scenario("first-chance-stages-dump")
        self.assertEqual(detail.split(" | ")[0], "carried on")
        self.assertIn("| staged 0 -> 1 | dump MDMP ", detail)
        self.assertIn("| logs copied | dump taken for the line just written", detail)
        self.assertNotIn("temporary", detail)

    # Target: inside kStageMinIntervalMs a fatal fault keeps its line, stages
    # nothing, and the limit is said once.
    def test_target_the_staging_is_rate_limited_and_says_so_once(self):
        detail = self.scenario("stage-rate-limited")
        self.assertTrue(detail.startswith("lines 1 -> 3 | staged 1 | refused 0 -> 2 | staging unchanged | limit lines 1 | "),
                        detail)
        self.assertIn("| limit lines 1 | staging limit reached", detail)

    # Case b. Baseline beside case a: the same fault inside ForgePact's own
    # guarded read is not a crash: no line, no staging, no refusal, counted.
    def test_target_a_guarded_read_fault_stages_nothing(self):
        self.assertEqual(self.scenario("guarded-read-stages-nothing"),
                         "depth 1 inside, 0 after | lines 3 -> 3 | staged 1 | refused 0 | guarded faults counted 3")

    def test_the_vectored_route_only_ever_continues_the_search(self):
        detail = self.scenario("continue-search-only")
        # EXCEPTION_CONTINUE_SEARCH is 0, six codes asked.
        self.assertTrue(detail.startswith("answers 0 0 0 0 0 0 |"), detail)
        self.assertIn("| lines 32 of 32 |", detail)

    # Baseline: ours on top, a check writes nothing. Target: a filter set over
    # ours is named, ours goes back on top, and the fold still reads only the
    # `exception ` lines.
    def test_a_filter_set_over_ours_is_found_named_and_re_armed(self):
        detail = self.scenario("filter-replaced")
        self.assertTrue(detail.startswith("baseline quiet | ours back on top | line filter replaced by "
                                          "crash_trap_harness.exe+0x"), detail)
        self.assertIn(", ours re-armed over it (1) | fold reads exception lines only |", detail)
        self.assertIn("unhandled armed (top-level, re-armed 1x, last over crash_trap_harness.exe+0x", detail)

    def test_a_filter_that_calls_ours_back_does_not_loop(self):
        self.assertEqual(self.scenario("filter-chain-walk"), "the other filter ran 1x | answer 0")

    def test_with_the_chain_full_the_other_filter_stays_on_top_and_is_said_once(self):
        detail = self.scenario("filter-left-on-top")
        self.assertTrue(detail.startswith("other on top | lines 7 -> 8 | line filter replaced by "), detail)
        self.assertIn(", left on top: ours was re-armed 7 times already |", detail)
        self.assertIn("unhandled replaced by crash_trap_harness.exe+0x", detail)

    # Targets: a crash leaves the line, the dump and the two logs.
    def test_target_an_access_violation_crash_leaves_line_dump_and_logs(self):
        detail = self.scenario("unhandled-av-child")
        self.assertTrue(detail.startswith("exit 0xC0000005 | dump MDMP "), detail)
        self.assertIn("| logs copied |", detail)
        self.assertIn("in-hook=harness_child_hook in-mod=mapreveal game-original=no cpp=none route=unhandled", detail)

    # The unhandled route is the second trigger: it takes no second dump of an
    # exception the first chance already staged.
    def test_the_unhandled_route_takes_no_second_dump_of_a_staged_exception(self):
        self.assertIn("| dump taken at first chance for this exception, 1 note of no second |",
                      self.scenario("unhandled-av-child"))

    def test_target_an_unhandled_cpp_exception_names_its_type(self):
        detail = self.scenario("unhandled-cpp-child")
        self.assertIn("dump MDMP ", detail)
        self.assertIn("| logs copied |", detail)
        self.assertIn("exception 0xE06D7363 at ", detail)
        self.assertIn("cpp=.?AVruntime_error@std@@ route=unhandled", detail)
        # A C++ exception never stages at first chance: the filter's dump is its only one.
        self.assertIn("| dump taken by the unhandled route |", detail)

    # Live 1's shape: the process ends before any top-level filter runs. The
    # staging is the whole report, and it is whole.
    def test_target_a_process_ended_after_first_chance_keeps_the_staging(self):
        detail = self.scenario("first-chance-child-terminated")
        self.assertTrue(detail.startswith("exit 0xC0000005 | dump MDMP "), detail)
        self.assertIn("| logs copied |", detail)
        self.assertIn(" route=first-chance | unhandled lines 0 | dump taken for that line |", detail)

    def test_the_next_session_rotates_the_crashed_one(self):
        detail = self.scenario("rotation")
        self.assertTrue(detail.startswith("prev: dump MDMP "), detail)
        self.assertIn("| fresh: session ", detail)


if __name__ == "__main__":
    unittest.main()
