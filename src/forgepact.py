#!/usr/bin/env python3
"""ForgePact - Hero Siege Game Mods control panel.

Single-file local web app: http://127.0.0.1:8766
Talks to BloodPactPlugin (Aurie/YYTK) over bp_ipc:
- settings apply instantly while the game is running
- while the game is closed, commands are queued in cmd.txt (the plugin
  processes them on startup)
- settings are re-applied automatically on every launch (background watcher)
Settings persist in %LOCALAPPDATA%/Hero_Siege/forgepact.json.
"""

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
]

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
    # Sub-toggle of map_reveal.  Only meaningful while map_reveal is on, and
    # separate from it because it is the half that costs frame time: revealing
    # the fog is free, populating the map is not.
    "map_reveal_packs": True,
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


# YYTK RunnerInterface cache: pre-writing it skips the ~1 min first-launch disassembly.
# Keyed by exe size; YYTK validates the size, so a stale cache (different exe build) is
# safely ignored (it just falls back to a one-time scan + re-caches). Known patched build:
KNOWN_RI_CACHE = {
    282105856: "282105856 190392081 190393250\n",   # 7.0.90, AuriePatcher-ed (verified 2026-09-02)
    303708672: "303708672 208216097 208217266\n",   # 7.0.30, AuriePatcher-ed (verified 2026-08-26)
    303584768: "303584768 208133041 208134210\n",   # previous S10 build, AuriePatcher-ed
    309551616: "309551616 207703889 207705042\n",   # older still
}


def ensure_ri_cache(cfg=None) -> bool:
    """Use only a verified cache; remove unknown caches so YYTK must rescan."""
    try:
        exe = exe_path(cfg)
        content = KNOWN_RI_CACHE.get(exe.stat().st_size)
        cache = exe.with_name(exe.name + ".yytkcache")
        if not content:
            if cache.exists():
                cache.unlink()
            return False
        if not cache.exists() or cache.read_text(errors="ignore").split()[:1] != content.split()[:1]:
            cache.write_text(content, encoding="ascii")
        return True
    except Exception:
        return False


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


def running_paths(name: str) -> list:
    """FULL PATHS of the running processes named `name`.

    Matching on the name alone is not enough: a player's Steam copy and their
    offline copy can be open at the same time.  The old name-only version
    treated a Hero_Siege.exe in a different folder as "the game is running" and
    atliyordu.
    """
    yollar = []
    try:
        r = subprocess.run(["tasklist", "/FI", f"IMAGENAME eq {name}", "/FO", "CSV", "/NH"],
                           capture_output=True, text=True, encoding="utf-8", errors="replace",
                           timeout=10, creationflags=CREATE_NO_WINDOW)
        pidler = []
        for satir in r.stdout.splitlines():
            parcalar = [p.strip('"') for p in satir.split('","')]
            if len(parcalar) >= 2 and parcalar[0].strip('"').lower() == name.lower():
                try:
                    pidler.append(int(parcalar[1]))
                except ValueError:
                    pass
        if not pidler:
            return yollar

        import ctypes
        from ctypes import wintypes
        k32 = ctypes.windll.kernel32
        PROCESS_QUERY_LIMITED_INFORMATION = 0x1000
        k32.OpenProcess.restype = wintypes.HANDLE
        k32.QueryFullProcessImageNameW.argtypes = [
            wintypes.HANDLE, wintypes.DWORD, wintypes.LPWSTR, ctypes.POINTER(wintypes.DWORD)]
        for pid in pidler:
            h = k32.OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, False, pid)
            if not h:
                continue
            try:
                buf = ctypes.create_unicode_buffer(32768)
                boyut = wintypes.DWORD(32768)
                if k32.QueryFullProcessImageNameW(h, 0, buf, ctypes.byref(boyut)):
                    yollar.append(buf.value)
            finally:
                k32.CloseHandle(h)
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
        # Only emitted to turn the pack pass OFF: the plugin defaults it on, so
        # the common case sends nothing extra (same rule as the rest of this
        # function - emit only what is actually needed).
        if not cfg.get("map_reveal_packs", True):
            out.append("reveal packs 0")
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
        value = int(cfg['drops'].get(key, 1))
        if value > 1:
            out.append(f"dropmult {key} {value}")
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
        return {"err": "mod source files missing: " + ", ".join(missing) +
                       " (put them in a 'modfiles' folder next to ForgePact)"}
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


