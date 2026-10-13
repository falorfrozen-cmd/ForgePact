"""The tracker service: session files in, the Tracker's snapshot and runs out.

The plugin's `tracker` mod appends protocol-v1 records to one file per game
process, `<game>\\bin\\bp_ipc\\tracker\\session-<pid>-<unix ms>.ndjson`. This
service reads those files read-only and keeps one `Ledger` per session that is
not finished yet. What it derives is a pure function of the file plus the
controls the panel persisted itself, so a session read after the panel was
closed, or re-read after a restart, gives the same totals and journal:

- **Controls are persisted before they take effect.** A pause, a resume, an
  end-run by hand, a save snapshot and a settings change are appended to the
  session's entry in `cursor.json`, with the byte offset of the session file
  they were applied at, before the ledger sees them. A control that cannot be
  written does not take effect. Re-reading the file from byte 0 applies each
  control again at the same offset, so it lands between the same records.
  The settings a session started under are kept in its entry too.
- **One ingest reads a bounded number of bytes**, oldest unfinished session
  first; `catching_up` says bytes were left.
- **A partial last line waits for its newline.** A line that is not a
  protocol-v1 record is counted (`bad_lines`) and skipped.
- **A session ends** when its pid is no longer a running game and its file has
  been read to the end. Its run is then finished by `stats.rs`'s rules and
  filed once; `cursor.json` remembers it, so reading it again files nothing.
- **Retention** keeps the newest 20 finalized session files and never removes
  an unfinalized one or a live pid's.

Every public method takes the one tracker lock. The service starts no thread:
the panel's watcher calls `tick`, and request threads call the rest.
"""

from __future__ import annotations

import os
import threading
import time
from pathlib import Path

from .ledger import Ledger, prefs_from_settings, Prefs
from .protocol import MAX_LINE_BYTES, ProtocolError, parse_line, session_file_identity
from .store import SettingsError, TrackerStore, import_tracker_data

READ_BUDGET = 256 * 1024
SESSIONS_KEPT = 20
GAME_EXE = "hero_siege.exe"


def game_pid_is_live(pid: int) -> bool:
    """Whether `pid` is a running `Hero_Siege.exe` (Windows only; elsewhere
    nothing is)."""
    if os.name != "nt" or pid <= 0:
        return False
    import ctypes
    from ctypes import wintypes

    kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel32.OpenProcess.restype = wintypes.HANDLE
    kernel32.OpenProcess.argtypes = (wintypes.DWORD, wintypes.BOOL, wintypes.DWORD)
    process_query_limited_information = 0x1000
    handle = kernel32.OpenProcess(process_query_limited_information, False, pid)
    if not handle:
        return False
    try:
        code = wintypes.DWORD()
        if not kernel32.GetExitCodeProcess(handle, ctypes.byref(code)) or code.value != 259:  # STILL_ACTIVE
            return False
        size = wintypes.DWORD(1024)
        buf = ctypes.create_unicode_buffer(size.value)
        if not kernel32.QueryFullProcessImageNameW(handle, 0, buf, ctypes.byref(size)):
            return False
        return os.path.basename(buf.value).lower() == GAME_EXE
    finally:
        kernel32.CloseHandle(handle)


