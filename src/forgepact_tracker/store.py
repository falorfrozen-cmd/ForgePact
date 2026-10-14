"""The tracker's own files, under `%LOCALAPPDATA%\\Hero_Siege\\tracker\\`.

| File | What it holds |
|---|---|
| `runs.json` | HS Offline Tracker's run list, newest first, at most 200 |
| `settings.json` | the Tracker's settings document |
| `cursor.json` | per session file: finalized, runs filed, the panel's controls |
| `import.json` | what the one-time import of the Tracker's data did |
| `shopping.json` | the Tracker tab's shopping list: a JSON list of strings, the Tracker's own format |
| `sounds\\<key>.<ext>` | the player's own alert sounds, one file per key |

The panel writes only whole files: a temporary file beside the target, then
`os.replace`, so a reader never sees half a document and a failed write
leaves the old one whole. The service holds one lock around every use.
Nothing here ever writes in HS Offline Tracker's own folder.

`import_tracker_data` is the one-time, read-only import of HS Offline
Tracker's data, after the Tracker's own `migrate_legacy_data`: it copies only
files missing here and never writes, renames or deletes anything in the
Tracker's folder.
"""

from __future__ import annotations

import json
import os
import re
import time
from pathlib import Path

RUNS_KEPT = 200

# The shopping list's bounds: the Tracker kept at most 200 entries, and an
# entry is one item name, not a note.
SHOPPING_MAX_ITEMS = 200
SHOPPING_MAX_CHARS = 120

# Custom alert sounds. A key is one of the alerts that has a sound of its own
# (the Tracker's `mail` is online-only and not ported) or a sound list's own
# file; anything else never reaches the filesystem. When a key has files of
# several kinds (an import from a Tracker that kept both), the first kind
# here is the one that plays, as the Tracker's readers took it.
SOUND_KEYS = ("satanic", "set", "heroic", "angelic", "unholy", "zone")
_LIST_KEY = re.compile(r"^list-[A-Za-z0-9_-]{1,64}$")
SOUND_TYPES = {"mp3": "audio/mpeg", "wav": "audio/wav", "ogg": "audio/ogg", "flac": "audio/flac"}
SOUND_MAX_BYTES = 8 * 1024 * 1024


def sound_key_ok(key) -> bool:
    """Whether `key` names a sound this store files."""
    return isinstance(key, str) and (key in SOUND_KEYS or bool(_LIST_KEY.match(key)))


def sound_bytes_match(ext: str, data: bytes) -> bool:
    """Whether `data` starts the way a file of kind `ext` does."""
    if ext == "wav":
        return len(data) >= 12 and data[:4] == b"RIFF" and data[8:12] == b"WAVE"
    if ext == "mp3":
        # a tagged file, or a bare MPEG audio frame (11 sync bits set)
        return data[:3] == b"ID3" or (len(data) >= 2 and data[0] == 0xFF and data[1] & 0xE0 == 0xE0)
    if ext == "ogg":
        return data[:4] == b"OggS"
    if ext == "flac":
        return data[:4] == b"fLaC"
    return False

# What the import copies, and what it leaves behind on purpose: window
# positions belong to the Tracker's windows, carried totals to its online
# reconciliation.
IMPORTED_FILES = ("settings.json", "runs.json", "shopping.json")
IMPORTED_FOLDER = "sounds"


def default_store_root() -> Path:
    """`%LOCALAPPDATA%\\Hero_Siege\\tracker`."""
    base = os.environ.get("LOCALAPPDATA") or str(Path.home() / "AppData" / "Local")
    return Path(base) / "Hero_Siege" / "tracker"


def default_tracker_source() -> Path:
    """HS Offline Tracker's data folder, `%LOCALAPPDATA%\\HS Offline Tracker`."""
    base = os.environ.get("LOCALAPPDATA") or str(Path.home() / "AppData" / "Local")
    return Path(base) / "HS Offline Tracker"


class SettingsError(ValueError):
    """A settings document with a field of the wrong type. The message names
    the field."""


class Refused(ValueError):
    """A request the store will not carry out (a bad sound, a shopping entry
    out of bounds). The message says why, in words the panel can show."""


