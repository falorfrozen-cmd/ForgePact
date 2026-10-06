# Map reveal — research log

Current shape (2026-09-22): the monster half of the map is now **pack
markers**, not a spawn pass. `reveal packs` / the panel's nested checkbox draw
one icon per `Enemy_Creator_*` spawner that has not given birth, inside the
game's own minimap layer (`ForgePact::PackMarkers`, hooking
`DrawMinimapDynamic` for the monster family and reusing its placement
arguments). The pass that really spawns every pack on arrival still exists as
`reveal spawn` / `map_reveal_spawn`, off by default. Why: a zone's worth of
living monsters is what the game cannot afford per frame at high density,
however they were born - see
[population-performance-analysis.md](population-performance-analysis.md). The
sections below record how the spawn pass was built and are kept as history.

Current local candidate (2026-09-21): the later storage-exhaustion investigation
and paced real-population candidate are in
[population-capacity.md](population-capacity.md). The first live run drained its
448 admissions, but the player reported a 15-20 second wait. The local follow-up
replaced the two-per-frame FIFO with a ready-caller guard. A later snapshot
used overflow storage without reported errors, but entry still hitched. The
next adaptive candidate took at least 56 seconds with 216 queued groups in a
live snapshot. The current local adjustment uses a five-second throughput target
across density copies, measured native construction and early admission, without
the minimum-frame-time feedback that starved the earlier queue. Live timing and
smoothness acceptance are pending;
the historical conclusions below describe earlier experiments.

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
The original fix dropped a zone with no creators (town: 0 creators, 8 enemies)
immediately. The local 2026-09-22 follow-up corrects that inference: a minimap
can appear before its creators, so an empty observation now keeps the bounded
pending poll alive. A rotating 32-candidate probe finds ready siblings without
letting one unready first creator block them. Per-creator readiness is still
checked at admission. See `population-capacity.md` for the regression evidence;
the historical live measurements below describe the earlier build.

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

## 11. Pack marker icons replaced a few seconds after arrival (issue #181)

### The symptom

Reported by the owner from play, and reproduced under instrumentation in
Live 1 below: with Map Reveal and its pack markers on, a new area first shows
one icon per unspawned pack by kind (ambush, ancient, champion, colossal
chest, legion, miniboss, normal; Live 1 saw ancient and miniboss icons go,
the other special kinds were not in that zone), and a few seconds later the
special-kind icons give way to generic ones while the player has not gone
near the packs. The marker list
gives a marker up when its spawner has not shown a numeric
`enemyCreatorTimer` within 600 frames of being listed, which is 5 s at 120 fps
and 10 s at 60 fps, about the delay described.

### The candidates, and what decides each

Five explanations fit the report. The instrument below was built so one live
session tells them apart; as written here, before that session, none was
measured. What Live 1 measured is under `### Live 1 results`.

- **H1, unarmed give-up (the leading candidate before Live 1).** If the six
  special spawner kinds never show a numeric `enemyCreatorTimer`, every
  special marker is dropped 600 frames after listing. A mixed cluster then
  shows the normal skull (the rarest kind wins only among the markers left),
  and a spot holding only special packs loses its icon. The support before
  Live 1 was static only: the SDK's script table has a Create-event closure
  for `Enemy_Creator_obj` and none for the other six creator objects, and
  `enemyCreatorTimer` had only been measured on `Enemy_Creator_obj` (the
  hub's `docs/RUNTIME_DATA_MODELS.md` § 11.2). Live 1 then read it absent on
  the ancient, miniboss, ambush and colossal chest spawners it sampled.
  Decided by `givenup=` per kind in `packmarks why`, with `age=` near 600.
- **H2, a false birth.** A spawner creates enemy-family objects that are not
  its pack (idle `*_Passive_obj` monsters come with the spawner,
  `docs/population-performance-analysis.md` § 8), so the create hooks report a
  birth, the marker is retired and the game's own dots show in its place.
  Decided by `attributed=` and the `creates:` names in `packmarks why`, read
  against `packmarks census` still showing the creator's `enemyArray` as not an
  array.
- **H3, a real early birth.** The special packs really are born on arrival,
  and the marker hands over to the game's dots by design (the README's Full
  Map Reveal row: "its real dots replace the marker"). Decided by the census
  showing `enemyArray` as an array 30 s after arrival without the player
  going near.
