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
| **Combat Modifiers** | Total Damage, Attack Speed, Faster Cast Rate, Defense, Life/Mana Replenish, physical and spell Critical Chance/Damage |
| **Character Stats** | Experience, Magic Find and Movement Speed use the character's current total value, including equipment bonuses |
| **Full Map Reveal** | Clears fog of war in every zone (toggleable; F5 in-game also toggles it) |
| **Auto-apply** | Saved settings are re-sent every time the game starts |

ForgePact does not write permanent stat changes into your save or modify the game exe
for individual settings. Features are resolved by script/object name and applied in
memory while the offline game is running. Turning a modifier off restores its normal
value for that session.

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

**Chaos Tower and Shadow Realm** need one extra step: the game rolls for them once
per run and then latches a flag so they can never appear again. ForgePact clears
that flag right before each zone's markers activate, so every fresh zone gets its
own honest roll.

**Chaos Tower** and **Shadow Realm** are once-per-run mechanics. Their activation code
reads persistent flags (`Controller_obj.shadowRealmSpawned`, the protected
`chaosTowerStarted` / `chaosTowerSpawnZone`), so multiplying the marker alone changes
nothing after the first copy. While either slider is above x1, ForgePact resets those
flags immediately before each marker copy activates; the game's own placement, roll and
object creation then run unchanged. Both also carry a difficulty gate (Shadow Realm
needs the third tier or higher, Chaos Tower the second) — with the slider on, that single
check is satisfied for the duration of the activation call only, so both can appear on
every difficulty. Verified live on 2026-09-03: two markers gave two Shadow Realm portals
and two Chaos Towers in one zone.

### Enemy Movement Speed
**World → Enemy Movement Speed** adds 0-300 % to how fast monsters run at you, which is the
quickest way to shorten Chaos Tower waves. The plugin hooks `PathFindStartPath`, the single
place the game turns an enemy's base speed into path speed
(`moveSpeedCur = moveSpeed × movementSpdMultiplier` → `path_start`), and scales the base speed
only for the duration of that call: the walk animation stays in step, slows and debuffs still
apply on top, and nothing compounds. **Only inside Chaos Tower** (default on) keeps every other
zone vanilla; goblins and online client movement use their own movement code and are not
touched. Command: `enemyspeed <multiplier> [ct|all]` (`enemyspeed 1.5 ct`), `enemyspeed` alone
prints the status with path-start and applied counters.

### Headhunter (Custom Forge mechanic)
Forge any item in the Item Editor with **Mechanic: Headhunter** and switch on **World →
Headhunter** in the panel. Killing a **rare or champion** monster then grants its affixes to
you as 20-second buffs, through the game's own on-kill dispatcher and `BuffAdd`:

| monster affix | buff you gain (20 s) |
|---|---|
| Extra Fast | movement speed |
| Berserker, Raging, Enraged, Extra Strong, Punisher, Sharpshooter, Multishot, Burst Shot | attack speed |
| Vampiric, Venomous | life replenish |
| Fire Enchanted, Pyromaniac, Blazing, Meteoric | fire skill damage |
| Lightning Enchanted, Thunder Caller | lightning skill damage |
| Cold Enchanted | cast rate (no cold-damage buff id measured yet) |
| Arcana's Curse, Possessed | arcane skill damage |
| Manaburn | mana replenish + arcane damage |
| Stoneskin, Thick Skin, Magic Resistant, Antimagus | physical + magic damage reduction |
| Shielding, Fearless, Divine, Fallen Angel | defense |
| Colossal, Champion, Commander, Guardian of Hell, Bloating | max life + max mana |
| Stealthy, Time Lapsing, Wasped, Haunted | dodge |
| Treasure Gobbler | magic find |
| Fractal | experience gain |

**Head labels.** Every stolen affix floats above the character's name bar in gold with its
seconds left (`Extra Fast 17s   Vampiric 19s`, three per row), so you can see what you took
without opening the buff bar. The labels are drawn right after the game's own HUD buff row
(`DrawHudBuffs`) and projected through the active camera, so they follow the character at any
resolution. Commands: `hhlabel on|off`, `hhlabeloffset <px>` (height above the head, default
150), `hhlabelmax <n>` (labels kept, default 12, oldest drops first), `hhlabelfont <index|off>`.

