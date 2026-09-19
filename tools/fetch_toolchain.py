"""Fetch the pinned third-party headers and binaries ForgePact needs to build,
and verify every hash before anything is written.

Run it:

    py tools/fetch_toolchain.py                  # fetch/verify against the repo root
    py tools/fetch_toolchain.py --verify-only     # hash what's on disk, download nothing
    py tools/fetch_toolchain.py --force           # replace a differing local file

What it fetches, from `tools/toolchain-pins.json`: the YYToolkit v4.0.1 and
Aurie v2.0.2 headers `plugin_build/include/` needs to compile
(`plugin/BUILD.md`; both are AGPL-3.0 and never ours to commit -- see
`.gitignore`), the two unmodified Aurie binaries, and two shipped mod files.
The modified `YYToolkit.dll` is now a plain file pin, downloaded directly
from the hub repository's own release (`hero-siege-offline-toolkit`, tag
`yytoolkit-v4.0.1-hs.1`) and verified by SHA-256 like every other entry here;
it is the "hs.1" build of the documented patch series the hub keeps under
`third_party/yytoolkit/` (see `yytoolkit-modified/NOTICE.md` and the
`YYToolkit-BUILD-INFO.json` installed beside it). It stays a pinned binary
for the same reason it always has: ship the exact binary that was launched
against the game, not a fresh CI build. The optional
`HSOfflineTrackerProducer.dll` is unchanged -- still extracted as a zip
member from the published `ForgePact-1.3.16.zip` (see
`docs/submodules/ForgePact/instructions.md`'s "The build half" for why an
already-launched binary is extracted rather than rebuilt in CI).

**All-or-nothing.** Every hash is verified -- for a zip-member entry, the
archive's own hash first, before any member is read out of it -- and only if
every one of them checks out are any files written. A single bad hash writes
nothing at all and exits 1, printing which `dest` failed and what hash was
expected versus what was actually seen.

A `dest` that already exists with the pinned hash is left alone and not
re-downloaded (printed `ok (already present)`). A `dest` that exists with a
*different* hash is refused during the verify phase -- exit 1, nothing
written -- unless `--force`, which exists for a developer's own hand-placed
file (the local CRLF `Aurie/shared.hpp` most maintainers already have from an
Aurie checkout, per `README.md`'s old "copy it in" instructions).

`--verify-only` hashes whatever is under `--root` against the manifest,
downloads nothing, and exits 1 on the first missing or mismatched file (an
empty root is therefore always a failure -- the negative control that proves
this flag actually checks something).

`run()` takes a callable `url -> bytes` in place of the network, the same
shape `tools/build_catalog.py`'s `GitHubFetcher` is substituted for in the
hub's own tests, so `tests/test_fetch_toolchain.py` needs no network either.
"""

from __future__ import annotations

import argparse
import contextlib
import hashlib
import io
import json
import sys
import tempfile
import urllib.request
import zipfile
from pathlib import Path
from typing import Callable, Dict, Iterator, List, Tuple

TOOLS_DIR = Path(__file__).resolve().parent
REPO_ROOT = TOOLS_DIR.parent
MANIFEST_PATH = TOOLS_DIR / "toolchain-pins.json"

Fetcher = Callable[[str], bytes]


class FetchError(Exception):
    """A hash did not match. Always fatal -- never a silent skip."""