- **H4, the icons are lost.** The icon sprites stop drawing and the
  dots-by-kind fallback shows. Decided by `loaded=7/7` and a growing
  `iconDraws=` in `packmarks stat` at arrival and 30 s later.
- **H5, the varied icons were the game's own.** What shows at arrival is the
  game's own per-type minimap drawing, and our markers then draw over it.
  Decided by `enumerations=` and `iconDraws=` at arrival and a screenshot with
  the markers off.

Ruled out statically: one kind's enumeration counting another kind's
spawners. The SDK's parent table lists all seven creator objects as root
objects, so `instance_number` and `instance_find` on one never return another's
instances.

### The instrument

Research build (`build.bat dev`) only, except the `kinds=` field. Counts are
per zone, reset when the zone generation changes.

- `packmarks stat`, both builds, gains
  `kinds=normal:<n>,ambush:<n>,ancient:<n>,champion:<n>,colossal_chest:<n>,legion:<n>,miniboss:<n>`,
  the markers held now per kind; the research build adds `retire=timer` or
  `retire=state`.
- `packmarks why` prints one line per kind,
  `packmarks why <kind>: listed=<n> marked=<n> spawned=<n> destroyed=<n> timergone=<n> givenup=<n> stateborn=<n> attributed=<n> age=<min>..<max> frame=<f>`:
  the retirements by rule, every create the hooks attributed to a spawner of
  that kind whether or not it retired the marker, and the frames between
  listing and retirement (`-1..-1` when none). Then, per kind,
  `packmarks why <kind> creates: <ObjectName>=<n> ...`, the four objects
  created most often, named by the runtime's `object_get_name`, or `none`.
- `packmarks census` walks every spawner of each kind once (never per frame)
  and prints
  `packmarks census <kind>: creators=<n> marked=<n> timer=<number>/<undefined>/<absent>/<other> enemyArray=<array>/<undefined>/<absent>/<other> lost=<n> stale=<n>`.
  `absent` means `variable_instance_exists` is false; `lost` is a spawner with
  no marker whose `enemyArray` is not an array (a pack still to come that the
  map no longer shows); `stale` is a marked spawner whose `enemyArray` is an
  array (a born pack still drawn as unspawned).
- `packmarks retire timer|state` picks the retirement rule for the session and
  answers `packmarks retire -> timer` or `-> state`. `timer` is today's
  behaviour. `state` is the candidate fix: a marker goes only when its
  spawner no longer exists or the spawner's own `enemyArray`, read by name, is
  an array (undefined before and while armed, an array once spawned:
  `docs/RUNTIME_DATA_MODELS.md` § 11.2 in the hub, measured on
  `Enemy_Creator_obj` only). Nothing else retires it, so an absent
  timer, a timer that went away and an attributed create all keep the marker.
  The check is made on the spawner itself, where the marker is used.

### Where the results go

The session that decides between these is Live procedure 1 of the hub
workorder `forgepact-181-map-reveal-icons` (capture
`forgepact-181-map-reveal-icons-live-1.md`): two fresh zones, one under each
rule, a forced birth by warping next to a special marker, and a revisit. Its
results are recorded here, under `### Live 1 results`, by that workorder's
record round. The sections above are the design; the results follow.

### Live 1 results

Run 2026-10-06 on the research build (installed DLL SHA-256 `d2bb1c72…`),
character slot 14, saves backed up and restored clean afterwards. Zone A was
The Depths of Hell (zone level 294) under `retire timer`, zone B Steam Train
(zone level 293) under `retire state`, then a forced birth in B by warping
beside an ambush marker, and a revisit of A. All 19 checks of the procedure
passed. The capture is the hub's
`.claude/workorders/forgepact-181-map-reveal-icons-live-1.md`; the numbers
below are read from it.

What each check showed:

- **The skull and devil icons are ours; the game draws none of them (H5
  ruled out, measured).** At arrival in A, `enumerations=18` and `iconDraws=781`; the
  owner saw white skulls, devil icons and coloured dots. With `reveal packs 0`
  only the coloured dots remained, which the owner called vanilla.
- **The sprites are not lost (H4 ruled out, measured).** `loaded=7/7` at
  arrival and 30 s later, with `iconDraws` growing from 781 to 4471.
- **Unarmed give-up is the mechanism (H1, measured).** In A, 15 ancient and 3
  miniboss spawners were listed and marked about a second before the first
  full read; by then all were gone, `givenup=15` at `age=600..602` and
  `givenup=3` at `age=600..603`, with no other retirement. The census found
  `enemyCreatorTimer` and `enemyArray` both absent (`variable_instance_exists`
  false) on all 18, at arrival and 30 s later, so as far as those two reads
  show, neither variable was going to arm them. The owner saw the devil icons
  go within 30 s, leaving the skulls and the dots. The census counted all 18
  as `lost`; on a kind with no `enemyArray`, `lost` only says the spawner has
  no marker and cannot tell a born pack from one still to come.
- **The special spawners carry no timer in B either (measured).** Under
  `state` in B the census read `enemyCreatorTimer` absent on all 9 ambush,
  10 ancient, 6 colossal chest and 4 miniboss spawners, at arrival and 30 s
  later; only the normal spawners carried one. So the 600-frame give-up
  would have dropped those markers too under `timer`.
- **No false birth retired a special marker (H2 not observed under `timer`,
  measured).** Ancient and miniboss had `attributed=0` and no attributed
  creates in A. Normal spawners did have creates attributed to them (58:
  `Skeleton_Mage_Fire_obj`, `Imp_Passive_obj`, `Hell_Beast_Passive_obj`).
  Later in the session, under `state`, miniboss spawners had creates
  attributed too (`Hellspawn_Guardsman_obj` 4 in B, `Servant_of_Devil_obj` 3
  on the revisit), from spawners about 3000 px from the player, and their
  markers held. Whether those creates were a miniboss birth is not
  established; if they were, a born pack's marker held, which is not what
  the marker is for.
- **No early birth (H3 not observed, measured).** At 30 s in A no special
  spawner had an `enemyArray` at all, so the census could show no born
  pack, and ancient and miniboss had `attributed=0`: no create was
  attributed to them. The attribution itself was shown working on special
  spawners later, when the forced ambush birth in B had 42 creates
  attributed.