def plugin_boot_count(cfg=None) -> int:
    """How many times the plugin has started, read from its own log.

    The plugin appends one 'BloodPact plugin loaded' line per game start, so a
    change in this count identifies a NEW game process even when the game was
    closed and reopened between two 5-second polls (a boolean running flag
    misses that and the startup commands are never sent).
    """
    try:
        text = (ipc_dir(cfg) / "out.txt").read_text(encoding="utf-8", errors="ignore")
        return text.count("BloodPact plugin loaded")
    except Exception:
        return -1


def watcher():
    """Re-apply the settings automatically every time the game LAUNCHES."""
    # False is intentional: if the panel itself starts after the game, the
    # first pass must still attach and apply the saved configuration.
    was_running = False
    last_boot = None
    while True:
        time.sleep(5)
        try:
            cfg = load_cfg()
            now = game_running(cfg)
            boot = plugin_boot_count(cfg)
            new_process = now and (not was_running or (last_boot is not None and boot != last_boot))
            if new_process and cfg.get("auto_apply"):
                if wait_for_plugin_ready(cfg):
                    apply_all(cfg)
            if now:
                last_boot = boot
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
                    cfg[sec][key] = max(1, min(100, int(val)))
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
                elif key in ("density_on", "auto_apply", "map_reveal", "map_reveal_packs", "headhunter", "tyrant", "beacon", "mod_filter_max_relics", "mod_orb_pickup_radius", "mod_pet_quest_pickup"):
                    cfg[key] = bool(val)
                save_cfg(cfg)
                live = ""
                if game_running(cfg):
                    if sec == "keys":
                        # A live change must also restore families moved back to
                        # x1; startup's sparse command list deliberately cannot.
                        send_cmds(build_key_cmds(cfg.get("keys", {}), include_resets=True), cfg)
                    elif sec == "drops":
                        send_cmds([f"dropmult {key} {int(val)}"], cfg)
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
                        send_cmds(cmds, cfg)
                    elif key == "map_reveal_packs":
                        send_cmds([f"reveal packs {1 if cfg['map_reveal_packs'] else 0}"], cfg)
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
                    elif key in ("rarity_rare", "rarity_ancient"):
                        # Always explicit: "rarity off" returns a live hook to vanilla.
                        send_cmds([rarity_cmd(cfg)], cfg)
                    elif key == "angelic_items":
                        send_cmds([angelic_cmd(cfg)], cfg)
                    elif key in ("enemy_speed", "enemy_speed_ct"):
                        # Always explicit: "enemyspeed 1 ct" turns a live hook back to vanilla.
                        send_cmds([enemy_speed_cmd(cfg)], cfg)
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
                # Silently continuing while a copy in another folder is open is
                # confusing: the settings go to THIS copy while the player plays
                # the other one.
                other = other_copy_running(cfg)
                if other:
                    self._json({"err": "A different copy of the game is already running:\n"
                                       f"{other}\n\n"
                                       "Close it first - settings are sent to the copy configured "
                                       "above, not to that one."}); return
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
:root{--bg:#0d0a08;--card:#171210;--card2:#1e1713;--ember:#ff7a1a;--ember2:#ffb347;--tx:#e8dcc8;--mut:#8a7a64;--line:#33261c;--ok:#5ad87a;--arcane:#a77cff;--blood:#ff5b6e;--steel:#65c7d5}
*{box-sizing:border-box}
*{scrollbar-width:thin;scrollbar-color:var(--line) var(--card)}
*::-webkit-scrollbar{width:10px;height:10px}
*::-webkit-scrollbar-track{background:var(--card)}
*::-webkit-scrollbar-thumb{background:var(--line);border-radius:6px;border:2px solid var(--card)}
*::-webkit-scrollbar-thumb:hover{background:var(--ember)}
body{margin:0;font:14px/1.5 'Segoe UI',sans-serif;background:radial-gradient(1200px 500px at 50% -150px,#2a1408 0%,var(--bg) 60%);color:var(--tx);min-height:100vh}
#wrap{max-width:980px;margin:0 auto;padding:26px 20px 60px}
header{display:flex;align-items:center;gap:16px;margin-bottom:6px;position:relative;padding:4px 0 10px}
header:after{content:"";position:absolute;left:0;right:0;bottom:0;height:1px;background:linear-gradient(90deg,var(--ember),#8b3b1600 70%);box-shadow:0 0 16px #ff7a1a55}
.logo{font-size:34px;filter:drop-shadow(0 0 12px #ff7a1a88)}
h1{font-size:26px;margin:0;letter-spacing:2px;background:linear-gradient(90deg,var(--ember2),var(--ember),#c44a0a);-webkit-background-clip:text;background-clip:text;color:transparent}
.sub{color:var(--mut);font-size:12px;letter-spacing:3px;text-transform:uppercase}
.control-dock{position:sticky;top:0;z-index:10;margin:8px 0 18px;padding:9px 0 12px;background:linear-gradient(180deg,#0d0a08fa 82%,#0d0a0800);backdrop-filter:blur(9px)}
#statusbar{display:flex;gap:10px;align-items:center;margin:9px 0 0;flex-wrap:wrap}
.tabbar{display:grid;grid-template-columns:repeat(5,minmax(100px,1fr));gap:8px;padding:5px;border:1px solid #33261c;border-radius:12px;background:#100c0ae8;box-shadow:0 5px 22px #0008}
.tabbtn{appearance:none;border:1px solid transparent;background:transparent;color:#8f816e;border-radius:8px;padding:9px 12px;cursor:pointer;font-size:12px;font-weight:700;letter-spacing:.9px;text-transform:uppercase;transition:.18s}
.tabbtn:hover{color:var(--ember2);background:#241711;border-color:#49301f}
.tabbtn.active{color:#fff2dc;background:linear-gradient(180deg,#563018,#33200e);border-color:#8e5526;box-shadow:inset 0 0 16px #ff8a2130,0 0 15px #ff7a1a20}
.tabbtn[data-tab="modifiers"].active{background:linear-gradient(180deg,#49305c,#291c35);border-color:#8059a4;box-shadow:inset 0 0 16px #a77cff30,0 0 15px #a77cff22}
.tabbtn[data-tab="mods"].active{background:linear-gradient(180deg,#204a43,#132c28);border-color:#388e7d;color:#d1fffa;box-shadow:inset 0 0 16px #388e7d30,0 0 15px #388e7d22}
.chip{padding:6px 14px;border-radius:20px;font-size:12px;border:1px solid var(--line);background:var(--card)}
.chip.on{border-color:var(--ok);color:var(--ok);box-shadow:0 0 12px #5ad87a22}
.chip.off{border-color:#777;color:#999}
.chip.warn{border-color:var(--ember);color:var(--ember2)}
.chip.err{border-color:#e05050;color:#ff8080}
.card{background:linear-gradient(180deg,var(--card2),var(--card));border:1px solid var(--line);border-radius:12px;padding:18px 22px;margin-bottom:18px;box-shadow:0 4px 24px #00000055}
.tab-card{display:none}
.tab-card.active{display:block;animation:tabIn .18s ease-out}
@keyframes tabIn{from{opacity:.25;transform:translateY(5px)}to{opacity:1;transform:none}}
.card h2{margin:0 0 4px;font-size:16px;color:var(--ember2);letter-spacing:1px}
.card .hint{color:var(--mut);font-size:12px;margin-bottom:14px}
.note{color:var(--mut);font-size:11px;margin:-6px 0 10px 2px;opacity:.85}
.row{display:flex;align-items:center;gap:14px;padding:9px 0;border-top:1px solid #221a13}
.row:first-of-type{border-top:none}
.row .lbl{width:200px;font-size:13px}
.row .lbl .tag{font-size:10px;color:var(--mut);margin-left:6px}
input[type=range]{flex:1;-webkit-appearance:none;height:6px;border-radius:3px;background:linear-gradient(90deg,#3a2516,#241811);outline:none}
.numedit{width:62px;background:#1a120c;color:#ffd9a0;border:1px solid #6b4a2a;border-radius:6px;font:inherit;text-align:right;padding:2px 4px;outline:none}
.numedit:focus{border-color:#ff9a3c}
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
.modifier-card{position:relative;overflow:hidden;border-color:#49375d;background:radial-gradient(800px 260px at 85% -70px,#44255b55 0%,transparent 62%),linear-gradient(180deg,#1c151f,#151116)}
.modifier-card:before{content:"";position:absolute;inset:0;pointer-events:none;background:linear-gradient(120deg,transparent 0 47%,#a77cff08 50%,transparent 53%)}
.section-title{display:flex;align-items:flex-start;justify-content:space-between;gap:16px;position:relative}
.live-badge{font-size:10px;letter-spacing:1.4px;color:#bdffd0;border:1px solid #34794a;background:#122619;padding:4px 9px;border-radius:999px;box-shadow:0 0 12px #5ad87a22}
.modifier-grid{display:grid;grid-template-columns:1fr 1fr;gap:12px;position:relative}
.modifier-group{background:#0d0b10aa;border:1px solid #342943;border-radius:10px;padding:11px 14px}
.modifier-group.wide{grid-column:1/-1}
.group-title{font-size:11px;text-transform:uppercase;letter-spacing:1.4px;margin-bottom:4px;font-weight:700}
.group-title.offense{color:var(--blood)}
.group-title.critical{color:var(--arcane)}
.group-title.sustain{color:var(--steel)}
.modifier-group .row{display:grid;grid-template-columns:minmax(145px,190px) 1fr 64px;gap:10px;padding:8px 0}
.modifier-group .row .lbl{width:auto}
.modifier-group .note{margin:-4px 0 8px}
@media(max-width:820px){.tabbar{grid-template-columns:1fr 1fr}.modifier-grid{grid-template-columns:1fr}.modifier-group.wide{grid-column:auto}.modifier-group .row{grid-template-columns:150px 1fr 64px}}
.sat-list{max-height:420px;overflow-y:auto;padding-right:4px}
.sat-list .row{display:flex;align-items:center;gap:10px;border-bottom:1px solid #241d2c}
.sat-list .row:last-child{border-bottom:none}
.sat-desc{font-size:11px;color:#8f816e;font-weight:normal}
.group-title.positive{color:#72d6a5}
.group-title.negative{color:#e0697a}
.sat-bulk{display:flex;gap:8px;margin:2px 0 8px}
.sat-bulk button{font-size:11px;padding:4px 10px}
</style></head><body><div id="wrap">
<header><div class="logo">&#128293;</div><div>
  <h1>FORGEPACT</h1><div class="sub">Hero Siege game mods &middot; live control</div>
</div></header>
<div class="control-dock">
<nav class="tabbar" role="tablist" aria-label="ForgePact categories">
  <button class="tabbtn" data-tab="setup" role="tab">&#9881; Setup</button>
  <button class="tabbtn" data-tab="modifiers" role="tab">&#9876; Modifiers</button>
  <button class="tabbtn" data-tab="world" role="tab">&#127757; World</button>
  <button class="tabbtn" data-tab="loot" role="tab">&#128176; Loot</button>
  <button class="tabbtn" data-tab="mods" role="tab">&#10024; Mods</button>
</nav>
<div id="statusbar">
  <span class="chip" id="chipGame">...</span>
  <span class="chip" id="chipApply">...</span>
  <label class="chip" style="display:flex;align-items:center;gap:8px;cursor:pointer">
    auto-apply on game launch
    <span class="switch"><input type="checkbox" id="autoapply"><span class="sl"></span></span>
  </label>
  <button class="btn" id="applyall">Apply All Now</button>
</div>
</div>

<div class="card tab-card" data-tab="setup">
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

<div class="card tab-card" data-tab="world">
  <h2>&#128127; Monster Density</h2>
  <div class="hint">Multiplies enemy spawners - applies to newly loaded zones.<br><b>Density and Special Content stack.</b> Each on its own is fine, but a high density together with high special-content rates can overload a heavy zone and crash the game on entry. Verified stable: density x3 with every special content at x20. If a zone crashes, lower density first.</div>
  <div class="note" style="color:#72d6a5;border:1px solid #245a43;border-radius:6px;padding:8px 12px;margin-bottom:10px">Density is applied once per creator placement. Returning to a previously visited zone does not multiply it again.</div>
  <div class="row">
    <span class="lbl">Density multiplier</span>
    <label class="switch"><input type="checkbox" id="den_on"><span class="sl"></span></label>
    <input type="range" id="den" min="1" max="5" step="0.5">
    <span class="val" id="denval">x3</span>
  </div>
</div>

<div class="card tab-card" data-tab="world">
  <h2>&#127939; Enemy Movement Speed</h2>
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

<div class="card tab-card" data-tab="world">
  <h2>&#127757; Special Content Spawns</h2>
  <div class="hint">Multiplies the game's own spawn markers, so the game places and runs each mechanic itself - nothing is hand-placed. Higher = more of that content per zone. Applies to newly loaded zones. (The Abyss is not listed: it sits behind a discovery gate that is not solved yet.)</div>
  <div id="spawners"></div>
</div>

<div class="card tab-card" data-tab="loot">
  <h2>&#128176; Drop Rates</h2>
  <div class="hint">All of these use the game's own dice - <b>nothing is forced</b>.
  <b>x5 means five times more likely than vanilla</b>; <b>off</b> (x1) leaves that drop completely untouched.
  Applies immediately, no zone reload needed.<br>
  Every multiplier scales the item's own vanilla roll, so x2 is twice the vanilla rate.
  Families the game never rolls outside their home zone (Dungeon and Angelic keys, Orbs,
  Scrolls, Shards, Fragments) first get their roll opened at the monster's normal-key
  chance; Relics use their own curve, explained on the row.</div>
  <div id="drops"></div>
  <div id="keys"></div>
</div>

<div class="card tab-card" data-tab="loot">
  <h2>&#128081; Angelic / Unholy Drops (Experimental)</h2>
  <div class="hint">The game only rolls for Angelic or Unholy items while an "Angelic item drop chance"
  effect (a Blood Pact or dungeon modifier) is active, so this is ForgePact's own die: on every monster
  kill it rolls, and on a hit the game itself builds one of its 49 real Angelic / Unholy uniques (no
  developer or event pieces) and drops it where the monster died.<br>
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
    <div><h2>&#9876; Combat &amp; Character Modifiers</h2>
      <div class="hint">Live modifiers use the character's current total value, including equipment and other bonuses. <b>Off</b> keeps the game at its normal value.</div>
    </div>
    <span class="live-badge">LIVE MEMORY</span>
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

<div class="card tab-card" data-tab="world">
  <h2>&#128128; Monster Rarity</h2>
  <div class="hint">Raises a share of the normal monsters to <b>Rare</b> (yellow) or <b>Ancient</b> (skull) as they spawn, through the game's own rarity setup: the monster gets that tier's stats, affixes and health bar exactly as if it had rolled that way. The two shares are separate and together stay at 100% or less - 25% Rare with 15% Ancient leaves 60% normal. Champions and the game's own rares are not touched. Stacks with Tyrant's Crown and Density.</div>
  <div class="row" style="border:none">
    <span class="lbl">Normal monsters raised to Rare</span>
    <input type="range" min="0" max="100" step="5" id="rarity_rare" value="0">
    <span class="val off" id="rarityrareval" style="width:64px">off</span>
  </div>
  <div class="row" style="border:none">
    <span class="lbl">Normal monsters raised to Ancient</span>
    <input type="range" min="0" max="100" step="5" id="rarity_ancient" value="0">
    <span class="val off" id="rarityancval" style="width:64px">off</span>
  </div>
  <div class="note" id="raritynote">off</div>
</div>

<div class="card modifier-card tab-card" data-tab="world">
  <h2><svg width="26" height="26" viewBox="0 0 64 64" style="vertical-align:-6px" xmlns="http://www.w3.org/2000/svg" aria-hidden="true">
    <defs>
      <radialGradient id="satRedGlow" cx="50%" cy="50%" r="50%">
        <stop offset="0%" stop-color="#ff6b7a" stop-opacity="1"/>
        <stop offset="55%" stop-color="#c81a34" stop-opacity="0.6"/>
        <stop offset="100%" stop-color="#c81a34" stop-opacity="0"/>
      </radialGradient>
      <radialGradient id="satSkullFill" cx="42%" cy="32%" r="75%">
        <stop offset="0%" stop-color="#2b2622"/>
        <stop offset="55%" stop-color="#14100d"/>
        <stop offset="100%" stop-color="#060504"/>
      </radialGradient>
      <linearGradient id="satFadeGrad" x1="0" y1="0" x2="0" y2="1">
        <stop offset="0" stop-color="black"/>
        <stop offset="0.35" stop-color="white"/>
        <stop offset="1" stop-color="white"/>
      </linearGradient>
      <mask id="satFadeTop"><rect x="0" y="0" width="64" height="64" fill="url(#satFadeGrad)"/></mask>
      <filter id="satSmokeBlur" x="-60%" y="-60%" width="220%" height="220%"><feGaussianBlur stdDeviation="2.2"/></filter>
      <filter id="satSoftBlur" x="-60%" y="-60%" width="220%" height="220%"><feGaussianBlur stdDeviation="1.1"/></filter>
    </defs>
    <g filter="url(#satSmokeBlur)" stroke-linecap="round" fill="none">
      <path d="M20,52 C14,46 18,38 12,30 C7,23 13,15 9,6" stroke="#15110e" stroke-width="5" opacity="0.22"/>
      <path d="M20,52 C14,46 18,38 12,30 C7,23 13,15 9,6" stroke="#100c0a" stroke-width="2" opacity="0.45"/>
      <path d="M44,52 C50,45 45,36 51,28 C56,20 49,13 54,4" stroke="#15110e" stroke-width="5" opacity="0.22"/>
      <path d="M44,52 C50,45 45,36 51,28 C56,20 49,13 54,4" stroke="#100c0a" stroke-width="2" opacity="0.45"/>
      <path d="M30,50 C26,44 31,40 27,34 C24,29 29,24 26,16" stroke="#15110e" stroke-width="4" opacity="0.18"/>
    </g>
    <ellipse cx="32" cy="34" rx="24" ry="22" fill="#0a0705" opacity="0.25" filter="url(#satSmokeBlur)"/>
    <ellipse cx="32" cy="30" rx="19" ry="17" fill="url(#satRedGlow)" opacity="0.5" filter="url(#satSoftBlur)"/>
    <g mask="url(#satFadeTop)">
      <path d="M32,6 C20,6 12,15 12,26 C12,33 15,37 18,40 C17,43 17,46 18,48 C20,52 24,54 32,54 C40,54 44,52 46,48 C47,46 47,43 46,40 C49,37 52,33 52,26 C52,15 44,6 32,6 Z" fill="url(#satSkullFill)"/>
      <ellipse cx="23" cy="27" rx="5.2" ry="6.4" fill="#050403"/>
      <ellipse cx="41" cy="27" rx="5.2" ry="6.4" fill="#050403"/>
      <circle cx="23" cy="28.5" r="2.6" fill="url(#satRedGlow)"/>
      <circle cx="41" cy="28.5" r="2.6" fill="url(#satRedGlow)"/>
      <path d="M32,33 L28.5,40 L35.5,40 Z" fill="#050403"/>
      <path d="M27,46 L27,52 M32,46 L32,52 M37,46 L37,52" stroke="#050403" stroke-width="1.6" stroke-linecap="round"/>
      <path d="M46,14 C50,19 52,23 52,26 C52,33 49,37 46,40" fill="none" stroke="#ff5b6e" stroke-width="1.6" opacity="0.85" filter="url(#satSoftBlur)"/>
    </g>
  </svg> Satanic Zone Mods</h2>
  <div class="hint">The World Section modifiers Hero Siege can roll onto a Satanic Zone. Every mod is on by default; deselect the ones you never want to see and the plugin keeps the game's own roll away from them. At least <span id="satminbuffnote">3</span> positive and <span id="satmindebuffnote">2</span> negative mods must stay enabled - a Satanic Zone still needs a pool to roll from.</div>
  <div class="modifier-grid">
    <div class="modifier-group">
      <div class="group-title positive">Positive</div>
      <div class="sat-bulk">
        <button class="btn" id="satbuffAll" type="button">Select All</button>
        <button class="btn" id="satbuffNone" type="button">Deselect All</button>
      </div>
      <div class="sat-list" id="satbuffs"></div>
    </div>
    <div class="modifier-group">
      <div class="group-title negative">Negative</div>
      <div class="sat-bulk">
        <button class="btn" id="satdebuffAll" type="button">Select All</button>
        <button class="btn" id="satdebuffNone" type="button">Deselect All</button>
      </div>
      <div class="sat-list" id="satdebuffs"></div>
    </div>
  </div>
</div>

<div class="card tab-card" data-tab="mods">
  <h2>&#10024; Gameplay Mods</h2>
  <div class="hint">Toggle custom game modifications, drop pool adjustments, and quality-of-life tweaks. Settings apply immediately while the game is running.</div>
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
        <span class="lbl" style="width:auto;flex:1">&#8627; Also fill the map with monsters<br><span style="font-size:11px;color:#8f816e;font-weight:normal">Most mob packs do not exist until you walk near them, so a revealed map still shows no monsters. This makes each new zone create its packs on arrival, so they appear on the minimap right away. It is the only part of this mod that adds work for the game - turn it off if a zone feels heavy.</span></span>
        <label class="switch"><input type="checkbox" id="map_reveal_packs"><span class="sl"></span></label>
        <span class="val" id="mrpval">on</span>
    </div>
    <div class="row" style="border:none">
        <span class="lbl" style="width:auto;flex:1">Pet collects quest items<br><span style="font-size:11px;color:#8f816e;font-weight:normal">While your pet is out, it walks to quest items on screen and picks them up for you - one at a time, crediting the quest objective exactly as collecting it by hand does. Only applies to pick-up quest items; things you activate, break or talk to are left alone.</span></span>
        <label class="switch"><input type="checkbox" id="mod_pet_quest_pickup"><span class="sl"></span></label>
        <span class="val" id="mpqpval">off</span>
    </div>
</div>

<div class="card tab-card" data-tab="mods">
  <h2>&#129686; Items</h2>
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
<div id="toast"></div>
<script>
let ST=null, tmr=null;
async function j(u,opt){const r=await fetch(u,opt);return r.json()}
function toast(m){const t=document.getElementById('toast');t.textContent=m;t.classList.add('show');clearTimeout(tmr);tmr=setTimeout(()=>t.classList.remove('show'),2200)}
function openTab(name,remember=true){
  if(!document.querySelector(`.tabbtn[data-tab="${name}"]`))name='modifiers';
  document.querySelectorAll('.tabbtn').forEach(b=>{const on=b.dataset.tab===name;b.classList.toggle('active',on);b.setAttribute('aria-selected',on?'true':'false')});
  document.querySelectorAll('.tab-card').forEach(c=>c.classList.toggle('active',c.dataset.tab===name));
  if(remember){try{sessionStorage.setItem('forgepact_tab',name)}catch(e){}}
  window.scrollTo({top:0,behavior:'smooth'});
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
// row greys out and reads "n/a" rather than silently claiming to be on.
function syncRevealPacks(parentOn,packsOn){
  const row=document.getElementById('map_reveal_packs_row');
  const box=document.getElementById('map_reveal_packs');
  const val=document.getElementById('mrpval');
  if(!row||!box||!val)return;
  box.disabled=!parentOn;
  row.style.opacity=parentOn?'1':'0.45';
  val.textContent=parentOn?(packsOn?'on':'off'):'n/a';
  val.className='val '+(parentOn&&packsOn?'':'off');
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
  return `<div class="row">
    <span class="lbl" style="width:auto;flex:1">${name}<br><span class="sat-desc">${desc}</span></span>
    <label class="switch"><input type="checkbox" data-sat-polarity="${polarity}" data-sat-id="${id}" ${enabled?'checked':''}><span class="sl"></span></label>
  </div>`;
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
    inp.type='number'; inp.className='numedit'; inp.min=r.min; inp.max=r.max; inp.step='any'; inp.value=r.value;
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
  syncRevealPacks(mr,mrp);
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
  rarityLoad(c);
  document.getElementById('hhval').className='val '+(hh?'':'off');
  document.getElementById('exepath').value=c.game_exe||'';
  document.getElementById('spawners').innerHTML=ST.spawners.map(([k,i,l,mx])=>row('spawners',k,l,c.spawners[k]||1,'',mx)).join('');
  document.getElementById('satminbuffnote').textContent=ST.minEnabledSatanicBuffs||3;
  document.getElementById('satmindebuffnote').textContent=ST.minEnabledSatanicDebuffs||2;
  const satPool=(polarity)=>(c.satanic_mods&&c.satanic_mods[polarity])||{};
  document.getElementById('satbuffs').innerHTML=(ST.satanicBuffs||[]).map(([id,name,desc])=>
    satRow('buff',id,name,desc,satPool('buff')[id]!==false)).join('');
  document.getElementById('satdebuffs').innerHTML=(ST.satanicDebuffs||[]).map(([id,name,desc])=>
    satRow('debuff',id,name,desc,satPool('debuff')[id]!==false)).join('');
  document.getElementById('keys').innerHTML=ST.keys.map(([k,l,t])=>{
    const v=(c.keys&&c.keys[k])||1;
    return row('keys',k,l,v,'',100,keyNote(k,t,v));
  }).join('');
  document.getElementById('drops').innerHTML=ST.drops.map(([k,l,h])=>
    row('drops',k,l,(c.drops&&c.drops[k])||1,h?` <span class="tag">${h}</span>`:'')).join('');
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
  den.oninput=()=>{document.getElementById('denval').textContent='x'+sliderVal(den)};
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
    syncRevealPacks(e.target.checked,document.getElementById('map_reveal_packs').checked);
    const res=await j('/api/set',{method:'POST',body:JSON.stringify({key:'map_reveal',value:e.target.checked})});
    toast('map reveal '+(e.target.checked?'ON':'OFF')+' - '+(res.ok||res.err));
  };
  document.getElementById('map_reveal_packs').onchange=async(e)=>{
    syncRevealPacks(document.getElementById('map_reveal').checked,e.target.checked);
    const res=await j('/api/set',{method:'POST',body:JSON.stringify({key:'map_reveal_packs',value:e.target.checked})});
    toast('map monsters '+(e.target.checked?'ON':'OFF')+' - '+(res.ok||res.err));
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
  bindSatanicMods();
}
// One row's checkbox posts {section:'satanic_mods', polarity, key:id, value}. The
// server re-validates the per-polarity floor (see /api/set), so a rejected
// deselect below the floor snaps the checkbox back rather than trusting the client.
function bindSatanicMods(){
  const setOne=async(box)=>{
    const polarity=box.dataset.satPolarity, id=box.dataset.satId, value=box.checked;
    const res=await j('/api/set',{method:'POST',body:JSON.stringify({section:'satanic_mods',polarity,key:id,value})});
    if(res.err){ box.checked=!value; toast(res.err); return false; }
    if(res.cfg) ST.cfg=res.cfg;
    return true;
  };
  document.querySelectorAll('input[data-sat-polarity]').forEach(box=>{
    box.onchange=()=>setOne(box);
  });
  // One request for the whole column (not one per row - that was the slow,
  // one-by-one-with-animation path). The server clamps a "deselect all" to
  // the polarity's floor itself (see /api/set), so the client just asks for
  // everything and repaints from whatever cfg comes back.
  const bulkSet=async(polarity,ids,value)=>{
    const res=await j('/api/set',{method:'POST',body:JSON.stringify({section:'satanic_mods',polarity,keys:ids,value})});
    if(res.err){ toast(res.err); return; }
    if(res.cfg){
      ST.cfg=res.cfg;
      const pool=res.cfg.satanic_mods[polarity]||{};
      document.querySelectorAll(`input[data-sat-polarity="${polarity}"]`).forEach(box=>{
        box.checked=pool[box.dataset.satId]!==false;
      });
    }
    toast(`${polarity==='buff'?'positive':'negative'} mods: ${value?'all selected':'deselected to the minimum'}`);
  };
  const wireBulk=(polarity,allId,noneId,list)=>{
    const ids=list.map(m=>String(m[0]));
    document.getElementById(allId).onclick=()=>bulkSet(polarity,ids,true);
    document.getElementById(noneId).onclick=()=>bulkSet(polarity,ids,false);
  };
  wireBulk('buff','satbuffAll','satbuffNone',ST.satanicBuffs||[]);
  wireBulk('debuff','satdebuffAll','satdebuffNone',ST.satanicDebuffs||[]);
}
setInterval(async()=>{const s=await j('/api/state');ST.gameRunning=s.gameRunning;ST.lastApplied=s.lastApplied;ST.queued=s.queued;ST.ipcOk=s.ipcOk;status()},5000);
boot();
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





