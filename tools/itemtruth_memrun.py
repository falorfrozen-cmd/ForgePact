#!/usr/bin/env python3
"""Measure what Item Truth evaluation requests leave behind in the game's memory.

Launches Hero Siege to the main menu (no character), queues evaluation requests
the way the Item Editor does (`itemtruth\\requests\\<id>.req`, ItemTruth.hpp),
samples the game's private bytes every second from outside the process, and
closes the game normally at the end. Nothing is placed, dropped or saved: every
item is built in memory by the game's own save loader and journaled.

    py -3 tools/itemtruth_memrun.py run --items 20000 [--mix] [--out DIR]
    py -3 tools/itemtruth_memrun.py control [--items 20000] [--out DIR]

`run` is the measurement: a settle period at the menu, one request of --items
items, then --post seconds more. `summary.json` gives the baseline (the last 20 s
before the request), the peak while the game builds, the level after, and the
difference per item. A build that keeps its items grows by their size, one that
does not stays flat.

`control` is its positive control, for the research build only (`build.bat
dev`): the same request twice in one launch, the second with `truthmem hold on`,
which keeps every evaluated item in a global struct the game's collector sees.
The first half must stay flat and the second must grow; a harness that cannot
see the second grow has not measured the first. The `truthmem` lines ForgePact
wrote are copied into `truthmem.txt`.

Items come from the shapes the running build has already evaluated (the
journal's own `"src":"eval"` records), so the game has built every shape at least
once. --mix draws them by class the way the Item Editor's seed-table sessions
did: white bases, uniques, socketed items and runewords (the heaviest build,
with a rune item inside the item). Seeds are redrawn, except a runeword's, whose
base must stay Common.

By default the journal files of the measured game session are moved into --out
afterwards, but only when every line in them belongs to this run's own
requests: the Item Editor reads every journal file, and a run's items are not
the player's. --keep-journal leaves them.

The game starts with Windows' default error mode (CREATE_DEFAULT_ERROR_MODE), not
this tool's: Python run from Git Bash has SEM_NOGPFAULTERRORBOX, and a game that
inherits it aborts without a dump or an event. The report also gives the game's
exit code (`exit_code`; 0xC0000409 is an abort), because a missing dump is not a
clean exit.

Windows only, standard library only. Needs Item Truth on (the Item Editor's
Game truth switch creates `itemtruth\\capture.request`) and the game closed.
See AGENTS.md, "Prove the Instrument Before Trusting a Negative Result".
"""

from __future__ import annotations

import argparse
import csv
import ctypes
import ctypes.wintypes as wt
import json
import os
import random
import subprocess
import sys
import time
from pathlib import Path

REQUEST_ID_SUFFIX = "memrun"
# Item timestamps of the run's requests: far above any real item's (unix ms)
# and apart from the Item Editor's seed-table ranges (1,795,000,000,000+).
TS_BASE = 1_799_000_000_000
CLASSES = ("white", "unique", "socket", "runeword")
# About the Item Editor's seed-table sessions of 2026-09-26, runewords weighted up.
MIX = (("white", 0.55), ("unique", 0.25), ("socket", 0.08), ("runeword", 0.12))
MAX_TEMPLATES = 600


# ---- the requests ---------------------------------------------------------------

def classify(definition: dict) -> str:
    """white (c 0), unique (c 1), socket (zz, no payload), runeword (rune payloads)."""
    if "s1" in definition:
        return "runeword"
    if "zz" in definition:
        return "socket"
    return "unique" if definition.get("c") in (1, 1.0) else "white"


