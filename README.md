# ForgePact — Hero Siege Season 10 Offline Mod Panel

A one-click control panel for tuning your **offline, single-player** Hero Siege runs.
Set monster density, special-content rates and drop multipliers from a small local
panel; settings are applied live while the game runs and re-applied on every launch.

## ✨ Features

| Feature | What it does |
| --- | --- |
| **Monster Density** | 1–5× more enemies in 0.5 steps (1, 1.5, 2 …), through the game's own `Enemy_Creator` spawners |
| **Special Content** | Rift Portals, Battlefields, Cursed Orbs, Summon Portals, Chaos Pillars, Chaos Tower — up to 100× per zone |
| **Drop Rates** | Gold, Dungeon Keys, Angelic Keys, Chaos + Crystal Keys, Bifröst Key and Relics — up to 100× |
| **Full Map Reveal** | Clears fog of war in every zone (toggleable; F5 in-game also toggles it) |
| **Auto-apply** | Saved settings are re-sent every time the game starts |

ForgePact never writes to the game's code. Everything above goes through the game's
own scripts and objects, hooked by name at runtime — so a game update shifts addresses
without breaking anything.

High multipliers are throttled automatically. Extra spawn markers are queued and created
a few per frame instead of all at once, and creation pauses while the room is at its
heaviest (during a zone load the game briefly passes through 20–30k instances before
settling). Without this, high settings crashed the game on zone entry.

Drops are **not forced**. Every multiplier feeds the game's own dice: an item's
`droprate.base` is a "1 in N" value, so `x5` divides it by five — five times more
likely, still random, still capped by the game's own rules. The vanilla value is stored
on first touch, so moving the slider twice never compounds. `x1` restores vanilla
exactly.

Key and Relic families are also gated a second time: outside their home zone the game
rolls their drop type at zero chance, so the item can never come up no matter how good
its rate is. ForgePact opens that outer roll for the families you enable — using the
monster's **own** key chance as the base, never a fixed number. Families the zone
already rolls natively are left untouched.

Special content is spawned through the game's **own** mechanic: ForgePact multiplies
the `Spawn_<Name>_obj` marker objects and opens the shared `eSt` gate, then the game
places and runs the mechanic itself. Nothing is faked or hand-placed.

### Known limitation — The Abyss
`Spawn_Abyss_obj` is **not** supported. It is the only mechanic in its family that
sets `discoverable = true`, which puts it behind a two-stage discover-then-activate
gate. The mechanic can be made to run, but it still declines to place its objects for
a reason we have not identified. Details and every ruled-out hypothesis are in
[`docs/S10-special-content-notes.md`](docs/S10-special-content-notes.md).

## 🔧 How to use

1. Run `ForgePact.exe` and set the path to your Season 10 `Hero_Siege.exe`.
2. Press **Install**. ForgePact will back up your exe, copy the mod files into the
   game folder, and patch the exe so it loads Aurie on start.
3. Press **Launch** and play offline. Change any setting in the panel and it applies
   immediately.

### What gets installed
Into the game's `bin` folder:

```
Hero_Siege.exe                   PATCHED IN PLACE by AuriePatcher
Hero_Siege.exe.aurie_backup      your original exe, kept for restore
AurieCore.dll                    Aurie Framework  (AGPL-3.0, unmodified)
mods/aurie/YYToolkit.dll         YYToolkit        (AGPL-3.0, one modified source file)
mods/aurie/BloodPactPlugin.dll   this project's mod plugin
bp_ipc/                          the panel's command channel (created on first launch)
```

**Your `Hero_Siege.exe` is modified.** The original is saved next to it as
`Hero_Siege.exe.aurie_backup`, and **Remove Plugin** in the panel restores it and
deletes the mod files. Install on an offline copy of the game, not on the one you
play online with.

## ⚠️ Offline only — read this

This works **only** with anti-cheat (**EAC**) disabled. It does **not** work on the
normal online Steam client and never will — EAC blocks it by design. This is a
**single-player / offline** tool.

It does **not** include, provide, or explain any anti-cheat bypass, crack, or any way
to obtain the game — you must already have a copy set up for offline play. **Do not
use it online.** Modding online games is against their rules, and the mod will not
load there anyway.

## 📦 Source layout

- `src/forgepact.py` — the control panel (Python; packaged with PyInstaller for releases).
- `plugin/ModuleMain.cpp` — **the mod plugin** (BloodPactPlugin). This is the active
  implementation: it hooks the GameMaker runtime through YYToolkit and receives the
  panel's commands over `bp_ipc`.
- `yytoolkit-modified/` — our YYToolkit build and the notes for the one changed file.
- `modfiles_shipped/` — the binaries copied into the game folder.
- `plugin_build/derle.bat` — builds the plugin. `derle.bat yayin` produces the shipping
  build (features only); without an argument it produces the development build, which
  additionally carries the diagnostic commands used to investigate the game.
- `build_release.py` — packages `dist/ForgePact/` (the release zip contents).
- `docs/S10-special-content-notes.md` — the Season 10 reverse-engineering log: mechanic
  addresses, gate behaviour, measured crash thresholds, and every approach that did not
  work (written in Turkish).
- `docs/dungeon-key-research.md` — how the two-stage key/relic drop system was found:
  the outer `LoadDrops` chance gate, the per-item `droprate.base` roll, and why keys
  outside their home zone can never drop without opening the outer gate (Turkish).

## 📜 License — AGPL-3.0

ForgePact is released under the **GNU Affero General Public License v3.0** (see
[LICENSE](LICENSE)). It loads **Aurie Framework** and **YYToolkit**, both AGPL-3.0,
so the copyleft applies to this project as a whole.

You may use, study, modify, and redistribute it under the same license. If you
distribute a modified version, you must also make its complete source available
under AGPL-3.0.

## 🙏 Credits

- **Aurie Framework** — https://github.com/AurieFramework/Aurie (AGPL-3.0)
- **YYToolkit** — https://github.com/AurieFramework/YYToolkit (AGPL-3.0)

See [CREDITS.md](CREDITS.md) for the full notices, including which files are
unmodified and what our YYToolkit change does.

ForgePact is an independent, fan-made project and is **not affiliated with or
endorsed by** AurieFramework, Panic Art Studios, or Hero Siege.
