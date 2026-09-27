"""What the panel's browser suites share when the Python suite runs them.

Not a test module (discovery only loads `test_*.py`): the skip conditions and
the npm helper that `test_panel_oracle_replay.py`, `test_panel_e2e*.py` and
`test_panel_perf.py` import, rather than each carrying a copy.

Each of those modules runs one `npm run <suite>` in `panel/` and nothing
else, because `tools/run_tests_parallel.py` parallelises by module: the seven
suites once lived in one module and ran back to back, about 13 minutes on one
worker while the other ~1,600 tests took one. They declare
`PARALLEL_GROUP = "panel-browser"`, which the runner caps at a few at a time
(its `DEFAULT_GROUP_LIMITS`). Each suite starts its own sandbox server on an
OS-assigned port with its own temp config and browser profile, so they share
only the built `panel/dist/` and the CPU. `test_panel_perf.py` measures frame
timings and declares `PARALLEL_EXCLUSIVE` instead, so it runs last and alone.

All of them drive the installed Edge headless through playwright-core against
`tests/panel_sandbox_server.py`, which needs a built `panel/dist/`. Each test
skips, naming what is missing, when `node`/`npm`, Edge, the installed dev
dependencies or the build are absent - build first with
`npm --prefix panel ci` and `npm --prefix panel run build`.
"""
import os
import shutil
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PANEL = ROOT / "panel"
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
