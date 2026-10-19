"""Read-only progress snapshots from Hero Siege's local character saves (#185).

A Python port of HS Offline Tracker v0.1.4's `save_source.rs` (MIT, see
`CREDITS.md`). The panel's tracker ledger polls it about once a second for
what only the character save knows: the character's identity and levels,
its experience, difficulty and coarse act, its lifetime kill total and the
game's `statistic...` counters (bosses killed, chests opened and the rest).

What it does not do is as deliberate. A save says nothing about the current
room, Magic Find, mail or current resources, nor whether an inventory change
was a drop, so none of those is inferred here. It never writes, locks or
renames a save: a file is opened for reading only, without a share lock,
and closed before the next one is looked at.

Each accepted save becomes one protocol v1 `progress` dict
(HS-Offline-Tracker's PROTOCOL.md, "Character progress"). Local saves carry
no season or Blood Pact field, so those keys are left out instead of being
filled with a guess. One key is added to the protocol's: `armed_baseline`,
true only on the record that replays the slot's value from when the game
started (see `SaveWatcher.poll`).

Standard library only (`zlib`, `base64`).
"""
from __future__ import annotations

import base64
import binascii
import math
import os
import re
import time
import zlib
from pathlib import Path

PROTOCOL = "hs-offline-tracker/1"
MAX_SAVE_BYTES = 4 * 1024 * 1024

# The save's innermost layer is a repeating 32-byte XOR over a UTF-16LE-like
# stream: once it is undone, every odd byte is zero and the even bytes are
# the UTF-8 text of an INI-style file.
HSS_XOR_KEY = bytes(
    [
        0xE3, 0x95, 0x3D, 0xB1, 0x01, 0x6B, 0xB6, 0x58, 0x54, 0x38, 0x3F, 0x46, 0xA1, 0x74, 0x29, 0xCC,
        0x45, 0x45, 0x51, 0xF2, 0xA7, 0xF7, 0xAB, 0xB7, 0x26, 0xF1, 0x37, 0xA8, 0x81, 0x91, 0xE6, 0x7E,
    ]
)

# `herosiege<N>.hss`, any case. `ether<N>`, `incarnation<N>` and the
# inventory-order files share the folder and are not character saves.
_CHARACTER_SAVE = re.compile(r"herosiege[0-9]+\.hss", re.IGNORECASE | re.ASCII)
# Rust's `is_ascii_whitespace`: space, tab, LF, form feed, CR.
_ASCII_WHITESPACE = frozenset(b" \t\n\x0c\r")
_I64_MIN = -(2**63)
_I64_MAX = 2**63
# How far past "now" a modification time may sit and still count as a save
# written since the game started, against a clock that drifted.
_FUTURE_SLACK_NS = 60 * 1_000_000_000


class SaveError(Exception):
    """A save that was read but could not be accepted."""


class EmptySlot(SaveError):
    """A character slot that holds no character: a file of NUL bytes only.

    Deleting a character at character select rewrites its slot as one NUL
    byte, and the game treats that slot as free afterwards
    (`docs/RUNTIME_DATA_MODELS.md` section 8.2). It is not a damaged save.
    """


def default_save_root() -> Path:
    """Where the game keeps its saves: `%LOCALAPPDATA%\\Hero_Siege`.

    Callers pass a root to `SaveWatcher` explicitly; tests never use this.
    """
    base = os.environ.get("LOCALAPPDATA")
    return (Path(base) if base else Path.cwd()) / "Hero_Siege"


class _Snapshot:
    __slots__ = ("path", "stamp", "data")

    def __init__(self, path: Path, stamp: tuple[int, int], data: bytes):
        self.path = path
        self.stamp = stamp
        self.data = data

    def same_as(self, other: "_Snapshot") -> bool:
        return self.stamp == other.stamp and self.data == other.data


def _stamp(st: os.stat_result) -> tuple[int, int]:
    return (st.st_size, st.st_mtime_ns)


