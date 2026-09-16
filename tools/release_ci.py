r"""Three small, stdlib-only pieces `forgepact-release.yml` needs and nothing
else does: normalise the typed tag, prove a `build.bat` copied in from `main`
still compiles the same thing as the tagged tree's own copy, and package
`dist/ForgePact/` into the release zip the same way every time.

Run it:

    py tools/release_ci.py tag --tag v1.3.20
    py tools/release_ci.py compile-line --tag-bat a.bat --ci-bat b.bat
    py tools/release_ci.py package --root . --version 1.3.20 --out dist_out \
        --info source_commit=<sha> --info tag=v1.3.20

**`tag`** normalises a typed version or tag the same way
`tools/forgepact_tag.py` does (`cut_release.VERSION`, one optional leading
`v`), and prints `tag=` / `version=` for `$GITHUB_OUTPUT`. It does not touch
git, refuse a taken tag, or compare against the tree -- that is
`forgepact_tag.py`'s job on the tagging workflow; this one only has to agree
with it on what a version looks like, because the two run in different
workflows and a typo here should fail the same way it would there.

**`compile-line`** is the guard behind copying `main`'s `plugin_build\build.bat`
(which can find MSVC on a GitHub-hosted runner) over the tagged tree's own
copy (which cannot -- see the module guide, "The build half"): the discovery
block above the `cl ` line may differ, but the `cl ` line itself and the
`set "FLAGS="` / `set "OUTPUT="` lines it depends on must not, or CI would be
compiling something other than what the tag says it ships.

**`package`** is `build_release.py`'s output turned into the same zip shape
every release has used since v1.3.16 (root `ForgePact-<version>/`,
`SHA256SUMS.txt`, `BUILD-INFO.json`) -- the shape the hub's own
`tools/build_catalog.py` (`derive_strip_prefix`, `archive_contains`) already
expects, so it is not new here, only reproduced.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import sys
import zipfile
from pathlib import Path
from typing import Dict, List, Tuple

sys.path.insert(0, str(Path(__file__).resolve().parent))

import cut_release  # noqa: E402  (sibling module, see the module docstring)

TOOLS_DIR = Path(__file__).resolve().parent
PINS_PATH = TOOLS_DIR / "toolchain-pins.json"

#: Matches `tools/forgepact_tag.py`'s `PREFIX` -- the two never need to
#: import each other (they run in different workflows/repos-of-truth for the
#: tag), but they must never disagree about what "vX.Y.Z" means.
PREFIX = "v"

NEEDED_MODFILES = ["AurieCore.dll", "AuriePatcher.exe", "YYToolkit.dll", "BloodPactPlugin.dll"]


def sha256_hex(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


# --------------------------------------------------------------------------
# tag
# --------------------------------------------------------------------------


def cmd_tag(raw: str) -> int:
    version = raw.strip()
    if version.startswith(PREFIX):
        version = version[len(PREFIX):]

    if not cut_release.VERSION.match(version):
        print(
            f"{raw.strip()!r} is not a version this can tag. Give three "
            f"numbers, as 1.2.3 or {PREFIX}1.2.3."
        )
        return 1

    print(f"tag={PREFIX}{version}")
    print(f"version={version}")
    return 0


# --------------------------------------------------------------------------
# compile-line
# --------------------------------------------------------------------------


def _bat_lines(path: Path) -> List[str]:
    # build.bat is CRLF on disk (.gitattributes: *.bat text eol=crlf);
    # text-mode decoding here is fine because nothing is written back.
    return [line.strip() for line in path.read_text(encoding="utf-8").splitlines()]


def _compile_contract(path: Path) -> Tuple[List[str], List[str], List[str]]:
    lines = _bat_lines(path)
    cl_lines = [line for line in lines if line.startswith("cl ")]
    flags_lines = sorted(line for line in lines if line.startswith('set "FLAGS='))
    output_lines = sorted(line for line in lines if line.startswith('set "OUTPUT='))
    return cl_lines, flags_lines, output_lines


def cmd_compile_line(tag_bat: Path, ci_bat: Path) -> int:
    tag_cl, tag_flags, tag_output = _compile_contract(tag_bat)
    ci_cl, ci_flags, ci_output = _compile_contract(ci_bat)

    ok = True
    if len(tag_cl) != 1:
        print(f"{tag_bat}: expected exactly one 'cl ' line, found {len(tag_cl)}: {tag_cl}")
        ok = False
    if len(ci_cl) != 1:
        print(f"{ci_bat}: expected exactly one 'cl ' line, found {len(ci_cl)}: {ci_cl}")
        ok = False
    if ok and tag_cl[0] != ci_cl[0]:
        print("the 'cl ' line differs between the tagged tree and this build:")
        print(f"  tag: {tag_cl[0]}")
        print(f"  ci:  {ci_cl[0]}")
        ok = False

    if tag_flags != ci_flags:
        print("the set \"FLAGS=...\" lines differ:")
        print(f"  tag: {tag_flags}")
        print(f"  ci:  {ci_flags}")
        ok = False

    if tag_output != ci_output:
        print("the set \"OUTPUT=...\" lines differ:")
        print(f"  tag: {tag_output}")
        print(f"  ci:  {ci_output}")
        ok = False

    if ok:
        print("compile line and FLAGS/OUTPUT sets agree between the tagged tree and this build")
        return 0
    return 1


# --------------------------------------------------------------------------
# package
# --------------------------------------------------------------------------


def cmd_package(root: Path, version: str, out: Path, info_pairs: List[str]) -> int:
    dist = root / "dist" / "ForgePact"
    exe = dist / "ForgePact.exe"
    modfiles = dist / "modfiles"

    missing = []
    if not exe.is_file():
        missing.append(str(exe))
    for name in NEEDED_MODFILES:
        if not (modfiles / name).is_file():
            missing.append(str(modfiles / name))
    if missing:
        print("ERROR: dist/ForgePact is incomplete, missing:")
        for path in missing:
            print(f"  {path}")
        return 1

    current = cut_release.current(root)
    if current != version:
        print(f"ERROR: src/forgepact.py says {current!r}, --version was {version!r}")
        return 1

    info: Dict[str, str] = {}
    for pair in info_pairs:
        if "=" not in pair:
            print(f"ERROR: --info {pair!r} is not key=value")
            return 1
        key, _, value = pair.partition("=")
        info[key] = value

    files = sorted(p for p in dist.rglob("*") if p.is_file())
    hashes: Dict[str, str] = {}
    for path in files:
        rel = path.relative_to(dist).as_posix()
        hashes[rel] = sha256_hex(path.read_bytes())

    plugin_sha256 = hashes["modfiles/BloodPactPlugin.dll"]
    exe_sha256 = hashes["ForgePact.exe"]

    pins = json.loads(PINS_PATH.read_text(encoding="utf-8"))["files"]
    pin_list = [{"dest": p["dest"], "sha256": p["sha256"]} for p in pins]

    build_info = {
        "version": version,
        "plugin_sha256": plugin_sha256,
        "exe_sha256": exe_sha256,
        "pins": pin_list,
        **info,
        # Always last and always literal: never something a caller's --info
        # could override into a lie about whether this build was launched.
        "live_gameplay_verified": False,
    }
    build_info_bytes = (
        json.dumps(build_info, indent=2, sort_keys=False) + "\n"
    ).encode("utf-8")

    sums_lines = [f"{hashes[rel]}  {rel}" for rel in sorted(hashes)]
    sums_bytes = ("\n".join(sums_lines) + "\n").encode("utf-8")

    root_name = f"ForgePact-{version}"
    out.mkdir(parents=True, exist_ok=True)
    zip_path = out / f"{root_name}.zip"

    with zipfile.ZipFile(zip_path, "w", zipfile.ZIP_DEFLATED) as zf:
        for path in files:
            rel = path.relative_to(dist).as_posix()
            zf.write(path, f"{root_name}/{rel}")
        zf.writestr(f"{root_name}/SHA256SUMS.txt", sums_bytes)
        zf.writestr(f"{root_name}/BUILD-INFO.json", build_info_bytes)

    zip_hash = sha256_hex(zip_path.read_bytes())
    sha_path = out / f"{root_name}.zip.sha256"
    sha_path.write_text(f"{zip_hash}  {root_name}.zip\n", encoding="utf-8")

    print(zip_path)
    print(sha_path)
    print(zip_hash)
    return 0


# --------------------------------------------------------------------------
# CLI
# --------------------------------------------------------------------------


def main(argv: List[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="ForgePact release-CI helpers: tag normalisation, the "
        "build.bat compile-line contract, and zip packaging.",
    )
    sub = parser.add_subparsers(dest="command", required=True)

    p_tag = sub.add_parser("tag", help="normalise a typed tag/version")
    p_tag.add_argument("--tag", required=True)

    p_cl = sub.add_parser(
        "compile-line", help="prove two build.bat files compile the same thing"
    )
    p_cl.add_argument("--tag-bat", required=True, type=Path)
    p_cl.add_argument("--ci-bat", required=True, type=Path)

    p_pkg = sub.add_parser("package", help="zip dist/ForgePact/ into a release")
    p_pkg.add_argument("--root", required=True, type=Path)
    p_pkg.add_argument("--version", required=True)
    p_pkg.add_argument("--out", required=True, type=Path)
    p_pkg.add_argument(
        "--info", action="append", default=[], metavar="KEY=VALUE",
        help="extra BUILD-INFO.json field, repeatable",
    )

    args = parser.parse_args(argv)

    if args.command == "tag":
        return cmd_tag(args.tag)
    if args.command == "compile-line":
        return cmd_compile_line(args.tag_bat, args.ci_bat)
    if args.command == "package":
        return cmd_package(args.root, args.version, args.out, args.info)
    parser.error(f"unknown command {args.command!r}")
    return 2


if __name__ == "__main__":
    raise SystemExit(main())
