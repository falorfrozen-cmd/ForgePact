"""The tracker's own files, under `%LOCALAPPDATA%\\Hero_Siege\\tracker\\`.

| File | What it holds |
|---|---|
| `runs.json` | HS Offline Tracker's run list, newest first, at most 200 |
| `settings.json` | the Tracker's settings document |
| `cursor.json` | per session file: finalized, runs filed, the panel's controls |
| `import.json` | what the one-time import of the Tracker's data did |
| `shopping.json`, `sounds\\*` | copied by the import, for the Tracker tab |

The panel writes only whole files: a temporary file beside the target, then
`os.replace`, so a reader never sees half a document and a failed write
leaves the old one whole. The service holds one lock around every use.

`import_tracker_data` is the one-time, read-only import of HS Offline
Tracker's data, after the Tracker's own `migrate_legacy_data`: it copies only
files missing here and never writes, renames or deletes anything in the
Tracker's folder.
"""

from __future__ import annotations

import json
import os
import time
from pathlib import Path

RUNS_KEPT = 200

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