class _Session:
    """One session file and what has been read of it."""

    def __init__(self, path: Path, pid: int, start_ms: int, entry: dict, prefs: Prefs):
        self.path = path
        self.name = path.name
        self.pid = pid
        self.start_ms = start_ms
        self.entry = entry
        self.offset = 0
        self.skipping = False
        self.bad_lines = 0
        self.live = False
        self.applied_controls = 0
        self.ledger = Ledger(file=self.name, prefs=prefs)

    def apply_due_controls(self, position: int) -> None:
        """Apply every persisted control recorded at or before `position`."""
        controls = self.entry.get("controls", [])
        while self.applied_controls < len(controls) and controls[self.applied_controls].get("offset", 0) <= position:
            _apply_control(self.ledger, controls[self.applied_controls])
            self.applied_controls += 1

    def read(self, budget: int) -> tuple[int, bool]:
        """Read at most about `budget` bytes of whole lines. Returns (bytes
        read, whether the end of the file was reached)."""
        try:
            fh = open(self.path, "rb")
        except OSError:
            return 0, True
        with fh:
            size = os.fstat(fh.fileno()).st_size
            if size < self.offset:
                # never expected of an append-only file; take what is there as read
                self.offset = size
            fh.seek(self.offset)
            data = fh.read(max(0, min(budget, size - self.offset)))
            # a line longer than the budget is read on to its end, up to the cap
            while b"\n" not in data and self.offset + len(data) < size and len(data) <= MAX_LINE_BYTES:
                more = fh.read(min(64 * 1024, size - self.offset - len(data)))
                if not more:
                    break
                data += more
        used = len(data)
        reached_end = self.offset + used >= size
        pos = 0
        if self.skipping:
            newline = data.find(b"\n")
            if newline < 0:
                self.offset += used
                return used, reached_end
            pos = newline + 1
            self.skipping = False
        while True:
            newline = data.find(b"\n", pos)
            if newline < 0:
                break
            self.apply_due_controls(self.offset + pos)
            self._line(data[pos:newline])
            pos = newline + 1
        self.offset += pos
        tail = len(data) - pos
        if tail > MAX_LINE_BYTES:
            # a line past the cap with no end in sight: counted once, skipped
            self.bad_lines += 1
            self.offset += tail
            self.skipping = True
        self.apply_due_controls(self.offset)
        return used, reached_end

    def _line(self, raw: bytes) -> None:
        if raw.endswith(b"\r"):
            raw = raw[:-1]
        if not raw.strip():
            return
        try:
            record = parse_line(raw)
        except ProtocolError:
            self.bad_lines += 1
            return
        self.ledger.apply(record)

    def unread_tail(self) -> int:
        try:
            return max(0, self.path.stat().st_size - self.offset)
        except OSError:
            return 0


def _apply_control(ledger: Ledger, control: dict) -> None:
    kind = control.get("kind")
    at = control.get("at_ms", 0)
    if kind == "pause":
        ledger.set_paused(bool(control.get("on")), at)
    elif kind == "reset":
        ledger.reset(at)
    elif kind == "save":
        ledger.apply_save(control.get("progress") or {}, at)
    elif kind == "prefs":
        ledger.set_prefs(Prefs.from_dict(control.get("prefs") or {}))


