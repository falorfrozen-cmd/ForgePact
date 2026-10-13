#!/usr/bin/env python3
"""Replay a tracker session file, or read one character save, without the panel.

The plugin's `tracker` mod (ForgePact #185) appends protocol-v1 records to
`<game>\\bin\\bp_ipc\\tracker\\session-<pid>-<unix ms>.ndjson`. This tool runs
one such file through the panel's own ledger (`src/forgepact_tracker`) and
prints what it adds up to: the gold, XP and kills earned, the drops counted
by rarity, the journal, the rooms in the order the file names them, the
character and the lines it could not read. It is the fast loop for the
sensor: a session from a live run can be checked here without a panel, and
Live procedure 1 compares its totals with the file's own sums.

    py -3 ForgePact/tools/tracker_replay.py <session file> [--json]
    py -3 ForgePact/tools/tracker_replay.py --save <N> [--json] [--saves-root <dir>]

`--save` takes the save file's number, not the character-select slot. The
files count from 0: slot N of character select loads `herosiege<N-1>.hss`, so
slot 14 (Sorak, in the live procedures) is `--save 13` (RDM § 8.2). It reads
`herosiege<N>.hss` through the panel's save reader
(`src/tracker_saves.py`) and prints the `progress` record it makes: the
character, experience, kills and the boss and chest tallies. The saves root
defaults to `%LOCALAPPDATA%\\Hero_Siege`; `hs2saves\\` is read first, the
legacy root only when `hs2saves` holds no character slot, as the reader does.

Read-only: it opens each file for reading and writes nothing anywhere.
Exit codes: 0 read, 1 the file or slot could not be read, 2 bad arguments.
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

FORGEPACT = Path(__file__).resolve().parent.parent
_SRC = FORGEPACT / "src"
if str(_SRC) not in sys.path:
    sys.path.insert(0, str(_SRC))

from forgepact_tracker.ledger import Ledger  # noqa: E402
from forgepact_tracker.protocol import ProtocolError, parse_line  # noqa: E402
import tracker_saves  # noqa: E402


def replay_session(path) -> dict:
    """What the ledger makes of one session file, read whole, with the alert
    settings' defaults. A last line without its newline counts as a bad line,
    as the panel counts it once the game has ended."""
    path = Path(path)
    data = path.read_bytes()
    ledger = Ledger(file=path.name)
    bad_lines = 0
    records = 0
    kinds: dict[str, int] = {}
    rooms: list[str] = []
    drop_records = 0
    lines = data.split(b"\n")
    tail = lines.pop()   # what follows the last newline: b"" for a whole file
    for raw in lines:
        if raw.endswith(b"\r"):
            raw = raw[:-1]
        if not raw.strip():
            continue
        try:
            record = parse_line(raw)
        except ProtocolError:
            bad_lines += 1
            continue
        records += 1
        kind = record["kind"]
        kinds[kind] = kinds.get(kind, 0) + 1
        if kind == "room" and isinstance(record.get("room"), str) and record["room"]:
            if not rooms or rooms[-1] != record["room"]:
                rooms.append(record["room"])
        elif kind == "ground_drop":
            drop_records += 1
        ledger.apply(record)
    if tail.strip():
        bad_lines += 1
    snapshot = ledger.snapshot()
    return {
        "file": path.name,
        "records": records,
        "kinds": dict(sorted(kinds.items())),
        "bad_lines": bad_lines,
        "gold": ledger.gold_earned,
        "xp": ledger.xp_earned,
        "kills": ledger.kills_earned,
        "drop_records": drop_records,
        "drops": {name: count["total"] for name, count in ledger.items.items() if count["total"]},
        "journal": len(ledger.drops),
        "journal_entries": [{"rarity": d["rarity"], "name": d["name"], "room": d["room"]} for d in ledger.drops],
        "rooms": rooms,
        "room": ledger.room,
        "satanic_zone": snapshot["satanic_zone"],
        "character": ledger.character["name"] if ledger.character else None,
        "status": ledger.status,
        "session_secs": snapshot["session_secs"],
    }


def save_path(slot: int, saves_root) -> Path:
    """The slot's file, where the save reader would look for it."""
    root = Path(saves_root)
    watcher = tracker_saves.SaveWatcher(root)
    wanted = "herosiege%d.hss" % slot
    for candidate in watcher.candidate_files():
        if candidate.name.lower() == wanted:
            return candidate
    nested = root / "hs2saves" / wanted
    return nested if nested.parent.is_dir() else root / wanted


def read_save(slot: int, saves_root) -> dict:
    """The `progress` record the save reader makes of one slot."""
    path = save_path(slot, saves_root)
    record = tracker_saves.progress_event(tracker_saves.decode_hss(path.read_bytes()))
    record["file"] = str(path)
    return record


def _print_session(result: dict) -> None:
    print("file        %s" % result["file"])
    print("records     %d  (bad lines %d)" % (result["records"], result["bad_lines"]))
    print("kinds       %s" % ", ".join("%s=%d" % kv for kv in result["kinds"].items()))
    print("character   %s" % (result["character"] or "-"))
    print("gold        %d" % result["gold"])
    print("xp          %d" % result["xp"])
    print("kills       %d" % result["kills"])
    print("drops       %s" % (", ".join("%s=%d" % kv for kv in result["drops"].items()) or "none"))
    print("journal     %d" % result["journal"])
    print("rooms       %s" % (" > ".join(result["rooms"]) or "none"))
    zone = result["satanic_zone"]
    print("satanic     %s" % (zone["zone"] if zone else "-"))


def _print_save(record: dict) -> None:
    for key in ("file", "name", "level", "hero_level", "experience", "kills", "difficulty", "hell_sub", "act",
                "hardcore"):
        print("%-11s %s" % (key, record.get(key)))
    print("tallies     %d counters" % len(record.get("tallies") or {}))


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n", 1)[0])
    parser.add_argument("session", nargs="?", help="a session-<pid>-<ms>.ndjson file")
    parser.add_argument("--save", type=int, metavar="N",
                        help="read save file herosiege<N>.hss instead. The files count from 0, so character-select "
                             "slot 14 is --save 13")
    parser.add_argument("--saves-root", default=None, help="the saves root (default %%LOCALAPPDATA%%\\Hero_Siege)")
    parser.add_argument("--json", action="store_true", help="print JSON")
    args = parser.parse_args(argv)
    if (args.session is None) == (args.save is None):
        parser.print_usage(sys.stderr)
        print("tracker_replay: give a session file or --save <N>, not both", file=sys.stderr)
        return 2
    try:
        if args.save is not None:
            root = args.saves_root if args.saves_root is not None else tracker_saves.default_save_root()
            result = read_save(args.save, root)
        else:
            result = replay_session(args.session)
    except (OSError, tracker_saves.SaveError) as exc:
        print("tracker_replay: %s" % exc, file=sys.stderr)
        return 1
    if args.json:
        print(json.dumps(result, indent=1, ensure_ascii=False))
    elif args.save is not None:
        _print_save(result)
    else:
        _print_session(result)
    return 0


if __name__ == "__main__":
    sys.exit(main())