def load_templates(journal: Path, per_class: int = MAX_TEMPLATES) -> dict[str, list[tuple[int, dict]]]:
    """(item type, definition) of evaluated items by class, newest files first."""
    found: dict[str, list[tuple[int, dict]]] = {name: [] for name in CLASSES}
    files = sorted(journal.glob("live-*.ndjson"), key=lambda p: p.stat().st_mtime, reverse=True)
    for path in files:
        try:
            with path.open("rb") as handle:
                for raw in handle:
                    if b"eval" not in raw:   # a cheap filter; the parsed record decides
                        continue
                    try:
                        record = json.loads(raw)
                    except ValueError:
                        continue
                    if not isinstance(record, dict) or record.get("src") != "eval" or record.get("kind") is not None:
                        continue
                    definition, item_type = record.get("def"), record.get("type")
                    if not isinstance(definition, dict) or not isinstance(item_type, int) or isinstance(item_type, bool):
                        continue
                    bucket = found[classify(definition)]
                    if len(bucket) < per_class:
                        bucket.append((item_type, definition))
        except OSError:
            continue
        if all(len(bucket) >= per_class for bucket in found.values()):
            break
    return found


def request_lines(templates: dict[str, list[tuple[int, dict]]], count: int, seed: int, ts_base: int,
                  mix: bool) -> list[str]:
    """`<item key>\\t<save data json>` lines, one distinct timestamp each.
    Without --mix only white bases are used, each with a fresh seed."""
    rng = random.Random(seed)
    shares = MIX if mix else (("white", 1.0),)
    usable = [(name, share) for name, share in shares if templates.get(name)]
    if not usable:
        raise ValueError("no evaluated items of the needed classes in the journal")
    total = sum(share for _, share in usable)
    lines = []
    for index in range(count):
        pick, acc, name = rng.random() * total, 0.0, usable[-1][0]
        for candidate, share in usable:
            acc += share
            if pick <= acc:
                name = candidate
                break
        item_type, definition = templates[name][rng.randrange(len(templates[name]))]
        data = json.loads(json.dumps(definition))
        if name != "runeword":
            data["a"] = float(rng.randrange(1, 2_147_483_647))
        text = json.dumps(data, separators=(",", ":"), ensure_ascii=True)
        lines.append(f"0-0-{ts_base + index}-{item_type}\t{text}\n")
    return lines


def write_request(requests: Path, lines: list[str], tag: str) -> str:
    requests.mkdir(parents=True, exist_ok=True)
    request_id = f"{int(time.time() * 1000)}-{REQUEST_ID_SUFFIX}{tag}"
    temp = requests / f"{request_id}.tmp"
    temp.write_text("".join(lines), encoding="utf-8", newline="\n")
    os.replace(temp, requests / f"{request_id}.req")
    return request_id


def session_files(journal: Path, pid: int) -> list[Path]:
    return sorted(journal.glob(f"live-*-{pid}-*.ndjson"))


def eval_progress(journal: Path, pid: int, request_id: str) -> tuple[int, bool]:
    """(items done, finished) of one request, from its game session's journal."""
    done, finished = 0, False
    marker = request_id.encode()
    for path in session_files(journal, pid):
        try:
            with path.open("rb") as handle:
                for raw in handle:
                    if marker not in raw:
                        continue
                    try:
                        record = json.loads(raw)
                    except ValueError:
                        continue
                    if isinstance(record, dict) and record.get("kind") == "eval" and record.get("req") == request_id:
                        done = max(done, int(record.get("done", 0)))
                        finished = finished or record.get("finished") is True
        except OSError:
            continue
    return done, finished


def only_our_lines(path: Path, request_ids: set[str]) -> bool:
    """True when every line of a journal file is one of our requests' records or
    progress lines: then the file holds nothing of the player's."""
    try:
        with path.open("rb") as handle:
            for raw in handle:
                if not raw.strip():
                    continue
                try:
                    record = json.loads(raw)
                except ValueError:
                    return False
                if not isinstance(record, dict) or record.get("req") not in request_ids:
                    return False
                if record.get("kind") not in (None, "eval") or (record.get("kind") is None and record.get("src") != "eval"):
                    return False
    except OSError:
        return False
    return True