def _read_once(path: Path) -> _Snapshot:
    """One bounded, read-only read of a save, refused if it moved meanwhile."""
    try:
        before = os.stat(path)
    except OSError as error:
        raise SaveError(str(error)) from None
    if not os.path.isfile(path):
        raise SaveError("not a regular file")
    if before.st_size == 0:
        raise SaveError("empty save")
    if before.st_size > MAX_SAVE_BYTES:
        raise SaveError("raw save exceeds the 4 MiB limit")
    try:
        # One byte past the limit, so a file that grew after the stat still
        # cannot slip past it.
        with open(path, "rb") as handle:
            data = handle.read(MAX_SAVE_BYTES + 1)
        after = os.stat(path)
    except OSError as error:
        raise SaveError(str(error)) from None
    if len(data) > MAX_SAVE_BYTES:
        raise SaveError("raw save exceeds the 4 MiB limit")
    if _stamp(before) != _stamp(after) or len(data) != before.st_size:
        raise SaveError("save changed during the read")
    return _Snapshot(Path(path), _stamp(after), data)


def _xor(data: bytes) -> bytes:
    if not data:
        return b""
    reps = len(data) // len(HSS_XOR_KEY) + 1
    key = (HSS_XOR_KEY * reps)[: len(data)]
    value = int.from_bytes(data, "little") ^ int.from_bytes(key, "little")
    return value.to_bytes(len(data), "little")


def decode_hss(raw: bytes) -> str:
    """Undo the save's three layers: Base64, zlib, then the XOR'd UTF-16.

    Fails closed: a truncated or corrupted layer raises `SaveError`, a file
    of NUL bytes only raises `EmptySlot`, and both the raw file and the
    inflated stream are held to `MAX_SAVE_BYTES`.
    """
    if not raw:
        raise SaveError("empty save")
    if len(raw) > MAX_SAVE_BYTES:
        raise SaveError("raw save exceeds the 4 MiB limit")
    if raw.count(0) == len(raw):
        raise EmptySlot("empty character slot (NUL bytes only)")

    cleaned = bytearray()
    saw_padding = False
    for byte in raw:
        if byte == 0:
            saw_padding = True
            continue
        if byte in _ASCII_WHITESPACE:
            continue
        if saw_padding:
            raise SaveError("non-padding data follows a NUL terminator")
        cleaned.append(byte)
    if not cleaned:
        raise SaveError("empty encoded payload")

    try:
        compressed = base64.b64decode(bytes(cleaned), validate=True)
    except (binascii.Error, ValueError) as error:
        raise SaveError(f"invalid Base64: {error}") from None
    inflater = zlib.decompressobj()
    try:
        obfuscated = inflater.decompress(compressed, MAX_SAVE_BYTES + 1)
    except zlib.error as error:
        raise SaveError(f"invalid zlib stream: {error}") from None
    if len(obfuscated) > MAX_SAVE_BYTES:
        raise SaveError("decompressed save exceeds the 4 MiB limit")
    if not inflater.eof:
        raise SaveError("invalid zlib stream: the stream ends early")
    if not obfuscated or len(obfuscated) % 2:
        raise SaveError("decoded save is not an even UTF-16-like byte stream")

    decoded = _xor(obfuscated)
    if decoded[1::2].count(0) != len(decoded) // 2:
        raise SaveError("decoded save has non-zero high bytes")
    try:
        text = decoded[0::2].decode("utf-8")
    except UnicodeDecodeError as error:
        raise SaveError(f"save text is not UTF-8: {error}") from None
    return text.replace("\r\n", "\n").replace("\r", "\n")


def _strip_quotes(value: str) -> str:
    value = value.strip()
    if len(value) >= 2 and value.startswith('"') and value.endswith('"'):
        return value[1:-1]
    return value


def _parsed_sections(text: str) -> tuple[dict[str, str], dict[str, str]]:
    """The `[0]` (character) and `[stat]` sections; everything else is ignored."""
    section = ""
    character: dict[str, str] = {}
    statistics: dict[str, str] = {}
    for line in text.split("\n"):
        line = line.strip()
        if len(line) >= 2 and line.startswith("[") and line.endswith("]"):
            section = line[1:-1]
            continue
        if "=" not in line:
            continue
        key, value = line.split("=", 1)
        if section == "0":
            target = character
        elif section == "stat":
            target = statistics
        else:
            continue
        target[key.strip()] = _strip_quotes(value)
    return character, statistics


def _number(value: str | None, key: str) -> int:
    """A save number (`"13.000000"`, either decimal mark) as an integer."""
    if value is None:
        raise SaveError(f"missing {key}")
    text = value.strip().replace(",", ".")
    if "_" in text:
        raise SaveError(f"invalid {key}")
    try:
        number = float(text)
    except ValueError:
        raise SaveError(f"invalid {key}") from None
    if not math.isfinite(number) or not (_I64_MIN <= number < _I64_MAX):
        raise SaveError(f"out-of-range {key}")
    return int(number)


