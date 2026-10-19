"""Embed the Mark 1 pieces' icons in the plugin, or check that they match.

ForgePact #166: each Mark 1 piece (helmet, body armor, boots, gloves) draws an
icon of the owner's own in the bag and on the ground, by the Flight charms'
route (assets/flight-charms/, ForgePact #158). The eight PNGs this folder
expects (ICONS.md) are the owner's own art, never art taken from the game, and
they are this script's inputs: it draws nothing and never writes a PNG.

Unlike the Flight charms' generator, a missing PNG is not an error. The header
embeds the PNGs that are present and listed in manifest.json; with none it is a
header with no icon and a zero count, which still compiles, and the plugin then
shows each piece in its base item's own art.

    py -3 assets/mark1/make_mark1_icons.py                   # write the header
    py -3 assets/mark1/make_mark1_icons.py --check           # verify only
    py -3 assets/mark1/make_mark1_icons.py --write-manifest  # record the PNGs present

Every mode first checks each PNG present: one of the eight expected names, an
8-bit RGBA PNG, not interlaced, every pixel's alpha 0 or 255, a bag icon at
exactly its size (a ground copy's size is recorded from the file), and listed
in manifest.json with the file's sha256. manifest.json lists only PNGs that are
present. The default mode then writes plugin/include/ForgePact/Mark1Icons.hpp.
`--check` writes nothing, and exits 0 only when every PNG passes and the header
is what this script would write (line endings aside, since a Windows checkout
may turn the header's LF into CRLF). `--write-manifest` checks the PNGs'
format, then rewrites manifest.json from the files present.

`--assets` and `--header` point the script at another folder and header; the
tests use them to run it against a temporary folder, never against shipped art.

Stdlib only.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import struct
import sys
import zlib
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
HEADER = ROOT / "plugin" / "include" / "ForgePact" / "Mark1Icons.hpp"
MANIFEST_NAME = "manifest.json"

# (file, piece, ground copy, width, height). The piece key is the word
# `mark1probe spawn` takes. A bag icon covers the piece's footprint at 32 px a
# cell (helmet, boots, gloves 2x2 cells, body armor 2x3); its size is checked
# exactly. A ground copy's size is None: the loader re-centres it, so its size
# is read from the file (about 24x24, the body armor about 24x32). Order: bag
# then ground, helmet, body armor, boots, gloves.
ICONS = (
    ("mark1_helmet.png", "helmet", False, 64, 64),
    ("mark1_helmet_ground.png", "helmet", True, None, None),
    ("mark1_body_armor.png", "body", False, 64, 96),
    ("mark1_body_armor_ground.png", "body", True, None, None),
    ("mark1_boots.png", "boots", False, 64, 64),
    ("mark1_boots_ground.png", "boots", True, None, None),
    ("mark1_gloves.png", "gloves", False, 64, 64),
    ("mark1_gloves_ground.png", "gloves", True, None, None),
)

PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"

Icon = tuple  # (file, piece, ground, width, height, data)


def png_chunks(data: bytes) -> list[tuple[bytes, bytes]]:
    """Answer a PNG's chunks as (type, body), or raise ValueError."""
    if data[:8] != PNG_SIGNATURE:
        raise ValueError("not a PNG")
    chunks = []
    at = 8
    while at + 8 <= len(data):
        length, kind = struct.unpack(">I4s", data[at:at + 8])
        body = data[at + 8:at + 8 + length]
        if len(body) != length:
            raise ValueError(f"chunk {kind!r} is cut short")
        chunks.append((kind, body))
        at += 12 + length
        if kind == b"IEND":
            break
    if not chunks or chunks[0][0] != b"IHDR" or len(chunks[0][1]) != 13:
        raise ValueError("no IHDR chunk")
    return chunks


def png_header(data: bytes) -> tuple[int, int, int, int, int]:
    """Answer (width, height, bit depth, colour type, interlace) from IHDR."""
    ihdr = png_chunks(data)[0][1]
    width, height, depth, colour, _compression, _filter, interlace = struct.unpack(">IIBBBBB", ihdr)
    return width, height, depth, colour, interlace


