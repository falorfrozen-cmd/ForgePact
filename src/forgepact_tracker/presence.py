"""Discord Rich Presence for the Tracker tab (#185): the session as a status.

The Discord client on the same machine accepts one activity over its local
IPC, the named pipe `\\\\.\\pipe\\discord-ipc-<0..9>`, and shows it under the
player's name. ForgePact runs and contacts no service of its own. Ported from
HS Offline Tracker's `presence.rs` (MIT, see CREDITS.md): the card, the 4 s
send gap, the 5 s answer timeout and the 30 s retry.

- **Only with an application ID.** Presence needs a Discord application owned
  by this project. The ID comes from `FORGEPACT_DISCORD_APP_ID` (16 to 24
  digits) or, when a release ships one, `APP_ID` below. With neither, the
  panel starts no thread, `/api/tracker` answers `presence:
  {"available": false}` and the tab hides the switch, as the Tracker's own
  releases did.
- **One daemon thread**, `forgepact-presence`, idle unless the tracker's
  settings say `discord: true` and the game runs, and clearing the activity
  when either stops. It reads its view (a snapshot taken under the tracker
  lock, by the caller's `view()`) and leaves the lock before any pipe I/O.
- **Every wait is bounded.** A pipe read polls `PeekNamedPipe` up to a
  timeout instead of blocking in `ReadFile`, so a Discord that accepts a
  write and never answers costs one written-off connection, not the thread.

Standard library only.
"""

from __future__ import annotations

import json
import os
import struct
import threading
import time
import uuid

from .items import room_name

APP_ID_ENV = "FORGEPACT_DISCORD_APP_ID"
# The owner's Discord application, when one is registered. Empty: presence is
# hidden unless the variable above holds an ID.
APP_ID = ""

THREAD_NAME = "forgepact-presence"

OP_HANDSHAKE = 0
OP_FRAME = 1
OP_CLOSE = 2

# Discord's own limit is five activity updates in twenty seconds.
SEND_GAP = 4.0
# How long an answer is waited for before the connection is written off.
ANSWER_GAP = 5.0
# Reconnect waits, doubling from the first to the cap while Discord stays away.
RETRY_FIRST = 5.0
RETRY_CAP = 30.0
POLL = 1.0
# How long the clear on stop() may wait, so stop() ends within a second.
STOP_ANSWER_GAP = 0.5

LINE = 120
NAMED = ("Unholy", "Angelic")
DIFFICULTIES = ("Normal", "Nightmare", "Hell", "Inferno")
APP_NAME = "ForgePact"
# The asset keys the Discord application carries.
LARGE_IMAGE = "logo"
SATANIC_IMAGE = "satanic"


# -- the application ID ------------------------------------------------------

def valid_app_id(value) -> bool:
    return isinstance(value, str) and 16 <= len(value) <= 24 and value.isascii() and value.isdigit()


def app_id(environ=None) -> str | None:
    """The application ID: the variable when set, else the shipped constant;
    None when neither is a valid ID."""
    environ = os.environ if environ is None else environ
    raw = environ.get(APP_ID_ENV)
    value = (raw if raw is not None else APP_ID).strip()
    return value if valid_app_id(value) else None


def available(environ=None) -> bool:
    return app_id(environ) is not None


# -- frames ------------------------------------------------------------------

def encode_frame(op: int, payload: dict) -> bytes:
    """A little-endian int32 opcode, an int32 length, then the JSON."""
    body = json.dumps(payload, separators=(",", ":")).encode("utf-8")
    return struct.pack("<ii", op, len(body)) + body


def decode_header(raw: bytes) -> tuple[int, int]:
    op, length = struct.unpack("<ii", raw[:8])
    return op, length


class PresenceError(Exception):
    """Discord answered, but not as asked: the connection is written off."""


# -- the card ----------------------------------------------------------------

def compact(n: int) -> str:
    mag = abs(n)
    if mag < 1_000:
        return str(n)
    if mag < 1_000_000:
        value, unit = n / 1e3, "k"
    elif mag < 1_000_000_000:
        value, unit = n / 1e6, "M"
    else:
        value, unit = n / 1e9, "B"
    return ("%.1f%s" if abs(value) < 10 else "%.0f%s") % (value, unit)


def clip(text: str, limit: int = LINE) -> str:
    return text if len(text) <= limit else text[: limit - 1] + "\u2026"


def zone_label(room: str) -> str:
    """What the game calls a room, else a name made from the asset's."""
    name = room_name(room)
    if name:
        return name
    if room[:4].lower() == "town":
        return "Town"
    return room[:-3].replace("_", " ") if room.endswith("_rm") else room.replace("_", " ")


def _int(value) -> int:
    return value if isinstance(value, int) and not isinstance(value, bool) else 0