class TrackerService:
    """The tracker's state for the panel.

    `store_root` is the tracker's folder (`%LOCALAPPDATA%\\Hero_Siege\\tracker`);
    `sessions` is the folder of session files, or a callable returning it (or
    None while the game folder is unknown). `is_live(pid)` says whether a pid is
    a running game; `clock_ms()` stamps the panel's own controls.
    `tracker_source` is HS Offline Tracker's folder: when given, the first start
    without an `import.json` imports from it.
    """

    def __init__(self, store_root, sessions, is_live=game_pid_is_live, clock_ms=None,
                 read_budget: int = READ_BUDGET, tracker_source=None):
        self.lock = threading.RLock()
        self.store = TrackerStore(store_root)
        self._sessions_dir = sessions
        self.is_live = is_live
        self.clock_ms = clock_ms or (lambda: int(time.time() * 1000))
        self.read_budget = max(1, int(read_budget))
        self.tracker_source = Path(tracker_source) if tracker_source is not None else None
        self.sessions: dict[str, _Session] = {}
        self.current: _Session | None = None
        self.catching_up = False
        self._finalized_view: dict | None = None
        cursor = self.store.read_json("cursor.json", {})
        self.cursor = cursor if isinstance(cursor, dict) and isinstance(cursor.get("sessions"), dict) else {
            "version": 1, "sessions": {}}
        with self.lock:
            if self.tracker_source is not None and not self.store.path("import.json").exists():
                try:
                    import_tracker_data(self.store, self.tracker_source)
                except OSError:
                    pass

    # -- reading -----------------------------------------------------------

    def _folder(self) -> Path | None:
        folder = self._sessions_dir() if callable(self._sessions_dir) else self._sessions_dir
        return Path(folder) if folder else None

    def _session_files(self) -> list[tuple[int, int, Path]]:
        folder = self._folder()
        if folder is None or not folder.is_dir():
            return []
        found = []
        try:
            entries = list(folder.iterdir())
        except OSError:
            return []
        for path in entries:
            identity = session_file_identity(path.name)
            if identity is not None and path.is_file():
                found.append((identity[1], identity[0], path))
        found.sort()
        return found

    def _entry(self, name: str) -> dict:
        return self.cursor["sessions"].setdefault(name, {"finalized": False, "filed": [], "controls": []})

    def _write_cursor(self) -> None:
        self.store.write_json("cursor.json", self.cursor)

    def _prefs(self) -> Prefs:
        return prefs_from_settings(self.store.settings())

    def _open(self, path: Path, pid: int, start_ms: int) -> _Session:
        entry = self._entry(path.name)
        if "prefs" not in entry:
            # the settings this session started under, so a re-read uses them
            entry["prefs"] = self._prefs().to_dict()
            try:
                self._write_cursor()
            except OSError:
                pass
        session = _Session(path, pid, start_ms, entry, Prefs.from_dict(entry["prefs"]))
        self.sessions[path.name] = session
        return session

    def tick(self) -> dict:
        """Ingest what the session files gained, finalize ended sessions and
        apply retention. Returns `{"catching_up": bool}`."""
        with self.lock:
            files = self._session_files()
            budget = self.read_budget
            catching_up = False
            for start_ms, pid, path in files:
                entry = self.cursor["sessions"].get(path.name)
                if entry is not None and entry.get("finalized"):
                    continue
                session = self.sessions.get(path.name) or self._open(path, pid, start_ms)
                # asked before the read: a process found dead has written its last
                session.live = bool(self.is_live(pid))
                if budget <= 0:
                    catching_up = True
                    continue
                used, reached_end = session.read(budget)
                budget -= used
                self._file_runs(session)
                if not reached_end:
                    catching_up = True
                elif not session.live:
                    self._finalize(session)
            self._choose_current(files)
            self._retain(files)
            self.catching_up = catching_up
            return {"catching_up": catching_up}

    def _choose_current(self, files) -> None:
        """The session the snapshot describes: the newest live one, else the
        newest of all."""
        chosen = None
        for _start, _pid, path in files:
            session = self.sessions.get(path.name)
            if session is not None and session.live:
                chosen = path.name
        if chosen is None and files:
            chosen = files[-1][2].name
        if chosen is None:
            self.current = None
            self._finalized_view = None
            return
        session = self.sessions.get(chosen)
        if session is not None:
            self.current = session
            self._finalized_view = None
        else:
            self.current = None
            pid = session_file_identity(chosen)[0]
            entry = self.cursor["sessions"].get(chosen, {})
            self._finalized_view = {"file": chosen, "pid": pid, "entry": entry}

    def _file_runs(self, session: _Session) -> None:
        for key, run in session.ledger.take_finished_runs():
            if key in session.entry.setdefault("filed", []):
                continue
            try:
                self.store.file_run(run)
                session.entry["filed"].append(key)
                self._write_cursor()
            except OSError:
                # runs.json refuses a run id it already holds, so the next
                # re-read of this session files it without a duplicate
                pass

    def _finalize(self, session: _Session) -> None:
        """The game process ended and its file is read: file its run once."""
        if session.unread_tail() > 0 and not session.skipping:
            # a partial last line the process never finished
            session.bad_lines += 1
        session.apply_due_controls(float("inf"))
        self._file_runs(session)
        ledger = session.ledger
        run = ledger.finish(ledger.now_ms if ledger.now_ms is not None else session.start_ms)
        if run is not None and run["id"] not in session.entry.setdefault("filed", []):
            try:
                self.store.file_run(run)
                session.entry["filed"].append(run["id"])
            except OSError:
                return
        session.entry["finalized"] = True
        session.entry["bad_lines"] = session.bad_lines
        session.entry["character"] = ledger.character
        session.entry["status"] = ledger.status
        try:
            self._write_cursor()
        except OSError:
            session.entry["finalized"] = False
            return
        del self.sessions[session.name]

    def _retain(self, files) -> None:
        finalized = [(start, pid, path) for start, pid, path in files
                     if self.cursor["sessions"].get(path.name, {}).get("finalized")]
        doomed = finalized[:-SESSIONS_KEPT] if len(finalized) > SESSIONS_KEPT else []
        changed = False
        for _start, pid, path in doomed:
            if self.is_live(pid):
                continue
            try:
                path.unlink()
            except OSError:
                continue
            self.cursor["sessions"].pop(path.name, None)
            changed = True
        if changed:
            try:
                self._write_cursor()
            except OSError:
                pass

    # -- what the panel reads ----------------------------------------------

    def _view(self) -> tuple[Ledger, dict]:
        if self.current is not None:
            s = self.current
            return s.ledger, {"file": s.name, "pid": s.pid, "live": s.live, "bad_lines": s.bad_lines}
        if self._finalized_view is not None:
            view = self._finalized_view
            entry = view["entry"]
            ledger = Ledger(file=view["file"], prefs=self._prefs())
            ledger.character = entry.get("character")
            return ledger, {"file": view["file"], "pid": view["pid"], "live": False,
                            "bad_lines": entry.get("bad_lines", 0)}
        return Ledger(prefs=self._prefs()), {"file": None, "pid": None, "live": False, "bad_lines": 0}

    def _status(self, ledger: Ledger, session: dict) -> str:
        if session["file"] is None or not session["live"]:
            return "waiting-for-game"
        if ledger.status.startswith("blocked|"):
            return "offline-error|" + ledger.status.split("|", 1)[1]
        if ledger.valid_records == 0:
            return "offline-waiting|"
        return "offline-live|%d" % ledger.valid_records

    def snapshot(self) -> dict:
        """The Tracker's `Snapshot`, plus `catching_up` and `session`."""
        with self.lock:
            ledger, session = self._view()
            snap = ledger.snapshot(self._status(ledger, session))
            snap["catching_up"] = self.catching_up
            snap["session"] = session
            return snap

    def extra(self) -> dict:
        """The Tracker's `Extra`: the character, the series and the journal."""
        with self.lock:
            return self._view()[0].extra()

    def runs(self) -> list:
        with self.lock:
            return self.store.runs()

    def take_zone_change(self) -> dict | None:
        with self.lock:
            return self.current.ledger.take_zone_change() if self.current is not None else None

    # -- controls ----------------------------------------------------------

    def _control(self, control: dict) -> dict:
        """Persist a control in the current session's entry, then apply it."""
        session = self.current
        if session is None:
            return {"ok": False, "error": "no tracker session is open"}
        control = dict(control, at_ms=int(self.clock_ms()), offset=session.offset)
        controls = session.entry.setdefault("controls", [])
        controls.append(control)
        try:
            self._write_cursor()
        except OSError as exc:
            controls.pop()
            return {"ok": False, "error": "cannot record the change: %s" % exc}
        session.apply_due_controls(session.offset)
        self._file_runs(session)
        return {"ok": True}

    def pause(self, on: bool) -> dict:
        with self.lock:
            return self._control({"kind": "pause", "on": bool(on)})

    def reset(self) -> dict:
        """End the current run by hand, filing it when it qualifies."""
        with self.lock:
            return self._control({"kind": "reset"})

    def accept_progress(self, progress: dict) -> dict:
        """A protocol-v1 `progress` dict from the character-save reader. The same
        snapshot twice in a row is one control."""
        with self.lock:
            if not isinstance(progress, dict):
                return {"ok": False, "error": "a progress record must be an object"}
            snapshot = {k: v for k, v in progress.items() if k not in ("protocol", "v")}
            session = self.current
            if session is not None:
                saves = [c for c in session.entry.get("controls", []) if c.get("kind") == "save"]
                if saves and saves[-1].get("progress") == snapshot:
                    return {"ok": True, "repeated": True}
            return self._control({"kind": "save", "progress": snapshot})

    def get_settings(self) -> dict:
        with self.lock:
            return self.store.settings()

    def set_settings(self, doc) -> dict:
        """Validate and store the whole settings document; the current session
        takes the new alert rules from the next record on."""
        with self.lock:
            try:
                saved = self.store.save_settings(doc)
            except SettingsError as exc:
                return {"ok": False, "error": str(exc)}
            except OSError as exc:
                return {"ok": False, "error": "cannot save the settings: %s" % exc}
            if self.current is not None:
                result = self._control({"kind": "prefs", "prefs": prefs_from_settings(self.store.settings()).to_dict()})
                if not result["ok"]:
                    return result
            return {"ok": True, "settings": self.store.settings()}

    def run_import(self) -> dict:
        """Import HS Offline Tracker's data again: only files still missing here
        are copied."""
        with self.lock:
            try:
                report = import_tracker_data(self.store, self.tracker_source)
            except OSError as exc:
                return {"ok": False, "error": str(exc), "copied": []}
            return dict(report, ok=True)
