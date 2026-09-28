"""panel_browser._run_tree ends a suite's whole process tree at its timeout.

npm runs each browser suite under cmd and node, which start Edge and the
sandboxes, and every one of them holds the output pipes. With
`subprocess.run(timeout=...)` only npm's own process was ended, and
communicate() then waited on the rest for as long as they lived: under
four-core load a sandbox that started late outlived `npm run e2e:motion`'s
900 s, and its worker never returned (2026-09-28). The same call with a
grandchild that sleeps 20 s returned after 20.2 s instead of its 3 s timeout.
"""
import sys
import time
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

# Not `from panel_browser import ...`: that line is what marks a module that
# drives the browser suites (test_run_tests_parallel), and this one does not.
import panel_browser  # noqa: E402

# A child that starts a grandchild on the same stdout and stderr, then both
# sleep far past the timeout under test.
PARENT = ("import subprocess, sys, time; "
          "subprocess.Popen([sys.executable, '-c', 'import time; time.sleep(120)']); "
          "print('child up', flush=True); time.sleep(120)")


@unittest.skipUnless(sys.platform == "win32", "the browser suites, and taskkill, are Windows only")
class RunTreeTests(unittest.TestCase):
    def test_a_timeout_ends_the_grandchild_that_holds_the_pipes(self):
        started = time.monotonic()
        code, out = panel_browser._run_tree([sys.executable, "-c", PARENT], timeout=3, label="parent")
        self.assertLess(time.monotonic() - started, 60, "waited on the grandchild's 120 s")
        self.assertNotEqual(code, 0)
        self.assertIn("child up", out)
        self.assertIn("parent: still running after 3 s; its process tree was ended", out)

    def test_a_finished_command_gives_its_code_then_stdout_then_stderr(self):
        code, out = panel_browser._run_tree(
            [sys.executable, "-c", "import sys; print('out'); print('err', file=sys.stderr); sys.exit(3)"],
            timeout=60)
        self.assertEqual(code, 3)
        self.assertEqual(out.split(), ["out", "err"])


if __name__ == "__main__":
    unittest.main()