def default_settings() -> dict:
    """The Tracker's `Settings::default()`, as a document."""
    def sound():
        return {"enabled": True, "volume": 0.5}
    return {
        "satanic": sound(), "set": sound(), "heroic": sound(), "angelic": sound(), "unholy": sound(),
        "mail": sound(), "zone": sound(),
        "zone_buffs": [],
        "alerts": ["Satanic", "Set", "Heroic", "Angelic", "Unholy"],
        "min_tier": 0,
        "notable": [
            {"label": "Angelic Key", "names": ["angelic key"]},
            {"label": "Satanic Dice", "names": ["satanic dice"]},
            {"label": "S runes", "names": ["qi", "xo", "sur", "ber", "jah", "drax", "zed"]},
            {"label": "SS runes", "names": ["fawn", "flo", "nju", "jol", "sus", "kek", "jord"]},
        ],
        "filters": [],
        "filter": "",
        "use_filter": True,
        "locked": False,
        "opacity": 1.0,
        "scale": 1.0,
        "auto_show": True,
        "autostart": False,
        "ticker": False,
        "debug_log": False,
        "sound_on_ground": True,
        "theme": "obsidian",
        "flourish": True,
        "flourish_scale": 1.0,
        "flourish_shade": 0.55,
        "flourish_secs": 4.0,
        "flourish_rarities": ["Satanic", "Set", "Heroic", "Angelic", "Unholy"],
        "flourish_tier": 1,
        "flourish_listed": False,
        "flourish_zone": True,
        "flourish_always": False,
        "discord": False,
        "compact": False,
        "ghost": True,
        "x11_backend": False,
        "hidden": [],
    }


def _is_int(value) -> bool:
    return isinstance(value, int) and not isinstance(value, bool)


def _is_number(value) -> bool:
    return isinstance(value, (int, float)) and not isinstance(value, bool)


def _str_list(value) -> bool:
    return isinstance(value, list) and all(isinstance(v, str) for v in value)


def _sound(value) -> bool:
    return (isinstance(value, dict) and isinstance(value.get("enabled", True), bool)
            and _is_number(value.get("volume", 0.5)))


def _notable(value) -> bool:
    return isinstance(value, list) and all(
        isinstance(g, dict) and isinstance(g.get("label"), str) and _str_list(g.get("names")) for g in value)


def _sound_list(value) -> bool:
    return (isinstance(value, dict) and isinstance(value.get("id", ""), str)
            and isinstance(value.get("name", ""), str) and isinstance(value.get("enabled", True), bool)
            and _is_number(value.get("volume", 0.5)) and _str_list(value.get("items", [])))


def _filters(value) -> bool:
    return isinstance(value, list) and all(
        isinstance(f, dict) and isinstance(f.get("id", ""), str) and isinstance(f.get("name", ""), str)
        and isinstance(f.get("lists", []), list) and all(_sound_list(item) for item in f.get("lists", []))
        for f in value)


# The fields the ledger reads, and the type check each must pass. Every other
# field (window placement, the overlay's look) is the tab's business and is
# kept as it was written, unknown fields included.
_CHECKS = {
    "alerts": _str_list,
    "min_tier": _is_int,
    "zone_buffs": lambda v: isinstance(v, list) and all(_is_int(b) and 0 <= b <= 255 for b in v),
    "notable": _notable,
    "filters": _filters,
    "filter": lambda v: isinstance(v, str),
    "use_filter": lambda v: isinstance(v, bool),
    "flourish": lambda v: isinstance(v, bool),
    "flourish_listed": lambda v: isinstance(v, bool),
    "flourish_rarities": _str_list,
    "flourish_tier": _is_int,
    "sound_on_ground": lambda v: isinstance(v, bool),
    "satanic": _sound, "set": _sound, "heroic": _sound, "angelic": _sound, "unholy": _sound,
    "mail": _sound, "zone": _sound,
}


def validate_settings(doc) -> None:
    """Raise `SettingsError` naming the first field the ledger reads that has
    the wrong type. Unknown fields are not checked."""
    if not isinstance(doc, dict):
        raise SettingsError("the settings document must be an object")
    for key, check in _CHECKS.items():
        if key in doc and not check(doc[key]):
            raise SettingsError("settings field %r has the wrong type" % key)


