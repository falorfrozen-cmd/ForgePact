#!/usr/bin/env python3
"""ForgePact - Hero Siege Game Mods control panel.

Tek dosyalik yerel web uygulamasi: http://127.0.0.1:8766
BloodPactPlugin (Aurie/YYTK) ile bp_ipc uzerinden konusur:
- ayarlar aninda uygulanir (oyun acikken)
- oyun kapaliyken cmd.txt kuyruga yazilir (plugin acilista isler)
- oyun her acilista otomatik yeniden uygulanir (arkaplan nobetcisi)
Ayarlar %LOCALAPPDATA%/Hero_Siege/forgepact.json icinde kalicidir.
"""

import json
import os
import subprocess
import sys
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import urlparse

PORT = 8766
# Windows bazen (Hyper-V/WSL) bir port araligini rezerve eder ve bind reddedilir.
# O yuzden sirayla bos bir port denenir; calisan port tarayicida acilir.
PORT_CANDIDATES = [8766, 8780, 8801, 8899, 9133, 9777]
ROOT = Path.home() / "AppData" / "Local" / "Hero_Siege"
CONFIG = ROOT / "forgepact.json"
DEFAULT_EXE = r""  # set your own Hero_Siege.exe path in the app's "Game Location" field

SPAWNERS = [
    ("rift", 3516, "Rift Portals"),
    ("battlefield", 4990, "Battlefields"),
    ("cursedorb", 5664, "Cursed Orbs"),
    ("summonportal", 3565, "Summon Portals"),
    ("chaospillars", 4624, "Chaos Pillars"),
]
DROPS = [
    ("relic", "Relics", "only works in Satanic Zones"),
    ("keys", "Dungeon Keys", ""),
    ("gold", "Gold", ""),
]

DEFAULTS = {
    "game_exe": DEFAULT_EXE,
    "density": 3,
    "density_on": True,
    "auto_apply": True,
    "map_reveal": True,
    "relic_gate": True,
    "block_online": True,
    "spawners": {k: 1 for k, *_ in SPAWNERS},
    "drops": {k: 1 for k, *_ in DROPS},
}

_lock = threading.Lock()


def load_cfg() -> dict:
    cfg = dict(DEFAULTS)
    if CONFIG.exists():
        try:
            saved = json.loads(CONFIG.read_text(encoding="utf-8"))
            for k, v in saved.items():
                if k in ("spawners", "drops"):
                    cfg[k] = {**cfg[k], **v}
                else:
                    cfg[k] = v
        except Exception:
            pass
    return cfg


def save_cfg(cfg: dict):
    CONFIG.write_text(json.dumps(cfg, indent=1), encoding="utf-8")


def exe_path(cfg=None) -> Path:
    cfg = cfg or load_cfg()
    return Path(cfg.get("game_exe") or DEFAULT_EXE)


def ipc_dir(cfg=None) -> Path:
    return exe_path(cfg).parent / "bp_ipc"


# YYTK RunnerInterface cache: pre-writing it skips the ~1 min first-launch disassembly.
# Keyed by exe size; YYTK validates the size, so a stale cache (different exe build) is
# safely ignored (it just falls back to a one-time scan + re-caches). Known patched build:
KNOWN_RI_CACHE = {309551616: "309551616 207703889 207705042\n"}


def ensure_ri_cache(cfg=None) -> bool:
    """Write the known YYTK RI cache next to the exe so the FIRST launch is also instant."""
    try:
        exe = exe_path(cfg)
        content = KNOWN_RI_CACHE.get(exe.stat().st_size)
        if not content:
            return False
        cache = exe.with_name(exe.name + ".yytkcache")
        if not cache.exists() or cache.read_text(errors="ignore").split()[:1] != content.split()[:1]:
            cache.write_text(content, encoding="ascii")
        return True
    except Exception:
        return False


MODDED_NAME = "Hero_Siege_Modded.exe"


def modded_exe(cfg=None) -> Path:
    """The PATCHED COPY we launch for modding (offline). The original Hero_Siege.exe
    is never modified, so launching it normally from Steam still works online."""
    return exe_path(cfg).with_name(MODDED_NAME)


WEBVIEW_WINDOW = None  # set in main() when running as a native pywebview window


