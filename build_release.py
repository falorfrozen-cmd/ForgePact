#!/usr/bin/env python3
"""ForgePact yayin paketini uretir:  dist/ForgePact/  -> zip'lenmeye hazir.

    py build_release.py

Uretilen duzen (panel frozen haldeyken exe'nin YANINDAKI modfiles/ klasorune
bakar - src/forgepact.py icindeki MODFILE_SOURCES):

    dist/ForgePact/
        ForgePact.exe
        modfiles/AurieCore.dll
        modfiles/AuriePatcher.exe
        modfiles/YYToolkit.dll
        modfiles/BloodPactPlugin.dll
        README.md
        CREDITS.md
        LICENSE

pywebview kuruluysa panel yerel bir masaustu penceresinde acilir; kurulu
degilse varsayilan tarayiciya duser (ikisi de calisir).
"""
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent
SRC = ROOT / "src" / "forgepact.py"
MODFILES = ROOT / "modfiles_shipped"
DIST = ROOT / "dist" / "ForgePact"
NEEDED = ["AurieCore.dll", "AuriePatcher.exe", "YYToolkit.dll", "BloodPactPlugin.dll"]


def main() -> int:
    if not SRC.is_file():
        print(f"HATA: {SRC} yok"); return 1

    eksik = [n for n in NEEDED if not (MODFILES / n).is_file()]
    if eksik:
        print("HATA: modfiles_shipped eksik ->", ", ".join(eksik)); return 1

    try:
        import PyInstaller  # noqa: F401
    except ImportError:
        print("HATA: PyInstaller yok  ->  py -m pip install pyinstaller"); return 1

    # Onceki paketten kalan ForgePact.exe dist/ klasorunu kilitler.  Onefile
    # bootloader'i gercek uygulamayi ALT SURECTE calistirdigi icin genelde iki
    # tane olur; ikisi de durdurulmali.
    if sys.platform == "win32":
        subprocess.run(["taskkill", "/F", "/IM", "ForgePact.exe"],
                       capture_output=True)

    build = ROOT / "build"
    for p in (DIST.parent, build):
        if p.exists():
            shutil.rmtree(p, ignore_errors=True)
        if p.exists():
            print(f"HATA: {p} silinemedi (dosya kilitli mi?)"); return 1

    print("== PyInstaller ==")
    cmd = [
        sys.executable, "-m", "PyInstaller", "--noconfirm", "--clean",
        "--onefile", "--windowed", "--name", "ForgePact",
        "--distpath", str(DIST.parent), "--workpath", str(build),
        "--specpath", str(build),
    ]
    # tkinter yalnizca dosya seciciyle ilgili ve YEDEK yol: birincil secici
    # comdlg32 (Win32) uzerinden aciliyor, o da olmazsa yol elle yazilabiliyor.
    # tkinter'i almak PIL'i de beraberinde getirip pakete ~30 MB ekliyor.
    for mod in ("tkinter", "PIL", "numpy", "pandas", "matplotlib", "scipy",
                "PyQt5", "PyQt6", "PySide2", "PySide6", "IPython",
                "pytest", "setuptools", "pip"):
        cmd += ["--exclude-module", mod]
    cmd.append(str(SRC))
    r = subprocess.run(cmd)
    if r.returncode != 0:
        print("HATA: PyInstaller basarisiz"); return r.returncode

    exe = DIST.parent / "ForgePact.exe"
    if not exe.is_file():
        print("HATA: ForgePact.exe uretilemedi"); return 1

    DIST.mkdir(parents=True, exist_ok=True)
    shutil.move(str(exe), str(DIST / "ForgePact.exe"))

    out_mod = DIST / "modfiles"
    out_mod.mkdir(exist_ok=True)
    for n in NEEDED:
        shutil.copy2(MODFILES / n, out_mod / n)

    for n in ("README.md", "CREDITS.md", "LICENSE"):
        p = ROOT / n
        if p.is_file():
            shutil.copy2(p, DIST / n)

    print("\n== paket ==")
    for p in sorted(DIST.rglob("*")):
        if p.is_file():
            print(f"  {p.relative_to(DIST).as_posix():<34} {p.stat().st_size:>9}")
    print(f"\nhazir: {DIST}")
    print("zip'le ve yayinla.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