class TrackerStore:
    """The tracker's folder. Every write is a whole file, replaced atomically."""

    def __init__(self, root):
        self.root = Path(root)

    def path(self, name: str) -> Path:
        return self.root / name

    def read_json(self, name: str, default=None):
        try:
            with open(self.path(name), "r", encoding="utf-8") as fh:
                return json.load(fh)
        except (OSError, ValueError):
            return default

    def write_json(self, name: str, value) -> None:
        """Write `value` as `name`, whole: a temporary file, then `os.replace`.
        On any failure the old file is left as it was, the temporary removed,
        and the error raised."""
        data = json.dumps(value, ensure_ascii=False, indent=1).encode("utf-8")
        self._write_bytes(self.path(name), data)

    def _write_bytes(self, target: Path, data: bytes) -> None:
        target.parent.mkdir(parents=True, exist_ok=True)
        tmp = target.with_name("%s.tmp-%d" % (target.name, os.getpid()))
        try:
            with open(tmp, "wb") as fh:
                fh.write(data)
                fh.flush()
                os.fsync(fh.fileno())
            os.replace(tmp, target)
        except BaseException:
            try:
                os.remove(tmp)
            except OSError:
                pass
            raise

    def copy_in(self, source: Path, name: str) -> None:
        """Copy one file in under `name`, whole, reading the source only."""
        with open(source, "rb") as fh:
            data = fh.read()
        self._write_bytes(self.path(name), data)

    # -- runs --------------------------------------------------------------

    def runs(self) -> list:
        runs = self.read_json("runs.json", [])
        return runs if isinstance(runs, list) else []

    def file_run(self, run: dict) -> bool:
        """Put a finished run at the head of the history, keeping the newest
        200. A run whose id is already there is not filed again."""
        runs = self.runs()
        run_id = run.get("id")
        if run_id is not None and any(isinstance(r, dict) and r.get("id") == run_id for r in runs):
            return False
        runs.insert(0, run)
        del runs[RUNS_KEPT:]
        self.write_json("runs.json", runs)
        return True

    def clear_runs(self) -> None:
        """Empty the run history, as the Tracker's `clear_runs` did: a whole
        `[]`, never a truncate in place."""
        self.write_json("runs.json", [])

    # -- the shopping list -------------------------------------------------

    def shopping(self) -> list:
        """The shopping list. Anything but a JSON list reads as empty, as the
        Tracker's `get_shopping` read it; entries that are not text are left out."""
        items = self.read_json("shopping.json", [])
        return [i for i in items if isinstance(i, str)] if isinstance(items, list) else []

    def set_shopping(self, items) -> list:
        """Write the whole list. Each entry is stripped and must then be 1 to
        120 characters; at most 200 entries. Raises `Refused` otherwise."""
        if not isinstance(items, list):
            raise Refused("items must be a list of item names")
        if len(items) > SHOPPING_MAX_ITEMS:
            raise Refused("the shopping list holds at most %d items" % SHOPPING_MAX_ITEMS)
        clean = []
        for item in items:
            if not isinstance(item, str):
                raise Refused("each shopping list entry must be text")
            item = item.strip()
            if not item:
                raise Refused("a shopping list entry cannot be blank")
            if len(item) > SHOPPING_MAX_CHARS:
                raise Refused("a shopping list entry is at most %d characters" % SHOPPING_MAX_CHARS)
            clean.append(item)
        self.write_json("shopping.json", clean)
        return clean

    # -- custom sounds -----------------------------------------------------

    def _sounds_dir(self) -> Path:
        return self.root / IMPORTED_FOLDER

    def sound_file(self, key: str) -> Path | None:
        """The file that plays for `key`, or None (also for a key this store
        does not file)."""
        if not sound_key_ok(key):
            return None
        for ext in SOUND_TYPES:
            path = self._sounds_dir() / ("%s.%s" % (key, ext))
            if path.is_file():
                return path
        return None

    def sounds(self) -> dict:
        """{key: {"ext", "stamp"}} for every key with a file. `stamp` changes
        whenever the file does."""
        listing = {}
        folder = self._sounds_dir()
        try:
            names = sorted(p.name for p in folder.iterdir())
        except OSError:
            return listing
        keys = {name.rsplit(".", 1)[0] for name in names if "." in name}
        for key in sorted(keys):
            path = self.sound_file(key)
            if path is None:
                continue
            try:
                st = path.stat()
            except OSError:
                continue
            listing[key] = {"ext": path.suffix[1:].lower(), "stamp": "%d-%d" % (st.st_mtime_ns, st.st_size)}
        return listing

    def put_sound(self, key: str, ext: str, data: bytes) -> None:
        """File `data` as `key`'s sound, whole, then remove the key's files of
        any other kind (written first and cleared after, so a failed write
        never leaves the key silent). Raises `Refused` for a key, kind, size
        or content this store will not file."""
        if not sound_key_ok(key):
            raise Refused("not a sound this panel files: %r" % key)
        if ext not in SOUND_TYPES:
            raise Refused("a sound must be a .wav, .mp3, .ogg or .flac file")
        if not data:
            raise Refused("the sound file is empty")
        if len(data) > SOUND_MAX_BYTES:
            raise Refused("the sound file is larger than 8 MiB")
        if not sound_bytes_match(ext, data):
            raise Refused("that file is not a .%s sound" % ext)
        self._write_bytes(self._sounds_dir() / ("%s.%s" % (key, ext)), data)
        self._remove_sound(key, keep=ext)

    def clear_sound(self, key: str) -> None:
        """Delete `key`'s sound, so the bundled one plays again."""
        if not sound_key_ok(key):
            raise Refused("not a sound this panel files: %r" % key)
        self._remove_sound(key)

    def copy_sound(self, source: str, key: str) -> bool:
        """Give `key` the sound `source` has. Nothing to copy is not an error
        (most lists have no sound of their own); returns whether one was."""
        if not sound_key_ok(source) or not sound_key_ok(key):
            raise Refused("not a sound this panel files")
        if source == key:
            raise Refused("a sound cannot be copied onto itself")
        path = self.sound_file(source)
        if path is None:
            return False
        with open(path, "rb") as fh:
            data = fh.read()
        ext = path.suffix[1:].lower()
        self._write_bytes(self._sounds_dir() / ("%s.%s" % (key, ext)), data)
        self._remove_sound(key, keep=ext)
        return True

    def _remove_sound(self, key: str, keep: str | None = None) -> None:
        for ext in SOUND_TYPES:
            if ext == keep:
                continue
            try:
                os.remove(self._sounds_dir() / ("%s.%s" % (key, ext)))
            except FileNotFoundError:
                pass

    # -- settings ----------------------------------------------------------

    def settings(self) -> dict:
        """The settings document with every field present: what was saved,
        over the defaults. A saved field the ledger reads that fails its type
        check reads as the default; unknown fields are kept."""
        doc = default_settings()
        saved = self.read_json("settings.json", {})
        if isinstance(saved, dict):
            for key, value in saved.items():
                check = _CHECKS.get(key)
                if check is None or check(value):
                    doc[key] = value
        return doc

    def save_settings(self, doc) -> dict:
        """Validate and write a whole settings document; returns it as stored."""
        validate_settings(doc)
        merged = default_settings()
        merged.update(doc)
        self.write_json("settings.json", merged)
        return merged


