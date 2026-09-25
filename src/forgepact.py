#!/usr/bin/env python3
"""ForgePact - Hero Siege Game Mods control panel.

Local web app: http://127.0.0.1:8766 (artwork in sibling panel_icons.py)
Talks to BloodPactPlugin (Aurie/YYTK) over bp_ipc:
- settings apply instantly while the game is running
- while the game is closed, commands are queued in cmd.txt (the plugin
  processes them on startup)
- settings are re-applied automatically on every launch (background watcher)
Settings persist in %LOCALAPPDATA%/Hero_Siege/forgepact.json.
"""

# ForgePact's version. Canonical: the panel is the always-present entry point
# and works with no compiled DLL at all, so tools/cut_release.py reads the
# current version from here. Do NOT hand-edit it - `py tools/cut_release.py
# <version>` moves every site at once and `--check` fails if they disagree.
__version__ = "1.4.5"

import hashlib
import json
import os
import socket
import struct
import subprocess
import sys
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import urlparse
from panel_icons import ICON_SPRITE, ICON_MAP_JS
import offline_launcher

try:
    from hs_game_sdk import (
        GameObject,
        GameScript,
        StatId,
        PROC_FAMILIES,
        EquipmentSlot,
        PlayerEquipment,
        scan_relic_levels,
        ModDefinition,
        GLOBAL_MOD_REGISTRY,
        SATANIC_BUFFS,
        SATANIC_DEBUFFS,
    )
except ImportError:
    _sdk_path = Path(__file__).resolve().parents[2] / "hs-game-sdk" / "python"
    if _sdk_path.exists() and str(_sdk_path) not in sys.path:
        sys.path.insert(0, str(_sdk_path))
    try:
        from hs_game_sdk import (
            GameObject,
            GameScript,
            StatId,
            PROC_FAMILIES,
            EquipmentSlot,
            PlayerEquipment,
            scan_relic_levels,
            ModDefinition,
            GLOBAL_MOD_REGISTRY,
            SATANIC_BUFFS,
            SATANIC_DEBUFFS,
        )
    except Exception:
        GameObject = None
        GameScript = None
        StatId = None
        PROC_FAMILIES = {}
        GLOBAL_MOD_REGISTRY = None
        SATANIC_BUFFS = ()
        SATANIC_DEBUFFS = ()

PORT = 8766
# Windows sometimes reserves a port range (Hyper-V/WSL) and refuses the bind.
# So free ports are tried in order; whichever works is opened in the browser.
PORT_CANDIDATES = [8766, 8780, 8801, 8899, 9133, 9777]
ROOT = Path.home() / "AppData" / "Local" / "Hero_Siege"
CONFIG = ROOT / "forgepact.json"
DEFAULT_EXE = r""  # set your own Hero_Siege.exe path in the app's "Game Location" field

# The second field is informational only: the S10 marker object index.
# The plugin resolves the marker BY NAME (specialrate -> asset_get_index),
# because these indices shift on every game update.
SPAWNERS = [
    ("rift", 4672, "Rift Portals", 100),
    ("battlefield", 4659, "Battlefields", 100),
    ("cursedorb", 4665, "Cursed Orbs", 100),
    ("summonportal", 4676, "Summon Portals", 100),
    ("chaospillars", 4662, "Chaos Pillars", 100),
    # Chaos Tower and Shadow Realm are "once per run" mechanics.  The plugin
    # resets their persistent flags right before each marker activates
    # (see plugin: Hook_ChaosTowerGate / Hook_ShadowRealmGate), so the game's
    # own placement code runs for every copy.
    ("chaostower", 4663, "Chaos Tower", 100),
    ("shadowrealm", 4674, "Shadow Realm", 100),
]
# Keys listed here are removed from saved settings and never emitted.  Empty
# since Chaos Tower's Season 10 route was decoded (2026-09-03).
DISABLED_SPAWNER_KEYS = set()
# Key families.  The third field is the LoadDrops drop type; when it is None
# that family's gate is already open and only the rate is adjusted.
KEYS = [
    ("dungeon", "Dungeon Keys", 12),
    ("angelic", "Angelic Keys", 16),
    ("chaos", "Chaos + Crystal Keys", None),
    ("bifrost", "Bifrost Key", None),
    ("relic", "Relics", 41),
    # Families below were live-tested 2026-09-06 (docs/drop-slider-static-analysis.md).
    # They carry NO drop type on purpose (1.3.13): the slider only scales the
    # family's own vanilla roll where the game already rolls it.  1.3.10-1.3.12
    # also opened the LoadDrops gate everywhere, which made every monster in
    # every zone drop fragments, scrolls and shards (player reports 2026-09-07).
    ("rune", "Runes", None),
    ("stone", "Gems (chipped to flawless)", None),
    ("bossgem", "Boss Gems", None),
    ("orb", "Orbs", None),
    ("scrollofra", "Scrolls of Ra", None),
    ("dimshard", "Dimensional Shards", None),
    ("battlefrag", "Battle Fragments", None),
    ("colosfrag", "Colosseum Fragments", None),
    # Not offered: Prime Evil parts share LoadDrops type 41 with Relics (the
    # plugin skips them while the relic gate rolls), and Satanic materials sit at
    # base 100,000-50,000,000, which no division reaches.
    # Ruby Keys: the game's own gate is already open (chances[18] = 1), only the
    # key's 1-in-1,500,000 roll needs scaling.
    ("ruby", "Ruby Keys", None),
]

DROPS = [
    ("gold", "Gold", ""),
    ("mining_ore", "Mining Ore Amount", ""),
]


def drop_multiplier(key, value) -> int:
    if key not in {k for k, *_ in DROPS}:
        raise ValueError("unknown drop setting")
    try:
        return max(1, min(10 if key == "mining_ore" else 100, int(float(value))))
    except (TypeError, ValueError, OverflowError):
        return 1


def drop_command(key, value) -> str:
    amount = drop_multiplier(key, value)
    return f"miningore {amount}" if key == "mining_ore" else f"dropmult {key} {amount}"

# Satanic Zone buff/debuff pool.  Names/ids/descriptions come from
# hs-game-sdk (hand-verified game knowledge, not mechanically extracted --
# see hs-game-sdk/curated/satanic_zone.json).  All on by default; deselecting
# one keeps the plugin from letting the game roll it into a future zone.
# Below the floor per column is refused client- and server-side.  Buffs and
# debuffs get different floors (user-set, 2026-09-10).
SATANIC_BUFF_LIST = [(m.id, m.name, m.description) for m in SATANIC_BUFFS]
SATANIC_DEBUFF_LIST = [(m.id, m.name, m.description) for m in SATANIC_DEBUFFS]
MIN_ENABLED_SATANIC_BUFFS = 3
MIN_ENABLED_SATANIC_DEBUFFS = 2

# Oyuncu istatistigi carpanlari.  Bunlar sabit bir taban deger yazmaz: oyunun
# hesapladigi guncel toplam YYToolkit tarafinda okunur ve DONUS degeri carpilir.
# Ucuncu alan panelde izin verilen guvenli/yararli ust sinir, dorduncu alan
# kaydiricinin adimidir.
STATS = [
    ("exp", "Experience", 100, 1),
    ("magicfind", "Magic Find", 100, 0.5),
    ("movespeed", "Movement Speed", 10, 0.5),
]

# Nihai sonuca yuzde ekleyen hassas ayarlar.  Panel yuzdeyi saklar; plugine
# 1 + yuzde/100 carpani gider (+25% -> 1.25, +100% -> 2.0).
PERCENT_STATS = [
    ("damage", "Total Damage", 1000, 5, "multiply"),
    ("attackspeed", "Attack Speed", 500, 5, "multiply"),
    ("castrate", "Faster Cast Rate", 500, 5, "add"),
    ("lifereplenish", "Life Replenish", 1000, 5, "multiply"),
    ("manareplenish", "Mana Replenish", 1000, 5, "multiply"),
    ("defense", "Defense", 1000, 5, "multiply"),
    ("critdamage", "Critical Strike Damage", 1000, 5, "multiply"),
    ("critchance", "Critical Strike Chance", 500, 5, "multiply"),
    ("spellcritdamage", "Spell Critical Strike Damage", 1000, 5, "multiply"),
    ("spellcritchance", "Spell Critical Strike Chance", 500, 5, "multiply"),
]

# Rare item quality.  These do not add drops - they change how good a drop is
# allowed to be.  Third field is the slider ceiling.
#   heroic  : the game's own Heroic chance (vanilla 28% per drop)
#   ceiling : the shared roll every rare ladder uses; raising it lifts Heroic,
#             Satanic and the normal rarity ladder all at once
#   satanic : the Satanic tier is chosen by monster level, so we let low-level
#             monsters count as higher level (capped at the game's own top row)
DEFAULTS = {
    "game_exe": DEFAULT_EXE,
    "density": 1,
    "density_on": False,
    "auto_apply": True,
    "map_reveal": False,
    # Sub-toggle of map_reveal.  Only meaningful while map_reveal is on. It
    # marks every pack's spot on the map (one icon per unspawned spawner,
    # no monster created); on by default because that is what revealing a
    # map is expected to show.
    "map_reveal_packs": True,
    # Second sub-toggle of map_reveal: the old "fill the map" pass that really
    # spawns every pack on arrival. Off by default - the living monsters are
    # what costs the game frame time at high density
    # (docs/population-performance-analysis.md).
    "map_reveal_spawn": False,
    "headhunter": False,
    "tyrant": False,
    "beacon": False,
    "mod_filter_max_relics": False,
    "mod_orb_pickup_radius": False,
    # Pet collects quest items on screen without hovering + pressing interact.
    # Scaffolding only as of 2026-09-10: the toggle/tick exist and count
    # candidates, but the actual collect call is pending live research (see
    # ForgePact/docs/pet-quest-collector-plan.md). Off by default like the
    # other mod toggles.
    "mod_pet_quest_pickup": False,
    # Auto-prospect (ForgePact #9): every item put into the Prospect Cube's
    # grid is prospected at once by the game's own Prospect. Off by default:
    # whatever is left in the grid when the game saves is lost.
    "mod_auto_prospect": False,
    # Its sub-option (Stage C): before each prospect, the previous prospect's
    # materials go from the grid to the materials tab, so only the newest
    # batch sits in the grid. On by default under the off-by-default parent;
    # the plugin defaults it on too, so only "off" is ever sent.
    "mod_auto_prospect_bag": True,
    # Marks the skill-bar slot of a toggle skill while it is switched on
    # (issue #11, Track B; covers every row in kToggleSkillRows). Off by
    # default like the other mod toggles; offline only, no co-op claim
    # (AGENTS.md "this is the rule of ForgePact").
    "mod_toggle_indicator": False,
    # Stops the double-cast proc from re-casting a covered toggle skill on its
    # own (issue #11, Track A), so a proc no longer flips the toggle straight
    # back. Off by default; offline only, like every mod here.
    "mod_toggle_guard": False,
    # Lets the pause menu's Restart work in combat (issue #8) instead of
    # waiting until the game has counted the player out of combat. Off by
    # default; offline only, like every mod here.
    "mod_restart_anytime": False,
    # Crafting from the stash (issue #14): a Crafting Cube recipe also counts
    # the stash's Materials and Socketable tabs, and at the craft only what the
    # bag is short of moves over. Off by default; offline only, like every mod
    # here.
    "mod_craft_mats": False,
    # Timed-skill countdown (issue #55): one of off/arc/bar/number/fade drawn
    # over each timed skill's hotbar slot. Covers the explicit rows of the
    # plugin's kSkillTimerRows, each measured in-game - a toggled-on skill
    # never gets a countdown. Off by default; a cast already running when you
    # turn it on shows as full (route B's latch takes the first reading it
    # sees).
    "mod_skill_timer_style": "off",
    # Monster Rarity: the share of normal monsters raised to Rare and to Ancient
    # (percent each, together at most 100; the rest stay normal).
    "rarity_rare": 0,
    "rarity_ancient": 0,
    # Angelic / Unholy drops: 1 = off, 2 = one die per kill at the Angelic Key's own
    # rate (1 in 7,500), every step above adds a die.
    "angelic_items": 1,
    # Enemy movement speed bonus in percent (0 = vanilla) and its scope.
    "enemy_speed": 0,
    "enemy_speed_ct": True,
    "spawners": {k: 1 for k, *_ in SPAWNERS},
    "drops": {k: 1 for k, *_ in DROPS},
    "keys": {k: 1 for k, *_ in KEYS},
    "stats": {k: 1 for k, *_ in STATS},
    "percent_stats": {k: 0 for k, *_ in PERCENT_STATS},
    # All mods enabled by default; a saved config only ever lists the ones a
    # user turned off, so a game update that adds new buff/debuff ids picks
    # up the "enabled" default automatically (see load_cfg's nested merge).
    "satanic_mods": {
        "buff": {str(i): True for i, *_ in SATANIC_BUFF_LIST},
        "debuff": {str(i): True for i, *_ in SATANIC_DEBUFF_LIST},
    },
}

_lock = threading.Lock()


def load_cfg() -> dict:
    cfg = dict(DEFAULTS)
    if CONFIG.exists():
        try:
            saved = json.loads(CONFIG.read_text(encoding="utf-8"))
            for k, v in saved.items():
                if k in ("spawners", "drops", "keys", "stats", "percent_stats"):
                    cfg[k] = {**cfg[k], **v}
                elif k == "satanic_mods" and isinstance(v, dict):
                    # Nested: merge each polarity's id->bool dict on its own,
                    # so ids missing from an older saved file (a mod added by
                    # a later update) still default to enabled rather than
                    # being dropped by a flat overwrite.
                    cfg[k] = {
                        "buff": {**cfg[k]["buff"], **v.get("buff", {})},
                        "debuff": {**cfg[k]["debuff"], **v.get("debuff", {})},
                    }
                else:
                    cfg[k] = v
        except Exception:
            pass
    for key in DISABLED_SPAWNER_KEYS:
        cfg.get("spawners", {}).pop(key, None)
    return cfg


def save_cfg(cfg: dict):
    CONFIG.write_text(json.dumps(cfg, indent=1), encoding="utf-8")


def exe_path(cfg=None) -> Path:
    cfg = cfg or load_cfg()
    return Path(cfg.get("game_exe") or DEFAULT_EXE)


def ipc_dir(cfg=None) -> Path:
    return exe_path(cfg).parent / "bp_ipc"


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


# --- Win32 process enumeration, set up exactly once -------------------------
# REPORTED 2026-09-16 (ForgePact PR #22 review): these prototypes used to be
# built INSIDE snapshot_processes() on every call - a fresh PROCESSENTRY32W
# class each time, with argtypes assigned onto the SHARED
# ctypes.windll.kernel32 function objects. Two threads doing that concurrently
# corrupt each other. Measured by the reviewer against the real functions, no
# mocks: 159 of 160 concurrent scans raised
#     argument 2: TypeError: expected LP_PROCESSENTRY32W instance instead of
#     pointer to PROCESSENTRY32W
# and - the part that matters - with only TWO workers, 39 of 40 running_paths()
# calls returned a FALSE NEGATIVE, against 5/5 correct sequentially.
#
# A false negative here is not a slow answer, it is a wrong one:
# running_paths() turns OSError into [], so game_running() reads False while
# the game is up. Commands are then queued instead of sent live, and the next
# successful scan can look like a brand-new launch and re-run auto-apply.
#
# The panel genuinely is concurrent here - watcher() polls every 5 s on its own
# daemon thread while /api/state is served on ThreadingHTTPServer handler
# threads - so this was reachable in normal use, not only under a stress test.
#
# Two properties, and the second matters as much as the first: define the
# structure and the prototypes ONCE, and hang them off a PRIVATE WinDLL handle.
# ctypes.windll is a process-wide cache, so assigning argtypes there mutates
# function objects any other library in this process may also be using.
#
# The launcher's processes() was checked against this and deliberately NOT
# changed: its PROCESSENTRY32W is module-level, so ctypes.POINTER(...) yields
# the same type object every call and its repeated argtypes assignment writes
# an identical value - idempotent, not a race. The defect here came from
# building a NEW class per call, which made the pointer type differ each time.
# The two copies are consistent in behaviour; only this one needed the fix.
if os.name == "nt":
    import ctypes as _ctypes
    from ctypes import wintypes as _wintypes

    class _PROCESSENTRY32W(_ctypes.Structure):
        _fields_ = [
            ("dwSize", _wintypes.DWORD), ("cntUsage", _wintypes.DWORD),
            ("th32ProcessID", _wintypes.DWORD),
            ("th32DefaultHeapID", _ctypes.POINTER(_ctypes.c_ulong)),
            ("th32ModuleID", _wintypes.DWORD), ("cntThreads", _wintypes.DWORD),
            ("th32ParentProcessID", _wintypes.DWORD), ("pcPriClassBase", _ctypes.c_long),
            ("dwFlags", _wintypes.DWORD), ("szExeFile", _wintypes.WCHAR * 260),
        ]

    _K32 = _ctypes.WinDLL("kernel32", use_last_error=True)
    _K32.CreateToolhelp32Snapshot.argtypes = [_wintypes.DWORD, _wintypes.DWORD]
    _K32.CreateToolhelp32Snapshot.restype = _wintypes.HANDLE
    _K32.Process32FirstW.argtypes = [_wintypes.HANDLE, _ctypes.POINTER(_PROCESSENTRY32W)]
    _K32.Process32FirstW.restype = _wintypes.BOOL
    _K32.Process32NextW.argtypes = [_wintypes.HANDLE, _ctypes.POINTER(_PROCESSENTRY32W)]
    _K32.Process32NextW.restype = _wintypes.BOOL
    _K32.CloseHandle.argtypes = [_wintypes.HANDLE]
    _K32.CloseHandle.restype = _wintypes.BOOL
    _K32.OpenProcess.argtypes = [_wintypes.DWORD, _wintypes.BOOL, _wintypes.DWORD]
    _K32.OpenProcess.restype = _wintypes.HANDLE
    _K32.QueryFullProcessImageNameW.argtypes = [
        _wintypes.HANDLE, _wintypes.DWORD, _wintypes.LPWSTR,
        _ctypes.POINTER(_wintypes.DWORD)]
    _K32.QueryFullProcessImageNameW.restype = _wintypes.BOOL
    _INVALID_HANDLE_VALUE = _wintypes.HANDLE(-1).value
else:                                   # pragma: no cover - non-Windows host
    _ctypes = None
    _PROCESSENTRY32W = None
    _K32 = None
    _INVALID_HANDLE_VALUE = None


def snapshot_processes() -> list:
    """(pid, image name) for every running process, without spawning one.

    Deliberately a second copy of
    HS-Offline-Launcher/src/hs_offline_launcher.py's ``processes()``: that
    application is a standalone single-file tool with no hs_game_sdk
    dependency, and routing a Win32 helper through the SDK would change its
    packaging for no functional gain.  Keep the two in step.

    The old panel implementation spawned a console process-listing tool on
    every poll, and wait_for_plugin_ready() calls game_running() every 0.25 s
    for up to 60 s - up to ~240 short-lived processes during game startup, at
    the most latency-sensitive moment there is.

    Raises OSError when the snapshot cannot be created or read; returns [] on
    a non-Windows host.
    """
    if os.name != "nt":
        return []
    # Nothing is defined or re-prototyped here: _K32 and _PROCESSENTRY32W are
    # module-level and configured once, so overlapping callers cannot corrupt
    # each other's argtypes. Only per-call state - the snapshot handle and one
    # entry buffer - is local, which is what makes this re-entrant.
    snapshot = _K32.CreateToolhelp32Snapshot(0x00000002, 0)   # TH32CS_SNAPPROCESS
    if snapshot in (None, _INVALID_HANDLE_VALUE):
        raise OSError("Windows process snapshot could not be created")
    rows = []
    entry = _PROCESSENTRY32W()
    entry.dwSize = _ctypes.sizeof(entry)
    try:
        ok = _K32.Process32FirstW(snapshot, _ctypes.byref(entry))
        if not ok:
            raise OSError("Windows process snapshot could not be read")
        while ok:
            rows.append((int(entry.th32ProcessID), entry.szExeFile))
            ok = _K32.Process32NextW(snapshot, _ctypes.byref(entry))
    finally:
        _K32.CloseHandle(snapshot)
    return rows


def process_image_path(pid: int) -> str:
    """The full image path of one PID, or "" when it cannot be opened."""
    if os.name != "nt":
        return ""
    # Same reason as snapshot_processes(): the prototypes live at module scope,
    # so this never writes to a shared function object while another thread is
    # inside a call on it.
    PROCESS_QUERY_LIMITED_INFORMATION = 0x1000
    h = _K32.OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, False, pid)
    if not h:
        return ""
    try:
        buf = _ctypes.create_unicode_buffer(32768)
        boyut = _wintypes.DWORD(32768)
        if _K32.QueryFullProcessImageNameW(h, 0, buf, _ctypes.byref(boyut)):
            return buf.value
    finally:
        _K32.CloseHandle(h)
    return ""


def running_paths(name: str) -> list:
    """FULL PATHS of the running processes named `name`.

    Matching on the name alone is not enough: a player's Steam copy and their
    offline copy can be open at the same time.  The old name-only version
    treated a Hero_Siege.exe in a different folder as "the game is running" and
    atliyordu.

    NOT cached, on purpose: game_running() decides whether a command goes out
    live or is queued into cmd.txt, so a stale "running" sends a command to a
    dead game and a stale "not running" silently queues one the player
    expected to apply now.  The fix for the per-poll cost is the cheaper
    enumeration above, not a memoised answer.

    A failed snapshot yields [] - i.e. "the game is not running" - which makes
    the panel queue instead of sending.  That is the pre-existing behaviour and
    it is harmless; the launcher's fail-closed posture belongs to the launcher,
    where an empty list gates a safety decision.
    """
    yollar = []
    try:
        wanted = name.lower()
        for pid, image in snapshot_processes():
            if image.lower() != wanted:
                continue
            path = process_image_path(pid)
            if path:
                yollar.append(path)
    except Exception:
        pass
    return yollar


def other_copy_running(cfg=None) -> str:
    """Return the path of a running Hero_Siege.exe OUTSIDE the configured copy."""
    target = exe_path(cfg)
    try:
        target_lower = str(target.resolve()).lower()
    except Exception:
        target_lower = str(target).lower()
    for p in running_paths(target.name):
        if p.lower() != target_lower:
            return p
    return ""


def game_running(cfg=None) -> bool:
    """True ONLY when the configured copy is the one running."""
    target = exe_path(cfg)
    try:
        target_lower = str(target.resolve()).lower()
    except Exception:
        target_lower = str(target).lower()
    return any(p.lower() == target_lower for p in running_paths(target.name))


def send_cmds(lines: list, cfg=None) -> str:
    """Append commands to cmd.txt (the plugin reads it every frame; if the game is
    closed they are processed on startup)."""
    d = ipc_dir(cfg)
    if not d.exists():
        exe = exe_path(cfg)
        if exe.parent.exists():
            try:
                d.mkdir(parents=True, exist_ok=True)
            except Exception:
                return "ERROR: bp_ipc folder not found next to the game exe (is the mod plugin installed?)"
        else:
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


def build_key_cmds(settings: dict, include_resets: bool = False) -> list:
    """Build only the key/drop-rate command group.

    Fresh processes need sparse commands.  A live slider change needs explicit
    x1/off resets because the current process may already contain modified repo
    values and an installed LoadDrops hook.
    """
    out = []
    gated = [(k, drop_type, int(settings.get(k, 1))) for k, _l, drop_type in KEYS
             if drop_type and int(settings.get(k, 1)) > 1]
    if include_resets:
        for _k, _l, drop_type in KEYS:
            if drop_type:
                out.append(f"dungeonkey del {drop_type}")
    for k, drop_type, multiplier in gated:
        # The gate only decides whether the family's own die is rolled at all
        # where the game would never roll it.  It opens at the monster's
        # normal-key chance (x1); the slider scales the item's own vanilla roll
        # below, so x2 is twice vanilla rather than (gate x2) * (roll x2) - the
        # x50 flood of 2026-09-06 was that square.  Relic keeps its own squared
        # curve (plugin g_DkTipOlcek): its vanilla rate away from the home zone
        # is zero, so there is no "twice" to keep to.
        gate = multiplier if k == "relic" else 1
        out.append(f"dungeonkey add {drop_type} {gate}")
    if gated:
        out.append("dungeonkey chance auto")
        out.append("dungeonkey on")
    elif include_resets:
        out.append("dungeonkey off")
    for k, _l, _drop_type in KEYS:
        value = max(1, int(settings.get(k, 1)))
        if value > 1 or include_resets:
            out.append(f"droprate group {k} {value}")
    return out


def _pct(value, default=0) -> int:
    try:
        return max(0, min(100, int(round(float(value)))))
    except (TypeError, ValueError):
        return default


