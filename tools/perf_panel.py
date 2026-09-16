#!/usr/bin/env python3
"""Measure the panel's two per-poll costs, before and after.

Both of the panel's background costs grow with something the player controls
and neither was ever measured:

* `plugin_boot_count` read the whole of an append-only `out.txt` every five
  seconds, so a poll got slower for every minute the session lasted;
* `running_paths` spawned a console process-listing tool per poll, and
  `wait_for_plugin_ready` calls it every 0.25 s for up to 60 s while the game
  starts.

This script runs both against a synthetic log and a process name that does not
exist, with no game, no network and no panel process - see AGENTS.md, "Limit
Rebuilds & Reruns During Development". It times the shipped implementation
against a reference copy of the pre-change one defined here, so the comparison
stays honest after the old code is gone, and exits non-zero if the improvement
falls below a floor.

Usage:
    py tools/perf_panel.py
    py tools/perf_panel.py --log-mb 128 --iterations 400
"""

from __future__ import annotations

import argparse
import os
import shutil
import statistics
import subprocess
import sys
import tempfile
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "src"))

import forgepact  # noqa: E402

MARKER = "BloodPact plugin loaded"
# A name no process has, so both implementations are timed on the enumeration
# itself rather than on opening handles.
ABSENT_PROCESS = "ForgePactPerfProbe.exe"
CREATE_NO_WINDOW = 0x08000000


def reference_full_scan(path: Path) -> int:
    """The pre-change `plugin_boot_count` body: whole file, decoded, scanned."""
    return path.read_text(encoding="utf-8", errors="ignore").count(MARKER)


def reference_tasklist(name: str) -> list:
    """The pre-change `running_paths` enumeration: one child process per call."""
    result = subprocess.run(
        ["tasklist", "/FI", f"IMAGENAME eq {name}", "/FO", "CSV", "/NH"],
        capture_output=True, text=True, encoding="utf-8", errors="replace",
        timeout=10, creationflags=CREATE_NO_WINDOW)
    pids = []
    for line in result.stdout.splitlines():
        parts = [p.strip('"') for p in line.split('","')]
        if len(parts) >= 2 and parts[0].strip('"').lower() == name.lower():
            try:
                pids.append(int(parts[1]))
            except ValueError:
                pass
    return pids


def median_ms(call, iterations: int) -> float:
    samples = []
    for _ in range(iterations):
        start = time.perf_counter()
        call()
        samples.append((time.perf_counter() - start) * 1000.0)
    return statistics.median(samples)


def write_log(path: Path, megabytes: float, boots: int) -> None:
    """A synthetic out.txt of the requested size with a known marker count."""
    filler = ("plugin chatter line that stands in for real output\n" * 64).encode("utf-8")
    banner = f"==== {MARKER} ====\n".encode("utf-8")
    target = int(megabytes * 1024 * 1024)
    written = 0
    every = max(1, target // max(1, boots))
    since_banner = 0
    with path.open("wb") as stream:
        while written < target:
            stream.write(filler)
            written += len(filler)
            since_banner += len(filler)
            if since_banner >= every:
                stream.write(banner)
                written += len(banner)
                since_banner = 0


def measure_boot_count(log: Path, cfg: dict, iterations: int) -> tuple[float, float, int]:
    # The full scan costs ~100 ms per call on a 64 MB log, so it is sampled
    # fewer times than the incremental path; both numbers are per-call medians
    # and stay comparable.
    full_iterations = max(3, iterations // 10)
    expected = reference_full_scan(log)

    full = median_ms(lambda: reference_full_scan(log), full_iterations)

    # What watcher() actually does: one warm call, then a call after the
    # plugin appended a line.
    forgepact.reset_boot_count_cache()
    forgepact.plugin_boot_count(cfg)

    # The append is the plugin's cost, not the panel's, so it happens outside
    # the timed region - what is measured is one poll over a log that grew
    # since the last one.
    samples = []
    for _ in range(iterations):
        with log.open("ab") as stream:
            stream.write(b"one more line of plugin chatter\n")
        start = time.perf_counter()
        forgepact.plugin_boot_count(cfg)
        samples.append((time.perf_counter() - start) * 1000.0)
    incremental = statistics.median(samples)

    got = forgepact.plugin_boot_count(cfg)
    if got != expected:
        raise SystemExit(f"FAIL boot-count: incremental={got} full-scan={expected}")
    return full, incremental, expected


def measure_process_scan(iterations: int) -> tuple[float, float]:
    reference_tasklist(ABSENT_PROCESS)                 # warm the child process path
    forgepact.running_paths(ABSENT_PROCESS)
    old = median_ms(lambda: reference_tasklist(ABSENT_PROCESS), iterations)
    new = median_ms(lambda: forgepact.running_paths(ABSENT_PROCESS), iterations)
    return old, new


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--log-mb", type=float, default=64.0,
                        help="size of the synthetic out.txt, in MB (default: 64)")
    parser.add_argument("--iterations", type=int, default=200,
                        help="calls per measurement (default: 200; the full-file scan "
                             "is sampled iterations/10 times and the process scan 20 "
                             "times, because each call costs orders of magnitude more)")
    parser.add_argument("--min-speedup", type=float, default=5.0,
                        help="fail unless the boot count got at least this much faster "
                             "(default: 5)")
    parser.add_argument("--min-process-speedup", type=float, default=1.5,
                        help="fail unless the process scan got at least this much "
                             "faster (default: 1.5)")
    args = parser.parse_args(argv)

    failures = []
    temp = Path(tempfile.mkdtemp(prefix="forgepact-perf-"))
    try:
        ipc = temp / "bin" / "bp_ipc"
        ipc.mkdir(parents=True)
        log = ipc / "out.txt"
        cfg = {"game_exe": str(temp / "bin" / "Hero_Siege.exe")}
        write_log(log, args.log_mb, boots=8)
        size_mb = log.stat().st_size / (1024 * 1024)

        full, incremental, boots = measure_boot_count(log, cfg, args.iterations)
        print(f"boot-count full-scan:    {full:9.3f} ms/call  "
              f"({size_mb:.1f} MB log, {boots} boots, {max(3, args.iterations // 10)} calls)")
        print(f"boot-count incremental:  {incremental:9.3f} ms/call  "
              f"({args.iterations} calls, one appended line each)")
        speedup = full / incremental if incremental > 0 else float("inf")
        print(f"boot-count speedup: {speedup:.1f}x")
        if speedup < args.min_speedup:
            failures.append(f"boot-count speedup {speedup:.1f}x is below {args.min_speedup}x")

        if os.name != "nt":
            print("process-scan: SKIPPED (the process snapshot is Windows-only)")
        elif not shutil.which("tasklist"):
            print("process-scan: SKIPPED (no reference implementation on PATH)")
        else:
            scans = 20
            old, new = measure_process_scan(scans)
            print(f"process-scan tasklist:   {old:9.3f} ms/call  ({scans} calls)")
            print(f"process-scan toolhelp:   {new:9.3f} ms/call  ({scans} calls)")
            ratio = old / new if new > 0 else float("inf")
            print(f"process-scan speedup: {ratio:.1f}x")
            if ratio < args.min_process_speedup:
                failures.append(
                    f"process-scan speedup {ratio:.1f}x is below {args.min_process_speedup}x")
    finally:
        forgepact.reset_boot_count_cache()
        shutil.rmtree(temp, ignore_errors=True)

    for line in failures:
        print(f"FAIL {line}")
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