def _win_open_file_dialog(initdir: str) -> str:
    """Native Win32 file-open dialog via comdlg32.GetOpenFileNameW. Works from any
    thread, appears in the foreground, and works in both the .py and the frozen .exe
    (unlike tkinter, which clashes with pywebview's GUI loop, or pywebview's own dialog,
    which opens hidden when triggered from the HTTP thread)."""
    import ctypes
    from ctypes import wintypes

    class OPENFILENAMEW(ctypes.Structure):
        _fields_ = [
            ("lStructSize", wintypes.DWORD), ("hwndOwner", wintypes.HWND),
            ("hInstance", wintypes.HINSTANCE), ("lpstrFilter", wintypes.LPCWSTR),
            ("lpstrCustomFilter", wintypes.LPWSTR), ("nMaxCustFilter", wintypes.DWORD),
            ("nFilterIndex", wintypes.DWORD), ("lpstrFile", wintypes.LPWSTR),
            ("nMaxFile", wintypes.DWORD), ("lpstrFileTitle", wintypes.LPWSTR),
            ("nMaxFileTitle", wintypes.DWORD), ("lpstrInitialDir", wintypes.LPCWSTR),
            ("lpstrTitle", wintypes.LPCWSTR), ("Flags", wintypes.DWORD),
            ("nFileOffset", wintypes.WORD), ("nFileExtension", wintypes.WORD),
            ("lpstrDefExt", wintypes.LPCWSTR), ("lCustData", ctypes.c_void_p),
            ("lpfnHook", ctypes.c_void_p), ("lpTemplateName", wintypes.LPCWSTR),
            ("pvReserved", ctypes.c_void_p), ("dwReserved", wintypes.DWORD),
            ("FlagsEx", wintypes.DWORD),
        ]

    # Own the dialog to the ForgePact window so it appears in the FOREGROUND (not behind it).
    owner = 0
    try:
        user32 = ctypes.windll.user32
        user32.FindWindowW.restype = wintypes.HWND
        user32.FindWindowW.argtypes = [wintypes.LPCWSTR, wintypes.LPCWSTR]
        owner = user32.FindWindowW(None, "ForgePact") or 0
    except Exception:
        owner = 0

    buf = ctypes.create_unicode_buffer(2048)
    ofn = OPENFILENAMEW()
    ofn.lStructSize = ctypes.sizeof(ofn)
    ofn.hwndOwner = owner
    ofn.lpstrFile = ctypes.cast(buf, wintypes.LPWSTR)
    ofn.nMaxFile = 2048
    ofn.lpstrFilter = "Hero_Siege.exe\0Hero_Siege.exe\0Executables (*.exe)\0*.exe\0All files (*.*)\0*.*\0\0"
    ofn.lpstrInitialDir = initdir
    ofn.lpstrTitle = "Select Hero_Siege.exe"
    # OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | OFN_DONTADDTORECENT
    ofn.Flags = 0x00080000 | 0x00001000 | 0x00000800 | 0x00000008 | 0x02000000
    comdlg32 = ctypes.windll.comdlg32
    comdlg32.GetOpenFileNameW.argtypes = [ctypes.POINTER(OPENFILENAMEW)]
    comdlg32.GetOpenFileNameW.restype = wintypes.BOOL
    if comdlg32.GetOpenFileNameW(ctypes.byref(ofn)):
        return buf.value
    return ""  # user cancelled


def pick_exe_dialog(cfg=None) -> str:
    """Open a native file picker and return the chosen path (or "" / "__ERR__...")."""
    cur = exe_path(cfg)
    initdir = str(cur.parent) if cur.exists() else str(Path.home())
    try:
        return _win_open_file_dialog(initdir)
    except Exception:
        pass
    # Fallback: tkinter (non-Windows or if the Win32 dialog fails).
    try:
        import tkinter as tk
        from tkinter import filedialog
        root = tk.Tk()
        root.withdraw()
        root.attributes("-topmost", True)
        path = filedialog.askopenfilename(
            parent=root, title="Select Hero_Siege.exe", initialdir=initdir,
            filetypes=[("Hero Siege", "Hero_Siege.exe"), ("Executable", "*.exe"), ("All files", "*.*")])
        root.destroy()
        return path or ""
    except Exception as e:
        return f"__ERR__{e}"


CREATE_NO_WINDOW = 0x08000000  # subprocess'in konsol penceresi acmasini engeller


def game_running(cfg=None) -> bool:
    name = exe_path(cfg).name
    try:
        r = subprocess.run(["tasklist", "/FI", f"IMAGENAME eq {name}"],
                           capture_output=True, text=True, timeout=10,
                           creationflags=CREATE_NO_WINDOW)
        return name.lower() in r.stdout.lower()
    except Exception:
        return False


def send_cmds(lines: list, cfg=None) -> str:
    """cmd.txt'ye komutlari ekle (plugin her frame okur; oyun kapaliysa acilista isler)."""
    d = ipc_dir(cfg)
    if not d.exists():
        return "ERROR: bp_ipc folder not found next to the game exe (is the mod plugin installed?)"
    with _lock:
        cmd = d / "cmd.txt"
        existing = ""
        if cmd.exists():
            try:
                existing = cmd.read_text(encoding="ascii", errors="ignore")
                if existing and not existing.endswith("\n"):
                    existing += "\n"
            except Exception:
                existing = ""
        cmd.write_text(existing + "\n".join(lines) + "\n", encoding="ascii")
    return f"{len(lines)} command(s) sent"


def build_cmds(cfg: dict) -> list:
    out = [f"density {min(5, cfg['density']) if cfg.get('density_on') else 1}"]
    out.append(f"reveal {1 if cfg.get('map_reveal', True) else 0}")
    out.append(f"relicgate {1 if cfg.get('relic_gate', False) else 0}")
    out.append(f"blockonline {1 if cfg.get('block_online', True) else 0}")
    for key, idx, *_ in SPAWNERS:
        out.append(f"multobj {idx} {int(cfg['spawners'].get(key, 1))}")
    for key, *_ in DROPS:
        out.append(f"dropmult {key} {int(cfg['drops'].get(key, 1))}")
    return out


