# Pet moves on from loot it cannot pick up (#94)

Issue #94, as the owner corrected it on 2026-09-27: with **lots** of things on
the ground to pick up (gold stacks, runes, other socketables such as rubies,
crafting materials, not only quest items), the pet gets stuck "jumping around"
one item and neither collects it nor moves on. This is the game's own
companion loot pickup (`Companion_obj`), not ForgePact's Pet Quest Collector,
which handles `Quest_Object_Parent_obj` items only. The owner's framing: a game
bug, to be fixed only if the fix is easy. It is: the mod `petunstick` (panel:
**Pet moves on from loot it cannot pick up**, Mods tab, Quality of Life, off by
default) needs no hook, no address and no call into a game script.

Everything below is labelled. **Static reading** means read from the game's
compiled code in a local decompiler and written here in our own words; no game
code is quoted (`AGENTS.md` § Legal), and objects and scripts are named by their
`hs-game-sdk` names. **Measured** means observed on the running game. A thing
that was looked for and not seen is written "not observed", never "does not
happen". The whole mechanism below is a static reading of the current build's
compiled events (2026-09-27). One session has run since: Live 1 of the
workorder `forgepact-pet-loot-stuck` (2026-09-28), which did not reproduce the
stuck state and saw the mod give nothing up, so none of the reading is
confirmed by a measurement yet (see [Live 1 results](#live-1-results-2026-09-28)).
The mod ships off by default in ForgePact 2.0.1.
The game facts are also recorded in the hub's `docs/RUNTIME_DATA_MODELS.md`
§ 10.6, the shared record every module reads.

**History.** The dev2 bug batch first read #94 as the Pet Quest Collector
circling one quest item, and changed that collector's target selection (a hold
on a failed target, a cursor over the quest-item family;
[dev2-bug-batch-research.md](dev2-bug-batch-research.md#94-the-pet-circles-one-quest-item-when-many-are-on-screen)).
The owner corrected the report on 2026-09-27: #94 is the companion's own loot
pickup, and this document is its reading. The quest-collector change stays as
an improvement of its own, without an issue number.

## Static reading

Read from `Companion_obj`'s Create, Begin Step, Step, Alarm 0 and
Collision-with-`Coin_obj` events, `Loot_Ground_obj`'s Create and Alarm 9,
`Coin_obj`'s Create and alarms, and the scripts `PickupLoot`,
`LootGroundActiveStep` and `LootGroundDeActiveStep`. The events were found
through the game's own event-registration table (pairs of an event name such
as `gml_Object_Companion_obj_Step_0` and its compiled function) and annotated
with the name-slot table described in
[pet-quest-collector-c-research.md](pet-quest-collector-c-research.md).
Object events have no script-table entry, so none of this can be hooked by
name; the mod does not need to.

### The pet's loot variables

`Companion_obj`'s Create gives it `lootList` (a ds_list), `lootTarget` (-4,
meaning none), `lootTimer` (0), `lootDistance` (1500 px), `playerRange`
(128 px), and `seekSpeed`, `baseSpeed` and `deltaSpeed` (0 at Create; Alarm 0
sets the speeds later from character data, and their values were not read),
plus `move` and `deltaTimer`. Its Begin Step sets `deltaTimer` from the game's
global delta factor and the built-in `speed` to `deltaSpeed * deltaTimer`, so
the engine's own motion moves the pet each frame by whatever `deltaSpeed` the
Step left behind.

### The scan

While `lootList` is empty and `lootTimer` has run out, the Step lists every
`Loot_Ground_obj` within `lootDistance` of the **player** (not the pet),
nearest to the player first. An item joins `lootList` only when all of these
hold: its item type is a tarot card, a socketable (except the affix-rolled
socketable bases), a crafting material, a key, or one specific consumable;
`itemActive` is true; its `itemCompanionTimer` is 0 or less; and the loot
filter shows it (`lootFilterVisible`). A second pass over the same circle adds
**every** `Coin_obj`, unfiltered. Then `lootTimer` is set to half a second of
frames.

### The target

When `lootList` is not empty and `lootTarget` does not name a live instance,
the first entry of `lootList` becomes the target. **That is the only place a
new target is chosen**: nothing else replaces a target that still exists.

### Travel and arrival

While the pet is further from its target than twice its `deltaSpeed`, it takes
`seekSpeed` as its `deltaSpeed`, turns toward the target and moves; a target
off screen (with a 256 px margin) makes the pet jump onto it.

Once within twice `deltaSpeed` it has arrived:

- **A ground item** (`Loot_Ground_obj`): the pet runs the game's `PickupLoot`
  script, with the item as `self`, for every ground item within 144 px of the
  **pet** that passes the same type, active and filter tests. An item whose
  pickup succeeds is destroyed. Whether it succeeded or not, the item is taken
  out of `lootList`, and then the list's first entry (the target's slot) is
  removed as well.
