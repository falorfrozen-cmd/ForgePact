#!/usr/bin/env python3
"""ForgePact - Hero Siege Game Mods control panel.

Local web app: http://127.0.0.1:8780 (frontend: ../panel, a Svelte + Vite
build served from PANEL_DIST)
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
__version__ = "2.1.0"

import copy
import hashlib
import json
import os
import re
import socket
import struct
import subprocess
import sys
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import unquote, urlparse
import offline_launcher

# The Satanic Zone pools are all the panel takes from the SDK, and the import
# names nothing else: a name from one of the SDK's generated tables
# (GameObject, GameScript, ...) builds that table's whole IntEnum on every
# panel start, sandbox and exe launch, used or not (hub hs-game-sdk guide,
# "Import cost"). tests/test_panel_sdk_import.py pins it.
try:
    from hs_game_sdk import SATANIC_BUFFS, SATANIC_DEBUFFS
except ImportError:
    _sdk_path = Path(__file__).resolve().parents[2] / "hs-game-sdk" / "python"
    if _sdk_path.exists() and str(_sdk_path) not in sys.path:
        sys.path.insert(0, str(_sdk_path))
    try:
        from hs_game_sdk import SATANIC_BUFFS, SATANIC_DEBUFFS
    except Exception:
        SATANIC_BUFFS = ()
        SATANIC_DEBUFFS = ()

PORT = 8780
# Windows sometimes reserves a port range (Hyper-V/WSL) and refuses the bind.
# So free ports are tried in order; whichever works is opened in the browser.
# The list used to begin with 8766. That port is one of 8765-8774, which the
# Item Editor keeps for itself, so with the editor open the panel landed on
# 8780 anyway, and the Toolkit Hub's check of 8766 reached the editor. The hub
# checks the first candidate (catalog/sources.toml): move both together. No
# version number in this comment: VersionStampTests allow exactly one.
PORT_CANDIDATES = [8780, 8801, 8899, 9133, 9777]
# Ports Chromium refuses to open, failing with net::ERR_UNSAFE_PORT before it
# connects. The panel window is pywebview on WebView2, which is Chromium, so a
# panel bound to one of these shows the player a blank window. The candidates
# above are clear of it; only a port the OS picks (the port-0 fallback in
# main()) can land here. On a machine whose dynamic port range is 1024-15000
# about 0.12% of port-0 binds do; Windows' default 49152-65535 holds none.
# Source: Chromium's net/base/port_util.cc kRestrictedPorts and the Fetch
# standard's "bad port" list, checked against installed Edge on 2026-09-26.
# 4190 and 6679 are Fetch-only so far, kept in case Chromium adopts them.
CHROMIUM_RESTRICTED_PORTS = frozenset({
    1, 7, 9, 11, 13, 15, 17, 19, 20, 21, 22, 23, 25, 37, 42, 43, 53, 69, 77,
    79, 87, 95, 101, 102, 103, 104, 109, 110, 111, 113, 115, 117, 119, 123,
    135, 137, 139, 143, 161, 179, 389, 427, 465, 512, 513, 514, 515, 526,
    530, 531, 532, 540, 548, 554, 556, 563, 587, 601, 636, 989, 990, 993,
    995, 1719, 1720, 1723, 2049, 3659, 4045, 4190, 5060, 5061, 6000, 6566,
    6665, 6666, 6667, 6668, 6669, 6679, 6697, 10080,
})
SAFE_BIND_ATTEMPTS = 10
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
    # The Key of Terror's six boss parts and their six infernal versions. No
    # drop type on purpose: the parts share LoadDrops type 41 with Relics, so
    # opening that gate would drop Relics as well, and the plugin skips the
    # part scripts while the Relic gate rolls. The slider only scales the
    # parts' own roll where the game already rolls it, which is on bosses.
    ("primeevil", "Prime Evil Parts", None),
    # Not offered: Satanic materials sit at base 100,000-50,000,000, which no
    # division reaches.
    # Ruby Keys: the game's own gate is already open (chances[18] = 1), only the
    # key's 1-in-1,500,000 roll needs scaling.
    ("ruby", "Ruby Keys", None),
]

DROPS = [
    ("gold", "Gold", ""),
    ("mining_ore", "Mining Ore Multiplier", ""),
    # A separate option beside Mining Ore Multiplier (issue #36): the game's own
    # dig completion runs this many times per node. The two work independently
    # and multiply when both are on. The plugin caps it at 10 too
    # (MiningOreMod.hpp's kMaxRolls).
    ("mining_ore_rolls", "Mining Ore Extra Rolls", ""),
]

# The drops rows with their own plugin command and a ceiling of 10; every other
# drops row is a `dropmult` up to 100.
MINING_DROP_COMMANDS = {"mining_ore": "miningore", "mining_ore_rolls": "miningrolls"}


def drop_multiplier(key, value) -> int:
    if key not in {k for k, *_ in DROPS}:
        raise ValueError("unknown drop setting")
    try:
        return max(1, min(10 if key in MINING_DROP_COMMANDS else 100, int(float(value))))
    except (TypeError, ValueError, OverflowError):
        return 1


def drop_command(key, value) -> str:
    amount = drop_multiplier(key, value)
    verb = MINING_DROP_COMMANDS.get(key)
    return f"{verb} {amount}" if verb else f"dropmult {key} {amount}"

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
# "add" rows are added to the game's own total instead (`statadd`): Faster
# Cast Rate and Skill Haste in points, All Skills in whole skill levels.
# Skill Haste stops at 200: the game counts at most 200 in total (measured,
# ForgePact#114 Live 1), so a larger bonus would change nothing.
PERCENT_STATS = [
    ("damage", "Total Damage", 1000, 5, "multiply"),
    ("attackspeed", "Attack Speed", 500, 5, "multiply"),
    ("castrate", "Faster Cast Rate", 500, 5, "add"),
    ("skillhaste", "Skill Haste", 200, 5, "add"),
    ("allskills", "All Skills", 100, 1, "add"),
    ("lifereplenish", "Life Replenish", 1000, 5, "multiply"),
    ("manareplenish", "Mana Replenish", 1000, 5, "multiply"),
    ("defense", "Defense", 1000, 5, "multiply"),
    ("critdamage", "Critical Strike Damage", 1000, 5, "multiply"),
    ("critchance", "Critical Strike Chance", 500, 5, "multiply"),
    ("spellcritdamage", "Spell Critical Strike Damage", 1000, 5, "multiply"),
    ("spellcritchance", "Spell Critical Strike Chance", 500, 5, "multiply"),
]
# A skill level has no fraction, so a typed All Skills value is kept whole.
WHOLE_PERCENT_STATS = frozenset({"allskills"})

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
    # Pet collects relics (#124, docs/pet-relic-collector-research.md): while
    # the pet is out it walks to relics on screen and picks each up through the
    # game's own loot pickup, never one the player already owns at 10/10. Its
    # own switch, separate from Pet collects quest items. Off by default like
    # the other mod toggles.
    "mod_pet_relic_pickup": False,
    # The pet moves on from loot it cannot pick up (#94): when the game's own
    # companion loot pickup sits on one item, the plugin holds that item back
    # for the pet and lets it choose another (docs/pet-loot-stuck-research.md).
    # Off by default like the other mod toggles.
    "mod_pet_loot_unstick": False,
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
    # Move all into the stash (ForgePact #68, docs/stash-move-research.md):
    # with the stash open, F4 moves the bag tab on show into the stash tab on
    # show, each item by the game's own move; what the tab has no room for,
    # or does not take, stays in the bag. Off by default; offline only, like
    # every mod here.
    "mod_stash_move_all": False,
    # Far scenery sleep (docs/far-sleep-research.md): a zone's far trees,
    # bushes, hay, rocks and fences sleep until a player comes near, so the
    # game stops walking them every frame. Off by default; offline only,
    # like every mod here.
    "mod_far_sleep": False,
    # Rolling density copies (docs/population-performance-analysis.md): with
    # Monster Density above x1 the extra spawners are made as the player
    # approaches instead of all at once when a zone loads. Off by default;
    # offline only, like every mod here.
    "density_rolling": False,
    # Sleep loot your filter hides (docs/hidden-loot-research.md): a ground
    # item the player's own loot filter hides is put to sleep at the end of
    # the frame it dropped in. Off by default; offline only, like every mod
    # here.
    "mod_hidden_loot": False,
    # Its child, "Show hidden loot while held": the key or mouse button that
    # shows the slept items while held, as a Windows virtual-key code from
    # HIDDEN_LOOT_KEYS (0 = none). Left Alt (164) by default, the owner's
    # choice (2026-09-28); only sent while the switch is on.
    "mod_hidden_loot_key": 164,
    # Gems of Incarnation (docs/incarnation-gems-research.md): every gem that
    # drops is Mythic (4-5 mods, a seed the game itself rolled Mythic), and every
    # gem's mods show their best tier's top value. Both off by default, like
    # every mod here (the owner's call, 2026-09-25).
    "mod_gem_mythic": False,
    "mod_gem_maxroll": False,
    # Which mods a Mythic gem carries: "all", or the ticked mods (GEM_AFFIXES).
    "gem_filter": "all",
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
    # Slider on/off switches, keyed by SLIDER_SWITCH_IDS.  Only switches the
    # player turned off are stored (`False`); a missing id means on, so every
    # older saved file reads as all-on.  An off slider keeps its value; the
    # backend acts as if it stood at its default (effective_cfg).
    "switches": {},
    # The panel's colour theme, painted as data-theme on the page's root.  A
    # panel setting only: no command ever carries it.
    "theme": "default",
}

# Settings that only the panel reads: /api/set saves them and tells the plugin
# nothing, even while the game runs.
PANEL_SETTINGS = ("theme",)

# Every slider that has an on/off switch: "<section>.<key>" for the table rows,
# the bare key for the four top-level sliders.  Monster Density is not here:
# `density_on` has always been its switch.  `enemy_speed_ct` is a scope, not a
# value, so it has no default to fall back to.
SLIDER_SWITCH_IDS = tuple(
    [f"stats.{k}" for k, *_ in STATS]
    + [f"percent_stats.{k}" for k, *_ in PERCENT_STATS]
    + [f"spawners.{k}" for k, *_ in SPAWNERS]
    + [f"drops.{k}" for k, *_ in DROPS]
    + [f"keys.{k}" for k, *_ in KEYS]
    + ["rarity_rare", "rarity_ancient", "angelic_items", "enemy_speed"])

THEME_NAME = re.compile(r"[a-z][a-z0-9-]{0,31}")


def switch_target(switch_id: str):
    """(section, key) a switch id names; section is None for a top-level slider."""
    section, _, key = switch_id.rpartition(".")
    return section or None, key


def switch_on(cfg: dict, switch_id: str) -> bool:
    return (cfg.get("switches") or {}).get(switch_id) is not False


def effective_cfg(cfg: dict) -> dict:
    """A copy of cfg as the game should see it: every slider whose switch is off
    holds its DEFAULTS value.  cfg itself keeps the remembered values."""
    eff = copy.deepcopy(cfg)
    for switch_id in SLIDER_SWITCH_IDS:
        if switch_on(cfg, switch_id):
            continue
        section, key = switch_target(switch_id)
        if section:
            eff.setdefault(section, {})[key] = DEFAULTS[section][key]
        else:
            eff[key] = DEFAULTS[key]
    return eff


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
    # The game's exit code (incident reports, issue #76): watcher() holds a
    # handle while the game runs and reads it once the process has ended.
    _K32.WaitForSingleObject.argtypes = [_wintypes.HANDLE, _wintypes.DWORD]
    _K32.WaitForSingleObject.restype = _wintypes.DWORD
    _K32.GetExitCodeProcess.argtypes = [_wintypes.HANDLE, _ctypes.POINTER(_wintypes.DWORD)]
    _K32.GetExitCodeProcess.restype = _wintypes.BOOL
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


# "Show hidden loot while held" (Sleep loot your filter hides): the keys and
# mouse buttons the panel offers, as (Windows virtual-key code, name), in the
# order the select lists them. panel/src/hidden-loot-keys.js carries the same
# list, code for code (test_hidden_loot_panel_contract.py). Left out: the left
# and right mouse buttons (1, 2, the game's own), and generic Alt (18), Right
# Alt (165) and F10 (121), which can put the game window into its menu mode.
HIDDEN_LOOT_KEYS = (
    ((0, "None"), (164, "Left Alt"), (17, "Ctrl"), (16, "Shift"), (9, "Tab"), (20, "Caps Lock"),
     (32, "Space"), (192, "Backquote"), (4, "Middle mouse"), (5, "Mouse 4"), (6, "Mouse 5"))
    + tuple((111 + n, f"F{n}") for n in range(1, 10)) + ((122, "F11"), (123, "F12"))
    + tuple((ord(c), c) for c in "ABCDEFGHIJKLMNOPQRSTUVWXYZ")
    + tuple((ord(c), c) for c in "0123456789")
)
HIDDEN_LOOT_KEY_CODES = frozenset(code for code, _name in HIDDEN_LOOT_KEYS)


def hidden_loot_key_value(value):
    """The key code a saved or posted value stands for: None is 0 (no key), an
    int the list offers is itself, anything else (bools included) is None."""
    if value is None:
        return 0
    if isinstance(value, int) and not isinstance(value, bool) and value in HIDDEN_LOOT_KEY_CODES:
        return value
    return None


def hidden_loot_key_cmd(cfg: dict) -> str:
    """`hiddenloot key <vk>` for the saved key; a hand-edited code the list
    does not offer sends the default, Left Alt, rather than a refused one."""
    code = hidden_loot_key_value(cfg.get("mod_hidden_loot_key", DEFAULTS["mod_hidden_loot_key"]))
    return f"hiddenloot key {DEFAULTS['mod_hidden_loot_key'] if code is None else code}"


# Every mod a Gem of Incarnation rolls (docs/incarnation-gems-research.md,
# measured on 14,521 game-built gems): (stat, category, label), worded as the
# game's tooltip words it. A skill grant is one row, named by its skill slot
# (462); its levels slot (463) goes with it.
GEM_CATEGORIES = ("Attack", "Skills", "Elemental skills", "Defense", "Life & mana", "Loot")
GEM_AFFIXES = (
    (28, "Attack", "#% Enhanced Damage"),
    (68, "Attack", "#% Increased Attack Speed"),
    (74, "Attack", "+# to Attack Rating"),
    (75, "Attack", "#% Increased Attack Rating"),
    (95, "Attack", "#% Chance for a Deadly Blow"),
    (128, "Attack", "+# to Physical Damage"),
    (448, "Attack", "+# to Minimum Weapon Damage"),
    (450, "Attack", "+# to Maximum Weapon Damage"),
    (101, "Skills", "#% Magic Skill Damage increased by"),
    (196, "Skills", "#% Faster Cast Rate"),
    (201, "Skills", "+# to All Skills"),
    (462, "Skills", "+# to a single skill"),
    (133, "Elemental skills", "+# to Fire Skill Damage"),
    (134, "Elemental skills", "#% Fire Skill Damage increased by"),
    (137, "Elemental skills", "+# to Cold Skill Damage"),
    (138, "Elemental skills", "#% Cold Skill Damage increased by"),
    (141, "Elemental skills", "+# to Arcane Skill Damage"),
    (142, "Elemental skills", "#% Arcane Skill Damage increased by"),
    (145, "Elemental skills", "+# to Lightning Skill Damage"),
    (146, "Elemental skills", "#% Lightning Skill Damage increased by"),
    (149, "Elemental skills", "+# to Poison Skill Damage"),
    (150, "Elemental skills", "#% Poison Skill Damage increased by"),
    (29, "Defense", "#% Enhanced Defense"),
    (173, "Defense", "#% to All Resistances"),
    (175, "Defense", "#% to Fire Resistance"),
    (177, "Defense", "#% to Cold Resistance"),
    (179, "Defense", "#% to Lightning Resistance"),
    (181, "Defense", "#% to Arcane Resistance"),
    (183, "Defense", "#% to Poison Resistance"),
    (52, "Life & mana", "+# to Life"),
    (53, "Life & mana", "#% Life Increased by"),
    (57, "Life & mana", "#% Life stolen per Hit"),
    (60, "Life & mana", "+# to Mana"),
    (61, "Life & mana", "#% Mana Increased by"),
    (64, "Life & mana", "#% Mana stolen per Hit"),
    (284, "Loot", "#% Increased Magic Find"),
)
GEM_AFFIX_IDS = frozenset(stat for stat, _c, _l in GEM_AFFIXES)


def gem_filter_value(value):
    """The gem mod filter to keep: "all", or the ticked mods (sorted). None: not
    a filter (junk, an unknown mod, or nothing ticked)."""
    if value == "all":
        return "all"
    if not isinstance(value, list) or not value:
        return None
    ids = set()
    for v in value:
        if isinstance(v, bool) or not isinstance(v, (int, float)) or v != int(v) or int(v) not in GEM_AFFIX_IDS:
            return None
        ids.add(int(v))
    return "all" if ids == GEM_AFFIX_IDS else sorted(ids)


def gem_filter_command(cfg: dict) -> str:
    value = gem_filter_value(cfg.get("gem_filter", "all")) or "all"
    return "gemfilter all" if value == "all" else "gemfilter " + ",".join(str(i) for i in value)


def build_cmds(cfg: dict) -> list:
    # A slider whose switch is off stands at its default here, so startup,
    # auto-apply and the launch watcher all leave it at vanilla.
    cfg = effective_cfg(cfg)
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
    if cfg.get("mod_pet_relic_pickup", False):
        # Safe to send at launch: the player build installs no hook for it,
        # only a per-frame tick while on, like petquest.
        out.append("petrelic 1")
    if cfg.get("mod_pet_loot_unstick", False):
        # Safe to send at launch: no hook, only a per-frame tick while on.
        out.append("petunstick 1")
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
    if cfg.get("mod_stash_move_all", False):
        # Safe to send at launch: `stashmoveall 1` only turns the switch on;
        # nothing moves until F4 is pressed with the stash open.
        out.append("stashmoveall 1")
    if cfg.get("mod_far_sleep", False):
        # Safe to send at launch: `farsleep 1` only turns the switch on; the
        # plugin touches nothing before a zone has settled with a player in
        # it, and never in town or a menu.
        out.append("farsleep 1")
    if cfg.get("density_rolling", False):
        # Safe to send at launch: `densityroll 1` only sets the reach the
        # plugin's density copy queue takes jobs within.
        out.append("densityroll 1")
    if cfg.get("mod_hidden_loot", False):
        # Safe to send at launch: `hiddenloot 1` only turns the switch on; the
        # plugin installs its hook on the first enable after setup. The key
        # goes first, 0 included, so the switch never starts with a stale one.
        out.append(hidden_loot_key_cmd(cfg))
        out.append("hiddenloot 1")
    if cfg.get("mod_gem_mythic", False):
        # Safe to send at launch, like toggleguard: `gemmythic 1` only arms it,
        # and the plugin hooks the gem drop once a player exists.
        out.append("gemmythic 1")
        # Only a narrowed filter is sent: the plugin starts with every mod.
        if (gem_filter_value(cfg.get("gem_filter", "all")) or "all") != "all":
            out.append(gem_filter_command(cfg))
    if cfg.get("mod_gem_maxroll", False):
        # Safe to send at launch: no hook of its own, CreateItemNew is hooked at init.
        out.append("gemmaxroll 1")
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

    # Checked again here, not only on entry: the backup work above can take a
    # while, and a DLL the game has loaded cannot be overwritten (issue #123).
    if game_running(cfg):
        return {"err": "Close the game first, then click Install again."}
    copying = "AurieCore.dll"
    try:
        _sh.copy2(core, b / "AurieCore.dll")
        copying = "the mods folder"
        (b / "mods" / "aurie").mkdir(parents=True, exist_ok=True)
        (b / "mods" / "native").mkdir(parents=True, exist_ok=True)
        copying = "YYToolkit.dll"
        _sh.copy2(yytk, b / "mods" / "aurie" / "YYToolkit.dll")
        copying = "BloodPactPlugin.dll"
        _sh.copy2(plug, b / "mods" / "aurie" / "BloodPactPlugin.dll")
        steps.append("mod DLLs installed/updated")
        if sensor is not None:
            copying = TRACKER_SENSOR_DLL
            _sh.copy2(sensor, b / "mods" / "aurie" / TRACKER_SENSOR_DLL)
            steps.append("HS Offline Tracker sensor installed")
    except OSError as e:
        # Before this, the handler's catch-all answered HTTP 500 with the raw
        # "[WinError 32] ..." text in a toast.
        return {"err": f"Could not write {copying} ({e.strerror or e}). It is in use or read-only: "
                       "close the game and any other tool using the mod files, then click Install again."}
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
            "chain": mod_chain(cfg), "pluginBuild": plugin_build_state(cfg, use_cache=False)}


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
            "chain": mod_chain(cfg), "pluginBuild": plugin_build_state(cfg, use_cache=False)}


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


STASH_MOVE_ALL_STATE = b"stashmoveall: state="
STASH_MOVE_ALL_TAIL = 64 * 1024


def stash_move_all_session(cfg=None) -> str:
    """What Move all into the stash says it is doing: `on`, `off`, or
    `off-after-loss` when a move it could not confirm turned it off for the
    rest of the session (review of ForgePact #68: the switch kept showing on).

    Read from the last `stashmoveall: state=` line of out.txt, which the
    plugin prints on every switch and after a loss. out.txt is rotated at
    plugin load, so its tail is this game session's; only the last 64 KB is
    read, since the line is printed at each switch and the file grows to
    megabytes. No line there (or no log) is an empty string: the plugin has
    not said anything, which is not the same as off."""
    try:
        path = ipc_dir(cfg) / "out.txt"
        with path.open("rb") as fh:
            fh.seek(0, 2)
            size = fh.tell()
            fh.seek(max(0, size - STASH_MOVE_ALL_TAIL))
            tail = fh.read()
    except Exception:
        return ""
    at = tail.rfind(STASH_MOVE_ALL_STATE)
    if at < 0:
        return ""
    state = tail[at + len(STASH_MOVE_ALL_STATE):].split(b"\n", 1)[0].split(b" ", 1)[0].strip()
    return {b"on": "on", b"off": "off", b"off-for-this-session": "off-after-loss"}.get(state, "")


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


# ---- incident reports (issue #76) -----------------------------------------
# The plugin is the one writer of a report bundle
# (bp_ipc\reports\<yyyymmdd-HHMMSS>_<perf|freeze|crash>\): it notices FPS
# drops and freezes while the game runs, and a crash at the next load, when
# the previous session's log has no clean-shutdown line. The panel never
# writes under reports\; it lists every report (Setup > Incident reports) and
# tells nobody about any of them: no toast, no message box, no notice of any
# kind, for an FPS drop, a freeze, a crash or an exit with an error (the
# owner, 2026-10-02). It records what only a process outside the game can
# see:
# - exit.json: the exit code of a game that ended with anything but 0, read
#   from a handle watcher() holds while the game runs, and the Windows
#   Application log's crash record for it. The plugin folds the file into the
#   next crash bundle and deletes it.
# - panel.json: this panel's version and pid; the plugin records the version
#   in a report.
# The design and its limits: docs/incident-report.md.
REPORTS_DIR = "reports"
EXIT_JSON = "exit.json"
PANEL_JSON = "panel.json"
REPORT_LIST_MAX = 10
PROCESS_QUERY_LIMITED_INFORMATION = 0x1000
SYNCHRONIZE = 0x00100000
_WAIT_OBJECT_0 = 0
_REPORT_DIR = re.compile(r"^(\d{4})(\d\d)(\d\d)-(\d\d)(\d\d)(\d\d)_(perf|freeze|crash)$")
# The crash records Windows Error Reporting writes. Readable without admin
# rights; on the machine this was written on it answered record 71576
# (another program's crash) on 2026-10-02.
APP_ERROR_QUERY = "*[System[Provider[@Name='Application Error'] and (EventID=1000)]]"
# What the panel recorded, which is all /api/state's incidents carries beside
# the report list; nothing here is ever shown to the player as a notice.
# lastExit: exit.json's facts for an exit with an error, None after a clean
# exit or before any. exitWatch counts what the exit watch did (D15): the pid
# whose handle is held, the exits read from a held handle and the last code.
# Without it "no exit was recorded" could not be told from "the panel never
# saw the game".
INCIDENTS = {
    "lastExit": None,
    "exitWatch": {"pidHeld": None, "exitsSeen": 0, "lastCode": None},
}
_INCIDENTS_LOCK = threading.Lock()
_REPORT_UTC: dict = {}
# The plugin's clean-shutdown marker (IncidentMonitor.hpp's ShutdownMarker):
# "==== clean shutdown ====" from the ExitProcess hook, "==== clean shutdown
# (detach) ====" from the unload fallback; the prefix is what counts.
CLEAN_SHUTDOWN_PREFIX = b"==== clean shutdown"
CLEAN_SHUTDOWN_TAIL = 64 * 1024


def open_exit_handle(pid):
    """A handle on process `pid` that can read its exit code once it ends, or
    None. The handle keeps the exit code readable after the process is gone."""
    if os.name != "nt" or not pid:
        return None
    handle = _K32.OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE, False, int(pid))
    return handle or None


def exit_code_of(handle):
    """The exit code of the process behind `handle`, or None while it runs.

    Whether it ended is asked of the handle itself (signalled), not read from
    the code: a process may exit with 259, which is also STILL_ACTIVE."""
    if os.name != "nt" or not handle:
        return None
    if _K32.WaitForSingleObject(handle, 0) != _WAIT_OBJECT_0:
        return None
    code = _wintypes.DWORD()
    if not _K32.GetExitCodeProcess(handle, _ctypes.byref(code)):
        return None
    return code.value


def close_exit_handle(handle):
    if os.name == "nt" and handle:
        _K32.CloseHandle(handle)


def exit_code_text(code) -> str:
    """An exit code as Windows writes one: 0xC0000005."""
    return f"0x{int(code) & 0xFFFFFFFF:08X}"


def game_pid(cfg=None):
    """The pid of the configured Hero_Siege.exe, or None. Matched on the full
    path, as game_running() is, so another copy of the game is not it."""
    target = exe_path(cfg)
    try:
        target_lower = str(target.resolve()).lower()
    except Exception:
        target_lower = str(target).lower()
    try:
        wanted = target.name.lower()
        for pid, image in snapshot_processes():
            if image.lower() == wanted and process_image_path(pid).lower() == target_lower:
                return pid
    except Exception:
        pass
    return None


def _xml_name(tag) -> str:
    return tag.rsplit("}", 1)[-1]


def parse_app_error_events(text: str) -> list:
    """The Application Error (1000) records in `wevtutil qe ... /f:xml` output,
    newest first as queried: app_name, module_name, exception_code,
    faulting_offset, event_record_id, process_id and time_created. [] for
    nothing, or for text that is not those records."""
    import xml.etree.ElementTree as ElementTree   # only when a game crashed

    body = re.sub(r"<\?xml[^>]*\?>", "", text or "").strip()
    if not body:
        return []
    try:
        root = ElementTree.fromstring("<Events>" + body + "</Events>")
    except ElementTree.ParseError:
        return []
    events = []
    for event in root:
        if _xml_name(event.tag) != "Event":
            continue
        data, record, created = {}, None, None
        for el in event.iter():
            name = _xml_name(el.tag)
            if name == "EventRecordID":
                record = el.text
            elif name == "TimeCreated":
                created = el.get("SystemTime")
            elif name == "Data" and el.get("Name"):
                data[el.get("Name")] = (el.text or "").strip()
        try:
            record = int(record)
        except (TypeError, ValueError):
            record = None
        pid = data.get("ProcessId") or ""
        try:
            pid = int(pid, 16) if pid.lower().startswith("0x") else int(pid)
        except ValueError:
            pid = None
        events.append({
            "app_name": data.get("AppName") or None,
            "module_name": data.get("ModuleName") or None,
            "exception_code": data.get("ExceptionCode") or None,
            "faulting_offset": data.get("FaultingOffset") or None,
            "event_record_id": record,
            "process_id": pid,
            "time_created": created,
        })
    return events


def query_app_errors(count: int = 20):
    """(records, probe): the newest `count` Application Error records, and
    {"queried", "records_seen"}, which tells "no record" (queried, 0 seen)
    from "could not read the log" (not queried)."""
    probe = {"queried": False, "records_seen": 0}
    if os.name != "nt":
        return [], probe
    try:
        result = subprocess.run(
            ["wevtutil", "qe", "Application", f"/c:{int(count)}", "/rd:true", "/f:xml", "/q:" + APP_ERROR_QUERY],
            capture_output=True, timeout=10, creationflags=CREATE_NO_WINDOW)
    except (OSError, subprocess.SubprocessError):
        return [], probe
    if result.returncode != 0:
        return [], probe
    events = parse_app_error_events(result.stdout.decode("utf-8", errors="replace"))
    return events, {"queried": True, "records_seen": len(events)}


def match_game_event(events, exe_name: str, pid=None):
    """The newest record of `exe_name` crashing; with a pid, only that
    process's record, so an older crash of the game never stands in."""
    name = (exe_name or "").lower()
    for event in events:
        if (event.get("app_name") or "").lower() != name:
            continue
        if pid is None or event.get("process_id") == pid:
            return event
    return None


def session_shut_down_cleanly(cfg=None) -> bool:
    r"""Whether the game session that just ended wrote ForgePact's clean-shutdown
    marker: a line starting "==== clean shutdown" after out.txt's last
    "==== BloodPact plugin loaded ====" line.

    The marker is the last thing the plugin writes, so only the tail is read
    (out.txt grows to megabytes). A banner above that tail leaves the whole
    tail inside the last session; a marker before the last banner is the
    previous session's. A missing or unreadable out.txt is False."""
    try:
        with (ipc_dir(cfg) / "out.txt").open("rb") as fh:
            fh.seek(0, 2)
            size = fh.tell()
            start = max(0, size - CLEAN_SHUTDOWN_TAIL)
            fh.seek(start)
            tail = fh.read()
    except OSError:
        return False
    lines = tail.split(b"\n")
    if start > 0:
        lines = lines[1:]          # the first line is cut by the seek
    session = []
    for line in lines:
        if line.lstrip(b"\xef\xbb\xbf").startswith(b"==== " + BOOT_MARKER):
            session = []
        else:
            session.append(line)
    return any(line.startswith(CLEAN_SHUTDOWN_PREFIX) for line in session)


def record_game_exit(cfg, code, pid=None, attempts: int = 3, wait: float = 2.0):
    r"""What the panel does when the game it watched has exited with `code`.

    0 is a clean exit: nothing is written and the last exit reads as none.
    Anything else writes bp_ipc\exit.json (only into a bp_ipc that exists)
    and is returned. Windows writes the crash record a moment after the
    process ends, so the log is read up to `attempts` times.

    An exit after ForgePact's clean-shutdown marker (a mod file aborting
    during exit, Known Limitations item 25) is still written, with
    after_clean_shutdown true, so the plugin can fold it into its next-load
    note rather than read it as a crash. Nobody is told about either kind
    (the owner, 2026-10-02): the exit is recorded and shown in /api/state."""
    if code == 0:
        INCIDENTS["lastExit"] = None
        return None
    exit_utc = time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())
    event, probe = None, {"queried": False, "records_seen": 0}
    for attempt in range(max(1, attempts)):
        events, probe = query_app_errors()
        event = match_game_event(events, exe_path(cfg).name, pid)
        if event or not probe["queried"]:
            break
        if attempt + 1 < attempts:
            time.sleep(wait)
    facts = {
        "exit_code": exit_code_text(code),
        "exit_utc": exit_utc,
        "faulting_module": event["module_name"] if event else None,
        "faulting_offset": event["faulting_offset"] if event else None,
        "exception_code": event["exception_code"] if event else None,
        "event_record_id": event["event_record_id"] if event else None,
        "event_probe": {"queried": bool(probe["queried"]), "records_seen": int(probe["records_seen"])},
        "after_clean_shutdown": session_shut_down_cleanly(cfg),
    }
    folder = ipc_dir(cfg)
    if folder.is_dir():
        try:
            staged = folder / (EXIT_JSON + ".tmp")
            staged.write_text(json.dumps(facts, indent=1), encoding="utf-8")
            os.replace(staged, folder / EXIT_JSON)
        except OSError:
            pass
    INCIDENTS["lastExit"] = facts
    return facts