# Mod file sources: the "modfiles" folder shipped next to ForgePact.
MODFILE_SOURCES = [
    Path(getattr(sys, "frozen", False) and Path(sys.executable).parent or Path(__file__).parent) / "modfiles",
]
PLUGIN_SOURCES = [
    Path(getattr(sys, "frozen", False) and Path(sys.executable).parent or Path(__file__).parent) / "modfiles",
]


def find_src(fname, sources):
    for s in sources:
        f = s / fname
        if f.exists():
            return f
    return None


def exe_is_patched(exe: Path) -> bool:
    try:
        head = exe.open("rb").read(4096)
        return b".aurie" in head
    except Exception:
        return False


def eac_status(exe: Path) -> str:
    """Classify the target: 'eac_free' (safe to patch in place), or 'legit_eac'
    (a real Steam/EAC install we must NOT modify - online would break and EAC
    relaunches into the clean exe). EAC-free = crack/Steam-emulated or no anti-cheat."""
    b = exe.parent
    if (b / "SmokeAPI.config.json").exists():
        return "eac_free"  # Steam-emulated / cracked copy -> moddable offline
    has_eac = (b / "EasyAntiCheat").exists() or (b / "EOSSDK-Win64-Shipping.dll").exists()
    has_bootstrap = (b / "start_protected_game.exe").exists()
    if not has_eac and not has_bootstrap:
        return "eac_free"  # no anti-cheat at all -> moddable
    return "legit_eac"  # real EAC present, not cracked -> do not patch in place


def mod_chain(cfg=None) -> dict:
    exe = exe_path(cfg)
    b = exe.parent
    return {
        "exeExists": exe.exists(),
        "patched": exe.exists() and exe_is_patched(exe),
        "aurieCore": (b / "AurieCore.dll").exists(),
        "yytk": (b / "mods" / "aurie" / "YYToolkit.dll").exists(),
        "plugin": (b / "mods" / "aurie" / "BloodPactPlugin.dll").exists(),
    }


def op_install_mod(cfg) -> dict:
    import shutil as _sh
    exe = exe_path(cfg)
    if not exe.exists():
        return {"err": "game exe not found - set Game Location first"}
    if game_running(cfg):
        return {"err": "Close the game first, then click Install again."}
    # No hard block: the user installs at their own risk (the original exe is backed up
    # and Remove Plugin restores it). eac_status() is only used for the informational
    # heads-up in the status line - it does NOT prevent installing.
    b = exe.parent
    core = find_src("AurieCore.dll", MODFILE_SOURCES)
    yytk = find_src("YYToolkit.dll", MODFILE_SOURCES)
    plug = find_src("BloodPactPlugin.dll", PLUGIN_SOURCES)
    patcher = find_src("AuriePatcher.exe", MODFILE_SOURCES)
    missing = [n for n, f in (("AurieCore.dll", core), ("YYToolkit.dll", yytk),
                              ("BloodPactPlugin.dll", plug), ("AuriePatcher.exe", patcher)) if f is None]
    if missing:
        return {"err": "mod source files missing: " + ", ".join(missing) +
                       " (put them in a 'modfiles' folder next to ForgePact)"}
    steps = []
    bak = exe.with_name(exe.name + ".aurie_backup")
    if not bak.exists():
        _sh.copy2(exe, bak)
        steps.append("exe backed up")
    _sh.copy2(core, b / "AurieCore.dll")
    (b / "mods" / "aurie").mkdir(parents=True, exist_ok=True)
    (b / "mods" / "native").mkdir(parents=True, exist_ok=True)
    _sh.copy2(yytk, b / "mods" / "aurie" / "YYToolkit.dll")
    _sh.copy2(plug, b / "mods" / "aurie" / "BloodPactPlugin.dll")
    steps.append("mod DLLs installed/updated")
    if not exe_is_patched(exe):
        r = subprocess.run([str(patcher), str(exe), str(b / "AurieCore.dll"), "install"],
                           capture_output=True, text=True, timeout=120,
                           creationflags=CREATE_NO_WINDOW)
        if exe_is_patched(exe):
            steps.append("exe patched")
        else:
            return {"err": "patching failed: " + (r.stdout or r.stderr or "?")[-200:]}
    else:
        steps.append("exe already patched")
    return {"ok": "MOD INSTALLED: " + ", ".join(steps) +
                  " - NOTE: only works on an EAC-free copy (online/EAC games will bounce back to the clean exe).",
            "chain": mod_chain(cfg)}


