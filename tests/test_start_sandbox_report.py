"""startSandbox (panel/tests/lib/browser.mjs) says what became of a sandbox.

A sandbox that exits before reporting its port, and a request to one that is
gone, must name the sandbox's state and quote its stderr, and every stderr
line must reach the suite's output labelled with its sandbox. 2.0.0's release
run 36372423744 printed only "connect ECONNREFUSED 127.0.0.1:64600", which
cannot tell a sandbox that had exited from one refusing connections while it
served, and a traceback no one could place.

Needs node and the panel's dev dependencies (browser.mjs imports
playwright-core), not Edge or a built panel: no browser is started.
"""
import json
import re
import shutil
import subprocess
import unittest
from pathlib import Path

PANEL = Path(__file__).resolve().parents[1] / "panel"
BROWSER_LIB = PANEL / "tests" / "lib" / "browser.mjs"

# A sandbox with a seed that cannot report its port within 0.5 s (a start
# takes seconds: importing hs_game_sdk alone is more), a seed key the sandbox
# refuses (exit 2, a message on its stderr), then a sandbox that starts, stops
# and is asked for its state afterwards. The two later cases take long enough
# for a late sandbox left running to reach its seed and print the failure the
# old start timeout caused.
SCRIPT = """
import { startSandbox } from %(lib)s;
const out = {};
try { await startSandbox({ seed: { theme: 'ledger' }, startTimeoutMs: 500 }); out.late = 'started'; }
catch (e) { out.late = e.message; }
try { await startSandbox({ seed: { no_such_key: 1 } }); out.early = 'started'; }
catch (e) { out.early = e.message; }
const sandbox = await startSandbox({ offline: true });
out.port = sandbox.port;
await sandbox.stop();
try { await sandbox.state(); out.gone = 'answered'; }
catch (e) { out.gone = e.message; }
console.log(JSON.stringify(out));
"""


def _missing():
    if not shutil.which("node"):
        return "node is not on PATH"
    if not (PANEL / "node_modules" / "playwright-core").is_dir():
        return "panel dev dependencies are not installed: run npm --prefix panel ci"
    return None


class StartSandboxReportTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        missing = _missing()
        if missing:
            raise unittest.SkipTest(missing)
        script = SCRIPT % {"lib": json.dumps(BROWSER_LIB.as_uri())}
        result = subprocess.run([shutil.which("node"), "--input-type=module", "-e", script],
                                cwd=PANEL, capture_output=True, text=True, encoding="utf-8",
                                errors="replace", timeout=600)
        lines = [line for line in result.stdout.splitlines() if line.startswith("{")]
        if result.returncode or not lines:
            raise AssertionError(f"node exited {result.returncode}:\n{result.stdout}\n{result.stderr}")
        cls.out = json.loads(lines[-1])
        cls.stderr = result.stderr

    def test_a_late_sandbox_is_stopped_before_its_seed_goes(self):
        self.assertIn("did not report its port within 0.5 s and was stopped", self.out["late"])
        self.assertRegex(self.out["late"], r"sandbox pid \d+ was stopped by SIGTERM")
        # Dropped while it still ran, the seed made the sandbox fail on the
        # missing file instead: "--seed: cannot read ...seed.json".
        self.assertNotIn("cannot read", self.stderr)

    def test_an_early_exit_quotes_the_sandboxs_own_reason(self):
        self.assertIn("sandbox server exited early", self.out["early"])
        self.assertRegex(self.out["early"], r"sandbox pid \d+ exited with code 2")
        self.assertIn("unknown config key(s) no_such_key", self.out["early"])

    def test_the_sandboxs_stderr_reaches_the_suite_labelled(self):
        self.assertRegex(self.stderr, re.compile(r"^\[sandbox pid \d+\] .*unknown config key\(s\) no_such_key", re.M))

    def test_a_refused_request_says_the_sandbox_had_exited(self):
        port = self.out["port"]
        self.assertIn(f"ECONNREFUSED 127.0.0.1:{port}", self.out["gone"])
        self.assertIn(f"sandbox :{port} exited with code 0", self.out["gone"])


if __name__ == "__main__":
    unittest.main()
