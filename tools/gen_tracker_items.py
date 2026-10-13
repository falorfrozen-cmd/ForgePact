#!/usr/bin/env python3
"""Generate the tracker's item and room table from HS Offline Tracker's data.

HS Offline Tracker (MIT, credited in `CREDITS.md`) keeps its item identities,
rarities, grades and room names as static arrays in
`HS-Offline-Tracker/src-tauri/src/items.rs`. ForgePact's panel-side tracker
ledger needs the same tables, so this script reads those six arrays -- `ITEMS`,
`RARITY_BY_NAME`, `TIER_BY_NAME`, `MUDDLED`, `BY_ID` and `ROOMS` -- and writes
them out as plain Python tuples in `src/forgepact_tracker/item_table.py`.

The parse is strict on purpose: every line inside an array must be one entry
of the array's own shape, and every array must hold as many entries as its
Rust declaration says. A line it cannot read is an error, never a skipped
item, because a quietly shorter table is a drop that stops being recognised.

    py -3 ForgePact/tools/gen_tracker_items.py            # write the module
    py -3 ForgePact/tools/gen_tracker_items.py --check    # exit 1 if stale

`tests/test_tracker_ledger_items.py` compares the committed module with what
this script makes of the Tracker checkout beside ForgePact.
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

FORGEPACT = Path(__file__).resolve().parent.parent
DEFAULT_SOURCE = FORGEPACT.parent / "HS-Offline-Tracker" / "src-tauri" / "src" / "items.rs"
DEFAULT_OUTPUT = FORGEPACT / "src" / "forgepact_tracker" / "item_table.py"

# One Rust string literal, escapes included.
_STR = r'"(?:[^"\\]|\\.)*"'
_INT = r"\d+"

# The shape of one entry of each table, by the Rust element type.
_SHAPES = {
    "(u32, &str)": (_INT, _STR),
    "(&str, &str)": (_STR, _STR),
    "(&str, u8)": (_STR, _INT),
    "(u32, &str, &str, u8)": (_INT, _STR, _STR, _INT),
    "&str": (_STR,),
}

TABLES = ("ITEMS", "RARITY_BY_NAME", "TIER_BY_NAME", "MUDDLED", "BY_ID", "ROOMS")

_DECL = re.compile(r"^static (?P<name>[A-Z_]+): \[(?P<elem>.+); (?P<count>\d+)\] = \[(?P<rest>.*)$")


def _unescape(literal: str) -> str:
    """The value of one Rust string literal (quotes included)."""
    body = literal[1:-1]
    out = []
    i = 0
    while i < len(body):
        ch = body[i]
        if ch != "\\":
            out.append(ch)
            i += 1
            continue
        nxt = body[i + 1]
        if nxt == "u":
            close = body.index("}", i)
            out.append(chr(int(body[i + 3:close], 16)))
            i = close + 1
            continue
        simple = {"n": "\n", "t": "\t", "r": "\r", "0": "\0", "\\": "\\", '"': '"', "'": "'"}
        if nxt not in simple:
            raise ValueError("unsupported escape \\%s in %s" % (nxt, literal))
        out.append(simple[nxt])
        i += 2
    return "".join(out)


def _value(token: str):
    return _unescape(token) if token.startswith('"') else int(token)


def _entry_pattern(elem: str) -> re.Pattern:
    parts = _SHAPES.get(elem)
    if parts is None:
        raise ValueError("unknown element type %r" % elem)
    if len(parts) == 1:
        return re.compile(r"(%s)" % parts[0])
    return re.compile(r"\(" + r",\s*".join("(%s)" % p for p in parts) + r"\)")


def _declarations(text: str) -> dict[str, tuple[str, int, int]]:
    """name -> (element type, declared count, line index) for each table."""
    found = {}
    for index, line in enumerate(text.split("\n")):
        m = _DECL.match(line.strip())
        if m and m.group("name") in TABLES:
            found[m.group("name")] = (m.group("elem"), int(m.group("count")), index)
    missing = [name for name in TABLES if name not in found]
    if missing:
        raise ValueError("items.rs declares no %s" % ", ".join(missing))
    return found


def declared_lengths(text: str) -> dict[str, int]:
    """How many entries each table's Rust declaration says it holds."""
    return {name: count for name, (_, count, _) in _declarations(text).items()}