def _optional_number(fields: dict[str, str], key: str) -> int:
    return _number(fields[key], key) if key in fields else 0


def _flat_key(key: str) -> str:
    return "".join(c for c in key if c.isascii() and c.isalnum()).lower()


def progress_event(text: str) -> dict:
    """One decoded save as a protocol v1 `progress` dict.

    Raises `SaveError` for the game's placeholder character ("New Char",
    "Unnamed") and for any missing or malformed identity field: an
    unreadable save is never reported as a character at level zero.
    """
    character, statistics = _parsed_sections(text)
    name = character.get("name", "").strip()
    if not name or name.lower() in ("new char", "unnamed"):
        raise SaveError("placeholder or missing character name")
    char_class = _number(character.get("class"), "class")
    if not 1 <= char_class <= 24:
        raise SaveError("unsupported character class")
    level = _number(character.get("level"), "level")
    if level <= 0:
        raise SaveError("invalid character level")
    experience = _number(character.get("experience"), "experience")

    # Every `statistic...` counter goes over, flattened to lower-case letters
    # and digits as the Tracker's parser keys them; which ones are shown is
    # the ledger's business.
    tallies: dict[str, int] = {}
    for key, value in statistics.items():
        flat = _flat_key(key)
        if len(flat) > len("statistic") and flat.startswith("statistic"):
            tallies[flat] = _number(value, key)
    kills = (
        _number(statistics["statisticTotalMonsterKills"], "statisticTotalMonsterKills")
        if "statisticTotalMonsterKills" in statistics
        else 0
    )

    return {
        "protocol": PROTOCOL,
        "v": 1,
        "kind": "progress",
        "name": name,
        "experience": experience,
        "hardcore": _optional_number(character, "hardcore"),
        "level": level,
        "hero_level": _optional_number(character, "herolevel"),
        "difficulty": _optional_number(character, "difficulty"),
        "hell_sub": _optional_number(character, "hell_subdifficulty"),
        # The second element of the flattened `act_previous` is the coarse
        # act. The `act_*` and `zone*,*` keys mark progression, not location.
        "act": _optional_number(character, "act_previous_1"),
        "kills": kills,
        "tallies": tallies,
        "armed_baseline": False,
    }


def _same_character(left: dict, right: dict) -> bool:
    a, b = left.get("name", ""), right.get("name", "")
    return bool(a) and a.encode("utf-8").lower() == b.encode("utf-8").lower()


