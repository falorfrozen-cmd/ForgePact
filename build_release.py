#!/usr/bin/env python3
"""Build the ForgePact release package:  dist/ForgePact/  -> ready to zip.

    py build_release.py

Layout produced (when frozen, the panel looks for the modfiles/ folder NEXT TO
the exe - see MODFILE_SOURCES in src/forgepact.py):

    dist/ForgePact/
        ForgePact.exe
        modfiles/AurieCore.dll
        modfiles/AuriePatcher.exe
        modfiles/YYToolkit.dll
        modfiles/BloodPactPlugin.dll
        modfiles/HSOfflineTrackerProducer.dll   (optional: HS Offline Tracker live sensor)
        README.md
        CREDITS.md
        LICENSE

If pywebview is installed the panel opens in a native desktop window; if not it
falls back to the default browser (both work).
"""
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent
SRC = ROOT / "src" / "forgepact.py"
# src/forgepact.py imports hs_game_sdk for the Satanic Zone mod pool (names,
# ids and descriptions).  Its own fallback inserts this path at RUNTIME, which
# a frozen build never reaches: PyInstaller resolves imports at BUILD time, and
# the exe runs from a temp unpack directory with no toolkit checkout above it.
# Without --paths the analysis logged "missing module named hs_game_sdk
# (optional)", the except branch set SATANIC_BUFFS/SATANIC_DEBUFFS to (), and
# the packaged panel served empty pool lists - the World tab's Satanic Zone
# section rendered its heading with no rows under it (user report 2026-09-14).
SDK_PY = ROOT.parent / "hs-game-sdk" / "python"
MODFILES = ROOT / "modfiles_shipped"
DIST = ROOT / "dist" / "ForgePact"
NEEDED = ["AurieCore.dll", "AuriePatcher.exe", "YYToolkit.dll", "BloodPactPlugin.dll"]
# Shipped when present; a package without them is still complete.
OPTIONAL = ["HSOfflineTrackerProducer.dll"]  # HS Offline Tracker live sensor


def main() -> int:
    if not SRC.is_file():
        print(f"ERROR: {SRC} does not exist"); return 1

    missing = [n for n in NEEDED if not (MODFILES / n).is_file()]
    if missing:
        print("ERROR: modfiles_shipped is incomplete ->", ", ".join(missing)); return 1

    # Is the plugin in the package the same as the last release build from source?
    # If not, anyone pressing Install gets the OLD plugin, today's commands are not
    # recognised - this happened once and the game crashed on startup.
    ship = ROOT / "plugin_build" / "BloodPactPlugin_ship.dll"
    packaged = MODFILES / "BloodPactPlugin.dll"
    if not ship.is_file():
        print("ERROR: plugin_build/BloodPactPlugin_ship.dll is missing")
        print("       Run plugin_build\\build.bat release before packaging.")
        return 1
    if not packaged.is_file() or ship.read_bytes() != packaged.read_bytes():
        print("ERROR: modfiles_shipped/BloodPactPlugin.dll does not match")
        print("       plugin_build/BloodPactPlugin_ship.dll.  Run")
        print("       plugin_build/build.bat release and copy the resulting DLL")
        print("       into modfiles_shipped.")
        return 1

    # Refused rather than warned: a package built without the SDK looks fine,
    # starts fine, and is missing a whole panel section.  That shipped once.
    if not (SDK_PY / "hs_game_sdk" / "__init__.py").is_file():
        print(f"ERROR: hs_game_sdk not found at {SDK_PY}")
        print("       The Satanic Zone mod pool comes from it; a package built")
        print("       without it has an empty World tab section.  Build from a")
        print("       full toolkit checkout, where hs-game-sdk sits next to")
        print("       ForgePact.")
        return 1

    try:
        import PyInstaller  # noqa: F401
    except ImportError:
        print("ERROR: PyInstaller is missing  ->  py -m pip install pyinstaller"); return 1

    # A ForgePact.exe left over from a previous package locks the dist/ folder.
    # The onefile bootloader runs the real app in a CHILD PROCESS, so there are
    # usually two of them; both have to be stopped.
    if sys.platform == "win32":
        subprocess.run(["taskkill", "/F", "/IM", "ForgePact.exe"],
                       capture_output=True)

    build = ROOT / "build"
    for p in (DIST, build):
        if p.exists():
            shutil.rmtree(p, ignore_errors=True)
        if p.exists():
            print(f"ERROR: could not delete {p} (is the file locked?)"); return 1

    print("== PyInstaller ==")
    cmd = [
        sys.executable, "-m", "PyInstaller", "--noconfirm", "--clean",
        "--onefile", "--windowed", "--name", "ForgePact",
        "--distpath", str(DIST.parent), "--workpath", str(build),
        "--specpath", str(build),
        "--paths", str(SDK_PY),
    ]
    # tkinter is only used by the file picker, and only as a FALLBACK: the primary
    # picker opens through comdlg32 (Win32), and failing that the path can be typed
    # by hand.  Bundling tkinter drags PIL in with it and adds ~30 MB to the package.
    for mod in ("tkinter", "PIL", "numpy", "pandas", "matplotlib", "scipy",
                "PyQt5", "PyQt6", "PySide2", "PySide6", "IPython",
                "pytest", "setuptools", "pip"):
        cmd += ["--exclude-module", mod]
    cmd.append(str(SRC))
    r = subprocess.run(cmd)
    if r.returncode != 0:
        print("ERROR: PyInstaller failed"); return r.returncode

    exe = DIST.parent / "ForgePact.exe"
    if not exe.is_file():
        print("ERROR: ForgePact.exe was not produced"); return 1

    # PyInstaller does not fail a build over an import it could not resolve, it
    # just notes it here.  The one import whose absence is invisible until a
    # player opens the World tab is checked explicitly.
    warn = build / "ForgePact" / "warn-ForgePact.txt"
    if warn.is_file() and "missing module named hs_game_sdk" in warn.read_text(
            encoding="utf-8", errors="replace"):
        print("ERROR: hs_game_sdk did not make it into the package")
        print(f"       (see {warn}).  The Satanic Zone lists would be empty.")
        return 1

    DIST.mkdir(parents=True, exist_ok=True)
    shutil.move(str(exe), str(DIST / "ForgePact.exe"))

    out_mod = DIST / "modfiles"
    out_mod.mkdir(exist_ok=True)
    for n in NEEDED:
        shutil.copy2(MODFILES / n, out_mod / n)
    for n in OPTIONAL:
        if (MODFILES / n).is_file():
            shutil.copy2(MODFILES / n, out_mod / n)

    for n in ("README.md", "CREDITS.md", "LICENSE"):
        p = ROOT / n
        if p.is_file():
            shutil.copy2(p, DIST / n)

    print("\n== package ==")
    for p in sorted(DIST.rglob("*")):
        if p.is_file():
            print(f"  {p.relative_to(DIST).as_posix():<34} {p.stat().st_size:>9}")
    print(f"\nready: {DIST}")
    print("zip it and publish.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