Commands: `headhunter on|off|force|status`, `hhdur <seconds>`, `hhmap <affix> <buffId> [v0] [v1]`,
`hhdefault <buffId>|off`. The panel sends `headhunter force` at every game start while the switch
is on; `force` also stands in for the equipped-item check, which is not finished yet.

### Tyrant's Crown (Custom Forge mechanic)
Forge a helmet in the Item Editor with **Mechanic: Tyrant's Crown** and switch on **World →
Tyrant's Crown** in the panel. While it is on, monsters that spawn near you rise from normal to
**rare** with a 30 % chance (they get two affixes), and every rare or champion carries **one more
affix**. Ancients and bosses are never touched.

How: `EnemyRaritySettings(typeId)` runs from `Enemy_Parent_obj` Alarm 4 with the monster as
self, after the spawner decided `enemyRarity` and filled `enemyAffix` / `affixList`, but before
the stats, affix effects and the health bar are built (live-traced 2026-09-05: entry and exit
state identical). ForgePact changes the rarity and the affix flags at its entry, and the game
builds the monster exactly as if it had rolled that way — yellow name, affix labels and affix
behaviour included. Only live-confirmed affix indices are handed out. Live test: 847 monsters,
259 raised, 354 extra affixes, no crash.

**Rare monsters hunt you.** While the crown is on, every rare or champion also uses the Beacon's
hunt rules (see below): a map-sized aggro range, no leash, and it is kept awake inside the
`beaconwake` radius, so rares come for you from across the zone while normal monsters keep the
vanilla rules. Live check 2026-09-05 at the default 15 %: 243 monsters seen, 30 raised to rare,
64 extra affixes.

Commands: `tyrant on|off|force|status`, `tyrantchance <pct>` (normal → rare, default 30),
`tyrantaffix <pct>` (extra affix on rares/champions, default 100). The panel sends `tyrant force`
at every game start while its switch is on. Research build: `raritytrace <n>` logs entry/exit
state of the next n monsters.

### Beacon (Custom Forge mechanic)
Forge an amulet in the Item Editor with **Mechanic: Beacon** and switch on **World → Beacon**
in the panel. While it is on, **every monster on the map hunts you** the moment it spawns, and
none of them turns back.

How: an idle monster runs `PathFindScanTick` every few frames: `instance_nearest` finds the
player and, if `point_distance` is below the monster's **`distance`** variable (300 px vanilla;
`aggroRange` is a different value), `PathFindTakeTarget(player)` sets the target and
`PathFindAggroBroadcast` wakes the pack. In chase state `PathFindLeashCheck` drops the target
when the monster strays too far from home. ForgePact hands every scanning monster a map-sized
`distance` (and `aggroRange`; the vanilla values are kept in `fp_distance` / `fp_aggroRange` and
restored when the hunt is off) and skips the leash check; the game's own scan, target and pack
code does the rest, so the behaviour is the vanilla one, only without the distance limit.
Verified 2026-09-05 with the crown: 32 rares chasing at once, 65 monsters widened, 14 700 leash
checks skipped, packs following their rare leader through the game's own broadcast.

**Wake radius.** The game freezes every instance outside the players' view boxes each frame
(`ActivateDeactivateProps` from `Controller_obj` Step), and a frozen monster never scans, so on
its own the range change only reaches monsters near the screen. After that call the Beacon
re-activates every monster (and every `Enemy_Creator*` spawner) within `beaconwake` px of the
player (default 4000 ≈ 3-4 screens) and puts the ones beyond back to sleep, so everything inside
the radius keeps hunting. `beaconwake all` keeps the whole map awake (watch the frame rate in
dense zones), `beaconwake off` leaves the game's own freezing alone, `beaconwake creators off`
stops waking spawners.

**How far "hunts you" reaches.** The game steps monsters from `Controller_obj` through
`EnemyStepHandleNew` and its `monsterHandleArray`, and monsters beyond roughly 1500 px never get
an AI tick even while active (measured: zero scans from farther away, with or without the freeze
pass). So hunted monsters come for you from anywhere inside that update zone (about 1.5 screens,
far beyond the vanilla 300 px) and keep coming once they have you; monsters still farther out
wait until they enter the zone. Forcing their AI tick from the plugin (`beaconfarstep on`,
experimental) crashed the game on zone entry and ships off; `beaconwake every <n>` (default 6)
runs the game's freeze pass every n frames while a hunt is on.