def watch_game_exit(cfg, running: bool, hold: dict):
    """One watcher() pass of the exit watch. `hold` is {"pid", "handle"}.

    A held handle whose process has ended is read and closed, whether or not
    another copy is running now (a restart between two polls); then, while
    the game runs and nothing is held, its process is opened. A game that was
    running before the panel started is opened on first sight the same way."""
    facts = None
    if hold["handle"] is not None:
        code = exit_code_of(hold["handle"])
        if code is not None:
            handle, pid = hold["handle"], hold["pid"]
            hold.update(pid=None, handle=None)
            close_exit_handle(handle)
            with _INCIDENTS_LOCK:
                watch = INCIDENTS["exitWatch"]
                watch.update(pidHeld=None, exitsSeen=watch["exitsSeen"] + 1, lastCode=exit_code_text(code))
            facts = record_game_exit(cfg, code, pid)
    if running and hold["handle"] is None:
        pid = game_pid(cfg)
        handle = open_exit_handle(pid) if pid else None
        if handle:
            hold.update(pid=pid, handle=handle)
            with _INCIDENTS_LOCK:
                INCIDENTS["exitWatch"]["pidHeld"] = pid
    return facts


def _report_utc(path: Path, match) -> str:
    """The report's time: report.json's "utc" when it is readable, else the
    folder name's stamp."""
    key = str(path)
    if key in _REPORT_UTC:
        return _REPORT_UTC[key]
    try:
        utc = json.loads((path / "report.json").read_text(encoding="utf-8")).get("utc")
        if isinstance(utc, str) and utc:
            _REPORT_UTC[key] = utc
            return utc
    except (OSError, ValueError, AttributeError):
        pass
    y, mo, d, h, mi, s = match.groups()[:6]
    return f"{y}-{mo}-{d}T{h}:{mi}:{s}"