- **A coin** (`Coin_obj`): the coin is moved onto the pet, so the pet's
  collision event with the coin credits the gold (through `PickUpGoldCheck` and
  `GoldLogAdd`), takes the coin out of `lootList` and plays the sound. Then the
  list's first entry is removed.
- **Arrival does not stop the pet.** `deltaSpeed` is left at the travel speed,
  so the engine carries the pet on through the target; the next frame it is
  too far again and turns back. With a target that goes away this lasts one
  frame and is invisible. With a target that stays it is the reported
  "jumping around one item".

### `itemCompanionTimer`: the game's own "not for the pet yet"

Every `Loot_Ground_obj` gets a positive `itemCompanionTimer` at Create (from a
global whose value was not read). Its Alarm 9, every 0.3 s of game speed,
takes 0.3 s worth of frames off it while it is above 0, so the timer is in the
game's frame units. The scan skips an item while it is above 0. `Coin_obj` has
no such variable. The player's own pickup path (`Loot_Manager_obj`,
`m_LootGroundPickupCheckStep`, `playerLootTarget`) does not read it.

### The gate

The whole loot block runs only when an unnamed one-argument helper returns
true. It reads the local-player data and checks that an instance exists; it is
most likely "this pet's player is the local one and exists". What it tests is
not established. The same helper gates the coin collision event.

### Why the pet pins itself to one item

Put together: a target is replaced **only** when its instance ceases to exist;
arrival does **not** stop the pet; and a ground item whose `PickupLoot` returns
false stays on the ground while `lootList` drains, and the next scan puts it
back (it still passes every test). So one item that cannot be picked up keeps
`lootTarget` for as long as it lies there. The pet swings through it every
frame, tries `PickupLoot` on everything within 144 px every frame, and nothing
in the game moves it on. That is the reported symptom's shape. The same holds
for a coin whose collision event does not remove it.

Why `PickupLoot` can return false (static reading of the script): the item's
instance is already gone or undefined; `ItemCheckHash` rejects the item; or,
on the offline path, the script returns the result of `AddToInventory`, so a
full grid or a full stack leaves the item on the ground and returns false.

## Not established

- **Which pickup failure the owner meets** (a full grid, a stack limit, a hash
  refusal) and whether "lots of things" acts through a full inventory, a stack
  limit or something else. No session has measured it. The mod does not need
  to know: it acts on the observable "same target, within reach, not going
  away".
- **Whether coins ever stick.** The reading allows it (a coin its collision
  event does not remove); not observed.