def settled(private_mb: list[float], seconds_at_menu: float, min_seconds: float,
            window: int = 30, span_mb: float = 5.0, release_mb: float = 150.0) -> bool:
    """Ready for a request: at least `min_seconds` at the menu, and the last
    `window` one-second samples steady (within `span_mb`) and all at least
    `release_mb` below the peak - that is, the menu's one-time release is behind
    it. Measured 2026-09-26: 240-370 MB, 45-126 s after launch, with or without
    requests, after brief dips of up to 133 MB that came back. A request queued
    before the release would have the release subtract from whatever it keeps."""
    if seconds_at_menu < min_seconds or len(private_mb) < window:
        return False
    recent = private_mb[-window:]
    return max(recent) - min(recent) <= span_mb and max(recent) <= max(private_mb) - release_mb


def summarize(rows: list[tuple[float, str, float, float]], items: int, request_phase: str = "eval") -> dict:
    """Baseline = mean of the last 20 samples before the request; after = the last sample."""
    before = [row[2] for row in rows if row[1] == "settle"][-20:]
    during = [row[2] for row in rows if row[1] == request_phase]
    after = [row[2] for row in rows if row[1] == "post"]
    out = {"items": items}
    if before:
        out["baseline_mb"] = round(sum(before) / len(before), 1)
    if during:
        out["peak_while_building_mb"] = max(during)
    if after:
        out["after_first_mb"], out["after_last_mb"], out["after_min_mb"] = after[0], after[-1], min(after)
    if before and after and items:
        out["delta_mb"] = round(after[-1] - out["baseline_mb"], 1)
        out["kb_per_item"] = round((after[-1] - out["baseline_mb"]) * 1024 / items, 2)
    return out


# ---- the game process -----------------------------------------------------------

class _Counters(ctypes.Structure):
    _fields_ = [("cb", wt.DWORD), ("PageFaultCount", wt.DWORD),
                ("PeakWorkingSetSize", ctypes.c_size_t), ("WorkingSetSize", ctypes.c_size_t),
                ("QuotaPeakPagedPoolUsage", ctypes.c_size_t), ("QuotaPagedPoolUsage", ctypes.c_size_t),
                ("QuotaPeakNonPagedPoolUsage", ctypes.c_size_t), ("QuotaNonPagedPoolUsage", ctypes.c_size_t),
                ("PagefileUsage", ctypes.c_size_t), ("PeakPagefileUsage", ctypes.c_size_t),
                ("PrivateUsage", ctypes.c_size_t)]


class _MemoryStatus(ctypes.Structure):
    _fields_ = [("dwLength", wt.DWORD), ("dwMemoryLoad", wt.DWORD),
                ("ullTotalPhys", ctypes.c_uint64), ("ullAvailPhys", ctypes.c_uint64),
                ("ullTotalPageFile", ctypes.c_uint64), ("ullAvailPageFile", ctypes.c_uint64),
                ("ullTotalVirtual", ctypes.c_uint64), ("ullAvailVirtual", ctypes.c_uint64),
                ("ullAvailExtendedVirtual", ctypes.c_uint64)]


FAST_FAIL_EXIT = 0xC0000409


def describe_exit(code: int | None) -> str:
    """The game's exit code as text. 0xC0000409 is a fast fail - abort() - which on
    2026-09-26 was a module's exit-time std::thread destructor (docs/item-truth-memory-research.md)."""
    if code is None:
        return "exit code unknown (still running or not read)"
    code &= 0xFFFFFFFF
    if code == 0:
        return "exit 0"
    if code == FAST_FAIL_EXIT:
        return "exit 0xC0000409: the game aborted (a fast fail) while it ran or exited"
    return f"exit 0x{code:08X}"