def incident_reports(cfg=None, limit=REPORT_LIST_MAX) -> list:
    r"""The report folders under bp_ipc\reports\, newest first: [{"dir",
    "kind", "utc"}]. Anything not named like a bundle is left out."""
    try:
        entries = list((ipc_dir(cfg) / REPORTS_DIR).iterdir())
    except OSError:
        return []
    found = []
    for path in entries:
        match = _REPORT_DIR.match(path.name)
        if match and path.is_dir():
            found.append((path, match))
    found.sort(key=lambda pm: pm[0].name, reverse=True)
    if limit is not None:
        found = found[:limit]
    return [{"dir": path.name, "kind": match.group(7), "utc": _report_utc(path, match)} for path, match in found]


def incidents_state(cfg) -> dict:
    """/api/state's "incidents", the counters copied under their lock."""
    with _INCIDENTS_LOCK:
        exit_watch = dict(INCIDENTS["exitWatch"])
    return {"reports": incident_reports(cfg), "lastExit": INCIDENTS["lastExit"], "exitWatch": exit_watch}


def write_panel_json(cfg=None) -> bool:
    r"""Write bp_ipc\panel.json ({"version", "pid"}) when its content would
    change. Never creates bp_ipc. True when it wrote."""
    folder = ipc_dir(cfg)
    if not folder.is_dir():
        return False
    body = json.dumps({"version": __version__, "pid": os.getpid()})
    path = folder / PANEL_JSON
    try:
        if path.read_text(encoding="utf-8") == body:
            return False
    except (OSError, ValueError):
        pass
    try:
        staged = folder / (PANEL_JSON + ".tmp")
        staged.write_text(body, encoding="utf-8")
        os.replace(staged, path)
        return True
    except OSError:
        return False


