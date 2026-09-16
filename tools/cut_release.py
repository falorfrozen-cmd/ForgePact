"""Move ForgePact's version, in both places at once.

Run it:

    py tools/cut_release.py 1.3.20
    py tools/cut_release.py --check
    py tools/cut_release.py --check --expect 1.3.20

Until 1.3.20 ForgePact had no version anywhere - not a `__version__`, not a
`#define`, nothing. The version existed only as a release-notes filename, a git
tag and prose, so "bump the version" could only ever mean "create a file", and
a running panel could not tell you which build it was. Two things now hold it:

* `src/forgepact.py`'s `__version__`, which is canonical because the panel is
  the always-present entry point and works with no compiled DLL at all;
* `plugin/include/ForgePact/Version.hpp`, which the plugin stamps into
  `bp_ipc/out.txt` next to its boot marker, so a bug report says which build
  wrote the log. The panel and the DLL are installed separately and can be
  different builds - Known Limitation 12 is exactly the kind of report where
  that matters.

Two more things are *checked* but not *written*, because they are derived:
`release-notes-v<version>.md` must exist (the module guide treats a version
with player-visible changes and no notes as an incomplete change, and this is
that rule's first mechanical enforcement), and the plugin's boot line must
reference `FORGEPACT_VERSION` rather than a literal.

The same `Site` list both writes and checks, so the setter and the checker
cannot drift apart - that is the property worth copying from the hub's
`tools/cut_release.py`, which this deliberately mirrors rather than imports.
ForgePact is a separate git repository and its guide says it can be built
standalone; a tool that only ran from inside a superproject checkout would mean
a standalone clone could neither bump nor verify its own version. The
superproject's copy is also hub-specific by construction (its ROOT and its
sites are hardcoded to `hub/`).

Like that one, this **deliberately does not touch git**, does not build, does
not stage a DLL and does not run `build_release.py`. It moves version strings
and verifies them. Nothing else.
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path
from typing import List, NamedTuple, Tuple

ROOT = Path(__file__).resolve().parent.parent

# Matches `\r\n` and `\n`, because the worktree is CRLF (`core.autocrlf=true`
# in both this repository and the superproject) and a pattern spanning two
# lines has to tolerate the `\r`.
NL = rb"\r?\n"
EOL = rb"(?=\r?\n)"


class Site(NamedTuple):
    """One place the version is written down."""

    path: str
    what: str
    #: Regex with the version between groups 1 and 2, anchored on something
    #: unique. `src/forgepact.py` is a 2,500-line file full of other numbers,
    #: so the anchor is the assignment at line start.
    pattern: bytes


def sites(version: bytes) -> List[Site]:
    v = re.escape(version)
    return [
        Site(
            "src/forgepact.py",
            "the panel, which is where the version is canonical",
            rb'(?m)^(__version__ = ")' + v + rb'(")' + EOL,
        ),
        Site(
            "plugin/include/ForgePact/Version.hpp",
            "the plugin, which stamps it into out.txt",
            rb'(?m)^(#define FORGEPACT_VERSION ")' + v + rb'(")' + EOL,
        ),
    ]


# Each component is `0` or an ASCII number that does not start with one, and
# both halves of that are load-bearing. `\d` matches non-ASCII digits, so a
# `\d`-based pattern accepts `1.3.2` followed by U+0663 and every version field
# in the tree then holds something no downstream tool will parse. `[0-9]` says
# what is meant.
VERSION = re.compile(r"^(?:0|[1-9][0-9]*)\.(?:0|[1-9][0-9]*)\.(?:0|[1-9][0-9]*)$")

#: Checked, never written: derived from the version rather than a site.
RELEASE_NOTES = "release-notes-v{version}.md"
BOOT_MARKER = "BloodPact plugin loaded"
BOOT_LINE_FILE = "plugin/include/ForgePact/ModManager.hpp"


def current(root: Path) -> str:
    """The version according to `src/forgepact.py`, the canonical site."""
    text = (root / "src" / "forgepact.py").read_bytes()
    found = re.search(rb'(?m)^__version__ = "([^"]+)"', text)
    if not found:
        raise SystemExit("src/forgepact.py has no __version__ line")
    return found.group(1).decode()


def derived(root: Path, version: str) -> Tuple[bool, List[str]]:
    """The checks that are not sites: release notes, and the boot stamp."""
    lines = []
    ok = True

    notes = root / RELEASE_NOTES.format(version=version)
    if notes.is_file():
        lines.append(f"  ok      {version}  {notes.name} -- the release notes exist")
    else:
        ok = False
        lines.append(
            f"  MISSING {version}  {notes.name} -- a version with player-visible "
            "changes and no release notes is an incomplete change"
        )

    boot = (root / BOOT_LINE_FILE).read_bytes()
    stamped = re.search(
        re.escape(BOOT_MARKER.encode()) + rb'[^"]*"\s*FORGEPACT_VERSION', boot
    )
    if stamped:
        lines.append(
            f"  ok      {version}  {BOOT_LINE_FILE} -- the boot line stamps "
            "FORGEPACT_VERSION"
        )
    else:
        ok = False
        lines.append(
            f"  MISSING {version}  {BOOT_LINE_FILE} -- the boot line must reference "
            "FORGEPACT_VERSION, never a literal"
        )

    return ok, lines


def check(root: Path, expect: str | None) -> Tuple[bool, List[str]]:
    """Does every site hold the same version, and is it the one expected?"""
    here = current(root)
    lines = []
    ok = True

    for site in sites(here.encode()):
        blob = (root / site.path).read_bytes()
        hits = len(re.findall(site.pattern, blob))
        if hits == 1:
            lines.append(f"  ok      {here}  {site.path} -- {site.what}")
        else:
            ok = False
            # Zero means it holds some other version; more than one means the
            # anchor stopped being unique and this script can no longer promise
            # it is rewriting the right line.
            lines.append(
                f"  MISSING {here}  {site.path} -- {site.what} ({hits} matches)"
            )

    derived_ok, derived_lines = derived(root, here)
    ok = ok and derived_ok
    lines.extend(derived_lines)

    if expect is not None and expect != here:
        ok = False
        lines.append(f"  MISMATCH the tree says {here}, expected {expect}")

    return ok, lines


def cut(root: Path, new: str) -> List[str]:
    """Rewrite every site, in binary so the CRLF endings and the BOM survive."""
    if not VERSION.match(new):
        raise SystemExit(f"{new!r} is not a three-part version like 1.2.3")

    here = current(root)
    if here == new:
        raise SystemExit(f"already at {new}")

    for site in sites(here.encode()):
        blob = (root / site.path).read_bytes()
        if len(re.findall(site.pattern, blob)) != 1:
            # Refusing here rather than rewriting what does match: a half-bumped
            # tree is worse than an un-bumped one, because --check would then
            # pass on the files it happened to look at.
            _, lines = check(root, None)
            raise SystemExit(
                "the tree does not agree about its current version, so it cannot "
                "be bumped safely:\n" + "\n".join(lines)
            )

    done = []
    for site in sites(here.encode()):
        path = root / site.path
        blob = path.read_bytes()
        # Text mode would read CRLF as `\n` and write it back bare, flipping the
        # whole file to LF for a one-line change - and src/forgepact.py is UTF-8
        # with a BOM as well, which a text-mode rewrite also drops.
        blob, count = re.subn(site.pattern, rb"\g<1>" + new.encode() + rb"\g<2>", blob)
        if count != 1:
            raise SystemExit(f"{site.path}: expected one match, found {count}")
        path.write_bytes(blob)
        done.append(f"  {here} -> {new}  {site.path} -- {site.what}")

    notes = root / RELEASE_NOTES.format(version=new)
    if not notes.is_file():
        done.append(
            f"  NOTE  {notes.name} does not exist yet - write it before releasing; "
            "--check will fail until you do"
        )
    return done


def main(argv: List[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="move ForgePact's version in both places at once "
                    "(the panel and the plugin). Does not touch git, does not "
                    "build, does not stage or package anything.",
    )
    parser.add_argument("version", nargs="?", help="the new version, e.g. 1.3.20")
    parser.add_argument(
        "--check",
        action="store_true",
        help="report the version at every site and fail if they disagree",
    )
    parser.add_argument(
        "--expect",
        default=None,
        help="with --check, also require the version to be this (e.g. the tag)",
    )
    parser.add_argument(
        "--root",
        type=Path,
        default=ROOT,
        help="the ForgePact checkout to act on (default: this script's repository)",
    )
    args = parser.parse_args(argv)

    if args.check:
        ok, lines = check(args.root, args.expect)
        print(f"ForgePact version: {current(args.root)}")
        print("\n".join(lines))
        if not ok:
            print("ERROR: ForgePact's version fields do not agree", file=sys.stderr)
            return 1
        return 0

    if not args.version:
        parser.error("give a version to cut, or --check")

    for line in cut(args.root, args.version):
        print(line)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