def sha256_hex(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def _dest_is_safe(dest: str) -> bool:
    """No absolute path, no `..` component -- a manifest `dest` writes only
    under `--root`, never outside it."""
    if not dest:
        return False
    normalized = dest.replace("\\", "/")
    if normalized.startswith("/"):
        return False
    if len(normalized) >= 2 and normalized[1] == ":":  # a drive letter, e.g. C:
        return False
    parts = normalized.split("/")
    if "" in parts or ".." in parts or "." in parts:
        return False
    return True


def urllib_fetcher(url: str) -> bytes:
    request = urllib.request.Request(
        url, headers={"User-Agent": "ForgePact-fetch-toolchain"}
    )
    with urllib.request.urlopen(request, timeout=60) as response:
        return response.read()


@contextlib.contextmanager
def temp_root() -> Iterator[Path]:
    """A throwaway directory for tests -- deleted on exit either way."""
    with tempfile.TemporaryDirectory(prefix="fpci-fetch-") as d:
        yield Path(d)


def _resolve_entry(
    entry: dict, fetcher: Fetcher, archive_cache: Dict[str, bytes]
) -> bytes:
    """Download (once, shared via `archive_cache`) and return this entry's bytes.

    A zip-member entry's `archive_sha256` is checked BEFORE the member is
    read out of the zip, so a corrupted or substituted archive is caught
    before any of its contents are trusted.
    """
    url = entry["url"]
    if url not in archive_cache:
        archive_cache[url] = fetcher(url)
    blob = archive_cache[url]

    if "member" in entry:
        actual_archive_hash = sha256_hex(blob)
        if actual_archive_hash != entry["archive_sha256"]:
            raise FetchError(
                f"{entry['dest']}  {actual_archive_hash}  "
                f"MISMATCH (archive sha256 expected {entry['archive_sha256']})"
            )
        with zipfile.ZipFile(io.BytesIO(blob)) as zf:
            data = zf.read(entry["member"])
    else:
        data = blob

    actual = sha256_hex(data)
    if actual != entry["sha256"]:
        raise FetchError(
            f"{entry['dest']}  {actual}  MISMATCH (expected {entry['sha256']})"
        )
    return data


def _verify_only(manifest: dict, root: Path) -> int:
    ok = True
    for entry in manifest["files"]:
        dest = root / entry["dest"]
        if not dest.is_file():
            print(f"{entry['dest']}  -  MISSING (not present under {root})")
            ok = False
            continue
        actual = sha256_hex(dest.read_bytes())
        if actual != entry["sha256"]:
            print(f"{entry['dest']}  {actual}  MISMATCH (expected {entry['sha256']})")
            ok = False
            continue
        print(f"{entry['dest']}  {actual}  ok")
    return 0 if ok else 1


def run(
    manifest: dict,
    *,
    root: Path,
    fetcher: Fetcher,
    verify_only: bool = False,
    force: bool = False,
) -> int:
    entries = manifest["files"]

    for entry in entries:
        if not _dest_is_safe(entry["dest"]):
            print(f"{entry['dest']}  -  REFUSED (unsafe destination)")
            return 1

    if verify_only:
        return _verify_only(manifest, root)

    archive_cache: Dict[str, bytes] = {}
    to_write: List[Tuple[Path, bytes]] = []
    statuses: List[str] = []
    failed = False

    for entry in entries:
        dest = root / entry["dest"]

        if dest.is_file():
            existing_hash = sha256_hex(dest.read_bytes())
            if existing_hash == entry["sha256"]:
                statuses.append(f"{entry['dest']}  {existing_hash}  ok (already present)")
                continue
            if not force:
                print(
                    f"{entry['dest']}  {existing_hash}  REFUSED (pin wants "
                    f"{entry['sha256']}; pass --force to replace a hand-placed file)"
                )
                failed = True
                continue
            # --force: fall through and re-fetch below, overwriting on write.

        try:
            data = _resolve_entry(entry, fetcher, archive_cache)
        except FetchError as exc:
            print(str(exc))
            failed = True
            continue

        to_write.append((dest, data))
        statuses.append(f"{entry['dest']}  {sha256_hex(data)}  ok")

    if failed:
        print("nothing written -- fix the mismatches above and re-run")
        return 1

    for dest, data in to_write:
        dest.parent.mkdir(parents=True, exist_ok=True)
        dest.write_bytes(data)

    for line in statuses:
        print(line)
    return 0


def main(argv: List[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="fetch and verify ForgePact's pinned build toolchain "
        "(headers + third-party binaries), all-or-nothing.",
    )
    parser.add_argument(
        "--root",
        type=Path,
        default=REPO_ROOT,
        help="ForgePact tree to fetch/verify into (default: this repo's root)",
    )
    parser.add_argument(
        "--verify-only",
        action="store_true",
        help="hash what's under --root against the manifest; download nothing",
    )
    parser.add_argument(
        "--force",
        action="store_true",
        help="replace a dest that exists with a different hash",
    )
    args = parser.parse_args(argv)

    manifest = json.loads(MANIFEST_PATH.read_text(encoding="utf-8"))
    return run(
        manifest,
        root=args.root,
        fetcher=urllib_fetcher,
        verify_only=args.verify_only,
        force=args.force,
    )


if __name__ == "__main__":
    raise SystemExit(main())