def open_reports_folder(cfg=None) -> dict:
    """/api/openreports: open bp_ipc\\reports in Explorer. The panel never
    creates it: no folder means no report has been saved yet."""
    folder = ipc_dir(cfg) / REPORTS_DIR
    if not folder.is_dir():
        return {"err": "no reports yet: ForgePact has not saved one"}
    try:
        os.startfile(str(folder))
    except (AttributeError, OSError) as e:
        return {"err": f"could not open the reports folder: {e}"}
    return {"ok": "opened the reports folder"}


def watcher():
    """Re-apply the settings automatically every time the game LAUNCHES."""
    # False is intentional: if the panel itself starts after the game, the
    # first pass must still attach and apply the saved configuration.
    was_running = False
    last_state = None
    exit_hold = {"pid": None, "handle": None}
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
        # Incident reports (issue #76), in a try of their own, so nothing here
        # can stop a launch from being noticed above. Recording only: a new
        # report is listed by /api/state and nobody is told.
        try:
            write_panel_json(cfg)
            watch_game_exit(cfg, now, exit_hold)
        except Exception:
            pass


# ---- the plugin in the game vs the one this ForgePact ships (issue #123) ----
# Updating ForgePact replaces the panel and its modfiles, never the copy in the
# game's mods\aurie: only Install Mod Plugin writes that one.  So after an
# update the game went on loading the old plugin with nothing saying so, and a
# switch the old plugin does not know showed ON while the plugin answered
# "command unavailable in player build" (Move all into the stash's
# `stashmoveall` and Extra packs' `densityroll`, sent to the plugin of the
# release before them).  No version is written here: this file names its own
# exactly once (VersionStampTests).
#
# Which build a DLL is, is read from its bytes, never by loading it: LoadLibrary
# would run the plugin's initialisers inside the panel and hold the file open.
# ModManager.hpp's boot line, "==== BloodPact plugin loaded ==== v<x.y.z>", is a
# string literal every plugin since 1.3.20 carries; older plugins carry the
# marker with no version.  The version is for the person reading the message;
# the SHA-256 decides whether two files are the same build, because a recut or
# a build from source carries the same version in different bytes.
PLUGIN_DLL = "BloodPactPlugin.dll"
_PLUGIN_VERSION_RE = re.compile(re.escape(BOOT_MARKER) + rb" ==== v(\d+(?:\.\d+){1,3})")
# The states in which the panel asks for Install Mod Plugin.
PLUGIN_STALE_STATES = ("older", "different", "unknown")
_PLUGIN_FACTS_LOCK = threading.Lock()
_PLUGIN_FACTS: dict = {}


