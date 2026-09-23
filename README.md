# ForgePact — Hero Siege Season 10 Offline Mod Panel

A one-click control panel for tuning your **offline, single-player** Hero Siege runs.
Set monster density, special-content rates and drop multipliers from a small local
panel; settings are applied live while the game runs and re-applied on every launch.

## ✨ Features

Dense zones are handled two ways by default: **Reveal full map** marks the
packs that do not exist yet instead of creating them, and Monster Density's
extra copies of a spawner are created over the following frames, nearest first.
The optional **Really spawn every pack on arrival (heavy)** sub-toggle, which
populates the whole zone early, still lags at high density: its v3 run at 4x
recorded latest scheduled work at 7.694 seconds and a peak frame interval of
232.430ms against a five-second target, and those counters do not prove every
group finished spawning.

The next local candidate shares caller classification within each existing
creation hook. Its production-body fixture reduced 2000 caller-object reads to
1000 per 1000 births, preserving nested enemy-chain behavior. This is a lookup
reduction, not a measured live frame-time improvement. It does not defer native
map objects, reduce density or add hooks to the player build.

The separate `build.bat profile` candidate measures nine map-generation stages
alongside AI/hunt/draw paths. Capture can start before preset-data generation;
the summary retains work before the first frame report. Player builds contain
none of these diagnostic hooks or the recorder. See
[capacity behavior and test scope](docs/population-capacity.md).