def op_remove_mod(cfg) -> dict:
    import shutil as _sh
    exe = exe_path(cfg)
    if not exe.exists():
        return {"err": "game exe not found - set Game Location first"}
    if game_running(cfg):
        return {"err": "Close the game first, then click Remove Plugin again."}
    bak = exe.with_name(exe.name + ".aurie_backup")
    if not bak.exists():
        return {"err": f"no backup found ({bak.name}) - nothing to restore. "
                       "The exe may already be the original, or it was installed from a different folder."}
    steps = []
    try:
        _sh.copy2(bak, exe)
        steps.append("original exe restored from backup")
    except Exception as e:
        return {"err": f"could not restore the exe: {e}"}
    if exe_is_patched(exe):
        return {"err": "restore ran but the exe still looks patched - check the .aurie_backup file."}
    b = exe.parent
    for rel in ("AurieCore.dll", "mods/aurie/YYToolkit.dll", "mods/aurie/BloodPactPlugin.dll"):
        try:
            p = b / rel
            if p.exists():
                p.unlink()
        except Exception:
            pass
    steps.append("mod files removed")
    return {"ok": "MOD REMOVED: " + ", ".join(steps) +
                  ". The game is back to its original (un-modded) exe. The .aurie_backup is kept so you can re-install anytime.",
            "chain": mod_chain(cfg)}


LAST = {"applied": None, "queued": False}


def apply_all(cfg: dict) -> str:
    msg = send_cmds(build_cmds(cfg), cfg)
    if not msg.startswith("ERROR"):
        LAST["applied"] = time.strftime("%H:%M:%S")
        LAST["queued"] = not game_running(cfg)
    return msg


def watcher():
    """Oyun her ACILDIGINDA ayarlari otomatik uygula."""
    was_running = game_running()
    while True:
        time.sleep(5)
        try:
            cfg = load_cfg()
            now = game_running(cfg)
            if now and not was_running and cfg.get("auto_apply"):
                # plugin "hazirim" diyene kadar bekle (out.txt taze yazilinca), max 60 sn
                out = ipc_dir(cfg) / "out.txt"
                t0 = time.time()
                while time.time() - t0 < 60:
                    try:
                        if out.exists() and out.stat().st_mtime >= t0 - 10:
                            break
                    except Exception:
                        pass
                    time.sleep(2)
                time.sleep(2)  # son log satirlari otursun
                apply_all(cfg)
            was_running = now
        except Exception:
            pass


class H(BaseHTTPRequestHandler):
    def log_message(self, *a):
        pass

    def _json(self, obj, code=200):
        b = json.dumps(obj, ensure_ascii=False).encode("utf-8")
        self.send_response(code)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(b)))
        self.end_headers()
        self.wfile.write(b)

    def do_GET(self):
        u = urlparse(self.path)
        if u.path == "/":
            b = HTML.encode("utf-8")
            self.send_response(200)
            self.send_header("Content-Type", "text/html; charset=utf-8")
            self.send_header("Content-Length", str(len(b)))
            self.end_headers()
            self.wfile.write(b)
        elif u.path == "/api/state":
            cfg = load_cfg()
            _exe = exe_path(cfg)
            self._json({"cfg": cfg, "gameRunning": game_running(cfg),
                        "ipcOk": ipc_dir(cfg).exists(),
                        "eacStatus": eac_status(_exe) if _exe.exists() else "",
                        "chain": mod_chain(cfg),
                        "spawners": [[k, i, l] for k, i, l in SPAWNERS],
                        "drops": [[k, l, h] for k, l, h in DROPS],
                        "lastApplied": LAST["applied"], "queued": LAST["queued"]})
        else:
            self._json({"err": "not found"}, 404)

    def do_POST(self):
        n = int(self.headers.get("Content-Length", 0))
        body = json.loads(self.rfile.read(n) or b"{}")
        u = urlparse(self.path)
        try:
            cfg = load_cfg()
            if u.path == "/api/set":
                sec, key, val = body.get("section"), body["key"], body["value"]
                if sec in ("spawners", "drops"):
                    cfg[sec][key] = int(val)
                elif key == "density":
                    cfg["density"] = max(1, min(5, int(val)))
                elif key in ("density_on", "auto_apply", "map_reveal", "block_online", "relic_gate"):
                    cfg[key] = bool(val)
                save_cfg(cfg)
                live = ""
                if game_running(cfg):
                    if sec == "spawners":
                        idx = next(i for k, i, *_ in SPAWNERS if k == key)
                        send_cmds([f"multobj {idx} {int(val)}"], cfg)
                    elif sec == "drops":
                        send_cmds([f"dropmult {key} {int(val)}"], cfg)
                    elif key in ("density", "density_on"):
                        send_cmds([f"density {cfg['density'] if cfg['density_on'] else 1}"], cfg)
                    elif key == "map_reveal":
                        send_cmds([f"reveal {1 if cfg['map_reveal'] else 0}"], cfg)
                    elif key == "block_online":
                        send_cmds([f"blockonline {1 if cfg['block_online'] else 0}"], cfg)
                    elif key == "relic_gate":
                        send_cmds([f"relicgate {1 if cfg['relic_gate'] else 0}"], cfg)
                    live = " (applied live)"
                    LAST["applied"] = time.strftime("%H:%M:%S")
                self._json({"ok": f"saved{live}", "cfg": cfg})
            elif u.path == "/api/setexe":
                p = (body.get("path") or "").strip().strip('"')
                if not p.lower().endswith(".exe"):
                    self._json({"err": "path must point to the game .exe"}); return
                if not Path(p).exists():
                    self._json({"err": "file not found: " + p}); return
                cfg["game_exe"] = p
                save_cfg(cfg)
                self._json({"ok": "game exe set", "cfg": cfg,
                            "ipcOk": ipc_dir(cfg).exists()})
            elif u.path == "/api/browseexe":
                p = pick_exe_dialog(cfg)
                if p.startswith("__ERR__"):
                    self._json({"err": "file picker unavailable: " + p[7:]}); return
                if not p:
                    self._json({"err": "no file selected"}); return
                if not p.lower().endswith(".exe") or not Path(p).exists():
                    self._json({"err": "that is not a valid .exe"}); return
                cfg["game_exe"] = p
                save_cfg(cfg)
                self._json({"ok": "game exe set: " + Path(p).name, "cfg": cfg,
                            "path": p, "ipcOk": ipc_dir(cfg).exists()})
            elif u.path == "/api/installmod":
                self._json(op_install_mod(cfg))
            elif u.path == "/api/removeplugin":
                self._json(op_remove_mod(cfg))
            elif u.path == "/api/applyall":
                msg = apply_all(cfg)
                suffix = "" if game_running(cfg) else " - will run when the game starts"
                self._json({"ok": msg + suffix} if not msg.startswith("ERROR") else {"err": msg})
            elif u.path == "/api/launch":
                exe = exe_path(cfg)
                if not exe.exists():
                    self._json({"err": "game exe not found - set Game Location first"}); return
                if game_running(cfg):
                    self._json({"err": "the game is already running"}); return
                if not exe_is_patched(exe):
                    self._json({"err": "exe is not patched - click Install Mod Plugin first"}); return
                try:
                    # Pre-write the YYTK RI cache so even the FIRST launch skips the ~1 min disassembly.
                    fast = ensure_ri_cache(cfg)
                    # Launch the patched (EAC-free copy) exe directly so the mod loads, offline.
                    subprocess.Popen([str(exe)], cwd=str(exe.parent), creationflags=0x00000008)
                    self._json({"ok": "Launching modded Hero Siege (direct, offline)"
                                + (" - fast (cache primed)" if fast else "") + "..."})
                except Exception as e:
                    self._json({"err": f"launch failed: {e}"})
            else:
                self._json({"err": "not found"}, 404)
        except Exception as e:
            self._json({"err": f"error: {e}"}, 500)