ANGELIC_BASE_ONE_IN = 7500   # the Angelic Key's own drop rate, one die per kill at x2
ANGELIC_MAX = 100


def angelic_mult(value) -> int:
    """Clamp the Angelic / Unholy slider to 1 (off) .. ANGELIC_MAX."""
    try:
        v = int(round(float(value)))
    except (TypeError, ValueError):
        return 1
    return max(1, min(ANGELIC_MAX, v))


def angelic_one_in(mult: int) -> int:
    """x2 = 1 in 7,500 kills, x3 = 1 in 3,750 ... (mult - 1 dice per kill)."""
    dice = max(0, angelic_mult(mult) - 1)
    return 0 if dice == 0 else max(1, int(round(ANGELIC_BASE_ONE_IN / dice)))


def angelic_cmd(cfg: dict) -> str:
    one_in = angelic_one_in(cfg.get("angelic_items", 1))
    return f"angelicdrop {one_in}" if one_in > 0 else "angelicdrop off"


def rarity_setting(cfg: dict):
    """(rare, ancient) shares of the Monster Rarity sliders, in percent of the
    normal monsters.  Ancient is honoured first; Rare is cut so the two never
    exceed 100 together."""
    ancient = _pct(cfg.get("rarity_ancient", 0))
    rare = min(_pct(cfg.get("rarity_rare", 0)), 100 - ancient)
    return rare, ancient


def rarity_cmd(cfg: dict) -> str:
    rare, ancient = rarity_setting(cfg)
    return f"rarity {rare} {ancient}" if rare > 0 or ancient > 0 else "rarity off"


ENEMY_SPEED_MAX = 300   # percent; x4 is where ranged sprinters stop being fair
ENEMY_SPEED_STEP = 5


def enemy_speed_pct(value) -> int:
    """Clamp an enemy speed bonus to the panel's 0..300 % range in 5 % steps."""
    try:
        pct = float(value)
    except (TypeError, ValueError):
        return 0
    if pct != pct:  # NaN
        return 0
    pct = max(0.0, min(float(ENEMY_SPEED_MAX), pct))
    return int(round(pct))   # whole percent; the slider itself moves in 5 % steps


def enemy_speed_cmd(cfg: dict) -> str:
    """Plugin command for the current enemy speed setting; x1 resets a live hook."""
    pct = enemy_speed_pct(cfg.get("enemy_speed", 0))
    scope = "ct" if cfg.get("enemy_speed_ct", True) else "all"
    return f"enemyspeed {1.0 + pct / 100.0:g} {scope}"


# Timed-skill countdown (issue #55): the four shipped looks plus off, the
# panel's own set - kept as its own tuple so build_cmds and /api/set share
# one validator rather than restating the list.
SKILL_TIMER_STYLES = ("off", "arc", "bar", "number", "fade")


def skill_timer_style_valid(value) -> bool:
    """Valid = exactly one of off|arc|bar|number|fade after trim+lower."""
    return isinstance(value, str) and value.strip().lower() in SKILL_TIMER_STYLES


def build_cmds(cfg: dict) -> list:
    d = min(5.0, float(cfg.get("density", 1))) if cfg.get("density_on") else 1.0
    # A new game process already starts at vanilla values.  Sending x1/Off
    # commands was not harmless: x1 stat commands installed pass-through hooks
    # and x1 drop-rate commands walked and rewrote hundreds of repository
    # entries.  Startup auto-apply now emits only features that are actually on.
    # Live slider changes still send their explicit reset command through
    # /api/set, so an enabled feature can be turned off in the current session.
    out = []
    if d > 1.0:
        out.append(f"density {d:g}")
    if cfg.get("map_reveal", False):
        out.append("reveal 1")
        # Only emitted to turn the pack markers OFF: the plugin defaults them
        # on, so the common case sends nothing extra (same rule as the rest of
        # this function - emit only what is actually needed).
        if not cfg.get("map_reveal_packs", True):
            out.append("reveal packs 0")
        # The old "fill the map" pass is opt-in now that the pack markers
        # exist: the plugin defaults it off, so it is only ever emitted to
        # turn it ON. (No version number here: the panel's version must
        # appear exactly once, on the __version__ line.)
        if cfg.get("map_reveal_spawn", False):
            out.append("reveal spawn 1")
    if cfg.get("headhunter", False):
        # Custom Forge Headhunter item: rare kills grant the monster's affixes as buffs.
        # "force" also covers the not-yet-finished equipped-belt check (see plugin notes).
        out.append("headhunter force")
    if cfg.get("tyrant", False):
        # Custom Forge Tyrant's Crown item: monsters near you rise to rare more often,
        # rares carry one more affix.  "force" stands in for the equipped-item check.
        out.append("tyrant force")
    if cfg.get("beacon", False):
        # Custom Forge Beacon amulet: every monster on the map hunts the player.
        out.append("beacon force")
    if cfg.get("mod_filter_max_relics", False):
        # Safe to send at launch: the plugin only ARMS the filter here and installs
        # the DropRelic hook once a player exists.  Withholding it used to mean the
        # toggle stayed on in the panel but did nothing after a game restart.
        out.append("relicfilter 1")
    if cfg.get("mod_orb_pickup_radius", False):
        out.append("orbpickup 10")
    if cfg.get("mod_pet_quest_pickup", False):
        # Safe to send at launch: no hook is installed, so unlike relicfilter
        # there is no arm/defer lifecycle to worry about.
        out.append("petquest 1")
    if cfg.get("mod_auto_prospect", False):
        # Safe to send at launch: like relicfilter, the plugin only ARMS the mod
        # here and installs its hook once the game has settled.
        out.append("autoprospect 1")
        # Only emitted to turn the move to the materials tab OFF: the plugin
        # defaults it on (the map_reveal_packs rule).
        if not cfg.get("mod_auto_prospect_bag", True):
            out.append("autoprospect bag 0")
    if cfg.get("mod_toggle_indicator", False):
        # Safe to send at launch: DrawHudBuffs is already hooked at init;
        # this only flips an atomic read at the top of the existing draw.
        out.append("toggleborder 1")
    if cfg.get("mod_toggle_guard", False):
        # Safe to send at launch, like relicfilter: `toggleguard 1` only arms
        # the guard, and the plugin installs its hook once a player exists.
        out.append("toggleguard 1")
    if cfg.get("mod_restart_anytime", False):
        # Safe to send at launch, like toggleguard: `restartanytime 1` only
        # arms it, and the plugin installs its hook once a player exists.
        out.append("restartanytime 1")
    if cfg.get("mod_craft_mats", False):
        # Safe to send at launch, like autoprospect: `craftmats 1` only turns
        # the switch on, and the plugin installs its hooks once the game has
        # settled.
        out.append("craftmats 1")
    skill_timer_style = str(cfg.get("mod_skill_timer_style", "off")).strip().lower()
    if skill_timer_style_valid(skill_timer_style) and skill_timer_style != "off":
        # Safe to send at launch, like toggleborder: the draw call already
        # runs in Hook_DrawHudBuffs; this only sets which style it uses. A
        # hand-edited invalid saved value falls through and emits nothing,
        # so the all-off startup list stays unchanged.
        out.append(f"skilltimer {skill_timer_style}")
    rare, ancient = rarity_setting(cfg)
    if rare > 0 or ancient > 0:
        out.append(f"rarity {rare} {ancient}")
    if angelic_one_in(cfg.get("angelic_items", 1)) > 0:
        out.append(angelic_cmd(cfg))
    if enemy_speed_pct(cfg.get("enemy_speed", 0)) > 0:
        # Enemies path-find toward you faster; "ct" keeps it to Chaos Tower.
        out.append(enemy_speed_cmd(cfg))
    for key, *_ in SPAWNERS:
        value = int(cfg['spawners'].get(key, 1))
        if value > 1:
            out.append(f"specialrate {key} {value}")
    for key, *_ in DROPS:
        value = drop_multiplier(key, cfg['drops'].get(key, 1))
        if value > 1:
            out.append(drop_command(key, value))
    for key, _label, ceiling, step in STATS:
        value = max(1.0, min(float(ceiling), float(cfg.get("stats", {}).get(key, 1))))
        value = round(value / step) * step
        if value > 1.0:
            out.append(f"stat {key} {value:g}")
    for key, _label, ceiling, step, mode in PERCENT_STATS:
        bonus = max(0.0, min(float(ceiling), float(cfg.get("percent_stats", {}).get(key, 0))))
        bonus = round(bonus / step) * step
        # A clean game launch does not need a hook for settings that are Off.
        # The live /api/set path still sends zero when a user turns an active
        # slider off, so the already-installed hook is reset in that session.
        if bonus <= 0:
            continue
        if mode == "add":
            out.append(f"statadd {key} {bonus:g}")
        else:
            out.append(f"stat {key} {1.0 + bonus / 100.0:g}")
    settings = cfg.get("keys", {})
    out.extend(build_key_cmds(settings, include_resets=False))
    for polarity in ("buff", "debuff"):
        pool = cfg.get("satanic_mods", {}).get(polarity, {})
        disabled = [k for k, v in pool.items() if not v]
        if disabled:
            out.append(f"satmods {polarity} {','.join(disabled)}")
    return out


# Mod file sources: the "modfiles" folder shipped next to ForgePact.
MODFILE_SOURCES = [
    Path(getattr(sys, "frozen", False) and Path(sys.executable).parent or Path(__file__).parent) / "modfiles",
    Path(__file__).resolve().parent.parent / "modfiles_shipped",
]
# HS Offline Tracker's live sensor (read-only Aurie module). Installed beside
# BloodPactPlugin.dll when it ships with ForgePact; optional.
TRACKER_SENSOR_DLL = "HSOfflineTrackerProducer.dll"
PLUGIN_SOURCES = [
    Path(getattr(sys, "frozen", False) and Path(sys.executable).parent or Path(__file__).parent) / "modfiles",
    Path(__file__).resolve().parent.parent / "modfiles_shipped",
]


def find_src(fname, sources):
    for s in sources:
        f = s / fname
        if f.exists():
            return f
    return None


def exe_is_patched(exe: Path) -> bool:
    try:
        with exe.open("rb") as handle:
            head = handle.read(4096)
        return b".aurie" in head
    except Exception:
        return False


def _sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def _pe_layout(path: Path) -> dict:
    """Parse the PE fields needed for exact Aurie/clean-base comparison."""
    file_size = path.stat().st_size
    with path.open("rb") as handle:
        dos = handle.read(64)
        if len(dos) != 64 or dos[:2] != b"MZ":
            raise ValueError(f"{path.name} is not a valid PE executable")
        pe_offset = struct.unpack_from("<I", dos, 0x3C)[0]
        if pe_offset < 64 or pe_offset + 24 > file_size:
            raise ValueError(f"{path.name} has an invalid PE header offset")
        handle.seek(pe_offset)
        if handle.read(4) != b"PE\0\0":
            raise ValueError(f"{path.name} has no PE signature")
        coff = handle.read(20)
        if len(coff) != 20:
            raise ValueError(f"{path.name} has a truncated COFF header")
        machine, section_count = struct.unpack_from("<HH", coff, 0)
        optional_size = struct.unpack_from("<H", coff, 16)[0]
        if not 1 <= section_count <= 96 or not 64 <= optional_size <= 4096:
            raise ValueError(f"{path.name} has invalid PE header sizes")
        optional = handle.read(optional_size)
        if len(optional) != optional_size:
            raise ValueError(f"{path.name} has a truncated optional header")
        optional_magic = struct.unpack_from("<H", optional, 0)[0]
        if optional_magic not in (0x10B, 0x20B):
            raise ValueError(f"{path.name} has an unsupported PE format")
        entry_point = struct.unpack_from("<I", optional, 16)[0]
        section_alignment = struct.unpack_from("<I", optional, 32)[0]
        size_of_image = struct.unpack_from("<I", optional, 56)[0]
        size_of_headers = struct.unpack_from("<I", optional, 60)[0]
        if section_alignment <= 0 or section_alignment & (section_alignment - 1):
            raise ValueError(f"{path.name} has invalid section alignment")
        section_table_offset = pe_offset + 24 + optional_size
        section_table_end = section_table_offset + section_count * 40
        if section_table_end > size_of_headers or size_of_headers > file_size:
            raise ValueError(f"{path.name} has an invalid PE section table")
        section_table = handle.read(section_count * 40)
        if len(section_table) != section_count * 40:
            raise ValueError(f"{path.name} has a truncated section table")

        sections = []
        for index in range(section_count):
            entry = section_table[index * 40:(index + 1) * 40]
            raw_name = entry[:8].rstrip(b"\0")
            (virtual_size, virtual_address, raw_size, raw_offset,
             relocations_offset, line_numbers_offset) = struct.unpack_from("<IIIIII", entry, 8)
            relocation_count, line_number_count = struct.unpack_from("<HH", entry, 32)
            characteristics = struct.unpack_from("<I", entry, 36)[0]
            if raw_size and raw_offset + raw_size > file_size:
                raise ValueError(f"{path.name} section {raw_name!r} is outside the file")
            sections.append({
                "name": raw_name,
                "virtual_size": virtual_size,
                "virtual_address": virtual_address,
                "raw_size": raw_size,
                "raw_offset": raw_offset,
                "relocations_offset": relocations_offset,
                "line_numbers_offset": line_numbers_offset,
                "relocation_count": relocation_count,
                "line_number_count": line_number_count,
                "characteristics": characteristics,
            })

        if not sections:
            raise ValueError(f"{path.name} has no PE sections")
        raw_end = max(
            [size_of_headers]
            + [section["raw_offset"] + section["raw_size"] for section in sections]
        )
        return {
            "file_size": file_size,
            "pe_offset": pe_offset,
            "file_header_offset": pe_offset + 4,
            "optional_header_offset": pe_offset + 24,
            "optional_size": optional_size,
            "section_table_offset": section_table_offset,
            "section_table_end": section_table_end,
            "machine": machine,
            "optional_magic": optional_magic,
            "number_of_sections": section_count,
            "entry_point": entry_point,
            "section_alignment": section_alignment,
            "size_of_image": size_of_image,
            "size_of_headers": size_of_headers,
            "raw_end": raw_end,
            "sections": sections,
        }


def _mapped_pe_export_u32(image: bytes, export_name: bytes) -> int | None:
    """Read a 32-bit exported data value from Aurie's mapped PE payload."""
    if len(image) < 64 or image[:2] != b"MZ":
        return None
    pe_offset = struct.unpack_from("<I", image, 0x3C)[0]
    if pe_offset < 64 or pe_offset + 24 > len(image):
        return None
    if image[pe_offset:pe_offset + 4] != b"PE\0\0":
        return None
    file_header_offset = pe_offset + 4
    optional_size = struct.unpack_from("<H", image, file_header_offset + 16)[0]
    optional_offset = file_header_offset + 20
    if optional_offset + optional_size > len(image) or optional_size < 104:
        return None
    magic = struct.unpack_from("<H", image, optional_offset)[0]
    data_directory_offset = 112 if magic == 0x20B else 96 if magic == 0x10B else 0
    if not data_directory_offset or data_directory_offset + 8 > optional_size:
        return None
    export_rva, export_size = struct.unpack_from(
        "<II", image, optional_offset + data_directory_offset
    )
    if export_size < 40 or export_rva + 40 > len(image):
        return None
    number_of_functions, number_of_names = struct.unpack_from(
        "<II", image, export_rva + 20
    )
    functions_rva, names_rva, ordinals_rva = struct.unpack_from(
        "<III", image, export_rva + 28
    )
    if (
        number_of_functions == 0
        or number_of_functions > 65_536
        or number_of_names > 65_536
        or functions_rva + number_of_functions * 4 > len(image)
        or names_rva + number_of_names * 4 > len(image)
        or ordinals_rva + number_of_names * 2 > len(image)
    ):
        return None
    for index in range(number_of_names):
        name_rva = struct.unpack_from("<I", image, names_rva + index * 4)[0]
        if name_rva >= len(image):
            return None
        name_end = image.find(b"\0", name_rva, min(len(image), name_rva + 256))
        if name_end < 0:
            return None
        if image[name_rva:name_end] != export_name:
            continue
        ordinal = struct.unpack_from("<H", image, ordinals_rva + index * 2)[0]
        if ordinal >= number_of_functions:
            return None
        value_rva = struct.unpack_from("<I", image, functions_rva + ordinal * 4)[0]
        if value_rva + 4 > len(image):
            return None
        return struct.unpack_from("<I", image, value_rva)[0]
    return None


def _same_aurie_base(exe: Path, backup: Path) -> bool:
    """Compare the complete reconstructed clean exe with its proposed backup."""
    try:
        if not exe_is_patched(exe) or exe_is_patched(backup):
            return False
        patched = _pe_layout(exe)
        clean = _pe_layout(backup)
        clean_sections = clean["sections"]
        patched_sections = patched["sections"]
        if any(section["name"] == b".aurie" for section in clean_sections):
            return False
        if len(patched_sections) != len(clean_sections) + 1:
            return False
        if any(section["name"] == b".aurie" for section in patched_sections[:-1]):
            return False
        aurie = patched_sections[-1]
        if aurie["name"] != b".aurie":
            return False
        if (
            patched["machine"] != clean["machine"]
            or patched["optional_magic"] != clean["optional_magic"]
            or patched["pe_offset"] != clean["pe_offset"]
            or patched["optional_size"] != clean["optional_size"]
            or patched["section_table_offset"] != clean["section_table_offset"]
        ):
            return False
        if clean["raw_end"] != clean["file_size"]:
            return False  # AuriePatcher does not preserve a pre-existing overlay.
        if aurie["raw_offset"] != clean["file_size"]:
            return False
        if patched["file_size"] != clean["file_size"] + aurie["raw_size"]:
            return False
        if patched["raw_end"] != patched["file_size"]:
            return False
        if (
            aurie["virtual_size"] <= 0
            or aurie["raw_size"] <= 0
            or aurie["raw_size"] > 16 * 1024 * 1024
            or aurie["virtual_size"] > aurie["raw_size"]
            or aurie["virtual_address"] != clean["size_of_image"]
            or aurie["characteristics"] != 0xE0000000
            or aurie["relocations_offset"] != 0
            or aurie["line_numbers_offset"] != 0
            or aurie["relocation_count"] != 0
            or aurie["line_number_count"] != 0
        ):
            return False
        expected_image_size = (
            aurie["virtual_address"]
            + aurie["virtual_size"]
            + clean["section_alignment"] - 1
        ) & ~(clean["section_alignment"] - 1)
        if patched["size_of_image"] != expected_image_size:
            return False
        if not (
            aurie["virtual_address"]
            <= patched["entry_point"]
            < aurie["virtual_address"] + aurie["virtual_size"]
        ):
            return False

        new_section_offset = (
            clean["section_table_offset"] + clean["number_of_sections"] * 40
        )
        header_end = new_section_offset + 40
        if (
            header_end > clean["size_of_headers"]
            or header_end > patched["size_of_headers"]
        ):
            return False
        with exe.open("rb") as patched_file, backup.open("rb") as clean_file:
            patched_file.seek(aurie["raw_offset"])
            aurie_image = patched_file.read(aurie["raw_size"])
            if len(aurie_image) != aurie["raw_size"]:
                return False
            if _mapped_pe_export_u32(aurie_image, b"g_OldOEP") != clean["entry_point"]:
                return False
            patched_file.seek(0)
            clean_file.seek(0)
            patched_header = bytearray(patched_file.read(header_end))
            clean_header = clean_file.read(header_end)
            if len(patched_header) != header_end or len(clean_header) != header_end:
                return False

            file_header_offset = clean["file_header_offset"]
            optional_header_offset = clean["optional_header_offset"]
            patched_header[file_header_offset + 2:file_header_offset + 4] = (
                clean_header[file_header_offset + 2:file_header_offset + 4]
            )
            patched_header[optional_header_offset + 16:optional_header_offset + 20] = (
                clean_header[optional_header_offset + 16:optional_header_offset + 20]
            )
            patched_header[optional_header_offset + 56:optional_header_offset + 60] = (
                clean_header[optional_header_offset + 56:optional_header_offset + 60]
            )
            patched_header[new_section_offset:header_end] = (
                clean_header[new_section_offset:header_end]
            )
            if bytes(patched_header) != clean_header:
                return False

            remaining = clean["file_size"] - header_end
            while remaining:
                size = min(4 * 1024 * 1024, remaining)
                patched_chunk = patched_file.read(size)
                clean_chunk = clean_file.read(size)
                if len(patched_chunk) != size or patched_chunk != clean_chunk:
                    return False
                remaining -= size
        return True
    except (OSError, ValueError, struct.error):
        return False


def _unique_sibling(path: Path, label: str) -> Path:
    stamp = time.strftime("%Y%m%d-%H%M%S")
    candidate = path.with_name(f"{path.name}.{label}-{stamp}")
    number = 2
    while candidate.exists():
        candidate = path.with_name(f"{path.name}.{label}-{stamp}-{number}")
        number += 1
    return candidate


def _stage_verified_copy(source: Path, destination: Path) -> tuple[Path, str]:
    """Copy beside destination and prove the copy/source did not change."""
    import shutil as _sh

    staged = _unique_sibling(destination, f"tmp-{os.getpid()}-{time.time_ns()}")
    try:
        source_before = _sha256_file(source)
        _sh.copy2(source, staged)
        source_after = _sha256_file(source)
        staged_hash = _sha256_file(staged)
        if source_before != source_after or staged_hash != source_after:
            raise RuntimeError(f"{source.name} changed while it was being copied")
        return staged, staged_hash
    except Exception:
        try:
            staged.unlink()
        except OSError:
            pass
        raise


def _atomic_verified_copy(source: Path, destination: Path) -> None:
    staged, expected_hash = _stage_verified_copy(source, destination)
    try:
        os.replace(staged, destination)
        if _sha256_file(destination) != expected_hash:
            raise RuntimeError(f"verification failed after replacing {destination.name}")
    finally:
        try:
            staged.unlink()
        except OSError:
            pass


