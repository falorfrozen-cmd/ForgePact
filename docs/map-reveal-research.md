# Map reveal — research log

Status (2026-09-11): **SOLVED AND SHIPPED.** Monsters now appear on the
revealed map, and the reason they never did turns out to have nothing to do
with visibility at all — see §10, which supersedes the 2026-09-10 conclusions
in §5 and §7. Short version: **most mob packs do not exist yet.**
`Enemy_Creator_*` spawners only give birth once the player comes within
~1050 px, so an unexplored zone is mostly empty of monsters and no draw flag
can reveal something that was never created. Telling the spawners the player
is adjacent — the Beacon's already-proven `beaconspawn` trick — populates the
zone: measured **148 → 866** enemies in one fresh zone, and **208 → 1273** in
another.

Shipped as a bounded one-shot per zone inside `MapRevealManager`, with its own
panel sub-toggle (`map_reveal_packs`) because it is the only half that costs
frame time.

### Correction to the 2026-09-10 session, worth reading before §5/§7/§8

Two of that session's conclusions were wrong, and both failed the same way —
**a measurement that was really measuring the instrument**:

- §5/§7 concluded enemies were "confirmed fully blocked, no lever at all."
  That was true of every *visibility* lever tried, and irrelevant: the
  question was never visibility. Nothing in that session counted how many
  enemies the zone was *supposed* to have, so "only the ones near me show up"
  was read as a draw gate when it was an empty map.
- §8 concluded `StatLightRadius` was "dead code" because a `HookOneScript`
  detour measured 0 calls. The pet-quest investigation independently proved
  that exact inference invalid on this YYC build — script-table hooks can read
  `native=3840, table=0` while the game calls the function thousands of times
  (`ForgePact/docs/pet-quest-collector-c-research.md`). **A 0 from
  `HookOneScript` on this build means nothing on its own.** `StatLightRadius`
  may well be live; it was simply never the relevant question, and the hook
  added for it was reverted.

The general lesson, now twice-learned: prefer a measurement whose *denominator*
you know (here: `instance_number` before vs after, plus `creatorprobe`'s
spawner census) over asking a human whether something looks different.

Live session, research build (`BloodPactPlugin_rel.dll`, dev build) against a
running `Hero_Siege.exe`, driven interactively through `bp_ipc\cmd.txt` /
`bp_ipc\out.txt` with the player confirming what actually rendered on screen
at each step (this session had no way to see the game window directly, only
the IPC text channel).

**Headline result: the plan's core premise doesn't hold.** The existing
`MapRevealManager` (`ds_grid_clear` on `objMinimap.minimapDiscoveredGrid`) —
already shipped — turns out to already reveal every icon category tested
except enemies/loot. `isDiscovered` does **not** gate the minimap icon for
any object family tested. The feature is not "the toggle is broken and needs
a discovery sweep"; it is "the toggle already works, its description just
undersold it, and enemies are the one thing it genuinely can't reach."

---

## 1. `minimapRevealed` is not a master flag

Baseline read: `oget objMinimap minimapRevealed` → `162816`, while
`minimapCellsX * minimapCellsY` was `627,759` for that zone. Looked plausibly
like a discovered-cell count.

Disproved: after `oset objMinimap minimapRevealed 0`, the value stayed `0`
(no automatic recompute within several seconds), and — critically — the
player reported the minimap was **fully fogged** at that moment even though
the *original* value (`162816`) would imply ~26% explored. The two are
uncorrelated. Then, after `reveal 1` fully cleared the fog (player-confirmed),
`minimapRevealed` stayed `0`. It does not track fog state in either
direction.

**Conclusion:** `minimapRevealed` is unrelated to the minimap's visual
fog/icon state. Not used anywhere in the shipped design.

## 2. `isDiscovered` does not gate mechanic icons — fog does

