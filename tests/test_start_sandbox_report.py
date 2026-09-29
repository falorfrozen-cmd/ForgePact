"""startSandbox (panel/tests/lib/browser.mjs) says what became of a sandbox.

A sandbox that exits before reporting its port, and a request to one that is
gone, must name the sandbox's state and quote its stderr, and every stderr
line must reach the suite's output labelled with its sandbox. 2.0.0's release
run 36372423744 printed only "connect ECONNREFUSED 127.0.0.1:64600", which
cannot tell a sandbox that had exited from one refusing connections while it
served, and a traceback no one could place.

The node script needs node and the panel's dev dependencies (browser.mjs
imports playwright-core), not Edge or a built panel: no browser is started.
"""
import json
import re
import shutil
import subprocess
import sys
import unittest
from pathlib import Path
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parent))
import panel_sandbox_server  # noqa: E402

PANEL = Path(__file__).resolve().parents[1] / "panel"
BROWSER_LIB = PANEL / "tests" / "lib" / "browser.mjs"

# Three sandboxes, and the script stops each one that starts:
# - a seeded one told to wait 10 s before it imports anything or reads its
#   seed (`startDelayMs`, the server's --start-delay), given 0.5 s to report
#   its port;
# - one with a seed key the server refuses (exit 2, a message on its stderr);
# - one told to wait 1 s, which starts, stops and is asked for its state
#   afterwards. That its start takes at least 1 s is the positive control:
#   the delay reaches the server, so the first sandbox is late by construction.
# The late case used to count on a start taking seconds (importing
# hs_game_sdk alone did). Since the SDK loads its tables on first use (hub PR
# #286) a start takes about 0.5 s, and 0.25 s without the SDK beside the
# checkout, so that sandbox could start in time, and then nothing stopped it:
# node never exited, and setUpClass waited out its 600 s timeout (2026-09-29).
# A late sandbox left running, as the old start timeout left it, would wake
# after its 10 s (longer than the timer's 5 s wait for it to close), find its
# seed gone and print "--seed: cannot read"; node waits for it, since it
# holds the pipes.
SCRIPT = """
import { startSandbox } from %(lib)s;
const out = {};
try {
  const started = await startSandbox({ seed: { theme: 'ledger' }, startTimeoutMs: 500, startDelayMs: 10000 });
  out.late = 'started';
  await started.stop();
} catch (e) { out.late = e.message; }
try {
  const started = await startSandbox({ seed: { no_such_key: 1 } });
  out.early = 'started';
  await started.stop();
} catch (e) { out.early = e.message; }
const asked = performance.now();
const sandbox = await startSandbox({ offline: true, startDelayMs: 1000 });
out.startMs = Math.round(performance.now() - asked);
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

    def test_a_start_delay_holds_the_port_back(self):
        # Without it the late case is late only on a machine slow enough.
        self.assertGreaterEqual(self.out["startMs"], 1000)

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


class StartDelayTests(unittest.TestCase):
    """The server's --start-delay comes before it imports anything or reads its seed.

    Waiting only before the port line would still make the late case late, but
    its sandbox would have read its seed by then, and "cannot read" could no
    longer catch a seed dropped while the sandbox ran.
    """

    def steps(self, argv):
        seen = []

        class Loaded(Exception):
            pass

        def load(parser, src):
            seen.append("load")
            raise Loaded  # main() reads the seed after this

        with patch.object(panel_sandbox_server, "time") as fake_time, \
                patch.object(panel_sandbox_server, "load", side_effect=load):
            fake_time.sleep.side_effect = lambda seconds: seen.append(("sleep", seconds))
            with self.assertRaises(Loaded):
                panel_sandbox_server.main(argv)
        return seen

    def test_the_delay_comes_before_the_import(self):
        self.assertEqual(self.steps(["--start-delay", "10", "--seed", "seed.json"]), [("sleep", 10.0), "load"])

    def test_no_delay_without_the_flag(self):
        self.assertEqual(self.steps(["--seed", "seed.json"]), ["load"])


if __name__ == "__main__":
    unittest.main()