| Feature | What it does |
| --- | --- |
| **Monster Density** | 1–5× more enemies in 0.5 steps (1, 1.5, 2 …), through the game's own `Enemy_Creator` spawners |
| **Special Content** | Rift Portals, Battlefields, Cursed Orbs, Summon Portals, Chaos Pillars, Chaos Tower — up to 100× per zone |
| **Drop Rates** | Gold, Dungeon Keys, Angelic Keys, Chaos + Crystal Keys, Bifröst Key and Relics — up to 100× |
| **Mining Ore Amount** | Loot → Mining Ore Amount, 1–10×. Scales the stack quantity of ore awarded by mining; x1 is normal. A worn Miner's Helmet replaces it with 4× instead of stacking |
| **Miner's Helmet** | A signature helmet forged in the Item Editor. While worn: 4× ore from every mining node, and Vein Resonance - finishing a dig also digs the two nearest veins within 192 units that you could mine yourself (4× each, no chaining). Mods → Items shows whether it is worn ([details](#miners-helmet)) |
| **Angelic / Unholy Drops (Experimental)** | ForgePact's own die per kill; on a hit it builds one of its 49 real Angelic / Unholy uniques, or (since 1.4.5) Tyrant's Crown or Headhunter. x2 = 1 in 7,500 kills, each step adds a die, typable |
| **Combat Modifiers** | Total Damage, Attack Speed, Faster Cast Rate, Defense, Life/Mana Replenish, physical and spell Critical Chance/Damage |
| **Character Stats** | Experience, Magic Find and Movement Speed use the character's current total value, including equipment bonuses |
| **Full Map Reveal** | Clears fog of war in every zone, so waypoints, dungeon entrances, chests, shrines and mining nodes show immediately (toggleable; F5 in-game also toggles it). Its sub-toggle marks every monster pack on the map: most packs do not exist until you walk near them, so the map shows one marker per pack, by pack kind, without creating a single monster; the pack is born by the game when you get close and its real dots replace the marker. A second, off-by-default sub-toggle keeps the old behaviour of really spawning every pack on arrival, which costs frame time for the whole zone at high density. Markers are small icons by pack kind (ivory skull normal, hooded face ambush, magenta horned mask ancient, cyan helmet champion, gold chest colossal chest, amber skull trio legion, crowned crimson skull mini boss); spawners closer than ~96 px to each other, such as density copies, share one icon with a count badge. The icons are written to `<game>\bin\bp_ipc\packmarks\<kind>.png` on first use and never overwritten, so you can replace any of them with your own PNG (any size, transparent background; `packmarks reload` picks it up in a running game). Plugin command `packmarks` (`stat`, `icons 0|1`, `iconscale <mult>`, `reload`, `cluster <world px|0>`, `badge 0|1`, `style <kind|all> <subimage> <r> <g> <b>`, `radius <kind|all> <px>`, `fill <kind|all> 0|1`, `outline 0|1 [px]`, `alpha`, `ring 0|1`, `scale`, `list`) adjusts the look live; dots by kind are the fallback when an icon cannot be loaded |
| **Pet Collects Quest Items** | While your pet is out it walks to pick-up quest items on screen and collects them one at a time, crediting the objective through the game's own collect. Pick-up items only; activate/break/talk objectives are left alone |
| **Mark A Running Toggle Skill** | For a fixed set of toggle skills measured in-game, each either with its toggle sub-talent allocated or a toggle on its own: a soft red outline appears around that skill's skill-bar slot the whole time the toggle is running, and disappears when it stops. A skill outside that set is not covered, and a plain cast lights nothing (off by default) |
| **Stop Double Cast Re-casting A Toggle Skill** | A double cast proc can cast one of that same fixed set of toggle skills a second time on its own, flipping its toggle straight back; with this on, that extra cast is skipped and the toggle stays the way your press left it. It only steps in when you actually have the skill's toggle sub-talent, or the skill is a toggle on its own; your own presses and other skills' double casts are untouched (off by default) |
| **Restart Zone At Any Time** | The pause menu's Restart works straight away, in combat too, instead of waiting until you have been out of combat for a few seconds. Use the mouse: Restart lights up once the cursor is on it (off by default) |
| **Timed skill countdown** | For a small set of timed skills measured and tested in-game, plus most other skills with both a duration and a real cooldown, covered by rule and untested: draws how much of the cast is left over its skill-bar slot, in one of four looks (arc / bar / number / fade), disappearing at zero. A few skills are left out where a measurement showed the timer on the skill's own object is not the skill's duration. Companion skills (turrets, totems, hydra) are not covered. A few skills whose duration is a buff on you, measured in-game, are covered too, and other buff-only skills are not. In a fight, hits can add a little time to some skills (roughly 0.2 s each in our test) and the countdown rises slightly to match. A skill switched on as a toggle never gets a countdown. Off by default; a cast already running when you turn it on shows as full until the next cast |
| **Satanic Zone Mods** | Pick which of the game's 25 positive / 26 negative World Section mods can roll onto a Satanic Zone; everything is on by default |
| **Auto-prospect** | Off by default. Every item you drag or click into the Prospect Cube's grid is prospected at once by the game's own Prospect, so the 9×6 grid stops being the limit on a batch. Before each prospect the previous prospect's batch of materials goes to your materials tab (a sub-switch, on by default), so only the newest batch stays in the grid; the item you put in, ore included, is prospected, not moved (one exception: a batch material swapped out and dropped straight back in still goes to the tab); anything left in it when the game saves is lost ([details](#auto-prospect)) |
| **Remove Owned Relics** | Relics already at maximum level (10 out of 10) in your equipped slots, backpack or inventory stop dropping again, so a relic drop is one you can still use |
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

