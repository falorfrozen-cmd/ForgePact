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

Children (the unhandled route), with Windows' crash dialog off: one dies of an
access violation, one of an unhandled C++ exception, each holding two fake
logs open for writing. Each leaves a `route=unhandled` line (the C++ one with
its type), a crash.dmp that starts with the minidump signature `MDMP`, and the
copies of both logs. A clean session started in the first child's directory
afterwards rotates the crashed session's folder to crashtrap.prev whole.

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
    "first-chance-av-line",
    "non-fatal-quiet",
    "continue-search-only",
    "unhandled-av-child",
    "unhandled-cpp-child",
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
        self.assertIn("lines 1 -> 1", self.scenario("non-fatal-quiet"))

    def test_the_vectored_route_only_ever_continues_the_search(self):
        detail = self.scenario("continue-search-only")
        # EXCEPTION_CONTINUE_SEARCH is 0, six codes asked.
        self.assertTrue(detail.startswith("answers 0 0 0 0 0 0 |"), detail)
        self.assertIn("| lines 32 of 32 |", detail)

    # Targets: a crash leaves the line, the dump and the two logs.
    def test_target_an_access_violation_crash_leaves_line_dump_and_logs(self):
        detail = self.scenario("unhandled-av-child")
        self.assertTrue(detail.startswith("exit 0xC0000005 | dump MDMP "), detail)
        self.assertIn("| logs copied |", detail)
        self.assertIn("in-hook=harness_child_hook in-mod=mapreveal game-original=no cpp=none route=unhandled", detail)

    def test_target_an_unhandled_cpp_exception_names_its_type(self):
        detail = self.scenario("unhandled-cpp-child")
        self.assertIn("dump MDMP ", detail)
        self.assertIn("| logs copied |", detail)
        self.assertIn("exception 0xE06D7363 at ", detail)
        self.assertIn("cpp=.?AVruntime_error@std@@ route=unhandled", detail)

    def test_the_next_session_rotates_the_crashed_one(self):
        detail = self.scenario("rotation")
        self.assertTrue(detail.startswith("prev: dump MDMP "), detail)
        self.assertIn("| fresh: session ", detail)


if __name__ == "__main__":
    unittest.main()