**Spawn as if approached (experimental, off).** The spawner's periodic check (decompiled) calls
`distance_to_object(Player_obj)` and spawns its pack (`alarm[2]`) below 1050 px. `beaconspawn on`
makes that builtin answer 0 to every awake spawner; live it produced no extra packs in the tested
zone (those spawners had already spent their pack at zone load), so it ships off by default.

Commands: `beacon on|off|force|status`, `beaconrange <px>` (default 1000000 = whole map),
`beaconmode all|rare` (rare: only rares and champions hunt you), `beaconwake <px>|all|off`,
`beaconwake creators on|off`, `beaconspawn on|off`. The panel sends `beacon force` at every game
start while its switch is on. Research build: `aggrotrace <n>` logs the target events (take / set /
broadcast) and the scan variables of each monster type, `spawntrace <n>` samples the spawner
checks, `creatorprobe` counts awake spawners.

### Custom Forge identity: name, special affix, description
A forged item's sidecar line can carry three identity extras next to its stats; the Item
Editor (Item Forge → Identity) writes them and the plugin applies them when the item's
struct is created:

| extra | where the game shows it | how it is applied |
|---|---|---|
| `name=` | the tooltip title | written over `itemInfoStruct["28"]` after the game localized it; the magic prefix/suffix fields `["5"]`/`["4"]` are blanked so the item is shown under exactly that name |
| `affix=` | gold rows above the stat list (up to 3 lines) | the struct is tagged `fp_affix`; hooks on `DrawInventoryItemV2` / `DrawInventoryStatsNew` draw the rows in front of the first real stat row and return the added height, so the stats, lore and the box move down with it (see below) |
| `lore=` | the italic description under the stats | private localization key in `itemInfoStruct["29"]` |

**Base stats for the Item Editor.** While the Custom Forge hooks are active the plugin also records
the finished `itemStatStruct` of every item the game builds (keyed by `itemTimeStamp`) and writes
`bp_ipc\itemstats.json` at most once every two seconds. The Item Editor reads it to list an owned
item's exact stats — rolled affixes included — as editable base rows in the Item Forge; without
the file it falls back to its own tooltip model (base rows only) and says so.

Headhunter items without an `affix=` show the built-in line *Steals the affixes of slain
rare monsters for 20s*.

How the affix rows fit in: `DrawInventoryItemV2(x, y, scale, item, …)` draws the whole
inventory tooltip and calls `DrawInventoryStatsNew(x, y, item, statId, label, format, style,
…)` once for every known stat. That helper draws a row only when the item has the stat and
returns the row height (30) or 0, and the caller adds the return value to its y cursor. The
plugin draws its rows at `y`, hands the game `y + rows·30` for its own row, and returns both
heights, so nothing overlaps and the box grows by exactly the rows added.

### Known limitation — The Abyss
`Spawn_Abyss_obj` is **not** supported. It is the only mechanic in its family that
sets `discoverable = true`, which puts it behind a two-stage discover-then-activate
gate. The mechanic can be made to run, but it still declines to place its objects for
a reason we have not identified. Details and every ruled-out hypothesis are in
[`docs/S10-special-content-notes.md`](docs/S10-special-content-notes.md).

## 🔧 How to use

The Install button also places the **HS Offline Tracker** live sensor (`HSOfflineTrackerProducer.dll`) beside the plugin when it ships with ForgePact. It is a separate, read-only module: it only reports gold, XP, kills, drops, room and satanic zone to the tracker and changes nothing in the game. Remove Plugin takes it away again.

1. Run `ForgePact.exe` and set the path to your Season 10 `Hero_Siege.exe`.
2. Press **Install Mod Plugin**. ForgePact will back up your exe, copy the mod files into the
   game folder, and patch the exe so it loads Aurie on start.
3. Press **Launch Modded Game** and play offline. Change any setting in the panel and it applies
   immediately.

After a Hero Siege update, press **Install Mod Plugin** again before launching. Steam replaces
the patched game exe during updates; ForgePact safely keeps the previous backup and prepares the
new game build.

All gameplay modifiers are **Off by default** for a new player. No manual hook,
command file or DLL copying is required; the two buttons above handle installation
and launch.

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

ForgePact verifies that the backup belongs to the same game build before restoring
it. If a game update has already replaced the exe with a newer clean copy, the old
backup is preserved as `Hero_Siege.exe.aurie_backup.stale-<date>` and the main
backup is refreshed. A missing, patched, or mismatched backup is never restored
over the current exe.

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
- `plugin_build/build.bat` — builds the plugin. `build.bat release` produces the shipping
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
