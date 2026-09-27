"""Build ForgePact.exe from a git ref or a working tree, in a temporary
directory, and print its size.

    py -3 tools/package_size.py --ref main      ->  ForgePact.exe main <bytes>
    py -3 tools/package_size.py --tree .        ->  ForgePact.exe tree <bytes>

For comparing what a change does to the onefile package (the Svelte panel's
build is bundled inside it). Both forms build the same way, so the two numbers
differ only by the source:

* ``--ref`` exports the ref of this ForgePact repository with ``git archive``.
* ``--tree`` copies a working tree's tracked and untracked-but-not-ignored
  files, as they are on disk. The directory may be the ForgePact checkout or
  a toolkit checkout that holds ForgePact/.

The files a package needs but git does not carry -- ``modfiles_shipped/*.dll``,
``modfiles_shipped/AuriePatcher.exe``, ``plugin_build/BloodPactPlugin_ship.dll``
and ``yytoolkit-modified/*`` -- are copied from this checkout, and
``hs-game-sdk`` is linked in beside the copy (a directory junction, removed
before the temporary tree is), so build_release.py's guards see what they see
here. A source that has ``panel/`` gets ``npm ci`` and ``npm run build`` first.

Note: build_release.py stops every running ForgePact.exe before it packages
(its existing behaviour), so measuring closes an open panel.
"""
import argparse
import io
import os
import shutil
import stat
import subprocess
import sys
import tarfile
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
# Needed by build_release.py, never tracked (fetched or compiled).
UNTRACKED_INPUTS = [
    ("modfiles_shipped", "*.dll"),
    ("modfiles_shipped", "AuriePatcher.exe"),
    ("plugin_build", "BloodPactPlugin_ship.dll"),
    ("yytoolkit-modified", "*"),
]


def resolve_tree(directory: Path) -> Path:
    """The ForgePact root a --tree argument names."""
    directory = directory.resolve()
    for candidate in (directory, directory / "ForgePact"):
        if (candidate / "build_release.py").is_file():
            return candidate
    raise SystemExit(f"ERROR: no build_release.py in {directory} or {directory / 'ForgePact'}")


def export_ref(ref: str, dest: Path) -> None:
    archive = subprocess.run(["git", "-C", str(ROOT), "archive", "--format=tar", ref],
                             capture_output=True)
    if archive.returncode != 0:
        raise SystemExit(f"ERROR: git archive {ref}: {archive.stderr.decode(errors='replace').strip()}")
    with tarfile.open(fileobj=io.BytesIO(archive.stdout)) as tar:
        tar.extractall(dest, filter="data")


def copy_tree(source: Path, dest: Path) -> None:
    listed = subprocess.run(
        ["git", "-C", str(source), "ls-files", "-z", "--cached", "--others", "--exclude-standard"],
        capture_output=True)
    if listed.returncode != 0:
        raise SystemExit(f"ERROR: git ls-files in {source}: {listed.stderr.decode(errors='replace').strip()}")
    for name in filter(None, listed.stdout.decode("utf-8").split("\0")):
        path = source / name
        if path.is_file():  # a tracked file deleted on disk is not part of the tree
            (dest / name).parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(path, dest / name)


def copy_untracked_inputs(dest: Path) -> None:
    for folder, pattern in UNTRACKED_INPUTS:
        for path in (ROOT / folder).glob(pattern):
            if path.is_file():
                (dest / folder).mkdir(parents=True, exist_ok=True)
                shutil.copy2(path, dest / folder / path.name)


def run(cmd: list, cwd: Path) -> None:
    result = subprocess.run(cmd, cwd=str(cwd), capture_output=True, text=True,
                            encoding="utf-8", errors="replace")
    if result.returncode != 0:
        sys.stderr.write(result.stdout + result.stderr)
        raise SystemExit(f"ERROR: {' '.join(map(str, cmd))} failed in {cwd} (exit {result.returncode})")


def build_panel(forgepact: Path) -> None:
    panel = forgepact / "panel"
    if not (panel / "package.json").is_file():
        return  # a ref from before the Svelte panel: the page is inside forgepact.py
    npm = shutil.which("npm")
    if not npm:
        raise SystemExit("ERROR: npm is not on PATH; the panel has to be built before packaging")
    run([npm, "ci"], panel)
    run([npm, "run", "build"], panel)


def _writable_then_retry(function, path, _exc):
    os.chmod(path, stat.S_IWRITE)
    function(path)


def measure(label: str, fill) -> int:
    temp = Path(tempfile.mkdtemp(prefix="forgepact-size-"))
    forgepact = temp / "ForgePact"
    link = temp / "hs-game-sdk"
    try:
        forgepact.mkdir()
        fill(forgepact)
        copy_untracked_inputs(forgepact)
        sdk = ROOT.parent / "hs-game-sdk"
        if sdk.is_dir():
            run(["cmd", "/c", "mklink", "/J", str(link), str(sdk)], temp)
        build_panel(forgepact)
        run([sys.executable, "build_release.py"], forgepact)
        exe = forgepact / "dist" / "ForgePact" / "ForgePact.exe"
        if not exe.is_file():
            raise SystemExit(f"ERROR: build_release.py did not produce {exe}")
        size = exe.stat().st_size
        print(f"ForgePact.exe {label} {size}")
        return size
    finally:
        # The junction goes first and on its own: removing it never touches
        # the checkout's hs-game-sdk, while a tree walk through it could.
        if os.path.isjunction(link) or link.is_symlink():
            os.rmdir(link)
        if not link.exists():
            shutil.rmtree(temp, onexc=_writable_then_retry)


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    source = parser.add_mutually_exclusive_group(required=True)
    source.add_argument("--ref", help="a git ref of this ForgePact repository")
    source.add_argument("--tree", type=Path, help="a working tree (ForgePact or the toolkit root)")
    args = parser.parse_args(argv)
    if args.ref:
        measure(args.ref, lambda dest: export_ref(args.ref, dest))
    else:
        tree = resolve_tree(args.tree)
        measure("tree", lambda dest: copy_tree(tree, dest))
    return 0


if __name__ == "__main__":
    sys.exit(main())