def rgba_alpha_values(data: bytes, width: int, height: int) -> set[int]:
    """Decode a non-interlaced 8-bit RGBA PNG and answer the alpha values it uses."""
    raw = zlib.decompress(b"".join(body for kind, body in png_chunks(data) if kind == b"IDAT"))
    stride = width * 4
    if len(raw) < height * (stride + 1):
        raise ValueError("pixel data is cut short")
    alphas: set[int] = set()
    prior = bytearray(stride)
    at = 0
    for _row in range(height):
        kind = raw[at]
        line = bytearray(raw[at + 1:at + 1 + stride])
        at += stride + 1
        for i in range(stride):
            left = line[i - 4] if i >= 4 else 0
            up = prior[i]
            corner = prior[i - 4] if i >= 4 else 0
            if kind == 0:
                guess = 0
            elif kind == 1:
                guess = left
            elif kind == 2:
                guess = up
            elif kind == 3:
                guess = (left + up) // 2
            elif kind == 4:
                p = left + up - corner
                pa, pb, pc = abs(p - left), abs(p - up), abs(p - corner)
                guess = left if pa <= pb and pa <= pc else (up if pb <= pc else corner)
            else:
                raise ValueError(f"unknown row filter {kind}")
            line[i] = (line[i] + guess) & 0xFF
        alphas.update(line[3::4])
        prior = line
    return alphas


def read_manifest(folder: Path) -> tuple[dict[str, dict], list[str]]:
    path = folder / MANIFEST_NAME
    try:
        entries = json.loads(path.read_text(encoding="utf-8"))["assets"]
        return {entry["file"]: entry for entry in entries}, []
    except (OSError, ValueError, KeyError, TypeError) as exc:
        return {}, [f"{MANIFEST_NAME}: unreadable ({exc})"]


def check_format(name: str, data: bytes, width: int | None, height: int | None) -> tuple[tuple[int, int] | None, list[str]]:
    """Check one PNG's format; answer its (width, height) and every problem found."""
    try:
        got_w, got_h, depth, colour, interlace = png_header(data)
    except ValueError as exc:
        return None, [f"{name}: unreadable ({exc})"]
    if (depth, colour) != (8, 6):
        return None, [f"{name}: is depth {depth} colour type {colour}, expected 8-bit RGBA"]
    if interlace:
        return None, [f"{name}: is interlaced, expected a non-interlaced PNG"]
    problems = []
    if width is not None and (got_w, got_h) != (width, height):
        problems.append(f"{name}: is {got_w}x{got_h}, expected {width}x{height}")
    try:
        partial = sorted(a for a in rgba_alpha_values(data, got_w, got_h) if a not in (0, 255))
    except (ValueError, zlib.error) as exc:
        return None, problems + [f"{name}: pixels unreadable ({exc})"]
    if partial:
        problems.append(f"{name}: has partial-alpha pixels (alpha {partial[0]}), expected alpha 0 or 255 only")
    return (got_w, got_h), problems


def check_icons(folder: Path) -> tuple[list[Icon], list[str]]:
    """Read and check the PNGs present; answer the ones to embed and every problem found."""
    manifest, problems = read_manifest(folder)
    if problems:
        return [], problems
    expected = {name for name, *_ in ICONS}
    for path in sorted(folder.glob("*.png")):
        if path.name not in expected:
            problems.append(f"{path.name}: not one of the eight expected names (ICONS.md)")
    for name in sorted(set(manifest) - expected):
        problems.append(f"{MANIFEST_NAME} lists {name}, not one of the eight expected names")
    icons: list[Icon] = []
    for name, piece, ground, width, height in ICONS:
        path = folder / name
        entry = manifest.get(name)
        if not path.is_file():
            if entry is not None:
                problems.append(f"{name}: listed in {MANIFEST_NAME} but not present")
            continue
        data = path.read_bytes()
        size, found = check_format(name, data, width, height)
        problems.extend(found)
        if entry is None:
            problems.append(f"{name}: present but not in {MANIFEST_NAME} (run with --write-manifest)")
            continue
        digest = hashlib.sha256(data).hexdigest()
        if entry.get("sha256") != digest:
            problems.append(f"{name}: sha256 {digest} does not match {MANIFEST_NAME}'s {entry.get('sha256')}")
        if size is not None and (entry.get("width"), entry.get("height")) != size:
            problems.append(f"{name}: {MANIFEST_NAME} says {entry.get('width')}x{entry.get('height')}, "
                            f"the file is {size[0]}x{size[1]}")
        if size is not None:
            icons.append((name, piece, ground, size[0], size[1], data))
    return icons, problems


