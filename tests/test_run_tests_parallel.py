"""Contract tests for tools/run_tests_parallel.py.

The runner is only worth having if it runs what serial discovery runs and
reports what serial unittest would report. So the real suite is checked for
id-set equality, and small fixture suites are run both ways, serially with
`python -m unittest discover` and through the runner, with the summaries
compared line for line (minus the timing).
"""
import importlib.util
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


class CoverageTests(unittest.TestCase):
    def test_the_real_suite_loads_exactly_what_serial_discovery_runs(self):
        discovered = runner.discover(ROOT / "tests")
        loaded = []
        for module in runner.shard(discovered):
            loaded.extend(test.id() for test in runner.iter_tests(
                runner.load_module(ROOT / "tests", module)))
        self.assertGreater(len(discovered), 1000)
        self.assertEqual(Counter(loaded), Counter(discovered))

    def test_a_missing_extra_or_doubled_id_is_a_problem(self):
        self.assertEqual(runner.coverage_problems(["a.T.x", "b.T.y"], ["b.T.y", "a.T.x"]), [])
        self.assertTrue(runner.coverage_problems(["a.T.x", "b.T.y"], ["a.T.x"]))
        self.assertTrue(runner.coverage_problems(["a.T.x"], ["a.T.x", "c.T.z"]))
        self.assertTrue(runner.coverage_problems(["a.T.x"], ["a.T.x", "a.T.x"]))

    def test_an_import_failure_stays_with_its_module(self):
        self.assertEqual(runner.module_of("unittest.loader._FailedTest.test_broken"), "test_broken")
        self.assertEqual(runner.module_of("test_x.Case.test_y"), "test_x")


if __name__ == "__main__":
    unittest.main()