def plugin_dll_facts(path: Path, use_cache: bool = True) -> dict | None:
    """{"sha256", "marker", "version"} of one plugin DLL, or None if unreadable.

    /api/state is polled for the whole session, so the answer is cached by
    (size, mtime_ns), as offline_launcher._exe_facts caches the exe's: a
    changed file is a different key, so an Install shows on the next poll.
    A launch decision passes use_cache=False.
    """
    name = str(path).lower()
    try:
        before = path.stat()
        key = (before.st_size, before.st_mtime_ns)
        if use_cache:
            with _PLUGIN_FACTS_LOCK:
                hit = _PLUGIN_FACTS.get(name)
            if hit is not None and hit[0] == key:
                return hit[1]
        data = path.read_bytes()
        after = path.stat()
    except OSError:
        return None
    found = _PLUGIN_VERSION_RE.search(data)
    facts = {"sha256": hashlib.sha256(data).hexdigest(),
             "marker": BOOT_MARKER in data,
             "version": found.group(1).decode("ascii") if found else None}
    # A file rewritten while it was being read is answered, not remembered.
    if (after.st_size, after.st_mtime_ns) == key:
        with _PLUGIN_FACTS_LOCK:
            _PLUGIN_FACTS[name] = (key, facts)
    return facts


def _plugin_version_key(version: str) -> tuple:
    parts = [int(part) for part in version.split(".")]
    return tuple(parts + [0] * (4 - len(parts)))