- **When the freed instance id is re-minted.** Live 2 captured the pet's
  target naming a decoration object (no item data on it), so the id stopped
  being the item the pet's scan chose by then; no sample caught the
  free-and-reuse itself, so "the item died and the game reused the id" is the
  reading that fits the samples, not a measured event
  ([Live 2 results](#live-2-results-2026-10-02)).
- **`seekSpeed`'s value** (set in Alarm 0 from character data) and
  **`itemCompanionTimer`'s starting value** (a global). The mod needs neither.
- **What the gate helper tests.**
- **The kind of value `lootList` reaches us as.** The game stores a ds_list
  handle; whether this runner hands it back as a real or as `VALUE_REF` was not
  measured, which is why the mod gates the clear on `ds_exists` and never on
  the kind.
- **The stuck state's frequency and its exact trigger for #94's own shape.**
  Live 1 (2026-09-28) did not reproduce an item that stays with the mod off,
  and with it on no give-up counter rose
  ([Live 1 results](#live-1-results-2026-09-28)). Live 2 (2026-10-02) did
  measure a stuck pet, but of a different shape - a stale target id naming a
  decoration ([Live 2 results](#live-2-results-2026-10-02)) - so #94's
  item-that-stays remains unobserved, which is not evidence that it does not
  happen.

## The fix and what it does not do

`petunstick 1` / `petunstick 0` (player build and research build; the panel
row `mod_pet_loot_unstick` sends `petunstick 1` only when it is on, and nothing
while off). While on, `FrameCallback` (the end of the frame, after the Step)
runs `PetLootUnstickTick()` in `plugin/ModuleMain.cpp`:

1. Resolve `Companion_obj`, `Loot_Ground_obj` and `Coin_obj` once through
   `asset_get_index`, with the `hs-game-sdk` constants as the fallback. Find
   the first `Companion_obj`; none (a menu, no pet out) forgets the watch.
2. Read the pet's `lootTarget` as a number, whatever kind carries it; below 0,
   or a target `instance_exists` denies, forgets the watch. Read the target's
   `object_index` and classify it by family (ground item, coin, or neither).
   **A target from neither family is given up on this very tick, without the
   watch** ([Live 2](#live-2-results-2026-10-02)): the pet's list only ever
   holds ground items and coins, so such an id is a stale one the game reused
   for something else, and it can never be picked up. An unreadable
   `object_index` keeps the watch route - a failed read is not evidence of a
   wrong id. The routing question is the header's `PetLootRoute`.
3. For a target from either family, read pet and target `x`/`y` and take the
   distance, then feed the game-independent `PetLootStuckWatch`
   (`plugin/include/ForgePact/PetLootUnstickMod.hpp`). It answers "give it up"
   on the frame the same target's run within `kPetLootStuckRadiusPx` (160 px;
   the game's pickup circle is 144 px) reaches `kPetLootStuckFrames`
   (90 frames, 1.5 s at 60 fps) in a row, and re-arms on answering: the run
   restarts at each give-up, so a target that stays the target and stays in
   reach (the game took it straight back) is given up again every
   `kPetLootStuckFrames` frames, never a frame earlier. No target, another
   target, a distance beyond the radius (or an unreadable one) or a skipped
   frame restarts the count, and so does a give-up the tick could not carry
   out (`give-up failed=`, which resets the watch).
4. Giving up writes only what the game itself reads next. On a ground item
   (by `object_index`, the ground-item family) the tick first asks
   `variable_instance_exists` whether the item carries `itemCompanionTimer`:
   the name comes from a static reading, and `variable_instance_set` with a
   name the instance lacks creates a stray variable without an error, which
   would count as held back while holding nothing. When the name is present
   it writes `itemCompanionTimer` = `kPetLootHoldFrames` (600 of the game's
   frames, about 10 s), so the next scan skips the item, and counts
   `held back=`. When it is absent it writes nothing to the item, counts
   `timer absent=` (keeping the last `object_index`, and logging the first one
   once), and still drops the target and clears the list below. An exists
   check or a timer write that throws counts `give-up failed=` and drops
   nothing, as does a `lootTarget` write that throws. Then, on every target, the pet's `lootTarget` = -4; and the pet's
   `lootList` is emptied with `ds_list_clear`, only when it reads as a finite
   number that `ds_exists(…, ds_type_list)` confirms is a live list, so the
   game's next scan (at most half a second later) rebuilds it without the held
   item. The clear is the write that matters: the timer only keeps the item
   out of a new scan, and a list left holding it would hand it straight back.
5. It counts what it did. `petunstick 0` prints one line,
   `FullStatLine()` from the header followed by the tick's own
   `PetLootLocalStatSuffix()`:
   `petunstick stat: held back=<n> coins released=<n> longest same-target=<n> frames ticks=<n> no pet=<n> no target=<n> target gone=<n> unreadable=<n> (last <read>) list not cleared=<n> (last <reason>) give-up failed=<n> (last <reason>) other kind=<n> (last object_index <n>) timer absent=<n> (last object_index <n>) re-picked while held=<n> (ground <n> coin <n>) (last ground timer read <n>)`.
   Each `(last …)` part appears only once its counter has something to name;
   the last one reads `(last ground timer unreadable)` when the read-back
   failed. The line says where the ticks went (`ticks=`, `no pet=`,
   `no target=`, `target gone=`), every refusal with the read it failed on
   (`unreadable=`, `list not cleared=`, `give-up failed=`), a target of
   neither family that was only dropped (`other kind=`) and a ground item
   with no timer (`timer absent=`), so a bug report can tell "did nothing"
   from "could not look" from "did the wrong thing".

   `re-picked while held=` is the check on the hold itself. `held back=`
   counts the timer write returning, not the item staying out of the pet's
   next scan. The tick remembers its last eight give-ups (id, frame, kind) in
   the header's `PetLootRepickRing`, and on every tick that sees a live target
   asks it whether that target is one of them taken back within
   `kPetLootHoldFrames`: either after the pet left it (a tick of no target,
   or another target in between), or straight back on the very next tick,
   the same id the tick had when it gave it up. The game can rescan and take
   the same item in the Step right after the drop; which of its scan and its
   target pick runs first in one Step is not established, and the count
   covers both. Each such take-back is counted once per give-up, split by
   kind (`ground`, `coin`), and the first is logged; a target that stays the
   target after being counted is not counted again on every tick. A target
   taken straight back is still in reach, so the watch gives it up again
   1.5 s later and it is counted again each time. On a ground item the tick
   also reads `itemCompanionTimer` back: a positive value says the hold took
   and the pet came back anyway, 0 or less says the game reset it or the
   write never landed.

   `longest same-target=` is the longest run between give-ups, so it cannot
   exceed 90 while the mod is on; below 90, no target reached the count. A
   pet that still looks stuck beside a rising `held back=` then reads one of
   two ways: ground re-picks rising (with the timer read-back) means the hold
   did not take and the same item keeps coming back; re-picks staying 0 while
   `held back=` rises means the pet is working through distinct items, one
   after another. Coin re-picks are expected, since a coin has no timer to
   hold it. `timer absent=` above 0 has two causes this session cannot
   separate: the item lacks the name, or `variable_instance_exists` answers
   false for every item on this runner, which is not measured. Record it; do
   not diagnose from it.

Why these numbers: a target within 160 px for 1.5 s that has not gone away has
had dozens of arrived frames of `PickupLoot` attempts, and a pet travelling to
a far item never counts because it is beyond the radius. The on-sight drop has
no number to tune: it fires on the tick that sees a target from neither
family, at any distance. 600 frames is the same
hold the Pet Quest Collector uses (`kPetQuestHoldFrames`): long enough that the
pet visibly moves on and other items get their turn, short enough that an item
whose pickup failed for a passing reason (the player made room) is tried again.
All three are constants in the header, so a live finding moves one number.

**What it does not do.** It collects, destroys and credits nothing, and calls
no game script: no `PickupLoot`, no `instance_destroy`, no coin moved. It does
not change what the pet picks up or how fast it walks, and it does not make a
failed pickup succeed: an item the game cannot put in the inventory stays on
the ground, and the pet comes back to it about ten seconds later. Dropping a
target from neither family destroys nothing either: there is no item there -
only an id the game reused - so the drop only frees the pet to take the next
scan. A coin gets no
timer (it has none), only a dropped target, so a coin that sticks again is
given up again and counted again (`coins released=`, and, when the pet takes
it back within the hold, `re-picked while held=` under `coin`). A coin taken
straight back on the next tick counts there too, each time the watch gives it
up again.

**Rejected alternatives**, so nobody re-proposes them:

- **Detouring the companion's Step event or its arrival branch** (stopping the
  pet on arrival, retargeting when `PickupLoot` fails): the event is unnamed
  compiled code with no script-table entry, reachable only through a measured
  address, which `AGENTS.md` § "Never Call an Address You Resolved by Hand"
  forbids in a player build, and it would change the game's own loop.
- **Calling `PickupLoot` or `instance_destroy` from the mod** to finish the
  pickup: it would invent a collect the game refused (with a full inventory the
  item would be lost), and calling game scripts cold has crashed before
  ([pet-quest-collector-c-research.md](pet-quest-collector-c-research.md#why-the-games-scripts-cannot-be-called-cold-measured-2026-09-11)).
- **Only setting `lootTarget` to -4**: the current list, or the next scan half
  a second later, hands the same item straight back, because it is the item
  nearest the player. The hold needs the item's own timer and an emptied list.
- **Writing `lootDistance` or the type filter**: that changes what the pet does
  with every item, not what it does when it is stuck.

**Tests.** `tests/test_pet_loot_unstick_behavior.py` +
`tests/pet_loot_unstick_harness.cpp` compile the real header: baseline
scenarios (the game's own rule keeps a surviving target; the watch never asks
while the mod is off; `latched_watch_keeps_a_target_taken_straight_back`, the
pre-fix rules written out as the reference the fix departs from: fed the same
id on every frame, including the one right after the give-up, they gave it up
once and counted no re-pick) and target scenarios (given up at exactly
`kPetLootStuckFrames` and not a frame earlier; a travelling target never
counts; another target or none restarts;
`ground_taken_straight_back_is_given_up_again` and
`coin_taken_straight_back_is_given_up_again`: a target taken straight back is
given up every `kPetLootStuckFrames` frames, each give-up followed by one
re-pick of its kind, and the longest run stays at the count;
`repick_counted_once_per_give_up`: a take-back after no target or another
target counts, an id never given up or a give-up `kPetLootHoldFrames` old
does not, and a target that stays counts once; a target that vanishes asks
for nothing). The re-pick decision (`PetLootRepickRing`) is header code, so
the harness runs it. `tests/test_pet_loot_unstick_contract.py` pins
the tick's shape (gated on the switch, the three names it writes, objects by
name, the timer only on a ground item, nothing collected, destroyed or hooked,
no kind check as the gate), the off default and the panel text. On the
comment-stripped source it also pins fix-1: the `itemCompanionTimer` write
sits inside the then-block of the answer of a `variable_instance_exists` call
on that name, made before it (with negative controls: the same check fails on
the tick with the exists call removed, the condition negated, or a write added
outside the guard), and `timer absent=` and `re-picked while held=` (with its
ground/coin split) reach the line `petunstick 0` prints, and that every tick
that sees a live target reaches the header's re-pick decision without
returning because the target equals the previous tick's (with negative
controls: the same check fails with that early return put back, in the note
function or in the tick, or with the decision taken out). No harness runs the
ground item's timer branch, where no `itemCompanionTimer` means
`timer absent=`; it is pinned by shape only.

**Live.** The same-target rule is still not confirmed in a live game: Live 1
(player DLL, one crowded spot with mixed loot, mod off then on) ran on
2026-09-28 and did not reproduce the stuck state, so the mod had nothing to
act on, and the owner shipped the mod off by default on that result
([Live 1 results](#live-1-results-2026-09-28)). The on-sight rule (added
2026-10-02, ForgePact #138) has the measured capture it was written from - a
stale target id naming a decoration - but the switch acting on that shape live
is not yet measured either ([Live 2 results](#live-2-results-2026-10-02)).

## Live 1 results (2026-09-28)

**Measured**, one session: the workorder `forgepact-pet-loot-stuck`'s Live 1,
player DLL SHA-256 `083d7e02…57c7` (boot line `==== BloodPact plugin loaded
==== v2.0.0`), slot 14 ("Sorak"). The bag was filled on the back end by
unstacking stacks, with `dropmult item`, `runes`, `gems` and `ores` at 3 for a
crowded ground, then the owner played about 90 s with the mod off and about
90 s with it on, in a crowded spot.

| Check | Result | What was seen |
|---|---|---|
| `dll-hash`, `marker`, `control` | pass | the expected DLL, its boot line, `pong`, and a `petunstick 0` line with every named field |
| `off-pet-stuck` | fail | the owner saw the pet stuck at no point with the mod off: the stuck state did not reproduce |
| `on-pet-moves-on` | not observed | the owner saw no stuck pet with the mod on, but `held back=`, `coins released=`, `timer absent=` and `other kind=` all stayed 0, so the mod gave nothing up |
| `unstick-counters` | pass | both `petunstick 0` lines recorded: at the end `ticks=27900`, `target gone=11691`, `longest same-target=13 frames` (under the 90-frame give-up), `re-picked while held=0` |

So the tick ran and watched the pet the whole on phase, and no target stayed
within reach for anywhere near the give-up count: the longest run was 13
frames. Neither the stuck state nor the mod acting on it has been observed
live. That is not observed, not evidence that either does not happen: one
session with a filled bag and a crowded ground did not reproduce what the
owner reported (#94), and it cannot say whether the stuck state needs a full
grid, a stack limit or something else (§ [Not established](#not-established)). The owner shipped the mod off by
default in ForgePact 2.0.1 (2026-09-28) on this result. The 2.0.1 player DLL,
built later from the merged tree, has not been run by any session.

## Live 2 results (2026-10-02)

**Measured**, one session: the owner's own save with the research DLL, the
owner playing a zone's content, and the pet watched from outside the game by a
poller over `bp_ipc` (about 2 s polls, `tgprobe`/`cb` reads; the watcher and
its log are not part of the repo). `petunstick` was off for the captures
below. This is the first live capture of a stuck pet at all, and its shape is
not #94's item-that-stays: the pet's `lootTarget` held a stale instance id
that by then named an object which is not loot.

| Time | Target id | What the id named | Pet -> target | Pet movement |
|---|---|---|---|---|
| 10:30:03 | 304090 | `Abyss_Jungle_Dead_Aztec_Skeleton_01_obj` (object 15, child of `Visual_Parent_obj`) | 71 px, holding | `move=true`, `deltaSpeed=21.9` (travel speed); `petmove` 17 over the window |
| 10:30:41 | 298393 | the same object (another instance) | 52 px, oscillating around it | `move=true`, `deltaSpeed=21.9`; `petmove` 19 |

Both targets read `itemType=undefined`, `visible=0`, `timer=-6`, and the pet's
own movement never took it away: it travelled to the object and then ground at
it while the player stood thousands of pixels away. Both appeared within
seconds of a zone change (about 10:28:59), the moment with the most instance
churn. A Corgi companion, room `Act_04_05`.

**What this establishes.** The pet can hold a `lootTarget` that is not a loot
object at all, and it will travel to and grind at it - the "stuck in random
places" report. The game's only check on the target is `instance_exists`, which
a reused id passes, so nothing re-targets the pet while the stranger lives.
The pet's own list only ever holds `Loot_Ground_obj` and `Coin_obj`
descendants, so an id that names neither is stale by construction, and
dropping it cannot lose an item.

**What this does not establish.** No sample caught the id being freed and
re-minted, so "the item died and the game reused the id" is the reading that
fits the samples (§ [Not established](#not-established)); which item started
it, what zone state lets the reuse happen, and whether loot density speeds it
up are all unmeasured. And the A/B of the switch on this shape was not run:
with the switch on for the last stretch of the session no such target
reappeared (`held back=0`, no `other kind=`), so the fix's live effect is not
measured - its capture, the harness scenarios and the tick's routing shape
are.

This session produced the on-sight rule in §
[The fix and what it does not do](#the-fix-and-what-it-does-not-do) (added
2026-10-02, ForgePact #138): a live target from neither family is given up on
the tick that sees it, and `other kind=` counts it.
