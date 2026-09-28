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


#: How long one `npm run <suite>` may run before its process tree is ended.
NPM_TIMEOUT = 900


def _npm(script):
    return _run_tree([shutil.which("npm"), "--prefix", str(PANEL), "run", script], NPM_TIMEOUT,
                     label=f"npm run {script}")


def _run_tree(args, timeout, label=None):
    """Run `args`; return its exit code and its stdout followed by its stderr.

    Past `timeout` seconds its whole process tree is ended and the output so
    far comes back with a line saying so. Ending only the process started
    here is not enough: npm runs a suite under cmd and node, which start Edge
    and the sandboxes, every one of them holds the output pipes, and
    communicate() waits on them for as long as they live. A sandbox that
    started late did exactly that under four-core load: `npm run e2e:motion`
    never finished, and `subprocess.run(timeout=900)` never returned
    (test_panel_browser_timeout.py).
    """
    proc = subprocess.Popen(args, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                            text=True, encoding="utf-8", errors="replace")
    try:
        out, err = proc.communicate(timeout=timeout)
    except subprocess.TimeoutExpired:
        if os.name == "nt":
            subprocess.run(["taskkill", "/PID", str(proc.pid), "/T", "/F"], capture_output=True)
        else:
            proc.kill()
        out, err = proc.communicate(timeout=60)
        note = f"\n{label or ' '.join(map(str, args))}: still running after {timeout} s; its process tree was ended\n"
        return proc.returncode or 1, out + err + note
    return proc.returncode, out + err
