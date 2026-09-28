"""Contract tests for tools/run_tests_parallel.py.

The runner is only worth having if it runs what serial discovery runs and
reports what serial unittest would report. So the real suite is checked for
id-set equality, and small fixture suites are run both ways, serially with
`python -m unittest discover` and through the runner, with the summaries
compared line for line (minus the timing).
"""
import importlib.util
import re
import subprocess
import sys
import tempfile
import textwrap
import unittest
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "tools" / "run_tests_parallel.py"

_spec = importlib.util.spec_from_file_location("forgepact_run_tests_parallel", SCRIPT)
runner = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(runner)


MIXED = {
    "test_a_pass.py": """
        import unittest
        class Passing(unittest.TestCase):
            def test_one(self): pass
            def test_two(self): pass
            @unittest.skip("skipped on purpose")
            def test_skipped(self): pass
            @unittest.expectedFailure
            def test_expected(self): self.fail("expected")
    """,
    "test_b_fail.py": """
        import unittest
        class Failing(unittest.TestCase):
            def test_fails(self): self.assertEqual(1, 2)
            def test_errors(self): raise RuntimeError("boom")
            def test_subtests(self):
                for i in range(3):
                    with self.subTest(i=i):
                        self.assertLess(i, 1)
    """,
    "test_c_class_skip.py": """
        import unittest
        class SkippedClass(unittest.TestCase):
            @classmethod
            def setUpClass(cls): raise unittest.SkipTest("no toolchain")
            def test_one(self): pass
            def test_two(self): pass
    """,
    "test_d_broken.py": """
        import no_such_module_anywhere
    """,
}

PASSING = {
    "test_a_pass.py": MIXED["test_a_pass.py"],
    "test_c_class_skip.py": MIXED["test_c_class_skip.py"],
}


def write_suite(root, files):
    for name, body in files.items():
        (root / name).write_text(textwrap.dedent(body), encoding="utf-8")


def tail(stderr):
    """unittest's closing 'Ran N tests' and status lines, timing dropped."""
    lines = [line for line in stderr.splitlines() if line.strip()]
    ran = next(line for line in reversed(lines) if line.startswith("Ran "))
    status = next(line for line in reversed(lines) if line.startswith(("OK", "FAILED")))
    return ran.split(" in ")[0], status