First pass (confounded): set `Chaos_Pillar_obj.isDiscovered = 1` on a pillar
~3681 units from the player, fog already fully cleared from an earlier test.
Player reported the icon appeared. Looked like confirmation, but fog being
already clear made this untestable — could have been fog alone.

Clean pass (fresh zone, fog untouched):
- `Mining_Node_obj.isDiscovered = 1` on a node ~9938 units away, **fog still
  present** → player: icon still hidden. `isDiscovered` alone did nothing.
- Reset `isDiscovered = 0` on that same node, then `reveal 1` (fog-clear
  only, no `isDiscovered` write) → player: "mining nodes showed up along
  with waypoint, chests and shrines."

**Conclusion:** for mechanics, waypoints, chests, and shrines, the minimap
icon is gated **only** by the fog grid cell at that world position. The
`isDiscovered` / `discoveryRange` family (confirmed present on
`Chaos_Pillar_obj` and `Mining_Node_obj`, both defaulting `false` /
`500`) governs something else — not measured further, out of scope (likely
interaction/tooltip/spawn-adjacent logic, not the map).

`Dungeon_Entrance_obj` has **no** `isDiscovered` var at all (confirmed via a
94→38-var full dump), and its icon still appeared purely from fog-clear —
consistent with the same fog-only rule.

**This means the existing, already-shipped `MapRevealManager` already
reveals every mechanic/waypoint/chest/shrine icon in the zone the moment its
fog-clear runs.** No per-object write is needed for this category — decision
1's "mechanics/interactables, waypoints and portals" coverage is already
complete for icon visibility, and has been all along.

## 3. Waypoint icon reveal does not unlock the waypoint