def plugin_build_state(cfg=None, use_cache: bool = True) -> dict:
    """How the plugin in the game compares with the one this ForgePact ships.

    `state` is one of:
      missing    no plugin in the game (mod_chain() already says so);
      no-bundle  nothing to compare with: a source checkout that has not run
                 Prepare-Plugin.bat, or a release extracted without modfiles;
      current    the same file, byte for byte;
      older      an older ForgePact's plugin (one from before 1.3.20 carries
                 no version, so it is older than any that does);
      newer      a newer ForgePact's plugin, under an older panel;
      different  the same version in different bytes (a recut, or a build
                 from source);
      unknown    a file under the plugin's name that cannot be read or that
                 carries no ForgePact boot line.
    `installed` and `bundled` are each side's version, None when it has none.
    """
    installed_path = exe_path(cfg).parent / "mods" / "aurie" / PLUGIN_DLL
    if not installed_path.exists():
        return {"state": "missing", "installed": None, "bundled": None}
    installed = plugin_dll_facts(installed_path, use_cache)
    bundled_path = find_src(PLUGIN_DLL, PLUGIN_SOURCES)
    bundled = plugin_dll_facts(bundled_path, use_cache) if bundled_path is not None else None
    result = {"installed": installed["version"] if installed else None,
              "bundled": bundled["version"] if bundled else None}
    # Byte-identical is current whatever the bytes say: sameness is the hash's
    # to decide, and the boot line only names the build.
    if installed is None:
        state = "unknown"
    elif bundled is not None and installed["sha256"] == bundled["sha256"]:
        state = "current"
    elif not installed["marker"]:
        state = "unknown"
    elif bundled is None:
        state = "no-bundle"
    elif installed["version"] is None:
        state = "older"
    elif bundled["version"] is None:
        state = "different"
    else:
        mine = _plugin_version_key(installed["version"])
        shipped = _plugin_version_key(bundled["version"])
        state = "older" if mine < shipped else "newer" if mine > shipped else "different"
    result["state"] = state
    return result


def plugin_stale_advice(build: dict) -> str:
    """What to tell a player whose game has a stale plugin; "" when it has not."""
    state = build.get("state")
    installed = "v" + build["installed"] if build.get("installed") else "an old version"
    shipped = "v" + build["bundled"] if build.get("bundled") else "a newer one"
    if state == "older":
        lead = f"The mod plugin in the game is {installed}, but this ForgePact ships {shipped}."
    elif state == "different":
        lead = f"The mod plugin in the game is not the {installed} build this ForgePact ships."
    elif state == "unknown":
        lead = "The mod plugin file in the game is not one this ForgePact can read."
    else:
        return ""
    return lead + " Close the game, then click Install Mod Plugin in Setup."


def refresh_stale_plugin(cfg) -> str:
    """Put this ForgePact's plugin into the game when the game's is older.

    Called only by launch_modded_game(), as offline_launcher.launch_game()'s
    `prepare` step.  That runs after launch_safety_blocker() has found no
    Hero_Siege.exe running (it refuses when the processes cannot be listed) and
    before the game starts, so the file replaced here is not loaded; the copy
    is staged beside it and swapped in whole (_atomic_verified_copy), so a
    failure leaves the old plugin as it was.

    It replaces BloodPactPlugin.dll and nothing else, and only when:
    - this is a release build of ForgePact (frozen).  From source, a plugin in
      the game is a developer's own build or a hand-installed test DLL;
    - the game's plugin is older than the bundled one: never a newer one, the
      same version in other bytes, or a file this panel cannot identify;
    - the game's YYToolkit.dll and AurieCore.dll are byte-identical to the
      bundled ones.  The plugin is compiled against YYToolkit's headers and
      the two must match (a mismatch crashes on a vtable), and those DLLs are
      shared with other tools such as HS Offline Tracker, so replacing them
      stays Install Mod Plugin's job, done when the player clicks it.
    Otherwise nothing is written and the answer is the advice to click
    Install Mod Plugin, for the launch message: the launch goes ahead as it
    did before this existed.  Returns "" when there is nothing to say, and
    never raises.
    """
    try:
        build = plugin_build_state(cfg, use_cache=False)
        advice = plugin_stale_advice(build)
        if build["state"] != "older" or not getattr(sys, "frozen", False):
            return advice
        game = exe_path(cfg).parent
        for name, installed in (("YYToolkit.dll", game / "mods" / "aurie" / "YYToolkit.dll"),
                                ("AurieCore.dll", game / "AurieCore.dll")):
            bundled = find_src(name, MODFILE_SOURCES)
            if bundled is None or not installed.exists() or _sha256_file(bundled) != _sha256_file(installed):
                return advice
        _atomic_verified_copy(find_src(PLUGIN_DLL, PLUGIN_SOURCES), game / "mods" / "aurie" / PLUGIN_DLL)
        after = plugin_build_state(cfg, use_cache=False)
    except Exception as exc:
        return f"The mod plugin could not be updated ({exc}). Close the game, then click Install Mod Plugin in Setup."
    if after["state"] != "current":
        return plugin_stale_advice(after) or "The mod plugin could not be updated."
    was = "v" + build["installed"] if build["installed"] else "an old version"
    return f"Mod plugin updated from {was} to v{after['installed']} before launch."