def _prepare_backup_from_clean_exe(exe: Path, backup: Path) -> Path | None:
    """Create/refresh a clean backup; preserve a stale backup under a new name.

    Returns the archived stale-backup path, or ``None`` when no rotation was
    needed.  The current clean exe is authoritative after a game update.
    """
    if exe_is_patched(exe):
        raise ValueError("cannot create a clean backup from an Aurie-patched exe")
    layout = _pe_layout(exe)  # Refuse to back up a malformed/non-PE file.
    if any(section["name"] == b".aurie" for section in layout["sections"]):
        raise ValueError("cannot create a clean backup from an Aurie-patched exe")
    current_hash = _sha256_file(exe)
    if backup.exists() and not exe_is_patched(backup) and _sha256_file(backup) == current_hash:
        return None

    staged, expected_hash = _stage_verified_copy(exe, backup)
    archived = None
    try:
        if backup.exists():
            archived = _unique_sibling(backup, "stale")
            os.replace(backup, archived)
        try:
            os.replace(staged, backup)
        except Exception:
            if archived is not None and archived.exists() and not backup.exists():
                os.replace(archived, backup)
                archived = None
            raise
        if _sha256_file(backup) != expected_hash:
            failed = _unique_sibling(backup, "failed-refresh")
            os.replace(backup, failed)
            if archived is not None and archived.exists():
                os.replace(archived, backup)
                archived = None
            raise RuntimeError(f"verification failed after refreshing {backup.name}")
        return archived
    finally:
        try:
            staged.unlink()
        except OSError:
            pass


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
        "trackerSensor": (b / "mods" / "aurie" / TRACKER_SENSOR_DLL).exists(),
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
    # HS Offline Tracker's live sensor rides along when it ships with ForgePact.
    # It is a separate, read-only Aurie module; a package without it installs
    # exactly as before, so its absence is not an error.
    sensor = find_src(TRACKER_SENSOR_DLL, PLUGIN_SOURCES)
    missing = [n for n, f in (("AurieCore.dll", core), ("YYToolkit.dll", yytk),
                              ("BloodPactPlugin.dll", plug), ("AuriePatcher.exe", patcher)) if f is None]
    if missing:
        remedy = ("Extract the complete ForgePact release, including its modfiles folder."
                  if getattr(sys, "frozen", False) else
                  "Source checkout: run Prepare-Plugin.bat in the ForgePact folder once, "
                  "then click Install Mod Plugin again. It needs Python, Visual Studio C++ "
                  "Build Tools and the full toolkit checkout.")
        return {"err": "Plugin installation files missing: " + ", ".join(missing) + ". " + remedy}
    steps = []
    bak = exe.with_name(exe.name + ".aurie_backup")
    patched_before_install = exe_is_patched(exe)
    try:
        if patched_before_install:
            if not bak.exists():
                return {"err": f"the exe is already Aurie-patched but {bak.name} is missing. "
                               "Install stopped so Remove Plugin cannot become unsafe. Restore a clean "
                               "Hero_Siege.exe, then install again."}
            if exe_is_patched(bak):
                return {"err": f"{bak.name} is also Aurie-patched, so it is not a safe restore point. "
                               "Install stopped; restore a clean Hero_Siege.exe first."}
            if not _same_aurie_base(exe, bak):
                return {"err": f"{bak.name} belongs to a different Hero Siege build. Install stopped "
                               "rather than keeping a stale restore point. Restore/verify a clean game "
                               "exe, then install again."}
            steps.append("clean backup verified for this build")
        else:
            backup_existed = bak.exists()
            archived = _prepare_backup_from_clean_exe(exe, bak)
            if archived is not None:
                steps.append(f"stale backup archived as {archived.name}")
                steps.append("backup refreshed for the current build")
            elif backup_existed:
                steps.append("clean backup verified")
            else:
                steps.append("exe backed up")
    except Exception as e:
        return {"err": f"could not validate/prepare the clean exe backup: {e}"}

    _sh.copy2(core, b / "AurieCore.dll")
    (b / "mods" / "aurie").mkdir(parents=True, exist_ok=True)
    (b / "mods" / "native").mkdir(parents=True, exist_ok=True)
    _sh.copy2(yytk, b / "mods" / "aurie" / "YYToolkit.dll")
    _sh.copy2(plug, b / "mods" / "aurie" / "BloodPactPlugin.dll")
    steps.append("mod DLLs installed/updated")
    if sensor is not None:
        _sh.copy2(sensor, b / "mods" / "aurie" / TRACKER_SENSOR_DLL)
        steps.append("HS Offline Tracker sensor installed")
    if not patched_before_install:
        patch_error = ""
        patch_output = ""
        try:
            r = subprocess.run([str(patcher), str(exe), str(b / "AurieCore.dll"), "install"],
                               capture_output=True, text=True, encoding="utf-8", errors="replace", timeout=120,
                               creationflags=CREATE_NO_WINDOW)
            patch_output = (r.stdout or r.stderr or "?")[-200:]
            if r.returncode != 0:
                patch_error = f"AuriePatcher exited with code {r.returncode}: {patch_output}"
            elif not exe_is_patched(exe):
                patch_error = "AuriePatcher reported success but the .aurie section is missing: " + patch_output
            elif not _same_aurie_base(exe, bak):
                patch_error = "AuriePatcher changed original game sections unexpectedly"
        except Exception as e:
            patch_error = str(e)

        if patch_error:
            try:
                _atomic_verified_copy(bak, exe)
                rollback = " The clean exe was restored from the verified backup."
            except Exception as restore_error:
                rollback = (f" Automatic restore also failed ({restore_error}); the verified clean copy is "
                            f"still available as {bak.name}.")
            return {"err": "patching failed: " + patch_error + rollback}
        steps.append("exe patched and base build verified")
    else:
        steps.append("exe already patched")
    return {"ok": "MOD INSTALLED: " + ", ".join(steps) +
                  " - NOTE: only works on an EAC-free copy (online/EAC games will bounce back to the clean exe).",
            "chain": mod_chain(cfg)}


def op_remove_mod(cfg) -> dict:
    exe = exe_path(cfg)
    if not exe.exists():
        return {"err": "game exe not found - set Game Location first"}
    if game_running(cfg):
        return {"err": "Close the game first, then click Remove Plugin again."}
    steps = []
    bak = exe.with_name(exe.name + ".aurie_backup")
    patched = exe_is_patched(exe)
    try:
        if patched:
            if not bak.exists():
                return {"err": f"no backup found ({bak.name}). Remove stopped without touching the "
                               "patched exe; restore a clean Hero_Siege.exe manually."}
            if exe_is_patched(bak):
                return {"err": f"{bak.name} is Aurie-patched too. Remove stopped without touching the "
                               "current exe; restore a clean Hero_Siege.exe manually."}
            if not _same_aurie_base(exe, bak):
                return {"err": f"{bak.name} belongs to a different Hero Siege build. Remove stopped "
                               "instead of downgrading/replacing the current exe. Verify the game files "
                               "to obtain a clean exe first."}
            _atomic_verified_copy(bak, exe)
            steps.append("matching original exe restored from backup")
        else:
            backup_existed = bak.exists()
            archived = _prepare_backup_from_clean_exe(exe, bak)
            steps.append("exe was already original; it was left unchanged")
            if archived is not None:
                steps.append(f"stale backup archived as {archived.name}")
                steps.append("backup refreshed for the current build")
            elif backup_existed:
                steps.append("clean backup verified")
            else:
                steps.append("clean backup created")
    except Exception as e:
        return {"err": f"could not safely remove the mod: {e}"}
    if exe_is_patched(exe):
        return {"err": "restore ran but the exe still looks patched - check the .aurie_backup file."}
    b = exe.parent
    for rel in ("AurieCore.dll", "mods/aurie/YYToolkit.dll", "mods/aurie/BloodPactPlugin.dll",
                "mods/aurie/" + TRACKER_SENSOR_DLL):
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


def wait_for_plugin_ready(cfg: dict, timeout: float = 60.0) -> bool:
    """Wait for this game's plugin to consume a harmless ping command.

    Watching out.txt timestamps was not sufficient when the panel was opened
    after an already-running game: a healthy plugin could have an old log and
    the saved settings were never applied until a slider was changed.
    """
    deadline = time.time() + timeout
    command_sent = False
    command_file = ipc_dir(cfg) / "cmd.txt"
    while time.time() < deadline:
        if not game_running(cfg):
            return False
        if not command_sent and ipc_dir(cfg).exists():
            if send_cmds(["ping"], cfg).startswith("ERROR"):
                time.sleep(0.5)
                continue
            command_sent = True
        if command_sent and not command_file.exists():
            return True
        time.sleep(0.25)
    return False


BOOT_MARKER = b"BloodPact plugin loaded"
BOOT_SCAN_CHUNK = 1 << 16
# Bytes actually read by the last scans; the timing harness and the tests read
# it to prove the incremental path is incremental rather than just faster.
BOOT_SCAN_BYTES = 0
_BOOT_LOCK = threading.Lock()
# Bytes kept from just before the counted region, to prove the file that
# produced the stored offset is still the file being read.
_BOOT_ANCHOR_BYTES = 64
# Offset and count belong together: the offset is only meaningful as "how far
# the stored count has already counted".  They are therefore only ever written
# as a pair, under the lock, from one scan.
# `ident` is (st_dev, st_ino) and `anchor` the bytes ending at `offset`.
# Both exist to answer one question the size alone cannot: is this the same
# file, with the same already-counted content, that produced that offset?
_BOOT_CACHE = {"path": None, "offset": 0, "count": 0, "ident": None, "anchor": b"",
               "size": -1, "mtime_ns": -1}


def reset_boot_count_cache() -> None:
    """Forget the incremental scan state (tests, and anything that moves the log)."""
    with _BOOT_LOCK:
        _BOOT_CACHE.update(path=None, offset=0, count=0, ident=None, anchor=b"",
                           size=-1, mtime_ns=-1)


def plugin_mod_state(cfg=None) -> dict:
    r"""What the plugin says it is actually doing, from `bp_ipc\modstate.json`.

    The panel stores what the player asked for; the plugin can refuse it for
    the rest of a session (an insert hook that went in table-only, a move pass
    that shut itself down after a material could not be accounted for). Review
    of ForgePact #54: without this the switch kept showing ON and a refused
    re-enable looked like it had worked. Missing or unreadable file means the
    plugin has not said anything, which is not the same as a refusal."""
    try:
        raw = (ipc_dir(cfg) / "modstate.json").read_text(encoding="utf-8", errors="replace")
        state = json.loads(raw)
        return state if isinstance(state, dict) else {}
    except Exception:
        return {}


def plugin_boot_count(cfg=None) -> int:
    """How many times the plugin has started, read from its own log.

    The plugin appends one 'BloodPact plugin loaded' line per game start, so a
    change in this count identifies a NEW game process even when the game was
    closed and reopened between two 5-second polls (a boolean running flag
    misses that and the startup commands are never sent).

    out.txt is append-only and grows to several megabytes over a session, so
    the scan resumes from where the previous one stopped instead of re-reading
    and re-decoding the whole file every five seconds.  The result must stay
    EXACTLY equal to a full-file count in every case, including rotation,
    truncation and replacement - a boot silently dropped here is a game
    restart the watcher never notices, and the saved settings are then never
    re-applied.  So the resumed read overlaps the previous one by
    len(marker)-1 bytes (a marker can straddle two polls) and discards any
    match that already ended inside the counted region.
    """
    global BOOT_SCAN_BYTES
    try:
        path = ipc_dir(cfg) / "out.txt"
        key = str(path).lower()
        with _BOOT_LOCK:
            offset = _BOOT_CACHE["offset"]
            count = _BOOT_CACHE["count"]
            st = path.stat()
            ident = (st.st_dev, st.st_ino)
            # REPORTED 2026-09-16 (PR #22 review): testing only "different path
            # or smaller" let a REPLACEMENT log of the same or greater size keep
            # the old offset and count. Reproduced: two markers counted (2), the
            # file atomically replaced with a same-size log holding one marker,
            # and this still answered 2 where a full scan answers 1. That loses
            # the count CHANGE watcher() uses to notice a restart between polls,
            # so auto-apply silently stops - the exact failure this cache was
            # required not to cause.
            #
            # Identity catches a replace-by-rename (a new st_ino for the path).
            # Checking the head instead of the anchor would not do: every
            # out.txt opens with the same boot banner, so two different logs
            # agree there.
            #
            # REPORTED AGAIN 2026-09-16, and this is the sharper case: a rewrite
            # in place can preserve the anchor bytes while changing a marker
            # earlier in the file (`marker*2 + tail` -> `marker + spaces +
            # tail`). Identity, size and the trailing bytes all match, so the
            # cache stayed at 2 where a full count says 1. My own
            # truncate-then-regrow test missed it because the fixture was short
            # enough to sit entirely inside the anchor - an assertion that could
            # not fail in the direction it was testing.
            #
            # So the cache is now trusted only when the file looks like a strict
            # APPEND: it grew, or nothing was written at all. A write that did
            # not extend the file is a rewrite by definition, whatever the bytes
            # happen to look like.
            #
            # NOT CLOSED, and deliberately so: a rewrite that BOTH grows the
            # file AND leaves the anchor bytes intact still reads as an append.
            # No O(1) metadata check can separate that from a real append - the
            # only sound test is re-reading the counted prefix, which is the
            # exact cost this cache exists to avoid. The plugin only ever
            # appends to out.txt, so the remaining case needs an external writer
            # that grows the file while rewriting earlier markers. Recorded as
            # "not covered", not as "cannot happen".
            #
            # ALSO NOT COVERED (measured 2026-09-16): a same-size rewrite in
            # place that lands inside the same filesystem timestamp tick as the
            # previous write leaves st_mtime_ns unchanged, so it reads as
            # "nothing was written" and keeps the stale count. The rewrite test
            # hit this about 2% of the time locally and once in CI. Same
            # reasoning as above: only an external writer produces it.
            grew = st.st_size > _BOOT_CACHE["size"]
            touched = st.st_mtime_ns != _BOOT_CACHE["mtime_ns"]
            if (_BOOT_CACHE["path"] != key
                    or _BOOT_CACHE["ident"] != ident
                    or st.st_size < offset
                    or (touched and not grew)):
                offset, count = 0, 0
            overlap = min(offset, len(BOOT_MARKER) - 1)
            found = 0
            with path.open("rb") as fh:
                anchor_want = _BOOT_CACHE["anchor"]
                if offset and anchor_want:
                    fh.seek(offset - len(anchor_want))
                    if fh.read(len(anchor_want)) != anchor_want:
                        offset, count, overlap = 0, 0, 0
                position = offset - overlap
                fh.seek(position)
                carry = b""
                while True:
                    chunk = fh.read(BOOT_SCAN_CHUNK)
                    if not chunk:
                        break
                    BOOT_SCAN_BYTES += len(chunk)
                    buf = carry + chunk
                    base = position - len(carry)
                    at = 0
                    while True:
                        hit = buf.find(BOOT_MARKER, at)
                        if hit < 0:
                            break
                        # Every already-counted marker ends at or before the
                        # stored offset; one straddling it does not.
                        if base + hit + len(BOOT_MARKER) > offset:
                            found += 1
                        at = hit + len(BOOT_MARKER)
                    position += len(chunk)
                    carry = buf[-(len(BOOT_MARKER) - 1):]
            anchor = b""
            if position:
                fh_anchor = min(position, _BOOT_ANCHOR_BYTES)
                with path.open("rb") as fh2:
                    fh2.seek(position - fh_anchor)
                    anchor = fh2.read(fh_anchor)
            # Re-stat rather than reuse `st`: the file may have been appended
            # to while this scan was reading it, and storing the pre-read size
            # would make the next call see "grew" for bytes already counted.
            final = path.stat()
            _BOOT_CACHE.update(path=key, offset=position, count=count + found,
                               ident=ident, anchor=anchor,
                               size=final.st_size, mtime_ns=final.st_mtime_ns)
            return count + found
    except Exception:
        reset_boot_count_cache()
        return -1


def plugin_boot_generation(cfg=None):
    """(file identity, boot count) - what watcher() actually needs to notice a
    new game process.

    plugin_boot_count() alone is not enough once out.txt is rotated at plugin
    load (see ModManager::Initialize()'s rotation): a freshly rotated file
    always opens with exactly one boot banner, so its count can coincidentally
    equal the count the OLD file held right before it was renamed away, and a
    bare count comparison then reads e.g. 1 -> 1 and misses the restart. File
    identity (st_dev, st_ino) changes on every rotation - a moved-then-recreated
    out.txt is a new inode - so pairing it with the count catches that case.
    This reuses the identity plugin_boot_count() just computed (from its own
    cache, under the same lock) rather than re-stat'ing the file.
    """
    count = plugin_boot_count(cfg)
    with _BOOT_LOCK:
        return (_BOOT_CACHE["ident"], count)


def watcher():
    """Re-apply the settings automatically every time the game LAUNCHES."""
    # False is intentional: if the panel itself starts after the game, the
    # first pass must still attach and apply the saved configuration.
    was_running = False
    last_state = None
    while True:
        time.sleep(5)
        try:
            cfg = load_cfg()
            now = game_running(cfg)
            state = plugin_boot_generation(cfg)
            new_process = now and (not was_running or (last_state is not None and state != last_state))
            if new_process and cfg.get("auto_apply"):
                if wait_for_plugin_ready(cfg):
                    apply_all(cfg)
            if now:
                last_state = state
            was_running = now
        except Exception:
            pass


def launch_modded_game(cfg: dict) -> dict:
    """Use the embedded launcher with this panel's path, never a second config."""
    def validate_plugin() -> str:
        chain = mod_chain(cfg)
        if not all(chain.get(key) for key in ("patched", "aurieCore", "yytk", "plugin")):
            return "The mod plugin installation is incomplete. Close the game and click Install Mod Plugin first."
        return ""

    return offline_launcher.launch_game(exe_path(cfg), validate_extra=validate_plugin)


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
            self._json({"cfg": cfg, "version": __version__,
                        "gameRunning": game_running(cfg),
                        "ipcOk": ipc_dir(cfg).exists(),
                        "pluginMods": plugin_mod_state(cfg),
                        "eacStatus": eac_status(_exe) if _exe.exists() else "",
                        "chain": mod_chain(cfg),
                        "spawners": [[k, i, l, mx] for k, i, l, mx in SPAWNERS],
                        "drops": [[k, l, h] for k, l, h in DROPS],
                        "stats": [[k, l, mx, step] for k, l, mx, step in STATS],
                        "percentStats": [[k, l, mx, step, mode] for k, l, mx, step, mode in PERCENT_STATS],
                        # Third field is the drop type: the panel's explanation text
                        # differs per family because they do not all mean the same thing.
                        "keys": [[k, l, t] for k, l, t in KEYS],
                        "satanicBuffs": [[i, l, d] for i, l, d in SATANIC_BUFF_LIST],
                        "satanicDebuffs": [[i, l, d] for i, l, d in SATANIC_DEBUFF_LIST],
                        "minEnabledSatanicBuffs": MIN_ENABLED_SATANIC_BUFFS,
                        "minEnabledSatanicDebuffs": MIN_ENABLED_SATANIC_DEBUFFS,
                        "lastApplied": LAST["applied"], "queued": LAST["queued"],
                        "launch": offline_launcher.launch_status()})
        else:
            self._json({"err": "not found"}, 404)

    def do_POST(self):
        n = int(self.headers.get("Content-Length", 0))
        body = json.loads(self.rfile.read(n) or b"{}")
        u = urlparse(self.path)
        try:
            cfg = load_cfg()
            if u.path == "/api/set":
                # key is optional only for the satanic_mods bulk form (body["keys"]
                # instead of a single body["key"]) - every other section still
                # requires it and gets a KeyError same as before if it's missing.
                sec, key, val = body.get("section"), body.get("key"), body["value"]
                if sec == "keys":
                    cfg[sec][key] = max(1, min(100, int(val)))
                elif sec == "stats":
                    # The slider moves in steps; a typed value is kept as typed (2 decimals).
                    ceiling = next((mx for k, _l, mx, _st in STATS if k == key), 100)
                    value = round(max(1.0, min(float(ceiling), float(val))), 2)
                    cfg.setdefault("stats", {})[key] = int(value) if value.is_integer() else value
                elif sec == "percent_stats":
                    ceiling = next((mx for k, _l, mx, _st, _mode in PERCENT_STATS if k == key), 1000)
                    value = round(max(0.0, min(float(ceiling), float(val))), 2)
                    cfg.setdefault("percent_stats", {})[key] = int(value) if value.is_integer() else value
                elif sec == "drops":
                    if key not in {k for k, *_ in DROPS}:
                        self._json({"err": "unknown drop setting"}, 400); return
                    cfg[sec][key] = drop_multiplier(key, val)
                elif sec == "spawners":
                    allowed = {k for k, _i, _l, _mx in SPAWNERS}
                    if key not in allowed:
                        self._json({"err": "special content is unavailable"}, 400)
                        return
                    ceiling = next((mx for k, _i, _l, mx in SPAWNERS if k == key), 100)
                    cfg[sec][key] = max(1, min(ceiling, int(val)))
                elif sec == "satanic_mods":
                    polarity = body.get("polarity")
                    if polarity not in ("buff", "debuff"):
                        self._json({"err": "polarity must be buff or debuff"}, 400); return
                    pool = cfg["satanic_mods"][polarity]
                    floor = MIN_ENABLED_SATANIC_BUFFS if polarity == "buff" else MIN_ENABLED_SATANIC_DEBUFFS
                    new_val = bool(val)
                    keys_bulk = body.get("keys")
                    if keys_bulk is not None:
                        # Select All / Deselect All: one request for the whole column
                        # instead of one per row, so it applies (and sends its one
                        # live "satmods" command) instantly rather than row-by-row.
                        ids = [str(k) for k in keys_bulk if str(k) in pool]
                        if not new_val:
                            # Never drop below the floor - silently keep enough of
                            # the requested ids enabled rather than erroring, since
                            # this is a "turn off everything you can" bulk action.
                            # Only ids that are CURRENTLY enabled count against the
                            # budget - one already off costs nothing to "re-disable".
                            enabled_ids = [k for k in ids if pool[k]]
                            allowed = max(0, len(enabled_ids) - floor)
                            ids = enabled_ids[:allowed]
                        for k in ids:
                            pool[k] = new_val
                    else:
                        if key not in pool:
                            self._json({"err": "unknown satanic mod id"}, 400); return
                        if pool[key] and not new_val:
                            # Deselecting one: refuse if it would drop this polarity's
                            # enabled count below its floor (buffs and debuffs differ).
                            enabled_after = sum(1 for v in pool.values() if v) - 1
                            if enabled_after < floor:
                                self._json({"err": f"at least {floor} {polarity}s must stay enabled"}, 400)
                                return
                        pool[key] = new_val
                elif key == "density":
                    # 0.5 steps: 1, 1.5, 2 ...  Whole numbers are stored as
                    # float("3") -> 3.0; the plugin prints with %g so it shows as "x3".
                    d = round(max(1.0, min(5.0, float(val))), 2)   # slider steps 0.5, typed values stay
                    cfg["density"] = int(d) if float(d).is_integer() else d
                elif key == "angelic_items":
                    cfg["angelic_items"] = angelic_mult(val)
                elif key == "enemy_speed":
                    cfg["enemy_speed"] = enemy_speed_pct(val)
                elif key == "enemy_speed_ct":
                    cfg["enemy_speed_ct"] = bool(val)
                elif key in ("rarity_rare", "rarity_ancient"):
                    cfg[key] = _pct(val)
                    # the two shares never exceed 100 together; the one just
                    # moved wins and the other gives way
                    other = "rarity_ancient" if key == "rarity_rare" else "rarity_rare"
                    cfg[other] = min(_pct(cfg.get(other, 0)), 100 - cfg[key])
                elif key in ("density_on", "auto_apply", "map_reveal", "map_reveal_packs", "map_reveal_spawn", "headhunter", "tyrant", "beacon", "mod_filter_max_relics", "mod_orb_pickup_radius", "mod_pet_quest_pickup", "mod_auto_prospect", "mod_auto_prospect_bag", "mod_toggle_indicator", "mod_toggle_guard", "mod_restart_anytime", "mod_craft_mats"):
                    cfg[key] = bool(val)
                elif key == "mod_skill_timer_style":
                    style = str(val).strip().lower()
                    if not skill_timer_style_valid(style):
                        self._json({"err": "invalid skilltimer style"}, 400)
                        return
                    cfg[key] = style
                save_cfg(cfg)
                live = ""
                if game_running(cfg):
                    if sec == "keys":
                        # A live change must also restore families moved back to
                        # x1; startup's sparse command list deliberately cannot.
                        send_cmds(build_key_cmds(cfg.get("keys", {}), include_resets=True), cfg)
                    elif sec == "drops":
                        send_cmds([drop_command(key, cfg[sec][key])], cfg)
                    elif sec == "stats":
                        send_cmds([f"stat {key} {float(cfg['stats'][key]):g}"], cfg)
                    elif sec == "percent_stats":
                        mode = next((md for k, _l, _mx, _st, md in PERCENT_STATS if k == key), "multiply")
                        bonus = float(cfg["percent_stats"][key])
                        command = f"statadd {key} {bonus:g}" if mode == "add" else f"stat {key} {1.0 + bonus / 100.0:g}"
                        send_cmds([command], cfg)
                    elif sec == "spawners":
                        send_cmds([f"specialrate {key} {int(val)}"], cfg)
                    elif sec == "satanic_mods":
                        polarity = body.get("polarity")
                        disabled_csv = ",".join(k for k, v in cfg["satanic_mods"][polarity].items() if not v)
                        send_cmds([f"satmods {polarity} {disabled_csv}"], cfg)
                    elif key in ("density", "density_on"):
                        send_cmds([f"density {cfg['density'] if cfg['density_on'] else 1}"], cfg)
                    elif key == "map_reveal":
                        cmds = [f"reveal {1 if cfg['map_reveal'] else 0}"]
                        # Turning the parent back on has to restate the child:
                        # `reveal 1` does not reset the plugin's pack flag, so
                        # without this a player who turned packs off, toggled
                        # the parent, and came back would silently get them on.
                        if cfg["map_reveal"]:
                            cmds.append(f"reveal packs {1 if cfg.get('map_reveal_packs', True) else 0}")
                            cmds.append(f"reveal spawn {1 if cfg.get('map_reveal_spawn', False) else 0}")
                        send_cmds(cmds, cfg)
                    elif key == "map_reveal_packs":
                        send_cmds([f"reveal packs {1 if cfg['map_reveal_packs'] else 0}"], cfg)
                    elif key == "map_reveal_spawn":
                        send_cmds([f"reveal spawn {1 if cfg['map_reveal_spawn'] else 0}"], cfg)
                    elif key == "headhunter":
                        send_cmds(["headhunter force" if cfg["headhunter"] else "headhunter off"], cfg)
                    elif key == "tyrant":
                        send_cmds(["tyrant force" if cfg["tyrant"] else "tyrant off"], cfg)
                    elif key == "beacon":
                        send_cmds(["beacon force" if cfg["beacon"] else "beacon off"], cfg)
                    elif key == "mod_filter_max_relics":
                        send_cmds([f"relicfilter {1 if cfg['mod_filter_max_relics'] else 0}"], cfg)
                    elif key == "mod_orb_pickup_radius":
                        send_cmds([f"orbpickup {10 if cfg['mod_orb_pickup_radius'] else 0}"], cfg)
                    elif key == "mod_pet_quest_pickup":
                        send_cmds([f"petquest {1 if cfg['mod_pet_quest_pickup'] else 0}"], cfg)
                    elif key == "mod_auto_prospect":
                        cmds = [f"autoprospect {1 if cfg['mod_auto_prospect'] else 0}"]
                        # Turning the parent on restates the child, as map
                        # reveal does: `autoprospect 1` leaves the plugin's
                        # bag flag as it was.
                        if cfg["mod_auto_prospect"]:
                            cmds.append(f"autoprospect bag {1 if cfg.get('mod_auto_prospect_bag', True) else 0}")
                        send_cmds(cmds, cfg)
                    elif key == "mod_auto_prospect_bag":
                        send_cmds([f"autoprospect bag {1 if cfg['mod_auto_prospect_bag'] else 0}"], cfg)
                    elif key == "mod_toggle_indicator":
                        send_cmds([f"toggleborder {1 if cfg['mod_toggle_indicator'] else 0}"], cfg)
                    elif key == "mod_toggle_guard":
                        send_cmds([f"toggleguard {1 if cfg['mod_toggle_guard'] else 0}"], cfg)
                    elif key == "mod_restart_anytime":
                        send_cmds([f"restartanytime {1 if cfg['mod_restart_anytime'] else 0}"], cfg)
                    elif key == "mod_craft_mats":
                        send_cmds([f"craftmats {1 if cfg['mod_craft_mats'] else 0}"], cfg)
                    elif key == "mod_skill_timer_style":
                        # Always explicit, including off: a style change (or
                        # turning it off) needs the plugin told either way.
                        send_cmds([f"skilltimer {cfg['mod_skill_timer_style']}"], cfg)
                    elif key in ("rarity_rare", "rarity_ancient"):
                        # Always explicit: "rarity off" returns a live hook to vanilla.
                        send_cmds([rarity_cmd(cfg)], cfg)
                    elif key == "angelic_items":
                        send_cmds([angelic_cmd(cfg)], cfg)
                    elif key in ("enemy_speed", "enemy_speed_ct"):
                        # Always explicit: "enemyspeed 1 ct" turns a live hook back to vanilla.
                        send_cmds([enemy_speed_cmd(cfg)], cfg)
                    live = " (commands sent to the plugin)"
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
                self._json(launch_modded_game(cfg))
            else:
                self._json({"err": "not found"}, 404)
        except Exception as e:
            self._json({"err": f"error: {e}"}, 500)


