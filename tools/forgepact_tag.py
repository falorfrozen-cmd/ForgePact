r"""Check the tag a ForgePact release is being cut as, and compose its notes.

Run it:

    py tools/forgepact_tag.py --tag v1.3.21 --existing v1.3.16
    py tools/forgepact_tag.py --tag 1.3.21 --tree 1.3.20 --existing $(git tag --list 'v*')
    py tools/forgepact_tag.py --compose-notes --version 1.3.20 --previous v1.3.16 \
        --generated generated.md --out release-notes.md

Two independent modes, one file, because both read the same tag shape
(`cut_release.VERSION`) and the workflow that calls this needs both in the
same run without a second script to keep in sync.

**Plan mode** (the default) prints four `key=value` lines for `$GITHUB_OUTPUT`
and nothing else:

    version=1.3.21
    tag=v1.3.21
    bump=true
    previous=v1.3.16

`bump=true` means the tree still says something else and has to be rewritten
with `cut_release.py` before the tag is created. `previous` is the highest
released `v*` tag numerically below the version, or empty when there is none
-- it is what the release-notes generator and the notes-composition rules
below both need.

Anything wrong exits non-zero having printed nothing, so the workflow stops
before it has bumped, committed, tagged or drafted anything.

Four refusals earn their place, because everything downstream trusts this
tag: the tree is rewritten to match it, the commit is pushed to `main`, the
tag is pushed, and a draft release is created on it.

**A tag that already exists.** Tagging into one that already has a release --
draft or published -- gives that tag two release objects, and GitHub's own
`releases/latest` then resolves to whichever one it calls latest. The
workflow's own "no release yet" step catches the draft case this alone
cannot see (a draft does not create its tag).

**A version below one already tagged.** `releases/latest` would point at it,
telling every hub asking for the newest ForgePact version something older
than what already shipped.

**A version below what `main` already holds -- the "below the tree" refusal,
and the one with no hub counterpart.** ForgePact did not tag every version it
shipped: `main` moved from 1.3.16 straight through 1.3.17-1.3.20 with no tag
or release for any of them, so the highest *tag* and the tree's *actual*
version can disagree. Without this refusal, `v1.3.17` would pass the
above-the-highest-tag check (1.3.17 > 1.3.16) and `cut_release.py 1.3.17`
would relabel 1.3.20's code as 1.3.17 -- rewriting history backwards on
`main`. Equal to the tree needs no bump; above it does.

**Anything that is not three plain numbers**, optionally `v`-prefixed. The
string reaches a shell and `git tag` either way, so this is not politeness
about formatting. Only a bare-lowercase `v` prefix is stripped -- `vv1.3.21`,
`V1.3.21` and `hub-v1.3.21` are refused rather than silently corrected, the
same as `cut_release.VERSION`'s own ASCII-only leading-zero and digit
handling (a `\d`-based check accepts non-ASCII digits Cargo/Python's own
`int()` would misparse or a downstream tool would refuse after the tree was
already rewritten).

The tag list is compared numerically, never sorted as text: ForgePact's real
tags are ragged (`v1.3.9` alongside `v1.3.16`), and a lexical sort gets that
wrong. Tags from another scheme (`hub-v9.0.0`, `catalog`, `v2.0.0-rc1`) are
ignored for every one of the checks above, and for `previous`.

**Why release notes are never a refusal here.** The whole point of this
change is that nobody has to author `release-notes-vX.Y.Z.md` before tagging
-- see "Notes composition" below. `cut_release.py --check` still refuses a
missing notes file by default; only `--allow-missing-notes`, which the tag
workflow passes, relaxes that.

**Notes composition** (`--compose-notes`) is the second mode, and it is the
only place that reads a release-notes file. Three pure functions do the
selection and assembly; the CLI mode is a thin wrapper that lists a
directory, calls them, and writes the result:

- `note_versions(names)` -- which bare filenames in a directory are actually
  `release-notes-v<version>.md` for a version `cut_release.VERSION` accepts.
  A version with a suffix (`-rc1`), a leading zero, or a filename spelled
  without the `v` is not a match; this repository writes and cuts versions in
  exactly the accepted shape, so nothing outside it should be picked up as if
  it were a release.
- `notes_plan(available, version, previous)` -- which versions between
  `previous` (exclusive) and `version` (inclusive) have a file, split into
  "the tagged version itself" and "everything skipped along the way",
  skipped sorted newest-first, **numerically**, never lexically, for the same
  ragged-tag reason as above. With no previous tag, "skipped" is empty by
  definition -- there is no lower bound to bound it with, and walking the
  entire history back to version 1 would bury the release meant to be at the
  top. This case is not reachable for ForgePact today (there is always a
  `previous` once anything is tagged after `v1.3.16`), and is written down as
  a deliberate rule rather than left to whatever an unbounded scan happens to
  do.
- `compose_body(version, top_text, generated, skipped)` -- turns that plan
  into the actual release body: the tagged version's own file if it exists,
  otherwise GitHub's generated notes under a visible banner demanding a
  rewrite before publishing (because a generated section is developer
  language, not player language, and this repository's release notes are
  read by players deciding whether to update); then every skipped version's
  notes, newest first, so a skipped version is never silently dropped from
  what players read just because nobody tagged it on its own. Sections are
  newest-first specifically because the hub's catalog truncates a release
  body at 8000 characters (`tools/build_catalog.py`'s `_trim_notes`) and
  appends a "see the release page" notice -- ForgePact's notes files for
  1.3.17-1.3.20 alone already total over 11000 characters, so something is
  always going to be cut, and newest-first means it is never the version
  players are updating to. The boilerplate "## How to update" section is
  kept only once, in the first file-sourced section, rather than once per
  concatenated version -- it is identical every time and would otherwise
  spend roughly 800 characters of that budget per repetition for no new
  information.

**Published notes** (`--published-notes --version X`) is the third mode, used
by `forgepact-notes-cleanup.yml` once a release is published. It prints the
bare filenames of every `release-notes-v<version>.md` at or below `X`,
numerically oldest first, one per line, and nothing else. Once `X` is published
its own file and every skipped version it rolled up are on the release page,
and anything older was carried by an earlier published release, so all of them
are safe to delete. A version with no file of its own still lists the older
ones. Versions above `X` are never listed. A malformed version exits non-zero
having printed nothing, so the workflow has nothing to delete.
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path
from typing import Iterable, List, NamedTuple, Optional, Tuple

sys.path.insert(0, str(Path(__file__).resolve().parent))

import cut_release

ROOT = Path(__file__).resolve().parent.parent

PREFIX = "v"

#: Anchored at both ends, ASCII-digit only -- see the module docstring and
#: `cut_release.VERSION`, which this literally is: the gate and the bumper
#: must never disagree about what is writable.
SHAPE = cut_release.VERSION

#: `release-notes-v<version>.md`, anchored so a suffix, a missing `v`, or
#: extra characters never match.
NOTES_NAME = re.compile(r"^release-notes-v(?P<version>.+)\.md$")

HOW_TO_UPDATE_HEADING = "## How to update"

BANNER = (
    "> **Draft notes, generated from pull request titles.** Rewrite the "
    "ForgePact {version} section for players before publishing: the Toolkit "
    "Hub shows this text to players."
)


class Plan(NamedTuple):
    version: str
    tag: str
    #: Whether the tree has to be rewritten before this can be tagged.
    bump: bool
    #: The highest released `v*` tag numerically below `version`, or `""`.
    previous: str


class NotesPlan(NamedTuple):
    #: Whether the tagged version's own notes file exists.
    top_from_file: bool
    #: Skipped versions' bare version strings, newest first, numerically.
    skipped: List[str]


def tag_names(refs: Iterable[str]) -> set:
    """Tag names out of whatever git printed.

    `git ls-remote --tags` gives `refs/tags/v1.3.16` and a peeled
    `refs/tags/v1.3.16^{}` for every annotated tag; `git tag --list` gives the
    bare name. Both arrive here.
    """
    names = set()
    for ref in refs:
        name = ref.strip()
        if not name:
            continue
        name = name.rsplit("refs/tags/", 1)[-1]
        if name.endswith("^{}"):
            name = name[: -len("^{}")]
        names.add(name)
    return names


def as_numbers(version: str) -> tuple:
    return tuple(int(part) for part in version.split("."))


def plan(raw: str, refs: Iterable[str], tree: str) -> Plan:
    version = raw.strip()
    if version.startswith(PREFIX):
        version = version[len(PREFIX):]

    if not SHAPE.match(version):
        hint = ""
        # `[0-9]` here for the same reason as in `cut_release.VERSION` itself:
        # with `\d` this would match `1.3.21` + U+0663 and advise dropping a
        # leading zero that is not there.
        if re.match(r"^[0-9]+\.[0-9]+\.[0-9]+$", version):
            hint = (
                " Drop the leading zero: cut_release.py will not write "
                "01.3.21."
            )
        raise SystemExit(
            f"{raw.strip()!r} is not a version this can tag. "
            f"Give three numbers, as 1.2.3 or {PREFIX}1.2.3.{hint}"
        )

    tag = PREFIX + version
    taken = tag_names(refs)
    if tag in taken:
        raise SystemExit(
            f"{tag} already exists. Tagging it again would give it a second "
            f"release, and releases/latest would resolve to whichever one "
            f"GitHub calls latest. Pick a higher version, or delete that tag "
            f"and its release first."
        )

    released = [
        name for name in taken if name.startswith(PREFIX) and SHAPE.match(name[len(PREFIX):])
    ]
    highest = None
    if released:
        highest = max(released, key=lambda name: as_numbers(name[len(PREFIX):]))
        if as_numbers(version) < as_numbers(highest[len(PREFIX):]):
            raise SystemExit(
                f"{tag} is behind {highest}, which is already tagged. "
                f"Publishing it would point releases/latest at an older "
                f"version than the one players are running. To release an "
                f"older line on purpose, push the tag by hand."
            )

    if as_numbers(version) < as_numbers(tree):
        raise SystemExit(
            f"{tag} is behind the tree, which is already at {tree}. "
            f"Tagging it would relabel {tree}'s code as {version}. If you "
            f"mean to tag the code that is on main right now, use {tree}."
        )

    return Plan(version, tag, bump=tree != version, previous=highest or "")


def note_versions(names: Iterable[str]) -> List[str]:
    """Which bare filenames are `release-notes-v<version>.md` for an accepted version.

    Order is unspecified; callers that need an order (`notes_plan`) sort it
    themselves.
    """
    versions = []
    for name in names:
        match = NOTES_NAME.match(name)
        if not match:
            continue
        version = match.group("version")
        if SHAPE.match(version):
            versions.append(version)
    return versions


def _previous_bound(previous: str) -> Optional[tuple]:
    """`previous` as a numeric bound, or None when there is no earlier tag."""
    if not previous:
        return None
    bare = previous[len(PREFIX):] if previous.startswith(PREFIX) else previous
    return as_numbers(bare)


def notes_plan(available: Iterable[str], version: str, previous: str) -> NotesPlan:
    top_from_file = version in set(available)

    bound = _previous_bound(previous)
    version_key = as_numbers(version)

    skipped = []
    if bound is not None:
        for candidate in available:
            if candidate == version:
                continue
            key = as_numbers(candidate)
            if bound < key < version_key:
                skipped.append(candidate)
    # With no previous tag, "skipped" has no lower bound -- see the module
    # docstring. Left empty rather than walking the whole history.

    skipped.sort(key=as_numbers, reverse=True)
    return NotesPlan(top_from_file=top_from_file, skipped=skipped)


def published_notes(available: Iterable[str], version: str) -> List[str]:
    """Versions whose notes a published `version` has made redundant, oldest first."""
    ceiling = as_numbers(version)
    return sorted((v for v in available if as_numbers(v) <= ceiling), key=as_numbers)


def _normalise(text: str) -> str:
    return text.replace("\r\n", "\n").strip()


def _english_list(items: List[str]) -> str:
    """Oxford-free: `a`, `a and b`, `a, b and c`."""
    if len(items) == 1:
        return items[0]
    if len(items) == 2:
        return f"{items[0]} and {items[1]}"
    return ", ".join(items[:-1]) + f" and {items[-1]}"


def _strip_how_to_update(text: str) -> str:
    lines = text.split("\n")
    for i, line in enumerate(lines):
        if line.strip() == HOW_TO_UPDATE_HEADING.strip():
            return "\n".join(lines[:i]).rstrip()
    return text


def _labelled(text: str, version: str) -> str:
    """Prepend a `# ForgePact <v>` heading if the section does not already have one."""
    for line in text.split("\n"):
        stripped = line.strip()
        if not stripped:
            continue
        if stripped.startswith("# "):
            return text
        break
    return f"# ForgePact {version}\n\n{text}"


def compose_body(
    version: str,
    top_text: Optional[str],
    generated: str,
    skipped: List[Tuple[str, str]],
) -> Tuple[str, str]:
    generated = _normalise(generated)
    skipped = [(v, _normalise(t)) for v, t in skipped]

    sections: List[str] = []
    preamble: List[str] = []
    first_file_section_used = False

    if top_text is not None:
        top = _labelled(_normalise(top_text), version)
        sections.append(top)
        first_file_section_used = True
        top_is_file = True
    else:
        top = f"# ForgePact {version}\n\n{generated}"
        sections.append(top)
        top_is_file = False
        preamble.append(BANNER.format(version=version))

    if skipped:
        names = [v for v, _ in skipped]
        preamble.append(
            f"This release also carries the notes for {_english_list(names)}, "
            f"which were never released on their own."
        )

    for v, text in skipped:
        section = _labelled(text, v)
        if not first_file_section_used:
            first_file_section_used = True
        else:
            section = _strip_how_to_update(section)
        sections.append(section)

    body = "\n\n---\n\n".join(sections)
    if preamble:
        body = "\n\n".join(preamble) + "\n\n" + body

    body = body.strip() + "\n"

    if top_is_file:
        source = "files" if skipped else "file"
    else:
        source = "mixed" if skipped else "generated"

    return body, source


def _read_notes_text(path: Path) -> str:
    return path.read_text(encoding="utf-8-sig")


def _compose_notes_main(args: argparse.Namespace) -> int:
    version = args.version.strip()
    if version.startswith(PREFIX):
        version = version[len(PREFIX):]
    if not SHAPE.match(version):
        print(f"{args.version!r} is not a version this can compose notes for.", file=sys.stderr)
        return 1

    root: Path = args.root
    names = [p.name for p in root.iterdir()]
    available = note_versions(names)

    plan_result = notes_plan(available, version, args.previous or "")

    top_text = None
    if plan_result.top_from_file:
        top_text = _read_notes_text(root / f"release-notes-v{version}.md")

    generated = _read_notes_text(Path(args.generated))

    skipped_pairs = [
        (v, _read_notes_text(root / f"release-notes-v{v}.md")) for v in plan_result.skipped
    ]

    body, source = compose_body(version, top_text, generated, skipped_pairs)

    Path(args.out).write_bytes(body.encode("utf-8"))

    # The top version is always first, whether it came from a file or was
    # generated -- `compose_body` puts it first in both cases.
    versions_in_order = [version] + plan_result.skipped
    print(f"source={source}")
    print(f"versions={' '.join(versions_in_order)}")
    return 0


def main(argv: Optional[List[str]] = None) -> int:
    parser = argparse.ArgumentParser(
        description="check the tag a ForgePact release is being cut as, or "
                    "compose the notes for one",
    )
    parser.add_argument("--tag", help="the tag to cut, as typed")
    parser.add_argument(
        "--tree",
        default=None,
        help="the version the tree holds (default: read it with cut_release.py)",
    )
    parser.add_argument(
        "--existing",
        nargs="*",
        default=[],
        help="tags that already exist, as names or refs",
    )
    parser.add_argument("--root", type=Path, default=ROOT)

    parser.add_argument(
        "--compose-notes",
        action="store_true",
        help="compose a release body instead of planning a tag",
    )
    parser.add_argument("--version", help="the version being tagged (compose-notes mode)")
    parser.add_argument(
        "--previous",
        default=None,
        help="the previous v* tag, or empty (compose-notes mode)",
    )
    parser.add_argument("--generated", help="path to GitHub's generated notes (compose-notes mode)")
    parser.add_argument("--out", help="path to write the composed body to (compose-notes mode)")

    parser.add_argument(
        "--published-notes",
        action="store_true",
        help="list the notes files a published --version makes redundant",
    )

    args = parser.parse_args(argv)

    if args.published_notes:
        if args.compose_notes:
            parser.error("--published-notes and --compose-notes are separate modes")
        if not args.version:
            parser.error("--published-notes needs --version")
        if args.tag or args.existing or args.generated or args.out or args.previous is not None:
            parser.error("--published-notes takes only --version and --root")
        version = args.version.strip()
        if version.startswith(PREFIX):
            version = version[len(PREFIX):]
        if not SHAPE.match(version):
            print(f"{args.version!r} is not a version this can list notes for.", file=sys.stderr)
            return 1
        available = note_versions(p.name for p in args.root.iterdir())
        for v in published_notes(available, version):
            print(f"release-notes-v{v}.md")
        return 0

    if args.compose_notes:
        if not args.version or not args.generated or not args.out:
            parser.error("--compose-notes needs --version, --generated and --out")
        if args.tag or args.existing:
            parser.error("--compose-notes does not take --tag or --existing")
        return _compose_notes_main(args)

    if args.version or args.generated or args.out:
        parser.error("--version/--generated/--out are only for --compose-notes")
    if not args.tag:
        parser.error("give --tag, or --compose-notes")

    tree = args.tree or cut_release.current(args.root)
    chosen = plan(args.tag, args.existing, tree)

    # Four bare lines: this is appended straight to `$GITHUB_OUTPUT`, and
    # `bump` is compared as a string because that is the only shape a step's
    # `if:` can test. Nothing is printed on the refusal path, so a workflow
    # that ignored the exit code would still have no version to act on.
    print(f"version={chosen.version}")
    print(f"tag={chosen.tag}")
    print(f"bump={'true' if chosen.bump else 'false'}")
    print(f"previous={chosen.previous}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