def launch_modded_game(cfg: dict) -> dict:
    """Use the embedded launcher with this panel's path, never a second config."""
    def validate_plugin() -> str:
        chain = mod_chain(cfg)
        if not all(chain.get(key) for key in ("patched", "aurieCore", "yytk", "plugin")):
            return "The mod plugin installation is incomplete. Close the game and click Install Mod Plugin first."
        return ""

    notes = []

    def refresh_plugin() -> None:
        note = refresh_stale_plugin(cfg)
        if note:
            notes.append(note)

    result = offline_launcher.launch_game(exe_path(cfg), validate_extra=validate_plugin,
                                          prepare=refresh_plugin)
    if notes and result.get("ok"):
        result["ok"] = result["ok"] + " " + " ".join(notes)
    return result


def _panel_dist() -> Path:
    """Where the built panel frontend (panel/, Svelte + Vite) lives.

    FORGEPACT_PANEL_DIST wins when set (tests and the sandbox server use it);
    a frozen exe reads the copy build_release.py bundled with --add-data
    "<panel/dist>;panel"; from source it is the checkout's panel/dist, written
    by `npm --prefix panel run build`.
    """
    override = os.environ.get("FORGEPACT_PANEL_DIST")
    if override:
        return Path(override)
    if getattr(sys, "frozen", False) and hasattr(sys, "_MEIPASS"):
        return Path(sys._MEIPASS) / "panel"
    return Path(__file__).resolve().parent.parent / "panel" / "dist"


# Resolved once; do_GET reads this global on every request, so a test can patch it.
PANEL_DIST = _panel_dist()
# The suffixes a Vite build of the panel produces. Anything else is served as
# opaque bytes rather than guessed at.
PANEL_MIME = {
    ".html": "text/html; charset=utf-8",
    ".js": "text/javascript",
    ".css": "text/css",
    ".svg": "image/svg+xml",
    ".woff2": "font/woff2",
    ".png": "image/png",
    ".json": "application/json",
    ".ico": "image/x-icon",
}


def panel_file(url_path: str):
    """The file under PANEL_DIST a GET path names, or None.

    None for anything whose resolved path leaves PANEL_DIST (``..``, encoded
    separators, a drive letter), for directories, and for names the OS rejects,
    so the handler answers all of them with the same 404.
    """
    try:
        root = Path(PANEL_DIST).resolve()
        target = (root / unquote(url_path).lstrip("/\\")).resolve()
        if target.is_relative_to(root) and target.is_file():
            return target
    except (OSError, ValueError):
        pass
    return None