def build_card(snap: dict, start_ms: int) -> dict:
    """The activity for one snapshot: `details` where the character is (act,
    zone, difficulty), `state` the haul, the session's start and the badge
    of the Satanic zone the character is standing in."""
    where = []
    act = _int(snap.get("act"))
    if act > 0:
        where.append("Act %d" % act)
    room = snap.get("room")
    if isinstance(room, str) and room:
        where.append(zone_label(room))
    character = snap.get("character") if isinstance(snap.get("character"), dict) else None
    if character is not None:
        difficulty = _int(character.get("difficulty"))
        if 0 <= difficulty < len(DIFFICULTIES):
            where.append(DIFFICULTIES[difficulty] + (" HC" if character.get("hardcore") else ""))
    if act <= 0 and not (isinstance(room, str) and room):
        where = ["Somewhere in Hero Siege"]
    details = " \u00b7 ".join(where)

    items = snap.get("items") if isinstance(snap.get("items"), dict) else {}
    named = {r: _int((items.get(r) or {}).get("total")) for r in NAMED}
    haul = []
    chase = max(0, _int(snap.get("ss")) - sum(named.values()))
    if chase:
        haul.append("%d SS" % chase)
    for rarity in NAMED:
        if named[rarity]:
            haul.append("%d %s" % (named[rarity], rarity))
    gold = _int((snap.get("gold") or {}).get("earned"))
    if gold > 0:
        haul.append("%s gold" % compact(gold))
    state = " \u00b7 ".join(haul) if haul else "just started"

    hover = APP_NAME
    if character is not None:
        hover = "%s \u00b7 level %d \u00b7 hero level %d" % (
            APP_NAME, _int(character.get("level")), _int(character.get("herolevel")))
    assets = {"large_image": LARGE_IMAGE, "large_text": clip(hover)}
    zone = (snap.get("satanic_zone") or {}).get("zone") if isinstance(snap.get("satanic_zone"), dict) else None
    if snap.get("satanic_here") and isinstance(zone, str) and zone:
        assets["small_image"] = SATANIC_IMAGE
        assets["small_text"] = clip("Satanic Zone \u00b7 " + zone_label(zone))
    return {"details": clip(details), "state": clip(state), "timestamps": {"start": int(start_ms)},
            "assets": assets}


# -- the transport -----------------------------------------------------------

class PipeTransport:
    """Discord's named pipes. `connect()` opens the first that exists and
    raises OSError when none does."""

    def __init__(self, names=None):
        self.names = list(names) if names is not None else [
            r"\\.\pipe\discord-ipc-%d" % i for i in range(10)]

    def connect(self) -> "PipeConnection":
        if os.name != "nt":
            raise OSError("Discord's IPC is a Windows named pipe here")
        last = None
        for name in self.names:
            try:
                return PipeConnection(open(name, "r+b", buffering=0))
            except OSError as e:
                last = e
        raise last or FileNotFoundError("no discord-ipc pipe")


_K32 = None


def _kernel32():
    """A private kernel32 handle with PeekNamedPipe's signature."""
    global _K32
    if _K32 is None:
        import ctypes
        from ctypes import wintypes
        k32 = ctypes.WinDLL("kernel32", use_last_error=True)
        k32.PeekNamedPipe.argtypes = [wintypes.HANDLE, wintypes.LPVOID, wintypes.DWORD,
                                      ctypes.POINTER(wintypes.DWORD), ctypes.POINTER(wintypes.DWORD),
                                      ctypes.POINTER(wintypes.DWORD)]
        k32.PeekNamedPipe.restype = wintypes.BOOL
        _K32 = (k32, ctypes, wintypes)
    return _K32


class PipeConnection:
    def __init__(self, handle):
        self.f = handle

    def _waiting(self) -> int:
        import msvcrt
        k32, ctypes, wintypes = _kernel32()
        avail = wintypes.DWORD(0)
        if not k32.PeekNamedPipe(msvcrt.get_osfhandle(self.f.fileno()), None, 0, None,
                                 ctypes.byref(avail), None):
            raise OSError(ctypes.get_last_error(), "the Discord pipe closed")
        return avail.value

    def _read_exact(self, n: int, deadline: float) -> bytes:
        out = b""
        while len(out) < n:
            waiting = self._waiting()
            if waiting:
                chunk = self.f.read(min(n - len(out), waiting))
                if not chunk:
                    raise OSError("the Discord pipe closed")
                out += chunk
                continue
            if time.monotonic() >= deadline:
                raise TimeoutError("Discord did not answer")
            time.sleep(0.01)
        return out

    def write(self, op: int, payload: dict) -> None:
        self.f.write(encode_frame(op, payload))

    def read(self, timeout: float) -> tuple[int, dict]:
        deadline = time.monotonic() + timeout
        op, length = decode_header(self._read_exact(8, deadline))
        if length < 0 or length > 1 << 20:
            raise OSError("a frame of %d bytes" % length)
        body = self._read_exact(length, deadline) if length else b"{}"
        return op, json.loads(body.decode("utf-8"))

    def close(self) -> None:
        try:
            self.f.close()
        except OSError:
            pass