HTML = r"""<!DOCTYPE html>
<html lang="en"><head><meta charset="utf-8"><title>ForgePact</title>
<style>
:root{--bg:#0d0a08;--card:#171210;--card2:#1e1713;--ember:#ff7a1a;--ember2:#ffb347;--tx:#e8dcc8;--mut:#8a7a64;--line:#33261c;--ok:#5ad87a}
*{box-sizing:border-box}
body{margin:0;font:14px/1.5 'Segoe UI',sans-serif;background:radial-gradient(1200px 500px at 50% -150px,#2a1408 0%,var(--bg) 60%);color:var(--tx);min-height:100vh}
#wrap{max-width:980px;margin:0 auto;padding:26px 20px 60px}
header{display:flex;align-items:center;gap:16px;margin-bottom:6px}
.logo{font-size:34px;filter:drop-shadow(0 0 12px #ff7a1a88)}
h1{font-size:26px;margin:0;letter-spacing:2px;background:linear-gradient(90deg,var(--ember2),var(--ember),#c44a0a);-webkit-background-clip:text;background-clip:text;color:transparent}
.sub{color:var(--mut);font-size:12px;letter-spacing:3px;text-transform:uppercase}
#statusbar{display:flex;gap:10px;align-items:center;margin:18px 0 22px;flex-wrap:wrap}
.chip{padding:6px 14px;border-radius:20px;font-size:12px;border:1px solid var(--line);background:var(--card)}
.chip.on{border-color:var(--ok);color:var(--ok);box-shadow:0 0 12px #5ad87a22}
.chip.off{border-color:#777;color:#999}
.chip.warn{border-color:var(--ember);color:var(--ember2)}
.chip.err{border-color:#e05050;color:#ff8080}
.card{background:linear-gradient(180deg,var(--card2),var(--card));border:1px solid var(--line);border-radius:12px;padding:18px 22px;margin-bottom:18px;box-shadow:0 4px 24px #00000055}
.card h2{margin:0 0 4px;font-size:16px;color:var(--ember2);letter-spacing:1px}
.card .hint{color:var(--mut);font-size:12px;margin-bottom:14px}
.row{display:flex;align-items:center;gap:14px;padding:9px 0;border-top:1px solid #221a13}
.row:first-of-type{border-top:none}
.row .lbl{width:200px;font-size:13px}
.row .lbl .tag{font-size:10px;color:var(--mut);margin-left:6px}
input[type=range]{flex:1;-webkit-appearance:none;height:6px;border-radius:3px;background:linear-gradient(90deg,#3a2516,#241811);outline:none}
input[type=range]::-webkit-slider-thumb{-webkit-appearance:none;width:18px;height:18px;border-radius:50%;background:radial-gradient(circle at 35% 35%,var(--ember2),var(--ember) 60%,#a03c08);cursor:pointer;box-shadow:0 0 10px #ff7a1a99}
.val{width:52px;text-align:center;font-weight:bold;font-size:15px;color:var(--ember2)}
.val.off{color:#777}
.switch{position:relative;width:46px;height:24px;flex:none}
.switch input{display:none}
.sl{position:absolute;inset:0;border-radius:24px;background:#2a1d13;border:1px solid var(--line);cursor:pointer;transition:.2s}
.sl:before{content:"";position:absolute;width:18px;height:18px;border-radius:50%;left:2px;top:2px;background:#6a5440;transition:.2s}
.switch input:checked + .sl{background:#3a2008;border-color:var(--ember)}
.switch input:checked + .sl:before{transform:translateX(22px);background:radial-gradient(circle at 35% 35%,var(--ember2),var(--ember));box-shadow:0 0 8px #ff7a1aaa}
.btn{background:linear-gradient(180deg,#4a2a10,#33200e);color:var(--ember2);border:1px solid #7a4a1d;border-radius:8px;padding:9px 20px;cursor:pointer;font-size:13px;letter-spacing:.5px}
.btn:hover{background:linear-gradient(180deg,#5e3514,#3d2812);box-shadow:0 0 14px #ff7a1a33}
#exepath{flex:1;background:#0d0907;color:var(--tx);border:1px solid var(--line);border-radius:6px;padding:8px 10px;font-size:12px}
#toast{position:fixed;bottom:24px;left:50%;transform:translateX(-50%);background:#1e150d;border:1px solid var(--ember);color:var(--ember2);border-radius:8px;padding:10px 22px;font-size:13px;opacity:0;transition:.3s;pointer-events:none;box-shadow:0 0 20px #ff7a1a44}
#toast.show{opacity:1}
.note{font-size:11px;color:var(--mut);font-style:italic}
</style></head><body><div id="wrap">
<header><div class="logo">&#128293;</div><div>
  <h1>FORGEPACT</h1><div class="sub">Hero Siege game mods &middot; live control</div>
</div></header>
<div id="statusbar">
  <span class="chip" id="chipGame">...</span>
  <span class="chip" id="chipApply">...</span>
  <label class="chip" style="display:flex;align-items:center;gap:8px;cursor:pointer">
    auto-apply on game launch
    <span class="switch"><input type="checkbox" id="autoapply"><span class="sl"></span></span>
  </label>
  <button class="btn" id="applyall">Apply All Now</button>
</div>

<div class="card">
  <h2>&#127918; Game Location</h2>
  <div class="hint">ForgePact talks to the mod plugin sitting next to this exe. Change it if your game lives somewhere else.</div>
  <div class="row" style="border:none">
    <input id="exepath" placeholder="C:\...\HeroSiege\bin\Hero_Siege.exe">
    <button class="btn" id="exebrowse" title="Open a file picker to choose Hero_Siege.exe">&#128193; Browse...</button>
    <button class="btn" id="exesave">Save</button>
    <button class="btn" id="installmod" title="One click: backs up the exe, copies mod DLLs, patches the exe">Install Mod Plugin</button>
    <button class="btn" id="removeplugin" title="Restores your original exe from the backup and removes the mod files (game must be closed)">Remove Plugin</button>
  </div>
  <div class="row" style="border:none;margin-top:6px">
    <button class="btn" id="launchgame" style="background:linear-gradient(180deg,#1f5a2a,#163f1e);color:#9be8a8;border-color:#2f8a44;font-weight:bold" title="Launches the patched Hero_Siege.exe directly (no EAC) so the mod loads and you stay offline">&#9654; Launch Modded Game</button>
    <span class="note" style="flex:1">Launches the patched exe <b>directly</b> (no EAC launcher) - mod loads, fully offline, online disabled.</span>
  </div>
  <div class="note" id="eacnote"></div>
  <div class="note" id="ipcnote"></div>
  <div class="note" id="chainnote"></div>
</div>

<div class="card">
  <h2>&#128127; Monster Density</h2>
  <div class="hint">Multiplies enemy spawners - applies to newly loaded zones. Adjust it here in the program.</div>
  <div class="note" style="color:#ffb347;border:1px solid #5a3a1a;border-radius:6px;padding:8px 12px;margin-bottom:10px">&#9888; Known quirk: re-entering a map you already visited stacks the multiplier (x3 can feel like x6+). For consistent density, avoid backtracking into cleared maps - or set Density to x1 here before going back.</div>
  <div class="row">
    <span class="lbl">Density multiplier</span>
    <label class="switch"><input type="checkbox" id="den_on"><span class="sl"></span></label>
    <input type="range" id="den" min="1" max="5" step="1">
    <span class="val" id="denval">x3</span>
  </div>
</div>

<div class="card">
  <h2>&#127757; Special Content Spawns</h2>
  <div class="hint">Each spawner rolls independently - higher = more of that content per zone. Applies to newly loaded zones.</div>
  <div id="spawners"></div>
</div>

<div class="card">
  <h2>&#128506; Map Reveal</h2>
  <div class="hint">Reveals the full minimap in every zone (removes fog of war). On by default - turn it off here if you want fog of war back.</div>
  <div class="row" style="border:none">
    <span class="lbl">Reveal full map</span>
    <label class="switch"><input type="checkbox" id="map_reveal"><span class="sl"></span></label>
    <span class="val" id="mapval">on</span>
  </div>
</div>

<div class="card">
  <h2>&#128176; Drop Rates</h2>
  <div class="hint">Multiplies per-kill drop rolls. Applies immediately to every kill.</div>
  <div id="drops"></div>
</div>
</div>
<div id="toast"></div>
<script>
let ST=null, tmr=null;
async function j(u,opt){const r=await fetch(u,opt);return r.json()}
function toast(m){const t=document.getElementById('toast');t.textContent=m;t.classList.add('show');clearTimeout(tmr);tmr=setTimeout(()=>t.classList.remove('show'),2200)}
function row(sec,key,label,val,tagHtml,max){
  return `<div class="row"><span class="lbl">${label}${tagHtml||''}</span>
    <input type="range" min="1" max="${max||100}" step="1" value="${val}" data-sec="${sec}" data-key="${key}">
    <span class="val ${val<=1?'off':''}" style="width:64px">${val<=1?'off':'x'+val}</span></div>`;
}
async function boot(){
  ST=await j('/api/state');
  const c=ST.cfg;
  document.getElementById('autoapply').checked=!!c.auto_apply;
  document.getElementById('den_on').checked=!!c.density_on;
  document.getElementById('den').value=c.density;
  document.getElementById('denval').textContent=(c.density_on?'x'+c.density:'off');
  document.getElementById('denval').className='val '+(c.density_on?'':'off');
  const mr=c.map_reveal!==false;
  document.getElementById('map_reveal').checked=mr;
  document.getElementById('mapval').textContent=mr?'on':'off';
  document.getElementById('mapval').className='val '+(mr?'':'off');
  document.getElementById('exepath').value=c.game_exe||'';
  document.getElementById('spawners').innerHTML=ST.spawners.map(([k,i,l])=>row('spawners',k,l,c.spawners[k]||1,'')).join('');
  document.getElementById('drops').innerHTML=ST.drops.map(([k,l,h])=>
    row('drops',k,l,c.drops[k]||1,h?` <span class="tag">${h}</span>`:'',k==='angelic'?10000:100)).join('');
  bind(); status();
}
function status(){
  const g=document.getElementById('chipGame'), a=document.getElementById('chipApply');
  g.textContent=ST.gameRunning?'GAME RUNNING - changes apply live':'game closed - changes queue for next launch';
  g.className='chip '+(ST.gameRunning?'on':'off');
  a.textContent=ST.lastApplied?('last applied: '+ST.lastApplied+(ST.queued?' (queued)':'')):'not applied yet this session';
  a.className='chip '+(ST.lastApplied?'warn':'off');
  const ch=ST.chain||{};
  const ok=ch.patched&&ch.aurieCore&&ch.yytk&&ch.plugin;
  const cn=document.getElementById('chainnote');
  if(ok){cn.textContent='';}
  else{
    const miss=[];
    if(!ch.patched)miss.push('exe not patched');
    if(!ch.aurieCore)miss.push('AurieCore.dll');
    if(!ch.yytk)miss.push('YYToolkit.dll');
    if(!ch.plugin)miss.push('mod plugin');
    cn.textContent='mod chain incomplete: '+miss.join(', ')+' - click "Install Mod Plugin" (game must be closed)';
    cn.style.color='#ffb347';
  }
  document.getElementById('ipcnote').textContent=ST.ipcOk?'':'bp_ipc appears after the first modded launch';
  document.getElementById('ipcnote').style.color='#8a7a64';
  const en=document.getElementById('eacnote');
  if(ST.eacStatus==='legit_eac'){en.textContent='Note: this looks like a Steam/EAC copy. If EAC is active, online play may break and the mod may not load (EAC can relaunch the clean exe). Your exe is backed up - Remove Plugin reverts it. For best results use an offline / EAC-off copy. Installing is allowed at your own risk.';en.style.color='#e0b060';}
  else if(ST.eacStatus==='eac_free'){en.textContent='';}
  else{en.textContent='';}
}
function bind(){
  document.querySelectorAll('input[type=range][data-sec]').forEach(r=>{
    const valEl=r.parentElement.querySelector('.val');
    r.oninput=()=>{const v=+r.value;valEl.textContent=v<=1?'off':'x'+v;valEl.className='val '+(v<=1?'off':'')};
    r.onchange=async()=>{
      const res=await j('/api/set',{method:'POST',body:JSON.stringify({section:r.dataset.sec,key:r.dataset.key,value:+r.value})});
      toast((r.dataset.key)+' = x'+r.value+' - '+(res.ok||res.err));
    };
  });
  const den=document.getElementById('den');
  den.oninput=()=>{document.getElementById('denval').textContent='x'+den.value};
  den.onchange=async()=>{const res=await j('/api/set',{method:'POST',body:JSON.stringify({key:'density',value:+den.value})});toast('density x'+den.value+' - '+(res.ok||res.err))};
  document.getElementById('den_on').onchange=async(e)=>{
    const res=await j('/api/set',{method:'POST',body:JSON.stringify({key:'density_on',value:e.target.checked})});
    document.getElementById('denval').textContent=e.target.checked?'x'+den.value:'off';
    document.getElementById('denval').className='val '+(e.target.checked?'':'off');
    toast('density '+(e.target.checked?'ON':'OFF')+' - '+(res.ok||res.err));
  };
  document.getElementById('autoapply').onchange=async(e)=>{
    await j('/api/set',{method:'POST',body:JSON.stringify({key:'auto_apply',value:e.target.checked})});
    toast('auto-apply '+(e.target.checked?'ON':'OFF'));
  };
  document.getElementById('map_reveal').onchange=async(e)=>{
    const res=await j('/api/set',{method:'POST',body:JSON.stringify({key:'map_reveal',value:e.target.checked})});
    document.getElementById('mapval').textContent=e.target.checked?'on':'off';
    document.getElementById('mapval').className='val '+(e.target.checked?'':'off');
    toast('map reveal '+(e.target.checked?'ON':'OFF')+' - '+(res.ok||res.err));
  };
  document.getElementById('applyall').onclick=async()=>{
    const res=await j('/api/applyall',{method:'POST',body:'{}'});
    toast(res.ok||res.err); ST.lastApplied=new Date().toTimeString().slice(0,8); status();
  };
  document.getElementById('installmod').onclick=async()=>{
    const btn=document.getElementById('installmod');
    btn.disabled=true; btn.textContent='Installing...';
    const res=await j('/api/installmod',{method:'POST',body:'{}'});
    btn.disabled=false; btn.textContent='Install Mod Plugin';
    if(res.chain)ST.chain=res.chain;
    toast(res.ok||res.err); status();
  };
  document.getElementById('removeplugin').onclick=async()=>{
    const btn=document.getElementById('removeplugin');
    btn.disabled=true; btn.textContent='Removing...';
    const res=await j('/api/removeplugin',{method:'POST',body:'{}'});
    btn.disabled=false; btn.textContent='Remove Plugin';
    if(res.chain)ST.chain=res.chain;
    toast(res.ok||res.err); status();
  };
  document.getElementById('exesave').onclick=async()=>{
    const res=await j('/api/setexe',{method:'POST',body:JSON.stringify({path:document.getElementById('exepath').value})});
    if(res.ok){ST.ipcOk=res.ipcOk}
    toast(res.ok||res.err); status();
  };
  document.getElementById('launchgame').onclick=async()=>{
    const btn=document.getElementById('launchgame'); const old=btn.innerHTML;
    btn.disabled=true; btn.textContent='Launching...';
    const res=await j('/api/launch',{method:'POST',body:'{}'});
    setTimeout(()=>{btn.disabled=false; btn.innerHTML=old;}, 3000);
    toast(res.ok||res.err);
  };
  document.getElementById('exebrowse').onclick=async()=>{
    const btn=document.getElementById('exebrowse'); const old=btn.innerHTML;
    btn.disabled=true; btn.textContent='Choose file...';
    const res=await j('/api/browseexe',{method:'POST',body:'{}'});
    btn.disabled=false; btn.innerHTML=old;
    if(res.path) document.getElementById('exepath').value=res.path;
    if(res.ipcOk!==undefined) ST.ipcOk=res.ipcOk;
    if(res.cfg) ST.cfg=res.cfg;
    toast(res.ok||res.err); status();
  };
}
setInterval(async()=>{const s=await j('/api/state');ST.gameRunning=s.gameRunning;ST.lastApplied=s.lastApplied;ST.queued=s.queued;ST.ipcOk=s.ipcOk;status()},5000);
boot();
</script></body></html>"""


def main():
    global PORT
    srv = None
    for p in PORT_CANDIDATES:
        try:
            srv = ThreadingHTTPServer(("127.0.0.1", p), H)
            PORT = p
            break
        except OSError:
            continue
    if srv is None:
        # Every candidate port was reserved/busy -> let the OS assign ANY free port (never fails).
        srv = ThreadingHTTPServer(("127.0.0.1", 0), H)
        PORT = srv.server_address[1]
    url = f"http://127.0.0.1:{PORT}"
    print(f"ForgePact running at {url}", flush=True)
    threading.Thread(target=watcher, daemon=True).start()
    threading.Thread(target=srv.serve_forever, daemon=True).start()
    # Show the UI in a NATIVE desktop window (no browser, no address bar). Falls back to
    # the default browser only if pywebview/WebView2 is unavailable.
    try:
        import webview
        global WEBVIEW_WINDOW
        WEBVIEW_WINDOW = webview.create_window("ForgePact", url, width=1140, height=860, min_size=(900, 600))
        webview.start()
    except Exception:
        import webbrowser
        print(f"ForgePact: {url}")
        threading.Timer(0.8, lambda: webbrowser.open(url)).start()
        srv.serve_forever()


if __name__ == "__main__":
    main()