**Mining Ore Amount** is a separate quantity control, not a drop-chance multiplier.
It changes the amount of Copper, Iron, Gold, Ruby, Jade or Tarethium ore in a
normal mining reward. It preserves the chosen ore type and is scoped to the
mining call. By design it rewrites only the ore stack's quantity, so mining XP,
gems, prospecting and monster loot are left to the game. It installs its two
native hooks only when raised above x1, and uses the original
reward unchanged if it cannot validate the reward parameters. Checked in play on
2026-09-23: at x10 a 6-ore reward dropped 60. A worn [Miner's Helmet](#miners-helmet)
replaces it with 4×. See [research and test scope](docs/mining-ore-research.md).

Dungeon Keys, Angelic Keys and Relics are also gated a second time: outside their home
zone the game rolls their drop type at zero chance, so the item can never come up no
matter how good its rate is. ForgePact opens that outer roll for those three families
when you raise them — using the monster's **own** key chance as the base, never a fixed
number; Prime Evil parts, which share the relic roll, are skipped. Every other family
(runes, gems, orbs, scrolls, shards, fragments, ruby keys) only scales its own roll where
the game already drops it, so zone rules stay intact.

### Using the panel

Use the sidebar to move between **Setup**, **Modifiers**, **World**, **Loot** and
**Mods**. On narrow windows these become tabs across the top. The header shows
whether the game is running and whether your latest setting has saved. **Auto-apply**
and **Apply all now** keep their existing behavior.

Every slider has **− / +** buttons and an editable value. Click the value, or focus
it and press Enter, to type an exact number; Enter applies and Escape cancels.
Saved decimal values are retained after reopening the panel. Disabled Monster
Density shows **off**; enabling it restores the saved multiplier. Search
**Modifiers** or **Loot**, or choose **Modified**, to find changed settings quickly.
These filters never alter your settings. **How it works** expands the full details
for density, rarity, special content and drops.

Setting names now have matching icons: coin stacks for Gold, distinct portals
and towers for special content, different key families, and combat/stat symbols.
Satanic Zone modifiers use the same visual families. Names remain visible; the
icons do not replace descriptions or selection checkmarks. All artwork is
embedded locally and stays sharp at different display scales.

**Mods** groups related switches in cards. The sidebar shows only the five
main sections: Setup, Modifiers, World, Loot and Mods. The Mods page itself
has two sub-tabs at the top, showing one card at a time: **Quality of Life**,
everything that is not tied to a specific forged item (the relic drop pool
filter, orb pickup radius, map reveal, pet quest pickup, auto-prospect, the
toggle marker/guard and the timed skill countdown), and **Items**, the custom
forge mechanics tied to items made in the Item Editor (Headhunter, Tyrant's
Crown, Beacon). Quality of Life opens first; clicking the other sub-tab (or
using the arrow keys) switches which card you see, and the panel remembers
the one you last had open until you close it. Map population depends on
Reveal full map; its switch is unavailable while the parent is off. All
settings still use the existing local configuration and game plugin. The
panel adds no UI dependencies.

The reusable icon pack lives in `src/panel_icons.py`, beside `forgepact.py`.
Keep both files together when copying the Python source. To export the 69
individual SVGs and an offline preview gallery, run from the ForgePact folder:

```powershell
py src/panel_icons.py "C:\path\to\ForgePact-Icon-Pack"
```

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

### Signature drops
Tyrant's Crown (Great Helm) and Headhunter (Heavy Belt) are two more items in the **Angelic /
Unholy Drops** pool above: they drop from the very same die as every other item in it, such as
**Liquor Holster**, at exactly the same rate - so they never drop while that slider is off (the
default), and more often as it is raised, along with everything else in the pool. They are not
part of the game's own Angelic roll (the Blood Pact / dungeon "Angelic item drop chance" effect) -
only ForgePact's own die drops them. They arrive as SS-tier Unholy items, fully set up, and the
plugin recognises them on every load even without the Item Editor. `sigdrop status`,
`sigdrop crown`, `sigdrop belt` and `sigdrop off` are a test command that forces every kill
to drop the named item (or turns that off); it does not change the normal drop rate, which
always follows the Angelic / Unholy Drops slider.

### Tier (Custom Forge)
A forged item can carry a Tier letter (`tier=1` C … `tier=5` SS in the runtime file; the Item
Editor 2.15.3 offers it under Appearance). It is the letter the tooltip prints and the value loot
filters use.

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

### Satanic Zone Mods
**World → Satanic Zone Mods** lists every World Section modifier Hero Siege can put on a
Satanic Zone — 25 positive (buffs) and 26 negative (debuffs), names and descriptions from
the game's own data. Everything starts enabled; deselecting one keeps the plugin from
letting a future zone roll it, while the game still rolls the rest itself — nothing is
forced. Positive mods keep a minimum of 3 enabled, negative mods a minimum of 2, since a
Satanic Zone still needs a pool to draw from. Plugin command: `satmods <buff|debuff> <csv
of disabled ids>`.

Click anywhere on a modifier card to enable or disable it. Checked green cards are
positive modifiers; checked rose cards are negative modifiers. Search by name or
effect, or use **All mods / Enabled / Disabled** to narrow the lists. The counters
always show the entire enabled pool, including modifiers hidden by a filter.
At the minimum, enable a replacement before removing another modifier; the panel
explains this next to each list. **Enable all** restores a whole column, including
filtered-out entries. **Restore defaults** enables both pools and leaves every
other ForgePact setting alone. Changes save automatically; existing selections
are retained when reopening the panel. Pending saves lock further pool edits,
and a failed save reads back the stored selections and displays an error.

The lists scroll independently, stack on narrow screens, and support Tab/Space
keyboard selection. These controls change only the eligible pool, not the number
of modifiers that the game rolls.

No single game routine could be pinned down as "the roll" (see the research doc), so this
does not hook one: a poll running every 15 frames off the plugin's existing frame callback
watches `Controller_obj.satanicZoneBuff`/`satanicZoneDebuff` and, the instant either array's
content changes, swaps out any id you disabled for a random still-enabled one — live-tested
correcting a disabled id within one poll tick, leaving every enabled id untouched.

The buff/debuff table lives in [`hs-game-sdk/curated/satanic_zone.json`](../hs-game-sdk/curated/satanic_zone.json)
(hand-verified game knowledge, not extracted from the binary) and is shared with the rest
of the toolkit through the generated `hs_game_sdk` bindings — see
[`tools/generate_satanic_zone_sdk.py`](../tools/generate_satanic_zone_sdk.py). Full method
and live findings are in
[`docs/satanic-zone-mods-research.md`](docs/satanic-zone-mods-research.md).

## Remove owned relics from drop pool

Mods tab → Quality of Life. While it is on, a relic that is already at 10/10 in your
equipped slots, backpack or inventory is withheld when the game rolls a relic drop,
so what lands is one you can still level.

It is not a forced reroll of the loot table: the maxed relics are excluded for the
duration of that one roll and their normal drop rates are restored immediately
afterwards, so every other relic keeps the odds the game gives it. Turning the
toggle off restores vanilla behaviour for the session.

The panel sends `relicfilter 1`, which only **arms** the mod — the `DropRelic` hook
goes in later, once a player instance exists. Installing it during character
selection stalled the runner for about a minute (measured 2026-09-09), so the plugin
defers it to its frame callback. That is why the mod applies a moment after you are
in-game rather than at launch.

The plugin reports what it is doing in `<game>\bin\bp_ipc\out.txt`:

```
relicfilter -> ON (armed, applies once you are in-game)
relicfilter: hook installed -> ON
relicfilter: holding back 3 of 5 maxed relic(s) on this roll
```

`out.txt` no longer keeps every session forever: once it passes 2 MB, the plugin
rotates it to `out.prev.txt` the next time the game starts (never mid-session),
replacing any older `out.prev.txt`, so old logs no longer pile up and the
previous log is never lost. The log still grows during a session, so one long
session can make either file larger than 2 MB. **If
you're attaching a log to a bug report, attach both `out.txt` and
`out.prev.txt`** — the session you actually want may be the one that was just
rotated into the `.prev` file (e.g. the game crashed and you relaunched before
sending the report).

The first two lines only mean the mod is *armed and hooked* — until 1.3.19 they were
all there was, and they printed just as happily while it held nothing back.

The third line is the one that reports what actually happened, and it says which of
these five states you are in. Only the first is the mod working:

| Line | What it means |
| --- | --- |
| `holding back N of M maxed relic(s) on this roll` | Working. `M` maxed relics were found, `N` of them were withheld from this roll |
| `scanned, no maxed relics to hold back` | Working, nothing to do — you own no relics at 10/10 yet |
| `no player resolved yet, nothing scanned` | The scan did not run. Normal for a moment after the hook installs; persistent means it cannot find your character |
| `all 156 relics maxed, filter stands down (nothing left to drop instead)` | Every relic is maxed, so there is nothing better to drop and the filter deliberately does nothing |
| `found N maxed relic(s) but held back none (repository lookup failed)` | The scan worked, the drop table entry could not be read — usually an index that moved in a game update |
| `found N maxed relic(s) but held back none (drop table write failed)` | Both worked, the change to the drop rate did not land |

A working roll that could not hold back everything it found says so too, rather than
rounding up: `holding back 2 of 5 maxed relic(s) on this roll (1 write(s) failed)`.

It is printed once per change of state, so a normal session stays quiet after the
first line. **No line at all means the filter is not running.**

The count is what the plugin *confirmed it changed* — each suppression is written
through a status-returning call and then read back — not what the scan found and not
what it attempted. Those are three different numbers, and the first two versions of
this line reported the wrong one: the original printed the scan's input before the
guards and writes had run at all, and its replacement counted the rollback list, which
grows before each write and therefore still counted writes that threw or silently did
nothing (both reported in review of PR #4). The rollback list is deliberately kept
separate and still covers every *attempt*, because a write whose outcome is unknown
must still be restored. `tests/test_relic_filter_behavior.py` runs the real hook
against every case in this table.

### Known limitation — The Abyss
`Spawn_Abyss_obj` is **not** supported. It is the only mechanic in its family that
sets `discoverable = true`, which puts it behind a two-stage discover-then-activate
gate. The mechanic can be made to run, but it still declines to place its objects for
a reason we have not identified. Details and every ruled-out hypothesis are in
[`docs/S10-special-content-notes.md`](docs/S10-special-content-notes.md).

## Auto-prospect

Mods tab → Quality of Life → **Auto-prospect items put in the Prospect Cube**. Off by
default. The cube's 9×6 prospect grid fills long before a full inventory is through
it; with this on, every item you drag or click into the grid is prospected straight
away by the game's own Prospect, exactly as if you had pressed the button.

- **The previous batch goes to your materials tab.** A Prospect leaves one single-cell
  stack per material type in the grid. With the sub-switch **Move the previous materials
  to your materials tab** (on by default under Auto-prospect; `autoprospect bag 1|0`),
  each new insert first moves the materials the previous prospect made to your materials
  tab - the game's own stack move, the one a click on a material makes - and then
  prospects, so only the newest batch stays in the grid. Only that batch moves: what
  moves is what ForgePact's own last prospect produced, and of that only materials,
  identified by their item type. The item you put in is prospected and not
  moved, ore included, and a material you put in the grid yourself stays - with one
  exception, not seen yet: a batch material you swap an item onto and drop straight
  back in is still part of the batch, so it goes to the tab instead. After the
  cube is reopened, Auto-prospect is turned off and on, something is taken out of the
  grid, or the grid changes without an insert landing, ForgePact forgets the batch and
  the next prospect moves nothing. A material with no stack in the materials tab yet
  (the first of its kind) takes the route the game's own click takes for it: the game's
  preferred grid for the item, then a place into it - measured landing in the main bag,
  not the materials tab. A material
  the game will not take stays in the grid, and the reason is logged once each:
  `autoprospect: no-preferred-grid - …` (the game named no grid for a new type),
  `not-placed` (the place was not confirmed), `not-added` (the stack add was not
  confirmed) or `move-failed` (the move could not be made or checked; the line
  names the step that failed); a refusal has not been observed yet (a full bag or
  materials tab was not tested). A material is
  cleared from the grid only after the game reports the move succeeded; if one leaves
  the grid without that (`vanished`), or the grid cannot show it gone after it
  (`cell-kept`), the move turns itself off for the session and says so. With the
  sub-switch off, materials stay in the grid as after a normal Prospect.
- With fewer than 6 free cells, auto-prospect holds back and leaves the item in the
  grid; the first time in a session it writes
  `autoprospect: grid-full - holding back - N free cells, needs 6; empty some of the grid`
  to `bp_ipc\out.txt`.
- **Anything still in the prospect grid when the game saves is lost.** The game
  itself does not keep that grid across a save (measured with an unmodded grid: items
  left there were gone after a save and a relaunch). With auto-prospect on, the newest
  batch of materials sits in the grid, so empty it before you leave the cube.
- ForgePact runs the game's Prospect, and its stack move, at a moment the game did not
  choose. The Prospect was tested with junk items; the move to the materials tab is
  being re-tested on this build (an earlier build also moved an inserted ore back to
  the tab unprospected). Prospecting an ore only has a chance of giving materials - the
  game's own Prospect, pressed by hand with the mod off, used an ore up and gave nothing
  on the same call that gave materials before - so an ore that vanishes with nothing in
  its place is the game, not the mod. Back up `%LOCALAPPDATA%\Hero_Siege` first.
- It says what it did. If the hook it needs cannot see the game's own inserts, it turns
  itself off with an `autoprospect: hook TABLE-ONLY -> OFF` line; otherwise
  `autoprospect: hook installed -> ON`, and `autoprospect: first prospect - …` once the
  first item has turned into materials, and `autoprospect: first move to bag - …` once
  the first batch has gone to the materials tab. If a Prospect it runs leaves the grid unchanged,
  it says that once too: `autoprospect: the Prospect ran but the grid did not change - …`
  (or `… could not be read afterwards - …` when it could not check).

How it works, and the research that proved ForgePact can run the Prospect itself, is in
[`docs/prospect-window-research.md`](docs/prospect-window-research.md) (§ Stage B; the
move to the materials tab in § Stage C; first-of-its-kind materials and the ore finding
in § Stage D).

## Restart zone at any time

Mods tab → Quality of Life → **Restart zone at any time**. Off by default.

The pause menu's Restart normally refuses while the game counts you as in
combat, and only works once you have been out of combat for a few seconds.
With this on it works straight away. ForgePact does not restart anything
itself: while the mouse is on Restart, it changes the one value the game
uses to grey the button out, inside the game's own call for the button under
the cursor, and the game's own Restart does the rest. The game sets that
value again every frame, so in combat the button still looks greyed until
the cursor is on it, and moving the mouse away puts the wait back.

- **Mouse only.** Keyboard navigation does not reach the pause menu's
  buttons, and a controller has not been tried.
- **Enemies nearby.** A restart while enemies are alive and attacking is
  accepted as the player's choice; the game's own Restart handles it.
- `restartanytime stat` prints `written=` (frames the in-combat wait was lifted),
  `passed=` (Restart was already allowed), `otherNode=` (another button had
  the cursor), `unreadable=` (the button's value could not be read, so
  nothing was written) and `hook=`.

How the value was found, over three research rounds, is in
[`docs/restart-always-available-research.md`](docs/restart-always-available-research.md).

## Menu layout (for tools that drive the menus)

`menulayout` is a read-only command for tools that play through the main menu
and character select for you, such as the toolkit's `hs-drive` helper. It
lists the live instances of a fixed set of menu objects, and the interface
pieces under them, each with its position on the game window, so such a tool
clicks where the game says a button is instead of at a fixed spot. It changes
nothing in the game and has no switch in the panel.

The reply is a header, one row per instance and a footer:

```
menulayout: room=<RoomName> gui=<W>x<H> window=<W>x<H> fullscreen=<0|1> view=<x>,<y>,<w>,<h>
  obj=<ObjectName> id=<id> gui=<x>,<y> win=<cx>,<cy> bbox=<l>,<t>,<r>,<b> visible=<0|1> sprite=<SpriteName|none> ... text=<label>
menulayout: listed=<n> absent=<names or none> capped=<0|1>
```

`win` is the point on the window's client area, computed from the game's own
GUI and window sizes. `menulayout <ObjectName>` lists that one object the
same way. An older ForgePact answers `command unavailable in player build:
menulayout`. How the positions were measured, and which objects are the save
cards and `PLAY`, is in
[`docs/menu-layout-research.md`](docs/menu-layout-research.md).

## 🔧 How to use

**Running from source:** Python opens the control panel, but the game also needs
the compiled plugin. In a full toolkit checkout, run `Prepare-Plugin.bat` once
(requires Python and Visual Studio C++ Build Tools). It verifies the pinned
dependencies and builds the player plugin into `modfiles_shipped/`, without
changing your game. Then open `src/forgepact.py` and use **Install Mod Plugin**
with the game closed. A source zip does not include the ignored DLLs; a complete
release zip does.

**Game open** reports the game process, not a confirmed plugin connection. If
the installation is incomplete, a warning appears on every tab with a shortcut
to Setup. **Commands sent** means settings were handed to the plugin's command
file; it does not claim the game has already applied them.

The Install button also places the **HS Offline Tracker** live sensor (`HSOfflineTrackerProducer.dll`) beside the plugin when it ships with ForgePact. It is a separate, read-only module: it only reports gold, XP, kills, drops, room and satanic zone to the tracker and changes nothing in the game. Remove Plugin takes it away again.

1. Run `ForgePact.exe` and set the path to your Season 10 `Hero_Siege.exe`.
2. Press **Install Mod Plugin**. ForgePact will back up your exe, copy the mod files into the
   game folder, and patch the exe so it loads Aurie on start.
3. Press **Launch Modded Game**. HS Offline Launcher is built into this button:
   it uses your ForgePact game path, starts Steam if necessary and sets the Steam
   environment before launching. You do not need to install or open a second
   launcher. Play with offline characters; saved modifiers apply through the plugin.

The Setup page keeps the launch result visible, including missing Steam/runtime
files or protection that is still active. It checks for an existing game and
inactive EAC before launch and after waiting for Steam. It never stops EAC or
falls back to launching without those checks. After a successful request, one
startup check reports whether the game process is still running; this is not a
confirmation that every gameplay modifier has applied.

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
mods/aurie/YYToolkit.dll         YYToolkit        (AGPL-3.0, modified — see yytoolkit-modified/)
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
- `yytoolkit-modified/` — a pointer at the modified YYToolkit's real source: the
  patch series in the toolkit hub's `third_party/yytoolkit/`, at the commit the
  shipped DLL was built from. Not the source itself, and not where a change to
  YYToolkit is made.
- `modfiles_shipped/` — the binaries copied into the game folder.
- `plugin_build/build.bat` — builds the plugin. `build.bat release` produces the shipping
  build (features only); `build.bat dev` produces the development build, which additionally
  carries the diagnostic commands used to investigate the game. The literal `dev` argument
  is required: `dev` is the only special-cased value, so a bare `build.bat` with no
  argument produces the *shipping* build, not the development one.
- `build_release.py` — packages `dist/ForgePact/` (the release zip contents).
- `tools/` — developer helpers, not shipped to players: `ipc.ps1` sends one command to
  the running plugin and prints only its reply, and `ghidra/ImportSymbols.java` names the
  stripped game binary in Ghidra from the game's own script table.
- `docs/S10-special-content-notes.md` — the Season 10 reverse-engineering log, in our own
  words: object, script and variable names with their indices, the special-content gates
  and what opens each, measured values and crash thresholds, our own commands and hooks,
  and every approach that did not work (written in Turkish).
- `docs/dungeon-key-research.md` — how the two-stage key/relic drop system was found:
  the outer `LoadDrops` chance gate, the per-item `droprate.base` roll, and why keys
  outside their home zone can never drop without opening the outer gate (Turkish).

### Building the plugin

`plugin_build/build.bat` compiles `plugin/ModuleMain.cpp` against three header-only
dependencies:

- **Aurie Framework** headers (`Aurie/shared.hpp`) — expected in
  `plugin_build/include/`, which is not tracked by this repository (the headers are
  upstream's, not ours). Run `py tools/fetch_toolchain.py` before the first build: it
  downloads this and every other pinned header/binary from its upstream commit or
  release and verifies its SHA-256 before writing anything, all-or-nothing (see
  `tools/toolchain-pins.json`). This is also what CI runs (`forgepact-release.yml`).
- **YYToolkit** shared headers (`YYToolkit/YYTK_Shared.hpp`, plus
  `YYTK_Shared_Types.cpp`, which `build.bat` compiles alongside `ModuleMain.cpp`) —
  same place, same tool, same reason.
- **hs-game-sdk** (`hs_game_sdk/hs_game_sdk.hpp`) — the typed Hero Siege object/player/room
  wrappers `ModuleMain.cpp` uses. This one is not yet a submodule of this repository; it
  lives in the
  [hero-siege-offline-toolkit](https://github.com/falorfrozen-cmd/hero-siege-offline-toolkit)
  super-repo (see `hs-game-sdk/` there) on its default branch. Until it is published as its
  own pinned dependency, building ForgePact standalone means checking that repo out
  alongside this one and pointing `build.bat` at `hs-game-sdk/cpp/include`. Building from
  inside a full toolkit checkout (where ForgePact is already a submodule next to
  `hs-game-sdk/`) needs no extra setup.

Without `hs-game-sdk/cpp/include` on the include path, compilation fails immediately at
the `#include <hs_game_sdk/hs_game_sdk.hpp>` line (`fatal error C1083`). The Python test
suite (`py -m unittest discover -s tests`) checks the plugin's *source* against its
documented contracts and does not compile it, so a green test run does not confirm the
plugin actually builds.

### Packaging the panel

`src/offline_launcher.py` is a normal Python import bundled into ForgePact.exe.
Keep it beside `src/forgepact.py` when running from source. Its launch engine is
included in this repository; neither source use nor release packaging needs an
HS-Offline-Launcher installation or checkout. The build refuses a missing module.
Its MIT license is included in `CREDITS.md`, which ships in the release package.

`build_release.py` needs hs-game-sdk too, for a different reason and from a different
path: `src/forgepact.py` imports `hs_game_sdk` for the Satanic Zone buff/debuff pool, so
the packager puts `hs-game-sdk/python` on PyInstaller's analysis path.

This is a hard requirement, and the script fails rather than warns — twice, once before
the build if the directory is missing and once after it if PyInstaller still reports the
module as missing. The panel's own import falls back to empty pools, and its runtime
`sys.path` fallback cannot rescue a frozen build (PyInstaller resolves imports when it
builds; the exe unpacks to a temp directory with no toolkit checkout above it). A package
built without the SDK therefore builds, starts, and looks completely normal — except the
World tab's Satanic Zone section has no rows under its heading. That shipped in every
release up to 1.3.18.

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

See [CREDITS.md](CREDITS.md) for the full notices, and `yytoolkit-modified/NOTICE.md`
for where the modified YYToolkit's complete corresponding source is.

ForgePact is an independent, fan-made project and is **not affiliated with or
endorsed by** AurieFramework, Panic Art Studios, or Hero Siege.

## Miner's Helmet

A high-defense signature helmet (Great Helm base, SS tier: +1000 Defense, +500%
Enhanced Defense, +20% Movement Speed, +20% All Resistances, +5 Light Radius)
with two mining mechanics that ForgePact runs while it is worn:

- **4× ore.** Every mining node gives exactly four times its ore. The helmet
  replaces the Mining Ore Amount slider rather than stacking with it; take it
  off and the slider applies again. By design it rewrites only ore amounts, so
  mining time and XP are left to the game (not measured separately).
- **Vein Resonance.** Finishing a dig also digs the two nearest veins within
  192 units of that node that you could mine yourself, through the game's own
  dig, with 4× ore each. Used-up veins, veins being dug and
  veins above your mining level are skipped, and a vein dug this way never
  starts another. `minerhelm veins 0|1` turns it off and on.

Forge it with the Item Editor (Item Forge → Forge a signature item → Miner's
Helmet). Mods → Items shows whether it is worn and how many veins Vein
Resonance has dug. `minerhelm status` prints the equipment read and the last
reward decision; `minerhelm probe [seconds]` logs the nearest node's dig state
for troubleshooting. Both mechanics were verified in play on 2026-09-23. The
golden pulse drawn at a finished dig is cosmetic and not yet confirmed on
screen. An earlier crash report against an experimental build (2026-09-21) was
not reproduced in those sessions; its cause was never identified. Design,
evidence and tests: [docs/miner-helmet-prototype.md](docs/miner-helmet-prototype.md).


## AFK FARM independent reward compatibility (local, 2026-09-22)

AFK FARM 0.5.0 owns its MF, XP, Gold and loot settings. During its short native
reward scope, ForgePact passes through reward stats, drop-repeat hooks, extra
LoadDrops gates and the relic filter. Outside that scope its normal settings
remain active. Combat/density modifiers are unchanged. Neither plugin rewrites
ForgePact's configuration. ForgePact is not required to use AFK FARM.

The shared `hs_game_sdk/reward_scope.hpp` publishes compatibility and original
repository denominators through a process-local named mapping; no cross-plugin
symbol calls are used. Both DLLs must be rebuilt against that header. Older
ForgePact DLLs have no isolation protocol and AFK refuses independent rewards
with an update message, rather than silently stacking multipliers.