def parse(text: str) -> dict[str, list]:
    """Every table as a list of tuples (a list of strings for `MUDDLED`).

    Raises `ValueError` for a line that is not an entry of its table's shape
    and for a table whose length disagrees with its declaration.
    """
    text = text.replace("\r\n", "\n")
    lines = text.split("\n")
    tables = {}
    for name, (elem, count, index) in _declarations(text).items():
        pattern = _entry_pattern(elem)
        rest = _DECL.match(lines[index].strip()).group("rest").strip()
        entries = []
        if rest:
            # a one-line table: `[a, b, c];`
            if not rest.endswith("];"):
                raise ValueError("%s: cannot read %r" % (name, lines[index]))
            body = rest[:-2].strip()
            pos = 0
            while pos < len(body):
                m = pattern.match(body, pos)
                if not m:
                    raise ValueError("%s: cannot read %r" % (name, body[pos:]))
                entries.append(tuple(_value(g) for g in m.groups()))
                pos = m.end()
                tail = re.compile(r"\s*,?\s*").match(body, pos)
                pos = tail.end()
        else:
            i = index + 1
            while True:
                if i >= len(lines):
                    raise ValueError("%s: no closing `];`" % name)
                line = lines[i].strip()
                i += 1
                if line == "];":
                    break
                if not line:
                    continue
                m = pattern.fullmatch(line.rstrip(","))
                if not m or not line.endswith(","):
                    raise ValueError("%s: cannot read line %d: %r" % (name, i, line))
                entries.append(tuple(_value(g) for g in m.groups()))
        if len(entries) != count:
            raise ValueError("%s: %d entries, the declaration says %d" % (name, len(entries), count))
        if elem == "&str":
            entries = [e[0] for e in entries]
        tables[name] = entries
    return tables


_HEADER = '''"""HS Offline Tracker's item and room tables, as data.

Generated by `tools/gen_tracker_items.py` from HS Offline Tracker's
`src-tauri/src/items.rs` (v0.1.4, MIT License, see `CREDITS.md`). Do not edit
by hand: change the Tracker's table or the generator and run it again.

- `ITEMS`: (packed identity, display name). An identity is
  `(item_type << 24) | (item_id << 8) | weapon_type`.
- `RARITY_BY_NAME`: (lower-cased name, rarity name).
- `TIER_BY_NAME`: (lower-cased name, grade 1 = D .. 6 = SS).
- `MUDDLED`: names two items answer to, which only an identity tells apart.
- `BY_ID`: (packed identity, name, rarity, grade) for those identities.
- `ROOMS`: (room asset name, the game's own name for it).
"""

'''


def render(text: str) -> str:
    """The `item_table.py` module this script writes for `items.rs` text."""
    tables = parse(text)
    out = [_HEADER]
    for name in TABLES:
        rows = tables[name]
        out.append("%s = (\n" % name)
        for row in rows:
            out.append("    %r,\n" % (row,))
        out.append(")\n\n")
    return "".join(out).rstrip("\n") + "\n"


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("--source", type=Path, default=DEFAULT_SOURCE, help="items.rs to read")
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT, help="module to write")
    parser.add_argument("--check", action="store_true", help="exit 1 when the module is stale")
    args = parser.parse_args(argv)
    if not args.source.is_file():
        print("no items.rs at %s" % args.source, file=sys.stderr)
        return 2
    rendered = render(args.source.read_text(encoding="utf-8"))
    current = args.output.read_text(encoding="utf-8") if args.output.is_file() else None
    if args.check:
        if current != rendered:
            print("%s is stale; run tools/gen_tracker_items.py" % args.output, file=sys.stderr)
            return 1
        print("%s is current" % args.output)
        return 0
    if current != rendered:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        with open(args.output, "w", encoding="utf-8", newline="\n") as fh:
            fh.write(rendered)
        print("wrote %s" % args.output)
    else:
        print("%s already current" % args.output)
    return 0


if __name__ == "__main__":
    sys.exit(main())
