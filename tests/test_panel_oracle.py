#!/usr/bin/env python3
"""The panel's browser suites, run from the Python suite.

`panel/` is a Svelte + Vite project; what proves a build of it behaves like
the page it replaced lives in `panel/tests/`:

- `npm run oracle:replay` replays `panel/tests/behaviour-oracle.json` (every
  control's POST bodies and plugin commands, recorded from the old page)
  against the current build and fails on any difference;
- `npm run e2e` runs the checks ported from the old agent-browser harnesses
  (saves, failures, filters, keyboard, install and launch paths).

Both drive the installed Edge headless through playwright-core against
`tests/panel_sandbox_server.py`, which needs a built `panel/dist/`. Each test
here skips, naming what is missing, when `node`/`npm`, Edge, the installed
dev dependencies or the build are absent - build first with
`npm --prefix panel ci` and `npm --prefix panel run build`. Together they
take a couple of minutes.
"""
import json
import os
import re
import shutil
import subprocess
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PANEL = ROOT / "panel"
ORACLE = PANEL / "tests" / "behaviour-oracle.json"
EDGE_PATHS = (
    Path(os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)")) / "Microsoft" / "Edge" / "Application" / "msedge.exe",
    Path(os.environ.get("ProgramFiles", r"C:\Program Files")) / "Microsoft" / "Edge" / "Application" / "msedge.exe",
)


def _missing():
    """What the browser suites need and this machine lacks, or None."""
    if not shutil.which("node"):
        return "node is not on PATH"
    if not shutil.which("npm"):
        return "npm is not on PATH"
    if not any(p.is_file() for p in EDGE_PATHS):
        return "Microsoft Edge is not installed (playwright-core drives it through channel 'msedge')"
    if not (PANEL / "node_modules" / "playwright-core").is_dir():
        return "panel dev dependencies are not installed: run npm --prefix panel ci"
    if not (PANEL / "dist" / "index.html").is_file():
        return "panel/dist/index.html is missing: run npm --prefix panel run build"
    return None


def _npm(script):
    result = subprocess.run([shutil.which("npm"), "--prefix", str(PANEL), "run", script],
                            capture_output=True, text=True, encoding="utf-8", errors="replace",
                            timeout=900)
    return result.returncode, result.stdout + result.stderr


class BehaviourOracleFileTests(unittest.TestCase):
    """The recording itself: no browser needed."""

    def test_oracle_was_recorded_from_the_old_page_and_covers_every_control(self):
        oracle = json.loads(ORACLE.read_text(encoding="utf-8"))
        self.assertEqual(oracle["recordedFrom"], "legacy")
        self.assertGreaterEqual(len(oracle["steps"]), 120)
        for step in oracle["steps"]:
            self.assertIn("posts", step)
            self.assertIn("cmds", step)
        # Every kind of control the walk knows was exercised, with live commands.
        actions = {s["action"] for s in oracle["steps"]}
        for action in ("click", "max", "min", "increment", "decrement", "type", "select"):
            self.assertIn(action, actions)
        self.assertTrue(any(s["cmds"] for s in oracle["steps"]), "no step recorded a plugin command")
        # Negative control: a disabled control is recorded as such, not as a click.
        self.assertTrue(any(s["action"] == "skipped-disabled" for s in oracle["steps"]))


class PanelBrowserSuiteTests(unittest.TestCase):
    def setUp(self):
        missing = _missing()
        if missing:
            self.skipTest(missing)

    def test_behaviour_oracle_replays_with_no_mismatch(self):
        code, out = _npm("oracle:replay")
        lines = [l for l in out.splitlines() if l.startswith("oracle: ")]
        self.assertEqual(code, 0, out[-4000:])
        self.assertTrue(lines and re.fullmatch(r"oracle: \d+ steps, 0 mismatches", lines[-1]), out[-4000:])

    def test_e2e_suite_passes(self):
        code, out = _npm("e2e")
        lines = [l for l in out.splitlines() if l.startswith("e2e: ")]
        self.assertEqual(code, 0, out[-4000:])
        self.assertTrue(lines and re.fullmatch(r"e2e: (\d+)/\1 checks passed", lines[-1]), out[-4000:])


if __name__ == "__main__":
    unittest.main()