class H(BaseHTTPRequestHandler):
    def log_message(self, *a):
        pass

    def _reply(self, code, headers, b):
        try:
            self.send_response(code)
            for name, value in headers:
                self.send_header(name, value)
            self.end_headers()
            self.wfile.write(b)
        except (ConnectionAbortedError, ConnectionResetError, BrokenPipeError):
            # The client went away mid-answer (the page closed while a poll was
            # in flight): end this request quietly instead of letting
            # socketserver print a traceback for a closed client.
            self.close_connection = True

    def _file(self, path: Path):
        b = path.read_bytes()
        headers = [("Content-Type", PANEL_MIME.get(path.suffix.lower(), "application/octet-stream")),
                   ("Content-Length", str(len(b)))]
        if path.suffix.lower() == ".html":
            # The page names its hashed assets; a cached copy would keep asking
            # for the previous build's files after an update.
            headers.append(("Cache-Control", "no-cache"))
        self._reply(200, headers, b)

    def _json(self, obj, code=200):
        b = json.dumps(obj, ensure_ascii=False).encode("utf-8")
        self._reply(code, [("Content-Type", "application/json; charset=utf-8"),
                           ("Content-Length", str(len(b)))], b)

    def do_GET(self):
        u = urlparse(self.path)
        index = Path(PANEL_DIST) / "index.html"
        if u.path == "/" and index.is_file():
            self._file(index)
        elif u.path == "/":
            # No build: say what to run rather than answer 200 with a page
            # that is not there.
            self._json({"err": "panel not built: run npm --prefix panel run build"}, 503)
        elif u.path == "/api/state":
            cfg = load_cfg()
            _exe = exe_path(cfg)
            self._json({"cfg": cfg, "version": __version__,
                        "gameRunning": game_running(cfg),
                        "ipcOk": ipc_dir(cfg).exists(),
                        "pluginMods": plugin_mod_state(cfg),
                        "stash_move_all_session": stash_move_all_session(cfg),
                        "eacStatus": eac_status(_exe) if _exe.exists() else "",
                        "chain": mod_chain(cfg),
                        "pluginBuild": plugin_build_state(cfg),
                        "spawners": [[k, i, l, mx] for k, i, l, mx in SPAWNERS],
                        "drops": [[k, l, h] for k, l, h in DROPS],
                        "stats": [[k, l, mx, step] for k, l, mx, step in STATS],
                        "percentStats": [[k, l, mx, step, mode] for k, l, mx, step, mode in PERCENT_STATS],
                        # Third field is the drop type: the panel's explanation text
                        # differs per family because they do not all mean the same thing.
                        "keys": [[k, l, t] for k, l, t in KEYS],
                        "satanicBuffs": [[i, l, d] for i, l, d in SATANIC_BUFF_LIST],
                        "gemAffixes": [[s, c, l] for s, c, l in GEM_AFFIXES],
                        "gemCategories": list(GEM_CATEGORIES),
                        "satanicDebuffs": [[i, l, d] for i, l, d in SATANIC_DEBUFF_LIST],
                        "minEnabledSatanicBuffs": MIN_ENABLED_SATANIC_BUFFS,
                        "minEnabledSatanicDebuffs": MIN_ENABLED_SATANIC_DEBUFFS,
                        "lastApplied": LAST["applied"], "queued": LAST["queued"],
                        "launch": offline_launcher.launch_status(),
                        "incidents": incidents_state(cfg)})
        elif u.path != "/api" and not u.path.startswith("/api/") and (f := panel_file(u.path)):
            self._file(f)
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
                    if key in WHOLE_PERCENT_STATS:
                        value = float(round(value))
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
                elif sec == "switches":
                    if not isinstance(key, str) or key not in SLIDER_SWITCH_IDS:
                        self._json({"err": "unknown switch"}, 400); return
                    # A fresh dict: load_cfg's copy of DEFAULTS is shallow, so
                    # editing cfg["switches"] in place could edit DEFAULTS.
                    switches = dict(cfg.get("switches") or {})
                    if bool(val):
                        switches.pop(key, None)   # on is the absence of an entry
                    else:
                        switches[key] = False
                    cfg["switches"] = switches
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
                elif key in ("density_on", "auto_apply", "map_reveal", "map_reveal_packs", "map_reveal_spawn", "headhunter", "tyrant", "beacon", "mod_filter_max_relics", "mod_orb_pickup_radius", "mod_pet_quest_pickup", "mod_pet_relic_pickup", "mod_pet_loot_unstick", "mod_auto_prospect", "mod_auto_prospect_bag", "mod_toggle_indicator", "mod_toggle_guard", "mod_restart_anytime", "mod_far_sleep", "mod_stash_move_all", "density_rolling", "mod_hidden_loot", "mod_craft_mats", "mod_gem_mythic", "mod_gem_maxroll"):
                    cfg[key] = bool(val)
                elif key == "mod_hidden_loot_key":
                    code = hidden_loot_key_value(val)
                    if code is None:
                        self._json({"err": "invalid hidden loot key"}, 400)
                        return
                    cfg[key] = code
                elif key == "gem_filter":
                    value = gem_filter_value(val)
                    if value is None:
                        self._json({"err": "tick at least one gem mod"}, 400)
                        return
                    cfg[key] = value
                elif key == "mod_skill_timer_style":
                    style = str(val).strip().lower()
                    if not skill_timer_style_valid(style):
                        self._json({"err": "invalid skilltimer style"}, 400)
                        return
                    cfg[key] = style
                elif key == "theme":
                    if not isinstance(val, str) or not THEME_NAME.fullmatch(val):
                        self._json({"err": "invalid theme"}, 400)
                        return
                    cfg["theme"] = val
                save_cfg(cfg)
                live = ""
                # A theme is the panel's own: nothing to tell the plugin.
                if game_running(cfg) and not (sec is None and key in PANEL_SETTINGS):
                    # Sliders send what the game should see: a slider whose
                    # switch is off sends its default's command, as density
                    # does while density_on is off.
                    eff = effective_cfg(cfg)
                    sid = key if sec == "switches" else (f"{sec}.{key}" if sec else key)
                    if sec == "switches" or (sid in SLIDER_SWITCH_IDS and not switch_on(cfg, sid)):
                        # A switch takes its slider's own branch below with the
                        # effective value, so off equals "set to default" and on
                        # equals "set to the remembered value"; a slider moved
                        # while off sends its default the same way.
                        sec, key = switch_target(sid)
                        val = eff[sec][key] if sec else eff[key]
                    if sec == "keys":
                        # A live change must also restore families moved back to
                        # x1; startup's sparse command list deliberately cannot.
                        send_cmds(build_key_cmds(eff.get("keys", {}), include_resets=True), cfg)
                    elif sec == "drops":
                        send_cmds([drop_command(key, eff[sec][key])], cfg)
                    elif sec == "stats":
                        send_cmds([f"stat {key} {float(eff['stats'][key]):g}"], cfg)
                    elif sec == "percent_stats":
                        mode = next((md for k, _l, _mx, _st, md in PERCENT_STATS if k == key), "multiply")
                        bonus = float(eff["percent_stats"][key])
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
                    elif key == "mod_pet_relic_pickup":
                        send_cmds([f"petrelic {1 if cfg['mod_pet_relic_pickup'] else 0}"], cfg)
                    elif key == "mod_pet_loot_unstick":
                        send_cmds([f"petunstick {1 if cfg['mod_pet_loot_unstick'] else 0}"], cfg)
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
                    elif key == "mod_far_sleep":
                        send_cmds([f"farsleep {1 if cfg['mod_far_sleep'] else 0}"], cfg)
                    elif key == "mod_stash_move_all":
                        send_cmds([f"stashmoveall {1 if cfg['mod_stash_move_all'] else 0}"], cfg)
                    elif key == "density_rolling":
                        send_cmds([f"densityroll {1 if cfg['density_rolling'] else 0}"], cfg)
                    elif key == "mod_hidden_loot":
                        cmds = [f"hiddenloot {1 if cfg['mod_hidden_loot'] else 0}"]
                        # Turning it on restates the key first, as map reveal
                        # restates its child: a key picked while the game was
                        # closed never reached the plugin.
                        if cfg["mod_hidden_loot"]:
                            cmds.insert(0, hidden_loot_key_cmd(cfg))
                        send_cmds(cmds, cfg)
                    elif key == "mod_hidden_loot_key":
                        # Always sent, switch on or off: the plugin stores the
                        # key and reads it only while the switch is on.
                        send_cmds([hidden_loot_key_cmd(cfg)], cfg)
                    elif key == "mod_gem_mythic":
                        cmds = [f"gemmythic {1 if cfg['mod_gem_mythic'] else 0}"]
                        if cfg["mod_gem_mythic"]:
                            cmds.append(gem_filter_command(cfg))
                        send_cmds(cmds, cfg)
                    elif key == "gem_filter":
                        send_cmds([gem_filter_command(cfg)], cfg)
                    elif key == "mod_gem_maxroll":
                        send_cmds([f"gemmaxroll {1 if cfg['mod_gem_maxroll'] else 0}"], cfg)
                    elif key == "mod_skill_timer_style":
                        # Always explicit, including off: a style change (or
                        # turning it off) needs the plugin told either way.
                        send_cmds([f"skilltimer {cfg['mod_skill_timer_style']}"], cfg)
                    elif key in ("rarity_rare", "rarity_ancient"):
                        # Always explicit: "rarity off" returns a live hook to vanilla.
                        send_cmds([rarity_cmd(eff)], cfg)
                    elif key == "angelic_items":
                        send_cmds([angelic_cmd(eff)], cfg)
                    elif key in ("enemy_speed", "enemy_speed_ct"):
                        # Always explicit: "enemyspeed 1 ct" turns a live hook back to vanilla.
                        send_cmds([enemy_speed_cmd(eff)], cfg)
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
            elif u.path == "/api/openreports":
                self._json(open_reports_folder(cfg))
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
# The policy's functions and timing constants live in the panel's
# panel/src/poll-policy.js, which the tests execute through node. Python keeps
# the watched-field list, which the tests hold equal to the module's own and to
# HS-Offline-Launcher's.
POLL_WATCHED_FIELDS = ["gameRunning", "ipcOk", "lastApplied", "queued"]


def bind_safe_server(bind, max_attempts=SAFE_BIND_ATTEMPTS):
    """Call bind() (no arguments; returns a server with .server_port and
    .server_close()) until it lands on a port outside
    CHROMIUM_RESTRICTED_PORTS, then return that server. A rejected server is
    kept open until a safe one is bound: closing it first could hand the same
    port straight back if the OS gives out port-0 ports in sequence (6665-6669
    are consecutive). Raises RuntimeError, naming every port tried, if
    max_attempts is exhausted first, closing every server bound on the way."""
    rejected = []
    for _ in range(max_attempts):
        server = bind()
        if server.server_port not in CHROMIUM_RESTRICTED_PORTS:
            for stale in rejected:
                stale.server_close()
            return server
        rejected.append(server)
    tried = [stale.server_port for stale in rejected]
    for stale in rejected:
        stale.server_close()
    raise RuntimeError(
        "no port outside CHROMIUM_RESTRICTED_PORTS after {} attempts, "
        "tried: {}".format(max_attempts, tried))


def refuse_to_start(message):
    """Say why the panel is not opening. The packaged exe is --windowed, so a
    print reaches nobody there; a message box does."""
    print(f"ForgePact: {message}", flush=True)
    if os.name == "nt":
        try:
            # A private handle, not ctypes.windll: see the Win32 note above.
            _ctypes.WinDLL("user32").MessageBoxW(None, message, "ForgePact", 0x10)
        except Exception:
            pass


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
        # Every candidate port was reserved/busy -> let the OS assign a free
        # port, but never one the panel window refuses to open: that is a
        # blank window, so say why and stop instead.
        try:
            srv = bind_safe_server(lambda: ThreadingHTTPServer(("127.0.0.1", 0), H))
        except RuntimeError:
            refuse_to_start(
                "ForgePact could not open its panel. Ports "
                + ", ".join(str(p) for p in PORT_CANDIDATES)
                + " are all in use, and every other port Windows offered is "
                "one the panel window is not allowed to open. Close programs "
                "you don't need and start ForgePact again.")
            return
        PORT = srv.server_port
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