class FixtureSuiteTests(unittest.TestCase):
    def run_both(self, files, jobs="3"):
        with tempfile.TemporaryDirectory(prefix="forgepact-runner-") as tmp:
            tmp = Path(tmp)
            suite = tmp / "suite"
            suite.mkdir()
            write_suite(suite, files)
            serial = subprocess.run([sys.executable, "-m", "unittest", "discover", "-s", "suite"],
                                    cwd=tmp, capture_output=True, text=True)
            parallel = subprocess.run([sys.executable, str(SCRIPT), "-j", jobs,
                                       "--start-dir", "suite", "--state-dir", str(tmp / "state")],
                                      cwd=tmp, capture_output=True, text=True)
            return serial, parallel

    def test_failures_errors_and_skips_are_reported_as_serial_reports_them(self):
        serial, parallel = self.run_both(MIXED)
        self.assertEqual(tail(parallel.stderr), tail(serial.stderr), parallel.stderr)
        self.assertEqual(serial.returncode, 1)
        self.assertEqual(parallel.returncode, 1)
        # The fixture has every kind unittest counts, so none may drop out.
        status = tail(parallel.stderr)[1]
        for part in ("failures=3", "errors=2", "skipped=2", "expected failures=1"):
            self.assertIn(part, status)
        self.assertIn("FAIL: test_fails (test_b_fail.Failing.test_fails)", parallel.stderr)
        self.assertIn("No module named 'no_such_module_anywhere'", parallel.stderr)

    def test_a_passing_run_exits_zero_and_keeps_skips_as_skips(self):
        serial, parallel = self.run_both(PASSING)
        self.assertEqual(tail(parallel.stderr), tail(serial.stderr), parallel.stderr)
        self.assertEqual(tail(parallel.stderr)[1], "OK (skipped=2, expected failures=1)")
        self.assertEqual(parallel.returncode, 0)

    def test_a_worker_that_dies_is_an_error_not_a_pass(self):
        files = dict(PASSING)
        files["test_e_crash.py"] = """
            import os, unittest
            class Crash(unittest.TestCase):
                def test_dies(self): os._exit(3)
        """
        with tempfile.TemporaryDirectory(prefix="forgepact-runner-") as tmp:
            tmp = Path(tmp)
            write_suite(tmp, files)
            run = subprocess.run([sys.executable, str(SCRIPT), "-j", "2", "--start-dir", str(tmp),
                                  "--state-dir", str(tmp / "state")],
                                 capture_output=True, text=True)
        self.assertEqual(run.returncode, 1, run.stderr)
        self.assertIn("test_e_crash (worker exit 3)", run.stderr)
        self.assertNotIn("never loaded", run.stderr)

    SOLO = """
        import os, time, unittest
        PARALLEL_GROUP = "solo"
        LOCK = os.path.join(os.path.dirname(os.path.abspath(__file__)), "solo.lock")
        class Solo(unittest.TestCase):
            def test_alone(self):
                fd = os.open(LOCK, os.O_CREAT | os.O_EXCL)
                try:
                    time.sleep(1.5)
                finally:
                    os.close(fd)
                    os.remove(LOCK)
    """

    def run_solo(self, *extra):
        # Each module holds an exclusive lock file for a while; two of them at
        # once fail on O_EXCL.
        with tempfile.TemporaryDirectory(prefix="forgepact-runner-") as tmp:
            tmp = Path(tmp)
            write_suite(tmp, {f"test_solo_{i}.py": self.SOLO for i in range(3)})
            return subprocess.run([sys.executable, str(SCRIPT), "-j", "3", "--start-dir", str(tmp),
                                   "--state-dir", str(tmp / "state"), *extra],
                                  capture_output=True, text=True)

    def test_a_group_never_runs_more_copies_than_its_limit(self):
        run = self.run_solo()
        self.assertEqual(run.returncode, 0, run.stderr)
        self.assertEqual(tail(run.stderr)[1], "OK")

    def test_the_lock_probe_does_collide_when_the_cap_allows_it(self):
        # Positive control: without it, a probe that could never collide
        # would make the test above pass for nothing.
        run = self.run_solo("--group-limit", "solo=3")
        self.assertEqual(run.returncode, 1, run.stderr)
        self.assertIn("FileExistsError", run.stderr)

    # Every module marks itself running for a while; an ordinary one leaves a
    # done marker when it ends. An exclusive module asserts, on entry and all
    # through its run, that no other module is running and every ordinary
    # module is done. The padding makes it the largest file, so with no
    # recorded durations it is the first the runner would pick.
    NORMAL = """
        import os, time, unittest
        HERE = os.path.dirname(os.path.abspath(__file__))
        NAME = os.path.splitext(os.path.basename(__file__))[0]
        class Normal(unittest.TestCase):
            def test_busy(self):
                mark = os.path.join(HERE, "running-" + NAME)
                open(mark, "w").close()
                try:
                    time.sleep(1.0)
                finally:
                    os.remove(mark)
                open(os.path.join(HERE, "done-" + NAME), "w").close()
    """
    EXCLUSIVE = """
        import os, time, unittest
        PARALLEL_EXCLUSIVE = {flag}
        HERE = os.path.dirname(os.path.abspath(__file__))
        NAME = os.path.splitext(os.path.basename(__file__))[0]
        # {padding}
        class Exclusive(unittest.TestCase):
            def others(self):
                return [f for f in os.listdir(HERE) if f.startswith("running-") and f != "running-" + NAME]
            def test_alone(self):
                mark = os.path.join(HERE, "running-" + NAME)
                open(mark, "w").close()
                try:
                    done = [f for f in os.listdir(HERE) if f.startswith("done-test_normal_")]
                    self.assertEqual(len(done), {normals}, "an ordinary module had not finished")
                    for _ in range(12):
                        self.assertEqual(self.others(), [])
                        time.sleep(0.1)
                finally:
                    os.remove(mark)
    """

    def run_exclusive(self, flag="True", exclusives=1, normals=3, jobs="4"):
        files = {f"test_normal_{i}.py": self.NORMAL for i in range(normals)}
        body = self.EXCLUSIVE.format(flag=flag, normals=normals, padding="x" * 4000)
        files.update({f"test_exclusive_{i}.py": body for i in range(exclusives)})
        with tempfile.TemporaryDirectory(prefix="forgepact-runner-") as tmp:
            tmp = Path(tmp)
            write_suite(tmp, files)
            return subprocess.run([sys.executable, str(SCRIPT), "-j", jobs, "--start-dir", str(tmp),
                                   "--state-dir", str(tmp / "state")],
                                  capture_output=True, text=True)

    def test_an_exclusive_module_runs_last_and_alone(self):
        run = self.run_exclusive()
        self.assertEqual(run.returncode, 0, run.stderr)
        self.assertEqual(tail(run.stderr), ("Ran 4 tests", "OK"))

    def test_two_exclusive_modules_never_overlap_each_other(self):
        run = self.run_exclusive(exclusives=2)
        self.assertEqual(run.returncode, 0, run.stderr)
        self.assertEqual(tail(run.stderr), ("Ran 5 tests", "OK"))

    def test_the_exclusive_probe_does_fail_on_an_ordinary_module(self):
        # Positive control: the same module with the flag off is scheduled
        # as any other, first by size and beside the rest, and the probe
        # says so; ordinary modules still share the workers.
        run = self.run_exclusive(flag="False")
        self.assertEqual(run.returncode, 1, run.stderr)
        self.assertIn("an ordinary module had not finished", run.stderr)

    # Leaves a marker named after itself when it runs, so a run that left it
    # out can be told from a run that loaded it and reported nothing.
    MARKER = """
        import os, unittest
        HERE = os.path.dirname(os.path.abspath(__file__))
        NAME = os.path.splitext(os.path.basename(__file__))[0]
        class Marker(unittest.TestCase):
            def test_leaves_a_mark(self):
                open(os.path.join(HERE, "ran-" + NAME), "w").close()
            def test_second(self): pass
    """

    def run_excluding(self, *extra, markers=("test_x_marker",)):
        files = dict(PASSING)
        files.update({f"{name}.py": self.MARKER for name in markers})
        with tempfile.TemporaryDirectory(prefix="forgepact-runner-") as tmp:
            tmp = Path(tmp)
            suite = tmp / "suite"
            suite.mkdir()
            write_suite(suite, files)
            run = subprocess.run([sys.executable, str(SCRIPT), "-j", "3", "--start-dir", "suite",
                                  "--state-dir", str(tmp / "state"), *extra],
                                 cwd=tmp, capture_output=True, text=True)
            ran = sorted(f.name[len("ran-"):] for f in suite.glob("ran-*"))
            reference = tmp / "reference"
            reference.mkdir()
            write_suite(reference, PASSING)
            serial = subprocess.run([sys.executable, "-m", "unittest", "discover", "-s", "reference"],
                                    cwd=tmp, capture_output=True, text=True)
        return run, ran, serial

    def test_an_excluded_module_does_not_run_and_is_named(self):
        run, ran, serial = self.run_excluding("--exclude-module", "test_x_marker")
        self.assertEqual(run.returncode, 0, run.stderr)
        self.assertEqual(ran, [])
        # What ran is what serial runs on the suite without that module, and
        # the id-set check is satisfied by discovery less that module.
        self.assertEqual(tail(run.stderr), tail(serial.stderr), run.stderr)
        self.assertNotIn("never loaded", run.stderr)
        self.assertIn("run_tests_parallel: left out 1 module(s), 2 test(s): test_x_marker (2)",
                      run.stderr)

    def test_the_marker_probe_does_mark_a_module_that_ran(self):
        # Positive control: without --exclude-module the same module runs,
        # leaves its marker and is counted, and nothing is reported left out.
        run, ran, serial = self.run_excluding()
        self.assertEqual(run.returncode, 0, run.stderr)
        self.assertEqual(ran, ["test_x_marker"])
        count = lambda stderr: int(tail(stderr)[0].split()[1])
        self.assertEqual(count(run.stderr), count(serial.stderr) + 2)
        self.assertNotIn("left out", run.stderr)

    def test_a_pattern_or_a_py_name_excludes_every_module_it_names(self):
        run, ran, _serial = self.run_excluding("--exclude-module", "test_x_*",
                                               markers=("test_x_one", "test_x_two", "test_y_kept"))
        self.assertEqual(run.returncode, 0, run.stderr)
        self.assertEqual(ran, ["test_y_kept"])
        self.assertIn("left out 2 module(s), 4 test(s): test_x_one (2), test_x_two (2)", run.stderr)
        run, ran, _serial = self.run_excluding("--exclude-module", "test_x_marker.py")
        self.assertEqual((run.returncode, ran), (0, []), run.stderr)

    def test_an_exclusion_that_matches_nothing_is_refused_before_anything_runs(self):
        run, ran, _serial = self.run_excluding("--exclude-module", "test_x_markr")
        self.assertEqual(run.returncode, 2, run.stderr)
        self.assertEqual(ran, [])
        self.assertIn("--exclude-module 'test_x_markr' matches no test module", run.stderr)

    # The MARKER module, declaring a PARALLEL_GROUP.
    GROUPED = 'PARALLEL_GROUP = "browser"\n' + textwrap.dedent(MARKER)

    def run_grouped(self, *extra):
        files = dict(PASSING)
        files["test_x_grouped.py"] = self.GROUPED
        files["test_y_plain.py"] = self.MARKER
        with tempfile.TemporaryDirectory(prefix="forgepact-runner-") as tmp:
            tmp = Path(tmp)
            suite = tmp / "suite"
            suite.mkdir()
            write_suite(suite, files)
            run = subprocess.run([sys.executable, str(SCRIPT), "-j", "3", "--start-dir", "suite",
                                  "--state-dir", str(tmp / "state"), *extra],
                                 cwd=tmp, capture_output=True, text=True)
            ran = sorted(f.name[len("ran-"):] for f in suite.glob("ran-*"))
        return run, ran

    def test_only_group_and_skip_group_split_the_suite_between_them(self):
        count = lambda stderr: int(tail(stderr)[0].split()[1])
        whole, whole_ran = self.run_grouped()
        only, only_ran = self.run_grouped("--only-group", "browser")
        rest, rest_ran = self.run_grouped("--skip-group", "browser")
        for run in (whole, only, rest):
            self.assertEqual(run.returncode, 0, run.stderr)
            self.assertNotIn("never loaded", run.stderr)
        self.assertEqual(whole_ran, ["test_x_grouped", "test_y_plain"])
        self.assertEqual(only_ran, ["test_x_grouped"])
        self.assertEqual(rest_ran, ["test_y_plain"])
        self.assertEqual(count(only.stderr), 2)
        self.assertEqual(count(only.stderr) + count(rest.stderr), count(whole.stderr))

    def test_a_group_no_module_declares_is_not_refused(self):
        # An old tagged tree predates the markers: --skip-group then runs the
        # whole suite and --only-group nothing, still covering it once.
        only, only_ran = self.run_grouped("--only-group", "no-such-group")
        rest, rest_ran = self.run_grouped("--skip-group", "no-such-group")
        self.assertEqual((only.returncode, only_ran), (0, []), only.stderr)
        self.assertEqual(tail(only.stderr)[0], "Ran 0 tests")
        self.assertEqual((rest.returncode, rest_ran), (0, ["test_x_grouped", "test_y_plain"]),
                         rest.stderr)

    def test_only_group_and_skip_group_are_exclusive(self):
        run, ran = self.run_grouped("--only-group", "browser", "--skip-group", "browser")
        self.assertEqual((run.returncode, ran), (2, []), run.stderr)

    def test_a_long_left_out_list_is_cut_short(self):
        line = runner.describe_left_out({f"test_m{i:02}": 1 for i in range(runner.LISTED + 3)})
        self.assertTrue(line.endswith(", and 3 more"), line)
        self.assertIn(f"left out {runner.LISTED + 3} module(s)", line)

    def test_only_a_top_level_true_marks_a_module_exclusive(self):
        with tempfile.TemporaryDirectory(prefix="forgepact-runner-") as tmp:
            tmp = Path(tmp)
            write_suite(tmp, {
                "test_yes.py": "PARALLEL_EXCLUSIVE = True\n",
                "test_no.py": "PARALLEL_EXCLUSIVE = False\n",
                "test_nested.py": "class C:\n    PARALLEL_EXCLUSIVE = True\n",
                "test_plain.py": "import unittest\n",
            })
            self.assertTrue(runner.exclusive_of("test_yes", tmp))
            for name in ("test_no", "test_nested", "test_plain", "test_absent"):
                self.assertFalse(runner.exclusive_of(name, tmp), name)