def import_tracker_data(store: TrackerStore, source) -> dict:
    """Copy HS Offline Tracker's settings, runs, shopping list and custom sounds
    from `source` into `store`, only where the file is missing there, and record
    what was done in `import.json`. Nothing in `source` is written, renamed or
    deleted. Returns the record."""
    source = Path(source) if source is not None else None
    report = {
        "source": str(source) if source is not None else None,
        "source_found": bool(source is not None and source.is_dir()),
        "copied": [],
        "skipped": [],
        "at_unix_ms": int(time.time() * 1000),
    }
    if report["source_found"]:
        wanted = [(source / name, name) for name in IMPORTED_FILES]
        sounds = source / IMPORTED_FOLDER
        if sounds.is_dir():
            for entry in sorted(sounds.iterdir()):
                if entry.is_file():
                    wanted.append((entry, "%s/%s" % (IMPORTED_FOLDER, entry.name)))
        for src, name in wanted:
            if not src.is_file():
                continue
            if store.path(name).exists():
                report["skipped"].append({"file": name, "reason": "already present"})
                continue
            try:
                store.copy_in(src, name)
            except OSError as exc:
                report["skipped"].append({"file": name, "reason": "copy failed: %s" % exc})
                continue
            report["copied"].append(name)
    store.write_json("import.json", report)
    return report