# ---- adaptive poll policy -------------------------------------------------
# Shared, deliberately verbatim, with
# HS-Offline-Launcher/src/hs_offline_launcher.py: same function, same three
# constant names, same values.  A fixed interval forces a trade nobody wins -
# fast costs poll work for the whole session, slow costs feedback latency at
# exactly the moments somebody is watching.  The trade only exists because the
# interval is fixed, and both clients can already tell when a change is
# plausible: the user just moved a control, or the payload they just received
# differs from the previous one.
#
# Known gap, documented rather than special-cased: a window OCCLUDED by a
# fullscreen game is not necessarily document.hidden, so it idles (one poll
# per 30 s) instead of suspending.
#
# Kept as a named constant instead of being buried in an inline arrow so the
# tests can assert its structure always and execute it through node when one
# is installed.
POLL_WATCHED_FIELDS = ["gameRunning", "ipcOk", "lastApplied", "queued"]

POLL_POLICY_JS = r"""
const POLL_FAST_MS = 2000;          // something just happened; the user is watching
const POLL_IDLE_MS = 30000;         // nothing has changed for a while
const POLL_FAST_WINDOW_MS = 15000;  // how long "just happened" lasts
const POLL_WATCHED_FIELDS = __POLL_WATCHED_FIELDS__;
// null means: do not schedule a poll at all.
function pollDelayMs(hidden, msSinceChange){
  if(hidden) return null;
  return msSinceChange < POLL_FAST_WINDOW_MS ? POLL_FAST_MS : POLL_IDLE_MS;
}
// Watched fields are dotted paths so a nested one (game.build) reads the same
// way as a flat one.
function pollFieldValue(payload, field){
  let cur = payload;
  for(const part of field.split('.')){
    if(cur === null || cur === undefined) return undefined;
    cur = cur[part];
  }
  return cur;
}
function pollPayloadChanged(prev, next){
  if(!prev) return true;
  return POLL_WATCHED_FIELDS.some(f =>
    JSON.stringify(pollFieldValue(prev, f)) !== JSON.stringify(pollFieldValue(next, f)));
}
// The change clock: a local action, or an observed difference in a watched
// field, resets it.  Anything else leaves it where it was.
function pollNextChangeAt(prev, next, localAction, now, lastChange){
  return (localAction || pollPayloadChanged(prev, next)) ? now : lastChange;
}
""".replace("__POLL_WATCHED_FIELDS__", json.dumps(POLL_WATCHED_FIELDS))