- **`state` keeps every kind (measured by the owner's eye).** In B, 80
  markers (normal 51, ambush 9, ancient 10, colossal chest 6, miniboss 4) all
  held from arrival to 30 s later, every kind `lost=0 stale=0 givenup=0`, and
  the owner confirmed the icons stayed. The counters could not have failed
  here: under `state` nothing retires a marker but a spawner that no longer
  exists or an `enemyArray` that is an array, so the evidence is what the
  owner saw.
- **`state` still retires born packs, for the kinds it can read (measured).**
  Warping beside an ambush marker turned 6 of the 9 ambush spawners'
  `enemyArray` from undefined to an array: `marked` 9 → 3, `stateborn=6`,
  `stale=0`, with 42 creates attributed. One more normal pack was born at the
  same time (`enemyArray` 11 → 12 arrays, `stateborn` 11 → 12). The owner saw
  packs appear, a mix of both.
- **The revisit read `stale=0`, but that proves little (measured).** Back in
  A under `state`, 10 s after arrival every kind read `stale=0`, with normal
  92, ancient 15 and miniboss 3 marked. On the revisit all 92 normal
  spawners read `enemyArray` undefined, including the 10 that read as an
  array before the player left, so no spawner could count as `stale`:
  `stale=0` held by construction. And `state` appears to have marked those
  10 spent normal spawners again (normal `marked=92`, where 82 were unspawned
  before the player left). Why the arrays read undefined on the revisit is
  not established.
- **Champion and legion were not observed.** Neither zone had a spawner of
  either kind (`inames` found no ambush or legion instance in A; B had none
  of champion or legion), so nothing here is measured for them.

**Route.** The mechanism behind #181 is unarmed give-up: the ancient and
miniboss spawners carry no `enemyCreatorTimer`, so the 600-frame rule dropped
their markers with no create attributed to them. The `state` candidate kept
every kind in B and retired the forced ambush birth and the normal births.
**Not established:** whether a born ancient or miniboss marker retires under
`state`. Those spawners carry no `enemyArray` (nor does the colossal chest),
so under `state` their marker goes only when the spawner itself no longer
exists, and no birth of either kind was forced in this session. A follow-up
session on the same build, Live procedure 3 of the same workorder, tested
it; its results follow.

### Live 3 results

Run 2026-10-06 on the same research build (installed DLL SHA-256
`d2bb1c72…`), character slot 14, saves backed up and restored clean
afterwards, `forgepact.json` unchanged. One fresh zone, Satanic The Depths of
Hell (zone level 514), under `retire state` throughout: 92 normal, 15 ancient
and 3 miniboss spawners listed, no other kind. The player was warped beside
an ancient marker, then a miniboss marker, then a normal marker as the
control, then a second ancient marker, about 5 s per warp, with the owner
watching. Six checks passed, one failed and one was not observed. The
capture is the hub's `.claude/workorders/forgepact-181-map-reveal-icons-live-3.md`.

- **The build and the instrument answered (pass, measured).** The DLL hash
  matched, `ping` answered, `packmarks why` printed its per-kind lines,
  `packmarks retire state` took, and `packmarks stat` read `hook=native` with
  `retire=state` about 2 s later.
- **Both kinds under test were present (pass, measured).** `listed=15` for
  ancient and `listed=3` for miniboss, all marked, `givenup=0` on both: under
  `state` neither kind was given up, as in Live 1's zone B.
- **A normal birth still retires its marker (control, pass, measured).** At
  the normal warp, normal `stateborn` rose 35 → 48 and the census's
  `enemyArray` array count 35 → 48, with `kinds` normal 57 → 44; the owner
  saw monsters there ("same for normal"). Whether the normal icon left the
  minimap was not reported by the owner.
- **The ancient markers stay under `state` at warps where the owner saw
  monsters appear (fail, measured).** At both ancient warps (markers 263690 and 264438) the owner
  saw monsters appear ("ancient warps spawned enemies") and saw the ancient
  markers stay ("ancient markers are not cleared as well"). Ancient
  `destroyed=0` and `stateborn=0` throughout, `kinds` ancient 15 → 15, and
  the census unchanged at `timer=0/0/15/0 enemyArray=0/0/15/0`: every
  ancient spawner still existed and still carried neither variable
  afterwards. Creates attributed to ancient spawners rose 0 → 3 at the first
  ancient warp, 3 → 7 at the miniboss warp and 7 → 10 between the control
  read and 5 s after the second ancient warp (about 3.5 min)
  (`Hell_Beast_Passive_obj` 3, `Skeleton_Mage_Fire_obj` 3,
  `Imp_Passive_obj` 2, `Undead_Priest_Passive_obj` 2).
- **A miniboss birth was not observed.** The owner saw the miniboss markers
  stay. Miniboss `attributed=0` with no creates, `destroyed=0`,
  `stateborn=0`, `kinds` miniboss 3 → 3 and the census unchanged. A
  screenshot at the miniboss warp shows named monsters ("Sacrilegious
  Goliath", "Infernal Mystic") nearby, but no create was attributed to a
  miniboss spawner and the owner did not say whether a miniboss pack
  appeared, so whether one was born is not established. Ancient-attributed
  creates rose 3 → 7 at that same warp, so the named monsters may belong to
  an ancient pack.

**Route: `kinds-birth: kept`.** The `state` candidate does not retire an
ancient marker at warps where the owner saw monsters appear, because the
ancient spawner outlives those creates and carries no `enemyArray` to read; so `state` as built cannot
ship as the fix. The measured lead: a create attributed to an ancient spawner
coincided with the warps where the owner saw monsters appear, while the
spawner's variables did not change. Normal packs were born at both ancient
warps too, so the owner's report does not single out an ancient pack; the
3 → 7 ancient creates came at the miniboss warp; and the 7 → 10 rise spans
about 3.5 minutes, not just the warp. Whether it marks every ancient
birth, and nothing else, is not established. For miniboss spawners no birth signal is
established: Live 1's attributed creates are not shown to be births, and in
this session named monsters showed near the miniboss warp with no create
attributed to a miniboss spawner. The
shipping retirement rule is being redesigned from these results.

### The second instrument (Live 4)

Live 3 left the ancient and miniboss birth unread: those spawners carry
neither `enemyCreatorTimer` nor `enemyArray`, and an attributed create is a
lead, not a measurement. The owner then asked for a measured rule for every
kind before anything ships, for zone revisits not to bring back an icon for
a pack already created, and for the kinds the game builds at zone arrival
(miniboss, legion, champion) to keep their icon while the pack lives. This
section is the design of the research build that measures those; as
written, before its session, none of it is measured.

**What the game is expected to do (static reading, not measured).** The
shared facts are in the hub's `docs/RUNTIME_DATA_MODELS.md` § 11.2; in
short, in our own words:

- All seven creator objects make a record in the game's protected value
  store when they are created and keep its key in their instance variable
  `spawnPack`. The pack's state is that record's value, which only the
  store's getter returns: reading `spawnPack` by name gives the key, not the
  state (the same trap as a monster's `damage`, the hub's
  `RUNTIME_DATA_MODELS.md` § 13.7). A key outside 0..262143 must never reach
  the getter: a -1 key faulted the game (§ 5.8 there).
- An ancient creator's state starts at 1. Once the player comes within
  about 1200 px it becomes 2, and the pack is built the next frame (state
  3); the creator stays. So an ancient pack exists at most a frame after
  the state leaves 1, which is why the candidate below reads 2 or more as
  born.
- Miniboss, legion and champion creators build their pack on their own
  timer or first steps after the zone is created, with no test of where the
  player is, and then set the state to 2: by this reading they are built at
  arrival, not on approach.
- A colossal chest creator builds its wave when its state is set, which
  opening the nearby `Colossal_Chest_obj` does; it then reads 2.
- An ambush creator fills `enemyArray` when it builds (measured in Live 1);
  for a normal creator, which state value means born was not read.
- Not read: what the zone state restores on a revisit, and anything in the
  miniboss, legion or champion creator that watches the pack after its
  build.

**What each new field reads** (the exact line shapes are in the hub's
`docs/submodules/ForgePact/instructions.md`, Command Reference, the
`packmarks why` row; research build only, except ` unread=`):

- `packmarks stat` gains ` unread=<n>` in both builds: birth-signal reads
  since the game started that could not be made. The research build adds
  `retire=kind` beside `timer` and `state`.
- `packmarks census` first prints a getter control on the boss probe's
  known slot, `gDataProtected[177]`: `-> proven` when `GPV` and
  `PC_GetVariableGMLWrapper`, both called by name, answer the same non-zero
  number for its key, so every protected-state read has a positive control
  through the same getter in the same session. Each kind line then adds `born=`
  (spawners whose kind's signal reads born now), `attributedUnborn=`
  (spawners with a create attributed to them this zone whose signal reads
  unborn, the H2 question asked again per spawner) and `spawnPack=`, a tally
  of the protected state by value.
- `packmarks census <kind>` prints one line per spawner of that kind: its
  id and position, whether it is marked and born, its protected state, its
  `enemyArray` kind, its attributed creates and its pack members alive out
  of those recorded.
- `packmarks creator <id>` prints one spawner whole: its object and kind,
  every protected value the static reading names (each as key and value,
  the key guard first), its recorded members by object, the distance to the
  nearest living instance of each object attributed to it and of
  `Enemy_Parent_obj`, and every instance variable it carries. The last line
  set is the net for a signal the static reading missed.
- `packmarks why` adds, per kind, `kindborn=`, `packgone=`, `held=`,
  `unlinked=`, `remembered=` and `sameid=` (below), and a second list,
  `other creates:`, the four non-enemy objects that kind's spawners created
  most, so a spawner's birth that makes no enemy-family object is still
  seen.

**The `kind` candidate (`packmarks retire kind`).** A marker is retired
when its spawner no longer exists, or by its kind's own entry in one table
(`kKindRules` in `PackMarkers.hpp`), asked of the spawner itself in the
rotating check, where the marker is used:

- normal and ambush: `enemyArray` is an array, as under `state` (measured
  on both in Live 1).
- ancient, champion, colossal_chest, legion, miniboss: the protected pack
  state, read through `spawnPack` and the getter by name, is 2 or more (the
  static reading above; not measured).
- Nothing else: no timer, no give-up, no attributed create. A read that
  cannot be made (a variable the kind needs is absent, the key guard refuses
  the key, a read throws) keeps the marker and counts in ` unread=`, so
  "held because unread" shows beside "held because unborn".
- normal, ambush, ancient and colossal_chest are in `birth` mode: the marker
  goes when the signal reads born (`kindborn=`), and a spawner already born
  when it is listed gets no marker at all (counted `kindborn` at age 0).
- miniboss, legion and champion are in `packgone` mode (the owner's
  decision): the birth signal is read and counted (`held=`, markers held
  now whose signal reads born) but retires nothing, and a spawner listed
  already born still gets a marker. The marker goes when the spawner no
  longer exists (`destroyed=`) or when its pack is gone (`packgone=`). It
  stays drawn on the spawner; nothing is ever drawn on a monster.

**The pack-gone signal is not established.** The static reading found no
creator event that watches its pack after the build, and no variable on a
built monster naming its creator, so neither the creator's state nor a link
back from the monster is expected to tell us the pack died; both are
readings of what was seen, not of everything (not read: the champion's
steps after its build, and what the miniboss and legion creators do with
`destroySelf` after theirs). Live 1 and Live 3 saw miniboss creators still
present after arrival (measured), so the spawner's own end does not mark it
either. The candidate is our own record instead: every enemy instance a
spawner creates (the create hooks' attribution, now also keeping the
created instance's id, read from the call's result after the original
returns) is that spawner's member, kept per spawner id for the whole game
session rather than per zone. Live 3 read miniboss `attributed=0` although
the pack is built at arrival, and one explanation, not established, is that
the build runs before the per-zone counters reset. The pack is gone when at
least one member was recorded and none of them exists now
(`instance_exists`, in the rotating check). A born `packgone` marker with no
member recorded is kept and counted `unlinked=`, never retired on a guess.
Two research reads stand beside it: `creator <id>`'s `near:` distances, and
its protected and variable lines read before and after a kill, which show
whether any creator value moves when the pack dies.

**The birth memory.** A spawner retired by a birth rule (`spawned`,
`timergone`, `stateborn`, `kindborn`, `packgone`, or born at listing in
`birth` mode) is remembered for the whole game session, by its id and by
the room key, kind and position rounded to whole pixels. An unknown room
key records and matches nothing by position. It is recorded under every
policy and applied only under `kind`: a later listing of a remembered
spawner gets no marker and counts `remembered=`. Both keys, because whether
a revisit keeps spawner ids is not known; `sameid=` counts listed spawners
whose id was listed on an earlier visit to the same room, which answers it.
A `packgone` spawner enters the memory only once its pack is gone, so a
living pack's marker comes back with the zone. `timer` and `state` are
unchanged.

**Where the results go.** The session that measures this is Live procedure
4 of the hub workorder `forgepact-181-map-reveal-icons` (capture
`forgepact-181-map-reveal-icons-live-4.md`): the getter control, a zone with
ancient, miniboss and colossal chest spawners under `retire kind`, a
miniboss read at arrival and then killed for the pack-gone signal, an
ancient birth forced by a warp, a normal birth as the positive control, a
colossal chest opened, a revisit, and champion and legion only if a zone
of the session has them.
Its record round writes the results under `### Live 4 results`.