def write_manifest(folder: Path) -> tuple[int, list[str]]:
    """Check the PNGs' format and rewrite manifest.json from the files present."""
    problems: list[str] = []
    assets = []
    for name, _piece, _ground, width, height in ICONS:
        path = folder / name
        if not path.is_file():
            continue
        data = path.read_bytes()
        size, found = check_format(name, data, width, height)
        problems.extend(found)
        if size is not None:
            assets.append({"file": name, "width": size[0], "height": size[1], "mode": "RGBA",
                           "bit_depth": 8, "sha256": hashlib.sha256(data).hexdigest()})
    if problems:
        return 0, problems
    text = json.dumps({"schema": 1, "assets": assets}, indent=2) + "\n"
    (folder / MANIFEST_NAME).write_text(text, encoding="utf-8", newline="\n")
    return len(assets), []


def header_text(icons: list[Icon]) -> str:
    parts = ["// Generated by assets/mark1/make_mark1_icons.py - do not edit by hand.",
             "// The Mark 1 pieces' bag and ground icons (ForgePact #166): the owner's own art,",
             "// nothing taken from the game. Source PNGs and their manifest.json:",
             "// assets/mark1/. Check with `make_mark1_icons.py --check`. With no icon embedded",
             "// kIconCount is 0 and every piece shows its base item's own art.",
             "#pragma once", "#include <cstddef>", "", "namespace ForgePact::Mark1Icons {",
             "// file: the PNG's file name, also the name it is written under in bp_ipc\\mark1\\.",
             "// piece: helmet, body, boots or gloves. ground: the ground copy, else the bag icon.",
             "// width/height: the embedded PNG's size in pixels. data/size: the PNG file's bytes.",
             "struct Icon { const char* file; const char* piece; bool ground; int width; int height;",
             "              const unsigned char* data; size_t size; };",
             "// The most icons there can be (a bag and a ground copy per piece): size per-icon",
             "// state by this, since kIconCount may be 0 and a zero-length array is ill-formed.",
             f"inline constexpr size_t kMaxIcons = {len(ICONS)};"]
    if not icons:
        parts.extend(["// No PNG is embedded: kIcons points at nothing and is never read.",
                      "inline constexpr const Icon* kIcons = nullptr;",
                      "inline constexpr size_t kIconCount = 0;", "}"])
        return "\n".join(parts) + "\n"
    for name, _piece, _ground, _w, _h, data in icons:
        parts.append(f"inline constexpr unsigned char k_{Path(name).stem}[] = {{")
        parts.extend("    " + ", ".join(f"0x{b:02x}" for b in data[i:i + 16]) + ","
                     for i in range(0, len(data), 16))
        parts.append("};")
    parts.append("inline constexpr Icon kIconTable[] = {")
    for name, piece, ground, width, height, _data in icons:
        stem = Path(name).stem
        parts.append(f'    {{ "{name}", "{piece}", {"true" if ground else "false"}, {width}, {height}, '
                     f"k_{stem}, sizeof(k_{stem}) }},")
    parts.extend(["};", "inline constexpr const Icon* kIcons = kIconTable;",
                  "inline constexpr size_t kIconCount = sizeof(kIconTable) / sizeof(kIconTable[0]);", "}"])
    return "\n".join(parts) + "\n"


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument("--check", action="store_true",
                      help="write nothing; exit 1 unless the PNGs and the header are current")
    mode.add_argument("--write-manifest", action="store_true",
                      help="check the PNGs' format and rewrite manifest.json from the files present")
    parser.add_argument("--assets", type=Path, default=HERE, help="the folder holding the PNGs and manifest.json")
    parser.add_argument("--header", type=Path, default=HEADER, help="the header to write or check")
    args = parser.parse_args(argv)

    if args.write_manifest:
        count, problems = write_manifest(args.assets)
        for problem in problems:
            print(f"FAIL {problem}", file=sys.stderr)
        if problems:
            return 1
        print(f"Wrote {MANIFEST_NAME} ({count} PNGs)")
        return 0

    icons, problems = check_icons(args.assets)
    for problem in problems:
        print(f"FAIL {problem}", file=sys.stderr)
    if problems:
        return 1
    text = header_text(icons)
    if args.check:
        try:
            current = args.header.read_bytes().replace(b"\r\n", b"\n")
        except OSError as exc:
            print(f"FAIL {args.header.name}: unreadable ({exc})", file=sys.stderr)
            return 1
        if current != text.encode("utf-8"):
            print(f"FAIL {args.header.name} is stale: re-run make_mark1_icons.py", file=sys.stderr)
            return 1
        print(f"OK {len(icons)} of {len(ICONS)} icons match {MANIFEST_NAME} and {args.header.name}")
        return 0
    args.header.write_text(text, encoding="utf-8", newline="\n")
    print(f"Wrote {args.header.name} ({len(icons)} of {len(ICONS)} icons, {sum(len(i[5]) for i in icons)} bytes)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