`Portal_Waypoint_obj.waypointActive` stayed `false` after the fog-clear
revealed its icon. Decision 2 ("waypoints are icon-only, `UnlockWaypoint` is
never called") is satisfied by the existing code with zero additional
logic — `ds_grid_clear` has no way to touch `waypointActive` and measurement
confirms it doesn't.

## 4. Quest objects — not independently confirmed this session

No `Quest_Object_Parent_obj` instance existed in either zone visited this
session (`instance_number(3979)` = 0 both times). The player did see a quest
marker clear "close enough to get cleared without having fog disabled" in
the second zone, i.e. via ordinary proximity, before any test — not usable
as evidence either way.

Given every other icon-drawing object measured (mechanics, waypoints,
dungeon entrance, chests, shrines) follows the same fog-only rule and shares
`objMinimap`'s single draw pipeline, it's a reasonable inference that quest
objects follow it too — but this is **inferred, not measured**. Flagged as
the one open gap; a follow-up session with an active, distant, unaccepted
quest item in view should confirm with `reveal 1` alone (no per-instance
write) before this is stated as fact anywhere user-facing.

## 5. Enemies/loot — confirmed fully blocked, more so than expected

`gnames minimapShow` confirmed `minimapShowMonsters` and
`minimapShowEnvironment` are globals (not `objMinimap` instance vars), and
both already read `1` (on) for this session/player.

`inames Enemy_Parent_obj discover` / `minimap` / `miniMap` — **zero matches**
across all 312 instance variables on a live enemy. There is no per-instance
discovery or minimap-draw flag on enemies at all, unlike mechanics.

Live confirmation: with the whole zone's fog cleared (`reveal 1`) and both
options already on, and 63 live `Enemy_Parent_obj` instances in the zone
(`instance_number(1429) = 63`), the player reported monster dots visible
**only next to them** — none of the other ~62 enemies scattered around the
cleared zone showed on the minimap.

**Conclusion:** enemy/loot dots are drawn from a hardcoded proximity/on-screen
check with no exposed per-instance override — not merely gated by the two
global options (which were already on and made no difference to the distant
enemies). Per decision 3, the two globals are never written by this mod
regardless. There is nothing left to lift here. This is a harder wall than
the plan anticipated ("if enemies carry their own `miniMapDraw`... we set
it" — they don't carry one at all).

## 7. Enemy activation experiments — every lever tried, all dead ends

Went deeper than §5 at the user's request, chasing the specific hypothesis
that a per-instance field or GameMaker's own activation system gates the
dot, not just the two global options.

- **`distancePlayer`/`myGridCellX`/`myGridCellY` are not spatial data in any
  usable sense.** A dormant enemy reads `distancePlayer = 100000` (sentinel)
  and `myGridCellX/Y = -1`. Reverse-engineered the grid formula from enemies
  that *do* have valid cells (`floor(x/16)`, `floor(y/16)`, confirmed against
  five live samples) — a spatial-hash concern, unrelated to the minimap.
  Writing the *true* distance (3479, computed from real positions, not a
  lie) into `distancePlayer` on a dormant instance produced no visible
  change and no enemy reaction. Worse: a later check showed a previously-set
  `500` had drifted to `15` over ~1.5s with **zero** movement by either
  party — `distancePlayer` behaves like a decrementing counter/timer, not a
  literal spatial distance. The field's semantics don't match its name;
  abandoned as a lever entirely.
- **`instance_activate_object(Enemy_Parent_obj)`** (GameMaker's own
  "reactivate deactivated instances" builtin) produced no measurable change
  in two separate zones (63 enemies, then a reloaded zone with 161), checked
  both by player observation and by re-reading `distancePlayer`/
  `myGridCellX` on a specific dormant instance immediately after the call
  (same command batch, minimal frame gap) — still sentinel values
  afterward. Either these instances were never truly GM-deactivated (more
  likely, given the field's now-disproven "spatial distance" theory), or
  something re-deactivates them faster than one activation call can
  overcome. No corroborating evidence either way; not pursued further.
- **Risk note for future sessions:** this zone's enemies all read
  `bossActive: true` at ~175k HP (`Arachnid`, `Broodmother`, `Undying`,
  `Skeletal Legion`) — a side effect of this session's Headhunter/Tyrant
  mods already being armed, not vanilla difficulty. `instance_activate_object`
  is a real engine-level wake call (not a cosmetic flag) and was tested
  against this boss-tier population at the user's explicit approval, twice,
  with no observed reaction either time. A future session should not assume
  that outcome generalizes to a real (unmodified) high-density pack without
  re-checking.

**Conclusion:** no per-instance field, and no engine-level activation call
tried, changed the enemy dot's visibility. Whatever draws it is either a
hardcoded proximity/on-screen check with no exposed override, or reads state
this session never found. Confirms and deepens §5.

## 8. `StatLightRadius` — a real, hookable, but dead script

The user's working theory going in: light radius is a player *stat* (visible
on the character sheet, distinct from the `light`/`lightEnabled` instance
vars checked in §7, which were `undefined` in the outdoor zones visited and
are a different, darkness-rendering-only mechanic per the object hierarchy).
`ForgePact::StatsManager` (`plugin/include/ForgePact/StatsManager.hpp`)
already hooks fourteen `Stat<Name>` scripts exactly this way (`StatMagicFind`,
`StatMovementSpeed`, etc.) — the natural place to add a fifteenth.

- `routineptr StatLightRadius` resolved (`gml_Script_StatLightRadius` exists
  at a real address) — a genuine script, not a guess. Added a fifteenth
  `FP_STAT_HOOK(StatLightRadius)` entry, built the dev DLL, reloaded into the
  running game, and set `stat lightradius 50`. Hook installed successfully
  (`stat list` showed `kanca kurulu`).
- First test: char sheet read `Light Radius: 0` for the loaded character. A
  0 base times any multiplier is still 0 — same zero-base problem this file
  already solved for `StatFasterCastRate` with an additive hook instead
  (never got that far here; the more basic problem below blocked it first).
- **The real finding: `cagri` (call count) stayed `0`** through opening the
  character sheet, and stayed `0` even after switching to a *different*
  character whose sheet reports `Light Radius: 10` (a genuinely non-zero
  base) — an action that forces a full stat recalculation for every other
  displayed stat. `StatLightRadius` was never invoked either time.
  `routineptr` on eight plausible alternate names (`StatLightRadiusFinal`,
  `GetLightRadius`, `PlayerLightRadius`, `LightRadius`,
  `StatLightRadiusTotal`, `CalculateLightRadius`, `UpdateLightRadius`,
  `StatLight`) all returned "not found."
- **Conclusion: `StatLightRadius` is real but vestigial/dead code in this
  build** — the character sheet's displayed value, and presumably whatever
  drives the actual rendered light circle, is computed somewhere else
  (most likely inlined in a larger stat-recalculation routine rather than
  its own named script). Finding the real getter needs decompilation work
  (Ghidra, same approach as `pet-quest-collector-research.md`'s "Native
  decompilation" section) — a separate, bigger effort than a hook-and-test
  cycle. Not pursued further this session per user's call.
- **Reverted.** The `StatLightRadius` entry was removed from
  `StatsManager.hpp` (`git checkout` back to the tracked version — diff is
  clean), the dev DLL was rebuilt from that reverted source, and the
  rebuilt DLL replaced the one running in the game. `stat list` should show
  fourteen entries again, matching pre-session state.
- Whether light radius (once a real getter is found) has *any* connection to
  minimap or enemy-dot visibility is itself still unconfirmed — the user's
  hypothesis was never actually tested, only blocked on finding the right
  hook point. Worth re-testing once the real script is identified.

## 9. Fog robustness (`minimapCellsX`/`minimapCellsY` change across zones)

Not a live resize test (would need the player to change resolution/window
mode mid-session, not done here), but confirmed indirectly: the two zones
visited had different grid dimensions (`1119×561` vs `1175×557`), i.e. the
grid is reallocated per zone already. The existing identity key
(instance + grid id + room) already changes on a zone transition since the
grid `ds_grid` id or room changes too, in every case observed. A live
in-place resize (e.g. alt-tab / resolution change without a zone change)
was not tested. Left as a defensive hardening addition (add cell dims to the
identity key) rather than a confirmed-bug fix.

---

## 10. The actual answer (2026-09-11): the packs are not there to reveal

Re-opened with the SDK's script table and the Ghidra project available, and
with `ForgePact/tools/ipc.ps1` making live measurement cheap.

### The census that settled it

`creatorprobe` (research command, counts each `Enemy_Creator_*` object before
and after `instance_activate_object`) in a zone the player had just entered:

| | |
| --- | --- |
| `Enemy_Creator_obj` | **271 awake / 271 total** |
| `Enemy_Creator_Ambush_obj` | 13 / 13 |
| `Enemy_Creator_Ancient_obj` | 19 / 19 |
| `Enemy_Creator_Legion_obj` | 7 / 7 |
| live `Enemy_Parent_obj` | **208** |

Two things fell out immediately:

1. **Nothing was frozen.** Every spawner was already awake, and
   `instance_number` (which counts only *active* instances) returned 208
   enemies including ones 7,300 px away. The 2026-09-10 theory that far
   monsters were deactivated was wrong — they were awake the whole time.
2. **310 spawners had not given birth.** ForgePact's own Beacon notes already
   documented why (`ModuleMain.cpp`, the `beaconspawn` comment): the creator's
   periodic check calls `distance_to_object(Player_obj)` and spawns its pack
   below ~1050 px. So the map was not hiding monsters; it did not have any yet.

### The fix, measured

Turning on the existing `beaconspawn` lie (creators are told the player is at
distance 0) with `beaconwake all`:

> **208 → 1273 live enemies in about five seconds**, `chasing=0` across every
> rarity (aggro was pinned to ~vanilla with `beaconrange 300`, so this stayed
> a population change, not a hunt).

Player confirmation, in order: *"all mobs seem to be visible on the map now"*
— then, with the Beacon switched fully off again, *"yes still visible, even on
reentering the zone they are still visible."* That last part is what makes the
shipped version cheap: **the spawn is one-shot and persists**, so nothing has
to be re-applied per frame.

### Dead ends closed on the way (so they are not re-tried)

- `visible` **is not the minimap gate.** Far enemies read `visible=0`, near
  ones `visible=1`, so it looked promising — but writes to it revert within a
  second, and gating the game's own prop pass to 1-in-60 frames
  (`beaconwake every 60`) did not stop the revert, so `ActivateDeactivateProps`
  is not the writer either. Moot regardless, once §10 showed the dots were
  missing because the monsters were.
- `inviewCheck` is **not** the culling flag: it reads 0 on a *visible* enemy
  as well as an invisible one, and a write to it persists while `visible`
  ignores it.
- `distancePlayer` is **not** a distance. Written to 500 on a stationary enemy
  with a stationary player, it read 15 a second later — it decays. Its 100000
  "sentinel" and `myGridCellX/Y = -1` are not proof of anything being asleep.
- `instance_activate_object` on the enemy family changed nothing measurable,
  in either of two zones — consistent with nothing having been deactivated.

### Decompiling `DrawMinimapDynamic` — started, then unnecessary

`gml_Script_DrawMinimapDynamic` (RVA `0x16F5820`, from `hs-game-sdk`'s
`scripts.json` + the project's `symbols.csv`) was decompiled locally to find
the dot's gate, along with `s_MinimapPoint`, `PlayerUpdateMinimap` and
`MinimapRefresh`. The live census answered the question first, so the read was
never finished and **no conclusion here rests on decompiled output**. The
local script (`ghidra_scripts/DecompileMinimap.java`, uncommitted) is a
starting point if the *loot*-dot half is ever picked up. Per `agents.md`, no
decompiled text is reproduced in this repo.

## What shipped

`MapRevealManager` gained a second half, deliberately shaped to stay cheap:

- On each new zone identity (the same check that already triggers the fog
  clear) it *arms* the pack pass, then opens a **900-frame window** only once
  the zone's creators report ready (see the regression section below) and
  counts it down per frame.
- While open, `Hook_distance_to_object` answers 0 for creator instances only.
  The window is in *frames* on purpose: the creators' own poll timer
  (`enemyCreatorTimer`, observed ~116) is frame-based too, so 900 frames is
  ~7 poll cycles at any frame rate.
- Outside the window the builtin costs one relaxed atomic load.
- The plugin never creates a monster itself — the game's own creator logic
  runs, so pack composition, density and rarity stay vanilla, and other
  ForgePact mods apply to them unchanged.
- Map-reveal shares the Beacon's builtin detour but **not** its hunt hooks
  (`InstallDistanceLieHook` vs `InstallBeaconHook`): revealing a map must not
  change how monsters behave.

### The regression this nearly shipped with, and the fix

The first build opened the window straight from the zone-identity change.
That change fires **while the new room is still loading**, and the result was
worse than doing nothing:

| | healthy zone | zone entered with the first build on |
| --- | --- | --- |
| `enemyCreatorTimer` on a live creator | `real:116` | **`undefined`** |
| packs spawned by the pass | 718 | 0 |
| packs spawned later, **by walking onto them** | (n/a) | **0** |

That last row is the important one: the spawners were not merely un-triggered,
they were **spent**. Answering `distance_to_object` with 0 before a creator has
finished initialising makes it take its spawn branch once, early, and come out
inert — so the mod left those zones *emptier than vanilla*, permanently.
Confirmed by A/B: the Beacon's own continuous `beaconspawn` produced nothing in
those zones either (`creatorLies` frozen), so it was the creators that were
broken, not the window.

Credit where due — the user called the shape of this before the data did:
*"looks like if its done too early it bugs out the spawners."*

The fix does not guess a delay. Readiness is **asked about**: the window only
opens once a live `Enemy_Creator_obj` reports a real `enemyCreatorTimer`, with
`Player_obj` present. A zone whose creators never become ready never gets
lied to, which is the right failure direction — vanilla behaviour, not damage.
A zone with no creators at all (town: 0 creators, 8 enemies) is dropped
immediately rather than polled.

### Measured on the fixed build

Entering a fresh zone with the mod already on, no commands sent:

| | |
| --- | --- |
| `zonesPopulated` | 1 (fired by itself on arrival) |
| `creatorLies` | 208 |
| enemies | **816** (broken zones sat at ~100) |
| spawn window | already closed by the time it was read |
| frame time | **7.80 ms avg (~128 fps)**, plugin 2.31% of frame time |

The earlier pre-fix build, when it did work (mod switched on while already
standing in a settled zone), measured `creatorLies=188`, enemies **148 → 866**,
8.09 ms average — i.e. the fix costs nothing and only changes *when* the
window opens.

Commands: `reveal 1|0`, `reveal packs 1|0`, and `reveal stat` (research build)
which reports both flags, zones populated, window frames left, creator lies,
and a live creator/enemy census for the current zone.

### Two more ways into the same damage, found by review (2026-09-12)

Origin's review of PR #2 pointed out that the readiness gate above only
protects the moment the window *opens*. Two paths reached the same
lie-to-an-uninitialised-creator state without ever going through it. Neither
was reproduced as an empty zone in live gameplay — they were found by
compiling the class against controlled API responses — but both are the exact
mechanism proven destructive above, so they are treated as real.

**1. The window outlived its zone.** Neither `ResetIdentity()` nor the
new-zone branch cleared an open `m_SpawnWindow`. Walking to another zone
mid-window left `WantsPackSpawn()` true while the next zone was still loading:

```text
ready_zone            window=900
loading_no_minimap    window=899 wants=1
unready_new_zone      window=898 wants=1 pending=1
```

`ResetIdentity()` is what `Tick()` calls when the minimap object, its grid or
the `ds_grid` behind it is missing — i.e. precisely during a room load — so
"we cannot see the map any more" now means "the window is void", via a single
`CloseSpawnWindow()` that every such path routes through. The new-zone branch
closes the old window before arming the next pass.

**2. The 20-frame throttle left a hole.** The identity work runs one tick in
20, so even with the fix above a transition could hand up to 20 frames of open
window to the next zone. The first attempt at this checked the room every
frame in `OnFrame` instead — **which was still wrong, and is the more useful
half of this section.**

### Why a frame-boundary check cannot work here (2026-09-12, second round)

`OnFrame` runs at `EVENT_FRAME`, which this bundled YYToolkit dispatches from
`HkPresent` — the **end** of the frame. The creators consume the permission in
their **step** events, earlier in the same frame. So on the first frame in a
new zone the distance hook still saw the previous zone's open window, and the
window only closed afterwards. Too late for exactly the call that does the
damage. Origin's review reproduced it against the production class:

```text
ready_zone                window=900 wants=1 creator_ready=1 distance=0
new_room_before_present   window=900 wants=1 creator_ready=0 distance=0   <- the damage
new_room_after_present    window=0   wants=0 creator_ready=0 distance=2500
```

No amount of extra identity tracking at Present fixes that ordering. A
render-time check cannot make a guarantee about a step-time consumer.

**The authorization moved to the consumer, and to the thing that actually
matters.** The damage mechanism is specific: answering 0 to a creator that has
not finished initialising makes it take its spawn branch once, early, and come
out inert. That is a property of *the creator in hand* — not of the zone, the
room key, or the map identity. So `Hook_distance_to_object` asks the creator,
at the moment it would change the result: a real `enemyCreatorTimer` means
initialised, and lying to it is safe however stale the window is.

This also makes the window's staleness a performance question rather than a
correctness one, and it means the pass keeps working across a transition
instead of failing closed: a *ready* creator in a newly-entered zone is still
served.

The same guard applies to the Beacon's lie. The spawner comes out inert
whichever feature answered, so the invariant belongs to the hook, not to one
caller; the Beacon's wake radius and continuous behaviour are otherwise
unchanged.

The per-frame identity check stays as defence in depth — it stops a window
lingering past its zone — but it is no longer load-bearing. It now compares
the **full** identity (room + minimap instance + grid), because a replaced or
missing minimap with an unchanged room key is a zone change too, which was the
second gap reported.

**And a window is never opened against an identity that could not be read.**
The first version stored `INT64_MIN` as its "unreadable" sentinel and opened
anyway, so a later failed read compared *equal* to it — "unreadable closes the
window" only held if the window had been opened with a valid key. `ReadIdentity`
now fails as a unit, and both callers treat failure as invalid.

### `reveal packs` on did not apply to the zone you were standing in

Same review, lower severity. `SetPacks(true)` set the flag and nothing else;
`Tick()` returns early while the zone identity is unchanged, so the current
zone was never armed and the checkbox did nothing until the next zone change
or a `reveal` off/on cycle:

```text
enable_packs_current_zone   window=0 pending=0
```

It now arms the current zone on an off→on edge (and only while `reveal`
itself is on). Deliberately `m_PacksPending = true` rather than opening the
window directly — the readiness gate is the whole reason the pass is safe, and
short-circuiting it here would have reintroduced the original bug by a third
route.

### The tests these produced

`tests/test_map_reveal_behavior.py` compiles the **real** `MapRevealManager`
and the **real** `Hook_distance_to_object` against controlled game-API
responses and calls the hook at the point in the frame order where it matters
— before the next `OnFrame`. Source-string assertions passed throughout the
period when the ordering was wrong, which is precisely the failure mode
`agents.md` warns about; they are kept for structure, but they are not the
regression net.

Each scenario was verified to fail against the code it describes before being
relied on. Reverting the authorization to the countdown alone reproduces the
reported result exactly:

```text
FAIL new_room_before_present/unready_creator got=0 want=2500
FAIL same_room_new_map/unready_before_present got=0 want=2500
FAIL map_lost/unready_before_present got=0 want=2500
```

One of those controls also caught a mistake in the *first* control: removing
the hook's standalone readiness guard changed nothing, because `MayPopulate`
checks readiness too. The guard is load-bearing only for the Beacon path, so
that path got its own scenario rather than leaving a line no test could fail
without.

## What this changes about the original plan

The original design (a per-object "discovery sweep" writing `isDiscovered`
across mechanic/waypoint/quest families) is **not needed** — the existing
fog-clear already achieves full coverage for those categories, confirmed
live (§1-3). Building the sweep anyway would add a budgeted `instance_number`
walk, new state, and new tests for a mechanism that does nothing measurable.
The toggle's own description is the only thing that undersold it; no plugin
code change was warranted for that part of the plan.

The real, confirmed gap is exactly the "honest limitation" section the plan
already called out for enemies/loot — and §5/§7 show it's unconditional (no
lever at all, not just the two options the plan anticipated). §8 shows the
one promising player-stat angle (`StatLightRadius`) is dead code in this
build, not a usable lever either, at least not yet.

Session ended here at the user's request ("we have achieved nothing and
reveal map mod behaves like it did before") with no plugin or panel changes
kept. A future session picking this up should:

1. Start from §8: find the real light-radius (and, ideally, general
   minimap/enemy visibility) getter via decompilation before trying another
   live hook-and-test cycle — the cheap options are exhausted.
2. If that turns up nothing connecting light radius to enemy dots either,
   the honest conclusion is that this toggle's description should simply be
   corrected to describe what `MapRevealManager` actually already does
   (mechanics/waypoints/quest markers, not enemies/loot), with no further
   code.