class Game:
    """The launched game: memory readings through its own process handle."""

    def __init__(self, exe: Path):
        self.exe = exe
        self.exit_code: int | None = None
        startup = subprocess.STARTUPINFO()
        startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
        startup.wShowWindow = 7   # SW_SHOWMINNOACTIVE: in the background
        # The default error mode, not this tool's: run from Git Bash, Python has
        # 0x3 (SEM_NOGPFAULTERRORBOX), and a game that inherits it aborts without
        # Windows writing a dump or an Application Error event. With the default,
        # a crash is reported as it would be for a player; the exit code is read too.
        self.process = subprocess.Popen([str(exe)], cwd=str(exe.parent), startupinfo=startup,
                                        creationflags=subprocess.CREATE_DEFAULT_ERROR_MODE)
        self.pid = self.process.pid
        kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
        self._kernel32 = kernel32
        kernel32.OpenProcess.argtypes = [wt.DWORD, wt.BOOL, wt.DWORD]
        kernel32.OpenProcess.restype = wt.HANDLE
        kernel32.K32GetProcessMemoryInfo.argtypes = [wt.HANDLE, ctypes.c_void_p, wt.DWORD]
        kernel32.K32GetProcessMemoryInfo.restype = wt.BOOL
        kernel32.CloseHandle.argtypes = [wt.HANDLE]
        self.handle = kernel32.OpenProcess(0x1000 | 0x0010, False, self.pid)   # QUERY_LIMITED | VM_READ
        if not self.handle:
            raise OSError(ctypes.get_last_error(), "OpenProcess")

    def alive(self) -> bool:
        return self.process.poll() is None

    def memory_mb(self) -> tuple[float, float, float]:
        """(private, working set, peak commit) in MB."""
        counters = _Counters()
        counters.cb = ctypes.sizeof(counters)
        if not self._kernel32.K32GetProcessMemoryInfo(self.handle, ctypes.byref(counters), counters.cb):
            raise OSError(ctypes.get_last_error(), "GetProcessMemoryInfo")
        mb = 1024 * 1024
        return counters.PrivateUsage / mb, counters.WorkingSetSize / mb, counters.PeakPagefileUsage / mb

    def free_commit_gb(self) -> float:
        status = _MemoryStatus()
        status.dwLength = ctypes.sizeof(status)
        self._kernel32.GlobalMemoryStatusEx(ctypes.byref(status))
        return status.ullAvailPageFile / 2**30

    def close(self, timeout: int = 30) -> bool:
        """The window's own close, like the player's; never forced."""
        subprocess.run(["powershell", "-NoProfile", "-NonInteractive", "-Command",
                        f"$g=Get-Process -Id {self.pid} -ErrorAction SilentlyContinue; "
                        f"if($g){{[void]$g.CloseMainWindow(); [void]$g.WaitForExit({timeout * 1000})}}"],
                       check=False, capture_output=True)
        try:
            self.process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            return False
        self.exit_code = self.process.returncode
        self._kernel32.CloseHandle(self.handle)
        return True


def game_running() -> bool:
    out = subprocess.run(["tasklist", "/FI", "IMAGENAME eq Hero_Siege.exe", "/FO", "CSV", "/NH"],
                         capture_output=True, text=True, check=False).stdout
    return "hero_siege.exe" in out.casefold()


def default_exe() -> Path | None:
    config = Path(os.environ.get("LOCALAPPDATA", "")) / "Hero_Siege" / "forgepact.json"
    try:
        exe = json.loads(config.read_text(encoding="utf-8")).get("game_exe")
    except (OSError, ValueError):
        return None
    return Path(exe) if exe else None


# ---- a measured session ------------------------------------------------------------