HTML = r"""<!DOCTYPE html>
<html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1"><title>ForgePact</title>
<style>
:root{--bg:#100d0b;--card:#191512;--card2:#211b16;--ember:#e99a4c;--ember2:#ffc47e;--tx:#eee2d2;--mut:#b7a996;--line:#43362a;--ok:#8bd3a6;--arcane:#ccb4ee;--blood:#f29ba6;--steel:#9dced5}
*{box-sizing:border-box;scrollbar-width:thin;scrollbar-color:#65513d #15120f}
body{margin:0;font:14px/1.5 'Segoe UI',sans-serif;background:var(--bg);color:var(--tx)}
button,input{font:inherit}button{touch-action:manipulation}button:disabled{opacity:.45;cursor:default!important}
button:focus-visible,input:focus-visible,summary:focus-visible,[role=button]:focus-visible{outline:2px solid var(--ember2);outline-offset:3px}
[hidden]{display:none!important}
#appShell{min-height:100vh;padding-left:222px}
.sidebar{position:fixed;inset:0 auto 0 0;width:222px;display:flex;flex-direction:column;padding:24px 14px 18px;border-right:1px solid var(--line);background:#17130f;z-index:20;overflow-y:auto}
.brand{display:flex;gap:8px;align-items:center;margin:0 0 34px}
.brand svg{width:57px;height:65px;flex:none;filter:drop-shadow(0 3px 3px #0005)}
.brand-name{font-size:20px;font-weight:750;letter-spacing:.4px;color:#ead6be}
.brand-sub{font-size:9px;letter-spacing:1.3px;color:var(--mut);margin-top:2px}
.tabbar{display:flex;flex-direction:column;gap:7px}
.tabbtn{display:flex;align-items:center;gap:13px;position:relative;text-align:left;border:1px solid transparent;border-radius:8px;background:transparent;color:#d8caba;padding:13px 14px;cursor:pointer;font-size:14px}
.tabbtn svg{width:22px;height:22px;flex:none;stroke:currentColor;fill:none;stroke-width:1.6;stroke-linecap:round;stroke-linejoin:round}
.tabbtn:hover{background:#261e17;color:var(--ember2)}
.tabbtn.active{color:var(--ember2);background:#3b2a1b;border-color:#6a482a}
.tabbtn.active:before{content:"";position:absolute;left:-1px;top:10px;bottom:10px;width:3px;border-radius:3px;background:var(--ember)}
.sidebar-foot{margin-top:auto;padding:22px 12px 0;color:var(--mut);font-size:12px}.sidebar-foot p{margin:4px 0}.sidebar-foot hr{border:0;border-top:1px solid var(--line);margin:20px 0 13px}
#wrap{max-width:1560px;margin:0 auto;padding:0 28px 50px;min-width:0}
.control-dock{position:sticky;top:0;z-index:10;background:#100d0bf7;border-bottom:1px solid var(--line);padding:14px 0;backdrop-filter:blur(10px)}
.topline{display:flex;align-items:center;justify-content:space-between;gap:12px;flex-wrap:wrap}
.breadcrumb{color:var(--mut);font-size:12px}.breadcrumb strong{color:var(--tx);font-weight:500;margin-left:8px}
#statusbar{display:flex;gap:14px;align-items:center;flex-wrap:wrap}
.chip{font-size:12px;color:var(--mut)}.chip.on{color:var(--ok)}.chip.err,.chip.warn{color:var(--ember2)}
#chipGame:before{content:"";display:inline-block;width:7px;height:7px;margin-right:7px;border-radius:50%;background:#8e867c}#chipGame.on:before{background:var(--ok)}
#saveIndicator{font-size:12px;min-width:105px;text-align:right;color:var(--mut)}#saveIndicator.error{color:#ffae91}#saveIndicator.saving{color:var(--ember2)}
.page-heading{padding:24px 0 20px;display:flex;align-items:flex-start;justify-content:space-between;gap:18px}
.page-heading h1{font-size:29px;letter-spacing:-.5px;line-height:1.2;margin:0 0 7px;color:#fff2e2}.page-heading p{margin:0;color:var(--mut);font-size:13px}
.page-actions{display:flex;align-items:center;gap:12px;padding-top:2px}.auto-control{display:flex;align-items:center;gap:8px;font-size:11px;color:var(--mut);white-space:nowrap}
.btn{background:#2e2218;color:var(--ember2);border:1px solid #755131;border-radius:7px;padding:9px 13px;cursor:pointer;font-size:12px;white-space:nowrap}.btn:hover{background:#3b2a1b;border-color:#ca8c4f}
.btn.primary{background:var(--ember);color:#201308;border-color:var(--ember);font-weight:650}.btn.primary:hover{background:var(--ember2)}
#workspace{display:grid;grid-template-columns:minmax(0,1fr) minmax(0,1fr);gap:18px;align-items:start}
.card{min-width:0;background:linear-gradient(145deg,#211b16,#171310);border:1px solid var(--line);border-radius:10px;padding:21px;margin:0;grid-column:1/-1}
.tab-card{display:none}.tab-card.active{display:block}.card.half{grid-column:auto;align-self:stretch}
.subtabbar{display:flex;gap:8px;grid-column:1/-1;margin:0 0 4px}
.subtabbtn{background:transparent;border:1px solid var(--line);border-radius:8px;color:var(--mut);padding:9px 16px;cursor:pointer;font-size:13px}
.subtabbtn:hover{background:#261e17;color:var(--ember2)}
.subtabbtn.active{color:var(--ember2);background:#3b2a1b;border-color:#6a482a}
.card h2{margin:0 0 6px;font-size:18px;letter-spacing:.1px;color:#f5e7d4;display:flex;align-items:center;gap:10px}
.card .hint{color:var(--mut);font-size:12px;line-height:1.65;margin-bottom:16px}
.note{font-size:12px;color:var(--mut);margin:8px 0;line-height:1.6}.note:empty{display:none}
.help-details{margin-top:12px;color:var(--mut);font-size:12px}.help-details summary{cursor:pointer;color:#d8bd97;font-size:12px;width:fit-content}.help-details[open] summary{margin-bottom:8px}.help-details .hint{margin:0}
.row{display:flex;align-items:center;gap:14px;padding:14px 0;border-top:1px solid #352b22;min-width:0}
.row .lbl{width:210px;font-size:13px;flex-shrink:0}.row .lbl .tag{font-size:10px;color:var(--mut);margin-left:6px}.row .lbl>span[style]{color:var(--mut)!important;line-height:1.65;font-size:12px!important}
.range-control{display:flex;gap:12px;align-items:center;min-width:0;flex:1}
input[type=range]{flex:1;min-width:55px;width:100%;appearance:none;height:6px;border-radius:5px;background:#4a3a2b;cursor:pointer;accent-color:var(--ember)}
input[type=range]::-webkit-slider-thumb{appearance:none;width:17px;height:17px;border-radius:50%;background:var(--ember2);border:2px solid #d39453;box-shadow:none}input[type=range]::-moz-range-thumb{width:14px;height:14px;border-radius:50%;background:var(--ember2);border:2px solid #d39453}
.value-stepper{display:flex;align-items:center;flex:none;border:1px solid #62472e;border-radius:6px;overflow:visible;background:#120f0d}
.step-button{padding:5px 9px;min-width:28px;min-height:32px;border:0;background:transparent;color:#dbc3a5;cursor:pointer;font-size:18px;line-height:1}.step-button:hover{background:#39291c;color:var(--ember2)}
.val{min-width:42px;width:60px;text-align:center;color:var(--ember2);font-size:13px;font-weight:600;flex:none}.val.off{color:#ab9a86}.value-stepper>.val{padding:4px;min-width:51px;border-left:1px solid #493725;border-right:1px solid #493725}
.numedit{width:100%;min-width:48px;border:0;background:#2a1e15;color:#fff0db;text-align:center;padding:0;outline:0;font-size:13px}
.switch{position:relative;width:42px;height:23px;flex:none;display:inline-block}.switch input{position:absolute;inset:0;opacity:0;width:100%;height:100%;margin:0;z-index:1;cursor:pointer}.switch input:disabled{cursor:not-allowed}.switch:focus-within{outline:2px solid var(--ember2);outline-offset:4px;border-radius:20px}
.sl{position:absolute;inset:0;border-radius:23px;background:#413529;border:1px solid #67513a;pointer-events:none}.sl:before{content:"";position:absolute;width:17px;height:17px;left:2px;top:2px;background:#b2a38f;border-radius:50%;transition:transform .12s}
.switch input:checked+.sl{background:#ad6b2e;border-color:#efb46a}.switch input:checked+.sl:before{transform:translateX(19px);background:#ffe2b4}
.feature-card:has(>.style-select){grid-template-columns:minmax(0,1fr) auto}.style-select{min-width:96px;grid-column:2/-1;background:#2a1e15;color:#fff0db;border:1px solid #62472e;border-radius:6px;padding:6px 8px;font-size:13px;cursor:pointer}.style-select:focus{outline:2px solid var(--ember2);outline-offset:2px}
#exepath{min-width:180px;flex:1;background:#100e0c;color:var(--tx);border:1px solid #65513d;border-radius:7px;padding:11px 12px;font-size:13px}.setup-path-row{flex-wrap:wrap}.setup-path-row #exepath{flex-basis:100%}.setup-launch{flex-wrap:wrap}
#toast{position:fixed;bottom:22px;left:calc(50% + 90px);transform:translateX(-50%);max-width:min(600px,calc(100vw - 36px));z-index:100;background:#292018;color:#ffe3be;border:1px solid #996a3e;border-radius:8px;padding:11px 18px;opacity:0;transition:opacity .15s;pointer-events:none;box-shadow:0 5px 25px #0005;font-size:12px}#toast.show{opacity:1}
.section-title{display:flex;align-items:flex-start;justify-content:space-between;gap:14px}.live-badge{font-size:10px;padding:4px 8px;white-space:nowrap;border:1px solid #3e6650;border-radius:20px;background:#18281f;color:var(--ok)}
.modifier-grid{display:grid;grid-template-columns:minmax(0,1fr) minmax(0,1fr);gap:16px}.modifier-group{min-width:0;background:#13100e;border:1px solid #403329;border-radius:9px;padding:16px}.modifier-group.wide{grid-column:1/-1}
.group-title{font-size:12px;font-weight:600;color:#e8c797;margin-bottom:7px}.group-title.offense{color:#f2aba6}.group-title.critical{color:#cab5e4}.group-title.sustain{color:#a0cec2}
.modifier-group .row{display:block;border:0;padding:10px 0}.modifier-group .row .lbl{display:block;width:auto;margin-bottom:9px}.modifier-group .note{margin:0 0 5px;font-size:11px}.modifier-group .setting-entry+.setting-entry{border-top:1px solid #31271f}
.setting-entry{min-width:0;padding:0 2px}.setting-entry .row{border:0}.setting-entry .note{margin:-5px 0 12px}.settings-grid{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:0 24px}.settings-grid .row{display:block}.settings-grid .lbl{display:block;width:auto;margin-bottom:9px}.settings-grid .setting-entry{border-bottom:1px solid #382b21}
#dropSettings>#drops,#dropSettings>#keys{display:contents}
#controlToolbar{display:flex;gap:12px;margin:0 0 18px}.control-search{flex:1;min-width:0;background:#17120f;border:1px solid #55422e;border-radius:7px;padding:10px 12px;color:var(--tx)}.control-search::placeholder{color:var(--mut)}.control-filters{display:flex;gap:4px;background:#17120f;border:1px solid #55422e;padding:4px;border-radius:7px}.control-filters button{background:transparent;border:0;border-radius:4px;color:var(--mut);padding:4px 13px;cursor:pointer}.control-filters button[aria-pressed=true]{background:#4d3420;color:#ffd396}
.empty-settings{color:var(--mut);padding:20px;text-align:center;grid-column:1/-1}
.hero-number{color:var(--ember2);font-size:35px;line-height:1.25;font-weight:700;margin:9px 0}.density-top{display:flex;align-items:center;justify-content:space-between}.density-top .row{border:0;padding:0;gap:8px}.density-top .lbl{width:auto;font-size:11px;color:var(--mut)}
#densityCard>.row{border:0;padding:6px 0}#densityCard>.row>.lbl{display:none}.density-scale{display:flex;justify-content:space-between;font-size:11px;color:var(--mut);margin-top:5px}
#rarityCard .row{border:0;display:flex;padding:12px 0}#rarityCard .lbl{display:block;width:62px;margin:0}#rarityCard .note{font-size:11px}
.mods-grid{display:flex;gap:12px;align-items:flex-start}.mods-col{flex:1 1 0;min-width:0}.mods-col>.feature-card,.mods-col>.feature-with-child{margin:0 0 12px!important}.feature-card{padding:15px!important;background:#15110e;border:1px solid #45352a!important;border-radius:8px;margin:0!important;align-items:flex-start}.feature-card>.lbl{flex:1!important;width:auto!important;min-width:0}.feature-card .switch{margin-top:1px}.feature-card>.val{min-width:0;width:24px;font-size:11px;margin-top:2px}.feature-card:has(>.switch>input:checked),.feature-with-child:has(>.feature-card:first-child>.switch>input:checked){border-color:#85603a!important}.feature-with-child{border:1px solid #45352a;border-radius:8px;background:#15110e;overflow:hidden}.feature-with-child>.feature-card{border:0!important;border-radius:0}.feature-with-child>#map_reveal_packs_row,.feature-with-child>#map_reveal_spawn_row,.feature-with-child>#mod_auto_prospect_bag_row{border:0!important;border-top:1px solid #45352a!important;margin:0!important;padding:14px!important;background:#1d1711;border-radius:0}
.feature-card{display:grid;grid-template-columns:minmax(0,1fr) 42px 24px;gap:8px 12px;align-content:start}.feature-card>.lbl{font-weight:600}.feature-description{grid-column:1/-1;color:var(--mut)!important;line-height:1.65;font-size:12px!important;font-weight:normal}.switch input:disabled+.sl{opacity:.4;filter:grayscale(1)}
#minerHelmetCard{display:block}#minerHelmetCard .hint{margin:10px 0}
@media(min-width:1700px){#wrap{padding-left:38px;padding-right:38px}}
@media(max-width:1150px){#appShell{padding-left:190px}.sidebar{width:190px;padding:20px 10px}.brand svg{width:44px}.brand-name{font-size:17px}.brand-sub{font-size:8px}.page-heading{flex-wrap:wrap}.modifier-grid{grid-template-columns:1fr}.mods-grid{flex-direction:column;align-items:stretch;gap:0}.settings-grid{grid-template-columns:1fr}.card.half{grid-column:1/-1}.row .lbl{width:180px}#wrap{padding:0 20px 40px}}
@media(max-width:720px){#appShell{padding-left:0}.sidebar{position:static;width:auto;padding:12px 14px;border-right:0;border-bottom:1px solid var(--line);overflow:visible}.brand{margin:0 0 10px}.brand svg{width:39px;height:39px}.brand-name{font-size:18px}.brand-sub{display:none}.tabbar{flex-direction:row;gap:3px}.tabbtn{flex:1;justify-content:center;padding:10px 6px;gap:4px;font-size:11px}.tabbtn svg{width:15px;height:15px}.sidebar-foot{display:none}#wrap{padding:0 14px 35px}.control-dock{position:static}.page-heading h1{font-size:25px}.page-actions{width:100%;justify-content:space-between;flex-wrap:wrap}.row{flex-wrap:wrap}.row .lbl{width:100%;flex-shrink:1}.row:has(.range-control)>.range-control{flex-basis:100%}.range-control{gap:8px}.step-button{padding:5px 6px}.value-stepper>.val{min-width:44px;width:52px!important}.card{padding:16px}#workspace{gap:14px}.topline{gap:8px}#statusbar{gap:10px}#saveIndicator{min-width:0}#toast{left:50%}#controlToolbar{flex-wrap:wrap}.control-search{flex-basis:100%}.control-filters{width:100%}.control-filters button{flex:1}.feature-card{flex-wrap:nowrap}.density-top .row{flex-wrap:nowrap}.section-title{flex-wrap:wrap}.subtabbtn{flex:1;justify-content:center;padding:9px 6px;font-size:12px}}
@media(prefers-reduced-motion:reduce){*{scroll-behavior:auto!important;transition:none!important}}
#satanicMods{background:linear-gradient(145deg,#211a17,#161210 65%);border-color:#48362b;padding:24px}
.sat-heading{display:flex;align-items:flex-start;justify-content:space-between;gap:18px}
#satanicMods h2{font-size:22px;letter-spacing:.3px;margin-bottom:6px}
.sat-intro{margin:0;color:#d1c3b2;font-size:13px}
.sat-intro span{display:block;color:#a99a89;font-size:12px;margin-top:3px}
.sat-button{font:inherit;font-size:12px;color:#ebc799;background:#271e18;border:1px solid #5b4533;border-radius:7px;padding:8px 12px;cursor:pointer;white-space:nowrap}
.sat-button:hover:not(:disabled){background:#37271b;border-color:#c78a4c}
.sat-button:disabled{opacity:.45;cursor:default}
.sat-toolbar{display:flex;gap:12px;align-items:center;margin:22px 0 18px}
.sat-search{display:flex;gap:10px;align-items:center;flex:1;min-width:0;background:#100e0d;border:1px solid #46392f;border-radius:8px;padding:0 12px;color:#a99a89}
.sat-search:focus-within{outline:2px solid var(--ember2);outline-offset:2px}
.sat-search svg{width:17px;height:17px;flex:none}
#satSearch{width:100%;min-width:0;padding:11px 0;background:none;border:0;outline:none;color:var(--tx);font:inherit;font-size:13px}
#satSearch::placeholder{color:#a99a89}
.sat-filters{display:flex;gap:3px;padding:4px;border:1px solid #403329;border-radius:8px;background:#100e0d}
.sat-filters button{font:inherit;font-size:12px;padding:7px 12px;color:#b6a794;background:transparent;border:1px solid transparent;border-radius:5px;cursor:pointer}
.sat-filters button[aria-pressed="true"]{background:#3c2a1b;border-color:#79512d;color:#ffcf90}
.sat-filters button:hover{color:#ffcf90}
.sat-columns{display:grid;grid-template-columns:minmax(0,1fr) minmax(0,1fr);gap:16px}
.sat-panel{--sat-accent:#85d4aa;--sat-tint:#182820;--sat-border:#385b48;min-width:0;border:1px solid #40362e;border-radius:10px;background:#110f0e;overflow:hidden}
.sat-panel[data-polarity="debuff"]{--sat-accent:#f19aa5;--sat-tint:#2b1b20;--sat-border:#65404a}
.sat-panel-head{padding:16px;border-bottom:1px solid #302720}
.sat-panel-title{display:flex;align-items:center;gap:8px;flex-wrap:wrap}
.sat-panel h3{margin:0;font-size:15px;font-weight:600;color:var(--sat-accent)}
.sat-sign{display:inline-grid;place-items:center;width:22px;height:22px;border:1px solid var(--sat-border);border-radius:6px;color:var(--sat-accent);font-size:16px;background:var(--sat-tint)}
.sat-count{margin-left:auto;color:var(--sat-accent);background:var(--sat-tint);border:1px solid var(--sat-border);border-radius:20px;padding:2px 8px;font-size:11px;white-space:nowrap}
.sat-panel-tools{display:flex;align-items:center;justify-content:space-between;gap:10px;margin-top:12px;color:#b6a794;font-size:12px}
.sat-panel-tools .sat-button{padding:5px 10px}
.sat-list{height:300px;overflow-y:auto;overscroll-behavior:contain;padding:10px;scrollbar-gutter:stable}
.sat-option{display:flex;align-items:flex-start;gap:12px;padding:13px 12px;margin-bottom:7px;min-height:70px;border:1px solid #352d27;border-radius:7px;background:#1a1613;cursor:pointer;transition:border-color .12s,background .12s}
.sat-option:last-child{margin-bottom:0}
.sat-option[hidden]{display:none}
.sat-option:hover{border-color:#7d6854;background:#231d18}
.sat-option.is-enabled{background:var(--sat-tint);border-color:var(--sat-border)}
.sat-option:focus-within{outline:2px solid var(--ember2);outline-offset:1px}
.sat-option input{appearance:none;width:19px;height:19px;flex:none;position:relative;margin:3px 0 0;border:1px solid #8c7b68;border-radius:4px;background:#100e0d;cursor:inherit}
.sat-option input:checked{background:var(--sat-accent);border-color:var(--sat-accent)}
.sat-option input:checked:after{content:"";position:absolute;left:5px;top:2px;width:5px;height:9px;border:solid #18211b;border-width:0 2px 2px 0;transform:rotate(45deg)}
.sat-option input:focus{outline:none}
.sat-option.is-locked{cursor:not-allowed}
#satanicMods[aria-busy="true"] .sat-option{cursor:wait}
.sat-name{display:block;font-size:13px;font-weight:600;color:#f0e4d4}
.sat-desc{display:block;font-size:12px;line-height:1.5;color:#b9ae9f;margin-top:3px;overflow-wrap:anywhere}
.sat-panel-foot{min-height:69px;padding:11px 16px;border-top:1px solid #302720;font-size:11px;color:#b7aa98}
.sat-panel-foot strong{display:block;color:#dbc5a7;font-size:12px;margin-bottom:2px;font-weight:500}
.sat-empty{padding:35px 12px;text-align:center;color:#b7aa98;font-size:13px}
.sat-footer{display:flex;align-items:center;justify-content:space-between;gap:14px;flex-wrap:wrap;padding-top:17px;font-size:12px;color:#b7aa98}
#satSummary{color:#a8d1b5}
#satSummary.is-invalid,#satSaveState.is-error{color:#ffbd89}
.sat-footer-note{margin-left:8px;color:#b7aa98}
#satanicMods button:focus-visible{outline:2px solid var(--ember2);outline-offset:3px}
@media(max-width:720px){.control-dock{position:static}#satanicMods{padding:18px}.sat-heading{flex-wrap:wrap}.sat-toolbar{flex-wrap:wrap}.sat-search{flex-basis:100%}.sat-filters{width:100%}.sat-filters button{flex:1}.sat-columns{grid-template-columns:1fr}.sat-list{height:340px}.sat-footer{align-items:flex-start;flex-direction:column}.sat-footer-note{display:block;margin:3px 0 0}}
.icon-definitions{position:absolute;overflow:hidden;pointer-events:none}
.setting-icon{width:28px;height:28px;flex:none;display:inline-block;vertical-align:middle;pointer-events:none;filter:drop-shadow(0 1px 1px #0005)}
.has-setting-icon{display:flex!important;align-items:center;gap:9px}.label-copy{min-width:0}
.row .lbl.has-setting-icon{line-height:1.4}.card h2.has-setting-icon{gap:10px}
#densityCard>.row>.lbl.has-setting-icon{display:none!important}
.group-title .setting-icon,.btn .setting-icon,.auto-control .setting-icon{width:20px;height:20px}
.sat-name{display:flex;align-items:center;gap:8px}.sat-name .setting-icon{width:24px;height:24px}.sat-desc{margin-left:32px}
.sat-option>span{min-width:0}.feature-card>.lbl .setting-icon{align-self:flex-start}
#rarityCard .lbl{width:96px;flex-shrink:0}
@media(max-width:720px){.setting-icon{width:25px;height:25px}.sat-option{gap:9px;padding:12px 10px}.sat-desc{margin-left:0}.sat-name .setting-icon{width:24px;height:24px}#rarityCard .lbl{width:96px}.btn.has-setting-icon{justify-content:center}}
.plugin-warning{display:flex;align-items:center;justify-content:space-between;gap:16px;padding:12px 16px;margin:0 0 18px;border:1px solid #805d32;border-radius:8px;background:#2c2115;color:#ffcf90;font-size:13px;line-height:1.5}.plugin-warning[hidden]{display:none}.plugin-warning .btn{flex:none}@media(max-width:720px){.plugin-warning{align-items:flex-start;flex-direction:column;gap:10px}}
.launch-feedback{padding:11px 13px;margin:10px 0;border:1px solid #483b2c;border-radius:7px;background:#181510;color:#c8b59b;font-size:13px;line-height:1.5;overflow-wrap:anywhere}.launch-feedback.starting{color:#ffcf90;border-color:#805d32}.launch-feedback.error,.launch-feedback.warning{color:#ffc397;border-color:#996140}.launch-feedback.verified{color:#9bdab8;border-color:#385b48}@media(max-width:720px){.setup-launch{align-items:flex-start}.setup-launch>.note{flex-basis:100%!important}.setup-launch>.btn{width:100%;justify-content:center}}
</style></head><body>""" + ICON_SPRITE + r"""<div id="appShell">
<aside class="sidebar" aria-label="ForgePact navigation">
  <!-- Anvil adapted from Falor's toolkit ToolIcon.svelte; closed body and continuous top face. -->
  <div class="brand"><svg viewBox="0 0 80 80" fill="none" aria-hidden="true"><defs><linearGradient id="forge-anvil" x1="15" y1="8" x2="65" y2="73" gradientUnits="userSpaceOnUse"><stop stop-color="#efc79b"/><stop offset=".48" stop-color="#b27a48"/><stop offset="1" stop-color="#563d2c"/></linearGradient></defs><g stroke="#efc79b" stroke-width="1.3" stroke-linejoin="round" stroke-linecap="round"><path d="M39 3 51 17 40 31 29 17Z" fill="url(#forge-anvil)"/><path d="m40 9-5 8 5 8 5-8Z" fill="#141619"/><path d="M28 27H47V30H70C68 38 60 42 47 43V55L55 64H28L35 55V43H24C15 43 8 38 3 30H28Z" fill="url(#forge-anvil)"/><path d="M3 30H70L66 34H8Z" fill="#d8aa7b"/><path d="M30 64h23l5 7H24Z" fill="url(#forge-anvil)"/><path d="M39 34v25m-7 8h19" opacity=".7"/></g></svg><div><div class="brand-name">FORGEPACT</div><div class="brand-sub">HERO SIEGE TOOLS</div></div></div>
  <nav class="tabbar" role="tablist" aria-label="ForgePact categories" aria-orientation="vertical"><button class="tabbtn" data-tab="setup" role="tab" id="nav-setup" aria-controls="workspace"><svg viewBox="0 0 24 24" aria-hidden="true"><path d="M10 3h4l1 3 3 1 3 2v4l-3 2-1 3-3 3h-4l-1-3-3-1-3-2v-4l3-2 1-3zM15 12a3 3 0 1 1-6 0 3 3 0 0 1 6 0"/></svg>Setup</button>
<button class="tabbtn" data-tab="modifiers" role="tab" id="nav-modifiers" aria-controls="workspace"><svg viewBox="0 0 24 24" aria-hidden="true"><path d="M3 6h18M3 12h18M3 18h18M7 3v6M16 9v6M10 15v6"/></svg>Modifiers</button>
<button class="tabbtn" data-tab="world" role="tab" id="nav-world" aria-controls="workspace"><svg viewBox="0 0 24 24" aria-hidden="true"><path d="M21 12a9 9 0 1 1-18 0 9 9 0 0 1 18 0M3 12h18M12 3c5 5 5 13 0 18-5-5-5-13 0-18"/></svg>World</button>
<button class="tabbtn" data-tab="loot" role="tab" id="nav-loot" aria-controls="workspace"><svg viewBox="0 0 24 24" aria-hidden="true"><path d="M4 8h16v12H4zM3 8l3-5h12l3 5M9 8v5h6V8"/></svg>Loot</button>
<button class="tabbtn" data-tab="mods" role="tab" id="nav-mods" aria-controls="workspace"><svg viewBox="0 0 24 24" aria-hidden="true"><path d="M4 4h6V2a3 3 0 0 1 6 0v2h5v6h-2a3 3 0 0 0 0 6h2v5h-6v-2a3 3 0 0 0-6 0v2H4v-6H2a3 3 0 0 1 0-6h2Z"/></svg>Mods</button>
</nav>
  <div class="sidebar-foot"><hr><p>Offline tools</p><p>Created by Falor</p><p id="panelver"></p></div>
</aside>
<main id="wrap">
  <div class="control-dock"><div class="topline">
    <div class="breadcrumb">ForgePact / <strong id="breadcrumbPage">Modifiers</strong></div>
    <div id="statusbar"><span class="chip" id="chipGame">Connecting...</span><span id="saveIndicator" role="status" aria-live="polite">Loading settings...</span></div>
  </div></div>
  <div id="pluginWarning" class="plugin-warning" role="status" hidden><span id="pluginWarningText"></span><button class="btn" type="button" onclick="openTab('setup')">Open Setup</button></div>
  <div class="page-heading"><div><h1 id="pageTitle">Character modifiers</h1><p id="pageDescription">Tune your character and combat bonuses.</p></div>
    <div class="page-actions"><label class="auto-control">Auto-apply <span class="switch"><input type="checkbox" id="autoapply" aria-label="Auto-apply on game launch"><span class="sl"></span></span></label><button class="btn" id="applyall" title="Send all saved settings to the game">Apply all now</button></div>
  </div>
  <div id="controlToolbar" hidden><input type="search" id="controlSearch" class="control-search" placeholder="Search settings by name or effect..." aria-label="Search settings in this section"><div class="control-filters" role="group" aria-label="Filter settings"><button data-control-filter="all" aria-pressed="true">All settings</button><button data-control-filter="modified" aria-pressed="false">Modified</button></div></div>
  <div id="workspace" role="tabpanel" aria-labelledby="nav-modifiers">
<div id="modsSubtabs" class="subtabbar" role="tablist" aria-label="Mods categories" hidden><button type="button" class="subtabbtn" role="tab" id="subtab-qol" aria-controls="qolCard" aria-selected="true" tabindex="0">Quality of Life</button><button type="button" class="subtabbtn" role="tab" id="subtab-items" aria-controls="itemsCard" aria-selected="false" tabindex="-1">Items</button></div>
<div class="card tab-card" data-tab="setup" id="setupCard">
  <h2>Game Location</h2>
  <div class="hint">ForgePact talks to the mod plugin sitting next to this exe. Change it if your game lives somewhere else.</div>
  <div class="row setup-path-row" style="border:none">
    <input id="exepath" placeholder="C:\...\HeroSiege\bin\Hero_Siege.exe">
    <button class="btn" id="exebrowse" title="Open a file picker to choose Hero_Siege.exe">&#128193; Browse...</button>
    <button class="btn" id="exesave">Save</button>
    <button class="btn" id="installmod" title="One click: backs up the exe, copies mod DLLs, patches the exe">Install Mod Plugin</button>
    <button class="btn" id="removeplugin" title="Restores your original exe from the backup and removes the mod files (game must be closed)">Remove Plugin</button>
  </div>
  <div class="row setup-launch" style="border:none;margin-top:6px">
    <button class="btn primary" id="launchgame" title="Start through the built-in HS Offline Launcher">&#9654; Launch Modded Game</button>
    <span class="note" style="flex:1"><b>HS Offline Launcher · Built in</b><br>Starts Steam if needed and launches your selected game with the correct Steam settings. Use offline characters.</span>
  </div>
  <div class="launch-feedback" id="launchFeedback" role="status" aria-live="polite">HS Offline Launcher is built in. No separate installation needed.</div>
  <div class="note" id="eacnote"></div>
  <div class="note" id="ipcnote"></div>
  <div class="note" id="chainnote"></div>
</div>

<div class="card tab-card" data-tab="world" id="densityCard">
  <h2>Monster Density</h2>
  <div class="hint">Multiplies enemy spawners - applies to newly loaded zones.<br><b>Density and Special Content stack.</b> Each on its own is fine, but a high density together with high special-content rates can overload a heavy zone and crash the game on entry. Verified stable: density x3 with every special content at x20. If a zone crashes, lower density first.</div>
  <div class="note" style="color:#72d6a5;border:1px solid #245a43;border-radius:6px;padding:8px 12px;margin-bottom:10px">Density is applied once per creator placement. Returning to a previously visited zone does not multiply it again.</div>
  <div class="row">
    <span class="lbl">Density multiplier</span>
    <label class="switch"><input type="checkbox" id="den_on"><span class="sl"></span></label>
    <input type="range" id="den" min="1" max="5" step="0.5">
    <span class="val" id="denval">x3</span>
  </div>
</div>

<div class="card tab-card" data-tab="world" id="speedCard">
  <h2>Enemy Movement Speed</h2>
  <div class="hint">Enemies run at you faster, so waves end sooner. Scales the game's own path speed (base speed &times; bonus); slows and debuffs still apply on top, goblins keep their own pace. <b>Only inside Chaos Tower</b> leaves every other zone vanilla - switch it off to speed up enemies everywhere.</div>
  <div class="row">
    <span class="lbl">Speed bonus</span>
    <input type="range" id="enemyspeed" min="0" max="300" step="5">
    <span class="val" id="enemyspeedval">off</span>
  </div>
  <div class="row" style="border:none">
    <span class="lbl">Only inside Chaos Tower</span>
    <label class="switch"><input type="checkbox" id="enemyspeed_ct"><span class="sl"></span></label>
    <span class="val" id="enemyspeedctval">CT only</span>
  </div>
</div>

<div class="card tab-card" data-tab="world" id="spawnsCard">
  <h2>Special Content Spawns</h2>
  <div class="hint">Multiplies the game's own spawn markers, so the game places and runs each mechanic itself - nothing is hand-placed. Higher = more of that content per zone. Applies to newly loaded zones. (The Abyss is not listed: it sits behind a discovery gate that is not solved yet.)</div>
  <div id="spawners"></div>
</div>

<div class="card tab-card" data-tab="loot" id="dropsCard">
  <h2>Drop Rates</h2>
  <div class="hint">All of these use the game's own dice - <b>nothing is forced</b>.
  <b>x5 means five times more likely than vanilla</b>; <b>off</b> (x1) leaves that drop completely untouched.
  Applies immediately, no zone reload needed.<br>
  Every multiplier scales the item's own vanilla roll, so x2 is twice the vanilla rate.
  Families the game never rolls outside their home zone (Dungeon and Angelic keys, Orbs,
  Scrolls, Shards, Fragments) first get their roll opened at the monster's normal-key
  chance; Relics use their own curve, explained on the row.</div>
  <div id="dropSettings">
    <div id="drops"></div>
    <div id="keys"></div>
  </div>
</div>

<div class="card tab-card" data-tab="loot" id="angelicCard">
  <h2>Angelic / Unholy Drops (Experimental)</h2>
  <div class="hint">The game only rolls for Angelic or Unholy items while an "Angelic item drop chance"
  effect (a Blood Pact or dungeon modifier) is active, so this is ForgePact's own die: on every monster
  kill it rolls, and on a hit the game itself builds one of its 49 real Angelic / Unholy uniques (no
  developer or event pieces) and drops it where the monster died. Headhunter and Tyrant's Crown share
  this same die and pool: they never drop while this is off (x1), and drop exactly as often as any
  other item in it.<br>
  <b>x2</b> is one die per kill at the Angelic Key's own rate (1 in 7,500), every step above adds a die.
  Click the value to type an exact number. <b>x1</b> is off.</div>
  <div class="row">
    <span class="lbl">Angelic / Unholy items</span>
    <input type="range" id="angelic_items" min="1" max="100" step="1" value="1">
    <span class="val off" id="angelicval" style="width:64px">off</span>
  </div>
  <div class="note" id="angelicnote">off</div>
</div>

<div class="card modifier-card tab-card" data-tab="modifiers">
  <div class="section-title">
    <div><h2>Combat &amp; Character Modifiers</h2>
      <div class="hint">Live modifiers use the character's current total value, including equipment and other bonuses. <b>Off</b> keeps the game at its normal value.</div>
    </div>
    <span class="live-badge">LIVE MODIFIERS</span>
  </div>
  <div class="modifier-grid">
    <div class="modifier-group">
      <div class="group-title sustain">Utility</div>
      <div id="stats"></div>
    </div>
    <div class="modifier-group">
      <div class="group-title offense">Offense</div>
      <div id="offensivestats"></div>
    </div>
    <div class="modifier-group">
      <div class="group-title sustain">Defense &amp; Sustain</div>
      <div id="sustainstats"></div>
    </div>
    <div class="modifier-group">
      <div class="group-title critical">Critical Strikes</div>
      <div id="criticalstats"></div>
    </div>
  </div>
</div>

<div class="card tab-card" data-tab="world" id="rarityCard">
  <h2>Monster Rarity</h2>
  <div class="hint">Raises a share of the normal monsters to <b>Rare</b> (yellow) or <b>Ancient</b> (skull) as they spawn, through the game's own rarity setup: the monster gets that tier's stats, affixes and health bar exactly as if it had rolled that way. The two shares are separate and together stay at 100% or less - 25% Rare with 15% Ancient leaves 60% normal. Champions, the game's own rares, and bosses are left alone - bosses already have their own scripted health and affixes. Stacks with Tyrant's Crown and Density.</div>
  <div class="row" style="border:none">
    <span class="lbl">Rare</span>
    <input type="range" min="0" max="100" step="5" id="rarity_rare" value="0">
    <span class="val off" id="rarityrareval" style="width:64px">off</span>
  </div>
  <div class="row" style="border:none">
    <span class="lbl">Ancient</span>
    <input type="range" min="0" max="100" step="5" id="rarity_ancient" value="0">
    <span class="val off" id="rarityancval" style="width:64px">off</span>
  </div>
  <div class="note" id="raritynote">off</div>
</div>

<section class="card tab-card" data-tab="world" id="satanicMods" aria-labelledby="satTitle" aria-busy="false">
  <div class="sat-heading">
    <div>
      <h2 id="satTitle">Satanic Zone Mods</h2>
      <p class="sat-intro">Choose which modifiers can roll in your zones.
        <span>Enabled mods are eligible, not guaranteed. The game still rolls your zone.</span>
      </p>
    </div>
    <button type="button" class="sat-button" id="satRestore" title="Enable every positive and negative zone modifier">&#8634; Restore defaults</button>
  </div>
  <div class="sat-toolbar">
    <label class="sat-search">
      <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" aria-hidden="true"><circle cx="10.5" cy="10.5" r="6.5"/><path d="m16 16 5 5"/></svg>
      <input type="search" id="satSearch" placeholder="Search by name or effect..." aria-label="Search zone modifiers" autocomplete="off">
    </label>
    <div class="sat-filters" role="group" aria-label="Filter zone modifiers">
      <button type="button" data-sat-filter="all" aria-pressed="true">All mods</button>
      <button type="button" data-sat-filter="enabled" aria-pressed="false">Enabled</button>
      <button type="button" data-sat-filter="disabled" aria-pressed="false">Disabled</button>
    </div>
  </div>
  <div class="sat-columns">
    <section class="sat-panel" data-polarity="buff" aria-labelledby="satbuffTitle">
      <div class="sat-panel-head">
        <div class="sat-panel-title"><span class="sat-sign" aria-hidden="true">+</span><h3 id="satbuffTitle">Positive modifiers</h3><span class="sat-count" id="satbuffCount"></span></div>
        <div class="sat-panel-tools"><span>Keep at least <strong id="satbuffMin">3</strong> enabled</span><button type="button" class="sat-button" id="satbuffAll" aria-label="Enable all positive modifiers">Enable all</button></div>
      </div>
      <div class="sat-list" id="satbuffs" role="group" aria-labelledby="satbuffTitle"></div>
      <div class="sat-panel-foot" id="satbuffHelp"></div>
    </section>
    <section class="sat-panel" data-polarity="debuff" aria-labelledby="satdebuffTitle">
      <div class="sat-panel-head">
        <div class="sat-panel-title"><span class="sat-sign" aria-hidden="true">&minus;</span><h3 id="satdebuffTitle">Negative modifiers</h3><span class="sat-count" id="satdebuffCount"></span></div>
        <div class="sat-panel-tools"><span>Keep at least <strong id="satdebuffMin">2</strong> enabled</span><button type="button" class="sat-button" id="satdebuffAll" aria-label="Enable all negative modifiers">Enable all</button></div>
      </div>
      <div class="sat-list" id="satdebuffs" role="group" aria-labelledby="satdebuffTitle"></div>
      <div class="sat-panel-foot" id="satdebuffHelp"></div>
    </section>
  </div>
  <div class="sat-footer">
    <div id="satSummary" role="status" aria-live="polite"></div>
    <span id="satSaveState" role="status" aria-live="polite">Changes save automatically</span>
  </div>
</section>

<div class="card tab-card" data-tab="mods" id="qolCard" role="tabpanel" aria-labelledby="subtab-qol">
  <h2>Quality of Life</h2>
  <div class="hint">Toggle drop pool adjustments and quality-of-life tweaks for your offline session. Settings apply immediately while the game is running.</div>
  <div class="row" style="border:none">
    <span class="lbl" style="width:auto;flex:1">Remove owned relics from drop pool<br><span style="font-size:11px;color:#8f816e;font-weight:normal">When a relic is dropped, prevents relics already at maximum level (10 out of 10) in your equipped slots, backpack, or inventory from dropping.</span></span>
    <label class="switch"><input type="checkbox" id="mod_filter_max_relics"><span class="sl"></span></label>
    <span class="val" id="mfmrval">on</span>
  </div>
    <div class="row" style="border:none">
        <span class="lbl" style="width:auto;flex:1">Experience and Magic Find orb pickup radius<br><span style="font-size:11px;color:#8f816e;font-weight:normal">Makes the player collect matching orbs from 10 times the normal distance.</span></span>
        <label class="switch"><input type="checkbox" id="mod_orb_pickup_radius"><span class="sl"></span></label>
        <span class="val" id="morval">off</span>
    </div>
    <div class="row" style="border:none">
        <span class="lbl" style="width:auto;flex:1">Reveal full map<br><span style="font-size:11px;color:#8f816e;font-weight:normal">Reveals the full minimap in every zone (removes fog of war). Waypoints, dungeon entrances, chests, shrines and mining nodes come with it - they are hidden by the fog, not by anything else.</span></span>
        <label class="switch"><input type="checkbox" id="map_reveal"><span class="sl"></span></label>
        <span class="val" id="mapval">on</span>
    </div>
    <div class="row" id="map_reveal_packs_row" style="border:none;margin-left:22px;border-left:1px solid #33261c;padding-left:14px">
        <span class="lbl" style="width:auto;flex:1">&#8627; Show every monster pack on the map<br><span style="font-size:11px;color:#8f816e;font-weight:normal">Most mob packs do not exist until you walk near them, so the revealed map used to show only the packs you had already met. This marks every pack's spot and kind (normal, champion, ancient, legion, mini boss) on the minimap the moment you arrive, including density copies, without creating a single monster: the pack is still born by the game when you walk near it, and its real dots replace the marker. Costs nothing per frame beyond the markers themselves.</span><span id="packMarkerStatus" class="hint" role="status" hidden></span></span>
        <label class="switch"><input type="checkbox" id="map_reveal_packs"><span class="sl"></span></label>
        <span class="val" id="mrpval">on</span>
    </div>
    <div class="row" id="map_reveal_spawn_row" style="border:none;margin-left:22px;border-left:1px solid #33261c;padding-left:14px">
        <span class="lbl" style="width:auto;flex:1">&#8627; Really spawn every pack on arrival (heavy)<br><span style="font-size:11px;color:#8f816e;font-weight:normal">The old way: each new zone creates all of its packs, including density copies, as you arrive. Every living monster costs the game frame time on top of the markers, so at high density this lags for the whole zone. Off by default; only for comparing against the markers.</span><span id="populationStatus" class="hint" role="status" hidden></span></span>
        <label class="switch"><input type="checkbox" id="map_reveal_spawn"><span class="sl"></span></label>
        <span class="val" id="mrsval">off</span>
    </div>
    <div class="row" style="border:none">
        <span class="lbl" style="width:auto;flex:1">Pet collects quest items<br><span style="font-size:11px;color:#8f816e;font-weight:normal">While your pet is out, it walks to quest items on screen and picks them up for you - one at a time, crediting the quest objective exactly as collecting it by hand does. Only applies to pick-up quest items; things you activate, break or talk to are left alone.</span></span>
        <label class="switch"><input type="checkbox" id="mod_pet_quest_pickup"><span class="sl"></span></label>
        <span class="val" id="mpqpval">off</span>
    </div>
    <div class="row" style="border:none">
        <span class="lbl" style="width:auto;flex:1">Auto-prospect items put in the Prospect Cube<br><span style="font-size:11px;color:#8f816e;font-weight:normal">Every item you drag or click into the Prospect Cube's grid is prospected straight away, as if you had pressed Prospect, so the grid never fills with items waiting their turn. Anything still in the prospect grid when the game saves is lost.</span></span>
        <label class="switch"><input type="checkbox" id="mod_auto_prospect"><span class="sl"></span></label>
        <span class="val" id="autoprospval">off</span>
    </div>
    <div class="row" id="mod_auto_prospect_bag_row" style="border:none;margin-left:22px;border-left:1px solid #33261c;padding-left:14px">
        <span class="lbl" style="width:auto;flex:1">&#8627; Move the previous materials to your materials tab<br><span style="font-size:11px;color:#8f816e;font-weight:normal">When the next item is prospected, the materials from the prospect before it go from the grid to your materials tab first, the way clicking them does. The newest batch stays in the grid where you can see it. A material the game will not take stays in the grid.</span></span>
        <label class="switch"><input type="checkbox" id="mod_auto_prospect_bag"><span class="sl"></span></label>
        <span class="val" id="apbagval">on</span>
    </div>
    <div class="row" style="border:none">
        <span class="lbl" style="width:auto;flex:1">Craft from the stash<br><span style="font-size:11px;color:#8f816e;font-weight:normal">Crafting Cube recipes also count the materials and socketables in your stash's Materials and Socketable tabs. When you craft, only what your bag is short of leaves the stash, and the stash is saved right after. Off by default.</span></span>
        <label class="switch"><input type="checkbox" id="mod_craft_mats"><span class="sl"></span></label>
        <span class="val" id="mcmval">off</span>
    </div>
    <div class="row" style="border:none">
        <span class="lbl" style="width:auto;flex:1">Mark a running toggle skill<br><span style="font-size:11px;color:#8f816e;font-weight:normal">For a fixed set of toggle skills, each measured in-game: draws a soft red outline around that skill's skill-bar slot while its toggle is running, so you can see at a glance that it is still active. The outline disappears when the toggle ends. A plain cast, made without the skill's toggle sub-talent, lights nothing.</span></span>
        <label class="switch"><input type="checkbox" id="mod_toggle_indicator"><span class="sl"></span></label>
        <span class="val" id="mtival">off</span>
    </div>
    <div class="row" style="border:none">
        <span class="lbl" style="width:auto;flex:1">Stop double cast re-casting a toggle skill<br><span style="font-size:11px;color:#8f816e;font-weight:normal">For that same fixed set of toggle skills: a double cast proc can cast one of them a second time on its own, which flips its toggle straight back to where it was before your press. With this on, that extra cast is skipped, so the toggle stays the way you set it. It only steps in when you actually have the skill's toggle sub-talent, or the skill is a toggle on its own; your own presses are never affected.</span></span>
        <label class="switch"><input type="checkbox" id="mod_toggle_guard"><span class="sl"></span></label>
        <span class="val" id="mtgval">off</span>
    </div>
    <div class="row" style="border:none">
        <span class="lbl" style="width:auto;flex:1">Restart zone at any time<br><span style="font-size:11px;color:#8f816e;font-weight:normal">The pause menu's Restart normally waits until you have been out of combat for a few seconds; with this on it works straight away. Use the mouse: in combat Restart still looks greyed until the cursor is on it, then lights up and works when clicked.</span></span>
        <label class="switch"><input type="checkbox" id="mod_restart_anytime"><span class="sl"></span></label>
        <span class="val" id="mraval">off</span>
    </div>
    <div class="row" style="border:none">
        <span class="lbl" style="width:auto;flex:1">Timed skill countdown<br><span style="font-size:11px;color:#8f816e;font-weight:normal">Shows how much time a timed skill has left, over that skill's slot on the skill bar, in the look you pick below. Works for most timed skills; toggles and companions (turrets, totems) don't get one. Off by default.</span></span>
        <select class="style-select" id="mod_skill_timer_style">
            <option value="off">Off</option>
            <option value="arc">Arc</option>
            <option value="bar">Bar</option>
            <option value="number">Number</option>
            <option value="fade">Fade</option>
        </select>
    </div>
</div>

<div class="card tab-card" data-tab="mods" id="itemsCard" role="tabpanel" aria-labelledby="subtab-items">
  <h2>Items</h2>
  <div class="row" id="minerHelmetCard">
    <div>
      <strong>Miner's Helmet</strong>
      <p class="hint">+1000 Defense &middot; +500% Enhanced Defense<br>+20% Movement Speed &middot; +20% All Resistances &middot; +5 Light Radius</p>
      <p class="hint">While worn, every mining node gives exactly 4&times; its ore. This replaces the Mining Ore Amount slider instead of stacking with it; with the helmet off, the slider applies as usual. <strong>Vein Resonance:</strong> finishing a dig also digs the two nearest veins within 192 units that you could mine yourself, each at 4&times;, through the game's own dig. A vein dug this way never starts another.</p>
      <p class="hint">Forge it in the Item Editor: Item Forge &rarr; Forge a signature item &rarr; Miner's Helmet.</p>
      <div id="minerHelmetStatus" role="status" aria-live="polite">Start the game to check the helmet.</div>
    </div>
  </div>
  <div class="hint">Custom forge mechanics tied to items made in the Item Editor. Settings apply immediately while the game is running.</div>
  <div class="row" style="border:none">
    <span class="lbl" style="width:auto;flex:1">Headhunter buffs on rare kills<br><span style="font-size:11px;color:#8f816e;font-weight:normal">For an item forged with Mechanic: Headhunter. While on, killing a rare or champion monster grants its affixes to you as 20-second buffs (Extra Fast &rarr; movement speed, Berserker/Raging/Enraged &rarr; attack speed, Vampiric &rarr; life replenish, elemental Enchanted &rarr; cast rate, others &rarr; movement speed for now). The equipped-belt check is still in progress, so the effect is active whenever this switch is on and the forged item exists.</span></span>
    <label class="switch"><input type="checkbox" id="headhunter"><span class="sl"></span></label>
    <span class="val" id="hhval">on</span>
  </div>
  <div class="row" style="border:none">
    <span class="lbl" style="width:auto;flex:1">Tyrant's Crown: more rares, richer rares<br><span style="font-size:11px;color:#8f816e;font-weight:normal">For an item forged with Mechanic: Tyrant's Crown. While on, normal monsters near you rise to rare more often (15% each) and every rare or champion carries one extra affix. Pairs with Headhunter: more rares, more affixes to steal.</span></span>
    <label class="switch"><input type="checkbox" id="tyrant"><span class="sl"></span></label>
    <span class="val" id="tyval">on</span>
  </div>
  <div class="row" style="border:none">
    <span class="lbl" style="width:auto;flex:1">Beacon: every monster hunts you<br><span style="font-size:11px;color:#8f816e;font-weight:normal">For an amulet forged with Mechanic: Beacon. While on, every monster on the map hunts you the moment it spawns and never turns back, through the game's own aggro system. Plugin commands: beaconmode rare limits it to rares and champions, beaconrange &lt;px&gt; caps the distance.</span></span>
    <label class="switch"><input type="checkbox" id="beacon"><span class="sl"></span></label>
    <span class="val" id="beval">on</span>
  </div>
</div>

</div>
<div class="note" id="chipApply" role="status"></div>
</main></div>
<div id="toast" role="status"></div>
<script>
""" + POLL_POLICY_JS + ICON_MAP_JS + r"""
let ST=null, tmr=null;
function iconMarkup(name){
  return name?`<svg class="setting-icon" data-icon="${name}" viewBox="0 0 32 32" aria-hidden="true" focusable="false"><use href="#fp-icon-${name}"></use></svg>`:'';
}
function decorateIconLabel(label,name){
  if(!label||!name||label.querySelector('.setting-icon'))return;
  const copy=document.createElement('span');copy.className='label-copy';
  while(label.firstChild)copy.append(label.firstChild);
  label.classList.add('has-setting-icon');label.innerHTML=iconMarkup(name);label.append(copy);
}
function decoratePanelIcons(){
  document.querySelectorAll('input[data-sec][data-key]').forEach(input=>
    decorateIconLabel(input.closest('.row')?.querySelector('.lbl'),PANEL_ICON_MAP.controls[input.dataset.sec]?.[input.dataset.key]));
  for(const [id,name] of Object.entries(PANEL_ICON_MAP.static))
    decorateIconLabel(document.getElementById(id)?.closest('.row')?.querySelector('.lbl'),name);
  for(const [id,name] of Object.entries(PANEL_ICON_MAP.sections))
    decorateIconLabel(document.getElementById(id)?.querySelector('h2'),name);
  for(const [id,name] of Object.entries(PANEL_ICON_MAP.actions)){
    const button=document.getElementById(id);
    if(!button.querySelector('.setting-icon')){
      button.textContent=button.textContent.replace(/^[\u{1F4C1}\u25B6]\s*/u,'');
      decorateIconLabel(button,name);
    }
  }
  document.querySelectorAll('.group-title').forEach((label,i)=>decorateIconLabel(label,['experience','damage','defense','critical-chance'][i]));
  decorateIconLabel(document.querySelector('.modifier-card h2'),'damage');
}
// Serialize panel writes because each server request saves the whole config.
let writeQueue=Promise.resolve(),pendingWrites=0;
async function j(u,opt){
  if(!opt||opt.method!=='POST'){const r=await fetch(u,opt);return r.json()}
  pendingWrites++;
  const indicator=document.getElementById('saveIndicator');
  indicator.textContent='Saving...';indicator.className='saving';
  const run=async()=>{
    try{
      const r=await fetch(u,opt),result=await r.json();
      if(!r.ok&&!result.err)result.err='Request failed ('+r.status+')';
      if(result.cfg&&ST)ST.cfg=result.cfg;
      indicator.textContent=result.err?'Could not save':u==='/api/set'?'✓ Saved':'Request completed';
      indicator.className=result.err?'error':'';
      return result;
    }catch(e){
      // A disconnected response may still have committed the settings. Read
      // them back before painting controls instead of assuming the write failed.
      if(u==='/api/set'){
        try{const r=await fetch('/api/state');if(r.ok){const state=await r.json();if(state.cfg&&ST)ST.cfg=state.cfg}}catch(_){}
      }
      indicator.textContent='Connection lost · retry';indicator.className='error';
      return {err:'Could not reach ForgePact. Reconnect and try again.'};
    }finally{
      pendingWrites--;
      if(pendingWrites){indicator.textContent='Saving...';indicator.className='saving'}
      setTimeout(()=>{if(!pendingWrites){refreshSavedControls();filterControlRows()}},0);
    }
  };
  const request=writeQueue.then(run,run);
  writeQueue=request.catch(()=>{});
  return request;
}
function toast(m){const t=document.getElementById('toast');t.textContent=m;t.classList.add('show');clearTimeout(tmr);tmr=setTimeout(()=>t.classList.remove('show'),2200)}
const PAGE_INFO={
  setup:['Game setup','Connect your offline game and manage the mod plugin.'],
  modifiers:['Character modifiers','Tune your character and combat bonuses.'],
  world:['World settings','Shape your zones. Keep every choice in sight.'],
  loot:['Loot settings','Adjust drop rates and see exactly what each multiplier changes.'],
  mods:['Mods','Choose the features you want for your offline adventure.']
};
let activeTab='modifiers',controlFilter='all',modsSubtab='qolCard';
function openTab(name,remember=true){
  if(!document.querySelector(`.tabbtn[data-tab="${name}"]`))name='modifiers';
  activeTab=name;
  document.querySelectorAll('.tabbtn').forEach(b=>{const on=b.dataset.tab===name;b.classList.toggle('active',on);b.setAttribute('aria-selected',on?'true':'false');b.tabIndex=on?0:-1});
  document.querySelectorAll('.tab-card').forEach(c=>{c.hidden=false;c.classList.toggle('active',c.dataset.tab===name)});
  document.getElementById('modsSubtabs').hidden=name!=='mods';
  if(name==='mods')openModsSubtab(modsSubtab,false);
  document.getElementById('workspace').setAttribute('aria-labelledby','nav-'+name);
  document.getElementById('pageTitle').textContent=PAGE_INFO[name][0];
  document.getElementById('pageDescription').textContent=PAGE_INFO[name][1];
  document.getElementById('breadcrumbPage').textContent=name[0].toUpperCase()+name.slice(1);
  document.getElementById('controlToolbar').hidden=!['loot','modifiers'].includes(name);
  document.getElementById('controlSearch').value='';controlFilter='all';filterControlRows();
  if(remember){try{sessionStorage.setItem('forgepact_tab',name)}catch(e){}}
  window.scrollTo({top:0,behavior:'instant'});
}
function openModsSubtab(id,remember=true){
  const buttons=[...document.querySelectorAll('.subtabbtn')];
  const target=buttons.some(b=>b.getAttribute('aria-controls')===id)?id:buttons[0].getAttribute('aria-controls');
  modsSubtab=target;
  buttons.forEach(b=>{
    const on=b.getAttribute('aria-controls')===target;
    b.classList.toggle('active',on);b.setAttribute('aria-selected',on?'true':'false');b.tabIndex=on?0:-1;
    if(activeTab==='mods')document.getElementById(b.getAttribute('aria-controls')).classList.toggle('active',on);
  });
  if(remember){try{sessionStorage.setItem('forgepact_mods_subtab',target)}catch(e){}}
}
function bindModsSubtabs(){
  document.querySelectorAll('.subtabbtn').forEach(button=>button.onclick=()=>openModsSubtab(button.getAttribute('aria-controls')));
  document.querySelectorAll('.subtabbtn').forEach((button,index,buttons)=>button.onkeydown=e=>{
    const direction=e.key==='ArrowRight'?1:e.key==='ArrowLeft'?-1:0;
    if(!direction&&!['Home','End'].includes(e.key))return;
    e.preventDefault();const next=e.key==='Home'?0:e.key==='End'?buttons.length-1:(index+direction+buttons.length)%buttons.length;
    buttons[next].click();buttons[next].focus();
  });
}
function angelicPaint(){
  const el=document.getElementById('angelic_items'); const v=sliderVal(el);
  const dice=Math.max(0,Math.round(v)-1); const oneIn=dice>0?Math.max(1,Math.round(7500/dice)):0;
  const val=document.getElementById('angelicval'); val.textContent=v>1?'x'+v:'off'; val.className='val '+(v>1?'':'off');
  document.getElementById('angelicnote').textContent=oneIn>0?`about 1 Angelic or Unholy item in ${oneIn.toLocaleString()} kills (${dice} ${dice>1?'dice':'die'} per kill at 1 in 7,500)`:'off - the game rolls only with an Angelic drop-chance effect';
}
function rarityPaint(){
  const r=sliderVal(document.getElementById('rarity_rare')), a=sliderVal(document.getElementById('rarity_ancient'));
  const rv=document.getElementById('rarityrareval'), av=document.getElementById('rarityancval');
  rv.textContent=r>0?r+'%':'off'; rv.className='val '+(r>0?'':'off');
  av.textContent=a>0?a+'%':'off'; av.className='val '+(a>0?'':'off');
  document.getElementById('raritynote').textContent=(r>0||a>0)?`of the normal monsters: ${a}% Ancient, ${r}% Rare, ${Math.max(0,100-r-a)}% stay normal`:'off - the game rolls rarity on its own';
}
function rarityLoad(c){
  document.getElementById('angelic_items').value=+(c.angelic_items||1); angelicPaint();
  document.getElementById('rarity_rare').value=+(c.rarity_rare||0);
  document.getElementById('rarity_ancient').value=+(c.rarity_ancient||0);
  rarityPaint();
}
// The monster half only does anything while the parent reveal is on, so the
// control is disabled and reads "n/a" rather than silently claiming to be on.
function syncRevealPacks(parentOn,packsOn,spawnOn){
  const row=document.getElementById('map_reveal_packs_row');
  const box=document.getElementById('map_reveal_packs');
  const val=document.getElementById('mrpval');
  if(!row||!box||!val)return;
  box.disabled=!parentOn;
  row.title=parentOn?'':'Enable Reveal full map first.';
  val.textContent=parentOn?(packsOn?'on':'off'):'n/a';
  val.className='val '+(parentOn&&packsOn?'':'off');
  // The heavy spawn pass is a second child of the same parent.
  const srow=document.getElementById('map_reveal_spawn_row');
  const sbox=document.getElementById('map_reveal_spawn');
  const sval=document.getElementById('mrsval');
  if(!srow||!sbox||!sval)return;
  sbox.disabled=!parentOn;
  srow.title=parentOn?'':'Enable Reveal full map first.';
  sval.textContent=parentOn?(spawnOn?'on':'off'):'n/a';
  sval.className='val '+(parentOn&&spawnOn?'':'off');
}
// Likewise the move to the materials tab only does anything while
// Auto-prospect is on.
// The plugin can refuse a switch for the rest of a session: the insert hook
// went in table-only, or the move pass shut itself down after a material it
// could not account for. `pluginMods` is what the plugin says it is doing;
// the switch keeps the saved preference, and the value beside it says what is
// actually happening, with the reason on hover (review of #54).
function applyPluginModState(pm){
  const packMarkerStatus=document.getElementById('packMarkerStatus'), packMarkers=pm?.packMarkers;
  if(packMarkerStatus){
    packMarkerStatus.hidden=!(ST?.gameRunning&&ST?.cfg?.map_reveal&&ST?.cfg?.map_reveal_packs&&packMarkers);
    packMarkerStatus.textContent=!packMarkers?'':
      packMarkers.hook==='failed'?'Pack markers unavailable: the minimap layer could not be hooked on this game version.':
      packMarkers.hook==='table'?'Pack markers may not draw on this game version (minimap hook attached table-only).':
      packMarkers.hook==='pending'?'Pack markers start once the game has settled.':
      packMarkers.marked>0?packMarkers.marked+' packs marked in this zone'+(packMarkers.spawned?' · '+packMarkers.spawned+' born so far':'')+'.':
      'No unspawned packs marked in this zone.';
  }
  const populationStatus=document.getElementById('populationStatus'), population=pm?.population;
  if(populationStatus){
    populationStatus.hidden=!(ST?.gameRunning&&ST?.cfg?.map_reveal&&ST?.cfg?.map_reveal_spawn&&population);
    populationStatus.textContent=!population?'':!population.capacityReady?'Early population unavailable: '+population.reason:
      !population.canPopulate?'Early population paused: '+population.reason:
      population.densityCopyReason?'Population waiting: '+population.densityCopyReason:
      population.unconfirmedPacks>0?'Map population is unverified: '+population.unconfirmedPacks+' groups could not be confirmed.':
      population.targetExceeded?'This zone exceeded the 5 s target.'+(population.queuedPacks?' '+population.queuedPacks+' groups waiting.':'')+(population.queuedDensityCopies?' '+population.queuedDensityCopies+' density copies waiting.':''):
      population.windowFrames>0||population.queuedDensityCopies>0?'Populating the map · 5 s target'+(population.queuedPacks?' · '+population.queuedPacks+' groups waiting':'')+(population.queuedDensityCopies?' · '+population.queuedDensityCopies+' density copies waiting':'')+'.':
      'Ready for the next zone.';
  }
  const helmet=pm?.minerHelmet;
  const helmetStatus=document.getElementById('minerHelmetStatus');
  if(helmetStatus)helmetStatus.textContent=!ST?.gameRunning?'Start the game to check the helmet.':
    !helmet?.available?'Waiting for the ForgePact plugin to report the helmet.':
    helmet.enabled?(helmet.reason||'Checking the equipped helmet...')+(helmet.bonusVeins>0?' \u00b7 Vein Resonance has dug '+helmet.bonusVeins+' extra veins this session.':''):
    'No Miner\'s Helmet loaded yet.';
  const miningNote=document.querySelector('.note[data-note="mining_ore"]');
  if(miningNote){
    const requested=Number(ST?.cfg?.drops?.mining_ore||1), mining=pm?.miningOre;
    let status='';
    if(ST?.gameRunning&&helmet?.enabled){
      status=' Miner\'s Helmet: x4 replaces this slider while the helmet is worn; with the helmet off, this slider applies.';
    }else if(ST?.gameRunning&&requested>1){
      if(mining?.unavailable)status=' Plugin could not enable this feature; mining remains at x1.';
      else if(mining?.ready&&mining.multiplier===requested)status=' Plugin ready at x'+requested+'.';
      else status=' Waiting for the matching mining plugin to confirm the setting.';
    }
    miningNote.textContent='Multiplies ore from mining. x1 is normal. Only the ore amount is rewritten; ore types, mining XP and other drops are left to the game.'+status;
  }
  const ap=(pm&&pm.autoprospect)||null;
  const parentVal=document.getElementById("autoprospval");
  const bagVal=document.getElementById("apbagval");
  const bagRow=document.getElementById("mod_auto_prospect_bag_row");
  if(!ap||!parentVal||!bagVal||!bagRow)return;
  const reason=ap.reason||"";
  const parentOn=document.getElementById("mod_auto_prospect").checked;
  const wantsBag=document.getElementById("mod_auto_prospect_bag").checked;
  if(ap.hookBlind){
    parentVal.textContent="off (plugin)";
    parentVal.className="val off";
    parentVal.title=reason||"the plugin turned auto-prospect off for this session";
  }else{
    // Recovery matters as much as the failure: a new launch reports healthy
    // state, and the label has to go back to the saved switch by itself -
    // the toast promised it starts again next launch (review of #54).
    parentVal.textContent=parentOn?"on":"off";
    parentVal.className="val "+(parentOn?"":"off");
    parentVal.title="";
  }
  if(wantsBag&&ap.bagPreference&&!ap.movePass&&!ap.hookBlind){
    bagVal.textContent="off (plugin)";
    bagVal.className="val off";
    bagRow.title=(reason||"the plugin turned the move off for this session")+" - it starts again next launch.";
  }else if(!ap.hookBlind){
    syncProspectBag(parentOn,wantsBag);   // repaints the label and the disabled state
  }
}
function syncProspectBag(parentOn,bagOn){
  const row=document.getElementById('mod_auto_prospect_bag_row');
  const box=document.getElementById('mod_auto_prospect_bag');
  const val=document.getElementById('apbagval');
  if(!row||!box||!val)return;
  box.disabled=!parentOn;
  row.title=parentOn?'':'Enable Auto-prospect first.';
  val.textContent=parentOn?(bagOn?'on':'off'):'n/a';
  val.className='val '+(parentOn&&bagOn?'':'off');
}
function sliderOff(sec,v){return sec==='percent_stats'?v<=0:v<=1}
function sliderText(sec,v){return sliderOff(sec,v)?'off':(sec==='percent_stats'?'+'+v+'%':'x'+v)}
function row(sec,key,label,val,tagHtml,max,note,step){
  const mx=max||100, off=sliderOff(sec,val), mn=sec==='percent_stats'?0:1;
  const n=note?`<div class="note" data-note="${key}">${note}</div>`:'';
  return `<div class="row"><span class="lbl">${label}${tagHtml||''}</span>
    <input type="range" min="${mn}" max="${mx}" step="${step||1}" value="${val}" data-sec="${sec}" data-key="${key}">
    <span class="val ${off?'off':''}" style="width:64px" title="Click to type a value">${sliderText(sec,val)}</span></div>${n}`;
}
function satRow(polarity,id,name,desc,enabled){
  const esc=s=>String(s).replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
  return `<label class="sat-option ${enabled?'is-enabled':''}">
    <input type="checkbox" data-sat-polarity="${polarity}" data-sat-id="${esc(id)}" ${enabled?'checked':''}
      aria-labelledby="sat-${polarity}-${esc(id)}" aria-describedby="sat-desc-${polarity}-${esc(id)} sat${polarity}Help">
    <span><span class="sat-name" id="sat-${polarity}-${esc(id)}">${iconMarkup(PANEL_ICON_MAP.satanic[polarity]?.[id])}<span>${esc(name)}</span></span>
      <span class="sat-desc" id="sat-desc-${polarity}-${esc(id)}">${esc(desc)}</span></span>
  </label>`;
}
// Click the value next to a slider to type it.  Sliders with 100-200 steps on a
// 200 px track skip values (80, 85, 95 ...); typing lands exactly.  Enter or
// leaving the box applies through the slider's own handlers, Escape cancels.
// A slider's value once the user is done with it: a typed value is taken as is; a dragged
// value snaps to the slider's own step (the range keeps step="any" after typing so the
// browser does not round the typed number away).
function sliderVal(r){
  let v=parseFloat(r.value); if(!isFinite(v))v=0;
  if(r.dataset.typed==='1')return v;
  const st=parseFloat(r.dataset.step0||r.step)||0;
  if(r.step==='any'&&st>0){v=Math.round(v/st)*st;v=+v.toFixed(3);r.value=v;}
  return v;
}
function typable(r,valEl){
  if(!r||!valEl||valEl.dataset.typable)return;
  valEl.dataset.typable='1'; valEl.style.cursor='text'; valEl.title='Click to type a value';
  valEl.onclick=()=>{
    if(valEl.querySelector('input'))return;
    const inp=document.createElement('input');
    inp.type='number'; inp.setAttribute('aria-label',r.getAttribute('aria-label')||'Setting value'); inp.className='numedit'; inp.min=r.min; inp.max=r.max; inp.step='any'; inp.value=r.value;
    valEl.textContent=''; valEl.appendChild(inp); inp.focus(); inp.select();
    let finished=false;
    const done=async(apply)=>{
      if(finished)return; finished=true;
      let v=parseFloat(inp.value);
      if(apply&&isFinite(v)){
        const mn=parseFloat(r.min), mx=parseFloat(r.max);
        v=Math.min(mx,Math.max(mn,v)); v=+v.toFixed(2);
        if(!r.dataset.step0)r.dataset.step0=r.step||'1';
        r.step='any'; r.value=v; r.dataset.typed='1';
        try{ if(r.oninput)r.oninput(); if(r.onchange)await r.onchange(); } finally { delete r.dataset.typed; }
      } else { if(r.oninput)r.oninput(); else valEl.textContent=r.value; }
      if(!pendingWrites){refreshSavedControls();filterControlRows()}
    };
    inp.onkeydown=(e)=>{ if(e.key==='Enter'){e.preventDefault();done(true);} else if(e.key==='Escape'){e.preventDefault();done(false);} };
    inp.onblur=()=>done(true);
  };
}
// "x2" on its own says nothing - it means something different per family.
// Chaos/Bifrost already have their gate open, so x2 really is double there.  For
// Dungeon and Angelic we make the game roll a die it normally never rolls.  For
// Relic the vanilla rate outside the home zone is ZERO, so there is no "multiple"
// at all - the slider decides how often the die is rolled.
// Experience is not a drop: the game's own calculation runs untouched and only
// its RESULT is multiplied, so your own XP bonuses survive and the slider always
// gives a true multiple.
function statNote(key,v){
  if(v<=1) return 'off';
  if(key==='exp') return `${v}x experience per kill, on top of your own bonuses`;
  if(key==='magicfind') return `${v}x your current total Magic Find, including all bonuses`;
  if(key==='movespeed') return `${v}x your current total Movement Speed, including all bonuses`;
  return `${v}x the current total`;
}
function percentStatNote(key,v){
  if(v<=0) return key==='damage' ? 'off - vanilla damage' : 'off - vanilla value';
  if(key==='damage') return `adds ${v}% to the final hit after the game finishes its own calculation (+100% doubles it)`;
  if(key==='castrate') return `adds ${v} Faster Cast Rate points to the current value`;
  if(key==='critchance'||key==='spellcritchance') return `increases the current Critical Strike Chance by ${v}% (the game's own cap still applies)`;
  return `adds ${v}% to the final value`;
}
function rareNote(key,v){
  if(v<=1) return 'off';
  if(key==='angelic'){
    if(v===2) return "the game's own angelic rate - it never rolls at all without this";
    return `the game's own angelic rate, multiplied ${v-1}x (measured: x10 works, higher breaks the game's check)`;
  }
  if(key==='heroic'){
    const p=Math.min(100,Math.round(28*v));
    return `${p}% chance for a Heroic item per drop (vanilla 28%)`;
  }
  if(key==='ceiling'){
    return `${v}x on every rare tier at once - takes effect when the NEXT map loads`;
  }
  if(key==='satanic'){
    return `monsters count as ${v}x their level for the Satanic tier roll (capped at level 200)`;
  }
  return `${v}x`;
}
function keyNote(key,dropType,v){
  if(v<=1) return 'off';
  if(key==='ruby') return `${v}x the key's own vanilla roll (base 1,500,000)`;
  if(dropType===null||dropType===undefined) return `${v}x its vanilla drop rate, only where the game drops it anyway`;
  if(key==='relic'){
    // Same curve as the plugin:  probability = 0.00025 * v^2  (clamped at 1.0)
    const p=Math.min(1,0.00025*v*v);
    return (p>=1)?'rolls on every kill':`rolls on about 1 kill in ${Math.round(1/p).toLocaleString()}`;
  }
  return `${v}x its vanilla drop rate; where the game never rolls this family, the roll is opened at the normal-key chance first`;
}
async function boot(){
  ST=await j('/api/state');
  const c=ST.cfg;
  document.querySelectorAll('.tabbtn').forEach(b=>b.onclick=()=>openTab(b.dataset.tab));
  let initial=c.game_exe?'modifiers':'setup';
  try{initial=sessionStorage.getItem('forgepact_tab')||initial}catch(e){}
  try{modsSubtab=sessionStorage.getItem('forgepact_mods_subtab')||modsSubtab}catch(e){}
  openTab(initial,false);
  document.getElementById('autoapply').checked=!!c.auto_apply;
  document.getElementById('den_on').checked=!!c.density_on;
  const esp=+(c.enemy_speed||0), esc=(c.enemy_speed_ct!==false);
  document.getElementById('enemyspeed').value=esp;
  document.getElementById('enemyspeedval').textContent=esp>0?'+'+esp+'%':'off';
  document.getElementById('enemyspeedval').className='val '+(esp>0?'':'off');
  document.getElementById('enemyspeed_ct').checked=esc;
  document.getElementById('enemyspeedctval').textContent=esc?'CT only':'all zones';
  document.getElementById('den').value=c.density;
  document.getElementById('denval').textContent=(c.density_on?'x'+c.density:'off');
  document.getElementById('denval').className='val '+(c.density_on?'':'off');
  const mr=c.map_reveal!==false;
  document.getElementById('map_reveal').checked=mr;
  document.getElementById('mapval').textContent=mr?'on':'off';
  document.getElementById('mapval').className='val '+(mr?'':'off');
  const mrp=c.map_reveal_packs!==false;
  document.getElementById('map_reveal_packs').checked=mrp;
  const mrs=!!c.map_reveal_spawn;
  document.getElementById('map_reveal_spawn').checked=mrs;
  syncRevealPacks(mr,mrp,mrs);
  const hh=!!c.headhunter;
  document.getElementById('headhunter').checked=hh;
  document.getElementById('hhval').textContent=hh?'on':'off';
  const ty=!!c.tyrant;
  document.getElementById('tyrant').checked=ty;
  document.getElementById('tyval').textContent=ty?'on':'off';
  document.getElementById('tyval').className='val '+(ty?'':'off');
  const be=!!c.beacon;
  document.getElementById('beacon').checked=be;
  document.getElementById('beval').textContent=be?'on':'off';
  document.getElementById('beval').className='val '+(be?'':'off');
  const mfmr=!!c.mod_filter_max_relics;
  document.getElementById('mod_filter_max_relics').checked=mfmr;
  document.getElementById('mfmrval').textContent=mfmr?'on':'off';
  document.getElementById('mfmrval').className='val '+(mfmr?'':'off');
    const mor=!!c.mod_orb_pickup_radius;
    document.getElementById('mod_orb_pickup_radius').checked=mor;
    document.getElementById('morval').textContent=mor?'on':'off';
    document.getElementById('morval').className='val '+(mor?'':'off');
    const mpqp=!!c.mod_pet_quest_pickup;
    document.getElementById('mod_pet_quest_pickup').checked=mpqp;
    document.getElementById('mpqpval').textContent=mpqp?'on':'off';
    document.getElementById('mpqpval').className='val '+(mpqp?'':'off');
    const maps=!!c.mod_auto_prospect;
    document.getElementById('mod_auto_prospect').checked=maps;
    document.getElementById('autoprospval').textContent=maps?'on':'off';
    document.getElementById('autoprospval').className='val '+(maps?'':'off');
    const apbag=c.mod_auto_prospect_bag!==false;
    document.getElementById('mod_auto_prospect_bag').checked=apbag;
    syncProspectBag(maps,apbag);
    const mti=!!c.mod_toggle_indicator;
    document.getElementById('mod_toggle_indicator').checked=mti;
    document.getElementById('mtival').textContent=mti?'on':'off';
    document.getElementById('mtival').className='val '+(mti?'':'off');
    const mtg=!!c.mod_toggle_guard;
    document.getElementById('mod_toggle_guard').checked=mtg;
    document.getElementById('mtgval').textContent=mtg?'on':'off';
    document.getElementById('mtgval').className='val '+(mtg?'':'off');
    const mra=!!c.mod_restart_anytime;
    document.getElementById('mod_restart_anytime').checked=mra;
    document.getElementById('mraval').textContent=mra?'on':'off';
    document.getElementById('mraval').className='val '+(mra?'':'off');
    const mcm=!!c.mod_craft_mats;
    document.getElementById('mod_craft_mats').checked=mcm;
    document.getElementById('mcmval').textContent=mcm?'on':'off';
    document.getElementById('mcmval').className='val '+(mcm?'':'off');
    document.getElementById('mod_skill_timer_style').value=c.mod_skill_timer_style||'off';
  rarityLoad(c);
  document.getElementById('hhval').className='val '+(hh?'':'off');
  document.getElementById('exepath').value=c.game_exe||'';
  document.getElementById('spawners').innerHTML=ST.spawners.map(([k,i,l,mx])=>row('spawners',k,l,c.spawners[k]||1,'',mx)).join('');
  renderSatanicMods();
  document.getElementById('keys').innerHTML=ST.keys.map(([k,l,t])=>{
    const v=(c.keys&&c.keys[k])||1;
    return row('keys',k,l,v,'',100,keyNote(k,t,v));
  }).join('');
  document.getElementById('drops').innerHTML=ST.drops.map(([k,l,h])=>
    row('drops',k,l,(c.drops&&c.drops[k])||1,h?` <span class="tag">${h}</span>`:'',k==='mining_ore'?10:100,
      k==='mining_ore'?'Multiplies ore from mining. x1 is normal. Only the ore amount is rewritten; ore types, mining XP and other drops are left to the game.':'')).join('');
  document.getElementById('stats').innerHTML=(ST.stats||[]).map(([k,l,mx,step])=>{
    const v=(c.stats&&c.stats[k])||1;
    return row('stats',k,l,v,'',mx,statNote(k,v),step);
  }).join('');
  const percentRows=(keys)=>(ST.percentStats||[]).filter(([k])=>keys.includes(k)).map(([k,l,mx,step,mode])=>{
    const v=(c.percent_stats&&c.percent_stats[k])||0;
    return row('percent_stats',k,l,v,'',mx,percentStatNote(k,v),step);
  }).join('');
  document.getElementById('offensivestats').innerHTML=percentRows(['damage','attackspeed','castrate']);
  document.getElementById('sustainstats').innerHTML=percentRows(['lifereplenish','manareplenish','defense']);
  document.getElementById('criticalstats').innerHTML=percentRows(['critdamage','critchance','spellcritdamage','spellcritchance']);
  bind(); preparePanelUI(); refreshSavedControls(); status(); paintVersion();
  document.getElementById('saveIndicator').textContent='Settings loaded';
}
function paintVersion(){
  // Rendered from /api/state, never embedded in this page: the panel and the
  // plugin DLL are installed separately and can be different builds, and a
  // bug report needs to say which one it is looking at.
  const el=document.getElementById('panelver');
  if(el&&ST&&ST.version)el.textContent=' \u00b7 v'+ST.version;
}
let launcherBusy=false;
function renderLaunchStatus(){
  const info=ST?.launch;
  const btn=document.getElementById('launchgame');
  btn.disabled=launcherBusy||!!ST?.gameRunning||info?.phase==='starting';
  const box=document.getElementById('launchFeedback');
  if(info){box.textContent=info.message;box.className='launch-feedback '+info.phase;}
}
function status(){
  const g=document.getElementById('chipGame'), a=document.getElementById('chipApply');
  const ch=ST.chain||{};
  const ok=ch.patched&&ch.aurieCore&&ch.yytk&&ch.plugin;
  g.textContent=ST.gameRunning?(ok?'Game open':'Game open · plugin missing'):'Game offline';
  g.title=ST.gameRunning?'This detects the game process. The plugin must be installed and loaded to apply modifiers.':'Settings are saved locally. Auto-apply sends them on game launch when enabled.';
  g.className='chip '+(ST.gameRunning?(ok?'on':'warn'):'off');
  a.textContent=ST.lastApplied?('commands sent: '+ST.lastApplied+(ST.queued?' (queued)':'')):'No settings sent this session';
  a.className='chip '+(ST.lastApplied?'warn':'off');
  const warning=document.getElementById('pluginWarning');
  warning.hidden=!!ok;
  document.getElementById('pluginWarningText').textContent=ch.exeExists?
    'Plugin not installed. Your settings are saved, but modifiers cannot apply. Close the game, then install the plugin in Setup.':
    'Choose your Hero_Siege.exe in Setup, then install the plugin to use modifiers.';
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
  renderLaunchStatus();
}
function bind(){
  document.querySelectorAll('input[type=range][data-sec]').forEach(r=>{
    const valEl=r.parentElement.querySelector('.val');
    const noteEl=r.parentElement.parentElement.querySelector(`.note[data-note="${r.dataset.key}"]`);
    const tipOf=(k)=>{const e=(ST.keys||[]).find(x=>x[0]===k);return e?e[2]:undefined;};
    r.oninput=()=>{const v=sliderVal(r);valEl.textContent=sliderText(r.dataset.sec,v);valEl.className='val '+(sliderOff(r.dataset.sec,v)?'off':'');
      if(noteEl&&r.dataset.sec==='keys')noteEl.textContent=keyNote(r.dataset.key,tipOf(r.dataset.key),v);
      if(noteEl&&r.dataset.sec==='stats')noteEl.textContent=statNote(r.dataset.key,v);
      if(noteEl&&r.dataset.sec==='percent_stats')noteEl.textContent=percentStatNote(r.dataset.key,v);
    };
    r.onchange=async()=>{
      const v=sliderVal(r);
      const res=await j('/api/set',{method:'POST',body:JSON.stringify({section:r.dataset.sec,key:r.dataset.key,value:v})});
      toast((r.dataset.key)+' = '+sliderText(r.dataset.sec,v)+' - '+(res.ok||res.err));
    };
    typable(r,valEl);
  });
  const den=document.getElementById('den');
  den.oninput=()=>{const v=sliderVal(den);document.getElementById('denval').textContent=document.getElementById('den_on').checked?'x'+v:'off'};
  den.onchange=async()=>{const v=sliderVal(den);const res=await j('/api/set',{method:'POST',body:JSON.stringify({key:'density',value:v})});toast('density x'+v+' - '+(res.ok||res.err))};
  typable(den,document.getElementById('denval'));
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
  const esp=document.getElementById('enemyspeed');
  const espText=(v)=>v>0?'+'+v+'%':'off';
  esp.oninput=()=>{const v=sliderVal(esp);document.getElementById('enemyspeedval').textContent=espText(v);document.getElementById('enemyspeedval').className='val '+(v>0?'':'off');};
  esp.onchange=async()=>{
    const v=sliderVal(esp);
    const res=await j('/api/set',{method:'POST',body:JSON.stringify({key:'enemy_speed',value:v})});
    toast('enemy speed '+espText(v)+' - '+(res.ok||res.err));
  };
  typable(esp,document.getElementById('enemyspeedval'));
  document.getElementById('enemyspeed_ct').onchange=async(e)=>{
    const res=await j('/api/set',{method:'POST',body:JSON.stringify({key:'enemy_speed_ct',value:e.target.checked})});
    document.getElementById('enemyspeedctval').textContent=e.target.checked?'CT only':'all zones';
    toast('enemy speed scope: '+(e.target.checked?'Chaos Tower only':'all zones')+' - '+(res.ok||res.err));
  };
  document.getElementById('map_reveal').onchange=async(e)=>{
    // Repaint the pair BEFORE awaiting the POST. If the panel's server is
    // gone the fetch throws, and anything after the await never runs - which
    // left the child row enabled and reading "on" under a switched-off
    // parent, inviting a click that could do nothing.
    document.getElementById('mapval').textContent=e.target.checked?'on':'off';
    document.getElementById('mapval').className='val '+(e.target.checked?'':'off');
    syncRevealPacks(e.target.checked,document.getElementById('map_reveal_packs').checked,document.getElementById('map_reveal_spawn').checked);
    const res=await j('/api/set',{method:'POST',body:JSON.stringify({key:'map_reveal',value:e.target.checked})});
    toast('map reveal '+(e.target.checked?'ON':'OFF')+' - '+(res.ok||res.err));
  };
  document.getElementById('map_reveal_packs').onchange=async(e)=>{
    syncRevealPacks(document.getElementById('map_reveal').checked,e.target.checked,document.getElementById('map_reveal_spawn').checked);
    const res=await j('/api/set',{method:'POST',body:JSON.stringify({key:'map_reveal_packs',value:e.target.checked})});
    toast('pack markers '+(e.target.checked?'ON':'OFF')+' - '+(res.ok||res.err));
  };
  document.getElementById('map_reveal_spawn').onchange=async(e)=>{
    syncRevealPacks(document.getElementById('map_reveal').checked,document.getElementById('map_reveal_packs').checked,e.target.checked);
    const res=await j('/api/set',{method:'POST',body:JSON.stringify({key:'map_reveal_spawn',value:e.target.checked})});
    toast('spawn every pack '+(e.target.checked?'ON':'OFF')+' - '+(res.ok||res.err));
  };
  document.getElementById('headhunter').onchange=async(e)=>{
    const res=await j('/api/set',{method:'POST',body:JSON.stringify({key:'headhunter',value:e.target.checked})});
    document.getElementById('hhval').textContent=e.target.checked?'on':'off';
    document.getElementById('hhval').className='val '+(e.target.checked?'':'off');
    toast('headhunter '+(e.target.checked?'ON':'OFF')+' - '+(res.ok||res.err));
  };
  document.getElementById('tyrant').onchange=async(e)=>{
    const res=await j('/api/set',{method:'POST',body:JSON.stringify({key:'tyrant',value:e.target.checked})});
    document.getElementById('tyval').textContent=e.target.checked?'on':'off';
    document.getElementById('tyval').className='val '+(e.target.checked?'':'off');
    toast('tyrant '+(e.target.checked?'ON':'OFF')+' - '+(res.ok||res.err));
  };
  document.getElementById('beacon').onchange=async(e)=>{
    const res=await j('/api/set',{method:'POST',body:JSON.stringify({key:'beacon',value:e.target.checked})});
    document.getElementById('beval').textContent=e.target.checked?'on':'off';
    document.getElementById('beval').className='val '+(e.target.checked?'':'off');
    toast('beacon '+(e.target.checked?'ON':'OFF')+' - '+(res.ok||res.err));
  };
  document.getElementById('mod_filter_max_relics').onchange=async(e)=>{
    const res=await j('/api/set',{method:'POST',body:JSON.stringify({key:'mod_filter_max_relics',value:e.target.checked})});
    const v=document.getElementById('mfmrval');v.textContent=e.target.checked?'on':'off';v.className='val '+(e.target.checked?'':'off');
    toast('Remove owned relics from drop pool '+(e.target.checked?'ON':'OFF')+' - '+(res.ok||res.err));
  };
    document.getElementById('mod_orb_pickup_radius').onchange=async(e)=>{
        const res=await j('/api/set',{method:'POST',body:JSON.stringify({key:'mod_orb_pickup_radius',value:e.target.checked})});
        const v=document.getElementById('morval');v.textContent=e.target.checked?'on':'off';v.className='val '+(e.target.checked?'':'off');
        toast('Orb pickup radius '+(e.target.checked?'10x ON':'OFF')+' - '+(res.ok||res.err));
    };
    document.getElementById('mod_pet_quest_pickup').onchange=async(e)=>{
        const res=await j('/api/set',{method:'POST',body:JSON.stringify({key:'mod_pet_quest_pickup',value:e.target.checked})});
        const v=document.getElementById('mpqpval');v.textContent=e.target.checked?'on':'off';v.className='val '+(e.target.checked?'':'off');
        toast('Pet collects quest items '+(e.target.checked?'ON':'OFF')+' - '+(res.ok||res.err));
    };
    document.getElementById('mod_auto_prospect').onchange=async(e)=>{
        const res=await j('/api/set',{method:'POST',body:JSON.stringify({key:'mod_auto_prospect',value:e.target.checked})});
        const v=document.getElementById('autoprospval');v.textContent=e.target.checked?'on':'off';v.className='val '+(e.target.checked?'':'off');
        syncProspectBag(e.target.checked,document.getElementById('mod_auto_prospect_bag').checked);
        const pmp=await pluginModsAfterSet();
        if(e.target.checked&&pmp&&pmp.autoprospect&&pmp.autoprospect.hookBlind){
          toast('The plugin has auto-prospect off this session: '+(pmp.autoprospect.reason||'it could not attach to the game'));
        }else{
        toast('Auto-prospect '+(e.target.checked?'ON - '+(document.getElementById('mod_auto_prospect_bag').checked?'the previous materials go to your materials tab':'materials stay in the grid'):'OFF')+' - '+(res.ok||res.err));
        }
    };
    document.getElementById('mod_auto_prospect_bag').onchange=async(e)=>{
        syncProspectBag(document.getElementById('mod_auto_prospect').checked,e.target.checked);
        const res=await j('/api/set',{method:'POST',body:JSON.stringify({key:'mod_auto_prospect_bag',value:e.target.checked})});
        // The plugin may refuse this for the rest of the session; say what
        // it is doing, not what was asked for (review of #54).
        const pm=await pluginModsAfterSet();
        const ap=pm&&pm.autoprospect;
        if(e.target.checked&&ap&&ap.bagPreference&&!ap.movePass){
          toast('The plugin is not moving materials this session: '+(ap.reason||'it turned the move off')+' - it starts again next launch');
        }else{
          toast('Materials to your materials tab '+(e.target.checked?'ON':'OFF')+' - '+(res.ok||res.err));
        }
    };
    document.getElementById('mod_toggle_indicator').onchange=async(e)=>{
        const res=await j('/api/set',{method:'POST',body:JSON.stringify({key:'mod_toggle_indicator',value:e.target.checked})});
        const v=document.getElementById('mtival');v.textContent=e.target.checked?'on':'off';v.className='val '+(e.target.checked?'':'off');
        toast('Toggle-skill outline '+(e.target.checked?'ON':'OFF')+' - '+(res.ok||res.err));
    };
    document.getElementById('mod_toggle_guard').onchange=async(e)=>{
        const res=await j('/api/set',{method:'POST',body:JSON.stringify({key:'mod_toggle_guard',value:e.target.checked})});
        const v=document.getElementById('mtgval');v.textContent=e.target.checked?'on':'off';v.className='val '+(e.target.checked?'':'off');
        toast('Toggle-skill double cast guard '+(e.target.checked?'ON':'OFF')+' - '+(res.ok||res.err));
    };
    document.getElementById('mod_restart_anytime').onchange=async(e)=>{
        const res=await j('/api/set',{method:'POST',body:JSON.stringify({key:'mod_restart_anytime',value:e.target.checked})});
        const v=document.getElementById('mraval');v.textContent=e.target.checked?'on':'off';v.className='val '+(e.target.checked?'':'off');
        toast('Restart zone at any time '+(e.target.checked?'ON':'OFF')+' - '+(res.ok||res.err));
    };
    document.getElementById('mod_craft_mats').onchange=async(e)=>{
        const res=await j('/api/set',{method:'POST',body:JSON.stringify({key:'mod_craft_mats',value:e.target.checked})});
        const v=document.getElementById('mcmval');v.textContent=e.target.checked?'on':'off';v.className='val '+(e.target.checked?'':'off');
        toast('Craft from the stash '+(e.target.checked?'ON':'OFF')+' - '+(res.ok||res.err));
    };
    document.getElementById('mod_skill_timer_style').onchange=async(e)=>{
        const res=await j('/api/set',{method:'POST',body:JSON.stringify({key:'mod_skill_timer_style',value:e.target.value})});
        toast('Timed skill countdown: '+e.target.value+' - '+(res.ok||res.err));
    };
  { const el=document.getElementById('angelic_items');
    el.oninput=angelicPaint;
    el.onchange=async()=>{ const v=sliderVal(el); const res=await j('/api/set',{method:'POST',body:JSON.stringify({key:'angelic_items',value:v})}); angelicPaint(); toast('angelic drops '+(v>1?'x'+v:'off')+' - '+(res.ok||res.err)); };
    typable(el,document.getElementById('angelicval')); }
  for(const key of ['rarity_rare','rarity_ancient']){
    const el=document.getElementById(key);
    el.oninput=rarityPaint;
    el.onchange=async()=>{
      const res=await j('/api/set',{method:'POST',body:JSON.stringify({key,value:sliderVal(el)})});
      // the server may have cut the other share so the two stay within 100
      if(res.cfg) rarityLoad(res.cfg); else rarityPaint();
      toast('monster rarity: '+document.getElementById('raritynote').textContent+' - '+(res.ok||res.err));
    };
    typable(el,document.getElementById(key==='rarity_rare'?'rarityrareval':'rarityancval'));
  }
  document.getElementById('applyall').onclick=async()=>{
    const res=await j('/api/applyall',{method:'POST',body:'{}'});
    toast(res.ok||res.err); if(!res.err)ST.lastApplied=new Date().toTimeString().slice(0,8); status();
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
    if(launcherBusy||ST.gameRunning||ST.launch?.phase==='starting')return;
    const btn=document.getElementById('launchgame'),old=btn.innerHTML;
    launcherBusy=true;btn.disabled=true;btn.textContent='Starting offline...';
    const box=document.getElementById('launchFeedback');
    box.textContent='Checking the game and Steam...';box.className='launch-feedback starting';
    try{
      const res=await j('/api/launch',{method:'POST',body:'{}'});
      ST.launch=res.launch||{phase:res.err?'error':'started',message:res.err||res.ok};
      toast(res.ok||res.err);
    }finally{
      launcherBusy=false;btn.innerHTML=old;renderLaunchStatus();
    }
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
  bindSatanicMods();
}
function preparePanelUI(){
  const workspace=document.getElementById('workspace');
  if(!workspace.dataset.navigationReady){
    workspace.dataset.navigationReady='1';
    const compact=matchMedia('(max-width:720px)');
    const orient=()=>document.querySelector('.tabbar').setAttribute('aria-orientation',compact.matches?'horizontal':'vertical');
    compact.addEventListener('change',orient);orient();
  }
  // Real DOM order matches the visual and keyboard order.
  ['densityCard','rarityCard','satanicMods','speedCard','spawnsCard'].forEach(id=>workspace.appendChild(document.getElementById(id)));
  for(const id of ['densityCard','rarityCard'])document.getElementById(id).classList.add('half');
  const summaries={
    densityCard:'Adjust the number of monster packs in newly loaded zones.',
    rarityCard:'Choose the share of normal monsters upgraded to Rare or Ancient.',
    speedCard:'Increase enemy movement speed. Choose all zones or Chaos Tower only.',
    spawnsCard:'Choose how frequently special content appears in new zones.',
    dropsCard:'Multiply drop chances. ×1 keeps a drop at its normal rate.',
    angelicCard:'Extra chances to drop Angelic / Unholy items on monster kills.'
  };
  for(const [id,summary] of Object.entries(summaries)){
    const card=document.getElementById(id);
    if(card.dataset.prepared)continue;
    card.dataset.prepared='1';
    const hint=card.querySelector('.hint'),details=document.createElement('details');
    details.className='help-details';details.innerHTML='<summary>How it works</summary>';
    hint.before(Object.assign(document.createElement('div'),{className:'hint',textContent:summary}));
    details.append(hint);
    if(id==='densityCard'){
      const note=card.querySelector('.note');if(note)details.append(note);
      const head=document.createElement('div');head.className='density-top';
      const heading=card.querySelector('h2');heading.before(head);head.append(heading);
      const toggle=card.querySelector('.switch');const row=document.createElement('div');row.className='row';
      row.innerHTML='<span class="lbl">Enabled</span>';row.append(toggle);head.append(row);
      const hero=document.createElement('div');hero.className='hero-number';hero.id='densityHero';
      card.querySelector(':scope>.row').before(hero);
    }
    card.append(details);
  }
  for(const id of ['spawners','dropSettings'])document.getElementById(id).classList.add('settings-grid');
  for(const id of ['qolCard','itemsCard']){
    const card=document.getElementById(id);
    if(card.querySelector('.mods-grid'))continue;
    const grid=document.createElement('div');grid.className='mods-grid';
    card.querySelectorAll(':scope>.row').forEach(row=>{
      row.classList.add('feature-card');
      const description=row.querySelector('.lbl>span');
      if(description){row.querySelector('.lbl>br')?.remove();description.classList.add('feature-description');row.append(description)}
      grid.append(row);
    });
    card.append(grid);
    if(id==='qolCard'){
      const parent=document.getElementById('map_reveal').closest('.row'),child=document.getElementById('map_reveal_packs_row'),spawnChild=document.getElementById('map_reveal_spawn_row');
      const group=document.createElement('div');group.className='feature-with-child';parent.before(group);group.append(parent,child,spawnChild);
      const apParent=document.getElementById('mod_auto_prospect').closest('.row'),apChild=document.getElementById('mod_auto_prospect_bag_row');
      const apGroup=document.createElement('div');apGroup.className='feature-with-child';apParent.before(apGroup);apGroup.append(apParent,apChild);
    }
    setupModsColumns(grid);
  }
  document.querySelectorAll('input[type=range]').forEach((range,index)=>{
    const row=range.closest('.row');if(!row)return;
    const label=row.querySelector('.lbl')?.textContent.trim()||'Density multiplier';
    range.setAttribute('aria-label',label);
    if(!range.id)range.id='setting-'+(range.dataset.sec||'value')+'-'+(range.dataset.key||index);
    if(range.parentElement.classList.contains('range-control'))return;
    const value=row.querySelector('.val');if(!value)return;
    const controls=document.createElement('div');controls.className='range-control';range.before(controls);controls.append(range);
    const stepper=document.createElement('div');stepper.className='value-stepper';
    for(const direction of [-1,1]){
      const button=document.createElement('button');button.type='button';button.className='step-button';button.textContent=direction<0?'−':'+';
      button.setAttribute('aria-label',(direction<0?'Decrease ':'Increase ')+label);
      button.onclick=async()=>{
        const step=parseFloat(range.dataset.step0||range.step)||1;
        range.value=Math.max(+range.min,Math.min(+range.max,+(Number(range.value)+direction*step).toFixed(2)));
        range.dispatchEvent(new Event('input',{bubbles:true}));
        if(range.onchange)await range.onchange();
        updateControlDecoration();filterControlRows();
      };
      stepper.append(button);if(direction<0)stepper.append(value);
    }
    controls.append(stepper);
    value.setAttribute('role','button');value.tabIndex=0;value.setAttribute('aria-label','Edit '+label);
    value.onkeydown=e=>{if(e.target===value&&['Enter',' '].includes(e.key)){e.preventDefault();value.click()}};
    range.addEventListener('input',updateControlDecoration);
    if(range.dataset.sec){
      const entry=document.createElement('div');entry.className='setting-entry';
      const note=row.nextElementSibling?.matches('.note')?row.nextElementSibling:null;
      row.before(entry);entry.append(row);if(note)entry.append(note);
      entry.dataset.search=(label+' '+(note?.textContent||'')).toLowerCase();
    }
  });
  document.querySelectorAll('.switch input').forEach(box=>{
    if(!box.getAttribute('aria-label'))box.setAttribute('aria-label',box.closest('.row')?.querySelector('.lbl')?.childNodes[0]?.textContent.trim()||'Enable setting');
  });
  document.getElementById('den_on').setAttribute('aria-label','Enable monster density');
  document.getElementById('exepath').setAttribute('aria-label','Hero Siege executable path');
  document.getElementById('controlSearch').oninput=filterControlRows;
  document.querySelectorAll('[data-control-filter]').forEach(button=>button.onclick=()=>{controlFilter=button.dataset.controlFilter;filterControlRows()});
  document.querySelectorAll('.tabbtn').forEach((button,index,buttons)=>button.onkeydown=e=>{
    const direction=['ArrowRight','ArrowDown'].includes(e.key)?1:['ArrowLeft','ArrowUp'].includes(e.key)?-1:0;
    if(!direction&&!['Home','End'].includes(e.key))return;
    e.preventDefault();const next=e.key==='Home'?0:e.key==='End'?buttons.length-1:(index+direction+buttons.length)%buttons.length;
    buttons[next].click();buttons[next].focus();
  });
  bindModsSubtabs();
  updateControlDecoration();filterControlRows();decoratePanelIcons();
}
function updateControlDecoration(){
  for(const range of document.querySelectorAll('input[type=range]')){
    const fill=100*(Number(range.value)-Number(range.min))/(Number(range.max)-Number(range.min));
    range.style.background=`linear-gradient(to right,var(--ember) ${fill}%,#4a3a2b ${fill}%)`;
    const buttons=range.parentElement.querySelectorAll('.step-button');
    if(buttons.length===2){buttons[0].disabled=+range.value<=+range.min;buttons[1].disabled=+range.value>=+range.max}
  }
  const hero=document.getElementById('densityHero');
  if(hero){
    const density=document.getElementById('den'),value=document.getElementById('denval'),on=document.getElementById('den_on').checked;
    hero.textContent=on?'×'+Number(density.value).toFixed(1):'off';
    if(!value.querySelector('input'))value.textContent=on?'x'+density.value:'off';
  }
}
function refreshSavedControls(){
  if(!ST?.cfg||document.querySelector('.numedit'))return;
  const c=ST.cfg,map={den:'density',enemyspeed:'enemy_speed',angelic_items:'angelic_items',rarity_rare:'rarity_rare',rarity_ancient:'rarity_ancient'};
  const painted=[];
  document.querySelectorAll('input[type=range]').forEach(range=>{
    const value=range.dataset.sec?c[range.dataset.sec]?.[range.dataset.key]:c[map[range.id]];
    if(value!==undefined){
      // Rendering saved decimal values must not snap them to the drag step.
      if(!range.dataset.step0)range.dataset.step0=range.step||'1';
      range.step='any';range.value=value;
      painted.push([range,range.dataset.typed]);range.dataset.typed='1';
    }
  });
  for(const [range] of painted)if(range.oninput)range.oninput();
  for(const [range,typed] of painted){if(typed===undefined)delete range.dataset.typed;else range.dataset.typed=typed}
  const booleans={den_on:'density_on',autoapply:'auto_apply',enemyspeed_ct:'enemy_speed_ct',map_reveal:'map_reveal',map_reveal_packs:'map_reveal_packs',map_reveal_spawn:'map_reveal_spawn',headhunter:'headhunter',tyrant:'tyrant',beacon:'beacon',mod_filter_max_relics:'mod_filter_max_relics',mod_orb_pickup_radius:'mod_orb_pickup_radius',mod_pet_quest_pickup:'mod_pet_quest_pickup',mod_auto_prospect:'mod_auto_prospect',mod_auto_prospect_bag:'mod_auto_prospect_bag',mod_toggle_indicator:'mod_toggle_indicator',mod_toggle_guard:'mod_toggle_guard',mod_restart_anytime:'mod_restart_anytime',mod_craft_mats:'mod_craft_mats'};
  for(const [id,key] of Object.entries(booleans))document.getElementById(id).checked=!!c[key];
  document.getElementById('mod_skill_timer_style').value=c.mod_skill_timer_style||'off';
  for(const [id,key] of Object.entries({hhval:'headhunter',tyval:'tyrant',beval:'beacon',mfmrval:'mod_filter_max_relics',morval:'mod_orb_pickup_radius',mpqpval:'mod_pet_quest_pickup',autoprospval:'mod_auto_prospect',mtival:'mod_toggle_indicator',mtgval:'mod_toggle_guard',mraval:'mod_restart_anytime',mcmval:'mod_craft_mats',mapval:'map_reveal'})){
    const value=document.getElementById(id);value.textContent=c[key]?'on':'off';value.className='val '+(c[key]?'':'off');
  }
  document.getElementById('enemyspeedctval').textContent=c.enemy_speed_ct?'CT only':'all zones';
  document.getElementById('denval').textContent=c.density_on?'x'+c.density:'off';
  document.getElementById('denval').className='val '+(c.density_on?'':'off');
  syncRevealPacks(!!c.map_reveal,!!c.map_reveal_packs,!!c.map_reveal_spawn);
  syncProspectBag(!!c.mod_auto_prospect,!!c.mod_auto_prospect_bag);
  applyPluginModState(ST.pluginMods);
  updateControlDecoration();decoratePanelIcons();
}
// Mods tab cards read top to bottom, left column first. The split is the
// earliest one where the left column is at least as tall as the right, so the
// left column is the taller one whenever the two differ.
function setupModsColumns(grid){
  const items=[...grid.children],left=document.createElement('div'),right=document.createElement('div');
  left.className=right.className='mods-col';grid.append(left,right);left.append(...items);
  let split=items.length;
  // Both columns keep the same width wherever an item sits, so a move never
  // resizes what is observed and the observer cannot loop.
  const balance=()=>{
    if(!grid.clientWidth)return;
    const heights=items.map(item=>item.hidden||!item.offsetParent?0:item.getBoundingClientRect().height+12);
    const total=heights.reduce((a,b)=>a+b,0);
    let k=0,sum=0;
    while(k<items.length&&sum<total-sum)sum+=heights[k++];
    if(k===split)return;
    split=k;left.append(...items.slice(0,k));right.append(...items.slice(k));
  };
  const observer=new ResizeObserver(balance);
  observer.observe(grid);items.forEach(item=>observer.observe(item));
}
function filterControlRows(){
  if(!document.getElementById('controlSearch'))return;
  const query=document.getElementById('controlSearch').value.trim().toLowerCase();
  document.querySelectorAll('[data-control-filter]').forEach(button=>button.setAttribute('aria-pressed',String(button.dataset.controlFilter===controlFilter)));
  const filtering=['loot','modifiers'].includes(activeTab);
  let count=0;
  document.querySelectorAll('.setting-entry').forEach(entry=>{
    const range=entry.querySelector('input[type=range]');
    const matches=(entry.textContent.toLowerCase().includes(query))&&(controlFilter==='all'||!sliderOff(range.dataset.sec,Number(range.value)));
    entry.hidden=filtering&&entry.closest('.tab-card').dataset.tab===activeTab&&!matches;
    if(entry.closest('.tab-card').dataset.tab===activeTab&&!entry.hidden)count++;
  });
  document.querySelectorAll('.modifier-group').forEach(group=>{group.hidden=filtering&&!group.querySelector('.setting-entry:not([hidden])')});
  // Angelic drops use a standalone card but participate in the same Loot filter.
  const angelic=document.getElementById('angelicCard');
  if(angelic){
    angelic.hidden=activeTab==='loot'&&(!angelic.textContent.toLowerCase().includes(query)||(controlFilter==='modified'&&+document.getElementById('angelic_items').value<=1));
    if(activeTab==='loot'&&!angelic.hidden)count++;
  }
  let empty=document.getElementById('emptySettings');
  if(!empty){empty=document.createElement('div');empty.id='emptySettings';empty.className='empty-settings';empty.textContent='No matching settings. Try another search or show all settings.';document.getElementById('workspace').append(empty)}
  empty.hidden=!filtering||count>0;
}
// These controls edit the allowed pool, never the game's roll or its minimums.
// Keep rows in place while saving so keyboard focus and list scroll do not jump.
const SAT_UI={filter:'all',busy:false};
function satData(polarity){
  return polarity==='buff'
    ? {list:ST.satanicBuffs||[],floor:ST.minEnabledSatanicBuffs||3}
    : {list:ST.satanicDebuffs||[],floor:ST.minEnabledSatanicDebuffs||2};
}
function satPool(polarity){return ST.cfg.satanic_mods?.[polarity]||{}}
function satCount(polarity){return satData(polarity).list.filter(([id])=>satPool(polarity)[id]!==false).length}
function renderSatanicMods(){
  for(const polarity of ['buff','debuff']){
    const list=document.getElementById(`sat${polarity}s`);
    list.innerHTML=satData(polarity).list.map(([id,name,desc])=>satRow(polarity,id,name,desc,satPool(polarity)[id]!==false)).join('')+
      '<p class="sat-empty" hidden>No matching modifiers.<br>Try another search or filter.</p>';
    list.querySelectorAll('.sat-option').forEach(row=>{row.dataset.search=row.textContent.toLocaleLowerCase()});
  }
  syncSatanicMods();
}
function filterSatanicMods(){
  const query=document.getElementById('satSearch').value.trim().toLocaleLowerCase();
  for(const polarity of ['buff','debuff']){
    const list=document.getElementById(`sat${polarity}s`);
    let visible=0;
    list.querySelectorAll('.sat-option').forEach(row=>{
      const enabled=row.querySelector('input').checked;
      const match=row.dataset.search.includes(query)&&(SAT_UI.filter==='all'||(SAT_UI.filter==='enabled')===enabled);
      row.hidden=!match;
      if(match)visible++;
    });
    const empty=list.querySelector('.sat-empty');
    empty.hidden=visible>0;
    if(!satData(polarity).list.length)empty.textContent='Modifier data unavailable. Restart with the complete ForgePact package.';
  }
  document.querySelectorAll('[data-sat-filter]').forEach(b=>b.setAttribute('aria-pressed',String(b.dataset.satFilter===SAT_UI.filter)));
}
function syncSatanicMods(){
  let valid=true,allEnabled=true,available=true;
  document.getElementById('satanicMods').setAttribute('aria-busy',String(SAT_UI.busy));
  for(const polarity of ['buff','debuff']){
    const {list,floor}=satData(polarity),count=satCount(polarity),pool=satPool(polarity);
    valid=valid&&count>=floor;
    available=available&&list.length>0;
    allEnabled=allEnabled&&count===list.length;
    document.getElementById(`sat${polarity}Count`).textContent=`${count} enabled`;
    document.getElementById(`sat${polarity}Min`).textContent=floor;
    document.getElementById(`sat${polarity}All`).disabled=SAT_UI.busy||!list.length||count===list.length;
    document.querySelectorAll(`input[data-sat-polarity="${polarity}"]`).forEach(box=>{
      box.checked=pool[box.dataset.satId]!==false;
      const locked=box.checked&&count<=floor;
      box.setAttribute('aria-disabled',String(SAT_UI.busy||locked));
      const row=box.closest('.sat-option');
      row.classList.toggle('is-enabled',box.checked);
      row.classList.toggle('is-locked',locked);
      row.title=locked?'Enable another modifier before removing this one.':'';
    });
    const help=document.getElementById(`sat${polarity}Help`);
    if(!list.length)help.innerHTML='<strong>Modifier data unavailable</strong>Selections cannot be edited.';
    else if(count<floor)help.innerHTML=`<strong>Enable ${floor-count} more to continue</strong>At least ${floor} modifiers must stay enabled.`;
    else if(count===floor)help.innerHTML='<strong>Minimum reached</strong>Enable another mod before removing one.';
    else help.innerHTML=`<strong>${count} of ${list.length} enabled</strong>You can disable ${count-floor} more. Keep at least ${floor}.`;
  }
  document.getElementById('satRestore').disabled=SAT_UI.busy||!available||allEnabled;
  const summary=document.getElementById('satSummary');
  summary.classList.toggle('is-invalid',!valid);
  summary.innerHTML=`${valid?'&#10003; Selection valid':available?'Selection incomplete':'Modifier data unavailable'} <span class="sat-footer-note">${satCount('buff')} positive &middot; ${satCount('debuff')} negative enabled</span>`;
  filterSatanicMods();
}
async function saveSatanicMods(changes){
  if(SAT_UI.busy)return;
  SAT_UI.busy=true;
  const focused=document.activeElement,saveState=document.getElementById('satSaveState');
  saveState.classList.remove('is-error');
  saveState.textContent='Saving changes...';
  syncSatanicMods();
  try{
    // Bulk actions are sequential, not one request per row. Restore defaults
    // uses the existing API for each pool and preserves every unrelated setting.
    for(const change of changes){
      const res=await j('/api/set',{method:'POST',body:JSON.stringify({section:'satanic_mods',...change})});
      if(res.err||!res.cfg)throw new Error(res.err||'The server did not confirm the save.');
      ST.cfg.satanic_mods=res.cfg.satanic_mods;
    }
    saveState.textContent='Saved · Changes save automatically';
  }catch(e){
    // A lost response may still have saved. Read back the real settings before
    // allowing another edit; never leave a speculative checkmark in the UI.
    let confirmed=false;
    try{const state=await j('/api/state');if(state.cfg){ST.cfg.satanic_mods=state.cfg.satanic_mods;confirmed=true}}catch(_){}
    saveState.classList.add('is-error');
    saveState.textContent=confirmed?'Save interrupted. Showing saved selections.':'Save unconfirmed. Reconnect and reload.';
    toast('Could not finish saving: '+e.message);
  }finally{
    SAT_UI.busy=false;
    syncSatanicMods();
    if(document.activeElement===document.body&&focused?.isConnected&&!focused.disabled)focused.focus({preventScroll:true});
  }
}
function bindSatanicMods(){
  document.getElementById('satSearch').oninput=filterSatanicMods;
  document.querySelectorAll('[data-sat-filter]').forEach(b=>{
    b.onclick=()=>{SAT_UI.filter=b.dataset.satFilter;filterSatanicMods()};
  });
  document.querySelectorAll('input[data-sat-polarity]').forEach(box=>{
    // aria-disabled keeps minimum-locked checkboxes reachable by keyboard so
    // their name, effect and the explanation can still be read together.
    box.onclick=e=>{
      if(box.getAttribute('aria-disabled')==='true'){
        e.preventDefault();
        if(!SAT_UI.busy)toast('Enable another modifier before removing this one.');
      }
    };
    box.onchange=()=>{
      if(SAT_UI.busy){syncSatanicMods();return}
      const polarity=box.dataset.satPolarity;
      if(!box.checked&&satCount(polarity)<=satData(polarity).floor){syncSatanicMods();return}
      saveSatanicMods([{polarity,key:box.dataset.satId,value:box.checked}]);
    };
  });
  const enableAll=polarity=>({polarity,keys:satData(polarity).list.map(m=>String(m[0])),value:true});
  for(const polarity of ['buff','debuff'])document.getElementById(`sat${polarity}All`).onclick=()=>saveSatanicMods([enableAll(polarity)]);
  document.getElementById('satRestore').onclick=()=>saveSatanicMods(['buff','debuff'].map(enableAll));
}
// Self-scheduling poll: fast while something is happening, idle when nothing
// is, suspended entirely while the window is hidden.
let pollTimer=null, pollLastChange=Date.now(), pollPrev=null;
function schedulePoll(){
  if(pollTimer){clearTimeout(pollTimer);pollTimer=null}
  const delay=pollDelayMs(document.hidden,Date.now()-pollLastChange);
  if(delay===null)return;
  pollTimer=setTimeout(pollOnce,delay);
}
function noteLocalAction(){pollLastChange=Date.now();schedulePoll()}
// One re-read after a switch is sent, so the toast reports the plugin's own
// answer. The plugin writes its state on its next frame batch, so give it a
// moment; a panel with no game running just gets the empty state back.
async function pluginModsAfterSet(){
  await new Promise(r=>setTimeout(r,700));
  try{const s=await j("/api/state");if(ST)ST.pluginMods=s.pluginMods;applyPluginModState(s.pluginMods);return s.pluginMods}catch(_){return null}
}
async function pollOnce(){
  pollTimer=null;
  try{
    const s=await j('/api/state');
    pollLastChange=pollNextChangeAt(pollPrev,s,false,Date.now(),pollLastChange);
    pollPrev=s;
    if(ST){ST.gameRunning=s.gameRunning;ST.lastApplied=s.lastApplied;ST.queued=s.queued;ST.ipcOk=s.ipcOk;ST.chain=s.chain;ST.eacStatus=s.eacStatus;ST.launch=s.launch;ST.pluginMods=s.pluginMods;status();applyPluginModState(s.pluginMods)}
  }catch(e){}
  schedulePoll();
}
// One listener instead of a call in every handler: any control the user
// touches is a local action, and so is pressing Apply.
['input','change','click'].forEach(ev=>document.addEventListener(ev,noteLocalAction,true));
document.addEventListener('visibilitychange',()=>{
  // Coming back: poll at once, so the first thing a returning user sees is
  // fresh, and reset the clock so the fast tier covers the time they look.
  if(document.hidden){schedulePoll()}else{pollLastChange=Date.now();pollOnce()}
});
// finally, not then: boot() does ~40 unguarded DOM lookups after its first
// await, and if any of them throws, a .then() never runs - so the panel would
// sit there forever with no poll scheduled and no message, every chip frozen on
// its initial value. The fixed setInterval this replaced was registered
// unconditionally and could not fail that way, so .then() alone was a
// regression. Say so in the UI as well: a panel that stops updating silently is
// the thing a user cannot report.
boot().catch(e=>{try{toast('panel failed to load: '+e)}catch(_){}})
      .finally(()=>{pollPrev=ST;schedulePoll()});
</script></body></html>"""


def main():
    global PORT
    srv = None
    for p in PORT_CANDIDATES:
        # ``ThreadingHTTPServer`` enables address reuse.  On Windows that can
        # allow two unrelated local tools to listen on the same port, causing
        # requests to land in the wrong application.  Skip any port that is
        # already accepting connections before attempting the bind.
        try:
            with socket.create_connection(("127.0.0.1", p), timeout=0.15):
                continue
        except OSError:
            pass
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