# -- the worker --------------------------------------------------------------

class Presence:
    """The worker. `view()` answers None while presence is off (the switch
    off, or no tracker), else `{"running": bool, "snapshot": dict}`; it takes
    the tracker lock itself and returns before any pipe I/O here. `connect()`
    returns a connection with `write(op, payload)`, `read(timeout)` and
    `close()`, or raises OSError when Discord is not there."""

    def __init__(self, client_id: str, view, connect=None, clock=time.monotonic,
                 wall_ms=lambda: int(time.time() * 1000), pid=None, poll: float = POLL):
        self.client_id = client_id
        self.view = view
        self.connect = connect or PipeTransport().connect
        self.clock = clock
        self.wall_ms = wall_ms
        self.pid = os.getpid() if pid is None else pid
        self.poll = poll
        self.conn = None
        self.shown: dict | None = None
        self.sent_at: float | None = None
        self.next_try = None
        self.backoff = RETRY_FIRST
        self._start: tuple | None = None
        self._stop = threading.Event()
        self.thread: threading.Thread | None = None

    # the thread

    def start(self) -> threading.Thread:
        if self.thread is None or not self.thread.is_alive():
            self._stop.clear()
            self.thread = threading.Thread(target=self._run, name=THREAD_NAME, daemon=True)
            self.thread.start()
        return self.thread

    def stop(self, timeout: float = 0.9) -> None:
        """End the thread (clearing the activity on its way out) within a second."""
        self._stop.set()
        if self.thread is not None:
            self.thread.join(timeout)

    def _run(self) -> None:
        while not self._stop.is_set():
            try:
                self.step()
            except Exception:
                self._drop(clear=False)
            self._stop.wait(self.poll)
        self._drop(clear=True, answer_gap=STOP_ANSWER_GAP)

    def state(self) -> dict:
        return {"available": True, "connected": self.conn is not None, "showing": self.shown is not None}

    # one pass

    def step(self) -> None:
        view = self.view()
        if not view or not view.get("running"):
            # the game closed or the switch went off: take the status down
            # rather than leave a finished run standing on the profile
            self._drop(clear=True)
            self.next_try = None
            self.backoff = RETRY_FIRST
            return
        now = self.clock()
        if self.conn is None:
            if self.next_try is not None and now < self.next_try:
                return
            try:
                self.conn = self._handshake()
            except (OSError, ValueError, PresenceError):
                self._retry_later(now)
                return
            self.backoff = RETRY_FIRST
            self.next_try = None
            self.shown = None
        if self.sent_at is not None and now - self.sent_at < SEND_GAP:
            return
        card = build_card(view.get("snapshot") or {}, self._start_ms(view.get("snapshot") or {}))
        if card == self.shown:
            return
        try:
            self._set_activity(card, ANSWER_GAP)
        except (OSError, ValueError, PresenceError):
            self._drop(clear=False)
            self._retry_later(now)
            return
        self.sent_at = now
        self.shown = card

    def _retry_later(self, now: float) -> None:
        self.next_try = now + self.backoff
        self.backoff = min(RETRY_CAP, self.backoff * 2)

    def _start_ms(self, snap: dict) -> int:
        """The session's start, fixed once per session file so the card does
        not move with every second of `session_secs`."""
        session = snap.get("session") if isinstance(snap.get("session"), dict) else {}
        key = session.get("file")
        if self._start is None or self._start[0] != key:
            self._start = (key, self.wall_ms() - _int(snap.get("session_secs")) * 1000)
        return self._start[1]

    def _handshake(self):
        conn = self.connect()
        try:
            conn.write(OP_HANDSHAKE, {"v": 1, "client_id": self.client_id})
            op, payload = conn.read(ANSWER_GAP)
            if op != OP_FRAME or payload.get("evt") != "READY":
                raise PresenceError("no READY: op %s %s" % (op, payload.get("evt")))
        except BaseException:
            conn.close()
            raise
        return conn

    def _set_activity(self, activity, answer_gap: float) -> None:
        nonce = uuid.uuid4().hex
        self.conn.write(OP_FRAME, {"cmd": "SET_ACTIVITY", "args": {"pid": self.pid, "activity": activity},
                                   "nonce": nonce})
        op, payload = self.conn.read(answer_gap)
        if op == OP_CLOSE or payload.get("evt") == "ERROR":
            raise PresenceError(str(payload.get("data")))

    def _drop(self, clear: bool, answer_gap: float = ANSWER_GAP) -> None:
        conn, self.conn = self.conn, None
        if conn is not None:
            if clear and self.shown is not None:
                self.conn = conn
                try:
                    self._set_activity(None, answer_gap)
                except (OSError, ValueError, PresenceError):
                    pass
                self.conn = None
            conn.close()
        self.shown = None