class Session:
    def __init__(self, args):
        self.args = args
        self.root = Path(os.environ.get("LOCALAPPDATA", "")) / "Hero_Siege" / "itemtruth"
        self.journal = self.root / "journal"
        self.exe = Path(args.game_exe) if args.game_exe else default_exe()
        if not self.exe or not self.exe.is_file():
            raise SystemExit("Hero_Siege.exe not found: pass --game-exe")
        if not (self.root / "capture.request").is_file():
            raise SystemExit("Item Truth is off: turn on Game truth in the Item Editor (itemtruth\\capture.request)")
        if game_running():
            raise SystemExit("Hero Siege is running: close it first")
        self.ipc = self.exe.parent / "bp_ipc"
        self.out = Path(args.out)
        self.out.mkdir(parents=True, exist_ok=True)
        self.templates = load_templates(self.journal)
        self.rows: list[tuple[float, str, float, float]] = []
        self.notes: list[str] = []
        self.requests: set[str] = set()
        self.phase = "boot"
        self.game: Game | None = None
        self.t0 = 0.0

    def note(self, text: str) -> None:
        line = f"{time.time() - self.t0:7.1f}s {text}"
        self.notes.append(line)
        print(line, flush=True)

    def sample(self) -> None:
        private, working, _peak = self.game.memory_mb()
        now = time.time() - self.t0
        self.rows.append((round(now, 1), self.phase, round(private, 1), round(working, 1)))
        if private / 1024 > self.args.max_private_gb:
            raise RuntimeError(f"safety stop: private {private:.0f} MB")
        if self.game.free_commit_gb() < self.args.min_free_commit_gb:
            raise RuntimeError("safety stop: the system's free commit is low")

    def wait(self, seconds: float) -> None:
        end = time.time() + seconds
        while time.time() < end:
            if not self.game.alive():
                raise RuntimeError("the game exited")
            self.sample()
            time.sleep(1.0)

    def command(self, text: str) -> None:
        with (self.ipc / "cmd.txt").open("a", encoding="utf-8") as handle:
            handle.write(text + "\n")
        self.note(f"cmd {text}")
        self.wait(1)

    def evaluate(self, phase: str, tag: str, ts_base: int) -> None:
        lines = request_lines(self.templates, self.args.items, self.args.seed + ts_base, ts_base, self.args.mix)
        request_id = write_request(self.root / "requests", lines, tag)
        self.requests.add(request_id)
        self.phase = phase
        self.note(f"queued {len(lines)} items as {request_id}")
        while True:
            self.wait(2)
            done, finished = eval_progress(self.journal, self.game.pid, request_id)
            if finished:
                self.note(f"{request_id} finished: {done} built")
                return
            if time.time() - self.t0 > self.args.timeout:
                raise RuntimeError("timed out waiting for the request")

    def start(self) -> None:
        self.game = Game(self.exe)
        self.t0 = time.time()
        self.note(f"launched pid {self.game.pid}")
        while True:
            if not self.game.alive():
                raise RuntimeError("the game exited during start")
            self.sample()
            try:
                if json.loads((self.root / "status.json").read_text(encoding="utf-8")).get("pid") == self.game.pid:
                    break
            except (OSError, ValueError):
                pass
            if time.time() - self.t0 > 240:
                raise RuntimeError("Item Truth did not start")
            time.sleep(1.0)
        self.phase = "settle"
        self.note("Item Truth journal up")
        up = time.time()
        while True:
            self.wait(1)
            private = [row[2] for row in self.rows]
            if settled(private, time.time() - up, self.args.settle):
                self.note("settled: the menu's one-time release is behind and the level is steady")
                return
            if time.time() - up > self.args.settle_max:
                self.note(f"not settled after {self.args.settle_max:.0f} s: going on (see samples.csv)")
                return

    def finish(self, error: str | None) -> dict:
        if error:
            self.note(error)
        closed = self.game.close() if self.game and self.game.alive() else None
        if closed is not None:
            self.note(f"closed normally: {closed}")
        exit_code = None
        if self.game:
            if self.game.exit_code is None:
                self.game.exit_code = self.game.process.poll()   # it ended by itself
            exit_code = self.game.exit_code
            # A missing dump is not a clean exit; the exit code is the one that tells.
            self.note(describe_exit(exit_code))
        moved = []
        if self.game and not self.args.keep_journal:
            for path in session_files(self.journal, self.game.pid):
                if only_our_lines(path, self.requests):
                    target = self.out / path.name
                    os.replace(path, target)
                    moved.append(path.name)
                else:
                    self.note(f"left {path.name} in place: it holds lines of other requests or of play")
        with (self.out / "samples.csv").open("w", newline="", encoding="utf-8") as handle:
            writer = csv.writer(handle)
            writer.writerow(["t", "phase", "private_mb", "working_set_mb"])
            writer.writerows(self.rows)
        return {"pid": self.game.pid if self.game else None, "exe": str(self.exe), "notes": self.notes,
                "requests": sorted(self.requests), "journal_moved": moved, "error": error,
                "exit_code": None if exit_code is None else f"0x{exit_code & 0xFFFFFFFF:08X}"}