class CoverageTests(unittest.TestCase):
    def test_the_real_suite_loads_exactly_what_serial_discovery_runs(self):
        discovered = runner.discover(ROOT / "tests")
        loaded = []
        for module in runner.shard(discovered):
            loaded.extend(test.id() for test in runner.iter_tests(
                runner.load_module(ROOT / "tests", module)))
        self.assertGreater(len(discovered), 1000)
        self.assertEqual(Counter(loaded), Counter(discovered))

    def test_the_default_suite_is_forgepacts_from_any_cwd(self):
        # From the hub root, a bare `tests` would be the hub's suite.
        with tempfile.TemporaryDirectory(prefix="forgepact-runner-") as tmp:
            listed = subprocess.run([sys.executable, str(SCRIPT), "--list"], cwd=tmp,
                                    capture_output=True, text=True, check=True).stdout.split()
        self.assertEqual(Counter(listed), Counter(runner.discover(ROOT / "tests")))

    def test_a_missing_extra_or_doubled_id_is_a_problem(self):
        self.assertEqual(runner.coverage_problems(["a.T.x", "b.T.y"], ["b.T.y", "a.T.x"]), [])
        self.assertTrue(runner.coverage_problems(["a.T.x", "b.T.y"], ["a.T.x"]))
        self.assertTrue(runner.coverage_problems(["a.T.x"], ["a.T.x", "c.T.z"]))
        self.assertTrue(runner.coverage_problems(["a.T.x"], ["a.T.x", "a.T.x"]))

    def test_the_id_set_check_still_catches_a_missing_id_beside_an_exclusion(self):
        ids = ["a.T.x", "a.T.y", "b.T.z", "c.T.w"]
        chosen = runner.left_out(runner.shard(ids), ["b"])
        self.assertEqual(chosen, {"b": 1})
        expected = runner.without(ids, chosen)
        self.assertEqual(expected, ["a.T.x", "a.T.y", "c.T.w"])
        self.assertEqual(runner.coverage_problems(expected, ["c.T.w", "a.T.y", "a.T.x"]), [])
        # A module that was not left out still has to load every id.
        self.assertTrue(runner.coverage_problems(expected, ["a.T.x", "c.T.w"]))
        # And an excluded module's id turning up is still an extra.
        self.assertTrue(runner.coverage_problems(expected, ["a.T.x", "a.T.y", "c.T.w", "b.T.z"]))
        with self.assertRaises(runner.UnknownModule):
            runner.left_out(runner.shard(ids), ["d*"])

    def test_the_real_suite_lists_less_the_excluded_modules(self):
        full = runner.discover(ROOT / "tests")
        run = subprocess.run([sys.executable, str(SCRIPT), "--list", "--exclude-module", "test_panel_e2e*",
                              "--exclude-module", "test_panel_perf"],
                             capture_output=True, text=True, check=True)
        listed = run.stdout.split()
        gone = [i for i in full if runner.module_of(i) == "test_panel_perf"
                or runner.module_of(i).startswith("test_panel_e2e")]
        self.assertGreaterEqual(len(gone), 6)
        self.assertEqual(Counter(listed) + Counter(gone), Counter(full))
        self.assertIn("left out 9 module(s)", run.stderr)

    def test_every_panel_browser_module_is_capped_or_alone(self):
        # A module that drives the panel's browser suites (through
        # panel_browser's npm helper) shares the panel-browser cap, or, if it
        # measures timing, runs alone; never as an ordinary module on every core.
        tests = ROOT / "tests"
        drivers = sorted(p.stem for p in tests.glob("test_*.py")
                         if re.search(r"^from panel_browser import", p.read_text(encoding="utf-8"), re.M))
        self.assertGreaterEqual(len(drivers), 7, drivers)
        for name in drivers:
            with self.subTest(module=name):
                self.assertTrue(runner.group_of(name, tests) == "panel-browser"
                                or runner.exclusive_of(name, tests))
        self.assertTrue(runner.exclusive_of("test_panel_perf", tests))
        self.assertIsNone(runner.group_of("test_panel_oracle", tests))

    def test_the_panel_browser_cap_has_a_default_and_can_be_overridden(self):
        self.assertEqual(runner.parse_limits([])["panel-browser"], 4)
        self.assertEqual(runner.parse_limits(["panel-browser=1"])["panel-browser"], 1)
        self.assertEqual(runner.parse_limits(["solo=2"]), {"panel-browser": 4, "solo": 2})

    def test_an_import_failure_stays_with_its_module(self):
        self.assertEqual(runner.module_of("unittest.loader._FailedTest.test_broken"), "test_broken")
        self.assertEqual(runner.module_of("test_x.Case.test_y"), "test_x")


if __name__ == "__main__":
    unittest.main()