class SaveWatcher:
    """Non-blocking double-snapshot reader over one save root.

    `poll` is meant to run about once a second. A slot seen changed is read
    once and held as pending; only an identical read on a later poll is
    decoded. Nothing sleeps, and a save caught between the game's truncate
    and its write is never mistaken for a reset of the character.

    Character saves are `<root>\\hs2saves\\herosiege<N>.hss`. The legacy
    location, `<root>\\herosiege<N>.hss`, is read only while `hs2saves`
    holds no character slot.
    """

    def __init__(self, root):
        self.root = Path(root)
        self.running = False
        self._observed: dict[Path, tuple[int, int]] = {}
        # Stable bytes of every slot when the game appeared. Which slot is
        # live is unknown until the game writes one; that slot's baseline is
        # then replayed just before its new value, so the progress earned
        # before the game's first, delayed save is not lost.
        self._armed_baselines: dict[Path, _Snapshot] = {}
        self._pending: _Snapshot | None = None
        self._last_accepted: tuple[Path, bytes] | None = None

    # -- lifecycle ----------------------------------------------------------

    def game_started(self) -> None:
        """Arm without guessing which slot is live; only a later write tells."""
        self._arm(None)

    def game_started_since(self, started_at: float) -> None:
        """Arm when joining a game process that was already running.

        `started_at` is the process start, in seconds since the epoch. A save
        written after it is the game's own evidence of the live slot, not a
        newest-slot guess, and it still has to pass the second identical read.
        """
        self._arm(int(started_at * 1_000_000_000))

    def game_stopped(self) -> None:
        self.running = False
        self._observed.clear()
        self._armed_baselines.clear()
        self._pending = None
        self._last_accepted = None

    def _arm(self, started_ns: int | None) -> None:
        self.running = True
        self._observed = self._current_stamps()
        self._armed_baselines = {}
        for path, stamp in self._observed.items():
            try:
                first = _read_once(path)
                second = _read_once(path)
            except SaveError:
                continue
            if first.stamp == stamp and first.same_as(second):
                self._armed_baselines[path] = second
        self._pending = None
        self._last_accepted = None

        if started_ns is None:
            return
        latest = time.time_ns() + _FUTURE_SLACK_NS
        eligible = sorted(
            (stamp[1], path)
            for path, stamp in self._observed.items()
            if started_ns <= stamp[1] <= latest
        )
        if eligible:
            try:
                self._pending = _read_once(eligible[-1][1])
            except SaveError:
                self._pending = None

    # -- polling ------------------------------------------------------------

    def poll(self) -> list[dict]:
        """Progress records accepted on this poll, oldest first.

        The first accepted write can return two: the slot's stable value from
        when the game started (`armed_baseline` true), then the value just
        written. That write is the one safe moment to pick one of many slot
        baselines, and doing so keeps the game's first delayed save from
        swallowing the start of the session. An empty slot returns nothing.

        Raises `SaveError` when a stable save cannot be decoded or a changed
        slot cannot be read.
        """
        if not self.running:
            return []

        if self._pending is not None:
            first, self._pending = self._pending, None
            try:
                second = _read_once(first.path)
            except SaveError:
                # Keep the stamp observed before: the changed file is retried
                # once a later poll sees it change again.
                return []
            if not first.same_as(second):
                # The game finished another write between the two reads. That
                # newer read becomes the first half of a new pair.
                self._pending = second
                return []
            self._observed[second.path] = second.stamp
            if self._last_accepted == (second.path, second.data):
                return []
            try:
                event = progress_event(decode_hss(second.data))
            except EmptySlot:
                # A deleted character. It identifies no live slot, so the
                # armed baselines stay for the first real write.
                return []
            except SaveError as error:
                raise SaveError(f"{second.path} rejected: {error}") from None

            events: list[dict] = []
            if self._last_accepted is None:
                baseline = self._armed_baselines.pop(second.path, None)
                if baseline is not None and baseline.data != second.data:
                    try:
                        baseline_event = progress_event(decode_hss(baseline.data))
                    except SaveError:
                        baseline_event = None
                    if baseline_event is not None and _same_character(baseline_event, event):
                        baseline_event["armed_baseline"] = True
                        events.append(baseline_event)
                # The changed slot has named the live character. Every other
                # slot's baseline is now a stale guess, never to be replayed.
                self._armed_baselines.clear()
            events.append(event)
            self._last_accepted = (second.path, second.data)
            return events

        now = self._current_stamps()
        self._observed = {p: s for p, s in self._observed.items() if p in now}
        changed = [(stamp, path) for path, stamp in now.items() if self._observed.get(path) != stamp]
        if not changed:
            return []

        # When several slots changed in one interval, the newest modification
        # time is the only defensible sign of the live one. Two slots tied on
        # it are ambiguous: choosing by slot number would be deterministic,
        # not true.
        changed.sort(key=lambda item: (item[0][1], item[1]))
        newest = changed[-1][0][1]
        if sum(1 for stamp, _ in changed if stamp[1] == newest) != 1:
            for stamp, path in changed:
                self._observed[path] = stamp
            return []
        _, selected = changed.pop()
        # The older changes were seen, but only the newest may name the live
        # character, so none of them is queued for a later poll.
        for stamp, path in changed:
            self._observed[path] = stamp

        try:
            self._pending = _read_once(selected)
        except SaveError as error:
            raise SaveError(f"{selected} could not be read: {error}") from None
        return []

    # -- the slots ----------------------------------------------------------

    def candidate_files(self) -> list[Path]:
        nested = _files_in(self.root / "hs2saves")
        return nested if nested else _files_in(self.root)

    def _current_stamps(self) -> dict[Path, tuple[int, int]]:
        stamps = {}
        for path in self.candidate_files():
            try:
                st = os.stat(path)
            except OSError:
                continue
            if os.path.isfile(path):
                stamps[path] = _stamp(st)
        return stamps


def _files_in(directory: Path) -> list[Path]:
    try:
        entries = list(os.scandir(directory))
    except OSError:
        return []
    return sorted(Path(entry.path) for entry in entries if _CHARACTER_SAVE.fullmatch(entry.name))