def cmd_run(args) -> int:
    session = Session(args)
    error = None
    try:
        session.start()
        session.evaluate("eval", "run", TS_BASE + args.ts_offset)
        session.phase = "post"
        session.wait(args.post)
    except (RuntimeError, OSError) as failure:
        error = str(failure)
    report = session.finish(error)
    report["summary"] = summarize(session.rows, args.items)
    (session.out / "summary.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    print(json.dumps(report["summary"], indent=2))
    return 1 if error else 0


def cmd_control(args) -> int:
    session = Session(args)
    out_txt = session.ipc / "out.txt"
    start = out_txt.stat().st_size if out_txt.exists() else 0
    error = None
    try:
        session.start()
        session.command("truthmem stat")
        session.evaluate("A-nohold", "a", TS_BASE + args.ts_offset)
        session.phase = "A-after"
        session.wait(30)
        session.command("truthmem gc")
        session.wait(15)
        session.command("truthmem hold on")
        session.evaluate("B-hold", "b", TS_BASE + args.ts_offset + args.items)
        session.phase = "B-after"
        session.wait(30)
        session.command("truthmem gc")
        session.wait(15)
        session.command("truthmem hold off")
        session.command("truthmem release")
        session.command("truthmem gc")
        session.phase = "released"
        session.wait(40)
        session.command("truthmem stat")
    except (RuntimeError, OSError) as failure:
        error = str(failure)
    report = session.finish(error)
    rows = session.rows

    def level(phase: str, last: int = 10) -> float | None:
        values = [row[2] for row in rows if row[1] == phase][-last:]
        return round(sum(values) / len(values), 1) if values else None

    settle = level("settle", 20)
    a_after, b_after, released = level("A-after"), level("B-after"), level("released")
    control = {"items_per_half": args.items, "settle_mb": settle, "a_after_mb": a_after,
               "b_after_mb": b_after, "released_mb": released}
    if settle is not None and a_after is not None:
        control["a_kb_per_item"] = round((a_after - settle) * 1024 / args.items, 2)
    if a_after is not None and b_after is not None:
        control["b_kb_per_item"] = round((b_after - a_after) * 1024 / args.items, 2)
    report["control"] = control
    try:
        with out_txt.open("rb") as handle:
            handle.seek(start)
            text = handle.read().decode("utf-8", "replace")
        (session.out / "truthmem.txt").write_text(
            "\n".join(line for line in text.splitlines() if line.startswith("truthmem")), encoding="utf-8")
    except OSError:
        pass
    (session.out / "summary.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    print(json.dumps(control, indent=2))
    return 1 if error else 0


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest="action", required=True)
    for name in ("run", "control"):
        p = sub.add_parser(name)
        p.add_argument("--items", type=int, default=20_000)
        p.add_argument("--mix", action="store_true", help="white, unique, socketed and runeword items")
        p.add_argument("--out", default=str(Path.cwd() / f"itemtruth-memrun-{time.strftime('%Y%m%d-%H%M%S')}"))
        p.add_argument("--game-exe", help="default: the ForgePact panel's game_exe")
        p.add_argument("--settle", type=float, default=60.0, help="least seconds at the menu before the first request")
        p.add_argument("--settle-max", type=float, default=300.0, help="most seconds to wait for a steady menu")
        p.add_argument("--post", type=float, default=180.0, help="`run`: seconds sampled after the request")
        p.add_argument("--seed", type=int, default=20260926)
        p.add_argument("--ts-offset", type=int, default=0)
        p.add_argument("--timeout", type=float, default=1800.0)
        p.add_argument("--max-private-gb", type=float, default=11.0)
        p.add_argument("--min-free-commit-gb", type=float, default=1.5)
        p.add_argument("--keep-journal", action="store_true")
    args = parser.parse_args(argv)
    if args.action == "control":
        args.mix = True
    return cmd_run(args) if args.action == "run" else cmd_control(args)


if __name__ == "__main__":
    sys.exit(main())
